// Mede a DUPLA do BAREBASS (Dupla.h: motor do ALIGN + DUCK com o kick do LINK) fora do plugin.
//   zig c++ -O2 -std=c++17 -target x86_64-windows-gnu teste_dupla.cpp -o teste_dupla.exe && teste_dupla.exe
// Confere: sem kick a saída é idêntica ao synth; KICK LOCK desligado + DUCK 0 também; com o KICK LOCK o grave
// kick + baixo fica perto da soma de potência (nem cancela nem embola) em qualquer fase de partida e nunca pior que
// sem ele; o DUCK abaixa na dose (50 % ≈ −6 dB no pico do kick) e volta entre as batidas; sem NaN e CPU baixa.
#include "../Source/Dsp.h"
#include "../Source/Dupla.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

using namespace barebass;

constexpr double fs = 48000.0;
static int falhas = 0;
static void confere (bool ok, const char* o) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALHOU", o); if (! ok) ++falhas; }
static double db (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }

// corpo de kick no estilo do CHUTE: 110 Hz caindo para fK em 40 ms, amplitude caindo em ~250 ms, a cada batida
static std::vector<float> kick (int n, double fK, double bpm)
{
    std::vector<float> k ((size_t) n, 0.0f);
    const int passo = (int) (fs * 60.0 / bpm);
    for (int b = 0; b < n; b += passo)
    {
        double ph = 0;
        for (int i = 0; i < (int) (0.6 * fs) && b + i < n; ++i)
        {
            const double t = i / fs;
            const double f = fK + (110.0 - fK) * std::exp (-t / 0.012);
            ph += f / fs;
            k[(size_t) (b + i)] = (float) (0.8 * std::exp (-t / 0.25) * std::sin (6.283185307179586 * ph));
        }
    }
    return k;
}

// baixo do BAREBASS: uma nota segura de F1 que começa em t0 (patch macio: SAW com cutoff baixo)
static std::vector<float> baixo (int n, int t0)
{
    auto e = std::make_unique<Engine>();
    e->prepare (fs);
    Ajustes a;
    a.cutoff = 160.0f; a.ligaSub = true; a.nivelSub = 0.8f; a.volumeDb = -3.0f;
    std::vector<float> s ((size_t) n, 0.0f);
    float* ch[1] = { s.data() };
    for (int b = 0; b < n; b += 256)
    {
        const int fim = std::min (n, b + 256);
        if (t0 >= b && t0 < fim)
        {
            if (t0 > b) e->render (ch, 1, b, t0 - b, a);
            e->noteOn (29, a);
            e->render (ch, 1, t0, fim - t0, a);
        }
        else e->render (ch, 1, b, fim - b, a);
    }
    return s;
}

// roda a dupla em blocos de 512 (no lugar) e devolve o menor duckDb visto
static double roda (Dupla& d, std::vector<float>& s, const float* k, bool trava, double duck)
{
    double gMin = 0;
    for (int b = 0; b < (int) s.size(); b += 512)
    {
        const int m = std::min (512, (int) s.size() - b);
        float* ch[1] = { s.data() + b };
        d.process (ch, 1, k != nullptr ? k + b : nullptr, m, trava, duck);
        gMin = std::min (gMin, (double) d.duckDb.load());
    }
    return gMin;
}

// soma no grave em relação à soma de potência, nos últimos 3 s, só onde o kick soa (dB; 0 = soma de potência)
static double somaGrave (const std::vector<float>& k, const std::vector<float>& b)
{
    double ek = 0, eb = 0, es = 0;
    const int n = (int) k.size();
    double env = 0;
    for (int i = 0; i < n; ++i)
    {
        env = std::max (std::fabs ((double) k[(size_t) i]), env * 0.9995);
        if (i < n - (int) (3 * fs) || env < 0.08) continue;
        const double kk = k[(size_t) i], bb = b[(size_t) i];
        ek += kk * kk; eb += bb * bb; es += (kk + bb) * (kk + bb);
    }
    return 10.0 * std::log10 (es / std::max (1e-12, ek + eb));
}

