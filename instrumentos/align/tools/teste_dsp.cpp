// Mede o Dsp.h fora do plugin com um rolling de psytrance a 145 BPM (K b B B, o b a −3 dB): kick com queda de
// 140 → fundamental da nota e baixo SAW com filtro LP ~250 Hz e envelope de filtro (sustain 0), como o real.
// zig c++ -O2 -std=c++17 -target x86_64-windows-gnu teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe
//
// Por batida imprime o grave (< 100 Hz) da soma kick + baixo em relação à soma de potência (0 dB = nem
// cancelamento nem soma): ENTRADA (como chegou) e SAÍDA (depois do ALIGN). Também confere que, desligado, a saída
// é idêntica à entrada, e que a parte de cima do baixo não muda.
#include "../Source/Dsp.h"
#include <cstdio>
#include <vector>
using namespace align;

struct Sim
{
    double fs = 48000, bpm = 145;
    double offset = 0;        // fase da nota do baixo no início (rad): é o que desalinha
    std::vector<int> notas;   // nota do baixo de cada batida
    double kickPh = 0;
    Svf filtro;
    double t = 0;

    static double hz (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }

    void amostra (double& k, double& b)
    {
        const double beat = 60.0 / bpm, s16 = beat / 4;
        const int nb = (int) std::floor (t / beat);
        const double tb = t - nb * beat;
        const int nota = notas[(size_t) nb % notas.size()];
        const double f1 = hz (nota);
        // kick: o mesmo a música inteira, queda exponencial de 140 Hz até G1 (49 Hz)
        const double fk = 49.0 + (140.0 - 49.0) * std::exp (-tb * 90.0);
        kickPh += 2 * pi * fk / fs;
        k = std::sin (kickPh) * std::min (1.0, tb / 0.001) * std::exp (-tb * 13.0) * 0.9;
        if (tb < 1.0 / fs) kickPh = 0;
        // baixo: 3 notas por tempo, saw com LP de envelope
        const int slot = (int) (tb / s16);
        b = 0;
        if (slot > 0)
        {
            const double u = tb - slot * s16, gate = s16 * 0.86;
            const double ph = f1 * u + offset / (2 * pi);
            const double saw = 2.0 * (ph - std::floor (ph + 0.5));
            const double env = std::min (1.0, u / 0.002) * (u < gate ? std::exp (-u * 7) : std::exp (-gate * 7) * std::max (0.0, 1 - (u - gate) / 0.006));
            filtro.set (fs, 120.0 + 260.0 * std::exp (-u * 35.0), 0.9);
            b = filtro.lp (saw) * env * (slot == 1 ? 0.62 * 0.708 : 0.62);
        }
        else filtro.lp (0.0);
        t += 1.0 / fs;
    }
};

// medidas no trecho do "b" (2ª semicolcheia), onde a cauda do kick encontra o baixo
struct Batida { double ek = 0, ein = 0, eout = 0, sin = 0, sout = 0, hin = 0, hout = 0; };

static int roda (const char* nome, double offsetGraus, std::vector<int> notas, int batidasDesl, int total, bool detalhe)
{
    Sim s; s.offset = offsetGraus * pi / 180.0; s.notas = notas;
    Engine e; e.prepare (s.fs);
    Lr4 lk, li, lo, hi, ho; for (auto* f : { &lk, &li, &lo, &hi, &ho }) f->set (s.fs, 100);
    const double beat = 60.0 / s.bpm;
    const int b0 = (int) (beat / 4 * s.fs), b1 = 2 * b0;
    const int porBatida = (int) std::lround (beat * s.fs);
    const int bloco = 256;
    std::vector<Batida> bt ((size_t) total);
    double maxDifDesl = 0;
    std::printf ("\n== %s (fase do baixo %+.0f°)\n", nome, offsetGraus);
    for (int nb = 0; nb < total; ++nb)
    {
        const bool lig = nb >= batidasDesl;
        Batida& B = bt[(size_t) nb];
        for (int i0 = 0; i0 < porBatida; i0 += bloco)
        {
            const int n = std::min (bloco, porBatida - i0);
            float L[bloco], R[bloco], K[bloco], in[bloco];
            for (int i = 0; i < n; ++i) { double k, b; s.amostra (k, b); K[i] = (float) k; L[i] = R[i] = in[i] = (float) b; }
            float* ch[2] = { L, R }; const float* kc[1] = { K };
            e.process (ch, 2, kc, 1, n, lig);
            for (int i = 0; i < n; ++i)
            {
                if (! lig) maxDifDesl = std::max (maxDifDesl, (double) std::fabs (L[i] - in[i]));
                const double kl = lk.lp (K[i]), bi = li.lp (in[i]), bo = lo.lp (L[i]);
                const double hin = hi.hp (in[i]), hout = ho.hp (L[i]);
                B.hin += hin * hin; B.hout += hout * hout;
                if (i0 + i < b0 || i0 + i >= b1) continue;
                B.ek += kl * kl; B.ein += bi * bi; B.eout += bo * bo;
                B.sin += (kl + bi) * (kl + bi); B.sout += (kl + bo) * (kl + bo);
            }
        }
        if (detalhe || nb == total - 1 || nb == batidasDesl - 1 || nb == batidasDesl + 1)
        {
            const double pot = B.ek + B.ein;
            char nn[8];
            std::printf ("batida %2d %-4s %s  entrada %+5.1f dB  saida %+5.1f dB  baixo grave %+5.1f dB  cima %+5.2f dB  opcao %2d%s rot %6.1f  duck %5.1f\n",
                         nb, nomeNota (notas[(size_t) nb % notas.size()], nn), lig ? "ON " : "off",
                         10 * std::log10 ((B.sin + 1e-12) / pot), 10 * std::log10 ((B.sout + 1e-12) / pot),
                         10 * std::log10 ((B.eout + 1e-12) / (B.ein + 1e-12)), 10 * std::log10 ((B.hout + 1e-12) / (B.hin + 1e-12)),
                         e.opcaoAtual() >> 1, (e.opcaoAtual() & 1) ? "i" : " ", e.leitura.rotDeg.load(), e.leitura.duckDb.load());
        }
    }
    std::printf ("desligado: maior diferenca saida x entrada = %.3g (tem de ser 0)   tela: low end %+.1f dB corr %+.2f nota %d xover %.0f Hz\n",
                 maxDifDesl, e.leitura.lowEndDb.load(), e.leitura.corr.load(), e.leitura.nota.load(), e.leitura.xoverHz.load());
    return e.opcaoAtual();
}

