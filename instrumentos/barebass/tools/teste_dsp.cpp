// Mede o Dsp.h do BAREBASS fora do plugin.
//   D:\Apps\granular-vst\toolchain\zig\ziglang\zig.exe c++ -O2 -std=c++17 -target x86_64-windows-gnu teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe
// Confere: baixo rolando em 1/16 a 145 BPM (nível, NaN, estalos, CPU); o filtro corta (cutoff baixo x alto, 12 x 24 dB);
// o ENV abre o filtro e o DECAY fecha; o unHUNF tira o grave depois do ataque sem mexer acima de 300 Hz;
// LEGATO não redispara o envelope e MONO redispara; GLIDE desliza; cada nota recomeça a onda do ponto inicial;
// ATTACK 0 sai seco; girar o unHUNF não estala; nota encostada não é cortada; LEDs desligam os osciladores.
#include "../Source/Dsp.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>
using namespace barebass;

constexpr double fs = 48000.0;
static int falhas = 0;
static void confere (bool ok, const char* o) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALHOU", o); if (! ok) ++falhas; }

struct Saida { std::vector<float> s; };

// renderiza n amostras em blocos de 256; eventos = (amostra, nota, liga)
struct Ev { int t, nota; bool on; };
static Saida toca (Engine& e, const Ajustes& a, int n, std::vector<Ev> evs)
{
    Saida r; r.s.assign ((size_t) n, 0.0f);
    float* ch[1] = { r.s.data() };
    size_t k = 0;
    for (int b = 0; b < n; b += 256)
    {
        const int fim = std::min (n, b + 256);
        int pos = b;
        while (k < evs.size() && evs[k].t < fim)
        {
            if (evs[k].t > pos) { e.render (ch, 1, pos, evs[k].t - pos, a); pos = evs[k].t; }
            if (evs[k].on) e.noteOn (evs[k].nota, a); else e.noteOff (evs[k].nota, a);
            ++k;
        }
        if (fim > pos) e.render (ch, 1, pos, fim - pos, a);
    }
    return r;
}

// energia acima de ~1 kHz (passa-alta de 2ª ordem simples) e abaixo de 80 Hz
static double rmsFaixa (const std::vector<float>& s, int i0, int i1, double fc, bool alta)
{
    const double w = std::tan (pi * fc / fs), q = 0.7071, n = 1.0 / (1.0 + w / q + w * w);
    double b0, b1, b2;
    if (alta) { b0 = n; b1 = -2.0 * n; b2 = n; } else { b0 = w * w * n; b1 = 2.0 * b0; b2 = b0; }
    const double a1 = 2.0 * (w * w - 1.0) * n, a2 = (1.0 - w / q + w * w) * n;
    // 4 biquads em cascata (48 dB/oitava): o medidor não pode vazar a faixa vizinha
    double z[4][2] {}, acc = 0;
    for (int i = 0; i < i1; ++i)
    {
        double y = s[(size_t) i];
        for (auto& zz : z)
        {
            const double x = y;
            y = b0 * x + zz[0];
            zz[0] = b1 * x - a1 * y + zz[1]; zz[1] = b2 * x - a2 * y;
        }
        if (i >= i0) acc += y * y;
    }
    return std::sqrt (acc / std::max (1, i1 - i0));
}
static double db (double x) { return 20.0 * std::log10 (std::max (1e-9, x)); }

