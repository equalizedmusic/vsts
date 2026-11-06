// Mede o Dsp.h fora do plugin com uma voz sintÃ©tica: pulso glotal (soma de harmÃ´nicos com a envoltÃ³ria de uma vogal
// "a" â€” formantes 700/1220/2600 Hz), melodia em DÃ“ MAIOR cantada desafinada (+35, âˆ’28, +18, âˆ’40 centsâ€¦), vibrato de
// 5,5 Hz em algumas notas, um slide, "consoantes" (ruÃ­do) e silÃªncios.
//   D:\Apps\granular-vst\toolchain\zig\ziglang\zig.exe c++ -O2 -std=c++17 -target x86_64-windows-gnu teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe
//
// Confere:
//   - DESLIGADO: a saÃ­da Ã© a entrada atrasada de D amostras, idÃªntica;
//   - AMOUNT 0: idÃªntica tambÃ©m (nenhum perÃ­odo pulado);
//   - LIGADO: o tom detectado (C maior), o erro de afinaÃ§Ã£o no miolo de cada nota antes e depois, o vibrato mantido
//     (desvio-padrÃ£o em volta do centro), o nÃ­vel por nota, estalos (maior salto entre amostras) e CPU.
#include "../Source/Dsp.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>
using namespace onkey;

struct Nota { double ini, dur; int midi; double cents; double vib; bool slideAntes; };

