#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace medidor
{
constexpr int    historyLen = 600;     // um ponto a cada 100 ms ⇒ 60 s de LUFS na tela
constexpr int    fftRingLen = 8192;    // amostras mono guardadas para o espectro (potência de 2)
constexpr int    gonioLen   = 4096;    // pares L/R guardados para o goniômetro (potência de 2)
constexpr int    histBins   = 1000;    // histogramas do integrado e da LRA: −70 a +30 LUFS em 0,1 LU
constexpr double histMin    = -70.0, histStep = 0.1;
constexpr float  silence    = -100.0f; // "sem leitura" nas telemetrias em LUFS
constexpr int    tpTaps     = 16;      // taps por fase do true peak (4 fases ⇒ FIR de 64)
constexpr int    momSubs    = 4;       // 4 × 100 ms = momentâneo (400 ms)
constexpr int    shortSubs  = 30;      // 30 × 100 ms = curto (3 s)
constexpr int    rmsSubs    = 3;       // 3 × 100 ms = RMS (300 ms)

// BS.1770: L = −0,691 + 10·log10(soma das médias quadráticas dos canais, já com o filtro K)
inline double energyToLufs (double e) { return e > 1.0e-15 ? -0.691 + 10.0 * std::log10 (e) : (double) silence; }

// biquad em double (forma direta II transposta)
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() { z1 = z2 = 0.0; }

    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// filtro K da BS.1770-4 (prateleira de agudos + passa-altas), recalculado para qualquer taxa
// (mesmas constantes do libebur128: em 48 kHz dá os coeficientes da norma)
inline void makeKWeighting (double fs, Biquad& shelf, Biquad& hp)
{
    const double pi = juce::MathConstants<double>::pi;
    {
        const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
        const double k  = std::tan (pi * f0 / fs);
        const double vh = std::pow (10.0, gainDb / 20.0);
        const double vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;
        shelf.b0 = (vh + vb * k / q + k * k) / a0;
        shelf.b1 = 2.0 * (k * k - vh) / a0;
        shelf.b2 = (vh - vb * k / q + k * k) / a0;
        shelf.a1 = 2.0 * (k * k - 1.0) / a0;
        shelf.a2 = (1.0 - k / q + k * k) / a0;
    }
    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const double k  = std::tan (pi * f0 / fs);
        const double a0 = 1.0 + k / q + k * k;
        hp.b0 = 1.0;
        hp.b1 = -2.0;
        hp.b2 = 1.0;
        hp.a1 = 2.0 * (k * k - 1.0) / a0;
        hp.a2 = (1.0 - k / q + k * k) / a0;
    }
}

// histograma de blocos (contagem + energia por faixa de 0,1 LU): o integrado e a LRA saem
// daqui sem guardar a música inteira e sem alocar nada
struct LoudnessHistogram
{
    std::array<int, histBins>    count {};
    std::array<double, histBins> energy {};
    int    total = 0;
    double totalEnergy = 0.0;

    void clear()
    {
        count.fill (0);
        energy.fill (0.0);
        total = 0;
        totalEnergy = 0.0;
    }

    void add (double lufs, double e)
    {
        const int b = juce::jlimit (0, histBins - 1, (int) std::floor ((lufs - histMin) / histStep));
        ++count[(size_t) b];
        energy[(size_t) b] += e;
        ++total;
        totalEnergy += e;
    }

    static double binCentre (int b) { return histMin + ((double) b + 0.5) * histStep; }
};

// integrado: média (em energia) dos blocos acima do portão relativo (média geral + relativeGate)
inline double gatedLoudness (const LoudnessHistogram& h, double relativeGate)
{
    if (h.total <= 0) return (double) silence;
    const double gate = energyToLufs (h.totalEnergy / (double) h.total) + relativeGate;
    double e = 0.0;
    int c = 0;
    for (int b = 0; b < histBins; ++b)
    {
        const auto i = (size_t) b;
        if (h.count[i] > 0 && LoudnessHistogram::binCentre (b) > gate)
        {
            e += h.energy[i];
            c += h.count[i];
        }
    }
    return c > 0 ? energyToLufs (e / (double) c) : (double) silence;
}

