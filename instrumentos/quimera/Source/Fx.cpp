// Rack de efeitos da QUIMERA: HIPER → DISTORÇÃO → PHASER → CHORUS → DELAY → REVERB → EQ → COMPRESSOR.
// Tudo feito aqui dentro (sem juce_dsp): biquad, Linkwitz-Riley, oversampling 2× polifásico IIR (latência zero).
// Cada bloco roda em pedaços de até 256 amostras; parâmetros lentos são recalculados por pedaço.

#include "Fx.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace quimera
{
namespace
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr double kPiD  = 3.14159265358979323846;
constexpr int   kChunk = 256;   // pedaço interno: aceita qualquer numSamples

inline float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }

// suavização de um polo: fração por amostra para uma constante de tempo de `seconds`
inline float smoothK (double fs, double seconds)
{
    return (float) (1.0 - std::exp (-1.0 / (std::max (1.0e-4, seconds) * fs)));
}

// coeficiente do passa-baixa de um polo em `hz`
inline float onePoleLp (double fs, double hz)
{
    return (float) (1.0 - std::exp (-2.0 * kPiD * hz / fs));
}

// coeficiente de envelope (ataque/soltura) em ms
inline float envCoef (double fs, double ms)
{
    return (float) std::exp (-1.0 / (std::max (0.05, ms) * 0.001 * fs));
}

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-6f)); }

// mix de delay/reverb: até 50% o seco fica inteiro e o molhado sobe; depois o seco desce
inline void wetDry (float m, float& dryG, float& wetG)
{
    dryG = std::min (1.0f, 2.0f - 2.0f * m);
    wetG = std::min (1.0f, 2.0f * m);
}

// joelho macio de 6 dB; devolve a redução em dB (≥ 0)
inline float kneeReduction (float levelDb, float thrDb, float ratio)
{
    const float knee  = 6.0f;
    const float over  = levelDb - thrDb;
    const float slope = 1.0f - 1.0f / std::max (1.0f, ratio);
    if (over <= -0.5f * knee) return 0.0f;
    if (over >= 0.5f * knee)  return over * slope;
    const float x = over + 0.5f * knee;
    return slope * x * x / (2.0f * knee);
}

// saturação de segurança nos laços: linear até 1, curva suave depois
inline float safeSat (float x)
{
    const float a = std::abs (x);
    if (a < 1.0f) return x;
    return std::copysign (1.0f + 0.5f * std::tanh ((a - 1.0f) * 2.0f), x);
}

struct Smooth
{
    float v = 0.0f, t = 0.0f;
    void  snap (float x) { v = t = x; }
    float next (float k)
    {
        v += (t - v) * k;
        if (std::abs (t - v) < 1.0e-6f) v = t;
        return v;
    }
};

// ---------------- linha de atraso (anel potência de 2) ----------------
class DelayLine
{
public:
    void prepare (int maxDelay)
    {
        int len = 16;
        while (len < maxDelay + 8) len <<= 1;
        buf.assign ((size_t) len, 0.0f);
        mask = len - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    int  maxDelay() const { return mask - 4; }

    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d amostras atrás (1 = a última gravada)
    float tap (int d) const { return buf[(size_t) ((w - d) & mask)]; }

    float readLin (float d) const   // d ≥ 1
    {
        const int   k = (int) d;
        const float t = d - (float) k;
        const float a = tap (k), b = tap (k + 1);
        return a + (b - a) * t;
    }
    float readCubic (float d) const   // d ≥ 2 (Hermite de 4 pontos)
    {
        const int   k = (int) d;
        const float t = d - (float) k;
        const float xm1 = tap (k - 1), x0 = tap (k), x1 = tap (k + 1), x2 = tap (k + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

// difusor passa-tudo de Schroeder
class Allpass
{
public:
    void  prepare (int samples) { d = std::max (1, samples); line.prepare (d + 2); }
    void  clear() { line.clear(); }
    float process (float x, float g)
    {
        const float z = line.tap (d);
        const float v = x + g * z;
        line.push (v);
        return z - g * v;
    }

private:
    DelayLine line;
    int d = 1;
};

// ---------------- biquad (TDF2, estado em double) com receitas RBJ ----------------
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void clear() { z1 = z2 = 0.0; }
    float process (float xf)
    {
        const double x = (double) xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        const double inv = 1.0 / A0;
        b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv; a1 = A1 * inv; a2 = A2 * inv;
    }

    static double w0 (double fs, double hz) { return 2.0 * kPiD * std::min (hz, fs * 0.45) / fs; }

    void lowpass (double fs, double hz, double q)
    {
        const double w = w0 (fs, hz), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    void highpass (double fs, double hz, double q)
    {
        const double w = w0 (fs, hz), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    void allpass (double fs, double hz, double q)
    {
        const double w = w0 (fs, hz), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1.0 - al, -2.0 * c, 1.0 + al, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    void peak (double fs, double hz, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = w0 (fs, hz), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
    }
    void lowShelf (double fs, double hz, double db)   // inclinação S = 1
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = w0 (fs, hz), c = std::cos (w), al = std::sin (w) / std::sqrt (2.0);
        const double sa = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1.0) - (A - 1.0) * c + sa),
             2.0 * A * ((A - 1.0) - (A + 1.0) * c),
             A * ((A + 1.0) - (A - 1.0) * c - sa),
             (A + 1.0) + (A - 1.0) * c + sa,
             -2.0 * ((A - 1.0) + (A + 1.0) * c),
             (A + 1.0) + (A - 1.0) * c - sa);
    }
    void highShelf (double fs, double hz, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = w0 (fs, hz), c = std::cos (w), al = std::sin (w) / std::sqrt (2.0);
        const double sa = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1.0) + (A - 1.0) * c + sa),
             -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
             A * ((A + 1.0) + (A - 1.0) * c - sa),
             (A + 1.0) - (A - 1.0) * c + sa,
             2.0 * ((A - 1.0) - (A + 1.0) * c),
             (A + 1.0) - (A - 1.0) * c - sa);
    }
};

