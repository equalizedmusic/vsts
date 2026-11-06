#pragma once

// ONKEY — afinação automática de voz (sem JUCE: o mesmo arquivo roda no plugin e no teste de bancada tools/teste_dsp.cpp).
//
// Caminho do som (latência fixa D ≈ 44 ms, o DAW compensa):
//   1. DETECÇÃO: a voz (soma mono) passa por um passa-baixas e é decimada para ~11-12 kHz; a cada 5 ms um quadro YIN
//      (diferença normalizada cumulativa) acha o período, que é refinado na taxa cheia. Sem periodicidade ou abaixo do
//      gate = sem voz (consoante, respiração, silêncio).
//   2. NOTA: o centro da nota é a média dos últimos ~180 ms DA MESMA NOTA (o vibrato se cancela na média); salto de
//      mais de 0,8 semitom que dura 20 ms abre nota nova. Deslizando (o centro anda mais de 8 semitons/s) não corrige:
//      slide e portamento passam como cantados.
//   3. TOM: histograma das classes de altura (memória ~40 s de voz) contra os perfis de Krumhansl-Kessler (24 tons).
//      Antes de travar o tom, corrige no cromático (só o que está entre duas notas).
//   4. CORREÇÃO: alvo = nota da escala mais perto do centro (com histerese). HUMANIZE é uma zona morta suave; RETUNE
//      SPEED é a constante de tempo com que a correção chega; VIBRATO 100% mantém o desvio em volta do centro.
//   5. SÍNTESE: TD-PSOLA. Grãos Hann de 2 períodos, um por período de saída, a fonte andando um período por grão (pula ou
//      repete um período quando a diferença passa de meio período). O envelope espectral (formantes) fica onde estava: a voz não fica "esquilo" nem "monstro".
//      FORMANT desloca os formantes de propósito (o grão é lido mais rápido/devagar). Sem voz, o grão sai do mesmo
//      lugar da entrada: consoantes e respiração passam idênticas. A saída é a média ponderada pelos pesos das janelas,
//      então o nível não muda com a quantidade de grãos sobrepostos.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace onkey
{
constexpr double kPi = 3.14159265358979323846;

inline const char* nomePc (int pc)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((pc % 12) + 12) % 12];
}

inline const char* nomeNota (int midi, char* buf)   // "A4"
{
    std::snprintf (buf, 8, "%s%d", nomePc (midi), midi / 12 - 1);
    return buf;
}

enum { escMaior, escMenor, escMenorHarm, escPentaMaior, escPentaMenor, escCromatica, numEscalas };

inline const char* nomeEscala (int e)
{
    static const char* n[numEscalas] = { "MAJOR", "MINOR", "HARM. MINOR", "MAJ. PENTA", "MIN. PENTA", "CHROMATIC" };
    return n[std::clamp (e, 0, numEscalas - 1)];
}

// bits das notas da escala, relativos à tônica (bit 0 = tônica)
inline unsigned mascara (int e)
{
    static const unsigned m[numEscalas] = { 0xAB5, 0x5AD, 0x9AD, 0x295, 0x4A9, 0xFFF };
    return m[std::clamp (e, 0, numEscalas - 1)];
}

// bits das classes de altura absolutas (bit 0 = C)
inline unsigned mascaraTom (int tonica, int e)
{
    const unsigned m = mascara (e);
    unsigned r = 0;
    for (int i = 0; i < 12; ++i)
        if ((m >> i) & 1u) r |= 1u << ((i + tonica) % 12);
    return r;
}

struct Ajustes
{
    bool ligado = true;
    float amount = 1.0f;       // 0..1
    float speed = 0.5f;        // 0 (natural, ~250 ms) .. 1 (instantâneo, 4 ms)
    float humanize = 0.2f;     // 0..1 → zona morta até 30 cents
    float vibrato = 1.0f;      // 1 = mantém o vibrato cantado, 0 = achata
    float formant = 0.0f;      // semitons, −3..+3
    float saidaDb = 0.0f;
    int tom = -1;              // −1 = AUTO, 0..11 (C..B)
    int escala = -1;           // −1 = AUTO, senão esc*
};