// LRA (EBU Tech 3342): curtos acima do portão relativo de −20 LU; diferença entre os percentis 10 e 95
inline double loudnessRange (const LoudnessHistogram& h, double& lo, double& hi)
{
    lo = hi = (double) silence;
    if (h.total <= 0) return 0.0;
    const double gate = energyToLufs (h.totalEnergy / (double) h.total) - 20.0;

    int c = 0;
    for (int b = 0; b < histBins; ++b)
        if (LoudnessHistogram::binCentre (b) > gate)
            c += h.count[(size_t) b];
    if (c <= 0) return 0.0;

    const double pLo = 0.10 * (double) (c - 1), pHi = 0.95 * (double) (c - 1);
    int  run = 0;
    bool gotLo = false;
    for (int b = 0; b < histBins; ++b)
    {
        const auto   i = (size_t) b;
        const double l = LoudnessHistogram::binCentre (b);
        if (h.count[i] <= 0 || l <= gate) continue;
        const double end = (double) (run + h.count[i]);
        if (! gotLo && pLo < end) { lo = l; gotLo = true; }
        if (pHi < end) { hi = l; break; }
        run += h.count[i];
    }
    return juce::jmax (0.0, hi - lo);
}

// ---------------------------------------------------------------------------------------------
// Medidor: só LÊ o áudio (não mexe em nada). Por amostra: filtro K, pico, true peak (4×),
// correlação e os anéis do espectro/goniômetro. A cada 100 ms: momentâneo, curto, integrado
// (portões −70 e −10), LRA, RMS e um ponto do histórico. Nada aloca depois do prepare.
// ---------------------------------------------------------------------------------------------
class MeterEngine
{
public:
    // ---- telemetria para a interface (LUFS em LUFS; picos/RMS em ganho linear) ----
    std::atomic<float> momentary { silence }, shortTerm { silence }, integrated { silence };
    std::atomic<float> shortMax { silence };
    std::atomic<float> lra { 0.0f }, lraLo { silence }, lraHi { silence };
    std::atomic<float> truePeakMax { 0.0f };             // máximo segurado desde o ZERAR
    std::atomic<float> seconds { 0.0f };                 // tempo medido desde o ZERAR
    std::atomic<float> rms { 0.0f }, corr { 0.0f };
    std::atomic<float> peakNow { 0.0f }, truePeakNow { 0.0f };   // maior desde a última leitura (a interface zera)

    std::array<std::atomic<float>, historyLen> histM {}, histS {};   // momentâneo e curto, um ponto a cada 100 ms
    std::atomic<int> histWrite { 0 };
    std::array<std::atomic<float>, fftRingLen> fftRing {};           // (L+R)/2 para o espectro
    std::atomic<int> fftWrite { 0 };
    std::array<std::atomic<float>, gonioLen> gonL {}, gonR {};       // L e R crus para o goniômetro
    std::atomic<int> gonWrite { 0 };

    MeterEngine()
    {
        designTruePeak();
        for (auto& v : histM) v.store (silence);
        for (auto& v : histS) v.store (silence);
        for (auto& v : fftRing) v.store (0.0f);
        for (auto& v : gonL) v.store (0.0f);
        for (auto& v : gonR) v.store (0.0f);
        gateI.clear();
        gateS.clear();
    }

    // ZERAR (qualquer thread): o áudio recomeça integrado, LRA e máximos no próximo bloco
    void requestReset() { resetFlag.store (true); }

    void prepare (double sampleRate)
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        for (size_t ch = 0; ch < 2; ++ch)
        {
            makeKWeighting (fs, shelf[ch], highPass[ch]);
            shelf[ch].reset();
            highPass[ch].reset();
            tpBuf[ch].fill (0.0f);
        }
        tpPos = 0;

        subLen = juce::jmax (1, (int) std::lround (fs * 0.1));
        subCount = 0;
        subSum = rmsSum = 0.0;
        subE.fill (0.0);
        rmsE.fill (0.0);
        subPos = rmsPos = subFilled = 0;

        corrA = (float) std::exp (-1.0 / (0.3 * fs));   // correlação com ~300 ms de memória
        corrB = 1.0f - corrA;
        cLR = cLL = cRR = 0.0f;

