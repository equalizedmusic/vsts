#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// GROOVADOR — uma voz de percussão. C++ puro (sem JUCE) para dar para medir o nível de cada som fora do plugin.
// Modelos:
//   ANALÓG. = seno com pitch env + segundo modo de membrana (BODY) + ruído no filtro (caixa, rim, tom)
//   808     = seno saturado (BODY = saturação), o "tom" da 808
//   FM      = portadora + moduladora (BODY = razão), o índice cai junto com a nota (bongô, cowbell, zap)
//   METAL   = 6 quadradas nas razões do hat da 808 (BODY = espalha as razões), tudo pelo filtro
//   RUÍDO   = só ruído no filtro; BODY = quantas tapas (1 a 4, o clap) e o espaço entre elas
// TONE = corte do filtro do ruído (passa-alta nos hats e no shaker, passa-banda nos outros); SNAP = estalo do ataque
// e quão rápido cai o pitch env; NOISE = quanto ruído entra; DRIVE = saturação da própria voz.
namespace grv
{
struct Sound
{
    int   model = 0, inst = 0;
    float hz = 200.0f;              // altura base já com o TUNE
    float decMs = 200.0f, tone = 4000.0f;
    float snap = 0.5f, body = 0.5f, noise = 0.5f, pEnv = 0.0f, drive = 0.0f, pan = 0.0f;
    float gain = 1.0f;              // nível linear (LEVEL)
    bool  highPass = false;         // hats e shaker
};

class DrumVoice
{
public:
    void prepare (double sampleRate) { fs = (float) sampleRate; active = false; }

    void trigger (const Sound& s, float velocity)
    {
        snd = s;
        active = true;
        killing = false;
        kill = 1.0f;
        t = 0;
        ph = ph2 = phm = 0.0f;
        for (int k = 0; k < 6; ++k) mph[k] = 0.13f * (float) k;
        ic1 = ic2 = 0.0f;
        ampB = ampN = pe = click = fmE = 1.0f;

        const float dec = std::max (5.0f, s.decMs);
        aB = std::exp (-6.9f / (dec * 0.001f * fs));
        aN = std::exp (-6.9f / ((s.model == 4 ? dec : dec * 0.75f) * 0.001f * fs));
        aP = std::exp (-6.9f / ((4.0f + (1.0f - s.snap) * 45.0f) * 0.001f * fs));
        aC = std::exp (-6.9f / (0.004f * fs));
        aF = std::exp (-6.9f / (dec * 0.45f * 0.001f * fs));
        aT = std::exp (-6.9f / (0.009f * fs));
        aK = std::exp (-6.9f / (0.004f * fs));

        taps = s.model == 4 ? 1 + (int) std::lround (s.body * 3.0f) : 1;
        tapLen = (int) ((7.0f + s.body * 6.0f) * 0.001f * fs);

        const float fc = std::clamp (s.tone, 40.0f, fs * 0.45f);
        g = std::tan (3.14159265f * fc / fs);
        k = s.highPass ? 1.4f : (s.model == 4 ? 0.9f : 1.2f);
        a1 = 1.0f / (1.0f + g * (g + k));

        gain = std::pow (std::clamp (velocity, 0.0f, 1.0f), 1.2f) * s.gain;
        const float p = std::clamp (s.pan, -1.0f, 1.0f);
        gl = std::cos ((p + 1.0f) * 0.785398f) * 1.414f;
        gr = std::sin ((p + 1.0f) * 0.785398f) * 1.414f;
        pre = 1.0f + s.drive * s.drive * 10.0f;
        post = 1.0f / std::sqrt (pre);
    }

    // a voz anterior do mesmo instrumento (ou o hat aberto quando o fechado toca) some em ~4 ms, sem clique
    void choke() { if (active) killing = true; }
    bool isActive() const { return active; }
    float level() const { return active ? std::max (ampB, ampN) * kill : 0.0f; }

