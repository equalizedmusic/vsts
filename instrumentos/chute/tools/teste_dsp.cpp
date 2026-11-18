// Bancada do motor do CHUTE (sem JUCE). Compila com Source/Dsp.h + Source/Modelos.h (gerado pelo modelos.cmake, que precisa da chave):
//   zig c++ -std=c++17 -O2 -I ../Source teste_dsp.cpp -o teste_dsp.exe
//   teste_dsp.exe <pasta de saída>
// Grava em <saída>/modelos_44k.f32 os presets de fábrica (modo PRESET, 450 ms cada, 44,1 kHz) para ouvir/conferir,
// e confere os knobs (nota da cauda, DECAY, CHOKE, LINK).
#include "Dsp.h"
#include "../Source/ChuteLink.h"
#include <cstdio>
#include <vector>
#include <string>

using namespace chute;

static int falhas = 0;
static void confere (bool ok, const char* o_que) { std::printf ("%s  %s\n", ok ? "ok  " : "FALHA", o_que); if (! ok) ++falhas; }

// frequência da cauda pelos cruzamentos por zero entre t1 e t2 (s)
static double freqEntre (const std::vector<float>& x, double fs, double t1, double t2)
{
    int n0 = -1, n1 = -1, conta = 0;
    double z0 = 0, z1 = 0;
    for (int i = (int) (t1 * fs) + 1; i < (int) (t2 * fs) && i < (int) x.size(); ++i)
        if (x[i - 1] <= 0 && x[i] > 0)
        {
            const double z = i - 1 + (-x[i - 1]) / (x[i] - x[i - 1]);
            if (n0 < 0) { n0 = i; z0 = z; } else { n1 = i; z1 = z; ++conta; }
        }
    return conta > 0 ? fs * conta / (z1 - z0) : 0.0;
}

static std::vector<float> toca (const Ajustes& a, int nota, double fs, double seg, float vel = 1.0f)
{
    Engine e; e.prepare (fs);
    std::vector<float> out ((size_t) (seg * fs), 0.0f);
    e.noteOn (nota, vel, a);
    float* p = out.data();
    e.render (&p, 1, 0, (int) out.size(), a);
    return out;
}

