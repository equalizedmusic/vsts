#pragma once
// MASTER DOS MAGOS — motor (C++ puro, sem JUCE: tools/teste_dsp.cpp compila este arquivo sozinho).
//
// Masterizador "par do MAESTRO". Cadeia (estéreo, na ordem do áudio):
//   TONE MATCH  9 sinos de 1 oitava (50 Hz … 12,8 kHz) que puxam o balanço tonal da música para o da REFERÊNCIA
//               (ou para a curva do STYLE, sem referência) + TONE TILT (shelves 300 Hz / 3 kHz).
//   DYNAMIC EQ  5 pontos que só agem quando o problema aparece: LOW PUNCH 63 Hz (+), MUD 240 Hz (−), HARSH 3,3 kHz (−),
//               DE-ESS 7,5 kHz (−) e AIR 12,5 kHz (+). Detector = energia da zona relativa à banda toda, comparada com
//               a mesma conta feita na curva alvo.
//   MULTIBAND   4 bandas (cortes 120 / 1,2 k / 6,5 kHz, módulo de Linkwitz-Riley de 4ª ordem em FASE LINEAR: a soma
//               é a entrada atrasada, exata), compressor por banda com limiar AUTO (o que ouviu no LISTEN), reposição automática
//               do ganho médio (segura o balanço), PUNCH = ataque, CHARACTER = razão + saturação quente nos graves.
//               STEREO por banda: graves em MONO abaixo de 120 Hz, WIDTH nas de cima.
//   DRIVE + LIMITER  ganho até o alvo de loudness e limitador true peak (detector com sobreamostragem 4×, lookahead
//               1,5 ms, teto −1 dBTP).
// LISTEN mede a música (espectro, LUFS, nível de cada banda). CAST (um clique) calcula tudo e, nos primeiros ~20 s
// tocando, acerta o DRIVE em malha fechada até o LUFS integrado chegar ao alvo (depois fica fixo).
// A/B: B = a entrada original atrasada (mesma latência) no MESMO loudness da saída; BYPASS = original sem igualar;
// REF = a referência tocando, também no loudness da saída.
// TONE, TILT, DYNAMIC EQ e crossover são UM estágio de FASE LINEAR (classe FaseLinear: convolução particionada).
// Latência = fase linear (bloco + meio filtro: 2560 amostras a 48 kHz) + lookahead do limitador + meio filtro do
// detector de true peak (informada ao DAW, que compensa).
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace magos
{
constexpr int NB = 27;          // bandas de análise (1/3 de oitava): 40·2^(b/3) Hz (as mesmas do MAESTRO)
constexpr int NE = 9;           // sinos do TONE MATCH (1 oitava): centro = banda 3k+1
constexpr int NZ = 5;           // pontos da DYNAMIC EQ
constexpr int NM = 4;           // bandas do multibanda
constexpr int NSTYLE = 6;
constexpr int NALVO = 8;        // opções de TARGET
constexpr int FFTN = 2048, HOP = 1024;
constexpr double kPi = 3.14159265358979323846;
constexpr float kTeto = -1.0f;  // dBTP

inline double freqBanda (int b) { return 40.0 * std::pow (2.0, b / 3.0); }
inline double freqEq (int k) { return freqBanda (3 * k + 1); }
inline float dbDe (double p) { return p > 1e-20 ? (float) (10.0 * std::log10 (p)) : -200.0f; }
inline double potDe (double db) { return std::pow (10.0, db / 10.0); }
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

inline const char* nomeStyle (int s)
{
    static const char* n[NSTYLE] = { "PSYTRANCE", "PROG PSY", "TECHNO", "HOUSE", "DRUM & BASS", "POP" };
    return s >= 0 && s < NSTYLE ? n[s] : "?";
}
// LUFS integrado alvo do STYLE (sem referência)
inline float lufsStyle (int s)
{
    static const float t[NSTYLE] = { -7.5f, -8.5f, -8.0f, -8.0f, -7.0f, -9.0f };
    return t[s < 0 || s >= NSTYLE ? 0 : s];
}
// TARGET: 0 = AUTO (referência; sem ela, o STYLE); os outros são LUFS fixos
inline float lufsOpcao (int o)
{
    static const float t[NALVO] = { 0.0f, -14.0f, -11.0f, -9.0f, -8.0f, -7.0f, -6.0f, -5.0f };
    return t[o < 0 || o >= NALVO ? 0 : o];
}
// forma do espectro alvo do STYLE (dB por banda de 1/3 de oitava; só a FORMA importa): corpo do grave + inclinação
inline float alvoStyle (int s, int b)
{
    static const float bump[NSTYLE] = { 6.5f, 5.5f, 6.0f, 5.0f, 7.0f, 3.5f };
    static const float incl[NSTYLE] = { 3.7f, 3.6f, 3.9f, 3.5f, 3.4f, 3.3f };   // dB por oitava acima de 120 Hz
    s = s < 0 || s >= NSTYLE ? 0 : s;
    const double o = std::log2 (freqBanda (b) / 60.0);
    const double corpo = bump[s] * std::exp (-o * o / (2.0 * 0.9 * 0.9));
    const double cai = incl[s] * std::max (0.0, std::log2 (freqBanda (b) / 120.0));
    const double ar = b >= 24 ? -2.0 * (b - 23) : 0.0;   // acima de 10 kHz cai mais
    return (float) (corpo - cai + ar);
}

// pontos da DYNAMIC EQ
struct Zona { const char* nome; double f, q; int sinal; float max; };   // sinal −1 = só corta, +1 = só realça
inline const Zona& zona (int z)
{
    static const Zona t[NZ] = { { "LOW PUNCH", 63.0, 1.0, +1, 2.5f }, { "MUD", 240.0, 1.2, -1, 4.0f },
                                { "HARSH", 3300.0, 1.4, -1, 4.0f },   { "DE-ESS", 7500.0, 1.6, -1, 4.0f },
                                { "AIR", 12500.0, 0.9, +1, 2.5f } };
    return t[z < 0 || z >= NZ ? 0 : z];
}
inline const char* nomeBanda (int m)
{
    static const char* n[NM] = { "LOW", "LOW-MID", "HIGH-MID", "HIGH" };
    return m >= 0 && m < NM ? n[m] : "?";
}
constexpr double fCorte[NM - 1] = { 120.0, 1200.0, 6500.0 };

// ------------------------------------------------------------------------------------------------------------------
// FFT real de 2048 (complexa de 1024 + desdobramento) — a mesma do MAESTRO
// ------------------------------------------------------------------------------------------------------------------
class FftReal
{
public:
    FftReal()
    {
        const int M = FFTN / 2;
        for (int i = 0; i < M / 2; ++i) { twRe.push_back ((float) std::cos (-2.0 * kPi * i / M)); twIm.push_back ((float) std::sin (-2.0 * kPi * i / M)); }
        for (int k = 0; k <= M; ++k) { twRRe.push_back ((float) std::cos (-2.0 * kPi * k / FFTN)); twRIm.push_back ((float) std::sin (-2.0 * kPi * k / FFTN)); }
        rev.resize ((size_t) M);
        int bits = 0;
        while ((1 << bits) < M) ++bits;
        for (int i = 0; i < M; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        zr.resize ((size_t) M);
        zi.resize ((size_t) M);
    }
    void potencia (const float* x, float* pot)
    {
        const int M = FFTN / 2;
        float* re = zr.data();
        float* im = zi.data();
        for (int i = 0; i < M; ++i) { const int r = rev[(size_t) i]; re[r] = x[2 * i]; im[r] = x[2 * i + 1]; }
        for (int len = 2; len <= M; len <<= 1)
        {
            const int meio = len / 2, passo = M / len;
            for (int i = 0; i < M; i += len)
                for (int j = 0; j < meio; ++j)
                {
                    const float wr = twRe[(size_t) (j * passo)], wi = twIm[(size_t) (j * passo)];
                    const int p = i + j, q = p + meio;
                    const float vr = re[q] * wr - im[q] * wi, vi = re[q] * wi + im[q] * wr;
                    re[q] = re[p] - vr; im[q] = im[p] - vi;
                    re[p] += vr; im[p] += vi;
                }
        }
        for (int k = 0; k <= M; ++k)
        {
            const int a = k % M, b = (M - k) % M;
            const float ar = re[a], ai = im[a], br = re[b], bi = -im[b];
            const float pr = 0.5f * (ar + br), pi = 0.5f * (ai + bi);
            const float dr = ar - br, di = ai - bi;
            const float ir = 0.5f * di, ii = -0.5f * dr;
            const float wr = twRRe[(size_t) k], wi = twRIm[(size_t) k];
            const float xr = pr + wr * ir - wi * ii, xi = pi + wr * ii + wi * ir;
            pot[k] = xr * xr + xi * xi;
        }
    }

private:
    std::vector<float> twRe, twIm, twRRe, twRIm, zr, zi;
    std::vector<int> rev;
};

// ------------------------------------------------------------------------------------------------------------------
// biquads RBJ (TDF-II, estado em double)
// ------------------------------------------------------------------------------------------------------------------
struct Coef { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline Coef sino (double fs, double f, double q, double db)
{
    const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w);
    const double a0 = 1.0 + al / A;
    Coef k;
    k.b0 = (1.0 + al * A) / a0; k.b1 = -2.0 * c / a0; k.b2 = (1.0 - al * A) / a0;
    k.a1 = -2.0 * c / a0; k.a2 = (1.0 - al / A) / a0;
    return k;
}
inline Coef passaBaixas (double fs, double f, double q)
{
    const double w = 2.0 * kPi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    Coef k;
    k.b0 = (1.0 - c) / 2.0 / a0; k.b1 = (1.0 - c) / a0; k.b2 = k.b0;
    k.a1 = -2.0 * c / a0; k.a2 = (1.0 - al) / a0;
    return k;
}
inline Coef passaAltas (double fs, double f, double q)
{
    const double w = 2.0 * kPi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    Coef k;
    k.b0 = (1.0 + c) / 2.0 / a0; k.b1 = -(1.0 + c) / a0; k.b2 = k.b0;
    k.a1 = -2.0 * c / a0; k.a2 = (1.0 - al) / a0;
    return k;
}
inline Coef passaTudo (double fs, double f, double q)
{
    const double w = 2.0 * kPi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    Coef k;
    k.b0 = (1.0 - al) / a0; k.b1 = -2.0 * c / a0; k.b2 = (1.0 + al) / a0;
    k.a1 = -2.0 * c / a0; k.a2 = (1.0 - al) / a0;
    return k;
}
inline Coef passaBanda (double fs, double f, double q)   // pico 0 dB
{
    const double w = 2.0 * kPi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    Coef k;
    k.b0 = al / a0; k.b1 = 0.0; k.b2 = -al / a0;
    k.a1 = -2.0 * c / a0; k.a2 = (1.0 - al) / a0;
    return k;
}
inline Coef shelf (double fs, double f, double db, bool agudo)
{
    const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * f / fs, c = std::cos (w);
    const double al = std::sin (w) / 2.0 * std::sqrt (2.0), r = 2.0 * std::sqrt (A) * al;
    Coef k;
    if (! agudo)
    {
        const double a0 = (A + 1) + (A - 1) * c + r;
        k.b0 = A * ((A + 1) - (A - 1) * c + r) / a0; k.b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
        k.b2 = A * ((A + 1) - (A - 1) * c - r) / a0; k.a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
        k.a2 = ((A + 1) + (A - 1) * c - r) / a0;
    }
    else
    {
        const double a0 = (A + 1) - (A - 1) * c + r;
        k.b0 = A * ((A + 1) + (A - 1) * c + r) / a0; k.b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        k.b2 = A * ((A + 1) + (A - 1) * c - r) / a0; k.a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
        k.a2 = ((A + 1) - (A - 1) * c - r) / a0;
    }
    return k;
}
inline double respostaDb (const Coef& k, double fs, double f)
{
    const double w = 2.0 * kPi * f / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const auto h = (k.b0 + k.b1 * z1 + k.b2 * z2) / (1.0 + k.a1 * z1 + k.a2 * z2);
    return 20.0 * std::log10 (std::max (1e-9, std::abs (h)));
}

struct Biquad
{
    Coef k;
    double s1[2] { 0, 0 }, s2[2] { 0, 0 };
    void zera() { s1[0] = s1[1] = s2[0] = s2[1] = 0; }
    inline float passa (float x, int c)
    {
        const double y = k.b0 * x + s1[c];
        s1[c] = k.b1 * x - k.a1 * y + s2[c];
        s2[c] = k.b2 * x - k.a2 * y;
        return (float) y;
    }
};

constexpr double qOitava = 1.414;
constexpr double qBw = 0.70710678118654752;
constexpr double fTiltG = 300.0, fTiltA = 3000.0;

// ------------------------------------------------------------------------------------------------------------------
// FASE LINEAR — todo o caminho do áudio (crossover, TONE MATCH, TILT, DYNAMIC EQ) é de fase linear.
//
// Os filtros são de fase ZERO (só módulo), janelados em N amostras e atrasados de N/2: nada gira a fase, o transiente
// sai simétrico e a soma das 4 bandas é a entrada atrasada, EXATA (os módulos das bandas somam 1 em toda frequência).
// Os ganhos entram em paralelo: E(f) = 1 + Σ (gᵢ − 1)·Sᵢ(f), Sᵢ = a forma (0…1) do sino / shelf. Cada forma é
// desenhada já multiplicada pela banda do crossover onde cai (Bₖ·Sᵢ), então mexer num ganho NÃO redesenha filtro:
// só muda o peso com que os espectros se somam. Por isso a DYN EQ pode mudar a cada bloco sem custo e sem clique
// (quando um ganho muda, a banda sai de duas somas, a velha e a nova, com rampa linear de um bloco).
//
// Convolução particionada uniforme (blocos de P, FFT de 2P): UMA FFT de ida por bloco (L e R juntos numa FFT complexa)
// e UMA de volta por banda (L no real, R no imaginário). Latência = P + N/2 (10,7 ms + 42,7 ms a 48 kHz).
// ------------------------------------------------------------------------------------------------------------------
constexpr int NF = NE + 2 + NZ;   // formas de ganho: 9 sinos do TONE, 2 shelves do TILT, 5 pontos da DYN EQ
constexpr int kIdxTilt = NE, kIdxDyn = NE + 2;

// forma (0…1) do ganho i em f: (|H(+6 dB)| − 1) normalizado — sino e shelf com o mesmo desenho dos RBJ
inline double forma (int i, double fs, double f)
{
    static const double g6 = std::pow (10.0, 6.0 / 20.0);
    Coef k;
    if (i < NE) k = sino (fs, freqEq (i), qOitava, 6.0);
    else if (i < kIdxDyn) k = shelf (fs, i == kIdxTilt ? fTiltG : fTiltA, 6.0, i != kIdxTilt);
    else { const auto& Z = zona (i - kIdxDyn); k = sino (fs, Z.f, Z.q, 6.0); }
    return (std::pow (10.0, respostaDb (k, fs, f) / 20.0) - 1.0) / (g6 - 1.0);
}
// passa-baixas do crossover (fase zero): módulo de Linkwitz-Riley de 4ª ordem; o passa-altas é 1 − ele (soma exata)
inline double lpCross (double fc, double f) { const double r = (f / fc) * (f / fc); return 1.0 / (1.0 + r * r); }
inline double bandaCross (int k, double f)
{
    const double de = k == 0 ? 0.0 : lpCross (fCorte[k - 1], f), ate = k == NM - 1 ? 1.0 : lpCross (fCorte[k], f);
    return ate - de;
}
// resposta (dB) do caminho com os ganhos gDb[NF]: 1 + Σ (gᵢ − 1)·Sᵢ(f)
inline double modeloDb (const float* gDb, double fs, double f)
{
    double h = 1.0;
    for (int i = 0; i < NF; ++i)
        if (std::abs (gDb[i]) > 1e-4f) h += (std::pow (10.0, gDb[i] / 20.0) - 1.0) * forma (i, fs, f);
    return 20.0 * std::log10 (std::max (1e-6, h));
}

// FFT complexa radix-2 (tamanho potência de 2), em lugar; inversa sem a divisão por n. Fatores de rotação guardados
// contíguos por estágio (o laço de dentro anda em memória seguida e o compilador vetoriza)
class FftC
{
public:
    void prepara (int n_)
    {
        n = n_;
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        wr.clear(); wi.clear();
        for (int len = 2; len <= n; len <<= 1)
            for (int j = 0; j < len / 2; ++j)
            {
                wr.push_back ((float) std::cos (-2.0 * kPi * j / len));
                wi.push_back ((float) std::sin (-2.0 * kPi * j / len));
            }
    }
    void faz (float* __restrict re, float* __restrict im, bool inversa) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        // inversa = troca re ↔ im na entrada e na saída (conj(FFT(conj x)) sem multiplicar nada)
        float* a = inversa ? im : re;
        float* b = inversa ? re : im;
        for (int i = 0; i < n; i += 2)
        {
            const float ar = a[i + 1], ai = b[i + 1];
            a[i + 1] = a[i] - ar; b[i + 1] = b[i] - ai;
            a[i] += ar; b[i] += ai;
        }
        const float* tw = wr.data() + 1;
        const float* tv = wi.data() + 1;
        for (int len = 4; len <= n; len <<= 1)
        {
            const int meio = len / 2;
            const float* cr = tw; const float* ci = tv;
            for (int i = 0; i < n; i += len)
            {
                float* __restrict pr = a + i; float* __restrict pi = b + i;
                float* __restrict qr = a + i + meio; float* __restrict qi = b + i + meio;
                for (int j = 0; j < meio; ++j)
                {
                    const float vr = qr[j] * cr[j] - qi[j] * ci[j], vi = qr[j] * ci[j] + qi[j] * cr[j];
                    qr[j] = pr[j] - vr; qi[j] = pi[j] - vi;
                    pr[j] += vr; pi[j] += vi;
                }
            }
            tw += meio; tv += meio;
        }
    }
    int tamanho() const { return n; }

private:
    int n = 0;
    std::vector<int> rev;
    std::vector<float> wr, wi;
};

class FaseLinear
{
public:
    // P = bloco (amostras), NP = partições: N = P·NP derivações por filtro
    void prepara (double fs_, int P_, int NP_)
    {
        fs = fs_; P = P_; NP = NP_; N = P * NP; M = 2 * P; K = P + 1;
        fft.prepara (M);
        desenha();
        const size_t tamFdl = (size_t) (NP * K);
        for (auto* v : { &xlr, &xli, &xrr, &xri }) v->assign (tamFdl, 0.0f);
        for (auto* v : { &zr, &zi }) v->assign ((size_t) M, 0.0f);
        for (auto* v : { &antL, &antR, &inL, &inR, &det }) v->assign ((size_t) P, 0.0f);
        for (int k = 0; k < NM; ++k) for (int c = 0; c < 2; ++c) saida[k][c].assign ((size_t) P, 0.0f);
        for (auto* v : { &yLr, &yLi, &yRr, &yRi }) v->assign ((size_t) K, 0.0f);
        tmpL.assign ((size_t) P, 0.0f); tmpR = tmpL;
        // filtros combinados com todos os ganhos em 0 dB: cada banda = a banda pura; detector = a soma delas
        float g1[NF];
        for (int i = 0; i < NF; ++i) { gAnt[i] = 1.0f; g1[i] = 1.0f; }
        GDr.assign ((size_t) (NP * K), 0.0f); GDi = GDr;
        for (int b = 0; b < NM; ++b)
        {
            Gr[b] = GDr; Gi[b] = GDr;
            combina (b, g1, false, Gr[b], Gi[b]);
            GrAnt[b] = Gr[b]; GiAnt[b] = Gi[b];
            combina (b, g1, true, GDr, GDi);
            formasDa[b].clear();
            for (const auto& F : filtros) if (F.banda == b && F.forma >= 0) formasDa[b].push_back (F.forma);
        }
        fatia = 0; pos = 0;
    }
    double alfaJanela = 0.25;   // fração das pontas em cosseno (0,25: medido na bancada, menor erro sem degrau)
    int latencia() const { return P + N / 2; }
    int bloco() const { return P; }
    int numFiltros() const { return (int) filtros.size(); }
    bool cheio() const { return pos == P; }

    // uma amostra: entra (l, r); sai a amostra de cada banda (já com os ganhos) e o detector (mono, só TONE + TILT)
    inline void amostra (float l, float r, float* bl, float* br, float& d)
    {
        inL[(size_t) pos] = l; inR[(size_t) pos] = r;
        for (int k = 0; k < NM; ++k) { bl[k] = saida[k][0][(size_t) pos]; br[k] = saida[k][1][(size_t) pos]; }
        d = det[(size_t) pos];
        ++pos;
    }

    // processa o bloco que acabou de encher, com os ganhos novos g[NF] (lineares)
    void processaBloco (const float* g)
    {
        // 1) FFT do segmento [bloco anterior | bloco atual]: L no real, R no imaginário
        float* re = zr.data(); float* im = zi.data();
        for (int t = 0; t < P; ++t)
        {
            re[t] = antL[(size_t) t]; im[t] = antR[(size_t) t];
            re[P + t] = inL[(size_t) t]; im[P + t] = inR[(size_t) t];
        }
        std::copy (inL.begin(), inL.end(), antL.begin());
        std::copy (inR.begin(), inR.end(), antR.begin());
        fft.faz (re, im, false);
        fatia = (fatia + NP - 1) % NP;
        {
            float* aLr = &xlr[(size_t) (fatia * K)]; float* aLi = &xli[(size_t) (fatia * K)];
            float* aRr = &xrr[(size_t) (fatia * K)]; float* aRi = &xri[(size_t) (fatia * K)];
            for (int k = 0; k < K; ++k)
            {
                const int q = (M - k) % M;
                const float ar = re[k], ai = im[k], br = re[q], bi = -im[q];   // b = conj(Z[M−k])
                aLr[k] = 0.5f * (ar + br); aLi[k] = 0.5f * (ai + bi);
                aRr[k] = 0.5f * (ai - bi); aRi[k] = -0.5f * (ar - br);
            }
        }
        // 2) cada banda: o filtro da banda é a soma dos filtros dela com os pesos (1 a banda pura, g − 1 as formas).
        //    Só se recombina quando um ganho dela mudou; aí a banda sai das duas somas (velha e nova) com rampa.
        bool mudouEst = false;
        for (int i = 0; i < kIdxDyn; ++i) if (std::abs (g[i] - gAnt[i]) > 1e-7f) mudouEst = true;
        for (int b = 0; b < NM; ++b)
        {
            bool mudou = false;
            for (int i : formasDa[b]) if (std::abs (g[i] - gAnt[i]) > 1e-7f) mudou = true;
            if (mudou)
            {
                std::swap (Gr[b], GrAnt[b]); std::swap (Gi[b], GiAnt[b]);
                if (mudouEst || ++incrementos[b] >= 64) { combina (b, g, false, Gr[b], Gi[b]); incrementos[b] = 0; }
                else
                {
                    // só a DYN EQ mexeu: o filtro novo = o velho + (g − g_ant)·forma, só das formas que mudaram
                    std::copy (GrAnt[b].begin(), GrAnt[b].end(), Gr[b].begin());
                    std::copy (GiAnt[b].begin(), GiAnt[b].end(), Gi[b].begin());
                    for (const auto& F : filtros)
                    {
                        if (F.banda != b || F.forma < 0) continue;
                        const float w = g[F.forma] - gAnt[F.forma];
                        if (w == 0.0f) continue;
                        const size_t nn = (size_t) (NP * K);
                        const float* hr = F.hr.data(); const float* hi = F.hi.data();
                        float* gr = Gr[b].data(); float* gi = Gi[b].data();
                        for (size_t k = 0; k < nn; ++k) { gr[k] += w * hr[k]; gi[k] += w * hi[k]; }
                    }
                }
            }
            convolui (Gr[b], Gi[b], false);
            volta (saida[b][0].data(), saida[b][1].data());
            if (mudou)
            {
                convolui (GrAnt[b], GiAnt[b], false);
                volta (tmpL.data(), tmpR.data());
                for (int t = 0; t < P; ++t)
                {
                    const float w = (float) (t + 1) / (float) P;
                    saida[b][0][(size_t) t] = tmpL[(size_t) t] + w * (saida[b][0][(size_t) t] - tmpL[(size_t) t]);
                    saida[b][1][(size_t) t] = tmpR[(size_t) t] + w * (saida[b][1][(size_t) t] - tmpR[(size_t) t]);
                }
            }
        }
        // 3) detector da DYN EQ: entrada com TONE + TILT (sem a DYN), mono
        if (mudouEst)
        {
            std::fill (GDr.begin(), GDr.end(), 0.0f); std::fill (GDi.begin(), GDi.end(), 0.0f);
            for (int b = 0; b < NM; ++b) combina (b, g, true, GDr, GDi);
        }
        convolui (GDr, GDi, true);
        volta (det.data(), tmpR.data());
        for (int i = 0; i < NF; ++i) gAnt[i] = g[i];
        pos = 0;
    }

private:
    struct Filtro { int banda, forma; std::vector<float> hr, hi; };   // forma −1 = a banda pura

    // G (+)= Σ dos filtros da banda b com os pesos; soEstatica = sem a DYN EQ e SOMANDO em G (detector)
    void combina (int b, const float* g, bool soEstatica, std::vector<float>& Gre, std::vector<float>& Gim)
    {
        const size_t n = (size_t) (NP * K);
        if (! soEstatica) { std::fill (Gre.begin(), Gre.end(), 0.0f); std::fill (Gim.begin(), Gim.end(), 0.0f); }
        for (const auto& F : filtros)
        {
            if (F.banda != b || (soEstatica && F.forma >= kIdxDyn)) continue;
            const float w = F.forma < 0 ? 1.0f : g[F.forma] - 1.0f;
            if (w == 0.0f) continue;
            const float* hr = F.hr.data(); const float* hi = F.hi.data();
            float* gr = Gre.data(); float* gi = Gim.data();
            for (size_t k = 0; k < n; ++k) { gr[k] += w * hr[k]; gi[k] += w * hi[k]; }
        }
    }
    // y = Σ_p X(bloco − p)·G_p, de L e R (mono = só (L+R)/2, no lugar do L)
    void convolui (const std::vector<float>& Gre, const std::vector<float>& Gim, bool mono)
    {
        std::fill (yLr.begin(), yLr.end(), 0.0f); std::fill (yLi.begin(), yLi.end(), 0.0f);
        std::fill (yRr.begin(), yRr.end(), 0.0f); std::fill (yRi.begin(), yRi.end(), 0.0f);
        float* oLr = yLr.data(); float* oLi = yLi.data(); float* oRr = yRr.data(); float* oRi = yRi.data();
        for (int p = 0; p < NP; ++p)
        {
            const size_t s = (size_t) (((fatia + p) % NP) * K);
            const float* hr = &Gre[(size_t) (p * K)]; const float* hi = &Gim[(size_t) (p * K)];
            const float* lr = &xlr[s]; const float* li = &xli[s]; const float* rr = &xrr[s]; const float* ri = &xri[s];
            if (mono)
                for (int k = 0; k < K; ++k)
                {
                    const float mr = 0.5f * (lr[k] + rr[k]), mi = 0.5f * (li[k] + ri[k]);
                    oLr[k] += mr * hr[k] - mi * hi[k]; oLi[k] += mr * hi[k] + mi * hr[k];
                }
            else
                for (int k = 0; k < K; ++k)
                {
                    oLr[k] += lr[k] * hr[k] - li[k] * hi[k]; oLi[k] += lr[k] * hi[k] + li[k] * hr[k];
                    oRr[k] += rr[k] * hr[k] - ri[k] * hi[k]; oRi[k] += rr[k] * hi[k] + ri[k] * hr[k];
                }
        }
    }
    // espectros (0…P) de L e R → tempo (overlap-save: a 2ª metade vale)
    void volta (float* outL, float* outR)
    {
        float* re = zr.data(); float* im = zi.data();
        for (int k = 0; k < K; ++k) { re[k] = yLr[(size_t) k] - yRi[(size_t) k]; im[k] = yLi[(size_t) k] + yRr[(size_t) k]; }
        for (int k = K; k < M; ++k)
        {
            const int q = M - k;   // L[k] = conj L[q], R[k] = conj R[q]  →  Z = conj(L[q]) + i·conj(R[q])
            re[k] = yLr[(size_t) q] + yRi[(size_t) q]; im[k] = -yLi[(size_t) q] + yRr[(size_t) q];
        }
        fft.faz (re, im, true);
        const float s = 1.0f / (float) M;
        for (int t = 0; t < P; ++t) { outL[t] = re[P + t] * s; outR[t] = im[P + t] * s; }
    }

    // desenho: módulo numa grade fina (8N), fase zero, IFFT, janela de N amostras centrada, partições de P
    void desenha()
    {
        filtros.clear();
        const int L = 8 * N;
        FftC grande; grande.prepara (L);
        std::vector<float> re ((size_t) L), im ((size_t) L), h ((size_t) N);
        std::vector<double> banda ((size_t) (L / 2 + 1));
        auto partiu = [&](int b, int fo, const std::vector<double>& mod)
        {
            for (int k = 0; k <= L / 2; ++k) { re[(size_t) k] = (float) mod[(size_t) k]; im[(size_t) k] = 0.0f; }
            for (int k = L / 2 + 1; k < L; ++k) { re[(size_t) k] = re[(size_t) (L - k)]; im[(size_t) k] = 0.0f; }
            grande.faz (re.data(), im.data(), true);
            for (int t = 0; t < N; ++t)
            {
                const int m = t - N / 2;   // centro em N/2
                h[(size_t) t] = re[(size_t) ((m + L) % L)] / (float) L * (float) janela (m);
            }
            Filtro F { b, fo, std::vector<float> ((size_t) (NP * K)), std::vector<float> ((size_t) (NP * K)) };
            for (int p = 0; p < NP; ++p)
            {
                std::fill (zr.begin(), zr.end(), 0.0f); std::fill (zi.begin(), zi.end(), 0.0f);
                for (int t = 0; t < P; ++t) zr[(size_t) t] = h[(size_t) (p * P + t)];
                fft.faz (zr.data(), zi.data(), false);
                for (int k = 0; k < K; ++k) { F.hr[(size_t) (p * K + k)] = zr[(size_t) k]; F.hi[(size_t) (p * K + k)] = zi[(size_t) k]; }
            }
            filtros.push_back (std::move (F));
        };
        zr.assign ((size_t) M, 0.0f); zi.assign ((size_t) M, 0.0f);
        std::vector<double> prod ((size_t) (L / 2 + 1));
        std::vector<std::vector<double>> formas ((size_t) NF, std::vector<double> ((size_t) (L / 2 + 1)));
        for (int i = 0; i < NF; ++i)
            for (int k = 0; k <= L / 2; ++k) formas[(size_t) i][(size_t) k] = forma (i, fs, fs * k / L);
        for (int b = 0; b < NM; ++b)
        {
            for (int k = 0; k <= L / 2; ++k) banda[(size_t) k] = bandaCross (b, fs * k / L);
            partiu (b, -1, banda);
            // as formas que caem nesta banda (cada forma é repartida entre as bandas: Σ Bₖ = 1, nada se perde)
            for (int i = 0; i < NF; ++i)
            {
                double mx = 0;
                for (int k = 0; k <= L / 2; ++k)
                {
                    prod[(size_t) k] = banda[(size_t) k] * formas[(size_t) i][(size_t) k];
                    mx = std::max (mx, std::abs (prod[(size_t) k]));
                }
                if (mx > 0.004) partiu (b, i, prod);
            }
        }
    }
    // janela: plana no miolo e cosseno nas pontas (Tukey) — pouco alarga o grave e mata o degrau das pontas
    double janela (int m) const
    {
        const double x = std::abs ((double) m) / (N / 2.0);   // 0 no centro, 1 na ponta
        const double a = alfaJanela;
        return x <= 1.0 - a ? 1.0 : 0.5 * (1.0 + std::cos (kPi * (x - (1.0 - a)) / a));
    }

    double fs = 48000.0;
    int P = 512, NP = 8, N = 4096, M = 1024, K = 513;
    FftC fft;
    std::vector<Filtro> filtros;
    std::vector<float> xlr, xli, xrr, xri;           // espectros dos últimos NP blocos (L, R)
    std::vector<float> Gr[NM], Gi[NM], GrAnt[NM], GiAnt[NM], GDr, GDi;   // filtro combinado de cada banda e do detector
    std::vector<int> formasDa[NM];
    int incrementos[NM] {};
    std::vector<float> yLr, yLi, yRr, yRi, zr, zi;
    std::vector<float> antL, antR, inL, inR, det, tmpL, tmpR;
    std::vector<float> saida[NM][2];
    float gAnt[NF] {};
    int fatia = 0, pos = 0;
};

// ------------------------------------------------------------------------------------------------------------------
// Loudness (BS.1770-4 / EBU R128): ponderação K, blocos de 100 ms; momentâneo 400 ms, curto 3 s, integrado com os
// portões (−70 LUFS absoluto, −10 LU relativo) por histograma de 0,1 dB.
// ------------------------------------------------------------------------------------------------------------------
class Loudness
{
public:
    void prepare (double fs_)
    {
        fs = fs_;
        {   // pré-filtro (shelf de agudos), calculado para a taxa
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            pre.k.b0 = (Vh + Vb * K / Q + K * K) / a0; pre.k.b1 = 2.0 * (K * K - Vh) / a0; pre.k.b2 = (Vh - Vb * K / Q + K * K) / a0;
            pre.k.a1 = 2.0 * (K * K - 1.0) / a0; pre.k.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {   // RLB (passa-altas); numerador 1, −2, 1 como na norma
            const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (kPi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            rlb.k.b0 = 1.0; rlb.k.b1 = -2.0; rlb.k.b2 = 1.0;
            rlb.k.a1 = 2.0 * (K * K - 1.0) / a0; rlb.k.a2 = (1.0 - K / Q + K * K) / a0;
        }
        nBloco = std::max (1, (int) std::lround (fs * 0.1));
        zera();
    }
    void zera()
    {
        pre.zera(); rlb.zera();
        conta = 0; acum = 0;
        for (auto& b : blocos) b = 0;
        nb = 0;
        momentaneo = curto = -200.0f;
        zeraIntegrado();
    }
    void zeraIntegrado()
    {
        std::fill (std::begin (hCont), std::end (hCont), 0u);
        std::fill (std::begin (hPot), std::end (hPot), 0.0);
        integrado = -200.0f;
    }
    void processa (const float* const* x, int nch, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            double s = 0;
            for (int c = 0; c < nch && c < 2; ++c)
            {
                const float y = rlb.passa (pre.passa (x[c][i], c), c);
                s += (double) y * y;
            }
            if (nch == 1) s *= 2.0;   // mono = os dois canais iguais
            acum += s;
            if (++conta >= nBloco) fechaBloco();
        }
    }
    float momentaneo = -200.0f, curto = -200.0f, integrado = -200.0f;

private:
    void fechaBloco()
    {
        blocos[nb % 30] = acum / conta;
        ++nb;
        acum = 0; conta = 0;
        auto media = [this] (int k)
        {
            const int m = std::min (k, nb);
            double s = 0;
            for (int i = 0; i < m; ++i) s += blocos[(nb - 1 - i) % 30];
            return m > 0 ? s / m : 0.0;
        };
        const double pm = media (4), pc = media (30);
        momentaneo = (float) (-0.691 + 10.0 * std::log10 (pm + 1e-20));
        curto = (float) (-0.691 + 10.0 * std::log10 (pc + 1e-20));
        if (nb >= 4 && momentaneo > -70.0f)
        {
            const int h = std::clamp ((int) std::lround ((momentaneo + 70.0f) * 10.0f), 0, NH - 1);
            ++hCont[h];
            hPot[h] += pm;
            double s = 0; uint64_t nn = 0;
            for (int i = 0; i < NH; ++i) { s += hPot[i]; nn += hCont[i]; }
            if (nn > 0)
            {
                const double rel = -0.691 + 10.0 * std::log10 (s / (double) nn) - 10.0;
                const int h0 = std::clamp ((int) std::ceil ((rel + 70.0) * 10.0), 0, NH);
                double s2 = 0; uint64_t n2 = 0;
                for (int i = h0; i < NH; ++i) { s2 += hPot[i]; n2 += hCont[i]; }
                if (n2 > 0) integrado = (float) (-0.691 + 10.0 * std::log10 (s2 / (double) n2));
            }
        }
    }
    static constexpr int NH = 800;   // −70 … +10 LUFS em 0,1
    Biquad pre, rlb;
    double fs = 48000.0, acum = 0;
    int nBloco = 4800, conta = 0, nb = 0;
    double blocos[30] {};
    uint32_t hCont[NH] {};
    double hPot[NH] {};
};

// ------------------------------------------------------------------------------------------------------------------
// detector de true peak: interpolação 4× (sinc com janela de Kaiser, 16 taps por fase). O pico devolvido é o do
// trecho kTpAtraso amostras atrás.
// ------------------------------------------------------------------------------------------------------------------
constexpr int kTpTaps = 16, kTpAtraso = kTpTaps / 2;
struct TabelaTp
{
    float t[4][kTpTaps];
    TabelaTp()
    {
        auto bessel0 = [] (double x) { double s = 1, t2 = 1; for (int k = 1; k < 30; ++k) { t2 *= (x / (2 * k)) * (x / (2 * k)); s += t2; } return s; };
        const double beta = 6.0;
        for (int ph = 0; ph < 4; ++ph)
            for (int j = 0; j < kTpTaps; ++j)
            {
                // j = 0 é a amostra mais antiga; ponto interpolado = amostra (kTpTaps/2 − 1) + ph/4
                const double x = (double) (j - (kTpTaps / 2 - 1)) - ph / 4.0;
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (kPi * x) / (kPi * x);
                const double u = x / (kTpTaps / 2.0 + 1.0);
                const double w = std::abs (u) >= 1.0 ? 0.0 : bessel0 (beta * std::sqrt (1.0 - u * u)) / bessel0 (beta);
                t[ph][j] = (float) (sinc * w);
            }
    }
    static const TabelaTp& get() { static const TabelaTp tb; return tb; }
};
struct TruePeak
{
    float anel[2][2 * kTpTaps] {};   // anel dobrado: a janela é sempre contígua
    int pos = 0;
    void zera() { std::memset (anel, 0, sizeof (anel)); pos = 0; }
    inline float empurra (float l, float r, int nch)
    {
        anel[0][pos] = anel[0][pos + kTpTaps] = l;
        anel[1][pos] = anel[1][pos + kTpTaps] = nch > 1 ? r : l;
        pos = (pos + 1) % kTpTaps;
        const auto& T = TabelaTp::get();
        float m = 0.0f;
        for (int c = 0; c < (nch > 1 ? 2 : 1); ++c)
        {
            const float* x = anel[c] + pos;
            for (int ph = 0; ph < 4; ++ph)
            {
                float s = 0.0f;
                for (int j = 0; j < kTpTaps; ++j) s += x[j] * T.t[ph][j];
                m = std::max (m, std::abs (s));
            }
        }
        return m;
    }
};

// ------------------------------------------------------------------------------------------------------------------
// pesos de cada bin de FFT em cada banda de 1/3 de oitava
// ------------------------------------------------------------------------------------------------------------------
struct PesosBandas
{
    struct Peso { int bin; float w; };
    std::vector<Peso> p[NB];
    void calcula (double fs)
    {
        const double df = fs / FFTN;
        for (int b = 0; b < NB; ++b)
        {
            p[b].clear();
            const double lo = b == 0 ? 20.0 : freqBanda (b) * std::pow (2.0, -1.0 / 6.0);
            const double hi = b == NB - 1 ? fs / 2 : freqBanda (b) * std::pow (2.0, 1.0 / 6.0);
            for (int k = 1; k < FFTN / 2; ++k)
            {
                const double a = (k - 0.5) * df, z = (k + 0.5) * df;
                const double sob = std::min (z, hi) - std::max (a, lo);
                if (sob > 0) p[b].push_back ({ k, (float) (sob / df) });
            }
        }
    }
};
inline std::vector<float> janelaHann()
{
    std::vector<float> j (FFTN);
    for (int i = 0; i < FFTN; ++i) j[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / FFTN));
    return j;
}
inline float normaHann()
{
    double w2 = 0;
    for (int i = 0; i < FFTN; ++i) { const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * i / FFTN); w2 += w * w; }
    return (float) (2.0 / ((double) FFTN * w2));
}

