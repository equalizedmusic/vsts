#pragma once
// CURVA — motor sem JUCE (dá para testar fora do plugin: tools/teste_curva.cpp).
//  - iso226: curvas de mesma audibilidade (ISO 226:2003, Fletcher-Munson modernas).
//  - compensacao(): quanto cada frequência precisa subir/descer para quem ouve a LISTEN phon perceber o equilíbrio
//    que ouviria a TARGET phon. Normalizada em 1 kHz (lá a diferença é zero por definição).
//  - faseMinima(): FIR de fase mínima com essa resposta (latência zero, sem pré-eco).
//  - Loudness (ITU-R BS.1770-4 / EBU R128: M, S, I, LRA), TruePeak (4x) e Correlacao.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace curva
{
constexpr double pi = 3.14159265358979323846;

// ───────────────────────────── ISO 226:2003 ─────────────────────────────
namespace iso226
{
constexpr int N = 29;
constexpr double f[N]  = { 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000,
                           1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500 };
constexpr double af[N] = { 0.532, 0.506, 0.480, 0.455, 0.432, 0.409, 0.387, 0.367, 0.349, 0.330, 0.315, 0.301, 0.288,
                           0.276, 0.267, 0.259, 0.253, 0.250, 0.246, 0.244, 0.243, 0.243, 0.243, 0.242, 0.242, 0.245,
                           0.254, 0.271, 0.301 };
constexpr double Lu[N] = { -31.6, -27.2, -23.0, -19.1, -15.9, -13.0, -10.3, -8.1, -6.2, -4.5, -3.1, -2.0, -1.1, -0.4,
                           0.0, 0.3, 0.5, 0.0, -2.7, -4.1, -1.0, 1.7, 2.5, 1.2, -2.1, -7.1, -11.2, -10.7, -3.1 };
constexpr double Tf[N] = { 78.5, 68.7, 59.5, 51.1, 44.0, 37.5, 31.5, 26.5, 22.1, 17.9, 14.4, 11.4, 8.6, 6.2, 4.4, 3.0,
                           2.2, 2.4, 3.5, 1.7, -1.3, -4.2, -6.0, -5.4, -1.5, 6.0, 12.6, 13.9, 12.3 };

// nível (dB SPL) que soa como `phon` na frequência f[i] da tabela
inline double splTabela (int i, double phon)
{
    const double Af = 4.47e-3 * (std::pow (10.0, 0.025 * phon) - 1.15)
                    + std::pow (0.4 * std::pow (10.0, (Tf[i] + Lu[i]) / 10.0 - 9.0), af[i]);
    return (10.0 / af[i]) * std::log10 (Af) - Lu[i] + 94.0;
}

// interpolação Catmull-Rom em log(f) de um valor tabelado nos 29 pontos (fora da tabela: segura a ponta)
template <typename V>
double interpola (double hz, V valor)
{
    if (hz <= f[0]) return valor (0);
    if (hz >= f[N - 1]) return valor (N - 1);
    const double x = std::log (hz);
    int i = 0;
    while (i < N - 2 && std::log (f[i + 1]) < x) ++i;
    const double x0 = std::log (f[i]), x1 = std::log (f[i + 1]);
    const double t = (x - x0) / (x1 - x0);
    const double p1 = valor (i), p2 = valor (i + 1);
    const double p0 = i > 0 ? valor (i - 1) : 2 * p1 - p2;
    const double p3 = i + 2 < N ? valor (i + 2) : 2 * p2 - p1;
    return 0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t);
}

// contorno de `phon` relativo a 1 kHz (dB): quanto mais nível o ouvido pede em hz do que em 1 kHz
inline double contorno (double hz, double phon)
{
    return interpola (hz, [phon] (int i) { return splTabela (i, phon) - phon; });
}
} // namespace iso226

// faixa dos knobs
constexpr double listenMin = 40.0, listenMax = 90.0, targetMin = 60.0, targetMax = 90.0;
constexpr double ganhoMax = 20.0, cortaMax = -12.0;   // a correção nunca passa disso

struct Ajuste
{
    double listen = 60.0, target = 83.0, amount = 1.0;
    bool operator== (const Ajuste& o) const { return listen == o.listen && target == o.target && amount == o.amount; }
    bool operator!= (const Ajuste& o) const { return ! (*this == o); }
};

