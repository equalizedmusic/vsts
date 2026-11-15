#pragma once

// GOGÓ — cadeia de voz automática (sem JUCE: o mesmo arquivo roda no plugin e na bancada tools/teste_dsp.cpp).
//
// Cadeia: INPUT (o LEARN acerta) → CLEAN → TUNE → RIDE → FET → OPTO → TONE → DE-ESS → WARMTH → SPACE → OUTPUT.
// Cada bloco tem liga/desliga com rampa de 20 ms (sem estalo). Desligado, o bloco não mexe no som.
//   CLEAN : HPF de 24 dB/oit. (AUTO = 0,7 × a nota mais grave que o TUNE ouviu), corta o "pop" (rajada abaixo de
//           ~90 Hz bem acima da média) e um expander suave entre as frases (GATE).
//   TUNE  : o motor do ONKEY (TuneDsp.h). Latência fixa de ~44 ms; em TRACKING o TUNE sai do caminho e a latência é 0.
//   RIDE  : vocal rider. Leva o nível médio de cada trecho cantado a −18 dBFS RMS, no máximo ±RANGE; parado no silêncio.
//   FET   : pega pico (estilo 1176): ataque de 0,3 ms, RATIO 4/8/12/20, joelho de 6 dB.
//   OPTO  : nivelador (estilo LA-2A): RMS com ênfase de agudo no detector, ratio 3, joelho de 10 dB, ataque de 10 ms e
//           release em dois estágios (metade volta em 60 ms, o resto em 0,5 a 4 s, mais lento quanto mais tempo comprimiu).
//           FET + OPTO ganham de volta 80% da redução média (o volume não cai quando o TAME sobe).
//   TONE  : TONE (inclina agudo/grave), MUD (sino de 300 Hz, corta quando o grave embolado passa da média do próprio
//           cantor), HARSH (sino de 3,2 kHz, só dinâmico), AIR (shelf de 11 kHz) e, com o MAESTRO LINK, um sino de
//           +1,5 dB onde os SYNTHS do MAESTRO deixam espaço (2 a 4 kHz).
//   DE-ESS: banda dividida. Tira a parte de cima (acima de FREQ) só quando ela chega perto do nível da voz inteira.
//   WARMTH: saturação TUBE (assimétrica) ou TAPE (simétrica, perde um pouco de agudo), com anti-aliasing (ADAA).
//   SPACE : PLATE (FDN de 8 linhas) + DELAY ping-pong no tempo do DAW, os dois abaixando enquanto a voz canta (DUCK).

#include "TuneDsp.h"

namespace gogo
{
using onkey::kPi;

enum Bloco { bClean, bTune, bRide, bFet, bOpto, bTone, bDeess, bWarmth, bSpace, numBlocos };
enum Style { stProg, stPsy, stPop, stTrap, stFunk, stPodcast, numStyles };
enum Role { rLead, rDouble, rAdlib, rSpoken, numRoles };
enum Nota { nOff, n4, n4D, n8, n8D, n16, n16D, numNotas };

inline const char* nomeBloco (int b)
{
    static const char* n[numBlocos] = { "CLEAN", "TUNE", "RIDE", "FET", "OPTO", "TONE", "DE-ESS", "WARMTH", "SPACE" };
    return n[std::clamp (b, 0, numBlocos - 1)];
}
inline const char* nomeStyle (int s)
{
    static const char* n[numStyles] = { "PROG TRANCE", "PSY TRANCE", "POP", "TRAP / HIP-HOP", "FUNK", "PODCAST" };
    return n[std::clamp (s, 0, numStyles - 1)];
}
inline const char* nomeRole (int r)
{
    static const char* n[numRoles] = { "LEAD", "DOUBLE / BACKING", "ADLIB / FX", "SPOKEN" };
    return n[std::clamp (r, 0, numRoles - 1)];
}
inline const char* nomeNota (int k)
{
    static const char* n[numNotas] = { "OFF", "1/4", "1/4 D", "1/8", "1/8 D", "1/16", "1/16 D" };
    return n[std::clamp (k, 0, numNotas - 1)];
}
inline double tempos (int k)   // em batidas (semínimas)
{
    static const double t[numNotas] = { 0.0, 1.0, 1.5, 0.5, 0.75, 0.25, 0.375 };
    return t[std::clamp (k, 0, numNotas - 1)];
}
inline int ratioFet (int i) { static const int r[4] = { 4, 8, 12, 20 }; return r[std::clamp (i, 0, 3)]; }

// ponto de partida de cada STYLE (o menu STYLE grava estes valores nos ajustes; depois cada um é livre)
struct Estilo { float decay, fb, duck, air, mud, harsh, deessHz; int nota, tipoWarmth; };
inline const Estilo& estilo (int s)
{
    static const Estilo e[numStyles] = {
        //  decay  fb     duck   air   mud    harsh  de-ess  delay  warmth
        { 2.6f, 0.38f, 0.60f, 2.5f, 0.50f, 0.50f, 6500.f, n8D, 0 },   // PROG TRANCE: plate longa, 1/8 pontuada
        { 1.6f, 0.45f, 0.75f, 3.0f, 0.55f, 0.60f, 6800.f, n8,  0 },   // PSY TRANCE: mais curta, 1/8 com mais repetição
        { 1.8f, 0.25f, 0.50f, 2.0f, 0.50f, 0.45f, 6500.f, n4,  0 },   // POP
        { 1.2f, 0.22f, 0.60f, 2.5f, 0.55f, 0.50f, 7000.f, n4,  1 },   // TRAP / HIP-HOP
        { 0.9f, 0.15f, 0.50f, 3.0f, 0.60f, 0.60f, 6500.f, n8,  1 },   // FUNK
        { 0.5f, 0.00f, 0.30f, 1.0f, 0.60f, 0.40f, 6000.f, nOff, 1 },  // PODCAST: sala curta, sem delay
    };
    return e[std::clamp (s, 0, numStyles - 1)];
}

struct Ajustes
{
    bool on[numBlocos] { true, true, true, true, true, true, true, true, true };
    bool bypass = false, tracking = false, natural = true;
    int role = rLead;
    float inputDb = 0.0f, outputDb = 0.0f;
    // macros
    float tune = 0.6f, tame = 0.5f, tone = 0.0f, airDb = 2.5f, deess = 0.5f, warmth = 0.25f, space = 0.3f;
    // ajustes de cada bloco
    bool hpfAuto = true; float hpfHz = 90.0f, gate = 0.3f;
    float speed = 0.5f, humanize = 0.2f, vibrato = 1.0f, formant = 0.0f; int root = -1, scale = -1;
    float rideRange = 6.0f;
    int fetRatio = 0; float fetRelMs = 80.0f;
    float optoEmph = 0.4f;
    float mud = 0.5f, harsh = 0.5f;
    float deessHz = 6500.0f; bool deessListen = false;
    int tipoWarmth = 0;    // 0 TUBE, 1 TAPE
    int nota = n8D; float fb = 0.38f, decay = 2.6f, duck = 0.6f, blend = 0.5f;
    double bpm = 128.0;
    float pocketHz = 0.0f; // MAESTRO LINK: onde os synths deixam espaço (0 = sem link)
};

// o que a tela lê
struct Medidas
{
    std::atomic<float> hpfHz { 90 }, graveHz { 0 }, gateDb { 0 };
    std::atomic<int> pops { 0 };
    std::atomic<float> rideDb { 0 }, fetGr { 0 }, optoGr { 0 }, optoRelMs { 500 }, makeupDb { 0 };
    std::atomic<float> mudDb { 0 }, harshDb { 0 }, pocketHz { 0 };
    std::atomic<float> deessGr { 0 };
    std::atomic<float> duckDb { 0 }, delayMs { 0 }, spaceGr { 0 }, limGr { 0 };
    std::atomic<float> inPk { -100 }, outPk { -100 }, outRms { -100 };
    std::atomic<bool> voz { false };
    std::atomic<int> learn { 0 };          // 0 parado, 1 ouvindo
    std::atomic<float> learnSeg { 0 };     // segundos de voz ouvidos no LEARN
    std::atomic<int> learnFeito { 0 };     // conta cada LEARN terminado (a tela/processador aplica o INPUT)
    std::atomic<float> learnInput { 0 };   // INPUT que o LEARN achou (dB)
};

// histórico do gráfico de dinâmica: um ponto a cada 10 ms
struct Historico
{
    static constexpr int size = 1024;      // ≈ 10 s
    float in[size] {}, out[size] {}, ride[size] {}, gr[size] {};
    std::atomic<int> pos { 0 };
};

namespace dsp
{
struct Bq
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double tick (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    static double w (double f, double fs) { return 2.0 * kPi * std::clamp (f, 10.0, 0.45 * fs) / fs; }
    void hp (double f, double q, double fs)
    {
        const double ww = w (f, fs), cw = std::cos (ww), al = std::sin (ww) / (2 * q);
        set ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
    }
    void lp (double f, double q, double fs)
    {
        const double ww = w (f, fs), cw = std::cos (ww), al = std::sin (ww) / (2 * q);
        set ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
    }
    void bp (double f, double q, double fs)   // pico de 0 dB
    {
        const double ww = w (f, fs), cw = std::cos (ww), al = std::sin (ww) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * cw, 1 - al);
    }
    void peak (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), ww = w (f, fs), cw = std::cos (ww), al = std::sin (ww) / (2 * q);
        set (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
    }
    void lowShelf (double f, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), ww = w (f, fs), cw = std::cos (ww), sA = std::sqrt (A);
        const double al = std::sin (ww) / 2 * std::sqrt (2.0);
        set (A * ((A + 1) - (A - 1) * cw + 2 * sA * al), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - 2 * sA * al),
             (A + 1) + (A - 1) * cw + 2 * sA * al, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - 2 * sA * al);
    }
    void highShelf (double f, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), ww = w (f, fs), cw = std::cos (ww), sA = std::sqrt (A);
        const double al = std::sin (ww) / 2 * std::sqrt (2.0);
        set (A * ((A + 1) + (A - 1) * cw + 2 * sA * al), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - 2 * sA * al),
             (A + 1) - (A - 1) * cw + 2 * sA * al, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - 2 * sA * al);
    }
};

