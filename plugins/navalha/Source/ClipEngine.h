#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace navalha
{
constexpr int historyCols = 480;   // colunas da forma de onda; cada uma ~8 ms ⇒ ~4 s na tela

// um lado do clipper, com joelho macio: t = |x|, c = teto, w = meia largura do joelho
// (abaixo de c − w passa igual; acima de c + w fica em c; no meio, curva sem quina)
inline float clipSide (float t, float c, float w)
{
    if (w <= 1.0e-6f) return juce::jmin (t, c);
    if (t <= c - w)   return t;
    if (t >= c + w)   return c;
    const float d = t - (c - w);
    return t - d * d / (4.0f * w);
}

struct Shape
{
    float ceiling = 1.0f, negCeiling = 1.0f, kneeW = 0.0f, negKneeW = 0.0f;
};

// TETO em dB; SUAVIDADE 0–1 = joelho até metade do teto; ASSIMETRIA 0–1 = lado negativo corta até 50% antes
inline Shape makeShape (float ceilingDb, float knee01, float asym01)
{
    Shape s;
    s.ceiling    = juce::Decibels::decibelsToGain (ceilingDb);
    s.negCeiling = s.ceiling * (1.0f - 0.5f * asym01);
    s.kneeW      = knee01 * 0.5f * s.ceiling;
    s.negKneeW   = knee01 * 0.5f * s.negCeiling;
    return s;
}

inline float clipSample (float x, const Shape& s)
{
    return x >= 0.0f ? clipSide (x, s.ceiling, s.kneeW) : -clipSide (-x, s.negCeiling, s.negKneeW);
}

struct Settings
{
    float drive = 0.0f, ceiling = -0.3f, knee = 0.2f, asym = 0.0f, link = 0.0f;   // dB, dB, 0–1, 0–1, 0–1
    bool  delta = false;
};

// ---------------------------------------------------------------------------------------------
// PRESSÃO (ganho) → 4× oversampling (FIR de fase linear) → corte por amostra (com LINK entre L/R)
// → volta para a taxa do projeto. A latência do oversampling é informada ao host; o seco e o sinal
// "empurrado" saem atrasados igual, então SÓ O CORTE (empurrado − cortado) é exatamente o que saiu.
// ---------------------------------------------------------------------------------------------
class ClipEngine
{
public:
    // ---- telemetria para a interface ----
    std::array<std::atomic<float>, historyCols> colIn {}, colOut {};   // pico de cada coluna: antes e depois do corte
    std::atomic<int>   colWrite { 0 };
    std::atomic<float> cutDb { 0.0f };       // maior corte desde a última leitura (dB, ≥ 0)
    std::atomic<float> clipShare { 0.0f };   // fração das amostras que encostaram no corte
    std::atomic<float> peakIn { 0.0f }, peakOut { 0.0f };

    int latency() const { return osLatency; }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        blockMax = juce::jmax (1, maxBlock);
        os.initProcessing ((size_t) blockMax);
        os.reset();
        osLatency = (int) std::lround (os.getLatencyInSamples());
        work.setSize (2, blockMax);
        gains.assign ((size_t) blockMax, 1.0f);

        int dl = 1;
        while (dl < osLatency + 8) dl <<= 1;
        ringMask = dl - 1;
        for (auto& r : dryRing) r.assign ((size_t) dl, 0.0f);
        for (auto& r : drvRing) r.assign ((size_t) dl, 0.0f);
        ringPos = 0;

        colLen = juce::jmax (1, (int) std::lround (fs * 0.008));
        colCount = 0;
        accIn = accOut = 0.0f;
        for (auto& c : colIn)  c.store (0.0f);
        for (auto& c : colOut) c.store (0.0f);
        colWrite.store (0);

        smDrive.reset (fs, 0.03);
        smDelta.reset (fs, 0.02);
        primed = false;
    }

    // out = cortado (ou só o corte); dry = entrada atrasada pela latência, para MIX e bypass
    void process (const float* inL, const float* inR, float* outL, float* outR, float* dryL, float* dryR, int n, const Settings& s)
    {
        for (int done = 0; done < n;)
        {
            const int m = juce::jmin (blockMax, n - done);
            processChunk (inL + done, inR + done, outL + done, outR + done, dryL + done, dryR + done, m, s);
            done += m;
        }
    }

private:
    void processChunk (const float* inL, const float* inR, float* outL, float* outR, float* dryL, float* dryR, int m, const Settings& s)
    {
        const float driveG = juce::Decibels::decibelsToGain (s.drive);
        if (! primed)
        {
            smDrive.setCurrentAndTargetValue (driveG);
            smDelta.setCurrentAndTargetValue (s.delta ? 1.0f : 0.0f);
            primed = true;
        }
        smDrive.setTargetValue (driveG);
        smDelta.setTargetValue (s.delta ? 1.0f : 0.0f);
        const auto shape = makeShape (s.ceiling, s.knee, s.asym);

        // ---- PRESSÃO e corte em 4× ----
        for (int i = 0; i < m; ++i)
            gains[(size_t) i] = smDrive.getNextValue();
        for (size_t ch = 0; ch < 2; ++ch)
        {
            const float* in = ch == 0 ? inL : inR;
            float* w = work.getWritePointer ((int) ch);
            for (int i = 0; i < m; ++i)
                w[i] = in[i] * gains[(size_t) i];
        }

        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) m);
        auto up = os.processSamplesUp (block);
        float* u0 = up.getChannelPointer (0);
        float* u1 = up.getChannelPointer (1);
        const size_t un = up.getNumSamples();
        int touched = 0;
        for (size_t j = 0; j < un; ++j)
        {
            const float x0 = u0[j], x1 = u1[j];
            const float y0 = clipSample (x0, shape), y1 = clipSample (x1, shape);
            if (y0 != x0 || y1 != x1) ++touched;

            if (s.link > 0.001f)
            {
                // LINK: os dois lados levam o mesmo corte (o maior), a imagem estéreo não mexe
                const float g0 = std::abs (x0) > 1.0e-9f ? y0 / x0 : 1.0f;
                const float g1 = std::abs (x1) > 1.0e-9f ? y1 / x1 : 1.0f;
                const float gl = juce::jmin (g0, g1);
                u0[j] = x0 * (g0 + (gl - g0) * s.link);
                u1[j] = x1 * (g1 + (gl - g1) * s.link);
            }
            else
            {
                u0[j] = y0;
                u1[j] = y1;
            }
        }
        os.processSamplesDown (block);

        // ---- saída, seco atrasado, desenho ----
        const float* wl = work.getReadPointer (0);
        const float* wr = work.getReadPointer (1);
        float pkIn = 0.0f, pkOut = 0.0f;
        for (int i = 0; i < m; ++i)
        {
            dryRing[0][(size_t) ringPos] = inL[i];
            dryRing[1][(size_t) ringPos] = inR[i];
            drvRing[0][(size_t) ringPos] = inL[i] * gains[(size_t) i];
            drvRing[1][(size_t) ringPos] = inR[i] * gains[(size_t) i];
            const int rp = (ringPos - osLatency) & ringMask;
            ringPos = (ringPos + 1) & ringMask;

            dryL[i] = dryRing[0][(size_t) rp];
            dryR[i] = dryRing[1][(size_t) rp];
            const float pl = drvRing[0][(size_t) rp], pr = drvRing[1][(size_t) rp];   // empurrado, alinhado
            const float yl = wl[i], yr = wr[i];

            const float dk = smDelta.getNextValue();   // SÓ O CORTE: empurrado − cortado
            outL[i] = yl + ((pl - yl) - yl) * dk;
            outR[i] = yr + ((pr - yr) - yr) * dk;

            const float a = juce::jmax (std::abs (pl), std::abs (pr));
            const float b = juce::jmax (std::abs (yl), std::abs (yr));
            accIn  = juce::jmax (accIn, a);
            accOut = juce::jmax (accOut, b);
            pkIn   = juce::jmax (pkIn, a);
            pkOut  = juce::jmax (pkOut, b);

            if (++colCount >= colLen)
            {
                const int c = colWrite.load (std::memory_order_relaxed);
                colIn[(size_t) c].store (accIn, std::memory_order_relaxed);
                colOut[(size_t) c].store (accOut, std::memory_order_relaxed);
                colWrite.store ((c + 1) % historyCols, std::memory_order_release);
                accIn = accOut = 0.0f;
                colCount = 0;
            }
        }

        // ---- telemetria (a interface lê e zera) ----
        const float cut = pkIn > 1.0e-6f && pkOut < pkIn ? juce::Decibels::gainToDecibels (pkIn / juce::jmax (pkOut, 1.0e-9f)) : 0.0f;
        cutDb.store (juce::jmax (cutDb.load(), cut));
        clipShare.store (juce::jmax (clipShare.load(), un > 0 ? (float) touched / (float) un : 0.0f));
        peakIn.store (juce::jmax (peakIn.load(), pkIn));
        peakOut.store (juce::jmax (peakOut.load(), pkOut));
    }

    double fs = 44100.0;
    int blockMax = 512, osLatency = 0;

    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    juce::AudioBuffer<float> work;
    std::vector<float> gains;
    juce::SmoothedValue<float> smDrive, smDelta;
    bool primed = false;

    std::array<std::vector<float>, 2> dryRing, drvRing;
    int ringPos = 0, ringMask = 0;

    int   colLen = 352, colCount = 0;
    float accIn = 0.0f, accOut = 0.0f;
};
} // namespace navalha