// correção (dB) em hz: contorno do volume de escuta − contorno do alvo, dosada pelo AMOUNT.
// Abaixo de 20 Hz desce até 0 em 10 Hz (não empurra subsônico nos falantes); acima de 12,5 kHz segura o valor.
inline double compensacao (double hz, const Ajuste& a)
{
    double g = iso226::contorno (hz, a.listen) - iso226::contorno (hz, a.target);
    if (hz < 20.0)
    {
        const double t = std::clamp ((hz - 10.0) / 10.0, 0.0, 1.0);
        g *= t * t * (3 - 2 * t);
    }
    return std::clamp (g, cortaMax, ganhoMax) * std::clamp (a.amount, 0.0, 1.0);
}

// AUTO GAIN: ganho que deixa ruído rosa (energia igual por oitava, 40 Hz–12 kHz) com a mesma energia antes e depois
inline double autoGanhoDb (const Ajuste& a)
{
    double s = 0; int n = 0;
    for (double oit = std::log2 (40.0); oit <= std::log2 (12000.0); oit += 1.0 / 24.0, ++n)
        s += std::pow (10.0, compensacao (std::pow (2.0, oit), a) / 10.0);
    return n > 0 ? -10.0 * std::log10 (s / n) : 0.0;
}

// ───────────────────────────── FFT (radix 2) ─────────────────────────────
inline void fft (std::vector<std::complex<double>>& x, bool inversa)
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
        const double ang = 2 * pi / (double) len * (inversa ? 1 : -1);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = x[i + j], v = x[i + j + len / 2] * w;
                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inversa) for (auto& v : x) v /= (double) n;
}

// FIR de fase mínima (método do cepstro) com a magnitude ganhoDb(hz). taps = tamanho do FIR (potência de 2).
template <typename G>
std::vector<float> faseMinima (double fs, int taps, G ganhoDb, double ganhoGeralDb = 0.0)
{
    const int n = taps * 4;   // grade 4x mais fina que o FIR: o grave (20–60 Hz) sai com resolução
    std::vector<std::complex<double>> x ((size_t) n);
    for (int k = 0; k <= n / 2; ++k)
    {
        const double hz = std::max (1.0, k * fs / n);
        const double db = ganhoDb (hz) + ganhoGeralDb;
        x[(size_t) k] = db * std::log (10.0) / 20.0;   // ln|H|
        if (k > 0 && k < n / 2) x[(size_t) (n - k)] = x[(size_t) k];
    }
    fft (x, true);   // cepstro real
    for (int i = 1; i < n / 2; ++i) { x[(size_t) i] *= 2.0; x[(size_t) (n - i)] = 0.0; }
    fft (x, false);
    for (auto& v : x) v = std::exp (v);
    fft (x, true);
    std::vector<float> h ((size_t) taps);
    const int fade = taps / 4;
    for (int i = 0; i < taps; ++i)
    {
        double w = 1.0;
        if (i >= taps - fade) w = 0.5 * (1 + std::cos (pi * (i - (taps - fade)) / fade));
        h[(size_t) i] = (float) (x[(size_t) i].real() * w);
    }
    return h;
}

// magnitude (dB) de um FIR em hz (para teste e conferência)
inline double respostaDb (const std::vector<float>& h, double fs, double hz)
{
    std::complex<double> s = 0;
    const double w = 2 * pi * hz / fs;
    for (size_t i = 0; i < h.size(); ++i) s += (double) h[i] * std::polar (1.0, -w * (double) i);
    return 20 * std::log10 (std::max (1e-12, std::abs (s)));
}

inline int tapsPara (double fs)
{
    int t = 1024;
    while (t < fs * 0.085) t <<= 1;   // ~85 ms: 4096 a 44,1/48 kHz
    return t;
}

