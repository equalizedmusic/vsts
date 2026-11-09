// Bancada do MAESTRO: mede o Dsp.h fora do plugin com um projeto de psytrance sintético (145 BPM, 11 faixas, cada uma
// com o próprio Engine no mesmo Registry, como no DAW).
//   D:\Apps\granular-vst\toolchain\zig\ziglang\zig.exe c++ -O2 -std=c++17 -target x86_64-windows-gnu teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe
//
// Confere:
//   1. PARADO (sem LEARN/LOCK/FOCUS): saída = entrada, idêntica;
//   2. LEARN 8 s + LOCK: nível de cada ROLE relativo ao KICK, antes e depois, contra o alvo do PSYTRANCE;
//   3. FOCUS no LEAD (toca um compasso sim, um não): o PAD perde quantos dB em 1–2 kHz com o LEAD tocando e quanto
//      com o LEAD calado (tem que voltar a ~0);
//   4. B (A/B): depois da rampa, saída = entrada de novo, idêntica;
//   5. nada de NaN/Inf e o maior salto entre amostras (estalo);
//   6. CPU: 150 faixas, 10 s de áudio, blocos de 256.
#include "../Source/Dsp.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>
using namespace maestro;

namespace
{
constexpr double fs = 48000.0, bpm = 145.0, pi2 = 2.0 * kPi;
constexpr int BL = 256;

struct Gerador
{
    int tipo = 0;
    double ph = 0, ph2 = 0, ph3 = 0, z1 = 0, z2 = 0, lp = 0;
    std::mt19937 rnd { 1 };
    float ruido() { return std::uniform_real_distribution<float> (-1.0f, 1.0f) (rnd); }

