#pragma once

// STOMPBOX — motor de som, sem JUCE (medido fora do plugin em tools/teste_dsp.cpp).
// Cadeia: INPUT -> NOISE GATE -> até 10 pedais em série -> AMP -> CAB -> OUTPUT.
// O sinal entra mono (guitarra). Chorus, delay e reverb abrem para estéreo; daí em diante cada pedal
// processa os dois lados com estado próprio. Os estágios que distorcem (drives, fuzz, amp) rodam com
// oversampling 2x (4x com HQ) por meio-banda IIR polifásico, sem latência.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace stomp
{
constexpr double pi = 3.14159265358979323846;
constexpr int maxSub = 32;   // o motor processa em pedaços de até 32 amostras (parâmetros suavizados por pedaço)

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clampf (float x, float a, float b) { return std::min (b, std::max (a, x)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float expMap (float lo, float hi, float t) { return lo * std::pow (hi / lo, clampf (t, 0.0f, 1.0f)); }

// --------------------------------------------------------------------------------------------------------------
// peças básicas

struct OnePole   // passa-baixa / passa-alta de 1ª ordem (TPT)
{
    float g = 0.5f, s = 0.0f;
    void set (double fc, double fs) { const double w = std::tan (pi * std::min (fc, fs * 0.49) / fs); g = (float) (w / (1.0 + w)); }
    void reset() { s = 0.0f; }
    float lp (float x) { const float v = (x - s) * g; const float y = v + s; s = y + v; return y; }
    float hp (float x) { return x - lp (x); }
};

struct Biquad   // RBJ, forma transposta direta II
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0.0f; }
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void norm (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0); }
    static double w0 (double f, double fs) { return 2.0 * pi * std::min (f, fs * 0.45) / fs; }
    void lowPass (double f, double q, double fs)
    { const double w = w0 (f, fs), c = std::cos (w), al = std::sin (w) / (2 * q); norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highPass (double f, double q, double fs)
    { const double w = w0 (f, fs), c = std::cos (w), al = std::sin (w) / (2 * q); norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void peak (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (f, fs), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double f, double db, double fs, double S = 0.8)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (f, fs), c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
              (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double f, double db, double fs, double S = 0.8)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (f, fs), c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
              (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

struct DcBlock { float x1 = 0, y1 = 0, r = 0.995f; void set (double fs) { r = (float) std::exp (-2 * pi * 12.0 / fs); }
                 void reset() { x1 = y1 = 0; } float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; } };

struct Ramp   // suavização de 1ª ordem por pedaço
{
    float v = 0, alvo = 0; bool primeiro = true;
    void set (float t) { alvo = t; if (primeiro) { v = t; primeiro = false; } }
    float next (float k = 0.25f) { v += (alvo - v) * k; return v; }
};

struct Rng { uint32_t s = 0x12345678u; float next() { s = s * 1664525u + 1013904223u; return (float) ((s >> 8) * (1.0 / 8388608.0)) - 1.0f; } };

// linha de atraso com leitura fracionária (Hermite)
struct DelayLine
{
    std::vector<float> b; int w = 0, mask = 0;
    void alloc (int minLen) { int n = 1; while (n < minLen + 8) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void reset() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
    float at (int d) const { return b[(size_t) ((w - 1 - d) & mask)]; }   // d = 0: a última escrita
    float read (float d) const   // d em amostras (>= 1)
    {
        const int i = (int) d; const float f = d - (float) i;
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

// --------------------------------------------------------------------------------------------------------------
// oversampling: meio-banda IIR polifásico (dois caminhos de allpass de 1ª ordem em z^2), sem latência

struct HalfbandCoefs
{
    static constexpr int n = 8;
    double c[n];
    HalfbandCoefs()
    {
        const double transicao = 0.04;   // banda de transição relativa à taxa alta
        double k = std::tan ((1.0 - transicao * 2.0) * pi / 4.0); k *= k;
        const double kk = std::pow (1.0 - k * k, 0.25), e = 0.5 * (1.0 - kk) / (1.0 + kk), e2 = e * e, e4 = e2 * e2;
        const double q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
        const int ordem = n * 2 + 1;
        for (int i = 0; i < n; ++i)
        {
            const int cc = i + 1;
            double num = 0.0, cur; int j = 1, m = 0;
            do { cur = std::pow (q, m * (m + 1)) * std::sin ((m * 2 + 1) * cc * pi / ordem) * j; num += cur; j = -j; ++m; } while (std::abs (cur) > 1e-30 && m < 100);
            double den = 0.0; j = -1; m = 1;
            do { cur = std::pow (q, m * m) * std::cos (m * 2 * cc * pi / ordem) * j; den += cur; j = -j; ++m; } while (std::abs (cur) > 1e-30 && m < 100);
            const double ww = num * std::pow (q, 0.25) / (den + 0.5), wwsq = ww * ww;
            const double x = std::sqrt ((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
            c[i] = (1.0 - x) / (1.0 + x);
        }
    }
    static const HalfbandCoefs& get() { static const HalfbandCoefs h; return h; }
};

struct AllpassChain   // caminho com os coeficientes pares ou ímpares
{
    float c[4] {}, x1[4] {}, y1[4] {};
    void init (int par) { const auto& h = HalfbandCoefs::get(); for (int i = 0; i < 4; ++i) c[i] = (float) h.c[i * 2 + par]; reset(); }
    void reset() { for (int i = 0; i < 4; ++i) x1[i] = y1[i] = 0.0f; }
    float process (float x)
    {
        for (int i = 0; i < 4; ++i) { const float y = c[i] * (x - y1[i]) + x1[i]; x1[i] = x; y1[i] = y; x = y; }
        return x;
    }
};

struct Halfband2x
{
    AllpassChain upA, upB, dnA, dnB; float impar = 0.0f;
    Halfband2x() { upA.init (0); upB.init (1); dnA.init (0); dnB.init (1); }
    void reset() { upA.reset(); upB.reset(); dnA.reset(); dnB.reset(); impar = 0.0f; }
    void up (float x, float& y0, float& y1) { y0 = upA.process (x); y1 = upB.process (x); }
    float down (float x0, float x1) { const float y = 0.5f * (dnA.process (x0) + dnB.process (impar)); impar = x1; return y; }
};

struct Oversampler   // fator 1, 2 ou 4 para um canal
{
    Halfband2x a, b; int fator = 2;
    float buf[maxSub * 4] {}, mid[maxSub * 2] {};
    void reset() { a.reset(); b.reset(); }
    int up (const float* x, int n)
    {
        if (fator == 1) { std::memcpy (buf, x, sizeof (float) * (size_t) n); return n; }
        if (fator == 2) { for (int i = 0; i < n; ++i) a.up (x[i], buf[2 * i], buf[2 * i + 1]); return n * 2; }
        for (int i = 0; i < n; ++i) a.up (x[i], mid[2 * i], mid[2 * i + 1]);
        for (int i = 0; i < n * 2; ++i) b.up (mid[i], buf[2 * i], buf[2 * i + 1]);
        return n * 4;
    }
    void down (float* y, int n)
    {
        if (fator == 1) { std::memcpy (y, buf, sizeof (float) * (size_t) n); return; }
        if (fator == 2) { for (int i = 0; i < n; ++i) y[i] = a.down (buf[2 * i], buf[2 * i + 1]); return; }
        for (int i = 0; i < n * 2; ++i) mid[i] = b.down (buf[2 * i], buf[2 * i + 1]);
        for (int i = 0; i < n; ++i) y[i] = a.down (mid[2 * i], mid[2 * i + 1]);
    }
};

// curvas de saturação
inline float tanhA (float x) { if (x > 3.0f) return 1.0f; if (x < -3.0f) return -1.0f; const float x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2); }
inline float hardish (float x, float p) { return x / std::pow (1.0f + std::pow (std::abs (x), p), 1.0f / p); }   // joelho de dureza p
inline float trilho (float x) { return 2.0f * tanhA (x * 0.5f); }   // o op-amp do pedal não passa da alimentação (9 V)

// --------------------------------------------------------------------------------------------------------------
// pedais

enum Tipo { Vazio = 0, Squash, Hump, Clearcut, Grit, Woolly, Quack, Swirl, Tide, Echo, Halo, numTipos };

struct Ctl          // o que o processador manda para cada pedal a cada bloco
{
    float k[4] {};  // knobs 0..1
    int sw = 0;     // chave
    bool on = false;
    double bpm = 120.0; bool sync = false;
};

// divisões de tempo do SYNC (em batidas)
inline int idxDiv (float k, int n) { return std::min (n - 1, (int) (clampf (k, 0, 1) * ((float) n - 0.001f))); }
inline double divisaoDelay (float k)   // TIME com SYNC: 1/32 a 1/2 pontuada
{
    static const double d[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 3.0 };
    return d[idxDiv (k, 13)];
}
inline const char* nomeDivisaoDelay (float k)
{
    static const char* n[] = { "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/2." };
    return n[idxDiv (k, 13)];
}
inline double divisaoLfo (float k)   // RATE/SPEED com SYNC: 2 compassos a 1/16 (período em batidas)
{
    static const double d[] = { 8.0, 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25 };
    return d[idxDiv (k, 7)];
}
inline const char* nomeDivisaoLfo (float k)
{
    static const char* n[] = { "2 BAR", "1 BAR", "1/2", "1/4", "1/8", "1/8T", "1/16" };
    return n[idxDiv (k, 7)];
}

struct Pedal
{
    virtual ~Pedal() = default;
    virtual void prepare (double fs, int os) = 0;   // os = fator de oversampling dos drives
    virtual void reset() = 0;
    // processa no lugar; st = o sinal já é estéreo (se não, só L vale)
    virtual void process (float* L, float* R, int n, bool& st, const Ctl& c) = 0;
    virtual bool tornaEstereo() const { return false; }
};

// pedal mono: um "Core" por lado
template <class Core>
struct Dual : Pedal
{
    Core a, b;
    void prepare (double fs, int os) override { a.prepare (fs, os); b.prepare (fs, os); }
    void reset() override { a.reset(); b.reset(); }
    void process (float* L, float* R, int n, bool& st, const Ctl& c) override
    {
        a.set (c); a.process (L, n);
        if (st) { b.set (c); b.process (R, n); }
    }
};

// ---- SQUASH: compressor de pedal (OTA). LEVEL, SUSTAIN, ATTACK, BLEND
struct SquashCore
{
    double fs = 48000; float env = 0, gr = 1; Ramp lvl, thr, mix; float atk = 0.01f, rel = 0.0f, grK = 0.02f; OnePole cor;
    void prepare (double s, int) { fs = s; cor.set (7500, fs); rel = (float) std::exp (-1.0 / (0.18 * fs)); grK = (float) (1.0 - std::exp (-1.0 / (0.0015 * fs))); reset(); }
    void reset() { env = 0; gr = 1; cor.reset(); }
    void set (const Ctl& c)
    {
        lvl.set (dbToGain (lerp (-12.0f, 18.0f, c.k[0])));
        thr.set (lerp (-8.0f, -46.0f, c.k[1]));
        atk = (float) std::exp (-1.0 / (expMap (0.0008f, 0.05f, c.k[2]) * fs));
        mix.set (c.k[3]);
    }
    void process (float* x, int n)
    {
        const float g = lvl.next(), t = thr.next(), m = mix.next();
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            env = a > env ? a + (env - a) * atk : a + (env - a) * rel;
            const float over = gainToDb (env) - t;
            constexpr float sl = 1.0f - 1.0f / 10.0f;   // ratio 10:1, joelho de 12 dB
            float red = 0.0f;
            if (over > 6.0f) red = sl * over;
            else if (over > -6.0f) { const float o = over + 6.0f; red = sl * o * o / 24.0f; }
            gr += (dbToGain (-red) - gr) * grK;
            const float wet = cor.lp (x[i] * gr) * g;
            x[i] = x[i] * (1.0f - m) + wet * m;
        }
    }
};
using SquashPedal = Dual<SquashCore>;

// ---- HUMP: overdrive de pedal verde (op-amp com diodos na realimentação, corcova de médios). DRIVE, LEVEL, TONE
struct HumpCore
{
    double fs = 48000; Oversampler os; OnePole hpIn, lpClip, toneLp; Biquad corcova; DcBlock dc;
    Ramp drive, lvl, tone;
    void prepare (double s, int f) { fs = s; os.fator = f; hpIn.set (720, fs * f); toneLp.set (720, fs); corcova.peak (750, 0.7, 3.0, fs); dc.set (fs); reset(); }
    void reset() { os.reset(); hpIn.reset(); lpClip.reset(); toneLp.reset(); corcova.reset(); dc.reset(); }
    void set (const Ctl& c) { drive.set (c.k[0]); lvl.set (dbToGain (lerp (-24.0f, 10.0f, c.k[1]))); tone.set (c.k[2]); }
    void process (float* x, int n)
    {
        const float d = drive.next(), g = lvl.next(), t = tone.next();
        const float ganho = 1.0f + (51.0f + 500.0f * d * d) / 4.7f;          // 21 a 41 dB nos agudos
        lpClip.set (1.0 / (2 * pi * (51e3 + 500e3 * d * d) * 51e-12), fs * os.fator);
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            const float v = os.buf[i];
            os.buf[i] = v + 0.45f * tanhA (lpClip.lp (hpIn.hp (v)) * ganho / 0.45f);
        }
        os.down (x, n);
        for (int i = 0; i < n; ++i)
        {
            const float lo = toneLp.lp (x[i]), hi = x[i] - lo;
            const float y = lo + hi * lerp (0.08f, 1.15f, t);                // TONE: o quanto do agudo passa
            x[i] = trilho (dc.process (corcova.process (y)) * g * 0.8f);
        }
    }
};
using HumpPedal = Dual<HumpCore>;

// ---- CLEARCUT: overdrive transparente (limpo + clipado juntos). GAIN, TREBLE, VOLUME, BASS; chave HEADROOM LOW/HIGH
struct ClearCore
{
    double fs = 48000; Oversampler os; OnePole hpIn; Biquad treble, bass, bump; DcBlock dc;
    Ramp gain, vol; float clipLvl = 0.3f; float tr = -1, ba = -1;
    void prepare (double s, int f) { fs = s; os.fator = f; hpIn.set (80, fs * f); bump.peak (1000, 0.6, 2.0, fs * f); dc.set (fs); tr = ba = -1; reset(); }
    void reset() { os.reset(); hpIn.reset(); treble.reset(); bass.reset(); bump.reset(); dc.reset(); }
    void set (const Ctl& c)
    {
        gain.set (c.k[0]); vol.set (dbToGain (lerp (-20.0f, 12.0f, c.k[2])));
        clipLvl = c.sw == 0 ? 0.28f : 0.55f;
        if (std::abs (c.k[1] - tr) > 0.002f) { tr = c.k[1]; treble.highShelf (2200, lerp (-9.0f, 8.0f, tr), fs); }
        if (std::abs (c.k[3] - ba) > 0.002f) { ba = c.k[3]; bass.lowShelf (180, lerp (-8.0f, 8.0f, ba), fs); }
    }
    void process (float* x, int n)
    {
        const float gk = gain.next(), v = vol.next();
        const float ganho = dbToGain (lerp (0.0f, 38.0f, gk));
        const float limpo = lerp (0.95f, 0.15f, gk), sujo = lerp (0.35f, 1.0f, gk);
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            const float s = os.buf[i];
            const float c = clipLvl * hardish (bump.process (hpIn.hp (s)) * ganho / clipLvl, 3.0f);
            os.buf[i] = s * limpo + c * sujo;
        }
        os.down (x, n);
        for (int i = 0; i < n; ++i) x[i] = trilho (bass.process (treble.process (dc.process (x[i]))) * v * 0.9f);
    }
};
using ClearPedal = Dual<ClearCore>;

// ---- GRIT: distorção de op-amp com diodos para a terra. DIST, FILTER, VOLUME; chave CLASSIC/MODERN
struct GritCore
{
    double fs = 48000; Oversampler os; OnePole hp1, hp2, slew, filtro; DcBlock dc; Ramp dist, vol; float fAtual = -1; int modo = 0;
    void prepare (double s, int f) { fs = s; os.fator = f; dc.set (fs); fAtual = -1; reset(); }
    void reset() { os.reset(); hp1.reset(); hp2.reset(); slew.reset(); filtro.reset(); dc.reset(); }
    void set (const Ctl& c)
    {
        dist.set (c.k[0]); vol.set (dbToGain (lerp (-30.0f, 8.0f, c.k[2]))); modo = c.sw;
        if (std::abs (c.k[1] - fAtual) > 0.002f) { fAtual = c.k[1]; filtro.set (expMap (16000.0f, 480.0f, fAtual), fs); }
        hp1.set (modo == 0 ? 60.0 : 160.0, fs * os.fator);
    }
    void process (float* x, int n)
    {
        const float d = dist.next(), v = vol.next();
        const float ganho = 1.0f + 2000.0f * d * d * d + 20.0f * d;          // até ~66 dB
        hp2.set (lerp (40.0f, 900.0f, d), fs * os.fator);                       // o ganho alto só vale nos agudos
        slew.set (std::max (2500.0, 1.0e6 * 1.2 / ganho), fs * os.fator);       // op-amp lento: abafa com o ganho
        const float th = modo == 0 ? 0.5f : 0.9f, p = modo == 0 ? 2.8f : 2.0f;
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            const float s = hp1.hp (os.buf[i]);
            const float a = s * (1.0f + 20.0f * d) + hp2.hp (s) * (ganho - 1.0f - 20.0f * d);
            os.buf[i] = th * hardish (slew.lp (a) / th, p);
        }
        os.down (x, n);
        for (int i = 0; i < n; ++i) x[i] = trilho (dc.process (filtro.lp (x[i])) * v);
    }
};
using GritPedal = Dual<GritCore>;

// ---- WOOLLY: fuzz de dois transistores (redondo). VOLUME, FUZZ; chave GE/SI
struct WoollyCore
{
    double fs = 48000; Oversampler os; OnePole hpIn, lpOut, lpPre; DcBlock dc, dc2; Ramp fuzz, vol; int modo = 0; float bias = 0;
    void prepare (double s, int f) { fs = s; os.fator = f; hpIn.set (90, fs * f); dc.set (fs * f); dc2.set (fs); reset(); }
    void reset() { os.reset(); hpIn.reset(); lpOut.reset(); lpPre.reset(); dc.reset(); dc2.reset(); bias = 0; }
    void set (const Ctl& c)
    {
        vol.set (dbToGain (lerp (-36.0f, 6.0f, c.k[0]))); fuzz.set (c.k[1]); modo = c.sw;
        lpOut.set (modo == 0 ? 4200.0 : 6500.0, fs);
        lpPre.set (modo == 0 ? 9000.0 : 14000.0, fs * os.fator);
    }
    void process (float* x, int n)
    {
        const float f = fuzz.next(), v = vol.next();
        const float ganho = dbToGain (lerp (6.0f, modo == 0 ? 40.0f : 50.0f, f));
        const float pos = modo == 0 ? 0.55f : 0.42f, neg = modo == 0 ? 0.9f : 0.5f;
        const float bk = (float) (1.0 - std::exp (-1.0 / (0.03 * fs * os.fator)));
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            float s = lpPre.lp (hpIn.hp (os.buf[i])) * ganho;
            // polarização que "afunda" com sinal forte (o engasgo do fuzz) e assimetria dos dois lados
            bias += (std::min (std::abs (s), 4.0f) * 0.08f - bias) * bk;
            s -= bias;
            const float y = s > 0 ? pos * tanhA (s / pos) : neg * hardish (s / neg, modo == 0 ? 2.0f : 4.0f);
            os.buf[i] = dc.process (y);
        }
        os.down (x, n);
        for (int i = 0; i < n; ++i) x[i] = trilho (dc2.process (lpOut.lp (x[i])) * v * 0.9f);
    }
};
using WoollyPedal = Dual<WoollyCore>;

// ---- QUACK: wah (passa-banda ressonante que varre). PEDAL (no AUTO, a sensibilidade), Q, RANGE; chave PEDAL/AUTO
struct QuackCore
{
    double fs = 48000; float ic1 = 0, ic2 = 0; Ramp pos, q, range; float env = 0; int modo = 0; float atk = 0, rel = 0, posAtual = -1, posK = 0.01f;
    void prepare (double s, int) { fs = s; atk = (float) std::exp (-1.0 / (0.004 * fs)); rel = (float) std::exp (-1.0 / (0.12 * fs)); posK = (float) (1.0 - std::exp (-1.0 / (0.012 * fs))); reset(); }
    void reset() { ic1 = ic2 = 0; env = 0; posAtual = -1; }
    void set (const Ctl& c) { pos.set (c.k[0]); q.set (c.k[1]); range.set (c.k[2]); modo = c.sw; }
    void process (float* x, int n)
    {
        const float p = pos.next(), qq = q.next(), r = range.next();
        const float lo = lerp (280.0f, 520.0f, r), hi = lerp (1500.0f, 2900.0f, r);
        const float Q = lerp (2.0f, 9.0f, qq), k = 1.0f / Q, ganhoPico = 1.4f * std::sqrt (Q) * k;
        if (posAtual < 0) posAtual = p;
        for (int i = 0; i < n; ++i)
        {
            float u = p;
            if (modo == 1)
            {
                const float a = std::abs (x[i]);
                env = a > env ? a + (env - a) * atk : a + (env - a) * rel;
                u = clampf (env * lerp (2.0f, 40.0f, p), 0.0f, 1.0f);
            }
            posAtual += (u - posAtual) * posK;
            const float fc = lo * std::pow (hi / lo, posAtual);
            const float g = (float) std::tan (pi * fc / fs);
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x[i] - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            x[i] = v1 * ganhoPico + v2 * 0.18f;   // pico + um pouco de grave
        }
    }
};
using QuackPedal = Dual<QuackCore>;

// ---- SWIRL: phaser de 4 estágios (JFET). SPEED, FEEDBACK; chave VINTAGE/MODERN
struct SwirlCore
{
    double fs = 48000; float s[4] {}; float fb = 0, ph = 0; Ramp spd, fbk; int modo = 0; double bpm = 120; bool sync = false; float kSpd = 0;
    void prepare (double f, int) { fs = f; reset(); }
    void reset() { for (auto& v : s) v = 0; fb = 0; }
    void set (const Ctl& c) { spd.set (c.k[0]); fbk.set (c.k[1]); modo = c.sw; bpm = c.bpm; sync = c.sync; kSpd = c.k[0]; }
    void process (float* x, int n)
    {
        const float sp = spd.next(), f = fbk.next();
        const double hz = sync ? bpm / 60.0 / divisaoLfo (kSpd) : expMap (0.08f, 9.0f, sp);
        const float inc = (float) (hz / fs), realim = modo == 0 ? f * 0.35f : f * 0.75f;
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1.0f) ph -= 1.0f;
            const float tri = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;
            const float fc = 180.0f * std::pow (2400.0f / 180.0f, tri * tri * (3 - 2 * tri));
            const float w = (float) std::tan (pi * fc / fs), a = (w - 1.0f) / (w + 1.0f);
            float v = x[i] + fb * realim;
            for (int k = 0; k < 4; ++k) { const float y = a * v + s[k]; s[k] = v - a * y; v = y; }
            fb = v;
            x[i] = (x[i] + v) * 0.62f;
        }
    }
};
using SwirlPedal = Dual<SwirlCore>;

