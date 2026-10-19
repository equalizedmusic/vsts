#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace miragem
{
// TOM do shimmer: quanto a cauda sobe (ou desce) a cada volta
inline constexpr int numIntervals = 4;
inline const char* const intervalNames[numIntervals] = { "Oitava", "Quinta", "Oitava + quinta", "Oitava abaixo" };

// JANELA do REVERSO no grid do host (em tempos de semínima)
inline constexpr int numRev = 4;
inline const char* const revNames[numRev] = { "1/4", "1/2", "1 comp", "2 comp" };
inline constexpr double revBeats[numRev]  = { 1.0, 2.0, 4.0, 8.0 };
inline constexpr double maxRevSec = 6.0;

inline constexpr int numLines = 8;
inline constexpr double maxPreSec = 0.3;

struct Settings
{
    double bpm = 145.0, ppq = 0.0;
    bool   synced = false;                     // host tocando: a janela do REVERSO cai no grid
    float  decay = 4.5f;                       // segundos até cair 60 dB
    float  size = 0.7f, predelayMs = 20.0f, dark = 0.4f, lowCutHz = 120.0f;
    float  shimmer = 0.35f, motion = 0.35f, duck = 0.0f;
    int    interval = 0, revLen = 1;
    bool   freeze = false, reverse = false;
};

// tamanho da sala: as 8 linhas crescem juntas (0,25× a 1,5× das medidas base)
inline float sizeFactor (float size01) { return 0.25f + 1.25f * juce::jlimit (0.0f, 1.0f, size01); }

// ESCURO: corte do filtro dentro de cada linha, 16 kHz → 1,3 kHz
inline float dampHz (float dark01) { return 16000.0f * std::pow (0.08f, juce::jlimit (0.0f, 1.0f, dark01)); }

// ---------------- pedaços ----------------
class Line
{
public:
    void prepare (int maxSamples)
    {
        int len = 1;
        while (len < maxSamples + 4) len <<= 1;
        buf.assign ((size_t) len, 0.0f);
        mask = len - 1;
        w = 0;
    }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float tap (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    float read (float d) const   // interpolação linear
    {
        const float pos = (float) w - juce::jmax (1.0f, d);
        const int   i0  = (int) std::floor (pos);
        const float t   = pos - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * t;
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

// difusor (passa-tudo de Schroeder): espalha o ataque antes de entrar na sala
class Allpass
{
public:
    void prepare (int samples) { d = juce::jmax (1, samples); line.prepare (d + 1); }
    float process (float x, float g)
    {
        const float z = line.tap (d);
        const float v = x + g * z;
        line.push (v);
        return z - g * v;
    }

private:
    Line line;
    int d = 1;
};

// pitch shift de duas cabeças com janela sen²: soma sempre 1, sem buraco no volume
class Shifter
{
public:
    void prepare (double fs)
    {
        win = (float) (0.05 * fs);
        line.prepare ((int) win + 8);
        ph = 0.0f;
    }
    float process (float x, float ratio)
    {
        line.push (x);
        float ph2 = ph + 0.5f;
        if (ph2 >= 1.0f) ph2 -= 1.0f;
        const float a = std::sin (juce::MathConstants<float>::pi * ph);
        const float b = std::sin (juce::MathConstants<float>::pi * ph2);
        const float y = line.read (ph * win + 2.0f) * a * a + line.read (ph2 * win + 2.0f) * b * b;
        ph += (1.0f - ratio) / win;
        ph -= std::floor (ph);
        return y;
    }

private:
    Line line;
    float win = 2205.0f, ph = 0.0f;
};

// histórico do nível da cauda: um pico a cada 10 ms (~5 s) — é a paisagem da tela
inline constexpr int histLen = 512;

class ReverbEngine
{
public:
    std::array<std::atomic<float>, histLen> hist {};
    std::atomic<int> histWrite { 0 };
    std::atomic<float> levelNow { 0.0f }, duckNow { 0.0f }, revPhase { 0.0f }, motionNow { 0.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxLine = (int) (fs * (baseMs[numLines - 1] * 0.001 * 1.5 + 0.006)) + 8;
        for (auto& l : lines) l.prepare (maxLine);
        for (auto& p : pre) p.prepare ((int) (fs * maxPreSec) + 8);
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < 4; ++k)
                diff[(size_t) ch][(size_t) k].prepare ((int) (fs * apMs[k] * (ch ? 1.07 : 1.0) * 0.001));
        for (auto& s : shift) s.prepare (fs);

        revLenMax = (int) (fs * maxRevSec);
        for (auto& side : rev)
            for (auto& b : side) b.assign ((size_t) revLenMax, 0.0f);
        revCur = 0; revW = 0; revLast = 0; revSeg = -1;

        lp.fill (0.0f);
        modPh.fill (0.0f);
        shHp.fill (0.0f); shHpX.fill (0.0f);
        cut1.fill (0.0f); cut2.fill (0.0f);
        env = 0.0f;
        sizeSm = -1.0f;
        primed = false;
        histStep = juce::jmax (1, (int) (fs * 0.01));
        histCount = 0; histPeak = 0.0f;
        for (auto& h : hist) h.store (0.0f);

        frzSm.reset (fs, 0.05);
        revSm.reset (fs, 0.03);
        shimSm.reset (fs, 0.05);
        duckSm.reset (fs, 0.02);
        preSm.reset (fs, 0.2);
        decaySm.reset (fs, 0.05);
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        if (! primed)
        {
            frzSm.setCurrentAndTargetValue (s.freeze ? 1.0f : 0.0f);
            revSm.setCurrentAndTargetValue (s.reverse ? 1.0f : 0.0f);
            shimSm.setCurrentAndTargetValue (s.shimmer);
            duckSm.setCurrentAndTargetValue (1.0f);
            preSm.setCurrentAndTargetValue (s.predelayMs * 0.001f * (float) fs);
            decaySm.setCurrentAndTargetValue (s.decay);
            primed = true;
        }
        frzSm.setTargetValue (s.freeze ? 1.0f : 0.0f);
        revSm.setTargetValue (s.reverse ? 1.0f : 0.0f);
        shimSm.setTargetValue (s.shimmer);
        preSm.setTargetValue (juce::jlimit (1.0f, (float) (fs * maxPreSec) - 4.0f, s.predelayMs * 0.001f * (float) fs));
        decaySm.setTargetValue (s.decay);

        const float sizeT = sizeFactor (s.size);
        if (sizeSm < 0.0f) sizeSm = sizeT;
        const float sizeGlide = 1.0f - std::exp (-1.0f / (0.25f * (float) fs));   // mexer no TAMANHO estica a sala
        const float lpC   = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (dampHz (s.dark), (float) (fs * 0.45)) / (float) fs);
        const float cutC  = std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (20.0f, 2000.0f, s.lowCutHz) / (float) fs);
        const float shHpC = std::exp (-juce::MathConstants<float>::twoPi * 250.0f / (float) fs);
        const float depth = s.motion * 0.0025f * (float) fs;   // até 2,5 ms de balanço em cada linha
        const float envAtt = 1.0f - std::exp (-1.0f / (0.005f * (float) fs));
        const float envRel = std::exp (-1.0f / (0.3f * (float) fs));

        float ratioA = 2.0f, ratioB = 2.0f, mixB = 0.0f;
        switch (s.interval)
        {
            case 1:  ratioA = ratioB = 1.4983f; break;              // quinta justa
            case 2:  ratioA = 2.0f; ratioB = 1.4983f; mixB = 0.5f; break;
            case 3:  ratioA = ratioB = 0.5f; break;
            default: break;
        }

        // janela do REVERSO: no grid quando o host toca; senão, conta sozinha
        const double beats   = revBeats[juce::jlimit (0, numRev - 1, s.revLen)];
        const int    segLen  = juce::jlimit ((int) (fs * 0.05), revLenMax, (int) (beats * 60.0 / juce::jmax (40.0, s.bpm) * fs));
        const double ppqStep = s.bpm / 60.0 / fs;
        const int    fadeN   = juce::jmax (1, (int) (fs * 0.006));

        float peak = 0.0f, gr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float frz = frzSm.getNextValue();
            const float shm = shimSm.getNextValue() * (1.0f - frz);
            const float dec = decaySm.getNextValue();
            sizeSm += (sizeT - sizeSm) * sizeGlide;

            // ---- duck: a cauda abaixa enquanto o seco está tocando ----
            const float x = juce::jmax (std::abs (inL[i]), std::abs (inR[i]));
            env = x > env ? env + (x - env) * envAtt : env * envRel;
            const float envDb = juce::Decibels::gainToDecibels (env, -100.0f);
            const float duckDb = -18.0f * s.duck * juce::jlimit (0.0f, 1.0f, (envDb + 40.0f) / 30.0f);
            duckSm.setTargetValue (juce::Decibels::decibelsToGain (duckDb));
            gr = juce::jmin (gr, duckDb);

            // ---- pré-delay e difusão (CONGELAR fecha a entrada) ----
            const float ig = 1.0f - frz;
            pre[0].push (inL[i] * ig);
            pre[1].push (inR[i] * ig);
            const float pd = preSm.getNextValue();
            float dL = pre[0].read (pd), dR = pre[1].read (pd);
            for (int k = 0; k < 4; ++k)
            {
                dL = diff[0][(size_t) k].process (dL, 0.62f);
                dR = diff[1][(size_t) k].process (dR, 0.62f);
            }

            // ---- sala: 8 linhas misturadas por Hadamard, cada uma com seu filtro e seu balanço ----
            std::array<float, numLines> y, f;
            for (int l = 0; l < numLines; ++l)
            {
                modPh[(size_t) l] += modHz[l] / (float) fs;
                if (modPh[(size_t) l] >= 1.0f) modPh[(size_t) l] -= 1.0f;
                const float len = (float) (baseMs[l] * 0.001 * fs) * sizeSm;
                const float m   = depth * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * modPh[(size_t) l]));
                y[(size_t) l] = lines[(size_t) l].read (len + m);

                const float g = std::pow (10.0f, -3.0f * len / ((float) fs * juce::jmax (0.1f, dec)));
                lp[(size_t) l] += (y[(size_t) l] - lp[(size_t) l]) * lpC;
                const float damped = lp[(size_t) l] + (y[(size_t) l] - lp[(size_t) l]) * frz;   // congelado: sem filtro
                f[(size_t) l] = damped * (g + (1.0f - g) * frz);
            }
            hadamard (f);

            const float oL = 0.5f * (y[0] + y[2] + y[4] + y[6]);
            const float oR = 0.5f * (y[1] + y[3] + y[5] + y[7]);

            // ---- shimmer: a saída sobe de tom e volta para a sala (cada volta sobe mais) ----
            std::array<float, 2> sh;
            for (int ch = 0; ch < 2; ++ch)
            {
                const float src = ch ? oR : oL;
                const float a = shift[(size_t) ch].process (src, ratioA);
                const float b = shift[(size_t) ch + 2].process (src, ratioB);
                const float v = a * (1.0f - mixB) + b * mixB;
                const float h = shHpC * (shHp[(size_t) ch] + v - shHpX[(size_t) ch]);   // sem grave embolando
                shHpX[(size_t) ch] = v;
                shHp[(size_t) ch]  = h;
                sh[(size_t) ch] = h;
            }

            for (int l = 0; l < numLines; ++l)
            {
                const bool left = (l % 2) == 0;
                const float in  = (left ? dL : dR) * 0.5f + (left ? sh[0] : sh[1]) * shm * 0.45f;
                lines[(size_t) l].push (sat (f[(size_t) l] + in));
            }

            // ---- molhado: corte de grave, um pouco do shimmer direto ----
            float w[2] = { oL + sh[0] * shm * 0.25f, oR + sh[1] * shm * 0.25f };
            for (int ch = 0; ch < 2; ++ch)
            {
                cut1[(size_t) ch] = cutC * cut1[(size_t) ch] + (1.0f - cutC) * w[ch];
                cut2[(size_t) ch] = cutC * cut2[(size_t) ch] + (1.0f - cutC) * cut1[(size_t) ch];
                w[ch] -= cut2[(size_t) ch];
            }

            // ---- REVERSO: grava uma janela e toca a anterior de trás para frente (a cauda cresce até o golpe) ----
            bool swap = revW >= revLenMax || (! s.synced && revW >= segLen);
            if (s.synced)
            {
                const auto seg = (int64_t) std::floor ((s.ppq + ppqStep * i) / beats);
                if (seg != revSeg) { swap = swap || revSeg >= 0; revSeg = seg; }
            }
            else
                revSeg = -1;
            if (swap)
            {
                revLast = revW;
                revCur ^= 1;
                revW = 0;
            }
            rev[(size_t) revCur][0][(size_t) revW] = w[0];
            rev[(size_t) revCur][1][(size_t) revW] = w[1];
            float rL = 0.0f, rR = 0.0f;
            if (revW < revLast)
            {
                const int idx = revLast - 1 - revW;
                const float fade = juce::jmin (1.0f, (float) revW / (float) fadeN, (float) (revLast - revW) / (float) fadeN);
                rL = rev[(size_t) (revCur ^ 1)][0][(size_t) idx] * fade;
                rR = rev[(size_t) (revCur ^ 1)][1][(size_t) idx] * fade;
            }
            ++revW;
            const float rv = revSm.getNextValue();
            const float dk = duckSm.getNextValue();
            outL[i] = (w[0] + (rL - w[0]) * rv) * dk;
            outR[i] = (w[1] + (rR - w[1]) * rv) * dk;

            const float pk = juce::jmax (std::abs (outL[i]), std::abs (outR[i]));
            peak = juce::jmax (peak, pk);
            histPeak = juce::jmax (histPeak, pk);
            if (++histCount >= histStep)
            {
                const int hw = histWrite.load (std::memory_order_relaxed);
                hist[(size_t) hw].store (histPeak, std::memory_order_relaxed);
                histWrite.store ((hw + 1) % histLen, std::memory_order_release);
                histCount = 0;
                histPeak = 0.0f;
            }
        }

        levelNow.store (juce::jmax (levelNow.load(), peak));
        duckNow.store (juce::jmin (duckNow.load(), gr));
        revPhase.store (revLast > 0 ? juce::jlimit (0.0f, 1.0f, (float) revW / (float) revLast) : 0.0f);
        motionNow.store (modPh[0]);
    }

