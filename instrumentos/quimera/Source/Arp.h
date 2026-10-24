#pragma once

// QUIMERA — arpejador. Fica ANTES do motor: lê as teclas do bloco e devolve outro MidiBuffer com as notas do
// arpejo no lugar delas (bend, pressão, CCs e o resto passam direto). O motor não sabe que existe arpejo.
//
// Passos: o padrão tem LENGTH passos (1–16); cada passo pode estar ligado ou em pausa e ter ACCENT.
// Com o host tocando, o passo segue a régua (o passo 1 cai no começo do compasso); parado, o relógio começa
// na primeira tecla. SWING atrasa os passos pares (o 2º de cada par). GATE 100% emenda uma nota na outra
// (no modo Legato do VOICE vira slide). SEGURAR (hold) trava o acorde: a próxima tecla depois de soltar todas
// começa um acorde novo. SCALE + ROOT: as teclas são encaixadas na escala; com UMA tecla só, o arpejo sobe pela
// escala a partir dela (OCTAVES oitavas), então dá para tocar escala inteira com um dedo.

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>
#include "Params.h"

namespace quimera
{
class Arp
{
public:
    std::atomic<int> stepNow { -1 };   // passo tocando agora (0..15) para a tela; −1 = parado

    void reset()
    {
        held.clear();
        thru.clear();
        sounding.clear();
        physical = 0;
        lastStep = -1;
        seqIdx = 0;
        gateOff = -1.0;
        freePpq = 0.0;
        wasOn = false;
        stepNow.store (-1);
    }

    // midi: bloco de entrada (já com as teclas da tela); out: o que vai para o motor
    void process (const juce::MidiBuffer& midi, juce::MidiBuffer& out, int n, const ArpSnap& a, bool latch,
                  bool running, double ppq0, double ppqPerSample)
    {
        out.clear();
        if (a.on != wasOn)
        {
            if (a.on)
            {
                // as teclas que estavam soando direto no motor precisam soltar (senão ficam presas)
                for (auto& t : thru) out.addEvent (juce::MidiMessage::noteOff (t.ch, t.note), 0);
                thru.clear();
                held.clear();
                physical = 0;
                lastStep = -1;
            }
            else
            {
                allOff (out, 0);
                held.clear();
                physical = 0;
                stepNow.store (-1);
            }
            wasOn = a.on;
        }
        if (! a.on)
        {
            // sem arpejo: tudo passa direto; só lembra quais teclas estão soando
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOn()) addThru (m.getNoteNumber(), m.getChannel());
                else if (m.isNoteOff()) removeThru (m.getNoteNumber(), m.getChannel());
                else if (m.isAllNotesOff() || m.isAllSoundOff()) thru.clear();
                out.addEvent (m, meta.samplePosition);
            }
            return;
        }

        if (latchWas && ! latch && physical == 0) held.clear();   // soltou o SEGURAR sem tecla apertada
        latchWas = latch;
        if (running != runningWas)   // o host começou ou parou: recomeça a contagem no relógio novo
        {
            allOff (out, 0);
            lastStep = -1;
            freePpq = 0.0;
            runningWas = running;
        }

        const double rb    = divBeats[juce::jlimit (0, numDivs - 1, a.rate)];
        const double gate  = juce::jlimit (0.05, 1.0, (double) a.gate * 0.01);
        const double swing = juce::jlimit (0.0, 1.0, (double) a.swing * 0.01) * 0.5 * rb;
        const int    len   = juce::jlimit (1, numArpSteps, a.length);

        auto it = midi.cbegin();
        for (int i = 0; i < n; ++i)
        {
            for (; it != midi.cend() && (*it).samplePosition <= i; ++it)
                handle ((*it).getMessage(), out, i, latch);

            const double p = running ? ppq0 + ppqPerSample * (double) i : freePpq;
            if (! running) freePpq += ppqPerSample;

            if (held.empty())
            {
                if (! sounding.empty()) allOff (out, i);
                lastStep = -1;
                stepNow.store (-1);
                continue;
            }

            const long long g = (long long) std::floor (p / rb + 1.0e-9);
            const double off = (g & 1) ? swing : 0.0;
            if (g != lastStep && p - (double) g * rb >= off - 1.0e-9)
            {
                lastStep = g;
                const int s = (int) (((g % len) + len) % len);
                stepNow.store (s);
                const bool tie = gate >= 0.999;
                if (! tie) allOff (out, i);
                if (a.step[(size_t) s])
                {
                    std::vector<Note> next;
                    pick (a, next);
                    const float acc = juce::jlimit (0.0f, 1.0f, a.accent * 0.01f);
                    for (auto& nt : next)
                    {
                        float v = (float) nt.vel;
                        v = a.acc[(size_t) s] ? v + (127.0f - v) * acc : v * (1.0f - 0.35f * acc);
                        out.addEvent (juce::MidiMessage::noteOn (nt.ch, nt.note, (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (v))), i);
                    }
                    if (tie) allOff (out, i, &next);   // emenda: a nota nova entra antes de a velha sair
                    sounding = next;
                    gateOff = (double) g * rb + off + gate * rb;
                }
                else if (tie)
                    allOff (out, i);
            }
            if (gateOff >= 0.0 && gate < 0.999 && p >= gateOff)
                allOff (out, i);
        }
        for (; it != midi.cend(); ++it)
            handle ((*it).getMessage(), out, n - 1, latch);
    }

