// Bancada do CHUTE LINK (sem JUCE): o CHUTE publica batidas na memória compartilhada e um "ALIGN" lê no mesmo processo.
//   zig c++ -std=c++17 -O2 -I ../Source teste_link.cpp ../Source/ChuteLink.cpp -o teste_link.exe
// Confere: (1) o corpo recriado bate amostra a amostra com o motor do CHUTE quando a batida chega no mesmo bloco;
// (2) batida que chega um bloco atrasada (o DAW processou o baixo antes do kick) é recriada na posição certa, só
// sem o pedaço que já passou; (3) a faixa do kick um bloco NA FRENTE: a batida espera o bloco dela (0.1.2); (4) volta do
// loop: a cauda que ainda soa continua e a batida velha não toca de novo na outra passada; (5) exportação mais rápida
// que o tempo real (relógio quase parado); (6) o CHUTE 2 s sem processar não derruba o LINK (0.1.2); (7) a vaga some
// quando o CHUTE é destruído.
#include "Dsp.h"
#include "ChuteLink.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace chute;
static int falhas = 0;
static void confere (bool ok, const std::string& o) { std::printf ("%s  %s\n", ok ? "ok  " : "FALHA", o.c_str()); if (! ok) ++falhas; }

int main()
{
    const double fs = 48000.0; const int bloco = 256;
    Ajustes a; padraoDoModelo (a, 19); a.modo = modoPreset; a.clickOn = false; a.volumeDb = 0.0f; a.velo = 0.0f;
    // o que o CHUTE toca (só o corpo), com a batida na amostra 1000 da linha do tempo
    const int total = 48000, B = 1000;
    std::vector<float> ref (total, 0.0f);
    Engine e; e.prepare (fs);
    std::vector<float> fase (nPts), amp (nPts);
    auto pub = std::make_unique<chutelink::Publicador>();
    confere (pub->ocupa(), "CHUTE ocupa uma vaga");
    // relógio: por padrão, tempo real (ms = amostra da linha do tempo / fs)
    auto msDe = [&] (double amostra) { return amostra / fs * 1000.0; };
    auto publica = [&] (int64_t pos, double wallMs)
    {
        const Voz& v = e.noteOn (-1, 1.0f, a);
        for (int k = 0; k < v.len; ++k) { fase[(size_t) k] = v.ph[k]; amp[(size_t) k] = v.am[k]; }
        pub->publica (pos, wallMs, true, (float) fs, v.notaMidi, (float) mtof (v.notaMidi), (float) v.sinal, (float) v.t0, fase.data(), amp.data(), v.len);
    };
    {
        float* p = ref.data();
        e.render (&p, 1, 0, B, a);
        publica (B, msDe (B));   // não é lido ainda: só para o motor tocar
        float* q = ref.data() + B;
        e.render (&q, 1, 0, total - B, a);
    }
    pub->vivo (29.0f);
    auto erroDb = [&] (const std::vector<float>& rec, int ini)
    {
        double err = 0, en = 0;
        for (int i = ini; i < total; ++i) { err += (rec[i] - ref[i]) * (double) (rec[i] - ref[i]); en += ref[i] * (double) ref[i]; }
        return 10 * std::log10 (err / en + 1e-30);
    };

    // (1)(2)(3) quando o CHUTE publica em relação ao bloco de quem lê: no mesmo bloco, um bloco depois, um bloco antes.
    // escala = quanto o relógio anda por amostra da linha do tempo (1 = tempo real; 0,1 = exportação 10× mais rápida)
    struct Caso { const char* nome; int atraso; double escala; int iniErro; };
    for (const Caso c : { Caso { "batida no mesmo bloco", 0, 1.0, 0 }, Caso { "batida 1 bloco atrasada (posição certa)", 1, 1.0, 1024 },
                          Caso { "faixa do kick 1 bloco NA FRENTE", -1, 1.0, 0 }, Caso { "exportação 10x mais rápida que o tempo real", 0, 0.1, 0 },
                          Caso { "exportação + batida 1 bloco atrasada", 1, 0.1, 1024 } })
    {
        chutelink::Leitor l;
        std::vector<float> rec (total, 0.0f);
        l.sincroniza(); l.esquece();   // a batida anterior (de referência) não entra
        bool publicada = false;
        const int blocoDaBatida = B / bloco * bloco;
        for (int b0 = 0; b0 < total; b0 += bloco)
        {
            const double wall = msDe (b0) * c.escala, wallB = msDe (B) * c.escala;
            if (! publicada && ((c.atraso == 0 && b0 == blocoDaBatida) || (c.atraso < 0 && b0 + bloco == blocoDaBatida)))
            { publica (B, wallB); publicada = true; }
            l.sincroniza();
            l.render (rec.data() + b0, std::min (bloco, total - b0), fs, true, b0, wall);
            if (! publicada && c.atraso > 0 && b0 == blocoDaBatida) { publica (B, wallB); publicada = true; }   // o CHUTE processou depois
        }
        const double db = erroDb (rec, c.iniErro);
        confere (db < -60.0, std::string (c.nome) + ": erro " + std::to_string (db) + " dB");
        if (c.atraso > 0)
        {
            double antes = 0; for (int i = B; i < 1024; ++i) antes += std::fabs (rec[i]);
            confere (antes == 0.0, "atrasada: o pedaço que já passou fica de fora (sem tocar fora do lugar)");
        }
    }
    // (4) loop: a batida em 40000; o DAW volta para 0 (o relógio segue andando). A cauda que ainda soa continua igual ao
    // CHUTE e, na outra passada, em 40000 não toca nada (não houve batida nova)
    {
        const int P = 40000;
        chutelink::Leitor l; l.sincroniza(); l.esquece();
        std::vector<float> rec (total, 0.0f);
        double wall = msDe (P);
        publica (P, wall);
        l.sincroniza();
        l.render (rec.data(), bloco, fs, true, P, wall);   // bloco 40000..40255: a batida começa na amostra 0
        wall += msDe (bloco);
        for (int b0 = 0; b0 + bloco <= P + bloco; b0 += bloco)   // a outra passada, de 0 até 40255
        {
            l.sincroniza();
            l.render (rec.data() + bloco + b0, bloco, fs, true, b0, wall);
            wall += msDe (bloco);
            if (b0 + bloco + bloco >= total) break;
        }
        // rec[0..] = a batida desde o início, contínua através da volta do loop: igual ao ref a partir de B
        double err = 0, en = 0;
        for (int i = 0; i < 12000; ++i) { err += (rec[i] - ref[B + i]) * (double) (rec[i] - ref[B + i]); en += ref[B + i] * (double) ref[B + i]; }
        confere (10 * std::log10 (err / en + 1e-30) < -60.0, "volta do loop: a cauda que ainda soa continua (erro "
                     + std::to_string (10 * std::log10 (err / en + 1e-30)) + " dB)");
        double s = 0; for (int i = 30000; i < total; ++i) s += std::fabs (rec[i]);
        confere (s == 0.0, "volta do loop: batida velha não toca de novo na outra passada");
    }
    // (6) o CHUTE fica 2 s sem processar (o DAW não chamou a faixa do kick): o LINK continua de pé
    {
        chutelink::Leitor l;
        confere (l.sincroniza(), "LINK de pé antes da pausa");
        std::this_thread::sleep_for (std::chrono::milliseconds (2100));
        confere (l.sincroniza() && l.vaga() >= 0, "CHUTE 2,1 s sem processar: o LINK continua de pé");
    }
    // (7) CHUTE apagado: a vaga é liberada
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
