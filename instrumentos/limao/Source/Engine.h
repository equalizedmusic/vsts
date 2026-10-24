#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Params.h"

// LIMÃO — motor: uma voz (mono, como o 303), filtro ladder com a realimentação resolvida sem atraso,
// envelope de filtro com DECAY, accent que acumula em notas seguidas, slide (portamento só entre notas ligadas)
// e os efeitos depois (drive, delay e reverb).
namespace limao
{
struct Settings
{
    int   wave = 0;
    float tune = 0.0f, cutoff = 520.0f, reso = 0.65f, envmod = 0.55f, decayMs = 380.0f;
    float accent = 0.6f, slideMs = 60.0f, acid = 0.0f;
    int   drvType = 0;
    float drive = 0.2f;
    bool  dlOn = false, rvOn = false;
    int   dlDiv = 3;
    float dlFb = 0.45f, dlMix = 0.25f, rvSize = 0.45f, rvMix = 0.2f;
    double bpm = 120.0;
};

// ---------------- a voz ----------------
class Voice
{
public:
    // telemetria para a tela
    std::atomic<float> cutNow { 500.0f }, envNow { 0.0f }, accNow { 0.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        z.fill (0.0f);
        hpIn = hpFb = 0.0f;
        phase = 0.0;
        amp = fenv = accCap = 0.0f;
        gate = false;
        stage = Stage::idle;
    }

    // nota nova: legato = desliza da nota atual sem redisparar os envelopes (slide do 303)
    void noteOn (int midiNote, bool accented, bool legato)
    {
        target   = (float) midiNote;
        accentOn = accented;
        if (legato && stage != Stage::idle)
            gliding = true;                  // desliza: sem redisparar envelope
        else
        {
            gliding = false;
            pitch   = target;
            fenv    = 1.0f;
            stage   = Stage::attack;
        }
        gate = true;
    }

    void noteOff()
    {
        gate = false;
        if (stage != Stage::idle) stage = Stage::release;
    }

    void allOff() { gate = false; stage = Stage::idle; amp = 0.0f; }

    bool isGateOn() const { return gate; }

    // gera n amostras (mono) em out
    void render (float* out, int n, const Settings& s)
    {
        const float sr = (float) fs;
        const float acid = s.acid;
        // ÁCIDO: abre envelope, ressonância e um pouco do corte juntos
        const float reso   = juce::jlimit (0.0f, 1.0f, s.reso + acid * 0.35f * (1.0f - s.reso));
        const float envmod = juce::jlimit (0.0f, 1.0f, s.envmod + acid * 0.5f * (1.0f - s.envmod));
        const float baseLg = std::log2 (juce::jlimit (30.0f, 14000.0f, s.cutoff)) + acid * 0.6f;

        // accent encurta o decay do filtro (no 303 a nota acentuada usa o decay mínimo)
        const float decMs  = accentOn ? juce::jmin (s.decayMs, 220.0f) : s.decayMs;
        const float fDec   = std::exp (-1.0f / (decMs * 0.001f * sr / 4.6f));      // ~ −40 dB em decMs
        const float aAtk   = 1.0f - std::exp (-1.0f / (0.002f * sr));
        const float aDec   = std::exp (-1.0f / (1.6f * sr));                        // o VCA cai devagar com a nota presa
        const float aRel   = std::exp (-1.0f / (0.008f * sr));
        const float glide  = 1.0f - std::exp (-1.0f / (juce::jmax (1.0f, s.slideMs) * 0.001f * sr / 3.0f));
        const float accRise = 1.0f - std::exp (-1.0f / (0.012f * sr));
        const float accFall = 1.0f - std::exp (-1.0f / ((0.08f + 0.25f * reso) * sr));  // reso alta: o accent "empilha"
        const float hpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 35.0f / sr);
        const float fbHp   = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 120.0f / sr);
        const float topLg  = std::log2 (juce::jmin (18000.0f, sr * 0.45f));
        const float k      = 4.15f * std::pow (reso, 0.8f);
        const float makeup = 1.0f + k * 0.42f;
        const float accAmt = s.accent;