// soma no trecho do b (dB em relação à soma de potência) depois de 8 batidas com a opção fixa
static double comOpcao (double offsetGraus, int opcao, bool guard, double* graveDb = nullptr)
{
    Sim s; s.offset = offsetGraus * pi / 180.0; s.notas = { 31 };
    Engine e; e.prepare (s.fs); e.guard = guard; e.forcaOpcao = opcao;
    Lr4 lk, li, lo; for (auto* f : { &lk, &li, &lo }) f->set (s.fs, 100);
    const int porBatida = (int) std::lround (60.0 / s.bpm * s.fs), b0 = porBatida / 4, b1 = 2 * b0;
    double ek = 0, ein = 0, eout = 0, so = 0;
    for (int nb = 0; nb < 10; ++nb)
        for (int i = 0; i < porBatida; ++i)
        {
            double k, b; s.amostra (k, b);
            float L = (float) b, R = (float) b, K = (float) k; float* ch[2] = { &L, &R }; const float* kc[1] = { &K };
            e.process (ch, 2, kc, 1, 1, true);
            const double kl = lk.lp (K), bi = li.lp (b), bo = lo.lp (L);
            if (nb < 8 || i < b0 || i >= b1) continue;
            ek += kl * kl; ein += bi * bi; eout += bo * bo; so += (kl + bo) * (kl + bo);
        }
    if (graveDb) *graveDb = 10 * std::log10 (eout / ein);
    return 10 * std::log10 (so / (ek + ein));
}

int main()
{
    // G1 parado, com o baixo em várias fases em relação à cauda do kick: a opção que o motor aprende contra a melhor
    // de verdade (testando as 34 sem LEVEL GUARD: a soma maior = mais em fase)
    for (double off : { 0.0, 90.0, 155.0, 180.0, 270.0 })
    {
        double melhor = -99; int om = 0;
        for (int o = 0; o < numOpc; ++o) { const double v = comOpcao (off, o, false); if (v > melhor) { melhor = v; om = o; } }
        const int ap = roda ("G1 parado", off, { 31 }, 4, 16, false);
        double gr; const double cg = comOpcao (off, ap, true, &gr);
        std::printf ("  sem guard: melhor opcao %d = %+.1f dB | aprendida %d = %+.1f dB | aprendida COM guard: soma %+.1f dB, grave do baixo %+.1f dB\n",
                     om, melhor, ap, comOpcao (off, ap, false), cg, gr);
    }
    // troca de nota a cada 4 batidas (G1, A1, E1, G1...): a memória por nota tem de valer na volta
    std::vector<int> seq; for (int n : { 31, 33, 28, 31, 33, 28 }) for (int i = 0; i < 4; ++i) seq.push_back (n);
    roda ("G1 A1 E1 trocando", 155.0, seq, 2, 24, true);
    // sem sidechain (kick mudo), ligado: o nível de cada frequência não muda (só a fase do crossover)
    for (double f : { 41.0, 80.0, 150.0, 400.0 })
    {
        Engine e; e.prepare (48000);
        float L[256], R[256]; float* ch[2] = { L, R }; double ei = 0, eo = 0;
        for (int b = 0; b < 400; ++b)
        {
            for (int i = 0; i < 256; ++i) L[i] = R[i] = (float) std::sin (2 * pi * f * (b * 256 + i) / 48000.0) * 0.5f;
            if (b > 40) for (int i = 0; i < 256; ++i) ei += (double) L[i] * L[i];
            e.process (ch, 2, nullptr, 0, 256, true);
            if (b > 40) for (int i = 0; i < 256; ++i) eo += (double) L[i] * L[i];
        }
        std::printf ("sem sidechain, ligado, %3.0f Hz: nivel %+.2f dB (tem de ser 0)  estado %d\n", f, 10 * std::log10 (eo / ei), e.leitura.estado.load());
    }
    return 0;
}
