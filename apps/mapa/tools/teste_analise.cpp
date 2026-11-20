// Teste da análise do MAPA fora do app (sem JUCE): lê um WAV PCM 16/24/32 ou float e imprime BPM, tom, partes e faixas.
// Compilar: clang++ -std=c++17 -O2 -I../Source teste_analise.cpp ../Source/Analise.cpp -o teste_analise
#include "Analise.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

static bool lerWav (const char* caminho, std::vector<float>& mono, double& sr, int& canais)
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
            mono.resize (n);
            const char* q = d.data() + p + 8;
            for (size_t i = 0; i < n; ++i)
            {
                double s = 0;
                for (int c = 0; c < canais; ++c, q += bps)
                {
                    if (fmt == 3 && bits == 32) { float v; std::memcpy (&v, q, 4); s += v; }
                    else if (bits == 16) { int16_t v; std::memcpy (&v, q, 2); s += v / 32768.0; }
                    else if (bits == 24) { int v = (uint8_t) q[0] | ((uint8_t) q[1] << 8) | ((int8_t) q[2] << 16); s += v / 8388608.0; }
                    else if (bits == 32) { int32_t v; std::memcpy (&v, q, 4); s += v / 2147483648.0; }
                }
                mono[i] = (float) (s / canais);
            }
            return true;
        }
        p += 8 + tam + (tam & 1);
    }
    return false;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("uso: teste_analise arquivo.wav [--notas]\n"); return 1; }
    mapa::EntradaAnalise in;
    if (! lerWav (argv[1], in.mono, in.sampleRate, in.canais)) { std::printf ("WAV inválido\n"); return 1; }
    in.nomeMusica = "teste"; in.caminhoAudio = argv[1];
    mapa::Projeto p;
    if (! mapa::analisar (in, p)) { std::printf ("análise falhou\n"); return 2; }
    std::printf ("BPM %.2f  tom %s  compassos %d  audioInicio %.3f tempos  corte %.3f s  1o tempo forte %.3f s  dur %.1f s\n",
                 p.bpm, p.tomTexto.c_str(), p.totalCompassos, p.audioInicioTempos, p.audioCorteSeg, p.primeiroTempoSeg, p.duracaoSeg);
    for (auto& pt : p.partes) std::printf ("  parte %-8s compassos %3d-%3d\n", pt.nome.c_str(), pt.compIni + 1, pt.compFim);
    const bool notas = argc > 2 && ! std::strcmp (argv[2], "--notas");
    for (auto& f : p.faixas)
    {
        std::printf ("  faixa %-10s %-22s", f.nome.c_str(), f.desc.c_str());
        size_t nn = 0;
        for (auto& c : f.clipes) { std::printf (" [%g-%g]", c.inicio / 4 + 1, c.fim / 4); nn += c.notas.size(); }
        std::printf ("  notas=%zu\n", nn);
        if (notas && ! f.clipes.empty())
        {
            const auto& c = f.clipes[0];
            std::printf ("     1o compasso: ");
            for (auto& n : c.notas) if (n.inicio < c.inicio + 4) std::printf ("%g:%d(%g) ", n.inicio - c.inicio, n.tom, n.dur);
            std::printf ("\n");
        }
    }
    return 0;
}
