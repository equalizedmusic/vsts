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

    // ---- versão "casada" (magnitude igual à do circuito analógico até perto de Nyquist; método de Vicanek):
    // polos por invariância ao impulso; zeros escolhidos para a resposta bater com a analógica em 0 Hz, em Nyquist e na
    // frequência do filtro. A transformada bilinear (RBJ) esmaga o agudo: um shelf em 16 kHz a 48 kHz sai pela metade.
    // protótipo analógico normalizado: H(s) = (n2 s² + n1 s + n0) / (d2 s² + d1 s + d0), s = jΩ/ω0
    void matched (double fs, double f0, double n2, double n1, double n0, double d2, double d1, double d0)
    {
        const double W = 2 * pi * f0 / fs;
        auto mag2 = [&] (double w)   // |H|² analógico na frequência digital w
        {
            const double o = w / W;
            const double nr = n0 - n2 * o * o, ni = n1 * o, dr = d0 - d2 * o * o, di = d1 * o;
            return (nr * nr + ni * ni) / (dr * dr + di * di);
        };
        const double wnT = W * std::sqrt (d0 / d2), zeta = (d1 / d2) / (2 * std::sqrt (d0 / d2));
        const double e = std::exp (-zeta * wnT);
        const double ca1 = zeta <= 1 ? -2 * e * std::cos (wnT * std::sqrt (1 - zeta * zeta))
                                     : -2 * e * std::cosh (wnT * std::sqrt (zeta * zeta - 1));
        const double ca2 = e * e;
        const double A0 = (1 + ca1 + ca2) * (1 + ca1 + ca2), A1 = (1 - ca1 + ca2) * (1 - ca1 + ca2), A2 = -4 * ca2;
        const double wm = std::min (W, 0.5 * pi);                 // terceiro ponto: a frequência do filtro (até fs/4)
        const double p1 = std::sin (wm / 2) * std::sin (wm / 2), p0 = 1 - p1, p2 = 4 * p0 * p1;
        const double B0 = A0 * mag2 (0.0), B1 = A1 * mag2 (pi);
        const double B2 = (mag2 (wm) * (A0 * p0 + A1 * p1 + A2 * p2) - B0 * p0 - B1 * p1) / p2;
        const double W0 = std::sqrt (std::max (0.0, B0)), W1 = std::sqrt (std::max (0.0, B1));
        const double S = (W0 + W1) / 2;
        const double nb0 = (S + std::sqrt (std::max (0.0, S * S + B2))) / 2;
        b0 = nb0; b1 = (W0 - W1) / 2; b2 = S - nb0; a1 = ca1; a2 = ca2;
    }
    // sino (RBJ analógico: A = ganho^½; o Q dos polos encolhe no corte, como no circuito)
    void peakM (double fs, double f, double q, double db)
    {
        if (std::abs (db) < 0.01) { flat(); return; }
        const double A = std::pow (10.0, db / 40.0);
        matched (fs, std::min (f, fs * 0.49), 1, A / q, 1, 1, 1 / (A * q), 1);
    }
    void lowShelfM (double fs, double f, double q, double db)
    {
        if (std::abs (db) < 0.01) { flat(); return; }
        const double A = std::pow (10.0, db / 40.0), sA = std::sqrt (A);
        matched (fs, f, A, A * sA / q, A * A, A, sA / q, 1);
    }
    void highShelfM (double fs, double f, double q, double db)
    {
        if (std::abs (db) < 0.01) { flat(); return; }
        const double A = std::pow (10.0, db / 40.0), sA = std::sqrt (A);
        matched (fs, std::min (f, fs * 0.49), A * A, A * sA / q, A, 1, sA / q, A);
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

// Saturação do pré: roda na taxa sobreamostrada (o processador sobe 4× no HQ).
// DRIVE = quanto o sinal entra quente no circuito (0..10 = +0..+30 dB); a saída devolve o ganho de pequeno sinal,
// então o nível fica parecido e o que muda é o caráter: harmônicas, grave que encorpa, transiente que arredonda.
// Régua: 0 VU = -18 dBFS; com DRIVE 0 o circuito tem ~20 dB de folga (limpo); DRIVE 5 já colore; DRIVE 10 rasga.
//  LIMPO   (pré de console britânica, VCA/transistor): limpo até perto do topo, depois 3ª harmônica e clipe macio.
//  API 312 (operacional discreto 2520 + transformador): joelho duro, 3ª forte com um pouco de 2ª, transiente
//          "achatado" com punch; o transformador de saída engorda o grave baixo.
//  BRIT 73 (classe A + transformadores Marinair): 2ª harmônica desde cedo (assimetria do classe A), o grave satura
//          ANTES no núcleo do transformador (fica mais gordo, com harmônicas, sem sumir), agudo amacia com drive.
struct PreSat
{
    PreModel model = PreModel::limpo;
    float drive = 1.0f;          // ganho para dentro do circuito (linear)
    float hot = 1.0f;            // folga do circuito: quanto 0 VU fica perto do ponto de saturação
    float makeup = 1.0f;         // o drive devolve um pouco de nível (o pré "abre" quando esquenta)
    Biquad emph, deemph;         // transformador: o grave entra mais quente na curva e é devolvido depois (linear = plano)
    OnePole soft;                // BRIT 73: agudo que amacia conforme o drive
    OnePole dc;
    float softMix = 0.0f;

    void prepare (double fsOver)
    {
        dc.lowPass (fsOver, 5.0); dc.reset();
        soft.lowPass (fsOver, 14000.0); soft.reset();
        emph.reset(); deemph.reset();
        if (model == PreModel::brit)     { emph.lowShelf (fsOver, 150.0, 0.6, 6.0);  deemph.lowShelf (fsOver, 150.0, 0.6, -6.0); }
        else if (model == PreModel::api) { emph.lowShelf (fsOver, 90.0, 0.6, 3.0);   deemph.lowShelf (fsOver, 90.0, 0.6, -3.0); }
        else                             { emph.flat(); deemph.flat(); }
        hot = model == PreModel::brit ? 0.42f : model == PreModel::api ? 0.8f : 1.0f;
    }
    void setDrive (float knob)   // 0..10 = +0..+20 dB para dentro do circuito
    {
        knob = std::max (0.0f, std::min (10.0f, knob));
        drive = dbToGain (knob * 2.0f);
        makeup = dbToGain (knob * 0.3f);
        softMix = model == PreModel::brit ? knob / 10.0f * 0.5f : 0.0f;
    }

    // tanh com bias (assimetria => 2ª harmônica), inclinação 1 em zero
    static float asym (float x, float bias)
    {
        const float t0 = std::tanh (bias), d0 = 1.0f - t0 * t0;
        return (std::tanh (x + bias) - t0) / d0;
    }
    // clipe de joelho ajustável: p alto = mais duro (inclinação 1 em zero, teto 1)
    static float knee (float x, float p) { return x / std::pow (1.0f + std::pow (std::abs (x), p), 1.0f / p); }

    float process (int ch, float x)
    {
        const float k = drive * hot;
        float u = x * k;
        float y;
        switch (model)
        {
            case PreModel::limpo:
                // folga grande; só o pico encosta no trilho
                y = knee (u * 0.85f, 3.2f) / 0.85f;
                break;
            case PreModel::api:
            {
                u = (float) emph.process (ch, u);
                // 2520: um pouco de 2ª, joelho duro de operacional
                y = knee (u * 1.1f + 0.05f * u * u, 2.4f) / 1.1f;
                y = (float) deemph.process (ch, y);
                break;
            }
            default:
            {
                // transformador de entrada: o grave encosta antes
                u = (float) emph.process (ch, u);
                const float t = knee (u * 0.9f, 2.0f) / 0.9f;
                // classe A: assimetria que dá 2ª harmônica já em nível moderado
                y = asym (t * 1.3f, 0.16f) / 1.3f;
                y = (float) deemph.process (ch, y);
                // agudo arredonda com o drive (o transformador de saída carregado)
                if (softMix > 0.0f) y += softMix * (soft.lp (ch, y) - y);
                break;
            }
        }
        y = dc.hp (ch, y);
        return y * makeup / k;
    }
    void reset() { emph.reset(); deemph.reset(); soft.reset(); dc.reset(); }
};

// cor fixa do circuito (fora da sobreamostragem): o "som de prateleira" de cada pré, sutil
struct PreColour
{
    Biquad a, b;
    void set (double fs, PreModel m)
    {
        if (m == PreModel::api)       { a.lowShelfM (fs, 60.0, 0.9, 0.8);  b.peakM (fs, 3000.0, 0.5, 0.6); }    // grave firme, presença
        else if (m == PreModel::brit) { a.peakM (fs, 110.0, 0.7, 0.9);     b.highShelfM (fs, 18000.0, 0.7, -0.8); }   // corpo, topo macio
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
        // filtros "casados" (Biquad::matched): a curva é a do circuito até o fim do agudo, sem o esmagamento da bilinear
        if (m == EqModel::e4k)
        {
            // Q proporcional (como no EQ preto da console britânica): pouco ganho = largo e musical, muito = cirúrgico
            auto qOf = [] (float g) { return 0.6 + std::pow (std::abs (g) / 15.0, 1.3) * 2.2; };
            band[0].highShelfM (fs, s.f[0], 0.8, s.g[0]);   // shelf com o leve afundado antes da subida
            band[1].peakM (fs, s.f[1], qOf (s.g[1]), s.g[1]);
            band[2].peakM (fs, s.f[2], qOf (s.g[2]), s.g[2]);
            band[3].lowShelfM (fs, s.f[3], 0.8, s.g[3]);
            drive = 0.0f;
        }
        else
        {
            band[0].highShelfM (fs, brit81Snap (0, s.f[0]), 0.6, s.g[0]);
            band[1].peakM (fs, brit81Snap (1, s.f[1]), 0.7, s.g[1]);
            band[2].peakM (fs, brit81Snap (2, s.f[2]), 0.7, s.g[2]);
            band[3].lowShelfM (fs, brit81Snap (3, s.f[3]), 1.2, s.g[3]);   // calombo/afundado do indutor
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
            const float k = 0.1f + 0.2f * drive;             // saturação leve do indutor em nível alto
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
