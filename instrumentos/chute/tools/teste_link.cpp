// Bancada do CHUTE LINK (sem JUCE): o CHUTE publica batidas na memória compartilhada e um "ALIGN" lê no mesmo processo.
//   zig c++ -std=c++17 -O2 -I ../Source teste_link.cpp ../Source/ChuteLink.cpp -o teste_link.exe
// Confere: (1) o corpo recriado bate amostra a amostra com o motor do CHUTE quando a batida chega no mesmo bloco;
// (2) batida que chega um bloco atrasada (o DAW processou o baixo antes do kick) é recriada na posição certa, só
// sem o pedaço que já passou; (3) o loop do DAW voltando no tempo não toca batida velha; (4) a vaga some quando o
// CHUTE é destruído.
#include "Dsp.h"
#include "ChuteLink.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace chute;
static int falhas = 0;
static void confere (bool ok, const std::string& o) { std::printf ("%s  %s\n", ok ? "ok  " : "FALHA", o.c_str()); if (! ok) ++falhas; }

int main()
{
    const double fs = 48000.0; const int bloco = 256;
    Ajustes a; padraoDoModelo (a, 19); a.modo = modoPreset; a.clickOn = false; a.volumeDb = 0.0f; a.velo = 0.0f;
    // o que o CHUTE toca (só o corpo), com a batida na amostra 1000 da linha do tempo
    const int total = 48000;
    std::vector<float> ref (total, 0.0f);
    Engine e; e.prepare (fs);
    std::vector<float> fase (nPts), amp (nPts);
    auto pub = std::make_unique<chutelink::Publicador>();
    confere (pub->ocupa(), "CHUTE ocupa uma vaga");
    auto publica = [&] (int64_t pos)
    {
        const Voz& v = e.noteOn (-1, 1.0f, a);
        for (int k = 0; k < v.len; ++k) { fase[(size_t) k] = v.ph[k]; amp[(size_t) k] = v.am[k]; }
        pub->publica (pos, 0.0, true, (float) fs, v.notaMidi, (float) mtof (v.notaMidi), (float) v.sinal, (float) v.t0, fase.data(), amp.data(), v.len);
    };
    {
        float* p = ref.data();
        e.render (&p, 1, 0, 1000, a);
        publica (1000);   // não é lido ainda: só para o motor tocar
        float* q = ref.data() + 1000;
        e.render (&q, 1, 0, total - 1000, a);
    }
    pub->vivo (29.0f);

    // (1) leitor sincroniza ANTES de cada bloco, a batida já publicada no bloco dela
    for (int atraso = 0; atraso < 2; ++atraso)
    {
        chutelink::Leitor l;
        std::vector<float> rec (total, 0.0f);
        // recomeça: nova publicação com a batida em 1000 (o leitor só vê o que vem depois de sincronizar a 1ª vez)
        l.sincroniza();
        bool publicada = false;
        for (int b0 = 0; b0 < total; b0 += bloco)
        {
            const bool bateAqui = 1000 >= b0 && 1000 < b0 + bloco;
            if (bateAqui && atraso == 0 && ! publicada) { publica (1000); publicada = true; }
            l.sincroniza();
            l.render (rec.data() + b0, std::min (bloco, total - b0), fs, true, b0, 0.0);
            if (bateAqui && atraso == 1 && ! publicada) { publica (1000); publicada = true; }   // o CHUTE processou depois
        }
        double err = 0, en = 0;
        const int ini = atraso ? 1024 : 0;   // atrasada: o 1º bloco depois da batida (1000..1023) não volta
        for (int i = ini; i < total; ++i) { err += (rec[i] - ref[i]) * (double) (rec[i] - ref[i]); en += ref[i] * (double) ref[i]; }
        const double db = 10 * std::log10 (err / en + 1e-30);
        confere (db < -60.0, (atraso ? "batida 1 bloco atrasada: posição certa, erro " : "batida no mesmo bloco: erro ") + std::to_string (db) + " dB");
        if (atraso)
        {
            double antes = 0; for (int i = 1000; i < 1024; ++i) antes += std::fabs (rec[i]);
            confere (antes == 0.0, "atrasada: o pedaço que já passou fica de fora (sem tocar fora do lugar)");
        }
    }
    // (3) loop: a batida publicada em 40000 e o DAW volta para 0. Ela não pode tocar de novo em 40000 na outra passada
    {
        chutelink::Leitor l; l.sincroniza();
        publica (40000);
        std::vector<float> rec (bloco, 0.0f);
        l.sincroniza();
        l.render (rec.data(), bloco, fs, true, 40000, 0.0);                 // a batida toca
        double s0 = 0; for (float v : rec) s0 += std::fabs (v);
        confere (s0 > 0.0, "batida em 40000 toca");
        std::fill (rec.begin(), rec.end(), 0.0f);
        l.render (rec.data(), bloco, fs, true, 0, 0.0);                     // voltou para 0 (loop)
        std::vector<float> depois (bloco, 0.0f);
        l.render (depois.data(), bloco, fs, true, 40000, 0.0);              // chegou em 40000 de novo, sem batida nova
        double s = 0; for (float v : depois) s += std::fabs (v);
        confere (s == 0.0, "loop do DAW: batida velha não toca na outra passada");
    }
    // (4) CHUTE apagado: a vaga é liberada
    {
        int idx[chutelink::NSL]; char nomes[chutelink::NSL][chutelink::NNOME];
        const int antes = chutelink::vivas (idx, nomes);
        pub.reset();
        const int depois = chutelink::vivas (idx, nomes);
        confere (antes == 1 && depois == 0, "vaga some quando o CHUTE é apagado (" + std::to_string (antes) + " → " + std::to_string (depois) + ")");
    }
    std::printf (falhas ? "\n%d FALHA(S)\n" : "\ntudo ok\n", falhas);
    return falhas ? 1 : 0;
}
