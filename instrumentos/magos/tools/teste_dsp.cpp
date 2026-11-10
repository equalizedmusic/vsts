// Bancada do MASTER DOS MAGOS: compila só o Dsp.h (sem JUCE) e confere o motor com sinais sintéticos.
//   zig c++ -O2 -std=c++17 tools/teste_dsp.cpp -o teste_dsp.exe && ./teste_dsp.exe
#include "../Source/Dsp.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace magos;

static int falhas = 0;
static void confere (bool ok, const char* oque, double v)
{
    std::printf ("  [%s] %-58s %8.2f\n", ok ? " ok " : "FALHA", oque, v);
    if (! ok) ++falhas;
}

// um drop de psy sintético (kick, baixo rolante, hats, pad), com "lama" em 250 Hz opcional
struct Musica
{
    double fs, t = 0, phK = 0, phB = 0, phP = 0, lp = 0, z = 0;
    std::mt19937 rng { 7 };
    float lama;
    Musica (double f, float l) : fs (f), lama (l) {}
    void gera (float* L, float* R, int n, float ganho)
    {
        std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        const double bpm = 145.0, pi2 = 2 * kPi;
        for (int i = 0; i < n; ++i, t += 1.0 / fs)
        {
            const double beat = t * bpm / 60.0, tb = (beat - std::floor (beat)) * 60.0 / bpm;
            const double six = beat * 4.0, t16 = (six - std::floor (six)) * 15.0 / bpm;
            const int k16 = (int) std::floor (six) % 4;
            if (tb < 1.0 / fs) phK = 0;
            phK += pi2 * (48.0 + 120.0 * std::exp (-tb * 60.0)) / fs;
            const double kick = std::sin (phK) * std::exp (-tb * 9.0) * 0.8;
            phB += 49.0 / fs;
            const double saw = 2.0 * (phB - std::floor (phB)) - 1.0;
            lp += 0.08 * (saw - lp);
            const double bass = k16 == 0 ? 0.0 : lp * std::exp (-t16 * 16.0) * 0.7;
            const double nn = u (rng), h = nn - z; z = nn;
            const double hat = std::fmod (beat, 1.0) >= 0.5 ? h * std::exp (-std::fmod (beat, 0.5) * 60.0 / bpm * 50.0) * 0.12 : 0.0;
            phP += 250.0 / fs;
            const double mud = lama * std::sin (pi2 * phP) * (0.6 + 0.4 * std::sin (pi2 * t * 0.5));
            double pad = 0;
            for (int k = 1; k <= 5; ++k) pad += std::sin (pi2 * t * 370.0 * k) / (k * 1.5);
            const double s = kick + bass + hat + mud + pad * 0.05;
            L[i] = (float) (s * ganho);
            R[i] = (float) ((s + h * 0.05) * ganho);
        }
    }
};

