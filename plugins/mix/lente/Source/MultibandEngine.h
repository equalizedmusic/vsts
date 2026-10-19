#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

namespace lente
{
constexpr int numBands  = 4;
constexpr int fftOrder  = 11;
constexpr int fftSize   = 1 << fftOrder;   // 2048 pontos no espectro
constexpr int scopeSize = 4096;            // anel com a saída (mono), lido pela interface

// ---- compressor: joelho macio de 6 dB; devolve a redução em dB (≥ 0) ----
constexpr float kneeDb = 6.0f;

inline float reductionDb (float levelDb, float thrDb, float ratio)
{
    const float over  = levelDb - thrDb;
    const float slope = 1.0f - 1.0f / juce::jmax (1.0f, ratio);
    if (over <= -0.5f * kneeDb) return 0.0f;
    if (over >= 0.5f * kneeDb)  return over * slope;
    const float x = over + 0.5f * kneeDb;
    return slope * x * x / (2.0f * kneeDb);
}

// ---- MACRO 0–1: mais compressão e mais drive em todas as bandas juntas ----
inline float macroThr (float thrDb, float m)   { return thrDb - 12.0f * m; }
inline float macroRatio (float ratio, float m) { return juce::jmin (30.0f, ratio + 3.0f * m); }
inline float macroDrive (float driveDb, float m) { return driveDb + 12.0f * m; }

// ---- DRIVE: tanh com compensação (em −12 dBFS sai no mesmo nível); de 0 a +6 dB entra aos poucos ----
constexpr float satRef = 0.25f;

struct DriveShape
{
    float d = 1.0f, norm = 1.0f, amount = 0.0f;
};

inline DriveShape makeDrive (float driveDb)
{
    DriveShape s;
    s.d      = juce::Decibels::decibelsToGain (juce::jmax (0.0f, driveDb));
    s.norm   = satRef / std::tanh (s.d * satRef);
    s.amount = juce::jlimit (0.0f, 1.0f, driveDb / 6.0f);
    return s;
}

inline float saturate (float x, const DriveShape& s)
{
    const float y = std::tanh (s.d * x) * s.norm;
    return x + (y - x) * s.amount;
}

struct BandSettings
{
    float thr = 0.0f, ratio = 2.0f, attMs = 10.0f, relMs = 150.0f, drive = 0.0f, gain = 0.0f;   // dB, :1, ms, ms, dB, dB
    bool  solo = false, mute = false;
};

struct Settings
{
    std::array<BandSettings, numBands> band {};
    std::array<float, 3> xover { { 120.0f, 1000.0f, 6000.0f } };   // Hz
    float macro = 0.0f;                                            // 0–1
};

// ---------------------------------------------------------------------------------------------
// 4 bandas com 3 crossovers Linkwitz-Riley de 4ª ordem em cascata. As bandas de baixo passam por
// all-pass nos cortes de cima, então a soma das 4 é plana (só gira a fase): sem latência.
// Por banda: compressor (detecção ligada L/R) → DRIVE → GANHO → solo/mudo.
// O "seco" do MIX é a soma das mesmas bandas sem processamento: mesma fase do molhado.
// ---------------------------------------------------------------------------------------------
class MultibandEngine
{
public:
    // ---- telemetria para a interface ----
    std::array<std::atomic<float>, numBands> grDb {};      // maior redução de cada banda desde a última leitura (dB)
    std::array<std::atomic<float>, scopeSize> scope {};    // saída para o espectro
    std::atomic<int> scopeWrite { 0 };

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), (juce::uint32) 2 };
        for (auto& f : split)
        {
            f.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            f.prepare (spec);
        }
        for (auto& f : allpass)
        {
            f.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
            f.prepare (spec);
        }
        for (int b = 0; b < numBands; ++b)
        {
            env[b] = 0.0f;
            smDrive[b].reset (fs, 0.05);
            smGain[b].reset (fs, 0.05);
            smAud[b].reset (fs, 0.02);
            lastDrive[b] = -1000.0f;
        }
        for (auto& g : grDb) g.store (0.0f);
        for (auto& s : scope) s.store (0.0f);
        scopeWrite.store (0);
        scopePos = 0;
        for (auto& a : applied) a = -1.0f;   // o prepare dos filtros voltou o corte para o padrão
        primed = false;
    }

    // out = processado; dry = soma das bandas sem processar (com o mesmo solo/mudo), para o MIX
    void process (const float* inL, const float* inR, float* outL, float* outR, float* dryL, float* dryR, int n, const Settings& s)
    {
        updateCrossovers (s, n);

        bool anySolo = false;
        for (const auto& b : s.band) anySolo = anySolo || b.solo;

        float attC[numBands], relC[numBands], thr[numBands], ratio[numBands], grMax[numBands];
        for (int b = 0; b < numBands; ++b)
        {
            const auto& bs = s.band[(size_t) b];
            attC[b]  = coef (bs.attMs);
            relC[b]  = coef (bs.relMs);
            thr[b]   = macroThr (bs.thr, s.macro);
            ratio[b] = macroRatio (bs.ratio, s.macro);
            grMax[b] = 0.0f;

            const float drv = macroDrive (bs.drive, s.macro);
            const float gn  = juce::Decibels::decibelsToGain (bs.gain);
            const float aud = anySolo ? (bs.solo ? 1.0f : 0.0f) : (bs.mute ? 0.0f : 1.0f);
            if (! primed)
            {
                smDrive[b].setCurrentAndTargetValue (drv);
                smGain[b].setCurrentAndTargetValue (gn);
                smAud[b].setCurrentAndTargetValue (aud);
            }
            smDrive[b].setTargetValue (drv);
            smGain[b].setTargetValue (gn);
            smAud[b].setTargetValue (aud);
        }
        primed = true;

        for (int i = 0; i < n; ++i)
        {
            float band[numBands][2];
            const float x[2] = { inL[i], inR[i] };
            for (int ch = 0; ch < 2; ++ch)
            {
                float lo = 0.0f, hi = 0.0f;
                split[0].processSample (ch, x[ch], lo, hi);
                const float b0 = lo;
                split[1].processSample (ch, hi, lo, hi);
                const float b1 = lo;
                split[2].processSample (ch, hi, lo, hi);
                band[2][ch] = lo;
                band[3][ch] = hi;
                band[1][ch] = allpass[2].processSample (ch, b1);                                  // all-pass em f3
                band[0][ch] = allpass[1].processSample (ch, allpass[0].processSample (ch, b0));   // all-pass em f2 e f3
            }

            float wl = 0.0f, wr = 0.0f, dl = 0.0f, dr = 0.0f;
            for (int b = 0; b < numBands; ++b)
            {
                const float l = band[b][0], r = band[b][1];
                const float lvl = juce::jmax (std::abs (l), std::abs (r));
                float& e = env[b];
                e = lvl > e ? lvl + attC[b] * (e - lvl) : lvl + relC[b] * (e - lvl);

                const float gr = reductionDb (juce::Decibels::gainToDecibels (e, -100.0f), thr[b], ratio[b]);
                grMax[b] = juce::jmax (grMax[b], gr);
                const float cg = gr > 0.0f ? juce::Decibels::decibelsToGain (-gr) : 1.0f;

                const float drv = smDrive[b].getNextValue();
                if (drv != lastDrive[b])
                {
                    lastDrive[b] = drv;
                    shape[b] = makeDrive (drv);
                }
                const float gn  = smGain[b].getNextValue();
                const float aud = smAud[b].getNextValue();

                const float yl = (shape[b].amount > 0.0f ? saturate (l * cg, shape[b]) : l * cg) * gn;
                const float yr = (shape[b].amount > 0.0f ? saturate (r * cg, shape[b]) : r * cg) * gn;
                wl += yl * aud;
                wr += yr * aud;
                dl += l * aud;
                dr += r * aud;
            }
            outL[i] = wl;
            outR[i] = wr;
            dryL[i] = dl;
            dryR[i] = dr;
        }

        for (int b = 0; b < numBands; ++b)
        {
            auto& a = grDb[(size_t) b];
            a.store (juce::jmax (a.load(), grMax[b]));
        }

        for (auto& f : split)   f.snapToZero();
        for (auto& f : allpass) f.snapToZero();
    }

    // a saída final (depois do MIX e do GANHO), para o espectro
    void pushScope (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            scope[(size_t) scopePos].store (0.5f * (l[i] + r[i]), std::memory_order_relaxed);
            scopePos = (scopePos + 1) % scopeSize;
        }
        scopeWrite.store (scopePos, std::memory_order_release);
    }

