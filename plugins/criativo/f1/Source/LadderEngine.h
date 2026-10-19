#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace f1
{
// divisões do LFO no grid do host (em tempos de semínima)
inline constexpr int numDivs = 14;
inline const char* const divNames[numDivs] = { "1/32", "1/16T", "1/16", "1/8T", "3/32", "1/8", "1/4T",
                                               "3/16", "1/4", "3/8", "1/2", "1 comp", "2 comp", "4 comp" };
inline constexpr double divBeats[numDivs] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0,
                                              0.75, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };

inline constexpr float minHz = 20.0f;
inline float maxHz (double fs) { return (float) juce::jmin (20000.0, fs * 0.45); }

// ganho de realimentação: 4 polos auto-oscila perto de 4; 2 polos só canta
inline float feedbackK (float res01, bool fourPoles) { return fourPoles ? 4.1f * res01 : 6.0f * res01; }
inline float makeup (float k, bool fourPoles)        { return 1.0f + k * (fourPoles ? 0.5f : 0.6f); }

struct Settings
{
    double bpm = 145.0, ppq = 0.0;
    bool   waiting = false;                  // host parado: o LFO fica na posição do cursor
    float  cutoff = 1200.0f, resonance = 0.35f, drive = 0.0f;   // drive em dB
    bool   fourPoles = true, lowRange = false;
    float  envAmt = 0.0f, smooth = 0.25f, sens = -18.0f;        // envAmt −1..1; sens em dBFS
    int    lfoDiv = 5;
    float  lfoAmt = 0.0f;
    bool   open = false, smoke = false;
};

// ---------------- FUMAÇA: reverb curto e escuro (4 pentes + 2 passa-tudo por canal) ----------------
class Smoke
{
public:
    void prepare (double fs)
    {
        const double k = fs / 44100.0;
        static constexpr int combLen[4] = { 1116, 1188, 1277, 1356 };
        static constexpr int apLen[2]   = { 556, 441 };
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int c = 0; c < 4; ++c)
            {
                combs[ch][c].assign ((size_t) juce::jmax (8, (int) ((combLen[c] + ch * 23) * k)), 0.0f);
                combPos[ch][c] = 0;
                combLp[ch][c] = 0.0f;
            }
            for (int a = 0; a < 2; ++a)
            {
                aps[ch][a].assign ((size_t) juce::jmax (8, (int) ((apLen[a] + ch * 23) * k)), 0.0f);
                apPos[ch][a] = 0;
            }
            tone[ch] = 0.0f;
        }
        toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2800.0f / (float) fs);
    }

    float process (int ch, float x)
    {
        float acc = 0.0f;
        for (int c = 0; c < 4; ++c)
        {
            auto& buf = combs[ch][c];
            auto& p   = combPos[ch][c];
            const float y = buf[(size_t) p];
            combLp[ch][c] = y * 0.55f + combLp[ch][c] * 0.45f;   // cada volta mais escura
            buf[(size_t) p] = x + combLp[ch][c] * 0.8f;
            if (++p >= (int) buf.size()) p = 0;
            acc += y;
        }
        acc *= 0.25f;
        for (int a = 0; a < 2; ++a)
        {
            auto& buf = aps[ch][a];
            auto& p   = apPos[ch][a];
            const float b = buf[(size_t) p];
            buf[(size_t) p] = acc + b * 0.5f;
            acc = b - acc * 0.5f;
            if (++p >= (int) buf.size()) p = 0;
        }
        tone[ch] += (acc - tone[ch]) * toneCoef;
        return tone[ch];
    }

private:
    std::vector<float> combs[2][4], aps[2][2];
    int combPos[2][4] {}, apPos[2][2] {};
    float combLp[2][4] {}, tone[2] {}, toneCoef = 0.3f;
};

// ---------------- filtro ladder de 4 degraus (ZDF, saturação na entrada da realimentação) ----------------
class LadderEngine
{
public:
    // ---- telemetria para a interface ----
    std::atomic<float> cutNow { 1200.0f }, envNow { 0.0f }, lfoNow { 0.0f }, smokeNow { 0.0f }, levelNow { 0.0f };
    std::atomic<float> rate { 44100.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        rate.store ((float) fs);
        for (auto& ch : st) ch.fill (0.0f);
        follower = 0.0f;
        lgSm = std::log2 (1200.0f);
        primed = false;
        openSm.reset (fs, 0.35);    // ABRIR sobe e desce como quem gira o knob rápido
        smokeSm.reset (fs, 0.25);
        resSm.reset (fs, 0.03);
        driveSm.reset (fs, 0.03);
        smoke.prepare (fs);
        cutCoef = 1.0f - std::exp (-1.0f / (0.0015f * (float) fs));
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        const float top = maxHz (fs);
        if (! primed)
        {
            openSm.setCurrentAndTargetValue (s.open ? 1.0f : 0.0f);
            smokeSm.setCurrentAndTargetValue (s.smoke ? 1.0f : 0.0f);
            resSm.setCurrentAndTargetValue (s.resonance);
            driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (s.drive));
            lgSm = std::log2 (juce::jlimit (minHz, top, s.cutoff * (s.lowRange ? 0.25f : 1.0f)));
            primed = true;
        }
        openSm.setTargetValue (s.open ? 1.0f : 0.0f);
        smokeSm.setTargetValue (s.smoke ? 1.0f : 0.0f);
        resSm.setTargetValue (s.resonance);
        driveSm.setTargetValue (juce::Decibels::decibelsToGain (s.drive));