// ---------------- oversampling 2× polifásico IIR (meia-banda, fase mínima, sem latência reportada) ----------------
// duas trilhas de passa-tudos de 1ª ordem; coeficientes intercalados (pares = trilha 0, ímpares = trilha 1)
constexpr int kHbCoefs = 8;
constexpr float kHb[kHbCoefs] = { 0.07711507983241622f, 0.2659685265210946f, 0.4820706250610472f, 0.6651041532634957f,
                                  0.7968204713315797f,  0.8841015085506159f, 0.9412514277740471f, 0.9820054141886075f };

struct HalfbandPaths
{
    float x[kHbCoefs] {}, y[kHbCoefs] {};
    void clear() { std::fill (x, x + kHbCoefs, 0.0f); std::fill (y, y + kHbCoefs, 0.0f); }
    void run (float& s0, float& s1)
    {
        for (int i = 0; i < kHbCoefs; i += 2)
        {
            const float t0 = (s0 - y[i]) * kHb[i] + x[i];
            x[i] = s0; y[i] = t0; s0 = t0;
            const float t1 = (s1 - y[i + 1]) * kHb[i + 1] + x[i + 1];
            x[i + 1] = s1; y[i + 1] = t1; s1 = t1;
        }
    }
};

struct Oversampler2x
{
    HalfbandPaths up, down;
    void clear() { up.clear(); down.clear(); }
    void upsample (float in, float& o0, float& o1)
    {
        o0 = in; o1 = in;
        up.run (o0, o1);
    }
    float downsample (float i0, float i1)
    {
        float s0 = i1, s1 = i0;
        down.run (s0, s1);
        return 0.5f * (s0 + s1);
    }
};

// =====================================================================================
// HIPER: 6 cópias com atraso curto modulado (detune), espalhadas no estéreo
// =====================================================================================
class Hyper
{
public:
    static constexpr int nV = 6;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& d : line) d.prepare ((int) (fs * 0.040) + 8);
        maxD = (float) (fs * 0.008);
        float sl = 0.0f, sr = 0.0f;
        for (int v = 0; v < nV; ++v)
        {
            const float a = (pans[v] + 1.0f) * 0.25f * kPi;   // pan de potência constante
            gl[v] = std::cos (a);
            gr[v] = std::sin (a);
            sl += gl[v] * gl[v];
            sr += gr[v] * gr[v];
            baseS[v] = (float) (baseMs[v] * 0.001 * fs);
        }
        normL = 1.0f / std::sqrt (std::max (sl, 1.0e-3f));
        normR = 1.0f / std::sqrt (std::max (sr, 1.0e-3f));
        k = smoothK (fs, 0.03);
        clear();
    }

    void clear()
    {
        for (auto& d : line) d.clear();
        for (int v = 0; v < nV; ++v) ph[v] = (float) v / (float) nV;
        primed = false;
    }

    void process (float* l, float* r, int n, const Snapshot& s, bool fresh)
    {
        if (fresh) clear();
        const auto& x = s.fx;
        const float rate  = 0.1f * std::pow (60.0f, clamp01 (x.hypRate / 100.0f));   // 0,1–6 Hz
        const float cents = 30.0f * clamp01 (x.hypDetune / 100.0f);                 // desvio de pico
        // seno no atraso: desvio de altura Δ = 2π·f·D/fs ⇒ D = Δ·fs / (2π·f)
        const float dev   = std::pow (2.0f, cents / 1200.0f) - 1.0f;
        const float depth = dev * (float) fs / (kTwoPi * rate);
        const float mix   = clamp01 (x.hypMix / 100.0f);
        if (! primed) { depthSm.snap (depth); mixSm.snap (mix); primed = true; }
        depthSm.t = depth;
        mixSm.t   = mix;
        const float inc = rate / (float) fs;

        for (int i = 0; i < n; ++i)
        {
            line[0].push (l[i]);
            line[1].push (r[i]);
            const float d = depthSm.next (k), m = mixSm.next (k);
            float wl = 0.0f, wr = 0.0f;
            for (int v = 0; v < nV; ++v)
            {
                ph[v] += inc * mult[v];
                if (ph[v] >= 1.0f) ph[v] -= 1.0f;
                const float dv    = std::min (maxD, d / mult[v]);
                const float delay = baseS[v] + dv * std::sin (kTwoPi * ph[v]);
                const float y     = line[pans[v] < 0.0f ? 0 : 1].readCubic (delay);
                wl += y * gl[v];
                wr += y * gr[v];
            }
            l[i] += (wl * normL - l[i]) * m;
            r[i] += (wr * normR - r[i]) * m;
        }
    }

private:
    static constexpr double baseMs[nV] = { 9.1, 12.7, 15.3, 18.9, 22.1, 26.3 };
    static constexpr float  pans[nV]   = { -1.0f, 1.0f, -0.55f, 0.55f, -0.2f, 0.2f };
    static constexpr float  mult[nV]   = { 1.0f, 1.13f, 0.87f, 1.27f, 0.79f, 1.07f };

    double fs = 44100.0;
    DelayLine line[2];
    float ph[nV] {}, gl[nV] {}, gr[nV] {}, baseS[nV] {};
    float maxD = 350.0f, normL = 1.0f, normR = 1.0f, k = 0.001f;
    Smooth depthSm, mixSm;
    bool primed = false;
};