int main()
{
    const int n = (int) (8.0 * fs);
    const double fK = 43.65;   // kick afinado em F1, como o baixo
    // andamento com um número inteiro de ciclos de F1 por batida (22): a fase kick x baixo se repete a cada batida
    const double bpm = 60.0 / (22.0 / fK);
    const auto k = kick (n, fK, bpm);

    // 1) sem kick (sem CHUTE no projeto): sai idêntico, com KICK LOCK e DUCK no máximo
    {
        std::printf ("1) sem CHUTE no projeto\n");
        Dupla d; d.prepare (fs);
        const auto b0 = baixo (n, 1000);
        auto b = b0;
        roda (d, b, nullptr, true, 1.0);
        bool igual = true;
        for (int i = 0; i < n; ++i) if (b[(size_t) i] != b0[(size_t) i]) { igual = false; break; }
        confere (igual, "saída idêntica ao synth (bit a bit)");
    }

    // 2) com CHUTE, KICK LOCK desligado e DUCK 0: idêntico
    {
        std::printf ("2) CHUTE no projeto, KICK LOCK desligado, DUCK 0\n");
        Dupla d; d.prepare (fs);
        const auto b0 = baixo (n, 1000);
        auto b = b0;
        roda (d, b, k.data(), false, 0.0);
        bool igual = true;
        for (int i = 0; i < n; ++i) if (b[(size_t) i] != b0[(size_t) i]) { igual = false; break; }
        confere (igual, "saída idêntica ao synth (bit a bit)");
    }

    // 3) KICK LOCK: a nota começa em 4 fases diferentes em relação ao kick
    {
        std::printf ("3) KICK LOCK (motor do ALIGN) com a nota começando em 8 fases diferentes\n");
        double piorAntes = 0, piorDepois = 0, piorPiora = -99, cpuMs = 0;
        bool nan = false;
        for (int j = 0; j < 8; ++j)
        {
            const int t0 = 1000 + (int) std::lround (j * fs / fK / 8.0);
            const auto b0 = baixo (n, t0);
            auto b = b0;
            Dupla d; d.prepare (fs);
            const auto c0 = std::chrono::steady_clock::now();
            roda (d, b, k.data(), true, 0.0);
            cpuMs += std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - c0).count();
            for (float v : b) if (! std::isfinite (v)) nan = true;
            const double antes = somaGrave (k, b0), depois = somaGrave (k, b);
            std::printf ("   fase %3d°: soma no grave %+5.1f dB -> %+5.1f dB (0 = soma de potência)\n", j * 45, antes, depois);
            piorAntes = std::max (piorAntes, std::fabs (antes));
            piorDepois = std::max (piorDepois, std::fabs (depois));
            piorPiora = std::max (piorPiora, std::fabs (depois) - std::fabs (antes));
        }
        std::printf ("   pior desvio: %.1f dB sem -> %.1f dB com; CPU %.2f%% de um núcleo\n", piorAntes, piorDepois, cpuMs / 8.0 / 8000.0 * 100.0);
        confere (! nan, "sem NaN");
        confere (piorDepois < 2.0, "com KICK LOCK o grave fica a menos de 2 dB da soma de potência em todas as fases");
        confere (piorPiora < 0.5, "nunca fica pior que sem KICK LOCK");
        confere (cpuMs / 8.0 / 8000.0 < 0.05, "CPU abaixo de 5% de um núcleo");
    }

    // 4) DUCK
    {
        std::printf ("4) DUCK (KICK LOCK desligado)\n");
        const auto b0 = baixo (n, 1000);
        for (double dk : { 0.5, 1.0 })
        {
            auto b = b0;
            Dupla d; d.prepare (fs);
            const double gMin = roda (d, b, k.data(), false, dk);
            // ganho real no pico do kick e no fim da batida (no último compasso)
            const int bat = (int) (fs * 60.0 / bpm), ini = ((n - 4 * bat) / bat) * bat;
            double pk = 0, pb = 0, fk = 0, fb = 0;
            for (int i = ini + 10; i < ini + (int) (0.006 * fs); ++i) { pk += b[(size_t) i] * b[(size_t) i]; pb += b0[(size_t) i] * b0[(size_t) i]; }
            for (int i = ini + bat - (int) (0.03 * fs); i < ini + bat - 10; ++i) { fk += b[(size_t) i] * b[(size_t) i]; fb += b0[(size_t) i] * b0[(size_t) i]; }
            const double gPico = 10.0 * std::log10 (pk / pb), gFim = 10.0 * std::log10 (fk / fb);
            std::printf ("   DUCK %3.0f%%: menor ganho %.1f dB, no pico do kick %.1f dB, no fim da batida %.1f dB\n", dk * 100, gMin, gPico, gFim);
            if (dk == 0.5) confere (gMin < -5.0 && gMin > -7.0, "DUCK 50%: ~ -6 dB no pico");
            else confere (gMin < -20.0, "DUCK 100%: abaixa quase tudo no pico");
            // no fim da batida o corpo do kick ainda soa (e^-2 = 13 % do pico): o DUCK segue ele, como no ALIGN
            const double esperado = db (1.0 - dk * std::exp (-(double) bat / fs / 0.25));
            std::printf ("   (esperado no fim da batida: %.1f dB)\n", esperado);
            confere (std::fabs (gFim - esperado) < 1.0, "no fim da batida segue a cauda do kick (volta junto com ela)");
        }
    }

    std::printf (falhas ? "\n%d FALHA(S)\n" : "\nTUDO OK\n", falhas);
    return falhas ? 1 : 0;
}