int main (int argc, char** argv)
{
    const std::string saida = argc > 1 ? argv[1] : ".";
    // 1) os presets de fábrica
    {
        std::FILE* f = std::fopen ((saida + "/modelos_44k.f32").c_str(), "wb");
        for (int m = 0; m < numModelos; ++m)
        {
            Ajustes a; padraoDoModelo (a, m);
            a.modo = modoPreset; a.volumeDb = 0.0f; a.velo = 0.0f;
            auto x = toca (a, -1, 44100.0, 0.45);
            std::fwrite (x.data(), sizeof (float), x.size(), f);
        }
        std::fclose (f);
        std::printf ("modelos_44k.f32 gravado (%d × 450 ms)\n", numModelos);
    }
    // 2) nota da cauda: PRESET = a do modelo; KEY·MIDI = a classe da nota tocada; TUNE soma semitons
    {
        Ajustes a; padraoDoModelo (a, 19);   // MARTELO, F1 (43,6 Hz)
        a.modo = modoPreset;
        const double f0 = freqEntre (toca (a, -1, 48000.0, 0.4), 48000.0, 0.15, 0.22);
        confere (std::fabs (f0 / modelos[19].fEnd - 1.0) < 0.04, ("PRESET: cauda " + std::to_string (f0) + " Hz").c_str());
        a.modo = modoKey;
        const double fg = freqEntre (toca (a, 43, 48000.0, 0.4), 48000.0, 0.15, 0.22);   // G (qualquer oitava) → G1 49 Hz
        confere (std::fabs (fg / mtof (31) - 1.0) < 0.04, ("KEY·MIDI G: cauda " + std::to_string (fg) + " Hz (G1 = 49,0)").c_str());
        a.modo = modoFixed; a.tune = -2.0f;
        const double fd = freqEntre (toca (a, 60, 48000.0, 0.4), 48000.0, 0.15, 0.22);   // F1 − 2 = D#1 38,9 Hz
        confere (std::fabs (fd / mtof (27) - 1.0) < 0.04, ("FIXED −2: cauda " + std::to_string (fd) + " Hz (D#1 = 38,9)").c_str());
    }
    // 3) DECAY dobra o comprimento; TAIL −40 encurta
    {
        auto fim = [] (const std::vector<float>& x, double fs)
        {
            float pk = 0; for (float v : x) pk = std::max (pk, std::fabs (v));
            int u = 0; for (int i = 0; i < (int) x.size(); ++i) if (std::fabs (x[i]) > pk * 0.1f) u = i;
            return u / fs * 1000.0;
        };
        Ajustes a; padraoDoModelo (a, 19); a.modo = modoPreset;
        const double base = fim (toca (a, -1, 48000.0, 1.2), 48000.0);
        a.decayMs *= 2.0f;
        const double dobro = fim (toca (a, -1, 48000.0, 1.2), 48000.0);
        confere (dobro > base * 1.6, ("DECAY ×2: " + std::to_string ((int) base) + " → " + std::to_string ((int) dobro) + " ms").c_str());
        padraoDoModelo (a, 19); a.modo = modoPreset; a.tailDb = -40.0f;
        const double curto = fim (toca (a, -1, 48000.0, 1.2), 48000.0);
        confere (curto < base * 0.9, ("TAIL −40: " + std::to_string ((int) curto) + " ms").c_str());
    }
    // 4) CHOKE corta a anterior (depois de 3 ms sobra só a batida nova); POLY deixa as duas soando
    {
        Ajustes a; padraoDoModelo (a, 19); a.modo = modoPreset;
        std::vector<float> so (48000, 0.0f);
        { Engine e; e.prepare (48000.0); float* p = so.data() + 2400; e.noteOn (-1, 1.0f, a); e.render (&p, 1, 0, 48000 - 2400, a); }
        for (int poly = 0; poly < 2; ++poly)
        {
            a.poly = poly == 1;
            Engine e; e.prepare (48000.0);
            std::vector<float> o (48000, 0.0f); float* p = o.data();
            e.noteOn (-1, 1.0f, a); e.render (&p, 1, 0, 2400, a);   // 50 ms
            e.noteOn (-1, 1.0f, a); e.render (&p, 1, 2400, 48000 - 2400, a);
            double err = 0, en = 0;
            for (int i = 2400 + 200; i < 48000; ++i) { err += (o[i] - so[i]) * (o[i] - so[i]); en += so[i] * (double) so[i]; }
            const double db = 10 * std::log10 (err / en + 1e-30);
            if (poly) confere (db > -20.0, ("POLY: a 1ª batida segue soando (" + std::to_string (db) + " dB)").c_str());
            else      confere (db < -60.0, ("CHOKE: sobra só a batida nova (" + std::to_string (db) + " dB)").c_str());
        }
    }
    // 5) LINK: a fase publicada recria o corpo igual ao motor (CLICK desligado)
    {
        Ajustes a; padraoDoModelo (a, 19); a.modo = modoPreset; a.clickOn = false; a.volumeDb = 0.0f; a.velo = 0.0f;
        Engine e; e.prepare (48000.0);
        std::vector<float> o (24000, 0.0f); float* p = o.data();
        const Voz& v = e.noteOn (-1, 1.0f, a);
        chutelink::Local l;
        l.len = v.len; l.fs = 48000.0f; l.sinal = (float) v.sinal; l.t0 = (float) v.t0;
        for (int k = 0; k < v.len; ++k) l.fase[k] = v.ph[k];
        for (int k = 0; k < v.len; ++k) l.amp[k] = v.am[k];
        l.valida = true;
        e.render (&p, 1, 0, 24000, a);
        double err = 0, en = 0;
        for (int i = 0; i < 24000; ++i) { const double r = l.corpo (i / 48000.0); err += (r - o[i]) * (r - o[i]); en += o[i] * (double) o[i]; }
        const double db = 10 * std::log10 (err / en + 1e-30);
        confere (db < -40.0, ("LINK recria o corpo: erro " + std::to_string (db) + " dB").c_str());
    }
    std::printf (falhas ? "\n%d FALHA(S)\n" : "\ntudo ok\n", falhas);
    return falhas ? 1 : 0;
}