// forma do espectro em dB, com a média entre 80 Hz e 8 kHz (bandas 3..23) em 0
inline void formaDb (const float* pot, float* out)
{
    double m = 0;
    for (int b = 0; b < NB; ++b) out[b] = dbDe ((double) pot[b] + 1e-14);
    for (int b = 3; b <= 23; ++b) m += out[b];
    m /= 21.0;
    for (int b = 0; b < NB; ++b) out[b] = (float) (out[b] - m);
}

// quanto da potência total cai na zona z (resposta do passa-banda da zona em cada banda), em dB
inline float relZona (const float* pot, int z)
{
    const auto& Z = zona (z);
    double tot = 0, dentro = 0;
    for (int b = 0; b < NB; ++b)
    {
        const double h = potDe (respostaDb (passaBanda (48000.0, Z.f, Z.q), 48000.0, freqBanda (b)));
        tot += pot[b];
        dentro += pot[b] * h;
    }
    return dbDe (dentro / (tot + 1e-20));
}

// MATCH %: 100 − 12 × desvio médio (dB) entre 63 Hz e 12,5 kHz
inline float casamento (const float* forma, const float* alvo)
{
    double s = 0;
    for (int b = 2; b <= 25; ++b) s += std::abs (forma[b] - alvo[b]);
    return clampf (100.0f - 12.0f * (float) (s / 24.0), 0.0f, 100.0f);
}

