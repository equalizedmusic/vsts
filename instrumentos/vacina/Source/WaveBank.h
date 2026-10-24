#pragma once

// VACINA — banco de tabelas de onda (WAVE). As mesmas 24 fórmulas da QUIMERA, montadas aqui por conta própria
// (FFT simples, sem depender do juce_dsp): cada quadro vira um ESPECTRO e cada mip é a inversa desse espectro
// truncado no que cabe abaixo de Nyquist naquela oitava — nota aguda não dá aliasing.
// Montado na primeira vez que alguém pede, compartilhado por todas as instâncias, só leitura depois (~19 MB).

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vacina
{
inline constexpr int numTables = 24;

class WaveBank
{
public:
    static constexpr int size = 2048;     // amostras do quadro em banda cheia (mip 0)
    static constexpr int frames = 32;     // quadros por tabela (o WAVE/SHAPE anda entre eles)
    static constexpr int numMips = 11;    // mip m guarda só os harmônicos 1..harmonics(m)

    static constexpr int harmonics (int mip) { return (size / 2) >> mip; }
    static constexpr int mipSize (int mip)
    {
        return mip == 0 ? size : ((size >> mip) * 2 > 64 ? (size >> mip) * 2 : 64);
    }

    static const WaveBank& instance()
    {
        static const WaveBank bank;
        return bank;
    }

    const float* data (int table, int frameIndex, int mip) const
    {
        table = std::clamp (table, 0, numTables - 1);
        frameIndex = std::clamp (frameIndex, 0, frames - 1);
        mip = std::clamp (mip, 0, numMips - 1);
        return samples.data() + (size_t) (table * frames + frameIndex) * (size_t) perFrame + (size_t) mipOffset[(size_t) mip];
    }

    static int mipFor (double cyclesPerSample)
    {
        const double maxHarm = 0.5 / std::max (1.0e-9, cyclesPerSample);
        int m = 0;
        while (m < numMips - 1 && (double) harmonics (m) > maxHarm) ++m;
        return m;
    }

    // leitura com a posição (0..1) entre dois quadros vizinhos; t = fase 0..1
    inline float read (int table, float pos, float t, int mip) const
    {
        const float fp = std::clamp (pos, 0.0f, 1.0f) * (float) (frames - 1);
        const int f0 = std::min (frames - 2, (int) fp);
        const float ff = fp - (float) f0;
        const int n = mipSize (mip);
        const float x = t * (float) n;
        int i0 = (int) x;
        const float fr = x - (float) i0;
        i0 = i0 >= n ? i0 - n : i0;
        const int i1 = i0 + 1 >= n ? 0 : i0 + 1;
        const float* a = data (table, f0, mip);
        const float* b = a + perFrame;
        const float va = a[i0] + (a[i1] - a[i0]) * fr;
        const float vb = b[i0] + (b[i1] - b[i0]) * fr;
        return va + (vb - va) * ff;
    }

private:
    static constexpr int    maxH   = size / 2 - 1;
    static constexpr int    tdSize = size * 4;          // quadro no tempo, 4× sobreamostrado
    static constexpr double pi     = 3.14159265358979323846;
    static constexpr double twoPi  = 2.0 * pi;

    struct Spec
    {
        std::vector<double> a, b;
        Spec() : a ((size_t) maxH + 1, 0.0), b ((size_t) maxH + 1, 0.0) {}
        void clear() { std::fill (a.begin(), a.end(), 0.0); std::fill (b.begin(), b.end(), 0.0); }
        void addSin (int h, double amp, double phase = 0.0)
        {
            if (h < 1 || h > maxH) return;
            a[(size_t) h] += amp * std::sin (phase);
            b[(size_t) h] += amp * std::cos (phase);
        }
    };

    // FFT complexa radix-2 (in-place); inverse = sinal + no expoente, sem dividir por n
    static void fft (std::vector<std::complex<double>>& x, bool inverse)
    {
        const size_t n = x.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (x[i], x[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = twoPi / (double) len * (inverse ? 1.0 : -1.0);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const auto u = x[i + k], v = x[i + k + len / 2] * w;
                    x[i + k] = u + v;
                    x[i + k + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }

    static double hash01 (std::uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16;
        return (double) x / 4294967296.0;
    }
    static double lerp (double a, double b, double t) { return a + (b - a) * t; }
    static double smooth (double t)                   { return t * t * (3.0 - 2.0 * t); }
    static double sineB (int h)   { return h == 1 ? 1.0 : 0.0; }
    static double triB (int h)
    {
        if ((h & 1) == 0) return 0.0;
        const double sgn = (((h - 1) / 2) & 1) != 0 ? -1.0 : 1.0;
        return 8.0 / (pi * pi) * sgn / ((double) h * (double) h);
    }
    static double sawB (int h)    { return (2.0 / pi) * ((h & 1) != 0 ? 1.0 : -1.0) / (double) h; }
    static double squareB (int h) { return (h & 1) != 0 ? 4.0 / (pi * (double) h) : 0.0; }
    static double classicB (int shape, int h)
    {
        switch (shape) { case 0: return sineB (h); case 1: return triB (h); case 2: return sawB (h); default: return squareB (h); }
    }
    static double triT (double p)  { return p < 0.25 ? 4.0 * p : p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0; }
    static double foldT (double x) { double t = (x + 1.0) * 0.25; t -= std::floor (t); return 1.0 - 4.0 * std::abs (t - 0.5); }
    static double bendT (double p, double k) { return p * (1.0 + k) / (1.0 + k * p); }
    static double formantGain (double f, const double* F, const double* bw, const double* g, int n)
    {
        double s = 0.0;
        for (int i = 0; i < n; ++i) { const double d = (f - F[i]) / bw[i]; s += g[i] / (1.0 + d * d); }
        return s;
    }

    // quadro no tempo (tdSize amostras, um ciclo) → espectro
    void fromTime (const std::vector<double>& x, Spec& s)
    {
        for (int i = 0; i < tdSize; ++i) work[(size_t) i] = { x[(size_t) i], 0.0 };
        fft (work, false);
        s.a[0] = s.b[0] = 0.0;
        for (int h = 1; h <= maxH; ++h)
        {
            s.a[(size_t) h] =  2.0 * work[(size_t) h].real() / (double) tdSize;
            s.b[(size_t) h] = -2.0 * work[(size_t) h].imag() / (double) tdSize;
        }
    }

    // espectro → todas as mips do quadro, normalizado pelo pico da mip 0 (mesmo ganho em todas)
    void write (int table, int frame, const Spec& s)
    {
        float* base = samples.data() + (size_t) (table * frames + frame) * (size_t) perFrame;
        for (int m = 0; m < numMips; ++m)
        {
            const int n = mipSize (m);
            const int hMax = std::min (std::min (harmonics (m), n / 2 - 1), maxH);
            std::vector<std::complex<double>> X ((size_t) n);
            for (int h = 1; h <= hMax; ++h)
            {
                const std::complex<double> c (s.a[(size_t) h] * 0.5, -s.b[(size_t) h] * 0.5);
                X[(size_t) h] = c;
                X[(size_t) (n - h)] = std::conj (c);
            }
            fft (X, true);
            float* out = base + mipOffset[(size_t) m];
            for (int i = 0; i < n; ++i) out[i] = (float) X[(size_t) i].real();
        }
        float peak = 0.0f;
        for (int i = 0; i < size; ++i) peak = std::max (peak, std::abs (base[i]));
        const float g = peak > 1.0e-9f ? 1.0f / peak : 0.0f;
        for (int i = 0; i < perFrame; ++i) base[i] *= g;
    }

    WaveBank()
    {
        int off = 0;
        for (int m = 0; m < numMips; ++m) { mipOffset[(size_t) m] = off; off += mipSize (m); }
        perFrame = off;
        samples.assign ((size_t) numTables * (size_t) frames * (size_t) perFrame + (size_t) perFrame, 0.0f);
        work.assign ((size_t) tdSize, {});
        buildAll();
    }

    void buildAll()
    {
        Spec s;
        std::vector<double> x ((size_t) tdSize, 0.0);
        constexpr int F = frames;
        static const double vowelA[3] = { 730.0, 1090.0, 2440.0 }, vowelE[3] = { 530.0, 1840.0, 2480.0 },
                            vowelI[3] = { 270.0, 2290.0, 3010.0 }, vowelO[3] = { 570.0, 840.0, 2410.0 },
                            vowelU[3] = { 300.0, 870.0, 2240.0 };

        auto timeFrame = [&] (int table, int fr, auto&& fn)
        {
            for (int i = 0; i < tdSize; ++i) x[(size_t) i] = fn ((double) i / (double) tdSize);
            fromTime (x, s);
            write (table, fr, s);
        };

        for (int fr = 0; fr < F; ++fr)
        {
            const double t = (double) fr / (double) (F - 1);

            { // 0 Clássica: seno → triângulo → serra → quadrada
                s.clear();
                const double seg = t * 3.0;
                const int i = std::min (2, (int) seg);
                const double f = seg - (double) i;
                for (int h = 1; h <= maxH; ++h) s.b[(size_t) h] = lerp (classicB (i, h), classicB (i + 1, h), f);
                write (0, fr, s);
            }
            { // 1 Pulso: largura 50% → 5%
                s.clear();
                const double d = 0.5 - 0.45 * t;
                for (int h = 1; h <= maxH; ++h)
                {
                    const double k = 2.0 / (pi * (double) h);
                    s.a[(size_t) h] = k * std::sin (twoPi * h * d);
                    s.b[(size_t) h] = k * (1.0 - std::cos (twoPi * h * d));
                }
                write (1, fr, s);
            }
            { // 2 Serra Dupla
                s.clear();
                const double dl = 0.5 * (1.0 - t);
                for (int h = 1; h <= maxH; ++h)
                {
                    const double sb = sawB (h) * 0.5;
                    s.b[(size_t) h] = sb + sb * std::cos (twoPi * h * dl);
                    s.a[(size_t) h] = -sb * std::sin (twoPi * h * dl);
                }
                write (2, fr, s);
            }
            { const double g = 1.0 + 5.0 * t; timeFrame (3, fr, [g] (double p) { return foldT (triT (p) * g); }); }   // 3 Dobrado
            { // 4 Metal
                s.clear();
                const double e = 1.0 + 0.6 * t;
                for (int n = 1; n <= 64; ++n)
                {
                    const int hp = (int) std::lround (std::pow ((double) n, e));
                    if (hp > maxH) break;
                    const double amp = (1.0 + (n % 3 == 0 ? 0.6 * t : 0.0)) / std::pow ((double) n, 0.85);
                    s.addSin (hp, amp, n == 1 ? 0.0 : twoPi * hash01 ((std::uint32_t) n * 7919u + 4u));
                }
                write (4, fr, s);
            }
            { // 5 Formante A-E-I-O-U
                s.clear();
                const double* vw[5] = { vowelA, vowelE, vowelI, vowelO, vowelU };
                const double vp = t * 4.0;
                const int vi = std::min (3, (int) vp);
                const double vf = vp - (double) vi;
                double Fq[3];
                for (int b = 0; b < 3; ++b) Fq[b] = lerp (vw[vi][b], vw[vi + 1][b], vf);
                const double bw[3] = { 90.0, 110.0, 160.0 }, g[3] = { 1.0, 0.6, 0.3 };
                for (int h = 1; h <= maxH; ++h)
                {
                    const double f = 130.0 * h;
                    if (f > 12000.0) break;
                    s.addSin (h, (0.02 + formantGain (f, Fq, bw, g, 3)) / std::pow ((double) h, 0.6));
                }
                write (5, fr, s);
            }
            { const double r = 1.0 + 7.0 * t; timeFrame (6, fr, [r] (double p) { double q = p * r; q -= std::floor (q); return 2.0 * q - 1.0; }); }
            { const double k = 12.0 * t; timeFrame (7, fr, [k] (double p) { return std::sin (twoPi * bendT (p, k)); }); }
            { // 8 Elétrico
                s.clear();
                const double w = smooth (t);
                for (int h = 1; h <= maxH; ++h)
                {
                    const double boost = 1.0 + 2.5 * t * std::min (1.0, std::log2 ((double) h) / 5.0);
                    s.b[(size_t) h] = lerp (squareB (h), sawB (h), w) * boost;
                }
                write (8, fr, s);
            }
            { s.clear(); for (int h = 1; h <= maxH; ++h) s.b[(size_t) h] = lerp (squareB (h), sawB (h), t); write (9, fr, s); }
            { // 10 Seno saturando
                const double g = 1.0 + 30.0 * t * t, nrm = 1.0 / std::tanh (g);
                timeFrame (10, fr, [g, nrm] (double p) { return std::tanh (g * std::sin (twoPi * p)) * nrm; });
            }
            { // 11 Escova
                s.clear();
                const double period = 4.0 + 12.0 * t;
                for (int h = 1; h <= maxH; ++h)
                {
                    const double comb = 1.0 - 0.6 * t * (0.5 + 0.5 * std::cos (twoPi * h / period));
                    const double ph = t * twoPi * 0.8 * hash01 ((std::uint32_t) h * 31u + 11u);
                    s.addSin (h, (2.0 / (pi * h)) * comb, ph);
                }
                write (11, fr, s);
            }
            { // 12 Cristal
                s.clear();
                s.addSin (1, 0.7);
                const double sc = 1.5 + 4.0 * t;
                for (int j = 1; j <= 10; ++j)
                {
                    const int h = (int) std::lround (std::pow ((double) j + 1.0, 1.4) * sc);
                    s.addSin (h, 0.5 / std::pow ((double) j, 0.6), twoPi * hash01 ((std::uint32_t) j * 977u + 3u));
                }
                write (12, fr, s);
            }
            { // 13 Digital
                const double steps  = std::max (4.0, (double) std::lround (lerp (64.0, 4.0, t)));
                const double levels = std::max (2.0, (double) std::lround (lerp (32.0, 3.0, t)));
                timeFrame (13, fr, [steps, levels, t] (double p)
                {
                    const double q = std::floor (p * steps) / steps;
                    const double y = std::sin (twoPi * q) * (1.0 - 0.5 * t) + (2.0 * q - 1.0) * 0.5 * t;
                    return std::floor (y * levels + 0.5) / levels;
                });
            }
            { // 14 Vocal
                s.clear();
                const double* vw[4] = { vowelO, vowelA, vowelE, vowelI };
                const double vp = t * 3.0;
                const int vi = std::min (2, (int) vp);
                const double vf = vp - (double) vi;
                double Fq[4];
                for (int b = 0; b < 3; ++b) Fq[b] = lerp (vw[vi][b], vw[vi + 1][b], vf);
                Fq[3] = 2900.0;
                const double bw[4] = { 80.0, 100.0, 120.0, 220.0 }, g[4] = { 1.0, 0.7, 0.4, 0.35 };
                for (int h = 1; h <= maxH; ++h)
                {
                    const double f = 220.0 * h;
                    if (f > 12000.0) break;
                    const double jit = 0.8 + 0.4 * hash01 ((std::uint32_t) h * 4099u + 7u);
                    s.addSin (h, jit * (0.015 + formantGain (f, Fq, bw, g, 4)) / std::pow ((double) h, 0.8),
                              twoPi * hash01 ((std::uint32_t) h * 2207u + 19u));
                }
                write (14, fr, s);
            }
            { s.clear(); s.addSin (fr + 1, 1.0); write (15, fr, s); }   // 15 Harmônicos
            { // 16 Granito
                s.clear();
                const double kp = t * 4.0;
                const int k0 = std::min (3, (int) kp);
                const double kf = smooth (kp - (double) k0);
                auto rnd = [] (int h, int k)
                {
                    double acc = 0.0;
                    for (int d = -2; d <= 2; ++d) acc += hash01 ((std::uint32_t) (h + d + 8) * 13u + (std::uint32_t) k * 1013u);
                    return acc / 5.0;
                };
                for (int h = 1; h <= maxH; ++h)
                {
                    const double amp = lerp (rnd (h, k0), rnd (h, k0 + 1), kf);
                    s.addSin (h, (0.15 + amp) / std::pow ((double) h, 0.75), twoPi * hash01 ((std::uint32_t) h * 6151u + 5u));
                }
                write (16, fr, s);
            }
            { // 17 Sinos
                const double mf = 3.0 + 10.0 * t, m0 = std::floor (mf), mfr = mf - m0, idx = 2.0 + 2.0 * t;
                timeFrame (17, fr, [m0, mfr, idx] (double p)
                {
                    const double th = twoPi * p;
                    const double a = std::sin (2.0 * th + idx * std::sin (m0 * th));
                    const double b = std::sin (2.0 * th + idx * std::sin ((m0 + 1.0) * th));
                    return a + (b - a) * mfr + 0.4 * std::sin (th);
                });
            }
            { const double idx = 6.0 * t; timeFrame (18, fr, [idx] (double p) { const double th = twoPi * p; return std::sin (th + idx * std::sin (th)); }); }
            { // 19 FM Metálico
                const double idx = 5.0 * t;
                timeFrame (19, fr, [idx] (double p) { const double th = twoPi * p; return std::sin (2.0 * th + idx * std::sin (7.0 * th)) + 0.5 * std::sin (th); });
            }
            { // 20 Espectral
                s.clear();
                const double shift = 24.0 * t;
                for (int h = 1; h <= maxH; ++h)
                {
                    const double c = std::max (0.0, std::cos (twoPi * ((double) h - 1.0 - shift) / 8.0));
                    const double c4 = c * c * c * c;
                    s.addSin (h, (h == 1 ? 0.5 : 0.0) + c4 / std::sqrt ((double) h), twoPi * hash01 ((std::uint32_t) h * 3301u + 2u));
                }
                write (20, fr, s);
            }
            { // 21 Órgão
                static const int harm[7] = { 1, 2, 3, 4, 5, 6, 8 };
                static const int reg[8][7] = { { 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 0, 0, 0, 0, 0 }, { 8, 8, 8, 0, 0, 0, 0 },
                                               { 8, 6, 8, 6, 0, 0, 0 }, { 8, 8, 8, 8, 0, 0, 8 }, { 6, 8, 8, 6, 4, 4, 6 },
                                               { 8, 8, 8, 8, 8, 8, 8 }, { 4, 8, 6, 8, 6, 8, 8 } };
                auto lvl = [] (int d) { return d <= 0 ? 0.0 : std::pow (10.0, (double) (d - 8) * 3.0 / 20.0); };
                s.clear();
                const double kp = t * 7.0;
                const int k0 = std::min (6, (int) kp);
                const double kf = kp - (double) k0;
                for (int d = 0; d < 7; ++d) s.addSin (harm[d], lerp (lvl (reg[k0][d]), lvl (reg[k0 + 1][d]), kf));
                write (21, fr, s);
            }
            { // 22 Corda
                s.clear();
                const double damp = 0.004 + 0.12 * t;
                for (int h = 1; h <= maxH; ++h)
                {
                    const double amp = std::abs (std::sin (pi * h * 0.13)) / (double) h * std::exp (-(double) (h - 1) * damp);
                    if (amp < 1.0e-7) continue;
                    s.addSin (h, amp, 0.3 * twoPi * hash01 ((std::uint32_t) h * 1543u + 9u));
                }
                write (22, fr, s);
            }
            { // 23 Vacina: serra orgânica → FM metálica
                const double w = smooth (t), idx = 0.5 + 4.5 * t;
                timeFrame (23, fr, [w, idx] (double p)
                {
                    const double th = twoPi * p;
                    const double saw = 2.0 * p - 1.0;
                    const double fm = 0.7 * std::sin (2.0 * th + idx * std::sin (7.0 * th)) + 0.5 * std::sin (th + 0.3 * idx * std::sin (th));
                    return saw + (fm - saw) * w;
                });
            }
        }
    }

    std::vector<float> samples;
    std::array<int, numMips> mipOffset {};
    int perFrame = 0;
    std::vector<std::complex<double>> work;
};
} // namespace vacina
