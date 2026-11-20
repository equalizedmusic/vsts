// MAPA 0.2 — teste do motor fora do app (sem JUCE).
//   teste_v2 notas  <mono_22050.wav> <nmp.onnx> [fmin fmax]       -> notas do Basic Pitch em CSV (stdout)
//   teste_v2 mapa   <estereo_44100.wav> <pasta modelos> <saida>   -> separa, transcreve, analisa; grava stems,
//                                                                     relatorio.txt e notas.csv em <saida>
// "pasta modelos" tem htdemucs.onnx e nmp.onnx. WAV de entrada: PCM 16/24/32 ou float 32.
#include "Analise.h"
#include "IA.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static bool lerWav (const char* caminho, std::vector<float>& L, std::vector<float>& R, double& sr, int& canais)
{
    std::ifstream f (caminho, std::ios::binary);
    if (! f) return false;
    std::vector<char> d ((std::istreambuf_iterator<char> (f)), {});
    if (d.size() < 44 || std::memcmp (d.data(), "RIFF", 4) || std::memcmp (d.data() + 8, "WAVE", 4)) return false;
    size_t p = 12; int fmt = 0, bits = 0; canais = 0; sr = 0;
    while (p + 8 <= d.size())
    {
        uint32_t tam; std::memcpy (&tam, d.data() + p + 4, 4);
        if (! std::memcmp (d.data() + p, "fmt ", 4))
        {
            uint16_t a, c, b; uint32_t s;
            std::memcpy (&a, d.data() + p + 8, 2); std::memcpy (&c, d.data() + p + 10, 2);
            std::memcpy (&s, d.data() + p + 12, 4); std::memcpy (&b, d.data() + p + 22, 2);
            fmt = a; canais = c; sr = s; bits = b;
            if (fmt == 0xFFFE) { uint16_t sub; std::memcpy (&sub, d.data() + p + 32, 2); fmt = sub; }
        }
        else if (! std::memcmp (d.data() + p, "data", 4))
        {
            const size_t bps = (size_t) bits / 8, n = std::min<size_t> (tam, d.size() - p - 8) / (bps * canais);
            L.resize (n); R.resize (n);
            const char* q = d.data() + p + 8;
            for (size_t i = 0; i < n; ++i)
                for (int c = 0; c < canais; ++c, q += bps)
                {
                    double s = 0;
                    if (fmt == 3 && bits == 32) { float v; std::memcpy (&v, q, 4); s = v; }
                    else if (bits == 16) { int16_t v; std::memcpy (&v, q, 2); s = v / 32768.0; }
                    else if (bits == 24) { int v = (uint8_t) q[0] | ((uint8_t) q[1] << 8) | ((int8_t) q[2] << 16); s = v / 8388608.0; }
                    else if (bits == 32) { int32_t v; std::memcpy (&v, q, 4); s = v / 2147483648.0; }
                    if (c == 0) L[i] = (float) s;
                    if (c == 1 || canais == 1) R[i] = (float) s;
                }
            return true;
        }
        p += 8 + tam + (tam & 1);
    }
    return false;
}

static bool gravarWav (const std::string& caminho, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::ofstream f (caminho, std::ios::binary);
    if (! f) return false;
    const uint32_t n = (uint32_t) L.size(), dados = n * 2 * 4;
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); u32 (36 + dados); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (3); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 8); u16 (8); u16 (32);
    f.write ("data", 4); u32 (dados);
    for (uint32_t i = 0; i < n; ++i) { f.write ((const char*) &L[i], 4); f.write ((const char*) &R[i], 4); }
    return (bool) f;
}

static std::vector<char> lerTudo (const std::string& c)
{
    std::ifstream f (c, std::ios::binary);
    return std::vector<char> ((std::istreambuf_iterator<char> (f)), {});
}