// ------------------------------------------------------------------------------------------------------------------
// análise de um áudio inteiro (a REFERÊNCIA; também a bancada): espectro médio dos quadros com som, LUFS integrado,
// true peak, maior LUFS curto
// ------------------------------------------------------------------------------------------------------------------
struct Analise
{
    bool ok = false;
    float espectro[NB] {};
    float lufs = -200.0f, picoTp = -200.0f, curtoMax = -200.0f;
    double segundos = 0.0;
};

inline Analise analisa (const float* L, const float* R, int64_t n, double fs)
{
    Analise a;
    if (L == nullptr || n < (int64_t) (fs * 1.0)) return a;
    if (R == nullptr) R = L;
    Loudness ld;
    ld.prepare (fs);
    TruePeak tp;
    FftReal fft;
    PesosBandas pb;
    pb.calcula (fs);
    const auto jan = janelaHann();
    const float norma = normaHann();
    std::vector<float> q (FFTN), pot (FFTN / 2 + 1);
    float pico = 0.0f;
    for (int64_t i0 = 0; i0 < n; i0 += 4096)
    {
        const int m = (int) std::min<int64_t> (4096, n - i0);
        const float* ch[2] = { L + i0, R + i0 };
        ld.processa (ch, 2, m);
        for (int i = 0; i < m; ++i) pico = std::max (pico, tp.empurra (ch[0][i], ch[1][i], 2));
        a.curtoMax = std::max (a.curtoMax, ld.curto);
    }
    std::vector<float> nivel;
    std::vector<float> bandas;
    for (int64_t i0 = 0; i0 + FFTN <= n; i0 += HOP)
    {
        double e = 0;
        for (int i = 0; i < FFTN; ++i)
        {
            const float m = 0.5f * (L[i0 + i] + R[i0 + i]);
            q[(size_t) i] = m * jan[(size_t) i];
            e += (double) m * m;
        }
        fft.potencia (q.data(), pot.data());
        for (int b = 0; b < NB; ++b)
        {
            double s = 0;
            for (auto& w : pb.p[b]) s += pot[(size_t) w.bin] * w.w;
            bandas.push_back ((float) (s * norma));
        }
        nivel.push_back (dbDe (e / FFTN));
    }
    float mx = -200.0f;
    for (float v : nivel) mx = std::max (mx, v);
    double soma[NB] {};
    int nq = 0;
    for (size_t k = 0; k < nivel.size(); ++k)
        if (nivel[k] > mx - 20.0f && nivel[k] > -70.0f)   // só os quadros com som (até 20 dB abaixo do mais alto)
        {
            for (int b = 0; b < NB; ++b) soma[b] += bandas[k * NB + (size_t) b];
            ++nq;
        }
    if (nq == 0) return a;
    for (int b = 0; b < NB; ++b) a.espectro[b] = (float) (soma[b] / nq);
    a.lufs = ld.integrado;
    a.picoTp = 20.0f * std::log10 (std::max (1e-9f, pico));
    a.segundos = (double) n / fs;
    a.ok = true;
    return a;
}