private:
    struct Note { int note = 0, vel = 100, ch = 1; };

    void addThru (int note, int ch)
    {
        for (auto& t : thru) if (t.note == note && t.ch == ch) return;
        if (thru.size() < 64) thru.push_back ({ note, 0, ch });
    }
    void removeThru (int note, int ch)
    {
        thru.erase (std::remove_if (thru.begin(), thru.end(), [=] (const Note& t) { return t.note == note && t.ch == ch; }), thru.end());
    }

    // solta o que está soando (menos o que está em `keep`: nota repetida na emenda continua)
    void allOff (juce::MidiBuffer& out, int pos, const std::vector<Note>* keep = nullptr)
    {
        for (auto& s : sounding)
        {
            bool kept = false;
            if (keep != nullptr)
                for (auto& k : *keep) if (k.note == s.note && k.ch == s.ch) kept = true;
            if (! kept) out.addEvent (juce::MidiMessage::noteOff (s.ch, s.note), pos);
        }
        sounding.clear();
        gateOff = -1.0;
    }

    void handle (const juce::MidiMessage& m, juce::MidiBuffer& out, int pos, bool latch)
    {
        if (m.isNoteOn())
        {
            if (held.empty() || (latch && physical == 0))
            {
                if (latch) held.clear();
                seqIdx = 0;
                if (! runningWas) { freePpq = 0.0; lastStep = -1; }   // parado: o primeiro passo sai na hora
            }
            ++physical;
            for (auto& h : held) if (h.note == m.getNoteNumber()) return;
            if (held.size() < 32) held.push_back ({ m.getNoteNumber(), m.getVelocity(), m.getChannel() });
        }
        else if (m.isNoteOff())
        {
            physical = juce::jmax (0, physical - 1);
            if (! latch)
            {
                const int nn = m.getNoteNumber();
                held.erase (std::remove_if (held.begin(), held.end(), [nn] (const Note& h) { return h.note == nn; }), held.end());
            }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear();
            physical = 0;
            allOff (out, pos);
            out.addEvent (m, pos);
        }
        else if (m.isSustainPedalOn() || m.isSustainPedalOff())
            return;   // com arpejo o pedal não segura as notas do arpejo (para travar o acorde: SEGURAR)
        else
            out.addEvent (m, pos);
    }

    // as notas do próximo passo
    void pick (const ArpSnap& a, std::vector<Note>& out)
    {
        out.clear();
        if (held.empty()) return;
        const int octaves = juce::jlimit (1, 4, a.octaves);
        std::vector<Note> base (held);
        // SCALE: as teclas entram encaixadas na escala (a nota de fora vai para a da escala mais perto)
        if (scales::active (a.scale))
        {
            for (auto& nt : base) nt.note = juce::jlimit (0, 127, scales::snap (nt.note, a.scale, a.root));
            std::vector<Note> uniq;
            for (auto& nt : base)
            {
                bool dup = false;
                for (auto& u : uniq) dup = dup || u.note == nt.note;
                if (! dup) uniq.push_back (nt);
            }
            base.swap (uniq);
        }
        if (a.mode != arpAsPlayed)
            std::sort (base.begin(), base.end(), [] (const Note& x, const Note& y) { return x.note < y.note; });

        if (a.mode == arpChord)
        {
            const int o = (int) (seqIdx++ % octaves);
            for (auto nt : base)
                if (nt.note + 12 * o <= 127) { nt.note += 12 * o; out.push_back (nt); }
            return;
        }

        std::vector<Note> seq;
        if (scales::active (a.scale) && base.size() == 1)
        {
            // uma tecla com SCALE: o arpejo anda pela escala, da tecla até OCTAVES oitavas acima (inclusive)
            Note nt = base.front();
            const int top = nt.note + 12 * octaves;
            for (int m = nt.note; m <= top && m <= 127; m = scales::step (m, 1, a.scale, a.root))
            {
                nt.note = m;
                seq.push_back (nt);
            }
        }
        else
            for (int o = 0; o < octaves; ++o)
                for (auto nt : base)
                    if (nt.note + 12 * o <= 127) { nt.note += 12 * o; seq.push_back (nt); }
        const int cnt = (int) seq.size();
        if (cnt == 0) return;
        const long long k = seqIdx++;
        int idx = 0;
        switch (a.mode)
        {
            case arpDown: idx = cnt - 1 - (int) (k % cnt); break;
            case arpUpDown:
            case arpDownUp:
            {
                const int cycle = juce::jmax (1, 2 * cnt - 2);
                const int ph = (int) (k % cycle);
                idx = cnt == 1 ? 0 : (ph < cnt ? ph : cycle - ph);
                if (a.mode == arpDownUp) idx = cnt - 1 - idx;
                break;
            }
            case arpRandom: idx = rng.nextInt (cnt); break;
            case arpConverge:   // de fora para dentro: grave, agudo, 2º grave, 2º agudo…
            {
                const int j = (int) (k % cnt);
                idx = (j & 1) ? cnt - 1 - j / 2 : j / 2;
                break;
            }
            case arpDiverge:    // do meio para fora
            {
                const int j = (int) (k % cnt), mid = (cnt - 1) / 2;
                idx = (j & 1) ? mid + (j + 1) / 2 : mid - j / 2;
                break;
            }
            default: idx = (int) (k % cnt); break;   // UP e AS PLAYED
        }
        out.push_back (seq[(size_t) juce::jlimit (0, cnt - 1, idx)]);
    }

    std::vector<Note> held, thru, sounding;
    int physical = 0;
    long long lastStep = -1, seqIdx = 0;
    double gateOff = -1.0, freePpq = 0.0;
    bool wasOn = false, latchWas = false, runningWas = false;
    juce::Random rng;
};
} // namespace quimera
