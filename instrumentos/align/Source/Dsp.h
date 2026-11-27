// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#pragma once

// Motor do ALIGN em C++ puro (sem JUCE), para o tools/teste_dsp.cpp medir fora do plugin.
//
// O baixo entra pelo canal principal e o kick pelo sidechain. Latência zero: nada olha para a frente.
//  - O baixo é dividido em grave e agudo por um crossover Linkwitz-Riley de 4ª ordem no X-OVER, que acompanha a
//    nota (~1,8 × a fundamental, 70 a 120 Hz). As duas metades somam plano (só um allpass, sem mudar o timbre).
//  - O grave do baixo passa por um BANCO de 16 allpass de 2ª ordem (18 a 170 Hz) + o caminho direto. Cada saída do
//    banco, com polaridade normal ou invertida, é uma "rotação" candidata (34 no total). O banco roda o tempo todo,
//    então a troca de correção é só um crossfade entre duas saídas que já estão quentes (sem clique).
//  - A cada batida do kick (onset no sidechain) fecha a janela da batida anterior: para cada candidata soma
//    Σ kick_grave · baixo_candidato. A que dá o maior valor é a que soma mais grave (menos cancelamento). A conta
//    é guardada por NOTA do baixo (a nota que tocava onde o kick e o baixo mais se cruzaram) e só troca a correção
//    guardada se o ganho passar de 1 dB.
//  - A correção da nota que está tocando é aplicada enquanto o kick soa (crossfade de 10 ms).
//  - LEVEL GUARD: depois de alinhar, a soma kick + baixo passaria da soma de potência (embola). Um ganho só no
//    grave do baixo, calculado pelos envelopes (não amostra a amostra, para não seguir o harmônico), segura a soma
//    em eK² + eB²: nem cancelamento nem soma. Teto de −6 dB.
//  - DUCK (0.3.0): sidechain clássico com dosagem. O baixo inteiro abaixa junto com o envelope do kick (o quanto o
//    kick está soando em relação ao pico dele), até −DUCK × 100%. Em 0 não faz nada (projeto antigo abre igual).
//  - O desligado (ALIGN off) devolve a entrada sem nenhuma alteração; a análise continua (serve de A/B e já
//    chega aprendido quando liga).
//  - TIMBRE PRESERVADO (0.3.5, preservaTimbre = true; o BAREBASS segue no modo antigo): o crossover sozinho já é um
//    allpass (~−110° na fundamental) e girar/inverter SÓ o grave abre um buraco no X-OVER (o baixo "afina"). Então:
//      · há a opção SECO = o baixo inteiro sem passar pelo crossover (± polaridade). Ela é o ponto de partida (nada
//        muda antes de aprender) e só perde para uma rotação que some pelo menos margemTimbre dB a mais;
//      · POLARITY inverte o baixo INTEIRO (grave e agudo juntos), não só o grave: inverter não mexe no timbre;
//      · no SECO o LEVEL GUARD é um low shelf dinâmico (fase mínima, sem crossover); em ganho 1 é a identidade;
//      · PHASE, POLARITY e LEVEL GUARD desligados = a entrada sai intacta (só o DUCK, se houver).

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace align
{
constexpr double pi = 3.14159265358979323846;

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// filtro TPT (Simper): pode mudar a frequência a cada amostra sem estourar
struct Svf
{
    double a1 = 0, a2 = 0, a3 = 0, k = 1.41421356, ic1 = 0, ic2 = 0;
    void set (double fs, double fc, double q)
    {
        const double g = std::tan (pi * std::min (fc, fs * 0.45) / fs);
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    double lp (double v0)
    {
        const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v2;
    }
    double hp (double v0)
    {
        const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v0 - k * v1 - v2;
    }
};

// Linkwitz-Riley de 4ª ordem (duas Butterworth de 2ª). LP + HP = allpass (soma plana, sem buraco no X-OVER) e as
// duas metades ficam em fase entre si: dá para girar o grave sem que o agudo "devolva" grave na soma.
struct Lr4
{
    Svf a, b, c, d;
    void set (double fs, double fc)
    {
        for (auto* s : { &a, &b, &c, &d }) s->set (fs, fc, 0.70710678);
    }
    void reset() { a.reset(); b.reset(); c.reset(); d.reset(); }
    double lp (double x) { return b.lp (a.lp (x)); }
    double hp (double x) { return d.hp (c.hp (x)); }
};

// low shelf TPT (Simper), modulável: ganho linear no grave = gLin (A² = gLin). Em gLin = 1 devolve a entrada exata.
struct LowShelf
{
    double a1 = 1, a2 = 0, a3 = 0, m1 = 0, m2 = 0, k = 1.41421356, ic1 = 0, ic2 = 0;
    void set (double fs, double fc, double gLin)
    {
        const double A = std::sqrt (std::max (gLin, 1.0e-4));
        const double g = std::tan (pi * std::min (fc, fs * 0.45) / fs) / std::sqrt (A);
        a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        m1 = k * (A - 1.0); m2 = A * A - 1.0;
    }
    void reset() { ic1 = ic2 = 0; }
    double process (double v0)
    {
        const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v0 + m1 * v1 + m2 * v2;
    }
};

// allpass de 2ª ordem (RBJ), em double (frequências baixas a 192 kHz)
struct Allpass
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (double fs, double fc, double q)
    {
        const double w = 2 * pi * fc / fs, al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = (1 - al) / a0; b1 = -2 * std::cos (w) / a0; b2 = 1.0;   // b2 = (1+al)/a0 = 1
        a1 = b1; a2 = b0;
    }
    void reset() { z1 = z2 = 0; }
    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    // fase (graus, negativa = atraso) na frequência f
    double phaseDeg (double fs, double f) const
    {
        const std::complex<double> z1c = std::polar (1.0, -2 * pi * f / fs), z2c = z1c * z1c;
        const auto h = (b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c);
        return std::arg (h) * 180.0 / pi;
    }
};

constexpr int numAp = 16;            // allpass do banco
constexpr int numCand = numAp + 1;   // + o caminho direto (índice 0)
constexpr int numOpc = numCand * 2;  // × polaridade (opção = cand * 2 + invertida)
constexpr int candSeco = numCand;    // 0.3.5: o baixo inteiro sem crossover (opções numOpc e numOpc + 1)
constexpr int opSeco = numOpc;
constexpr int numOpcTot = numOpc + 2;
constexpr int maxCh = 2;
constexpr int semNota = 128;         // memória de quando a nota não foi reconhecida

inline const char* nomeNota (int n, char* buf)
{
    static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (n < 0 || n > 127) { std::strcpy (buf, "--"); return buf; }
    std::snprintf (buf, 8, "%s%d", nn[n % 12], n / 12 - 1);
    return buf;
}

// memória de uma nota
struct Memoria
{
    double x[numCand] {};   // Σ kick·candidato, médio
    double ek = 0, eb = 0;  // energias médias do grave do kick e do baixo na janela
    double xHi = 0;         // Σ kick·agudo do baixo (comum a todas as rotações; polaridade inteira inverte junto)
    double xSeco = 0;       // Σ kick·baixo inteiro (opção SECO)
    double xIn = 0;         // Σ kick_grave·baixo_grave da entrada (o cruzamento do SECO no grave, p/ o LEVEL GUARD)
    int opcao = -1;         // correção guardada (-1 = ainda nenhuma)
    int opcaoRestrita = -1; // a melhor só entre as opções que a dosagem permite (PHASE/POLARITY desligados)
    int restricaoDe = -1;   // para qual combinação de PHASE/POLARITY a opcaoRestrita foi escolhida
    int vezes = 0;
};

// o que a tela lê (atômicos, escritos no áudio)
struct Leitura
{
    std::atomic<float> lowEndDb { 0.0f };    // perda (−) ou soma (+) no grave em relação à soma de potência
    std::atomic<float> corr { 0.0f };        // correlação kick × baixo no grave (−1..+1)
    std::atomic<float> duckDb { 0.0f };      // LEVEL GUARD: menor ganho da última batida
    std::atomic<float> duckScDb { 0.0f };    // DUCK: menor ganho do último bloco (dB, ≤ 0)
    std::atomic<float> rotDeg { 0.0f };      // rotação aplicada na fundamental da nota (graus, 0..−360)
    std::atomic<int>   invertida { 0 };
    std::atomic<float> xoverHz { 90.0f };
    std::atomic<float> f0 { 0.0f };          // fundamental da nota atual do baixo
    std::atomic<int>   nota { -1 };          // nota atual (MIDI) ou −1
    std::atomic<int>   estado { 0 };         // 0 sem sidechain, 1 aprendendo, 2 acompanhando
    std::atomic<float> batidaS { 0.0f };     // duração da batida medida pelos onsets (s)
    std::atomic<std::uint32_t> notas[4] {};  // bitset das notas com correção guardada
    std::atomic<bool>  temKick { false }, temBaixo { false };
};

// buffers do osciloscópio (anel, escritos no áudio, lidos pela tela sem trava: só desenho). Guardado a ~48 kHz
// (decimado em 88,2/96/192 kHz) num anel de tamanho fixo, alocado uma vez: trocar a taxa não realoca nada.
struct Scope
{
    static constexpr int size = 1 << 18, mask = size - 1;   // ~5,4 s a 48 kHz
    std::vector<float> kF, kL, bF, bL, sem, alvo;   // kick cheio/grave, baixo (saída) cheio/grave, grave sem LEVEL GUARD, alvo
    std::atomic<std::int64_t> pos { 0 };            // amostras guardadas (absoluto)
    std::atomic<double> taxa { 48000.0 };           // amostras guardadas por segundo
    int dec = 1;
    static constexpr int numOnsets = 16;
    std::atomic<std::int64_t> onsets[numOnsets] {};
    std::atomic<int> numOn { 0 };

    Scope() { for (auto* v : { &kF, &kL, &bF, &bL, &sem, &alvo }) v->assign ((size_t) size, 0.0f); }

    void prepare (double fs)
    {
        dec = std::max (1, (int) std::lround (fs / 48000.0));
        taxa = fs / dec;
        pos = 0; numOn = 0;
        for (auto& o : onsets) o = 0;
    }
    void onset (std::int64_t p)
    {
        const int i = numOn.load();
        onsets[i % numOnsets].store (p);
        numOn.store (i + 1);
    }
};

class Engine
{
public:
    Leitura leitura;
    Scope scope;
    bool guard = true;      // LEVEL GUARD liga/desliga
    int forcaOpcao = -1;    // teste: aplica esta opção fixa em vez da aprendida

    // DOSAGEM (0.2.0): o ALIGN segue sempre automático; isto só limita o que ele pode fazer.
    // Os padrões reproduzem a 0.1.0 (projeto antigo abre igual).
    bool usaFase = true;        // PHASE: rotação do grave (o banco de allpass). Desligado = só o caminho direto
    bool usaPolaridade = true;  // POLARITY: pode inverter o grave
    double guardDepth = 1.0;    // LEVEL GUARD DEPTH 0..1: g = 1 − depth·(1 − gNota)
    int react = 1;              // REACT 0 lento, 1 médio, 2 rápido
    double xoMax = 110.0;       // X-OVER MAX (60..150 Hz): teto do X-OVER que acompanha a nota
    double duck = 0.0;          // DUCK 0..1 (0.3.0): quanto o baixo abaixa com o kick (0 = desligado)
    bool preservaTimbre = false;  // 0.3.5: SECO + polaridade inteira + shelf (ALIGN liga; BAREBASS fica no antigo)
    double margemTimbre = 1.0;    // dB que uma rotação tem de somar a mais que o SECO para valer

    // o host muda a dosagem bloco a bloco (sem alocar; o REACT ajusta o crossfade)
    void setReact (int r)
    {
        r = std::clamp (r, 0, 2);
        if (r == react && fadeReact == r) return;
        react = r; fadeReact = r;
        static constexpr double ms[3] = { 0.020, 0.010, 0.005 };
        fade = 1.0 / (ms[r] * fs);
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < numAp; ++i)
            fcAp[i] = 18.0 * std::pow (170.0 / 18.0, i / (double) (numAp - 1));
        for (int c = 0; c < maxCh; ++c)
            for (int i = 0; i < numAp; ++i) ap[c][i].set (fs, fcAp[i], qAp);
        for (int i = 0; i < numAp; ++i) apMid[i].set (fs, fcAp[i], qAp);
        xo = std::min (90.0, xoMax); xoAlvo = xo; aplicaXo();
        fadeReact = -1; setReact (react);
        wetPasso = 1.0 / (0.020 * fs);
        envA = std::exp (-1.0 / (0.0003 * fs)); envR = std::exp (-1.0 / (0.020 * fs));
        slowC = std::exp (-1.0 / (0.200 * fs));
        lowR = std::exp (-1.0 / (0.030 * fs));
        bPicoC = std::exp (-1.0 / (1.0 * fs));
        gEnv = std::exp (-1.0 / (0.012000000000000000452177 * fs));
        gAtk = std::exp (-1.0 / (0.004 * fs)); gRel = std::exp (-1.0 / (0.015 * fs));
        xoK = 1.0 - std::exp (-32.0 / (0.050 * fs));
        dRel = std::exp (-1.0 / (0.040 * fs)); dPicoC = std::exp (-1.0 / (2.0 * fs));
        dAtk = std::exp (-1.0 / (0.001 * fs)); dSolta = std::exp (-1.0 / (0.012 * fs));
        scope.prepare (fs);
        reset();
    }

    void reset()
    {
        for (int c = 0; c < maxCh; ++c) { split[c].reset(); for (auto& a : ap[c]) a.reset(); }
        for (auto& a : apMid) a.reset();
        for (auto* f : { &kSplit, &bInDisp, &bOutDisp, &bSemDisp }) f->reset();
        for (auto& s : shelf) s.reset();
        gShelf = 1.0; xoShelf = -1.0;
        for (auto& m : mem) m = Memoria();
        for (auto& b : leitura.notas) b = 0;
        novaJanela();
        opAtual = opNeutra(); opAntes = opAtual; xf = 1.0; wet = 0.0;
        envF = envS = 0; kLowEnv = 0; bEnv = 0; bPico = 0; jJunto = false;
        desdeOnset = 1 << 30; semKick = 1 << 30;
        eK2 = eB2 = 0; g = 1.0; gNota = 1.0;
        dEnv = 0; dPico = 0; gd = 1.0;
        zcTem = false; zcArmado = false; zcPer = 0; blAnt = 0; notaAgora = -1; notaAplica = -1; f0Agora = 0;
        ultOnset = -1; numInt = 0;
        agora = 0; scopePos = 0; decConta = 0;
    }

    // baixo: nch canais (1 ou 2), processado no lugar. kick: nk canais (0 = sem sidechain).
    void process (float* const* bass, int nch, const float* const* kick, int nk, int n, bool ligado)
    {
        nch = std::min (nch, maxCh);
        const double wetAlvo = ligado ? 1.0 : 0.0;
        double gdMin = 1.0;
        for (int i = 0; i < n; ++i, ++agora)
        {
            // ---- kick (mono) ----
            double k = 0;
            if (nk > 0) { for (int c = 0; c < nk; ++c) k += kick[c][i]; k /= nk; }
            const double kl = kSplit.lp (k);
            onsetDoKick (k);
            const double ka = std::fabs (kl);
            kLowEnv = ka > kLowEnv ? ka : kLowEnv * lowR;
            kLowPico = std::max (kLowPico, kLowEnv);

            // ---- baixo: grave de cada canal e o banco ----
            double y[maxCh][numCand];
            double hiC[maxCh], xC[maxCh];
            double bm = 0, hm = 0;
            for (int c = 0; c < nch; ++c)
            {
                const double x = bass[c][i];
                xC[c] = x; bm += x;
                const double lo = split[c].lp (x);
                hiC[c] = split[c].hp (x); hm += hiC[c];
                y[c][0] = lo;
                for (int a = 0; a < numAp; ++a) y[c][a + 1] = ap[c][a].process (lo);
            }
            bm /= nch; hm /= nch;
            // mid para a análise (linear: média dos canais)
            double ym[numCand];
            for (int a = 0; a < numCand; ++a) { double s = 0; for (int c = 0; c < nch; ++c) s += y[c][a]; ym[a] = s / nch; }
            const double bl = ym[0];
            bEnv = std::fabs (bl) > bEnv ? std::fabs (bl) : bEnv * lowR;
            bPico = std::max (bPico * bPicoC, bEnv);
            altura (bl);

            // ---- janela da batida ----
            // Só conta onde a cauda do kick e uma nota do baixo tocam juntas (fora disso é cabeça do kick com resto de
            // filtro do baixo, que engana a conta). Cada candidata já é o grave que sairia (com a fase do crossover),
            // então ela é comparada com o kick CRU: o que soa na mix é kick + candidata.
            const double bIn = bInDisp.lp (bm);   // entrada do baixo no grave, com o mesmo filtro do kick (medidor)
            if (bEnv > 0.25000000000000000669344 * bPico && bPico > 1e-4 && kLowEnv > 0.02 * kLowPicoAnt)
            {
                for (int a = 0; a < numCand; ++a) jx[a] += k * ym[a];
                jxHi += k * hm; jxSeco += k * bm;
                jek += kl * kl; jeb += bIn * bIn; jxIn += kl * bIn;
                ++jn;
                jJunto = true;
            }
            else jJunto = false;
            const double sobre = kLowEnv * bEnv;   // onde kick e baixo mais se cruzam: é a nota que vale
            if (sobre > jSobreMax) { jSobreMax = sobre; jNota = notaAgora; }

            // ---- correção da nota que está tocando (troca enquanto o kick soa, com crossfade) ----
            // No onset vale a nota que foi o "b" da batida anterior (no rolling a nota quase sempre repete); quando
            // a altura da nota nova é reconhecida (~2 ciclos), vale ela. Nota sem memória: mantém a correção atual.
            const bool kickSoa = kLowEnv > 0.05 * kLowPicoAnt;
            if (xf >= 1.0 && kickSoa && semKick < (std::int64_t) (2.0 * fs))
            {
                const int alvo = forcaOpcao >= 0 ? forcaOpcao : opcaoPermitida (mem[notaAplica >= 0 ? notaAplica : semNota]);
                if (alvo >= 0 && alvo != opAtual) { opAntes = opAtual; opAtual = alvo; xf = 0.0; }
            }
            {
                const Memoria& mm = mem[notaAplica >= 0 ? notaAplica : semNota];
                const double xs = ((opAtual & 1) ? -1.0 : 1.0) * ((opAtual >> 1) == candSeco ? mm.xIn : mm.x[opAtual >> 1]);
                gNota = (mm.vezes > 0 && xs > 0 && mm.eb > 1e-12)
                          ? std::clamp ((-xs + std::sqrt (xs * xs + mm.eb * mm.eb)) / mm.eb, 0.5, 1.0) : 1.0;
            }
            const int ca = opAtual >> 1, cb = opAntes >> 1;
            const double sa = (opAtual & 1) ? -1.0 : 1.0, sb = (opAntes & 1) ? -1.0 : 1.0;
            const double t = xf;
            double yAp[maxCh], yApM;
            {
                const int ya = std::min (ca, numCand - 1), yb = std::min (cb, numCand - 1);   // SECO não usa o banco
                for (int c = 0; c < nch; ++c) yAp[c] = t * sa * y[c][ya] + (1 - t) * sb * y[c][yb];
                yApM = t * sa * ym[ya] + (1 - t) * sb * ym[yb];
                if (xf < 1.0) xf = std::min (1.0, xf + fade);
            }

            // ---- LEVEL GUARD ----
            // Pela energia da NOTA (memória: Σ kick·baixo e as energias no trecho em que os dois se cruzam), não
            // amostra a amostra: no ataque de cada nota o baixo ainda está em zero e a conta instantânea cortaria
            // tudo. Ganho g que deixa o trecho na soma de potência: Ek + g²·Eb + 2g·X = Ek + Eb. Vale só enquanto a
            // cauda do kick e a nota tocam juntas; fora disso, 1.
            eK2 = gEnv * eK2 + (1 - gEnv) * kl * kl;
            eB2 = gEnv * eB2 + (1 - gEnv) * bIn * bIn;
            double gAlvo = 1.0;
            if (guard && jJunto) gAlvo = 1.0 - std::clamp (guardDepth, 0.0, 1.0) * (1.0 - gNota);
            g = gAlvo < g ? gAtk * g + (1 - gAtk) * gAlvo : gRel * g + (1 - gRel) * gAlvo;

            // ---- saída ----
            wet = wetAlvo > wet ? std::min (wetAlvo, wet + wetPasso) : std::max (wetAlvo, wet - wetPasso);
            // DUCK: envelope do kick inteiro (ataque na hora, solta em 40 ms) relativo ao pico dele (2 s)
            {
                const double ak = std::fabs (k);
                dEnv = ak > dEnv ? ak : dEnv * dRel;
                dPico = std::max (dPico * dPicoC, dEnv);
                const double alvo = duck > 0.0 && dPico > 1e-4 ? 1.0 - std::clamp (duck, 0.0, 1.0) * std::min (1.0, dEnv / dPico) : 1.0;
                gd = alvo < gd ? dAtk * gd + (1 - dAtk) * alvo : dSolta * gd + (1 - dSolta) * alvo;
            }
            const double gDuck = 1.0 - wet * (1.0 - gd);
            gdMin = std::min (gdMin, gDuck);
            double outFull, semFull;
            if (! preservaTimbre)
            {
                for (int c = 0; c < nch; ++c)
                    if (wet > 0.0) bass[c][i] = (float) (gDuck * (xC[c] + wet * (hiC[c] + g * yAp[c] - xC[c])));
                outFull = bm + wet * (hm + g * yApM - bm);
                semFull = bm + wet * (hm + yApM - bm);
            }
            else
            {
                // SECO: low shelf dinâmico no baixo inteiro (ganho 1 = identidade). Rotação: crossover + banco, e a
                // polaridade vale para o baixo INTEIRO (agudo junto), sem buraco no X-OVER.
                const bool usaSeco = ca == candSeco || (t < 1.0 && cb == candSeco);
                if (usaSeco && ((agora & 7) == 0 || std::fabs (g - gShelf) > 0.002))
                    if (std::fabs (g - gShelf) > 1.0e-6 || xoShelf != xo)
                    {
                        gShelf = g; xoShelf = xo;
                        for (auto& s : shelf) s.set (fs, 1.25 * xo, gShelf);
                    }
                auto via = [&] (int cand, double sgn, int c, double seco, double gg)
                { return sgn * (cand == candSeco ? seco : hiC[c] + gg * y[c][cand]); };
                outFull = 0; semFull = 0;
                for (int c = 0; c < nch; ++c)
                {
                    const double seco = shelf[c].process (xC[c]);
                    double o = t * via (ca, sa, c, seco, g), so = t * via (ca, sa, c, xC[c], 1.0);
                    if (t < 1.0) { o += (1 - t) * via (cb, sb, c, seco, g); so += (1 - t) * via (cb, sb, c, xC[c], 1.0); }
                    if (wet > 0.0) bass[c][i] = (float) (gDuck * (xC[c] + wet * (o - xC[c])));
                    outFull += o; semFull += so;
                }
                outFull = bm + wet * (outFull / nch - bm);
                semFull = bm + wet * (semFull / nch - bm);
            }
            // saída no grave com o mesmo filtro do kick: na tela e no medidor, a fase entre os dois é a real
            const double outLow = bOutDisp.lp (outFull);
            const double semLow = bSemDisp.lp (semFull);
            jgMin = std::min (jgMin, wet > 0.5 ? g : 1.0);
            if (jJunto) { jxo += kl * outLow; jeo += outLow * outLow; }

            // ---- osciloscópio ----
            if (++decConta >= scope.dec)
            {
                decConta = 0;
                const int p = (int) (scopePos & Scope::mask);
                scope.kF[p] = (float) k; scope.kL[p] = (float) kl;
                scope.bF[p] = (float) outFull; scope.bL[p] = (float) outLow;
                scope.sem[p] = (float) semLow;
                scope.alvo[p] = (float) std::sqrt (2.0 * (eK2 + eB2));
                ++scopePos;
            }

            // X-OVER acompanha a nota (devagar, sem degrau)
            if ((agora & 31) == 0) xoAlvo = std::min (xoAlvo, xoMax);
            if ((agora & 31) == 0 && std::fabs (xoAlvo - xo) > 0.01)
            {
                xo += (xoAlvo - xo) * xoK;
                aplicaXo();
            }
        }
        scope.pos.store (scopePos);
        leitura.duckScDb = gainToDb ((float) gdMin);
        leitura.temKick = semKick < (std::int64_t) (2.0 * fs);
        leitura.temBaixo = bEnv > 1e-4;
        leitura.estado = ! leitura.temKick.load() ? 0 : (mem[notaAplica >= 0 ? notaAplica : semNota].opcao >= 0 ? 2 : 1);
        // fase e polaridade desligadas juntas: nada a aplicar, volta ao direto (com crossfade, sem clique)
        if (forcaOpcao < 0 && ! usaFase && ! usaPolaridade && opAtual != opNeutra() && xf >= 1.0) { opAntes = opAtual; opAtual = opNeutra(); xf = 0.0; }
        // 0.3.5: dosagem mudou e a correção aplicada não é mais permitida (ex.: desligou o PHASE) = volta já, sem
        // esperar a próxima batida
        if (forcaOpcao < 0 && preservaTimbre && ! permitida (opAtual) && xf >= 1.0)
        {
            const int alvo = opcaoPermitida (mem[notaAplica >= 0 ? notaAplica : semNota]);
            opAntes = opAtual; opAtual = alvo >= 0 && permitida (alvo) ? alvo : opNeutra(); xf = 0.0;
        }
        leitura.nota = notaAplica;
        leitura.f0 = (float) f0Agora;
        leitura.xoverHz = (float) xo;
        leitura.invertida = opAtual & 1;
        const double fRef = f0Agora > 0 ? f0Agora : 50.0;
        double rot = (opAtual >> 1) == 0 || (opAtual >> 1) == candSeco ? 0.0 : apMid[(opAtual >> 1) - 1].phaseDeg (fs, fRef);
        if (rot > 0) rot -= 360.0;
        leitura.rotDeg = (float) rot;
    }

    // para o teste e a tela
    int opcaoAtual() const { return opAtual; }
    const Memoria& memoria (int nota) const { return mem[nota >= 0 && nota < 128 ? nota : semNota]; }
    double fcDoBanco (int i) const { return fcAp[i]; }

private:
    int restricao() const { return (usaFase ? 1 : 0) | (usaPolaridade ? 2 : 0); }
    // sem correção nenhuma: o caminho direto (antigo) ou o SECO (0.3.5, a entrada intacta)
    int opNeutra() const { return preservaTimbre ? opSeco : 0; }
    int numOpcoes() const { return preservaTimbre ? numOpcTot : numOpc; }
    bool permitida (int o) const
    {
        const int c = o >> 1;
        if (preservaTimbre ? (c != candSeco && ! usaFase) : (c != 0 && ! usaFase)) return false;
        if (c == candSeco && ! preservaTimbre) return false;
        return usaPolaridade || (o & 1) == 0;
    }
    // o quanto a opção soma com o kick (Σ kick·saída). No modo antigo só o grave; no 0.3.5 o baixo inteiro, porque a
    // polaridade vale para o agudo também e o SECO concorre com as rotações
    double valor (const Memoria& m, int o) const
    {
        const double s = (o & 1) ? -1.0 : 1.0;
        const int c = o >> 1;
        if (! preservaTimbre) return s * m.x[c];
        return c == candSeco ? s * m.xSeco : s * (m.x[c] + m.xHi);
    }
    double ganhoDb (const Memoria& m, double va, double vb) const   // quanto va soma a mais que vb (dB)
    {
        const double base = m.ek + m.eb;
        return 10.0 * std::log10 (std::max (base + 2 * va, 1e-12 * base) / std::max (base + 2 * vb, 1e-12 * base));
    }
    // a melhor entre as opções (todas ou só as permitidas). No 0.3.5 uma rotação só vence o SECO se somar pelo menos
    // margemTimbre dB a mais (rotação mexe no timbre; o SECO não)
    int escolhe (const Memoria& m, bool soPermitidas) const
    {
        int melhor = opNeutra(), seco = -1; double v = -1e300, vs = -1e300;
        for (int o = 0; o < numOpcoes(); ++o)
        {
            if (soPermitidas && ! permitida (o)) continue;
            const double vo = valor (m, o);
            if (vo > v) { v = vo; melhor = o; }
            if ((o >> 1) == candSeco && vo > vs) { vs = vo; seco = o; }
        }
        if (preservaTimbre && seco >= 0 && (melhor >> 1) != candSeco && ganhoDb (m, v, vs) < margemTimbre) melhor = seco;
        return melhor;
    }
    int melhorRestrita (const Memoria& m) const { return escolhe (m, true); }

    // a correção que vale para esta nota com a dosagem atual (−1 = ainda nada aprendido)
    int opcaoPermitida (const Memoria& m) const
    {
        if (m.opcao < 0) return -1;
        if (usaFase && usaPolaridade) return m.opcao;
        if (! usaFase && ! usaPolaridade) return opNeutra();
        if (m.restricaoDe == restricao() && m.opcaoRestrita >= 0 && permitida (m.opcaoRestrita)) return m.opcaoRestrita;
        return melhorRestrita (m);   // a dosagem acabou de mudar: a melhor entre as permitidas (a próxima batida fixa)
    }

    void aplicaXo()
    {
        for (int c = 0; c < maxCh; ++c) split[c].set (fs, xo);
        for (auto* f : { &kSplit, &bInDisp, &bOutDisp, &bSemDisp }) f->set (fs, xo);
    }

    void novaJanela()
    {
        std::fill (std::begin (jx), std::end (jx), 0.0);
        jek = jeb = jxo = jeo = jxIn = jxHi = jxSeco = 0; jn = 0; jSobreMax = 0; jNota = -1; jgMin = 1.0;
        kLowPicoAnt = std::max (kLowPico, 1e-9); kLowPico = 0;
    }

    void onsetDoKick (double k)
    {
        const double a = std::fabs (k);
        envF = a > envF ? envA * envF + (1 - envA) * a : envR * envF;
        const bool novo = envF > 3.0 * envS + 1e-6 && envF > 0.01 && desdeOnset > (std::int64_t) (0.08000000000000000168804 * fs);
        envS = slowC * envS + (1 - slowC) * envF;
        ++desdeOnset;
        if (envF > 0.003000000000000000258444) semKick = 0; else ++semKick;
        if (! novo) return;
        if (ultOnset >= 0)
        {
            const double dt = (double) (agora - ultOnset) / fs;
            if (dt > 0.15 && dt < 2.0) { intervalos[numInt++ % 4] = dt; }
            double v[4]; const int m = std::min (numInt, 4);
            for (int j = 0; j < m; ++j) v[j] = intervalos[j];
            std::sort (v, v + m);
            if (m > 0) leitura.batidaS = (float) v[m / 2];
        }
        ultOnset = agora; desdeOnset = 0;
        scope.onset (scopePos);
        if (jNota >= 0) notaAplica = jNota;
        fechaJanela();
        novaJanela();
    }

    // fim da batida: aprende para a nota e atualiza os medidores
    void fechaJanela()
    {
        if (jn < (int) (0.010 * fs)) return;   // menos de 10 ms de cruzamento: nada a medir
        const double ek = jek / jn, eb = jeb / jn;
        if (ek < 1e-8 || eb < 1e-8) { leitura.lowEndDb = 0.0f; leitura.corr = 0.0f; return; }   // um dos dois mudo
        const double norm = std::sqrt (jek * jeb);
        if (norm <= 0) return;

        // medidores: com o ALIGN desligado mede a entrada; ligado, a saída
        const double soPot = jek + jeb;
        double soma, corr;
        if (wet > 0.5) { soma = jek + jeo + 2 * jxo; corr = jxo / std::sqrt (jek * std::max (jeo, 1e-12)); }
        else           { soma = soPot + 2 * jxIn; corr = jxIn / norm; }
        const float db = (float) (10.0 * std::log10 (std::max (soma, 1e-12) / soPot));
        leitura.lowEndDb = 0.6f * leitura.lowEndDb.load() + 0.4f * db;
        leitura.corr = 0.6f * leitura.corr.load() + 0.4000000000000000475668f * (float) std::clamp (corr, -1.0000000000000000578674, 1.0);
        leitura.duckDb = gainToDb ((float) jgMin);

        // pouco cruzamento (kick e baixo não se encostam): nada a aprender
        double maxX = 0; for (double v : jx) maxX = std::max (maxX, std::fabs (v));
        if (maxX / norm < 0.05) return;

        const int slot = jNota >= 0 ? jNota : semNota;
        Memoria& m = mem[slot];
        static constexpr double aReact[3] = { 0.2, 0.4, 0.7 }, histReact[3] = { 1.5, 1.0, 0.5 };
        const double a = m.vezes == 0 ? 1.0 : aReact[std::clamp (react, 0, 2)];
        for (int c = 0; c < numCand; ++c) m.x[c] += a * (jx[c] / jn - m.x[c]);
        m.xHi += a * (jxHi / jn - m.xHi); m.xSeco += a * (jxSeco / jn - m.xSeco); m.xIn += a * (jxIn / jn - m.xIn);
        m.ek += a * (ek - m.ek); m.eb += a * (eb - m.eb);
        ++m.vezes;

        const int melhor = escolhe (m, false);
        const int atual = m.opcao >= 0 ? m.opcao : opNeutra();
        const double ganho = ganhoDb (m, valor (m, melhor), valor (m, atual));
        if (m.opcao < 0 || ganho > histReact[std::clamp (react, 0, 2)]) m.opcao = melhor;
        // a correção com fase/polaridade limitadas também troca só acima da histerese (sem ficar pulando)
        {
            const int mr = melhorRestrita (m);
            const int ar = m.opcaoRestrita >= 0 ? m.opcaoRestrita : opNeutra();
            const double gr = ganhoDb (m, valor (m, mr), valor (m, ar));
            if (m.opcaoRestrita < 0 || ! permitida (ar) || gr > histReact[std::clamp (react, 0, 2)]) m.opcaoRestrita = mr;
            m.restricaoDe = restricao();
        }
        if (slot < 128) leitura.notas[slot >> 5].fetch_or (1u << (slot & 31));
    }

    // altura do baixo pelo cruzamento por zero (subida) do grave, com histerese
    void altura (double bl)
    {
        if (bEnv < 1e-4) { zcTem = false; zcArmado = false; zcPer = 0; blAnt = bl; return; }
        if (bl < -0.15 * bEnv) zcArmado = true;   // histerese: só conta a subida depois de um vale de verdade
        if (zcArmado && blAnt <= 0.0 && bl > 0.0)
        {
            zcArmado = false;
            const double t = (double) (agora - 1) + (-blAnt) / std::max (bl - blAnt, 1e-12);   // instante interpolado
            if (zcTem)
            {
                const double per = t - zcTempo, f = fs / per;
                if (f >= 25.0 && f <= 160.0)
                {
                    if (zcPer > 0 && std::fabs (per - zcPer) < 0.03 * per)   // dois ciclos iguais: é a nota
                    {
                        const double fm = fs / (0.5 * (per + zcPer));
                        const int nt = std::clamp ((int) std::lround (69.0 + 12.0 * std::log2 (fm / 440.0)), 0, 127);
                        notaAgora = nt; notaAplica = nt; f0Agora = fm;
                        xoAlvo = std::clamp (1.6 * fm, 60.0, std::clamp (xoMax, 60.0, 150.0));
                    }
                    zcPer = per;
                }
                else zcPer = 0;
            }
            zcTempo = t; zcTem = true;
        }
        blAnt = bl;
    }

    double fs = 48000.0;
    static constexpr double qAp = 0.7;
    double fcAp[numAp] {};
    Lr4 split[maxCh], kSplit, bInDisp, bOutDisp, bSemDisp;
    Allpass ap[maxCh][numAp], apMid[numAp];
    LowShelf shelf[maxCh];   // LEVEL GUARD no SECO (0.3.5)
    double gShelf = 1.0, xoShelf = -1.0;
    double xo = 90.0, xoAlvo = 90.0, xoK = 0;

    // janela da batida
    double jx[numCand] {}, jek = 0, jeb = 0, jxo = 0, jeo = 0, jxIn = 0, jSobreMax = 0, jgMin = 1.0;
    double jxHi = 0, jxSeco = 0;
    int jn = 0, jNota = -1;
    Memoria mem[129];

    int opAtual = 0, opAntes = 0;
    double xf = 1.0000000000000000266277, fade = 0.001, wet = 0.0, wetPasso = 0.001;
    int fadeReact = -1;

    // onset do kick
    double envF = 0, envS = 0, envA = 0, envR = 0, slowC = 0;
    std::int64_t desdeOnset = 1 << 30, semKick = 1 << 30, ultOnset = -1;
    double intervalos[4] {}; int numInt = 0;
    double kLowEnv = 0, kLowPico = 0, kLowPicoAnt = 1e-9, lowR = 0, bEnv = 0, bPico = 0, bPicoC = 0;
    bool jJunto = false;

    // LEVEL GUARD
    double eK2 = 0, eB2 = 0, g = 1.0, gNota = 1.0, gEnv = 0, gAtk = 0, gRel = 0;

    // DUCK
    double dEnv = 0, dPico = 0, dRel = 0, dPicoC = 0, dAtk = 0, dSolta = 0, gd = 1.0;

    // altura
    bool zcTem = false, zcArmado = false;
    int notaAgora = -1, notaAplica = -1;
    double zcTempo = 0, zcPer = 0, blAnt = 0, f0Agora = 0;

    std::int64_t agora = 0, scopePos = 0;
    int decConta = 0;
};
} // namespace align