// ------------------------------------------------------------------------------------------------------------------
// Ajustes (parâmetros) e o que o CAST guarda no projeto
// ------------------------------------------------------------------------------------------------------------------
struct Ajustes
{
    float amount = 0.72f;      // 0..1: escala o que o CAST faz (TONE, DYN EQ, multibanda)
    float character = 0.3f;    // 0 CLEAN … 1 WARM
    float punch = 0.0f;        // −5 … +5 (ataque do multibanda)
    float width = 1.0f;        // 0 … 2
    float tilt = 0.0f;         // −3 … +3 dB
    bool monoBass = true;
    int ouvir = 0;             // 0 A (master), 1 B (original no mesmo loudness), 2 REF, 3 BYPASS
    int sim = 0;               // monitor: 0 nada, 1 PHONE, 2 EARBUDS, 3 CAR, 4 MONO

    // MANUAL (por cima do AUTO): o multibanda com limiar / razão / ataque / soltura / ganho de cada banda, e a DYNAMIC EQ
    // com liga / RANGE (quanto pode mexer) / THRESH (quantos dB fora do alvo antes de agir) de cada ponto. Funciona com
    // e sem CAST (sem CAST, a DYN EQ mede contra a curva do STYLE / REFERÊNCIA).
    bool mbManual = false, eqManual = false;
    float mbThr[NM] { -20.0f, -20.0f, -20.0f, -20.0f };   // dB (envelope de pico da banda)
    float mbRat[NM] { 2.0f, 2.5f, 3.0f, 1.6f };
    float mbAtk[NM] { 30.0f, 15.0f, 5.0f, 2.0f };          // ms
    float mbRel[NM] { 160.0f, 120.0f, 80.0f, 60.0f };      // ms
    float mbGain[NM] {};                                    // dB
    bool eqOn[NZ] { true, true, true, true, true };
    float eqRange[NZ] { 2.5f, 4.0f, 4.0f, 4.0f, 2.5f };    // dB
    float eqThr[NZ] { 1.5f, 1.5f, 1.5f, 1.5f, 1.5f };      // dB
};

