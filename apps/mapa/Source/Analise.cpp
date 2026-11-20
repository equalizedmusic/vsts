// MAPA — análise da música de referência (C++17 puro, sem JUCE).
// Etapas: mono -> 22 kHz -> espectrogramas -> BPM e grade (tempo forte) -> tom -> compassos e passos de 1/16 ->
// elementos por compasso (kick, baixo, hats, clap, pad, lead) com notas -> partes (novidade entre blocos de 4/8
// compassos) -> nomes das partes -> FX (risers antes da virada) -> clipes por faixa.
#include "Analise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <numeric>
#ifdef MAPA_DEBUG
#include <cstdio>
#endif

namespace mapa
{
namespace
{
constexpr double PI = 3.14159265358979323846;

const char* NOMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// ------------------------------------------------------------------ utilidades
double mediana (std::vector<double> v)
{
    if (v.empty()) return 0;
    std::nth_element (v.begin(), v.begin() + (long) (v.size() / 2), v.end());
    return v[v.size() / 2];
}

double percentil (std::vector<double> v, double p)
{
    if (v.empty()) return 0;
    std::sort (v.begin(), v.end());
    const double pos = std::clamp (p, 0.0, 1.0) * (double) (v.size() - 1);
    const size_t i = (size_t) pos;
    const double fr = pos - (double) i;
    if (i + 1 < v.size()) return v[i] * (1 - fr) + v[i + 1] * fr;
    return v[i];
}

double db (double energia) { return 10.0 * std::log10 (energia + 1e-12); }

// FFT radix-2 iterativa, in-place
void fft (std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * PI / (double) len;
        const std::complex<float> wl ((float) std::cos (ang), (float) std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<float> w (1, 0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// passa-baixa FIR (sinc janelado) + decimação inteira
std::vector<float> decimar (const std::vector<float>& x, int fator)
{
    if (fator <= 1) return x;
    const int taps = 16 * fator + 1, meio = taps / 2;
    const double corte = 0.45 / fator;   // fração da taxa original
    std::vector<float> h ((size_t) taps);
    double soma = 0;
    for (int i = 0; i < taps; ++i)
    {
        const double t = i - meio;
        const double s = t == 0 ? 2 * corte : std::sin (2 * PI * corte * t) / (PI * t);
        const double w = 0.42 - 0.5 * std::cos (2 * PI * i / (taps - 1)) + 0.08 * std::cos (4 * PI * i / (taps - 1));
        h[(size_t) i] = (float) (s * w);
        soma += s * w;
    }
    for (auto& v : h) v = (float) (v / soma);
    const long n = (long) x.size();
    std::vector<float> y ((size_t) (n / fator));
    for (long o = 0; o < (long) y.size(); ++o)
    {
        const long c = o * fator;
        double acc = 0;
        for (int i = 0; i < taps; ++i)
        {
            const long idx = c + i - meio;
            if (idx >= 0 && idx < n) acc += h[(size_t) i] * x[(size_t) idx];
        }
        y[(size_t) o] = (float) acc;
    }
    return y;
}

struct Espectro
{
    int N = 2048, hop = 256, bins = 0, frames = 0;
    double sr = 22050;
    std::vector<float> mag;   // frames x bins

    float at (int f, int k) const { return mag[(size_t) f * (size_t) bins + (size_t) k]; }
    int binDe (double hz) const { return std::clamp ((int) std::lround (hz * N / sr), 0, bins - 1); }
    double hzDe (int k) const { return k * sr / N; }
    double tempoDe (double f) const { return (f * hop + N * 0.5) / sr; }   // centro do quadro, segundos
    double quadroDe (double t) const { return (t * sr - N * 0.5) / hop; }
};

bool stft (const std::vector<float>& x, double sr, int N, int hop, Espectro& e,
           const std::function<void (float)>& prog, const std::atomic<bool>* cancelar)
{
    e.N = N; e.hop = hop; e.sr = sr; e.bins = N / 2 + 1;
    const long n = (long) x.size();
    e.frames = n >= N ? (int) ((n - N) / hop + 1) : 1;
    e.mag.assign ((size_t) e.frames * (size_t) e.bins, 0.0f);
    std::vector<float> jan ((size_t) N);
    for (int i = 0; i < N; ++i) jan[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2 * PI * i / N));
    std::vector<std::complex<float>> buf ((size_t) N);
    for (int f = 0; f < e.frames; ++f)
    {
        if ((f & 1023) == 0)
        {
            if (cancelar && cancelar->load()) return false;
            if (prog) prog ((float) f / (float) e.frames);
        }
        const long base = (long) f * hop;
        for (int i = 0; i < N; ++i)
        {
            const long idx = base + i;
            buf[(size_t) i] = { idx < n ? x[(size_t) idx] * jan[(size_t) i] : 0.0f, 0.0f };
        }
        fft (buf);
        float* m = &e.mag[(size_t) f * (size_t) e.bins];
        for (int k = 0; k < e.bins; ++k) m[k] = std::abs (buf[(size_t) k]) * (2.0f / (float) N);
    }
    return true;
}

// energia (soma de mag²) de uma faixa de frequência, por quadro
std::vector<float> energiaBanda (const Espectro& e, double lo, double hi)
{
    std::vector<float> out ((size_t) e.frames);
    const int a = e.binDe (lo), b = std::max (a, e.binDe (hi));
    for (int f = 0; f < e.frames; ++f)
    {
        double s = 0;
        for (int k = a; k <= b; ++k) { const double v = e.at (f, k); s += v * v; }
        out[(size_t) f] = (float) s;
    }
    return out;
}

// fluxo espectral (log) de uma faixa: subidas de energia = ataques
std::vector<float> fluxoBanda (const Espectro& e, double lo, double hi)
{
    std::vector<float> out ((size_t) e.frames, 0.0f);
    const int a = e.binDe (lo), b = std::max (a, e.binDe (hi));
    for (int f = 1; f < e.frames; ++f)
    {
        double s = 0;
        for (int k = a; k <= b; ++k)
        {
            const double d = std::log1p (100.0 * e.at (f, k)) - std::log1p (100.0 * e.at (f - 1, k));
            if (d > 0) s += d;
        }
        out[(size_t) f] = (float) s;
    }
    return out;
}

// tira a média móvel (realça os ataques) e corta o negativo
void realcar (std::vector<float>& v, int janela)
{
    const int n = (int) v.size();
    std::vector<double> acc ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i) acc[(size_t) i + 1] = acc[(size_t) i] + v[(size_t) i];
    std::vector<float> o ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const int a = std::max (0, i - janela), b = std::min (n, i + janela + 1);
        const double m = (acc[(size_t) b] - acc[(size_t) a]) / (b - a);
        o[(size_t) i] = (float) std::max (0.0, v[(size_t) i] - m);
    }
    v.swap (o);
}

// valor interpolado de um envelope por quadro, num instante t (segundos)
float noTempo (const std::vector<float>& env, const Espectro& e, double t)
{
    const double q = e.quadroDe (t);
    if (q < 0 || q >= (double) env.size() - 1) return 0;
    const size_t i = (size_t) q;
    const float fr = (float) (q - (double) i);
    return env[i] * (1 - fr) + env[i + 1] * fr;
}

// máximo de um envelope numa janela [t+a, t+b]
float maxJanela (const std::vector<float>& env, const Espectro& e, double t, double a, double b)
{
    int qa = (int) std::floor (e.quadroDe (t + a)), qb = (int) std::ceil (e.quadroDe (t + b));
    qa = std::max (qa, 0); qb = std::min (qb, (int) env.size() - 1);
    float m = 0;
    for (int q = qa; q <= qb; ++q) m = std::max (m, env[(size_t) q]);
    return m;
}

// média de um envelope numa janela [t0, t1]
double mediaJanela (const std::vector<float>& env, const Espectro& e, double t0, double t1)
{
    int qa = (int) std::floor (e.quadroDe (t0)), qb = (int) std::ceil (e.quadroDe (t1));
    qa = std::max (qa, 0); qb = std::min (qb, (int) env.size() - 1);
    if (qb < qa) return 0;
    double s = 0;
    for (int q = qa; q <= qb; ++q) s += env[(size_t) q];
    return s / (qb - qa + 1);
}

// ------------------------------------------------------------------ tempo
// nota de fase: média do envelope nas posições phi + k*P (k = 0..), para o período P
double notaFase (const std::vector<float>& env, const Espectro& e, double P, double phi, double dur)
{
    double s = 0; int n = 0;
    for (double t = phi; t < dur; t += P) { s += noTempo (env, e, t); ++n; }
    return n ? s / n : 0;
}

void melhorFase (const std::vector<float>& env, const Espectro& e, double P, double dur, int passos,
                 double& faseOut, double& notaOut)
{
    notaOut = -1; faseOut = 0;
    for (int i = 0; i < passos; ++i)
    {
        const double phi = P * i / passos;
        const double s = notaFase (env, e, P, phi, dur);
        if (s > notaOut) { notaOut = s; faseOut = phi; }
    }
    // refino fino em volta
    const double d = P / passos;
    for (int i = -10; i <= 10; ++i)
    {
        double phi = faseOut + d * i / 10.0;
        if (phi < 0) phi += P;
        const double s = notaFase (env, e, P, phi, dur);
        if (s > notaOut) { notaOut = s; faseOut = phi; }
    }
    faseOut = std::fmod (faseOut, P);
}

// ------------------------------------------------------------------ croma
// croma por quadro numa faixa de frequência (12 valores por quadro)
std::vector<float> croma (const Espectro& e, double lo, double hi)
{
    std::vector<float> c ((size_t) e.frames * 12, 0.0f);
    const int a = std::max (1, e.binDe (lo)), b = e.binDe (hi);
    std::vector<int> pc ((size_t) e.bins, -1);
    std::vector<float> peso ((size_t) e.bins, 0.0f);
    for (int k = a; k <= b; ++k)
    {
        const double midi = 69 + 12 * std::log2 (e.hzDe (k) / 440.0);
        const double r = std::round (midi);
        const double dist = std::abs (midi - r);
        pc[(size_t) k] = ((int) r % 12 + 12) % 12;
        peso[(size_t) k] = (float) std::max (0.0, 1.0 - 2.0 * dist);   // bin no meio de dois semitons conta pouco
    }
    for (int f = 0; f < e.frames; ++f)
        for (int k = a; k <= b; ++k)
            if (pc[(size_t) k] >= 0)
                c[(size_t) f * 12 + (size_t) pc[(size_t) k]] += e.at (f, k) * peso[(size_t) k];
    return c;
}

void cromaMedia (const std::vector<float>& c, const Espectro& e, double t0, double t1, double out[12])
{
    for (int i = 0; i < 12; ++i) out[i] = 0;
    int qa = (int) std::floor (e.quadroDe (t0)), qb = (int) std::ceil (e.quadroDe (t1));
    qa = std::max (qa, 0); qb = std::min (qb, e.frames - 1);
    if (qb < qa) return;
    for (int q = qa; q <= qb; ++q)
        for (int i = 0; i < 12; ++i) out[i] += c[(size_t) q * 12 + (size_t) i];
    for (int i = 0; i < 12; ++i) out[i] /= (qb - qa + 1);
}

// perfis de Krumhansl-Kessler
const double PERFIL_MAIOR[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
const double PERFIL_MENOR[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

double correlacao (const double* a, const double* b, int rot)
{
    double ma = 0, mb = 0;
    for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
    ma /= 12; mb /= 12;
    double num = 0, da = 0, dbb = 0;
    for (int i = 0; i < 12; ++i)
    {
        const double x = a[(i + rot) % 12] - ma, y = b[i] - mb;
        num += x * y; da += x * x; dbb += y * y;
    }
    return num / std::sqrt (da * dbb + 1e-12);
}

// ------------------------------------------------------------------ altura do baixo
// nota MIDI (28..50) num trecho do áudio decimado (~2,7 kHz), por YIN (período no tempo). A análise por espectro
// errava um semitom: notas curtas repetidas a cada 1/16 criam um "pente" no espectro que desloca o pico.
// forca = energia do trecho quando achou período confiável; naEscala: se a nota cair fora da escala e a vizinha
// estiver dentro, puxa para ela.
int alturaBaixo (const std::vector<float>& x, double sr, double t0, double t1, const bool* naEscala, double& forca)
{
    forca = 0;
    long a = (long) (t0 * sr), b = (long) (t1 * sr);
    a = std::max (a, 0L); b = std::min (b, (long) x.size());
    const int tauMin = std::max (2, (int) (sr / 150.0)), tauMax = (int) (sr / 40.0) + 2;
    const long W = (b - a) - tauMax - 1;
    if (W < tauMax) return -1;
    std::vector<double> d ((size_t) tauMax + 2, 0.0), cm ((size_t) tauMax + 2, 1.0);
    double energia = 0;
    for (long i = 0; i < W; ++i) energia += (double) x[(size_t) (a + i)] * x[(size_t) (a + i)];
    if (energia <= 1e-9) return -1;
    double soma = 0;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        double s = 0;
        for (long i = 0; i < W; ++i)
        {
            const double v = (double) x[(size_t) (a + i)] - x[(size_t) (a + i + tau)];
            s += v * v;
        }
        d[(size_t) tau] = s;
        soma += s;
        cm[(size_t) tau] = soma > 0 ? s * tau / soma : 1.0;
    }
    // primeiro mergulho abaixo de 0,2 (desce até o fundo); senão o mínimo global
    int tau = -1;
    for (int t = tauMin; t <= tauMax; ++t)
        if (cm[(size_t) t] < 0.2)
        {
            while (t + 1 <= tauMax && cm[(size_t) t + 1] < cm[(size_t) t]) ++t;
            tau = t;
            break;
        }
    if (tau < 0)
    {
        double mn = 1e9;
        for (int t = tauMin; t <= tauMax; ++t) if (cm[(size_t) t] < mn) { mn = cm[(size_t) t]; tau = t; }
        if (mn > 0.45) return -1;   // sem período claro (ruído, kick puro)
    }
    double off = 0;
    if (tau > 1 && tau + 1 <= tauMax + 1)
    {
        const double y0 = cm[(size_t) tau - 1], y1 = cm[(size_t) tau], y2 = cm[(size_t) tau + 1];
        const double den = y0 - 2 * y1 + y2;
        if (std::abs (den) > 1e-12) off = std::clamp (0.5 * (y0 - y2) / den, -0.5, 0.5);
    }
    const double f = sr / (tau + off);
    int midi = (int) std::lround (69 + 12 * std::log2 (f / 440.0));
    if (midi < 28 || midi > 50) return -1;
    if (naEscala && ! naEscala[midi % 12])
    {
        if (naEscala[(midi + 1) % 12] && ! naEscala[(midi + 11) % 12]) ++midi;
        else if (naEscala[(midi + 11) % 12] && ! naEscala[(midi + 1) % 12]) --midi;
    }
    forca = energia / (double) W;
    return midi;
}

// ------------------------------------------------------------------ estruturas por compasso
constexpr int PASSOS = 16;   // 1/16 por compasso de 4/4

struct Compasso
{
    bool temAudio = false;
    double eTot = -120, eLow = -120, eMid = -120, eHigh = -120;   // dB
    float kick[PASSOS] {}, hat[PASSOS] {}, clap[PASSOS] {}, baixoAtq[PASSOS] {};
    double low[PASSOS] {};       // energia do baixo por passo (dB)
    int alturaTempo[4] { -1, -1, -1, -1 };   // nota do baixo em cada tempo
    double cPad[12] {}, cPad1[12] {}, cPad2[12] {};
    double tonalPad = 0, tonalLead = 0, estab = 0, variacao = 0, eLead = -120, eHighRiser = -120;
    int leadPc[PASSOS];
    // presença detectada
    bool kickOn = false, baixoOn = false, hatsOn = false, clapOn = false, padOn = false, leadOn = false;
};

double topoMedia3 (const double c[12])
{
    double v[12];
    std::copy (c, c + 12, v);
    std::sort (v, v + 12, std::greater<double>());
    const double tot = std::accumulate (v, v + 12, 0.0);
    return tot > 0 ? (v[0] + v[1] + v[2]) / tot : 0;
}

double cosseno (const double* a, const double* b)
{
    double n = 0, da = 0, dbb = 0;
    for (int i = 0; i < 12; ++i) { n += a[i] * b[i]; da += a[i] * a[i]; dbb += b[i] * b[i]; }
    return n / std::sqrt (da * dbb + 1e-12);
}

// liga/desliga por compasso -> trechos [ini, fim) limpos
std::vector<std::pair<int, int>> trechos (std::vector<bool> on, const std::vector<int>& cortes)
{
    const int n = (int) on.size();
    // tapa buraco de 1 compasso e tira compasso solto
    for (int j = 1; j + 1 < n; ++j)
        if (! on[(size_t) j] && on[(size_t) j - 1] && on[(size_t) j + 1]) on[(size_t) j] = true;
    for (int j = 0; j < n; ++j)
    {
        const bool ant = j > 0 && on[(size_t) j - 1], dep = j + 1 < n && on[(size_t) j + 1];
        if (on[(size_t) j] && ! ant && ! dep) on[(size_t) j] = false;
    }
    std::vector<std::pair<int, int>> r;
    for (int j = 0; j < n;)
    {
        if (! on[(size_t) j]) { ++j; continue; }
        int k = j;
        while (k < n && on[(size_t) k]) ++k;
        r.push_back ({ j, k });
        j = k;
    }
    // encosta nas viradas de parte quando está a até 2 compassos delas
    for (auto& t : r)
        for (int c : cortes)
        {
            if (std::abs (t.first - c) <= 2) t.first = c;
            if (std::abs (t.second - c) <= 2) t.second = c;
        }
    std::vector<std::pair<int, int>> ok;
    for (auto& t : r)
        if (t.second - t.first >= 2)
        {
            if (! ok.empty() && t.first <= ok.back().second) ok.back().second = std::max (ok.back().second, t.second);
            else ok.push_back (t);
        }
    return ok;
}

uint32_t corParte (const std::string& n)
{
    if (n.rfind ("DROP", 0) == 0) return 0xb8ff3a;
    if (n.rfind ("BUILD", 0) == 0) return 0x22e4ff;
    if (n.rfind ("BREAK", 0) == 0) return 0x9b6bff;
    if (n.rfind ("GROOVE", 0) == 0) return 0xffb03a;
    return 0x5b6170;   // INTRO / OUTRO
}
} // namespace

const char* nomeNota (int pc) { return NOMES[((pc % 12) + 12) % 12]; }

bool analisar (const EntradaAnalise& in, Projeto& out, std::function<void (float, const char*)> progresso,
               const std::atomic<bool>* cancelar)
{
    auto prog = [&] (float p, const char* etapa) { if (progresso) progresso (std::clamp (p, 0.0f, 1.0f), etapa); };
    auto parar = [&] { return cancelar && cancelar->load(); };

    out = Projeto {};
    out.nomeMusica = in.nomeMusica;
    out.caminhoAudio = in.caminhoAudio;
    out.sampleRate = in.sampleRate;
    out.canais = in.canais;
    out.duracaoSeg = in.mono.size() / in.sampleRate;
    if (out.duracaoSeg < 8) return false;

    // ---------------- 1. 22 kHz e espectrogramas
    prog (0.02f, "preparando o áudio");
    const int fator = std::max (1, (int) std::lround (in.sampleRate / 22050.0));
    const std::vector<float> x = decimar (in.mono, fator);
    const double sr = in.sampleRate / fator;
    const double dur = x.size() / sr;
    if (parar()) return false;

    Espectro ep;   // percussivo: 2048 / 256 (~11,6 ms)
    if (! stft (x, sr, 2048, 256, ep, [&] (float p) { prog (0.05f + 0.25f * p, "lendo o espectro"); }, cancelar))
        return false;
    Espectro et;   // tonal: 8192 / 1024 (2,7 Hz por bin)
    if (! stft (x, sr, 8192, 1024, et, [&] (float p) { prog (0.30f + 0.15f * p, "lendo as notas"); }, cancelar))
        return false;

    const int fatorB = std::max (1, (int) std::lround (sr / 2756.0));
    const std::vector<float> xb = decimar (x, fatorB);
    const double srb = sr / fatorB;
    if (parar()) return false;

    prog (0.46f, "achando o BPM");
    auto fluxTot = fluxoBanda (ep, 30, 11000);
    auto fluxKick = fluxoBanda (ep, 30, 110);
    auto fluxBaixo = fluxoBanda (ep, 60, 250);
    auto fluxClap = fluxoBanda (ep, 900, 3500);
    auto fluxHat = fluxoBanda (ep, 6000, 10500);
    const int jm = (int) std::lround (0.25 * sr / ep.hop);
    for (auto* v : { &fluxTot, &fluxKick, &fluxBaixo, &fluxClap, &fluxHat }) realcar (*v, jm);

    const auto eLowF = energiaBanda (ep, 35, 115);   // só o grave de verdade: pad e lead quase nunca descem daqui
    const auto eMidF = energiaBanda (ep, 200, 3000);
    const auto eHighF = energiaBanda (ep, 3000, 10500);
    const auto eLeadF = energiaBanda (et, 400, 4000);
    const auto eRiserF = energiaBanda (ep, 4000, 10500);

    // envelope para o tempo: total + kick, normalizados
    std::vector<float> envT (fluxTot.size());
    {
        const double m1 = std::accumulate (fluxTot.begin(), fluxTot.end(), 0.0) / fluxTot.size() + 1e-9;
        const double m2 = std::accumulate (fluxKick.begin(), fluxKick.end(), 0.0) / fluxKick.size() + 1e-9;
        for (size_t i = 0; i < envT.size(); ++i) envT[i] = (float) (fluxTot[i] / m1 + fluxKick[i] / m2);
    }

    // ---------------- 2. BPM: autocorrelação com pente (1, 2 e 4 tempos) + preferência suave em ~130
    const double hopSeg = ep.hop / sr;
    const int maxLag = (int) std::ceil (4 * 60.0 / 68.0 / hopSeg) + 2;
    std::vector<double> acf ((size_t) maxLag + 1, 0.0);
    {
        const int n = (int) envT.size();
        for (int L = 1; L <= maxLag && L < n; ++L)
        {
            double s = 0;
            for (int i = L; i < n; ++i) s += (double) envT[(size_t) i] * envT[(size_t) i - L];
            acf[(size_t) L] = s / (n - L);
        }
    }
    auto acfI = [&] (double L)
    {
        if (L < 1 || L >= maxLag - 1) return 0.0;
        const size_t i = (size_t) L;
        const double fr = L - (double) i;
        return acf[i] * (1 - fr) + acf[i + 1] * fr;
    };
    double bpm0 = 120, nota0 = -1;
    for (double b = 70; b <= 190; b += 0.25)
    {
        const double L = 60.0 / b / hopSeg;
        const double s = acfI (L) + 0.5 * acfI (2 * L) + 0.25 * acfI (4 * L);
        const double z = std::log2 (b / 130.0) / 0.8;
        const double w = std::exp (-0.5 * z * z);
        if (s * w > nota0) { nota0 = s * w; bpm0 = b; }
    }
    if (parar()) return false;

    // refino: a grade que mais acerta os ataques ao longo da música inteira
    std::vector<float> envR (envT.size());
    for (size_t i = 0; i < envR.size(); ++i)   // suaviza 3 quadros (tolerância de ~12 ms)
        envR[i] = (envT[i] + 0.5f * (i > 0 ? envT[i - 1] : 0) + 0.5f * (i + 1 < envT.size() ? envT[i + 1] : 0));
    double bpm = bpm0, fase = 0, notaB = -1;
    for (double b = bpm0 - 1.0; b <= bpm0 + 1.0001; b += 0.01)
    {
        double f, s;
        melhorFase (envR, ep, 60.0 / b, dur, 32, f, s);
        if (s > notaB) { notaB = s; bpm = b; fase = f; }
    }
    prog (0.55f, "acertando a grade");
    // BPM redondo quando está a menos de 0,06 do inteiro (ou do meio)
    if (std::abs (bpm - std::round (bpm)) < 0.06) bpm = std::round (bpm);
    else if (std::abs (bpm * 2 - std::round (bpm * 2)) < 0.08) bpm = std::round (bpm * 2) / 2;
    else bpm = std::round (bpm * 100) / 100;
    {
        double s;
        melhorFase (envR, ep, 60.0 / bpm, dur, 64, fase, s);
    }
    const double P = 60.0 / bpm;
    out.bpm = bpm;
    if (parar()) return false;

    // primeiro som da música (RMS de 5 ms passa de -30 dB do pico; volta até -50 dB para pegar o começo do ataque)
    double tPrimeiroSom = -1;
    {
        const int w = std::max (1, (int) (0.005 * sr));
        std::vector<float> rms (x.size() / (size_t) w);
        float mx = 0;
        for (size_t i = 0; i < rms.size(); ++i)
        {
            double s = 0;
            for (int k = 0; k < w; ++k) { const double v = x[i * (size_t) w + (size_t) k]; s += v * v; }
            rms[i] = (float) std::sqrt (s / w);
            mx = std::max (mx, rms[i]);
        }
        for (size_t i = 0; i < rms.size(); ++i)
            if (rms[i] > mx * 0.0316f)
            {
                size_t k = i;
                while (k > 0 && rms[k - 1] > mx * 0.00316f && i - k < 40) --k;
                // dentro do bloco de 5 ms: primeira amostra acima de -50 dB
                size_t a = k * (size_t) w;
                for (size_t q = a; q < a + (size_t) w && q < x.size(); ++q)
                    if (std::abs (x[q]) > mx * 0.00316f) { a = q; break; }
                tPrimeiroSom = (double) a / sr;
                break;
            }
    }

    // ---------------- 3. tempo forte (1 do compasso): onde as mudanças acontecem
    {
        const int nb = (int) ((dur - fase) / P);
        std::vector<std::array<double, 4>> fb ((size_t) std::max (nb, 0));
        for (int k = 0; k < nb; ++k)
        {
            const double t0 = fase + k * P, t1 = t0 + P;
            fb[(size_t) k] = { db (mediaJanela (eLowF, ep, t0, t1)), db (mediaJanela (eMidF, ep, t0, t1)),
                               db (mediaJanela (eHighF, ep, t0, t1)), (double) maxJanela (fluxKick, ep, t0, -0.03, 0.04) };
        }
        double nota[4] = { 0, 0, 0, 0 };
        for (int k = 4; k + 4 <= nb; ++k)
        {
            double d = 0;
            for (int c = 0; c < 3; ++c)
            {
                double a = 0, b = 0;
                for (int i = 0; i < 4; ++i) { a += fb[(size_t) (k - 4 + i)][(size_t) c]; b += fb[(size_t) (k + i)][(size_t) c]; }
                d += std::abs (b - a) / 4;
            }
            nota[k % 4] += d;
        }
        int r = (int) (std::max_element (nota, nota + 4) - nota);
        double t1 = fase + r * P;
        // Música de pista quase sempre começa NO 1 do compasso. Se o primeiro som cai na grade (ou meio tempo fora,
        // quando a grade pegou o contratempo do baixo/hat), ele manda: acerta a fase exata e o tempo forte.
        if (tPrimeiroSom >= 0)
        {
            const double e = (tPrimeiroSom - fase) / P;
            const double d0 = std::abs (e - std::round (e));
            const double dh = std::abs (e - 0.5 - std::round (e - 0.5));
            if (std::min (d0, dh) < 0.15)
                t1 = tPrimeiroSom;
        }
        while (t1 - 4 * P >= 0) t1 -= 4 * P;
        out.primeiroTempoSeg = t1;
        const double t1t = t1 / P;
        if (t1t < 0.25) { out.audioCorteSeg = t1; out.audioInicioTempos = 0; }
        else { out.audioCorteSeg = 0; out.audioInicioTempos = 4.0 - t1t; }
    }
    out.totalCompassos = std::max (1, (int) std::ceil (out.audioFimTempos() / 4.0 - 1e-6));
    const int J = out.totalCompassos;
    auto segDe = [&] (double tempos) { return out.temposParaSegAudio (tempos); };   // tempos do projeto -> s no áudio

    const auto crPad = croma (et, 100, 1500);
    const auto crLead = croma (et, 400, 4000);

    // ---------------- 5. compassos e passos
    prog (0.66f, "lendo compasso por compasso");
    std::vector<Compasso> cs ((size_t) J);
    for (int j = 0; j < J; ++j)
    {
        Compasso& c = cs[(size_t) j];
        const double ta = segDe (j * 4.0), tb = segDe (j * 4.0 + 4);
        c.temAudio = tb > 0.05 && ta < dur - 0.05;
        if (! c.temAudio) continue;
        const double a = std::max (0.0, ta), b = std::min (dur, tb);
        c.eLow = db (mediaJanela (eLowF, ep, a, b));
        c.eMid = db (mediaJanela (eMidF, ep, a, b));
        c.eHigh = db (mediaJanela (eHighF, ep, a, b));
        c.eTot = db (std::pow (10, c.eLow / 10) + std::pow (10, c.eMid / 10) + std::pow (10, c.eHigh / 10));
        c.eLead = db (mediaJanela (eLeadF, et, a, b));
        c.eHighRiser = db (mediaJanela (eRiserF, ep, a, b));
        const double passo = P / 4;
        for (int s = 0; s < PASSOS; ++s)
        {
            const double t = segDe (j * 4.0 + s * 0.25);
            c.kick[s] = maxJanela (fluxKick, ep, t, -0.03, 0.035);
            c.hat[s] = maxJanela (fluxHat, ep, t, -0.025, 0.03);
            c.clap[s] = maxJanela (fluxClap, ep, t, -0.025, 0.035);
            c.baixoAtq[s] = maxJanela (fluxBaixo, ep, t, -0.025, 0.035);
            c.low[s] = db (mediaJanela (eLowF, ep, t + 0.3 * passo, t + 0.9 * passo));
        }
        for (int bt = 0; bt < 4; ++bt)
        {
            const double t = segDe (j * 4.0 + bt);
            double forca;
            c.alturaTempo[bt] = alturaBaixo (xb, srb, t + 0.3 * P, t + P, nullptr, forca);
        }
        cromaMedia (crPad, et, a, b, c.cPad);
        cromaMedia (crPad, et, a, (a + b) / 2, c.cPad1);
        cromaMedia (crPad, et, (a + b) / 2, b, c.cPad2);
        c.tonalPad = topoMedia3 (c.cPad);
        c.estab = cosseno (c.cPad1, c.cPad2);
        double cl[12];
        cromaMedia (crLead, et, a, b, cl);
        c.tonalLead = topoMedia3 (cl);
        int muda = 0, ant = -1;
        for (int s = 0; s < PASSOS; ++s)
        {
            const double t = segDe (j * 4.0 + s * 0.25);
            double cs12[12];
            cromaMedia (crLead, et, t, t + passo, cs12);
            const int pc = (int) (std::max_element (cs12, cs12 + 12) - cs12);
            const double tot = std::accumulate (cs12, cs12 + 12, 0.0);
            c.leadPc[s] = (tot > 0 && cs12[pc] / tot > 0.22) ? pc : -1;
            if (c.leadPc[s] >= 0 && ant >= 0 && c.leadPc[s] != ant) ++muda;
            if (c.leadPc[s] >= 0) ant = c.leadPc[s];
        }
        c.variacao = muda / 16.0;
        if ((j & 7) == 0)
        {
            if (parar()) return false;
            prog (0.66f + 0.18f * (float) j / (float) J, "lendo compasso por compasso");
        }
    }

    // ---------------- 6. limiares globais e presença por compasso
    std::vector<double> vk, vh, vc, vtot, vlow, vlead;
    for (auto& c : cs)
    {
        if (! c.temAudio) continue;
        for (int s = 0; s < PASSOS; ++s) { vk.push_back (c.kick[s]); vh.push_back (c.hat[s]); vc.push_back (c.clap[s]); }
        vtot.push_back (c.eTot); vlow.push_back (c.eLow); vlead.push_back (c.eLead);
    }
    const double thrK = 0.30 * percentil (vk, 0.97), thrH = 0.30 * percentil (vh, 0.95), thrC = 0.35 * percentil (vc, 0.97);
    const double topoTot = percentil (vtot, 0.95), topoLead = percentil (vlead, 0.95);

    int kickPat[PASSOS] = {};   // usado para separar baixo de kick
    for (auto& c : cs)
    {
        if (! c.temAudio || c.eTot < topoTot - 45) continue;
        // kick: os passos fortes do compasso (bem acima do baixo rolando)
        float mx = *std::max_element (c.kick, c.kick + PASSOS);
        int nk = 0;
        for (int s = 0; s < PASSOS; ++s)
            if (c.kick[s] > thrK && c.kick[s] > 0.55f * mx) ++nk;
        c.kickOn = nk >= 2 && mx > 1.4 * thrK;
        if (c.kickOn)
            for (int s = 0; s < PASSOS; ++s)
                if (c.kick[s] > thrK && c.kick[s] > 0.55f * mx) ++kickPat[s];
        int nh = 0;
        for (int s = 0; s < PASSOS; ++s) if (c.hat[s] > thrH) ++nh;
        c.hatsOn = nh >= 4;
        // clap/caixa: 2 e 4 bem mais fortes que o resto
        const double bb = (c.clap[4] + c.clap[12]) / 2;
        double resto = 0;
        for (int s = 0; s < PASSOS; ++s) if (s != 4 && s != 12) resto += c.clap[s];
        resto /= 14;
        c.clapOn = bb > thrC && bb > 1.6 * resto;
        c.padOn = c.tonalPad > 0.42 && c.estab > 0.88 && c.eMid > topoTot - 24;
        c.leadOn = c.tonalLead > 0.40 && c.variacao >= 0.12 && c.eLead > topoLead - 15;
    }
    // baixo: energia grave nos passos SEM kick, comparada ao topo da música
    bool passoKick[PASSOS];
    {
        int nKickBars = 0;
        for (auto& c : cs) if (c.kickOn) ++nKickBars;
        for (int s = 0; s < PASSOS; ++s) passoKick[s] = nKickBars > 0 && kickPat[s] * 2 >= nKickBars;
    }
    std::vector<double> mBaixo ((size_t) J, -120);
    for (int j = 0; j < J; ++j)
    {
        auto& c = cs[(size_t) j];
        if (! c.temAudio) continue;
        double s = 0; int n = 0;
        for (int p = 0; p < PASSOS; ++p)
            if (! (c.kickOn && passoKick[p]) && ! (c.kickOn && p > 0 && passoKick[p - 1] && (p % 4) == 1)) { s += std::pow (10, c.low[p] / 10); ++n; }
        mBaixo[(size_t) j] = n ? db (s / n) : -120;
    }
    {
        std::vector<double> v;
        for (int j = 0; j < J; ++j) if (cs[(size_t) j].temAudio) v.push_back (mBaixo[(size_t) j]);
        const double topo = percentil (v, 0.95);
        for (int j = 0; j < J; ++j) cs[(size_t) j].baixoOn = cs[(size_t) j].temAudio && mBaixo[(size_t) j] > topo - 10;
    }
    if (parar()) return false;

    // ---------------- 4. tom (croma do meio + histograma do baixo)
    prog (0.85f, "achando o tom");
    double hist[12] = {};
    for (int f = 0; f < et.frames; ++f)
        for (int i = 0; i < 12; ++i) hist[i] += std::sqrt ((double) crPad[(size_t) f * 12 + (size_t) i]);
    // baixo: só os compassos em que há baixo (o rabo do kick é seno limpo e enganaria)
    double histB[12] = {};
    for (auto& c : cs)
        if (c.baixoOn)
            for (int bt = 0; bt < 4; ++bt)
                if (c.alturaTempo[bt] >= 0) histB[c.alturaTempo[bt] % 12] += 1.0;
    {
        const double sA = std::accumulate (hist, hist + 12, 0.0) + 1e-12, sB = std::accumulate (histB, histB + 12, 0.0) + 1e-12;
        for (int i = 0; i < 12; ++i) hist[i] = hist[i] / sA + 1.5 * histB[i] / sB;   // o baixo diz a tônica
    }
#ifdef MAPA_DEBUG
    for (int i = 0; i < 12; ++i) std::fprintf (stderr, "%s:%.3f(b%.3f) ", NOMES[i], hist[i], histB[i]);
    std::fputc ('\n', stderr);
#endif
    double melhorK = -2; int tonica = 0; bool menor = true;
    for (int t = 0; t < 12; ++t)
    {
        const double cM = correlacao (hist, PERFIL_MAIOR, t);
        const double cm = correlacao (hist, PERFIL_MENOR, t) * 1.03;   // eletrônica pende para menor
        if (cM > melhorK) { melhorK = cM; tonica = t; menor = false; }
        if (cm > melhorK) { melhorK = cm; tonica = t; menor = true; }
    }
    out.tonica = tonica; out.menor = menor;
    out.tomTexto = std::string (NOMES[tonica]) + (menor ? " m" : "");
    bool naEscala[12] = {};
    {
        const int maior[7] = { 0, 2, 4, 5, 7, 9, 11 }, men[7] = { 0, 2, 3, 5, 7, 8, 10 };
        for (int i = 0; i < 7; ++i) naEscala[(tonica + (menor ? men[i] : maior[i])) % 12] = true;
    }
    if (parar()) return false;

    // ---------------- 7. partes: novidade entre blocos, viradas em múltiplos de 4 compassos
    prog (0.86f, "separando as partes");
    const int j0 = out.audioInicioTempos > 0 ? 1 : 0;   // compasso do 1º tempo forte
    std::vector<std::vector<double>> fv ((size_t) J);
    {
        std::vector<double> mn (4, 0), sd (4, 0);
        int n = 0;
        for (auto& c : cs) if (c.temAudio) { mn[0] += c.eTot; mn[1] += c.eLow; mn[2] += c.eMid; mn[3] += c.eHigh; ++n; }
        for (auto& v : mn) v /= std::max (n, 1);
        for (auto& c : cs) if (c.temAudio)
        {
            const double e[4] = { c.eTot, c.eLow, c.eMid, c.eHigh };
            for (int i = 0; i < 4; ++i) sd[(size_t) i] += (e[i] - mn[(size_t) i]) * (e[i] - mn[(size_t) i]);
        }
        for (auto& v : sd) v = std::sqrt (v / std::max (n, 1)) + 1e-6;
        for (int j = 0; j < J; ++j)
        {
            const auto& c = cs[(size_t) j];
            const double e[4] = { c.eTot, c.eLow, c.eMid, c.eHigh };
            std::vector<double> f;
            for (int i = 0; i < 4; ++i) f.push_back (c.temAudio ? std::clamp ((e[i] - mn[(size_t) i]) / sd[(size_t) i], -3.0, 3.0) : -3.0);
            f.push_back (c.kickOn ? 1.6 : 0); f.push_back (c.baixoOn ? 1.2 : 0); f.push_back (c.hatsOn ? 0.9 : 0);
            f.push_back (c.clapOn ? 0.9 : 0); f.push_back (c.padOn ? 0.6 : 0); f.push_back (c.leadOn ? 0.6 : 0);
            fv[(size_t) j] = f;
        }
    }
    auto distBlocos = [&] (int b, int L)
    {
        const int a0 = std::max (0, b - L), a1 = b, b0 = b, b1 = std::min (J, b + L);
        if (a1 - a0 < 2 || b1 - b0 < 2) return 0.0;
        const size_t D = fv[0].size();
        double d = 0;
        for (size_t i = 0; i < D; ++i)
        {
            double ma = 0, mb = 0;
            for (int j = a0; j < a1; ++j) ma += fv[(size_t) j][i];
            for (int j = b0; j < b1; ++j) mb += fv[(size_t) j][i];
            ma /= (a1 - a0); mb /= (b1 - b0);
            d += (ma - mb) * (ma - mb);
        }
        return std::sqrt (d);
    };
    std::vector<std::pair<double, int>> cand;
    for (int b = j0 + 4; b <= J - 4; b += 4)
    {
        double nv = 0.6 * distBlocos (b, 8) + 0.4 * distBlocos (b, 4);
        if (((b - j0) % 8) == 0) nv *= 1.1;   // frase de 8 compassos é o mais comum
        cand.push_back ({ nv, b });
    }
    std::vector<int> cortes;
    if (! cand.empty())
    {
        std::vector<double> nvs;
        for (auto& c : cand) nvs.push_back (c.first);
        const double mx = *std::max_element (nvs.begin(), nvs.end());
        const double thr = std::max (0.28 * mx, 1.4 * mediana (nvs));
        std::sort (cand.begin(), cand.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        for (auto& c : cand)
        {
            if (c.first < thr) break;
            const int minDist = c.first > 0.6 * mx ? 4 : 8;
            bool ok = true;
            for (int k : cortes) if (std::abs (k - c.second) < minDist) ok = false;
            if (ok) cortes.push_back (c.second);
            if (cortes.size() >= 14) break;
        }
    }
    std::sort (cortes.begin(), cortes.end());
    // última parte com menos de 8 compassos é o rabo do fim (reverb/fade): junta com a anterior
    while (! cortes.empty() && J - cortes.back() < 8) cortes.pop_back();
    std::vector<int> lim { 0 };
    for (int k : cortes) lim.push_back (k);
    lim.push_back (J);

    struct InfoParte { int a, b; double e, kick; std::string nome; };
    std::vector<InfoParte> ps;
    double eMax = -200;
    for (size_t i = 0; i + 1 < lim.size(); ++i)
    {
        InfoParte p { lim[i], lim[i + 1], 0, 0, "" };
        int n = 0, nk = 0;
        for (int j = p.a; j < p.b; ++j)
            if (cs[(size_t) j].temAudio) { p.e += std::pow (10, cs[(size_t) j].eTot / 10); ++n; if (cs[(size_t) j].kickOn) ++nk; }
        p.e = n ? db (p.e / n) : -120;
        p.kick = n ? (double) nk / n : 0;
        eMax = std::max (eMax, p.e);
        ps.push_back (p);
    }
    const int np = (int) ps.size();
    // 1ª passada: DROP / sem kick / com kick fraco
    for (int i = 0; i < np; ++i)
    {
        auto& p = ps[(size_t) i];
        const double rel = p.e - eMax;
        if (p.kick >= 0.5 && rel >= -3.5) p.nome = "DROP";
        else if (p.kick < 0.5) p.nome = "BREAK";
        else p.nome = "GROOVE";
    }
    for (int i = 0; i < np; ++i)
    {
        auto& p = ps[(size_t) i];
        const bool antesDoDrop = i + 1 < np && ps[(size_t) i + 1].nome == "DROP";
        if (p.nome != "DROP" && antesDoDrop && (p.b - p.a) <= 16) p.nome = "BUILD";
    }
    if (np > 1)
    {
        if (ps[0].nome != "DROP") ps[0].nome = "INTRO";
        // o que vem antes do 1º kick (sem ser BUILD) ainda é INTRO
        for (int i = 1; i < np && ps[(size_t) i].nome == "BREAK" && ps[(size_t) i - 1].nome == "INTRO"; ++i)
            ps[(size_t) i].nome = "INTRO";
        if (ps[(size_t) np - 1].nome != "DROP") ps[(size_t) np - 1].nome = "OUTRO";
    }
    else ps[0].nome = "INTRO";
    // junta vizinhas de mesmo nome
    std::vector<InfoParte> pj;
    for (auto& p : ps)
    {
        if (! pj.empty() && pj.back().nome == p.nome) pj.back().b = p.b;
        else pj.push_back (p);
    }
    // numera repetidas (DROP, DROP 2 ...)
    {
        std::vector<std::pair<std::string, int>> cont;
        for (auto& p : pj)
        {
            if (p.nome == "INTRO" || p.nome == "OUTRO") continue;
            int n = 0;
            for (auto& c : cont) if (c.first == p.nome) n = ++c.second;
            if (n == 0) { cont.push_back ({ p.nome, 1 }); n = 1; }
            if (n > 1) p.nome += " " + std::to_string (n);
        }
    }
    std::vector<int> cortesFinais;
    for (auto& p : pj)
    {
        out.partes.push_back ({ p.nome, p.a, p.b, corParte (p.nome) });
        if (p.a > 0) cortesFinais.push_back (p.a);
    }
    if (parar()) return false;

    // ---------------- 8. faixas, clipes e notas
    prog (0.92f, "montando as faixas");
    const double ini = out.audioInicioTempos, fim = out.audioFimTempos();
    {
        Faixa ref { "REFERÊNCIA", "ÁUDIO · original", 0xcfd3dc, TipoFaixa::audio, Elemento::referencia, false, {} };
        ref.clipes.push_back ({ ini, fim, {} });
        out.faixas.push_back (ref);
    }
    // padrão de 16 passos por maioria dentro do trecho (loop limpo, sem o ruído compasso a compasso)
    auto padrao = [&] (int a, int b, auto ativo)
    {
        std::array<bool, PASSOS> pat {};
        int n = 0;
        int cont[PASSOS] = {};
        for (int j = a; j < b; ++j)
        {
            if (! cs[(size_t) j].temAudio) continue;
            ++n;
            for (int s = 0; s < PASSOS; ++s) if (ativo (cs[(size_t) j], s)) ++cont[s];
        }
        for (int s = 0; s < PASSOS; ++s) pat[(size_t) s] = n > 0 && cont[s] * 2 >= n;
        return pat;
    };
    auto descreveBateria = [] (const std::array<bool, PASSOS>& p, const char* padraoTxt)
    {
        int n = 0, tempos = 0, contra = 0;
        for (int s = 0; s < PASSOS; ++s) { if (p[(size_t) s]) { ++n; if (s % 4 == 0) ++tempos; if (s % 4 == 2) ++contra; } }
        if (n == 4 && tempos == 4) return std::string ("1 por tempo");
        if (n == 4 && contra == 4) return std::string ("contratempo");
        if (n >= 14) return std::string ("1/16");
        if (n == 8 && tempos + contra == 8) return std::string ("1/8");
        if (n == 2 && p[4] && p[12]) return std::string ("2 e 4");
        return std::string (padraoTxt);
    };

    auto faixaBateria = [&] (const char* nome, uint32_t cor, Elemento el, int nota, auto presente, auto ativo, const char* txt)
    {
        std::vector<bool> on ((size_t) J);
        for (int j = 0; j < J; ++j) on[(size_t) j] = presente (cs[(size_t) j]);
        auto tr = trechos (on, cortesFinais);
        if (tr.empty()) return;
        Faixa f { nome, "", cor, TipoFaixa::midi, el, true, {} };
        std::string desc;
        for (auto& t : tr)
        {
            Clipe cl { t.first * 4.0, t.second * 4.0, {} };
            const auto pat = padrao (t.first, t.second, ativo);
            if (desc.empty()) desc = descreveBateria (pat, txt);
            for (int j = t.first; j < t.second; ++j)
                for (int s = 0; s < PASSOS; ++s)
                    if (pat[(size_t) s]) cl.notas.push_back ({ j * 4.0 + s * 0.25, 0.125, nota, s % 4 == 0 ? 110 : 90 });
            f.clipes.push_back (cl);
        }
        f.desc = "MIDI · " + desc;
        out.faixas.push_back (f);
    };

    faixaBateria ("KICK", 0xb8ff3a, Elemento::kick, 36,
                  [] (const Compasso& c) { return c.kickOn; },
                  [&] (const Compasso& c, int s) { const float mx = *std::max_element (c.kick, c.kick + PASSOS);
                                                   return c.kick[s] > thrK && c.kick[s] > 0.55f * mx; },
                  "padrão próprio");

    // BAIXO: ritmo por maioria, altura de cada tempo do próprio compasso
    {
        std::vector<bool> on ((size_t) J);
        for (int j = 0; j < J; ++j) on[(size_t) j] = cs[(size_t) j].baixoOn;
        auto tr = trechos (on, cortesFinais);
        if (! tr.empty())
        {
            Faixa f { "BAIXO", "", 0x8fd16a, TipoFaixa::midi, Elemento::baixo, false, {} };
            int rolling = 0, total = 0;
            for (auto& t : tr)
            {
                Clipe cl { t.first * 4.0, t.second * 4.0, {} };
                auto ativo = [&] (const Compasso& c, int s)
                {
                    if (c.kickOn && passoKick[s]) return false;
                    double mx = -200;
                    for (int p = 0; p < PASSOS; ++p) if (! (c.kickOn && passoKick[p])) mx = std::max (mx, c.low[p]);
                    return c.low[s] > mx - 6;
                };
                const auto pat = padrao (t.first, t.second, ativo);
                // ataque por passo: separa notas repetidas de nota longa
                const auto atq = padrao (t.first, t.second, [&] (const Compasso& c, int s)
                                         { return c.baixoAtq[s] > 0.25 * percentil (std::vector<double> (c.baixoAtq, c.baixoAtq + PASSOS), 1.0); });
                for (int s = 0; s < PASSOS; ++s) if (pat[(size_t) s]) { ++total; if (s % 4 != 0) ++rolling; }
                for (int j = t.first; j < t.second; ++j)
                {
                    const auto& c = cs[(size_t) j];
                    int s = 0;
                    while (s < PASSOS)
                    {
                        if (! pat[(size_t) s]) { ++s; continue; }
                        int e = s + 1;
                        while (e < PASSOS && pat[(size_t) e] && ! atq[(size_t) e] && (e % 4) != 0) ++e;
                        int alt = c.alturaTempo[s / 4];
                        if (alt >= 0 && ! naEscala[alt % 12])
                        {
                            if (naEscala[(alt + 1) % 12] && ! naEscala[(alt + 11) % 12]) ++alt;
                            else if (naEscala[(alt + 11) % 12] && ! naEscala[(alt + 1) % 12]) --alt;
                        }
                        if (alt < 0) alt = 36 + (tonica + 12 - 0) % 12 - (tonica > 7 ? 12 : 0);
                        cl.notas.push_back ({ j * 4.0 + s * 0.25, (e - s) * 0.25 - 0.02, alt, 100 });
                        s = e;
                    }
                }
                f.clipes.push_back (cl);
            }
            f.desc = total > 0 && rolling * 4 >= total * 3 && total >= 10 ? "MIDI · rolling" : "MIDI · " + out.tomTexto;
            out.faixas.push_back (f);
        }
    }

    faixaBateria ("HATS", 0x22e4ff, Elemento::hats, 42,
                  [] (const Compasso& c) { return c.hatsOn; },
                  [&] (const Compasso& c, int s) { return c.hat[s] > thrH; }, "padrão próprio");
    faixaBateria ("CLAP", 0xffb03a, Elemento::clap, 39,
                  [] (const Compasso& c) { return c.clapOn; },
                  [&] (const Compasso& c, int s)
                  {
                      double resto = 0;
                      for (int p = 0; p < PASSOS; ++p) resto += c.clap[p];
                      resto /= PASSOS;
                      return c.clap[s] > thrC && c.clap[s] > 1.6 * resto;
                  }, "padrão próprio");

    // LEAD: nota dominante por passo, juntando passos iguais
    {
        std::vector<bool> on ((size_t) J);
        for (int j = 0; j < J; ++j) on[(size_t) j] = cs[(size_t) j].leadOn;
        auto tr = trechos (on, cortesFinais);
        if (! tr.empty())
        {
            Faixa f { "LEAD", "MIDI · " + out.tomTexto, 0xff3dbb, TipoFaixa::midi, Elemento::lead, false, {} };
            for (auto& t : tr)
            {
                Clipe cl { t.first * 4.0, t.second * 4.0, {} };
                for (int j = t.first; j < t.second; ++j)
                {
                    const auto& c = cs[(size_t) j];
                    int s = 0;
                    while (s < PASSOS)
                    {
                        const int pc = c.leadPc[s];
                        if (pc < 0) { ++s; continue; }
                        int e = s + 1;
                        while (e < PASSOS && c.leadPc[e] == pc) ++e;
                        int nota = 72 + pc;
                        if (! naEscala[pc]) nota += naEscala[(pc + 1) % 12] ? 1 : -1;   // puxa para a escala
                        cl.notas.push_back ({ j * 4.0 + s * 0.25, (e - s) * 0.25 - 0.02, nota, 96 });
                        s = e;
                    }
                }
                f.clipes.push_back (cl);
            }
            out.faixas.push_back (f);
        }
    }

    // PAD: um acorde da escala por compasso (tríade), compassos iguais viram nota longa
    {
        std::vector<bool> on ((size_t) J);
        for (int j = 0; j < J; ++j) on[(size_t) j] = cs[(size_t) j].padOn;
        auto tr = trechos (on, cortesFinais);
        if (! tr.empty())
        {
            Faixa f { "PAD", "MIDI · acordes", 0x9b6bff, TipoFaixa::midi, Elemento::pad, false, {} };
            int esc[7];
            {
                const int maior[7] = { 0, 2, 4, 5, 7, 9, 11 }, men[7] = { 0, 2, 3, 5, 7, 8, 10 };
                for (int i = 0; i < 7; ++i) esc[i] = (tonica + (menor ? men[i] : maior[i])) % 12;
            }
            for (auto& t : tr)
            {
                Clipe cl { t.first * 4.0, t.second * 4.0, {} };
                int grauAnt = -1; double inicioAnt = 0;
                auto fecha = [&] (int grau, double a, double b)
                {
                    if (grau < 0) return;
                    const int r = esc[grau], t3 = esc[(grau + 2) % 7], t5 = esc[(grau + 4) % 7];
                    int base = 48 + r;
                    if (base > 55) base -= 12;
                    int n3 = 48 + t3; while (n3 <= base) n3 += 12;
                    int n5 = 48 + t5; while (n5 <= n3) n5 += 12;
                    for (int n : { base, n3, n5 }) cl.notas.push_back ({ a, b - a - 0.05, n, 80 });
                };
                for (int j = t.first; j < t.second; ++j)
                {
                    const auto& c = cs[(size_t) j];
                    int grau = 0; double melhor = -1;
                    for (int g = 0; g < 7; ++g)
                    {
                        const double s = c.cPad[esc[g]] + 0.6 * c.cPad[esc[(g + 2) % 7]] + 0.5 * c.cPad[esc[(g + 4) % 7]];
                        if (s > melhor) { melhor = s; grau = g; }
                    }
                    if (grau != grauAnt) { fecha (grauAnt, inicioAnt, j * 4.0); grauAnt = grau; inicioAnt = j * 4.0; }
                }
                fecha (grauAnt, inicioAnt, t.second * 4.0);
                f.clipes.push_back (cl);
            }
            out.faixas.push_back (f);
        }
    }

    // FX: riser = agudo subindo nos 2–4 compassos antes de uma virada para parte mais forte
    {
        Faixa f { "FX", "MIDI · risers", 0xe6e6e6, TipoFaixa::midi, Elemento::fx, false, {} };
        for (size_t i = 1; i < out.partes.size(); ++i)
        {
            const int b = out.partes[i].compIni;
            if (b < 4) continue;
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (int k = 0; k < 4; ++k)
            {
                const double y = cs[(size_t) (b - 4 + k)].eHighRiser;
                sx += k; sy += y; sxx += k * k; sxy += k * y;
            }
            const double incl = (4 * sxy - sx * sy) / (4 * sxx - sx * sx);
            const double sobe = cs[(size_t) b - 1].eHighRiser - cs[(size_t) b - 4].eHighRiser;
            if (incl > 1.2 && sobe > 3.5)
            {
                const int a = incl > 2.5 ? b - 4 : b - 2;
                f.clipes.push_back ({ a * 4.0, b * 4.0, {} });
            }
        }
        if (! f.clipes.empty()) out.faixas.push_back (f);
    }

    prog (1.0f, "pronto");
    return true;
}
} // namespace mapa