        float lastFc = 500.0f;
        for (int i = 0; i < n; ++i)
        {
            // envelopes
            switch (stage)
            {
                case Stage::attack:  amp += (1.0f - amp) * aAtk; if (amp > 0.995f) stage = Stage::decay; break;
                case Stage::decay:   amp = amp * aDec + 0.35f * (1.0f - aDec); break;
                case Stage::release: amp *= aRel; if (amp < 1.0e-4f) { amp = 0.0f; stage = Stage::idle; } break;
                case Stage::idle:    break;
            }
            fenv *= fDec;
            const float accTarget = accentOn && gate ? fenv : 0.0f;
            accCap += (accTarget - accCap) * (accTarget > accCap ? accRise : accFall);

            if (stage == Stage::idle) { out[i] = 0.0f; continue; }

            // altura (slide só quando ligado)
            if (gliding) pitch += (target - pitch) * glide;
            const float hz = 440.0f * std::exp2 ((pitch + s.tune - 69.0f) / 12.0f);
            const double inc = juce::jlimit (0.0, 0.45, (double) hz / fs);

            // oscilador com polyBLEP
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            const float t = (float) phase, dt = (float) inc;
            float osc;
            if (s.wave == 0)
                osc = 2.0f * t - 1.0f - blep (t, dt);
            else
            {
                // quadrada do 303: tirada da serra, com o topo levemente inclinado
                float sq = t < 0.5f ? 1.0f : -1.0f;
                sq += blep (t, dt);
                float t2 = t + 0.5f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                sq -= blep (t2, dt);
                osc = sq * 0.85f + (t - 0.5f) * 0.25f;
            }

            // corte: knob + envelope + accent (que soma no corte e no volume)
            float lg = baseLg + envmod * fenv * 4.8f + accAmt * accCap * 2.6f;
            lg = juce::jlimit (4.5f, topLg, lg);
            const float fc = std::exp2 (lg);
            lastFc = fc;

            // entrada com passa-altas leve (o 303 não tem grave embolado)
            hpIn += (osc - hpIn) * hpCoef;
            const float x = (osc - hpIn) * 0.9f;

            // ladder 4 polos ZDF, saída no 3º degrau (18 dB/oit, como o 303), passa-altas na realimentação
            const float g  = std::tan (juce::MathConstants<float>::pi * fc / sr);
            const float G  = g / (1.0f + g);
            const float S1 = z[0] / (1.0f + g), S2 = z[1] / (1.0f + g), S3 = z[2] / (1.0f + g), S4 = z[3] / (1.0f + g);
            const float G2 = G * G, G4 = G2 * G2;
            const float est = (G4 * x + G2 * G * S1 + G2 * S2 + G * S3 + S4) / (1.0f + k * G4);
            hpFb += (est - hpFb) * fbHp;
            float u = std::tanh ((x - k * (est - hpFb * 0.6f)) * 1.2f);
            float y3 = 0.0f;
            for (int d = 0; d < 4; ++d)
            {
                const float v = G * (u - z[(size_t) d]);
                const float y = v + z[(size_t) d];
                z[(size_t) d] = y + v;
                u = y;
                if (d == 2) y3 = y;
            }

            const float vca = amp * (1.0f + (accentOn ? accAmt * 0.9f : 0.0f));
            out[i] = y3 * makeup * vca * 0.55f;
        }

        cutNow.store (lastFc);
        envNow.store (fenv);
        accNow.store (accCap);
    }

private:
    enum class Stage { idle, attack, decay, release };

    static float blep (float t, float dt)
    {
        if (dt <= 0.0f) return 0.0f;
        if (t < dt)        { t /= dt;               return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    double fs = 44100.0, phase = 0.0;
    std::array<float, 4> z {};
    float hpIn = 0.0f, hpFb = 0.0f;
    float pitch = 36.0f, target = 36.0f;
    float amp = 0.0f, fenv = 0.0f, accCap = 0.0f;
    bool  gate = false, gliding = false, accentOn = false;
    Stage stage = Stage::idle;
};

// ---------------- efeitos: drive (mono) → delay pingue-pongue escuro → reverb ----------------
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
        timeSm.reset (fs, 0.08);
        timeSm.setCurrentAndTargetValue ((float) (fs * 0.375));
        crushHold = 0.0f;
        crushCount = 0.0f;

        reverb.setSampleRate (fs);
        reverb.reset();
        wetL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        wetR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        dlMixSm.reset (fs, 0.05);
        rvMixSm.reset (fs, 0.05);
    }

    // in: voz mono; sai em l/r
    void process (const float* in, float* l, float* r, int n, const Settings& s)
    {
        // ---- drive ----
        const float d = s.drive;
        const float pre = 1.0f + d * d * 24.0f;
        const float post = 1.0f / std::sqrt (pre) * (1.0f + d * 0.6f);
        const float bits = 16.0f - d * 12.5f;
        const float levels = std::exp2 (bits) * 0.5f;
        const float hold = 1.0f + d * 10.0f;    // reduz a taxa junto
        for (int i = 0; i < n; ++i)
        {
            float x = in[i];
            if (s.drvType == 0)        // TUBE: assimétrico, quente
                x = (std::tanh (x * pre + 0.25f) - std::tanh (0.25f)) * post;
            else if (s.drvType == 1)   // PEDAL: corte duro com bico de médio
            {
                const float v = x * pre * 1.5f;
                x = juce::jlimit (-1.0f, 1.0f, v - v * v * v * 0.08f) * post * 0.9f;
            }
            else                       // BITCRUSH
            {
                crushCount += 1.0f;
                if (crushCount >= hold) { crushCount -= hold; crushHold = std::round (x * levels) / levels; }
                x = d < 0.01f ? x : crushHold;
            }
            l[i] = r[i] = x;
        }

        // ---- delay no tempo, pingue-pongue, cada volta mais escura ----
        const double beats = delayBeats[juce::jlimit (0, 5, s.dlDiv)];
        timeSm.setTargetValue ((float) juce::jlimit (1.0, (double) dl[0].size() - 4.0, beats * 60.0 / s.bpm * fs));
        dlMixSm.setTargetValue (s.dlOn ? s.dlMix : 0.0f);
        const float fb = s.dlFb;
        const float lpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2600.0f / (float) fs);
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
            const float dry = l[i];
            // entrada só na esquerda; as voltas cruzam de lado
            dl[0][(size_t) dlPos] = dry * (mix > 0.0005f ? 1.0f : 0.0f) + std::tanh (dlLp[1] * fb);
            dl[1][(size_t) dlPos] = std::tanh (dlLp[0] * fb);
            if (++dlPos >= len) dlPos = 0;
            l[i] = dry + yl * mix;
            r[i] = r[i] + yr * mix;
        }

        // ---- reverb ----
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.65f * s.rvSize;
        rp.damping = 0.55f;
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
    std::array<float, 2> dlLp {};
    int dlPos = 0;
    juce::SmoothedValue<float> timeSm, dlMixSm, rvMixSm;
    float crushHold = 0.0f, crushCount = 0.0f;
    juce::Reverb reverb;
    std::vector<float> wetL, wetR;
};
} // namespace limao