// o que a tela lê (escrito no áudio a cada quadro de 5 ms)
struct Leitura
{
    std::atomic<int> estado { 0 };          // 0 = esperando voz, 1 = ouvindo (tom ainda não travado), 2 = tom travado
    std::atomic<int> tomDet { -1 };         // tom detectado (0..11) e modo (0 maior, 1 menor)
    std::atomic<int> modoDet { 0 };
    std::atomic<float> confianca { 0.0f };  // correlação do tom detectado (0..1)
    std::atomic<float> ouvido { 0.0f };     // segundos de voz ouvidos (para o "ouvindo")
    std::atomic<int> tomUsado { -1 };       // o que está valendo agora (AUTO ou manual)
    std::atomic<int> escalaUsada { escCromatica };
    std::atomic<unsigned> mascaraUsada { 0xFFF };
    std::atomic<int> nota { -1 };           // nota alvo (MIDI) ou −1
    std::atomic<float> desvio { 0.0f };     // cents: centro da nota cantada − alvo
    std::atomic<float> correcao { 0.0f };   // cents aplicados agora
    std::atomic<bool> voz { false };
    std::atomic<float> hz { 0.0f };
};

// histórico para o gráfico: um ponto por quadro (5 ms); 0 = sem voz
struct Historico
{
    static constexpr int size = 2048;       // ≈ 10 s
    float entrada[size] {}, saida[size] {};
    std::int16_t alvo[size] {};
    std::atomic<int> pos { 0 };             // quadros escritos (o último é pos − 1)
};

class Engine
{
public:
    Leitura leitura;
    Historico hist;

    int latencia() const { return D; }
    double taxa() const { return fs; }
    double segPorQuadro() const { return hop / fs; }

    void prepare (double sampleRate)
    {
        fs = sampleRate > 8000.0 ? sampleRate : 48000.0;
        dec = std::max (1, (int) std::lround (fs / 11025.0));
        fsd = fs / dec;
        hopD = std::max (1, (int) std::lround (0.005 * fsd));
        hop = hopD * dec;
        Wd = (int) std::lround (0.022 * fsd);
        tauMinD = std::max (2, (int) std::floor (fsd / 1100.0));
        tauMaxD = (int) std::ceil (fsd / 72.0);
        Wf = (int) std::lround (0.012 * fs);
        Pmax = fs / 72.0;
        Pu = 0.006 * fs;
        Lcria = 1.6 * Pmax;
        lag = (double) dec * (Wd + tauMaxD) * 0.5;
        D = (int) std::ceil (lag + Lcria + hop) + 8;

        // passa-baixas da decimação: Butterworth de 4ª ordem (2 biquads) em 0,38 × a taxa decimada
        const double fc = std::min (0.38 * fsd, 0.45 * fs);
        const double qs[2] = { 0.5411961, 1.3065630 };
        for (int s = 0; s < 2; ++s) lp[s].lowpass (fc, qs[s], fs);

        dif.assign ((size_t) tauMaxD + 2, 0.0);
        cmn.assign ((size_t) tauMaxD + 2, 0.0);
        jan.assign ((size_t) (Wd + tauMaxD + 2), 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& c : xr) std::fill (std::begin (c), std::end (c), 0.0f);
        std::fill (std::begin (xm), std::end (xm), 0.0f);
        std::fill (std::begin (xd), std::end (xd), 0.0f);
        for (auto& f : lp) f.z1 = f.z2 = 0.0;
        n = 0; nd = 0; contDec = 0;
        nq = 0; c0 = 0.0;
        desl = 0.0;
        nGraos = 0; sProx = 0.0; hAnt = Pu;
        mix = 1.0f; ganho = 1.0f; gama = 1.0;
        vozAnt = false; nivelMax = 0.0;
        nm3 = 0;
        novaNota();
        cApl = 0.0;
        std::fill (std::begin (histPc), std::end (histPc), 0.0);
        quadrosVoz = 0; tomTravado = false; tomAtual = -1; candidato = -1; contaCand = 0; quadrosDesdeAval = 0;
        semVoz = 1000;
        leitura.estado = 0; leitura.tomDet = -1; leitura.modoDet = 0; leitura.confianca = 0.0f; leitura.ouvido = 0.0f;
        leitura.nota = -1; leitura.voz = false; leitura.correcao = 0.0f; leitura.desvio = 0.0f; leitura.hz = 0.0f;
        hist.pos = 0;
    }