    void render (float& l, float& r)
    {
        if (! active) return;
        const float twoPi = 6.2831853f;
        pe *= aP;
        const float f = snd.hz * std::exp2 (snd.pEnv * pe * (1.0f / 12.0f));
        const float inc = std::min (0.45f, f / fs);

        float tonal = 0.0f;
        switch (snd.model)
        {
            case 0:     // ANALÓG.
            {
                ph = wrap (ph + inc);
                ph2 = wrap (ph2 + std::min (0.45f, inc * 1.593f));
                tonal = (std::sin (twoPi * ph) + snd.body * 0.55f * std::sin (twoPi * ph2)) / (1.0f + snd.body * 0.3f);
                break;
            }
            case 1:     // 808
            {
                ph = wrap (ph + inc);
                const float drv = 1.0f + snd.body * 5.0f;
                tonal = std::tanh (std::sin (twoPi * ph) * drv) / std::tanh (drv);
                break;
            }
            case 2:     // FM
            {
                fmE *= aF;
                ph  = wrap (ph + inc);
                phm = wrap (phm + std::min (0.45f, inc * (1.0f + snd.body * 3.0f)));
                tonal = std::sin (twoPi * ph + 4.0f * fmE * std::sin (twoPi * phm));
                break;
            }
            case 3:     // METAL
            {
                static const float ratios[6] = { 1.0f, 1.483f, 1.800f, 2.546f, 2.630f, 3.897f };
                const float spread = 0.6f + snd.body * 0.8f;
                float sum = 0.0f;
                for (int q = 0; q < 6; ++q)
                {
                    mph[q] = wrap (mph[q] + std::min (0.45f, inc * std::pow (ratios[q], spread)));
                    sum += mph[q] < 0.5f ? 1.0f : -1.0f;
                }
                tonal = sum * (1.0f / 6.0f);
                break;
            }
            default: break;     // RUÍDO
        }

        const float white = nextNoise();

        // envelopes: corpo; ruído (com as tapas do clap); estalo
        ampB *= aB;
        if (taps > 1 && t < (taps - 1) * tapLen)
        {
            if (t % tapLen == 0) ampN = 1.0f;
            ampN *= aT;
        }
        else
        {
            if (taps > 1 && t == (taps - 1) * tapLen) ampN = 1.0f;
            ampN *= aN;
        }
        click *= aC;

        float out;
        if (snd.model == 3)
        {
            const float x = tonal * (1.0f - snd.noise * 0.5f) + white * snd.noise * 0.7f;
            out = filter (x) * ampB * 2.2f;
        }
        else
        {
            const float nAmt = snd.model == 4 ? 0.35f + 0.65f * snd.noise : snd.noise;
            out = tonal * ampB * (1.0f - snd.noise * 0.45f) + filter (white) * ampN * nAmt * 1.3f;
        }
        out += white * click * snd.snap * 0.45f;

        if (snd.drive > 0.001f) out = std::tanh (out * pre) * post * 1.25f;
        if (killing) { kill *= aK; if (kill < 1.0e-4f) active = false; }
        out *= gain * kill;
        l += out * gl;
        r += out * gr;

        ++t;
        if (ampB < 1.0e-4f && ampN < 1.0e-4f && t > taps * tapLen) active = false;
    }

private:
    static float wrap (float x) { return x >= 1.0f ? x - 1.0f : x; }

    float nextNoise()
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    // SVF (TPT): passa-alta ou passa-banda normalizado
    float filter (float x)
    {
        const float v1 = a1 * (ic1 + g * (x - ic2));
        const float v2 = ic2 + g * v1;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return snd.highPass ? x - k * v1 - v2 : k * v1;
    }

    Sound snd;
    float fs = 44100.0f;
    bool  active = false, killing = false;
    long  t = 0;
    int   taps = 1, tapLen = 1;
    float ph = 0.0f, ph2 = 0.0f, phm = 0.0f, mph[6] {};
    float ampB = 0.0f, ampN = 0.0f, pe = 0.0f, click = 0.0f, fmE = 0.0f, kill = 1.0f;
    float aB = 0.0f, aN = 0.0f, aP = 0.0f, aC = 0.0f, aF = 0.0f, aT = 0.0f, aK = 0.0f;
    float g = 0.1f, k = 1.0f, a1 = 1.0f, ic1 = 0.0f, ic2 = 0.0f;
    float gain = 1.0f, gl = 1.0f, gr = 1.0f, pre = 1.0f, post = 1.0f;
    uint32_t rng = 22222u;
};
} // namespace grv
