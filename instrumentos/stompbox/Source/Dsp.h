#pragma once

// STOMPBOX — motor de som, sem JUCE (medido fora do plugin em tools/teste_dsp.cpp).
// Cadeia: INPUT -> NOISE GATE -> até 10 pedais em série -> AMP -> CAB -> OUTPUT.
// O sinal entra mono (guitarra). Chorus, delay e reverb abrem para estéreo; daí em diante cada pedal
// processa os dois lados com estado próprio. Os estágios que distorcem (drives, fuzz, amp) rodam com
// oversampling 2x (4x com HQ) por meio-banda IIR polifásico, sem latência.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
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
// diodos (sem pow por amostra): clip2 = joelho macio (par de silício na realimentação do op-amp, como no TS);
// clip4 = joelho duro (diodos para a terra depois de um resistor, como no RAT; germânio do Klon)
inline float clip2 (float x) { return x / std::sqrt (1.0f + x * x); }
inline float clip4 (float x) { const float x2 = x * x; return x / std::sqrt (std::sqrt (1.0f + x2 * x2)); }
inline float tanAprox (float w) { const float w2 = w * w; return w * (1.0f + w2 * (0.33333f + w2 * 0.13333f)); }   // tan(w), w < 0,4

// --------------------------------------------------------------------------------------------------------------
// pedais

enum Tipo { Vazio = 0, Squash, Hump, Clearcut, Grit, Woolly, Quack, Swirl, Tide, Echo, Halo, Bender, numTipos };

// BENDER: intervalos do SHIFT (no bico do pedal), em semitons
inline int benderSemitons (float k)
{
    static const int s[] = { -24, -12, -7, 5, 7, 12, 24 };
    return s[std::min (6, (int) (std::min (1.0f, std::max (0.0f, k)) * 6.999f))];
}
inline const char* benderNome (float k)
{
    static const char* n[] = { "-2 OCT", "-1 OCT", "-5TH", "+4TH", "+5TH", "+1 OCT", "+2 OCT" };
    return n[std::min (6, (int) (std::min (1.0f, std::max (0.0f, k)) * 6.999f))];
}

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

// ---- SQUASH: compressor de pedal com OTA (linha Dyna/Ross). LEVEL, SUSTAIN, ATTACK, BLEND.
// O OTA não tem joelho em dB: o ganho cai com a corrente de controle, g = 1 / (1 + S·env). Ataque rápido, release
// lento (~0,35 s): o "squish" no ataque e o "pump" quando a nota morre. O OTA satura de leve (o grão do pedal).
struct SquashCore
{
    double fs = 48000; float env = 0, gr = 1; Ramp lvl, sus, mix; float atk = 0.01f, rel = 0.0f, grK = 0.02f; OnePole cor;
    void prepare (double s, int) { fs = s; cor.set (6500, fs); rel = (float) std::exp (-1.0 / (0.35 * fs)); grK = (float) (1.0 - std::exp (-1.0 / (0.0008 * fs))); reset(); }
    void reset() { env = 0; gr = 1; cor.reset(); }
    void set (const Ctl& c)
    {
        lvl.set (dbToGain (lerp (-12.0f, 18.0f, c.k[0])));
        sus.set (expMap (1.5f, 60.0f, c.k[1]));
        atk = (float) std::exp (-1.0 / (expMap (0.0008f, 0.05f, c.k[2]) * fs));
        mix.set (c.k[3]);
    }
    void process (float* x, int n)
    {
        const float g = lvl.next(), S = sus.next(), m = mix.next();
        const float comp = 1.0f + 0.22f * S;   // o pedal devolve o nível que o SUSTAIN tira
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            env = a > env ? a + (env - a) * atk : a + (env - a) * rel;
            gr += (1.0f / (1.0f + S * 4.0f * env) - gr) * grK;
            const float u = x[i] * gr * comp;
            const float wet = cor.lp (0.6f * tanhA (u / 0.6f)) * g;
            x[i] = x[i] * (1.0f - m) + wet * m;
        }
    }
};
using SquashPedal = Dual<SquashCore>;