// ---- TIDE: chorus/vibrato de BBD (abre para estéreo). RATE, DEPTH, MIX; chave CHORUS/VIBRATO
struct TidePedal : Pedal
{
    double fs = 48000; DelayLine dl[2]; OnePole lp[2]; float ph = 0; Ramp rate, depth, mix;
    bool tornaEstereo() const override { return true; }
    void prepare (double f, int) override { fs = f; for (auto& d : dl) d.alloc ((int) (0.04 * fs)); for (auto& l : lp) l.set (6500, fs); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); for (auto& l : lp) l.reset(); }
    void process (float* L, float* R, int n, bool& st, const Ctl& c) override
    {
        rate.set (c.k[0]); depth.set (c.k[1]); mix.set (c.k[2]);
        const int modo = c.sw;
        const float r = rate.next(), d = depth.next(), m = mix.next();
        const double hz = c.sync ? c.bpm / 60.0 / divisaoLfo (c.k[0]) : expMap (0.1f, 8.0f, r);
        const float inc = (float) (hz / fs);
        const float centro = (float) (0.0075 * fs), prof = (float) ((modo == 0 ? 0.0045 : 0.0028) * fs) * d;
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = st ? R[i] : L[i];
            ph += inc; if (ph >= 1.0f) ph -= 1.0f;
            const float tri = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f;
            dl[0].push (inL); dl[1].push (inR);
            const float wl = lp[0].lp (dl[0].read (centro + prof * tri));
            const float wr = lp[1].lp (dl[1].read (centro - prof * tri));
            if (modo == 0) { L[i] = inL * (1.0f - 0.5f * m) + wl * m * 0.85f; R[i] = inR * (1.0f - 0.5f * m) + wr * m * 0.85f; }
            else { L[i] = inL * (1.0f - m) + wl * m; R[i] = inR * (1.0f - m) + wr * m; }
        }
        st = true;
    }
};

