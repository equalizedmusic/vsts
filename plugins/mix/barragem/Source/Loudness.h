#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace barragem
{
// ---------------- biquad em double (o passa-alta de 38 Hz precisa de precisão) ----------------
struct BiquadD
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0.0; }
};

// K-weighting da ITU-R BS.1770 (prateleira de agudo + passa-alta RLB), recalculado para qualquer taxa
inline void designKWeighting (double fs, BiquadD& shelf, BiquadD& hp)
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

// ---------------------------------------------------------------------------------------------
// LUFS (BS.1770-4 / EBU R128): blocos de 100 ms; MOMENTÂNEO = 400 ms, CURTO = 3 s,
// INTEGRADO = blocos de 400 ms (75% de sobreposição) com gate absoluto de −70 LUFS e relativo de −10 LU.
// O integrado guarda os blocos num histograma de 0,1 LU (memória fixa, mede horas sem crescer).
// ---------------------------------------------------------------------------------------------
class LoudnessMeter
{
public:
    static constexpr float silence = -100.0f;   // "sem leitura"

    std::atomic<float> momentary { silence }, shortTerm { silence }, integrated { silence };
    std::atomic<float> seconds { 0.0f };          // tempo medido desde o último ZERAR
    std::atomic<bool>  resetRequest { false };    // a interface pede, o áudio zera

    void prepare (double fs)
    {
        designKWeighting (fs, shelf[0], hp[0]);
        shelf[1] = shelf[0];
        hp[1] = hp[0];
        for (auto& f : shelf) f.reset();
        for (auto& f : hp) f.reset();

        blockLen = juce::jmax (1, (int) std::lround (fs * 0.1));
        acc = 0.0;
        count = 0;
        ring.fill (0.0);
        ringPos = 0;
        filled = 0;
        momentary.store (silence);
        shortTerm.store (silence);
        resetIntegrated();
    }

    void resetIntegrated()
    {
        binEnergy.fill (0.0);
        binCount.fill (0);
        blocks = 0;
        integrated.store (silence);
        seconds.store (0.0f);
    }

    void push (float l, float r) noexcept
    {
        const double a = hp[0].process (shelf[0].process ((double) l));
        const double b = hp[1].process (shelf[1].process ((double) r));
        acc += a * a + b * b;
        if (++count >= blockLen)
            finishBlock();
    }

private:
    static constexpr int ringSize = 30;    // 30 × 100 ms = 3 s
    static constexpr int numBins  = 750;   // −70 a +5 LUFS em passos de 0,1

    static float toLufs (double meanSquare)
    {
        return meanSquare > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (meanSquare)) : silence;
    }

    double meanOfLast (int k) const
    {
        double s = 0.0;
        for (int j = 0; j < k; ++j)
            s += ring[(size_t) ((ringPos - 1 - j + ringSize) % ringSize)];
        return s / (double) k;
    }

    void finishBlock()
    {
        ring[(size_t) ringPos] = acc / (double) blockLen;
        ringPos = (ringPos + 1) % ringSize;
        filled = juce::jmin (filled + 1, ringSize);
        acc = 0.0;
        count = 0;
        ++blocks;
        seconds.store ((float) blocks * 0.1f);

        if (filled >= 4)
        {
            const double ms = meanOfLast (4);
            const float  lm = toLufs (ms);
            momentary.store (lm);
            if (lm > -70.0f)
            {
                const int bin = juce::jlimit (0, numBins - 1, (int) std::floor ((lm + 70.0f) * 10.0f));
                binEnergy[(size_t) bin] += ms;
                binCount[(size_t) bin] += 1;
            }
        }
        shortTerm.store (filled >= ringSize ? toLufs (meanOfLast (ringSize)) : silence);
        integrated.store (computeIntegrated());
    }

    float computeIntegrated() const
    {
        double e = 0.0;
        juce::int64 n = 0;
        for (int b = 0; b < numBins; ++b)
        {
            e += binEnergy[(size_t) b];
            n += binCount[(size_t) b];
        }
        if (n == 0) return silence;

        const float rel   = toLufs (e / (double) n) - 10.0f;   // gate relativo
        const int   first = juce::jlimit (0, numBins - 1, (int) std::floor ((rel + 70.0f) * 10.0f));
        double e2 = 0.0;
        juce::int64 n2 = 0;
        for (int b = first; b < numBins; ++b)
        {
            e2 += binEnergy[(size_t) b];
            n2 += binCount[(size_t) b];
        }
        return n2 > 0 ? toLufs (e2 / (double) n2) : silence;
    }

    std::array<BiquadD, 2> shelf, hp;
    int    blockLen = 4800, count = 0, ringPos = 0, filled = 0;
    double acc = 0.0;
    std::array<double, ringSize> ring {};
    std::array<double, numBins> binEnergy {};
    std::array<juce::int64, numBins> binCount {};
    juce::int64 blocks = 0;
};
} // namespace barragem