// ---- HUMP: Tube Screamer (TS808). DRIVE, LEVEL, TONE.
// Estágio de clip: op-amp não inversor com o par de diodos NA REALIMENTAÇÃO ⇒ saída = entrada + (ganho−1)·(sinal
// filtrado) limitado pelos diodos. Entrada do ganho: passa-alta de 720 Hz (4,7k + 47 nF), passa-baixa de 51 pF com
// 51k + DRIVE (500k, curva log): é daí que vem a corcova de médios. Tom: passivo de 723 Hz (1k + 220 nF) + o controle
// ativo de agudo.
struct HumpCore
{
    double fs = 48000; Oversampler os; OnePole hpIn, lpClip, toneLp; DcBlock dc; float dAtual = -1, ganho = 12;
    Ramp drive, lvl, tone;
    void prepare (double s, int f) { fs = s; os.fator = f; hpIn.set (720, fs * f); toneLp.set (723, fs); dc.set (fs); dAtual = -1; reset(); }
    void reset() { os.reset(); hpIn.reset(); lpClip.reset(); toneLp.reset(); dc.reset(); }
    void set (const Ctl& c) { drive.set (c.k[0]); lvl.set (dbToGain (lerp (-24.0f, 10.0f, c.k[1]))); tone.set (c.k[2]); }
    void process (float* x, int n)
    {
        const float d = drive.next(), g = lvl.next(), t = tone.next();
        if (std::abs (d - dAtual) > 0.001f)
        {
            dAtual = d;
            const double r = 51e3 + 500e3 * (double) (d * d);
            ganho = (float) (r / 4.7e3);
            lpClip.set (1.0 / (2 * pi * r * 51e-12), fs * os.fator);
        }
        constexpr float vd = 0.55f;   // queda do par de silício
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            const float v = os.buf[i];
            os.buf[i] = v + vd * clip2 (lpClip.lp (hpIn.hp (v)) * ganho / vd);
        }
        os.down (x, n);
        for (int i = 0; i < n; ++i)
        {
            const float lo = toneLp.lp (x[i]), hi = x[i] - lo;
            const float y = lo + hi * lerp (0.06f, 1.2f, t);
            x[i] = trilho (dc.process (y) * g * 0.8f);
        }
    }
};
using HumpPedal = Dual<HumpCore>;

// ---- CLEARCUT: Klon. GAIN, TREBLE, VOLUME, BASS; chave HEADROOM LOW/HIGH.
// Dois caminhos somados: o limpo (cai conforme o GAIN sobe, o pot duplo) e o clipado por diodos de germânio
// (joelho duro e baixo, ~0,3 V); o caminho sujo tem o médio na frente (corta grave antes do clip).
struct ClearCore
{
    double fs = 48000; Oversampler os; OnePole hpIn; Biquad treble, bass, bump; DcBlock dc;
    Ramp gain, vol; float clipLvl = 0.3f; float tr = -1, ba = -1;
    void prepare (double s, int f) { fs = s; os.fator = f; hpIn.set (160, fs * f); bump.peak (1000, 0.6, 3.0, fs * f); dc.set (fs); tr = ba = -1; reset(); }
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
        const float ganho = dbToGain (lerp (2.0f, 40.0f, gk));
        const float limpo = lerp (0.95f, 0.12f, gk), sujo = lerp (0.35f, 1.0f, gk);
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            const float s = os.buf[i];
            const float c = clipLvl * clip4 (bump.process (hpIn.hp (s)) * ganho / clipLvl);
            os.buf[i] = s * limpo + c * sujo;
        }
        os.down (x, n);
        for (int i = 0; i < n; ++i) x[i] = trilho (bass.process (treble.process (dc.process (x[i]))) * v * 0.9f);
    }
};
using ClearPedal = Dual<ClearCore>;

// ---- GRIT: RAT. DIST, FILTER, VOLUME; chave CLASSIC/MODERN.
// Ganho do LM308 = 1 + DIST·Y, com Y = (47 Ω + 2,2 µF) ∥ (560 Ω + 4,7 µF): até ~45 dB no médio (acima de 60 Hz) e
// ~67 dB acima de 1,5 kHz. O op-amp é lento (GBW ~1 MHz): com o ganho alto a banda fecha e o som escurece. Depois,
// 1N914 duros para a terra (CLASSIC) ou LEDs, mais altos e abertos, com o grave apertado (MODERN). FILTER = RC de
// 1,5k + 100k·pot com 3,3 nF (32 kHz a 475 Hz).
struct GritCore
{
    double fs = 48000; Oversampler os; OnePole hp1, h60, h1539, slew, filtro; DcBlock dc; Ramp dist, vol; float fAtual = -1, dAtual = -1; int modo = -1;
    float gMed = 0, gAlto = 0;
    void prepare (double s, int f)
    {
        fs = s; os.fator = f; dc.set (fs); fAtual = dAtual = -1; modo = -1;
        h60.set (1.0 / (2 * pi * 560.0 * 4.7e-6), fs * f); h1539.set (1.0 / (2 * pi * 47.0 * 2.2e-6), fs * f);
        reset();
    }
    void reset() { os.reset(); hp1.reset(); h60.reset(); h1539.reset(); slew.reset(); filtro.reset(); dc.reset(); }
    void set (const Ctl& c)
    {
        dist.set (c.k[0]); vol.set (dbToGain (lerp (-30.0f, 8.0f, c.k[2])));
        if (c.sw != modo) { modo = c.sw; hp1.set (modo == 0 ? 30.0 : 160.0, fs * os.fator); }
        if (std::abs (c.k[1] - fAtual) > 0.002f) { fAtual = c.k[1]; filtro.set (1.0 / (2 * pi * (1500.0 + 100e3 * fAtual) * 3.3e-9), fs); }
    }
    void process (float* x, int n)
    {
        const float d = dist.next(), v = vol.next();
        if (std::abs (d - dAtual) > 0.001f)
        {
            dAtual = d;
            const double r = 100e3 * (0.02 * d + 0.98 * d * d * d);   // pot log
            gMed = (float) (r / 560.0); gAlto = (float) (r / 47.0);
            slew.set (std::max (1800.0, 1.0e6 / (1.0 + gMed + gAlto)), fs * os.fator);
        }
        const float vd = modo == 0 ? 0.55f : 1.3f, comp = modo == 0 ? 1.0f : 0.55f;
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            const float s = hp1.hp (os.buf[i]);
            float a = s + gMed * h60.hp (s) + gAlto * h1539.hp (s);
            a = 4.0f * tanhA (slew.lp (a) * 0.25f);                    // trilhos de 9 V do op-amp
            os.buf[i] = vd * (modo == 0 ? clip4 (a / vd) : clip2 (a / vd)) * comp;
        }
        os.down (x, n);
        for (int i = 0; i < n; ++i) x[i] = trilho (dc.process (filtro.lp (x[i])) * v);
    }
};
using GritPedal = Dual<GritCore>;

