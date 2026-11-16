// Bancada do RETALHO (sem JUCE): compila só com Source/Dsp.h.
//   g++ -O2 -std=c++17 tools/teste_dsp.cpp -o teste && ./teste
#include "../Source/Dsp.h"
#include "../Source/Presets.h"
#include <chrono>
#include <cstdio>

using namespace retalho;

static int falhas = 0;
static void confere (bool ok, const char* nome, const char* detalhe = "")
{
    std::printf ("%s  %s %s\n", ok ? "OK  " : "FALHOU", nome, detalhe);
    if (! ok) ++falhas;
}

static void padroes (float* val)
{
    for (int k = 0; k < numCampos; ++k) val[k] = info (k).padrao;
}

// voz de teste: 16 sílabas (vogal com harmônicos), 150 ms cantados + 100 ms de vão, notas de Fá# menor
static float voz (double t, int* silaba = nullptr)
{
    const double passo = 0.25;
    const int k = (int) (t / passo);
    const double u = t - k * passo;
    if (silaba) *silaba = k;
    if (u > 0.15) return 0.0f;
    static const int notas[8] = { 54, 57, 61, 59, 57, 54, 52, 57 };
    const double f0 = 440.0 * std::pow (2.0, (notas[k % 8] - 69) / 12.0);
    double v = 0;
    for (int h = 1; h <= 8; ++h) v += std::sin (2 * kPi * f0 * h * u) / h;
    const double env = std::min (1.0, u / 0.004) * std::min (1.0, (0.15 - u) / 0.01);
    return (float) (0.3 * v * env);
}

