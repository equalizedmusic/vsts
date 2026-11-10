// Abre o editor do plugin, toca um loop de psytrance a 145 BPM por alguns segundos (para medidores,
// mandala, rolos e fogo estarem vivos) e grava a tela em PNG a 200%. Instrumento (SHOT_SYNTH): em vez do loop,
// toca um acorde segurado por MIDI com pressão (MPE/aftertouch) subindo.
// Uso: shot_<plugin> <pasta de saída>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

#if SHOT_GUITARRA
 #include "PluginProcessor.h"   // STOMPBOX: monta boards de conferência (todos os pedais, os 5 amps, o TUNER)
#endif

#ifndef SHOT_MESA
 #define SHOT_MESA 0
#endif
#ifndef SHOT_ALIGN
 #define SHOT_ALIGN 0
#endif
#ifndef SHOT_VOZ
 #define SHOT_VOZ 0
#endif
#ifndef SHOT_GUITARRA
 #define SHOT_GUITARRA 0
#endif
#ifndef SHOT_BAIXO
 #define SHOT_BAIXO 0
#endif
#ifndef SHOT_MAESTRO
 #define SHOT_MAESTRO 0
#endif
#ifndef SHOT_MAGOS
 #define SHOT_MAGOS 0
#endif
#if SHOT_MAGOS
 #include "PluginProcessor.h"   // MASTER DOS MAGOS: LISTEN / CAST no motor e um MAESTRO LINK simulado (8 grupos)
#endif
#if SHOT_MAESTRO
 #include "Dsp.h"   // MAESTRO: o Registry (LEARN / LOCK) das cópias abertas no mesmo programa
#endif
// MESA: a foto mostra a CONSOLE cheia, com MESA CANAL (canais e grupos) tocando o mesmo loop; e a janela de um canal
static std::vector<std::unique_ptr<juce::AudioProcessor>> extras;
#if SHOT_MESA
juce::AudioProcessor* mesaCriaCanal();
#endif

namespace
{
constexpr double fs = 48000.0, bpm = 145.0;
constexpr int    block = 512;
#ifndef SHOT_SYNTH
 #define SHOT_SYNTH 0
#endif
#ifndef SHOT_W
 #define SHOT_W 2000
 #define SHOT_H 1160
#endif

// host "tocando": régua andando no BPM
class ShotPlayHead : public juce::AudioPlayHead
{
public:
    std::atomic<double> ppq { 0.0 };

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (true);
        p.setPpqPosition (ppq.load());
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

// bumbo em todo tempo, baixo rolante nas semicolcheias, chimbal no contratempo e um pad
class AudioThread : public juce::Thread
{
public:
    AudioThread (juce::AudioProcessor& p, ShotPlayHead& h) : juce::Thread ("shot-audio"), proc (p), head (h) {}