// ---- WOOLLY: Fuzz Face (dois transistores com realimentação). VOLUME, FUZZ; chave GE/SI.
// GE: menos ganho, mais macio e assimétrico (harmônicos pares), escuro; SI: mais ganho, corte duro, mais agudo.
// A polarização "afunda" com sinal forte (o engasgo do fuzz, mais forte com o FUZZ baixo) e limpa com pouco sinal
// (baixe o INPUT como se fosse o volume da guitarra). Entrada por 2,2 µF: grave cheio.
struct WoollyCore
{
    double fs = 48000; Oversampler os; OnePole hpIn, lpOut, lpPre; DcBlock dc, dc2; Ramp fuzz, vol; int modo = -1; float bias = 0;
    void prepare (double s, int f) { fs = s; os.fator = f; hpIn.set (35, fs * f); dc.set (fs * f); dc2.set (fs); modo = -1; reset(); }
    void reset() { os.reset(); hpIn.reset(); lpOut.reset(); lpPre.reset(); dc.reset(); dc2.reset(); bias = 0; }
    void set (const Ctl& c)
    {
        vol.set (dbToGain (lerp (-36.0f, 6.0f, c.k[0]))); fuzz.set (c.k[1]);
        if (c.sw != modo) { modo = c.sw; lpOut.set (modo == 0 ? 4000.0 : 6500.0, fs); lpPre.set (modo == 0 ? 8000.0 : 14000.0, fs * os.fator); }
    }
    void process (float* x, int n)
    {
        const float f = fuzz.next(), v = vol.next();
        const float ganho = dbToGain (lerp (6.0f, modo == 0 ? 38.0f : 50.0f, f));
        const float pos = modo == 0 ? 0.45f : 0.42f, neg = modo == 0 ? 1.3f : 0.5f;
        const float bk = (float) (1.0 - std::exp (-1.0 / (0.03 * fs * os.fator)));
        const float afunda = lerp (0.14f, 0.06f, f);
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            float s = lpPre.lp (hpIn.hp (os.buf[i])) * ganho;
            bias += (std::min (std::abs (s), 4.0f) * afunda - bias) * bk;
            s -= bias;
            const float y = s > 0 ? pos * tanhA (s / pos) : neg * (modo == 0 ? clip2 (s / neg) : clip4 (s / neg));
            os.buf[i] = dc.process (y);
        }
        os.down (x, n);
        for (int i = 0; i < n; ++i) x[i] = trilho (dc2.process (lpOut.lp (x[i])) * v * 0.9f);
    }
};
using WoollyPedal = Dual<WoollyCore>;

// ---- QUACK: wah Cry Baby. PEDAL (no AUTO, a sensibilidade), Q, RANGE; chave PEDAL/AUTO.
// Passa-banda ressonante (o indutor) de ~350 Hz no calcanhar a ~2,2 kHz no bico, Q de 4 a 10, e o grave que o
// indutor deixa passar por baixo. Coeficientes refeitos a cada 4 amostras (sem tan/pow por amostra).
struct QuackCore
{
    double fs = 48000; float ic1 = 0, ic2 = 0; Ramp pos, q, range; float env = 0; int modo = 0; float atk = 0, rel = 0, posAtual = -1, posK = 0.01f;
    float a1 = 1, a2 = 0, a3 = 0; int conta = 0;
    void prepare (double s, int) { fs = s; atk = (float) std::exp (-1.0 / (0.004 * fs)); rel = (float) std::exp (-1.0 / (0.12 * fs)); posK = (float) (1.0 - std::exp (-1.0 / (0.012 * fs))); reset(); }
    void reset() { ic1 = ic2 = 0; env = 0; posAtual = -1; conta = 0; }
    void set (const Ctl& c) { pos.set (c.k[0]); q.set (c.k[1]); range.set (c.k[2]); modo = c.sw; }
    void process (float* x, int n)
    {
        const float p = pos.next(), qq = q.next(), r = range.next();
        const float lo = lerp (300.0f, 420.0f, r), hi = lerp (1800.0f, 2600.0f, r), lnR = std::log (hi / lo);
        const float Q = lerp (4.0f, 10.0f, qq), k = 1.0f / Q, ganhoPico = 1.3f * std::sqrt (Q) * k;
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
            if ((conta++ & 3) == 0)
            {
                const float fc = lo * std::exp (lnR * posAtual);
                const float g = tanAprox ((float) (pi * fc / fs));
                a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
            }
            const float v3 = x[i] - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            x[i] = v1 * ganhoPico + v2 * 0.2f;   // pico + o grave do indutor
        }
    }
};
using QuackPedal = Dual<QuackCore>;

