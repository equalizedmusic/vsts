#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace sala
{
// PRÉ-DELAY no tempo do projeto (em tempos de semínima); 0 = livre, em ms
inline constexpr int numSync = 6;
inline const char* const syncNames[numSync] = { "Livre", "1/64", "1/32", "1/16", "1/8", "1/4" };
inline constexpr double syncBeats[numSync]  = { 0.0, 0.0625, 0.125, 0.25, 0.5, 1.0 };

inline constexpr int    numLines = 8;
inline constexpr int    numTaps  = 16;
inline constexpr double maxPreSec = 1.0;
inline constexpr float  maxDecay  = 12.0f;
inline constexpr float  modMs     = 0.35f;   // balanço sutil das linhas (tira o som metálico)

// reflexões iniciais: tempos base (ms) de cada lado e ganho de cada uma (sinal alternado)
inline constexpr float tapMsL[numTaps]  = { 4.3f, 7.9f, 11.2f, 14.7f, 19.1f, 23.8f, 27.3f, 32.6f,
                                            37.4f, 41.9f, 47.2f, 52.8f, 58.1f, 63.9f, 70.3f, 77.6f };
inline constexpr float tapMsR[numTaps]  = { 5.1f, 8.6f, 12.4f, 16.3f, 20.2f, 24.9f, 29.5f, 33.8f,
                                            39.1f, 44.0f, 48.9f, 54.7f, 60.2f, 66.1f, 72.4f, 79.0f };
inline constexpr float tapGain[numTaps] = { 0.85f, -0.78f, 0.72f, -0.66f, 0.60f, -0.55f, 0.50f, -0.46f,
                                            0.42f, -0.38f, 0.35f, -0.32f, 0.29f, -0.26f, 0.24f, -0.22f };

// medidas base das 8 linhas da sala (ms), primos entre si
inline constexpr float lineMs[numLines] = { 31.3f, 37.9f, 41.7f, 47.3f, 53.9f, 59.3f, 67.1f, 73.7f };
inline constexpr float modHz[numLines]  = { 0.11f, 0.17f, 0.23f, 0.29f, 0.37f, 0.43f, 0.53f, 0.61f };
inline constexpr float apMs[4]          = { 4.7f, 3.6f, 12.7f, 9.3f };

struct Settings
{
    float predelayMs = 15.0f;   // já resolvido (livre ou no tempo)
    float size = 0.5f, decay = 1.6f, diffusion = 0.75f, early = 0.5f;   // 0–1, s, 0–1, 0–1
    float lowCutHz = 120.0f, highCutHz = 12000.0f, damp = 0.35f;        // Hz, Hz, 0–1
    float duck = 0.0f, width = 1.0f;                                     // 0–1, 0–1,5
};

// TAMANHO: as 8 linhas crescem juntas (0,3× a 1,6×); as reflexões iniciais, menos (0,4× a 1,4×)
inline float sizeFactor (float size01) { return 0.3f + 1.3f * juce::jlimit (0.0f, 1.0f, size01); }
inline float erFactor (float size01)   { return 0.4f + 1.0f * juce::jlimit (0.0f, 1.0f, size01); }

// AMORTECIMENTO: corte do filtro dentro de cada linha, 20 kHz → 1 kHz
inline float dampHz (float damp01) { return 20000.0f * std::pow (0.05f, juce::jlimit (0.0f, 1.0f, damp01)); }

// REFLEXÕES (densidade): quantas das 16 reflexões entram (com transição suave entre uma e outra)
inline float tapWeight (int k, float early01) { return juce::jlimit (0.0f, 1.0f, early01 * (float) numTaps - (float) k); }

// coeficiente do filtro de um polo
inline float onePole (float hz, float fs) { return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / fs); }

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
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float tap (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    float read (float d) const   // interpolação linear
    {
        const float pos = (float) w - juce::jlimit (1.0f, (float) (mask - 2), d);
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
    void clear() { line.clear(); }
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

// ---------------------------------------------------------------------------------------------
// PRÉ-DELAY → REFLEXÕES INICIAIS (16 ecos por lado) + DIFUSÃO (4 passa-tudo) → SALA (rede de
// 8 linhas misturadas por Hadamard, cada uma com amortecimento e um balanço lento) → LARGURA →
// EQ DA CAUDA (passa-altas e passa-baixas de 2 polos) → DUCK. Sem latência. A realimentação de
// cada linha é 10^(−3·L/(fs·decay)), sempre < 1, e a matriz conserva a energia: a cauda sempre cai.
// ---------------------------------------------------------------------------------------------
class RoomEngine
{
public:
    // ---- telemetria para a interface (a interface lê e zera) ----
    std::atomic<float> levelNow { 0.0f };   // pico do molhado
    std::atomic<float> duckNow { 0.0f };    // quanto o DUCK abaixou (dB, ≤ 0)

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const float f = (float) fs;
        const int maxLine = (int) (f * 0.001f * (lineMs[numLines - 1] * sizeFactor (1.0f) + 2.0f * modMs)) + 8;
        for (auto& l : lines) l.prepare (maxLine);
        for (auto& p : pre) p.prepare ((int) (fs * maxPreSec) + 8);
        const int maxEr = (int) (f * 0.001f * juce::jmax (tapMsL[numTaps - 1], tapMsR[numTaps - 1]) * erFactor (1.0f)) + 8;
        for (auto& e : er) e.prepare (maxEr);
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < 4; ++k)
                diff[(size_t) ch][(size_t) k].prepare ((int) (f * apMs[k] * (ch == 1 ? 1.09f : 1.0f) * 0.001f));

        preSm.reset (fs, 0.2);
        decaySm.reset (fs, 0.05);
        diffSm.reset (fs, 0.05);
        widthSm.reset (fs, 0.05);
        duckSm.reset (fs, 0.02);
        clearState();
        primed = false;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        const float f   = (float) fs;
        const float nyq = 0.45f * f;
        const float preT = juce::jlimit (1.0f, f * (float) maxPreSec, s.predelayMs * 0.001f * f);
        const float decT = juce::jlimit (0.1f, maxDecay, s.decay);
        const float sizeT = sizeFactor (s.size), erT = erFactor (s.size);

        if (! primed)
        {
            preSm.setCurrentAndTargetValue (preT);
            decaySm.setCurrentAndTargetValue (decT);
            diffSm.setCurrentAndTargetValue (s.diffusion);
            widthSm.setCurrentAndTargetValue (s.width);
            duckSm.setCurrentAndTargetValue (1.0f);
            sizeSm = sizeT;
            erSm = erT;
            for (int k = 0; k < numTaps; ++k)
                twSm[(size_t) k] = tapWeight (k, s.early);
            primed = true;
        }
        preSm.setTargetValue (preT);
        decaySm.setTargetValue (decT);
        diffSm.setTargetValue (s.diffusion);
        widthSm.setTargetValue (s.width);

        std::array<float, numTaps> tw;
        for (int k = 0; k < numTaps; ++k)
            tw[(size_t) k] = tapWeight (k, s.early);

        const float glide   = 1.0f - std::exp (-1.0f / (0.25f * f));   // mexer no TAMANHO estica a sala
        const float twGlide = 1.0f - std::exp (-1.0f / (0.03f * f));
        const float dampC = onePole (juce::jmin (dampHz (s.damp), nyq), f);
        const float lowC  = onePole (juce::jlimit (10.0f, nyq, s.lowCutHz), f);
        const float highC = onePole (juce::jlimit (10.0f, nyq, s.highCutHz), f);
        const float depth = modMs * 0.001f * f;
        const float envAtt = 1.0f - std::exp (-1.0f / (0.005f * f));
        const float envRel = std::exp (-1.0f / (0.25f * f));
        const float msToSamples = 0.001f * f;
        const float erScale = 0.35f;

        float peak = 0.0f, gr = 0.0f;
        bool bad = false;
        for (int i = 0; i < n; ++i)
        {
            const float pd  = preSm.getNextValue();
            const float dec = decaySm.getNextValue();
            const float dif = diffSm.getNextValue();
            const float wid = widthSm.getNextValue();
            sizeSm += (sizeT - sizeSm) * glide;
            erSm   += (erT - erSm) * glide;

            // ---- DUCK: a cauda abaixa enquanto o seco está tocando ----
            const float x = juce::jmax (std::abs (inL[i]), std::abs (inR[i]));
            env = x > env ? env + (x - env) * envAtt : env * envRel;
            const float envDb  = juce::Decibels::gainToDecibels (env, -100.0f);
            const float duckDb = -24.0f * s.duck * juce::jlimit (0.0f, 1.0f, (envDb + 40.0f) / 30.0f);
            duckSm.setTargetValue (juce::Decibels::decibelsToGain (duckDb));
            gr = juce::jmin (gr, duckDb);

            // ---- PRÉ-DELAY ----
            pre[0].push (inL[i]);
            pre[1].push (inR[i]);
            const float pL = pre[0].read (pd), pR = pre[1].read (pd);

            // ---- REFLEXÕES INICIAIS ----
            er[0].push (pL);
            er[1].push (pR);
            float eL = 0.0f, eR = 0.0f;
            for (int k = 0; k < numTaps; ++k)
            {
                float& wk = twSm[(size_t) k];
                wk += (tw[(size_t) k] - wk) * twGlide;
                if (wk < 1.0e-5f) continue;
                const float gk = tapGain[k] * wk * erScale;
                eL += er[0].read (tapMsL[k] * msToSamples * erSm) * gk;
                eR += er[1].read (tapMsR[k] * msToSamples * erSm) * gk * (k % 3 == 0 ? -1.0f : 1.0f);
            }

            // ---- DIFUSÃO ----
            const float ag = 0.75f * dif;
            float dL = pL, dR = pR;
            for (int k = 0; k < 4; ++k)
            {
                dL = diff[0][(size_t) k].process (dL, ag);
                dR = diff[1][(size_t) k].process (dR, ag);
            }

            // ---- SALA: 8 linhas misturadas por Hadamard ----
            std::array<float, numLines> y, fb;
            for (int l = 0; l < numLines; ++l)
            {
                float& ph = modPh[(size_t) l];
                ph += modHz[l] / f;
                if (ph >= 1.0f) ph -= 1.0f;
                const float len = lineMs[l] * msToSamples * sizeSm;
                const float m   = depth * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * ph));
                y[(size_t) l] = lines[(size_t) l].read (len + m);

                const float g = std::pow (10.0f, -3.0f * (len + m) / (f * dec));   // < 1 sempre
                float& lp = damp[(size_t) l];
                lp += (y[(size_t) l] - lp) * dampC;
                fb[(size_t) l] = lp * g;
            }
            hadamard (fb);
            for (int l = 0; l < numLines; ++l)
            {
                const float in = ((l % 2) == 0 ? dL : dR) * 0.5f;
                lines[(size_t) l].push (fb[(size_t) l] + in);
            }

            // cauda longa soma mais energia: compensa um pouco para o MIX não pular de volume
            const float lateGain = 0.5f / std::sqrt (1.0f + 0.5f * dec);
            const float oL = (y[0] + y[2] + y[4] + y[6]) * lateGain;
            const float oR = (y[1] + y[3] + y[5] + y[7]) * lateGain;

            // ---- LARGURA (meio/lado do molhado) ----
            float w[2] = { eL + oL, eR + oR };
            const float mid  = 0.5f * (w[0] + w[1]);
            const float side = 0.5f * (w[0] - w[1]) * wid;
            w[0] = mid + side;
            w[1] = mid - side;

            // ---- EQ DA CAUDA: passa-altas e passa-baixas, 2 polos cada ----
            for (int ch = 0; ch < 2; ++ch)
            {
                const size_t c = (size_t) ch;
                hp1[c] += (w[ch] - hp1[c]) * lowC;
                float h = w[ch] - hp1[c];
                hp2[c] += (h - hp2[c]) * lowC;
                h -= hp2[c];
                lp1[c] += (h - lp1[c]) * highC;
                lp2[c] += (lp1[c] - lp2[c]) * highC;
                w[ch] = lp2[c];
            }

            const float dk = duckSm.getNextValue();
            outL[i] = w[0] * dk;
            outR[i] = w[1] * dk;
            const float aL = std::abs (outL[i]), aR = std::abs (outR[i]);
            if (! (aL < 1.0e6f && aR < 1.0e6f)) bad = true;   // NaN cai aqui também (comparação falsa)
            peak = juce::jmax (peak, aL, aR);
        }

        // proteção: se algo virou NaN/infinito, zera a sala e devolve silêncio
        if (bad)
        {
            clearState();
            for (int i = 0; i < n; ++i)
                outL[i] = outR[i] = 0.0f;
            peak = 0.0f;
        }

        levelNow.store (juce::jmax (levelNow.load(), peak));
        duckNow.store (juce::jmin (duckNow.load(), gr));
    }

private:
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

    void clearState()
    {
        for (auto& l : lines) l.clear();
        for (auto& p : pre) p.clear();
        for (auto& e : er) e.clear();
        for (auto& side : diff)
            for (auto& a : side) a.clear();
        damp.fill (0.0f);
        modPh.fill (0.0f);
        twSm.fill (0.0f);
        hp1.fill (0.0f); hp2.fill (0.0f); lp1.fill (0.0f); lp2.fill (0.0f);
        env = 0.0f;
    }

    double fs = 44100.0;
    std::array<Line, numLines> lines;
    std::array<Line, 2> pre, er;
    std::array<std::array<Allpass, 4>, 2> diff;
    std::array<float, numLines> damp {}, modPh {};
    std::array<float, numTaps> twSm {};
    std::array<float, 2> hp1 {}, hp2 {}, lp1 {}, lp2 {};
    float env = 0.0f, sizeSm = 1.0f, erSm = 1.0f;
    bool primed = false;
    juce::SmoothedValue<float> preSm, decaySm, diffSm, widthSm, duckSm;
};
} // namespace sala