inline double coef (double ms, double fs) { return std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * fs)); }
inline double db (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-6)); }
inline double lin (double d) { return std::pow (10.0, d / 20.0); }

// liga/desliga com rampa linear de 20 ms
struct Rampa
{
    float v = 1.0f;
    float passo (bool alvo, float p) { v += std::clamp ((alvo ? 1.0f : 0.0f) - v, -p, p); return v; }
};

// log(cosh(u)) sem estourar
inline double logcosh (double u) { const double a = std::abs (u); return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; }

// atraso com leitura fracionária
struct Linha
{
    std::vector<float> b; int m = 0; std::int64_t w = 0;
    void aloca (int n) { int s = 1; while (s < n + 4) s <<= 1; b.assign ((size_t) s, 0.0f); m = s - 1; w = 0; }
    void limpa() { std::fill (b.begin(), b.end(), 0.0f); }
    void poe (float x) { b[(size_t) (w & m)] = x; ++w; }
    float le (double atraso) const   // atraso em amostras (≥ 1)
    {
        const double p = (double) w - std::max (1.0, atraso);
        const double f = std::floor (p);
        const auto i = (std::int64_t) f;
        const float t = (float) (p - f);
        const float a = b[(size_t) (i & m)], c = b[(size_t) ((i + 1) & m)];
        return a + (c - a) * t;
    }
    float leInt (int atraso) const { return b[(size_t) ((w - atraso) & m)]; }
};
} // namespace dsp

class Cadeia
{
public:
    Medidas med;
    Historico hist;
    onkey::Engine& tune() { return *afinador; }

    Cadeia() : afinador (std::make_unique<onkey::Engine>()) {}

    // latência com os ajustes de agora (TRACKING = 0)
    int latencia (bool tracking) const { return tracking ? 0 : afinador->latencia(); }
    double taxa() const { return fs; }

    void prepare (double sampleRate)
    {
        fs = sampleRate > 8000.0 ? sampleRate : 48000.0;
        afinador->prepare (fs);
        for (int c = 0; c < 2; ++c) seco[c].aloca (afinador->latencia() + maxPedaco + 8);
        dlL.aloca ((int) (2.1 * fs)); dlR.aloca ((int) (2.1 * fs));
        pre.aloca ((int) (0.12 * fs));
        static const double msLinha[8] = { 29.7, 37.1, 41.1, 43.7, 53.0, 59.9, 67.7, 73.1 };
        for (int k = 0; k < 8; ++k) fdn[k].aloca ((int) (msLinha[k] * 0.001 * fs * 1.6) + 8);
        static const double msAp[4] = { 4.7, 3.6, 12.7, 9.3 };
        for (int k = 0; k < 4; ++k) { ap[k].aloca ((int) (msAp[k] * 0.001 * fs) + 4); apLen[k] = (int) (msAp[k] * 0.001 * fs); }
        reset();
    }