struct Feitico
{
    bool feito = false;
    float tom[NE] {};          // dB com AMOUNT 100 %
    float limiar[NM] {};       // dB (nível médio da banda no LISTEN)
    float drive = 0.0f;        // dB
    float alvoLufs = -8.0f;
    float alvoForma[NB] {};    // forma alvo usada (dB, média 0)
    float relAlvo[NZ] {};      // energia relativa de cada zona na curva alvo (dB)
    float antesLufs = -200.0f; // LUFS da música no LISTEN (tela)
    float casamentoAntes = 0;  // MATCH % antes (tela)
};

// referência carregada (a thread da interface monta; o áudio só lê; quem apaga é o processador)
struct Referencia
{
    Analise an;
    std::vector<float> L, R;   // trecho para ouvir (já na taxa do projeto)
    char nome[96] {};
};

// ------------------------------------------------------------------------------------------------------------------
class Engine
{
public:
    Engine()
    {
        jan = janelaHann();
        norma = normaHann();
        anel.assign (FFTN, 0.0f); anelS.assign (FFTN, 0.0f);
        quadro.assign (FFTN, 0.0f);
        pot.assign (FFTN / 2 + 1, 0.0f);
        for (int b = 0; b < NB; ++b) { telaIn[b] = 0.0f; telaOut[b] = 0.0f; aprEsp[b] = 0.0f; }
        for (int i = 0; i < kVizN; ++i) { vizIn[i].store (0.0f, std::memory_order_relaxed); vizOut[i].store (0.0f, std::memory_order_relaxed); }
        for (int z = 0; z < NZ; ++z) zonaDb[z] = 0.0f;
        for (int m = 0; m < NM; ++m) { grBanda[m] = 0.0f; inBanda[m] = -120.0f; outBanda[m] = -120.0f; limiarAgora[m] = 0.0f; largura[m] = 1.0f; razaoAgora[m] = 1.0f; }
        prepare (48000.0);
    }

    void prepare (double fs_)
    {
        fs = fs_ > 1000.0 ? fs_ : 48000.0;
        pb.calcula (fs);
        ldIn.prepare (fs); ldOut.prepare (fs);
        nLook = std::max (8, (int) std::lround (fs * 0.0015));
        // fase linear: bloco de 512 e filtros de 4096 a 44,1/48 kHz (o grave até 30 Hz sai a 0,1 dB da curva; com
        // 2048 errava 0,6 dB abaixo de 80 Hz); o mesmo TEMPO nas taxas altas
        int r = 1;
        while (fs > 70000.0 * r && r < 8) r *= 2;
        fl.prepara (fs, 512 * r, 8);
        latLim = nLook + kTpAtraso;
        latencia = fl.latencia() + latLim;
        for (auto& d : linhaSeco) d.assign ((size_t) (latencia + 1), 0.0f);
        for (auto& d : linhaLim) d.assign ((size_t) (latLim + 1), 0.0f);
        posSeco = posLim = 0;
        tpLim.zera(); tpOut.zera();
        reqAnel.assign ((size_t) (nLook + 1), 1.0f);
        boxAnel.assign ((size_t) (nLook + 1), 1.0f);
        posW = 0; boxSoma = (double) (nLook + 1); gSol = 1.0;
        for (int z = 0; z < NZ; ++z) { det[z].zera(); detP[z] = 0; gZona[z] = 0; det[z].k = passaBanda (fs, zona (z).f, zona (z).q); }
        detTot = 0;
        for (int m = 0; m < NM; ++m) { envC[m] = -120.0f; grC[m] = 0.0f; grMedia[m] = 0.0f; rmsBanda[m] = 0; }
        cDet = 1.0 - std::exp (-1.0 / (0.012 * fs));
        // ganhos (TONE, TILT, DYN EQ) andam por bloco da fase linear, com rampa dentro do bloco
        const float blq = (float) fl.bloco();
        cAtq = 1.0f - std::exp (-blq / (0.010f * (float) fs));
        cRel = 1.0f - std::exp (-blq / (0.150f * (float) fs));
        cEst = 1.0f - std::exp (-blq / (0.050f * (float) fs));
        cMedia = 1.0f - std::exp (-1.0f / (4.0f * (float) fs));
        cRms = 1.0 - std::exp (-1.0 / (0.3 * fs));
        for (auto& s : simF) s.zera();
        simAtual = -1;
        posAnel = cheio = posAnelS = cheioS = 0;
        refPos = 0;
        recalcEq (true);
    }

    int latenciaAmostras() const { return latencia; }
    double taxa() const { return fs; }

    // ---------------------------------------------------------------------------------------------------- estado
    Feitico feitico;                            // o processador grava / restaura com o áudio parado (setState)
    std::atomic<int> modo { 0 };                // 0 parado, 1 LISTEN, 2 CASTING (acertando o DRIVE), 3 CAST
    std::atomic<int> genListen { 0 }, genCast { 0 }, genReset { 0 };
    std::atomic<int> style { 0 }, alvoOpcao { 0 };
    std::atomic<bool> casaTom { true }, casaLoud { true };
    std::atomic<Referencia*> ref { nullptr };

