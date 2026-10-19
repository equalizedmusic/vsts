#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

namespace torno
{
constexpr int historyCols = 500;   // colunas do histórico; cada uma ~8 ms ⇒ ~4 s na tela

// curva efetiva (já com o MACRO APERTO): limiar e joelho em dB, razão N:1
struct Curve
{
    float threshold = -18.0f, ratio = 4.0f, knee = 6.0f;
};

// APERTO 0–1: desce o limiar até 24 dB e multiplica a razão até 2,5× (no máximo 20:1)
inline Curve makeCurve (float thresholdDb, float ratio, float kneeDb, float squeeze01)
{
    Curve c;
    c.threshold = thresholdDb - 24.0f * squeeze01;
    c.ratio     = juce::jlimit (1.0f, 20.0f, ratio * (1.0f + 1.5f * squeeze01));
    c.knee      = juce::jmax (0.0f, kneeDb);
    return c;
}

// saída (dB) para uma entrada (dB): reta abaixo do limiar, razão acima, joelho quadrático no meio
inline float curveOut (float x, const Curve& c)
{
    const float d = x - c.threshold;
    const float halfKnee = c.knee * 0.5f;
    if (c.knee > 0.01f && std::abs (d) <= halfKnee)
    {
        const float t = d + halfKnee;
        return x + (1.0f / c.ratio - 1.0f) * t * t / (2.0f * c.knee);
    }
    return d <= 0.0f ? x : c.threshold + d / c.ratio;
}

// compensação automática: metade da redução que um pico em 0 dBFS levaria
inline float autoMakeupDb (const Curve& c)
{
    return juce::jlimit (0.0f, 24.0f, 0.5f * (0.0f - curveOut (0.0f, c)));
}

struct Settings
{
    float squeeze = 0.0f;                                   // 0–1
    float threshold = -18.0f, ratio = 4.0f, knee = 6.0f;    // dB, N:1, dB
    float attackMs = 10.0f, releaseMs = 150.0f;
    float hpfHz = 20.0f;                                    // 20 = filtro desligado
    float makeupDb = 0.0f;
    bool  autoRelease = false, autoMakeup = false, listen = false;
};

inline bool hpfActive (float hz) { return hz > 20.5f; }

inline float totalMakeupDb (const Settings& s, const Curve& c)
{
    return s.makeupDb + (s.autoMakeup ? autoMakeupDb (c) : 0.0f);
}

// passa-altas de 2ª ordem (Butterworth), forma transposta direta II
struct HighPass
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void set (double fs, double hz)
    {
        hz = juce::jlimit (10.0, fs * 0.45, hz);
        const double w0 = juce::MathConstants<double>::twoPi * hz / fs;
        const double cw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * 0.7071067811865476);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 + cw) * 0.5 / a0);
        b1 = (float) (-(1.0 + cw) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * cw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void reset() { z1 = 0.0f; z2 = 0.0f; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// ---------------------------------------------------------------------------------------------
// Detector de pico ligado (L/R levam o mesmo ganho) → filtro passa-altas no detector →
// curva com joelho em dB → envelope de ataque/soltura no domínio de dB → ganho.
// Sem lookahead: latência zero. OUVIR SIDECHAIN troca a saída pelo sinal do detector filtrado.
// ---------------------------------------------------------------------------------------------
class CompEngine
{
public:
    // ---- telemetria para a interface ----
    std::array<std::atomic<float>, historyCols> colIn {}, colGr {};   // pico de entrada (ganho) e redução máx. (dB) de cada coluna
    std::atomic<int>   colWrite { 0 };
    std::atomic<float> grDb { 0.0f };        // maior redução desde a última leitura (dB, ≥ 0)
    std::atomic<float> detDb { -120.0f };    // maior nível no detector desde a última leitura (dBFS)

    void prepare (double sampleRate)
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        for (auto& h : hp) h.reset();
        lastHz = -1.0f;
        env = 0.0f;
        avgGr = 0.0f;

        colLen = juce::jmax (1, (int) std::lround (fs * 0.008));
        colCount = 0;
        accIn = 0.0f;
        accGr = 0.0f;
        for (auto& c : colIn) c.store (0.0f);
        for (auto& c : colGr) c.store (0.0f);
        colWrite.store (0);

        smMakeup.reset (fs, 0.05);
        smListen.reset (fs, 0.02);
        primed = false;
    }

    // out = comprimido com compensação (ou o detector, com OUVIR SIDECHAIN)
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        const Curve c = makeCurve (s.threshold, s.ratio, s.knee, s.squeeze);
        const float makeupG = juce::Decibels::decibelsToGain (totalMakeupDb (s, c));
        const float listenT = s.listen ? 1.0f : 0.0f;
        if (! primed)
        {
            smMakeup.setCurrentAndTargetValue (makeupG);
            smListen.setCurrentAndTargetValue (listenT);
            primed = true;
        }
        smMakeup.setTargetValue (makeupG);
        smListen.setTargetValue (listenT);

        const bool hpOn = hpfActive (s.hpfHz);
        if (hpOn && std::abs (s.hpfHz - lastHz) > 0.01f)
        {
            for (auto& h : hp) h.set (fs, (double) s.hpfHz);
            lastHz = s.hpfHz;
        }

        const float aAtt  = coef (juce::jmax (0.05f, s.attackMs));
        const float aRel  = coef (juce::jmax (1.0f, s.releaseMs));
        const float aFast = coef (40.0f), aSlow = coef (600.0f), aAvg = coef (1000.0f);

        float blockGr = 0.0f, blockDet = -120.0f;
        for (int i = 0; i < n; ++i)
        {
            const float xl = inL[i], xr = inR[i];
            const float sl = hpOn ? hp[0].process (xl) : xl;
            const float sr = hpOn ? hp[1].process (xr) : xr;

            const float det   = juce::jmax (std::abs (sl), std::abs (sr));
            const float detDbNow = juce::Decibels::gainToDecibels (det, -120.0f);
            const float target = juce::jmax (0.0f, detDbNow - curveOut (detDbNow, c));

            if (target > env)
            {
                env = aAtt * env + (1.0f - aAtt) * target;
            }
            else
            {
                float rel = aRel;
                if (s.autoRelease)
                {
                    // SOLTURA AUTO: pico isolado (acima da média) solta rápido; redução constante solta devagar
                    const float k = env > 1.0e-3f ? juce::jlimit (0.0f, 1.0f, avgGr / env) : 1.0f;
                    rel = aFast + (aSlow - aFast) * k;
                }
                env = rel * env + (1.0f - rel) * target;
            }
            avgGr = aAvg * avgGr + (1.0f - aAvg) * env;

            const float g  = juce::Decibels::decibelsToGain (-env) * smMakeup.getNextValue();
            const float lk = smListen.getNextValue();
            outL[i] = xl * g * (1.0f - lk) + sl * lk;
            outR[i] = xr * g * (1.0f - lk) + sr * lk;

            const float pk = juce::jmax (std::abs (xl), std::abs (xr));
            accIn    = juce::jmax (accIn, pk);
            accGr    = juce::jmax (accGr, env);
            blockGr  = juce::jmax (blockGr, env);
            blockDet = juce::jmax (blockDet, detDbNow);

            if (++colCount >= colLen)
            {
                const int w = colWrite.load (std::memory_order_relaxed);
                colIn[(size_t) w].store (accIn, std::memory_order_relaxed);
                colGr[(size_t) w].store (accGr, std::memory_order_relaxed);
                colWrite.store ((w + 1) % historyCols, std::memory_order_release);
                accIn = 0.0f;
                accGr = 0.0f;
                colCount = 0;
            }
        }

        // ---- telemetria (a interface lê e zera) ----
        grDb.store (juce::jmax (grDb.load(), blockGr));
        detDb.store (juce::jmax (detDb.load(), blockDet));
    }

private:
    float coef (float ms) const
    {
        return (float) std::exp (-1.0 / (0.001 * (double) ms * fs));
    }

    double fs = 44100.0;
    std::array<HighPass, 2> hp;
    float lastHz = -1.0f;
    float env = 0.0f, avgGr = 0.0f;   // redução atual e a média lenta (dB)
    juce::SmoothedValue<float> smMakeup, smListen;
    bool primed = false;

    int   colLen = 352, colCount = 0;
    float accIn = 0.0f, accGr = 0.0f;
};
} // namespace torno