    void reset()
    {
        afinador->reset();
        for (int c = 0; c < 2; ++c)
        {
            seco[c].limpa();
            for (auto& f : hpf[c]) f.reset();
            lfBand[c].reset(); meio[c].reset(); deHi[c].reset();
            for (auto& f : eq[c]) f.reset();
            dcX[c] = dcY[c] = 0.0; satX[c] = 0.0; tapeLp[c] = 0.0;
        }
        dlL.limpa(); dlR.limpa(); pre.limpa();
        for (auto& l : fdn) l.limpa();
        for (auto& l : ap) l.limpa();
        for (auto& v : fdnLp) v = 0.0;
        for (auto& f : envBp) f.reset();
        for (auto& f : sendF) f.reset();
        fbLpL = fbLpR = fbHpL = fbHpR = 0.0;
        gIn = (float) dsp::lin (aj.inputDb); gOut = 1.0f; mBypass = 0.0f;
        for (auto& r : rampa) r.v = 1.0f;
        lfEnv = lfMedio = fullEnv = gPop = 1.0; gPop = 1.0; popAnt = false;
        gateEnv = 0.0; gateG = 1.0;
        rideMs = 0.0; rideNivel = -18.0; rideG = 0.0;
        fetEnv = 0.0; optoMs = 0.0; sFast = sSlow = 0.0; memoria = 0.0; grMedio = 0.0; makeup = 0.0;
        mudMs = mudLongo = fullMs = harshMs = harshLongo = 0.0; mudDb = harshDb = 0.0; eqCont = 0; eqVersao = -1;
        hiEnv = fullEnvD = 0.0; deGr = 0.0;
        duckEnv = 0.0; tDelay = 0.0;
        vozRefMs = 0.01; wetMs = 0.0; wetGr = 0.0; limG = 1.0;
        decayS = -1.0; tamS = -1.0;
        lfoFase[0] = 0.0; lfoFase[1] = 0.25; lfoFase[2] = 0.5; lfoFase[3] = 0.75;
        graveHz = 0.0; fcHpf = -1.0;
        histCont = 0; histInPk = histOutPk = 0.0f;
        medCont = 0; outPkAcc = 0.0; outRmsAcc = 0.0;
        learnE = 0.0; learnN = 0; learnAtivo = false;
        hist.pos = 0;
    }

    void iniciaLearn() { learnE = 0.0; learnN = 0; learnAtivo = true; med.learn = 1; med.learnSeg = 0.0f; }
    void paraLearn() { if (learnAtivo) terminaLearn(); }

    // processa no lugar; nch = 1 ou 2. Blocos grandes do DAW vão em pedaços de até 2048 amostras.
    void process (float* const* ch, int nch, int ns, const Ajustes& a)
    {
        for (int ini = 0; ini < ns; ini += maxPedaco)
        {
            float* p[2] = { ch[0] + ini, ch[std::clamp (nch, 1, 2) - 1] + ini };
            processaPedaco (p, nch, std::min (maxPedaco, ns - ini), a);
        }
    }

