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
    // senoide em 0 VU (-18 dBFS RMS = pico -15) e +6 VU; DRIVE 0, 3, 5, 7, 10
    for (int m = 0; m < 3; ++m)
        for (float lvl : { -15.0f, -9.0f })
            for (float drv : { 0.0f, 3.0f, 5.0f, 7.0f, 10.0f })
            {
                PreSat s; s.model = (PreModel) m; s.prepare (fs); s.setDrive (drv);
                for (double f0 : { 80.0, 1000.0 })
                {
                    s.reset();
                    std::vector<float> y; const double a = dbToGain (lvl);
                    for (int i = 0; i < (int) fs; ++i) { float v = s.process (0, (float) (a * std::sin (2 * pi * f0 * i / fs))); if (i >= fs / 2) y.push_back (v); }
                    const double h1 = amp (y, fs, f0);
                    double thd = 0; for (int k = 2; k <= 7; ++k) { const double hk = amp (y, fs, k * f0); thd += hk * hk; }
                    std::printf ("%-8s pico %+4.0f drive %4.1f %5.0f Hz: ganho %+5.2f dB  H2 %6.1f  H3 %6.1f  THD %6.2f%%\n", nomes[m], lvl, drv, f0,
                                 20 * std::log10 (h1 / a), 20 * std::log10 (amp (y, fs, 2 * f0) / h1 + 1e-12),
                                 20 * std::log10 (amp (y, fs, 3 * f0) / h1 + 1e-12), 100 * std::sqrt (thd) / h1);
                }
            }

    // EQ: digital (casado) x circuito analógico, a 48 kHz
    const double fe = 48000;
    auto analog = [] (double f, double f0, double n2, double n1, double n0, double d2, double d1, double d0)
    {
        const double o = f / f0, nr = n0 - n2 * o * o, ni = n1 * o, dr = d0 - d2 * o * o, di = d1 * o;
        return 10 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di));
    };
    auto resp = [&] (Biquad& b, double f)
    {
        const double w = 2 * pi * f / fe;
        const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        const double nr = b.b0 + b.b1 * cr + b.b2 * c2r, ni = b.b1 * ci + b.b2 * c2i;
        const double dr = 1 + b.a1 * cr + b.a2 * c2r, di = b.a1 * ci + b.a2 * c2i;
        return 10 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di));
    };
    struct T { const char* n; int tipo; double f0, q, db; };
    for (T t : { T { "sino 1k Q1 +12", 0, 1000, 1, 12 }, T { "sino 7k Q2 +12", 0, 7000, 2, 12 }, T { "sino 7k Q1 -12", 0, 7000, 1, -12 },
                 T { "shelf alto 16k +10", 2, 16000, 0.8, 10 }, T { "shelf alto 8k -8", 2, 8000, 0.8, -8 }, T { "shelf baixo 60 +10", 1, 60, 0.8, 10 } })
    {
        Biquad b; const double A = std::pow (10.0, t.db / 40.0), sA = std::sqrt (A);
        double n2, n1, n0, d2, d1, d0;
        if (t.tipo == 0)      { b.peakM (fe, t.f0, t.q, t.db);      n2 = 1; n1 = A / t.q; n0 = 1; d2 = 1; d1 = 1 / (A * t.q); d0 = 1; }
        else if (t.tipo == 1) { b.lowShelfM (fe, t.f0, t.q, t.db);  n2 = A; n1 = A * sA / t.q; n0 = A * A; d2 = A; d1 = sA / t.q; d0 = 1; }
        else                  { b.highShelfM (fe, t.f0, t.q, t.db); n2 = A * A; n1 = A * sA / t.q; n0 = A; d2 = 1; d1 = sA / t.q; d0 = A; }
        Biquad r; if (t.tipo == 0) r.peak (fe, t.f0, t.q, t.db); else if (t.tipo == 1) r.lowShelf (fe, t.f0, t.q, t.db); else r.highShelf (fe, t.f0, t.q, t.db);
        std::printf ("%-20s", t.n);
        double em = 0, er = 0;
        for (double f = 20; f < 23900; f *= 1.05)
        {
            const double a = analog (f, t.f0, n2, n1, n0, d2, d1, d0);
            em = std::max (em, std::abs (resp (b, f) - a)); er = std::max (er, std::abs (resp (r, f) - a));
        }
        std::printf (" erro max casado %5.2f dB | bilinear %5.2f dB   @16k: analog %+5.1f casado %+5.1f bilinear %+5.1f\n", em, er,
                     analog (16000, t.f0, n2, n1, n0, d2, d1, d0), resp (b, 16000), resp (r, 16000));
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