static double hzDe (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

struct Voz
{
    double fs;
    std::vector<Nota> notas;
    std::vector<float> sinal;
    std::vector<double> alvoM;   // nota cantada pretendida (MIDI exato, sem desvio) por amostra; 0 = sem voz
    std::vector<double> cantM;   // nota cantada de fato (centro, sem vibrato)

    explicit Voz (double f) : fs (f)
    {
        // C D E F G A B C ... (C maior), com desvios e vibrato
        const int mel[] = { 60, 62, 64, 65, 67, 69, 71, 72, 71, 69, 67, 65, 64, 62, 60, 64, 67, 72, 67, 64, 60, 62, 64, 62 };
        const double desv[] = { +35, -28, +18, -40, +25, -15, +42, -30, +10, -38, +30, -20, +45, -25, +15, -35, +28, -18, +38, -42, +22, -30, +12, -26 };
        double t = 0.3;
        for (int i = 0; i < 24; ++i)
        {
            const double dur = (i % 4 == 3) ? 0.9 : 0.45;
            const bool vib = (i % 4 == 3);
            notas.push_back ({ t, dur, mel[i], desv[i], vib ? 0.35 : 0.0, i == 10 || i == 17 });
            t += dur + ((i % 3 == 2) ? 0.12 : 0.03);
        }
        const auto total = (size_t) ((t + 0.4) * fs);
        sinal.assign (total, 0.0f);
        alvoM.assign (total, 0.0);
        cantM.assign (total, 0.0);

        std::mt19937 rng (7);
        std::normal_distribution<double> ruido (0.0, 1.0);
        double fase = 0.0;
        auto formante = [] (double f)
        {
            auto pico = [] (double f, double fc, double bw) { const double x = (f - fc) / bw; return 1.0 / (1.0 + x * x); };
            return 0.05 + pico (f, 700, 110) + 0.7 * pico (f, 1220, 120) + 0.35 * pico (f, 2600, 180);
        };
        for (size_t k = 0; k < total; ++k)
        {
            const double tt = k / fs;
            const Nota* nt = nullptr;
            for (auto& n : notas) if (tt >= n.ini && tt < n.ini + n.dur) { nt = &n; break; }
            if (! nt)
            {
                // entre notas: "consoante" curta (ruÃ­do agudo) ou silÃªncio
                double v = 0.0;
                for (auto& n : notas)
                    if (tt >= n.ini - 0.05 && tt < n.ini - 0.01) v = 0.05 * ruido (rng);
                sinal[k] = (float) v;
                continue;
            }
            const double u = tt - nt->ini;
            double m = nt->midi + nt->cents / 100.0;
            // slide: comeÃ§a 2 semitons abaixo e chega em 120 ms
            if (nt->slideAntes && u < 0.12) m -= 2.0 * (1.0 - u / 0.12);
            const double vib = nt->vib * std::sin (2 * kPi * 5.5 * std::max (0.0, u - 0.15)) * std::min (1.0, std::max (0.0, (u - 0.15) / 0.2));
            const double f0 = hzDe (m + vib);
            fase += f0 / fs;
            fase -= std::floor (fase);
            double v = 0.0;
            for (int h = 1; h * f0 < std::min (fs * 0.45, 5000.0); ++h)
                v += formante (h * f0) / h * std::sin (2 * kPi * h * fase);
            const double env = std::min (1.0, u / 0.02) * std::min (1.0, (nt->dur - u) / 0.03);
            sinal[k] = (float) (0.25 * v * env);
            if (! (nt->slideAntes && u < 0.15)) { alvoM[k] = nt->midi; cantM[k] = nt->midi + nt->cents / 100.0; }
        }
    }
};

// YIN simples na taxa cheia, para medir a saÃ­da sem usar o detector do prÃ³prio plugin
static double yin (const std::vector<float>& x, size_t centro, double fs)
{
    const int W = (int) (0.025 * fs), tmax = (int) (fs / 70), tmin = (int) (fs / 1100);
    if (centro < (size_t) (W + tmax) || centro + W + tmax >= x.size()) return 0;
    const size_t ini = centro - (W + tmax) / 2;
    std::vector<double> d (tmax + 2), c (tmax + 2);
    double s = 0; c[0] = 1;
    for (int t = 1; t <= tmax; ++t)
    {
        double a = 0;
        for (int j = 0; j < W; ++j) { const double e = (double) x[ini + j] - x[ini + j + t]; a += e * e; }
        d[t] = a; s += a; c[t] = s > 0 ? a * t / s : 1;
    }
    int b = -1;
    for (int t = tmin; t < tmax; ++t) if (c[t] < 0.1) { while (t + 1 < tmax && c[t + 1] < c[t]) ++t; b = t; break; }
    if (b < 0) return 0;
    double tau = b;
    const double den = c[b - 1] - 2 * c[b] + c[b + 1];
    if (den > 0) tau += 0.5 * (c[b - 1] - c[b + 1]) / den;
    return 69 + 12 * std::log2 (fs / tau / 440.0);
}

static std::vector<float> roda (Engine& e, const std::vector<float>& in, const Ajustes& a, double& seg)
{
    std::vector<float> L (in), R (in);
    const int bloco = 256;
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < in.size(); i += bloco)
    {
        const int n = (int) std::min<size_t> (bloco, in.size() - i);
        float* ch[2] = { L.data() + i, R.data() + i };
        e.process (ch, 2, n, a);
    }
    seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    for (size_t i = 0; i < L.size(); ++i)
        if (std::abs (L[i] - R[i]) > 1e-6f) { std::printf ("ERRO: L != R em %zu\n", i); break; }
    return L;
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        Voz v (fs);
        auto e = std::make_unique<Engine>();
        e->prepare (fs);
        const int D = e->latencia();
        std::printf ("\n=== %.0f Hz  latencia %d amostras (%.1f ms)  audio %.1f s ===\n", fs, D, 1000.0 * D / fs, v.sinal.size() / fs);

        // 1) desligado
        {
            Ajustes a; a.ligado = false;
            double seg; e->reset();
            auto out = roda (*e, v.sinal, a, seg);
            double mx = 0;
            for (size_t i = D + (size_t) (0.05 * fs); i < out.size(); ++i) mx = std::max (mx, (double) std::abs (out[i] - v.sinal[i - D]));
            std::printf ("DESLIGADO: maior diferenca para a entrada atrasada = %.2e\n", mx);
        }
        // 2) amount 0
        {
            Ajustes a; a.amount = 0;
            double seg; e->reset();
            auto out = roda (*e, v.sinal, a, seg);
            double mx = 0, se = 0, ss = 0;
            for (size_t i = D; i < out.size(); ++i)
            {
                const double d = out[i] - v.sinal[i - D];
                mx = std::max (mx, std::abs (d)); se += d * d; ss += (double) v.sinal[i - D] * v.sinal[i - D];
            }
            std::printf ("AMOUNT 0: maior diferenca = %.2e  (SNR %.1f dB)\n", mx, 10 * std::log10 (ss / std::max (se, 1e-30)));
        }
        // 3) ligado, padrÃ£o
        for (int modo = 0; modo < 2; ++modo)
        {
            Ajustes a;
            if (modo == 1) { a.speed = 1.0f; a.humanize = 0.0f; a.vibrato = 0.0f; }
            double seg; e->reset();
            auto out = roda (*e, v.sinal, a, seg);
            int nan = 0; double maxSalto = 0, maxSaltoIn = 0;
            for (size_t i = 1; i < out.size(); ++i)
            {
                if (! std::isfinite (out[i])) ++nan;
                maxSalto = std::max (maxSalto, (double) std::abs (out[i] - out[i - 1]));
                maxSaltoIn = std::max (maxSaltoIn, (double) std::abs (v.sinal[i] - v.sinal[i - 1]));
            }
            char nn[8];
            const int tom = e->leitura.tomDet.load();
            std::printf ("%s: tom detectado %s %s (conf %.2f)  NaN %d  maior salto entre amostras %.3f (entrada %.3f)  CPU %.1f%% de 1 nucleo\n",
                         modo == 0 ? "LIGADO (padrao)" : "LIGADO (speed 100, humanize 0, vibrato 0)",
                         tom >= 0 ? nomePc (tom) : "?", e->leitura.modoDet.load() ? "menor" : "maior", e->leitura.confianca.load(),
                         nan, maxSalto, maxSaltoIn, 100.0 * seg / (v.sinal.size() / fs));
            (void) nn;
            double somaAntes = 0, somaDepois = 0; int cont = 0;
            double sdVibIn = 0, sdVibOut = 0; int nVib = 0;
            std::printf ("  nota  pretendida  cantado(c)  saida(c)   nivel(dB)\n");
            for (size_t k = 0; k < v.notas.size(); ++k)
            {
                const auto& nt = v.notas[k];
                // miolo da nota: depois de 250 ms (a correÃ§Ã£o chegou) atÃ© 40 ms antes do fim
                const double ini = nt.ini + 0.25, fim = nt.ini + nt.dur - 0.04;
                if (fim <= ini) continue;
                double sIn = 0, sOut = 0, s2In = 0, s2Out = 0; int c = 0;
                double eIn = 0, eOut = 0;
                for (double tt = ini; tt < fim; tt += 0.005)
                {
                    const size_t ci = (size_t) (tt * fs);
                    const double mi = yin (v.sinal, ci, fs), mo = yin (out, ci + D, fs);
                    if (mi <= 0 || mo <= 0) continue;
                    sIn += mi; sOut += mo; s2In += mi * mi; s2Out += mo * mo; ++c;
                }
                for (size_t i = (size_t) (ini * fs); i < (size_t) (fim * fs); ++i)
                { eIn += (double) v.sinal[i] * v.sinal[i]; eOut += (double) out[i + D] * out[i + D]; }
                if (c < 5) continue;
                const double mIn = sIn / c, mOut = sOut / c;
                const double cIn = (mIn - nt.midi) * 100, cOut = (mOut - nt.midi) * 100;
                const double sdIn = std::sqrt (std::max (0.0, s2In / c - mIn * mIn)) * 100, sdOut = std::sqrt (std::max (0.0, s2Out / c - mOut * mOut)) * 100;
                if (k >= 8) { somaAntes += std::abs (cIn); somaDepois += std::abs (cOut); ++cont; }   // depois do tom travar
                if (nt.vib > 0) { sdVibIn += sdIn; sdVibOut += sdOut; ++nVib; }
                std::printf ("  %3s%d  %+6.0f      %+7.1f     %+7.1f    %+5.2f%s%s\n", nomePc (nt.midi), nt.midi / 12 - 1, nt.cents, cIn, cOut,
                             10 * std::log10 (eOut / std::max (eIn, 1e-20)), nt.vib > 0 ? (std::string ("   vibrato sd ") + std::to_string ((int) sdIn) + " -> " + std::to_string ((int) sdOut) + " c").c_str() : "",
                             k < 8 ? "  (antes do tom travar)" : "");
            }
            std::printf ("  erro medio (notas 9-24): antes %.1f c  depois %.1f c   |  vibrato sd medio: entrada %.1f c  saida %.1f c\n",
                         somaAntes / std::max (1, cont), somaDepois / std::max (1, cont), sdVibIn / std::max (1, nVib), sdVibOut / std::max (1, nVib));
        }
    }
    return 0;
}
