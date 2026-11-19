#pragma once
// CHUTE: motor do kick, sem JUCE (o teste em tools/ compila só com este arquivo e o Bancos.h).
//
// Cada batida = ATAQUE + CORPO, sempre a partir de um MODELO do catálogo (Bancos.h: a fábrica + os bancos instalados):
//  - ATAQUE (CLICK): os primeiros ms do modelo, tocados como estão (com LEVEL e TONE). Termina em LENGTH com
//    crossfade de 5 ms para o corpo.
//  - CORPO (BODY): um seno com a curva de pitch e o envelope do modelo, medidos a 1 ms. Os knobs mexem nas curvas:
//      TUNE / modo  → a nota da cauda (KEY·MIDI = a nota tocada, FIXED = a nota do modelo afinada, PRESET = a
//                     afinação original do modelo); TUNE soma semitons
//      PITCH        → onde a curva começa (Hz); DROP → a velocidade da queda (ms); DECAY → o comprimento (ms, até
//                     −20 dB); TAIL → o nível do corpo no fim do DECAY (−20 dB = como o modelo)
//    Nos valores do preset o corpo sai igual ao do modelo. A frequência é constante dentro de cada ms (a fase é
//    exata nos pontos medidos).
//  - DRIVE (WARM / HARD / FOLD) na soma; TONE fecha o agudo SÓ do que o DRIVE acrescenta (DRIVE em 0 = o TONE não
//    mexe em nada). 0.1.2: o DRIVE anda suave amostra a amostra (girar não estala) e entra aos poucos nos primeiros
//    10% do knob; o ganho máximo caiu de +30 para +18 dB (+12 no FOLD) e o HARD virou um corte de joelho firme.
//  - Kick é mono: a batida nova sempre corta a anterior em 3 ms (o seletor CHOKE / POLY saiu na 0.1.2).
// Latência zero. Saída mono (a mesma nos dois canais).
#include "Bancos.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace chute
{
constexpr double pi = 3.14159265358979323846;
constexpr int nPts = 1200;   // corpo: até 1,2 s (um ponto por ms)
constexpr int maxVozes = 4;

enum Modo { modoKey = 0, modoFixed, modoPreset };
enum Drive { driveWarm = 0, driveHard, driveFold };

inline double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
inline double ftom (double f) { return 69.0 + 12.0 * std::log2 (std::max (f, 1.0) / 440.0); }

struct Ajustes
{
    int   modelo = 0;
    bool  clickOn = true, bodyOn = true;
    float clickDb = 0.0f;      // −24..+12
    float clickTone = 0.0f;    // −12..+12 dB (inclinação: grave ↔ agudo do ataque)
    float clickMs = 12.0f;     // 2..25
    int   modo = modoKey;
    float tune = 0.0f;         // semitons −12..+12
    float pitchHz = 400.0f;    // início da curva de pitch
    float dropMs = 60.0f;      // queda
    float decayMs = 200.0f;    // comprimento (até −20 dB)
    float tailDb = -20.0f;     // nível no fim do DECAY (−40..−6)
    int   driveTipo = driveWarm;
    float drive = 0.0f;        // 0..1
    float toneHz = 20000.0f;   // passa-baixa do que o DRIVE acrescenta
    float volumeDb = -3.0f;
    float velo = 0.6f;         // 0..1
};

// valores de fábrica do corpo de um modelo (o preset)
inline void padraoDoModelo (Ajustes& a, int m)
{
    m = std::clamp (m, 0, numModelos() - 1);
    const Modelo& M = modelo (m);
    a.modelo = m;
    a.pitchHz = std::round (M.fIni);
    a.dropMs = std::round (M.dropMs);
    a.decayMs = std::round (M.decayMs);
    a.tailDb = -20.0f;
    a.tune = 0.0f;
}

// frequência da cauda do corpo para uma nota MIDI tocada (nota < 0 = nenhuma: a do modelo)
inline double fCauda (const Ajustes& a, int nota)
{
    const Modelo& M = modelo (a.modelo);
    switch (a.modo)
    {
        case modoPreset: return M.fEnd * std::pow (2.0, a.tune / 12.0);
        case modoFixed:  return mtof (M.nota + a.tune);
        default:
        {
            if (nota < 0) return mtof (M.nota + a.tune);
            // KEY·MIDI: a classe da nota tocada (C, C#…) na oitava mais perto da nota do modelo
            const int base = M.nota;
            int d = ((nota % 12) - (base % 12) + 12) % 12;
            if (d > 6) d -= 12;
            return mtof (base + d + a.tune);
        }
    }
}

// curvas do corpo (1 ms) para os ajustes: frequência (Hz) e amplitude. Devolve quantos pontos valem.
inline int curvasCorpo (const Ajustes& a, double fT, float* fq, float* am)
{
    const Modelo& M = modelo (a.modelo);
    const double r0 = (double) M.fIni / M.fEnd;
    double gama = 1.0;
    if (r0 > 1.05) gama = std::clamp (std::log (std::max (1.0, (double) a.pitchHz) / fT) / std::log (r0), 0.0, 4.0);
    const double sd = std::clamp ((double) a.dropMs / std::max (1.0f, M.dropMs), 0.05, 20.0);
    const double sa = std::clamp ((double) a.decayMs / std::max (1.0f, M.decayMs), 0.1, 6.0);
    const double kt = std::clamp ((double) a.tailDb / -20.0, 0.25, 3.0);
    double amax = 1e-9;
    const int nCurva = (int) M.d.size();
    for (int k = 0; k < nCurva; ++k) amax = std::max (amax, (double) M.a[k]);
    int len = 0;
    for (int k = 0; k < nPts; ++k)
    {
        // d é a frequência média do ms k (centro k + 0,5): na escala sd o ms k cai em (k + 0,5) / sd − 0,5
        const double td = (k + 0.5) / sd - 0.5;
        double dv;
        if (td <= 0.0) dv = M.d[0];
        else if (td >= nCurva - 1) dv = M.d[nCurva - 1];
        else { const int i = (int) td; const double f = td - i; dv = M.d[i] + f * (M.d[i + 1] - M.d[i]); }
        fq[k] = (float) (fT * std::pow (std::max (dv, 1e-3), gama));
        const double ta = k / sa;
        double av;
        if (ta >= nCurva - 1) av = 0.0;
        else { const int i = (int) ta; const double f = ta - i; av = M.a[i] + f * (M.a[i + 1] - M.a[i]); }
        am[k] = (float) (amax * std::pow (std::max (av, 0.0) / amax, kt));
        if (am[k] > 1e-5f) len = k + 2;
    }
    return std::min (len, nPts);
}

// fase do corpo (ciclos) no início de cada ms: integra a frequência constante por ms a partir da fase do modelo
inline void faseCorpo (const Ajustes& a, const float* fq, int len, float* fase)
{
    const Modelo& M = modelo (a.modelo);
    double ph = M.fase0;
    for (int k = 0; k < len; ++k) { fase[k] = (float) ph; ph += fq[k] * 0.001; }
}

// uma batida soando
struct Voz
{
    bool ativa = false;
    int modelo = 0;
    double t = 0.0, dt = 1.0 / 48000.0;   // s desde o início
    double fase = 0.0;
    float fq[nPts] {}, am[nPts] {}, ph[nPts] {};   // frequência, amplitude e fase (ciclos) no início de cada ms
    int len = 0;
    double ganho = 1.0;                   // velocity
    double corte = 1.0, corteVel = 0.0;   // CHOKE: some em 3 ms
    double lpTom = 0.0;                   // filtro do TONE do ataque
    double sinal = 1.0, t0 = 0.0;
    double clickFim = 0.012;
    float notaMidi = 0.0f;

    // ataque e corpo separados (a tela desenha um de cada cor)
    void proxima (const Ajustes& a, double cTom, double gLo, double gHi, double gClick, double& click, double& corpo)
    {
        click = 0.0; corpo = 0.0;
        const Modelo& M = chute::modelo (modelo);
        // ataque: o modelo a 44,1 kHz, interpolado (Catmull-Rom)
        double gc = 0.0;
        if (t < clickFim + 0.005)
        {
            gc = t < clickFim ? 1.0 : 1.0 - (t - clickFim) / 0.005;
            const double p = t * M.taxaClick;
            const int i = (int) p;
            if (i + 2 < (int) M.click.size())
            {
                const double f = p - i;
                auto c = [&] (int k) { return k < 0 ? 0.0 : M.click[k] / 32767.0; };
                const double y0 = c (i - 1), y1 = c (i), y2 = c (i + 1), y3 = c (i + 2);
                const double x = y1 + 0.5 * f * (y2 - y0 + f * (2 * y0 - 5 * y1 + 4 * y2 - y3 + f * (3 * (y1 - y2) + y3 - y0)));
                lpTom += cTom * (x - lpTom);
                const double tom = gLo * lpTom + gHi * (x - lpTom);
                if (a.clickOn) click = gc * gClick * tom;
            }
        }
        // corpo
        const int k = (int) (t * 1000.0);
        if (k < len)
        {
            if (t >= t0)
            {
                const double fr = t * 1000.0 - k;
                const double amp = am[k] + fr * ((k + 1 < len ? am[k + 1] : 0.0f) - am[k]);
                const double peso = a.clickOn ? 1.0 - gc : 1.0;
                fase = ph[k] + fr * fq[k] * 0.001;   // frequência constante dentro do ms: fase exata
                if (a.bodyOn) corpo = peso * sinal * amp * std::sin (2.0 * pi * fase);
            }
        }
        click *= ganho * corte; corpo *= ganho * corte;
        if (corteVel > 0.0) { corte -= corteVel; if (corte <= 0.0) { corte = 0.0; ativa = false; } }
        t += dt;
        if (k >= len && t > clickFim + 0.006) ativa = false;
    }
};

// o que a tela lê (escrito pelo áudio, lido a 30 Hz)
struct Leitura
{
    std::atomic<float> notaMidi { 0.0f };   // nota da cauda do corpo agora (fracionária)
    std::atomic<float> fHz { 0.0f };
    std::atomic<float> pico { 0.0f };       // pico da saída (decai devagar, para o medidor)
    std::atomic<uint32_t> batidas { 0 };
    std::atomic<int> ultimaNota { -1 };     // última nota MIDI tocada
};

class Engine
{
public:
    Leitura leitura;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& v : voz) { v.ativa = false; v.dt = 1.0 / fs; }
        picoC = std::exp (-1.0 / (0.3 * fs));
        lp1 = lp2 = 0; ic1 = ic2 = 0;
        volSuave = -1.0; drvSuave = -1.0;
    }

    // dispara uma batida. Devolve a voz (para o LINK copiar as curvas)
    const Voz& noteOn (int nota, float vel, const Ajustes& a)
    {
        Voz* v = nullptr;
        for (auto& o : voz) if (o.ativa && o.corteVel == 0.0) o.corteVel = 1.0 / (0.003 * fs);   // a anterior some em 3 ms
        for (auto& o : voz) if (! o.ativa) { v = &o; break; }
        if (v == nullptr)   // todas ocupadas: a mais velha
        {
            v = &voz[0];
            for (auto& o : voz) if (o.t > v->t) v = &o;
        }
        const Modelo& M = modelo (a.modelo);
        const double fT = fCauda (a, nota);
        v->modelo = std::clamp (a.modelo, 0, numModelos() - 1);
        v->len = curvasCorpo (a, fT, v->fq, v->am);
        faseCorpo (a, v->fq, v->len, v->ph);
        v->fase = M.fase0;
        v->t = 0.0; v->dt = 1.0 / fs;
        v->ganho = 1.0 - std::clamp ((double) a.velo, 0.0, 1.0) * (1.0 - std::clamp ((double) vel, 0.0, 1.0));
        v->corte = 1.0; v->corteVel = 0.0;
        v->lpTom = 0.0;
        v->sinal = M.sinal; v->t0 = M.t0;
        v->clickFim = std::clamp ((double) a.clickMs, 2.0, 25.0) * 0.001;
        v->notaMidi = (float) ftom (fT);
        v->ativa = true;
        leitura.notaMidi = v->notaMidi;
        leitura.fHz = (float) fT;
        leitura.ultimaNota = nota;
        leitura.batidas.fetch_add (1);
        return *v;
    }

    void allNotesOff() { for (auto& v : voz) if (v.ativa) v.corteVel = 1.0 / (0.003 * fs); }

    // nch canais iguais (mono). ini/n: trecho do bloco
    void render (float* const* out, int nch, int ini, int n, const Ajustes& a)
    {
        const double cTom = 1.0 - std::exp (-2.0 * pi * 1500.0 / fs);
        const double gLo = std::pow (10.0, -a.clickTone / 40.0), gHi = std::pow (10.0, a.clickTone / 40.0);
        const double gClick = std::pow (10.0, a.clickDb / 20.0);
        const double volAlvo = std::pow (10.0, a.volumeDb / 20.0);
        if (volSuave < 0.0) volSuave = volAlvo;
        const double volC = std::exp (-1.0 / (0.01 * fs));
        // DRIVE: o knob anda suave (15 ms) e o ganho é recalculado a cada 16 amostras: girar não dá degrau nem estalo
        const double drvAlvo = std::clamp ((double) a.drive, 0.0, 1.0);
        if (drvSuave < 0.0) drvSuave = drvAlvo;
        const double drvC = std::exp (-1.0 / (0.015 * fs));
        // TONE: passa-baixa de 2ª ordem (TPT) só no que o DRIVE acrescenta; aberto (≥ 19,5 kHz) = não filtra
        const bool tomOn = a.toneHz < 19500.0f;
        const double g = std::tan (pi * std::min ((double) a.toneHz, fs * 0.45) / fs), k = 1.4142135;
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        double pre = 1.0, comp = 1.0, mol = 0.0;
        float pk = leitura.pico.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            double x = 0.0;
            for (auto& v : voz)
                if (v.ativa)
                {
                    double c, b;
                    v.proxima (a, cTom, gLo, gHi, gClick, c, b);
                    x += c + b;
                }
            drvSuave = drvAlvo + (drvSuave - drvAlvo) * drvC;
            if ((i & 15) == 0) ganhosDrive (a.driveTipo, drvSuave, pre, comp, mol);
            // o que o DRIVE acrescenta (ou tira) = saturado − limpo; o TONE filtra só essa diferença
            double dif = mol > 0.0 ? mol * (comp * forma (a.driveTipo, x * pre) - x) : 0.0;
            if (tomOn)
            {
                const double v3 = dif - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
                dif = v2;
            }
            else ic1 = ic2 = 0.0;
            x += dif;
            volSuave = volAlvo + (volSuave - volAlvo) * volC;
            const float s = (float) (x * volSuave);
            for (int c = 0; c < nch; ++c) out[c][ini + i] = s;
            pk = std::max (pk * (float) picoC, std::fabs (s));
        }
        leitura.pico.store (pk, std::memory_order_relaxed);
    }

    bool soando() const { for (auto& v : voz) if (v.ativa) return true; return false; }
    double taxa() const { return fs; }

    // WARM = tanh; HARD = joelho firme (x / (1 + x⁴)^¼: reto até perto de 1 e achata sem a quina do corte seco, que
    // gerava aliasing áspero); FOLD = seno (dobra a onda quando passa de 1)
    static double forma (int tipo, double x)
    {
        switch (tipo)
        {
            case driveHard: { const double x2 = x * x; return x / std::sqrt (std::sqrt (1.0 + x2 * x2)); }
            case driveFold: return std::sin (0.5 * pi * x);
            default:        return std::tanh (x);
        }
    }

    // ganhos do DRIVE para o knob d (0..1): pre = ganho antes da forma (curva d^1,5 até +18 dB; +12 no FOLD),
    // comp = um pico de 0,9 sai em ~0,9, mol = quanto do saturado entra (0 → 1 nos primeiros 10% do knob: do 0 para o
    // 1% não há salto)
    static void ganhosDrive (int tipo, double d, double& pre, double& comp, double& mol)
    {
        d = std::clamp (d, 0.0, 1.0);
        mol = std::min (1.0, d / 0.1);
        if (mol < 1.0e-4) { pre = 1.0; comp = 1.0; mol = 0.0; return; }
        const double maxDb = tipo == driveFold ? 12.0 : 18.0;
        pre = std::pow (10.0, maxDb * std::pow (d, 1.5) / 20.0);
        // FOLD: o seno sobe e desce com o ganho; a compensação usa só a subida até o 1º pico (depois fica fixa), senão
        // ela pularia de ×1 para ×3 a cada dobra enquanto o knob gira
        if (tipo == driveFold) comp = 0.9 / std::sin (std::min (0.5 * pi * 0.9 * pre, 0.5 * pi));
        else comp = 0.9 / std::max (0.3, std::fabs (forma (tipo, 0.9 * pre)));
    }

    // a batida isolada, para a tela: ataque e corpo separados (antes do DRIVE / TONE / VOLUME), n amostras a fs
    static int previa (const Ajustes& a, int nota, double fs, float* click, float* corpo, int n)
    {
        static Voz v;   // grande demais para a pilha; só a tela (thread da mensagem) usa
        const Modelo& M = modelo (a.modelo);
        v.modelo = std::clamp (a.modelo, 0, numModelos() - 1);
        v.len = curvasCorpo (a, fCauda (a, nota), v.fq, v.am);
        faseCorpo (a, v.fq, v.len, v.ph);
        v.fase = M.fase0; v.t = 0; v.dt = 1.0 / fs; v.ganho = 1; v.corte = 1; v.corteVel = 0; v.lpTom = 0;
        v.sinal = M.sinal; v.t0 = M.t0; v.clickFim = std::clamp ((double) a.clickMs, 2.0, 25.0) * 0.001; v.ativa = true;
        const double cTom = 1.0 - std::exp (-2.0 * pi * 1500.0 / fs);
        const double gLo = std::pow (10.0, -a.clickTone / 40.0), gHi = std::pow (10.0, a.clickTone / 40.0);
        const double gClick = std::pow (10.0, a.clickDb / 20.0);
        int fim = 0;
        for (int i = 0; i < n; ++i)
        {
            double c = 0, b = 0;
            if (v.ativa) { v.proxima (a, cTom, gLo, gHi, gClick, c, b); fim = i + 1; }
            click[i] = (float) c; corpo[i] = (float) b;
        }
        return fim;
    }

private:
    double fs = 48000.0;
    Voz voz[maxVozes];
    double picoC = 0.999, lp1 = 0, lp2 = 0, ic1 = 0, ic2 = 0, volSuave = -1.0, drvSuave = -1.0;
};
} // namespace chute