private:
    static constexpr double baseMs[numLines] = { 29.7, 37.1, 41.1, 43.7, 53.1, 59.9, 67.7, 73.3 };
    static constexpr float  modHz[numLines]  = { 0.13f, 0.21f, 0.29f, 0.37f, 0.47f, 0.59f, 0.71f, 0.89f };
    static constexpr double apMs[4]          = { 4.7, 3.6, 12.7, 9.3 };

    static void hadamard (std::array<float, numLines>& v)
    {
        for (int h = 1; h < numLines; h *= 2)
            for (int i = 0; i < numLines; i += 2 * h)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[(size_t) j], b = v[(size_t) (j + h)];
                    v[(size_t) j] = a + b;
                    v[(size_t) (j + h)] = a - b;
                }
        for (auto& x : v) x *= 0.35355339f;   // 1/√8: conserva a energia
    }

    static float sat (float x)
    {
        // linear até 1 e curva suave depois: o shimmer longo não explode
        const float a = std::abs (x);
        if (a < 1.0f) return x;
        return std::copysign (1.0f + 0.5f * std::tanh ((a - 1.0f) / 0.5f), x);
    }

    double fs = 44100.0;
    std::array<Line, numLines> lines;
    std::array<Line, 2> pre;
    std::array<std::array<Allpass, 4>, 2> diff;
    std::array<Shifter, 4> shift;   // [0,1] = tom A esq/dir, [2,3] = tom B
    std::array<std::array<std::vector<float>, 2>, 2> rev;
    int revLenMax = 1, revCur = 0, revW = 0, revLast = 0;
    int64_t revSeg = -1;
    std::array<float, numLines> lp {}, modPh {};
    std::array<float, 2> shHp {}, shHpX {}, cut1 {}, cut2 {};
    float env = 0.0f, sizeSm = -1.0f;
    int histStep = 441, histCount = 0;
    float histPeak = 0.0f;
    bool primed = false;
    juce::SmoothedValue<float> frzSm, revSm, shimSm, duckSm, preSm, decaySm;
};
} // namespace miragem
