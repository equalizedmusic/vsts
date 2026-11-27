// 0.3.5 — TIMBRE PRESERVADO (relato de cliente no Cubase, 27/11/2026): ligar o ALIGN com tudo desligado mudava a fase e
// o timbre; com o baixo já em fase o PHASE deixava o baixo "mais seco"; o POLARITY "afinava" o baixo.
// Compara o modo antigo (preservaTimbre = false, o do BAREBASS) com o novo no mesmo rolling do teste_dsp.cpp.
// clang++ -O2 -std=c++17 teste_timbre.cpp -o teste_timbre.exe && teste_timbre.exe
#include "../Source/Dsp.h"
#include <cstdio>
#include <vector>
using namespace align;

struct Sim
{
    double fs = 48000, bpm = 145, offset = 0, kickPh = 0, t = 0;
    int nota = 31;
    Svf filtro;
    static double hz (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }
    void amostra (double& k, double& b)
    {
        const double beat = 60.0 / bpm, s16 = beat / 4;
        const int nb = (int) std::floor (t / beat);
        const double tb = t - nb * beat, f1 = hz (nota);
        const double fk = 49.0 + (140.0 - 49.0) * std::exp (-tb * 90.0);
        kickPh += 2 * pi * fk / fs;
        k = std::sin (kickPh) * std::min (1.0, tb / 0.001) * std::exp (-tb * 13.0) * 0.9;
        if (tb < 1.0 / fs) kickPh = 0;
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

struct Cfg { bool novo, fase, pol, guard; };
static double medidor = 0;
struct Res { double entrada, saida, banda, difMax, difInvMax, grave; int opcao; };

// 16 batidas; mede as 6 últimas. banda = energia 60–200 Hz do baixo fora do kick (timbre do baixo sozinho), saída/entrada
static Res roda (double offGraus, Cfg c)
{
    Sim s; s.offset = offGraus * pi / 180.0;
    Engine e; e.preservaTimbre = c.novo; e.usaFase = c.fase; e.usaPolaridade = c.pol; e.guard = c.guard; e.prepare (s.fs);
    Lr4 lk, li, lo; for (auto* f : { &lk, &li, &lo }) f->set (s.fs, 100);
    Lr4 bhI, blI, bhO, blO; bhI.set (s.fs, 60); bhO.set (s.fs, 60); blI.set (s.fs, 200); blO.set (s.fs, 200);
    const int porBatida = (int) std::lround (60.0 / s.bpm * s.fs), b0 = porBatida / 4, b1 = 2 * b0;
    double ek = 0, ein = 0, eout = 0, si = 0, so = 0, bi2 = 0, bo2 = 0, dif = 0, difInv = 0;
    for (int nb = 0; nb < 16; ++nb)
        for (int i0 = 0; i0 < porBatida; i0 += 256)
        {
            const int n = std::min (256, porBatida - i0);
            float L[256], R[256], K[256], in[256];
            for (int i = 0; i < n; ++i) { double k, b; s.amostra (k, b); K[i] = (float) k; L[i] = R[i] = in[i] = (float) b; }
            float* ch[2] = { L, R }; const float* kc[1] = { K };
            e.process (ch, 2, kc, 1, n, true);
            for (int i = 0; i < n; ++i)
            {
                const double kl = lk.lp (K[i]), bi = li.lp (in[i]), bo = lo.lp (L[i]);
                const double vi = blI.lp (bhI.hp (in[i])), vo = blO.lp (bhO.hp (L[i]));
                if (nb < 10) continue;
                dif = std::max (dif, (double) std::fabs (L[i] - in[i]));
                difInv = std::max (difInv, (double) std::fabs (L[i] + in[i]));
                const int p = i0 + i;
                if (p >= 2 * b0) { bi2 += vi * vi; bo2 += vo * vo; }   // 3ª e 4ª semicolcheia: kick já sumiu
                if (p < b0 || p >= b1) continue;
                ek += kl * kl; ein += bi * bi; eout += bo * bo; si += (kl + bi) * (kl + bi); so += (kl + bo) * (kl + bo);
            }
        }
    medidor = e.leitura.lowEndDb.load();
    return { 10 * std::log10 (si / (ek + ein)), 10 * std::log10 (so / (ek + ein)), 10 * std::log10 (bo2 / bi2), dif, difInv,
             10 * std::log10 (eout / ein), e.opcaoAtual() };
}

static const char* nomeOp (int o, char* b)
{
    if ((o >> 1) == candSeco) std::snprintf (b, 16, "SECO%s", (o & 1) ? " inv" : "");
    else std::snprintf (b, 16, "rot%2d%s", o >> 1, (o & 1) ? " inv" : "");
    return b;
}

int main()
{
    struct { const char* nome; bool fase, pol, guard; } doses[] = {
        { "tudo desligado     ", false, false, false }, { "so PHASE           ", true, false, false },
        { "so POLARITY        ", false, true, false },  { "so LEVEL GUARD     ", false, false, true },
        { "PHASE+POLARITY     ", true, true, false },   { "tudo ligado        ", true, true, true } };
    for (double off : { 0.0, 90.0, 155.0, 180.0, 270.0 })
    {
        std::printf ("\n== baixo com fase %+.0f graus\n", off);
        for (auto& d : doses)
            for (bool novo : { false, true })
            {
                const Res r = roda (off, { novo, d.fase, d.pol, d.guard });
                char b[16];
                std::printf ("%s %s  soma: entrada %+5.1f -> saida %+5.1f dB | grave baixo %+5.1f dB | timbre 60-200 Hz sem kick %+5.2f dB | |out-in| %.3g |out+in| %.3g | LOW END na tela %+5.1f | %s\n",
                             d.nome, novo ? "NOVO " : "antes", r.entrada, r.saida, r.grave, r.banda, r.difMax, r.difInvMax, medidor, nomeOp (r.opcao, b));
            }
    }
    return 0;
}