    // processa no lugar; nch = 1 ou 2
    void process (float* const* ch, int nch, int ns, const Ajustes& a)
    {
        nch = std::clamp (nch, 1, 2);
        aj = a;
        const float alvoMix = a.ligado ? 1.0f : 0.0f;
        const float passoMix = (float) (1.0 / (0.02 * fs));
        const float alvoGanho = (float) std::pow (10.0, a.saidaDb / 20.0);
        const float kG = (float) (1.0 - std::exp (-1.0 / (0.02 * fs)));

        for (int i = 0; i < ns; ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < nch; ++c) { xr[c][n & M] = ch[c][i]; mono += ch[c][i]; }
            mono /= (float) nch;
            xm[n & M] = mono;

            double y = mono;
            for (auto& f : lp) y = f.tick (y);
            if (++contDec >= dec)
            {
                contDec = 0;
                xd[nd & MD] = (float) y;
                ++nd;
                if (nd % hopD == 0 && nd >= (std::int64_t) (Wd + tauMaxD + 2)) quadro();
            }

            // amostra de saída: a entrada de D amostras atrás
            const std::int64_t t = n - D;
            float wet[2] = { 0.0f, 0.0f };
            if (t >= 0)
            {
                while (sProx <= (double) t + Lcria) criaGrao();
                double acc[2] = { 0.0, 0.0 }, ws = 0.0;
                int k = 0;
                for (int g = 0; g < nGraos; ++g)
                {
                    const Grao& gr = graos[g];
                    const double off = (double) t - gr.s;
                    if (off >= gr.L) continue;              // terminou: sai da lista
                    graos[k++] = gr;
                    if (off <= -gr.L) continue;             // ainda não começou
                    const double w = 0.5 + 0.5 * std::cos (kPi * off / gr.L);
                    const double pos = gr.a + off * gr.gama;
                    for (int c = 0; c < nch; ++c) acc[c] += w * ler (c, pos);
                    ws += w;
                }
                nGraos = k;
                for (int c = 0; c < nch; ++c)
                    wet[c] = ws > 1.0e-6 ? (float) (acc[c] / ws) : xr[c][t & M];
            }

            mix += std::clamp (alvoMix - mix, -passoMix, passoMix);
            ganho += (alvoGanho - ganho) * kG;
            const float g = 1.0f + (ganho - 1.0f) * mix;
            for (int c = 0; c < nch; ++c)
            {
                const float seco = t >= 0 ? xr[c][t & M] : 0.0f;
                ch[c][i] = (seco + (wet[c] - seco) * mix) * g;
            }
            ++n;
        }
    }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass (double f, double q, double sr)
        {
            const double w = 2 * kPi * f / sr, cw = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            b0 = (1 - cw) / 2 / a0; b1 = (1 - cw) / a0; b2 = b0; a1 = -2 * cw / a0; a2 = (1 - al) / a0;
        }
        double tick (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct Quadro { bool voz = false; float P = 0; float r = 1; };
    struct Grao { double s, a, L, gama; };

    static constexpr int N = 1 << 16, M = N - 1;          // anéis na taxa cheia (> 0,3 s a 192 kHz)
    static constexpr int ND = 1 << 13, MD = ND - 1;       // anel decimado
    static constexpr int NQ = 2048;                        // quadros guardados
    static constexpr int maxGraos = 24;

    float ler (int c, double pos) const   // Hermite cúbica
    {
        const double f = std::floor (pos);
        const auto i = (std::int64_t) f;
        const float t = (float) (pos - f);
        const float* x = xr[c];
        const float y0 = x[(i - 1) & M], y1 = x[i & M], y2 = x[(i + 1) & M], y3 = x[(i + 2) & M];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    Quadro consulta (double tempo) const
    {
        if (nq == 0) return {};
        double f = (tempo - c0) / hop;
        const double ini = (double) std::max<std::int64_t> (0, nq - (NQ - 4));
        f = std::clamp (f, ini, (double) (nq - 1));
        const auto i0 = (std::int64_t) std::floor (f);
        const auto i1 = std::min<std::int64_t> (i0 + 1, nq - 1);
        const double fr = f - (double) i0;
        const Quadro& q0 = quadros[i0 % NQ];
        const Quadro& q1 = quadros[i1 % NQ];
        if (q0.voz && q1.voz)
            return { true, (float) (q0.P + (q1.P - q0.P) * fr), (float) (q0.r + (q1.r - q0.r) * fr) };
        if (q0.voz && fr < 0.5) return q0;
        if (q1.voz && fr >= 0.5) return q1;
        return {};
    }

    // Um grão por período de SAÍDA (h = P / r). A fonte anda um período inteiro por grão; a diferença acumula em
    // 'desl' (fonte − saída). Passou de meio período: pula (subindo) ou repete (descendo) um período. Sem correção
    // (r = 1) 'desl' não muda e a saída é a própria entrada (Hann com salto P soma 1).
    void criaGrao()
    {
        const double s = sProx;
        const Quadro q = consulta (s);
        Grao g;
        g.s = s;
        double P, r;
        if (q.voz)
        {
            P = std::clamp ((double) q.P, 8.0, Pmax * 1.2);
            r = std::clamp ((double) q.r, 0.5, 2.0);
            if (desl > 0.5 * P) desl -= P;
            else if (desl < -0.5 * P) desl += P;
            g.a = s + desl;
            g.gama = gama;
            desl += P - P / r;
        }
        else
        {
            P = Pu; r = 1.0;
            desl = 0.0;
            g.a = s; g.gama = 1.0;
        }
        const double h = std::clamp (P / r, 8.0, Lcria);
        g.L = std::min (Lcria, std::max ({ P / g.gama, h, hAnt }));
        hAnt = h;
        sProx = s + h;
        if (nGraos < maxGraos) graos[nGraos++] = g;
    }

    // ---- quadro de análise (a cada 5 ms) ------------------------------------------------------------------------
    void quadro()
    {
        const int len = Wd + tauMaxD;
        double e = 0.0;
        for (int j = 0; j < len; ++j)
        {
            const float v = xd[(nd - len + j) & MD];
            jan[(size_t) j] = v;
            e += (double) v * v;
        }
        const double rms = std::sqrt (e / len);
        nivelMax = std::max (rms, nivelMax * 0.9995);

        // YIN
        cmn[0] = 1.0;
        double soma = 0.0;
        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            double d = 0.0;
            const float* a = jan.data();
            const float* b = jan.data() + tau;
            for (int j = 0; j < Wd; ++j) { const double x = (double) a[j] - b[j]; d += x * x; }
            dif[(size_t) tau] = d;
            soma += d;
            cmn[(size_t) tau] = soma > 1.0e-20 ? d * tau / soma : 1.0;
        }
        int melhor = -1;
        for (int tau = tauMinD; tau < tauMaxD; ++tau)
            if (cmn[(size_t) tau] < 0.15)
            {
                while (tau + 1 < tauMaxD && cmn[(size_t) tau + 1] < cmn[(size_t) tau]) ++tau;
                melhor = tau;
                break;
            }
        if (melhor < 0)
        {
            melhor = tauMinD;
            for (int tau = tauMinD; tau < tauMaxD; ++tau)
                if (cmn[(size_t) tau] < cmn[(size_t) melhor]) melhor = tau;
        }
        const double aper = cmn[(size_t) melhor];
        double tauD = melhor;
        if (melhor > 1 && melhor < tauMaxD)
        {
            const double y0 = cmn[(size_t) melhor - 1], y1 = cmn[(size_t) melhor], y2 = cmn[(size_t) melhor + 1];
            const double den = y0 - 2 * y1 + y2;
            if (den > 1.0e-12) tauD += std::clamp (0.5 * (y0 - y2) / den, -0.5, 0.5);
        }

        const bool temNivel = rms > 0.0035 && rms > nivelMax * 0.012;
        const bool voz = temNivel && aper < (vozAnt ? 0.35 : 0.25);

        double tau = tauD * dec;
        if (voz && dec > 1)   // refina na taxa cheia
        {
            const int tc = (int) std::lround (tau), lo = std::max (2, tc - dec - 1), hi = tc + dec + 1;
            const std::int64_t n0 = n + 1 - (Wf + hi);
            double dv[64]; int nv = 0, im = 0;
            for (int tt = lo; tt <= hi && nv < 64; ++tt, ++nv)
            {
                double d = 0.0;
                for (int j = 0; j < Wf; ++j)
                {
                    const double x = (double) xm[(n0 + j) & M] - xm[(n0 + j + tt) & M];
                    d += x * x;
                }
                dv[nv] = d;
                if (d < dv[im]) im = nv;
            }
            if (im > 0 && im < nv - 1)
            {
                const double den = dv[im - 1] - 2 * dv[im] + dv[im + 1];
                tau = lo + im + (den > 1.0e-20 ? std::clamp (0.5 * (dv[im - 1] - dv[im + 1]) / den, -0.5, 0.5) : 0.0);
            }
        }

        Quadro q;
        double mIn = 0.0, mOut = 0.0;
        int alvo = -1;
        if (voz)
        {
            const double f0 = fs / tau;
            double m = 69.0 + 12.0 * std::log2 (f0 / 440.0);
            // mediana de 3 contra erro de oitava de um quadro só
            m3[nm3 % 3] = m; ++nm3;
            if (nm3 >= 3 && vozAnt)
            {
                const double a = m3[0], b = m3[1], c = m3[2];
                m = std::max (std::min (a, b), std::min (std::max (a, b), c));
            }
            const double shift = corrige (m, alvo);
            mIn = m; mOut = m + shift;
            q.voz = true;
            q.P = (float) (fs / (440.0 * std::pow (2.0, (m - 69.0) / 12.0)));
            q.r = (float) std::pow (2.0, shift / 12.0);
            leitura.hz = (float) (440.0 * std::pow (2.0, (m - 69.0) / 12.0));
            semVoz = 0;
        }
        else
        {
            if (vozAnt) novaNota();
            nm3 = 0;
            ++semVoz;
            leitura.nota = -1;
        }
        vozAnt = voz;
        gama = std::pow (2.0, std::clamp ((double) aj.formant, -3.0, 3.0) / 12.0);

        if (nq == 0) c0 = (double) n - lag;
        quadros[nq % NQ] = q;
        ++nq;

        const int hp = hist.pos.load (std::memory_order_relaxed);
        hist.entrada[hp % Historico::size] = (float) mIn;
        hist.saida[hp % Historico::size] = (float) mOut;
        hist.alvo[hp % Historico::size] = (std::int16_t) alvo;
        hist.pos.store (hp + 1, std::memory_order_release);

        leitura.voz = voz;
        leitura.estado = tomTravado ? 2 : (quadrosVoz > 0 ? 1 : 0);
        leitura.ouvido = (float) (quadrosVoz * hop / fs);
    }

    void novaNota()
    {
        notaN = 0; contaSalto = 0; alvoAnt = -1; cApl = 0.0;
    }

    // devolve o deslocamento em semitons para o quadro (m = nota cantada, em MIDI fracionário)
    double corrige (double m, int& alvo)
    {
        const double seg = hop / fs;
        if (! vozAnt) novaNota();
        notaBuf[notaN % 64] = m; ++notaN;
        auto media = [&] (int k) { k = (int) std::min<std::int64_t> (k, notaN); double s = 0; for (int i = 0; i < k; ++i) s += notaBuf[(notaN - 1 - i) % 64]; return s / k; };
        const int nLongo = std::max (4, (int) std::lround (0.18 / seg));
        const double curto = media (6);
        double cen = media (nLongo);
        if (notaN > 8 && std::abs (curto - cen) > 0.8)
        {
            if (++contaSalto >= 4)
            {
                // nota nova: recomeça a média com os últimos quadros (os do salto)
                double ult[4];
                for (int i = 0; i < 4; ++i) ult[i] = notaBuf[(notaN - 4 + i) % 64];
                const double cAnt = cApl;
                novaNota();
                cApl = cAnt * 0.5;   // não zera de uma vez: a correção da nota anterior sai junto com a nova
                for (double v : ult) notaBuf[notaN++ % 64] = v;
                cen = media (4);
            }
        }
        else contaSalto = 0;

        // deslizando? o centro anda mais de 8 semitons/s
        cenHist[ncen % 8] = cen; ++ncen;
        const bool desliza = notaN > 6 && ncen > 6 && std::abs (cen - cenHist[(ncen - 7) % 8]) / (6 * seg) > 8.0;

        // escala que vale agora
        const int tomU = aj.tom >= 0 ? aj.tom : (tomTravado ? tomAtual % 12 : -1);
        int escU = aj.escala >= 0 ? aj.escala : (tomTravado ? (tomAtual >= 12 ? escMenor : escMaior) : escCromatica);
        if (tomU < 0) escU = escCromatica;
        const unsigned mask = mascaraTom (std::max (0, tomU), escU);
        leitura.tomUsado = tomU; leitura.escalaUsada = escU; leitura.mascaraUsada = mask;

        // alvo: nota da escala mais perto do centro, com histerese
        int melhor = -1; double dm = 1e9;
        const int base = (int) std::floor (cen);
        for (int k = base - 7; k <= base + 8; ++k)
            if ((mask >> (((k % 12) + 12) % 12)) & 1u)
                if (std::abs (cen - k) < dm) { dm = std::abs (cen - k); melhor = k; }
        if (alvoAnt >= 0 && ((mask >> (alvoAnt % 12)) & 1u) && std::abs (cen - alvoAnt) < dm + 0.15) melhor = alvoAnt;
        alvoAnt = melhor;
        alvo = melhor;

        double c = melhor - cen;
        const double tol = std::clamp ((double) aj.humanize, 0.0, 1.0) * 0.30;
        if (tol > 1.0e-4) c -= tol * std::tanh (c / tol);
        const double alvoC = desliza ? 0.0 : std::clamp ((double) aj.amount, 0.0, 1.0) * c;
        const double sp = std::clamp ((double) aj.speed, 0.0, 1.0);
        const double tauS = 0.004 + 0.25 * (1.0 - sp) * (1.0 - sp);
        cApl += (alvoC - cApl) * (1.0 - std::exp (-seg / tauS));
        const double dev = m - cen;
        const double shift = std::clamp (cApl - (1.0 - std::clamp ((double) aj.vibrato, 0.0, 1.0)) * std::clamp ((double) aj.amount, 0.0, 1.0) * dev, -12.0, 12.0);

        leitura.nota = melhor;
        leitura.desvio = (float) ((cen - melhor) * 100.0);
        leitura.correcao = (float) (shift * 100.0);

        if (! desliza) aprendeTom (m);
        return shift;
    }

    // ---- tom (Krumhansl-Kessler) ---------------------------------------------------------------------------------
    void aprendeTom (double m)
    {
        const double seg = hop / fs;
        const double k = std::exp (-seg / 40.0);
        for (auto& h : histPc) h *= k;
        histPc[(((int) std::lround (m)) % 12 + 12) % 12] += 1.0;
        ++quadrosVoz;
        if (++quadrosDesdeAval < (int) std::lround (0.25 / seg)) return;
        quadrosDesdeAval = 0;
        if (quadrosVoz * seg < 3.0) return;

        static const double maior[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double menor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        double corr[24];
        double mh = 0; for (double h : histPc) mh += h; mh /= 12;
        for (int md = 0; md < 2; ++md)
        {
            const double* p = md ? menor : maior;
            double mp = 0; for (int i = 0; i < 12; ++i) mp += p[i]; mp /= 12;
            for (int r = 0; r < 12; ++r)
            {
                double sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < 12; ++i)
                {
                    const double x = histPc[(i + r) % 12] - mh, y = p[i] - mp;
                    sxy += x * y; sxx += x * x; syy += y * y;
                }
                corr[md * 12 + r] = (sxx > 1e-12 && syy > 1e-12) ? sxy / std::sqrt (sxx * syy) : 0.0;
            }
        }
        int b = 0;
        for (int i = 1; i < 24; ++i) if (corr[i] > corr[b]) b = i;
        if (! tomTravado)
        {
            if (corr[b] > 0.45)
            {
                if (b == candidato) ++contaCand; else { candidato = b; contaCand = 1; }
                if (contaCand >= 3) { tomTravado = true; tomAtual = b; contaCand = 0; }
            }
        }
        else if (b != tomAtual)
        {
            if (b == candidato) ++contaCand; else { candidato = b; contaCand = 1; }
            if (contaCand >= 8 && corr[b] > corr[tomAtual] + 0.04) { tomAtual = b; contaCand = 0; }   // ~2 s firme
        }
        else contaCand = 0;
        if (tomTravado)
        {
            leitura.tomDet = tomAtual % 12;
            leitura.modoDet = tomAtual >= 12 ? 1 : 0;
            leitura.confianca = (float) std::max (0.0, corr[tomAtual]);
        }
    }

    // ---- estado ---------------------------------------------------------------------------------------------------
    double fs = 48000.0, fsd = 12000.0, lag = 0.0, Pmax = 600.0, Pu = 288.0, Lcria = 960.0;
    int dec = 4, hop = 240, hopD = 60, Wd = 264, tauMinD = 10, tauMaxD = 167, Wf = 576, D = 2000;
    Ajustes aj;

    float xr[2][N] {};
    float xm[N] {};
    float xd[ND] {};
    Biquad lp[2];
    std::int64_t n = 0, nd = 0;
    int contDec = 0;
    std::vector<double> dif, cmn;
    std::vector<float> jan;

    Quadro quadros[NQ];
    std::int64_t nq = 0;
    double c0 = 0.0;


    Grao graos[maxGraos] {};
    int nGraos = 0;
    double sProx = 0.0, hAnt = 288.0, gama = 1.0, desl = 0.0;
    float mix = 1.0f, ganho = 1.0f;

    bool vozAnt = false;
    double nivelMax = 0.0;
    double m3[3] {};
    int nm3 = 0;

    double notaBuf[64] {};
    std::int64_t notaN = 0;
    int contaSalto = 0, alvoAnt = -1;
    double cApl = 0.0;
    double cenHist[8] {};
    std::int64_t ncen = 0;

    double histPc[12] {};
    std::int64_t quadrosVoz = 0;
    bool tomTravado = false;
    int tomAtual = -1, candidato = -1, contaCand = 0, quadrosDesdeAval = 0, semVoz = 0;
};
} // namespace onkey
