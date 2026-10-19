#pragma once

#include "MeterEngine.h"
#include <array>
#include <atomic>
#include <cmath>
#include <utility>
#include <vector>

namespace medidor
{
// ---------------------------------------------------------------------------------------------
// Espectro (roda na thread da interface, nunca no áudio): pega as últimas 4096 amostras do anel,
// janela de Hann, FFT radix-2 própria e junta os bins em colunas em escala logarítmica
// (20 Hz a 20 kHz). "Ao vivo" sobe na hora e cai rápido; "média" anda devagar (~3 s).
// Escala: um seno de 0 dBFS marca 0 dB.
// ---------------------------------------------------------------------------------------------
class SpectrumAnalyzer
{
public:
    static constexpr int   order = 12, fftSize = 1 << order, cols = 220;
    static constexpr float floorDb = -120.0f;

    SpectrumAnalyzer()
        : re ((size_t) fftSize, 0.0f), im ((size_t) fftSize, 0.0f), win ((size_t) fftSize, 0.0f),
          cosT ((size_t) (fftSize / 2), 0.0f), sinT ((size_t) (fftSize / 2), 0.0f),
          rev ((size_t) fftSize, 0), power ((size_t) (fftSize / 2 + 1), 0.0f)
    {
        const double twoPi = juce::MathConstants<double>::twoPi;
        double sum = 0.0;
        for (int i = 0; i < fftSize; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (twoPi * (double) i / (double) (fftSize - 1));
            win[(size_t) i] = (float) w;
            sum += w;
        }
        winSum = (float) sum;

        for (int k = 0; k < fftSize / 2; ++k)
        {
            cosT[(size_t) k] = (float) std::cos (twoPi * (double) k / (double) fftSize);
            sinT[(size_t) k] = (float) std::sin (twoPi * (double) k / (double) fftSize);
        }

        for (int i = 0; i < fftSize; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b)
                if ((i & (1 << b)) != 0) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }

        live.fill (floorDb);
        avg.fill (floorDb);
    }

    // frequência da borda esquerda da coluna c (c = cols dá a borda direita da última)
    static double colEdge (int c) { return 20.0 * std::pow (1000.0, (double) c / (double) cols); }

    const std::array<float, cols>& liveDb() const { return live; }
    const std::array<float, cols>& avgDb() const  { return avg; }

    void update (const std::array<std::atomic<float>, fftRingLen>& ring, int writePos, double sampleRate, float dt)
    {
        for (int i = 0; i < fftSize; ++i)
        {
            const int idx = (writePos - fftSize + i + fftRingLen) & (fftRingLen - 1);
            re[(size_t) i] = ring[(size_t) idx].load (std::memory_order_relaxed) * win[(size_t) i];
            im[(size_t) i] = 0.0f;
        }
        transform();

        const float norm = 2.0f / juce::jmax (1.0f, winSum);
        for (int k = 0; k <= fftSize / 2; ++k)
        {
            const float a = re[(size_t) k] * norm, b = im[(size_t) k] * norm;
            power[(size_t) k] = a * a + b * b;
        }

        const double sr    = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double binHz = sr / (double) fftSize;
        const double nyq   = sr * 0.5;
        const float  fall  = juce::jmin (1.0f, dt * 8.0f);
        const float  slow  = juce::jmin (1.0f, dt / 3.0f);
        const int    last  = fftSize / 2;

        for (int c = 0; c < cols; ++c)
        {
            const double f0 = colEdge (c), f1 = colEdge (c + 1);
            float pw = 0.0f;
            if (f0 < nyq)
            {
                const double k0 = f0 / binHz, k1 = juce::jmin (f1, nyq) / binHz;
                const int a = (int) std::ceil (k0), b = (int) std::floor (k1);
                if (b >= a)
                {
                    for (int k = juce::jmax (1, a); k <= juce::jmin (last, b); ++k)
                        pw = juce::jmax (pw, power[(size_t) k]);
                }
                else
                {
                    // coluna mais estreita que um bin (graves): interpola entre os dois vizinhos
                    const double kc = 0.5 * (k0 + k1);
                    const int    k  = (int) std::floor (kc);
                    const float  t  = (float) (kc - (double) k);
                    const float  p0 = power[(size_t) juce::jlimit (0, last, k)];
                    const float  p1 = power[(size_t) juce::jlimit (0, last, k + 1)];
                    pw = p0 + (p1 - p0) * t;
                }
            }

            const float db = pw > 1.0e-13f ? 10.0f * std::log10 (pw) : floorDb;
            const auto  i  = (size_t) c;
            live[i] = db > live[i] ? db : live[i] + (db - live[i]) * fall;
            avg[i]  = primed ? avg[i] + (db - avg[i]) * slow : db;
        }
        primed = true;
    }

private:
    void transform()
    {
        for (int i = 0; i < fftSize; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j)
            {
                std::swap (re[(size_t) i], re[(size_t) j]);
                std::swap (im[(size_t) i], im[(size_t) j]);
            }
        }

        for (int len = 2; len <= fftSize; len <<= 1)
        {
            const int half = len / 2, step = fftSize / len;
            for (int i = 0; i < fftSize; i += len)
            {
                for (int j = 0; j < half; ++j)
                {
                    const float wr = cosT[(size_t) (j * step)], wi = -sinT[(size_t) (j * step)];
                    const auto  a  = (size_t) (i + j), b = (size_t) (i + j + half);
                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr;
                    im[b] = im[a] - ti;
                    re[a] += tr;
                    im[a] += ti;
                }
            }
        }
    }

    std::vector<float> re, im, win, cosT, sinT;
    std::vector<int>   rev;
    std::vector<float> power;
    float winSum = 1.0f;
    bool  primed = false;
    std::array<float, cols> live {}, avg {};
};
} // namespace medidor