    // t em segundos; devolve a amostra (mono)
    float amostra (double t)
    {
        const double beat = t * bpm / 60.0, tb = (beat - std::floor (beat)) * 60.0 / bpm;
        const double six = beat * 4.0, t16 = (six - std::floor (six)) * 15.0 / bpm;
        const int k16 = (int) std::floor (six) % 4, bar = (int) std::floor (beat / 4.0);
        switch (tipo)
        {
            case 0: { // KICK
                if (tb < 1.0 / fs) ph = 0;
                ph += pi2 * (48.0 + 120.0 * std::exp (-tb * 60.0)) / fs;
                return (float) (std::sin (ph) * std::exp (-tb * 9.0) * 0.9);
            }
            case 1: { // BASS rolante (alto demais de propósito)
                if (k16 == 0) return 0.0f;
                ph += 49.0 / fs;
                const double saw = 2.0 * (ph - std::floor (ph)) - 1.0;
                lp += 0.06 * (saw - lp);
                return (float) (lp * std::exp (-t16 * 18.0) * 0.9);
            }
            case 2: { // HAT (contratempo)
                const double tt = std::fmod (beat, 1.0) >= 0.5 ? (std::fmod (beat, 0.5)) * 60.0 / bpm : 1.0;
                const float n = ruido();
                const double h = n - z1; z1 = n;
                return (float) (h * std::exp (-tt * 50.0) * 0.25);
            }
            case 3: { // CLAP (2 e 4)
                const int b = (int) std::floor (beat) % 2;
                if (b != 1) return 0.0f;
                const float n = ruido();
                z1 += 0.3 * (n - z1);
                return (float) ((n - z1) * std::exp (-tb * 25.0) * 0.4);
            }
            case 4: { // LEAD 01: toca um compasso sim, um não (1–3 kHz)
                if (bar % 2 == 1) return 0.0f;
                static const double notas[] = { 740, 880, 988, 880, 659, 740, 880, 1109 };
                const double f = notas[(int) std::floor (six) % 8];
                ph += f / fs;
                double s = 0;
                for (int h = 1; h <= 5; ++h) s += std::sin (pi2 * ph * h) / h;
                return (float) (s * 0.22);
            }
            case 5: { // ACID
                ph += 98.0 / fs;
                const double saw = 2.0 * (ph - std::floor (ph)) - 1.0;
                const double fc = 300.0 + 900.0 * (0.5 + 0.5 * std::sin (pi2 * t * 0.25));
                const double g = std::tan (kPi * fc / fs), k = 0.3;
                const double v1 = (z1 + g * (saw - z2 - k * z1)) / (1 + g * (g + k));
                const double v2 = z2 + g * v1;
                z1 = 2 * v1 - z1; z2 = 2 * v2 - z2;
                return (float) (v2 * (k16 == 0 ? 0.0 : 0.25));
            }
            case 6: { // PAD 02: acorde sustentado com médios de sobra (alto demais de propósito)
                ph += 370.0 / fs; ph2 += 554.0 / fs; ph3 += 740.0 / fs;
                double s = 0;
                for (int h = 1; h <= 6; ++h) s += (std::sin (pi2 * ph * h) + std::sin (pi2 * ph2 * h) + std::sin (pi2 * ph3 * h)) / (h * 1.5);
                return (float) (s * 0.09);
            }
            case 7: { // FX riser (ruído subindo a cada 4 compassos)
                const double u = std::fmod (beat, 16.0) / 16.0;
                const float n = ruido();
                lp += (0.02 + 0.5 * u) * (n - lp);
                return (float) (lp * u * 0.3);
            }
            case 8: { // VOX (vogal)
                ph += 220.0 / fs;
                double s = 0;
                for (int h = 1; h <= 14; ++h)
                {
                    const double f = 220.0 * h;
                    const double env = std::exp (-std::pow ((f - 700) / 250, 2)) + 0.6 * std::exp (-std::pow ((f - 1220) / 300, 2)) + 0.3 * std::exp (-std::pow ((f - 2600) / 400, 2));
                    s += std::sin (pi2 * ph * h) * env;
                }
                return (float) (s * (bar % 4 == 2 ? 0.12 : 0.0));
            }
            case 9: { // ATMOS (ruído escuro)
                const float n = ruido();
                lp += 0.01 * (n - lp);
                z1 += 0.05 * (n - z1);
                return (float) ((lp * 3.0 + (z1 - lp) * 0.6) * 0.15);
            }
            default: { // RIDE
                const double tt = std::fmod (beat, 0.25) * 60.0 / bpm;
                const float n = ruido();
                const double h = n - z1; z1 = n;
                return (float) (h * std::exp (-tt * 20.0) * 0.08);
            }
        }
    }
};

// ponderação K (BS.1770 a 48 kHz) para medir o nível de verdade, fora do motor
struct MedeK
{
    Biquad a, b;
    double soma = 0; long n = 0;
    MedeK()
    {
        a.k = { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
        b.k = { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
    }
    void soma1 (float x) { const float y = b.passa (a.passa (x, 0), 0); soma += (double) y * y; ++n; }
    double db() const { return n > 0 ? 10.0 * std::log10 (soma / n + 1e-20) : -200; }
    void zera() { soma = 0; n = 0; }
};

// nível "enquanto toca" (como o MAESTRO mede): blocos de 1024 em que a ENTRADA passa de -60 dB K
struct MedePar
{
    MedeK ki, ko;
    double bi = 0, bo = 0, si = 0, so = 0; long n = 0, cnt = 0;
    void soma (float in, float out)
    {
        const float yi = ki.b.passa (ki.a.passa (in, 0), 0), yo = ko.b.passa (ko.a.passa (out, 0), 0);
        bi += (double) yi * yi; bo += (double) yo * yo;
        if (++n == 1024) { if (10.0 * std::log10 (bi / 1024 + 1e-20) > -60.0) { si += bi; so += bo; cnt += 1024; } bi = bo = 0; n = 0; }
    }
    double dbIn() const { return cnt > 0 ? 10.0 * std::log10 (si / cnt) : -200; }
    double dbOut() const { return cnt > 0 ? 10.0 * std::log10 (so / cnt) : -200; }
    void zera() { bi = bo = si = so = 0; n = cnt = 0; }
};

struct Faixa
{
    std::string nome;
    int role;
    Gerador g;
    std::unique_ptr<Engine> e;
    Ajustes aj;
    std::vector<float> L, R;
    MedeK medeIn, medeOut;
    MedePar par;
};

// roda todas as faixas de t0 a t1; f(i, tempo, entrada, saida) por amostra
template <typename F>
void roda (std::vector<std::unique_ptr<Faixa>>& fx, double& t, double dur, F&& cada)
{
    const long nb = (long) (dur * fs / BL);
    for (long k = 0; k < nb; ++k)
    {
        for (size_t i = 0; i < fx.size(); ++i)
        {
            auto& f = *fx[i];
            f.L.resize (BL); f.R.resize (BL);
            std::vector<float> in (BL);
            for (int j = 0; j < BL; ++j) { in[(size_t) j] = f.g.amostra (t + j / fs); f.L[(size_t) j] = f.R[(size_t) j] = in[(size_t) j]; }
            float* ch[2] = { f.L.data(), f.R.data() };
            f.e->process (ch, 2, BL, f.aj);
            for (int j = 0; j < BL; ++j) cada (i, t + j / fs, in[(size_t) j], f.L[(size_t) j], f.R[(size_t) j]);
        }
        t += BL / fs;
    }
}
} // namespace

int main()
{
    auto& R = Registry::get();
    const char* nomes[] = { "Kick", "Bass", "Hat", "Clap", "Lead 01", "Acid", "Pad 02", "FX Riser", "Vox", "Atmos", "Ride" };
    const int roles[] = { KICK, BASS, PERC, PERC, LEAD, ACID, PAD, FX, VOX, ATMOS, PERC };
    std::vector<std::unique_ptr<Faixa>> fx;
    for (int i = 0; i < 11; ++i)
    {
        auto f = std::make_unique<Faixa>();
        f->nome = nomes[i]; f->role = roles[i];
        f->g.tipo = i == 10 ? 10 : i;
        f->g.rnd.seed ((unsigned) (i + 3));
        f->e = std::make_unique<Engine>();
        f->e->prepare (fs);
        f->e->defineNome (nomes[i]);
        fx.push_back (std::move (f));
    }
    double t = 0;
    int falhas = 0;

    // 1. parado: idêntico
    {
        double maxd = 0;
        roda (fx, t, 3.0, [&] (size_t, double, float in, float l, float r) { maxd = std::max ({ maxd, (double) std::abs (l - in), (double) std::abs (r - in) }); });
        std::printf ("1. PARADO: maior diferenca entrada/saida = %.3g %s\n", maxd, maxd == 0 ? "(identica) OK" : "FALHOU");
        if (maxd != 0) ++falhas;
        int certos = 0;
        for (auto& f : fx) certos += f->e->slot().role.load() == f->role;
        std::printf ("   ROLE pelo nome: %d/11 certos;", certos);
        for (auto& f : fx) std::printf (" %s=%s", f->nome.c_str(), nomeRole (f->e->slot().role.load()));
        std::printf ("\n");
        // AUTO pelo espectro (sem nome)
        Engine sozinho; sozinho.prepare (fs);
        (void) sozinho;
    }

    // 2. LEARN 8 s + LOCK
    R.amount = 1.0f;
    R.topo = 0;   // o nível do LOCK medido sem o KEEP KICK / BASS ON TOP (que é dinâmico)
    R.learn (0.0);
    for (auto& f : fx) f->medeIn.zera();
    const double ciclo = 16 * 60.0 / bpm * 2;   // 2 ciclos do riser (16 tempos): a janela não pega o riser pela metade
    roda (fx, t, ciclo, [&] (size_t i, double, float in, float, float) { fx[i]->medeIn.soma1 (in); });
    R.lock();
    roda (fx, t, 1.0, [] (size_t, double, float, float, float) {});   // rampa
    for (auto& f : fx) f->par.zera();
    roda (fx, t, ciclo, [&] (size_t i, double, float in, float l, float) { fx[i]->par.soma (in, l); });
    {
        double pin[NR] {}, pout[NR] {};
        for (auto& f : fx) { pin[f->role] += std::pow (10.0, f->par.dbIn() / 10); pout[f->role] += std::pow (10.0, f->par.dbOut() / 10); }
        const double kIn = 10 * std::log10 (pin[KICK]), kOut = 10 * std::log10 (pout[KICK]);
        std::printf ("2. LOCK (PSYTRANCE, AMOUNT 100%%): nivel do ROLE relativo ao KICK\n   ROLE    alvo   antes  depois  erro\n");
        double piorAntes = 0, piorDepois = 0;
        for (int r = 0; r < NR; ++r)
        {
            if (pin[r] <= 0) continue;
            const double a = 10 * std::log10 (pin[r]) - kIn, d = 10 * std::log10 (pout[r]) - kOut, al = alvoNivel (0, r);
            std::printf ("   %-6s %6.1f %6.1f %6.1f %+5.1f\n", nomeRole (r), al, a, d, d - al);
            piorAntes = std::max (piorAntes, std::abs (a - al));
            piorDepois = std::max (piorDepois, std::abs (d - al));
        }
        std::printf ("   pior erro: antes %.1f dB, depois %.1f dB %s\n", piorAntes, piorDepois, piorDepois < 1.0 ? "OK" : "FALHOU");
        if (piorDepois >= 1.0) ++falhas;
        const double kApr = fx[0]->e->slot().aprNivelDb.load(), kMed = fx[0]->par.dbIn();
        for (auto& f : fx)
        {
            std::printf ("   %-9s medido pelo MAESTRO %+5.1f x bancada %+5.1f (rel. KICK) |", f->nome.c_str(),
                         f->e->slot().aprNivelDb.load() - kApr, f->par.dbIn() - kMed);
            std::printf (" saida-entrada %+5.1f LEVEL %+5.1f  TONE", f->par.dbOut() - f->par.dbIn(), f->e->travado.ganhoBruto);
            for (int k = 0; k < NE; ++k) std::printf (" %+4.1f", f->e->travado.tomBruto[k]);
            std::printf ("\n");
        }
    }

    R.topo = 1;
    // 3. FOCUS no LEAD: o PAD em 1–2 kHz com e sem o LEAD tocando
    {
        auto& lead = *fx[4];
        auto& pad = *fx[6];
        auto medeBanda = [&] (bool foco)
        {
            lead.aj.focus = foco;
            roda (fx, t, 1.0, [] (size_t, double, float, float, float) {});
            Biquad bi, bo; bi.k = sino (fs, 1400.0, 2.0, 0); // placeholder
            // passa-faixa 1–2 kHz (RBJ BPF, ganho 0 dB no pico)
            const double w = 2 * kPi * 1414.0 / fs, al = std::sin (w) / (2 * 1.41), c = std::cos (w), a0 = 1 + al;
            Coef k; k.b0 = al / a0; k.b1 = 0; k.b2 = -al / a0; k.a1 = -2 * c / a0; k.a2 = (1 - al) / a0;
            bi.k = k; bo.k = k;
            double eiOn = 0, eoOn = 0, eiOff = 0, eoOff = 0;
            double cOn = 0, cOff = 0; long nOn = 0, nOff = 0;
            roda (fx, t, 8.0, [&] (size_t i, double tt, float in, float l, float)
            {
                if (i != 6) return;
                {
                    const int bar = (int) std::floor (tt * bpm / 60.0 / 4.0);
                    const float c5 = pad.e->slot().corte[5].load();
                    if (bar % 2 == 0) { cOn += c5; ++nOn; } else { cOff += c5; ++nOff; }
                }
                const float yi = bi.passa (in, 0), yo = bo.passa (l, 0);
                const int bar = (int) std::floor (tt * bpm / 60.0 / 4.0);
                const double dentro = std::fmod (tt * bpm / 60.0, 4.0);
                if (dentro < 0.5 || dentro > 3.5) return;   // longe da troca de compasso
                if (bar % 2 == 0) { eiOn += yi * yi; eoOn += yo * yo; } else { eiOff += yi * yi; eoOff += yo * yo; }
            });
            std::printf ("   [FOCUS %s] corte medio do PAD em 1,6 kHz: LEAD tocando %+.1f dB, LEAD calado %+.1f dB\n", foco ? "on" : "off",
                         cOn / std::max (1L, nOn), cOff / std::max (1L, nOff));
            return std::make_pair (10 * std::log10 (eoOn / eiOn), 10 * std::log10 (eoOff / eiOff));
        };
        const auto sem = medeBanda (false);
        const auto com = medeBanda (true);
        const double dOn = com.first - sem.first, dOff = com.second - sem.second;
        std::printf ("3. FOCUS no LEAD (MAX CUT %.0f dB, SPEED MED): PAD 02 em 1-2 kHz\n", R.maxCut.load());
        std::printf ("   com o LEAD tocando: %+.1f dB   com o LEAD calado: %+.1f dB  %s\n", dOn, dOff,
                     (dOn < -2.0 && dOff > -1.0) ? "OK" : "FALHOU");
        if (! (dOn < -2.0 && dOff > -1.0)) ++falhas;
        std::printf ("   PAD cede para: %s; cortes agora:", pad.e->slot().cedePara.load() == lead.e->indice() ? "LEAD 01" : "?");
        for (int k = 0; k < NE; ++k) std::printf (" %+.1f", pad.e->slot().corte[k].load());
        std::printf ("\n   LEAD (em FOCUS) cortes:");
        for (int k = 0; k < NE; ++k) std::printf (" %+.1f", lead.e->slot().corte[k].load());
        std::printf ("\n");
    }

    // 4. B: volta a ser idêntico
    {
        R.modoB = 1;
        roda (fx, t, 1.0, [] (size_t, double, float, float, float) {});
        double maxd = 0, salto = 0;
        bool nan = false;
        size_t qual = 0; float vin = 0, vout = 0;
        roda (fx, t, 3.0, [&] (size_t i, double, float in, float l, float r)
        {
            const double d = std::max (std::abs (l - in), std::abs (r - in));
            if (d > maxd) { maxd = d; qual = i; vin = in; vout = l; }
            if (! std::isfinite (l) || ! std::isfinite (r)) nan = true;
        });
        std::printf ("4. B (A/B): maior diferenca = %.3g %s\n", maxd, maxd == 0 ? "(identica) OK" : "FALHOU");
        if (maxd != 0) std::printf ("   faixa %s: entrada %.3g saida %.3g\n", fx[qual]->nome.c_str(), vin, vout);
        if (maxd != 0) ++falhas;
        R.modoB = 0;
        // 5. estalo na volta para A (com FOCUS ligado): maior salto entre amostras, entrada x saída, no PAD
        double saltoIn = 0, ant = 0, antIn = 0;
        roda (fx, t, 2.0, [&] (size_t i, double, float in, float l, float)
        {
            if (! std::isfinite (l)) nan = true;
            if (i != 6) return;
            salto = std::max (salto, (double) std::abs (l - ant)); ant = l;
            saltoIn = std::max (saltoIn, (double) std::abs (in - antIn)); antIn = in;
        });
        std::printf ("5. NaN/Inf: %s; maior salto no PAD voltando para A: saida %.3f x entrada %.3f %s\n", nan ? "SIM - FALHOU" : "nenhum",
                     salto, saltoIn, salto <= saltoIn * 1.2 ? "OK" : "VERIFICAR");
        if (nan) ++falhas;
    }

    // 6. CPU: 150 faixas
    {
        R.reset();
        std::vector<std::unique_ptr<Faixa>> muitas;
        for (int i = 0; i < 150; ++i)
        {
            auto f = std::make_unique<Faixa>();
            f->g.tipo = i % 11;
            f->g.rnd.seed ((unsigned) i);
            f->e = std::make_unique<Engine>();
            f->e->prepare (fs);
            f->nome = std::string (nomes[i % 11]) + " " + std::to_string (i);
            f->e->defineNome (f->nome.c_str());
            muitas.push_back (std::move (f));
        }
        double tt = 0;
        // só a análise (parado: nenhuma EQ ligada)
        {
            std::vector<float> L (BL), Rr (BL), pre (BL);
            for (int j = 0; j < BL; ++j) pre[(size_t) j] = (float) std::sin (0.07 * j) * 0.2f;
            const auto ini = std::chrono::steady_clock::now();
            for (long k = 0; k < (long) (5.0 * fs / BL); ++k)
                for (auto& f : muitas)
                {
                    std::copy (pre.begin(), pre.end(), L.begin()); std::copy (pre.begin(), pre.end(), Rr.begin());
                    float* ch[2] = { L.data(), Rr.data() };
                    f->e->process (ch, 2, BL, f->aj);
                }
            const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - ini).count();
            std::printf ("6. CPU so da analise (parado): %.2f%% de UM nucleo por faixa\n", 100.0 * seg / 5.0 / 150.0);
        }
        R.learn (0.0);
        roda (muitas, tt, 2.0, [] (size_t, double, float, float, float) {});
        R.lock();
        muitas[4]->aj.focus = true;   // um LEAD em FOCUS: as outras 149 cedem
        // gera a entrada antes (o gerador não entra na conta)
        const double dur = 10.0;
        const auto ini = std::chrono::steady_clock::now();
        const long nb = (long) (dur * fs / BL);
        std::vector<float> L (BL), Rr (BL), pre (BL);
        for (int j = 0; j < BL; ++j) pre[(size_t) j] = (float) std::sin (0.07 * j) * 0.2f;
        for (long k = 0; k < nb; ++k)
            for (auto& f : muitas)
            {
                std::copy (pre.begin(), pre.end(), L.begin());
                std::copy (pre.begin(), pre.end(), Rr.begin());
                float* ch[2] = { L.data(), Rr.data() };
                f->e->process (ch, 2, BL, f->aj);
            }
        const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - ini).count();
        std::printf ("6. CPU: 150 faixas x %.0f s de audio = %.2f s de processamento (%.1f%% de UM nucleo; %.2f%% por faixa)\n",
                     dur, seg, 100.0 * seg / dur, 100.0 * seg / dur / 150.0);
        int ligadas = R.ligadas();
        std::printf ("   faixas no Registry: %d\n", ligadas);
    }

    std::printf ("\n%s (%d falha%s)\n", falhas == 0 ? "TUDO OK" : "COM FALHAS", falhas, falhas == 1 ? "" : "s");
    return falhas;
}