// =====================================================================================
// DISTORÇÃO: 6 tipos; os contínuos rodam a 2× (polifásico IIR)
// =====================================================================================
class Distortion
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        k = smoothK (fs, 0.02);
        dcR = (float) (1.0 - 2.0 * kPiD * 10.0 / fs);
        clear();
    }

    void clear()
    {
        for (auto& o : os) o.clear();
        for (auto& t : tone) t.clear();
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
        holdL = holdR = 0.0f;
        holdPh = 1.0f;
        lastToneHz = -1.0f;
        primed = false;
    }

    void process (float* l, float* r, int n, const Snapshot& s, bool fresh)
    {
        if (fresh) clear();
        const auto& x = s.fx;
        const int   type = juce::jlimit (0, 5, x.dstType);
        const float d01  = clamp01 (x.dstDrive / 100.0f);
        const float mix  = clamp01 (x.dstMix / 100.0f);

        // DRIVE: 0..+36 dB na entrada e compensação pelo nível de um sinal de referência
        const float g    = dbToGain (36.0f * d01);
        const float ref  = 0.3f;
        const bool  digital = type == 3 || type == 4;   // bitcrush/downsample: o drive vira bits/taxa
        const float probe = type == 2 ? std::sin (0.5f * kPi * std::min (1.0f, ref * g)) : shape (type, ref * g);
        const float comp  = digital ? 1.0f : ref / std::max (1.0e-4f, std::abs (probe));
        const float gIn   = digital ? 1.0f : g;

        // TOM: passa-baixa 1 kHz → 20 kHz depois do shaper
        const float toneHz = 1000.0f * std::pow (20.0f, clamp01 (x.dstTone / 100.0f));
        if (std::abs (toneHz - lastToneHz) > 0.5f)
        {
            lastToneHz = toneHz;
            for (auto& t : tone) t.lowpass (fs, (double) toneHz, 0.7071);
        }

        if (! primed || type != lastType)
        {
            // trocar de tipo: ganho e compensação vão direto para o novo valor
            gSm.snap (gIn);
            compSm.snap (comp);
            if (! primed) mixSm.snap (mix);
            primed = true;
            lastType = type;
        }
        gSm.t    = gIn;
        compSm.t = comp;
        mixSm.t  = mix;

        const float steps   = std::pow (2.0f, (16.0f - 14.0f * d01) - 1.0f);   // bits 16 → 2
        const float holdInc = 1.0f / std::pow (32.0f, d01);                    // taxa até /32

        for (int i = 0; i < n; ++i)
        {
            const float gi = gSm.next (k), cg = compSm.next (k), m = mixSm.next (k);
            const float dl = l[i], dr = r[i];
            float yl = dl, yr = dr;

            if (type == 3)
            {
                yl = std::round (dl * steps) / steps;
                yr = std::round (dr * steps) / steps;
            }
            else if (type == 4)
            {
                holdPh += holdInc;
                if (holdPh >= 1.0f)
                {
                    holdPh -= 1.0f;
                    holdL = dl;
                    holdR = dr;
                }
                yl = holdL;
                yr = holdR;
            }
            else
            {
                float a0, a1;
                os[0].upsample (dl * gi, a0, a1);
                yl = os[0].downsample (shape (type, a0), shape (type, a1)) * cg;
                os[1].upsample (dr * gi, a0, a1);
                yr = os[1].downsample (shape (type, a0), shape (type, a1)) * cg;
            }

            // tira o DC (válvula e assimétrico geram) e passa no tom
            const float hl = yl - dcX[0] + dcR * dcY[0];
            dcX[0] = yl; dcY[0] = hl;
            const float hr = yr - dcX[1] + dcR * dcY[1];
            dcX[1] = yr; dcY[1] = hr;
            yl = tone[0].process (hl);
            yr = tone[1].process (hr);

            l[i] = dl + (yl - dl) * m;
            r[i] = dr + (yr - dr) * m;
        }
    }

private:
    static float shape (int type, float v)
    {
        switch (type)
        {
            case 0:  return std::tanh (v + 0.25f) - 0.24491866f;               // válvula: tanh deslocada (assimetria suave)
            case 1:  return juce::jlimit (-1.0f, 1.0f, v);                     // duro
            case 2:  return std::sin (0.5f * kPi * v);                          // dobra senoidal
            case 5:                                                             // assimétrico: retificação parcial
            {
                const float t = std::tanh (v);
                return 0.55f * t + 0.45f * std::abs (t);
            }
            default: return v;
        }
    }

    double fs = 44100.0;
    Oversampler2x os[2];
    Biquad tone[2];
    float dcX[2] {}, dcY[2] {}, dcR = 0.999f;
    float holdL = 0.0f, holdR = 0.0f, holdPh = 1.0f;
    float lastToneHz = -1.0f, k = 0.001f;
    int lastType = -1;
    Smooth gSm, compSm, mixSm;
    bool primed = false;
};

// =====================================================================================
// PHASER: 6 passa-tudos de 1ª ordem, LFO exponencial, realimentação; LFO do direito a 90°
// =====================================================================================
class Phaser
{
public:
    static constexpr int nS = 6;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        k = smoothK (fs, 0.03);
        clear();
    }

    void clear()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int st = 0; st < nS; ++st) { x1[ch][st] = 0.0f; y1[ch][st] = 0.0f; }
            fbz[ch] = 0.0f;
        }
        ph = 0.0;
        primed = false;
    }

    void process (float* l, float* r, int n, const Snapshot& s, bool fresh)
    {
        if (fresh) clear();
        const auto& x = s.fx;
        const float rate  = juce::jlimit (0.01f, 40.0f, x.phsRate);
        const float depth = clamp01 (x.phsDepth / 100.0f);
        const float fb    = juce::jlimit (0.0f, 0.95f, x.phsFb / 100.0f);
        const float mix   = clamp01 (x.phsMix / 100.0f);
        if (! primed) { depthSm.snap (depth); fbSm.snap (fb); mixSm.snap (mix); primed = true; }
        depthSm.t = depth; fbSm.t = fb; mixSm.t = mix;

        const double inc  = (double) rate / fs;
        const float  fsf  = (float) fs;
        const float  fMax = 0.45f * fsf;
        float* io[2] = { l, r };

        for (int i = 0; i < n; ++i)
        {
            ph += inc;
            if (ph >= 1.0) ph -= 1.0;
            const float dep = depthSm.next (k), f = fbSm.next (k), m = mixSm.next (k);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float lfo = std::sin (kTwoPi * (float) ph + (ch ? 0.5f * kPi : 0.0f));
                const float hz  = juce::jlimit (20.0f, fMax, 700.0f * std::exp2 (lfo * 2.5f * dep));   // ±2,5 oitavas
                const float t   = std::tan (kPi * hz / fsf);
                const float a   = (t - 1.0f) / (t + 1.0f);
                const float dry = io[ch][i];
                float v = dry + f * fbz[ch];
                for (int st = 0; st < nS; ++st)
                {
                    const float y = a * v + x1[ch][st] - a * y1[ch][st];
                    x1[ch][st] = v;
                    y1[ch][st] = y;
                    v = y;
                }
                fbz[ch] = v;
                // 100% = seco e molhado meio a meio (entalhes mais fundos)
                io[ch][i] = dry + (v - dry) * 0.5f * m;
            }
        }
    }

