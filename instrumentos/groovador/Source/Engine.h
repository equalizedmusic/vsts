#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Params.h"
#include "Drum.h"

// GROOVADOR — motor: 2 vozes por instrumento (a batida nova solta a anterior em ~4 ms, sem clique; o hat fechado
// corta o aberto), depois os efeitos: drive (com TONE), delay pingue-pongue no tempo e reverb.
namespace grv
{
struct FxSettings
{
    int   drvType = 0;
    float drive = 0.2f, tone = 0.8f;
    bool  dlOn = false, rvOn = true;
    int   dlDiv = 3;
    float dlFb = 0.4f, dlMix = 0.18f, rvSize = 0.45f, rvMix = 0.14f;
    double bpm = 120.0;
};

class Engine
{
public:
    // telemetria para a tela: nível de cada instrumento (os LEDs da fileira)
    std::array<std::atomic<float>, numInst> meter {};

    void prepare (double sampleRate)
    {
        for (auto& v : voices) v.prepare (sampleRate);
        for (auto& m : meter) m.store (0.0f);
        peak.fill (0.0f);
    }

    void trigger (int inst, const Sound& s, float velocity)
    {
        inst = juce::jlimit (0, numInst - 1, inst);
        if (inst == hatC)
            for (int a = 0; a < 2; ++a) voices[(size_t) (hatO * 2 + a)].choke();
        auto& cur = alt[(size_t) inst];
        voices[(size_t) (inst * 2 + cur)].choke();
        cur ^= 1;
        voices[(size_t) (inst * 2 + cur)].trigger (s, velocity);
        peak[(size_t) inst] = juce::jmax (peak[(size_t) inst], velocity);
    }

    void allOff()
    {
        for (auto& v : voices) v.choke();
    }

    void render (float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            float a = 0.0f, b = 0.0f;
            for (auto& v : voices) v.render (a, b);
            l[i] += a;
            r[i] += b;
        }
        for (int k = 0; k < numInst; ++k)
        {
            const float lv = juce::jmax (voices[(size_t) (k * 2)].level(), voices[(size_t) (k * 2 + 1)].level());
            meter[(size_t) k].store (juce::jmax (lv * peak[(size_t) k], 0.0f));
            if (lv < 0.001f) peak[(size_t) k] = 0.0f;
        }
    }

private:
    std::array<DrumVoice, numInst * 2> voices;
    std::array<int, numInst> alt {};
    std::array<float, numInst> peak {};
};

