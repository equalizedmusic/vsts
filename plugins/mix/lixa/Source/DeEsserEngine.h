#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

namespace lixa
{
constexpr int historyCols = 240;           // colunas do histórico de redução; cada uma ~16 ms ⇒ ~4 s na tela
constexpr int fftOrder    = 11;
constexpr int fftSize     = 1 << fftOrder; // 2048 pontos para o espectro (a FFT roda na interface)
constexpr int specRing    = 4096;          // anel de amostras que a interface lê (potência de 2)

// limites da faixa de detecção (os mesmos dos parâmetros) e distância mínima entre as bordas
constexpr float minLo = 1000.0f, maxLo = 12000.0f, minHi = 2000.0f, maxHi = 20000.0f, minRatio = 1.2f;

// ---------------- biquad (RBJ, Butterworth Q = 0,707); dois em série = 24 dB/oitava ----------------
struct Coefs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
};

inline Coefs designButter (double sampleRate, double freq, bool highPass)
{
    const double w0    = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    const double cw    = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * 0.7071067811865476);
    const double a0    = 1.0 + alpha;
    const double nb0   = highPass ? (1.0 + cw) * 0.5 : (1.0 - cw) * 0.5;
    const double nb1   = highPass ? -(1.0 + cw) : (1.0 - cw);

    Coefs c;
    c.b0 = (float) (nb0 / a0);
    c.b1 = (float) (nb1 / a0);
    c.b2 = c.b0;
    c.a1 = (float) (-2.0 * cw / a0);
    c.a2 = (float) ((1.0 - alpha) / a0);
    return c;
}

struct Biquad
{
    float z1 = 0.0f, z2 = 0.0f;

    void reset() { z1 = z2 = 0.0f; }

