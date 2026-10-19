#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace chama
{
// tudo sai de um número só, a CHAMA (0–1):
inline float cutoffHz (float h)   { return 20.0f * std::pow (100.0f, std::pow (h, 1.3f)); }   // passa-alta 20 Hz → 2 kHz
inline float resonanceQ (float h) { return 0.707f + 2.6f * std::pow (h, 1.5f); }               // pico no corte: o "assobio" da subida
inline float widthAmount (float h) { return 1.0f + 1.5f * h; }                                 // lados (S) até 250%
inline float noiseGain (float h, float noise01) { return noise01 * h * h * 0.3f; }
inline float washAmount (float h, float space01) { return space01 * std::pow (h, 1.3f) * 0.8f; }

// gate rítmico: entra a partir de 60% e acelera (1/8 → 1/16 → 1/32)
inline float gateDepth (float h)
{
    const float t = juce::jlimit (0.0f, 1.0f, (h - 0.6f) / 0.4f);
    return t * t * (3.0f - 2.0f * t) * 0.6f;
}
inline double gateBeats (float h) { return h < 0.75f ? 0.5 : h < 0.9f ? 0.25 : 0.125; }
inline const char* gateText (float h) { return h < 0.75f ? "1/8" : h < 0.9f ? "1/16" : "1/32"; }

struct Settings
{
    double bpm = 145.0, ppq = 0.0;
    bool   playing = false;   // gate só anda com o host tocando (no grid)
    float  chama = 0.0f, noise = 0.5f, space = 0.5f;
    bool   drop = false;      // DROP: apaga a chama na hora (20 ms)
};

// ---------------------------------------------------------------------------------------------
// passa-alta ressonante subindo → abre o estéreo → ruído filtrado subindo junto → reverb crescendo
// → gate no tempo nos últimos 40% → ganho de compensação + saturação suave
// ---------------------------------------------------------------------------------------------
class FlameEngine
{
public:
    // ---- telemetria para a interface ----
    std::atomic<float> heat { 0.0f }, level { 0.0f };

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        const int n = juce::jmax (1, maxBlock);
        revL.assign ((size_t) n, 0.0f);
        revR.assign ((size_t) n, 0.0f);
        reverb.setSampleRate (fs);
        reverb.reset();

        h = 0.0f;
        kSlow = (float) (1.0 - std::exp (-1.0 / (0.06 * fs)));
        kFast = (float) (1.0 - std::exp (-1.0 / (0.02 * fs)));
        kGate = (float) (1.0 - std::exp (-1.0 / (0.003 * fs)));
        gateEnv = 1.0f;
        washSm = 0.0f;
        for (auto* s : { &hp[0], &hp[1], &nz[0], &nz[1] }) *s = Svf {};
        primed = false;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        if ((int) revL.size() < n)
        {
            revL.resize ((size_t) n);
            revR.resize ((size_t) n);
        }
        if (! primed) { h = s.drop ? 0.0f : s.chama; primed = true; }

        const float target = s.drop ? 0.0f : s.chama;
        const float k = s.drop ? kFast : kSlow;
        const double beatsPerSample = s.bpm / 60.0 / fs;
        const float hBlock = h;

        // ---- filtro, estéreo, ruído (por amostra; coeficientes a cada 16) ----
        for (int i = 0; i < n; ++i)
        {
            h += (target - h) * k;
            if ((i & 15) == 0)
            {
                const float fc = juce::jmin (cutoffHz (h), (float) fs * 0.45f);
                hp[0].set (fs, fc, resonanceQ (h));
                hp[1].copyCoefs (hp[0]);
                const float fn = juce::jlimit (300.0f, (float) fs * 0.42f, fc * 4.0f);
                nz[0].set (fs, fn, 2.0f);
                nz[1].copyCoefs (nz[0]);
                width = widthAmount (h);
                midG  = 1.0f - 0.25f * h;
                nzG   = noiseGain (h, s.noise);
            }

            float l = hp[0].highpass (inL[i]);
            float r = hp[1].highpass (inR[i]);
            const float m = (l + r) * 0.5f * midG, sd = (l - r) * 0.5f * width;
            l = m + sd;
            r = m - sd;
            if (nzG > 1.0e-5f)
            {
                l += nz[0].bandpass (rng.nextFloat() * 2.0f - 1.0f) * nzG;
                r += nz[1].bandpass (rng.nextFloat() * 2.0f - 1.0f) * nzG;
            }
            outL[i] = l;
            outR[i] = r;
            revL[(size_t) i] = l;
            revR[(size_t) i] = r;
        }

        // ---- reverb crescendo (só o molhado, somado por cima) ----
        juce::Reverb::Parameters rp;
        rp.roomSize   = 0.55f + 0.4f * h;
        rp.damping    = 0.35f;
        rp.wetLevel   = 0.33f;
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (revL.data(), revR.data(), n);
        const float washFrom = washSm, washTo = washAmount (h, s.space);
        washSm = washTo;

        // ---- gate no tempo + compensação + saturação suave ----
        const float depth = s.playing ? gateDepth (hBlock) : 0.0f;
        const double gb   = gateBeats (hBlock);
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float wv = washFrom + (washTo - washFrom) * ((float) i / (float) n);
            float l = outL[i] + revL[(size_t) i] * wv;
            float r = outR[i] + revR[(size_t) i] * wv;

            const double ph = (s.ppq + (double) i * beatsPerSample) / gb;
            const bool   off = (ph - std::floor (ph)) >= 0.5;
            gateEnv += ((off ? 1.0f - depth : 1.0f) - gateEnv) * kGate;

            const float g = gateEnv * (1.0f + 0.6f * h);
            l = softClip (l * g);
            r = softClip (r * g);
            outL[i] = l;
            outR[i] = r;
            pk = juce::jmax (pk, std::abs (l), std::abs (r));
        }

        heat.store (h);
        level.store (juce::jmax (level.load(), pk));
    }

private:
    // filtro de estado variável (TPT): estável com o corte andando
    struct Svf
    {
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, kq = 1.4f, ic1 = 0.0f, ic2 = 0.0f;

        void set (double fs, float hz, float q)
        {
            const float g = (float) std::tan (juce::MathConstants<double>::pi * (double) hz / fs);
            kq = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + kq));
            a2 = g * a1;
            a3 = g * a2;
        }
        void copyCoefs (const Svf& o) { a1 = o.a1; a2 = o.a2; a3 = o.a3; kq = o.kq; }

        void tick (float x, float& v1, float& v2)
        {
            const float v3 = x - ic2;
            v1 = a1 * ic1 + a2 * v3;
            v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
        }
        float highpass (float x) { float v1, v2; tick (x, v1, v2); return x - kq * v1 - v2; }
        float bandpass (float x) { float v1, v2; tick (x, v1, v2); return kq * v1; }   // pico em 0 dB
    };

    static float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    }

    double fs = 44100.0;
    float  h = 0.0f, kSlow = 0.001f, kFast = 0.003f, kGate = 0.01f, gateEnv = 1.0f, washSm = 0.0f;
    float  width = 1.0f, midG = 1.0f, nzG = 0.0f;
    bool   primed = false;
    Svf    hp[2], nz[2];
    juce::Reverb reverb;
    std::vector<float> revL, revR;
    juce::Random rng;
};
} // namespace chama
