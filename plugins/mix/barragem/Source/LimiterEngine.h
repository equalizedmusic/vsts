#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Loudness.h"

namespace barragem
{
constexpr int   historyCols = 480;    // colunas da forma de onda; cada uma ~8 ms ⇒ ~4 s na tela
constexpr float maxLookMs   = 5.0f;   // a janela de lookahead é sempre esta (latência fixa)

inline int nextPow2 (int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

// ---------------- mínimo numa janela deslizante (fila monotônica, O(1) por amostra) ----------------
class SlidingMin
{
public:
    void prepare (int window)
    {
        const int size = nextPow2 (window + 2);
        val.assign ((size_t) size, 1.0f);
        when.assign ((size_t) size, (juce::int64) 0);
        mask  = size - 1;
        front = 0;
        count = 0;
    }

    float push (juce::int64 now, float x, int window) noexcept
    {
        while (count > 0 && val[(size_t) ((front + count - 1) & mask)] >= x)
            --count;
        const int back = (front + count) & mask;
        val[(size_t) back]  = x;
        when[(size_t) back] = now;
        ++count;
        while (when[(size_t) front] <= now - window)
        {
            front = (front + 1) & mask;
            --count;
        }
        return val[(size_t) front];
    }

private:
    std::vector<float> val;
    std::vector<juce::int64> when;
    int mask = 0, front = 0, count = 0;
};

// ---------------- média móvel (a rampa do lookahead: desce reto até o pico, sem quina) ----------------
class BoxAverage
{
public:
    void prepare (int maxLen)
    {
        const int size = nextPow2 (maxLen + 2);
        buf.assign ((size_t) size, 1.0f);
        mask = size - 1;
        pos  = 0;
        len  = 1;
        sum  = 1.0;
    }

    void setLength (int n)
    {
        n = juce::jlimit (1, mask, n);
        if (n == len) return;
        len = n;
        resum();
    }

    float push (float x) noexcept
    {
        buf[(size_t) pos] = x;
        pos = (pos + 1) & mask;
        if (pos == 0) resum();   // de tempos em tempos soma de novo (sem acúmulo de erro)
        else          sum += (double) x - (double) buf[(size_t) ((pos - 1 - len) & mask)];
        return (float) (sum / (double) len);
    }

private:
    void resum()
    {
        sum = 0.0;
        for (int k = 1; k <= len; ++k)
            sum += (double) buf[(size_t) ((pos - k) & mask)];
    }

    std::vector<float> buf;
    int    mask = 0, pos = 0, len = 1;
    double sum = 1.0;
};

struct Settings
{
    float drive = 0.0f, ceiling = -1.0f, releaseMs = 100.0f, lookMs = 2.0f, link = 1.0f;   // dB, dBTP, ms, ms, 0–1
    bool  autoRel = true, delta = false;
};

// ---------------------------------------------------------------------------------------------
// ENCHER (ganho) → detecção em true peak (4× oversampling, só no caminho de detecção) → ganho
// necessário por canal (com LINK) → mínimo segurado na janela inteira → rampa do LOOKAHEAD
// (média móvel) → SOLTURA (fixa ou automática) → aplicado no áudio atrasado pela janela.
// O áudio não passa pelo filtro do oversampling: o que sai é o próprio sinal, só com ganho.
// Atraso fixo = 5 ms + latência do filtro (o host compensa uma vez, sem mudar ao girar o LOOKAHEAD).
// ---------------------------------------------------------------------------------------------
class LimiterEngine
{
public:
    // ---- telemetria para a interface ----
    std::array<std::atomic<float>, historyCols> colIn {}, colOut {}, colGr {};   // pico antes/depois e redução (dB) por coluna
    std::atomic<int>   colWrite { 0 };
    std::atomic<float> grDb { 0.0f };                      // maior redução desde a última leitura (dB, ≥ 0)
    std::atomic<float> peakIn { 0.0f }, peakOut { 0.0f };  // depois do ENCHER e na saída do limiter
    LoudnessMeter loud;                                    // LUFS da saída do limiter

    int latency() const { return delay; }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        blockMax = juce::jmax (1, maxBlock);
        os.initProcessing ((size_t) blockMax);
        os.reset();
        const int osLat = (int) std::ceil (os.getLatencyInSamples());

        lookMax = juce::jmax (1, (int) std::ceil ((double) maxLookMs * 0.001 * fs));
        delay   = lookMax + osLat;
        holdLen = delay + 1;

        work.setSize (2, blockMax);
        gains.assign ((size_t) blockMax, 1.0f);

        const int rs = nextPow2 (delay + 2);
        ringMask = rs - 1;
        for (auto& r : dryRing) r.assign ((size_t) rs, 0.0f);
        for (auto& r : drvRing) r.assign ((size_t) rs, 0.0f);
        ringPos = 0;

        for (auto& m : minWin) m.prepare (holdLen);
        for (auto& b : box) b.prepare (lookMax);
        env.fill (1.0f);
        avgG = 1.0f;
        autoCoef = 0.0f;
        avgA = (float) (1.0 - std::exp (-1.0 / (0.8 * fs)));
        now = 0;

        colLen = juce::jmax (1, (int) std::lround (fs * 0.008));
        colCount = 0;
        accIn = accOut = 0.0f;
        accG = 1.0f;
        for (auto& c : colIn)  c.store (0.0f);
        for (auto& c : colOut) c.store (0.0f);
        for (auto& c : colGr)  c.store (0.0f);
        colWrite.store (0);

        loud.prepare (fs);
        smDrive.reset (fs, 0.03);
        smDelta.reset (fs, 0.02);
        primed = false;
    }

    // out = limitado (ou só a diferença); dry = entrada atrasada pela latência, para o bypass
    void process (const float* inL, const float* inR, float* outL, float* outR, float* dryL, float* dryR, int n, const Settings& s)
    {
        if (loud.resetRequest.exchange (false))
            loud.resetIntegrated();

        for (int done = 0; done < n;)
        {
            const int m = juce::jmin (blockMax, n - done);
            processChunk (inL + done, inR + done, outL + done, outR + done, dryL + done, dryR + done, m, s);
            done += m;
        }
    }

private:
    float releaseCoef (float sec) const
    {
        return (float) std::exp (-1.0 / (juce::jmax (0.0005, (double) sec) * fs));
    }

    void updateAutoCoef (float relSec)
    {
        // AUTO: pico isolado solta rápido; quando o limiter trabalha o tempo todo, solta devagar (não bombeia)
        const float avgDb = -juce::Decibels::gainToDecibels (avgG, -60.0f);
        const float k = juce::jlimit (0.3f, 4.0f, 0.3f + 0.5f * avgDb);
        autoCoef = releaseCoef (relSec * k);
    }

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

        const float ceilG  = juce::Decibels::decibelsToGain (s.ceiling);
        const int   look   = juce::jlimit (1, lookMax, (int) std::lround ((double) s.lookMs * 0.001 * fs));
        const float relSec = juce::jmax (0.001f, s.releaseMs * 0.001f);
        const float fixedCoef = releaseCoef (relSec);
        const float link = juce::jlimit (0.0f, 1.0f, s.link);
        for (auto& b : box) b.setLength (look);
        if (s.autoRel) updateAutoCoef (relSec);

        // ---- ENCHER ----
        for (int i = 0; i < m; ++i)
            gains[(size_t) i] = smDrive.getNextValue();
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* in = ch == 0 ? inL : inR;
            float* w = work.getWritePointer (ch);
            for (int i = 0; i < m; ++i)
                w[i] = in[i] * gains[(size_t) i];
        }