    float process (float x, const Coefs& c)
    {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// mantém a faixa válida: baixa < alta / 1,2 e alta abaixo de 45% da taxa de amostragem
inline void clampBand (float& lo, float& hi, double sampleRate)
{
    const float top = juce::jmax (minHi, juce::jmin (maxHi, (float) (sampleRate * 0.45)));
    hi = juce::jlimit (minHi, top, hi);
    lo = juce::jlimit (minLo, juce::jmax (minLo, juce::jmin (maxLo, hi / minRatio)), lo);
}

// quanto baixar (dB, ≥ 0) com o nível da faixa `over` dB acima do LIMIAR:
// razão 5:1 com joelho de 6 dB, nunca mais do que a REDUÇÃO MÁXIMA
inline float reductionDb (float over, float range)
{
    const float knee = 6.0f, slope = 0.8f;
    float r = 0.0f;
    if (over >= knee * 0.5f)
        r = slope * over;
    else if (over > -knee * 0.5f)
    {
        const float t = over + knee * 0.5f;
        r = slope * t * t / (2.0f * knee);
    }
    return juce::jlimit (0.0f, juce::jmax (0.0f, range), r);
}

struct Settings
{
    float lo = 4000.0f, hi = 10000.0f;       // Hz
    float threshold = -24.0f, range = 8.0f;  // dB
    float attackMs = 1.0f, releaseMs = 40.0f;
    float link = 1.0f;                       // 0–1
    bool  wide = false, delta = false;
};

// ---------------------------------------------------------------------------------------------
// Faixa = passa-altas (BAIXA) + passa-baixas (ALTA), 24 dB/oitava. O detector segue o pico da faixa
// (ATAQUE/SOLTURA) e calcula a redução contra o LIMIAR; o LINK puxa os dois lados para o maior corte.
// Só a faixa: saída = entrada − faixa × (1 − ganho)  (sem redução sai idêntico, latência zero).
// Banda larga: saída = entrada × ganho. SÓ O CORTE: entrada − saída (o que foi tirado).
// ---------------------------------------------------------------------------------------------
class DeEsserEngine
{
public:
    // ---- telemetria para a interface ----
    std::array<std::atomic<float>, historyCols> colGr {}, colLvl {};   // por coluna: maior redução (dB) e nível da faixa (dB)
    std::atomic<int>   colWrite { 0 };
    std::array<std::atomic<float>, specRing> specIn {}, specOut {};    // mono (L+R)/2, antes e depois
    std::atomic<int>   specWrite { 0 };
    std::atomic<float> grDb { 0.0f };          // maior redução desde a última leitura (dB, ≥ 0)
    std::atomic<float> bandDb { -120.0f };     // maior nível da faixa desde a última leitura (dB)

    void prepare (double sampleRate)
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        for (auto& ch : filt)
            for (auto& f : ch)
                f.reset();
        env = { { 0.0f, 0.0f } };

        colLen = juce::jmax (1, (int) std::lround (fs * 0.016));
        colCount = 0;
        accGr = 0.0f;
        accLvl = -120.0f;
        for (auto& c : colGr)  c.store (0.0f);
        for (auto& c : colLvl) c.store (-120.0f);
        colWrite.store (0);

        for (auto& s : specIn)  s.store (0.0f);
        for (auto& s : specOut) s.store (0.0f);
        specPos = 0;
        specWrite.store (0);

        smDelta.reset (fs, 0.02);
        primed = false;
    }

    // in: entrada (L, R); out: com a redução (ou só o que foi tirado, no SÓ O CORTE)
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        float lo = s.lo, hi = s.hi;
        clampBand (lo, hi, fs);
        tgtLo = lo;
        tgtHi = hi;
        if (! primed)
        {
            curLo = lo;
            curHi = hi;
            updateCoefs();
            smDelta.setCurrentAndTargetValue (s.delta ? 1.0f : 0.0f);
            primed = true;
        }
        smDelta.setTargetValue (s.delta ? 1.0f : 0.0f);

        const float atkC  = coefFor (s.attackMs);
        const float relC  = coefFor (s.releaseMs);
        const float link  = juce::jlimit (0.0f, 1.0f, s.link);
        float pkGr = 0.0f, pkLvl = -120.0f;

        for (int i = 0; i < n; ++i)
        {
            if ((i & 31) == 0) glide();

            const float x[2] = { inL[i], inR[i] };
            float band[2] = { 0.0f, 0.0f }, red[2] = { 0.0f, 0.0f };
            float lvlMax = -120.0f;
            for (int c = 0; c < 2; ++c)
            {
                auto& f = filt[(size_t) c];
                float b = f[0].process (x[c], hpC);
                b = f[1].process (b, hpC);
                b = f[2].process (b, lpC);
                b = f[3].process (b, lpC);
                band[c] = b;

                const float a = std::abs (b);
                float& e = env[(size_t) c];
                e = a + (e - a) * (a > e ? atkC : relC);
                const float lvl = juce::Decibels::gainToDecibels (e, -120.0f);
                lvlMax = juce::jmax (lvlMax, lvl);
                red[c] = reductionDb (lvl - s.threshold, s.range);
            }

            // LINK: os dois lados levam o mesmo corte (o maior), a imagem estéreo não mexe
            const float rMax = juce::jmax (red[0], red[1]);
            red[0] += (rMax - red[0]) * link;
            red[1] += (rMax - red[1]) * link;

            const float dk = smDelta.getNextValue();
            float y[2] = { 0.0f, 0.0f }, o[2] = { 0.0f, 0.0f };
            for (int c = 0; c < 2; ++c)
            {
                const float g = juce::Decibels::decibelsToGain (-red[c]);
                y[c] = s.wide ? x[c] * g : x[c] - band[c] * (1.0f - g);
                const float removed = x[c] - y[c];
                o[c] = y[c] + (removed - y[c]) * dk;
            }
            outL[i] = o[0];
            outR[i] = o[1];

            // ---- desenho ----
            specIn[(size_t) specPos].store (0.5f * (x[0] + x[1]), std::memory_order_relaxed);
            specOut[(size_t) specPos].store (0.5f * (y[0] + y[1]), std::memory_order_relaxed);
            specPos = (specPos + 1) & (specRing - 1);

            const float grNow = juce::jmax (red[0], red[1]);
            pkGr   = juce::jmax (pkGr, grNow);
            pkLvl  = juce::jmax (pkLvl, lvlMax);
            accGr  = juce::jmax (accGr, grNow);
            accLvl = juce::jmax (accLvl, lvlMax);
            if (++colCount >= colLen)
            {
                const int c = colWrite.load (std::memory_order_relaxed);
                colGr[(size_t) c].store (accGr, std::memory_order_relaxed);
                colLvl[(size_t) c].store (accLvl, std::memory_order_relaxed);
                colWrite.store ((c + 1) % historyCols, std::memory_order_release);
                accGr = 0.0f;
                accLvl = -120.0f;
                colCount = 0;
            }
        }

        // ---- telemetria (a interface lê e zera) ----
        specWrite.store (specPos, std::memory_order_release);
        grDb.store (juce::jmax (grDb.load(), pkGr));
        bandDb.store (juce::jmax (bandDb.load(), pkLvl));
    }

private:
    float coefFor (float ms) const
    {
        const double t = juce::jmax (0.01, (double) ms) * 0.001 * fs;
        return (float) std::exp (-1.0 / t);
    }

    void updateCoefs()
    {
        hpC = designButter (fs, (double) curLo, true);
        lpC = designButter (fs, (double) curHi, false);
    }

    // a faixa desliza até o alvo em passos de 32 amostras (arrastar a borda não dá clique)
    void glide()
    {
        if (std::abs (tgtLo - curLo) < 0.5f && std::abs (tgtHi - curHi) < 0.5f)
        {
            if (curLo != tgtLo || curHi != tgtHi)
            {
                curLo = tgtLo;
                curHi = tgtHi;
                updateCoefs();
            }
            return;
        }
        curLo += (tgtLo - curLo) * 0.2f;
        curHi += (tgtHi - curHi) * 0.2f;
        updateCoefs();
    }

    double fs = 44100.0;
    float curLo = 4000.0f, curHi = 10000.0f, tgtLo = 4000.0f, tgtHi = 10000.0f;
    Coefs hpC, lpC;
    std::array<std::array<Biquad, 4>, 2> filt {};
    std::array<float, 2> env { { 0.0f, 0.0f } };
    juce::SmoothedValue<float> smDelta;
    bool primed = false;

    int   colLen = 768, colCount = 0;
    float accGr = 0.0f, accLvl = -120.0f;
    int   specPos = 0;
};
} // namespace lixa