private:
    double fs = 44100.0, ph = 0.0;
    float x1[2][nS] {}, y1[2][nS] {}, fbz[2] {};
    float k = 0.001f;
    Smooth depthSm, fbSm, mixSm;
    bool primed = false;
};

// =====================================================================================
// CHORUS: 3 vozes por lado, 7–25 ms moduladas; o direito defasado 90°
// =====================================================================================
class Chorus
{
public:
    static constexpr int nV = 3;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& d : line) d.prepare ((int) (fs * 0.030) + 8);
        for (int v = 0; v < nV; ++v) baseS[v] = (float) (baseMs[v] * 0.001 * fs);
        k = smoothK (fs, 0.03);
        clear();
    }

    void clear()
    {
        for (auto& d : line) d.clear();
        ph = 0.0;
        primed = false;
    }

    void process (float* l, float* r, int n, const Snapshot& s, bool fresh)
    {
        if (fresh) clear();
        const auto& x = s.fx;
        const float rate  = juce::jlimit (0.01f, 40.0f, x.choRate);
        const float depth = clamp01 (x.choDepth / 100.0f) * (float) (0.0045 * fs);   // até ±4,5 ms
        const float mix   = clamp01 (x.choMix / 100.0f);
        if (! primed) { depthSm.snap (depth); mixSm.snap (mix); primed = true; }
        depthSm.t = depth; mixSm.t = mix;
        const double inc = (double) rate / fs;
        const float  norm = 0.57735f;   // 1/√3

        for (int i = 0; i < n; ++i)
        {
            line[0].push (l[i]);
            line[1].push (r[i]);
            ph += inc;
            if (ph >= 1.0) ph -= 1.0;
            const float d = depthSm.next (k), m = mixSm.next (k);
            float wl = 0.0f, wr = 0.0f;
            for (int v = 0; v < nV; ++v)
            {
                const float p = (float) ph + (float) v / (float) nV;
                wl += line[0].readCubic (baseS[v] + d * std::sin (kTwoPi * p));
                wr += line[1].readCubic (baseS[v] + d * std::sin (kTwoPi * p + 0.5f * kPi));
            }
            l[i] += (wl * norm - l[i]) * m;
            r[i] += (wr * norm - r[i]) * m;
        }
    }

private:
    static constexpr double baseMs[nV] = { 11.5, 16.0, 20.5 };

    double fs = 44100.0, ph = 0.0;
    DelayLine line[2];
    float baseS[nV] {}, k = 0.001f;
    Smooth depthSm, mixSm;
    bool primed = false;
};

// =====================================================================================
// DELAY: sempre em sync; ping-pong; tom no laço (escuro ← neutro → fino)
// =====================================================================================
class Delay
{
public:
    static constexpr double maxSec = 6.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& d : line) d.prepare ((int) (fs * maxSec) + 8);
        maxS = (float) std::min (line[0].maxDelay(), (int) (fs * maxSec) + 4);
        k = smoothK (fs, 0.02);
        kTime = smoothK (fs, 0.12);
        clear();
    }

    void clear()
    {
        for (auto& d : line) d.clear();
        lp[0] = lp[1] = hp[0] = hp[1] = 0.0f;
        primed = false;
    }

    // tempo alvo (ms) no clock dado
    static double timeMs (const Snapshot& s, const Clock& clock)
    {
        const double bpm = juce::jlimit (20.0, 999.0, clock.bpm);
        const int    div = juce::jlimit (0, numDivs - 1, s.fx.dlyDiv);
        return std::min (maxSec, divBeats[div] * 60.0 / bpm) * 1000.0;
    }

    void process (float* l, float* r, int n, const Snapshot& s, const Clock& clock, bool fresh)
    {
        if (fresh) clear();
        const auto& x = s.fx;
        const float target = juce::jlimit (2.0f, maxS, (float) (timeMs (s, clock) * 0.001 * fs));
        const float fb     = juce::jlimit (0.0f, 0.95f, x.dlyFb / 100.0f);
        const float ping   = x.dlyPing ? 1.0f : 0.0f;
        const float mix    = clamp01 (x.dlyMix / 100.0f);
        const float tone   = juce::jlimit (0.0f, 100.0f, x.dlyTone);

        // abaixo de 50: passa-baixa desce até 800 Hz; acima: passa-alta sobe até 2,5 kHz (10 Hz no neutro, tira DC)
        const float lpC = tone >= 50.0f ? 1.0f : onePoleLp (fs, 20000.0 * std::pow (0.04, (50.0 - (double) tone) / 50.0));
        const float hpC = onePoleLp (fs, tone <= 50.0f ? 10.0 : 10.0 * std::pow (250.0, ((double) tone - 50.0) / 50.0));

        if (! primed)
        {
            timeCur = (double) target; fbSm.snap (fb); pingSm.snap (ping); mixSm.snap (mix);
            primed = true;
        }
        fbSm.t = fb; pingSm.t = ping; mixSm.t = mix;
        const double tgt = (double) target;

        for (int i = 0; i < n; ++i)
        {
            timeCur += (tgt - timeCur) * kTime;     // mudar o BPM desliza o tempo (sem estalo); double: chega no alvo
            const float d = (float) timeCur;
            const float f = fbSm.next (k), p = pingSm.next (k), m = mixSm.next (k);

            float o[2] = { line[0].readCubic (d), line[1].readCubic (d) };
            for (int ch = 0; ch < 2; ++ch)
            {
                lp[ch] += (o[ch] - lp[ch]) * lpC;
                hp[ch] += (lp[ch] - hp[ch]) * hpC;
                o[ch] = lp[ch] - hp[ch];
            }

            const float inL = l[i], inR = r[i];
            const float mono = 0.5f * (inL + inR);
            // ping-pong: entra tudo pela esquerda e a realimentação cruza os lados
            const float wL = (1.0f - p) * inL + p * mono + f * ((1.0f - p) * o[0] + p * o[1]);
            const float wR = (1.0f - p) * inR + f * ((1.0f - p) * o[1] + p * o[0]);
            line[0].push (safeSat (wL));
            line[1].push (safeSat (wR));

            float dg, wg;
            wetDry (m, dg, wg);
            l[i] = inL * dg + o[0] * wg;
            r[i] = inR * dg + o[1] * wg;
        }
    }