    // ---------------------------------------------------------------------------------------------------- telas
    std::atomic<float> telaIn[NB], telaOut[NB];     // potência lenta
    std::atomic<float> aprEsp[NB];                  // LISTEN: espectro médio (BEFORE)
    std::atomic<float> zonaDb[NZ];                  // DYN EQ agora (dB)
    std::atomic<float> grBanda[NM], inBanda[NM], outBanda[NM], limiarAgora[NM], largura[NM], razaoAgora[NM];
    std::atomic<float> grLim { 0.0f }, tpOutDb { -200.0f }, tpMaxDb { -200.0f };
    std::atomic<float> lufsIn { -200.0f }, lufsOut { -200.0f }, curtoIn { -200.0f }, curtoOut { -200.0f };
    std::atomic<float> plr { 0.0f }, correl { 1.0f };
    std::atomic<float> aprSegundos { 0.0f }, castSegundos { 0.0f }, driveAgora { 0.0f };
    std::atomic<int> clipou { 0 }, aprQuadros { 0 };
    std::atomic<int> alvoAlto { 0 };                // o CAST parou antes do alvo (limitador já tirava > 5 dB)

    // ANALISADOR da tela (estilo Pro-Q, 10 Hz – 35 kHz): o áudio só grava o mono da ENTRADA e da SAÍDA num anel; a
    // FFT grande roda na thread da interface (custo zero no áudio). vizPos = próxima posição a escrever.
    static constexpr int kVizN = 1 << 15;           // cabe a FFT de 32768 (192 kHz)
    std::unique_ptr<std::atomic<float>[]> vizIn { new std::atomic<float>[kVizN] }, vizOut { new std::atomic<float>[kVizN] };
    std::atomic<int> vizPosIn { 0 }, vizPosOut { 0 };

    // ---------------------------------------------------------------------------------------------------- áudio
    void process (float* const* ch, int nch, int n, const Ajustes& a)
    {
        if (n <= 0 || nch <= 0) return;
        amt = clampf (a.amount, 0.0f, 1.0f);
        tiltAlvo = clampf (a.tilt, -3.0f, 3.0f);
        aj = a;
        if (const int g = genReset.load(); g != genResetVisto) { genResetVisto = g; feitico = Feitico {}; modo = 0; ldOut.zeraIntegrado(); }
        if (const int g = genListen.load(); g != genListenVisto) { genListenVisto = g; zeraAprendido(); modo = 1; }
        if (const int g = genCast.load(); g != genCastVisto) { genCastVisto = g; cast(); }
        if (const int g = genRestaura.load(); g != genRestauraVisto) { genRestauraVisto = g; feitico = pendente; modo = feitico.feito ? 3 : 0; }

        float* L = ch[0];
        float* R = nch > 1 ? ch[1] : nullptr;
        {
            const float* e[2] = { L, R ? R : L };
            ldIn.processa (e, nch > 1 ? 2 : 1, n);
        }
        // análise da ENTRADA (mono) e linha do seco (B / BYPASS) com a mesma latência
        if ((int) seco[0].size() < n) for (auto& s : seco) s.resize ((size_t) n);
        const int LS = latencia + 1;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            anel[(size_t) posAnel] = 0.5f * (l + r);
            vizIn[vizPI].store (anel[(size_t) posAnel], std::memory_order_relaxed);
            vizPI = (vizPI + 1) & (kVizN - 1);
            posAnel = (posAnel + 1) % FFTN;
            if (++cheio >= HOP) { cheio = 0; analisaQuadro (true); }
            linhaSeco[0][(size_t) posSeco] = l; linhaSeco[1][(size_t) posSeco] = r;
            posSeco = (posSeco + 1) % LS;
            seco[0][(size_t) i] = linhaSeco[0][(size_t) posSeco];
            seco[1][(size_t) i] = linhaSeco[1][(size_t) posSeco];
        }
        vizPosIn.store (vizPI, std::memory_order_release);

        // TONE MATCH + TILT + DYNAMIC EQ + crossover, tudo em fase linear (FaseLinear), e o multibanda por amostra
        const bool feito = feitico.feito;
        prepMultibanda (a);
        for (int i = 0; i < n; ++i)
        {
            if (fl.cheio()) blocoFaseLinear (feito);
            float bl[NM], br[NM], dm;
            fl.amostra (L[i], R ? R[i] : L[i], bl, br, dm);
            // detectores da DYN EQ: mono, depois do TONE / TILT e antes da DYN (o ganho novo entra no próximo bloco)
            detTot += cDet * ((double) dm * dm - detTot);
            for (int z = 0; z < NZ; ++z)
            {
                const float y = det[z].passa (dm, 0);
                detP[z] += cDet * ((double) y * y - detP[z]);
            }
            float sl, sr;
            multibandaAmostra (bl, br, feito, sl, sr);
            L[i] = sl;
            if (R) R[i] = sr;
        }
        fimMultibanda();
        for (int z = 0; z < NZ; ++z) zonaDb[z] = gZona[z];

        // DRIVE + LIMITADOR true peak (atrasa `latencia`)
        const float drv = feito ? feitico.drive : 0.0f;
        driveAgora = drv;
        limita (L, R, n, drv, a.character);

        mede (L, R, n);

        // malha do DRIVE depois do CAST: até ~24 s tocando; a cada 2 s mede o integrado da janela e corrige. Com o
        // limitador trabalhando, 1 dB de DRIVE dá menos de 1 dB de LUFS: a inclinação é medida passo a passo
        if (modo.load() == 2)
        {
            if (ldOut.curto > -50.0f) castT += (double) n / fs;
            castSegundos = (float) castT;
            if (castT - castUltimo >= 2.0 && ldOut.integrado > -60.0f)
            {
                castUltimo = castT;
                const float medido = ldOut.integrado, erro = feitico.alvoLufs - medido;
                if (passoAnt != 0.0f && std::abs (passoAnt) > 0.2f)
                    inclinacao = clampf ((medido - lufsAnt) / passoAnt, 0.2f, 1.0f);
                float passo = clampf (0.85f * erro / inclinacao, -6.0f, 6.0f);
                // freio: com o limitador tirando mais de 5 dB em média, não sobe mais (o alvo é alto demais para a mix)
                const float grMedio = nGrJan > 0 ? (float) (somaGrJan / nGrJan) : 0.0f;
                somaGrJan = 0; nGrJan = 0;
                if (grMedio < -kGrMax) { passo = std::min (passo, 0.8f * (grMedio + kGrMax)); alvoAlto = 1; }
                else if (alvoAlto.load() && passo > 0.0f) passo = std::min (passo, 0.5f * (grMedio + kGrMax));
                const float novo = clampf (feitico.drive + passo, -12.0f, 24.0f);
                passoAnt = novo - feitico.drive;
                lufsAnt = medido;
                feitico.drive = novo;
                ldOut.zeraIntegrado();
                if (castT >= 24.0 || std::abs (erro) < 0.2f) modo = 3;
            }
            if (ldOut.curto > -50.0f) { somaGrJan += grLimSuave; ++nGrJan; }
        }
        if (modo.load() == 1 && ldIn.curto > -50.0f) { aprT += (double) n / fs; aprSegundos = (float) aprT; }

        ouve (L, R, n, a);
        simula (L, R, n, a.sim);
    }

    // -------------------------------------------------------------------------------------------- comandos (UI)
    void listen() { ++genListen; }
    void lancaCast() { ++genCast; }
    void reset() { ++genReset; }
    // projeto aberto: o CAST gravado entra no próximo bloco (o áudio copia; a interface não mexe no do áudio)
    void restaura (const Feitico& f) { pendente = f; ++genRestaura; }

    // curva do TONE MATCH + TILT (+ DYN EQ agora), para a tela (dB em f)
    double respostaAgora (double f, bool comDyn) const
    {
        float g[NF] {};
        for (int k = 0; k < NE; ++k) g[k] = tomSuave[k];
        g[kIdxTilt] = -tiltSuave * 0.5f; g[kIdxTilt + 1] = tiltSuave * 0.5f;
        if (comDyn) for (int z = 0; z < NZ; ++z) g[kIdxDyn + z] = zonaDb[z].load();
        return modeloDb (g, fs, f);
    }
    int numFiltrosFaseLinear() const { return fl.numFiltros(); }

    // forma alvo (dB, média 0): a da referência, se há; senão a do STYLE
    void formaAlvo (float* out) const
    {
        const auto* r = ref.load();
        if (r != nullptr && r->an.ok) { formaDb (r->an.espectro, out); return; }
        float p[NB];
        for (int b = 0; b < NB; ++b) p[b] = (float) potDe (alvoStyle (style.load(), b));
        formaDb (p, out);
    }
    float lufsAlvo() const
    {
        const int o = alvoOpcao.load();
        if (o > 0) return lufsOpcao (o);
        const auto* r = ref.load();
        if (casaLoud.load() && r != nullptr && r->an.ok && r->an.lufs > -40.0f) return clampf (r->an.lufs, -16.0f, -4.0f);
        return lufsStyle (style.load());
    }

