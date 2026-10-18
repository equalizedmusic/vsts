#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// A FITA: grava a entrada (mono) num buffer circular de 4 s e toca grãos lidos dele.
class GrainEngine
{
public:
    static constexpr int    maxGrains     = 96;
    static constexpr int    peakCols      = 400;      // colunas da forma de onda mostrada na tela
    static constexpr double bufferSeconds = 4.0;

    struct Settings
    {
        float sizeMs = 180.0f, density = 25.0f, sprayMs = 300.0f, pitchSt = 0.0f, jitterSt = 0.0f;
        float spread = 0.7f, feedback = 0.25f;
        bool  reverse = false, freeze = false;
    };

    // retrato dos grãos para a tela (escrito pelo áudio, lido pela interface)
    struct GrainView
    {
        std::atomic<bool>  active { false }, reverse { false };
        std::atomic<float> lag { 0.0f }, span { 0.0f }, pitch { 0.0f }, env { 0.0f };
    };

    GrainEngine()
    {
        for (int i = 0; i < winSize; ++i)
        {
            auto s = std::sin (juce::MathConstants<double>::pi * i / (winSize - 1));
            window[(size_t) i] = (float) (s * s);
        }
        window[(size_t) winSize] = 0.0f;
        for (auto& p : peaks) p.store (0.0f);
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        bufLen = juce::jmax (1024, (int) std::ceil (sr * bufferSeconds));
        buffer.assign ((size_t) bufLen, 0.0f);
        colLen = juce::jmax (1, bufLen / peakCols);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        samplesToNext = 0.0;
        colCount = 0;
        colPeak = 0.0f;
        peakIndex = 0;
        for (auto& g : grains) g.active = false;
        for (auto& v : views)  v.active.store (false);
        for (auto& p : peaks)  p.store (0.0f);
        peakWrite.store (0);
    }

    void process (const float* in, float* wetL, float* wetR, int numSamples, const Settings& s)
    {
        const double interval = sampleRate / juce::jmax (0.5f, s.density);
        const float  overlap  = s.density * s.sizeMs * 0.001f;
        const float  norm     = 1.0f / std::sqrt (juce::jmax (1.0f, overlap));

        for (int i = 0; i < numSamples; ++i)
        {
            float l = 0.0f, r = 0.0f;

            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float smp = read (g.pos) * win ((float) g.age / (float) g.len);
                l += smp * g.gainL;
                r += smp * g.gainR;
                g.pos += g.inc;
                if (g.pos >= bufLen) g.pos -= bufLen;
                else if (g.pos < 0.0) g.pos += bufLen;
                if (++g.age >= g.len) g.active = false;
            }

            if (! s.freeze)
            {
                // feedback: o que os grãos tocam volta para a fita (saturado para não explodir)
                const float v = in[i] + std::tanh (s.feedback * 0.5f * (l + r));
                buffer[(size_t) writePos] = v;
                colPeak = juce::jmax (colPeak, std::abs (v));
                if (++colCount >= colLen)
                {
                    peaks[(size_t) peakIndex].store (colPeak, std::memory_order_relaxed);
                    peakIndex = (peakIndex + 1) % peakCols;
                    peakWrite.store (peakIndex, std::memory_order_release);
                    colCount = 0;
                    colPeak = 0.0f;
                }
                if (++writePos >= bufLen) writePos = 0;
            }

            samplesToNext -= 1.0;
            if (samplesToNext <= 0.0)
            {
                spawn (s, norm);
                samplesToNext += interval * (0.6 + 0.8 * rng.nextDouble());
            }

            wetL[i] = l;
            wetR[i] = r;
        }

        publishViews();
    }

    std::array<GrainView, maxGrains>          views;
    std::array<std::atomic<float>, peakCols>  peaks;
    std::atomic<int>                          peakWrite { 0 };

private:
    struct Grain
    {
        bool   active = false;
        double pos = 0.0, inc = 1.0, rate = 1.0;
        int    len = 1, age = 0;
        float  gainL = 0.0f, gainR = 0.0f, st = 0.0f;
    };

    static constexpr int winSize = 1024;

    float win (float ph) const
    {
        const float x = juce::jlimit (0.0f, 1.0f, ph) * (winSize - 1);
        const int   i = (int) x;
        return window[(size_t) i] + (x - (float) i) * (window[(size_t) i + 1] - window[(size_t) i]);
    }

    float read (double pos) const
    {
        const int   i0 = (int) pos;
        const int   i1 = i0 + 1 >= bufLen ? 0 : i0 + 1;
        const float f  = (float) (pos - i0);
        return buffer[(size_t) i0] + f * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }

    void spawn (const Settings& s, float norm)
    {
        Grain* g = nullptr;
        for (auto& x : grains)
            if (! x.active) { g = &x; break; }
        if (g == nullptr) return;

        const float  st   = juce::jlimit (-36.0f, 36.0f, s.pitchSt + (rng.nextFloat() * 2.0f - 1.0f) * s.jitterSt);
        const double rate = std::pow (2.0, st / 12.0);
        const bool   rev  = s.reverse && rng.nextBool();
        double spray      = rng.nextDouble() * s.sprayMs * 0.001 * sampleRate;
        double len        = juce::jmax (16.0, s.sizeMs * 0.001 * sampleRate);
        const double margin = 64.0;

        // o grão não pode cruzar o cabeçote de gravação nem sair da fita
        auto need = [&] (double l) { return margin + spray + l * (rate + 1.0) + (rev ? 0.0 : l * juce::jmax (0.0, rate - 1.0)); };
        while (need (len) > bufLen - margin && len > 32.0) len *= 0.75;
        if (need (len) > bufLen - margin) spray = 0.0;

        const double delay = margin + spray + (rev ? 0.0 : len * juce::jmax (0.0, rate - 1.0));
        double pos = writePos - delay;
        while (pos < 0.0) pos += bufLen;

        const float p   = (rng.nextFloat() * 2.0f - 1.0f) * s.spread;
        const float ang = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;

        g->active = true;
        g->pos    = pos;
        g->rate   = rate;
        g->inc    = rev ? -rate : rate;
        g->len    = (int) len;
        g->age    = 0;
        g->st     = st;
        g->gainL  = std::cos (ang) * juce::MathConstants<float>::sqrt2 * norm;
        g->gainR  = std::sin (ang) * juce::MathConstants<float>::sqrt2 * norm;
    }

    void publishViews()
    {
        for (size_t k = 0; k < grains.size(); ++k)
        {
            const auto& g = grains[k];
            auto& v = views[k];
            if (g.active)
            {
                double lag = (double) writePos - g.pos;
                if (lag < 0.0) lag += bufLen;
                v.lag.store ((float) (lag / bufLen), std::memory_order_relaxed);
                v.span.store ((float) (g.len * g.rate / bufLen), std::memory_order_relaxed);
                v.pitch.store (g.st, std::memory_order_relaxed);
                v.env.store (win ((float) g.age / (float) g.len), std::memory_order_relaxed);
                v.reverse.store (g.inc < 0.0, std::memory_order_relaxed);
            }
            v.active.store (g.active, std::memory_order_release);
        }
    }

    double sampleRate = 44100.0;
    std::vector<float> buffer;
    int    bufLen = 1024, writePos = 0;
    double samplesToNext = 0.0;
    int    colLen = 1, colCount = 0, peakIndex = 0;
    float  colPeak = 0.0f;
    std::array<Grain, maxGrains> grains;
    std::array<float, winSize + 1> window {};
    juce::Random rng;
};
