// Mede o Dsp.h do STOMPBOX fora do plugin.
//   D:\Apps\granular-vst\toolchain\zig\ziglang\zig.exe c++ -O2 -std=c++17 -target x86_64-windows-gnu teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe
// Confere: o meio-banda do oversampling (passa até 20 kHz, rejeita o que dobraria); cada pedal com knobs nos extremos
// e em todas as chaves (sem NaN, sem estouro, sem DC); o aliasing de um drive em 2x; o bypass sem estalo; o eco no
// tempo do SYNC; o reverb que morre; os 5 amps limpo/sujo (nível e quanto distorcem); todos os presets (nível, CPU).
#include "../Source/Dsp.h"
#include "../Source/Presets.h"
#include <algorithm>
#include <chrono>
#include <complex>
#include <cstdio>
#include <vector>
using namespace stomp;

constexpr double fs = 48000.0;
static int falhas = 0;
static void confere (bool ok, const char* o) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALHOU", o); if (! ok) ++falhas; }
static double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }

// guitarra sintética: Karplus-Strong tocando um riff de power chord (E2/B2/E3) e notas soltas; pico ~0.3
static std::vector<float> guitarra (int n)
{
    std::vector<float> s ((size_t) n, 0.0f);
    struct Corda { std::vector<float> d; int p = 0; float lp = 0; };
    Rng rng;
    const double notas[][3] = { { 82.41, 123.47, 164.81 }, { 98.0, 146.83, 196.0 }, { 110.0, 164.81, 220.0 }, { 73.42, 110.0, 146.83 } };
    const int passo = (int) (fs * 0.5);
    for (int k = 0; k * passo < n; ++k)
        for (int c = 0; c < 3; ++c)
        {
            Corda co; const int len = (int) (fs / notas[k % 4][c]); co.d.resize ((size_t) len);
            for (auto& v : co.d) v = rng.next() * 0.12f;
            for (int i = k * passo + c * 90; i < std::min (n, (k + 1) * passo + 20000); ++i)
            {
                const float v = co.d[(size_t) co.p]; const int q = (co.p + 1) % len;
                co.d[(size_t) co.p] = 0.996f * 0.5f * (v + co.d[(size_t) q]); co.p = q;
                s[(size_t) i] += v * (i > (k + 1) * passo - 600 ? std::max (0.0f, 1.0f - (float) (i - ((k + 1) * passo - 600)) / 600.0f) : 1.0f);
            }
        }
    float pk = 0; for (auto v : s) pk = std::max (pk, std::abs (v));
    for (auto& v : s) v *= 0.3f / pk;
    return s;
}

struct Medida { double pico = 0, rms = 0, dc = 0; bool finito = true; };
static Medida mede (const std::vector<float>& x, int ini = 0)
{
    Medida m; double s2 = 0, s1 = 0; int c = 0;
    for (size_t i = (size_t) ini; i < x.size(); ++i) { const double v = x[i]; if (! std::isfinite (v)) m.finito = false; m.pico = std::max (m.pico, std::abs (v)); s2 += v * v; s1 += v; ++c; }
    m.rms = std::sqrt (s2 / std::max (1, c)); m.dc = s1 / std::max (1, c);
    return m;
}

// roda um pedal (mono na entrada) e devolve L
static std::vector<float> rodaPedal (int tipo, const Ctl& c, const std::vector<float>& in, std::vector<float>* outR = nullptr, int os = 2)
{
    auto p = criaPedal (tipo); p->prepare (fs, os);
    std::vector<float> L = in, R = in;
    for (size_t o = 0; o < L.size(); o += maxSub)
    {
        const int n = (int) std::min ((size_t) maxSub, L.size() - o);
        bool st = false;
        if (p->tornaEstereo()) std::memcpy (&R[o], &L[o], sizeof (float) * (size_t) n);
        p->process (&L[o], &R[o], n, st, c);
    }
    if (outR) *outR = R;
    return L;
}

// energia numa frequência (Goertzel)
static double goertzel (const std::vector<float>& x, double f, int ini)
{
    const double w = 2 * pi * f / fs; std::complex<double> acc = 0; int n = 0;
    for (size_t i = (size_t) ini; i < x.size(); ++i, ++n) acc += (double) x[i] * std::polar (1.0, -w * (double) n);
    return std::abs (acc) * 2.0 / std::max (1, n);
}