        // ---- detecção em 4× (o áudio não passa por aqui) ----
        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) m);
        auto up = os.processSamplesUp (block);
        const float* u0 = up.getChannelPointer (0);
        const float* u1 = up.getChannelPointer (1);
        const size_t factor = juce::jmax ((size_t) 1, up.getNumSamples() / (size_t) m);

        const float* wl = work.getReadPointer (0);
        const float* wr = work.getReadPointer (1);
        float pkIn = 0.0f, pkOut = 0.0f, minG = 1.0f;
        for (int i = 0; i < m; ++i)
        {
            float dl = std::abs (wl[i]), dr = std::abs (wr[i]);
            const size_t base = (size_t) i * factor;
            for (size_t k = 0; k < factor; ++k)
            {
                dl = juce::jmax (dl, std::abs (u0[base + k]));
                dr = juce::jmax (dr, std::abs (u1[base + k]));
            }

            // ganho necessário para o pico (entre amostras) ficar no TETO; LINK puxa os dois para o menor
            float gl = dl > ceilG ? ceilG / dl : 1.0f;
            float gr = dr > ceilG ? ceilG / dr : 1.0f;
            const float gm = juce::jmin (gl, gr);
            gl += (gm - gl) * link;
            gr += (gm - gr) * link;

            const float hl = minWin[0].push (now, gl, holdLen);
            const float hr = minWin[1].push (now, gr, holdLen);
            ++now;
            const float bl = box[0].push (hl);
            const float br = box[1].push (hr);

            if (s.autoRel && (now & 31) == 0) updateAutoCoef (relSec);
            const float coef = s.autoRel ? autoCoef : fixedCoef;
            env[0] = bl < env[0] ? bl : bl + (env[0] - bl) * coef;
            env[1] = br < env[1] ? br : br + (env[1] - br) * coef;
            const float gNow = juce::jmin (env[0], env[1]);
            avgG += (gNow - avgG) * avgA;

            // ---- áudio atrasado pela janela ----
            dryRing[0][(size_t) ringPos] = inL[i];
            dryRing[1][(size_t) ringPos] = inR[i];
            drvRing[0][(size_t) ringPos] = wl[i];
            drvRing[1][(size_t) ringPos] = wr[i];
            const int rp = (ringPos - delay) & ringMask;
            ringPos = (ringPos + 1) & ringMask;

            dryL[i] = dryRing[0][(size_t) rp];
            dryR[i] = dryRing[1][(size_t) rp];
            const float xl = drvRing[0][(size_t) rp], xr = drvRing[1][(size_t) rp];
            const float yl = juce::jlimit (-ceilG, ceilG, xl * env[0]);   // trava final de segurança
            const float yr = juce::jlimit (-ceilG, ceilG, xr * env[1]);
            loud.push (yl, yr);

            const float dk = smDelta.getNextValue();   // OUVIR DIFERENÇA: cheio − limitado
            outL[i] = yl + ((xl - yl) - yl) * dk;
            outR[i] = yr + ((xr - yr) - yr) * dk;

            const float a = juce::jmax (std::abs (xl), std::abs (xr));
            const float b = juce::jmax (std::abs (yl), std::abs (yr));
            accIn  = juce::jmax (accIn, a);
            accOut = juce::jmax (accOut, b);
            accG   = juce::jmin (accG, gNow);
            pkIn   = juce::jmax (pkIn, a);
            pkOut  = juce::jmax (pkOut, b);
            minG   = juce::jmin (minG, gNow);

            if (++colCount >= colLen)
            {
                const int c = colWrite.load (std::memory_order_relaxed);
                colIn[(size_t) c].store (accIn, std::memory_order_relaxed);
                colOut[(size_t) c].store (accOut, std::memory_order_relaxed);
                colGr[(size_t) c].store (juce::jmax (0.0f, -juce::Decibels::gainToDecibels (accG, -60.0f)), std::memory_order_relaxed);
                colWrite.store ((c + 1) % historyCols, std::memory_order_release);
                accIn = accOut = 0.0f;
                accG = 1.0f;
                colCount = 0;
            }
        }

        // ---- telemetria (a interface lê e zera) ----
        grDb.store (juce::jmax (grDb.load(), juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minG, -60.0f))));
        peakIn.store (juce::jmax (peakIn.load(), pkIn));
        peakOut.store (juce::jmax (peakOut.load(), pkOut));
    }

    double fs = 44100.0;
    int blockMax = 512, lookMax = 1, delay = 0, holdLen = 1;

    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    juce::AudioBuffer<float> work;
    std::vector<float> gains;
    juce::SmoothedValue<float> smDrive, smDelta;
    bool primed = false;

    std::array<SlidingMin, 2> minWin;
    std::array<BoxAverage, 2> box;
    std::array<float, 2> env { { 1.0f, 1.0f } };
    float avgG = 1.0f, avgA = 0.0f, autoCoef = 0.0f;
    juce::int64 now = 0;

    std::array<std::vector<float>, 2> dryRing, drvRing;
    int ringPos = 0, ringMask = 0;

    int   colLen = 352, colCount = 0;
    float accIn = 0.0f, accOut = 0.0f, accG = 1.0f;
};
} // namespace barragem