        momentary.store (silence);
        shortTerm.store (silence);
        rms.store (0.0f);
        corr.store (0.0f);
        resetFlag.store (false);
        resetMeasures();
    }

    // stereo = false: entrada mono (um canal só na soma do loudness, como pede a norma)
    void process (const float* inL, const float* inR, int n, bool stereo)
    {
        if (resetFlag.exchange (false))
            resetMeasures();

        int gPos = gonWrite.load (std::memory_order_relaxed);
        int fPos = fftWrite.load (std::memory_order_relaxed);
        float pk = 0.0f, tp = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float l = inL[i];
            const float r = stereo ? inR[i] : l;

            // ---- loudness (filtro K) e RMS ----
            const double kl = highPass[0].process (shelf[0].process ((double) l));
            double e  = kl * kl;
            double sq = (double) l * (double) l;
            if (stereo)
            {
                const double kr = highPass[1].process (shelf[1].process ((double) r));
                e += kr * kr;
                sq = 0.5 * (sq + (double) r * (double) r);
            }
            subSum += e;
            rmsSum += sq;

            // ---- pico e true peak ----
            pk = juce::jmax (pk, std::abs (l), std::abs (r));
            tpPos = (tpPos + tpTaps - 1) % tpTaps;
            tp = juce::jmax (tp, truePeakOf (0, l));
            if (stereo) tp = juce::jmax (tp, truePeakOf (1, r));

            // ---- correlação ----
            cLR = cLR * corrA + l * r * corrB;
            cLL = cLL * corrA + l * l * corrB;
            cRR = cRR * corrA + r * r * corrB;

            // ---- anéis da interface ----
            gonL[(size_t) gPos].store (l, std::memory_order_relaxed);
            gonR[(size_t) gPos].store (r, std::memory_order_relaxed);
            gPos = (gPos + 1) & (gonioLen - 1);
            fftRing[(size_t) fPos].store (0.5f * (l + r), std::memory_order_relaxed);
            fPos = (fPos + 1) & (fftRingLen - 1);

            if (++subCount >= subLen)
                finishSubBlock();
        }

        gonWrite.store (gPos, std::memory_order_release);
        fftWrite.store (fPos, std::memory_order_release);

        tpHeld = juce::jmax (tpHeld, tp);
        truePeakMax.store (tpHeld);
        peakNow.store (juce::jmax (peakNow.load(), pk));
        truePeakNow.store (juce::jmax (truePeakNow.load(), tp));
        const float den = std::sqrt (cLL * cRR);
        corr.store (den > 1.0e-9f ? juce::jlimit (-1.0f, 1.0f, cLR / den) : 0.0f);
    }

