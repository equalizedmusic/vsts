// Bancada do GOGÓ (sem JUCE): voz sintética com verso baixo, refrão alto, "s" e "p", passada pela cadeia inteira.
// Compilar (zig):  zig c++ -O2 -std=c++17 -I ../Source teste_dsp.cpp -o teste_dsp.exe
// Confere: tempo de CPU, nada de NaN, nível do verso x refrão depois do RIDE/OPTO, GR de cada bloco, "pops", de-ess
// no "s", bypass = entrada atrasada pela latência, tudo desligado = transparente, e estalo ao ligar/desligar bloco.
#include "Dsp.h"
#include <chrono>
#include <cstdio>
#include <random>

namespace
{
constexpr double fs = 48000.0;
constexpr int bloco = 256;

struct Voz
{
    std::mt19937 rnd { 7 };
    double ph = 0.0, hpS = 0.0, hpSx = 0.0, lpP = 0.0;
    // 0,42 s por nota (como a foto do ONKEY); ganho: verso (−14 dB) até 12 s, refrão depois
    double amostra (double t)
    {
        static const int mel[] = { 57, 60, 64, 62, 60, 59, 57, 64, 65, 64, 62, 60 };
        static const double desv[] = { 0.30, -0.25, 0.18, -0.35, 0.22, -0.40, 0.15, -0.28, 0.33, -0.20, 0.38, -0.30 };
        const double passo = 0.42;
        if (t >= 22.0) return 0.0;   // pausa no fim: cauda do SPACE
        const int k = (int) std::floor (t / passo);
        const double u = t - k * passo;
        const double nivel = t < 12.0 ? 0.2 : 1.0;
        std::uniform_real_distribution<double> d (-1.0, 1.0);
        double v = 0.0;
        if (k % 8 == 7)        // "s": ruído de agudo
        {
            const double n = d (rnd);
            hpS = 0.6 * (hpS + n - hpSx); hpSx = n;
            if (u < 0.18) v = hpS * 0.35 * std::sin (u / 0.18 * kPiD());
        }
        else if (k % 8 == 3 && u < 0.04)   // "p": tranco de grave (70 Hz + ruído abaixo de ~150 Hz)
        {
            lpP += (d (rnd) - lpP) * 0.02;
            v = 0.8 * (std::sin (2 * kPiD() * 70.0 * u) + 3.0 * lpP) * (1.0 - u / 0.04);
        }
        else if (u < passo - 0.03)
        {
            const double vib = k % 4 == 2 ? 0.3 * std::sin (2 * kPiD() * 5.5 * u) * std::min (1.0, u / 0.2) : 0.0;
            const double f0 = 440.0 * std::pow (2.0, (mel[k % 12] + desv[k % 12] + vib - 69.0) / 12.0);
            ph += f0 / fs; ph -= std::floor (ph);
            auto pico = [] (double f, double fc, double bw) { const double x = (f - fc) / bw; return 1.0 / (1.0 + x * x); };
            for (int h = 1; h * f0 < 9000.0; ++h)
            {
                const double fh = h * f0;
                v += (0.05 + pico (fh, 700, 110) + 0.7 * pico (fh, 1220, 120) + 0.35 * pico (fh, 2600, 180) + 0.2 * pico (fh, 320, 90)) / h
                     * std::sin (2 * kPiD() * h * ph);
            }
            v *= 0.22 * std::min (1.0, u / 0.02) * std::min (1.0, (passo - 0.03 - u) / 0.03);
        }
        return v * nivel;
    }
    static constexpr double kPiD() { return 3.14159265358979323846; }
};

double rmsDb (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; size_t n = 0;
    for (size_t i = a; i < b && i < v.size(); ++i) { s += (double) v[i] * v[i]; ++n; }
    return 10.0 * std::log10 (s / std::max<size_t> (1, n) + 1e-20);
}

struct Saida { std::vector<float> l, r; double cpu = 0; };

Saida roda (gogo::Cadeia& cad, gogo::Ajustes a, const std::vector<float>& in, int alternaBloco = -1, double tAlterna = 0)
{
    Saida s; s.l.resize (in.size()); s.r.resize (in.size());
    std::vector<float> L (bloco), R (bloco);
    auto t0 = std::chrono::steady_clock::now();
    for (size_t p = 0; p < in.size(); p += bloco)
    {
        const int n = (int) std::min<size_t> (bloco, in.size() - p);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = in[p + (size_t) i];
        if (alternaBloco >= 0 && (double) p / fs >= tAlterna) a.on[alternaBloco] = ! a.on[alternaBloco], alternaBloco = -1;
        float* ch[2] = { L.data(), R.data() };
        cad.process (ch, 2, n, a);
        for (int i = 0; i < n; ++i) { s.l[p + (size_t) i] = L[(size_t) i]; s.r[p + (size_t) i] = R[(size_t) i]; }
    }
    s.cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return s;
}
} // namespace