// ---- SWIRL: Phase 90 (4 estágios de JFET). SPEED, FEEDBACK; chave VINTAGE/MODERN.
// VINTAGE = o "script" (realimentação baixa, varredura macia); MODERN = o de logo em bloco (mais realimentação,
// os vales mais fundos). Coeficientes a cada 4 amostras.
struct SwirlCore
{
    double fs = 48000; float s[4] {}; float fb = 0, ph = 0, a = 0; int conta = 0; Ramp spd, fbk; int modo = 0; double bpm = 120; bool sync = false; float kSpd = 0;
    void prepare (double f, int) { fs = f; reset(); }
    void reset() { for (auto& v : s) v = 0; fb = 0; conta = 0; }
    void set (const Ctl& c) { spd.set (c.k[0]); fbk.set (c.k[1]); modo = c.sw; bpm = c.bpm; sync = c.sync; kSpd = c.k[0]; }
    void process (float* x, int n)
    {
        const float sp = spd.next(), f = fbk.next();
        const double hz = sync ? bpm / 60.0 / divisaoLfo (kSpd) : expMap (0.08f, 9.0f, sp);
        const float inc = (float) (hz / fs), realim = modo == 0 ? f * 0.35f : f * 0.75f;
        const float ln = std::log (2000.0f / 160.0f);
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1.0f) ph -= 1.0f;
            if ((conta++ & 3) == 0)
            {
                const float tri = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;
                const float fc = 160.0f * std::exp (ln * tri * tri * (3 - 2 * tri));
                const float w = tanAprox ((float) (pi * fc / fs)); a = (w - 1.0f) / (w + 1.0f);
            }
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

// ---- BENDER: pitch shifter de pedal (abre para estéreo). PEDAL (calcanhar = som normal, bico = o intervalo),
// SHIFT (intervalo no bico, 7 passos), MIX (10 = só o som deslocado); chave WHAMMY/DETUNE.
// Deslocamento no tempo com duas cabeças de leitura que se revezam (janela sin²), como os pedais de 1ª geração:
// latência média de ~1/2 janela (WHAMMY 16 a 32 ms conforme o salto, DETUNE ~11 ms) só no som deslocado.
struct Shifter
{
    DelayLine dl; float ph = 0, W = 1000;
    void alloc (int n) { dl.alloc (n); }
    void reset() { dl.reset(); ph = 0; }
    float tick (float x, float razao)
    {
        dl.push (x);
        ph += (1.0f - razao) / W;
        ph -= std::floor (ph);
        float y = 0;
        for (int i = 0; i < 2; ++i)
        {
            float f = ph + 0.5f * (float) i; if (f >= 1.0f) f -= 1.0f;
            const float s = (float) std::sin (pi * f);
            y += s * s * dl.read (4.0f + f * W);
        }
        return y;
    }
};

struct BenderPedal : Pedal
{
    double fs = 48000; Shifter sh[2]; Biquad aa; Ramp mix; float semi = 0, semiK = 0.01f; int modoAnt = -1;
    bool tornaEstereo() const override { return true; }
    void prepare (double f, int) override
    {
        fs = f; for (auto& s : sh) s.alloc ((int) (0.09 * fs));
        semiK = (float) (1.0 - std::exp (-1.0 / (0.012 * fs)));
        reset();
    }
    void reset() override { for (auto& s : sh) s.reset(); aa.reset(); semi = 0; modoAnt = -1; }
    void process (float* L, float* R, int n, bool& st, const Ctl& c) override
    {
        const int modo = c.sw;
        if (modo != modoAnt) { for (auto& s : sh) { s.reset(); s.W = (float) ((modo == 0 ? 0.032 : 0.022) * fs); } modoAnt = modo; }
        mix.set (c.k[2]);
        const float m = mix.next();
        const float pos = clampf (c.k[0], 0.0f, 1.0f);
        if (modo == 0)
        {
            const float alvo = (float) benderSemitons (c.k[1]) * pos;
            // janela maior quanto maior o salto (para cima: emenda menos frequente; para baixo: cabe o período da nota grave)
            const float rAlvo = std::pow (2.0f, alvo / 12.0f), wAlvo = (float) (0.032 * fs) * std::sqrt (std::max (rAlvo, 1.0f / rAlvo));
            // para cima, corta o que passaria de Nyquist depois de deslocado
            const float razaoMax = std::pow (2.0f, std::max (0.0f, alvo) / 12.0f);
            aa.lowPass (std::min (16000.0, 0.42 * fs / razaoMax), 0.7, fs);
            for (int i = 0; i < n; ++i)
            {
                semi += (alvo - semi) * semiK;
                sh[0].W += (wAlvo - sh[0].W) * 0.0005f;
                const float dry = st ? 0.5f * (L[i] + R[i]) : L[i];
                const float y = sh[0].tick (aa.process (dry), std::pow (2.0f, semi / 12.0f));
                const float b = clampf (std::abs (semi) * 2.0f, 0.0f, 1.0f);   // perto de 0 semitom, volta ao som limpo
                const float wet = dry + (y - dry) * b;
                const float out = dry + (wet - dry) * m;
                L[i] = out; R[i] = out;
            }
        }
        else
        {
            // DETUNE: uma voz um pouco acima (L) e outra um pouco abaixo (R); o PEDAL dá até 30 cents
            const float alvo = 0.3f * pos;
            for (auto& s : sh) s.W = (float) (0.022 * fs);
            for (int i = 0; i < n; ++i)
            {
                semi += (alvo - semi) * semiK;
                const float r = std::pow (2.0f, semi / 12.0f);
                const float inL = L[i], inR = st ? R[i] : L[i], dry = 0.5f * (inL + inR);
                const float a = sh[0].tick (dry, r), b = sh[1].tick (dry, 1.0f / r);
                L[i] = inL + (0.7f * (inL + a) - inL) * m;
                R[i] = inR + (0.7f * (inR + b) - inR) * m;
            }
        }
        st = true;
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
        case Bender:   return std::make_unique<BenderPedal>();
        default:      return nullptr;
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

// tone stack FMV (Fender/Marshall/Vox): função de transferência analítica de 3ª ordem (Yeh & Smith, 2006) com os
// valores de componentes de cada amp, discretizada por bilinear na taxa do oversampling; refeita só quando um knob
// muda. R1 = pot de agudo, R2 = pot de grave, R3 = pot de médio, R4 = resistor de inclinação; C1 agudo, C2 grave, C3 médio.
struct StackVals { double R1, R2, R3, R4, C1, C2, C3; };
inline const StackVals& stackAmp (int m)
{
    static const StackVals v[numAmps] = {
        { 250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9 },   // BLONDE 65: blackface (médio quase fixo)
        { 1e6,   1e6,   10e3, 100e3, 56e-12,  6.8e-9, 22e-9 },   // CHIME 30: top boost (agudo de 56 pF, grave magro)
        { 220e3, 1e6,   25e3, 33e3,  470e-12, 22e-9,  22e-9 },   // PLEX 68: Marshall
        { 220e3, 1e6,   25e3, 56e3,  470e-12, 47e-9,  22e-9 },   // CITRUS: britânico com o grave mais gordo
        { 250e3, 1e6,   25e3, 47e3,  250e-12, 20e-9,  20e-9 },   // IRON HG: médio cavado (inclinação de Bassman)
    };
    return v[std::clamp (m, 0, (int) numAmps - 1)];
}

struct ToneStack
{
    double b[4] {}, a[4] {}, z[3] {};
    void reset() { z[0] = z[1] = z[2] = 0.0; }
    // coeficientes analógicos: H(s) = (b1 s + b2 s² + b3 s³) / (1 + a1 s + a2 s² + a3 s³); t, m, l = posição dos pots 0..1
    static void analog (const StackVals& v, double t, double m, double l, double* B, double* A)
    {
        const double R1 = v.R1, R2 = v.R2, R3 = v.R3, R4 = v.R4, C1 = v.C1, C2 = v.C2, C3 = v.C3;
        B[0] = 0.0;
        B[1] = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        B[2] = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
             + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
             + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        const double C123 = C1 * C2 * C3;
        B[3] = l * m * (C123 * R1 * R2 * R3 + C123 * R2 * R3 * R4) - m * m * (C123 * R1 * R3 * R3 + C123 * R3 * R3 * R4)
             + m * (C123 * R1 * R3 * R3 + C123 * R3 * R3 * R4) + t * C123 * R1 * R3 * R4 - t * m * C123 * R1 * R3 * R4
             + t * l * C123 * R1 * R2 * R4;
        A[0] = 1.0;
        A[1] = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        A[2] = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
             + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        A[3] = l * m * (C123 * R1 * R2 * R3 + C123 * R2 * R3 * R4) - m * m * (C123 * R1 * R3 * R3 + C123 * R3 * R3 * R4)
             + m * (C123 * R3 * R3 * R4 + C123 * R1 * R3 * R3 - C123 * R1 * R3 * R4) + l * C123 * R1 * R2 * R4 + C123 * R1 * R3 * R4;
    }
    static double magnitude (const StackVals& v, double t, double m, double l, double f)
    {
        double B[4], A[4]; analog (v, t, m, l, B, A);
        const std::complex<double> s (0.0, 2 * pi * f);
        const auto num = s * (B[1] + s * (B[2] + s * B[3])), den = 1.0 + s * (A[1] + s * (A[2] + s * A[3]));
        return std::abs (num / den);
    }
    static double taperLog (double k) { return (std::exp (3.5 * k) - 1.0) / (std::exp (3.5) - 1.0); }   // pot de grave (log)
    void set (const StackVals& v, double t, double m, double l, double fs, double ganho)
    {
        double B[4], A[4]; analog (v, t, m, l, B, A);
        static const double P[4][4] = { { 1, 3, 3, 1 }, { 1, 1, -1, -1 }, { 1, -1, -1, 1 }, { 1, -3, 3, -1 } };   // (1-z)^k (1+z)^(3-k)
        const double c = 2.0 * fs; double nb[4] {}, na[4] {}, ck = 1.0;
        for (int k = 0; k < 4; ++k) { for (int i = 0; i < 4; ++i) { nb[i] += B[k] * ck * P[k][i]; na[i] += A[k] * ck * P[k][i]; } ck *= c; }
        for (int i = 0; i < 4; ++i) { b[i] = nb[i] / na[0] * ganho; a[i] = na[i] / na[0]; }
    }
    float process (float xf)
    {
        const double x = xf, y = b[0] * x + z[0];
        z[0] = b[1] * x - a[1] * y + z[1]; z[1] = b[2] * x - a[2] * y + z[2]; z[2] = b[3] * x - a[3] * y;
        return (float) y;
    }
};

// por modelo: corte do capacitor de catodo nos degraus (0 = totalmente desviado, grave cheio) e polarização do power
// (classe A do CHIME: assimétrico, harmônicos pares)
inline float catodoAmp (int m) { static const float f[numAmps] = { 0.0f, 70.0f, 110.0f, 120.0f, 260.0f }; return f[std::clamp (m, 0, (int) numAmps - 1)]; }
inline float classeA (int m) { return m == Chime30 ? 0.22f : 0.0f; }

struct AmpCore
{
    double fs = 48000; Oversampler os; int modelo = -1, canal = -1; int fator = 2;
    OnePole hpIn, hpEst[3], lpEst[3], hpCat[3]; Biquad brilho; DcBlock dcEst[3];
    ToneStack stack; double stackNorm = 1.0; Biquad pres, hpOut; float sag = 0, sagK = 0.0004f;
    float k[6] { -1, -1, -1, -1, -1, -1 }; Ramp pre, master;
    void prepare (double s, int f) { fs = s; fator = f; os.fator = f; modelo = canal = -1; for (auto& v : k) v = -1; sagK = (float) (1.0 - std::exp (-1.0 / (0.05 * fs * f))); reset(); }
    void reset()
    {
        os.reset(); hpIn.reset(); brilho.reset(); stack.reset(); pres.reset(); hpOut.reset(); sag = 0;
        for (int i = 0; i < 3; ++i) { hpEst[i].reset(); lpEst[i].reset(); dcEst[i].reset(); hpCat[i].reset(); }
    }
    void set (int m, int ch, const float* kn)   // kn: GAIN BASS MID TREBLE PRESENCE MASTER (0..1)
    {
        const double fo = fs * fator;
        const auto& v = vozAmp (m);
        const bool mudou = m != modelo || ch != canal;
        if (mudou)
        {
            const bool trocouModelo = m != modelo;
            modelo = m; canal = ch;
            hpIn.set (m == IronHG ? 70.0 : 35.0, fo);
            for (int i = 0; i < 3; ++i)
            {
                hpEst[i].set (i == 0 ? 30.0 : v.hpEntre, fo); lpEst[i].set (v.lpEntre * (i == 0 ? 1.4f : 1.0f), fo); dcEst[i].set (fo);
                hpCat[i].set (std::max (10.0f, catodoAmp (m)), fo);
            }
            hpOut.highPass (45, 0.7, fs);
            if (trocouModelo)   // nível do stack com os knobs no meio = 0 dB em 700 Hz
                stackNorm = 1.0 / std::max (1e-6, ToneStack::magnitude (stackAmp (m), 0.5, 0.5, ToneStack::taperLog (0.5), 700.0));
        }
        if (mudou || std::abs (kn[0] - k[0]) > 0.002f) brilho.highShelf (2500, v.brilho * (1.0f - kn[0]) + 1.0f, fo);
        if (mudou || std::abs (kn[1] - k[1]) > 0.002f || std::abs (kn[2] - k[2]) > 0.002f || std::abs (kn[3] - k[3]) > 0.002f)
            stack.set (stackAmp (m), clampf (kn[3], 0, 1), clampf (kn[2], 0, 1), ToneStack::taperLog (clampf (kn[1], 0, 1)), fo, stackNorm);
        if (mudou || std::abs (kn[4] - k[4]) > 0.002f) pres.highShelf (3500, lerp (-3.0f, 9.0f, kn[4]), fo);   // PRESENCE: tira realimentação dos agudos no power
        for (int i = 0; i < 6; ++i) k[i] = kn[i];
        const float gmax = ch == 0 ? v.ganhoClean : v.ganhoDirty;
        pre.set (dbToGain (lerp (0.0f, gmax, kn[0])));
        master.set (kn[5]);
    }
    static float triodo (float x)   // válvula: o lado positivo (corrente de grade) amacia antes; o negativo (corte) é mais duro
    {
        return x >= 0.0f ? tanhA (x) : 1.15f * clip4 (x / 1.15f);
    }
    void process (float* x, int n)
    {
        const auto& v = vozAmp (modelo);
        const float g = pre.next(), mk = master.next();
        const int estagios = canal == 0 ? 1 : v.estagiosDirty;
        const float pot = dbToGain (lerp (-12.0f, 12.0f, mk) * v.potencia);
        const float limpoComp = estagios == 1 ? 1.0f / std::max (1.0f, std::sqrt (g) * 0.5f) : 1.0f;
        const bool temCatodo = catodoAmp (modelo) > 0.0f;
        const float ca = classeA (modelo), caOff = tanhA (ca);
        const int m = os.up (x, n);
        for (int i = 0; i < m; ++i)
        {
            float s = brilho.process (hpIn.hp (os.buf[i])) * g;
            for (int e = 0; e < estagios; ++e)
            {
                if (e > 0) s *= 2.4f;
                if (temCatodo) s = 0.35f * s + 0.65f * hpCat[e].hp (s);   // catodo parcialmente desviado: grave com menos ganho
                s = triodo (s + 0.08f);
                s = dcEst[e].process (s);
                s = lpEst[e].lp (hpEst[e].hp (s));
            }
            s *= limpoComp;
            s = pres.process (stack.process (s));
            // power amp push-pull com sag: o ganho cede quando o sinal fica forte; classe A = assimétrico
            sag += (std::abs (s) - sag) * sagK;
            const float pp = s * pot / (1.0f + 0.35f * sag * v.potencia);
            os.buf[i] = 0.9f * (tanhA (pp / 0.9f + ca) - caOff) / std::max (0.6f, std::min (pot, 1.6f));
        }
        os.down (x, n);
        const float out = dbToGain (lerp (-30.0f, 0.0f, mk) + v.trim + (canal == 0 ? 7.0f : 0.0f));   // o canal limpo sai no nível do sujo
        for (int i = 0; i < n; ++i) x[i] = hpOut.process (x[i]) * out;
    }
};

// --------------------------------------------------------------------------------------------------------------
// CAB: 1x12 OPEN, 2x12, 4x12; mic DYN/RIBBON; MIC POS (centro -> borda); DISTANCE (perto -> longe)
// Cada caixa é um FIR curto de fase mínima (sem latência) gerado no prepare a partir da curva típica do falante
// microfonado (pontos em dB, interpolados em log-frequência): a ressonância da caixa, o médio, os picos de quebra do
// cone entre 2 e 5 kHz (com a irregularidade fina de um falante de verdade) e a queda íngreme acima de ~5,5 kHz.
// O microfone (posição, distância, dinâmico/fita) são filtros simples por cima.

inline void fftInPlace (std::vector<std::complex<double>>& a, bool inversa)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2 * pi / (double) len * (inversa ? 1 : -1);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j) { const auto u = a[i + j], t = a[i + j + len / 2] * w; a[i + j] = u + t; a[i + j + len / 2] = u - t; w *= wl; }
        }
    }
    if (inversa) for (auto& x : a) x /= (double) n;
}

