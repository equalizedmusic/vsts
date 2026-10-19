#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

namespace soco
{
constexpr int historyCols = 480;   // colunas da forma de onda; cada uma ~6 ms ⇒ ~3 s na tela

constexpr float maxAttackDb  = 15.0f;   // o quanto o ATAQUE pode subir/descer no máximo
constexpr float maxSustainDb = 12.0f;   // idem para o SUSTAIN
constexpr float levelFloor   = 1.0e-4f; // −80 dB: abaixo disso o detector trata como silêncio

// filtro do detector (o que ele "ouve" para achar o golpe)
enum DetMode { detAll = 0, detLow, detHigh, numDetModes };
inline const char* const detModeNames[numDetModes] = { "Tudo", "Graves", "Agudos" };

// coeficiente de um seguidor de envelope com constante de tempo em ms
inline float coefMs (float ms, float fs)
{
    return std::exp (-1000.0f / (juce::jmax (0.01f, ms) * fs));
}

// clipper suave de proteção: passa igual até −2,3 dBFS e encosta em −0,3 dBFS (joelho sem quina)
inline float softClip (float x)
{
    constexpr float c = 0.966f, w = 0.2f;
    const float t = std::abs (x);
    if (t <= c - w) return x;
    float y = c;
    if (t < c + w)
    {
        const float d = t - (c - w);
        y = t - d * d / (4.0f * w);
    }
    return x < 0.0f ? -y : y;
}

// ---------------------------------------------------------------------------------------------
// DETECTOR independente de nível: compara envelopes (razão em dB), não o nível absoluto.
//   ATAQUE : envelope rápido × envelope lento (ataque = FOCO). No começo do golpe o rápido sobe
//            antes; a diferença é o "tamanho" do transiente.
//   SUSTAIN: dois envelopes com o mesmo ataque e solturas diferentes. Na cauda o lento fica em
//            cima; a diferença é o quanto o som está "soltando".
// O lento segue o rápido (em cascata), então som contínuo (baixo, pad) quase não mexe.
// ---------------------------------------------------------------------------------------------
class Detector
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        setFocus (focus);
        reset();
    }

    void setFocus (float ms)
    {
        focus = ms;
        aFast = coefMs (0.2f, fs);
        aSlow = coefMs (ms, fs);
        rAtk  = coefMs (ms * 3.0f + 30.0f, fs);
        const float rs = ms * 2.0f + 20.0f;
        aSus     = coefMs (1.0f, fs);
        rSusFast = coefMs (rs, fs);
        rSusSlow = coefMs (rs * 8.0f, fs);
        smooth   = coefMs (0.5f, fs);
    }

    void reset()
    {
        eFast = eSlow = sFast = sSlow = 0.0f;
        atkSm = susSm = 0.0f;
    }

    // level = nível retificado (≥ 0); atkAmt e susAmt em −1..+1; devolve o ganho de cada parte em dB
    void process (float level, float atkAmt, float susAmt, float& atkDb, float& susDb)
    {
        follow (eFast, level, aFast, rAtk);
        follow (eSlow, eFast, aSlow, rAtk);
        follow (sFast, level, aSus, rSusFast);
        follow (sSlow, sFast, aSus, rSusSlow);

        const float aRaw = juce::jmax (0.0f, 20.0f * std::log10 ((eFast + levelFloor) / (eSlow + levelFloor)));
        const float sRaw = juce::jmax (0.0f, 20.0f * std::log10 ((sSlow + levelFloor) / (sFast + levelFloor)));
        const float aT = atkAmt * juce::jmin (aRaw * 0.75f, maxAttackDb);
        const float sT = susAmt * juce::jmin (sRaw * 0.5f, maxSustainDb);
        atkSm = aT + (atkSm - aT) * smooth;
        susSm = sT + (susSm - sT) * smooth;
        atkDb = atkSm;
        susDb = susSm;
    }