int main()
{
    auto e = std::make_unique<Engine>();
    e->prepare (fs);

    // 1) baixo rolando: F1 em 1/16 (145 BPM), nota de 60% do passo, padrão padrão
    {
        std::printf ("1) baixo rolando (F1, 1/16, 145 BPM), patch padrão\n");
        Ajustes a;
        const int passo = (int) (fs * 60.0 / 145.0 / 4.0), n = (int) (fs * 4.0);
        std::vector<Ev> evs;
        for (int t = 0, k = 0; t < n - passo; t += passo, ++k)
        {
            if (k % 4 == 0) continue;                 // o kick fica no tempo: o baixo toca nos 3 contratempos
            evs.push_back ({ t, 29, true });
            evs.push_back ({ t + passo * 6 / 10, 29, false });
        }
        e->reset();
        const auto t0 = std::chrono::steady_clock::now();
        auto r = toca (*e, a, n, evs);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        double pk = 0, salto = 0; bool nan = false;
        for (size_t i = 0; i < r.s.size(); ++i)
        {
            if (! std::isfinite (r.s[i])) nan = true;
            pk = std::max (pk, (double) std::abs (r.s[i]));
            if (i) salto = std::max (salto, (double) std::abs (r.s[i] - r.s[i - 1]));
        }
        std::printf ("   pico %.2f (%.1f dBFS), maior salto entre amostras %.3f, CPU %.2f%% de um núcleo\n", pk, db (pk), salto, ms / 4000.0 * 100.0);
        confere (! nan, "sem NaN");
        confere (pk > 0.1 && pk < 1.2, "nível de saída razoável");
        confere (ms / 4000.0 < 0.05, "CPU abaixo de 5% de um núcleo");
    }

    // 2) filtro: nota segura de SAW, ENV 0
    {
        std::printf ("2) filtro (SAW em A1, sem ENV)\n");
        auto mede = [&] (float cut, bool s24)
        {
            Ajustes a; a.envAmt = 0; a.cutoff = cut; a.slope24 = s24; a.reso = 0; a.hunf = 0;
            a.nivel[1] = 0; a.nivelSub = 0; a.onda[0] = ondaSaw; a.oitava[0] = 0; a.fino[0] = 0;
            e->reset();
            auto r = toca (*e, a, (int) fs, { { 0, 33, true } });
            return rmsFaixa (r.s, (int) (fs * 0.3), (int) fs, 1000.0, true);
        };
        const double alto = mede (8000, true), baixo24 = mede (300, true), baixo12 = mede (300, false);
        std::printf ("   acima de 1 kHz: cutoff 8 kHz %.1f dB · cutoff 300 Hz 24 dB %.1f dB · 12 dB %.1f dB\n", db (alto), db (baixo24), db (baixo12));
        confere (db (alto) - db (baixo24) > 30.0, "cutoff baixo corta o agudo (> 30 dB)");
        confere (db (baixo12) - db (baixo24) > 8.0, "24 dB corta mais que 12 dB");
    }

    // 3) ENV + DECAY: o filtro abre no ataque e fecha
    {
        std::printf ("3) ENV abre e DECAY fecha\n");
        Ajustes a; a.hunf = 0;
        e->reset();
        auto r = toca (*e, a, (int) (fs * 0.6), { { 0, 33, true } });
        const double ini = rmsFaixa (r.s, 0, (int) (fs * 0.02), 1000.0, true);
        const double fim = rmsFaixa (r.s, (int) (fs * 0.4), (int) (fs * 0.6), 1000.0, true);
        std::printf ("   agudo nos primeiros 20 ms %.1f dB · depois de 400 ms %.1f dB\n", db (ini), db (fim));
        confere (db (ini) - db (fim) > 20.0, "o envelope abre o filtro e o decay fecha");
    }

    // 4) unHUNF: o grave cai depois do ataque (sem atrasar o ataque); acima de 300 Hz não muda
    {
        std::printf ("4) unHUNF (nota longa em F1, filtro aberto)\n");
        auto mede = [&] (float h, double& gIni, double& gFim, double& agudo)
        {
            Ajustes a; a.hunf = h; a.envAmt = 0; a.cutoff = 3000;
            e->reset();
            auto r = toca (*e, a, (int) fs, { { 0, 29, true } });
            gIni = rmsFaixa (r.s, (int) (fs * 0.002), (int) (fs * 0.015), 70.0, false);
            gFim = rmsFaixa (r.s, (int) (fs * 0.5), (int) (fs * 0.9), 70.0, false);
            agudo = rmsFaixa (r.s, (int) (fs * 0.5), (int) (fs * 0.9), 300.0, true);
        };
        double i0, f0, a0, i1, f1, a1;
        mede (0.0f, i0, f0, a0);
        mede (0.8f, i1, f1, a1);
        std::printf ("   grave (< 70 Hz) no ataque: sem %.1f dB · com %.1f dB\n", db (i0), db (i1));
        std::printf ("   grave depois de 500 ms:    sem %.1f dB · com %.1f dB\n", db (f0), db (f1));
        std::printf ("   acima de 300 Hz depois:    sem %.1f dB · com %.1f dB\n", db (a0), db (a1));
        confere (std::abs (db (i0) - db (i1)) < 3.0, "o ataque do grave fica");
        confere (db (f0) - db (f1) > 10.0, "o grave some depois (unHUNF 80% > 10 dB)");
        confere (std::abs (db (a0) - db (a1)) < 1.5, "acima de 300 Hz não muda");
    }

    // 4b) unHUNF no rolling (pedido dele 07/11: na 0.1.2 ficou fraco): F#1 em 1/16 a 143 BPM (≈105 ms por nota).
    //     No fim de cada nota (60–100 ms) o grave abaixo de 50 Hz tem que cair bem; acima de 150 Hz quase nada.
    {
        std::printf ("4b) unHUNF no rolling F#1 1/16 a 143 BPM\n");
        const int passo = (int) (fs * 60.0 / 143.0 / 4.0);
        auto mede = [&] (float h, double& sub, double& meio)
        {
            Ajustes a; a.hunf = h;
            e->reset();
            std::vector<Ev> ev;
            for (int k = 0; k < 8; ++k) { ev.push_back ({ k * passo, 30, true }); ev.push_back ({ k * passo + passo - 200, 30, false }); }
            auto r = toca (*e, a, passo * 8, ev);
            sub = meio = 0;
            for (int k = 4; k < 8; ++k)
            {
                sub += rmsFaixa (r.s, k * passo + (int) (fs * 0.06), k * passo + (int) (fs * 0.1), 50.0, false) / 4;
                meio += rmsFaixa (r.s, k * passo + (int) (fs * 0.06), k * passo + (int) (fs * 0.1), 150.0, true) / 4;
            }
        };
        double s0, m0, s5, m5, s1, m1;
        mede (0.0f, s0, m0); mede (0.5f, s5, m5); mede (1.0f, s1, m1);
        std::printf ("   < 50 Hz no fim da nota: 0%% %.1f dB · 50%% %.1f dB · 100%% %.1f dB\n", db (s0), db (s5), db (s1));
        std::printf ("   > 150 Hz no fim da nota: 0%% %.1f dB · 50%% %.1f dB · 100%% %.1f dB\n", db (m0), db (m5), db (m1));
        confere (db (s0) - db (s5) > 6.0, "50% tira mais de 6 dB do grave < 50 Hz no fim da nota");
        confere (db (s0) - db (s1) > 18.0, "100% tira mais de 18 dB do grave < 50 Hz no fim da nota");
        confere (std::abs (db (m0) - db (m1)) < 1.5, "acima de 150 Hz não muda");
    }

    // 5) LEGATO x MONO
    {
        std::printf ("5) LEGATO não redispara, MONO redispara\n");
        for (int leg = 0; leg < 2; ++leg)
        {
            Ajustes a; a.legato = leg == 1;
            e->reset();
            const int t2 = (int) (fs * 0.3);
            toca (*e, a, t2 + 64, { { 0, 33, true }, { t2, 36, true } });
            const float env = e->leitura.env.load();
            std::printf ("   %s: envelope logo depois da 2ª nota presa = %.2f\n", leg ? "LEGATO" : "MONO", env);
            confere (leg ? env < 0.2f : env > 0.8f, leg ? "legato: segue sem redisparar" : "mono: redispara");
        }
    }

    // 6) GLIDE
    {
        std::printf ("6) GLIDE 200 ms\n");
        Ajustes a; a.glide = 0.2f; a.legato = true;
        e->reset();
        const int t2 = (int) (fs * 0.2);
        toca (*e, a, t2 + (int) (fs * 0.05), { { 0, 33, true }, { t2, 45, true } });
        const float meio = e->leitura.freq.load();
        toca (*e, a, (int) (fs * 0.5), {});
        const float fim = e->leitura.freq.load();
        std::printf ("   A1 (55 Hz) -> A2 (110 Hz): 50 ms depois %.1f Hz, 550 ms depois %.1f Hz\n", meio, fim);
        confere (meio > 58 && meio < 105, "no meio do caminho");
        confere (std::abs (fim - 110.0f) < 1.0f, "chega na nota");
    }

    // patch INIT (filtro aberto, sem unHUNF): o que sai é a onda crua, dá para comparar nota com nota
    auto init = []
    {
        Ajustes a; a.fino[0] = a.fino[1] = 0; a.oitava[1] = 0; a.nivel[0] = a.nivel[1] = 0.8f; a.nivelSub = 0.8f;
        a.cutoff = 18000; a.reso = 0; a.envAmt = 0; a.sus = 1; a.hunf = 0;
        return a;
    };

    // 7) toda nota recomeça a onda do ponto inicial (rolando em F#1, 143 BPM, notas de 90% do passo: o release da
    //    anterior ainda soa quando a próxima entra)
    {
        std::printf ("7) fase: cada nota começa igual (F#1, 1/16, 143 BPM)\n");
        for (int patch = 0; patch < 2; ++patch)
        {
            Ajustes a = patch == 0 ? init() : Ajustes {};
            if (patch == 1) a.hunf = 0;
            const int passo = (int) (fs * 60.0 / 143.0 / 4.0);
            std::vector<Ev> evs;
            for (int k = 1; k < 4; ++k) { evs.push_back ({ k * passo, 30, true }); evs.push_back ({ k * passo + passo * 9 / 10, 30, false }); }
            e->reset();
            auto r = toca (*e, a, passo * 4, evs);
            double dif = 0, ref = 0;
            for (int i = 0; i < 400; ++i)
            {
                dif = std::max (dif, (double) std::abs (r.s[(size_t) (2 * passo + i)] - r.s[(size_t) (3 * passo + i)]));
                ref = std::max (ref, (double) std::abs (r.s[(size_t) (2 * passo + i)]));
            }
            std::printf ("   %s: 2ª x 3ª nota, primeiras 400 amostras: maior diferença %.4f (pico %.2f)\n",
                         patch == 0 ? "INIT" : "ROLLING", dif, ref);
            confere (dif < 0.05 * ref, patch == 0 ? "INIT: as notas saem iguais" : "ROLLING: as notas saem iguais");
        }
    }

    // 8) ATTACK 0 = ataque seco: a SAW sai do topo já na 2ª amostra
    {
        std::printf ("8) ATTACK 0: clique do ataque\n");
        Ajustes a = init(); a.liga[1] = false; a.ligaSub = false;
        e->reset();
        toca (*e, a, 2400, {});   // o CUTOFF anda suave (10 ms): deixa ele chegar nos 18 kHz do INIT antes da nota
        auto r = toca (*e, a, 64, { { 0, 30, true } });
        std::printf ("   amostras 0..3: %.3f %.3f %.3f %.3f\n", r.s[0], r.s[1], r.s[2], r.s[3]);
        confere (r.s[1] > 0.3f || r.s[2] > 0.3f, "a onda sai do ponto inicial na hora (sem rampa)");
    }

    // 9) girar o unHUNF com a nota soando não estala; 10% tira pouco
    {
        std::printf ("9) unHUNF: girar não estala; o começo do knob é suave\n");
        Ajustes a; a.envAmt = 0; a.cutoff = 3000; a.hunf = 0.15f;
        e->reset();
        const int n0 = (int) (fs * 0.5);
        auto r1 = toca (*e, a, n0, { { 0, 30, true } });
        Ajustes b = a; b.hunf = 0.8f;
        auto r2 = toca (*e, b, n0, {});
        double base = 0, giro = 0;
        for (int i = n0 / 2; i < n0; ++i) base = std::max (base, (double) std::abs (r1.s[(size_t) i] - r1.s[(size_t) i - 1]));
        giro = std::abs (r2.s[0] - r1.s[(size_t) n0 - 1]);
        for (int i = 1; i < 4800; ++i) giro = std::max (giro, (double) std::abs (r2.s[(size_t) i] - r2.s[(size_t) i - 1]));
        std::printf ("   maior salto entre amostras: nota parada %.4f · girando 15 -> 80%% %.4f\n", base, giro);
        confere (giro < base * 1.1 + 1e-4, "sem estalo ao girar");
        std::printf ("   o que sai de um sub de 30 Hz no fim: 10%% %.1f dB · 30%% %.1f dB · 50%% %.1f dB · 100%% %.1f dB\n", db (hunfPiso (0.1)),
                     db (hunfPiso (0.3)), db (hunfPiso (0.5)), db (hunfPiso (1.0)));
        confere (db (hunfPiso (0.1)) > -1.5, "10% tira menos de 1,5 dB do sub de 30 Hz");
    }

    // 10) nota encostada: o "liga" da nova chega antes do "solta" da anterior (mesma nota)
    {
        std::printf ("10) nota encostada (liga antes do solta, mesma amostra)\n");
        Ajustes a;
        e->reset();
        toca (*e, a, 2000, { { 0, 30, true }, { 1000, 30, true }, { 1000, 30, false } });
        confere (e->leitura.gate.load(), "a nota nova continua tocando");
        toca (*e, a, 1000, { { 10, 30, false } });
        confere (! e->leitura.gate.load(), "o solta dela fecha a nota");
    }

    // 11) LEDs: OSC 1, OSC 2 e SUB desligados = silêncio
    {
        std::printf ("11) LEDs dos osciladores\n");
        Ajustes a = init(); a.liga[0] = a.liga[1] = false; a.ligaSub = false;
        e->reset();
        auto r = toca (*e, a, 4800, { { 0, 30, true } });
        double pk = 0; for (float v : r.s) pk = std::max (pk, (double) std::abs (v));
        confere (pk < 1e-6, "tudo desligado = silêncio");
    }

    // 12) VELOCITY (0.3.0): com VELO em 100% a nota fraca sai mais baixa e mais fechada; em 0% toda nota igual
    {
        std::printf ("12) VELOCITY\n");
        auto nivel = [&] (float velo, float v)
        {
            Ajustes a = init(); a.velo = velo; a.cutoff = 300; a.envAmt = 0.6f; a.sus = 0.3f;
            e->reset(); toca (*e, a, 2400, {});
            e->noteOn (33, a, v);
            auto r = toca (*e, a, (int) (fs * 0.3), {});
            double s2 = 0; for (float x : r.s) s2 += (double) x * x;
            return std::sqrt (s2 / (double) r.s.size());
        };
        const double forte = nivel (1.0f, 1.0f), fraca = nivel (1.0f, 0.4f), igual = nivel (0.0f, 0.4f);
        std::printf ("   VELO 100%%: velocity 127 %.1f dB · velocity ~50 %.1f dB · VELO 0%% com ~50: %.1f dB\n", db (forte), db (fraca), db (igual));
        confere (db (forte) - db (fraca) > 6.0, "VELO 100%: nota fraca mais baixa (> 6 dB)");
        confere (std::abs (db (forte) - db (igual)) < 0.1, "VELO 0%: velocity não muda nada");
    }

    std::printf (falhas ? "\n%d FALHA(S)\n" : "\nTUDO OK\n", falhas);
    return falhas ? 1 : 0;
}