struct CurvaFalante { int n; float f[30], db[30]; };
inline const CurvaFalante& curvaCab (int c)
{
    static const CurvaFalante v[3] = {
        // 1x12 aberto, falante alnico "azul": brilhante, grave solto (caixa aberta corta abaixo de ~110 Hz)
        { 24, { 50, 70, 90, 110, 140, 200, 300, 450, 700, 1000, 1400, 1800, 2200, 2700, 3200, 3700, 4300, 5000, 6000, 7000, 8500, 10000, 13000, 20000 },
              { -24, -14, -7, -2, 0, -1, -2, -2.5f, -1.5f, 0, 1, 2.5f, 3.5f, 5, 4, 6, 4, 1, -7, -15, -25, -33, -44, -60 } },
        // 2x12 fechado, falante de 30 W "moderno": médio-agudo na cara (2–4 kHz), grave firme
        { 25, { 50, 70, 90, 115, 140, 200, 300, 450, 700, 1000, 1200, 1500, 2000, 2500, 3000, 3500, 4000, 4600, 5200, 6000, 7000, 8500, 10000, 13000, 20000 },
              { -18, -9, -2, 1.5f, 0, -2, -3, -3.5f, -2, 0.5f, 2.5f, 1, 4, 7, 6, 3.5f, 5, 1, -5, -12, -20, -28, -36, -46, -60 } },
        // 4x12 fechado, falante "verde" de 25 W: médio macio e redondo, ressonância grande, agudo que cai cedo
        { 24, { 50, 70, 90, 105, 130, 180, 250, 400, 600, 900, 1300, 1700, 2100, 2600, 3100, 3600, 4100, 4700, 5300, 6200, 7500, 9000, 12000, 20000 },
              { -17, -8, -1, 2.5f, 1, -1, -2, -3, -2, 0, 1.5f, 3, 4.5f, 5, 3, 4.5f, 1.5f, -3, -9, -17, -27, -35, -45, -60 } },
    };
    return v[std::clamp (c, 0, 2)];
}