private:
    float coef (float ms) const
    {
        const double t = juce::jmax (0.05, (double) ms) * 0.001 * fs;
        return (float) std::exp (-1.0 / t);
    }

    void updateCrossovers (const Settings& s, int n)
    {
        // em ordem e com folga mínima entre os cortes; nunca perto de Nyquist
        const float top = (float) (fs * 0.45);
        float t[3];
        t[0] = juce::jmin (top / 1.44f, juce::jmax (20.0f, s.xover[0]));
        t[1] = juce::jmin (top / 1.2f,  juce::jmax (t[0] * 1.2f, s.xover[1]));
        t[2] = juce::jmin (top,         juce::jmax (t[1] * 1.2f, s.xover[2]));

        const float k = (float) (1.0 - std::exp (-(double) n / (0.03 * fs)));   // ~30 ms para chegar
        for (int i = 0; i < 3; ++i)
        {
            cur[i] = primed ? cur[i] * std::pow (t[i] / cur[i], k) : t[i];
            if (std::abs (cur[i] - applied[i]) > applied[i] * 1.0e-4f)
            {
                applied[i] = cur[i];
                split[(size_t) i].setCutoffFrequency (cur[i]);
                if (i == 1) allpass[0].setCutoffFrequency (cur[i]);
                if (i == 2) { allpass[1].setCutoffFrequency (cur[i]); allpass[2].setCutoffFrequency (cur[i]); }
            }
        }
    }

    double fs = 44100.0;
    bool primed = false;

    std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> split;     // cortes f1, f2, f3
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> allpass;   // banda 1: f2 e f3; banda 2: f3
    float cur[3] = { 120.0f, 1000.0f, 6000.0f };
    float applied[3] = { -1.0f, -1.0f, -1.0f };

    float env[numBands] = {};
    juce::SmoothedValue<float> smDrive[numBands], smGain[numBands], smAud[numBands];
    float lastDrive[numBands] = {};
    DriveShape shape[numBands];

    int scopePos = 0;
};
} // namespace lente