int main()
{
    const double dur = 24.0;
    std::vector<float> in ((size_t) (dur * fs));
    Voz voz;
    for (size_t i = 0; i < in.size(); ++i) in[i] = (float) voz.amostra (i / fs);
    std::printf ("entrada: verso %.1f dB RMS, refrao %.1f dB RMS\n", rmsDb (in, (size_t) (2 * fs), (size_t) (11 * fs)), rmsDb (in, (size_t) (13 * fs), (size_t) (21.5 * fs)));

    int erros = 0;
    auto confere = [&] (bool ok, const char* o) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALHOU", o); if (! ok) ++erros; };

    // 1. cadeia inteira (padrão PROG TRANCE)
    auto cad = std::make_unique<gogo::Cadeia>();
    cad->prepare (fs);
    gogo::Ajustes a;
    a.inputDb = 8.0f;   // como o LEARN deixaria (média perto de −18 dBFS)
    auto s = roda (*cad, a, in);
    const int lat = cad->latencia (false);
    bool nan = false; double pk = 0;
    for (size_t i = 0; i < s.l.size(); ++i) { if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) nan = true; pk = std::max (pk, (double) std::abs (s.l[i])); }
    const double vs = rmsDb (s.l, (size_t) (4 * fs), (size_t) (11 * fs)), rf = rmsDb (s.l, (size_t) (15 * fs), (size_t) (21.5 * fs));
    std::printf ("\n1. cadeia inteira: CPU %.2f%% de um nucleo, latencia %d amostras (%.1f ms)\n", 100.0 * s.cpu / dur, lat, 1000.0 * lat / fs);
    std::printf ("   saida: verso %.1f dB, refrao %.1f dB (diferenca %.1f dB; na entrada era 14 dB), pico %.2f\n", vs, rf, rf - vs, pk);
    std::printf ("   medidas: HPF %.0f Hz (grave %.0f Hz), pops %d, ride %.1f dB, FET %.1f, OPTO %.1f (release %.0f ms), makeup %.1f\n",
                 cad->med.hpfHz.load(), cad->med.graveHz.load(), cad->med.pops.load(), cad->med.rideDb.load(), cad->med.fetGr.load(),
                 cad->med.optoGr.load(), cad->med.optoRelMs.load(), cad->med.makeupDb.load());
    auto& L = cad->tune().leitura;
    std::printf ("   TUNE: tom %s %s (estado %d)\n", L.tomDet.load() >= 0 ? onkey::nomePc (L.tomDet.load()) : "-", L.modoDet.load() ? "menor" : "maior", L.estado.load());
    confere (! nan, "sem NaN/inf");
    confere (pk <= 1.0, "pico <= 1.0");
    confere (rf - vs < 7.0, "verso e refrao ficaram mais perto (RIDE + OPTO)");
    confere (L.estado.load() == 2, "TUNE travou o tom");

    // 1b. "p" com o HPF em 80 Hz (voz grave): o CLEAN tem que achar e cortar
    {
        auto c = std::make_unique<gogo::Cadeia>(); c->prepare (fs);
        gogo::Ajustes b = a; b.hpfAuto = false; b.hpfHz = 80.0f;
        roda (*c, b, in);
        std::printf ("\n1b. HPF 80 Hz: pops achados %d (a voz tem %d)\n", c->med.pops.load(), (int) (22.0 / 0.42 / 8.0));
        confere (c->med.pops.load() >= 4, "CLEAN pegou os 'p'");
    }

    // 2. DE-ESS no "s": nível do agudo do "s" com e sem DE-ESS
    {
        auto c1 = std::make_unique<gogo::Cadeia>(); c1->prepare (fs);
        auto c2 = std::make_unique<gogo::Cadeia>(); c2->prepare (fs);
        gogo::Ajustes b = a; b.on[gogo::bSpace] = false;
        gogo::Ajustes b0 = b; b0.on[gogo::bDeess] = false;
        auto s1 = roda (*c1, b, in), s0 = roda (*c2, b0, in);
        // "s" do refrão: nota 7 de cada 8 (0,42 s) → k = 31 → t = 13,02 s; mede 13,02..13,18 + latência
        const size_t a0 = (size_t) (13.03 * fs) + (size_t) lat, a1 = (size_t) (13.17 * fs) + (size_t) lat;
        std::printf ("\n2. DE-ESS: 's' com %.1f dB, sem %.1f dB\n", rmsDb (s1.l, a0, a1), rmsDb (s0.l, a0, a1));
        confere (rmsDb (s0.l, a0, a1) - rmsDb (s1.l, a0, a1) > 2.0, "DE-ESS baixou o 's' mais de 2 dB");
        const size_t v0 = (size_t) (13.5 * fs) + (size_t) lat, v1 = (size_t) (13.8 * fs) + (size_t) lat;
        confere (std::abs (rmsDb (s0.l, v0, v1) - rmsDb (s1.l, v0, v1)) < 1.0, "vogal quase igual com e sem DE-ESS (< 1 dB)");
    }

    // 3. BYPASS = entrada atrasada pela latência
    {
        auto c = std::make_unique<gogo::Cadeia>(); c->prepare (fs);
        gogo::Ajustes b = a; b.bypass = true;
        auto sb = roda (*c, b, in);
        double dmax = 0;
        for (size_t i = (size_t) fs; i < in.size(); ++i) dmax = std::max (dmax, (double) std::abs (sb.l[i] - in[i - (size_t) lat]));
        std::printf ("\n3. BYPASS: maior diferenca para a entrada atrasada = %.2e\n", dmax);
        confere (dmax < 1e-6, "BYPASS igual a entrada (atrasada)");
    }

    // 4. tudo desligado = transparente (só a latência)
    {
        auto c = std::make_unique<gogo::Cadeia>(); c->prepare (fs);
        gogo::Ajustes b = a;
        for (auto& o : b.on) o = false;
        b.inputDb = 0.0f;   // e a voz pela metade: o teto suave (acima de -0,9 dBFS) fica de fora
        std::vector<float> meia (in); for (auto& v : meia) v *= 0.5f;
        auto sb = roda (*c, b, meia);
        double dmax = 0;
        for (size_t i = (size_t) fs; i < in.size(); ++i) dmax = std::max (dmax, (double) std::abs (sb.l[i] - meia[i - (size_t) lat]));
        std::printf ("\n4. todos os blocos desligados: maior diferenca = %.2e\n", dmax);
        confere (dmax < 1e-4, "transparente com tudo desligado");
    }

    // 5. TRACKING: latência 0
    {
        auto c = std::make_unique<gogo::Cadeia>(); c->prepare (fs);
        gogo::Ajustes b = a; b.tracking = true;
        for (auto& o : b.on) o = false;
        b.inputDb = 0.0f;   // e a voz pela metade: o teto suave (acima de -0,9 dBFS) fica de fora
        std::vector<float> meia (in); for (auto& v : meia) v *= 0.5f;
        auto sb = roda (*c, b, meia);
        double dmax = 0;
        for (size_t i = (size_t) fs; i < in.size(); ++i) dmax = std::max (dmax, (double) std::abs (sb.l[i] - meia[i]));
        std::printf ("\n5. TRACKING (tudo desligado): diferenca sem atraso = %.2e, latencia %d\n", dmax, c->latencia (true));
        confere (dmax < 1e-4 && c->latencia (true) == 0, "TRACKING sem latencia");
    }

    // 6. estalo ao ligar/desligar cada bloco no meio do refrão: maior salto entre amostras perto da troca x referência
    {
        std::printf ("\n6. liga/desliga no meio da frase (maior salto entre amostras, 50 ms em volta):\n");
        auto c0 = std::make_unique<gogo::Cadeia>(); c0->prepare (fs);
        auto ref = roda (*c0, a, in);
        const double tA = 16.1;
        for (int bl = 0; bl < gogo::numBlocos; ++bl)
        {
            auto c = std::make_unique<gogo::Cadeia>(); c->prepare (fs);
            auto sx = roda (*c, a, in, bl, tA);
            const size_t i0 = (size_t) ((tA - 0.025) * fs) + (size_t) lat, i1 = (size_t) ((tA + 0.05) * fs) + (size_t) lat;
            double salto = 0, saltoRef = 0;
            for (size_t i = i0; i < i1; ++i)
            {
                salto = std::max (salto, (double) std::abs (sx.l[i] - sx.l[i - 1]));
                saltoRef = std::max (saltoRef, (double) std::abs (ref.l[i] - ref.l[i - 1]));
            }
            std::printf ("   %-7s salto %.3f (ligado direto %.3f)\n", gogo::nomeBloco (bl), salto, saltoRef);
            if (salto > 2.5 * saltoRef + 0.02) { std::printf ("   [FALHOU] estalo no %s\n", gogo::nomeBloco (bl)); ++erros; }
        }
    }

    // 7. SPACE: nível do efeito depois que a voz para (cauda) e durante a voz (DUCK)
    {
        auto c1 = std::make_unique<gogo::Cadeia>(); c1->prepare (fs);
        auto c2 = std::make_unique<gogo::Cadeia>(); c2->prepare (fs);
        gogo::Ajustes b0 = a; b0.on[gogo::bSpace] = false;
        auto sw = roda (*c1, a, in), sd = roda (*c2, b0, in);
        std::vector<float> wet (in.size());
        for (size_t i = 0; i < in.size(); ++i) wet[i] = sw.l[i] - sd.l[i];
        std::printf ("\n7. SPACE: efeito %.1f dB durante o refrao (voz %.1f dB); cauda logo depois da voz %.1f dB, 1 s depois %.1f dB\n",
                     rmsDb (wet, (size_t) (15 * fs), (size_t) (21.5 * fs)), rmsDb (sd.l, (size_t) (15 * fs), (size_t) (21.5 * fs)),
                     rmsDb (wet, (size_t) (22.1 * fs), (size_t) (22.6 * fs)), rmsDb (wet, (size_t) (23.0 * fs), (size_t) (23.5 * fs)));
        double pkw = 0; bool nw = false;
        for (float v : wet) { pkw = std::max (pkw, (double) std::abs (v)); if (! std::isfinite (v)) nw = true; }
        confere (! nw && pkw < 1.0, "SPACE estavel");
    }

    // 8. LEARN
    {
        auto c = std::make_unique<gogo::Cadeia>(); c->prepare (fs);
        c->iniciaLearn();
        std::vector<float> baixo (in.size());
        for (size_t i = 0; i < in.size(); ++i) baixo[i] = in[i] * 0.1f;
        roda (*c, a, baixo);
        c->paraLearn();
        std::printf ("\n8. LEARN numa voz 20 dB mais baixa: INPUT = %+.1f dB (%.1f s de voz)\n", c->med.learnInput.load(), c->med.learnSeg.load());
        confere (c->med.learnFeito.load() == 1 && c->med.learnInput.load() > 10.0f, "LEARN subiu o INPUT");
    }

    std::printf ("\n%s (%d falha%s)\n", erros ? "COM FALHAS" : "TUDO OK", erros, erros == 1 ? "" : "s");
    return erros ? 1 : 0;
}