struct CabCore
{
    double fs = 48000, fsFir = 0; int taps = 0; std::vector<float> fir[3], hist; int hp = 0;
    Biquad f[5]; DelayLine sala; float salaMix = 0; int salaD = 1;
    int cab = -1, mic = -1; float pos = -1, dist = -1;
    void prepare (double s)
    {
        fs = s;
        if (fsFir != fs) { projeta(); fsFir = fs; }   // HQ liga/desliga sem refazer (e sem alocar)
        sala.alloc ((int) (0.01 * fs)); cab = mic = -1; reset();
    }
    void reset() { for (auto& b : f) b.reset(); sala.reset(); std::fill (hist.begin(), hist.end(), 0.0f); hp = 0; }
    static double dbCurva (const CurvaFalante& c, double fr, int qual)
    {
        double d;
        if (fr <= c.f[0]) d = c.db[0] - 18.0 * std::log2 (c.f[0] / std::max (fr, 1.0));          // abaixo: 18 dB/oitava
        else if (fr >= c.f[c.n - 1]) d = c.db[c.n - 1];
        else
        {
            int i = 0; while (fr > c.f[i + 1]) ++i;
            const double t = std::log (fr / c.f[i]) / std::log ((double) c.f[i + 1] / c.f[i]);
            d = c.db[i] + (c.db[i + 1] - c.db[i]) * t;
        }
        // irregularidade fina do cone (determinística), só do médio-agudo para cima
        const double lf = std::log2 (std::max (fr, 1.0));
        const double w = clampf ((float) ((lf - std::log2 (900.0)) / (std::log2 (4000.0) - std::log2 (900.0))), 0.0f, 1.0f);
        d += w * (1.3 * std::sin (2 * pi * 5.3 * lf + qual) + 0.8 * std::sin (2 * pi * 11.7 * lf + 2.0 * qual) + 0.5 * std::sin (2 * pi * 23.1 * lf + 0.7 * qual));
        return std::max (d, -80.0);
    }
    void projeta()
    {
        taps = std::min (1024, std::max (64, (int) std::lround (0.009 * fs)));
        int N = 4096; while (N < taps * 8 || N < (int) (fs / 6.0)) N <<= 1;
        std::vector<std::complex<double>> a ((size_t) N);
        for (int c = 0; c < 3; ++c)
        {
            const auto& cv = curvaCab (c);
            for (int k = 0; k <= N / 2; ++k)
            {
                const double lm = dbCurva (cv, (double) k * fs / N, c) * std::log (10.0) / 20.0;   // ln|H|
                a[(size_t) k] = lm; if (k > 0 && k < N / 2) a[(size_t) (N - k)] = lm;
            }
            fftInPlace (a, true);                                    // cepstro real
            for (int i = 1; i < N / 2; ++i) a[(size_t) i] *= 2.0;
            for (int i = N / 2 + 1; i < N; ++i) a[(size_t) i] = 0.0;
            fftInPlace (a, false);
            for (auto& x : a) x = std::exp (x);                      // espectro de fase mínima
            fftInPlace (a, true);
            fir[c].assign ((size_t) taps, 0.0f);
            const int fade = taps / 4;
            for (int i = 0; i < taps; ++i)
            {
                const double w = i < taps - fade ? 1.0 : 0.5 * (1.0 + std::cos (pi * (double) (i - (taps - fade)) / fade));
                fir[c][(size_t) i] = (float) (a[(size_t) i].real() * w);
            }
            // 0 dB em 1 kHz
            std::complex<double> h1 = 0; for (int i = 0; i < taps; ++i) h1 += (double) fir[c][(size_t) i] * std::polar (1.0, -2 * pi * 1000.0 * i / fs);
            const float g = (float) (1.0 / std::max (1e-6, std::abs (h1)));
            for (auto& x : fir[c]) x *= g;
        }
        hist.assign ((size_t) taps * 2, 0.0f); hp = 0;
    }
    void set (int c, int mc, float p, float d)
    {
        if (c == cab && mc == mic && std::abs (p - pos) < 0.002f && std::abs (d - dist) < 0.002f) return;
        cab = std::clamp (c, 0, 2); mic = mc; pos = p; dist = d;
        f[0].highShelf (2500, lerp (2.0f, -7.0f, p), fs);                                      // centro brilha, borda escurece
        f[1].peak (lerp (3300.0f, 2000.0f, p), 1.2, lerp (2.0f, 0.0f, p), fs);
        if (mc == 0) f[2].peak (5200, 2.0, 2.0, fs); else f[2].highShelf (3500, -5.0, fs);      // o pico do dinâmico / o veludo da fita
        f[3].lowShelf (180, lerp (3.5f, -4.0f, d) + (mc == 1 ? 1.5f : 0.0f), fs);              // proximidade
        f[4].highPass (40, 0.7, fs);
        salaD = std::max (1, (int) (lerp (0.0012f, 0.0075f, d) * fs));
        salaMix = d * 0.35f;
    }
    void process (float* x, int n)
    {
        const float* h = fir[cab].data();
        for (int i = 0; i < n; ++i)
        {
            hp = (hp == 0 ? taps : hp) - 1;
            hist[(size_t) hp] = hist[(size_t) (hp + taps)] = x[i];
            const float* xs = hist.data() + hp;
            float a0 = 0, a1 = 0, a2 = 0, a3 = 0; int j = 0;
            for (; j + 3 < taps; j += 4) { a0 += h[j] * xs[j]; a1 += h[j + 1] * xs[j + 1]; a2 += h[j + 2] * xs[j + 2]; a3 += h[j + 3] * xs[j + 3]; }
            for (; j < taps; ++j) a0 += h[j] * xs[j];
            float s = (a0 + a1) + (a2 + a3);
            for (auto& b : f) s = b.process (s);
            sala.push (s);
            x[i] = (s + sala.at (salaD) * salaMix) * 1.1f;
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
