#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace ringer
{
// divisões do LFO no grid do host (em tempos de semínima)
inline constexpr int numDivs = 14;
inline const char* const divNames[numDivs] = { "1/32", "1/16T", "1/16", "1/8T", "3/32", "1/8", "1/4T",
                                               "3/16", "1/4", "3/8", "1/2", "1 comp", "2 comp", "4 comp" };
inline constexpr double divBeats[numDivs] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0,
                                              0.75, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };

enum Shape { shapeSine, shapeTri, shapeSquare, shapeRamp, shapeRandom };

// FREQUÊNCIA 0–100% na faixa: baixa 0,5–80 Hz (tremolo), alta 30 Hz–4 kHz (metálico)
inline float carrierHz (float freq01, bool high)
{
    const float lo = high ? 30.0f : 0.5f, hi = high ? 4000.0f : 80.0f;
    return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, freq01));
}

// AFINAR em semitom: prende a portadora na nota mais perto (lá = 440 Hz)
inline float snapSemitone (float hz)
{
    const float n = std::round (12.0f * std::log2 (hz / 440.0f));
    return 440.0f * std::exp2 (n / 12.0f);
}

inline float lfoShape (int shape, float ph, float held)
{
    switch (shape)
    {
        case shapeTri:    return 1.0f - 4.0f * std::abs (ph - 0.5f);
        case shapeSquare: return ph < 0.5f ? 1.0f : -1.0f;
        case shapeRamp:   return 2.0f * ph - 1.0f;
        case shapeRandom: return held;
        default:          return std::sin (juce::MathConstants<float>::twoPi * ph);
    }
}

struct Settings
{
    double bpm = 145.0, ppq = 0.0;
    bool   waiting = false;     // host parado: o LFO (no grid) fica na posição do cursor
    float  freq = 0.58f, drive = 0.0f, stereo = 0.25f;   // drive em dB
    bool   high = true, snap = false;
    float  lfoAmt = 0.3f, lfoPhase = 0.0f;               // fase 0..1
    int    lfoDiv = 8, lfoShape = shapeSine;
    bool   lfoFree = false, twice = false;
};

// telemetria do osciloscópio: entrada, portadora e saída (canal esquerdo), uma a cada 'decim' amostras
inline constexpr int scopeLen = 1024;

class RingEngine
{
public:
    std::array<std::atomic<float>, scopeLen> scIn {}, scCar {}, scOut {};
    std::atomic<int> scWrite { 0 };
    std::atomic<float> hzNow { 440.0f }, lfoNow { 0.0f }, carPhase { 0.0f }, levelNow { 0.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        phL = 0.0;
        freePh = 0.0;
        lastSeg = -1;
        held = 0.0f;
        primed = false;
        hzSm.reset (fs, 0.02);
        hzSm.setCurrentAndTargetValue (440.0f);
        twiceSm.reset (fs, 0.08);
        driveSm.reset (fs, 0.03);
        decim = juce::jmax (1, (int) std::round (fs / 22050.0));
        decCount = 0;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        if (! primed)
        {
            twiceSm.setCurrentAndTargetValue (s.twice ? 1.0f : 0.0f);
            driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (s.drive));
            primed = true;
        }
        twiceSm.setTargetValue (s.twice ? 1.0f : 0.0f);
        driveSm.setTargetValue (juce::Decibels::decibelsToGain (s.drive));

        const double beats   = divBeats[juce::jlimit (0, numDivs - 1, s.lfoDiv)];
        const double ppqStep = s.bpm / 60.0 / fs;
        const float  base    = carrierHz (s.freq, s.high);
        const float  maxF    = (float) (fs * 0.45);
        const float  twoPi   = juce::MathConstants<float>::twoPi;
        // ESTÉREO: a portadora da direita anda adiantada (até meia volta)
        const double offR = 0.5 * s.stereo;

        float hz = base, lfo = 0.0f, peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            // LFO: no grid segue a régua do host; LIVRE corre sozinho no BPM
            double ph;
            if (s.lfoFree)
            {
                freePh += ppqStep / beats;
                freePh -= std::floor (freePh);
                ph = freePh;
            }
            else
            {
                const double p = (s.ppq + (s.waiting ? 0.0 : ppqStep * i)) / beats;
                const auto seg = (int64_t) std::floor (p);
                if (seg != lastSeg) { lastSeg = seg; held = rnd.nextFloat() * 2.0f - 1.0f; }
                ph = p - std::floor (p);
            }
            ph += s.lfoPhase;
            ph -= std::floor (ph);
            if (s.lfoFree && ph < prevFreePh) held = rnd.nextFloat() * 2.0f - 1.0f;
            prevFreePh = ph;
            lfo = lfoShape (s.lfoShape, (float) ph, held);

            // portadora: LFO em oitavas (±2 no máximo), DOBRAR sobe uma oitava
            float f = base * std::exp2 (s.lfoAmt * 2.0f * lfo + twiceSm.getNextValue());
            if (s.snap) f = snapSemitone (f);
            hzSm.setTargetValue (juce::jlimit (0.1f, maxF, f));
            hz = hzSm.getNextValue();

            phL += hz / fs;
            phL -= std::floor (phL);
            const float cL = std::sin (twoPi * (float) phL);
            const float cR = std::sin (twoPi * (float) (phL + offR));

            // DRIVE: satura a entrada antes de multiplicar (o pedal também esquenta)
            const float dg = driveSm.getNextValue();
            const float xl = std::tanh (inL[i] * dg) / std::sqrt (dg);
            const float xr = std::tanh (inR[i] * dg) / std::sqrt (dg);
            outL[i] = xl * cL;
            outR[i] = xr * cR;
            peak = juce::jmax (peak, std::abs (outL[i]), std::abs (outR[i]));

            if (++decCount >= decim)
            {
                decCount = 0;
                const int w = scWrite.load (std::memory_order_relaxed);
                scIn[(size_t) w].store (inL[i], std::memory_order_relaxed);
                scCar[(size_t) w].store (cL, std::memory_order_relaxed);
                scOut[(size_t) w].store (outL[i], std::memory_order_relaxed);
                scWrite.store ((w + 1) % scopeLen, std::memory_order_release);
            }
        }

        hzNow.store (hz);
        lfoNow.store (lfo);
        carPhase.store ((float) phL);
        levelNow.store (juce::jmax (levelNow.load(), peak));
    }

    int decimation() const { return decim; }

private:
    double fs = 44100.0, phL = 0.0, freePh = 0.0, prevFreePh = 0.0;
    int64_t lastSeg = -1;
    float held = 0.0f;
    bool primed = false;
    int decim = 2, decCount = 0;
    juce::Random rnd;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> hzSm;
    juce::SmoothedValue<float> twiceSm, driveSm;
};
} // namespace ringer