// ---- ECHO: delay BBD escuro ou fita (abre para estéreo). TIME, FEEDBACK, MIX, MOD; chave BBD/TAPE; SYNC global
struct EchoPedal : Pedal
{
    double fs = 48000; DelayLine dl[2]; OnePole lp[2], hp[2]; Biquad lp2[2]; float tempo = -1, tK = 0.0003f; Ramp fb, mix, mod;
    float ph = 0, ph2 = 0; Rng rng; float ruido = 0;
    bool tornaEstereo() const override { return true; }
    void prepare (double f, int) override { fs = f; for (auto& d : dl) d.alloc ((int) (1.6 * fs)); tK = (float) (1.0 - std::exp (-1.0 / (0.08 * fs))); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); for (int i = 0; i < 2; ++i) { lp[i].reset(); hp[i].reset(); lp2[i].reset(); } tempo = -1; }
    static float tempoSeg (const Ctl& c) { return c.sync ? (float) (divisaoDelay (c.k[0]) * 60.0 / std::max (30.0, c.bpm)) : expMap (0.04f, 1.2f, c.k[0]); }
    void process (float* L, float* R, int n, bool& st, const Ctl& c) override
    {
        fb.set (c.k[1] * 1.08f); mix.set (c.k[2]); mod.set (c.k[3]);
        const int modo = c.sw;
        const float alvoT = std::min (1.5f, tempoSeg (c)) * (float) fs;
        if (tempo < 0) tempo = alvoT;
        const float f = fb.next(), m = mix.next(), md = mod.next();
        const float corte = modo == 0 ? expMap (3400.0f, 1700.0f, std::min (1.0f, alvoT / (float) fs)) : 4800.0f;
        for (int k = 0; k < 2; ++k) { lp[k].set (corte, fs); hp[k].set (modo == 0 ? 90.0 : 140.0, fs); lp2[k].lowPass (corte * 1.6, 0.6, fs); }
        const float incA = (float) ((modo == 0 ? 0.6 : 0.45) / fs), incB = (float) (6.3 / fs);
        const float sat = modo == 0 ? 0.9f : 0.7f;
        for (int i = 0; i < n; ++i)
        {
            tempo += (alvoT - tempo) * tK;   // girar o TIME estica o eco, como na fita
            ph += incA; if (ph >= 1) ph -= 1; ph2 += incB; if (ph2 >= 1) ph2 -= 1;
            ruido += (rng.next() - ruido) * 0.002f;
            const float desvio = modo == 0 ? md * 0.0035f * (float) std::sin (2 * pi * ph)
                                           : md * (0.004f * (float) std::sin (2 * pi * ph) + 0.0009f * (float) std::sin (2 * pi * ph2) + 0.02f * ruido);
            const float dL = std::max (4.0f, tempo * (1.0f + desvio)), dR = std::max (4.0f, tempo * (1.0f + desvio) * 1.013f);
            const float inL = L[i], inR = st ? R[i] : L[i];
            float wl = dl[0].read (dL), wr = dl[1].read (dR);
            wl = lp2[0].process (lp[0].lp (hp[0].hp (wl))); wr = lp2[1].process (lp[1].lp (hp[1].hp (wr)));
            dl[0].push (sat * tanhA ((inL + wl * f) / sat));
            dl[1].push (sat * tanhA ((inR + wr * f) / sat));
            L[i] = inL + wl * m * 1.1f; R[i] = inR + wr * m * 1.1f;
        }
        st = true;
    }
};

