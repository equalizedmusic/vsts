#pragma once
// MESA — processamento de um canal, em C++ puro (sem JUCE: dá para medir fora do plugin, ver tools/teste_dsp.cpp).
// Nível de referência: -18 dBFS = 0 VU, a régua das consoles (0 VU = +4 dBu; clipa perto de +24 dBu ≈ +2 dBFS).
//
// PRÉ      LIMPO (sem cor) · API 312 (operacional discreto + transformador: 3ª harmônica, ataque firme)
//          BRIT 73 (classe A + transformador: 2ª harmônica, grave que engorda primeiro)
// EQ       E-4K (console britânica de 4 bandas, Q proporcional ao ganho) · BRIT 81 (frequências em degraus, Q largo,
//          prateleira de grave com o "calombo" do indutor)
// DINÂMICA VCA do canal (THR/RAT + gate) · COLA 4K (compressor de barramento: grupos e MASTER)

#include <algorithm>
#include <cmath>

namespace mesa
{
constexpr double pi = 3.14159265358979323846;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// ------------------------------------------------------------------------------------------------ biquad (RBJ)
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};

    float process (int ch, float x)
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
    void flat() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void peak (double fs, double f, double q, double db)
    {
        if (std::abs (db) < 0.01) { flat(); return; }
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * std::min (f, fs * 0.49) / fs, al = std::sin (w) / (2 * q), c = std::cos (w);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    // q = 0.707: prateleira sem calombo; q maior = com o calombo/afundado do lado de lá (estilo indutor)
    void lowShelf (double fs, double f, double q, double db)
    {
        if (std::abs (db) < 0.01) { flat(); return; }
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * std::min (f, fs * 0.49) / fs, c = std::cos (w);
        const double al = std::sin (w) / (2 * q), s = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
             (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highShelf (double fs, double f, double q, double db)
    {
        if (std::abs (db) < 0.01) { flat(); return; }
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w);
        const double al = std::sin (w) / (2 * q), s = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
             (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
    void highPass (double fs, double f, double q)
    {
        const double w = 2 * pi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
};

struct OnePole   // passa-alta/baixa de 1ª ordem
{
    double a = 0, s[2] {};
    void lowPass (double fs, double f) { a = 1.0 - std::exp (-2 * pi * f / fs); }
    float lp (int ch, float x) { s[ch] += a * (x - s[ch]); return (float) s[ch]; }
    float hp (int ch, float x) { return x - lp (ch, x); }
    void reset() { s[0] = s[1] = 0; }
};

// ------------------------------------------------------------------------------------------------ PRÉ
enum class PreModel { limpo = 0, api = 1, brit = 2 };

// saturação: roda na taxa sobreamostrada (o processador sobe 4× no HQ). ganho de pequeno sinal = 1 em todos os modelos.
struct PreSat
{
    PreModel model = PreModel::limpo;
    float drive = 1.0f;          // ganho para dentro da curva (o processador devolve 1/drive depois)
    OnePole lowBand;             // BRIT 73: o grave satura antes (núcleo do transformador)
    OnePole dc;

    void prepare (double fsOver) { lowBand.lowPass (fsOver, 140.0); dc.lowPass (fsOver, 4.0); lowBand.reset(); dc.reset(); }

    static float curve (float x, float k, float bias)
    {
        // tanh assimétrica normalizada: inclinação 1 em zero, satura em ~1/k; o bias cria a 2ª harmônica
        const float t0 = std::tanh (k * bias), d0 = k * (1.0f - t0 * t0);
        return (std::tanh (k * (x + bias)) - t0) / d0;
    }

    float process (int ch, float x)
    {
        if (model == PreModel::limpo)
            return x;
        x *= drive;
        float y;
        if (model == PreModel::api)
        {
            // operacional discreto: joelho mais duro (3ª), pouca assimetria
            y = curve (x, 0.6f, 0.05f);
        }
        else
        {
            // classe A: assimetria forte (2ª); o grave entra mais quente no transformador
            const float lo = lowBand.lp (ch, x);
            const float e = 0.45f * lo;
            y = curve (x + e, 0.5f, 0.14f) - e;
        }
        y = dc.hp (ch, y);
        return y / drive;
    }
};

// cor fixa do circuito (fora da sobreamostragem): o "som de prateleira" de cada pré, sutil
struct PreColour
{
    Biquad a, b;
    void set (double fs, PreModel m)
    {
        if (m == PreModel::api)       { a.lowShelf (fs, 70.0, 0.7, 0.6);  b.peak (fs, 2800.0, 0.6, 0.4); }
        else if (m == PreModel::brit) { a.lowShelf (fs, 60.0, 0.8, 0.9);  b.highShelf (fs, 13000.0, 0.6, 0.7); }
        else                          { a.flat(); b.flat(); }
    }
    float process (int ch, float x) { return b.process (ch, a.process (ch, x)); }
    void reset() { a.reset(); b.reset(); }
};

// passa-alta de 18 dB/oitava (como nos canais de console)
struct Hpf18
{
    Biquad bq; OnePole p1; bool on = false;
    void set (double fs, float hz)
    {
        on = hz > 20.5f;
        if (on) { bq.highPass (fs, hz, 1.0); p1.lowPass (fs, hz); }
    }
    float process (int ch, float x) { return on ? p1.hp (ch, bq.process (ch, x)) : x; }
    void reset() { bq.reset(); p1.reset(); }
};

// ------------------------------------------------------------------------------------------------ EQ
enum class EqModel { e4k = 0, brit81 = 1 };

struct EqSettings { float g[4] {}, f[4] { 8000, 2500, 500, 100 }; };   // HF, HMF, LMF, LF

// BRIT 81: as frequências andam em degraus (como a chave rotativa do original)
inline const float* brit81Steps (int band, int& n)
{
    static const float hf[]  = { 8000, 10000, 12000, 16000 };
    static const float hmf[] = { 700, 1600, 3200, 4800, 7200 };
    static const float lmf[] = { 220, 360, 700, 1600 };
    static const float lf[]  = { 35, 60, 110, 220 };
    switch (band) { case 0: n = 4; return hf; case 1: n = 5; return hmf; case 2: n = 4; return lmf; default: n = 4; return lf; }
}
inline float brit81Snap (int band, float hz)
{
    int n; const float* s = brit81Steps (band, n);
    float best = s[0];
    for (int i = 1; i < n; ++i)
        if (std::abs (std::log (hz / s[i])) < std::abs (std::log (hz / best))) best = s[i];
    return best;
}

struct Eq
{
    Biquad band[4];
    EqModel model = EqModel::e4k;
    float drive = 0.0f;   // BRIT 81: o indutor satura um pouco com reforço

    void set (double fs, EqModel m, const EqSettings& s)
    {
        model = m;
        if (m == EqModel::e4k)
        {
            // Q proporcional: pouco ganho = largo, muito ganho = estreito
            auto qOf = [] (float g) { return 0.55 + std::abs (g) / 15.0 * 1.6; };
            band[0].highShelf (fs, s.f[0], 0.707, s.g[0]);
            band[1].peak (fs, s.f[1], qOf (s.g[1]), s.g[1]);
            band[2].peak (fs, s.f[2], qOf (s.g[2]), s.g[2]);
            band[3].lowShelf (fs, s.f[3], 0.707, s.g[3]);
            drive = 0.0f;
        }
        else
        {
            band[0].highShelf (fs, brit81Snap (0, s.f[0]), 0.6, s.g[0]);
            band[1].peak (fs, brit81Snap (1, s.f[1]), 0.8, s.g[1]);
            band[2].peak (fs, brit81Snap (2, s.f[2]), 0.8, s.g[2]);
            band[3].lowShelf (fs, brit81Snap (3, s.f[3]), 1.15, s.g[3]);   // calombo/afundado do indutor
            float boost = 0.0f;
            for (float g : s.g) boost += std::max (0.0f, g);
            drive = std::min (1.0f, boost / 30.0f);
        }
    }
    float process (int ch, float x)
    {
        for (auto& b : band) x = b.process (ch, x);
        if (drive > 0.0f)
        {
            const float k = 0.15f + 0.35f * drive;           // saturação leve do indutor em nível alto
            x = std::tanh (k * x) / k;
        }
        return x;
    }
    void reset() { for (auto& b : band) b.reset(); }
};

// ------------------------------------------------------------------------------------------------ dinâmica
inline float coef (double fs, double ms) { return (float) std::exp (-1.0 / (fs * ms * 0.001)); }

// compressor feed-forward no domínio de dB; joelho suave
inline float gainComputer (float inDb, float thr, float ratio, float knee)
{
    const float over = inDb - thr;
    if (2 * over < -knee) return 0.0f;
    if (2 * std::abs (over) <= knee) { const float t = over + knee / 2; return (1 / ratio - 1) * t * t / (2 * knee); }
    return (1 / ratio - 1) * over;
}

// VCA do canal: ataque rápido, relaxamento automático (rápido em transiente, lento em sustentado)
struct ChannelComp
{
    float thr = 10.0f, ratio = 4.0f;
    float env = 0.0f, slow = 0.0f, grDb = 0.0f;
    float atk = 0, relF = 0, relS = 0;
    void prepare (double fs) { atk = coef (fs, 3.0); relF = coef (fs, 90.0); relS = coef (fs, 900.0); env = slow = grDb = 0; }
    bool active() const { return thr < 9.9f && ratio > 1.01f; }
    // devolve o ganho (linear) para a amostra; det = pico do estéreo
    float gain (float det)
    {
        const float target = -gainComputer (gainToDb (det), thr, ratio, 6.0f);   // dB de redução (>= 0)
        // "slow" acompanha a média longa da redução: o relaxamento volta rápido até ela e devagar daí para baixo
        slow = relS * slow + (1 - relS) * target;
        if (target > env)       env = atk * env + (1 - atk) * target;
        else if (env > slow)    env = relF * env + (1 - relF) * std::max (target, slow);
        else                    env = relS * env + (1 - relS) * target;
        grDb = env;
        return dbToGain (-env);
    }
};

// gate/expansor do canal (GATE no mínimo = desligado)
struct Gate
{
    float thr = -80.0f, gain = 1.0f, hold = 0;
    float atk = 0, rel = 0; int holdN = 0;
    void prepare (double fs) { atk = coef (fs, 0.4); rel = coef (fs, 120.0); holdN = (int) (fs * 0.03); gain = 1; hold = 0; }
    bool active() const { return thr > -79.5f; }
    float process (float det)
    {
        const float d = gainToDb (det);
        float target = 1.0f;
        if (d > thr) hold = (float) holdN;
        else if (hold > 0) hold -= 1;
        else target = dbToGain (std::max (-40.0f, (d - thr) * 3.0f));   // expansão 1:4, até -40 dB
        gain = target > gain ? atk * gain + (1 - atk) * target : rel * gain + (1 - rel) * target;
        return gain;
    }
};

// COLA 4K: compressor de barramento VCA (estéreo ligado). Ataque 0,1–30 ms, relaxamento 0,1–1,2 s ou AUTO.
struct BusComp
{
    float thr = 0.0f, ratio = 4.0f, makeup = 0.0f, mix = 1.0f;
    float atkMs = 10.0f, relS = -1.0f;   // relS < 0 = AUTO
    bool scHpf = false;
    double fs = 48000;
    float env = 0, slow = 0, grDb = 0;
    float cA = 0, cR = 0, cF = 0, cS = 0;
    Biquad hp;
    void prepare (double rate) { fs = rate; env = slow = grDb = 0; hp.highPass (fs, 90.0, 0.707); hp.reset(); update(); }
    void update()
    {
        cA = coef (fs, atkMs);
        cR = coef (fs, std::max (0.05f, relS) * 1000.0);
        cF = coef (fs, 100.0); cS = coef (fs, 1500.0);   // AUTO: dois tempos, o lento só pega sustentação longa
    }
    void process (float& l, float& r)
    {
        float dl = l, dr = r;
        if (scHpf) { dl = hp.process (0, l); dr = hp.process (1, r); }
        const float det = std::max (std::abs (dl), std::abs (dr));
        const float target = -gainComputer (gainToDb (det), thr, ratio, 4.0f);
        if (target > env) env = cA * env + (1 - cA) * target;
        else if (relS >= 0) env = cR * env + (1 - cR) * target;
        else
        {
            slow = slow < target ? target : cS * slow + (1 - cS) * target;
            const float floor_ = slow * 0.6f;
            env = cF * env + (1 - cF) * std::max (target, floor_);
        }
        grDb = env;
        const float g = dbToGain (makeup - env);
        l = l * (1 - mix) + l * g * mix;
        r = r * (1 - mix) + r * g * mix;
    }
};

// ------------------------------------------------------------------------------------------------ pan (balanço, centro = 0 dB)
inline void panGains (float p, float& gl, float& gr)
{
    gl = p > 0 ? std::cos (p * (float) pi * 0.5f) : 1.0f;
    gr = p < 0 ? std::cos (-p * (float) pi * 0.5f) : 1.0f;
}
} // namespace mesa
