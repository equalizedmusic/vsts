// Mede o Dsp.h fora do plugin: harmônicas do PRÉ, curva do EQ e redução dos compressores.
// zig c++ -O2 -std=c++17 -target x86_64-windows-gnu teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe
#include "../Source/Dsp.h"
#include <cstdio>
#include <vector>
using namespace mesa;

static double amp (const std::vector<float>& x, double fs, double f)   // amplitude da senoide f (DFT de 1 ponto)
{
    double re = 0, im = 0;
    for (size_t i = 0; i < x.size(); ++i) { re += x[i] * std::cos (2 * pi * f * i / fs); im += x[i] * std::sin (2 * pi * f * i / fs); }
    return 2 * std::sqrt (re * re + im * im) / x.size();
}

int main()
{
    const double fs = 192000;   // o PRÉ roda sobreamostrado
    const char* nomes[] = { "LIMPO", "API 312", "BRIT 73" };
    for (int m = 0; m < 3; ++m)
        for (float lvl : { -18.0f, -6.0f, 0.0f })
            for (float drv : { 0.0f, 12.0f })
            {
                PreSat s; s.model = (PreModel) m; s.prepare (fs); s.drive = dbToGain (drv);
                for (double f0 : { 100.0, 1000.0 })
                {
                    std::vector<float> y; const double a = dbToGain (lvl);
                    for (int i = 0; i < (int) fs; ++i) { float v = s.process (0, (float) (a * std::sin (2 * pi * f0 * i / fs))); if (i >= fs / 2) y.push_back (v); }
                    const double h1 = amp (y, fs, f0), h2 = amp (y, fs, 2 * f0), h3 = amp (y, fs, 3 * f0);
                    std::printf ("%-8s in %+5.0f dBFS drive %+3.0f  %5.0f Hz: ganho %+5.2f dB  H2 %6.1f dB  H3 %6.1f dB\n", nomes[m], lvl, drv, f0,
                                 20 * std::log10 (h1 / a), 20 * std::log10 (h2 / h1 + 1e-12), 20 * std::log10 (h3 / h1 + 1e-12));
                }
            }

    const double fe = 48000;
    for (int m = 0; m < 2; ++m)
    {
        Eq e; EqSettings s; s.g[0] = 6; s.f[0] = 10000; s.g[1] = -6; s.f[1] = 3000; s.g[2] = 4; s.f[2] = 400; s.g[3] = 5; s.f[3] = 60;
        e.set (fe, (EqModel) m, s);
        std::printf ("EQ %s:", m ? "BRIT 81" : "E-4K");
        for (double f0 : { 30.0, 60.0, 150.0, 400.0, 1000.0, 3000.0, 10000.0, 16000.0 })
        {
            e.reset(); std::vector<float> y; const double a = 0.05;
            for (int i = 0; i < (int) fe; ++i) { float v = e.process (0, (float) (a * std::sin (2 * pi * f0 * i / fe))); if (i >= fe / 2) y.push_back (v); }
            std::printf ("  %g Hz %+5.1f", f0, 20 * std::log10 (amp (y, fe, f0) / a));
        }
        std::printf ("\n");
    }

    ChannelComp c; c.prepare (fe); c.thr = -20; c.ratio = 4;
    for (float lvl : { -30.0f, -20.0f, -10.0f, 0.0f })
    {
        float g = 1; for (int i = 0; i < (int) fe; ++i) g = c.gain (dbToGain (lvl));
        std::printf ("VCA thr -20 4:1  pico %+4.0f dBFS -> GR %5.2f dB\n", lvl, -gainToDb (g));
    }
    BusComp b; b.thr = -10; b.ratio = 4; b.prepare (fe);
    for (float lvl : { -20.0f, -10.0f, 0.0f })
    {
        float l = 0, r = 0;
        for (int i = 0; i < (int) fe * 2; ++i) { l = r = dbToGain (lvl); b.process (l, r); }
        std::printf ("COLA thr -10 4:1 AUTO  %+4.0f dBFS -> GR %5.2f dB\n", lvl, b.grDb);
    }
    return 0;
}