// ---- HALO: reverb de mola, plate ou hall (abre para estéreo). DECAY, TONE, MIX, PRE-DLY; chave SPRING/PLATE/HALL
struct HaloPedal : Pedal
{
    double fs = 48000;
    DelayLine pre, apIn[4], tankAp1[2], tankD1[2], tankAp2[2], tankD2[2];   // plate / hall: tanque de Dattorro
    OnePole bw, damp[2]; float tankFb[2] {}; float lfo = 0;
    DelayLine molaD[2]; float molaAp[2][12] {}, molaApX[2][12] {}; OnePole molaLp[2], molaHp[2]; float molaFb[2] {};
    Biquad toneOut[2]; Ramp dec, mix; int modo = -1; float toneAtual = -1;
    bool tornaEstereo() const override { return true; }
    int L (double s) const { return std::max (2, (int) (s * fs / 29761.0)); }
    void prepare (double f, int) override
    {
        fs = f;
        pre.alloc ((int) (0.25 * fs));
        const int apl[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i) apIn[i].alloc (L (apl[i] * 2));
        for (int k = 0; k < 2; ++k) { tankAp1[k].alloc (L (908 * 2 + 64)); tankD1[k].alloc (L (4453 * 2)); tankAp2[k].alloc (L (2656 * 2)); tankD2[k].alloc (L (3720 * 2)); molaD[k].alloc ((int) (0.08 * fs)); }
        bw.set (9000, fs); toneAtual = -1; modo = -1;
        reset();
    }
    void reset() override
    {
        pre.reset(); for (auto& a : apIn) a.reset();
        for (int k = 0; k < 2; ++k)
        {
            tankAp1[k].reset(); tankD1[k].reset(); tankAp2[k].reset(); tankD2[k].reset(); molaD[k].reset();
            damp[k].reset(); molaLp[k].reset(); molaHp[k].reset(); toneOut[k].reset(); tankFb[k] = molaFb[k] = 0;
            for (int j = 0; j < 12; ++j) molaAp[k][j] = molaApX[k][j] = 0;
        }
        bw.reset();
    }
    static float allpass (DelayLine& d, float x, int len, float g)
    {
        const float v = d.at (len - 1);
        const float u = x + g * v;
        d.push (u);
        return v - g * u;
    }
    static float allpassMod (DelayLine& d, float x, float len, float g)
    {
        const float v = d.read (len - 1);
        const float u = x + g * v;
        d.push (u);
        return v - g * u;
    }
    void process (float* Lb, float* Rb, int n, bool& st, const Ctl& c) override
    {
        dec.set (c.k[0]); mix.set (c.k[2]);
        const float preMs = c.k[3] * 200.0f;
        if (c.sw != modo) { modo = c.sw; reset(); toneAtual = -1; }
        if (std::abs (c.k[1] - toneAtual) > 0.002f)
        {
            toneAtual = c.k[1];
            for (int k = 0; k < 2; ++k) toneOut[k].highShelf (2500, lerp (-12.0f, 4.0f, toneAtual), fs);
            const float dc = expMap (2500.0f, 12000.0f, toneAtual);
            for (int k = 0; k < 2; ++k) { damp[k].set (modo == 2 ? dc * 0.7f : dc, fs); molaLp[k].set (lerp (2500.0f, 5500.0f, toneAtual), fs); molaHp[k].set (220, fs); }
        }
        const float d = dec.next(), m = mix.next();
        const float preD = std::max (1.0f, preMs * 0.001f * (float) fs);
        if (modo == 0) processaMola (Lb, Rb, n, st, d, m, preD);
        else processaTanque (Lb, Rb, n, st, d, m, preD, modo == 2);
        st = true;
    }
    void processaTanque (float* Lb, float* Rb, int n, bool st, float d, float m, float preD, bool hall)
    {
        const float s = hall ? 1.0f : 0.62f;   // hall usa o tanque inteiro; plate, mais curto e denso
        const float decay = hall ? lerp (0.5f, 0.925f, d) : lerp (0.35f, 0.92f, d);
        const float lfoInc = (float) (1.0 / fs);
        for (int i = 0; i < n; ++i)
        {
            const float inL = Lb[i], inR = st ? Rb[i] : Lb[i];
            pre.push ((inL + inR) * 0.5f);
            float x = bw.lp (pre.read (preD));
            x = allpass (apIn[0], x, L (142 * s), 0.75f);
            x = allpass (apIn[1], x, L (107 * s), 0.75f);
            x = allpass (apIn[2], x, L (379 * s), 0.625f);
            x = allpass (apIn[3], x, L (277 * s), 0.625f);
            lfo += lfoInc; if (lfo >= 1) lfo -= 1;
            const float ex = (float) (std::sin (2 * pi * lfo) * 8.0 * fs / 29761.0);
            float a = x + tankFb[1] * decay, b = x + tankFb[0] * decay;
            a = allpassMod (tankAp1[0], a, (float) L (672 * s) + ex, -0.7f);
            b = allpassMod (tankAp1[1], b, (float) L (908 * s) - ex, -0.7f);
            tankD1[0].push (a); tankD1[1].push (b);
            float a2 = damp[0].lp (tankD1[0].at (L (4453 * s))) * decay, b2 = damp[1].lp (tankD1[1].at (L (4217 * s))) * decay;
            a2 = allpass (tankAp2[0], a2, L (1800 * s), 0.5f);
            b2 = allpass (tankAp2[1], b2, L (2656 * s), 0.5f);
            tankD2[0].push (a2); tankD2[1].push (b2);
            tankFb[0] = tankD2[0].at (L (3720 * s)); tankFb[1] = tankD2[1].at (L (3163 * s));
            const float yl = 0.6f * (tankD1[1].at (L (266 * s)) + tankD1[1].at (L (2974 * s)) - tankAp2[1].at (L (1913 * s)) + tankD2[1].at (L (1996 * s))
                                     - tankD1[0].at (L (1990 * s)) - tankAp2[0].at (L (187 * s)) - tankD2[0].at (L (1066 * s)));
            const float yr = 0.6f * (tankD1[0].at (L (353 * s)) + tankD1[0].at (L (3627 * s)) - tankAp2[0].at (L (1228 * s)) + tankD2[0].at (L (2673 * s))
                                     - tankD1[1].at (L (2111 * s)) - tankAp2[1].at (L (335 * s)) - tankD2[1].at (L (121 * s)));
            const float wl = toneOut[0].process (yl), wr = toneOut[1].process (yr);
            Lb[i] = inL * (1.0f - 0.35f * m) + wl * m;
            Rb[i] = inR * (1.0f - 0.35f * m) + wr * m;
        }
    }
    void processaMola (float* Lb, float* Rb, int n, bool st, float d, float m, float preD)
    {
        const float decay = lerp (0.45f, 0.9f, d);
        const int len[2] = { (int) (0.0331 * fs), (int) (0.0397 * fs) };
        const float ap = 0.62f;   // dispersão: o "piu" da mola
        for (int i = 0; i < n; ++i)
        {
            const float inL = Lb[i], inR = st ? Rb[i] : Lb[i];
            pre.push ((inL + inR) * 0.5f);
            const float x = pre.read (preD);
            float out[2];
            for (int k = 0; k < 2; ++k)
            {
                float v = x * 0.6f + molaFb[k] * decay;
                for (int j = 0; j < 12; ++j)
                {
                    const float y = -ap * v + molaApX[k][j] + ap * molaAp[k][j];
                    molaApX[k][j] = v; molaAp[k][j] = y; v = y;
                }
                v = molaHp[k].hp (molaLp[k].lp (v));
                molaD[k].push (tanhA (v));
                molaFb[k] = molaD[k].at (len[k]);
                out[k] = toneOut[k].process (molaFb[k]);
            }
            Lb[i] = inL * (1.0f - 0.3f * m) + out[0] * m * 1.3f;
            Rb[i] = inR * (1.0f - 0.3f * m) + out[1] * m * 1.3f;
        }
    }
};