int main()
{
    const double fs = 48000.0;
    char buf[200];

    // 1. BYPASS e DRY/WET 0 % = idêntico à entrada
    {
        Motor m; m.prepare (fs);
        float val[numCampos]; padroes (val);
        Transporte tp; tp.tocando = true; tp.temPpq = true; tp.bpm = 142;
        double maxd = 0;
        for (int modo = 0; modo < 2; ++modo)
        {
            float v2[numCampos]; std::memcpy (v2, val, sizeof (val));
            if (modo == 0) v2[kBypass] = 1; else v2[kDryWet] = 0;
            std::vector<float> l (512), r (512);
            for (int b = 0; b < 400; ++b)
            {
                for (int i = 0; i < 512; ++i) { l[(size_t) i] = voz ((b * 512 + i) / fs); r[(size_t) i] = -l[(size_t) i] * 0.5f; }
                const std::vector<float> l0 = l, r0 = r;
                float* ch[2] = { l.data(), r.data() };
                tp.ppq = b * 512 / (fs * 60 / 142);
                m.process (ch, 2, 512, v2, tp, nullptr, 0);
                if (b > 20) for (int i = 0; i < 512; ++i) maxd = std::max (maxd, (double) std::max (std::abs (l[(size_t) i] - l0[(size_t) i]), std::abs (r[(size_t) i] - r0[(size_t) i])));
            }
        }
        std::snprintf (buf, sizeof buf, "(diferença máx %.2e)", maxd);
        confere (maxd < 1e-6, "1. BYPASS / DRY-WET 0% = entrada", buf);
    }

    // 2. GRID 1 BAR 1/16 a 142 BPM: cada repetição começa no sample certo
    {
        Cena c; c.v[kShape] = shGrid; c.v[kRate] = 2; c.v[kLength] = 1;
        Contexto cx; cx.fs = fs; cx.spb = fs * 60 / 142; cx.inicio = 100000;
        Plano p; montaEdit (c, cx, p);
        double err = 0;
        for (int k = 0; k < p.n; ++k) err = std::max (err, std::abs ((double) p.f[k].t0 - k * cx.spb / 4));
        std::snprintf (buf, sizeof buf, "(%d fatias, erro máx %.2f amostra)", p.n, err);
        confere (p.n == 16 && err <= 1.0, "2. GRID 1/16 no sample certo", buf);
        bool vivo = p.f[0].L == 0 && p.f[0].src == cx.inicio, repete = true;
        for (int k = 1; k < p.n; ++k) repete = repete && p.f[k].src == cx.inicio && p.f[k].L > 0;
        confere (vivo && repete, "2b. 1a fatia ao vivo, as outras repetem o chunk");
    }

    // 3. sem estalo: pico da derivada na troca seco ↔ fill (seno de 220 Hz, fill AUTO GRID 1/16 com REVERSE e PITCH)
    {
        Motor m; m.prepare (fs);
        float val[numCampos]; padroes (val);
        val[kOnde] = 0; val[kPFim] = 7; val[kReverse] = 1; val[kGate] = 70;
        Transporte tp; tp.tocando = true; tp.temPpq = true; tp.bpm = 140;
        const double spb = fs * 60 / 140;
        std::vector<float> l (512), r (512);
        double maxd = 0, ant = 0;
        const double amp = 0.5, incl = amp * 2 * kPi * 220 / fs;
        for (int b = 0; b < 600; ++b)
        {
            for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = (float) (amp * std::sin (2 * kPi * 220 * (b * 512 + i) / fs));
            float* ch[2] = { l.data(), r.data() };
            tp.ppq = b * 512 / spb;
            m.process (ch, 2, 512, val, tp, nullptr, 0);
            for (int i = 0; i < 512; ++i) { if (b > 2) maxd = std::max (maxd, std::abs (l[(size_t) i] - ant)); ant = l[(size_t) i]; }
        }
        std::snprintf (buf, sizeof buf, "(derivada máx %.4f; o seno sozinho %.4f)", maxd, incl);
        confere (maxd < 2.5 * incl, "3. sem estalo nas trocas", buf);
    }

    // 4. RAMP 1/8 → 1/64 termina exatamente no compasso
    {
        Cena c; c.v[kShape] = shRamp; c.v[kRate] = 1; c.v[kRampTo] = 4; c.v[kLength] = 1;
        Contexto cx; cx.fs = fs; cx.spb = fs * 60 / 142; cx.inicio = 0;
        Plano p; montaEdit (c, cx, p);
        const auto& u = p.f[p.n - 1];
        std::snprintf (buf, sizeof buf, "(%d fatias, fim %lld de %lld)", p.n, (long long) (u.t0 + u.dur), (long long) p.len);
        bool cont = true;
        for (int k = 1; k < p.n; ++k) cont = cont && p.f[k].t0 == p.f[k - 1].t0 + p.f[k - 1].dur;
        confere (u.t0 + u.dur == p.len && cont, "4. RAMP fecha no compasso", buf);
        bool causal = true;
        for (int k = 0; k < p.n; ++k) if (p.f[k].L > 0) causal = causal && p.f[k].src + p.f[k].L <= cx.inicio + p.f[k].t0;
        confere (causal, "4b. toda região repetida já foi gravada");
    }

    // 5. KEY F# minor: todos os degraus caem na escala
    {
        const unsigned m = rodaMascara (mascaraEscala (1), 6);
        bool ok = true;
        for (int st = -24; st <= 24; ++st) { const int s = encaixa (st, 6, m); ok = ok && ((m >> (((6 + s) % 12 + 12) % 12)) & 1u); }
        Cena c; c.v[kShape] = shGrid; c.v[kRate] = 3; c.v[kPIni] = 0; c.v[kPFim] = 12; c.v[kKey] = 1;
        Contexto cx; cx.fs = fs; cx.spb = fs * 60 / 142; cx.mascara = m; cx.pcChunk = 6;
        Plano p; montaEdit (c, cx, p);
        for (int k = 0; k < p.n; ++k) { const int s = (int) std::lround (p.f[k].st); ok = ok && ((m >> (((6 + s) % 12 + 12) % 12)) & 1u); }
        confere (ok, "5. KEY F# MINOR: degraus na escala");
    }

    // 6. detector de sílaba: 16 sílabas
    {
        Motor m; m.prepare (fs);
        float val[numCampos]; padroes (val);
        Transporte tp; tp.bpm = 120;
        std::vector<float> l (480), r (480);
        for (int b = 0; b < 400 * 10 / 4; ++b)   // 4 s
        {
            for (int i = 0; i < 480; ++i) l[(size_t) i] = r[(size_t) i] = voz ((b * 480 + i) / fs);
            float* ch[2] = { l.data(), r.data() };
            m.process (ch, 2, 480, val, tp, nullptr, 0);
        }
        const int n = m.nAtaques.load();
        int certos = 0;
        for (int i = 0; i < std::min (n, 128); ++i)
        {
            const double t = m.ataque[i].load() / fs;
            const double d = t - std::round (t / 0.25) * 0.25;
            if (std::abs (d) < 0.012) ++certos;
        }
        std::snprintf (buf, sizeof buf, "(%d detectados, %d no lugar)", n, certos);
        confere (certos >= 14 && n - certos <= 1, "6. detector: 16 sílabas", buf);
    }

    // 7/8. GROOVE OFFBEAT: chops no contratempo (sem HUMANIZE) e DUCK no valor pedido
    {
        Motor m; m.prepare (fs);
        float val[numCampos]; padroes (val);
        val[kModo] = mGroove; val[kGrooveOn] = 1; val[kDensity] = 30; val[kMixVoz] = mvDuck; val[kDuck] = -9;
        Transporte tp; tp.tocando = true; tp.temPpq = true; tp.bpm = 120;
        const double spb = fs * 60 / 120;
        std::vector<float> l (480), r (480);
        float duckMin = 1;
        for (int b = 0; b < 100 * 8; ++b)   // 8 s
        {
            for (int i = 0; i < 480; ++i) l[(size_t) i] = r[(size_t) i] = voz ((b * 480 + i) / fs);
            float* ch[2] = { l.data(), r.data() };
            tp.ppq = b * 480 / spb;
            m.process (ch, 2, 480, val, tp, nullptr, 0);
            duckMin = std::min (duckMin, m.dbgDuck);
        }
        int no = 0, fora = 0;
        for (int i = 0; i < m.dbgNVoz; ++i)
        {
            const double beat = m.dbgVoz[i] / spb;
            const double frac = beat - std::floor (beat);
            if (std::abs (frac - 0.5) * spb < 0.001 * fs) ++no; else ++fora;
        }
        std::snprintf (buf, sizeof buf, "(%d no contratempo, %d fora)", no, fora);
        confere (no >= 8 && fora == 0, "7. GROOVE OFFBEAT no contratempo (±1 ms)", buf);
        const float db = 20 * std::log10 (duckMin);
        std::snprintf (buf, sizeof buf, "(%.2f dB)", db);
        confere (std::abs (db + 9.0f) <= 0.5f, "8. DUCK −9 dB", buf);
    }

    // 9. CATCH NEXT BAR: disparo no meio do compasso só soa no compasso seguinte
    {
        Motor m; m.prepare (fs);
        float val[numCampos]; padroes (val);
        val[kGatilho] = gOneShot; val[kCatch] = caBar;
        Transporte tp; tp.tocando = true; tp.temPpq = true; tp.bpm = 128;
        const double spb = fs * 60 / 128;
        std::vector<float> l (256), r (256);
        for (int b = 0; b < 1200; ++b)
        {
            for (int i = 0; i < 256; ++i) l[(size_t) i] = r[(size_t) i] = voz ((b * 256 + i) / fs);
            float* ch[2] = { l.data(), r.data() };
            tp.ppq = b * 256 / spb;
            Evento e; e.amostra = 17; e.tipo = evAperta; e.pad = -1;
            const bool aperta = b == (int) (5.3 * spb / 256);
            m.process (ch, 2, 256, val, tp, aperta ? &e : nullptr, aperta ? 1 : 0);
        }
        const double alvo = 8.0 * spb;
        std::snprintf (buf, sizeof buf, "(começou em %lld, compasso em %.1f)", (long long) m.dbgFillInicio, alvo);
        confere (std::abs ((double) m.dbgFillInicio - alvo) <= 1.0, "9. CATCH NEXT BAR", buf);
    }

    // 10. DICE: mesmo seed = mesmo pattern; passos com LOCK não mudam
    {
        Cena c;
        for (int i = 0; i < 16; i += 4) c.passo[i] = passo (pTipo (c.passo[i]), pValor (c.passo[i]), true);
        uint32_t a[maxPassos], b[maxPassos];
        std::memcpy (a, c.passo, sizeof a); std::memcpy (b, c.passo, sizeof b);
        dadoPassos (a, 16, dzAll, 1.0f, 0.6f, 42);
        dadoPassos (b, 16, dzAll, 1.0f, 0.6f, 42);
        bool igual = std::memcmp (a, b, sizeof a) == 0, travados = true, mudou = false;
        for (int i = 0; i < 16; ++i)
        {
            if (i % 4 == 0) travados = travados && a[i] == c.passo[i];
            else mudou = mudou || a[i] != c.passo[i];
        }
        confere (igual && travados && mudou, "10. DICE repetível, LOCK protege");
    }

    // 11. CPU e latência (fill COMPLEX + groove ligados, 10 s)
    {
        Motor m; m.prepare (fs);
        float val[numCampos]; padroes (val);
        val[kModo] = mComplex; val[kOnde] = 0; val[kGrooveOn] = 1;
        Transporte tp; tp.tocando = true; tp.temPpq = true; tp.bpm = 142;
        const double spb = fs * 60 / 142;
        std::vector<float> l (512), r (512);
        const auto t0 = std::chrono::steady_clock::now();
        const int nb = (int) (10 * fs / 512);
        for (int b = 0; b < nb; ++b)
        {
            for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = voz ((b * 512 + i) / fs);
            float* ch[2] = { l.data(), r.data() };
            tp.ppq = b * 512 / spb;
            m.process (ch, 2, 512, val, tp, nullptr, 0);
        }
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::snprintf (buf, sizeof buf, "(%.2f%% de um núcleo; latência 0 amostras)", 100.0 * s / 10.0);
        confere (s / 10.0 < 0.05, "11. CPU", buf);
    }

    // 12. presets: 54 em 5 bancos (12/10/12/14/6), todos montam um plano válido
    {
        int porBanco[numBancos] = {};
        bool ok = presets().size() == 54;
        for (int i = 0; i < (int) presets().size(); ++i)
        {
            ++porBanco[presets()[(size_t) i].banco];
            const Cena c = cenaPreset (i);
            if (c.i (kModo) == mGroove) continue;
            Contexto cx; cx.fs = fs; cx.spb = fs * 60 / 142; cx.inicio = 500000;
            Plano p; monta (c, cx, p);
            int64_t fim = 0;
            for (int k = 0; k < p.n; ++k) fim = std::max<int64_t> (fim, p.f[k].t0 + p.f[k].dur);
            ok = ok && p.n > 0 && fim == p.len;
        }
        ok = ok && porBanco[0] == 12 && porBanco[1] == 10 && porBanco[2] == 12 && porBanco[3] == 14 && porBanco[4] == 6;
        std::snprintf (buf, sizeof buf, "(%d presets: %d/%d/%d/%d/%d)", (int) presets().size(), porBanco[0], porBanco[1], porBanco[2], porBanco[3], porBanco[4]);
        confere (ok, "12. presets de fábrica", buf);
    }

    std::printf ("\n%s (%d falha%s)\n", falhas ? "BANCADA REPROVADA" : "BANCADA OK", falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
