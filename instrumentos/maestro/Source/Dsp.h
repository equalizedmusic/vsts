#pragma once
// MAESTRO — motor (C++ puro, sem JUCE: o teste tools/teste_dsp.cpp compila este arquivo sozinho).
//
// Uma instância por faixa. Todas as instâncias do mesmo binário, no mesmo processo do DAW, se enxergam pelo
// Registry (memória estática do binário): cada uma publica o espectro em 27 bandas de 1/3 de oitava (40 Hz–16 kHz),
// o nível, o papel (ROLE) e se está em FOCUS; e lê o que as outras publicaram.
//
// Pensado para GRUPOS (stems): o produtor faz a pré-mix de cada canal e joga em 8 grupos (KICK, BASS, DRUMS, HATS,
// SYNTHS, AMBIENCE, VOCAL, FX); o MAESTRO vai em cada grupo e reconhece o grupo pelo nome. Funciona também canal a canal.
//
//  LEARN  (vale para o projeto todo): cada faixa mede o nível e o espectro médios enquanto toca (só quadros com som).
//  LOCK   cada faixa calcula sozinha, com o que todas aprenderam:
//           - LEVEL: o ROLE inteiro (soma de potência das faixas do ROLE) vai para o alvo do STYLE, relativo ao KICK.
//             Todas as faixas do mesmo ROLE andam juntas: o equilíbrio interno (34 percussões) fica como estava.
//           - TONE MATCH: EQ estática suave (9 bandas de 1 oitava) puxando a forma do espectro para a do ROLE.
//           - Trocar o ROLE de uma faixa depois do LOCK refaz o LOCK de todas (com o que aprenderam no LEARN).
//  LOW-END GUARD: shelf DINÂMICO abaixo de 120 Hz em quem não é KICK nem BASS, só enquanto o KICK / BASS toca
//         (sem passa-altas em ninguém; KICK e BASS nunca são cortados por ele).
//  FOCUS  (por faixa, quantas quiser): as outras cedem espaço só onde e quando a faixa em FOCUS toca — EQ dinâmica nas mesmas
//         9 bandas (corte proporcional ao quanto mascaram a FOCUS naquela banda, até MAX CUT). Com KEEP KICK / BASS
//         ON TOP, KICK e BASS valem como FOCUS abaixo de ~400 Hz. Duas FOCUS que batem na mesma banda cedem um pouco
//         uma para a outra, por igual (até 3 dB).
//
// Áudio: só IIR (biquads RBJ de fase mínima): latência ZERO. Sem LOCK e sem FOCUS, a saída é a entrada, idêntica.
// A análise (FFT de 2048, salto de 1024) roda na própria thread de áudio de cada faixa, sobre a ENTRADA (antes do
// MAESTRO): o que uma faixa corta não muda o que as outras medem.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace maestro
{
constexpr int NB = 27;          // bandas de análise (1/3 de oitava): 40·2^(b/3) Hz
constexpr int NE = 9;           // bandas de EQ (1 oitava): centro = banda de análise 3k+1 (50, 100, 200 … 12,8 kHz)
constexpr int NR = 8;           // papéis = os 8 grupos (stems)
constexpr int NS = 256;         // vagas no Registry
constexpr int NSTYLE = 6;
constexpr int FFTN = 2048, HOP = 1024;
constexpr int kTopo = 10;       // KEEP KICK / BASS ON TOP vale até a banda 10 (~400 Hz)
constexpr double kPi = 3.14159265358979323846;

enum Role { KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX };
enum Fonte { MANUAL = 0, PELO_NOME = 1, PELO_SOM = 2 };

inline const char* nomeRole (int r)
{
    static const char* n[NR] = { "KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "AMBIENCE", "VOCAL", "FX" };
    return r >= 0 && r < NR ? n[r] : "?";
}
inline const char* nomeStyle (int s)
{
    static const char* n[NSTYLE] = { "PSYTRANCE", "PROG PSY", "TECHNO", "HOUSE", "DRUM & BASS", "POP" };
    return s >= 0 && s < NSTYLE ? n[s] : "?";
}
inline double freqBanda (int b) { return 40.0 * std::pow (2.0, b / 3.0); }
inline double freqEq (int k) { return freqBanda (3 * k + 1); }

// nível de cada ROLE (soma das faixas do ROLE) relativo ao KICK, por STYLE
inline float alvoNivel (int style, int role)
{
    static const float t[NSTYLE][NR] = {
        //  KICK  BASS  DRUMS   HATS  SYNTHS   AMB   VOCAL    FX
        {  0.0f, -1.0f, -9.0f, -12.0f, -4.0f, -12.0f, -6.0f, -10.0f },   // PSYTRANCE
        {  0.0f, -2.0f, -9.0f, -12.0f, -5.0f, -10.0f, -5.0f, -11.0f },   // PROG PSY
        {  0.0f, -3.0f, -7.0f, -10.0f, -7.0f, -11.0f, -7.0f, -10.0f },   // TECHNO
        {  0.0f, -3.0f, -8.0f, -10.0f, -6.0f, -11.0f, -2.0f, -11.0f },   // HOUSE
        { -1.0f,  0.0f, -5.0f,  -9.0f, -6.0f, -11.0f, -4.0f, -10.0f },   // DRUM & BASS
        { -2.0f, -3.0f, -9.0f, -11.0f, -5.0f, -11.0f,  0.0f, -12.0f } }; // POP
    return t[style < 0 || style >= NSTYLE ? 0 : style][role < 0 || role >= NR ? 0 : role];
}

// forma de espectro de cada ROLE (dB por banda de EQ, 50 Hz … 12,8 kHz); o TONE MATCH só olha a FORMA
inline float alvoTom (int role, int k)
{
    static const float t[NR][NE] = {
        {   0,  -1,  -8, -14, -16, -14, -12, -16, -22 },   // KICK: corpo 50–100, clique 3–5 k
        {  -2,   0,  -4, -10, -14, -16, -20, -26, -30 },   // BASS
        { -20, -12,  -6,  -4,  -4,  -4,  -4,  -6, -10 },   // DRUMS (snare, clap, percs)
        { -36, -30, -24, -18, -12,  -6,  -2,   0,  -2 },   // HATS (hats, ride, pratos)
        { -30, -18,  -6,  -2,   0,   0,  -2,  -6, -10 },   // SYNTHS (lead, acid, arp, pluck)
        { -24, -14,  -6,  -3,  -2,  -3,  -4,  -5,  -6 },   // AMBIENCE (pads, atmos, texturas)
        { -30, -18,  -6,  -2,   0,   0,  -2,  -6, -10 },   // VOCAL
        { -24, -14,  -8,  -4,  -2,  -2,  -2,  -4,  -6 } }; // FX
    return t[role < 0 || role >= NR ? 0 : role][k];
}

inline float dbDe (double p) { return p > 1e-20 ? (float) (10.0 * std::log10 (p)) : -200.0f; }
inline double potDe (double db) { return std::pow (10.0, db / 10.0); }
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// papel pelo nome da faixa (minúsculas); −1 = não reconheceu
inline int roleDoNome (const char* nome)
{
    if (nome == nullptr || nome[0] == 0) return -1;
    char s[64];
    int n = 0;
    for (; nome[n] && n < 63; ++n) s[n] = (char) ((nome[n] >= 'A' && nome[n] <= 'Z') ? nome[n] + 32 : nome[n]);
    s[n] = 0;
    auto tem = [&] (const char* k) { return std::strstr (s, k) != nullptr; };
    if (tem ("kick") || tem ("kik") || tem ("bd ") || std::strcmp (s, "bd") == 0 || tem ("bumbo")) return KICK;
    if (tem ("bass") || tem ("sub") || tem ("baixo") || tem ("808")) return BASS;
    if (tem ("vox") || tem ("vocal") || tem ("voice") || tem ("voc") || tem ("speech") || tem ("voz")) return VOCAL;
    if (tem ("hat") || tem ("hh") || tem ("ride") || tem ("cymb") || tem ("crash") || tem ("prato") || tem ("shaker")) return HATS;
    if (tem ("drum") || tem ("snare") || tem ("snr") || tem ("clap") || tem ("perc") || tem ("tom") || tem ("rim")
        || tem ("top") || tem ("conga") || tem ("bongo") || tem ("caixa") || tem ("bateria")) return DRUMS;
    if (tem ("fx") || tem ("riser") || tem ("sweep") || tem ("impact") || tem ("uplift") || tem ("downlift") || tem ("whoosh")
        || tem ("noise") || tem ("fill") || tem ("reverse") || tem ("zap") || tem ("efeito")) return FX;
    if (tem ("amb") || tem ("atmo") || tem ("pad") || tem ("drone") || tem ("texture") || tem ("space") || tem ("choir")) return AMBIENCE;
    if (tem ("synth") || tem ("lead") || tem ("acid") || tem ("303") || tem ("arp") || tem ("pluck") || tem ("key") || tem ("seq")
        || tem ("melod") || tem ("stab") || tem ("piano") || tem ("guitar") || tem ("chord") || tem ("string") || tem ("squelch")) return SYNTHS;
    return -1;
}

// ------------------------------------------------------------------------------------------------------------------
// Registry: o que cada faixa publica para as outras. Escrito por uma thread (a de áudio da dona; nome e dono pela
// thread da interface), lido por todas. Só atômicos: leitura rasgada entre dois quadros não faz mal (é medida).
// ------------------------------------------------------------------------------------------------------------------
struct Slot
{
    std::atomic<int> usado { 0 };
    std::atomic<void*> dono { nullptr };      // MaestroProcessor* (só a thread da interface usa)
    std::atomic<int> ordem { 0 };             // ordem de chegada (identidade na sessão)
    std::atomic<int> role { AMBIENCE };       // papel resolvido (AUTO já decidido)
    std::atomic<int> roleAuto { 0 };          // de onde veio: MANUAL, PELO_NOME, PELO_SOM
    std::atomic<unsigned> uid { 0 };          // identidade gravada no projeto (faixa duplicada = uid repetido)
    std::atomic<int> focus { 0 };
    std::atomic<int> nomeSeq { 0 };
    char nome[32] {};                         // nome da faixa no DAW (UTF-8; vazio = não se sabe)

    // vivo
    std::atomic<float> vivo[NB];              // potência por banda, envoltória rápida, JÁ com o ganho estático (LOCK)
    std::atomic<float> tela[NB];              // potência por banda, média lenta (~300 ms), da entrada (mapa)
    std::atomic<float> nivelDb { -200.0f };   // nível K da entrada, lento
    std::atomic<int> tocando { 0 };

    // LEARN
    std::atomic<float> aprPot[NB];            // potência média por banda nos quadros com som
    std::atomic<float> aprNivelDb { -200.0f };
    std::atomic<int> aprQuadros { 0 };

    // resultado (para as telas das outras)
    std::atomic<float> ganhoDb { 0.0f };      // LEVEL aplicado agora (com AMOUNT e TRIM)
    std::atomic<float> corte[NE];             // corte dinâmico aplicado agora (dB, ≤ 0)
    std::atomic<float> tom[NE];               // TONE MATCH aplicado agora (dB)
    std::atomic<int> cedePara { -1 };         // vaga da FOCUS que mais pesa no corte (−1 = não cede)
    static constexpr int NCEDE = 6;
    std::atomic<int> cede[NCEDE];             // todas as FOCUS para as quais cede agora (vagas)
    std::atomic<int> nCede { 0 };
    std::atomic<float> shelf { 0.0f };        // LOW-END GUARD aplicado agora (dB, ≤ 0)
    std::atomic<int> mascaraFoco { 0 };       // bandas de EQ (bits) em que o corte é para a FOCUS

    Slot()
    {
        for (int b = 0; b < NB; ++b) { vivo[b] = 0.0f; tela[b] = 0.0f; aprPot[b] = 0.0f; }
        for (int k = 0; k < NE; ++k) { corte[k] = 0.0f; tom[k] = 0.0f; }
        for (int k = 0; k < NCEDE; ++k) cede[k] = -1;
    }

    void escreveNome (const char* s)
    {
        nomeSeq.fetch_add (1);
        std::snprintf (nome, sizeof (nome), "%s", s != nullptr ? s : "");
        nomeSeq.fetch_add (1);
    }
    void leNome (char* out, int n) const
    {
        for (int t = 0; t < 4; ++t)
        {
            const int a = nomeSeq.load();
            std::snprintf (out, (size_t) n, "%s", nome);
            if ((a & 1) == 0 && a == nomeSeq.load()) return;
        }
    }
};

struct Registry
{
    // estado global do projeto (gravado no estado de cada faixa; quem abre o projeto devolve aqui)
    std::atomic<int> modo { 0 };              // 0 = parado, 1 = LEARN, 2 = LOCK
    std::atomic<int> genLearn { 0 }, genLock { 0 }, genReset { 0 };
    std::atomic<double> learnIni { 0.0 };     // relógio (s) do início do LEARN (tela)
    std::atomic<int> style { 0 };
    std::atomic<float> amount { 0.75f };      // 0..1
    std::atomic<float> maxCut { 6.0f };       // dB
    std::atomic<int> speed { 1 };             // 0 SLOW, 1 MED, 2 FAST
    std::atomic<int> guard { 1 };             // LOW-END GUARD (shelf dinâmico < 120 Hz)
    std::atomic<int> genRoles { 0 };          // muda quando o ROLE de uma faixa muda depois do LOCK (refaz o LOCK)
    std::atomic<int> topo { 1 };              // KEEP KICK / BASS ON TOP
    std::atomic<int> modoB { 0 };             // A/B: 1 = B (tudo como era, para comparar)
    std::atomic<int> versao { 0 };            // muda quando uma faixa entra ou sai
    std::atomic<int> proxOrdem { 1 };
    std::atomic<int> trava { 0 };

    Slot slot[NS];

    static Registry& get() { static Registry r; return r; }

    int entra()
    {
        int z = 0;
        while (! trava.compare_exchange_weak (z, 1)) z = 0;
        int achou = -1;
        for (int i = 0; i < NS; ++i)
            if (slot[i].usado.load() == 0)
            {
                auto& s = slot[i];
                for (int b = 0; b < NB; ++b) { s.vivo[b] = 0.0f; s.tela[b] = 0.0f; s.aprPot[b] = 0.0f; }
                for (int k = 0; k < NE; ++k) { s.corte[k] = 0.0f; s.tom[k] = 0.0f; }
                s.nivelDb = -200.0f; s.tocando = 0; s.aprNivelDb = -200.0f; s.aprQuadros = 0;
                s.ganhoDb = 0.0f; s.cedePara = -1; s.mascaraFoco = 0; s.focus = 0; s.role = AMBIENCE; s.roleAuto = PELO_SOM; s.dono = nullptr;
                s.nCede = 0; s.shelf = 0.0f; s.uid = 0;
                s.escreveNome ("");
                s.ordem = proxOrdem.fetch_add (1);
                s.usado = 1;
                achou = i;
                break;
            }
        trava.store (0);
        ++versao;
        return achou;
    }
    void sai (int i)
    {
        if (i < 0 || i >= NS) return;
        slot[i].focus = 0;
        slot[i].dono = nullptr;
        slot[i].usado = 0;
        ++versao;
    }
    int ligadas() const
    {
        int n = 0;
        for (auto& s : slot) n += s.usado.load();
        return n;
    }

    // botões globais
    void learn (double agora) { learnIni = agora; ++genLearn; modo = 1; }
    void lock() { if (modo.load() == 1) { modo = 2; ++genLock; } }
    void reset() { modo = 0; ++genReset; }
};

// ------------------------------------------------------------------------------------------------------------------
// FFT real de 2048 (complexa de 1024 + desdobramento)
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

    // x: FFTN amostras reais; pot: FFTN/2+1 valores |X[k]|². Aritmética à mão (std::complex sem fast-math confere NaN
    // em cada multiplicação e custa o dobro)
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
            const float ar = re[a], ai = im[a], br = re[b], bi = -im[b];          // b = conj (Z[M-k])
            const float pr = 0.5f * (ar + br), pi = 0.5f * (ai + bi);            // par
            const float dr = ar - br, di = ai - bi;                              // (a - b)
            const float ir = 0.5f * di, ii = -0.5f * dr;                         // ímpar = (a - b) · (−i/2)
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
// biquad RBJ (TDF-II, estado em double)
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
inline Coef passaAltas (double fs, double f, double q)
{
    const double w = 2.0 * kPi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    Coef k;
    k.b0 = (1.0 + c) / 2.0 / a0; k.b1 = -(1.0 + c) / a0; k.b2 = k.b0;
    k.a1 = -2.0 * c / a0; k.a2 = (1.0 - al) / a0;
    return k;
}
// shelf de graves RBJ (S = 1)
inline Coef shelfGrave (double fs, double f, double db)
{
    const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * f / fs, c = std::cos (w);
    const double al = std::sin (w) / 2.0 * std::sqrt (2.0), r = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1) + (A - 1) * c + r;
    Coef k;
    k.b0 = A * ((A + 1) - (A - 1) * c + r) / a0;
    k.b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
    k.b2 = A * ((A + 1) - (A - 1) * c - r) / a0;
    k.a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
    k.a2 = ((A + 1) + (A - 1) * c - r) / a0;
    return k;
}
// |H(f)| em dB (para as telas)
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

constexpr double qOitava = 1.414;   // largura de 1 oitava
constexpr double fGuard = 120.0;      // LOW-END GUARD: shelf de graves em 120 Hz
constexpr int kGuard = 4;             // bandas de análise 0..4 (até ~113 Hz) = o grave do KICK / BASS
constexpr float kShelfMax = 12.0f;    // dB, com AMOUNT 100 %

// ponderação K (BS.1770) no centro de cada banda: o nível "como se ouve"
inline float pesoK (int b)
{
    const double fs = 48000.0, f = freqBanda (b);
    Coef s1, s2;
    s1.b0 = 1.53512485958697; s1.b1 = -2.69169618940638; s1.b2 = 1.19839281085285; s1.a1 = -1.69065929318241; s1.a2 = 0.73248077421585;
    s2.b0 = 1.0; s2.b1 = -2.0; s2.b2 = 1.0; s2.a1 = -1.99004745483398; s2.a2 = 0.99007225036621;
    return (float) potDe (respostaDb (s1, fs, f) + respostaDb (s2, fs, f));
}

// ------------------------------------------------------------------------------------------------------------------
// Ajustes que vêm dos parâmetros da faixa
// ------------------------------------------------------------------------------------------------------------------
struct Ajustes
{
    int roleParam = -1;     // −1 = AUTO
    bool focus = false;
    float trimDb = 0.0f;
};

// o que a faixa guarda no projeto
struct Travado
{
    bool travado = false;
    float ganhoBruto = 0.0f;          // dB com AMOUNT 100 %
    float tomBruto[NE] {};            // dB com AMOUNT 100 %
    float nivelAntes = -200.0f;       // nível aprendido (tela)
    float alvoNivel = -200.0f;        // nível alvo calculado (tela)
};

class Engine
{
public:
    Engine()
    {
        auto& R = Registry::get();
        vaga = R.entra();
        if (vaga < 0) { vagaLocal = std::make_unique<Slot>(); }
        janela.resize (FFTN);
        for (int i = 0; i < FFTN; ++i) janela[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / FFTN));
        double w2 = 0;
        for (float w : janela) w2 += (double) w * w;
        normPot = (float) (2.0 / ((double) FFTN * w2));   // |X|² → potência média (seno de pico 1 = 0,5)
        anel.assign (FFTN, 0.0f);
        quadro.assign (FFTN, 0.0f);
        pot.assign (FFTN / 2 + 1, 0.0f);
        for (int b = 0; b < NB; ++b) kPeso[b] = pesoK (b);
        genLearnVisto = R.genLearn.load();
        genLockVisto = R.genLock.load();
        genResetVisto = R.genReset.load();
        genRolesVisto = R.genRoles.load();
        prepare (48000.0);
    }
    ~Engine() { Registry::get().sai (vaga); }

    Slot& slot() { return vaga >= 0 ? Registry::get().slot[vaga] : *vagaLocal; }
    int indice() const { return vaga; }

    void prepare (double fs_)
    {
        fs = fs_ > 1000.0 ? fs_ : 48000.0;
        // pesos de cada bin em cada banda (sobreposição do bin com a banda, em Hz)
        const double df = fs / FFTN;
        for (int b = 0; b < NB; ++b)
        {
            pesos[b].clear();
            // a primeira banda desce até 20 Hz e a última vai até Nyquist: nada da energia fica de fora
            const double lo = b == 0 ? 20.0 : freqBanda (b) * std::pow (2.0, -1.0 / 6.0);
            const double hi = b == NB - 1 ? fs / 2 : freqBanda (b) * std::pow (2.0, 1.0 / 6.0);
            for (int k = 1; k < FFTN / 2; ++k)
            {
                const double a = (k - 0.5) * df, z = (k + 0.5) * df;
                const double sob = std::min (z, hi) - std::max (a, lo);
                if (sob > 0) pesos[b].push_back ({ k, (float) (sob / df) });
            }
        }
        for (int k = 0; k < NE; ++k) { eq[k].zera(); gEq[k] = 0.0f; ativa[k] = false; }
        sh.zera();
        shelfSuave = 0.0f; shelfEq = 0.0f; shelfAtivo = false; alvoShelf = 0.0f;
        ganhoLin = 1.0f;
        posAnel = 0; cheio = 0;
        calculaEq (true);
    }

    // estado salvo no projeto
    Travado travado;

    // ------------------------------------------------------------------------------------------------------
    void process (float* const* ch, int nch, int n, const Ajustes& a)
    {
        auto& R = Registry::get();
        auto& S = slot();
        S.focus = a.focus ? 1 : 0;
        roleParam = a.roleParam;

        // botões globais (LEARN / LOCK / RESET ALL) que alguma faixa apertou
        if (const int g = R.genLearn.load(); g != genLearnVisto) { genLearnVisto = g; zeraAprendido(); }
        if (const int g = R.genReset.load(); g != genResetVisto) { genResetVisto = g; travado = Travado {}; zeraAprendido(); }
        if (const int g = R.genLock.load(); g != genLockVisto) { genLockVisto = g; trava(); }
        // ROLE mudou depois do LOCK (nesta ou em outra faixa): refaz o LOCK com o que aprendeu (sem o LEARN, fica)
        if (const int g = R.genRoles.load(); g != genRolesVisto) { genRolesVisto = g; if (travado.travado && nApr > 0) trava(); }
        aprendendo = R.modo.load() == 1;

        // análise da ENTRADA (mono), quadro a cada HOP amostras
        for (int i = 0; i < n; ++i)
        {
            float m = ch[0][i];
            if (nch > 1) m = 0.5f * (m + ch[1][i]);
            anel[(size_t) posAnel] = m;
            posAnel = (posAnel + 1) % FFTN;
            if (++cheio >= HOP) { cheio = 0; analisa (a); }
        }

        // alvos do áudio
        const bool B = R.modoB.load() != 0;
        const float amt = clampf (R.amount.load(), 0.0f, 1.0f);
        const bool trav = travado.travado;
        const int role = S.role.load();
        float alvoG = 0.0f;
        if (! B) alvoG = (trav ? amt * travado.ganhoBruto : 0.0f) + a.trimDb;
        (void) role;
        for (int k = 0; k < NE; ++k)
            alvoTomK[k] = (! B && trav) ? amt * travado.tomBruto[k] : 0.0f;

        // ballística do corte dinâmico por sub-bloco de 32
        const int sp = R.speed.load();
        const float tAt = sp == 2 ? 0.005f : (sp == 0 ? 0.040f : 0.015f);
        const float tRel = sp == 2 ? 0.060f : (sp == 0 ? 0.300f : 0.120f);
        constexpr int SUB = 32;
        const float cAt = 1.0f - std::exp (-SUB / (tAt * (float) fs));
        const float cRel = 1.0f - std::exp (-SUB / (tRel * (float) fs));
        const float cEst = 1.0f - std::exp (-SUB / (0.050f * (float) fs));   // estático (TONE, LOCK): 50 ms
        const double alvoGLin = std::pow (10.0, alvoG / 20.0);
        const double cG = 1.0 - std::exp (-1.0 / (0.030 * fs));

        for (int i0 = 0; i0 < n; i0 += SUB)
        {
            const int m = std::min (SUB, n - i0);
            for (int k = 0; k < NE; ++k)
            {
                const float alvoC = B ? 0.0f : alvoCorte[k];
                corteSuave[k] += (alvoC < corteSuave[k] ? cAt : cRel) * (alvoC - corteSuave[k]);
                tomSuave[k] += cEst * (alvoTomK[k] - tomSuave[k]);
                if (std::abs (corteSuave[k]) < 0.01f && alvoC == 0.0f) corteSuave[k] = 0.0f;   // 0,01 dB: inaudível
                if (std::abs (tomSuave[k] - alvoTomK[k]) < 0.01f) tomSuave[k] = alvoTomK[k];
            }
            {
                const float alvoS = B ? 0.0f : alvoShelf;
                shelfSuave += (alvoS < shelfSuave ? cAt : cRel) * (alvoS - shelfSuave);
                if (std::abs (shelfSuave) < 0.01f && alvoS == 0.0f) shelfSuave = 0.0f;
            }
            calculaEq (false);
            for (int c = 0; c < nch; ++c)
            {
                float* x = ch[c] + i0;
                for (int k = 0; k < NE; ++k)
                    if (ativa[k])
                        for (int i = 0; i < m; ++i) x[i] = eq[k].passa (x[i], c);
            }
            // LOW-END GUARD: shelf dinâmico de graves
            if (shelfAtivo)
                for (int c = 0; c < nch; ++c)
                {
                    float* x = ch[c] + i0;
                    for (int i = 0; i < m; ++i) x[i] = sh.passa (x[i], c);
                }
            // LEVEL (+ TRIM)
            if (ganhoLin != 1.0f || alvoGLin != 1.0f)
            {
                double g = ganhoLin;
                for (int i = 0; i < m; ++i)
                {
                    g += cG * (alvoGLin - g);
                    for (int c = 0; c < nch; ++c) ch[c][i0 + i] = (float) (ch[c][i0 + i] * g);
                }
                ganhoLin = std::abs (g - alvoGLin) < 1e-6 * alvoGLin ? alvoGLin : g;
            }
        }

        S.ganhoDb = (float) (20.0 * std::log10 (std::max (1e-6, ganhoLin)));
        for (int k = 0; k < NE; ++k) { S.corte[k] = corteSuave[k]; S.tom[k] = tomSuave[k]; }
        S.shelf = shelfSuave;
    }

    double taxa() const { return fs; }
    bool aprendendoAgora() const { return aprendendo; }
    float guardAgora() const { return shelfSuave; }   // dB (≤ 0)

    // curva da EQ que está valendo agora (tom + corte + guard), para a tela
    double respostaAgora (double f, bool soTom) const
    {
        double db = 0;
        for (int k = 0; k < NE; ++k)
        {
            const float g = soTom ? tomSuave[k] : tomSuave[k] + corteSuave[k];
            if (std::abs (g) > 1e-3f) db += respostaDb (sino (fs, freqEq (k), qOitava, g), fs, f);
        }
        if (! soTom && shelfSuave < -0.01f) db += respostaDb (shelfGrave (fs, fGuard, shelfSuave), fs, f);
        return db;
    }

    // nome vindo do DAW (thread da interface)
    void defineNome (const char* s)
    {
        slot().escreveNome (s);
        nomeRole = roleDoNome (s);
    }

private:
    struct Peso { int bin; float w; };

    void zeraAprendido()
    {
        for (int b = 0; b < NB; ++b) somaApr[b] = 0.0;
        somaNivel = 0.0; nApr = 0; nPerc = 0; nQuadros = 0; nNotas = 0; nPlano = 0;
        somaPlano = somaNiv = somaNiv2 = somaBaixo = 0.0;
        auto& S = slot();
        for (int b = 0; b < NB; ++b) S.aprPot[b] = 0.0f;
        S.aprNivelDb = -200.0f;
        S.aprQuadros = 0;
    }

    void analisa (const Ajustes& a)
    {
        auto& R = Registry::get();
        auto& S = slot();
        // quadro: as últimas FFTN amostras do anel, com janela
        float pico = 0.0f;
        for (int i = 0; i < FFTN; ++i)
        {
            const float v = anel[(size_t) ((posAnel + i) % FFTN)];
            pico = std::max (pico, std::abs (v));
            quadro[(size_t) i] = v * janela[(size_t) i];
        }
        float banda[NB];
        double total = 0.0;
        float plano = -1.0f;   // planura dentro das bandas (ruído ≈ 0,56, tom ≈ 0); −1 = não medida
        if (pico < 1e-5f)
            for (int b = 0; b < NB; ++b) banda[b] = 0.0f;
        else
        {
            fft.potencia (quadro.data(), pot.data());
            float pkBanda = 0.0f;
            for (int b = 0; b < NB; ++b)
            {
                double s = 0;
                for (auto& p : pesos[b]) s += (double) pot[(size_t) p.bin] * p.w;
                banda[b] = (float) (s * normPot);
                total += banda[b] * kPeso[b];
                pkBanda = std::max (pkBanda, banda[b]);
            }
            if (nQuadros < 3000 && a.roleParam < 0 && nomeRole.load() < 0)   // só enquanto o AUTO precisa (~1 min de som)
            {
                // planura DENTRO de cada banda (tom = picos entre os bins), média das bandas a até 40 dB do pico:
                // não depende da inclinação do espectro (um ruído escuro continua sendo ruído)
                double soma = 0;
                int nb = 0;
                for (int b = 0; b < NB; ++b)
                {
                    if (pesos[b].size() < 4 || banda[b] < pkBanda * 1e-4f) continue;
                    double sl = 0, sp = 0;
                    for (auto& pp : pesos[b]) { const double v = (double) pot[(size_t) pp.bin] + 1e-20; sl += std::log (v); sp += v; }
                    const double n = (double) pesos[b].size();
                    soma += std::exp (sl / n) / (sp / n);
                    ++nb;
                }
                if (nb > 0) plano = (float) (soma / nb);
            }
        }
        const float nivel = dbDe (total);
        const bool som = nivel > -60.0f;

        // espectro lento (mapa) e nível
        const float cTela = 1.0f - std::exp (-(float) HOP / (0.30f * (float) fs));
        for (int b = 0; b < NB; ++b) telaP[b] += cTela * (banda[b] - telaP[b]);
        nivelLento += cTela * ((float) total - nivelLento);
        S.nivelDb = dbDe (nivelLento);
        S.tocando = som ? 1 : 0;

        // medidas do som para o ROLE AUTO: ataques (salto de nível), notas (mudança de espectro), planura, nível
        if (som && nQuadros < 3000)
        {
            ++nQuadros;
            if (nivel > nivelAnt + 6.0f) ++nPerc;
            float fluxo = 0.0f;
            for (int b = 3; b < NB; ++b)
            {
                const float d = dbDe (banda[b] + 1e-12) - dbDe (bandaAnt[b] + 1e-12);
                if (dbDe (banda[b]) > nivel - 40.0f && d > 6.0f) fluxo += std::min (d, 20.0f);   // ruído oscila menos que 6 dB
            }
            if (fluxo > 30.0f) ++nNotas;
            if (plano >= 0) { somaPlano += plano; ++nPlano; }
            somaNiv += nivel; somaNiv2 += (double) nivel * nivel;
            double tt = 0, bx = 0;
            for (int b = 0; b < NB; ++b) { tt += banda[b]; if (freqBanda (b) < 120.0) bx += banda[b]; }
            if (tt > 0) somaBaixo += bx / tt;
        }
        nivelAnt = nivel;
        for (int b = 0; b < NB; ++b) bandaAnt[b] = banda[b];

        // LEARN: média de potência nos quadros com som
        if (aprendendo && som)
        {
            for (int b = 0; b < NB; ++b) somaApr[b] += banda[b];
            somaNivel += total;
            ++nApr;
            for (int b = 0; b < NB; ++b) S.aprPot[b] = (float) (somaApr[b] / nApr);
            S.aprNivelDb = dbDe (somaNivel / nApr);
            S.aprQuadros = nApr;
        }

        // papel: o escolhido, ou AUTO (nome da faixa; senão o espectro)
        int role = a.roleParam;
        S.roleAuto = role >= 0 ? MANUAL : (nomeRole.load() >= 0 ? PELO_NOME : PELO_SOM);
        if (role < 0) role = nomeRole.load() >= 0 ? nomeRole.load() : roleDoEspectro();
        S.role = role;
        // ROLE mudou depois do LOCK (escolhido na mão, nome que chegou): todas refazem o LOCK no próximo bloco
        if (travado.travado && roleUltimo >= 0 && role != roleUltimo && R.modo.load() == 2) ++R.genRoles;
        roleUltimo = role;

        // vivo (com o ganho estático já aplicado): sobe na hora e cai 4,6 dB por quadro (a ballística fica no corte)
        const float dec = 0.35f;
        const float gEst = travado.travado ? std::pow (10.0f, (R.amount.load() * travado.ganhoBruto + a.trimDb) / 10.0f)
                                           : std::pow (10.0f, a.trimDb / 10.0f);
        for (int b = 0; b < NB; ++b)
        {
            const int k = std::min (NE - 1, b / 3);
            const float gTom = travado.travado ? std::pow (10.0f, R.amount.load() * travado.tomBruto[k] / 10.0f) : 1.0f;
            const float v = banda[b] * gEst * gTom;
            envVivo[b] = std::max (v, envVivo[b] * dec);
            S.vivo[b] = envVivo[b];
            S.tela[b] = telaP[b];
        }

        cedeEspaco (a, role);
        guardaGrave (role, S.roleAuto.load() == PELO_SOM);
    }

    // ROLE pelo som (quando o nome da faixa não diz): ataques e notas por segundo, planura, centro do espectro
    int roleDoEspectro() const
    {
        if (nQuadros < 40) return roleSom >= 0 ? roleSom : AMBIENCE;
        double tot = 0, cent = 0;
        for (int b = 0; b < NB; ++b)
        {
            const double p = nApr > 0 ? somaApr[b] / nApr : telaP[b];
            tot += p;
            cent += p * std::log2 (freqBanda (b));
        }
        if (tot < 1e-12) return AMBIENCE;
        const double fc = std::pow (2.0, cent / tot);
        const double seg = nQuadros * (double) HOP / fs;
        const double ataques = nPerc / seg, notas = nNotas / seg;
        const double baixo = somaBaixo / nQuadros;
        const double plano = nPlano > 0 ? somaPlano / nPlano : 0.0;
        const double media = somaNiv / nQuadros, desvio = std::sqrt (std::max (0.0, somaNiv2 / nQuadros - media * media));
        int r;
        if (baixo > 0.5) r = (ataques > 1.2 && ataques < 4.5) ? KICK : BASS;   // bumbo: ~1 ataque por tempo
        else if (plano > 0.38 && ataques > 0.8)                               // ruído com ataques: bateria
            r = ataques < 1.8 ? DRUMS : (fc > 4000.0 ? HATS : DRUMS);         // caixa / clap no 2 e 4; hats e ride mais
        else if (fc > 5000.0) r = HATS;                                        // pratos: quase tudo no agudo
        else if (plano > 0.38) r = desvio > 4.0 ? FX : AMBIENCE;              // ruído sem ataque
        else if (fc > 3500.0) r = HATS;
        else if (ataques > 2.0 && desvio > 8.0 && fc > 1500.0) r = DRUMS;
        else if (ataques > 4.0 || notas > 2.5) r = SYNTHS;                     // acid (semicolcheias) ou melodia
        else r = AMBIENCE;
        roleSom = r;
        return r;
    }

    // FOCUS: quanto esta faixa cede, por banda, para as faixas em FOCUS (e KICK/BASS embaixo, com KEEP ON TOP).
    // Várias FOCUS ao mesmo tempo: as outras cedem para todas; entre duas FOCUS que batem na mesma banda, as duas
    // cedem um pouco, por igual (a conta é simétrica: o que uma corta, a outra corta)
    void cedeEspaco (const Ajustes& a, int role)
    {
        auto& R = Registry::get();
        auto& S = slot();
        float corteB[NB];
        int mascara = 0;   // bandas de EQ em que a FOCUS pediu corte
        for (int b = 0; b < NB; ++b) corteB[b] = 0.0f;
        constexpr int MAXF = 32;
        int focos[MAXF];
        int nFocos = 0;

        const bool euFoco = a.focus;
        const bool topo = ! euFoco && travado.travado && R.topo.load() != 0 && role != KICK && role != BASS;   // só depois do LOCK
        // dois grupos, cada um com a própria referência: as faixas em FOCUS (espectro todo) e KICK/BASS (só embaixo)
        double F[2][NB];
        for (int g = 0; g < 2; ++g) for (int b = 0; b < NB; ++b) F[g][b] = 0.0;
        bool algum[2] { false, false };
        for (int i = 0; i < NS; ++i)
        {
            if (i == vaga) continue;
            auto& o = R.slot[i];
            if (o.usado.load() == 0) continue;
            const bool fo = o.focus.load() != 0;
            const int ro = o.role.load();
            const bool baixo = topo && (ro == KICK || ro == BASS);
            if (! fo && ! baixo) continue;
            const int g = fo ? 0 : 1;
            float soma = 0.0f;
            for (int b = 0; b < NB; ++b)
            {
                if (g == 1 && b > kTopo) break;
                const float v = o.vivo[b].load();
                F[g][b] += v;
                soma += v;
            }
            if (soma > 1e-10f) algum[g] = true;
            if (fo && soma > 1e-10f && nFocos < MAXF) focos[nFocos++] = i;
        }

        const float amt = clampf (R.amount.load(), 0.0f, 1.0f), mc = clampf (R.maxCut.load(), 0.0f, 18.0f);
        float meuPk = -200.0f;
        for (int b = 0; b < NB; ++b) meuPk = std::max (meuPk, dbDe (envVivo[b]));
        for (int g = 0; g < 2; ++g)
        {
            if (! algum[g] || mc <= 0.0f) { refPk[g] = -200.0f; continue; }
            float pk = -200.0f;
            for (int b = 0; b < NB; ++b) pk = std::max (pk, dbDe (F[g][b]));
            // referência: o pico do grupo, segurado (cai 6 dB/s). Quando a FOCUS cala, as bandas dela caem abaixo da
            // referência e o corte some; quando volta, a referência sobe junto na hora
            refPk[g] = std::max (pk, refPk[g] - 6.0f * (float) HOP / (float) fs);
            pk = refPk[g];
            if (pk < -70.0f) continue;
            for (int b = 0; b < NB; ++b)
            {
                const float lf = dbDe (F[g][b]), ly = dbDe (envVivo[b]);
                if (lf < -90.0f || ly < -80.0f) continue;
                const float imp = clampf ((lf - (pk - 30.0f)) / 18.0f, 0.0f, 1.0f);   // a banda importa para quem tem FOCUS?
                float c;
                if (euFoco)
                {
                    // FOCUS x FOCUS: só onde as duas importam e estão perto em nível; até 3 dB (ou metade do MAX CUT)
                    const float impEu = clampf ((ly - (meuPk - 30.0f)) / 18.0f, 0.0f, 1.0f);
                    const float perto = 1.0f - std::abs (ly - lf) / 12.0f;
                    if (perto <= 0.0f || imp <= 0.0f || impEu <= 0.0f) continue;
                    c = -std::min (std::min (3.0f, 0.5f * mc), amt * imp * impEu * perto * 4.0f);
                }
                else
                {
                    const float excesso = ly - lf + 3.0f;                                // mascara (até 3 dB abaixo ainda conta)
                    if (excesso <= 0.0f || imp <= 0.0f) continue;
                    c = -std::min (mc, amt * imp * excesso);
                }
                corteB[b] = std::min (corteB[b], c);
                if (g == 0 && c < -0.5f) mascara |= 1 << std::min (NE - 1, b / 3);
            }
        }
        // corte por banda de EQ: metade do pior + metade da média das 3 bandas
        for (int k = 0; k < NE; ++k)
        {
            float pior = 0.0f, med = 0.0f;
            for (int j = 0; j < 3; ++j) { pior = std::min (pior, corteB[3 * k + j]); med += corteB[3 * k + j] / 3.0f; }
            alvoCorte[k] = 0.5f * pior + 0.5f * med;
        }
        // para QUAIS FOCUS cede: as que têm, numa banda cortada, até 10 dB do total das FOCUS ali (a mais pesada primeiro)
        int lista[Slot::NCEDE];
        float pesoL[Slot::NCEDE];
        int nl = 0;
        if (mascara != 0)
            for (int j = 0; j < nFocos; ++j)
            {
                auto& o = R.slot[focos[j]];
                float peso = 0.0f;
                for (int b = 0; b < NB; ++b)
                {
                    if (corteB[b] > -0.5f) continue;
                    const float v = o.vivo[b].load();
                    if (dbDe (v) > dbDe (F[0][b]) - 10.0f) peso += v;
                }
                if (peso <= 0.0f) continue;
                int pos;
                if (nl < Slot::NCEDE) pos = nl++;
                else if (peso > pesoL[Slot::NCEDE - 1]) pos = Slot::NCEDE - 1;
                else continue;
                lista[pos] = focos[j]; pesoL[pos] = peso;
                while (pos > 0 && pesoL[pos] > pesoL[pos - 1])
                {
                    std::swap (lista[pos], lista[pos - 1]); std::swap (pesoL[pos], pesoL[pos - 1]); --pos;
                }
            }
        // "cede para a FOCUS" só conta o corte pedido pelas faixas em FOCUS (o do KEEP KICK / BASS ON TOP é à parte)
        S.mascaraFoco = mascara;
        for (int k = 0; k < Slot::NCEDE; ++k) S.cede[k] = k < nl ? lista[k] : -1;
        S.nCede = nl;
        S.cedePara = nl > 0 ? lista[0] : -1;
    }

    // LOW-END GUARD: shelf de graves (120 Hz) dinâmico. Só depois do LOCK, só em quem não é KICK nem BASS, e só
    // enquanto o grave do KICK / BASS toca; o tanto depende de quanto grave esta faixa tem perto do deles.
    // Em dúvida (ROLE adivinhado pelo som e a faixa tem muito grave), não mexe.
    void guardaGrave (int role, bool peloSom)
    {
        auto& R = Registry::get();
        alvoShelf = 0.0f;
        const bool duvida = peloSom && nQuadros > 0 && somaBaixo / nQuadros > 0.35;
        if (! travado.travado || R.guard.load() == 0 || role == KICK || role == BASS || duvida) { refKB = -200.0f; return; }
        double kb = 0.0;
        for (int i = 0; i < NS; ++i)
        {
            if (i == vaga) continue;
            auto& o = R.slot[i];
            if (o.usado.load() == 0) continue;
            const int ro = o.role.load();
            if (ro != KICK && ro != BASS) continue;
            for (int b = 0; b <= kGuard; ++b) kb += o.vivo[b].load();
        }
        const float lkb = dbDe (kb);
        refKB = std::max (lkb, refKB - 6.0f * (float) HOP / (float) fs);
        if (refKB < -70.0f || lkb < -90.0f) return;
        double eu = 0.0;
        for (int b = 0; b <= kGuard; ++b) eu += envVivo[b];
        const float leu = dbDe (eu);
        if (leu < -90.0f) return;
        const float ativo = clampf ((lkb - (refKB - 18.0f)) / 12.0f, 0.0f, 1.0f);   // o KICK / BASS está tocando?
        const float imp = clampf ((leu - (lkb - 42.0f)) / 18.0f, 0.0f, 1.0f);       // esta faixa tem grave que atrapalha?
        alvoShelf = -kShelfMax * clampf (R.amount.load(), 0.0f, 1.0f) * ativo * imp;
    }

    // LOCK: nível do ROLE para o alvo do STYLE; TONE MATCH pela forma do espectro
    void trava()
    {
        auto& R = Registry::get();
        auto& S = slot();
        travado = Travado {};
        travado.travado = true;
        if (nApr <= 0) return;   // não tocou no LEARN: fica como está (o GUARD vale)

        const int style = R.style.load();
        double potRole[NR] {};
        bool tem[NR] {};
        for (int i = 0; i < NS; ++i)
        {
            auto& o = R.slot[i];
            if (o.usado.load() == 0 || o.aprQuadros.load() <= 0) continue;
            const int r = o.role.load();
            if (r < 0 || r >= NR) continue;
            potRole[r] += potDe (o.aprNivelDb.load());
            tem[r] = true;
        }
        if (vaga < 0) { const int r = S.role.load(); potRole[r] += potDe (S.aprNivelDb.load()); tem[r] = true; }

        // âncora: o KICK; sem KICK, a média que menos mexe em todos
        double ancora;
        if (tem[KICK]) ancora = dbDe (potRole[KICK]) - alvoNivel (style, KICK);
        else
        {
            double s = 0; int n = 0;
            for (int r = 0; r < NR; ++r) if (tem[r]) { s += dbDe (potRole[r]) - alvoNivel (style, r); ++n; }
            ancora = n > 0 ? s / n : 0.0;
        }
        const int meu = S.role.load();
        const float atual = dbDe (potRole[meu]);
        const float alvo = (float) ancora + alvoNivel (style, meu);

        // TONE MATCH: forma (dB relativo ao total) contra a forma do ROLE; metade da diferença, ±4 / +3 dB
        double oit[NE] {};
        double tot = 0, totT = 0;
        for (int b = 0; b < NB; ++b) { oit[std::min (NE - 1, b / 3)] += somaApr[b] / nApr; }
        for (int k = 0; k < NE; ++k) { tot += oit[k]; totT += potDe (alvoTom (meu, k)); }
        float bruto[NE] {};
        for (int k = 0; k < NE; ++k)
        {
            const float m = dbDe (oit[k]) - dbDe (tot), t = alvoTom (meu, k) - dbDe (totT);
            if (m < -30.0f || t < -30.0f) continue;      // banda sem conteúdo: não mexe (não sobe ruído)
            bruto[k] = clampf (0.5f * (t - m), -4.0f, 3.0f);
        }
        for (int k = 0; k < NE; ++k)
        {
            const float a = k > 0 ? bruto[k - 1] : bruto[k], c = k < NE - 1 ? bruto[k + 1] : bruto[k];
            travado.tomBruto[k] = 0.25f * a + 0.5f * bruto[k] + 0.25f * c;
        }
        // o tom mexe no nível: desconta (K-ponderado), para o LEVEL continuar no alvo. Resposta de verdade
        // das 9 bandas somadas (as vizinhas se sobrepõem) no centro de cada banda de análise
        double antes = 0, depois = 0;
        for (int b = 0; b < NB; ++b)
        {
            const double p = somaApr[b] / nApr * kPeso[b];
            double db = 0;
            for (int k = 0; k < NE; ++k)
                if (travado.tomBruto[k] != 0.0f) db += respostaDb (sino (fs, freqEq (k), qOitava, travado.tomBruto[k]), fs, freqBanda (b));
            antes += p;
            depois += p * potDe (db);
        }
        const float dTom = dbDe (depois) - dbDe (antes);
        travado.ganhoBruto = clampf (alvo - atual - dTom, -12.0f, 6.0f);
        travado.nivelAntes = atual;
        travado.alvoNivel = alvo;
    }

    void calculaEq (bool forca)
    {
        for (int k = 0; k < NE; ++k)
        {
            const float g = tomSuave[k] + corteSuave[k];
            if (! forca && std::abs (g - gEq[k]) < 0.01f && ! (g == 0.0f && ativa[k])) continue;
            const bool era = ativa[k];
            gEq[k] = g;
            ativa[k] = g != 0.0f;
            if (ativa[k])
            {
                eq[k].k = sino (fs, freqEq (k), qOitava, g);
                if (! era) eq[k].zera();
            }
        }
        if (forca || std::abs (shelfSuave - shelfEq) >= 0.02f || (shelfSuave == 0.0f && shelfAtivo))
        {
            const bool era = shelfAtivo;
            shelfEq = shelfSuave;
            shelfAtivo = shelfSuave != 0.0f;
            if (shelfAtivo)
            {
                sh.k = shelfGrave (fs, fGuard, shelfSuave);
                if (! era) sh.zera();
            }
        }
    }

    int vaga = -1;
    std::unique_ptr<Slot> vagaLocal;     // Registry cheio: funciona sozinha
    double fs = 48000.0;
    FftReal fft;
    std::vector<float> janela, anel, quadro, pot;
    std::vector<Peso> pesos[NB];
    float kPeso[NB] {};
    float normPot = 1.0f;
    int posAnel = 0, cheio = 0;

    float telaP[NB] {}, envVivo[NB] {};
    float nivelLento = 0.0f, nivelAnt = -200.0f;
    double somaApr[NB] {};
    double somaNivel = 0.0;
    int nApr = 0, nPerc = 0, nQuadros = 0, nNotas = 0, nPlano = 0;
    double somaPlano = 0, somaNiv = 0, somaNiv2 = 0, somaBaixo = 0;
    float bandaAnt[NB] {};
    mutable int roleSom = -1;
    bool aprendendo = false;
    int roleParam = -1;
    std::atomic<int> nomeRole { -1 };

    int genLearnVisto = 0, genLockVisto = 0, genResetVisto = 0;

    float refPk[2] { -200.0f, -200.0f };
    float alvoCorte[NE] {}, corteSuave[NE] {}, alvoTomK[NE] {}, tomSuave[NE] {}, gEq[NE] {};
    bool ativa[NE] {};
    Biquad eq[NE], sh;
    // LOW-END GUARD
    float refKB = -200.0f, alvoShelf = 0.0f, shelfSuave = 0.0f, shelfEq = 0.0f;
    bool shelfAtivo = false;
    int roleUltimo = -1, genRolesVisto = 0;
    double ganhoLin = 1.0;
};
} // namespace maestro