int main()
{
    const double fs = 48000.0;
    const int B = 512;

    std::printf ("1) LUFS: seno de 997 Hz a -20 dBFS nos 2 canais = -20,0 LUFS (BS.1770: 0 dBFS em 1 canal = -3,01)\n");
    {
        Loudness ld; ld.prepare (fs);
        std::vector<float> l (B);
        double ph = 0;
        for (int k = 0; k < (int) (fs * 10 / B); ++k)
        {
            for (int i = 0; i < B; ++i) { l[(size_t) i] = (float) (0.1 * std::sin (ph)); ph += 2 * kPi * 997.0 / fs; }
            const float* c[2] = { l.data(), l.data() };
            ld.processa (c, 2, B);
        }
        confere (std::abs (ld.integrado + 20.0f) < 0.1f, "integrado (LUFS)", ld.integrado);
        confere (std::abs (ld.curto + 20.0f) < 0.1f, "curto (LUFS)", ld.curto);
    }

    std::printf ("2) TRUE PEAK: seno em fs/4 defasado 45 graus (amostras em 0,707) tem pico real 1,0\n");
    {
        TruePeak tp;
        float mx = 0, mxAmostra = 0;
        for (int i = 0; i < 4000; ++i)
        {
            const float v = (float) std::sin (2 * kPi * (fs / 4) * i / fs + kPi / 4);
            mxAmostra = std::max (mxAmostra, std::abs (v));
            mx = std::max (mx, tp.empurra (v, v, 2));
        }
        confere (mxAmostra < 0.72f, "pico de amostra (lin)", mxAmostra);
        confere (mx > 0.95f && mx < 1.05f, "true peak medido (lin)", mx);
    }

    std::printf ("3) Antes do CAST: multibanda soma plana (ruído branco a -30 dBFS, sem limitar)\n");
    {
        Engine e; e.prepare (fs);
        Ajustes a; a.monoBass = false; a.width = 1.0f;
        std::mt19937 rng (3);
        std::normal_distribution<float> g (0.0f, 0.03f);
        std::vector<float> L (B), R (B), entL, saiL;
        for (int k = 0; k < (int) (fs * 8 / B); ++k)
        {
            for (int i = 0; i < B; ++i) { L[(size_t) i] = g (rng); R[(size_t) i] = L[(size_t) i]; }
            entL.insert (entL.end(), L.begin(), L.end());
            float* ch[2] = { L.data(), R.data() };
            e.process (ch, 2, B, a);
            saiL.insert (saiL.end(), L.begin(), L.end());
        }
        const auto ai = analisa (entL.data(), entL.data(), (int64_t) entL.size(), fs);
        const auto ao = analisa (saiL.data(), saiL.data(), (int64_t) saiL.size(), fs);
        float pior = 0;
        for (int b = 1; b < NB - 1; ++b) pior = std::max (pior, std::abs (dbDe (ao.espectro[b]) - dbDe (ai.espectro[b])));
        confere (pior < 0.5f, "maior desvio por banda 50 Hz-12,7 kHz (dB)", pior);
        confere (std::abs (ao.lufs - ai.lufs) < 0.2f, "LUFS saida - entrada (dB)", ao.lufs - ai.lufs);
        std::printf ("     latência = %d amostras (%.2f ms)\n", e.latenciaAmostras(), e.latenciaAmostras() * 1000.0 / fs);
    }

    std::printf ("4) BYPASS = entrada atrasada exatamente da latência\n");
    {
        Engine e; e.prepare (fs);
        Ajustes a; a.ouvir = 3;
        std::vector<float> L (B, 0.0f), R (B, 0.0f);
        L[10] = R[10] = 0.5f;
        float* ch[2] = { L.data(), R.data() };
        e.process (ch, 2, B, a);
        int onde = -1;
        for (int i = 0; i < B; ++i) if (std::abs (L[(size_t) i]) > 0.4f) onde = i;
        confere (onde == 10 + e.latenciaAmostras(), "posição do impulso (amostra)", onde);
    }

    std::printf ("5) LISTEN 15 s -> CAST -> 30 s: música a ~-17 LUFS com lama em 250 Hz, STYLE PSYTRANCE\n");
    {
        Engine e; e.prepare (fs);
        e.style = 0;
        Ajustes a;
        Musica mu (fs, 0.25f);
        std::vector<float> L (B), R (B), saiL, saiR, entL, entR;
        float tpMax = 0;
        e.listen();
        for (int k = 0; k < (int) (fs * 15 / B); ++k)
        {
            mu.gera (L.data(), R.data(), B, 0.45f);
            entL.insert (entL.end(), L.begin(), L.end()); entR.insert (entR.end(), R.begin(), R.end());
            float* ch[2] = { L.data(), R.data() };
            e.process (ch, 2, B, a);
        }
        std::printf ("     LISTEN: %.1f s, entrada %.1f LUFS\n", e.aprSegundos.load(), e.lufsIn.load());
        e.lancaCast();
        const auto t0 = std::chrono::steady_clock::now();
        const int nBlocos = (int) (fs * 30 / B);
        for (int k = 0; k < nBlocos; ++k)
        {
            mu.gera (L.data(), R.data(), B, 0.45f);
            float* ch[2] = { L.data(), R.data() };
            e.process (ch, 2, B, a);
            if (k > nBlocos / 2)
            {
                saiL.insert (saiL.end(), L.begin(), L.end()); saiR.insert (saiR.end(), R.begin(), R.end());
                for (int i = 0; i < B; ++i) tpMax = std::max (tpMax, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
            }
        }
        const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const auto& f = e.feitico;
        std::printf ("     CAST: alvo %.1f LUFS, drive %.1f dB, modo %d; TONE:", f.alvoLufs, f.drive, e.modo.load());
        for (int k = 0; k < NE; ++k) std::printf (" %+.1f", f.tom[k]);
        std::printf ("\n     limiares:");
        for (int m = 0; m < NM; ++m) std::printf (" %.1f", f.limiar[m]);
        std::printf ("  | DYN EQ agora:");
        for (int z = 0; z < NZ; ++z) std::printf (" %s %+.1f", zona (z).nome, e.zonaDb[z].load());
        std::printf ("\n");
        const auto ao = analisa (saiL.data(), saiR.data(), (int64_t) saiL.size(), fs);
        const auto ai = analisa (entL.data(), entR.data(), (int64_t) entL.size(), fs);
        std::printf ("     limitador: redução %.1f dB; freio do alvo alto: %s\n", e.grLim.load(), e.alvoAlto.load() ? "SIM (parou antes do alvo)" : "não");
        confere (std::abs (ao.lufs - f.alvoLufs) < 0.6f || (e.alvoAlto.load() && e.grLim.load() > -7.5f), "LUFS da saída - alvo (ou freio com GR <= 5 dB)", ao.lufs - f.alvoLufs);
        confere (ao.picoTp <= kTeto + 0.15f, "true peak da saída (dBTP)", ao.picoTp);
        confere (20.0f * std::log10 (tpMax) <= kTeto + 0.001f, "pico de amostra da saída (dBFS)", 20.0f * std::log10 (tpMax));
        float fi[NB], fo[NB], alvo[NB];
        formaDb (ai.espectro, fi); formaDb (ao.espectro, fo);
        e.formaAlvo (alvo);
        const float ci = casamento (fi, alvo), co = casamento (fo, alvo);
        std::printf ("     MATCH com o alvo: antes %.0f %% -> depois %.0f %%\n", ci, co);
        confere (co > ci + 5.0f, "MATCH melhorou (pontos)", co - ci);
        confere (f.tom[2] < -0.5f, "TONE corta a lama (sino de 200 Hz, dB)", f.tom[2]);
        const double rt = 30.0 / seg;
        confere (rt > 20.0, "velocidade (x tempo real, 1 núcleo)", rt);
    }

    std::printf ("5b) Mesmo drop com TARGET -9 LUFS: tem de chegar sem o freio\n");
    {
        Engine e; e.prepare (fs);
        e.alvoOpcao = 3;
        Ajustes a;
        Musica mu (fs, 0.25f);
        std::vector<float> L (B), R (B), sL, sR;
        e.listen();
        for (int k = 0; k < (int) (fs * 12 / B); ++k) { mu.gera (L.data(), R.data(), B, 0.45f); float* ch[2] = { L.data(), R.data() }; e.process (ch, 2, B, a); }
        e.lancaCast();
        for (int k = 0; k < (int) (fs * 34 / B); ++k)
        {
            mu.gera (L.data(), R.data(), B, 0.45f); float* ch[2] = { L.data(), R.data() }; e.process (ch, 2, B, a);
            if (k > (int) (fs * 26 / B)) { sL.insert (sL.end(), L.begin(), L.end()); sR.insert (sR.end(), R.begin(), R.end()); }
        }
        const auto ao = analisa (sL.data(), sR.data(), (int64_t) sL.size(), fs);
        std::printf ("     drive %.1f dB, limitador %.1f dB, freio %s\n", e.feitico.drive, e.grLim.load(), e.alvoAlto.load() ? "SIM" : "não");
        confere (std::abs (ao.lufs + 9.0f) < 0.5f, "LUFS da saída - (-9) (dB)", ao.lufs + 9.0f);
        confere (ao.picoTp <= kTeto + 0.15f, "true peak (dBTP)", ao.picoTp);
    }

    std::printf ("6) Igualador do A/B: B no mesmo loudness da saída\n");
    {
        Engine e; e.prepare (fs);
        Ajustes a;
        Musica mu (fs, 0.0f);
        std::vector<float> L (B), R (B), bL, bR;
        e.listen();
        for (int k = 0; k < (int) (fs * 8 / B); ++k) { mu.gera (L.data(), R.data(), B, 0.18f); float* ch[2] = { L.data(), R.data() }; e.process (ch, 2, B, a); }
        e.lancaCast();
        for (int k = 0; k < (int) (fs * 25 / B); ++k) { mu.gera (L.data(), R.data(), B, 0.18f); float* ch[2] = { L.data(), R.data() }; e.process (ch, 2, B, a); }
        const float saida = e.curtoOut.load();
        a.ouvir = 1;
        for (int k = 0; k < (int) (fs * 6 / B); ++k)
        {
            mu.gera (L.data(), R.data(), B, 0.18f);
            float* ch[2] = { L.data(), R.data() };
            e.process (ch, 2, B, a);
            if (k > (int) (fs * 3 / B)) { bL.insert (bL.end(), L.begin(), L.end()); bR.insert (bR.end(), R.begin(), R.end()); }
        }
        Loudness ld; ld.prepare (fs);
        const float* c[2] = { bL.data(), bR.data() };
        ld.processa (c, 2, (int) bL.size());
        confere (std::abs (ld.integrado - saida) < 1.0f, "B (LUFS) - saída (LUFS)", ld.integrado - saida);
    }

    std::printf ("\n%s (%d falha%s)\n", falhas == 0 ? "TUDO OK" : "HÁ FALHAS", falhas, falhas == 1 ? "" : "s");
    return falhas == 0 ? 0 : 1;
}
