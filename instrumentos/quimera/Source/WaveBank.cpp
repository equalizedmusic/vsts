#include "WaveBank.h"

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// Montagem do banco: cada quadro vira um ESPECTRO (cosseno a[h] + seno b[h], h = 1..1023) e cada mip é a FFT
// inversa desse espectro truncado em harmonics(m). Quadros definidos no tempo são gerados 4× sobreamostrados e
// passam por uma FFT direta antes (o aliasing da forma ingênua cai para ~−77 dB). Nada copiado: tudo é fórmula.

namespace quimera
{
namespace
{
constexpr int    maxH    = WaveBank::size / 2 - 1;   // 1023: o bin de Nyquist fica de fora
constexpr int    tdSize  = WaveBank::size * 4;       // quadro no tempo, 4× sobreamostrado
constexpr int    tdOrder = 13;
constexpr double pi      = 3.14159265358979323846;
constexpr double twoPi   = 2.0 * pi;

// espectro de um quadro: x(θ) = Σ a[h]·cos(hθ) + b[h]·sin(hθ)
struct Spec
{
    std::vector<double> a, b;
    Spec() : a ((size_t) maxH + 1, 0.0), b ((size_t) maxH + 1, 0.0) {}

    void clear()
    {
        std::fill (a.begin(), a.end(), 0.0);
        std::fill (b.begin(), b.end(), 0.0);
    }

    // amp·sin(hθ + fase)
    void addSin (int h, double amp, double phase = 0.0)
    {
        if (h < 1 || h > maxH) return;
        a[(size_t) h] += amp * std::sin (phase);
        b[(size_t) h] += amp * std::cos (phase);
    }
};

// ruído determinístico (mesma tabela em toda máquina)
inline double hash01 (std::uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return (double) x / 4294967296.0;
}

inline double lerp (double a, double b, double t) { return a + (b - a) * t; }
inline double smooth (double t)                   { return t * t * (3.0 - 2.0 * t); }

// formas clássicas como série de senos (coeficiente b[h])
inline double sineB (int h)   { return h == 1 ? 1.0 : 0.0; }
inline double triB (int h)
{
    if ((h & 1) == 0) return 0.0;
    const double sgn = (((h - 1) / 2) & 1) != 0 ? -1.0 : 1.0;
    return 8.0 / (pi * pi) * sgn / ((double) h * (double) h);
}
inline double sawB (int h)    { return (2.0 / pi) * ((h & 1) != 0 ? 1.0 : -1.0) / (double) h; }
inline double squareB (int h) { return (h & 1) != 0 ? 4.0 / (pi * (double) h) : 0.0; }

inline double classicB (int shape, int h)
{
    switch (shape)
    {
        case 0:  return sineB (h);
        case 1:  return triB (h);
        case 2:  return sawB (h);
        default: return squareB (h);
    }
}

// formas no tempo (fase p em 0..1)
inline double triT (double p)  { return p < 0.25 ? 4.0 * p : p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0; }
inline double foldT (double x) { double t = (x + 1.0) * 0.25; t -= std::floor (t); return 1.0 - 4.0 * std::abs (t - 0.5); }
inline double bendT (double p, double k) { return p * (1.0 + k) / (1.0 + k * p); }

// formantes (Hz) das vogais e a curva de ressonância
const double vowelA[3] = { 730.0, 1090.0, 2440.0 };
const double vowelE[3] = { 530.0, 1840.0, 2480.0 };
const double vowelI[3] = { 270.0, 2290.0, 3010.0 };
const double vowelO[3] = { 570.0, 840.0, 2410.0 };
const double vowelU[3] = { 300.0, 870.0, 2240.0 };

inline double formantGain (double f, const double* F, const double* bw, const double* g, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double d = (f - F[i]) / bw[i];
        s += g[i] / (1.0 + d * d);
    }
    return s;
}

class Builder
{
public:
    Builder (float* dest, const int* mipOff, int framePitch)
        : dst (dest), offs (mipOff), perFrame (framePitch)
    {
        for (int o = 6; o <= 11; ++o)
            fft[(size_t) o] = std::make_unique<juce::dsp::FFT> (o);
        fft[(size_t) tdOrder] = std::make_unique<juce::dsp::FFT> (tdOrder);
        work.assign ((size_t) tdSize * 2, 0.0f);

        // calibra a escala e o sentido da inversa (cada backend do JUCE tem a sua convenção)
        for (int o = 6; o <= 11; ++o)
        {
            const int n = 1 << o;
            clearWork (n);
            work[2] = 1.0f;                                   // bin 1 = cosseno
            fft[(size_t) o]->performRealOnlyInverseTransform (work.data());
            const float c = work[0];
            invScale[(size_t) o] = std::abs (c) > 1.0e-12f ? 1.0f / c : 1.0f;

            clearWork (n);
            work[3] = -1.0f;                                  // deve virar +seno
            fft[(size_t) o]->performRealOnlyInverseTransform (work.data());
            invSign[(size_t) o] = work[(size_t) (n / 4)] >= 0.0f ? 1.0f : -1.0f;
        }

        // e a direta: seno de amplitude 1 no bin 1
        clearWork (tdSize);
        for (int i = 0; i < tdSize; ++i)
            work[(size_t) i] = (float) std::sin (twoPi * (double) i / (double) tdSize);
        fft[(size_t) tdOrder]->performRealOnlyForwardTransform (work.data(), true);
        fwdK = -(double) work[3];
        if (std::abs (fwdK) < 1.0e-9) fwdK = 1.0;
    }