// ───────────────────────────── medidores ─────────────────────────────
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
    double proc (double x, int c)
    {
        const double y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    void zera() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

// Loudness BS.1770-4: K-weighting, M (400 ms), S (3 s), I (com gates -70 / -10) e LRA (EBU Tech 3342).
class Loudness
{
public:
    static constexpr double semSinal = -200.0;
    std::atomic<float> momentary { -200.0f }, shortTerm { -200.0f }, integrado { -200.0f }, lra { 0.0f },
                       maxMomentary { -200.0f }, maxShort { -200.0f };

    void prepara (double fs)
    {
        taxa = fs;
        // estágio 1: shelf de cabeça (+4 dB acima de ~1,7 kHz)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            s1.b0 = (Vh + Vb * K / Q + K * K) / a0; s1.b1 = 2.0 * (K * K - Vh) / a0; s1.b2 = (Vh - Vb * K / Q + K * K) / a0;
            s1.a1 = 2.0 * (K * K - 1.0) / a0;       s1.a2 = (1.0 - K / Q + K * K) / a0;
        }
        // estágio 2: passa-alta RLB (~38 Hz)
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            s2.b0 = 1.0; s2.b1 = -2.0; s2.b2 = 1.0;
            s2.a1 = 2.0 * (K * K - 1.0) / a0; s2.a2 = (1.0 - K / Q + K * K) / a0;
        }
        passo = std::max (1, (int) std::lround (fs * 0.1));   // blocos de 100 ms
        zera();
    }

    void zera()
    {
        s1.zera(); s2.zera();
        acum = 0; nAcum = 0; nBlocos = 0; idx = 0;
        blocos.fill (0.0);
        histI.fill (0); energiaI.fill (0); histS.fill (0);
        zeraPedido = false;
        momentary = shortTerm = integrado = (float) semSinal; lra = 0; maxMomentary = maxShort = (float) semSinal;
    }

    void pedeZerar() { zeraPedido = true; }   // botão RESET (thread da tela): zera no próximo bloco de áudio

    // ch: 1 ou 2 canais (pesos 1,0 — L/R do BS.1770)
    void processa (const float* const* ch, int nch, int n)
    {
        if (zeraPedido.load()) zera();
        for (int i = 0; i < n; ++i)
        {
            double e = 0;
            for (int c = 0; c < std::min (nch, 2); ++c)
            {
                const double y = s2.proc (s1.proc ((double) ch[c][i], c), c);
                e += y * y;
            }
            if (nch == 1) e *= 2.0;   // mono tocado nos dois falantes
            acum += e;
            if (++nAcum >= passo) fechaBloco();
        }
    }

private:
    static constexpr int nHist = 1000;   // 0,1 LU de -70 a +30
    static double paraLufs (double ms) { return ms > 0 ? -0.691 + 10.0 * std::log10 (ms) : semSinal; }
    static int bin (double l) { return std::clamp ((int) std::floor ((l + 70.0) * 10.0), 0, nHist - 1); }
    static double deBin (int b) { return -70.0 + (b + 0.5) * 0.1; }

    void fechaBloco()
    {
        blocos[(size_t) idx] = acum / nAcum;
        idx = (idx + 1) % 30;
        acum = 0; nAcum = 0;
        ++nBlocos;
        auto media = [this] (int k)
        {
            double s = 0;
            for (int j = 1; j <= k; ++j) s += blocos[(size_t) ((idx - j + 30) % 30)];
            return s / k;
        };
        const double msM = nBlocos >= 4 ? media (4) : media ((int) nBlocos) * nBlocos / 4.0;
        const double lM = paraLufs (msM);
        momentary = (float) lM;
        if (lM > maxMomentary.load()) maxMomentary = (float) lM;
        if (nBlocos >= 4 && lM > -70.0) { histI[(size_t) bin (lM)]++; energiaI[(size_t) bin (lM)] += msM; }
        // integrado: gate absoluto -70, relativo -10 LU
        {
            double s = 0; int64_t c = 0;
            for (int b = 0; b < nHist; ++b) { s += energiaI[(size_t) b]; c += histI[(size_t) b]; }
            if (c > 0)
            {
                const double gate = paraLufs (s / c) - 10.0;
                double s2e = 0; int64_t c2 = 0;
                for (int b = std::max (0, bin (gate)); b < nHist; ++b) { s2e += energiaI[(size_t) b]; c2 += histI[(size_t) b]; }
                integrado = (float) (c2 > 0 ? paraLufs (s2e / c2) : semSinal);
            }
        }
        if (nBlocos >= 30)
        {
            const double lS = paraLufs (media (30));
            shortTerm = (float) lS;
            if (lS > maxShort.load()) maxShort = (float) lS;
            if (lS > -70.0) histS[(size_t) bin (lS)]++;
            // LRA: S acima de -70 e de (média de S) − 20 LU; 95º − 10º percentil
            double se = 0; int64_t c = 0;
            for (int b = 0; b < nHist; ++b) if (histS[(size_t) b] > 0) { se += histS[(size_t) b] * std::pow (10.0, (deBin (b) + 0.691) / 10.0); c += histS[(size_t) b]; }
            if (c > 0)
            {
                const int b0 = std::max (0, bin (paraLufs (se / c) - 20.0));
                int64_t tot = 0;
                for (int b = b0; b < nHist; ++b) tot += histS[(size_t) b];
                if (tot > 0)
                {
                    const int64_t q10 = (int64_t) std::ceil (tot * 0.10), q95 = (int64_t) std::ceil (tot * 0.95);
                    int64_t acc = 0; int p10 = b0, p95 = b0; bool a10 = false;
                    for (int b = b0; b < nHist; ++b)
                    {
                        acc += histS[(size_t) b];
                        if (! a10 && acc >= q10) { p10 = b; a10 = true; }
                        if (acc >= q95) { p95 = b; break; }
                    }
                    lra = (float) ((p95 - p10) * 0.1);
                }
            }
        }
        else shortTerm = (float) paraLufs (media ((int) nBlocos) * nBlocos / 30.0);
    }

    Biquad s1, s2;
    double taxa = 48000, acum = 0;
    int nAcum = 0, passo = 4800, idx = 0;
    int64_t nBlocos = 0;
    std::array<double, 30> blocos {};
    std::array<int64_t, nHist> histI {}, histS {};
    std::array<double, nHist> energiaI {};
    std::atomic<bool> zeraPedido { false };
};