private:
    static void follow (float& e, float x, float a, float r)
    {
        const float c = x > e ? a : r;
        e = x + (e - x) * c;
    }

    float fs = 44100.0f, focus = 12.0f;
    float aFast = 0.0f, aSlow = 0.0f, rAtk = 0.0f, aSus = 0.0f, rSusFast = 0.0f, rSusSlow = 0.0f, smooth = 0.0f;
    float eFast = 0.0f, eSlow = 0.0f, sFast = 0.0f, sSlow = 0.0f, atkSm = 0.0f, susSm = 0.0f;
};

// passa-baixa de um polo (TPT); o passa-alta sai como x − passa-baixa
struct OnePole
{
    float s = 0.0f;
    float lp (float x, float G)
    {
        const float v = (x - s) * G;
        const float y = v + s;
        s = y + v;
        return y;
    }
};

struct Settings
{
    float attack = 0.0f, sustain = 0.0f;   // −1..+1
    float focusMs = 12.0f;
    int   detMode = detAll;
    float detFreq = 200.0f;                // Hz
    float mix = 1.0f;                      // 0–1
    float outDb = 0.0f;
    bool  clip = true, delta = false;
};

// ---------------------------------------------------------------------------------------------
// detector (com filtro) → ganho de ATAQUE + SUSTAIN, o mesmo nos dois canais (a imagem não mexe)
// → MIX → OUVIR DIFERENÇA → GANHO → clipper de proteção. Sem latência.
// ---------------------------------------------------------------------------------------------
class SocoEngine
{
public:
    // ---- telemetria para a interface ----
    std::array<std::atomic<float>, historyCols> colBefore {}, colAfter {};   // pico de cada coluna: antes e depois
    std::array<std::atomic<float>, historyCols> colAtk {}, colSus {};        // ganho de cada parte (dB, com sinal)
    std::atomic<int>   colWrite { 0 };
    std::atomic<float> atkNow { 0.0f }, susNow { 0.0f };                     // maior ganho (em módulo) desde a última leitura

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        det.prepare (sampleRate);
        lastFocus = -1.0f;
        for (auto& ch : filt)
            for (auto& st : ch) st.s = 0.0f;

        colLen = juce::jmax (1, (int) std::lround (sampleRate * 0.006));
        colCount = 0;
        accBefore = accAfter = accAtk = accSus = 0.0f;
        for (auto& c : colBefore) c.store (0.0f);
        for (auto& c : colAfter)  c.store (0.0f);
        for (auto& c : colAtk)    c.store (0.0f);
        for (auto& c : colSus)    c.store (0.0f);
        colWrite.store (0);

        smAtk.reset (sampleRate, 0.03);
        smSus.reset (sampleRate, 0.03);
        smMix.reset (sampleRate, 0.05);
        smOut.reset (sampleRate, 0.05);
        smDelta.reset (sampleRate, 0.02);
        smClip.reset (sampleRate, 0.02);
        primed = false;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        const float outG = juce::Decibels::decibelsToGain (s.outDb);
        if (! primed)
        {
            smAtk.setCurrentAndTargetValue (s.attack);
            smSus.setCurrentAndTargetValue (s.sustain);
            smMix.setCurrentAndTargetValue (s.mix);
            smOut.setCurrentAndTargetValue (outG);
            smDelta.setCurrentAndTargetValue (s.delta ? 1.0f : 0.0f);
            smClip.setCurrentAndTargetValue (s.clip ? 1.0f : 0.0f);
            primed = true;
        }
        smAtk.setTargetValue (s.attack);
        smSus.setTargetValue (s.sustain);
        smMix.setTargetValue (s.mix);
        smOut.setTargetValue (outG);
        smDelta.setTargetValue (s.delta ? 1.0f : 0.0f);
        smClip.setTargetValue (s.clip ? 1.0f : 0.0f);

        if (std::abs (s.focusMs - lastFocus) > 1.0e-3f)
        {
            det.setFocus (s.focusMs);
            lastFocus = s.focusMs;
        }