private:
    // interpolador 4× (seno janelado, 64 taps em 4 fases): as 4 posições entre amostras
    void designTruePeak()
    {
        constexpr int total = 4 * tpTaps;
        const double pi = juce::MathConstants<double>::pi;
        const double centre = (double) (total - 1) * 0.5;
        std::array<double, 4 * tpTaps> h {};
        for (int i = 0; i < total; ++i)
        {
            const double x = ((double) i - centre) / 4.0;
            const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double t = (double) i / (double) (total - 1);
            const double win = 0.42 - 0.5 * std::cos (2.0 * pi * t) + 0.08 * std::cos (4.0 * pi * t);   // Blackman
            h[(size_t) i] = sinc * win;
        }
        for (int p = 0; p < 4; ++p)
        {
            double sum = 0.0;
            for (int k = 0; k < tpTaps; ++k) sum += h[(size_t) (4 * k + p)];
            for (int k = 0; k < tpTaps; ++k)   // cada fase com ganho 1 em DC
                tpCoef[(size_t) (p * tpTaps + k)] = (float) (h[(size_t) (4 * k + p)] / sum);
        }
    }

    // maior |valor| entre a amostra e as 4 interpoladas (tpPos já andou para esta amostra)
    float truePeakOf (size_t ch, float x)
    {
        auto& b = tpBuf[ch];
        b[(size_t) tpPos] = x;
        b[(size_t) (tpPos + tpTaps)] = x;
        const float* w = b.data() + tpPos;   // w[k] = amostra de k passos atrás
        float m = std::abs (x);
        for (int p = 0; p < 4; ++p)
        {
            const float* c = tpCoef.data() + p * tpTaps;
            float acc = 0.0f;
            for (int k = 0; k < tpTaps; ++k) acc += c[k] * w[k];
            m = juce::jmax (m, std::abs (acc));
        }
        return m;
    }

    void resetMeasures()
    {
        gateI.clear();
        gateS.clear();
        tpHeld = 0.0f;
        shortHeld = silence;
        blocks = 0;
        integrated.store (silence);
        shortMax.store (silence);
        lra.store (0.0f);
        lraLo.store (silence);
        lraHi.store (silence);
        truePeakMax.store (0.0f);
        seconds.store (0.0f);
    }

    void finishSubBlock()
    {
        subE[(size_t) subPos] = subSum / (double) subLen;
        subPos = (subPos + 1) % shortSubs;
        subFilled = juce::jmin (shortSubs, subFilled + 1);
        rmsE[(size_t) rmsPos] = rmsSum / (double) subLen;
        rmsPos = (rmsPos + 1) % rmsSubs;
        subSum = rmsSum = 0.0;
        subCount = 0;

        // últimos 4 (momentâneo) e últimos 30 (curto) pedaços de 100 ms
        const int nm = juce::jmin (momSubs, subFilled);
        double mE = 0.0, sE = 0.0;
        for (int k = 0; k < subFilled; ++k)
        {
            const double v = subE[(size_t) ((subPos - 1 - k + shortSubs) % shortSubs)];
            sE += v;
            if (k < nm) mE += v;
        }
        mE /= (double) nm;
        sE /= (double) subFilled;

        const bool   fullM = subFilled >= momSubs, fullS = subFilled >= shortSubs;
        const double mL = energyToLufs (mE), sL = energyToLufs (sE);
        const float  mShow = fullM ? (float) mL : silence;
        const float  sShow = fullM ? (float) sL : silence;   // antes de 3 s o curto mostra o que já tem
        momentary.store (mShow);
        shortTerm.store (sShow);

        // integrado: blocos de 400 ms a cada 100 ms (75% de sobreposição), portão absoluto −70
        ++blocks;
        if (fullM && mL > -70.0)
        {
            gateI.add (mL, mE);
            integrated.store ((float) gatedLoudness (gateI, -10.0));
        }
        // LRA e máximo do curto: só com a janela de 3 s cheia
        if (fullS && sL > -70.0)
        {
            gateS.add (sL, sE);
            double lo = 0.0, hi = 0.0;
            const double range = loudnessRange (gateS, lo, hi);
            lra.store ((float) range);
            lraLo.store ((float) lo);
            lraHi.store ((float) hi);
            shortHeld = juce::jmax (shortHeld, (float) sL);
            shortMax.store (shortHeld);
        }
        seconds.store ((float) blocks * 0.1f);

        double r = 0.0;
        for (auto v : rmsE) r += v;
        rms.store ((float) std::sqrt (r / (double) rmsSubs));

        const int w = histWrite.load (std::memory_order_relaxed);
        histM[(size_t) w].store (mShow, std::memory_order_relaxed);
        histS[(size_t) w].store (sShow, std::memory_order_relaxed);
        histWrite.store ((w + 1) % historyLen, std::memory_order_release);
    }

    double fs = 44100.0;
    std::array<Biquad, 2> shelf, highPass;

    std::array<float, 4 * tpTaps> tpCoef {};
    std::array<std::array<float, 2 * tpTaps>, 2> tpBuf {};   // cada amostra gravada duas vezes: janela contínua
    int   tpPos = 0;
    float tpHeld = 0.0f, shortHeld = silence;

    int    subLen = 4410, subCount = 0;
    double subSum = 0.0, rmsSum = 0.0;
    std::array<double, shortSubs> subE {};
    std::array<double, rmsSubs>   rmsE {};
    int subPos = 0, rmsPos = 0, subFilled = 0, blocks = 0;

    float corrA = 0.999f, corrB = 0.001f, cLR = 0.0f, cLL = 0.0f, cRR = 0.0f;

    LoudnessHistogram gateI, gateS;   // blocos de 400 ms (integrado) e curtos de 3 s (LRA)
    std::atomic<bool> resetFlag { false };
};
} // namespace medidor
