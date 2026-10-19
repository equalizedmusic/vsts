#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace horizonte
{
constexpr int   scopeSize = 8192;    // amostras guardadas para o vetorscópio (potência de 2)
constexpr float monoOffHz = 20.0f;   // GRAVE MONO abaixo disso = desligado

struct Settings
{
    float width = 1.0f, widthLow = 1.0f, widthHigh = 1.0f;   // 0–2 (0% a 200%)
    float xover = 400.0f;                                    // DIVISÃO graves/agudos (Hz)
    float monoHz = 0.0f;                                     // GRAVE MONO (Hz); < monoOffHz = desligado
    float balance = 0.0f;                                    // −1 (esquerda) a +1 (direita)
    float rotation = 0.0f;                                   // graus; + = para a direita
    bool  monoListen = false, sideListen = false;            // performance (os presets não mexem)
};

// ---------------- filtro de estado variável (TPT), Butterworth (Q = 0,707): passa-baixa e passa-alta ----------------
struct SvfCoefs
{
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 1.41421356f;
};

inline SvfCoefs makeSvf (float freq, double fs)
{
    SvfCoefs c;
    const double f = juce::jlimit (10.0, fs * 0.45, (double) freq);
    const float  g = (float) std::tan (juce::MathConstants<double>::pi * f / fs);
    c.a1 = 1.0f / (1.0f + g * (g + c.k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    return c;
}

struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;

    void reset() { ic1 = ic2 = 0.0f; }

    void process (float x, const SvfCoefs& c, float& lp, float& hp) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        hp = x - c.k * v1 - v2;
    }
};

// ---------------- divisor Linkwitz-Riley de 4ª ordem: grave + agudo = passa-tudo (soma plana) ----------------
struct Lr4
{
    Svf first, low, high;

    void reset()
    {
        first.reset();
        low.reset();
        high.reset();
    }

    void split (float x, const SvfCoefs& c, float& lo, float& hi) noexcept
    {
        float l1 = 0.0f, h1 = 0.0f, unused = 0.0f;
        first.process (x, c, l1, h1);
        low.process (l1, c, lo, unused);
        high.process (h1, c, unused, hi);
    }
};

// resposta de um LR4 em magnitude (os dois lados ficam em fase): parte grave na frequência f
inline float lr4LowShare (float f, float fc)
{
    const float r = f / juce::jmax (1.0f, fc);
    const float r4 = r * r * r * r;
    return 1.0f / (1.0f + r4);
}

// ---------------------------------------------------------------------------------------------
// L/R → M/S → divide em graves/agudos (DIVISÃO) e aplica as larguras → MACRO → GRAVE MONO (zera o S
// abaixo da frequência) → volta para L/R → ROTAÇÃO → BALANÇO → (vetorscópio e correlação) → escuta
// (OUVIR EM MONO / SÓ O LADO). O M passa pelos mesmos divisores somados, para ficar em fase com o S.
// Só filtros IIR: latência zero.
// ---------------------------------------------------------------------------------------------
class ImageEngine
{
public:
    // ---- telemetria para a interface ----
    std::array<std::atomic<float>, scopeSize> scopeL {}, scopeR {};   // saída (antes da escuta), amostra a amostra
    std::atomic<int>   scopeWrite { 0 };
    std::atomic<float> powL { 0.0f }, powR { 0.0f }, powLR { 0.0f };  // médias de ~300 ms: L², R², L·R

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        xM.reset();
        xS.reset();
        bM.reset();
        bS.reset();

        for (auto* sm : { &smWidth, &smLow, &smHigh, &smMono, &smRot, &smBal })
            sm->reset (fs, 0.05);
        smListenMono.reset (fs, 0.02);
        smListenSide.reset (fs, 0.02);

        avgCoef  = (float) (1.0 - std::exp (-1.0 / (0.3 * fs)));
        freqCoef = (float) (1.0 - std::exp (-(double) chunk / (0.05 * fs)));
        sLL = sRR = sLR = 0.0f;

        for (auto& v : scopeL) v.store (0.0f);
        for (auto& v : scopeR) v.store (0.0f);
        pos = 0;
        scopeWrite.store (0);
        powL.store (0.0f);
        powR.store (0.0f);
        powLR.store (0.0f);
        primed = false;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& st)
    {
        const float monoAmt = st.monoHz >= monoOffHz ? 1.0f : 0.0f;
        const float rotRad  = juce::degreesToRadians (st.rotation);
        const float tgtX    = std::log (juce::jlimit (20.0f, 20000.0f, st.xover));
        const float tgtB    = std::log (juce::jlimit (monoOffHz, 20000.0f, st.monoHz));

        if (! primed)
        {
            smWidth.setCurrentAndTargetValue (st.width);
            smLow.setCurrentAndTargetValue (st.widthLow);
            smHigh.setCurrentAndTargetValue (st.widthHigh);
            smMono.setCurrentAndTargetValue (monoAmt);
            smRot.setCurrentAndTargetValue (rotRad);
            smBal.setCurrentAndTargetValue (st.balance);
            smListenMono.setCurrentAndTargetValue (st.monoListen ? 1.0f : 0.0f);
            smListenSide.setCurrentAndTargetValue (st.sideListen ? 1.0f : 0.0f);
            curX = tgtX;
            curB = tgtB;
            primed = true;
        }
        smWidth.setTargetValue (st.width);
        smLow.setTargetValue (st.widthLow);
        smHigh.setTargetValue (st.widthHigh);
        smMono.setTargetValue (monoAmt);
        smRot.setTargetValue (rotRad);
        smBal.setTargetValue (st.balance);
        smListenMono.setTargetValue (st.monoListen ? 1.0f : 0.0f);
        smListenSide.setTargetValue (st.sideListen ? 1.0f : 0.0f);

        for (int start = 0; start < n; start += chunk)
        {
            const int end = juce::jmin (n, start + chunk);

            // frequências andam suaves (em escala log), recalculadas a cada pedaço de 32 amostras
            curX += (tgtX - curX) * freqCoef;
            curB += (tgtB - curB) * freqCoef;
            const auto cx = makeSvf (std::exp (curX), fs);
            const auto cb = makeSvf (std::exp (curB), fs);

            for (int i = start; i < end; ++i)
            {
                const float wM  = smWidth.getNextValue(), wLo = smLow.getNextValue(), wHi = smHigh.getNextValue();
                const float mo  = smMono.getNextValue(), rt = smRot.getNextValue(), bl = smBal.getNextValue();
                const float lsm = smListenMono.getNextValue(), lss = smListenSide.getNextValue();

                // ---- M/S e larguras por faixa ----
                const float mid  = 0.5f * (inL[i] + inR[i]);
                const float side = 0.5f * (inL[i] - inR[i]);
                float mLo = 0.0f, mHi = 0.0f, sLo = 0.0f, sHi = 0.0f;
                xM.split (mid, cx, mLo, mHi);
                xS.split (side, cx, sLo, sHi);
                const float m1 = mLo + mHi;
                const float s1 = (sLo * wLo + sHi * wHi) * wM;

                // ---- GRAVE MONO: tira o S abaixo da frequência ----
                float m2Lo = 0.0f, m2Hi = 0.0f, s2Lo = 0.0f, s2Hi = 0.0f;
                bM.split (m1, cb, m2Lo, m2Hi);
                bS.split (s1, cb, s2Lo, s2Hi);
                const float m2 = m2Lo + m2Hi;
                const float s2 = s2Hi + s2Lo * (1.0f - mo);

                const float l0 = m2 + s2, r0 = m2 - s2;

                // ---- ROTAÇÃO (gira a imagem; + = para a direita) ----
                const float cs = std::cos (rt), sn = std::sin (rt);
                const float l1 = l0 * cs - r0 * sn;
                const float r1 = l0 * sn + r0 * cs;

                // ---- BALANÇO ----
                const float gl = bl > 0.0f ? 1.0f - bl : 1.0f;
                const float gr = bl < 0.0f ? 1.0f + bl : 1.0f;
                const float l = l1 * gl, r = r1 * gr;

                // ---- vetorscópio e correlação (o som que sai, antes da escuta) ----
                scopeL[(size_t) pos].store (l, std::memory_order_relaxed);
                scopeR[(size_t) pos].store (r, std::memory_order_relaxed);
                pos = (pos + 1) & (scopeSize - 1);
                sLL += avgCoef * (l * l - sLL);
                sRR += avgCoef * (r * r - sRR);
                sLR += avgCoef * (l * r - sLR);

                // ---- escuta: OUVIR EM MONO (M nos dois) ou SÓ O LADO (S nos dois; ganha do mono) ----
                const float mm = 0.5f * (l + r), ss = 0.5f * (l - r);
                float ol = l + (mm - l) * lsm;
                float orr = r + (mm - r) * lsm;
                ol  += (ss - ol) * lss;
                orr += (ss - orr) * lss;
                outL[i] = ol;
                outR[i] = orr;
            }
        }

        scopeWrite.store (pos, std::memory_order_release);
        powL.store (sLL);
        powR.store (sRR);
        powLR.store (sLR);
    }

private:
    static constexpr int chunk = 32;

    double fs = 44100.0;
    Lr4 xM, xS, bM, bS;   // DIVISÃO (M e S) e GRAVE MONO (M e S)
    juce::SmoothedValue<float> smWidth, smLow, smHigh, smMono, smRot, smBal, smListenMono, smListenSide;
    float curX = 6.0f, curB = 3.0f;   // log das frequências
    float avgCoef = 0.0001f, freqCoef = 0.1f;
    float sLL = 0.0f, sRR = 0.0f, sLR = 0.0f;
    int   pos = 0;
    bool  primed = false;
};
} // namespace horizonte
