#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace picote
{
inline constexpr int numSteps = 16;

// DIVISÃO: tamanho de cada passo no grid do host (em tempos de semínima)
inline constexpr int numDivs = 5;
inline const char* const divNames[numDivs] = { "1/8", "1/8T", "1/16", "1/16T", "1/32" };
inline constexpr double divBeats[numDivs]  = { 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

enum FilterType { filtLow, filtHigh, filtBand };
inline constexpr int numFilters = 3;
inline const char* const filterNames[numFilters] = { "Low-pass", "High-pass", "Band-pass" };

inline constexpr double maxHoldSec = 2.0;

struct Settings
{
    double bpm = 145.0, ppq = 0.0;
    bool   waiting = false;                    // host parado: o padrão fica no passo do cursor
    int    div = 2, length = 16, offset = 0, filtType = filtLow;
    float  depth = 1.0f, swing = 0.0f, width = 0.6f, attackMs = 2.0f, releaseMs = 15.0f, reso = 0.25f;
    std::array<float, numSteps> vol {}, pan {}, flt {};   // vol 0..1, pan −1..1, flt 0..1
    bool   hold = false;
};

// onde o grid está: passo (já com swing) e quanto dele passou
struct StepPos
{
    int64_t index = 0;     // passo absoluto desde o começo da música
    double  frac = 0.0;    // 0..1 dentro do passo
    double  lenBeats = 0.25;
};

inline StepPos stepAt (double ppq, double beatsPerStep, float swing01)
{
    // swing empurra o passo par: o primeiro do par dura 1 + s, o segundo 1 − s (s até 0,5)
    const double s = 0.5 * juce::jlimit (0.0, 1.0, (double) swing01);
    const double steps = ppq / beatsPerStep;
    const double pair = std::floor (steps * 0.5);
    const double u = steps - pair * 2.0;
    StepPos p;
    if (u < 1.0 + s) { p.index = (int64_t) pair * 2;     p.frac = u / (1.0 + s);               p.lenBeats = beatsPerStep * (1.0 + s); }
    else             { p.index = (int64_t) pair * 2 + 1; p.frac = (u - 1.0 - s) / (1.0 - s);   p.lenBeats = beatsPerStep * (1.0 - s); }
    return p;
}

// passo do padrão que toca no passo absoluto 'index' (PASSOS limita, DESLOCAR gira)
inline int patternStep (int64_t index, int length, int offset)
{
    length = juce::jlimit (1, numSteps, length);
    const auto k = (index + offset) % length;
    return (int) (k < 0 ? k + length : k);
}

// FILTRO de cada passo: 0 = fechado, 1 = aberto (no passa-faixa, 0 = grave e 1 = agudo)
inline float stepCutoff (float open01, int type)
{
    open01 = juce::jlimit (0.0f, 1.0f, open01);
    switch (type)
    {
        case filtHigh: return 20.0f * std::pow (200.0f, 1.0f - open01);    // 4 kHz → 20 Hz
        case filtBand: return 200.0f * std::pow (40.0f, open01);           // 200 Hz → 8 kHz
        default:       return 150.0f * std::pow (133.0f, open01);          // 150 Hz → 20 kHz
    }
}

// envelope de um passo (desenho do editor): sobe no ATTACK a partir de 'start', fica aberto até a LARGURA e cai no RELEASE
inline float stepEnvelope (double tSec, double stepSec, float width01, float attackMs, float releaseMs, float start = 0.0f)
{
    const double open = juce::jmax (0.001, stepSec * (double) width01);
    const double att  = juce::jmax (0.001, attackMs * 0.001);
    const double rel  = juce::jmax (0.003, releaseMs * 0.001);
    double e = start + (1.0 - start) * juce::jmin (1.0, tSec / att);
    if (tSec > open) e *= std::exp (-(tSec - open) / (rel * 0.35));
    return (float) e;
}

class GateEngine
{
public:
    std::atomic<int>   stepNow { 0 };
    std::atomic<float> fracNow { 0.0f }, gainNow { 1.0f };
    std::array<std::atomic<float>, numSteps> stepPeak {};   // o que passou por cada passo na última volta
    std::atomic<bool>  holding { false };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        holdLenMax = (int) (fs * maxHoldSec);
        holdSeamMax = juce::jmax (16, (int) (0.004 * fs));
        for (auto& b : holdBuf) b.assign ((size_t) (holdLenMax + holdSeamMax), 0.0f);
        holdState = holdOff;
        holdW = 0; holdLen = 0; holdSeam = 0;
        ic1.fill (0.0f); ic2.fill (0.0f);
        gainSm = 1.0f; panSm = 0.0f; cutSm = -1.0f; openSm = -1.0f; lvl = 0.0f;
        lastIdx = -1; stepAcc = 0.0f;
        freePpq = 0.0;
        for (auto& p : stepPeak) p.store (0.0f);
        holdMix.reset (fs, 0.005);
        holdMix.setCurrentAndTargetValue (0.0f);
        smGain = 1.0f - std::exp (-1.0f / (0.0004f * (float) fs));   // 0,4 ms: tira o clique sem amolecer o corte
        smPan  = 1.0f - std::exp (-1.0f / (0.003f * (float) fs));
        smCut  = 1.0f - std::exp (-1.0f / (0.002f * (float) fs));
    }

    // own = true quando o host não dá a régua: o motor conta o tempo sozinho
    void process (float* l, float* r, int n, const Settings& s, bool own)
    {
        const double beatsPerStep = divBeats[juce::jlimit (0, numDivs - 1, s.div)];
        const double ppqStep = s.waiting ? 0.0 : s.bpm / 60.0 / fs;
        const double basePpq = own ? freePpq : s.ppq;
        const float  q = 0.6f + 9.0f * s.reso * s.reso;
        const float  k = 1.0f / q;
        // envelope contínuo: sobe em reta no ATTACK e cai em curva no RELEASE, sempre a partir de onde está.
        // Recomeçar do zero em cada passo cortava o som na virada (o estalo, pior com LARGURA alta ou RELEASE longo)
        const float riseStep = 1.0f / (juce::jmax (0.001f, s.attackMs * 0.001f) * (float) fs);
        const float fallCoef = 1.0f - std::exp (-1.0f / (juce::jmax (0.001f, s.releaseMs * 0.001f * 0.35f) * (float) fs));

        // PRENDER: grava o próximo passo inteiro e repete até soltar
        if (s.hold && holdState == holdOff) holdState = holdArmed;
        if (! s.hold && holdState != holdOff) holdState = holdOff;
        holdMix.setTargetValue (holdState == holdLooping ? 1.0f : 0.0f);

        int step = 0;
        double frac = 0.0;
        for (int i = 0; i < n; ++i)
        {
            auto pos = stepAt (basePpq + ppqStep * i, beatsPerStep, s.swing);
            // o PPQ do host pode voltar uma fração na virada do bloco: recuar um passo é tremida, não pulo
            if (lastIdx >= 0 && pos.index == lastIdx - 1) { pos.index = lastIdx; pos.frac = 0.0; }
            const double stepSec = pos.lenBeats * 60.0 / s.bpm;
            const bool newStep = pos.index != lastIdx;
            if (newStep)
            {
                if (lastIdx >= 0) stepPeak[(size_t) lastStep].store (stepAcc, std::memory_order_relaxed);
                stepAcc = 0.0f;
                lastIdx = pos.index;
                if (holdState == holdArmed)
                {
                    holdState = holdRecording;
                    holdStep = patternStep (pos.index, s.length, s.offset);
                    holdLen = juce::jlimit (1, holdLenMax, (int) (stepSec * fs));
                    holdSeam = juce::jmin (holdSeamMax, holdLen / 4);
                    holdW = 0;
                }
            }

            step = holdState >= holdRecording ? holdStep : patternStep (pos.index, s.length, s.offset);
            lastStep = step;
            frac = pos.frac;

            // entrada: viva, ou o passo preso tocando em laço alinhado ao grid
            float xl = l[i], xr = r[i];
            if (holdState == holdRecording)
            {
                holdBuf[0][(size_t) holdW] = xl;
                holdBuf[1][(size_t) holdW] = xr;
                // grava uns ms a mais do que o passo: é o que emenda o fim do laço no começo sem estalo
                if (++holdW >= holdLen + holdSeam) holdState = holdLooping;
            }
            const float hm = holdMix.getNextValue();
            if (hm > 0.0f && holdLen > 0)
            {
                const int rp = (int) ((int64_t) (pos.frac * stepSec * fs) % holdLen);
                float hl = holdBuf[0][(size_t) rp], hr = holdBuf[1][(size_t) rp];
                if (rp < holdSeam)
                {
                    const float x = (float) rp / (float) holdSeam;
                    hl = hl * x + holdBuf[0][(size_t) (holdLen + rp)] * (1.0f - x);
                    hr = hr * x + holdBuf[1][(size_t) (holdLen + rp)] * (1.0f - x);
                }
                xl += (hl - xl) * hm;
                xr += (hr - xr) * hm;
            }
            stepAcc = juce::jmax (stepAcc, std::abs (xl), std::abs (xr));

            // ganho do passo: PROFUNDIDADE decide o quanto fecha entre um golpe e outro
            const bool  gateOpen = pos.frac * stepSec < juce::jmax (0.001, stepSec * (double) s.width);
            const float lTar = gateOpen ? s.vol[(size_t) step] : 0.0f;
            if (lvl < lTar) lvl = juce::jmin (lTar, lvl + riseStep);
            else            lvl += (lTar - lvl) * fallCoef;
            const float gTar = 1.0f - s.depth + s.depth * lvl;
            gainSm += (gTar - gainSm) * smGain;
            panSm  += (s.pan[(size_t) step] - panSm) * smPan;

            // filtro do passo (state variable TPT); 100% aberto no passa-baixa/alta = sem filtro
            // a abertura também desliza: o "aberto = sem filtro" mudava de uma vez entre passos e estalava
            if (openSm < 0.0f) openSm = s.flt[(size_t) step];
            openSm += (s.flt[(size_t) step] - openSm) * smCut;
            const float open = openSm;
            const float cTar = std::log (stepCutoff (open, s.filtType));
            if (cutSm < 0.0f) cutSm = cTar;
            cutSm += (cTar - cutSm) * smCut;
            const float fc = juce::jmin (std::exp (cutSm), (float) (fs * 0.45));
            const float g  = std::tan (juce::MathConstants<float>::pi * fc / (float) fs);
            const float a1 = 1.0f / (1.0f + g * (g + k));
            float yv[2];
            const float xin[2] = { xl, xr };
            for (int ch = 0; ch < 2; ++ch)
            {
                const float v1 = a1 * (ic1[(size_t) ch] + g * (xin[ch] - ic2[(size_t) ch]));
                const float v2 = ic2[(size_t) ch] + g * v1;
                ic1[(size_t) ch] = 2.0f * v1 - ic1[(size_t) ch];
                ic2[(size_t) ch] = 2.0f * v2 - ic2[(size_t) ch];
                const float hp = xin[ch] - k * v1 - v2;
                const float y  = s.filtType == filtHigh ? hp : s.filtType == filtBand ? v1 * k : v2;
                // aberto no passa-baixa/alta: o sinal passa direto (sem a ressonância colorindo)
                const float bypass = s.filtType == filtBand ? 0.0f : juce::jlimit (0.0f, 1.0f, (open - 0.95f) * 20.0f);
                yv[ch] = y + (xin[ch] - y) * bypass;
            }

            // pan do passo (balanço: o lado oposto abaixa)
            const float gl = gainSm * juce::jmin (1.0f, 1.0f - panSm);
            const float gr = gainSm * juce::jmin (1.0f, 1.0f + panSm);
            l[i] = yv[0] * gl;
            r[i] = yv[1] * gr;
        }

        if (own && ! s.waiting) freePpq += (double) n * s.bpm / 60.0 / fs;
        else if (! own) freePpq = s.ppq;

        stepNow.store (step);
        fracNow.store ((float) frac);
        gainNow.store (gainSm);
        holding.store (holdState != holdOff);
    }

private:
    enum HoldState { holdOff, holdArmed, holdRecording, holdLooping };

    double fs = 44100.0, freePpq = 0.0;
    std::array<std::vector<float>, 2> holdBuf;
    int holdLenMax = 1, holdLen = 0, holdW = 0, holdStep = 0, holdSeamMax = 16, holdSeam = 0;
    HoldState holdState = holdOff;
    std::array<float, 2> ic1 {}, ic2 {};
    float gainSm = 1.0f, panSm = 0.0f, cutSm = -1.0f, openSm = -1.0f, lvl = 0.0f;
    float smGain = 0.05f, smPan = 0.01f, smCut = 0.01f;
    int64_t lastIdx = -1;
    int lastStep = 0;
    float stepAcc = 0.0f;
    juce::SmoothedValue<float> holdMix;
};
} // namespace picote
