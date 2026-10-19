#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace repeteco
{
// divisões do LFO no grid do host (em tempos de semínima)
inline constexpr int numDivs = 14;
inline const char* const divNames[numDivs] = { "1/32", "1/16T", "1/16", "1/8T", "3/32", "1/8", "1/4T",
                                               "3/16", "1/4", "3/8", "1/2", "1 comp", "2 comp", "4 comp" };
inline constexpr double divBeats[numDivs] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0,
                                              0.75, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };

// TEMPO no grid: o knob anda por 8 divisões; FAIXA escolhe as curtas ou as longas
inline constexpr int timeSteps = 8;
inline const char* const shortNames[timeSteps] = { "1/32", "1/16T", "1/16", "1/8T", "3/32", "1/8", "1/4T", "3/16" };
inline constexpr double shortBeats[timeSteps]  = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75 };
inline const char* const longNames[timeSteps]  = { "1/8", "1/4T", "3/16", "1/4", "3/8", "1/2", "3/4", "1 comp" };
inline constexpr double longBeats[timeSteps]   = { 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0 };

inline constexpr double maxDelaySec = 4.0;
inline constexpr int bbdStages = 1024;

enum Shape { shapeSine, shapeTri, shapeSquare, shapeRandom };

struct Settings
{
    double bpm = 145.0, ppq = 0.0;
    bool   waiting = false;                       // host parado: o LFO fica na posição do cursor
    float  time = 0.9f;                           // 0..1
    bool   longRange = false, free = false;       // FAIXA longa; SYNC livre (ms)
    float  drive = 0.0f;                          // dB
    float  pingPong = 0.0f, feedback = 0.45f;     // feedback 0..1,2
    int    lfoDiv = 10, lfoShape = shapeSine;
    float  lfoAmt = 0.15f, dark = 0.5f;
    bool   infinite = false, kill = false;
};

inline int timeIndex (float time01) { return juce::jlimit (0, timeSteps - 1, (int) std::round (time01 * (timeSteps - 1))); }

inline double freeMs (float time01, bool longRange)
{
    const double lo = longRange ? 100.0 : 20.0, hi = longRange ? 2000.0 : 400.0;
    return lo * std::pow (hi / lo, (double) juce::jlimit (0.0f, 1.0f, time01));
}

inline double delaySeconds (const Settings& s)
{
    if (s.free) return freeMs (s.time, s.longRange) * 0.001;
    const int k = timeIndex (s.time);
    const double beats = s.longRange ? longBeats[k] : shortBeats[k];
    return juce::jmin (maxDelaySec, beats * 60.0 / juce::jmax (40.0, s.bpm));
}

inline juce::String timeName (const Settings& s)
{
    if (s.free) return juce::String (juce::roundToInt (freeMs (s.time, s.longRange))) + " ms";
    const int k = timeIndex (s.time);
    return s.longRange ? longNames[k] : shortNames[k];
}

// ESCURO: corte do filtro dentro do laço (12 kHz → 1,2 kHz); delay longo escurece mais, como o relógio lento do BBD
inline float loopHz (float dark01, double delaySec)
{
    const float bbd = (float) juce::jlimit (0.35, 1.0, std::sqrt (0.3 / juce::jmax (0.01, delaySec)));
    return 12000.0f * std::pow (0.1f, dark01) * bbd;
}

// histórico do que entra nos baldes: um pico a cada 5 ms (dá para ~5 s)
inline constexpr int histLen = 1024;

class BbdEngine
{
public:
    std::array<std::atomic<float>, histLen> hist {};
    std::atomic<int> histWrite { 0 };
    std::atomic<float> delayNow { 0.3f }, fbNow { 0.45f }, lfoNow { 0.0f }, levelNow { 0.0f };
    std::atomic<float> histStepSec { 0.005f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        len = 1;
        while (len < (int) (fs * (maxDelaySec + 0.1)) + 8) len <<= 1;
        mask = len - 1;
        for (auto& b : buf) b.assign ((size_t) len, 0.0f);
        writePos = 0;
        lp1.fill (0.0f); lp2.fill (0.0f); hp.fill (0.0f); hpX.fill (0.0f);
        baseSm = -1.0;
        modSm = 0.0f;
        held = 0.0f;
        lastSeg = -1;
        primed = false;
        histStep = juce::jmax (1, (int) (fs * 0.005));
        histStepSec.store ((float) histStep / (float) fs);
        histCount = 0;
        histPeak = 0.0f;
        for (auto& h : hist) h.store (0.0f);
        glide    = 1.0f - std::exp (-1.0f / (0.12f * (float) fs));   // mexer no TEMPO estica o eco
        modGlide = 1.0f - std::exp (-1.0f / (0.005f * (float) fs));
        fbSm.reset (fs, 0.05);
        infSm.reset (fs, 0.03);
        inSm.reset (fs, 0.01);
        driveSm.reset (fs, 0.03);
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& s)
    {
        const double dSec = delaySeconds (s);
        const float  base = (float) juce::jlimit (4.0, (double) len - 8.0, dSec * fs);
        if (! primed)
        {
            fbSm.setCurrentAndTargetValue (s.feedback);
            infSm.setCurrentAndTargetValue (s.infinite ? 1.0f : 0.0f);
            inSm.setCurrentAndTargetValue (s.kill || s.infinite ? 0.0f : 1.0f);
            driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (s.drive));
            primed = true;
        }
        if (baseSm < 0.0) baseSm = base;
        fbSm.setTargetValue (s.feedback);
        infSm.setTargetValue (s.infinite ? 1.0f : 0.0f);
        inSm.setTargetValue (s.kill || s.infinite ? 0.0f : 1.0f);
        driveSm.setTargetValue (juce::Decibels::decibelsToGain (s.drive));