    // quadro no tempo (tdSize amostras, um ciclo) → espectro
    void fromTime (const std::vector<double>& x, Spec& s)
    {
        clearWork (tdSize);
        for (int i = 0; i < tdSize; ++i)
            work[(size_t) i] = (float) x[(size_t) i];
        fft[(size_t) tdOrder]->performRealOnlyForwardTransform (work.data(), true);
        const double ka = 1.0 / std::abs (fwdK), kb = -1.0 / fwdK;
        s.a[0] = s.b[0] = 0.0;
        for (int h = 1; h <= maxH; ++h)
        {
            s.a[(size_t) h] = (double) work[(size_t) (2 * h)] * ka;
            s.b[(size_t) h] = (double) work[(size_t) (2 * h + 1)] * kb;
        }
    }

    // espectro → todas as mips do quadro, normalizado pelo pico da mip 0 (mesmo ganho em todas)
    void write (int table, int frame, const Spec& s)
    {
        float* base = dst + (size_t) (table * WaveBank::frames + frame) * (size_t) perFrame;
        for (int m = 0; m < WaveBank::numMips; ++m)
        {
            const int n = WaveBank::mipSize (m);
            const int o = orderOf (n);
            const int hMax = std::min (std::min (WaveBank::harmonics (m), n / 2 - 1), maxH);
            clearWork (n);
            const float sg = invSign[(size_t) o];
            for (int h = 1; h <= hMax; ++h)
            {
                work[(size_t) (2 * h)]     = (float) s.a[(size_t) h];
                work[(size_t) (2 * h + 1)] = (float) (-s.b[(size_t) h]) * sg;
            }
            fft[(size_t) o]->performRealOnlyInverseTransform (work.data());
            float* out = base + offs[m];
            const float k = invScale[(size_t) o];
            for (int i = 0; i < n; ++i)
                out[i] = work[(size_t) i] * k;
        }

        float peak = 0.0f;
        for (int i = 0; i < WaveBank::size; ++i)
            peak = std::max (peak, std::abs (base[i]));
        const float g = peak > 1.0e-9f ? 1.0f / peak : 0.0f;
        for (int i = 0; i < perFrame; ++i)
            base[i] *= g;
    }

private:
    static int orderOf (int n)
    {
        int o = 0;
        while ((1 << o) < n) ++o;
        return o;
    }

    void clearWork (int n) { std::fill (work.begin(), work.begin() + (std::ptrdiff_t) (2 * n), 0.0f); }