inline std::unique_ptr<Pedal> criaPedal (int t)
{
    switch (t)
    {
        case Squash:   return std::make_unique<SquashPedal>();
        case Hump:     return std::make_unique<HumpPedal>();
        case Clearcut: return std::make_unique<ClearPedal>();
        case Grit:     return std::make_unique<GritPedal>();
        case Woolly:   return std::make_unique<WoollyPedal>();
        case Quack:    return std::make_unique<QuackPedal>();
        case Swirl:    return std::make_unique<SwirlPedal>();
        case Tide:     return std::make_unique<TidePedal>();
        case Echo:     return std::make_unique<EchoPedal>();
        case Halo:     return std::make_unique<HaloPedal>();
        default:       return nullptr;
    }
}

// --------------------------------------------------------------------------------------------------------------
// AMP: 5 modelos (nomes próprios; o som segue a referência). GAIN BASS MID TREBLE PRESENCE MASTER + CLEAN/DIRTY

enum AmpModelo { Blonde65 = 0, Chime30, Plex68, Citrus, IronHG, numAmps };

struct AmpVoz
{
    float ganhoClean, ganhoDirty;    // dB máximos do pré
    int estagiosDirty;               // degraus de válvula no canal sujo (1 a 3)
    float brilho;                    // dB de agudo no pré (cai com o GAIN)
    float hpEntre;                   // passa-alta entre degraus (aperta o grave no sujo)
    float lpEntre;                   // passa-baixa entre degraus
    float midF, midQ, midFaixa;      // centro, Q e quanto o MID cava (dB a menos com MID em 0)
    float bassF, trebF;
    float potencia;                  // quanto o power amp satura (0..1)
    float trim;                      // nível de saída do modelo (dB)
};