private:
    double fs = 44100.0;
    DelayLine line[2];
    float maxS = 1000.0f, k = 0.001f;
    double kTime = 0.001, timeCur = 1000.0;
    float lp[2] {}, hp[2] {};
    Smooth fbSm, pingSm, mixSm;
    bool primed = false;
};

// =====================================================================================
// REVERB: FDN de 8 linhas (Hadamard) com difusão na entrada, abafamento e modulação leve
// =====================================================================================
class Reverb
{
public:
    static constexpr int nL = 8;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxLine = (int) (fs * (baseMs[2][nL - 1] * 0.001 * 1.5 + 0.0015)) + 16;
        for (auto& ln : lines) ln.prepare (maxLine);
        for (auto& p : pre) p.prepare ((int) (fs * 0.03) + 8);
        for (int ch = 0; ch < 2; ++ch)
            for (int j = 0; j < 4; ++j)
                diff[ch][j].prepare ((int) (fs * apMs[j] * (ch ? 1.07 : 1.0) * 0.001));
        maxLen = (float) std::min (lines[0].maxDelay(), maxLine - 4);
        k = smoothK (fs, 0.03);
        sizeGlide = smoothK (fs, 0.25);
        cutC = (float) std::exp (-2.0 * kPiD * 90.0 / fs);
        clear();
    }

    void clear()
    {
        for (auto& ln : lines) ln.clear();
        for (auto& p : pre) p.clear();
        for (auto& side : diff)
            for (auto& a : side) a.clear();
        for (int i = 0; i < nL; ++i) { lp[i] = 0.0f; modPh[i] = (float) i / (float) nL; }
        cut1[0] = cut1[1] = cut2[0] = cut2[1] = 0.0f;
        primed = false;
    }

    void process (float* l, float* r, int n, const Snapshot& s, bool fresh)
    {
        if (fresh) clear();
        const auto& x = s.fx;
        const int   type = juce::jlimit (0, 2, x.revType);
        const float sf   = 0.55f + 0.95f * clamp01 (x.revSize / 100.0f);   // 0,55× a 1,5× das medidas
        const float rt   = juce::jlimit (0.2f, 20.0f, x.revDecay);
        const float mix  = clamp01 (x.revMix / 100.0f);
        const float dampHz = std::min ((float) (fs * 0.45),
                                       18000.0f * std::pow (0.065f, clamp01 (x.revDamp / 100.0f)) * dampMul[type]);
        const float lpC   = onePoleLp (fs, (double) dampHz);
        const float preS  = std::max (1.0f, preMs[type] * 0.001f * (float) fs);
        const float depth = modMs[type] * 0.001f * (float) fs;

        float target[nL];
        for (int i = 0; i < nL; ++i)
            target[i] = juce::jlimit (4.0f, maxLen - depth - 2.0f, (float) (baseMs[type][i] * 0.001 * fs) * sf);

        if (! primed)
        {
            for (int i = 0; i < nL; ++i) lenSm[i] = target[i];
            decSm = rt;
            diffG = diffT[type];
            depthSm.snap (depth);
            preSm.snap (preS);
            mixSm.snap (mix);
            primed = true;
        }
        const float kc = (float) (1.0 - std::exp (-(double) n / (0.05 * fs)));
        decSm += (rt - decSm) * kc;
        diffG += (diffT[type] - diffG) * kc;
        depthSm.t = depth; preSm.t = preS; mixSm.t = mix;

        // ganho de cada linha para cair 60 dB em RT60 (pelo comprimento atual)
        float g[nL];
        for (int i = 0; i < nL; ++i)
            g[i] = std::pow (10.0f, -3.0f * lenSm[i] / ((float) fs * decSm));

        const float modInc = 1.0f / (float) fs;
        for (int i = 0; i < n; ++i)
        {
            const float pd = preSm.next (k), dep = depthSm.next (k), m = mixSm.next (k);
            const float inL = l[i], inR = r[i];

            // pré-delay e difusão
            pre[0].push (inL);
            pre[1].push (inR);
            float dL = pre[0].readLin (pd), dR = pre[1].readLin (pd);
            for (int j = 0; j < 4; ++j)
            {
                dL = diff[0][j].process (dL, diffG);
                dR = diff[1][j].process (dR, diffG);
            }

            // 8 linhas: leitura modulada, abafamento, ganho de decaimento
            float y[nL], f[nL];
            for (int j = 0; j < nL; ++j)
            {
                lenSm[j] += (target[j] - lenSm[j]) * sizeGlide;   // tamanho/tipo esticam a sala, sem estalo
                modPh[j] += modHz[j] * modInc;
                if (modPh[j] >= 1.0f) modPh[j] -= 1.0f;
                const float len = lenSm[j] + dep * (0.5f + 0.5f * std::sin (kTwoPi * modPh[j]));
                y[j] = lines[j].readLin (len);
                lp[j] += (y[j] - lp[j]) * lpC;
                f[j] = lp[j] * g[j];
            }
            hadamard (f);
            for (int j = 0; j < nL; ++j)
                lines[j].push (safeSat (f[j] + ((j & 1) ? dR : dL) * 0.5f));

            float w[2] = { 0.5f * (y[0] - y[2] + y[4] + y[6]),
                           0.5f * (y[1] + y[3] - y[5] + y[7]) };
            // corte de grave no molhado (sem embolar)
            for (int ch = 0; ch < 2; ++ch)
            {
                cut1[ch] = cutC * cut1[ch] + (1.0f - cutC) * w[ch];
                cut2[ch] = cutC * cut2[ch] + (1.0f - cutC) * cut1[ch];
                w[ch] -= cut2[ch];
            }

            float dg, wg;
            wetDry (m, dg, wg);
            l[i] = inL * dg + w[0] * wg;
            r[i] = inR * dg + w[1] * wg;
        }
    }