private:
    void zeraAprendido()
    {
        for (int b = 0; b < NB; ++b) { somaApr[b] = 0; aprEsp[b] = 0.0f; }
        for (int m = 0; m < NM; ++m) somaBanda[m] = 0;
        nAprQ = 0; nAprB = 0; aprT = 0; aprSegundos = 0.0f; aprQuadros = 0;
        ldIn.zeraIntegrado();
    }

    void analisaQuadro (bool entrada)
    {
        const auto& a = entrada ? anel : anelS;
        const int base = entrada ? posAnel : posAnelS;
        for (int i = 0; i < FFTN; ++i) quadro[(size_t) i] = a[(size_t) ((base + i) % FFTN)] * jan[(size_t) i];
        fft.potencia (quadro.data(), pot.data());
        double e = 0;
        float bs[NB];
        for (int b = 0; b < NB; ++b)
        {
            double s = 0;
            for (auto& w : pb.p[b]) s += pot[(size_t) w.bin] * w.w;
            bs[b] = (float) (s * norma);
            e += bs[b];
        }
        auto* tela = entrada ? telaIn : telaOut;
        for (int b = 0; b < NB; ++b) tela[b] = tela[b].load() + 0.08f * (bs[b] - tela[b].load());
        if (entrada && modo.load() == 1 && dbDe (e) > -55.0f)
        {
            ++nAprQ;
            aprQuadros = nAprQ;
            for (int b = 0; b < NB; ++b) { somaApr[b] += bs[b]; aprEsp[b] = (float) (somaApr[b] / nAprQ); }
        }
    }

    // ---------------------------------------------------------------------------------------------------- CAST
    void cast()
    {
        if (nAprQ < 4) return;   // sem LISTEN (a interface avisa antes)
        Feitico f;
        f.feito = true;
        float atual[NB], potA[NB], alvoP[NB];
        for (int b = 0; b < NB; ++b) potA[b] = (float) (somaApr[b] / nAprQ);
        formaDb (potA, atual);
        formaAlvo (f.alvoForma);
        // diferença por banda → por oitava → sinos (4 passadas: os sinos vizinhos se somam)
        float quer[NE] {};
        for (int k = 0; k < NE; ++k)
        {
            double s = 0;
            for (int j = 0; j < 3; ++j) s += f.alvoForma[3 * k + j] - atual[3 * k + j];
            quer[k] = casaTom.load() ? clampf ((float) (s / 3.0) * 0.85f, -6.0f, 6.0f) : 0.0f;
        }
        // a resposta é a do caminho de fase linear (sinos em paralelo), o mesmo modelo que o áudio usa
        auto soTom = [] (const float* tom, float* g) { for (int i = 0; i < NF; ++i) g[i] = i < NE ? tom[i] : 0.0f; };
        for (int it = 0; it < 6; ++it)
        {
            float g[NF], r[NE];
            soTom (f.tom, g);
            for (int k = 0; k < NE; ++k) r[k] = (float) modeloDb (g, 48000.0, freqEq (k));
            for (int k = 0; k < NE; ++k) f.tom[k] = clampf (f.tom[k] + 0.7f * (quer[k] - r[k]), -6.0f, 6.0f);
        }
        for (int b = 0; b < NB; ++b) alvoP[b] = (float) potDe (f.alvoForma[b]);
        for (int z = 0; z < NZ; ++z) f.relAlvo[z] = relZona (alvoP, z);
        // multibanda: limiar = nível médio da banda no LISTEN + a mudança que o TONE faz nela
        for (int m = 0; m < NM; ++m)
        {
            const double lo = m == 0 ? 0.0 : fCorte[m - 1], hi = m == NM - 1 ? 1e9 : fCorte[m];
            double s = 0, sT = 0;
            float g[NF];
            soTom (f.tom, g);
            for (int b = 0; b < NB; ++b)
            {
                const double fb = freqBanda (b);
                if (fb < lo || fb >= hi) continue;
                const double eqDb = modeloDb (g, 48000.0, fb);
                s += potA[b] * potDe (eqDb);
                sT += potA[b];
            }
            const float muda = sT > 0 ? dbDe (s / sT) : 0.0f;
            const float nivel = nAprB > 0 ? dbDe (somaBanda[m] / nAprB) : -30.0f;
            f.limiar[m] = nivel + muda;
        }
        f.alvoLufs = lufsAlvo();
        f.antesLufs = ldIn.integrado;
        f.drive = ldIn.integrado > -60.0f ? clampf (f.alvoLufs - ldIn.integrado, -12.0f, 24.0f) : 0.0f;
        f.casamentoAntes = casamento (atual, f.alvoForma);
        feitico = f;
        castT = 0; castUltimo = 0;
        passoAnt = 0.0f; lufsAnt = -200.0f; inclinacao = 1.0f; somaGrJan = 0; nGrJan = 0; alvoAlto = 0;
        ldOut.zeraIntegrado();
        tpMaxDb = -200.0f; clipou = 0;
        modo = 2;
    }

    // TONE e TILT andam suaves (~50 ms) até o alvo; um passo por bloco da fase linear
    void recalcEq (bool ja)
    {
        const float c = ja ? 1.0f : cEst;
        for (int k = 0; k < NE; ++k)
        {
            const float alvo = feitico.feito ? amt * feitico.tom[k] : 0.0f;
            tomSuave[k] += c * (alvo - tomSuave[k]);
            if (std::abs (tomSuave[k] - alvo) < 0.005f) tomSuave[k] = alvo;
        }
        tiltSuave += c * (tiltAlvo - tiltSuave);
        if (std::abs (tiltSuave - tiltAlvo) < 0.005f) tiltSuave = tiltAlvo;
    }

    // a cada bloco da fase linear: TONE / TILT suaves, ganho de cada ponto da DYN EQ e os ganhos para os filtros
    void blocoFaseLinear (bool feito)
    {
        recalcEq (false);
        const bool man = aj.eqManual;
        // MANUAL sem CAST: a energia de cada ponto na curva alvo (STYLE / REFERÊNCIA), refeita a cada ~0,3 s
        if (man && ! feito && (contaRelSemCast++ % 32) == 0)
        {
            float forma[NB], p[NB];
            formaAlvo (forma);
            for (int b = 0; b < NB; ++b) p[b] = (float) potDe (forma[b]);
            for (int z = 0; z < NZ; ++z) relSemCast[z] = relZona (p, z);
        }
        for (int z = 0; z < NZ; ++z)
        {
            const auto& Z = zona (z);
            float alvo = 0.0f;
            if ((feito || man) && detTot > 1e-7 && (! man || aj.eqOn[z]))
            {
                const float rel = feito ? feitico.relAlvo[z] : relSemCast[z];
                const float desvio = dbDe (detP[z] / detTot) - rel;   // > 0 sobra, < 0 falta
                const float tol = man ? clampf (aj.eqThr[z], 0.0f, 12.0f) : 1.5f;
                const float mx = man ? clampf (aj.eqRange[z], 0.0f, 8.0f) : Z.max;
                if (Z.sinal < 0 && desvio > tol) alvo = -std::min (mx, (desvio - tol) * 0.8f);
                if (Z.sinal > 0 && desvio < -tol && detTot > 1e-5) alvo = std::min (mx, (-desvio - tol) * 0.6f);
                if (! man) alvo *= amt;
            }
            gZona[z] += (std::abs (alvo) > std::abs (gZona[z]) ? cAtq : cRel) * (alvo - gZona[z]);
            if (std::abs (gZona[z]) < 0.01f && alvo == 0.0f) gZona[z] = 0.0f;
        }
        float g[NF];
        for (int k = 0; k < NE; ++k) g[k] = std::pow (10.0f, tomSuave[k] / 20.0f);
        g[kIdxTilt] = std::pow (10.0f, -tiltSuave * 0.5f / 20.0f);
        g[kIdxTilt + 1] = std::pow (10.0f, tiltSuave * 0.5f / 20.0f);
        for (int z = 0; z < NZ; ++z) g[kIdxDyn + z] = std::pow (10.0f, gZona[z] / 20.0f);
        fl.processaBloco (g);
    }

    // ------------------------------------------------------------------------------------------------ multibanda
    // as 4 bandas chegam separadas pelo crossover de fase linear (FaseLinear); aqui, compressor e estéreo por banda
    void prepMultibanda (const Ajustes& a)
    {
        static const float atqBase[NM] = { 30.0f, 15.0f, 5.0f, 2.0f }, relBase[NM] = { 160.0f, 120.0f, 80.0f, 60.0f };
        static const float razBase[NM] = { 2.0f, 2.5f, 3.0f, 1.6f };
        const float fp = std::pow (2.0f, clampf (a.punch, -5.0f, 5.0f) / 2.5f);   // PUNCH > 0 = ataque mais lento
        const float wid = clampf (a.width, 0.0f, 2.0f);
        mbLados[0] = a.monoBass ? 0.0f : 1.0f; mbLados[1] = 1.0f + (wid - 1.0f) * 0.5f; mbLados[2] = wid; mbLados[3] = wid;
        mbQuente = clampf (a.character, 0.0f, 1.0f);
        mbAtivo = feitico.feito || a.mbManual;
        for (int k = 0; k < NM; ++k)
        {
            if (a.mbManual)
            {
                mbCA[k] = 1.0f - std::exp (-1.0f / (clampf (a.mbAtk[k], 0.1f, 300.0f) * 0.001f * (float) fs));
                mbCR[k] = 1.0f - std::exp (-1.0f / (clampf (a.mbRel[k], 5.0f, 3000.0f) * 0.001f * (float) fs));
                mbRaz[k] = clampf (a.mbRat[k], 1.0f, 20.0f);
                mbLim[k] = clampf (a.mbThr[k], -60.0f, 0.0f);
                mbGanho[k] = std::pow (10.0f, clampf (a.mbGain[k], -12.0f, 12.0f) / 20.0f);
            }
            else
            {
                mbCA[k] = 1.0f - std::exp (-1.0f / (atqBase[k] * fp * 0.001f * (float) fs));
                mbCR[k] = 1.0f - std::exp (-1.0f / (relBase[k] * 0.001f * (float) fs));
                mbRaz[k] = 1.0f + (razBase[k] - 1.0f) * (0.6f + 0.8f * mbQuente) * (0.4f + 0.6f * amt);
                mbLim[k] = feitico.limiar[k] + 4.0f - 6.0f * amt;   // AMOUNT baixo = limiar acima do nível médio
                mbGanho[k] = 1.0f;
            }
            mbSLados[k] = 0; mbSMeio[k] = 0;
        }
    }

    inline void multibandaAmostra (const float* bl, const float* br, bool feito, float& sl, float& sr)
    {
        sl = 0.0f; sr = 0.0f;
        for (int k = 0; k < NM; ++k)
        {
            const float pk = std::max (bl[k] * bl[k], br[k] * br[k]);
            rmsBanda[k] += cRms * ((double) pk - rmsBanda[k]);
            const float nivel = dbDe ((double) pk + 1e-20);
            envC[k] += (nivel > envC[k] ? mbCA[k] : mbCR[k]) * (nivel - envC[k]);
            float gr = 0.0f;
            if (mbAtivo)
            {
                constexpr float joelho = 6.0f;
                const float acima = envC[k] - mbLim[k];
                float sobra = 0.0f;
                if (acima > joelho * 0.5f) sobra = acima;
                else if (acima > -joelho * 0.5f) sobra = (acima + joelho * 0.5f) * (acima + joelho * 0.5f) / (2.0f * joelho);
                gr = -sobra * (1.0f - 1.0f / mbRaz[k]);
            }
            grC[k] = gr;
            grMedia[k] += cMedia * (gr - grMedia[k]);   // reposição automática (~4 s): o balanço da banda fica
            const float g = std::exp ((gr - grMedia[k]) * 0.11512925f) * mbGanho[k];
            float l = bl[k] * g, r = br[k] * g;
            if (feito && mbQuente > 0.02f && k < 2)     // CHARACTER quente: saturação suave nos graves
            {
                const float dr = 1.0f + 2.0f * mbQuente;
                l = std::tanh (l * dr) / dr; r = std::tanh (r * dr) / dr;
            }
            const float meio = 0.5f * (l + r), lado = 0.5f * (l - r) * mbLados[k];
            mbSMeio[k] += (double) meio * meio; mbSLados[k] += (double) lado * lado;
            sl += meio + lado; sr += meio - lado;
        }
    }

    void fimMultibanda()
    {
        for (int k = 0; k < NM; ++k)
        {
            grBanda[k] = grC[k];
            inBanda[k] = envC[k];
            outBanda[k] = envC[k] + grC[k] - grMedia[k] + 20.0f * std::log10 (std::max (1e-6f, mbGanho[k]));
            limiarAgora[k] = mbAtivo ? mbLim[k] : 0.0f;
            razaoAgora[k] = mbRaz[k];
            if (mbSMeio[k] > 1e-12) largura[k] = largura[k].load() + 0.1f * ((float) std::sqrt (mbSLados[k] / mbSMeio[k]) - largura[k].load());
        }
        if (modo.load() == 1 && ldIn.curto > -50.0f)
        {
            for (int k = 0; k < NM; ++k) somaBanda[k] += rmsBanda[k];
            ++nAprB;
        }
    }

    // ------------------------------------------------------------------------------------------------ limitador
    // req[t] = ganho que o pico (true peak) da amostra t − kTpAtraso pede. G = média móvel (W) do mínimo móvel (W)
    // dos req, com soltura: a amostra que sai (latencia atrás) recebe G ≤ o req dela (fica abaixo do teto)
    void limita (float* L, float* R, int n, float driveDb, float character)
    {
        const float g0 = std::pow (10.0f, driveDb / 20.0f);
        const float teto = std::pow (10.0f, kTeto / 20.0f);
        const int W = nLook + 1, LL = latLim + 1;
        const double cSol = 1.0 - std::exp (-1.0 / ((0.060 + 0.080 * character) * fs));
        float gMin = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i] * g0, r = (R ? R[i] : L[i]) * g0;
            const float pk = tpLim.empurra (l, r, R ? 2 : 1);
            reqAnel[(size_t) posW] = pk > teto ? teto / pk : 1.0f;
            float mn = 1.0f;
            for (float v : reqAnel) mn = std::min (mn, v);
            gSol = mn < gSol ? mn : gSol + cSol * (mn - gSol);
            boxSoma += gSol - boxAnel[(size_t) posW];
            boxAnel[(size_t) posW] = (float) gSol;
            posW = (posW + 1) % W;
            const float g = (float) std::min (1.0, boxSoma / W);
            linhaLim[0][(size_t) posLim] = l; linhaLim[1][(size_t) posLim] = r;
            posLim = (posLim + 1) % LL;
            const float ol = linhaLim[0][(size_t) posLim] * g, orr = linhaLim[1][(size_t) posLim] * g;
            L[i] = clampf (ol, -teto, teto);   // segurança: nunca passa do teto (amostra)
            if (R) R[i] = clampf (orr, -teto, teto);
            gMin = std::min (gMin, g);
        }
        boxSoma = 0;   // tira o erro acumulado da soma
        for (float v : boxAnel) boxSoma += v;
        const float d = 20.0f * std::log10 (std::max (1e-6f, gMin));
        grLimSuave = d < grLimSuave ? d : grLimSuave + 0.15f * (d - grLimSuave);
        grLim = grLimSuave;
    }

    // ------------------------------------------------------------------------------------------------ medidas
    void mede (const float* L, const float* R, int n)
    {
        const float* s[2] = { L, R ? R : L };
        ldOut.processa (s, R ? 2 : 1, n);
        double sLL = 0, sRR = 0, sLR = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            tpJanela = std::max (tpJanela, tpOut.empurra (l, r, R ? 2 : 1));
            sLL += (double) l * l; sRR += (double) r * r; sLR += (double) l * r;
            anelS[(size_t) posAnelS] = 0.5f * (l + r);
            vizOut[vizPO].store (anelS[(size_t) posAnelS], std::memory_order_relaxed);
            vizPO = (vizPO + 1) & (kVizN - 1);
            posAnelS = (posAnelS + 1) % FFTN;
            if (++cheioS >= HOP) { cheioS = 0; analisaQuadro (false); }
        }
        vizPosOut.store (vizPO, std::memory_order_release);
        if (sLL > 1e-9 && sRR > 1e-9) correlSuave += 0.05f * ((float) (sLR / std::sqrt (sLL * sRR)) - correlSuave);
        correl = correlSuave;
        contaTp += n;
        if (contaTp > fs * 0.1)
        {
            const float d = 20.0f * std::log10 (std::max (1e-9f, tpJanela));
            tpOutDb = d;
            tpHist[tpPos] = d; tpPos = (tpPos + 1) % 30;   // 3 s
            float mx = -200.0f;
            for (float v : tpHist) mx = std::max (mx, v);
            if (d > tpMaxDb.load()) tpMaxDb = d;
            if (d > kTeto + 0.1f) clipou = 1;
            plr = ldOut.curto > -60.0f ? mx - ldOut.curto : 0.0f;
            tpJanela = 0.0f; contaTp = 0;
        }
        lufsIn = ldIn.integrado; lufsOut = ldOut.integrado;
        curtoIn = ldIn.curto; curtoOut = ldOut.curto;
    }

    // ------------------------------------------------------------------------------------------------ monitor
    void ouve (float* L, float* R, int n, const Ajustes& a)
    {
        // igualador de loudness: (saída − entrada) em LUFS curto, lento
        if (ldOut.curto > -60.0f && ldIn.curto > -60.0f) difLoud += 0.02f * ((ldOut.curto - ldIn.curto) - difLoud);
        if (a.ouvir == 1 || a.ouvir == 3)
        {
            const float g = a.ouvir == 1 ? std::pow (10.0f, clampf (difLoud, -24.0f, 24.0f) / 20.0f) : 1.0f;
            for (int i = 0; i < n; ++i)
            {
                L[i] = seco[0][(size_t) i] * g;
                if (R) R[i] = seco[1][(size_t) i] * g;
            }
            return;
        }
        const auto* r = ref.load();
        if (a.ouvir == 2 && r != nullptr && r->L.size() > 1000 && r->R.size() == r->L.size())
        {
            // REF no loudness da saída (LUFS integrado da referência vs curto da saída)
            const float alvo = ldOut.curto > -60.0f ? ldOut.curto : feitico.alvoLufs;
            const float g = std::pow (10.0f, clampf (alvo - r->an.lufs, -24.0f, 12.0f) / 20.0f);
            const int64_t N = (int64_t) r->L.size();
            for (int i = 0; i < n; ++i)
            {
                const size_t p = (size_t) (refPos % N);
                L[i] = r->L[p] * g;
                if (R) R[i] = r->R[p] * g;
                ++refPos;
            }
        }
    }

    void simula (float* L, float* R, int n, int sim)
    {
        if (sim != simAtual)
        {
            simAtual = sim;
            for (auto& s : simF) { s.zera(); s.k = Coef {}; }
            if (sim == 1)        // PHONE: alto-falante de celular (mono, sem grave, pico em 2–3 kHz)
            {
                simF[0].k = passaAltas (fs, 320.0, 0.9); simF[1].k = passaAltas (fs, 320.0, 0.9);
                simF[2].k = sino (fs, 2500.0, 1.0, 5.0); simF[3].k = passaBaixas (fs, 11000.0, 0.7);
            }
            else if (sim == 2)   // EARBUDS baratos: grave fraco, agudo áspero
            {
                simF[0].k = passaAltas (fs, 90.0, 0.7); simF[1].k = shelf (fs, 200.0, -5.0, false);
                simF[2].k = sino (fs, 4000.0, 1.2, 4.0); simF[3].k = passaBaixas (fs, 15000.0, 0.7);
            }
            else if (sim == 3)   // CAR: grave inflado, médio embolado, agudo abafado
            {
                simF[0].k = shelf (fs, 80.0, 6.0, false); simF[1].k = sino (fs, 300.0, 1.0, 3.0);
                simF[2].k = shelf (fs, 6000.0, -5.0, true); simF[3].k = passaAltas (fs, 30.0, 0.7);
            }
        }
        if (sim == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R ? R[i] : L[i];
            if (sim == 1 || sim == 4) l = r = 0.5f * (l + r);
            if (sim != 4)
                for (auto& f : simF) { l = f.passa (l, 0); r = f.passa (r, 1); }
            L[i] = l;
            if (R) R[i] = r;
        }
    }

    // ---------------------------------------------------------------------------------------------------- dados
    double fs = 48000.0;
    float amt = 0.72f;
    PesosBandas pb;
    FftReal fft;
    std::vector<float> jan, anel, anelS, quadro, pot;
    float norma = 1.0f;
    int posAnel = 0, cheio = 0, posAnelS = 0, cheioS = 0;
    int vizPI = 0, vizPO = 0;
    Loudness ldIn, ldOut;
    double somaApr[NB] {}, somaBanda[NM] {};
    int nAprQ = 0, nAprB = 0;
    double aprT = 0, castT = 0, castUltimo = 0;
    float passoAnt = 0.0f, lufsAnt = -200.0f, inclinacao = 1.0f;
    double somaGrJan = 0;
    int nGrJan = 0;
    static constexpr float kGrMax = 5.0f;   // dB de redução média do limitador que o CAST aceita
    int genListenVisto = 0, genCastVisto = 0, genResetVisto = 0, genRestauraVisto = 0;
    Feitico pendente;
    std::atomic<int> genRestaura { 0 };

    FaseLinear fl;
    Biquad det[NZ];
    float tomSuave[NE] {}, tiltSuave = 0.0f, tiltAlvo = 0.0f;
    double detP[NZ] {}, detTot = 0, cDet = 0.001, cRms = 0.0001;
    float gZona[NZ] {};
    float cAtq = 0.1f, cRel = 0.01f, cEst = 0.05f, cMedia = 1e-5f;

    float envC[NM] {}, grC[NM] {}, grMedia[NM] {};
    float mbCA[NM] {}, mbCR[NM] {}, mbRaz[NM] {}, mbLim[NM] {}, mbLados[NM] {}, mbQuente = 0.0f;
    float mbGanho[NM] { 1.0f, 1.0f, 1.0f, 1.0f };
    bool mbAtivo = false;
    Ajustes aj;                                  // os do bloco atual (a DYN EQ lê o MANUAL)
    float relSemCast[NZ] {};
    unsigned contaRelSemCast = 0;
    double mbSLados[NM] {}, mbSMeio[NM] {};
    double rmsBanda[NM] {};

    int nLook = 72, latLim = 80, latencia = 80;
    std::vector<float> linhaSeco[2], linhaLim[2], seco[2];
    int posSeco = 0, posLim = 0;
    TruePeak tpLim, tpOut;
    std::vector<float> reqAnel, boxAnel;
    int posW = 0;
    double boxSoma = 1.0, gSol = 1.0;
    float grLimSuave = 0.0f, tpJanela = 0.0f, tpHist[30] {}, correlSuave = 1.0f;
    int tpPos = 0;
    double contaTp = 0;
    float difLoud = 0.0f;
    int64_t refPos = 0;
    Biquad simF[4];
    int simAtual = -1;
};
} // namespace magos