        const float lpHz = juce::jmin (loopHz (s.dark, dSec), (float) (fs * 0.45));
        const float lpC  = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * lpHz / (float) fs);
        const float hpC  = std::exp (-juce::MathConstants<float>::twoPi * 40.0f / (float) fs);
        const float depth = s.lfoAmt * (float) juce::jmin (0.006 * fs, 0.25 * dSec * fs);   // até 6 ms
        const double beats   = divBeats[juce::jlimit (0, numDivs - 1, s.lfoDiv)];
        const double ppqStep = s.waiting ? 0.0 : s.bpm / 60.0 / fs;
        const float  pp = s.pingPong;

        float lfo = 0.0f, peak = 0.0f, fb = s.feedback;
        for (int i = 0; i < n; ++i)
        {
            // LFO no grid: balança o tempo de leve (o "chorus" do BBD)
            const double p = (s.ppq + ppqStep * i) / beats;
            const auto seg = (int64_t) std::floor (p);
            if (seg != lastSeg) { lastSeg = seg; held = rnd.nextFloat() * 2.0f - 1.0f; }
            const float ph = (float) (p - std::floor (p));
            switch (s.lfoShape)
            {
                case shapeTri:    lfo = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
                case shapeSquare: lfo = ph < 0.5f ? 1.0f : -1.0f; break;
                case shapeRandom: lfo = held; break;
                default:          lfo = std::sin (juce::MathConstants<float>::twoPi * ph); break;
            }
            modSm += (lfo * depth - modSm) * modGlide;
            baseSm += (base - baseSm) * glide;
            const float d = juce::jlimit (4.0f, (float) len - 4.0f, (float) baseSm + modSm);

            const float yL = read (0, d), yR = read (1, d);

            // laço: sem grave embolando, cada volta mais escura; INFINITO segura sem filtrar
            const float inf = infSm.getNextValue();
            fb = fbSm.getNextValue();
            const float g  = fb + (1.0f - fb) * inf;
            const float y[2] = { yL, yR };
            float f[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float h = hpC * (hp[(size_t) ch] + y[ch] - hpX[(size_t) ch]);
                hpX[(size_t) ch] = y[ch];
                hp[(size_t) ch]  = h;
                lp1[(size_t) ch] += (h - lp1[(size_t) ch]) * lpC;
                lp2[(size_t) ch] += (lp1[(size_t) ch] - lp2[(size_t) ch]) * lpC;
                f[ch] = lp2[(size_t) ch] + (y[ch] - lp2[(size_t) ch]) * inf;
            }
            const float aL = (1.0f - pp) * f[0] + pp * f[1];
            const float aR = (1.0f - pp) * f[1] + pp * f[0];

            // entrada: com PING-PONG ela entra só pela esquerda e o eco vai e volta
            const float dg = driveSm.getNextValue();
            const float ig = inSm.getNextValue();
            const float xL = inL[i] * dg * ig, xR = inR[i] * dg * ig;
            const float sL = xL + (0.5f * (xL + xR) - xL) * pp;
            const float sR = xR * (1.0f - pp);

            // o balde satura: acima de 100% de realimentação o eco cresce até o limite e canta
            const float wL = sat (sL + aL * g), wR = sat (sR + aR * g);
            buf[0][(size_t) writePos] = wL;
            buf[1][(size_t) writePos] = wR;
            writePos = (writePos + 1) & mask;

            const float post = 1.0f / std::sqrt (dg);
            outL[i] = yL * post;
            outR[i] = yR * post;
            peak = juce::jmax (peak, std::abs (outL[i]), std::abs (outR[i]));

            histPeak = juce::jmax (histPeak, std::abs (wL), std::abs (wR));
            if (++histCount >= histStep)
            {
                const int w = histWrite.load (std::memory_order_relaxed);
                hist[(size_t) w].store (histPeak, std::memory_order_relaxed);
                histWrite.store ((w + 1) % histLen, std::memory_order_release);
                histCount = 0;
                histPeak = 0.0f;
            }
        }

        delayNow.store ((float) (baseSm / fs));
        fbNow.store (fb);
        lfoNow.store (lfo);
        levelNow.store (juce::jmax (levelNow.load(), peak));
    }

private:
    static float sat (float x)
    {
        // linear até ~0,6 e curva suave depois (não deixa a auto-oscilação explodir)
        const float a = std::abs (x);
        if (a < 0.6f) return x;
        return std::copysign (0.6f + 0.6f * std::tanh ((a - 0.6f) / 0.6f), x);
    }

    float read (int ch, float delay) const
    {
        // Hermite de 4 pontos: o eco desliza sem chiado quando o tempo muda
        const float pos = (float) writePos - delay;
        const int   i1  = (int) std::floor (pos);
        const float t   = pos - (float) i1;
        const auto& b = buf[(size_t) ch];
        const float y0 = b[(size_t) ((i1 - 1) & mask)], y1 = b[(size_t) (i1 & mask)];
        const float y2 = b[(size_t) ((i1 + 1) & mask)], y3 = b[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    double fs = 44100.0, baseSm = -1.0;
    std::array<std::vector<float>, 2> buf;
    int len = 1, mask = 0, writePos = 0;
    std::array<float, 2> lp1 {}, lp2 {}, hp {}, hpX {};
    float modSm = 0.0f, held = 0.0f, glide = 0.001f, modGlide = 0.01f;
    int64_t lastSeg = -1;
    int histStep = 220, histCount = 0;
    float histPeak = 0.0f;
    bool primed = false;
    juce::Random rnd;
    juce::SmoothedValue<float> fbSm, infSm, inSm, driveSm;
};
} // namespace repeteco