    void run() override
    {
        juce::AudioBuffer<float> buf (SHOT_ALIGN ? 4 : 2, block);
        juce::MidiBuffer midi;
        juce::Random rnd (7);
        const double twoPi = juce::MathConstants<double>::twoPi;

        while (! threadShouldExit())
        {
            auto* l = buf.getWritePointer (0);
            auto* r = buf.getWritePointer (1);
            for (int i = 0; i < block; ++i, t += 1.0 / fs)
            {
                const double beat = t * bpm / 60.0;
                const double inBeat = (beat - std::floor (beat)) * 60.0 / bpm;            // s desde o tempo
                const double six = beat * 4.0;
                const int    k16 = (int) std::floor (six) % 4;
                const double in16 = (six - std::floor (six)) * 15.0 / bpm;                // s desde a semicolcheia

                kickPh += twoPi * (45.0 + 110.0 * std::exp (-inBeat * 30.0)) / fs;
                const double kick = std::sin (kickPh) * std::exp (-inBeat * 7.0) * 0.85;

                bassPh += 46.25 / fs;                                                     // F#1
                const double saw  = 2.0 * (bassPh - std::floor (bassPh)) - 1.0;
                const double bass = k16 == 0 ? 0.0 : saw * std::exp (-in16 * 28.0) * 0.32;

                const bool   off  = std::fmod (beat, 1.0) >= 0.5;
                const double hatT = (std::fmod (beat, 0.5)) * 60.0 / bpm;
                const double hat  = off ? (rnd.nextFloat() * 2.0 - 1.0) * std::exp (-hatT * 60.0) * 0.18 : 0.0;

                padPh1 += 369.99 / fs;
                padPh2 += 554.37 / fs;
                const double pad = 0.06 * std::sin (twoPi * padPh1) + 0.05 * std::sin (twoPi * padPh2);

                l[i] = (float) (kick + bass + hat + pad);
                r[i] = (float) (kick + bass - hat * 0.6 + pad * 0.8);
            }
           #if SHOT_ALIGN
            // ALIGN: rolling de psy (K b B B, o b a -3 dB). Entrada = BAIXO (saw com LP de envelope, sustain 0),
            // sidechain = KICK (queda até G1). A nota começa ~155° fora da cauda do kick: cancela no grave.
            {
                auto* bl = buf.getWritePointer (0); auto* br = buf.getWritePointer (1);
                auto* kl = buf.getWritePointer (2); auto* kr = buf.getWritePointer (3);
                const double t0 = t - block / fs;
                for (int i = 0; i < block; ++i)
                {
                    const double tt = t0 + i / fs, beatLen = 60.0 / bpm, s16 = beatLen / 4;
                    const double tb = tt - std::floor (tt / beatLen) * beatLen;
                    const double fk = 49.0 + 91.0 * std::exp (-tb * 90.0);
                    if (tb < 1.0 / fs) aKick = 0;
                    aKick += twoPi * fk / fs;
                    const double kick = std::sin (aKick) * std::min (1.0, tb / 0.001) * std::exp (-tb * 13.0) * 0.9;
                    const int slot = (int) (tb / s16);
                    double bass = 0;
                    if (slot > 0)
                    {
                        const double u = tb - slot * s16, gate = s16 * 0.86;
                        const double ph = 49.0 * u + 155.0 / 360.0;
                        const double saw = 2.0 * (ph - std::floor (ph + 0.5));
                        const double env = std::min (1.0, u / 0.002) * (u < gate ? std::exp (-u * 7) : std::exp (-gate * 7) * std::max (0.0, 1 - (u - gate) / 0.006));
                        const double fc = 120.0 + 260.0 * std::exp (-u * 35.0), gg = std::tan (juce::MathConstants<double>::pi * fc / fs), kq = 1.0 / 0.9;
                        const double a1 = 1.0 / (1.0 + gg * (gg + kq)), a2 = gg * a1, a3 = gg * a2;
                        const double v3 = saw - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
                        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
                        bass = v2 * env * (slot == 1 ? 0.62 * 0.708 : 0.62);
                    }
                    bl[i] = br[i] = (float) bass;
                    kl[i] = kr[i] = (float) kick;
                }
            }
           #endif
           #if SHOT_VOZ
            // ONKEY: voz sintética (vogal "a", formantes 700/1220/2600 Hz) cantando em Lá menor, cada nota desafinada
            // de 15 a 40 cents, vibrato de 5,5 Hz nas notas longas e um respiro (ruído) entre as frases
            {
                auto* vl = buf.getWritePointer (0); auto* vr = buf.getWritePointer (1);
                static const int mel[] = { 57, 60, 64, 62, 60, 59, 57, 64, 65, 64, 62, 60 };
                static const double desv[] = { 0.30, -0.25, 0.18, -0.35, 0.22, -0.40, 0.15, -0.28, 0.33, -0.20, 0.38, -0.30 };
                const double t0 = t - block / fs, passo = 0.42;
                for (int i = 0; i < block; ++i)
                {
                    const double tt = t0 + i / fs;
                    const int k = (int) std::floor (tt / passo);
                    const double u = tt - k * passo;
                    const bool longa = k % 4 == 3, pausa = k % 8 == 7;
                    const int nota = mel[k % 12];
                    double v = 0.0;
                    if (pausa) v = u < 0.12 ? (rnd.nextFloat() * 2.0 - 1.0) * 0.02 * std::sin (u / 0.12 * 3.14159) : 0.0;
                    else if (u < passo - 0.03)
                    {
                        const double vib = longa ? 0.3 * std::sin (twoPi * 5.5 * u) * std::min (1.0, u / 0.2) : 0.0;
                        const double f0 = 440.0 * std::pow (2.0, (nota + desv[k % 12] + vib - 69.0) / 12.0);
                        vozPh += f0 / fs; vozPh -= std::floor (vozPh);
                        auto pico = [] (double f, double fc, double bw) { const double x = (f - fc) / bw; return 1.0 / (1.0 + x * x); };
                        for (int h = 1; h * f0 < 5000.0; ++h)
                        {
                            const double fh = h * f0;
                            v += (0.05 + pico (fh, 700, 110) + 0.7 * pico (fh, 1220, 120) + 0.35 * pico (fh, 2600, 180)) / h * std::sin (twoPi * h * vozPh);
                        }
                        v *= 0.22 * std::min (1.0, u / 0.02) * std::min (1.0, (passo - 0.03 - u) / 0.03);
                    }
                    vl[i] = vr[i] = (float) v;
                }
            }
           #endif
           #if SHOT_GUITARRA
            // STOMPBOX: guitarra sintética (power chord de E/G/A em colcheias, cordas que decaem), no lugar do loop
            {
                auto* gl = buf.getWritePointer (0); auto* gr = buf.getWritePointer (1);
                static const double raiz[] = { 82.41, 82.41, 98.0, 110.0 };
                const double t0 = t - block / fs, passo = 60.0 / bpm / 2.0;
                for (int i = 0; i < block; ++i)
                {
                    const double tt = t0 + i / fs;
                    const int k = (int) std::floor (tt / passo);
                    const double u = tt - k * passo, f = raiz[(k / 4) % 4];
                    double v = 0.0;
                    for (double n : { 1.0, 1.5, 2.0 })
                        for (int h = 1; h <= 10; ++h)
                            v += std::sin (twoPi * f * n * h * u) / h * std::exp (-u * (3.0 + h * 1.5));
                    v *= 0.07 * std::min (1.0, u / 0.002);
                    gl[i] = gr[i] = (float) v;
                }
            }
           #endif
            head.ppq.store (head.ppq.load() + block * bpm / 60.0 / fs);
           #if SHOT_SYNTH
            // instrumento: silêncio na entrada; acorde de Lá menor com 9ª e um baixo, pressão subindo
            buf.clear();
            midi.clear();
           #if SHOT_BAIXO
            // BAREBASS (uma voz): baixo rolando em F1, 1/16 nos três contratempos, nota de 60% do passo. Depois de 1 s
            // de áudio as notas não soltam mais (cada 1/16 redispara): a foto pega uma nota tocando, não o intervalo
            {
                const bool solta = (double) blocks * block < fs * 1.0;
                const double passo = fs * 60.0 / bpm / 4.0;
                const double b0 = (double) blocks * block, b1 = b0 + block;
                for (int k = (int) std::floor (b0 / passo); k * passo < b1; ++k)
                {
                    const double on = k * passo, off = on + passo * 0.6;
                    if (k % 4 != 0 && on >= b0 && on < b1) midi.addEvent (juce::MidiMessage::noteOn (1, 29, (juce::uint8) 100), (int) (on - b0));
                    if (solta && k % 4 != 0 && off >= b0 && off < b1) midi.addEvent (juce::MidiMessage::noteOff (1, 29), (int) (off - b0));
                }
            }
            if (false)
           #else
            if (blocks == 0)
           #endif
                for (int note : { 45, 57, 60, 64, 67, 71 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (blocks % 8 == 0)
                midi.addEvent (juce::MidiMessage::channelPressureChange (1, juce::jmin (127, blocks)), 0);
            if (blocks % 8 == 4)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::jmin (127, blocks / 2)), 0);
            ++blocks;
           #endif
           #if SHOT_MAESTRO
            // MAESTRO: cada cópia toca o próprio GRUPO (stem) de um drop de psy (as extras na ordem de criação; a
            // principal é o AMBIENCE). O LEAD toca sempre, para a foto pegar o AMBIENCE cedendo.
            for (size_t k = 0; k <= extras.size(); ++k)
            {
                // KICK, BASS, DRUMS (clap), HATS (hat + ride), SYNTHS (lead + acid), VOCAL, FX, AMBIENCE (pad + atmos)
                static const int stem[8][2] = { { 0, -1 }, { 1, -1 }, { 3, -1 }, { 2, 4 }, { 5, 6 }, { 8, -1 }, { 7, -1 }, { 10, 9 } };
                const int st = k < extras.size() ? (int) k : 7;
                auto* bl = buf.getWritePointer (0); auto* br = buf.getWritePointer (1);
                const double t0 = t - block / fs;
                for (int i = 0; i < block; ++i)
                {
                    float v = 0.0f;
                    for (int tipo : stem[st]) if (tipo >= 0) v += mae[(size_t) tipo].amostra (t0 + i / fs, tipo, rnd);
                    bl[i] = v; br[i] = v * (st == 3 ? 0.8f : 1.0f);
                }
                if (k < extras.size()) extras[k]->processBlock (buf, midi);
            }
           #endif
           #if SHOT_MESA
            for (size_t k = 0; k < extras.size(); ++k)
            {
                juce::AudioBuffer<float> copy (buf);
                copy.applyGain (0.35f + 0.08f * (float) (k % 7));
                extras[k]->processBlock (copy, midi);
            }
           #endif
           #if SHOT_MAGOS
            if (auto* A = maestrolink::area())
                for (int r = 0; r < maestrolink::NR; ++r) A->vaga[r].batida.fetch_add (1);
           #endif
            proc.processBlock (buf, midi);
            juce::Thread::sleep (10);   // ~tempo real (512 amostras = 10,7 ms)
        }
    }

private:
    juce::AudioProcessor& proc;
    ShotPlayHead& head;
    double t = 0.0, kickPh = 0.0, bassPh = 0.0, padPh1 = 0.0, padPh2 = 0.0, aKick = 0.0, ic1 = 0.0, ic2 = 0.0, vozPh = 0.0;
   #if SHOT_MAESTRO
    // uma faixa de um drop de psytrance a 145 BPM (0 KICK, 1 BASS, 2 HAT, 3 CLAP, 4 RIDE, 5 LEAD, 6 ACID, 7 FX,
    // 8 VOX, 9 ATMOS, 10 PAD)
    struct FaixaMae
    {
        double ph = 0, ph2 = 0, ph3 = 0, z1 = 0, z2 = 0, lp = 0;
        float amostra (double tt, int tipo, juce::Random& rr)
        {
            const double pi2 = juce::MathConstants<double>::twoPi;
            const double beat = tt * bpm / 60.0, tb = (beat - std::floor (beat)) * 60.0 / bpm;
            const double six = beat * 4.0, t16 = (six - std::floor (six)) * 15.0 / bpm;
            const int k16 = (int) std::floor (six) % 4, bar = (int) std::floor (beat / 4.0);
            const double n = rr.nextFloat() * 2.0 - 1.0;
            switch (tipo)
            {
                case 0: if (tb < 1.0 / fs) ph = 0; ph += pi2 * (48.0 + 120.0 * std::exp (-tb * 60.0)) / fs; return (float) (std::sin (ph) * std::exp (-tb * 9.0) * 0.9);
                case 1: { if (k16 == 0) return 0.0f; ph += 49.0 / fs; const double saw = 2.0 * (ph - std::floor (ph)) - 1.0; lp += 0.06 * (saw - lp); return (float) (lp * std::exp (-t16 * 18.0) * 0.9); }
                case 2: { const double tt2 = std::fmod (beat, 1.0) >= 0.5 ? std::fmod (beat, 0.5) * 60.0 / bpm : 1.0; const double h = n - z1; z1 = n; return (float) (h * std::exp (-tt2 * 50.0) * 0.25); }
                case 3: { if ((int) std::floor (beat) % 2 != 1) return 0.0f; z1 += 0.3 * (n - z1); return (float) ((n - z1) * std::exp (-tb * 25.0) * 0.4); }
                case 4: { const double tt2 = std::fmod (beat, 0.25) * 60.0 / bpm; const double h = n - z1; z1 = n; return (float) (h * std::exp (-tt2 * 20.0) * 0.08); }
                case 5: { static const double notas[] = { 740, 880, 988, 880, 659, 740, 880, 1109 }; ph += notas[(int) std::floor (six) % 8] / fs;
                          double s = 0; for (int h = 1; h <= 5; ++h) s += std::sin (pi2 * ph * h) / h; return (float) (s * 0.22); }
                case 6: { ph += 98.0 / fs; const double saw = 2.0 * (ph - std::floor (ph)) - 1.0; const double fc = 300.0 + 900.0 * (0.5 + 0.5 * std::sin (pi2 * tt * 0.25));
                          const double g = std::tan (juce::MathConstants<double>::pi * fc / fs), kq = 0.3; const double v1 = (z1 + g * (saw - z2 - kq * z1)) / (1 + g * (g + kq));
                          const double v2 = z2 + g * v1; z1 = 2 * v1 - z1; z2 = 2 * v2 - z2; return (float) (v2 * (k16 == 0 ? 0.0 : 0.25)); }
                case 7: { const double u = std::fmod (beat, 16.0) / 16.0; lp += (0.02 + 0.5 * u) * (n - lp); return (float) (lp * u * 0.3); }
                case 8: { ph += 220.0 / fs; double s = 0; for (int h = 1; h <= 14; ++h) { const double f = 220.0 * h;
                          s += std::sin (pi2 * ph * h) * (std::exp (-std::pow ((f - 700) / 250, 2)) + 0.6 * std::exp (-std::pow ((f - 1220) / 300, 2))); }
                          return (float) (s * (bar % 4 == 2 ? 0.12 : 0.0)); }
                case 9: { lp += 0.01 * (n - lp); z1 += 0.05 * (n - z1); return (float) ((lp * 3.0 + (z1 - lp) * 0.6) * 0.15); }
                default: { ph += 370.0 / fs; ph2 += 554.0 / fs; ph3 += 740.0 / fs; double s = 0;
                           for (int h = 1; h <= 6; ++h) s += (std::sin (pi2 * ph * h) + std::sin (pi2 * ph2 * h) + std::sin (pi2 * ph3 * h)) / (h * 1.5);
                           return (float) (s * 0.09); }
            }
        }
    };
    std::vector<FaixaMae> mae = std::vector<FaixaMae> (11);
   #endif
    int blocks = 0;
};

void setParam (juce::AudioProcessor& p, const juce::String& pid, float value)
{
    for (auto* prm : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() == pid)
                rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

// ajustes para a foto ficar representativa
void applyLook (juce::AudioProcessor& p)
{
    const juce::String name (SHOT_PLUGIN);
    if (name == "CHAMA")   { setParam (p, "chama", 78.0f); setParam (p, "noise", 70.0f); setParam (p, "space", 70.0f); }
    if (name == "NAVALHA") { setParam (p, "drive", 12.0f); setParam (p, "ceiling", -4.0f); setParam (p, "knee", 10.0f); }
    if (name == "FITA")    { setParam (p, "drive", 55.0f); setParam (p, "wow", 45.0f); }
    if (name == "F1")      { setParam (p, "cutoff", 700.0f); setParam (p, "resonance", 70.0f); setParam (p, "envamt", 60.0f); setParam (p, "lfoamt", 30.0f); }
    if (name == "RINGER")  { setParam (p, "freq", 55.0f); setParam (p, "lfoamt", 40.0f); }
    if (name == "REPETECO") { setParam (p, "feedback", 70.0f); setParam (p, "mix", 45.0f); }
    if (name == "MIRAGEM") { setParam (p, "shimmer", 60.0f); setParam (p, "motion", 60.0f); setParam (p, "mix", 45.0f); }
    if (name == "PICOTE")
    {
        setParam (p, "swing", 20.0f); setParam (p, "pan02", -60.0f); setParam (p, "pan04", 60.0f);
        // a foto sai num instante qualquer do compasso: nenhum passo mudo, senão os medidores aparecem zerados
        const char* const fracos[] = { "vol02", "vol04", "vol06", "vol08", "vol10", "vol12", "vol14", "vol16" };
        const float niveis[] = { 35.0f, 55.0f, 35.0f, 70.0f, 35.0f, 55.0f, 35.0f, 80.0f };
        for (int i = 0; i < 8; ++i) setParam (p, fracos[i], niveis[i]);
    }
    if (name == "PRISMA")  { setParam (p, "macro", 55.0f); setParam (p, "drive1", 6.0f); }
    if (name == "TORNO")   { setParam (p, "squeeze", 40.0f); }
    if (name == "BARRAGEM") { setParam (p, "drive", 8.0f); }
    if (name == "CONSOLE") { setParam (p, "drive", 65.0f); }
    if (name == "SOCO")    { setParam (p, "attack", 60.0f); setParam (p, "sustain", -30.0f); }
    if (name == "LIXA")    { setParam (p, "thresh", -42.0f); setParam (p, "range", 10.0f); }
    if (name == "HORIZONTE") { setParam (p, "width", 140.0f); }
    if (name == "LIMAO")
    {
        // o sequenciador toca o padrão do preset (host tocando a 145); ácido e delay para a foto ter vida
        setParam (p, "acid", 35.0f); setParam (p, "dlon", 1.0f); setParam (p, "drive", 35.0f);
    }
    if (name == "QUIMERA")
    {
        // os 3 osciladores (analógico, modelado e wavetable), o filtro B e alguns efeitos ligados
        setParam (p, "o1tab", 23.0f); setParam (p, "o1pos", 35.0f); setParam (p, "o1uni", 4.0f);
        setParam (p, "o2on", 1.0f);   setParam (p, "o2tab", 5.0f);  setParam (p, "o2pos", 60.0f); setParam (p, "o2semi", 7.0f);
        setParam (p, "o3on", 1.0f);   setParam (p, "o3tab", 17.0f); setParam (p, "o3pos", 45.0f); setParam (p, "o3oct", 1.0f);
        setParam (p, "o3warp", 8.0f); setParam (p, "o3wamt", 30.0f);
        setParam (p, "facut", 1400.0f); setParam (p, "fares", 55.0f); setParam (p, "faenv", 40.0f);
        setParam (p, "fbon", 1.0f);
        setParam (p, "e2d", 900.0f); setParam (p, "e2s", 30.0f);
        setParam (p, "m01src", 4.0f); setParam (p, "m01dst", 2.0f);  setParam (p, "m01amt", 40.0f);   // LFO 1 → osc 1 posição
        setParam (p, "m02src", 9.0f); setParam (p, "m02dst", 19.0f); setParam (p, "m02amt", 35.0f);   // pressão → corte A
        setParam (p, "l1div", 10.0f);
        setParam (p, "hyon", 1.0f); setParam (p, "chon", 1.0f); setParam (p, "dlon", 1.0f); setParam (p, "rvon", 1.0f);
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    ShotPlayHead head;
    proc->setPlayHead (&head);
   #if SHOT_ALIGN
    // ALIGN: baixo (estéreo) + sidechain (estéreo), como o DAW liga quando escolhe a faixa do kick
    {
        auto layout = proc->getBusesLayout();
        if (layout.inputBuses.size() > 1) layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        const bool ok = proc->setBusesLayout (layout);
        if (auto* sc = proc->getBus (true, 1)) sc->enable (true);
        proc->setRateAndBufferSizeDetails (fs, block);
        std::printf ("ALIGN: layout %s, entradas %d, sidechain %s\n", ok ? "ok" : "recusado", proc->getTotalNumInputChannels(),
                     proc->getBus (true, 1) != nullptr && proc->getBus (true, 1)->isEnabled() ? "ligado" : "desligado");
    }
   #else
    proc->setPlayConfigDetails (SHOT_SYNTH ? 0 : 2, 2, fs, block);
   #endif
    proc->prepareToPlay (fs, block);
   #if SHOT_GUITARRA
    proc->setCurrentProgram (1);   // STOMPBOX: o board do esboço aprovado (BLUES CRUNCH, amp CITRUS)
   #endif
   #if SHOT_BAIXO
    proc->setCurrentProgram (1);   // BAREBASS abre no INIT (filtro aberto); a foto mostra o ROLLING
   #endif
    applyLook (*proc);
   #if SHOT_MESA
    {
        // o plugin principal é a CONSOLE (master); os canais e grupos são MESA CANAL abertos no mesmo programa
        struct Ch { const char* name; juce::uint32 col; int mode, grp; float pre, eq, gain, drive, hpf, fader, pan, thr; };
        const Ch chs[] = {
            { "KICK", 0xffe8473c, 0, 1, 2, 0, 3, 3, 30, -2.4f, 0, -16 },   { "CAIXA", 0xffff8a3d, 0, 1, 2, 0, 2, 2, 90, -1.5f, 0.1f, -14 },
            { "HIHAT", 0xfff2d24b, 0, 1, 0, 0, 0, 0, 300, -6, 0.3f, 10 },  { "OH L", 0xfff2a33a, 0, 1, 1, 0, 1, 1, 120, -4.5f, -0.7f, -20 },
            { "OH R", 0xfff2a33a, 0, 1, 1, 0, 1, 1, 120, -4.5f, 0.7f, -20 }, { "BAIXO", 0xff3d8bff, 0, 2, 2, 1, 4, 4, 35, -0.6f, 0, -18 },
            { "GTR L", 0xff3fbf6a, 0, 2, 2, 1, 2, 3, 110, -2.7f, -0.75f, -22 }, { "GTR R", 0xff3fbf6a, 0, 2, 2, 1, 2, 3, 110, -2.7f, 0.75f, -22 },
            { "TECLA", 0xff22d8ff, 0, 2, 0, 1, 0, 0, 60, -4.2f, -0.2f, 10 }, { "VOZ", 0xffc49bff, 0, 3, 1, 0, 3, 3, 90, 0.8f, 0, -24 },
            { "CORO", 0xffc49bff, 0, 3, 1, 0, 1, 1, 140, -5.5f, 0.2f, -20 }, { "FX", 0xff9aa0a8, 0, 0, 0, 0, 0, 0, 200, -9.0f, 0.4f, 10 },
            { "BATERIA", 0xffe8473c, 1, 1, 0, 0, 0, 0, 20, -1.2f, 0, -14 }, { "BASE", 0xff3d8bff, 1, 2, 0, 0, 0, 0, 20, -2.0f, 0, -12 },
            { "VOZES", 0xffc49bff, 1, 3, 0, 0, 0, 0, 20, -0.5f, 0, -16 } };
        juce::AudioProcessor::TrackProperties tp;
        tp.name = juce::String ("Master"); tp.colourARGB = 0xffd0d0d0u;
        proc->updateTrackProperties (tp);
        setParam (*proc, "mthr", -12); setParam (*proc, "mmkp", 3);
        for (auto& c : chs)
        {
            std::unique_ptr<juce::AudioProcessor> q (mesaCriaCanal());
            q->setPlayHead (&head);
            q->setPlayConfigDetails (2, 2, fs, block);
            q->prepareToPlay (fs, block);
            juce::AudioProcessor::TrackProperties t;
            // como no Ableton: "N-Nome" (o número vira a posição na console)
            t.name = juce::String ((int) extras.size() + 1) + "-" + juce::String (c.name); t.colourARGB = c.col;
            q->updateTrackProperties (t);
            setParam (*q, "mode", (float) c.mode); setParam (*q, "grp", (float) c.grp); setParam (*q, "pre", c.pre);
            setParam (*q, "eqm", c.eq); setParam (*q, "gain", c.gain); setParam (*q, "drive", c.drive); setParam (*q, "hpf", c.hpf);
            setParam (*q, "fader", c.fader); setParam (*q, "pan", c.pan); setParam (*q, "thr", c.thr);
            setParam (*q, "hfg", (float) ((int) c.col % 7 - 3)); setParam (*q, "lmfg", (float) ((int) (c.col >> 8) % 9 - 4));
            if (c.mode == 1) { setParam (*q, "rat", 4); setParam (*q, "cmix", 70); }
            // modelos variados de compressor e de EQ, para a foto mostrar todos
            const int k = (int) extras.size();
            const int dyns[] = { 2, 0, 0, 3, 3, 3, 0, 0, 0, 3, 2, 0, 1, 1, 1 };
            setParam (*q, "dynm", (float) dyns[k % 15]);
            if (k == 2 || k == 6 || k == 7) setParam (*q, "eqm", 2);
            if (k == 0) { setParam (*q, "lfbell", 1); setParam (*q, "lpf", 9000); setParam (*q, "rat", 20); }
            extras.push_back (std::move (q));
        }
    }
   #endif

   #if SHOT_MAESTRO
    // MAESTRO: 7 cópias a mais no mesmo programa (os grupos do drop); a principal é o AMBIENCE
    {
        const char* nomes[] = { "KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "VOCAL", "FX" };
        for (auto* nm : nomes)
        {
            std::unique_ptr<juce::AudioProcessor> q (createPluginFilter());
            q->setPlayHead (&head);
            q->setPlayConfigDetails (2, 2, fs, block);
            q->prepareToPlay (fs, block);
            juce::AudioProcessor::TrackProperties tp;
            tp.name = juce::String (nm);
            q->updateTrackProperties (tp);
            extras.push_back (std::move (q));
        }
        juce::AudioProcessor::TrackProperties tp;
        tp.name = juce::String ("AMBIENCE");
        proc->updateTrackProperties (tp);
    }
   #endif
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditorAndMakeActive());
    editor->setSize (SHOT_W, SHOT_H);   // 200%

    AudioThread audio (*proc, head);
    audio.startThread();
   #if SHOT_MAGOS
    // MAESTRO LINK simulado: 8 grupos de um drop de psy (o BASS embola em 240 Hz, o SYNTHS 2 dB acima do STYLE);
    // LISTEN 6 s (foto), CAST e a foto principal no fim
    if (auto* A = maestrolink::area())
    {
        static const float alvo[8] = { 0.0f, -1.0f, -9.0f, -12.0f, -4.0f, -12.0f, -6.0f, -10.0f };
        static const float nivel[8] = { -12.0f, -13.0f, -21.0f, -24.5f, -14.0f, -24.0f, -18.5f, -22.0f };
        static const float centro[8] = { 60.0f, 110.0f, 900.0f, 9000.0f, 2500.0f, 600.0f, 1500.0f, 4000.0f };
        A->style = 0; A->travado = 1; A->topo = 0;
        std::snprintf (A->nomeStyle, sizeof (A->nomeStyle), "PSYTRANCE");
        for (int r = 0; r < 8; ++r)
        {
            A->alvoRole[r] = alvo[r];
            auto& V = A->vaga[r];
            V.role = r; V.tocando = 1; V.focus = 0; V.nivelDb = nivel[r]; V.trimDb = 0.0f;
            for (int b = 0; b < maestrolink::NB; ++b)
            {
                const double f = 40.0 * std::pow (2.0, b / 3.0), o = std::log2 (f / centro[r]);
                double db = nivel[r] - 6.0 * o * o;
                if (r == 1 && std::abs (std::log2 (f / 240.0)) < 0.5) db += 9.0;   // BASS embolando em 240 Hz
                V.espectro[b] = (float) std::pow (10.0, db / 10.0);
            }
            V.batida = 1;
        }
    }
   #endif
   #if SHOT_MAESTRO
    // LEARN (foto), LOCK (foto) e FOCUS em SYNTHS + DRUMS: a foto principal é o AMBIENCE cedendo para os dois;
    // depois a janela do SYNTHS
    {
        auto fotoDe = [&] (juce::AudioProcessorEditor& ed, const juce::String& nome)
        {
            ed.setSize (SHOT_W, SHOT_H);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            const auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
            const auto f = outDir.getChildFile (nome + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat png;
            std::printf ("%s: %s\n", nome.toRawUTF8(), os.openedOk() && png.writeImageToStream (img, os) ? "ok" : "falhou");
        };
        auto& R = maestro::Registry::get();
        R.amount = 0.75f;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);
        R.learn (juce::Time::getMillisecondCounterHiRes() / 1000.0 - 14.0);   // o relógio da tela mostra 0:18
        juce::MessageManager::getInstance()->runDispatchLoopUntil (4000);
        fotoDe (*editor, "MAESTRO-LEARN");
        R.lock();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);
        fotoDe (*editor, "MAESTRO-LOCK");
        setParam (*extras[4], "focus", 1.0f);   // SYNTHS
        setParam (*extras[2], "focus", 1.0f);   // DRUMS
    }
   #endif
   #if SHOT_MAGOS
    if (auto* mg = dynamic_cast<MagosProcessor*> (proc.get()))
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);
        mg->motor.listen();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (7000);
        editor->setSize (SHOT_W, SHOT_H);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        const auto imgL = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto fL = outDir.getChildFile ("MAGOS-LISTEN.png");
        fL.deleteFile();
        juce::FileOutputStream osL (fL);
        juce::PNGImageFormat pngL;
        std::printf ("MAGOS-LISTEN: %s\n", osL.openedOk() && pngL.writeImageToStream (imgL, osL) ? "ok" : "falhou");
        mg->motor.lancaCast();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (9000);
    }
   #endif
   #if SHOT_ALIGN
    // desligado (ouvindo e aprendendo) e depois ligado
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (3500);
        editor->setSize (SHOT_W, SHOT_H);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        const auto imgOff = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto fOff = outDir.getChildFile ("ALIGN-OFF.png");
        fOff.deleteFile();
        juce::FileOutputStream osOff (fOff);
        juce::PNGImageFormat pngOff;
        std::printf ("ALIGN-OFF: %s\n", osOff.openedOk() && pngOff.writeImageToStream (imgOff, osOff) ? "ok" : "falhou");
        setParam (*proc, "align", 1.0f);
    }
   #endif
   #if SHOT_VOZ
    // ONKEY: o tom trava depois de ~4 s de voz; espera mais para o gráfico encher
    juce::MessageManager::getInstance()->runDispatchLoopUntil (4000);
   #endif
    juce::MessageManager::getInstance()->runDispatchLoopUntil (4500);
    // o editor pode ter voltado ao tamanho salvo depois de abrir: de novo em 200% para a foto
    editor->setSize (SHOT_W, SHOT_H);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
   #if SHOT_VOZ
    // ONKEY com o AJUSTE FINO aberto
    {
        juce::MemoryBlock mb;
        proc->getStateInformation (mb);
        if (auto x = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
        {
            x->setAttribute ("fino", 1);
            juce::MemoryBlock m2;
            juce::AudioProcessor::copyXmlToBinary (*x, m2);
            proc->setStateInformation (m2.getData(), (int) m2.getSize());
        }
        editor->setSize (SHOT_W, SHOT_H);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (800);
        const auto imgF = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto fF = outDir.getChildFile ("ONKEY-FINO.png");
        fF.deleteFile();
        juce::FileOutputStream osF (fF);
        juce::PNGImageFormat pngF;
        std::printf ("ONKEY-FINO: %s\n", osF.openedOk() && pngF.writeImageToStream (imgF, osF) ? "ok" : "falhou");
    }
   #endif
   #if SHOT_MESA
    // a mesma console com zoom de 140% e as linhas INPUT e DYN recolhidas: a lateral e a seção central não podem mudar
    {
        juce::MemoryBlock mb;
        proc->getStateInformation (mb);
        if (auto x = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
        {
            x->setAttribute ("uizoom", 1.4); x->setAttribute ("uirec", 6); x->setAttribute ("uigeral", 0);
            juce::MemoryBlock m2;
            juce::AudioProcessor::copyXmlToBinary (*x, m2);
            proc->setStateInformation (m2.getData(), (int) m2.getSize());
        }
        editor->setSize (SHOT_W, SHOT_H);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (800);
        const auto img3 = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto f3 = outDir.getChildFile ("MESA-ZOOM.png");
        f3.deleteFile();
        juce::FileOutputStream os3 (f3);
        juce::PNGImageFormat png3;
        std::printf ("MESA-ZOOM: %s\n", os3.openedOk() && png3.writeImageToStream (img3, os3) ? "ok" : "falhou");
    }
    // a janela do MESA CANAL (o canal VOZ), a 200%
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed2 (extras[9]->createEditorAndMakeActive());
        ed2->setSize (800, 2000);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1200);
        ed2->setSize (800, 2000);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        const auto img2 = ed2->createComponentSnapshot (ed2->getLocalBounds(), true, 1.0f);
        const auto f2 = outDir.getChildFile ("MESA-CANAL.png");
        f2.deleteFile();
        juce::FileOutputStream os2 (f2);
        juce::PNGImageFormat png2;
        std::printf ("MESA-CANAL: %s\n", os2.openedOk() && png2.writeImageToStream (img2, os2) ? "ok" : "falhou");
    }
   #endif
   #if SHOT_MAESTRO
    // a janela do SYNTHS (em FOCUS, junto com o DRUMS), a 200%
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed2 (extras[4]->createEditorAndMakeActive());
        ed2->setSize (SHOT_W, SHOT_H);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1200);
        ed2->setSize (SHOT_W, SHOT_H);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        const auto img2 = ed2->createComponentSnapshot (ed2->getLocalBounds(), true, 1.0f);
        const auto f2 = outDir.getChildFile ("MAESTRO-FOCUS.png");
        f2.deleteFile();
        juce::FileOutputStream os2 (f2);
        juce::PNGImageFormat png2;
        std::printf ("MAESTRO-FOCUS: %s\n", os2.openedOk() && png2.writeImageToStream (img2, os2) ? "ok" : "falhou");
    }
   #endif
   #if SHOT_GUITARRA
    // STOMPBOX: fotos de conferência dos textos: todos os pedais, cada amp, amp desligado + bypass e o TUNER
    if (auto* sb = dynamic_cast<StompboxProcessor*> (proc.get()))
    {
        auto foto = [&] (const juce::String& nome, juce::Rectangle<int> recorte)
        {
            editor->setSize (SHOT_W, SHOT_H);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
            const auto img = editor->createComponentSnapshot (recorte.isEmpty() ? editor->getLocalBounds() : recorte, true, 1.0f);
            const auto f = outDir.getChildFile (nome + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat png;
            std::printf ("%s: %s\n", nome.toRawUTF8(), os.openedOk() && png.writeImageToStream (img, os) ? "ok" : "falhou");
        };
        auto monta = [&] (std::initializer_list<int> tipos)
        {
            int s = 0;
            for (int t : tipos)
            {
                StompboxProcessor::SlotDados d; d.tipo = t; d.on = true;
                const auto& inf = stomp::infoPedal (t);
                for (int k = 0; k < 4; ++k) d.k[k] = k < inf.nk ? inf.def[k] : 5.0f;
                sb->grava (s++, d);
            }
            for (; s < stomp::numSlots; ++s) sb->grava (s, StompboxProcessor::SlotDados());
            sb->rolagem = 0;
        };
        const int sw = SHOT_W, sh = SHOT_H;
        monta ({ 1, 2, 3, 4, 5, 6 });
        foto ("STOMPBOX-PEDAIS-1", {});
        setParam (*proc, "sync", 1.0f);   // o ECHO mostra a divisão do SYNC no rótulo do TIME
        monta ({ 7, 8, 9, 10, 11, 3 });
        { auto d = sb->le (3); d.sw = 1; sb->grava (3, d); auto e = sb->le (1); e.on = false; sb->grava (1, e); }   // HALO no PLATE, TIDE em BYPASS
        foto ("STOMPBOX-PEDAIS-2", {});
        setParam (*proc, "sync", 0.0f);
        for (int a = 0; a < stomp::numAmps; ++a)
        {
            setParam (*proc, "amp", (float) a); setParam (*proc, "canal", (float) (a % 2));
            foto ("STOMPBOX-AMP-" + juce::String (a + 1), { 0, sh * 600 / 900, sw, sh - sh * 600 / 900 });
        }
        setParam (*proc, "ampon", 0.0f);
        foto ("STOMPBOX-AMP-OFF", { 0, sh * 600 / 900, sw, sh - sh * 600 / 900 });
        setParam (*proc, "ampon", 1.0f);
        sb->afinador = true;
        foto ("STOMPBOX-TUNER", {});
        sb->afinador = false;
    }
   #endif
    audio.stopThread (3000);

    const auto file = outDir.getChildFile (juce::String (SHOT_PLUGIN) + ".png");
    file.deleteFile();
    bool ok = false;
    {
        juce::FileOutputStream os (file);
        juce::PNGImageFormat png;
        ok = os.openedOk() && png.writeImageToStream (image, os);
    }

    editor.reset();
    extras.clear();
    proc->releaseResources();
    proc.reset();
    std::printf ("%s: %s\n", SHOT_PLUGIN, ok ? "ok" : "falhou");
    return ok ? 0 : 1;
}
