#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace espiral
{
constexpr int numVoices = 4;    // vozes defasadas de 1/4 de volta: uma entra no grave enquanto outra some no agudo
constexpr int maxStages = 12;

enum Mode { modePhaser = 0, modeFlanger };
enum Direction { dirUp = 0, dirDown, dirPingPong };

inline int stageCount (int idx)
{
    static const int counts[] = { 4, 6, 8, 12 };
    return counts[juce::jlimit (0, 3, idx)];
}

// duração de uma volta inteira da espiral, em tempos (semínimas)
inline double cycleBeats (int idx)
{
    static const double beats[] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
    return beats[juce::jlimit (0, 6, idx)];
}

inline float rangeOctaves (float range01) { return 1.0f + 9.0f * range01; }
inline float toneHz (float tone01)        { return 1000.0f * std::pow (20.0f, tone01); }

// janela de cada voz ao longo da volta: nasce e morre em zero, então a troca grave↔agudo não estala
inline float voiceWindow (float p) { return (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p)) * (2.0f / (float) numVoices); }

inline float frac01 (float x) { return x - std::floor (x); }

struct Settings
{
    double bpm = 145.0, ppq = 0.0;
    bool   playing = false;
    int    cycle = 3, direction = dirUp, mode = modePhaser, stages = 2;
    float  range = 0.6f, centre = 800.0f, stereo = 0.25f, resonance = 0.55f, drive = 0.15f, tone = 0.85f;
    bool   freeze = false, twice = false;
    bool   waiting = false;   // host parado: a espiral fica na posição do cursor e anda no play
};

class SpiralEngine
{
public:
    // ---- telemetria para a interface (canal esquerdo) ----
    std::atomic<float> phase { 0.0f }, lowHz { 100.0f }, highHz { 8000.0f }, rate { 44100.0f };
    std::array<std::atomic<float>, numVoices> voiceHz {}, voiceGain {};
    std::atomic<int> turn { 0 };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        rate.store ((float) fs);

        int len = 1;
        while (len < (int) (fs * 0.06) + 8) len <<= 1;
        mask = len - 1;
        for (auto& d : delay) d.assign ((size_t) len, 0.0f);
        writePos = 0;

        for (auto& ch : stage) for (auto& v : ch) v.fill (0.0f);
        for (auto& ch : fbPhaser) ch.fill (0.0f);
        toneLp.fill (0.0f);

        for (auto* sm : { &smLow, &smHigh, &smFbP, &smFbF, &smDrive, &smTone, &smMode, &smStereo })
            sm->reset (fs, 0.06);
        primed = false;
        wasPlaying = false;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        // ---- faixa da varredura em log2(Hz) ----
        const float oct   = rangeOctaves (s.range);
        const float cLog  = std::log2 (s.centre);
        const float minLg = std::log2 (20.0f), maxLg = std::log2 ((float) juce::jmin (18000.0, fs * 0.45));
        float lo = cLog - oct * 0.5f, hi = cLog + oct * 0.5f;
        if (lo < minLg) { hi = juce::jmin (maxLg, hi + (minLg - lo)); lo = minLg; }
        if (hi > maxLg) { lo = juce::jmax (minLg, lo - (hi - maxLg)); hi = maxLg; }

        const float fbP  = s.resonance * 0.9f;
        const float fbF  = s.resonance * 0.85f;
        const float mode = s.mode == modeFlanger ? 1.0f : 0.0f;
        const float st   = s.stereo * 0.5f / (float) numVoices;   // máximo: meia distância entre vozes

        if (! primed)
        {
            smLow.setCurrentAndTargetValue (lo);      smHigh.setCurrentAndTargetValue (hi);
            smFbP.setCurrentAndTargetValue (fbP);     smFbF.setCurrentAndTargetValue (fbF);
            smDrive.setCurrentAndTargetValue (s.drive); smTone.setCurrentAndTargetValue (s.tone);
            smMode.setCurrentAndTargetValue (mode);   smStereo.setCurrentAndTargetValue (st);
        }
        smLow.setTargetValue (lo);      smHigh.setTargetValue (hi);
        smFbP.setTargetValue (fbP);     smFbF.setTargetValue (fbF);
        smDrive.setTargetValue (s.drive); smTone.setTargetValue (s.tone);
        smMode.setTargetValue (mode);   smStereo.setTargetValue (st);

        // ---- relógio: a volta segue a régua do host; desvio (loop, CICLO novo) é corrigido sem salto ----
        const double beats   = cycleBeats (s.cycle);
        const double hostPos = s.ppq / beats;
        if (! primed || (s.playing && ! wasPlaying) || s.waiting)
        {
            pos = hostPos;       // play (ou parado): alinha na régua
            offset = 0.0;
        }
        else if (beats != lastBeats || s.freeze || s.twice)
            offset = pos - hostPos;   // segue de onde está; ao soltar, continua dali
        lastBeats  = beats;
        wasPlaying = s.playing;
        primed     = true;