inline const AmpVoz& vozAmp (int m)
{
    static const AmpVoz v[numAmps] = {
        // BLONDE 65: clean americano, muito headroom, médio cavado, agudo brilhante
        { 14.0f, 30.0f, 1, 4.0f, 40.0f, 9000.0f, 450.0f, 0.9f, 14.0f, 90.0f, 3000.0f, 0.35f, 0.0f },
        // CHIME 30: classe A, agudo que "toca sino", corta o grave, quebra cedo
        { 20.0f, 36.0f, 2, 3.0f, 110.0f, 7500.0f, 1100.0f, 0.8f, 8.0f, 120.0f, 2600.0f, 0.75f, -1.0f },
        // PLEX 68: crunch britânico, médio na frente
        { 18.0f, 40.0f, 2, 2.5f, 140.0f, 7000.0f, 680.0f, 0.7f, 9.0f, 110.0f, 2500.0f, 0.6f, -1.5f },
        // CITRUS: britânico encorpado, médio grave gordo
        { 18.0f, 44.0f, 3, 1.0f, 100.0f, 6000.0f, 520.0f, 0.75f, 10.0f, 100.0f, 2200.0f, 0.55f, -2.0f },
        // IRON HG: high-gain moderno, grave apertado, médio cavado
        { 20.0f, 52.0f, 3, 2.0f, 220.0f, 6500.0f, 750.0f, 0.8f, 16.0f, 85.0f, 3200.0f, 0.4f, -3.0f },
    };
    return v[std::clamp (m, 0, (int) numAmps - 1)];
}