    float* dst;
    const int* offs;
    int perFrame;
    std::array<std::unique_ptr<juce::dsp::FFT>, 14> fft;
    std::array<float, 14> invScale {}, invSign {};
    double fwdK = 1.0;
    std::vector<float> work;
};

// ---------------- as 24 tabelas ----------------
void buildAll (Builder& B)
{
    Spec s;
    std::vector<double> x ((size_t) tdSize, 0.0);
    constexpr int F = WaveBank::frames;

    // quadro definido no tempo: fn(p) com p = fase 0..1
    auto timeFrame = [&] (int table, int fr, auto&& fn)
    {
        for (int i = 0; i < tdSize; ++i)
            x[(size_t) i] = fn ((double) i / (double) tdSize);
        B.fromTime (x, s);
        B.write (table, fr, s);
    };

    for (int fr = 0; fr < F; ++fr)
    {
        const double t = (double) fr / (double) (F - 1);

        // 0 Clássica: seno → triângulo → serra → quadrada
        {
            s.clear();
            const double seg = t * 3.0;
            const int i = std::min (2, (int) seg);
            const double f = seg - (double) i;
            for (int h = 1; h <= maxH; ++h)
                s.b[(size_t) h] = lerp (classicB (i, h), classicB (i + 1, h), f);
            B.write (0, fr, s);
        }

        // 1 Pulso: largura 50% → 5%
        {
            s.clear();
            const double d = 0.5 - 0.45 * t;
            for (int h = 1; h <= maxH; ++h)
            {
                const double k = 2.0 / (pi * (double) h);
                s.a[(size_t) h] = k * std::sin (twoPi * h * d);
                s.b[(size_t) h] = k * (1.0 - std::cos (twoPi * h * d));
            }
            B.write (1, fr, s);
        }

        // 2 Serra Dupla: a 2ª serra sai de meio ciclo (oitava, oca) e chega em fase (cheia)
        {
            s.clear();
            const double dl = 0.5 * (1.0 - t);
            for (int h = 1; h <= maxH; ++h)
            {
                const double sb = sawB (h) * 0.5;
                s.b[(size_t) h] = sb + sb * std::cos (twoPi * h * dl);
                s.a[(size_t) h] = -sb * std::sin (twoPi * h * dl);
            }
            B.write (2, fr, s);
        }

        // 3 Triângulo Dobrado: wavefolder com ganho crescente
        {
            const double g = 1.0 + 5.0 * t;
            timeFrame (3, fr, [g] (double p) { return foldT (triT (p) * g); });
        }

        // 4 Metal: série harmônica esticada (parciais n^e arredondados), fases fixas sorteadas
        {
            s.clear();
            const double e = 1.0 + 0.6 * t;
            for (int n = 1; n <= 64; ++n)
            {
                const int hp = (int) std::lround (std::pow ((double) n, e));
                if (hp > maxH) break;
                const double amp = (1.0 + (n % 3 == 0 ? 0.6 * t : 0.0)) / std::pow ((double) n, 0.85);
                s.addSin (hp, amp, n == 1 ? 0.0 : twoPi * hash01 ((std::uint32_t) n * 7919u + 4u));
            }
            B.write (4, fr, s);
        }

        // 5 Formante: fonte rica × ressonâncias das vogais A-E-I-O-U (fixas no harmônico, como uma tabela)
        {
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
            B.write (5, fr, s);
        }

        // 6 Sincronizado: escrava serra de 1× a 8× com reset no ciclo
        {
            const double r = 1.0 + 7.0 * t;
            timeFrame (6, fr, [r] (double p) { double q = p * r; q -= std::floor (q); return 2.0 * q - 1.0; });
        }

        // 7 Dobra: distorção de fase crescente (o seno corre no começo e se arrasta no fim)
        {
            const double k = 12.0 * t;
            timeFrame (7, fr, [k] (double p) { return std::sin (twoPi * bendT (p, k)); });
        }

        // 8 Elétrico: quadrada → serra com agudos reforçados
        {
            s.clear();
            const double w = smooth (t);
            for (int h = 1; h <= maxH; ++h)
            {
                const double boost = 1.0 + 2.5 * t * std::min (1.0, std::log2 ((double) h) / 5.0);
                s.b[(size_t) h] = lerp (squareB (h), sawB (h), w) * boost;
            }
            B.write (8, fr, s);
        }

        // 9 Quadrada-Serra: os pares entram aos poucos
        {
            s.clear();
            for (int h = 1; h <= maxH; ++h)
                s.b[(size_t) h] = lerp (squareB (h), sawB (h), t);
            B.write (9, fr, s);
        }

        // 10 Seno-Quadrada: seno saturando
        {
            const double g = 1.0 + 30.0 * t * t;
            const double nrm = 1.0 / std::tanh (g);
            timeFrame (10, fr, [g, nrm] (double p) { return std::tanh (g * std::sin (twoPi * p)) * nrm; });
        }

        // 11 Escova: serra com pente de amplitude e fases que se espalham (chiado)
        {
            s.clear();
            const double period = 4.0 + 12.0 * t;
            for (int h = 1; h <= maxH; ++h)
            {
                const double comb = 1.0 - 0.6 * t * (0.5 + 0.5 * std::cos (twoPi * h / period));
                const double ph = t * twoPi * 0.8 * hash01 ((std::uint32_t) h * 31u + 11u);
                s.addSin (h, (2.0 / (pi * h)) * comb, ph);
            }
            B.write (11, fr, s);
        }

        // 12 Cristal: fundamental + parciais altos e esparsos que sobem
        {
            s.clear();
            s.addSin (1, 0.7);
            const double sc = 1.5 + 4.0 * t;
            for (int j = 1; j <= 10; ++j)
            {
                const int h = (int) std::lround (std::pow ((double) j + 1.0, 1.4) * sc);
                s.addSin (h, 0.5 / std::pow ((double) j, 0.6), twoPi * hash01 ((std::uint32_t) j * 977u + 3u));
            }
            B.write (12, fr, s);
        }

        // 13 Digital: degraus no tempo e na amplitude, cada vez mais grossos
        {
            const double steps  = std::max (4.0, (double) std::lround (lerp (64.0, 4.0, t)));
            const double levels = std::max (2.0, (double) std::lround (lerp (32.0, 3.0, t)));
            timeFrame (13, fr, [steps, levels, t] (double p)
            {
                const double q = std::floor (p * steps) / steps;
                const double y = std::sin (twoPi * q) * (1.0 - 0.5 * t) + (2.0 * q - 1.0) * 0.5 * t;
                return std::floor (y * levels + 0.5) / levels;
            });
        }

        // 14 Vocal: coro O → A → E → I com formante do cantor e fases soltas
        {
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
            B.write (14, fr, s);
        }

        // 15 Harmônicos: o harmônico (quadro+1) sozinho
        {
            s.clear();
            s.addSin (fr + 1, 1.0);
            B.write (15, fr, s);
        }

        // 16 Granito: 5 espectros aleatórios suavizados, interpolados ao longo da posição (fases fixas)
        {
            s.clear();
            const double kp = t * 4.0;
            const int k0 = std::min (3, (int) kp);
            const double kf = smooth (kp - (double) k0);
            auto rnd = [] (int h, int k)
            {
                double acc = 0.0;
                for (int d = -2; d <= 2; ++d)
                    acc += hash01 ((std::uint32_t) (h + d + 8) * 13u + (std::uint32_t) k * 1013u);
                return acc / 5.0;
            };
            for (int h = 1; h <= maxH; ++h)
            {
                const double amp = lerp (rnd (h, k0), rnd (h, k0 + 1), kf);
                s.addSin (h, (0.15 + amp) / std::pow ((double) h, 0.75), twoPi * hash01 ((std::uint32_t) h * 6151u + 5u));
            }
            B.write (16, fr, s);
        }

        // 17 Sinos: FM com portadora no 2º harmônico e moduladora de razão crescente (cruzando entre inteiros)
        {
            const double mf = 3.0 + 10.0 * t;
            const double m0 = std::floor (mf), mfr = mf - m0;
            const double idx = 2.0 + 2.0 * t;
            timeFrame (17, fr, [m0, mfr, idx] (double p)
            {
                const double th = twoPi * p;
                const double a = std::sin (2.0 * th + idx * std::sin (m0 * th));
                const double b = std::sin (2.0 * th + idx * std::sin ((m0 + 1.0) * th));
                return a + (b - a) * mfr + 0.4 * std::sin (th);
            });
        }

        // 18 FM Linear: razão 1, índice 0 → 6
        {
            const double idx = 6.0 * t;
            timeFrame (18, fr, [idx] (double p) { const double th = twoPi * p; return std::sin (th + idx * std::sin (th)); });
        }

        // 19 FM Metálico: razão 3,5 (portadora 2, moduladora 7) + fundamental para a altura não subir
        {
            const double idx = 5.0 * t;
            timeFrame (19, fr, [idx] (double p)
            {
                const double th = twoPi * p;
                return std::sin (2.0 * th + idx * std::sin (7.0 * th)) + 0.5 * std::sin (th);
            });
        }

        // 20 Espectral: pente de dentes a cada 8 harmônicos que anda com a posição
        {
            s.clear();
            const double shift = 24.0 * t;
            for (int h = 1; h <= maxH; ++h)
            {
                const double c = std::max (0.0, std::cos (twoPi * ((double) h - 1.0 - shift) / 8.0));
                const double c4 = c * c * c * c;
                s.addSin (h, (h == 1 ? 0.5 : 0.0) + c4 / std::sqrt ((double) h), twoPi * hash01 ((std::uint32_t) h * 3301u + 2u));
            }
            B.write (20, fr, s);
        }

        // 21 Órgão: 8 registros de drawbar (8' 4' 2⅔' 2' 1⅗' 1⅓' 1'), 3 dB por degrau
        {
            static const int harm[7] = { 1, 2, 3, 4, 5, 6, 8 };
            static const int reg[8][7] = { { 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 0, 0, 0, 0, 0 }, { 8, 8, 8, 0, 0, 0, 0 },
                                           { 8, 6, 8, 6, 0, 0, 0 }, { 8, 8, 8, 8, 0, 0, 8 }, { 6, 8, 8, 6, 4, 4, 6 },
                                           { 8, 8, 8, 8, 8, 8, 8 }, { 4, 8, 6, 8, 6, 8, 8 } };
            auto lvl = [] (int d) { return d <= 0 ? 0.0 : std::pow (10.0, (double) (d - 8) * 3.0 / 20.0); };
            s.clear();
            const double kp = t * 7.0;
            const int k0 = std::min (6, (int) kp);
            const double kf = kp - (double) k0;
            for (int d = 0; d < 7; ++d)
                s.addSin (harm[d], lerp (lvl (reg[k0][d]), lvl (reg[k0 + 1][d]), kf));
            B.write (21, fr, s);
        }

        // 22 Corda: espectro de corda dedilhada (pente do ponto de ataque); o brilho cai ao longo
        {
            s.clear();
            const double damp = 0.004 + 0.12 * t;
            for (int h = 1; h <= maxH; ++h)
            {
                const double amp = std::abs (std::sin (pi * h * 0.13)) / (double) h * std::exp (-(double) (h - 1) * damp);
                if (amp < 1.0e-7) continue;
                s.addSin (h, amp, 0.3 * twoPi * hash01 ((std::uint32_t) h * 1543u + 9u));
            }
            B.write (22, fr, s);
        }

        // 23 Quimera: serra orgânica → FM metálica
        {
            const double w = smooth (t);
            const double idx = 0.5 + 4.5 * t;
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
} // namespace

WaveBank::WaveBank()
{
    int off = 0;
    for (int m = 0; m < numMips; ++m)
    {
        mipOffset[m] = off;
        off += mipSize (m);
    }
    perFrame = off;
    samples.assign ((size_t) numTables * (size_t) frames * (size_t) perFrame, 0.0f);

    Builder b (samples.data(), mipOffset, perFrame);
    buildAll (b);
}

const WaveBank& WaveBank::instance()
{
    static const WaveBank bank;
    return bank;
}
} // namespace quimera