    static constexpr int maxPedaco = 2048;

private:
    void processaPedaco (float* const* ch, int nch, int ns, const Ajustes& a)
    {
        nch = std::clamp (nch, 1, 2);
        aj = a;
        const bool tracking = a.tracking;
        const int lat = latencia (tracking);
        const float pr = (float) (1.0 / (0.02 * fs));
        const float kG = (float) (1.0 - std::exp (-1.0 / (0.03 * fs)));
        const float alvoIn = (float) dsp::lin (std::clamp (a.inputDb, -24.0f, 24.0f));

        atualizaHpf();

        // ---- 1. INPUT, LEARN, CLEAN ---------------------------------------------------------------------------
        for (int i = 0; i < ns; ++i)
        {
            for (int c = 0; c < nch; ++c) seco[c].poe (ch[c][i]);
            if (learnAtivo) aprende (ch, nch, i);
            gIn += (alvoIn - gIn) * kG;
            double x[2] = { 0, 0 };
            for (int c = 0; c < nch; ++c) x[c] = ch[c][i] * gIn;
            const float mC = rampa[bClean].passo (a.on[bClean], pr);
            if (mC > 0.0f)
            {
                double y[2] = { x[0], x[1] };
                clean (y, nch);
                for (int c = 0; c < nch; ++c) x[c] += (y[c] - x[c]) * mC;
            }
            double pk = 0;
            for (int c = 0; c < nch; ++c) { ch[c][i] = (float) x[c]; pk = std::max (pk, std::abs (x[c])); }
            histInPk = std::max (histInPk, (float) pk);
        }

        // ---- 2. TUNE (motor do ONKEY) ---------------------------------------------------------------------------
        if (! tracking)
        {
            onkey::Ajustes t;
            const float tm = std::clamp (a.tune, 0.0f, 1.0f);
            t.ligado = a.on[bTune] && tm > 0.002f;
            t.amount = std::min (1.0f, tm * 2.0f);
            float sp = std::clamp (a.speed, 0.0f, 1.0f);
            sp += (1.0f - sp) * std::max (0.0f, tm * 2.0f - 1.0f);   // TUNE acima de 50% = mais rápido (até o "robô")
            float hu = a.humanize;
            if (a.natural) { sp = std::min (sp, 0.75f); hu = std::max (hu, 0.15f); }
            t.speed = sp; t.humanize = hu; t.vibrato = a.vibrato; t.formant = a.formant;
            t.tom = a.root; t.escala = a.scale; t.saidaDb = 0.0f;
            afinador->process (ch, nch, ns, t);
        }
        {
            auto& L = afinador->leitura;
            const float hz = L.hz.load();
            if (! tracking && L.voz.load() && hz > 55.0f && hz < 1200.0f)
                graveHz = graveHz <= 0.0 ? hz : graveHz + (hz - graveHz) * (hz < graveHz ? 0.3 : 0.002);
        }

        // ---- 3. RIDE → SPACE, OUTPUT, BYPASS --------------------------------------------------------------------
        const double nat = a.natural ? 1.0 : 0.0;
        const double kRmsRide = 1.0 - dsp::coef (50.0, fs), kNivel = 1.0 - dsp::coef (400.0, fs), kRideG = 1.0 - dsp::coef (150.0, fs);
        const double range = std::clamp ((double) a.rideRange, 0.0, 12.0) * (a.role == rDouble ? 0.7 : 1.0);
        const double rangeU = range;   // o RIDE já é natural: KEEP IT NATURAL não mexe nele
        const double alvoRide = a.role == rSpoken ? -20.0 : -18.0;
        const double kFetA = 1.0 - dsp::coef (0.3, fs), kFetR = dsp::coef (std::clamp ((double) a.fetRelMs, 30.0, 1200.0), fs);
        const double tame = std::clamp ((double) a.tame, 0.0, 1.0) + (a.role == rDouble ? 0.15 : 0.0);
        const double thrFet = -4.0 - 14.0 * tame + 3.0 * nat;
        const double ratF = nat > 0 ? 4.0 : (double) ratioFet (a.fetRatio);
        const double thrOpto = -15.0 - 10.0 * tame + 2.0 * nat;
        const double kOptoE = 1.0 - dsp::coef (5.0, fs), kAt = 1.0 - dsp::coef (10.0, fs), kRelF = dsp::coef (60.0, fs);
        const double kMem3 = 1.0 - dsp::coef (3000.0, fs), kMem10 = 1.0 - dsp::coef (10000.0, fs);
        const double kGrMed = 1.0 - dsp::coef (1500.0, fs);
        const double emph = std::clamp ((double) a.optoEmph, 0.0, 1.0);
        const double kHp1 = std::exp (-2.0 * kPi * 1500.0 / fs);
        // DE-ESS
        const double deFc = std::clamp ((double) a.deessHz, 3000.0, std::min (12000.0, 0.4 * fs));
        if (std::abs (deFc - deFcAnt) > 1.0) { for (int c = 0; c < 2; ++c) deHi[c].hp (deFc, 0.7071, fs); deFcAnt = deFc; }
        const double kDeA = 1.0 - dsp::coef (0.5, fs), kDeR = dsp::coef (50.0, fs);
        const double de = std::clamp ((double) a.deess, 0.0, 1.0) * (a.role == rDouble ? 1.2 : 1.0);
        const double thrDe = -2.0 - 12.0 * de;
        const double maxDe = de > 0.001 ? (nat > 0 ? 6.0 : 10.0) : 0.0;
        // WARMTH
        const double w = std::clamp ((double) a.warmth, 0.0, 1.0) * (nat > 0 ? 0.6 : 1.0);
        const double kSat = 1.0 + 4.0 * w, bSat = a.tipoWarmth == 0 ? 0.18 : 0.0;
        const double tb = std::tanh (kSat * bSat), g0 = kSat * (1.0 - tb * tb);
        const double misturaSat = std::min (1.0, w * 3.0);
        const double kTape = a.tipoWarmth == 1 ? 1.0 - std::exp (-2.0 * kPi * (18000.0 - 7000.0 * w) / fs) : 1.0;
        const double kDc = 1.0 - 2.0 * kPi * 10.0 / fs;
        // SPACE
        const double bpm = a.bpm > 20.0 && a.bpm < 400.0 ? a.bpm : 128.0;
        const double alvoDelay = std::min (2.0 * fs, tempos (a.nota) * 60.0 / bpm * fs);
        if (tDelay <= 0.0) tDelay = alvoDelay;
        const double fbD = std::clamp ((double) a.fb, 0.0, 0.9);
        const double escala = a.role == rDouble ? 0.6 : (a.role == rAdlib ? 1.3 : (a.role == rSpoken ? 0.3 : 1.0)) * (nat > 0 ? 0.85 : 1.0);
        const double sp = std::clamp ((double) a.space, 0.0, 1.0) * escala;
        const double bl = std::clamp ((double) a.blend, 0.0, 1.0);
        const double nPlate = sp * 0.9 * std::min (1.0, 2.0 * (1.0 - bl));
        const double nDelay = a.nota == nOff ? 0.0 : sp * 0.7 * std::min (1.0, 2.0 * bl);
        const double kDuckA = 1.0 - dsp::coef (5.0, fs), kDuckR = dsp::coef (250.0, fs), kDuckG = 1.0 - dsp::coef (60.0, fs);
        const double duckMax = std::clamp ((double) a.duck, 0.0, 1.0) * 15.0;
        const double decay = std::clamp ((double) a.decay, 0.3, 6.0);
        const double tam = std::clamp (0.55 + 0.22 * decay, 0.6, 1.55);
        // DECAY/FEEDBACK/SPACE/BLEND deslizam (antes pulavam de uma vez: o tamanho das linhas da plate e o pré-delay
        // mudavam de posição de leitura no meio da cauda e estalava ao girar o knob)
        const double kBloco = 1.0 - std::exp (-(double) ns / (0.08 * fs));
        decayS = decayS < 0.0 ? decay : decayS + (decay - decayS) * kBloco;
        const double tamB = std::clamp (0.55 + 0.22 * decayS, 0.6, 1.55);
        double gLinha[8];
        static const double msLinha[8] = { 29.7, 37.1, 41.1, 43.7, 53.0, 59.9, 67.7, 73.1 };
        for (int k = 0; k < 8; ++k)
            gLinha[k] = std::pow (10.0, -3.0 * std::max (8.0, msLinha[k] * tamB * 0.001 * fs) / (decayS * fs));
        const double kDamp = std::exp (-2.0 * kPi * 6500.0 / fs);
        const double preD = (0.010 + 0.012 * decay) * fs;
        const double kPar = 1.0 - dsp::coef (60.0, fs);
        if (tamS < 0.0) { tamS = tam; preDS = preD; fbS = fbD; nPlateS = nPlate; nDelayS = nDelay; }
        if (std::abs (tam - tamAnt) > 1e-3 || sendVersao != (int) std::lround (fs))
        {
            sendF[0].hp (250.0, 0.7071, fs); sendF[1].lp (7000.0, 0.7071, fs); sendF[2].hp (250.0, 0.7071, fs); sendF[3].lp (7000.0, 0.7071, fs);
            tamAnt = tam; sendVersao = (int) std::lround (fs);
        }
        const double kFbLp = std::exp (-2.0 * kPi * 5000.0 / fs), kFbHp = std::exp (-2.0 * kPi * 200.0 / fs);
        const double modAmp = 0.00015 * fs;
        static const double hzLfo[4] = { 0.53, 0.71, 0.89, 1.13 };
        for (int k = 0; k < 4; ++k) lfoPasso[k] = hzLfo[k] / fs;
        // GUARD do SPACE e limitador da saída
        const double kVozRef = 1.0 - dsp::coef (1500.0, fs), kWetMs = 1.0 - dsp::coef (20.0, fs);
        // solta em 250 ms enquanto canta; sem voz quase segura (4 s), senão o GUARD "levanta" a cauda e ela não cai
        const double kWetA = 1.0 - dsp::coef (5.0, fs), kWetR = dsp::coef (250.0, fs), kWetRCauda = dsp::coef (4000.0, fs);
        const double kLimR = dsp::coef (120.0, fs);
        const float alvoOut = (float) dsp::lin (std::clamp (a.outputDb, -24.0f, 24.0f));
        const int passoHist = std::max (1, (int) (0.01 * fs));

        for (int i = 0; i < ns; ++i)
        {
            double x[2] = { ch[0][i], nch > 1 ? ch[1][i] : ch[0][i] };
            double mono = nch > 1 ? 0.5 * (x[0] + x[1]) : x[0];
            double grTot = 0.0;

            // RIDE
            {
                rideMs += (mono * mono - rideMs) * kRmsRide;
                const double nv = 10.0 * std::log10 (rideMs + 1e-12);
                const bool voz = nv > -55.0 && nv > gateLimiar + 12.0;
                if (voz) rideNivel += (nv - rideNivel) * kNivel;
                const double desejado = voz ? std::clamp (alvoRide - rideNivel, -rangeU, rangeU) : rideG;
                rideG += (desejado - rideG) * kRideG;
                vozAgora = voz;
                const float m = rampa[bRide].passo (a.on[bRide], (float) (1.0 / (0.02 * fs)));
                const double g = dsp::lin (rideG * m);
                x[0] *= g; x[1] *= g; mono *= g;
            }
            // FET
            {
                const float m = rampa[bFet].passo (a.on[bFet], (float) (1.0 / (0.02 * fs)));
                const double pk = std::max (std::abs (x[0]), std::abs (x[1]));
                fetEnv = pk > fetEnv ? fetEnv + (pk - fetEnv) * kFetA : fetEnv * kFetR;
                const double gr = reducao (dsp::db (fetEnv), thrFet, ratF, 6.0) * m;
                fetGrAgora = gr;
                grTot += gr;
                const double g = dsp::lin (-gr);
                x[0] *= g; x[1] *= g; mono *= g;
            }
            // OPTO
            {
                const float m = rampa[bOpto].passo (a.on[bOpto], (float) (1.0 / (0.02 * fs)));
                hp1 = kHp1 * (hp1 + mono - hp1x); hp1x = mono;   // passa-altas de 1 polo (1,5 kHz) para a ênfase
                const double det = mono + emph * 1.0 * hp1;
                optoMs += (det * det - optoMs) * kOptoE;
                const double alvo = reducao (10.0 * std::log10 (optoMs + 1e-12) + 3.0, thrOpto, 3.0, 10.0);
                if (alvo > sFast) sFast += (alvo - sFast) * kAt; else sFast = alvo + (sFast - alvo) * kRelF;
                const double tSlow = 0.5 + 3.5 * memoria;
                const double kRelS = dsp::coef (tSlow * 1000.0, fs);
                if (alvo > sSlow) sSlow += (alvo - sSlow) * kAt; else sSlow = alvo + (sSlow - alvo) * kRelS;
                memoria += alvo > 1.0 ? (1.0 - memoria) * kMem3 : -memoria * kMem10;
                optoRel = tSlow;
                const double gr = 0.5 * (sFast + sSlow) * m;
                optoGrAgora = gr;
                grTot += gr;
                const double g = dsp::lin (-gr);
                x[0] *= g; x[1] *= g; mono *= g;
            }
            // ganho de volta (80% da redução média de FET + OPTO, medida com voz)
            {
                if (vozAgora) grMedio += (grTot - grMedio) * kGrMed;
                makeup += (0.8 * grMedio - makeup) * kGrMed;
                const double g = dsp::lin (makeup);
                x[0] *= g; x[1] *= g; mono *= g;
            }
            // TONE
            {
                const float m = rampa[bTone].passo (a.on[bTone], (float) (1.0 / (0.02 * fs)));
                // detectores (no mono antes do EQ)
                const double bm = envBp[0].tick (mono), bh = envBp[1].tick (mono);
                const double k30 = kTone30;
                mudMs += (bm * bm - mudMs) * k30; harshMs += (bh * bh - harshMs) * k30; fullMs += (mono * mono - fullMs) * k30;
                if (++eqCont >= 32)
                {
                    eqCont = 0;
                    atualizaTone (m);
                }
                if (m > 0.0f || eqAtivo)
                    for (int c = 0; c < nch; ++c)
                    {
                        double y = x[c];
                        for (auto& f : eq[c]) y = f.tick (y);
                        x[c] = y;
                    }
                if (nch == 1) x[1] = x[0];
                mono = nch > 1 ? 0.5 * (x[0] + x[1]) : x[0];
            }
            // DE-ESS
            {
                const float m = rampa[bDeess].passo (a.on[bDeess], (float) (1.0 / (0.02 * fs)));
                double hi[2] = { 0, 0 }, hpk = 0, fpk = 0;
                for (int c = 0; c < nch; ++c)
                {
                    hi[c] = deHi[c].tick (x[c]);
                    hpk = std::max (hpk, std::abs (hi[c])); fpk = std::max (fpk, std::abs (x[c]));
                }
                hiEnv = hpk > hiEnv ? hiEnv + (hpk - hiEnv) * kDeA : hiEnv * kDeR;
                fullEnvD = fpk > fullEnvD ? fullEnvD + (fpk - fullEnvD) * kDeA : fullEnvD * kDeR;
                const double rel = dsp::db (hiEnv) - dsp::db (fullEnvD);
                const double alvo = hiEnv > 0.002 ? std::clamp ((rel - thrDe) * 1.5, 0.0, maxDe) : 0.0;
                deGr = alvo > deGr ? alvo : alvo + (deGr - alvo) * dsp::coef (60.0, fs);
                const double gr = deGr * m;
                const double g = dsp::lin (-gr);
                for (int c = 0; c < nch; ++c)
                {
                    const double tira = (1.0 - g) * hi[c];
                    x[c] = a.deessListen && m > 0.5f ? tira : x[c] - tira;
                }
                if (nch == 1) x[1] = x[0];
                deGrAgora = gr;
            }
            // WARMTH
            {
                const float m = rampa[bWarmth].passo (a.on[bWarmth], (float) (1.0 / (0.02 * fs)));
                if (m > 0.0f && misturaSat > 0.0)
                    for (int c = 0; c < nch; ++c)
                    {
                        const double xc = x[c];
                        // ADAA de 1ª ordem de tanh(k(x + b))
                        const double d = xc - satX[c];
                        double f;
                        if (std::abs (d) > 1.0e-5)
                            f = (dsp::logcosh (kSat * (xc + bSat)) - dsp::logcosh (kSat * (satX[c] + bSat))) / (kSat * d);
                        else
                            f = std::tanh (kSat * (0.5 * (xc + satX[c]) + bSat));
                        satX[c] = xc;
                        double y = (f - tb) / g0;
                        if (a.tipoWarmth == 1) { tapeLp[c] += (y - tapeLp[c]) * kTape; y = tapeLp[c]; }
                        // corta o DC da assimetria
                        const double yd = y - dcX[c] + kDc * dcY[c];
                        dcX[c] = y; dcY[c] = yd;
                        x[c] += (yd - x[c]) * misturaSat * m;
                    }
                if (nch == 1) x[1] = x[0];
            }
            // SPACE
            double wetL = 0.0, wetR = 0.0;
            {
                const float m = rampa[bSpace].passo (a.on[bSpace], (float) (1.0 / (0.02 * fs)));
                const double s = nch > 1 ? 0.5 * (x[0] + x[1]) : x[0];
                const double as = std::abs (s);
                duckEnv = as > duckEnv ? duckEnv + (as - duckEnv) * kDuckA : duckEnv * kDuckR;
                const double alvoDuck = -duckMax * std::clamp ((dsp::db (duckEnv) + 40.0) / 22.0, 0.0, 1.0);
                duckDb += (alvoDuck - duckDb) * kDuckG;
                tamS += (tam - tamS) * kPar; preDS += (preD - preDS) * kPar; fbS += (fbD - fbS) * kPar;
                nPlateS += (nPlate - nPlateS) * kPar; nDelayS += (nDelay - nDelayS) * kPar;
                if (nPlateS < 1e-6) nPlateS = nPlate;
                if (nDelayS < 1e-6) nDelayS = nDelay;
                if (m > 0.0f && (nPlateS > 0.0 || nDelayS > 0.0))
                {
                    double e = sendF[1].tick (sendF[0].tick (s));
                    const double gd = dsp::lin (duckDb) * m;
                    // DELAY ping-pong
                    tDelay += (alvoDelay - tDelay) * 0.0005;
                    double dL = 0.0, dR = 0.0;
                    if (nDelayS > 0.0 && tDelay > 8.0)
                    {
                        dL = dlL.le (tDelay); dR = dlR.le (tDelay);
                        fbLpL += (dL - fbLpL) * (1.0 - kFbLp); fbLpR += (dR - fbLpR) * (1.0 - kFbLp);
                        fbHpL = kFbHp * fbHpL + (1.0 - kFbHp) * fbLpL; fbHpR = kFbHp * fbHpR + (1.0 - kFbHp) * fbLpR;
                        const double rL = fbLpL - fbHpL, rR = fbLpR - fbHpR;
                        // satura de leve DENTRO da realimentação: com feedback alto as repetições não se empilham
                        auto satFb = [] (double v) { return 0.5 * std::tanh (v * 2.0); };
                        dlL.poe ((float) (e + satFb (fbS * rR)));
                        dlR.poe ((float) satFb (fbS * rL));
                    }
                    else { dlL.poe (0.0f); dlR.poe (0.0f); }
                    // PLATE (FDN de 8 linhas, Hadamard)
                    double pL = 0.0, pR = 0.0;
                    if (nPlateS > 0.0)
                    {
                        pre.poe ((float) e);
                        double v = pre.le (preDS);
                        for (int k = 0; k < 4; ++k)   // difusão de entrada (all-pass)
                        {
                            const double atr = ap[k].leInt (apLen[k]);
                            const double u = v + 0.6 * atr;
                            ap[k].poe ((float) u);
                            v = atr - 0.6 * u;
                        }
                        // 4 das 8 linhas balançam ±0,15 ms bem devagar: quebra a ressonância metálica do decay longo
                        for (int k = 0; k < 4; ++k) { lfoFase[k] += lfoPasso[k]; if (lfoFase[k] >= 1.0) lfoFase[k] -= 1.0; }
                        double o[8];
                        for (int k = 0; k < 8; ++k)
                        {
                            const double len = std::max (8.0, msLinha[k] * tamS * 0.001 * fs);
                            const double r = fdn[k].le ((k & 1) ? len + modAmp * std::sin (2.0 * kPi * lfoFase[k >> 1]) : len);
                            fdnLp[k] = r + (fdnLp[k] - r) * kDamp;
                            o[k] = fdnLp[k] * gLinha[k];
                        }
                        // Hadamard 8 (rápido), normalizado
                        double h[8];
                        for (int k = 0; k < 8; ++k) h[k] = o[k];
                        for (int len = 1; len < 8; len <<= 1)
                            for (int ii = 0; ii < 8; ii += len << 1)
                                for (int j = ii; j < ii + len; ++j) { const double p = h[j], q = h[j + len]; h[j] = p + q; h[j + len] = p - q; }
                        const double nrm = 0.35355339059327373;
                        for (int k = 0; k < 8; ++k) fdn[k].poe ((float) (h[k] * nrm + v * ((k & 1) ? -0.5 : 0.5)));
                        pL = o[0] - o[2] + o[4] - o[6] + 0.5 * (o[1] + o[5]);
                        pR = o[1] - o[3] + o[5] - o[7] + 0.5 * (o[2] + o[6]);
                        pL *= 0.6; pR *= 0.6;
                    }
                    wetL = 3.0 * gd * (nPlateS * pL + nDelayS * dL);
                    wetR = 3.0 * gd * (nPlateS * pR + nDelayS * dR);
                }
                // GUARD: o efeito nunca passa de 5 dB abaixo do nível médio da voz (com SPACE/DECAY/FEEDBACK no talo
                // o plate e o delay ressoavam, somavam +10 dB e o teto da saída distorcia). Abaixo disso não mexe.
                if (vozAgora) vozRefMs += (s * s - vozRefMs) * kVozRef;
                wetMs += (0.5 * (wetL * wetL + wetR * wetR) - wetMs) * kWetMs;
                const double alvoGr = wetMs > 1e-10 ? reducao (10.0 * std::log10 (wetMs), 10.0 * std::log10 (vozRefMs + 1e-12) - 5.0, 20.0, 4.0) : 0.0;
                wetGr = alvoGr > wetGr ? wetGr + (alvoGr - wetGr) * kWetA : alvoGr + (wetGr - alvoGr) * (vozAgora ? kWetR : kWetRCauda);
                const double gg = dsp::lin (-wetGr);
                wetL *= gg; wetR *= gg;
            }
            // OUTPUT
            gOut += (alvoOut - gOut) * kG;
            const float mB = (mBypass += std::clamp ((a.bypass ? 1.0f : 0.0f) - mBypass, -pr, pr));
            double y2[2] = { 0.0, 0.0 }, pkPre = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                y2[c] = (x[c] + (nch > 1 ? (c == 0 ? wetL : wetR) : 0.5 * (wetL + wetR))) * gOut;
                pkPre = std::max (pkPre, std::abs (y2[c]));
            }
            // limitador de saída em −1 dBFS: abaixa o GANHO (os dois canais juntos) em vez de achatar a onda
            {
                const double alvoLim = pkPre > kTeto ? kTeto / pkPre : 1.0;
                limG = alvoLim < limG ? alvoLim : alvoLim + (limG - alvoLim) * kLimR;
                if (limG < 0.977) ++amostrasLimitadas;   // mais de 0,2 dB
                limMin = std::min (limMin, limG);
            }
            double pk = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                double y = y2[c] * limG;
                if (std::abs (y) > 0.999) y = y > 0 ? 0.999 : -0.999;   // rede de segurança (não deveria chegar aqui)
                const double dry = seco[c].leInt (ns - i + lat);
                y = y + (dry - y) * mB;
                ch[c][i] = (float) y;
                pk = std::max (pk, std::abs (y));
            }
            outPkAcc = std::max (outPkAcc, pk);
            outRmsAcc += pk * pk;
            histOutPk = std::max (histOutPk, (float) pk);
            grFrame = std::max (grFrame, (float) grTot);

