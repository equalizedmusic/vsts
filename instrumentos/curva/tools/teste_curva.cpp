// Teste do motor da CURVA sem JUCE: c++ -std=c++17 -O2 -I../Source teste_curva.cpp -o teste_curva
// Confere: ISO 226 em 1 kHz = phon; valores conhecidos do contorno; FIR de fase mínima segue a curva pedida;
// loudness de seno de 1 kHz (-3,01 LUFS num canal a 0 dBFS); true peak de seno em fs/4 com fase de 45°.
#include "Curva.h"
#include <cstdio>

static int falhas = 0;
static void confere (bool ok, const char* o_que, double v)
{
    std::printf ("%s %-60s %8.2f\n", ok ? "ok  " : "FALHA", o_que, v);
    if (! ok) ++falhas;
}

int main()
{
    using namespace curva;
    for (double ph : { 20.0, 40.0, 60.0, 83.0, 90.0 })
        confere (std::abs (iso226::splTabela (17, ph) - ph) < 0.05, "ISO 226: 1 kHz soa como ele mesmo", iso226::splTabela (17, ph));
    // contorno de 40 phon em 100 Hz = 64,4 dB SPL; 80 phon em 100 Hz = 92,5 dB (tabela da ISO 226:2003)
    confere (std::abs (iso226::splTabela (7, 40) - 64.4) < 0.2, "40 phon em 100 Hz = 64,4 dB SPL", iso226::splTabela (7, 40));
    confere (std::abs (iso226::splTabela (7, 80) - 92.5) < 0.2, "80 phon em 100 Hz = 92,5 dB SPL", iso226::splTabela (7, 80));

    Ajuste a; a.listen = 50; a.target = 83; a.amount = 1;
    std::printf ("\ncorreção LISTEN 50 → TARGET 83 (dB):\n");
    for (double hz : { 10.0, 20.0, 31.5, 50.0, 100.0, 200.0, 500.0, 1000.0, 3150.0, 8000.0, 12500.0, 16000.0 })
        std::printf ("  %7.1f Hz  %+6.2f\n", hz, compensacao (hz, a));
    confere (compensacao (40, a) > 8, "ouvindo baixo: grave sobe", compensacao (40, a));
    confere (std::abs (compensacao (1000, a)) < 0.01, "1 kHz não muda", compensacao (1000, a));
    Ajuste igual; igual.listen = igual.target = 83;
    confere (std::abs (compensacao (50, igual)) < 1e-9, "LISTEN = TARGET: plano", compensacao (50, igual));
    std::printf ("  AUTO GAIN: %+.2f dB\n", autoGanhoDb (a));

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const auto h = faseMinima (fs, tapsPara (fs), [&a] (double hz) { return compensacao (hz, a); });
        double pior = 0, piorHz = 0;
        for (double hz = 25; hz < 16000; hz *= 1.05)
        {
            // abaixo de 40 Hz o FIR de ~85 ms tem menos resolução: tolera o dobro
            const double e = std::abs (respostaDb (h, fs, hz) - compensacao (hz, a)) / (hz < 40 ? 2.0 : 1.0);
            if (e > pior) { pior = e; piorHz = hz; }
        }
        char t[96]; std::snprintf (t, sizeof t, "FIR %d taps a %.0f Hz segue a curva (pior erro em %.0f Hz)", (int) h.size(), fs, piorHz);
        confere (pior < 0.5, t, pior);
        // fase mínima: a energia está no começo
        double e0 = 0, et = 0;
        for (size_t i = 0; i < h.size(); ++i) { et += h[i] * h[i]; if (i < h.size() / 8) e0 += h[i] * h[i]; }
        confere (e0 / et > 0.95, "fase mínima: 95% da energia no primeiro 1/8 (sem pré-eco)", e0 / et);
    }

    {
        const double fs = 48000;
        Loudness L; L.prepara (fs);
        std::vector<float> l (4800), r (4800, 0.0f);
        double ph = 0;
        for (int b = 0; b < 100; ++b)   // 10 s
        {
            for (auto& v : l) { v = (float) std::sin (ph); ph += 2 * pi * 997 / fs; }
            const float* ch[2] = { l.data(), r.data() };
            L.processa (ch, 2, 4800);
        }
        confere (std::abs (L.integrado.load() + 3.01) < 0.1, "seno 997 Hz 0 dBFS num canal = -3,01 LUFS (I)", L.integrado.load());
        confere (std::abs (L.momentary.load() + 3.01) < 0.1, "idem M", L.momentary.load());
        confere (std::abs (L.shortTerm.load() + 3.01) < 0.1, "idem S", L.shortTerm.load());
        confere (L.lra.load() < 0.5, "sinal constante: LRA ~0", L.lra.load());
    }
    {
        const double fs = 48000;
        TruePeak T;
        std::vector<float> x (4800);
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) std::sin (2 * pi * 12000 * i / fs + pi / 4);   // amostras em ±0,707
        const float* ch[1] = { x.data() };
        const float tp = T.processa (ch, 1, (int) x.size());
        confere (tp > 0.97 && tp < 1.03, "true peak de seno fs/4 com amostras em 0,707 ~ 1,0", tp);
    }
    std::printf ("\n%s (%d falha(s))\n", falhas ? "REPROVADO" : "APROVADO", falhas);
    return falhas ? 1 : 0;
}
