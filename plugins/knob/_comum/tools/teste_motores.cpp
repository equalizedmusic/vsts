// Teste dos motores sem JUCE: roda um Pedal.cpp em todas as formas, nos 3 TRIGGERs e em vários valores do knob,
// com um sinal de bateria + acorde, e confere NaN, pico, nível e se o efeito mexe no som.
// Compilar (um pedal por vez):  zig c++ -O2 -std=c++17 -I ../Source -I <pedal>/Source teste_motores.cpp <pedal>/Source/Pedal.cpp
// Uso: teste.exe [saida.wav]  (grava 8 s do pedal na forma padrão, knob padrão, para ouvir)
#include "Motor.h"
#include <cstdio>
#include <cstring>
#include <string>

using namespace sk;

static void sinal (std::vector<float>& L, std::vector<float>& R, double sr, double bpm)
{
    const size_t n = L.size();
    const double spb = sr * 60.0 / bpm;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double b = i / spb;                 // tempo em tempos
        const double fb = b - std::floor (b);
        const double kick = std::exp (-fb * 14.0) * std::sin (2 * 3.14159 * (50 + 90 * std::exp (-fb * 30)) * fb * spb / sr);
        const double h8 = (b * 2) - std::floor (b * 2);
        const double hat = std::exp (-h8 * 60) * (((i * 2654435761u) >> 8) % 1000 / 500.0 - 1.0) * 0.25;
        const double acorde = 0.12 * (std::sin (2 * 3.14159 * 220 * t) + std::sin (2 * 3.14159 * 277.2 * t) + std::sin (2 * 3.14159 * 329.6 * t));
        L[i] = (float) (0.6 * kick + hat + acorde);
        R[i] = (float) (0.6 * kick - hat * 0.7 + acorde * 0.9);
    }
}

static void wav (const char* arq, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (arq, "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) L.size(), dados = n * 4;
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dados); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dados);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, L[i])) * 32767);
        int16_t b = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, R[i])) * 32767);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

struct Resultado { bool nan; float pico, rmsIn, rmsOut, dif; };

static Resultado rodar (int forma, float valor, int gatilho, int rateIdx, double sr, int bloco, std::vector<float>* oL = nullptr, std::vector<float>* oR = nullptr)
{
    const auto& in = info();
    const double bpm = 124.0, segundos = 8.0;
    const size_t n = (size_t) (sr * segundos);
    std::vector<float> L (n), R (n);
    sinal (L, R, sr, bpm);
    const auto L0 = L, R0 = R;
    auto m = criarMotor();
    m->preparar (sr);
    std::vector<double> pos ((size_t) bloco);
    std::vector<uint8_t> disp ((size_t) bloco);
    const double bps = bpm / 60.0 / sr;
    double posDisparo = 1.0e6;
    for (size_t ini = 0; ini < n; ini += (size_t) bloco)
    {
        const int k = (int) std::min ((size_t) bloco, n - ini);
        for (int i = 0; i < k; ++i)
        {
            const size_t a = ini + (size_t) i;
            const double beat = a * bps;
            disp[(size_t) i] = 0;
            // disparo a cada 2 tempos, começando no tempo 1 (MIDI/ÁUDIO)
            if (gatilho != gatInt && a > 0 && std::floor (beat / 2.0 - 0.5) != std::floor ((a - 1) * bps / 2.0 - 0.5) && beat > 0.9)
            { disp[(size_t) i] = 1; posDisparo = 0.0; }
            pos[(size_t) i] = gatilho == gatInt ? beat : posDisparo;
            posDisparo += bps;
        }
        Bloco b;
        b.sr = sr; b.bpm = bpm; b.compasso = 4.0; b.rate = in.rates[(size_t) rateIdx]; b.valor = valor; b.forma = forma;
        b.gatilho = gatilho; b.pos = pos.data(); b.disparo = disp.data();
        m->processar (L.data() + ini, R.data() + ini, k, b);
        if (m->nivel != m->nivel) return { true, 0, 0, 0, 0 };
    }
    Resultado r { false, 0, 0, 0, 0 };
    double si = 0, so = 0, sd = 0;
    for (size_t i = 0; i < n; ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) r.nan = true;
        r.pico = std::max (r.pico, std::max (std::abs (L[i]), std::abs (R[i])));
        si += L0[i] * L0[i] + R0[i] * R0[i];
        so += L[i] * L[i] + R[i] * R[i];
        sd += (L[i] - L0[i]) * (L[i] - L0[i]) + (R[i] - R0[i]) * (R[i] - R0[i]);
    }
    r.rmsIn = (float) std::sqrt (si / (2 * n));
    r.rmsOut = (float) std::sqrt (so / (2 * n));
    r.dif = (float) std::sqrt (sd / (2 * n));
    if (oL) { *oL = L; *oR = R; }
    return r;
}

int main (int argc, char** argv)
{
    const auto& in = info();
    int falhas = 0;
    std::printf ("%s: %d formas\n", in.nome, (int) in.formas.size());
    for (int f = 0; f < (int) in.formas.size(); ++f)
        for (int g = 0; g < 3; ++g)
            for (float v : { 0.0f, 0.5f, 1.0f })
                for (int ri : { 0, in.ratePadrao, 6 })
                    for (double sr : { 44100.0, 96000.0 })
                    {
                        const auto r = rodar (f, v, g, ri, sr, sr > 50000 ? 512 : 333);
                        const bool ruim = r.nan || r.pico > 4.0f || (v == 0.0f && ! in.mixSoma && r.dif > 0.02f);
                        if (ruim || (ri == in.ratePadrao && sr < 50000 && v == 1.0f))
                            std::printf ("  %-16s gat=%d v=%.1f rate=%-6s sr=%.0f  pico=%.2f rmsIn=%.3f rmsOut=%.3f dif=%.3f %s\n",
                                         in.formas[(size_t) f], g, v, in.rateNomes[(size_t) ri], sr, r.pico, r.rmsIn, r.rmsOut, r.dif,
                                         ruim ? "<<< FALHA" : "");
                        if (ruim) ++falhas;
                    }
    // o visor: a curva tem de ficar em 0..1 e não pode dar NaN
    for (int f = 0; f < (int) in.formas.size(); ++f)
        for (int ri = 0; ri < 7; ++ri)
        {
            const double per = in.periodoVisor (in.rates[(size_t) ri], 4.0);
            for (int k = 0; k <= 200; ++k)
            {
                const float y = in.desenho (f, 0.7f, per * k / 200.0 * 0.99999, in.rates[(size_t) ri], 4.0);
                if (! std::isfinite (y)) { std::printf ("  visor NaN forma %d\n", f); ++falhas; break; }
            }
        }
    if (argc > 1)
    {
        std::vector<float> L, R;
        rodar (in.formaPadrao, in.valorPadrao / 10.0f, gatInt, in.ratePadrao, 44100.0, 512, &L, &R);
        wav (argv[1], L, R, 44100);
    }
    std::printf ("%s: %s (%d falhas)\n", in.nome, falhas ? "FALHOU" : "ok", falhas);
    return falhas ? 1 : 0;
}
