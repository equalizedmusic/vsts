#pragma once

// Banco de tabelas da QUIMERA, gerado por código na primeira vez que alguém pede (nada copiado de outro synth).
// Cada tabela tem `frames` quadros; cada quadro tem versões limitadas em banda por oitava (mipmaps) para não dar
// aliasing nas notas agudas. Compartilhado por todas as instâncias, só leitura depois de pronto (~19 MB).

#include <juce_core/juce_core.h>
#include <vector>
#include "Params.h"

namespace quimera
{
class WaveBank
{
public:
    static constexpr int size = 2048;     // amostras do quadro em banda cheia (mip 0)
    static constexpr int frames = 32;     // quadros por tabela (a POSIÇÃO anda entre eles)
    static constexpr int numMips = 11;    // mip m guarda só os harmônicos 1..harmonics(m)

    static constexpr int harmonics (int mip) { return (size / 2) >> mip; }
    // mip m tem mipSize(m) amostras (2× o mínimo, o que deixa a interpolação linear limpa); mip 0 = size
    static constexpr int mipSize (int mip)
    {
        return mip == 0 ? size : ((size >> mip) * 2 > 64 ? (size >> mip) * 2 : 64);
    }

    static const WaveBank& instance();    // monta na primeira chamada (estática local, thread-safe)

    // quadro inteiro (mip 0, banda cheia, `size` amostras), pico normalizado em ~1 — é o que a tela desenha
    const float* frame (int table, int frameIndex) const { return data (table, frameIndex, 0); }

    // quadro limitado em banda: mipSize(mip) amostras, um ciclo, sem amostra de guarda (o índice dá a volta)
    const float* data (int table, int frameIndex, int mip) const
    {
        table = juce::jlimit (0, numTables - 1, table);
        frameIndex = juce::jlimit (0, frames - 1, frameIndex);
        mip = juce::jlimit (0, numMips - 1, mip);
        return samples.data() + (size_t) (table * frames + frameIndex) * (size_t) perFrame + (size_t) mipOffset[(size_t) mip];
    }

    // mip para um incremento de fase (ciclos por amostra): o maior conjunto de harmônicos abaixo de Nyquist
    static int mipFor (double cyclesPerSample)
    {
        const double maxHarm = 0.5 / juce::jmax (1.0e-9, cyclesPerSample);
        int m = 0;
        while (m < numMips - 1 && (double) harmonics (m) > maxHarm) ++m;
        return m;
    }

private:
    WaveBank();
    std::vector<float> samples;
    int mipOffset[numMips] {};
    int perFrame = 0;   // soma dos mipSize
};
} // namespace quimera