private:
    // medidas base (ms) por tipo: Sala, Placa, Hall
    static constexpr double baseMs[3][nL] = {
        { 10.3, 12.9, 14.7, 17.1, 19.3, 21.9, 24.7, 27.3 },
        { 15.1, 18.7, 21.3, 24.1, 27.7, 30.1, 33.9, 36.7 },
        { 33.7, 39.1, 44.3, 49.9, 55.1, 61.3, 67.1, 73.7 } };
    static constexpr float  modHz[nL]  = { 0.13f, 0.21f, 0.29f, 0.37f, 0.47f, 0.59f, 0.71f, 0.89f };
    static constexpr double apMs[4]    = { 4.7, 3.6, 12.7, 9.3 };
    static constexpr float  diffT[3]   = { 0.60f, 0.75f, 0.65f };   // difusão (placa = mais densa)
    static constexpr float  preMs[3]   = { 3.0f, 1.0f, 15.0f };
    static constexpr float  modMs[3]   = { 0.12f, 0.20f, 0.45f };
    static constexpr float  dampMul[3] = { 1.0f, 1.25f, 0.9f };     // placa mais brilhante, hall mais macio

    static void hadamard (float* v)
    {
        for (int h = 1; h < nL; h *= 2)
            for (int i = 0; i < nL; i += 2 * h)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b;
                    v[j + h] = a - b;
                }
        for (int i = 0; i < nL; ++i) v[i] *= 0.35355339f;   // 1/√8: conserva a energia
    }

    double fs = 44100.0;
    DelayLine lines[nL];
    DelayLine pre[2];
    Allpass diff[2][4];
    float lenSm[nL] {}, lp[nL] {}, modPh[nL] {};
    float cut1[2] {}, cut2[2] {};
    float maxLen = 1000.0f, k = 0.001f, sizeGlide = 0.0001f, cutC = 0.99f;
    float decSm = 2.5f, diffG = 0.65f;
    Smooth depthSm, preSm, mixSm;
    bool primed = false;
};

// =====================================================================================
// EQ: shelf de graves (120 Hz), pico de médios (Q 0,9), shelf de agudos (8 kHz)
// =====================================================================================
class Eq
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        clear();
    }

    void clear()
    {
        for (int ch = 0; ch < 2; ++ch) { lo[ch].clear(); mid[ch].clear(); hi[ch].clear(); }
        primed = false;
        appLo = appMid = appHi = appF = -1000.0f;
    }

    void process (float* l, float* r, int n, const Snapshot& s, bool fresh)
    {
        if (fresh) clear();
        const auto& x = s.fx;
        const float tLo  = juce::jlimit (-18.0f, 18.0f, x.eqLow);
        const float tMid = juce::jlimit (-18.0f, 18.0f, x.eqMid);
        const float tHi  = juce::jlimit (-18.0f, 18.0f, x.eqHigh);
        const float tF   = juce::jlimit (20.0f, 20000.0f, x.eqMidF);

        if (! primed) { cLo = tLo; cMid = tMid; cHi = tHi; cF = tF; primed = true; }
        // aproximação curta (~20 ms) para os ganhos não pularem; coeficientes só quando mudam
        const float kc = (float) (1.0 - std::exp (-(double) n / (0.02 * fs)));
        cLo  += (tLo - cLo) * kc;
        cMid += (tMid - cMid) * kc;
        cHi  += (tHi - cHi) * kc;
        cF   *= std::pow (tF / cF, kc);
        if (std::abs (tLo - cLo) < 0.01f)   cLo = tLo;
        if (std::abs (tMid - cMid) < 0.01f) cMid = tMid;
        if (std::abs (tHi - cHi) < 0.01f)   cHi = tHi;
        if (std::abs (tF - cF) < tF * 1.0e-4f) cF = tF;

        if (cLo != appLo)
        {
            appLo = cLo;
            for (auto& b : lo) b.lowShelf (fs, 120.0, (double) cLo);
        }
        if (cMid != appMid || cF != appF)
        {
            appMid = cMid; appF = cF;
            for (auto& b : mid) b.peak (fs, (double) cF, 0.9, (double) cMid);
        }
        if (cHi != appHi)
        {
            appHi = cHi;
            for (auto& b : hi) b.highShelf (fs, std::min (8000.0, fs * 0.4), (double) cHi);
        }

        for (int i = 0; i < n; ++i)
        {
            l[i] = hi[0].process (mid[0].process (lo[0].process (l[i])));
            r[i] = hi[1].process (mid[1].process (lo[1].process (r[i])));
        }
    }

