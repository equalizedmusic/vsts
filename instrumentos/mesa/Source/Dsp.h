#pragma once
// MESA — processamento de um canal, em C++ puro (sem JUCE: dá para medir fora do plugin, ver tools/teste_dsp.cpp).
// Nível de referência: -18 dBFS = 0 VU, a régua das consoles (0 VU = +4 dBu; clipa perto de +24 dBu ≈ +2 dBFS).
//
// PRÉ      E-4K (pré de console britânica a transistor: limpo, firme, odd no topo) · API 312 (operacional discreto
//          + transformador: 3ª harmônica, ataque firme) · BRIT 73 (classe A + transformador: 2ª harmônica, grave que
//          engorda primeiro). Cada cópia tem uma pequena diferença entre L e R, como dois canais de hardware.
// EQ       E-4K (console britânica de 4 bandas, Q no knob) · BRIT 81 (frequências em degraus, Q largo / ALTO, prateleira
//          de grave com o "calombo" do indutor) · API 550 (degraus de frequência e de ganho, Q proporcional ao ganho)
// DINÂMICA VCA 4K (compressor do canal da console) · COLA 4K (compressor de barramento) · FET 76 (FET, ataque em µs,
//          razões 4/8/12/20 e ALL) · OPTO 2A (célula ótica com válvula: ataque 10 ms, soltura em dois tempos e memória)

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
    void lowPassM (double fs, double f, double q) { matched (fs, std::min (f, fs * 0.45), 0, 0, 1, 1, 1 / q, 1); }

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
enum class PreModel { limpo = 0, api = 1, brit = 2 };   // limpo = o E-4K (o nome da 0.4 ficou no código)