int main()
{
    const char* nomes[] = { "", "SQUASH", "HUMP", "CLEARCUT", "GRIT", "WOOLLY", "QUACK", "SWIRL", "TIDE", "ECHO", "HALO", "BENDER" };
    const int nChaves[] = { 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 2 };

    std::printf ("== meio-banda (2x a 96 kHz)\n");
    {
        const auto& h = HalfbandCoefs::get();
        std::printf ("  coeficientes:"); for (double c : h.c) std::printf (" %.5f", c); std::printf ("\n");
        auto tom = [] (double f, int n) { std::vector<float> s ((size_t) n); for (int i = 0; i < n; ++i) s[(size_t) i] = (float) std::sin (2 * pi * f * i / 96000.0); return s; };
        auto desce = [] (const std::vector<float>& x) { Halfband2x hb; std::vector<float> y (x.size() / 2); for (size_t i = 0; i < y.size(); ++i) y[i] = hb.down (x[2 * i], x[2 * i + 1]); return y; };
        auto amp = [] (const std::vector<float>& y) { double m = 0; for (size_t i = y.size() / 2; i < y.size(); ++i) m = std::max (m, (double) std::abs (y[i])); return m; };
        const double a1 = amp (desce (tom (1000, 96000))), a20 = amp (desce (tom (19000, 96000))), aRej = amp (desce (tom (30000, 96000))), aRej2 = amp (desce (tom (40000, 96000)));
        std::printf ("  1 kHz %.2f dB | 19 kHz %.2f dB | 30 kHz (dobraria p/ 18k) %.1f dB | 40 kHz %.1f dB\n", db (a1), db (a20), db (aRej), db (aRej2));
        confere (std::abs (db (a1)) < 0.1 && std::abs (db (a20)) < 0.5, "passa até 19 kHz sem mexer (<0,5 dB)");
        confere (db (aRej) < -70 && db (aRej2) < -70, "rejeita o que dobraria (< -70 dB)");
        // sobe e desce: 1 kHz volta inteiro
        Oversampler os; os.fator = 4; std::vector<float> x ((size_t) 4800), y ((size_t) 4800);
        for (int i = 0; i < 4800; ++i) x[(size_t) i] = (float) std::sin (2 * pi * 1000 * i / fs);
        for (int o = 0; o < 4800; o += maxSub) { os.up (&x[(size_t) o], maxSub); os.down (&y[(size_t) o], maxSub); }
        double m = 0; for (int i = 2400; i < 4800; ++i) m = std::max (m, (double) std::abs (y[(size_t) i]));
        confere (std::abs (db (m)) < 0.1, "4x sobe e desce 1 kHz sem perda");
    }

    const auto gtr = guitarra ((int) (fs * 4));
    const auto mIn = mede (gtr);
    std::printf ("\n== guitarra de teste: pico %.2f dB, RMS %.1f dB\n", db (mIn.pico), db (mIn.rms));

    std::printf ("\n== cada pedal: knobs 0 / 5 / 10 em todas as chaves (pico, RMS, DC)\n");
    for (int t = 1; t < numTipos; ++t)
    {
        bool ok = true; double pmax = 0, rmin = 1e9, rmax = 0;
        for (int sw = 0; sw < nChaves[t]; ++sw)
            for (float v : { 0.0f, 0.5f, 1.0f })
            {
                Ctl c; c.on = true; c.sw = sw; for (auto& k : c.k) k = v;
                if (t == Echo) c.k[1] = std::min (v, 0.85f);
                std::vector<float> R; const auto y = rodaPedal (t, c, gtr, &R);
                const auto m = mede (y, (int) fs / 2), mr = mede (R, (int) fs / 2);
                ok = ok && m.finito && mr.finito && m.pico < 4.0 && std::abs (m.dc) < 0.02;
                pmax = std::max (pmax, m.pico); rmin = std::min (rmin, m.rms); rmax = std::max (rmax, m.rms);
            }
        // o "meio" (knobs em 5, chave 0) perto do nível de entrada
        Ctl c; c.on = true; for (auto& k : c.k) k = 0.5f;
        const auto mm = mede (rodaPedal (t, c, gtr), (int) fs / 2);
        std::printf ("  %-9s pico máx %6.1f dB | RMS %6.1f a %6.1f dB | knobs em 5: RMS %+5.1f dB da entrada\n", nomes[t], db (pmax), db (rmin), db (rmax), db (mm.rms) - db (mIn.rms));
        char o[96]; std::snprintf (o, sizeof o, "%s: sem NaN, pico < +12 dB, sem DC", nomes[t]); confere (ok, o);
        std::snprintf (o, sizeof o, "%s: knobs em 5 a ±12 dB da entrada", nomes[t]); confere (std::abs (db (mm.rms) - db (mIn.rms)) < 12.0, o);
    }

    std::printf ("\n== aliasing: GRIT no máximo com seno de 3,1 kHz (o que não é harmônico)\n");
    for (int os : { 1, 2, 4 })
    {
        std::vector<float> s ((size_t) fs); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3f * (float) std::sin (2 * pi * 3100.0 * (double) i / fs);
        Ctl c; c.on = true; c.k[0] = 1.0f; c.k[1] = 0.0f; c.k[2] = 0.7f;
        const auto y = rodaPedal (Grit, c, s, nullptr, os);
        double harm = 0, alias = 0;
        for (int h = 1; h * 3100 < 24000; ++h) harm = std::max (harm, goertzel (y, h * 3100.0, (int) fs / 4));
        for (int h = 8; h <= 15; ++h) { const double f = std::abs (std::fmod (h * 3100.0 + 24000.0, 48000.0) - 24000.0); if (std::fmod (f, 3100.0) > 50 && std::fmod (f, 3100.0) < 3050) alias = std::max (alias, goertzel (y, f, (int) fs / 4)); }
        std::printf ("  %dx: pior dobra %.1f dB abaixo do maior harmônico\n", os, db (harm) - db (alias));
        if (os == 2) confere (db (harm) - db (alias) > 45, "GRIT em 2x: dobra > 45 dB abaixo");
    }

    std::printf ("\n== bypass sem estalo (liga/desliga o HUMP no meio do riff)\n");
    {
        // maior salto entre amostras com o pedal sempre ligado, sempre desligado e trocando a cada ~0,2 s
        auto roda = [&] (int modo) {
            Board b; b.prepare (fs, false); BoardCtl bc; bc.ampOn = false; bc.slot[0].tipo = Hump;
            for (auto& k : bc.slot[0].c.k) k = 0.7f;
            std::vector<float> L = gtr, R = gtr; double salto = 0; float ant = 0;
            for (size_t o = 0; o < L.size(); o += 256)
            {
                bc.slot[0].c.on = modo == 0 ? true : modo == 1 ? false : ((o / 256) / 40) % 2 == 0;
                const int n = (int) std::min ((size_t) 256, L.size() - o);
                b.process (&L[o], &R[o], n, false, bc);
                for (int i = 0; i < n; ++i) { if (o + (size_t) i > 0) salto = std::max (salto, (double) std::abs (L[o + (size_t) i] - ant)); ant = L[o + (size_t) i]; }
            }
            return salto;
        };
        const double sOn = roda (0), sOff = roda (1), sTroca = roda (2);
        std::printf ("  maior salto: sempre ligado %.3f | sempre desligado %.3f | trocando %.3f\n", sOn, sOff, sTroca);
        confere (sTroca <= std::max (sOn, sOff) * 1.1, "trocar não cria salto maior que o do próprio som");
    }

    std::printf ("\n== ECHO com SYNC: 1/4 a 120 BPM = 500 ms\n");
    {
        std::vector<float> imp ((size_t) fs, 0.0f); imp[100] = 0.5f;
        Ctl c; c.on = true; c.sync = true; c.bpm = 120; c.k[0] = 8.5f / 13.0f; c.k[1] = 0.3f; c.k[2] = 1.0f; c.k[3] = 0.0f;
        const auto y = rodaPedal (Echo, c, imp);
        int pos = 0; double mx = 0; for (size_t i = 2000; i < y.size(); ++i) if (std::abs (y[i]) > mx) { mx = std::abs (y[i]); pos = (int) i; }
        std::printf ("  divisão %s, eco em %.1f ms\n", nomeDivisaoDelay (c.k[0]), (pos - 100) * 1000.0 / fs);
        confere (std::abs ((pos - 100) * 1000.0 / fs - 500.0) < 8.0, "eco a 500 ms (±8)");
        // FEEDBACK no máximo: auto-oscila mas não explode
        c.k[1] = 1.0f; c.sync = false; c.k[0] = 0.3f;
        auto g2 = gtr; g2.resize ((size_t) (fs * 10), 0.0f);
        const auto m = mede (rodaPedal (Echo, c, g2));
        std::printf ("  FEEDBACK 10 por 10 s: pico %.1f dB\n", db (m.pico));
        confere (m.finito && m.pico < 3.0, "auto-oscilação contida (< +10 dB)");
    }

    std::printf ("\n== HALO: a cauda morre (DECAY 10, entrada para aos 1 s)\n");
    for (int sw = 0; sw < 3; ++sw)
    {
        std::vector<float> s ((size_t) (fs * 14), 0.0f); for (size_t i = 0; i < (size_t) fs; ++i) s[i] = gtr[i];
        Ctl c; c.on = true; c.sw = sw; c.k[0] = 1.0f; c.k[1] = 0.5f; c.k[2] = 1.0f; c.k[3] = 0.2f;
        const auto y = rodaPedal (Halo, c, s);
        auto rmsEm = [&] (double t0, double t1) { double a = 0; int n = 0; for (size_t i = (size_t) (t0 * fs); i < (size_t) (t1 * fs); ++i, ++n) a += (double) y[i] * y[i]; return std::sqrt (a / n); };
        const double r1 = rmsEm (1.0, 1.5), r2 = rmsEm (12.0, 14.0);
        const char* nm[] = { "SPRING", "PLATE", "HALL" };
        std::printf ("  %-6s cauda logo após: %.1f dB | 11 s depois: %.1f dB\n", nm[sw], db (r1), db (r2));
        char o[64]; std::snprintf (o, sizeof o, "%s: cauda cai mais de 40 dB", nm[sw]); confere (db (r1) - db (r2) > 40.0 && mede (y).finito, o);
    }

    std::printf ("\n== AMPs (cab 4x12): RMS de saída e quanto distorcem (seno de 220 Hz, GAIN 5, MASTER 6)\n");
    {
        const char* an[] = { "BLONDE 65", "CHIME 30", "PLEX 68", "CITRUS", "IRON HG" };
        for (int a = 0; a < numAmps; ++a)
            for (int ch = 0; ch < 2; ++ch)
            {
                std::vector<float> s ((size_t) fs); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.2f * (float) std::sin (2 * pi * 220.0 * (double) i / fs);
                AmpCore amp; amp.prepare (fs, 2); const float kn[6] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.6f };
                for (size_t o = 0; o < s.size(); o += maxSub) { amp.set (a, ch, kn); amp.process (&s[o], (int) std::min ((size_t) maxSub, s.size() - o)); }
                const double f1 = goertzel (s, 220, (int) fs / 4);
                double hs = 0; for (int h = 2; h <= 15; ++h) { const double v = goertzel (s, 220.0 * h, (int) fs / 4); hs += v * v; }
                const auto m = mede (s, (int) fs / 4);
                std::printf ("  %-9s %-5s RMS %6.1f dB | THD %5.1f%%\n", an[a], ch ? "DIRTY" : "CLEAN", db (m.rms), 100.0 * std::sqrt (hs) / std::max (f1, 1e-9));
                char o[64]; std::snprintf (o, sizeof o, "%s %s sem NaN e < 0 dBFS", an[a], ch ? "DIRTY" : "CLEAN"); confere (m.finito && m.pico < 1.0, o);
            }
    }

    std::printf ("\n== BENDER: seno de 220 Hz, pedal no bico, em cada intervalo (frequência que sai)\n");
    {
        for (int iv = 0; iv < 7; ++iv)
        {
            std::vector<float> s ((size_t) fs); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3f * (float) std::sin (2 * pi * 220.0 * (double) i / fs);
            Ctl c; c.on = true; c.sw = 0; c.k[0] = 1.0f; c.k[1] = ((float) iv + 0.5f) / 7.0f; c.k[2] = 1.0f;
            const auto y = rodaPedal (Bender, c, s);
            const double alvo = 220.0 * std::pow (2.0, benderSemitons (c.k[1]) / 12.0);
            // pitch por quadro (autocorrelação normalizada em quadros de 4096), mediana dos quadros
            std::vector<double> fr;
            for (int i0 = (int) fs / 2; i0 + 4096 + 1200 <= (int) y.size(); i0 += 2048)
            {
                std::vector<double> r (1100, 0.0);
                for (int lag = 1; lag < 1100; ++lag)
                {
                    double s = 0, e1 = 0, e2 = 0;
                    for (int i = 0; i < 4096; i += 2) { const double u = y[(size_t) (i0 + i)], v = y[(size_t) (i0 + i + lag)]; s += u * v; e1 += u * u; e2 += v * v; }
                    r[(size_t) lag] = s / std::sqrt (e1 * e2 + 1e-12);
                }
                int l0 = 1; while (l0 < 1099 && r[(size_t) l0] > 0) ++l0;            // passa o lóbulo do lag 0
                double mx = -1; for (int l = l0; l < 1099; ++l) mx = std::max (mx, r[(size_t) l]);
                int melhor = l0;                                                     // o 1º pico perto do maior
                for (int l = l0 + 1; l < 1098; ++l)
                    if (r[(size_t) l] >= 0.9 * mx && r[(size_t) l] >= r[(size_t) l - 1] && r[(size_t) l] >= r[(size_t) l + 1]) { melhor = l; break; }
                fr.push_back (fs / melhor);
            }
            std::sort (fr.begin(), fr.end());
            const double med = fr[fr.size() / 2];
            std::printf ("  %-6s alvo %7.1f Hz | medido %7.1f Hz | RMS %5.1f dB\n", benderNome (c.k[1]), alvo, med, db (mede (y, (int) fs / 2).rms));
            char o[80]; std::snprintf (o, sizeof o, "BENDER %s: sai no intervalo (±4%%, a emenda dos grãos espalha a medida)", benderNome (c.k[1]));
            confere (std::abs (med / alvo - 1.0) < 0.04 && mede (y).finito, o);
        }
        // calcanhar: sai o som original intacto
        std::vector<float> s ((size_t) fs); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3f * (float) std::sin (2 * pi * 220.0 * (double) i / fs);
        Ctl c; c.on = true; c.k[0] = 0.0f; c.k[1] = 0.8f; c.k[2] = 1.0f;
        const auto y = rodaPedal (Bender, c, s);
        double dif = 0; for (size_t i = 1000; i < y.size(); ++i) dif = std::max (dif, (double) std::abs (y[i] - s[i]));
        confere (dif < 1e-4, "BENDER no calcanhar = som original, sem atraso");
    }

    std::printf ("\n== tone stack (FMV, valores de componentes de cada amp): dB relativo a 700 Hz com tudo em 5\n");
    {
        const char* an[] = { "BLONDE 65", "CHIME 30", "PLEX 68", "CITRUS", "IRON HG" };
        auto mag = [] (int a, double t, double m, double l, double f) { return db (ToneStack::magnitude (stackAmp (a), t, m, ToneStack::taperLog (l), f)); };
        bool ok = true, okDig = true;
        for (int a = 0; a < numAmps; ++a)
        {
            const double ref = mag (a, .5, .5, .5, 700);
            std::printf ("  %-9s 100 Hz %+5.1f | 500 Hz %+5.1f | 1 kHz %+5.1f | 3 kHz %+5.1f || BASS 0→10 em 100 Hz %+5.1f | MID 0→10 em 500 Hz %+5.1f | TREBLE 0→10 em 3 kHz %+5.1f\n",
                         an[a], mag (a, .5, .5, .5, 100) - ref, mag (a, .5, .5, .5, 500) - ref, mag (a, .5, .5, .5, 1000) - ref, mag (a, .5, .5, .5, 3000) - ref,
                         mag (a, .5, .5, 1, 100) - mag (a, .5, .5, 0, 100), mag (a, .5, 1, .5, 500) - mag (a, .5, 0, .5, 500), mag (a, 1, .5, .5, 3000) - mag (a, 0, .5, .5, 3000));
            ok = ok && mag (a, .5, .5, 1, 100) > mag (a, .5, .5, 0, 100) + 3 && mag (a, 1, .5, .5, 3000) > mag (a, 0, .5, .5, 3000) + 3 && mag (a, .5, 1, .5, 500) > mag (a, .5, 0, .5, 500) + 1;
            // o digital (bilinear a 96 kHz) segue o analógico
            ToneStack ts; ts.set (stackAmp (a), .5, .5, ToneStack::taperLog (.5), 96000.0, 1.0);
            for (double f : { 100.0, 1000.0, 5000.0 })
            {
                std::vector<float> s (96000); double acc = 0; int nn = 0; std::complex<double> c = 0;
                for (int i = 0; i < 96000; ++i) { const float y = ts.process ((float) std::sin (2 * pi * f * i / 96000.0)); if (i >= 48000) { c += (double) y * std::polar (1.0, -2 * pi * f * i / 96000.0); ++nn; } }
                acc = std::abs (c) * 2.0 / nn;
                okDig = okDig && std::abs (db (acc) - mag (a, .5, .5, .5, f)) < 0.3;
            }
        }
        confere (ok, "BASS, MID e TREBLE mexem onde devem (grave, médio, agudo)");
        confere (okDig, "stack digital = circuito analógico (±0,3 dB em 100 Hz / 1 kHz / 5 kHz)");
    }

    std::printf ("\n== CAB (FIR de fase mínima, mic no centro e perto): dB relativo a 1 kHz\n");
    {
        const char* cn[] = { "1x12 OPEN", "2x12", "4x12" };
        bool ok = true;
        for (int c = 0; c < 3; ++c)
        {
            CabCore cab; cab.prepare (fs); cab.set (c, 0, 0.0f, 0.0f);
            std::vector<float> h ((size_t) fs / 4, 0.0f); h[0] = 1.0f;
            for (size_t o = 0; o < h.size(); o += maxSub) cab.process (&h[o], maxSub);
            auto resp = [&] (double f) { std::complex<double> acc = 0; for (size_t i = 0; i < h.size(); ++i) acc += (double) h[i] * std::polar (1.0, -2 * pi * f * (double) i / fs); return db (std::abs (acc)); };
            const double r = resp (1000);
            std::printf ("  %-9s (%d taps) 80 Hz %+5.1f | 120 %+5.1f | 500 %+5.1f | 2,5k %+5.1f | 4k %+5.1f | 6k %+5.1f | 8k %+5.1f | 12k %+5.1f\n", cn[c], cab.taps,
                         resp (80) - r, resp (120) - r, resp (500) - r, resp (2500) - r, resp (4000) - r, resp (6000) - r, resp (8000) - r, resp (12000) - r);
            ok = ok && resp (8000) - r < -15 && resp (12000) - r < -30 && resp (2500) - r > 0;
        }
        confere (ok, "cab: presença em 2,5 kHz, agudo cai (8 kHz < -15 dB, 12 kHz < -30 dB)");
    }

    std::printf ("\n== WOOLLY GE x SI (seno de 200 Hz, 0,1 de pico, FUZZ 7): harmônicos relativos à fundamental\n");
    {
        double h2[2], h3[2];
        for (int sw = 0; sw < 2; ++sw)
        {
            std::vector<float> s ((size_t) fs); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.1f * (float) std::sin (2 * pi * 200.0 * (double) i / fs);
            Ctl c; c.on = true; c.sw = sw; c.k[0] = 0.6f; c.k[1] = 0.7f;
            const auto y = rodaPedal (Woolly, c, s);
            const double f1 = goertzel (y, 200, (int) fs / 2);
            h2[sw] = db (goertzel (y, 400, (int) fs / 2)) - db (f1); h3[sw] = db (goertzel (y, 600, (int) fs / 2)) - db (f1);
            std::printf ("  %s: 2º harmônico %+5.1f dB | 3º %+5.1f dB\n", sw ? "SI" : "GE", h2[sw], h3[sw]);
        }
        confere (h2[0] - h3[0] > h2[1] - h3[1], "GE mais par (2º sobre 3º) que o SI");
    }

    std::printf ("\n== GRIT (RAT) DIST 10, sinal pequeno: ganho do op-amp por frequência (antes dos diodos)\n");
    {
        GritCore g; g.prepare (fs, 2); Ctl c; c.on = true; c.k[0] = 1.0f; c.k[1] = 0.0f; c.k[2] = 0.5f; g.set (c);
        // o ganho de pequeno sinal de 1 + DIST·Y (com o GBW do LM308), direto pela resposta dos filtros
        for (double f : { 100.0, 500.0, 3000.0 })
        {
            GritCore q; q.prepare (fs, 2); q.set (c);
            std::vector<float> s ((size_t) fs / 2); for (size_t i = 0; i < s.size(); ++i) s[i] = 1e-5f * (float) std::sin (2 * pi * f * (double) i / fs);
            std::vector<float> x2 = s; for (size_t o = 0; o < x2.size(); o += maxSub) q.process (&x2[o], maxSub);
            std::printf ("  %5.0f Hz: %+5.1f dB (saída/entrada, com o VOLUME em 5)\n", f, db (goertzel (x2, f, (int) fs / 4)) - db (goertzel (s, f, (int) fs / 4)));
        }
    }

    std::printf ("\n== presets de fábrica no board inteiro (guitarra de teste; CPU em 48 kHz)\n");
    {
        const auto& ps = presetsFabrica();
        for (size_t pi_ = 0; pi_ < ps.size(); ++pi_)
            for (int hq = 0; hq < 2; ++hq)
            {
                const auto& p = ps[pi_];
                Board b; b.prepare (fs, hq == 1); BoardCtl bc;
                for (size_t s = 0; s < p.pedais.size(); ++s)
                {
                    bc.slot[s].tipo = p.pedais[s].tipo; bc.slot[s].c.on = p.pedais[s].on; bc.slot[s].c.sw = p.pedais[s].sw;
                    for (int k = 0; k < 4; ++k) bc.slot[s].c.k[k] = p.pedais[s].k[k] / 10.0f;
                }
                bc.amp = p.amp; bc.canal = p.canal; for (int k = 0; k < 6; ++k) bc.ampK[k] = p.ampK[k] / 10.0f;
                bc.ampOn = p.ampOn; bc.cab = p.cab; bc.mic = p.mic; bc.micPos = p.micPos / 10.0f; bc.dist = p.dist / 10.0f; bc.gateThr = p.gateThr; bc.outDb = p.outDb; bc.inDb = p.inDb;
                std::vector<float> L = gtr, R = gtr;
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t o = 0; o < L.size(); o += 256) b.process (&L[o], &R[o], (int) std::min ((size_t) 256, L.size() - o), false, bc);
                const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                const auto m = mede (L, (int) fs / 2), mr = mede (R, (int) fs / 2);
                // com TODOS os pedais do preset ligados (pior caso de CPU)
                if (hq == 0) std::printf ("  %-14s pico %6.1f dB | RMS %6.1f dB (p/ -16: OUTPUT %+5.1f) | CPU %.2f%% (HQ: ", p.nome, db (std::max (m.pico, mr.pico)), db (m.rms), -16.0 - db (m.rms) + p.outDb, 100.0 * seg / 4.0);
                else std::printf ("%.2f%%)\n", 100.0 * seg / 4.0);
                if (hq == 0) { char o[80]; std::snprintf (o, sizeof o, "%s: sem NaN, pico < +6 dBFS, RMS > -40 dB", p.nome); confere (m.finito && mr.finito && m.pico < 2.0 && db (m.rms) > -40.0, o); }
            }
        // pior caso: 10 pedais ligados (drives e todos os de tempo) + amp em HQ
        Board b; b.prepare (fs, true); BoardCtl bc;
        const int tipos[10] = { Squash, Hump, Clearcut, Grit, Woolly, Quack, Swirl, Tide, Echo, Halo };
        for (int s = 0; s < 10; ++s) { bc.slot[s].tipo = tipos[s]; bc.slot[s].c.on = true; for (auto& k : bc.slot[s].c.k) k = 0.4f; }
        bc.amp = IronHG; bc.canal = 1;
        std::vector<float> L = gtr, R = gtr;
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t o = 0; o < L.size(); o += 256) b.process (&L[o], &R[o], (int) std::min ((size_t) 256, L.size() - o), false, bc);
        const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  pior caso (10 pedais + IRON HG, HQ): CPU %.2f%% de um núcleo, saída finita: %s\n", 100.0 * seg / 4.0, mede (L).finito ? "sim" : "NÃO");
        // HQ = 16x desde a 0.1.5 (o som de "downsampler" dos drives em 2x): 10 drives + amp é o caso extremo
        confere (seg / 4.0 < 0.40 && mede (L).finito, "pior caso < 40% de um núcleo");
    }

    std::printf ("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK", falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