private:
    double fs = 44100.0;
    Biquad lo[2], mid[2], hi[2];
    float cLo = 0.0f, cMid = 0.0f, cHi = 0.0f, cF = 1000.0f;
    float appLo = -1000.0f, appMid = -1000.0f, appHi = -1000.0f, appF = -1000.0f;
    bool primed = false;
};

// =====================================================================================
// COMPRESSOR: Simples (feed-forward 4:1) ou Multibanda (3 bandas LR4, desce e sobe, estilo OTT)
// =====================================================================================
class Compressor
{
public:
    static constexpr int nB = 3;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const double f1 = std::min (120.0, fs * 0.2), f2 = std::min (2500.0, fs * 0.4);
        const double q = 0.70710678;
        for (int ch = 0; ch < 2; ++ch)
        {
            lp1[ch][0].lowpass (fs, f1, q);  lp1[ch][1].lowpass (fs, f1, q);
            hp1[ch][0].highpass (fs, f1, q); hp1[ch][1].highpass (fs, f1, q);
            lp2[ch][0].lowpass (fs, f2, q);  lp2[ch][1].lowpass (fs, f2, q);
            hp2[ch][0].highpass (fs, f2, q); hp2[ch][1].highpass (fs, f2, q);
            ap2[ch].allpass (fs, f2, q);   // LR4 LP + HP = passa-tudo de 2ª ordem: a banda grave acompanha a fase
        }
        attS = envCoef (fs, 5.0);
        relS = envCoef (fs, 120.0);
        for (int b = 0; b < nB; ++b)
        {
            attB[b] = envCoef (fs, bandAttMs[b]);
            relB[b] = envCoef (fs, bandRelMs[b]);
        }
        k = smoothK (fs, 0.02);
        clear();
    }

    void clear()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int j = 0; j < 2; ++j)
            {
                lp1[ch][j].clear(); hp1[ch][j].clear(); lp2[ch][j].clear(); hp2[ch][j].clear();
            }
            ap2[ch].clear();
        }
        envS = 0.0f;
        for (int b = 0; b < nB; ++b) envB[b] = 0.0f;
        primed = false;
    }

    // devolve a maior redução do pedaço (dB, ≥ 0)
    float process (float* l, float* r, int n, const Snapshot& s, bool fresh)
    {
        if (fresh) clear();
        const auto& x = s.fx;
        const float amt  = clamp01 (x.cmpAmt / 100.0f);
        const float mix  = clamp01 (x.cmpMix / 100.0f);
        const float mode = x.cmpMode == 1 ? 1.0f : 0.0f;
        if (! primed) { amtSm.snap (amt); mixSm.snap (mix); modeSm.snap (mode); primed = true; }
        amtSm.t = amt; mixSm.t = mix; modeSm.t = mode;

        float grMax = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = amtSm.next (k), m = mixSm.next (k), mw = modeSm.next (k);
            const float inL = l[i], inR = r[i];
            float sL = inL, sR = inR, mL = inL, mR = inR;

            if (mw < 1.0f)
            {
                // ---- simples: limiar de 0 a −36 dB, 4:1, ganho de compensação automático ----
                const float thr = -36.0f * a;
                const float makeup = -thr * 0.75f * 0.5f;
                const float lvl = std::max (std::abs (inL), std::abs (inR));
                envS = lvl > envS ? lvl + attS * (envS - lvl) : lvl + relS * (envS - lvl);
                const float gr = kneeReduction (gainToDb (envS), thr, 4.0f);
                grMax = std::max (grMax, gr * (1.0f - mw));
                const float gn = dbToGain (makeup - gr);
                sL = inL + (inL * gn - inL) * m;
                sR = inR + (inR * gn - inR) * m;
            }

            if (mw > 0.0f)
            {
                // ---- multibanda: separa em 3, cada banda desce acima de −16 dB e sobe abaixo de −40 dB ----
                float band[nB][2];
                const float in[2] = { inL, inR };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float lo   = lp1[ch][1].process (lp1[ch][0].process (in[ch]));
                    const float rest = hp1[ch][1].process (hp1[ch][0].process (in[ch]));
                    band[0][ch] = ap2[ch].process (lo);
                    band[1][ch] = lp2[ch][1].process (lp2[ch][0].process (rest));
                    band[2][ch] = hp2[ch][1].process (hp2[ch][0].process (rest));
                }
                float wL = 0.0f, wR = 0.0f, dL = 0.0f, dR = 0.0f;
                for (int b = 0; b < nB; ++b)
                {
                    const float bl = band[b][0], br = band[b][1];
                    const float lvl = std::max (std::abs (bl), std::abs (br));
                    float& e = envB[b];
                    e = lvl > e ? lvl + attB[b] * (e - lvl) : lvl + relB[b] * (e - lvl);
                    const float lDb  = gainToDb (e);
                    const float down = kneeReduction (lDb, -16.0f, 4.0f);
                    float up = 0.0f;
                    if (lDb < -40.0f)
                    {
                        const float gate = juce::jlimit (0.0f, 1.0f, (lDb + 70.0f) / 10.0f);   // não levanta o silêncio
                        up = std::min (18.0f, (-40.0f - lDb) * (2.0f / 3.0f)) * gate;
                    }
                    grMax = std::max (grMax, down * a * mw);
                    const float gn = dbToGain (a * (up - down + 3.0f));
                    wL += bl * gn; wR += br * gn;
                    dL += bl;      dR += br;
                }
                // o seco do MIX é a soma das bandas sem processar: mesma fase do molhado
                mL = dL + (wL - dL) * m;
                mR = dR + (wR - dR) * m;
            }

            l[i] = sL + (mL - sL) * mw;
            r[i] = sR + (mR - sR) * mw;
        }
        return grMax;
    }