// True peak: interpola 4x (FIR polifásico, sinc com janela de Kaiser-ish, 12 taps por fase) e pega o maior |x|
class TruePeak
{
public:
    TruePeak()
    {
        constexpr int L = 4, T = 12;
        for (int p = 0; p < L; ++p)
            for (int k = 0; k < T; ++k)
            {
                const double t = (k - T / 2 + 1) - p / (double) L;   // atraso fracionário
                const double s = std::abs (t) < 1e-9 ? 1.0 : std::sin (pi * t) / (pi * t);
                const double w = 0.5 * (1 + std::cos (pi * t / (T / 2.0)));
                c[p][k] = std::abs (t) < T / 2.0 ? s * w : 0.0;
            }
    }
    void zera() { for (auto& h : hist) h.fill (0.0); pos = 0; }
    // devolve o pico verdadeiro (linear) do bloco
    float processa (const float* const* ch, int nch, int n)
    {
        double m = 0;
        for (int i = 0; i < n; ++i)
        {
            pos = (pos + 1) % 12;
            for (int ci = 0; ci < std::min (nch, 2); ++ci)
            {
                hist[(size_t) ci][(size_t) pos] = ch[ci][i];
                m = std::max (m, (double) std::abs (ch[ci][i]));
                for (int p = 1; p < 4; ++p)
                {
                    double y = 0;
                    for (int k = 0; k < 12; ++k) y += c[p][k] * hist[(size_t) ci][(size_t) ((pos - k + 12) % 12)];
                    m = std::max (m, std::abs (y));
                }
            }
        }
        return (float) m;
    }

private:
    double c[4][12] {};
    std::array<std::array<double, 12>, 2> hist {};
    int pos = 0;
};

// limitador de segurança (SAFE): teto em -0,3 dBFS, ataque imediato, solta em ~120 ms. Só protege o monitor.
class Seguranca
{
public:
    void prepara (double fs) { solta = std::exp (-1.0 / (0.12 * fs)); g = 1.0; }
    float reducaoDb() const { return (float) (20.0 * std::log10 (std::max (1e-6, gMin))); }
    void processa (float* const* ch, int nch, int n)
    {
        constexpr double teto = 0.9661;   // -0,3 dBFS
        gMin = 1.0;
        for (int i = 0; i < n; ++i)
        {
            double p = 0;
            for (int c = 0; c < nch; ++c) p = std::max (p, (double) std::abs (ch[c][i]));
            const double alvo = p * g > teto ? teto / p : 1.0;
            g = alvo < g ? alvo : alvo + (g - alvo) * solta;
            if (p * g > teto) g = teto / p;
            for (int c = 0; c < nch; ++c) ch[c][i] = (float) (ch[c][i] * g);
            gMin = std::min (gMin, g);
        }
    }

private:
    double g = 1.0, solta = 0.999, gMin = 1.0;
};
} // namespace curva