        const float fc = juce::jlimit (20.0f, fs * 0.45f, s.detFreq);
        const float gg = std::tan (juce::MathConstants<float>::pi * fc / fs);
        const float G  = gg / (1.0f + gg);
        const int mode = juce::jlimit (0, numDetModes - 1, s.detMode);

        float atkBig = 0.0f, susBig = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float xl = inL[i], xr = inR[i];
            const float dl = detFilter (0, xl, mode, G), dr = detFilter (1, xr, mode, G);

            float aDb = 0.0f, sDb = 0.0f;
            det.process (juce::jmax (std::abs (dl), std::abs (dr)), smAtk.getNextValue(), smSus.getNextValue(), aDb, sDb);
            const float g = juce::Decibels::decibelsToGain (aDb + sDb);

            // MIX: k = 1 sem efeito, g com 100%
            const float k  = 1.0f + (g - 1.0f) * smMix.getNextValue();
            const float ml = xl * k, mr = xr * k;

            // OUVIR DIFERENÇA: só o que o SOCO acrescentou (ou tirou, com a fase invertida)
            const float dk = smDelta.getNextValue();
            const float og = smOut.getNextValue();
            float yl = (ml + ((ml - xl) - ml) * dk) * og;
            float yr = (mr + ((mr - xr) - mr) * dk) * og;

            const float ck = smClip.getNextValue();
            if (ck > 0.0f)
            {
                yl += (softClip (yl) - yl) * ck;
                yr += (softClip (yr) - yr) * ck;
            }
            outL[i] = yl;
            outR[i] = yr;

            // ---- desenho ----
            accBefore = juce::jmax (accBefore, std::abs (xl), std::abs (xr));
            accAfter  = juce::jmax (accAfter,  std::abs (ml), std::abs (mr));
            if (std::abs (aDb) > std::abs (accAtk)) accAtk = aDb;
            if (std::abs (sDb) > std::abs (accSus)) accSus = sDb;
            if (std::abs (aDb) > std::abs (atkBig)) atkBig = aDb;
            if (std::abs (sDb) > std::abs (susBig)) susBig = sDb;

            if (++colCount >= colLen)
            {
                const int c = colWrite.load (std::memory_order_relaxed);
                colBefore[(size_t) c].store (accBefore, std::memory_order_relaxed);
                colAfter[(size_t) c].store (accAfter, std::memory_order_relaxed);
                colAtk[(size_t) c].store (accAtk, std::memory_order_relaxed);
                colSus[(size_t) c].store (accSus, std::memory_order_relaxed);
                colWrite.store ((c + 1) % historyCols, std::memory_order_release);
                accBefore = accAfter = accAtk = accSus = 0.0f;
                colCount = 0;
            }
        }

        // ---- telemetria (a interface lê e zera) ----
        if (std::abs (atkBig) > std::abs (atkNow.load())) atkNow.store (atkBig);
        if (std::abs (susBig) > std::abs (susNow.load())) susNow.store (susBig);
    }

private:
    float detFilter (int ch, float x, int mode, float G)
    {
        auto& st = filt[(size_t) ch];
        if (mode == detLow)
            return st[1].lp (st[0].lp (x, G), G);
        if (mode == detHigh)
        {
            const float a = x - st[0].lp (x, G);
            return a - st[1].lp (a, G);
        }
        return x;
    }

    float fs = 44100.0f, lastFocus = -1.0f;
    Detector det;
    std::array<std::array<OnePole, 2>, 2> filt {};   // [canal][estágio]: 12 dB/oitava
    juce::SmoothedValue<float> smAtk, smSus, smMix, smOut, smDelta, smClip;
    bool primed = false;

    int   colLen = 264, colCount = 0;
    float accBefore = 0.0f, accAfter = 0.0f, accAtk = 0.0f, accSus = 0.0f;
};
} // namespace soco