private:
    static constexpr double bandAttMs[nB] = { 12.0, 6.0, 3.0 };
    static constexpr double bandRelMs[nB] = { 180.0, 120.0, 80.0 };

    double fs = 44100.0;
    Biquad lp1[2][2], hp1[2][2], lp2[2][2], hp2[2][2], ap2[2];
    float envS = 0.0f, envB[nB] {};
    float attS = 0.0f, relS = 0.0f, attB[nB] {}, relB[nB] {};
    float k = 0.001f;
    Smooth amtSm, mixSm, modeSm;
    bool primed = false;
};

// liga/desliga com crossfade de ~10 ms entre a entrada e a saída do efeito
struct Switch
{
    float g = 0.0f;
    bool  running = false, fresh = false;
};
} // namespace

// =====================================================================================
struct FxRack::Impl
{
    double fs = 44100.0;
    float  xfStep = 1.0f / 441.0f;
    bool   prepared = false;

    Hyper hyper;  Distortion dist;  Phaser phaser;  Chorus chorus;
    Delay delay;  Reverb reverb;    Eq eq;          Compressor comp;
    Switch swHyp, swDst, swPhs, swCho, swDly, swRev, swEq, swCmp;
    std::array<float, kChunk> dryL {}, dryR {};

    void prepare (double sampleRate)
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        xfStep = (float) (1.0 / (0.01 * fs));
        hyper.prepare (fs);  dist.prepare (fs);  phaser.prepare (fs);  chorus.prepare (fs);
        delay.prepare (fs);  reverb.prepare (fs); eq.prepare (fs);     comp.prepare (fs);
        resetSwitches();
        prepared = true;
    }

    void resetSwitches()
    {
        for (auto* sw : { &swHyp, &swDst, &swPhs, &swCho, &swDly, &swRev, &swEq, &swCmp })
            *sw = Switch();
    }

    void reset()
    {
        // os efeitos ligados voltam limpos e entram com o crossfade
        resetSwitches();
        hyper.clear(); dist.clear(); phaser.clear(); chorus.clear();
        delay.clear(); reverb.clear(); eq.clear(); comp.clear();
    }

    template <typename Fn>
    void stage (Switch& sw, bool on, float* l, float* r, int n, Fn&& fn)
    {
        if (on && ! sw.running) { sw.running = true; sw.fresh = true; }
        if (! sw.running) return;   // desligado: não gasta CPU
        const bool fresh = sw.fresh;
        sw.fresh = false;

        if (on && sw.g >= 1.0f) { fn (l, r, n, fresh); return; }

        std::copy (l, l + n, dryL.begin());
        std::copy (r, r + n, dryR.begin());
        fn (l, r, n, fresh);
        const float step = on ? xfStep : -xfStep;
        float g = sw.g;
        for (int i = 0; i < n; ++i)
        {
            g = juce::jlimit (0.0f, 1.0f, g + step);
            l[i] = dryL[(size_t) i] + (l[i] - dryL[(size_t) i]) * g;
            r[i] = dryR[(size_t) i] + (r[i] - dryR[(size_t) i]) * g;
        }
        sw.g = g;
        if (! on && g <= 0.0f) sw.running = false;   // terminou de sair: para de rodar
    }

    // devolve a maior redução do compressor no bloco (dB)
    float process (float* l, float* r, int numSamples, const Snapshot& s, const Clock& clock)
    {
        const auto& x = s.fx;
        float gr = 0.0f;
        for (int off = 0; off < numSamples; off += kChunk)
        {
            const int n = std::min (kChunk, numSamples - off);
            float* L = l + off;
            float* R = r + off;
            stage (swHyp, x.hypOn, L, R, n, [this, &s] (float* a, float* b, int m, bool f) { hyper.process (a, b, m, s, f); });
            stage (swDst, x.dstOn, L, R, n, [this, &s] (float* a, float* b, int m, bool f) { dist.process (a, b, m, s, f); });
            stage (swPhs, x.phsOn, L, R, n, [this, &s] (float* a, float* b, int m, bool f) { phaser.process (a, b, m, s, f); });
            stage (swCho, x.choOn, L, R, n, [this, &s] (float* a, float* b, int m, bool f) { chorus.process (a, b, m, s, f); });
            stage (swDly, x.dlyOn, L, R, n, [this, &s, &clock] (float* a, float* b, int m, bool f) { delay.process (a, b, m, s, clock, f); });
            stage (swRev, x.revOn, L, R, n, [this, &s] (float* a, float* b, int m, bool f) { reverb.process (a, b, m, s, f); });
            stage (swEq,  x.eqOn,  L, R, n, [this, &s] (float* a, float* b, int m, bool f) { eq.process (a, b, m, s, f); });
            stage (swCmp, x.cmpOn, L, R, n, [this, &s, &gr] (float* a, float* b, int m, bool f)
                   { gr = std::max (gr, comp.process (a, b, m, s, f)); });
        }
        return gr;
    }
};

// =====================================================================================
FxRack::FxRack() : impl (std::make_unique<Impl>()) {}
FxRack::~FxRack() = default;

void FxRack::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);   // processa em pedaços internos de 256: qualquer bloco serve
    impl->prepare (sampleRate);
    compGainReductionDb.store (0.0f);
    delayTimeMs.store (0.0f);
}

void FxRack::reset()
{
    impl->reset();
    compGainReductionDb.store (0.0f);
}

void FxRack::process (float* left, float* right, int numSamples, const Snapshot& s, const Clock& clock)
{
    delayTimeMs.store ((float) Delay::timeMs (s, clock));
    if (! impl->prepared || left == nullptr || right == nullptr || numSamples <= 0)
        return;
    const float gr = impl->process (left, right, numSamples, s, clock);
    compGainReductionDb.store (s.fx.cmpOn ? std::max (0.0f, gr) : 0.0f);
}
} // namespace quimera