// ---------------- efeitos: drive + tone → delay pingue-pongue → reverb ----------------
class Fx
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        const int len = (int) (fs * 2.2) + 8;     // 3/8 a 40 BPM cabe
        for (auto& b : dl) b.assign ((size_t) len, 0.0f);
        dlPos = 0;
        dlLp.fill (0.0f);
        toneLp.fill (0.0f);
        dcX.fill (0.0f);
        dcY.fill (0.0f);
        timeSm.reset (fs, 0.08);
        timeSm.setCurrentAndTargetValue ((float) (fs * 0.375));
        driveSm.reset (fs, 0.03);
        driveReady = false;
        reverb.setSampleRate (fs);
        reverb.reset();
        wetL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        wetR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        dlMixSm.reset (fs, 0.05);
        rvMixSm.reset (fs, 0.05);
    }

    void process (float* l, float* r, int n, const FxSettings& s)
    {
        // ---- drive + tone ----
        if (! driveReady) { driveSm.setCurrentAndTargetValue (s.drive); driveReady = true; }
        driveSm.setTargetValue (s.drive);
        const float dcR = 1.0f - juce::MathConstants<float>::twoPi * 12.0f / (float) fs;
        const float toneHz = 1200.0f * std::exp2 (s.tone * 4.0f);      // 1,2 a 19 kHz
        const float toneC  = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (toneHz, (float) fs * 0.45f) / (float) fs);
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float d = driveSm.getNextValue();
            const float pre = 1.0f + d * d * 20.0f;
            const float post = 1.0f / std::sqrt (pre) * (1.0f + d * 0.5f);
            for (int c = 0; c < 2; ++c)
            {
                float x = ch[c][i];
                if (s.drvType == 0)        x = std::tanh (x * pre) * post;                                      // TAPE
                else if (s.drvType == 1)   x = (std::tanh (x * pre + 0.25f) - std::tanh (0.25f)) * post;        // TUBE
                else                       x = std::sin (x * pre * 1.2f) * post * 1.1f;                          // FOLD
                toneLp[(size_t) c] += (x - toneLp[(size_t) c]) * toneC;
                x = toneLp[(size_t) c];
                const float y = x - dcX[(size_t) c] + dcR * dcY[(size_t) c];
                dcX[(size_t) c] = x;
                dcY[(size_t) c] = y;
                ch[c][i] = y;
            }
        }

        // ---- delay no tempo, pingue-pongue, cada volta mais escura ----
        const double beats = delayBeats[juce::jlimit (0, 5, s.dlDiv)];
        timeSm.setTargetValue ((float) juce::jlimit (1.0, (double) dl[0].size() - 4.0, beats * 60.0 / s.bpm * fs));
        dlMixSm.setTargetValue (s.dlOn ? s.dlMix : 0.0f);
        const float fb = s.dlFb;
        const float lpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4200.0f / (float) fs);
        const int len = (int) dl[0].size();
        for (int i = 0; i < n; ++i)
        {
            const float mix = dlMixSm.getNextValue();
            const float t = timeSm.getNextValue();
            float rp = (float) dlPos - t;
            if (rp < 0.0f) rp += (float) len;
            const int i0 = (int) rp;
            const float fr = rp - (float) i0;
            const int i1 = (i0 + 1) % len;
            const float yl = dl[0][(size_t) i0] + (dl[0][(size_t) i1] - dl[0][(size_t) i0]) * fr;
            const float yr = dl[1][(size_t) i0] + (dl[1][(size_t) i1] - dl[1][(size_t) i0]) * fr;
            dlLp[0] += (yl - dlLp[0]) * lpC;
            dlLp[1] += (yr - dlLp[1]) * lpC;
            const float in = (l[i] + r[i]) * 0.5f * (mix > 0.0005f ? 1.0f : 0.0f);
            dl[0][(size_t) dlPos] = in + std::tanh (dlLp[1] * fb);
            dl[1][(size_t) dlPos] = std::tanh (dlLp[0] * fb);
            if (++dlPos >= len) dlPos = 0;
            l[i] += yl * mix;
            r[i] += yr * mix;
        }

        // ---- reverb ----
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.6f * s.rvSize;
        rp.damping = 0.5f;
        rp.width = 1.0f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        reverb.setParameters (rp);
        rvMixSm.setTargetValue (s.rvOn ? s.rvMix : 0.0f);
        if (s.rvOn || rvMixSm.isSmoothing())
        {
            if ((int) wetL.size() < n) { wetL.resize ((size_t) n); wetR.resize ((size_t) n); }
            std::copy (l, l + n, wetL.begin());
            std::copy (r, r + n, wetR.begin());
            reverb.processStereo (wetL.data(), wetR.data(), n);
            for (int i = 0; i < n; ++i)
            {
                const float m = rvMixSm.getNextValue();
                l[i] = l[i] * (1.0f - m * 0.5f) + wetL[(size_t) i] * m * 0.6f;
                r[i] = r[i] * (1.0f - m * 0.5f) + wetR[(size_t) i] * m * 0.6f;
            }
        }
        else
            rvMixSm.skip (n);
    }

private:
    double fs = 44100.0;
    std::array<std::vector<float>, 2> dl;
    std::array<float, 2> dlLp {}, toneLp {}, dcX {}, dcY {};
    int dlPos = 0;
    juce::SmoothedValue<float> timeSm, dlMixSm, rvMixSm, driveSm;
    bool driveReady = false;
    juce::Reverb reverb;
    std::vector<float> wetL, wetR;
};
} // namespace grv