static double agora()
{
    return std::chrono::duration<double> (std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main (int argc, char** argv)
{
    if (argc >= 4 && ! std::strcmp (argv[1], "notas"))
    {
        std::vector<float> L, R; double sr; int ch;
        if (! lerWav (argv[2], L, R, sr, ch)) { std::fprintf (stderr, "WAV inválido\n"); return 1; }
        auto mono = mapa::somaMono (L, R);
        const auto m = lerTudo (argv[3]);
        mapa::Transcritor t; std::string erro;
        if (! t.carregar (m.data(), m.size(), erro)) { std::fprintf (stderr, "%s\n", erro.c_str()); return 1; }
        mapa::ParametrosNotas par;
        if (argc >= 6) { par.fMin = std::atof (argv[4]); par.fMax = std::atof (argv[5]); }
        std::vector<mapa::NotaAudio> notas;
        const double t0 = agora();
        if (! t.transcrever (mono, par, notas, {}, nullptr, erro)) { std::fprintf (stderr, "%s\n", erro.c_str()); return 1; }
        std::fprintf (stderr, "%zu notas em %.2f s\n", notas.size(), agora() - t0);
        for (auto& n : notas) std::printf ("%.4f,%.4f,%d,%.4f\n", n.ini, n.fim, n.tom, n.amp);
        return 0;
    }
    if (argc >= 5 && ! std::strcmp (argv[1], "mapa"))
    {
        const std::string modelos = argv[3], saida = argv[4];
        std::vector<float> L, R; double sr; int ch;
        if (! lerWav (argv[2], L, R, sr, ch) || sr != 44100) { std::fprintf (stderr, "precisa WAV 44.1 kHz\n"); return 1; }
        mapa::EntradaAnalise in;
        in.sampleRate = sr; in.canais = ch; in.nomeMusica = "teste"; in.caminhoAudio = argv[2];
        in.mono = mapa::somaMono (L, R);
        const auto bpModelo = lerTudo (modelos + "/nmp.onnx");
        double t0 = agora();
        bool ok = false;
        const char* nomesSt[] = { "bateria", "baixo", "outros", "voz" };
        if (std::getenv ("MAPA_REUSAR"))
        {
            // reaproveita os stems de uma rodada anterior (só transcreve de novo): para ajustar a análise rápido
            ok = true;
            for (int s = 0; s < mapa::numStems && ok; ++s)
            {
                double srS; int chS;
                ok = lerWav ((saida + "/stem_" + nomesSt[s] + ".wav").c_str(), in.stems.canal[(size_t) s][0], in.stems.canal[(size_t) s][1], srS, chS);
            }
            mapa::Transcritor tr; std::string erro;
            ok = ok && tr.carregar (bpModelo.data(), bpModelo.size(), erro);
            struct T { int s; std::vector<mapa::NotaAudio>* d; double a, b; } ts[] = {
                { mapa::stemBaixo, &in.notasBaixo, 25, 420 }, { mapa::stemVoz, &in.notasVoz, 70, 1400 }, { mapa::stemOutros, &in.notasOutros, 50, 5000 } };
            for (auto& t : ts)
            {
                if (! ok) break;
                mapa::ParametrosNotas par; par.fMin = t.a; par.fMax = t.b;
                ok = tr.transcrever (mapa::metadeDaTaxa (mapa::somaMono (in.stems.canal[(size_t) t.s][0], in.stems.canal[(size_t) t.s][1])), par, *t.d, {}, nullptr, erro);
            }
        }
        else
            ok = mapa::processarComIA (L, R, modelos + "/htdemucs.onnx", bpModelo.data(), bpModelo.size(), in,
            [] (float p, const char* e) { static int ult = -1; const int q = (int) (p * 20); if (q != ult) { ult = q; std::fprintf (stderr, "  %3d%% %s\n", (int) (p * 100), e); } },
            nullptr);
        if (! ok) { std::fprintf (stderr, "IA falhou: %s\n", in.erroIA.c_str()); return 2; }
        std::fprintf (stderr, "IA: %.1f s\n", agora() - t0);
        const char* nomes[] = { "bateria", "baixo", "outros", "voz" };
        for (int s = 0; s < mapa::numStems && ! std::getenv ("MAPA_REUSAR"); ++s)
            gravarWav (saida + "/stem_" + nomes[s] + ".wav", in.stems.canal[(size_t) s][0], in.stems.canal[(size_t) s][1], 44100);
        t0 = agora();
        mapa::Projeto p;
        if (! mapa::analisar (in, p)) { std::fprintf (stderr, "análise falhou\n"); return 3; }
        std::fprintf (stderr, "análise: %.1f s\n", agora() - t0);
        FILE* rel = std::fopen ((saida + "/relatorio.txt").c_str(), "w");
        FILE* csv = std::fopen ((saida + "/notas.csv").c_str(), "w");
        std::fprintf (rel, "BPM %.2f  tom %s  compassos %d  audioInicio %.3f tempos  corte %.3f s  1o tempo forte %.3f s  dur %.1f s\n",
                      p.bpm, p.tomTexto.c_str(), p.totalCompassos, p.audioInicioTempos, p.audioCorteSeg, p.primeiroTempoSeg, p.duracaoSeg);
        for (auto& pt : p.partes) std::fprintf (rel, "  parte %-8s compassos %3d-%3d\n", pt.nome.c_str(), pt.compIni + 1, pt.compFim);
        std::fprintf (csv, "faixa,inicio_tempos,dur_tempos,tom,vel,inicio_seg_audio\n");
        for (auto& f : p.faixas)
        {
            size_t nn = 0;
            std::fprintf (rel, "  faixa %-12s %-26s", f.nome.c_str(), f.desc.c_str());
            for (auto& c : f.clipes) { std::fprintf (rel, " [%g-%g]", c.inicio / 4 + 1, c.fim / 4); nn += c.notas.size();
                for (auto& n : c.notas) std::fprintf (csv, "%s,%.4f,%.4f,%d,%d,%.4f\n", f.nome.c_str(), n.inicio, n.dur, n.tom, n.vel, p.temposParaSegAudio (n.inicio)); }
            std::fprintf (rel, "  notas=%zu\n", nn);
        }
        std::fclose (rel); std::fclose (csv);
        std::fprintf (stderr, "ok\n");
        return 0;
    }
    std::printf ("uso: teste_v2 notas <wav22k> <nmp.onnx> [fmin fmax] | teste_v2 mapa <wav44k> <modelos> <saida>\n");
    return 1;
}