// Saturação do pré: roda na taxa sobreamostrada (o processador sobe 4× no HQ).
// DRIVE = quanto o sinal entra quente no circuito (0..10 = +0..+30 dB); a saída devolve o ganho de pequeno sinal,
// então o nível fica parecido e o que muda é o caráter: harmônicas, grave que encorpa, transiente que arredonda.
// Régua: 0 VU = -18 dBFS; com DRIVE 0 o circuito tem ~20 dB de folga (limpo); DRIVE 5 já colore; DRIVE 10 rasga.
//  E-4K    (pré de console britânica, transistor): limpo até perto do topo, um fio de 2ª, depois 3ª e clipe macio.
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
    float tol[2] { 1.0f, 1.0f };  // tolerância dos componentes: L e R nunca são idênticos (0,97..1,03)

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
                // folga grande; só o pico encosta no trilho (um fio de 2ª do par diferencial)
                y = knee (u * 0.85f + 0.004f * tol[ch] * u * u, 3.2f) / 0.85f;
                break;
            case PreModel::api:
            {
                u = (float) emph.process (ch, u);
                // 2520: um pouco de 2ª, joelho duro de operacional
                y = knee (u * 1.1f + 0.05f * tol[ch] * u * u, 2.4f) / 1.1f;
                y = (float) deemph.process (ch, y);
                break;
            }
            default:
            {
                // transformador de entrada: o grave encosta antes
                u = (float) emph.process (ch, u);
                const float t = knee (u * 0.9f, 2.0f) / 0.9f;
                // classe A: assimetria que dá 2ª harmônica já em nível moderado
                y = asym (t * 1.3f, 0.16f * tol[ch]) / 1.3f;
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
    Biquad a[2], b[2];   // um por lado: a tolerância desloca um pouco a frequência de L e de R
    void set (double fs, PreModel m, const float* tol)
    {
        for (int c = 0; c < 2; ++c)
        {
            const double t = tol[c];
            if (m == PreModel::api)       { a[c].lowShelfM (fs, 60.0 * t, 0.9, 0.8);  b[c].peakM (fs, 3000.0 * t, 0.5, 0.6); }     // grave firme, presença
            else if (m == PreModel::brit) { a[c].peakM (fs, 110.0 * t, 0.7, 0.9);     b[c].highShelfM (fs, 18000.0 * t, 0.7, -0.8); } // corpo, topo macio
            else                          { a[c].lowShelfM (fs, 28.0 * t, 0.7, -0.6); b[c].highShelfM (fs, 11000.0 * t, 0.7, 0.4); } // fundo justo, topo aberto
        }
    }
    float process (int ch, float x) { return b[ch].process (ch, a[ch].process (ch, x)); }
    void reset() { for (int c = 0; c < 2; ++c) { a[c].reset(); b[c].reset(); } }
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

// passa-baixa de 12 dB/oitava (o LPF do canal da console), casado: corta igual até perto de Nyquist
struct Lpf12
{
    Biquad bq; bool on = false;
    void set (double fs, float hz)
    {
        on = hz < 19900.0f;
        if (on) bq.lowPassM (fs, hz, 0.707);
    }
    float process (int ch, float x) { return on ? bq.process (ch, x) : x; }
    void reset() { bq.reset(); }
};

// ------------------------------------------------------------------------------------------------ EQ
enum class EqModel { e4k = 0, brit81 = 1, api550 = 2 };

// HF, HMF, LMF, LF; q só vale nas médias (1 e 2); bell[0] = HF em sino, bell[1] = LF em sino
struct EqSettings { float g[4] {}, f[4] { 8000, 2500, 500, 100 }, q[4] { 1, 1, 1, 1 }; bool bell[2] {}; };

// BRIT 81 e API 550: as frequências andam em degraus (como a chave rotativa do original)
inline const float* eqSteps (EqModel m, int band, int& n)
{
    static const float hf[]  = { 8000, 10000, 12000, 16000 };
    static const float hmf[] = { 700, 1600, 3200, 4800, 7200 };
    static const float lmf[] = { 220, 360, 700, 1600 };
    static const float lf[]  = { 35, 60, 110, 220 };
    static const float ahf[]  = { 2500, 5000, 7000, 10000, 12500, 15000 };
    static const float ahmf[] = { 800, 1500, 3000, 5000, 8000 };
    static const float almf[] = { 180, 240, 500, 700, 1000 };
    static const float alf[]  = { 30, 40, 50, 100, 200, 300, 400 };
    if (m == EqModel::api550)
        switch (band) { case 0: n = 6; return ahf; case 1: n = 5; return ahmf; case 2: n = 5; return almf; default: n = 7; return alf; }
    switch (band) { case 0: n = 4; return hf; case 1: n = 5; return hmf; case 2: n = 4; return lmf; default: n = 4; return lf; }
}
inline float eqSnapF (EqModel m, int band, float hz)
{
    if (m == EqModel::e4k) return hz;
    int n; const float* s = eqSteps (m, band, n);
    float best = s[0];
    for (int i = 1; i < n; ++i)
        if (std::abs (std::log (hz / s[i])) < std::abs (std::log (hz / best))) best = s[i];
    return best;
}
// API 550: o ganho também anda em degraus (chave de 2, 4, 6, 9, 12, 15 dB)
inline float eqSnapG (EqModel m, float db)
{
    if (m != EqModel::api550) return db;
    static const float st[] = { 0, 2, 4, 6, 9, 12, 15 };
    const float a = std::abs (db);
    float best = 0;
    for (float s : st) if (std::abs (a - s) < std::abs (a - best)) best = s;
    return db < 0 ? -best : best;
}
// BRIT 81: Q das médias em duas posições (normal / ALTO)
inline float brit81Q (float q) { return q < 1.4f ? 0.7f : 1.9f; }

struct Eq
{
    Biquad band[4];
    EqModel model = EqModel::e4k;
    float drive = 0.0f;   // BRIT 81 / API 550: o indutor / o operacional satura um pouco com reforço

    void set (double fs, EqModel m, const EqSettings& s)
    {
        model = m;
        float g[4], f[4];
        for (int b = 0; b < 4; ++b) { g[b] = eqSnapG (m, s.g[b]); f[b] = eqSnapF (m, b, s.f[b]); }
        // filtros "casados" (Biquad::matched): a curva é a do circuito até o fim do agudo, sem o esmagamento da bilinear
        if (m == EqModel::e4k)
        {
            // Q das médias no knob (EQ marrom); shelf com o leve afundado antes da subida; BELL = sino largo
            if (s.bell[0]) band[0].peakM (fs, f[0], 0.9, g[0]); else band[0].highShelfM (fs, f[0], 0.8, g[0]);
            band[1].peakM (fs, f[1], s.q[1], g[1]);
            band[2].peakM (fs, f[2], s.q[2], g[2]);
            if (s.bell[1]) band[3].peakM (fs, f[3], 0.9, g[3]); else band[3].lowShelfM (fs, f[3], 0.8, g[3]);
            drive = 0.0f;
        }
        else if (m == EqModel::brit81)
        {
            if (s.bell[0]) band[0].peakM (fs, f[0], 0.8, g[0]); else band[0].highShelfM (fs, f[0], 0.6, g[0]);
            band[1].peakM (fs, f[1], brit81Q (s.q[1]), g[1]);
            band[2].peakM (fs, f[2], brit81Q (s.q[2]), g[2]);
            if (s.bell[1]) band[3].peakM (fs, f[3], 0.8, g[3]); else band[3].lowShelfM (fs, f[3], 1.2, g[3]);   // calombo do indutor
            float boost = 0.0f;
            for (float x : g) boost += std::max (0.0f, x);
            drive = std::min (1.0f, boost / 30.0f);
        }
        else
        {
            // Q proporcional: pouco ganho = largo e musical, muito = estreito; o knob Q das médias abre/fecha a base
            auto qp = [] (float base, float x) { return base * (0.45 + std::abs (x) / 15.0 * 1.2); };
            if (s.bell[0]) band[0].peakM (fs, f[0], qp (1.0f, g[0]), g[0]); else band[0].highShelfM (fs, f[0], 0.7, g[0]);
            band[1].peakM (fs, f[1], qp (s.q[1], g[1]), g[1]);
            band[2].peakM (fs, f[2], qp (s.q[2], g[2]), g[2]);
            if (s.bell[1]) band[3].peakM (fs, f[3], qp (1.0f, g[3]), g[3]); else band[3].lowShelfM (fs, f[3], 0.7, g[3]);
            float boost = 0.0f;
            for (float x : g) boost += std::max (0.0f, x);
            drive = std::min (1.0f, boost / 40.0f);
        }
    }
    float process (int ch, float x)
    {
        for (auto& b : band) x = b.process (ch, x);
        if (drive > 0.0f)
        {
            const float k = 0.1f + 0.2f * drive;             // saturação leve do indutor/operacional em nível alto
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

// ---- compressor do canal/grupo: quatro circuitos
//  VCA 4K  compressor do canal da console britânica: feed-forward, joelho macio, ataque 0,1–30 ms, soltura 0,05–4 s ou
//          AUTO (volta rápido no transiente e devagar no sustentado). Limpo.
//  COLA 4K o compressor de barramento da mesma console: razões 1,5/2/4/10, ataque 0,1/0,3/1/3/10/30 ms, soltura
//          0,1/0,3/0,6/1,2 s ou AUTO. Limpo, "cola" a mixagem.
//  FET 76  compressor/limitador a FET: ataque de 20 a 800 µs, soltura 50 ms a 1,1 s, razões 4/8/12/20 e ALL (todas
//          as teclas apertadas: joelho duro, mais redução no transiente e muito mais sujeira). O FET distorce mais
//          quanto mais reduz (2ª harmônica), e o estágio de saída classe A arredonda o pico.
//  OPTO 2A nivelador ótico com válvula: detector na SAÍDA (realimentado) com o agudo realçado, ataque ~10 ms, joelho
//          bem largo, soltura em dois tempos (metade em ~60 ms, o resto em 0,3–3 s) e memória: quanto mais tempo
//          comprimindo, mais devagar solta. A válvula soma 2ª harmônica e amacia o agudo.
enum class DynModel { vca = 0, cola = 1, fet = 2, opto = 3 };

// knobs ATAQUE e SOLTURA (0..10) -> tempo de cada modelo
inline float dynAtkMs (DynModel m, float k)
{
    k = std::max (0.0f, std::min (10.0f, k));
    static const float cola[] = { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
    switch (m)
    {
        case DynModel::cola: return cola[(int) std::lround (k / 2.0f)];
        case DynModel::fet:  return 0.02f * std::pow (40.0f, k / 10.0f);    // 20–800 µs
        case DynModel::opto: return 3.0f * std::pow (10.0f, k / 10.0f);     // 3–30 ms (o original fica em ~10)
        default:             return 0.1f * std::pow (300.0f, k / 10.0f);    // 0,1–30 ms
    }
}
inline float dynRelS (DynModel m, float k)   // < 0 = AUTO
{
    k = std::max (0.0f, std::min (10.0f, k));
    static const float cola[] = { 0.1f, 0.3f, 0.6f, 1.2f, -1.0f };
    switch (m)
    {
        case DynModel::cola: return cola[(int) std::lround (k / 2.5f)];
        case DynModel::fet:  return 0.05f * std::pow (22.0f, k / 10.0f);    // 50 ms – 1,1 s
        case DynModel::opto: return 0.3f * std::pow (10.0f, k / 10.0f);     // fase lenta 0,3–3 s
        default:             return k >= 9.9f ? -1.0f : 0.05f * std::pow (80.0f, k / 10.0f);
    }
}
// RAZÃO: COLA e FET andam nas teclas do original; FET acima de 19,5 = ALL (devolve 100)
inline float dynRatio (DynModel m, float r)
{
    if (m == DynModel::cola) return r < 1.75f ? 1.5f : r < 3.0f ? 2.0f : r < 7.0f ? 4.0f : 10.0f;
    if (m == DynModel::fet)  return r < 6.0f ? 4.0f : r < 10.0f ? 8.0f : r < 16.0f ? 12.0f : r < 19.5f ? 20.0f : 100.0f;
    return r;
}

struct Dyn
{
    DynModel model = DynModel::vca;
    float thr = 10.0f, ratio = 4.0f, atkMs = 3.0f, relS = -1.0f, mix = 1.0f;
    double fs = 48000;
    float env = 0, slow = 0, grDb = 0, gPrev = 1;
    float cA = 0, cR = 0, cF = 0, cS = 0, cUp = 0;
    Biquad sc;        // OPTO 2A: o detector ouve mais o agudo
    OnePole dc;       // tira o DC que a 2ª harmônica do FET/válvula deixa
    OnePole tubo;     // OPTO 2A: o agudo da válvula/transformador amacia

    bool active() const { return thr < 9.9f; }
    void prepare (double rate)
    {
        fs = rate; env = slow = grDb = 0; gPrev = 1;
        sc.highShelf (fs, 2000.0, 0.7, 6.0); sc.reset();
        dc.lowPass (fs, 6.0); dc.reset();
        tubo.lowPass (fs, 19000.0); tubo.reset();
        update();
    }
    void update()
    {
        const float a = model == DynModel::fet && ratio > 50.0f ? atkMs * 0.5f : atkMs;   // ALL: o ataque "atropela"
        cA = coef (fs, a);
        cR = coef (fs, std::max (0.02f, relS) * 1000.0);
        cF = coef (fs, model == DynModel::opto ? 60.0 : model == DynModel::cola ? 100.0 : 90.0);
        cS = coef (fs, model == DynModel::cola ? 1500.0 : 900.0);
        cUp = coef (fs, 700.0);
    }
    // joelho macio: parte positiva de "over" com transição de largura knee
    static float joelho (float over, float knee)
    {
        if (2 * over < -knee) return 0.0f;
        if (2 * std::abs (over) <= knee) { const float t = over + knee / 2; return t * t / (2 * knee); }
        return over;
    }
    void process (float& l, float& r)
    {
        const float xl = l, xr = r;
        float target = 0.0f;
        switch (model)
        {
            case DynModel::opto:
            {
                // realimentado: o detector lê a saída (entrada × ganho anterior); estável porque o ataque é >= 3 ms
                const float d = std::max (std::abs ((float) sc.process (0, xl)), std::abs ((float) sc.process (1, xr))) * gPrev;
                // realimentado: a razão efetiva fica k(r-1)/(1+k(r-1)) => RAZÃO 4 ~ 2,8:1 (COMPRESS), 20 ~ 12:1 (LIMIT)
                target = std::min (40.0f, (std::min (ratio, 20.0f) - 1.0f) * joelho (gainToDb (d) - thr, 14.0f) * 0.6f);
                break;
            }
            case DynModel::fet:
            {
                const float d = std::max (std::abs (xl), std::abs (xr));
                const bool all = ratio > 50.0f;
                const float rr = all ? 20.0f : ratio;
                target = (1.0f - 1.0f / rr) * joelho (gainToDb (d) - thr, all ? 0.5f : 3.0f);
                if (all) target *= 1.15f;   // ALL: passa do ponto (a curva "afunda" além do limite)
                break;
            }
            case DynModel::cola:
                target = (1.0f - 1.0f / ratio) * joelho (gainToDb (std::max (std::abs (xl), std::abs (xr))) - thr, 4.0f);
                break;
            default:
                target = (1.0f - 1.0f / ratio) * joelho (gainToDb (std::max (std::abs (xl), std::abs (xr))) - thr, 6.0f);
                break;
        }

        // VCA AUTO: "slow" acompanha a média longa da redução; a soltura volta rápido até ela e devagar daí para baixo
        if (model == DynModel::vca && relS < 0) slow = cS * slow + (1 - cS) * target;
        if (target > env) env = cA * env + (1 - cA) * target;
        else if (model == DynModel::opto)
        {
            // soltura em dois tempos: rápido até metade da "memória", devagar daí para baixo
            const float piso = std::max (target, 0.5f * slow);
            env = env > piso ? cF * env + (1 - cF) * piso : cR * env + (1 - cR) * target;
        }
        else if (relS >= 0) env = cR * env + (1 - cR) * target;
        else if (model == DynModel::cola)
        {
            slow = slow < target ? target : cS * slow + (1 - cS) * target;
            env = cF * env + (1 - cF) * std::max (target, slow * 0.6f);
        }
        else env = env > slow ? cF * env + (1 - cF) * std::max (target, slow) : cS * env + (1 - cS) * target;
        if (model == DynModel::opto)   // memória da célula: carrega devagar enquanto comprime, descarrega com a soltura
            slow = env > slow ? cUp * slow + (1 - cUp) * env : cR * slow + (1 - cR) * env;

        grDb = env;
        const float g = dbToGain (-env);
        gPrev = g;
        float yl = xl * g, yr = xr * g;
        if (model == DynModel::fet)
        {
            // o FET distorce na proporção da redução; ALL suja bem mais. Saída classe A: o pico arredonda.
            const float b = (ratio > 50.0f ? 0.35f : 0.14f) * (1.0f - g) + 0.015f;
            yl += (float) dc.hp (0, b * yl * yl);
            yr += (float) dc.hp (1, b * yr * yr);
            yl = std::tanh (yl * 1.2f) / 1.2f; yr = std::tanh (yr * 1.2f) / 1.2f;
        }
        else if (model == DynModel::opto)
        {
            yl += (float) dc.hp (0, 0.06f * yl * yl);
            yr += (float) dc.hp (1, 0.06f * yr * yr);
            yl += 0.5f * (tubo.lp (0, yl) - yl);
            yr += 0.5f * (tubo.lp (1, yr) - yr);
        }
        l = xl + (yl - xl) * mix;
        r = xr + (yr - xr) * mix;
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
