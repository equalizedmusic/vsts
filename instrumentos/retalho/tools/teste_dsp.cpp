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

    // 14. RENDER DO ARQUIVO (0.1.3): amostra inteira na mão, efeito começa no ataque escolhido, sem play
    {
        const int N = (int) (4.0 * fs);
        std::vector<float> a ((size_t) N), b ((size_t) N);
        for (int i = 0; i < N; ++i) { a[(size_t) i] = voz (i / fs); b[(size_t) i] = a[(size_t) i]; }
        Trecho tr; tr.d[0] = a.data(); tr.d[1] = b.data(); tr.n = N; tr.base = 0;
        const int64_t T = (int64_t) (0.75 * fs);   // 4ª sílaba
        Ataques at; for (int k = 0; k < 16; ++k) at.v[at.n++] = (int64_t) (k * 0.25 * fs);
        Contexto cx; cx.fs = fs; cx.spb = fs * 60 / 120; cx.inicio = T; cx.at = &at; cx.offline = true;

        // GRID 1/16, 1 BAR: toda repetição começa igual ao arquivo no ataque
        Cena c; c.v[kShape] = shGrid; c.v[kRate] = 2; c.v[kLength] = 1;
        Plano p; montaEdit (c, cx, p);
        Render r; r.inicia (p, (float) fs);
        std::vector<float> o ((size_t) p.len);
        for (int64_t i = 0; i < p.len; ++i) { float x, y; r.amostra (i, N - 1, tr, x, y); o[(size_t) i] = x; }
        double err = 0;
        for (int k = 0; k < p.n; ++k)
            for (int i = 400; i < 1200 && p.f[k].t0 + i < p.len; ++i)   // depois do fade-in, antes do fade-out
                err = std::max (err, (double) std::abs (o[(size_t) (p.f[k].t0 + i)] / p.f[k].ganho - a[(size_t) (T + i)]));   // DECAY à parte
        std::snprintf (buf, sizeof buf, "(%d repetições, diferença máx %.3f)", p.n, err);
        confere (p.n == 16 && err < 0.02, "14. arquivo: cada repetição = o ataque escolhido", buf);

        // todos os presets COMPLEX/EDIT soam do começo ao fim (nada vira silêncio por "ainda não gravado")
        int mudos = 0, total = 0;
        for (int i = 0; i < (int) presets().size(); ++i)
        {
            const Cena pc = cenaPreset (i);
            if (pc.i (kModo) == mGroove) continue;
            Plano pp; monta (pc, cx, pp);
            Render rr; rr.inicia (pp, (float) fs);
            double e = 0;
            for (int64_t s = 0; s < pp.len; ++s) { float x, y; rr.amostra (s, N - 1, tr, x, y); e += (double) x * x + (double) y * y; }
            ++total;
            if (e / std::max<int64_t> (1, pp.len) < 1e-5) ++mudos;
        }
        std::snprintf (buf, sizeof buf, "(%d presets, %d mudos)", total, mudos);
        confere (total > 0 && mudos == 0, "14b. arquivo: todo preset sai com som", buf);
    }

    // 15. PICOTE (0.1.4): a frase fica LIMPA na maior parte; o efeito só em alguns pontos; tudo soa; trecho limpo = original
    {
        const int N = (int) (8.0 * fs);
        std::vector<float> a ((size_t) N), b ((size_t) N);
        for (int i = 0; i < N; ++i) { a[(size_t) i] = voz (i / fs); b[(size_t) i] = a[(size_t) i]; }
        Trecho tr; tr.d[0] = a.data(); tr.d[1] = b.data(); tr.n = N; tr.base = 0;
        Contexto cx; cx.fs = fs; cx.spb = fs * 60 / 120; cx.inicio = 0; cx.offline = true;
        static const float minLimpo[3] = { 0.65f, 0.55f, 0.45f };
        bool limpoOk = true, fimOk = true, somOk = true, igualOk = true, fimEfeito = true;
        double piorLimpo[3] = { 1, 1, 1 }, difMax = 0;
        for (int rc = 0; rc < numReceitas; ++rc)
            for (int q = 0; q < 3; ++q)
                for (int len = 1; len <= 3; ++len)
                    for (int sd = 1; sd <= 3; ++sd)
                    {
                        Cena c; c.v[kModo] = mPicote; c.v[kReceita] = (float) rc; c.v[kQuanto] = (float) q; c.v[kLength] = (float) len; c.v[kSeed] = (float) sd;
                        const int nU = (int) std::lround (compassosLength (len) * 8);
                        std::vector<int8_t> tp; std::vector<int16_t> sg;
                        sorteiaPicote (c, nU, 8, tp, sg);
                        int limpos = 0; for (auto x : tp) if (x < 0) ++limpos;
                        const double fl = (double) limpos / nU;
                        piorLimpo[q] = std::min (piorLimpo[q], fl);
                        limpoOk = limpoOk && fl >= minLimpo[q] && limpos < nU;
                        fimEfeito = fimEfeito && tp[(size_t) (nU - 1)] >= 0;
                        Plano p; monta (c, cx, p);
                        int64_t fim = 0;
                        for (int k = 0; k < p.n; ++k) fim = std::max<int64_t> (fim, p.f[k].t0 + p.f[k].dur);
                        fimOk = fimOk && p.n > 0 && fim == p.len;
                        Render r; r.inicia (p, (float) fs);
                        double e = 0;
                        for (int64_t s = 0; s < p.len; ++s)
                        {
                            float x, y; r.amostra (s, N - 1, tr, x, y); e += (double) x * x;
                            // trecho limpo (longe das bordas das fatias) = a voz original
                            const int u = (int) (s / (cx.spb * 0.5));
                            if (u < nU && tp[(size_t) u] < 0)
                            {
                                bool longe = true;
                                for (int k = 0; k < p.n; ++k)
                                    if (std::abs (s - p.f[k].t0) < 600 || std::abs (s - (p.f[k].t0 + p.f[k].dur)) < 600) { longe = false; break; }
                                if (longe) difMax = std::max (difMax, (double) std::abs (x - a[(size_t) s]));
                            }
                        }
                        somOk = somOk && e / (double) p.len > 1e-5;
                    }
        igualOk = difMax < 1e-4;
        std::snprintf (buf, sizeof buf, "(limpo m\xc3\xadnimo: POUCO %.0f%% \xc2\xb7 M\xc3\x89" "DIO %.0f%% \xc2\xb7 MUITO %.0f%%)", piorLimpo[0] * 100, piorLimpo[1] * 100, piorLimpo[2] * 100);
        confere (limpoOk, "15. PICOTE: a maior parte da frase fica limpa", buf);
        confere (fimEfeito, "15b. PICOTE: o fim da frase sempre ganha efeito");
        confere (fimOk && somOk, "15c. PICOTE: 16 receitas x 3 quantidades x 3 tamanhos fecham na frase e soam");
        std::snprintf (buf, sizeof buf, "(diferen\xc3\xa7" "a m\xc3\xa1x %.1e)", difMax);
        confere (igualOk, "15d. PICOTE: trecho limpo = a voz original", buf);
    }

    // 16. ESTICA (0.1.4): as 6 direções soam, nunca passam do volume da voz, mesmo SEED = mesmo som; CONGELA fica parado
    {
        const int N = (int) (4.0 * fs);
        std::vector<float> a ((size_t) N), b ((size_t) N);
        float pv = 0;
        for (int i = 0; i < N; ++i) { a[(size_t) i] = voz (i / fs); b[(size_t) i] = a[(size_t) i]; pv = std::max (pv, 2 * std::abs (a[(size_t) i])); }
        Trecho tr; tr.d[0] = a.data(); tr.d[1] = b.data(); tr.n = N; tr.base = 0;
        const int64_t len = (int64_t) (2.0 * fs);
        std::vector<float> l ((size_t) len), r ((size_t) len), l2 ((size_t) len), r2 ((size_t) len);
        bool som = true, alto = false, igual = true;
        double espalhaCongela = 0, espalhaMeio = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int d = 0; d < numDirecoes; ++d)
        {
            Cena c; c.v[kModo] = mEstica; c.v[kEDir] = (float) d; c.v[kEInt] = 100;
            std::vector<Grao> gs;
            renderEstica (c, tr, 0, N, fs, fs * 0.5, len, l.data(), r.data(), &gs);
            renderEstica (c, tr, 0, N, fs, fs * 0.5, len, l2.data(), r2.data(), nullptr);
            double e = 0; float pk = 0;
            for (int64_t i = 0; i < len; ++i) { e += (double) l[(size_t) i] * l[(size_t) i]; pk = std::max (pk, std::abs (l[(size_t) i]) + std::abs (r[(size_t) i])); igual = igual && l[(size_t) i] == l2[(size_t) i]; }
            som = som && e / (double) len > 1e-5 && ! gs.empty();
            alto = alto || pk > pv * 1.001f;
            float mn = 1, mx = 0;
            for (auto& g : gs) { mn = std::min (mn, g.src); mx = std::max (mx, g.src); }
            if (d == esCongela) espalhaCongela = mx - mn;
            if (d == esMeio) espalhaMeio = mx - mn;
        }
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        confere (som && ! alto, "16. ESTICA: 6 dire\xc3\xa7\xc3\xb5" "es soam e nunca passam do volume da voz");
        confere (igual, "16b. ESTICA: mesmo SEED = mesmo resultado");
        std::snprintf (buf, sizeof buf, "(CONGELA anda %.0f%% da amostra; MEIO TEMPO %.0f%%)", espalhaCongela * 100, espalhaMeio * 100);
        confere (espalhaCongela < 0.03 && espalhaMeio > 0.2, "16c. ESTICA: CONGELA fica num ponto, MEIO TEMPO anda", buf);
        std::snprintf (buf, sizeof buf, "(12 renders de 2 s em %.0f ms)", s * 1000);
        confere (s < 3.0, "16d. ESTICA: render r\xc3\xa1pido na tela", buf);
    }

    std::printf ("\n%s (%d falha%s)\n", falhas ? "BANCADA REPROVADA" : "BANCADA OK", falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