struct AmpCore
{
    double fs = 48000; Oversampler os; int modelo = -1, canal = -1; int fator = 2;
    OnePole hpIn, hpEst[3], lpEst[3]; Biquad brilho; DcBlock dcEst[3];
    Biquad bass, mid, treb, pres, hpOut; float sag = 0, sagK = 0.0004f;
    float k[6] { -1, -1, -1, -1, -1, -1 }; Ramp pre, master;
    void prepare (double s, int f) { fs = s; fator = f; os.fator = f; modelo = canal = -1; for (auto& v : k) v = -1; sagK = (float) (1.0 - std::exp (-1.0 / (0.05 * fs * f))); reset(); }
    void reset() { os.reset(); hpIn.reset(); brilho.reset(); for (int i = 0; i < 3; ++i) { hpEst[i].reset(); lpEst[i].reset(); dcEst[i].reset(); } bass.reset(); mid.reset(); treb.reset(); pres.reset(); hpOut.reset(); sag = 0; }
    void set (int m, int ch, const float* kn)   // kn: GAIN BASS MID TREBLE PRESENCE MASTER (0..1)
    {
        const double fo = fs * fator;
        const auto& v = vozAmp (m);
        const bool mudou = m != modelo || ch != canal;
        if (mudou)
        {
            modelo = m; canal = ch;
            hpIn.set (m == IronHG ? 70.0 : 35.0, fo);
            for (int i = 0; i < 3; ++i) { hpEst[i].set (i == 0 ? 30.0 : v.hpEntre, fo); lpEst[i].set (v.lpEntre * (i == 0 ? 1.4f : 1.0f), fo); dcEst[i].set (fo); }
            hpOut.highPass (45, 0.7, fs);
        }
        if (mudou || std::abs (kn[0] - k[0]) > 0.002f) brilho.highShelf (2500, v.brilho * (1.0f - kn[0]) + 1.0f, fo);
        if (mudou || std::abs (kn[1] - k[1]) > 0.002f) bass.lowShelf (v.bassF, lerp (-12.0f, 9.0f, kn[1]), fs);
        if (mudou || std::abs (kn[2] - k[2]) > 0.002f) mid.peak (v.midF, v.midQ, lerp (-v.midFaixa, 6.0f, kn[2]), fs);
        if (mudou || std::abs (kn[3] - k[3]) > 0.002f) treb.highShelf (v.trebF, lerp (-12.0f, 9.0f, kn[3]), fs);
        if (mudou || std::abs (kn[4] - k[4]) > 0.002f) pres.highShelf (4500, lerp (-2.0f, 8.0f, kn[4]), fs);
        for (int i = 0; i < 6; ++i) k[i] = kn[i];
        const float gmax = ch == 0 ? v.ganhoClean : v.ganhoDirty;
        pre.set (dbToGain (lerp (0.0f, gmax, kn[0])));
        master.set (kn[5]);
    }
    static float triodo (float x)   // válvula: o lado positivo amacia antes, o negativo corta mais duro
    {
        return x >= 0.0f ? tanhA (x) : 1.15f * hardish (x / 1.15f, 2.4f);
    }
    void process (float* x, int n)
    {
        const auto& v = vozAmp (modelo);
        const float g = pre.next(), mk = master.next();
        const int estagios = canal == 0 ? 1 : v.estagiosDirty;
        const float pot = dbToGain (lerp (-12.0f, 12.0f, mk) * v.potencia);
        const float limpoComp = estagios == 1 ? 1.0f / std::max (1.0f, std::sqrt (g) * 0.5f) : 1.0f;
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            float s = brilho.process (hpIn.hp (os.buf[i])) * g;
            for (int e = 0; e < estagios; ++e)
            {
                s = triodo (s * (e == 0 ? 1.0f : 2.4f) + 0.08f);
                s = dcEst[e].process (s);
                s = lpEst[e].lp (hpEst[e].hp (s));
            }
            s *= limpoComp;
            // power amp com sag: o ganho cede quando o sinal fica forte
            sag += (std::abs (s) - sag) * sagK;
            const float pp = s * pot / (1.0f + 0.35f * sag * v.potencia);
            os.buf[i] = 0.9f * tanhA (pp / 0.9f) / std::max (0.6f, std::min (pot, 1.6f));
        }
        os.down (x, n);
        const float out = dbToGain (lerp (-30.0f, 0.0f, mk) + v.trim + (canal == 0 ? 7.0f : 0.0f));   // o canal limpo sai no nível do sujo
        for (int i = 0; i < n; ++i)
            x[i] = hpOut.process (pres.process (treb.process (mid.process (bass.process (x[i]))))) * out;
    }
};

// --------------------------------------------------------------------------------------------------------------
// CAB: 1x12 OPEN, 2x12, 4x12; mic DYN/RIBBON; MIC POS (centro -> borda); DISTANCE (perto -> longe)

struct CabCore
{
    double fs = 48000; Biquad f[9]; DelayLine sala; float salaMix = 0; int salaD = 1;
    int cab = -1, mic = -1; float pos = -1, dist = -1;
    void prepare (double s) { fs = s; sala.alloc ((int) (0.01 * fs)); cab = mic = -1; reset(); }
    void reset() { for (auto& b : f) b.reset(); sala.reset(); }
    void set (int c, int mc, float p, float d)
    {
        if (c == cab && mc == mic && std::abs (p - pos) < 0.002f && std::abs (d - dist) < 0.002f) return;
        cab = c; mic = mc; pos = p; dist = d;
        const bool aberta = c == 0;
        const float res = c == 2 ? 105.0f : c == 1 ? 120.0f : 140.0f;          // ressonância da caixa
        f[0].highPass (aberta ? 95.0 : 70.0, aberta ? 0.6 : 1.1, fs);
        f[1].peak (res, 1.4, aberta ? 1.0 : (c == 2 ? 4.5 : 3.0), fs);
        f[2].peak (420, 1.0, c == 2 ? -3.5 : -2.0, fs);                        // o "buraco" de médio-grave
        f[3].peak (lerp (1800.0f, 3200.0f, 1 - p), 1.3, lerp (2.0f, 5.0f, 1 - p) - (mc == 1 ? 2.5f : 0.0f), fs);
        const float corte = lerp (6500.0f, 3400.0f, p) * (mc == 1 ? 0.78f : 1.0f) * (aberta ? 1.08f : 1.0f);
        f[4].lowPass (corte, 0.75, fs);
        f[5].lowPass (corte * 1.25f, 0.55, fs);
        f[6].peak (mc == 0 ? 5200.0 : 3800.0, 2.0, mc == 0 ? 3.5 : -2.0, fs);   // o pico do dinâmico / o veludo da fita
        f[7].lowShelf (160, lerp (4.0f, -4.0f, d) + (mc == 1 ? 1.5f : 0.0f), fs); // proximidade
        f[8].peak (8000, 0.7, -6.0, fs);
        salaD = std::max (1, (int) (lerp (0.0012f, 0.0075f, d) * fs));
        salaMix = d * 0.35f;
    }
    void process (float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            float s = x[i];
            for (auto& b : f) s = b.process (s);
            sala.push (s);
            x[i] = (s + sala.at (salaD) * salaMix) * 1.6f;
        }
    }
};

// --------------------------------------------------------------------------------------------------------------
// NOISE GATE de entrada (THRESHOLD em -90 = desligado)

struct Gate
{
    double fs = 48000; float env = 0, g = 1; int hold = 0; float atk = 0, envRel = 0; bool aberto = true;
    void prepare (double s) { fs = s; atk = (float) (1.0 - std::exp (-1.0 / (0.0008 * fs))); envRel = (float) std::exp (-1.0 / (0.02 * fs)); reset(); }
    void reset() { env = 0; g = 1; hold = 0; aberto = true; }
    void process (float* x, int n, float thrDb, float relMs)
    {
        if (thrDb <= -89.5f) { aberto = true; g = 1; return; }
        const float rel = (float) (1.0 - std::exp (-1.0 / (relMs * 0.001 * fs)));
        const float abre = dbToGain (thrDb), fecha = dbToGain (thrDb - 6.0f);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            env = a > env ? a : a + (env - a) * envRel;
            if (env > abre) { aberto = true; hold = (int) (0.02 * fs); }
            else if (env < fecha) { if (hold > 0) --hold; else aberto = false; }
            g += ((aberto ? 1.0f : 0.0f) - g) * (aberto ? atk : rel);
            x[i] *= g;
        }
    }
};

