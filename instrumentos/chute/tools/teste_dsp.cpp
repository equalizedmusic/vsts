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
    // 4) kick mono: a batida nova corta a anterior (depois de 3 ms sobra só a batida nova)
    {
        Ajustes a; padraoDoModelo (a, 19); a.modo = modoPreset;
        std::vector<float> so (48000, 0.0f);
        { Engine e; e.prepare (48000.0); float* p = so.data() + 2400; e.noteOn (-1, 1.0f, a); e.render (&p, 1, 0, 48000 - 2400, a); }
        Engine e; e.prepare (48000.0);
        std::vector<float> o (48000, 0.0f); float* p = o.data();
        e.noteOn (-1, 1.0f, a); e.render (&p, 1, 0, 2400, a);   // 50 ms
        e.noteOn (-1, 1.0f, a); e.render (&p, 1, 2400, 48000 - 2400, a);
        double err = 0, en = 0;
        for (int i = 2400 + 200; i < 48000; ++i) { err += (o[i] - so[i]) * (o[i] - so[i]); en += so[i] * (double) so[i]; }
        const double db = 10 * std::log10 (err / en + 1e-30);
        confere (db < -60.0, ("CHOKE: sobra só a batida nova (" + std::to_string (db) + " dB)").c_str());
    }
    // 4b) DRIVE: girar o knob não estala; TONE com DRIVE 0 não muda nada; TONE fecha só o agudo da saturação
    {
        // blocos de 256 amostras, o knob mudando de 1% em 1% a cada bloco (como a mão girando)
        auto rendeDrive = [] (Ajustes a, int tipo, int modoDrive, std::vector<float>& o)
        {
            a.driveTipo = tipo;
            Engine e; e.prepare (48000.0);
            o.assign (48000, 0.0f);
            e.noteOn (-1, 1.0f, a);
            for (int b = 0; b < 48000; b += 256)
            {
                if (modoDrive == 0) a.drive = 0.0f;
                else if (modoDrive == 1) a.drive = 1.0f;
                else a.drive = std::min (1.0f, std::floor (b / 256.0f) * 0.01f);   // 0 → 100% em ~0,5 s
                float* p = o.data();
                e.render (&p, 1, b, std::min (256, 48000 - b), a);
            }
        };
        // "aspereza" = maior 2ª diferença depois do ataque (a 1ª batida do corpo já passou)
        auto aspero = [] (const std::vector<float>& o)
        {
            double m = 0;
            for (int i = 2400; i < (int) o.size(); ++i) m = std::max (m, (double) std::fabs (o[i] - 2 * o[i - 1] + o[i - 2]));
            return m;
        };
        Ajustes a; padraoDoModelo (a, 37); a.modo = modoPreset; a.decayMs = 900.0f; a.volumeDb = 0.0f; a.velo = 0.0f;
        const char* nomes[] = { "WARM", "HARD", "FOLD" };
        for (int tipo = 0; tipo < 3; ++tipo)
        {
            std::vector<float> o0, o1, og;
            rendeDrive (a, tipo, 0, o0); rendeDrive (a, tipo, 1, o1); rendeDrive (a, tipo, 2, og);
            const double ref = std::max (aspero (o0), aspero (o1)), gira = aspero (og);
            confere (gira <= ref * 1.15, (std::string ("DRIVE ") + nomes[tipo] + " girando 0→100%: aspereza " + std::to_string (gira)
                                          + " (parado: " + std::to_string (ref) + ")").c_str());
        }
        // quanto a saturação muda o som (energia de saturado − limpo, relativa ao limpo), para comparar versões
        for (int tipo = 0; tipo < 3; ++tipo)
        {
            Ajustes b = a; b.driveTipo = tipo; b.drive = 0.0f;
            const auto limpo = toca (b, -1, 48000.0, 0.6);
            std::printf ("      %s:", nomes[tipo]);
            for (float d : { 0.25f, 0.5f, 1.0f })
            {
                b.drive = d;
                const auto x = toca (b, -1, 48000.0, 0.6);
                double e = 0, en = 0;
                for (size_t i = 0; i < x.size(); ++i) { e += (x[i] - limpo[i]) * (double) (x[i] - limpo[i]); en += limpo[i] * (double) limpo[i]; }
                std::printf ("  %d%% %+.1f dB", (int) (d * 100), 10 * std::log10 (e / en + 1e-30));
            }
            std::printf ("\n");
        }
        // TONE com DRIVE 0: idêntico ao TONE aberto
        {
            Ajustes b = a; b.drive = 0.0f; b.toneHz = 20000.0f;
            const auto x = toca (b, -1, 48000.0, 0.6);
            b.toneHz = 1000.0f;
            const auto y = toca (b, -1, 48000.0, 0.6);
            double d = 0; for (size_t i = 0; i < x.size(); ++i) d = std::max (d, (double) std::fabs (x[i] - y[i]));
            confere (d < 1.0e-6, ("TONE 1 kHz com DRIVE 0 não muda o som (diferença " + std::to_string (d) + ")").c_str());
        }
        // TONE com DRIVE 60%: o grave (< 150 Hz) fica igual ao TONE aberto e o agudo (> 3 kHz) cai
        {
            auto banda = [] (const std::vector<float>& x, double fc, bool alta)
            {
                const double c = 1.0 - std::exp (-2.0 * 3.141592653589793 * fc / 48000.0);
                double l[4] {}, en = 0;
                for (float v : x)
                {
                    double s = v;
                    for (auto& e : l) { e += c * (s - e); s = alta ? s - e : e; }   // 4 polos (24 dB/oitava)
                    en += s * s;
                }
                return 10 * std::log10 (en + 1e-30);
            };
            Ajustes b = a; b.drive = 0.0f; b.driveTipo = driveHard;
            const auto limpo = toca (b, -1, 48000.0, 0.6);
            b.drive = 0.6f; b.toneHz = 20000.0f;
            const auto x = toca (b, -1, 48000.0, 0.6);
            b.toneHz = 1500.0f;
            const auto y = toca (b, -1, 48000.0, 0.6);
            std::vector<float> sx (x.size()), sy (y.size());   // o que a saturação acrescenta
            for (size_t i = 0; i < x.size(); ++i) { sx[i] = x[i] - limpo[i]; sy[i] = y[i] - limpo[i]; }
            const double dg = banda (y, 150, false) - banda (x, 150, false), da = banda (sy, 3000, true) - banda (sx, 3000, true);
            const double dl = banda (y, 3000, true) - banda (limpo, 3000, true);
            confere (std::fabs (dg) < 0.5, ("TONE 1,5 kHz (DRIVE 60%): grave muda " + std::to_string (dg) + " dB").c_str());
            confere (da < -12.0, ("TONE 1,5 kHz (DRIVE 60%): agudo da saturação cai " + std::to_string (da) + " dB").c_str());
            std::printf ("      (agudo da saída com TONE 1,5 kHz vs. kick limpo: %+.1f dB: o click do preset fica)\n", dl);
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