            if (++histCont >= passoHist)
            {
                histCont = 0;
                const int hp = hist.pos.load (std::memory_order_relaxed);
                hist.in[hp % Historico::size] = (float) dsp::db (histInPk);
                hist.out[hp % Historico::size] = (float) dsp::db (histOutPk);
                hist.ride[hp % Historico::size] = a.on[bRide] ? (float) rideG : 0.0f;
                hist.gr[hp % Historico::size] = grFrame;
                hist.pos.store (hp + 1, std::memory_order_release);
                histInPk = histOutPk = 0.0f; grFrame = 0.0f;
            }
        }
        publica (ns);
    }

    // redução (dB, positiva) de um compressor de joelho suave
    static double reducao (double nivelDb, double thr, double ratio, double joelho)
    {
        const double d = nivelDb - thr;
        const double s = 1.0 - 1.0 / ratio;
        if (2.0 * d < -joelho) return 0.0;
        if (2.0 * std::abs (d) <= joelho) { const double t = d + joelho * 0.5; return s * t * t / (2.0 * joelho); }
        return s * d;
    }

    void atualizaHpf()
    {
        double fc;
        if (aj.hpfAuto)
        {
            const double base = graveHz > 0.0 ? graveHz : 130.0;
            fc = std::clamp (base * (aj.role == rSpoken ? 0.6 : 0.7), 50.0, 170.0) + (aj.role == rDouble ? 30.0 : 0.0);
        }
        else fc = std::clamp ((double) aj.hpfHz, 20.0, 400.0);
        if (std::abs (fc - fcHpf) > 0.5)
        {
            fcHpf = fc;
            for (int c = 0; c < 2; ++c) { hpf[c][0].hp (fc, 0.5411961, fs); hpf[c][1].hp (fc, 1.3065630, fs); lfBand[c].lp (std::max (150.0, 1.8 * fc), 0.7071, fs); meio[c].bp (1000.0, 0.5, fs); }
        }
        gateLimiar = aj.gate <= 0.001f ? -200.0 : -75.0 + 35.0 * std::clamp ((double) aj.gate, 0.0, 1.0);
    }

    void clean (double* x, int nch)
    {
        double lf[2] = { 0, 0 }, lpk = 0, mpk = 0;
        for (int c = 0; c < nch; ++c)
        {
            x[c] = hpf[c][1].tick (hpf[c][0].tick (x[c]));
            lf[c] = lfBand[c].tick (x[c]);
            lpk = std::max (lpk, std::abs (lf[c])); mpk = std::max (mpk, std::abs (meio[c].tick (x[c])));
        }
        // POP ("p", "b"): rajada de grave que aparece de repente (3× a média dos últimos 300 ms) e passa de 2× o médio
        // da voz (vogal tem formantes no médio; o "p" é quase só grave)
        const double kA = 1.0 - dsp::coef (0.5, fs), kR = dsp::coef (60.0, fs);
        lfEnv = lpk > lfEnv ? lfEnv + (lpk - lfEnv) * kA : lfEnv * kR;
        fullEnv = mpk > fullEnv ? fullEnv + (mpk - fullEnv) * kA : fullEnv * kR;
        lfMedio += (lfEnv - lfMedio) * (1.0 - dsp::coef (300.0, fs));
        double alvo = 1.0;
        if (lfEnv > 0.02 && lfEnv > 3.0 * lfMedio + 1e-4 && lfEnv > 2.0 * fullEnv)
            alvo = std::clamp (2.0 * fullEnv / lfEnv, 0.15, 1.0);
        gPop = alvo < gPop ? gPop + (alvo - gPop) * (1.0 - dsp::coef (0.5, fs)) : alvo + (gPop - alvo) * dsp::coef (80.0, fs);
        const bool pop = gPop < 0.85;
        if (pop && ! popAnt) med.pops = med.pops.load() + 1;
        popAnt = pop;
        if (gPop < 0.999)
            for (int c = 0; c < nch; ++c) x[c] -= lf[c] * (1.0 - gPop);
        // GATE: expander 1:2 abaixo do limiar, até −24 dB
        const double e = std::max (std::abs (x[0]), nch > 1 ? std::abs (x[1]) : 0.0);
        gateEnv = e > gateEnv ? gateEnv + (e - gateEnv) * (1.0 - dsp::coef (1.0, fs)) : gateEnv * dsp::coef (100.0, fs);
        const double eDb = dsp::db (gateEnv);
        const double alvoG = eDb < gateLimiar ? dsp::lin (std::max (-24.0, (eDb - gateLimiar))) : 1.0;
        gateG = alvoG > gateG ? gateG + (alvoG - gateG) * (1.0 - dsp::coef (2.0, fs)) : alvoG + (gateG - alvoG) * dsp::coef (120.0, fs);
        for (int c = 0; c < nch; ++c) x[c] *= gateG;
    }

    // TONE: a cada 32 amostras recalcula os sinos dinâmicos e os shelves
    void atualizaTone (float m)
    {
        const double seg = 32.0 / fs;
        const double kLongo = 1.0 - std::exp (-seg / 8.0);
        const double rMud = 10.0 * std::log10 ((mudMs + 1e-12) / (fullMs + 1e-12));
        const double rHarsh = 10.0 * std::log10 ((harshMs + 1e-12) / (fullMs + 1e-12));
        const bool voz = vozAgora && fullMs > 1e-6;
        if (voz)
        {
            if (mudLongo == 0.0) { mudLongo = rMud; harshLongo = rHarsh; }
            mudLongo += (rMud - mudLongo) * kLongo;
            harshLongo += (rHarsh - harshLongo) * kLongo;
        }
        const double nat = aj.natural ? 3.0 : 5.0;
        const double mu = std::clamp ((double) aj.mud, 0.0, 1.0), ha = std::clamp ((double) aj.harsh, 0.0, 1.0);
        double alvoMud = 0.0, alvoHarsh = 0.0;
        if (voz)
        {
            alvoMud = std::min (nat, mu * (1.0 + 0.6 * std::clamp (rMud - mudLongo, 0.0, 6.0)));
            alvoHarsh = std::min (nat, ha * 0.8 * std::clamp (rHarsh - harshLongo, 0.0, 6.0));
        }
        else alvoMud = mu * 1.0;
        const double kSuave = 1.0 - std::exp (-seg / 0.03);
        mudDb += (alvoMud - mudDb) * kSuave;
        harshDb += (alvoHarsh - harshDb) * kSuave;
        mudDbM = mudDb * m; harshDbM = harshDb * m;

        const double t = std::clamp ((double) aj.tone, -1.0, 1.0);
        const double air = std::clamp ((double) aj.airDb, -6.0, 9.0) - (aj.role == rDouble ? 1.0 : 0.0);
        const double pocket = aj.pocketHz > 100.0f ? 1.5 : 0.0;
        const double alvos[6] = { -1.5 * t * m, -mudDbM, -harshDbM, pocket * m, 4.0 * t * m, air * m };
        bool muda = eqVersao != (int) std::lround (fs) || std::abs (aj.pocketHz - pocketAnt) > 1.0f;
        for (int k = 0; k < 6; ++k) if (std::abs (alvos[k] - eqG[k]) > 0.02) muda = true;
        eqAtivo = false;
        for (int k = 0; k < 6; ++k) if (std::abs (alvos[k]) > 0.01) eqAtivo = true;
        if (! muda) return;
        const double fMud = aj.role == rDouble ? 350.0 : 300.0;
        for (int k = 0; k < 6; ++k) eqG[k] = alvos[k];
        pocketAnt = aj.pocketHz;
        for (int c = 0; c < 2; ++c)
        {
            eq[c][0].lowShelf (180.0, eqG[0], fs);
            eq[c][1].peak (fMud, 1.2, eqG[1], fs);
            eq[c][2].peak (3200.0, 2.0, eqG[2], fs);
            eq[c][3].peak (aj.pocketHz > 100.0f ? aj.pocketHz : 3000.0, 1.4, eqG[3], fs);
            eq[c][4].highShelf (2500.0, eqG[4], fs);
            eq[c][5].highShelf (11000.0, eqG[5], fs);
        }
        if (eqVersao != (int) std::lround (fs))
        {
            envBp[0].bp (fMud, 1.2, fs); envBp[1].bp (3200.0, 2.0, fs);
            kTone30 = 1.0 - dsp::coef (30.0, fs);
            eqVersao = (int) std::lround (fs);
        }
    }

    void aprende (float* const* ch, int nch, int i)
    {
        double v = 0;
        for (int c = 0; c < nch; ++c) v += (double) ch[c][i] * ch[c][i];
        v /= nch;
        learnBlocoE += v; ++learnBlocoN;
        if (learnBlocoN >= (int) (0.01 * fs))
        {
            const double r = learnBlocoE / learnBlocoN;
            if (10.0 * std::log10 (r + 1e-12) > -60.0) { learnE += learnBlocoE; learnN += learnBlocoN; }
            learnBlocoE = 0.0; learnBlocoN = 0;
            med.learnSeg = (float) (learnN / fs);
            if (learnN >= 20.0 * fs) terminaLearn();
        }
    }

    void terminaLearn()
    {
        learnAtivo = false;
        med.learn = 0;
        if (learnN >= 2.0 * fs)
        {
            const double rmsDb = 10.0 * std::log10 (learnE / learnN + 1e-12);
            med.learnInput = (float) std::clamp (-18.0 - rmsDb, -24.0, 24.0);
            med.learnFeito = med.learnFeito.load() + 1;
        }
    }

    void publica (int ns)
    {
        medCont += ns;
        if (medCont < (int) (0.033 * fs)) return;
        const double n = medCont;
        medCont = 0;
        med.hpfHz = (float) fcHpf;
        med.graveHz = (float) graveHz;
        med.gateDb = (float) dsp::db (gateG);
        med.rideDb = (float) rideG;
        med.fetGr = (float) fetGrAgora;
        med.optoGr = (float) optoGrAgora;
        med.optoRelMs = (float) (optoRel * 1000.0);
        med.makeupDb = (float) makeup;
        med.mudDb = (float) mudDbM;
        med.harshDb = (float) harshDbM;
        med.pocketHz = aj.pocketHz;
        med.deessGr = (float) deGrAgora;
        med.duckDb = (float) duckDb;
        med.spaceGr = (float) wetGr;
        med.limGr = (float) -dsp::db (limG);
        med.delayMs = (float) (aj.nota == nOff ? 0.0 : tDelay / fs * 1000.0);
        med.inPk = (float) dsp::db (gIn);
        med.outPk = (float) dsp::db (outPkAcc);
        med.outRms = (float) (10.0 * std::log10 (outRmsAcc / n + 1e-12));
        med.voz = vozAgora;
        outPkAcc = 0.0; outRmsAcc = 0.0;
    }

    std::unique_ptr<onkey::Engine> afinador;
    double fs = 48000.0;
    Ajustes aj;
    dsp::Rampa rampa[numBlocos];
    float gIn = 1.0f, gOut = 1.0f, mBypass = 0.0f;
    dsp::Linha seco[2];

    // CLEAN
    dsp::Bq hpf[2][2], lfBand[2], meio[2];
    double fcHpf = -1.0, graveHz = 0.0, gateLimiar = -200.0;
    double lfEnv = 0, lfMedio = 0, fullEnv = 0, gPop = 1, gateEnv = 0, gateG = 1;
    bool popAnt = false;
    // RIDE / FET / OPTO
    double rideMs = 0, rideNivel = -18, rideG = 0;
    bool vozAgora = false;
    double fetEnv = 0, fetGrAgora = 0;
    double hp1 = 0, hp1x = 0, optoMs = 0, sFast = 0, sSlow = 0, memoria = 0, optoRel = 0.5, optoGrAgora = 0;
    double grMedio = 0, makeup = 0;
    // TONE
    dsp::Bq eq[2][6], envBp[2];
    double eqG[6] {}, mudMs = 0, harshMs = 0, fullMs = 0, mudLongo = 0, harshLongo = 0, mudDb = 0, harshDb = 0, mudDbM = 0, harshDbM = 0;
    double kTone30 = 0.001;
    float pocketAnt = -1.0f;
    int eqCont = 0, eqVersao = -1;
    bool eqAtivo = false;
    // DE-ESS
    dsp::Bq deHi[2];
    double deFcAnt = -1, hiEnv = 0, fullEnvD = 0, deGr = 0, deGrAgora = 0;
    // WARMTH
    double satX[2] {}, dcX[2] {}, dcY[2] {}, tapeLp[2] {};
    // SPACE
    dsp::Bq sendF[4];
    dsp::Linha dlL, dlR, pre, fdn[8], ap[4];
    int apLen[4] {};
    double fdnLp[8] {};
    double fbLpL = 0, fbLpR = 0, fbHpL = 0, fbHpR = 0, duckEnv = 0, duckDb = 0, tDelay = 0, tamAnt = -1;
    int sendVersao = -1;
    double lfoFase[4] { 0.0, 0.25, 0.5, 0.75 }, lfoPasso[4] {};
    double vozRefMs = 0.01, wetMs = 0, wetGr = 0;
    double decayS = -1, tamS = -1, preDS = 0, fbS = 0, nPlateS = 0, nDelayS = 0;
    // OUTPUT
    static constexpr double kTeto = 0.891;   // −1 dBFS
    double limG = 1.0;
public:
    // bancada: quantas amostras o limitador da saída abaixou mais de 0,2 dB, e o menor ganho
    std::int64_t amostrasLimitadas = 0;
    double limMin = 1.0;
private:
    // medidas
    int histCont = 0, medCont = 0;
    float histInPk = 0, histOutPk = 0, grFrame = 0;
    double outPkAcc = 0, outRmsAcc = 0;
    // LEARN
    bool learnAtivo = false;
    double learnE = 0, learnBlocoE = 0;
    std::int64_t learnN = 0;
    int learnBlocoN = 0;
};
} // namespace gogo