// --------------------------------------------------------------------------------------------------------------
// o board inteiro

constexpr int numSlots = 10;

struct SlotCtl { int tipo = 0; Ctl c; };

struct BoardCtl
{
    SlotCtl slot[numSlots];
    float inDb = 0, outDb = 0, gateThr = -90, gateRel = 120;
    bool ampOn = true; int amp = 0, canal = 0; float ampK[6] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    int cab = 2, mic = 0; float micPos = 0.3f, dist = 0.2f;
};

struct Board
{
    double fs = 48000; int os = 2;
    struct Slot
    {
        std::unique_ptr<Pedal> pedais[numTipos];   // um de cada tipo, criados no prepare (trocar não aloca)
        int tipo = 0; float mixOn = 0;              // crossfade do bypass
        float dryL[maxSub] {}, dryR[maxSub] {};
    } slots[numSlots];
    AmpCore amp[2]; CabCore cab[2]; Gate gate; Ramp inG, outG; float ampMix = 1;
    float bufAL[maxSub] {}, bufAR[maxSub] {};
    float picoIn = 0, picoOut = 0;   // pico desde a última leitura da tela
    bool gateAberto = true;

    void prepare (double s, bool hq)
    {
        fs = s; os = hq ? 4 : 2;
        for (auto& sl : slots)
        {
            for (int t = 1; t < numTipos; ++t) { if (! sl.pedais[t]) sl.pedais[t] = criaPedal (t); sl.pedais[t]->prepare (fs, os); }
            sl.mixOn = 0; sl.tipo = 0;
        }
        for (int k = 0; k < 2; ++k) { amp[k].prepare (fs, os); cab[k].prepare (fs); }
        gate.prepare (fs);
    }
    void reset()
    {
        for (auto& sl : slots) for (int t = 1; t < numTipos; ++t) if (sl.pedais[t]) sl.pedais[t]->reset();
        for (int k = 0; k < 2; ++k) { amp[k].reset(); cab[k].reset(); }
        gate.reset();
    }

    // L e R: entrada (a guitarra em L; com estereoIn, a média dos dois) e saída estéreo
    void process (float* L, float* R, int n, bool estereoIn, const BoardCtl& b)
    {
        for (int ofs = 0; ofs < n; ofs += maxSub)
            bloco (L + ofs, R + ofs, std::min (maxSub, n - ofs), estereoIn, b);
    }

    void bloco (float* L, float* R, int n, bool estereoIn, const BoardCtl& b)
    {
        bool st = false;
        inG.set (dbToGain (b.inDb)); outG.set (dbToGain (b.outDb));
        const float gi = inG.next (0.3f);
        float pin = 0;
        for (int i = 0; i < n; ++i) { L[i] = (estereoIn ? 0.5f * (L[i] + R[i]) : L[i]) * gi; pin = std::max (pin, std::abs (L[i])); }
        picoIn = std::max (pin, picoIn);
        gate.process (L, n, b.gateThr, b.gateRel);
        gateAberto = gate.aberto;

        for (int s = 0; s < numSlots; ++s)
        {
            auto& sl = slots[s]; const auto& sc = b.slot[s];
            if (sc.tipo != sl.tipo)
            {
                sl.tipo = sc.tipo; sl.mixOn = 0;
                if (sl.tipo > 0 && sl.tipo < numTipos) sl.pedais[sl.tipo]->reset();
            }
            if (sl.tipo <= 0 || sl.tipo >= numTipos) continue;
            auto* p = sl.pedais[sl.tipo].get();
            const float alvo = sc.c.on ? 1.0f : 0.0f;
            if (alvo == 0.0f && sl.mixOn < 0.0005f) { sl.mixOn = 0; continue; }   // desligado e assentado: não gasta CPU
            std::memcpy (sl.dryL, L, sizeof (float) * (size_t) n);
            std::memcpy (sl.dryR, st ? R : L, sizeof (float) * (size_t) n);
            if (! st && p->tornaEstereo()) std::memcpy (R, L, sizeof (float) * (size_t) n);
            p->process (L, R, n, st, sc.c);
            const float m0 = sl.mixOn;
            sl.mixOn += (alvo - sl.mixOn) * 0.35f;
            if (std::abs (sl.mixOn - alvo) < 0.0005f) sl.mixOn = alvo;
            const float m1 = sl.mixOn;
            if (m0 < 1.0f || m1 < 1.0f)
                for (int i = 0; i < n; ++i)
                {
                    const float m = m0 + (m1 - m0) * (float) i / (float) n;
                    L[i] = sl.dryL[i] + (L[i] - sl.dryL[i]) * m;
                    if (st) R[i] = sl.dryR[i] + (R[i] - sl.dryR[i]) * m;
                }
        }

        // AMP + CAB (com crossfade ao ligar/desligar)
        const float alvoAmp = b.ampOn ? 1.0f : 0.0f;
        if (alvoAmp > 0.0f || ampMix > 0.0005f)
        {
            std::memcpy (bufAL, L, sizeof (float) * (size_t) n);
            std::memcpy (bufAR, st ? R : L, sizeof (float) * (size_t) n);
            for (int k = 0; k < (st ? 2 : 1); ++k)
            {
                float* x = k == 0 ? L : R;
                amp[k].set (b.amp, b.canal, b.ampK); amp[k].process (x, n);
                cab[k].set (b.cab, b.mic, b.micPos, b.dist); cab[k].process (x, n);
            }
            const float m0 = ampMix; ampMix += (alvoAmp - ampMix) * 0.35f; if (std::abs (ampMix - alvoAmp) < 0.0005f) ampMix = alvoAmp;
            const float m1 = ampMix;
            if (m0 < 1.0f || m1 < 1.0f)
                for (int i = 0; i < n; ++i)
                {
                    const float m = m0 + (m1 - m0) * (float) i / (float) n;
                    L[i] = bufAL[i] + (L[i] - bufAL[i]) * m;
                    if (st) R[i] = bufAR[i] + (R[i] - bufAR[i]) * m;
                }
        }
        else ampMix = 0;

        if (! st) std::memcpy (R, L, sizeof (float) * (size_t) n);
        const float go = outG.next (0.3f);
        float po = 0;
        for (int i = 0; i < n; ++i)
        {
            L[i] *= go; R[i] *= go;
            if (! std::isfinite (L[i])) L[i] = 0;
            if (! std::isfinite (R[i])) R[i] = 0;
            po = std::max (po, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        picoOut = std::max (po, picoOut);
    }
};

} // namespace stomp