        double speed = s.freeze || s.waiting ? 0.0 : (s.twice ? 2.0 : 1.0);
        if (! s.freeze && ! s.twice && ! s.waiting)
        {
            double err = hostPos + offset - pos;
            err -= std::round (err);                         // uma volta a mais ou a menos é a mesma posição
            speed += juce::jlimit (-0.5, 0.5, err * 4.0);    // acelera/freia até 50% para recuperar
        }
        const double inc = s.bpm / 60.0 / fs / beats * speed;

        const int   nst   = stageCount (s.stages);
        const float twoPi = juce::MathConstants<float>::twoPi;
        float phi = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            pos += inc;
            const double c = std::floor (pos);
            const float  f = (float) (pos - c);
            phi = s.direction == dirUp   ? f
                : s.direction == dirDown ? 1.0f - f
                : (((int64_t) c & 1) ? 1.0f - f : f);

            const float lg = smLow.getNextValue(), hg = smHigh.getNextValue();
            const float fp = smFbP.getNextValue(), ff = smFbF.getNextValue();
            const float dr = smDrive.getNextValue(), tn = smTone.getNextValue();
            const float md = smMode.getNextValue(), so = smStereo.getNextValue();
            const float g  = 1.0f + 5.0f * dr;
            const float lpC = 1.0f - std::exp (-twoPi * toneHz (tn) / (float) fs);

            const float in[2] = { inL[i], inR[i] };
            float out[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float x = in[ch];
                float wetP = 0.0f, wetF = 0.0f;
                for (int v = 0; v < numVoices; ++v)
                {
                    const float p  = frac01 (phi + (float) v / (float) numVoices + (ch == 1 ? so : 0.0f));
                    const float w  = voiceWindow (p);
                    const float hz = std::exp2 (lg + (hg - lg) * p);

                    // phaser: cadeia de passa-tudo de 1ª ordem, com realimentação por voz
                    const float t = std::tan (juce::MathConstants<float>::pi * hz / (float) fs);
                    const float a = (t - 1.0f) / (t + 1.0f);
                    float u = x + std::tanh (fp * fbPhaser[(size_t) ch][(size_t) v]);
                    auto& sv = stage[(size_t) ch][(size_t) v];
                    for (int k = 0; k < nst; ++k)
                    {
                        const float y = a * u + sv[(size_t) k];
                        sv[(size_t) k] = u - a * y;
                        u = y;
                    }
                    fbPhaser[(size_t) ch][(size_t) v] = u;
                    wetP += w * u;

                    // flanger: o primeiro vale do pente fica em hz (atraso = 1/2hz)
                    const float d = juce::jlimit (3.0f, (float) (mask - 4), (float) fs / (2.0f * hz));
                    wetF += w * readDelay (ch, d);
                }

                delay[(size_t) ch][(size_t) writePos] = x + std::tanh (ff * wetF);

                float wet = wetP + (wetF - wetP) * md;
                wet = std::tanh (g * wet) / g;                  // DRIVE: sinal baixo passa igual, pico satura
                toneLp[(size_t) ch] += lpC * (wet - toneLp[(size_t) ch]);
                out[ch] = 0.5f * (x + toneLp[(size_t) ch]);
            }
            outL[i] = out[0];
            outR[i] = out[1];
            writePos = (writePos + 1) & mask;
        }

        // estágios fora de uso começam do zero quando voltarem
        for (auto& ch : stage) for (auto& v : ch) for (int k = nst; k < maxStages; ++k) v[(size_t) k] = 0.0f;

        // ---- telemetria ----
        const float lg = smLow.getCurrentValue(), hg = smHigh.getCurrentValue();
        phase.store (phi);
        lowHz.store (std::exp2 (lg));
        highHz.store (std::exp2 (hg));
        turn.store ((int) std::floor (pos));
        for (int v = 0; v < numVoices; ++v)
        {
            const float p = frac01 (phi + (float) v / (float) numVoices);
            voiceHz[(size_t) v].store (std::exp2 (lg + (hg - lg) * p));
            voiceGain[(size_t) v].store (voiceWindow (p));
        }
    }

private:
    // leitura fracionária (Hermite de 4 pontos) d amostras atrás
    float readDelay (int ch, float d) const
    {
        const auto& b = delay[(size_t) ch];
        const float rp = (float) writePos - d;
        const int   i1 = (int) std::floor (rp);
        const float t  = rp - (float) i1;
        const float y0 = b[(size_t) ((i1 - 1) & mask)], y1 = b[(size_t) (i1 & mask)];
        const float y2 = b[(size_t) ((i1 + 1) & mask)], y3 = b[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    double fs = 44100.0;
    double pos = 0.0, offset = 0.0, lastBeats = 0.0;
    bool   primed = false, wasPlaying = false;

    std::array<std::array<std::array<float, maxStages>, numVoices>, 2> stage {};
    std::array<std::array<float, numVoices>, 2> fbPhaser {};
    std::array<std::vector<float>, 2> delay;
    std::array<float, 2> toneLp {};
    int writePos = 0, mask = 0;

    juce::SmoothedValue<float> smLow, smHigh, smFbP, smFbF, smDrive, smTone, smMode, smStereo;
};
} // namespace espiral