        // envelope: ataque e soltura pela SUAVIZAÇÃO
        const float atk = 1.0f - std::exp (-1.0f / ((0.001f + 0.03f * s.smooth) * (float) fs));
        const float rel = 1.0f - std::exp (-1.0f / ((0.02f + 0.6f * s.smooth) * (float) fs));
        const float floorDb = s.sens - 36.0f;

        const double beats   = divBeats[juce::jlimit (0, numDivs - 1, s.lfoDiv)];
        const double ppqStep = s.waiting ? 0.0 : s.bpm / 60.0 / fs;
        const float  baseLg  = std::log2 (juce::jmax (1.0f, s.cutoff * (s.lowRange ? 0.25f : 1.0f)));
        const float  topLg   = std::log2 (top), botLg = std::log2 (minHz);
        const float  twoPi   = juce::MathConstants<float>::twoPi;

        float env01 = 0.0f, lfo = 0.0f, fc = 1000.0f, peak = 0.0f, sm = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = juce::jmax (std::abs (inL[i]), std::abs (inR[i]));
            follower += (a - follower) * (a > follower ? atk : rel);
            env01 = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (follower, -100.0f) - floorDb) / 36.0f);

            const double ph = (s.ppq + ppqStep * i) / beats;
            lfo = std::sin (twoPi * (float) (ph - std::floor (ph)));

            float lg = baseLg + s.envAmt * env01 * 4.0f + s.lfoAmt * lfo * 2.0f;
            const float op = openSm.getNextValue();
            lg += (topLg - lg) * op;
            lg = juce::jlimit (botLg, topLg, lg);
            lgSm += (lg - lgSm) * cutCoef;
            fc = std::exp2 (lgSm);

            const float g  = std::tan (juce::MathConstants<float>::pi * fc / (float) fs);
            const float G  = g / (1.0f + g);
            const float k  = feedbackK (resSm.getNextValue(), s.fourPoles);
            const float mk = makeup (k, s.fourPoles);
            const float dg = driveSm.getNextValue();
            const float post = 1.0f / std::sqrt (dg);
            sm = smokeSm.getNextValue();

            const float in[2] = { inL[i], inR[i] };
            float out[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& z = st[(size_t) ch];
                const float x = in[ch] * dg;
                // y = G·u + S por degrau; resolve a realimentação sem atraso de uma amostra
                const float S1 = z[0] / (1.0f + g), S2 = z[1] / (1.0f + g), S3 = z[2] / (1.0f + g), S4 = z[3] / (1.0f + g);
                float fb;
                if (s.fourPoles)
                {
                    const float G2 = G * G, G4 = G2 * G2;
                    fb = (G4 * x + G2 * G * S1 + G2 * S2 + G * S3 + S4) / (1.0f + k * G4);
                }
                else
                    fb = (G * G * x + G * S1 + S2) / (1.0f + k * G * G);

                float u = std::tanh (x - k * fb);
                float y2 = 0.0f;
                for (int d = 0; d < 4; ++d)
                {
                    const float v = G * (u - z[(size_t) d]);
                    const float y = v + z[(size_t) d];
                    z[(size_t) d] = y + v;
                    u = y;
                    if (d == 1) y2 = y;
                }
                float y = (s.fourPoles ? u : y2) * mk * post;

                // FUMAÇA: o filtrado embaça num reverb curto e escuro
                const float r = smoke.process (ch, y * sm);
                y = y * (1.0f - 0.3f * sm) + r * 0.9f;
                out[ch] = y;
                peak = juce::jmax (peak, std::abs (y));
            }
            outL[i] = out[0];
            outR[i] = out[1];
        }

        cutNow.store (fc);
        envNow.store (env01);
        lfoNow.store (lfo);
        smokeNow.store (sm);
        levelNow.store (juce::jmax (levelNow.load(), peak));
    }

private:
    double fs = 44100.0;
    std::array<std::array<float, 4>, 2> st {};
    float follower = 0.0f, lgSm = 10.0f, cutCoef = 0.01f;
    bool  primed = false;
    juce::SmoothedValue<float> openSm, smokeSm, resSm, driveSm;
    Smoke smoke;
};
} // namespace f1
