#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Params.h"

// VACINA — motor. Virtual-analógico leve: a modulação (envelopes, LFOs, matriz, glide, corte) roda em blocos de
// controle de 32 amostras e só o que precisa de áudio (osciladores, filtros, ganho) roda por amostra, com os
// coeficientes do filtro e o ganho interpolados dentro do bloco (sem degrau, sem estalo).
//   OSC1 CLASSIC (SHAPE: seno → triângulo → serra → pulso) ou SUPER (7 serras com SPREAD), OSC2 CLASSIC com
//   SYNC e FM (vindo do OSC1), SUB (quadrada uma oitava abaixo) e NOISE → OSC VOLUME (satura acima da metade) →
//   FILTRO 1 (LP 24 ladder "analógico", LP 12, HP, BP, BS) e FILTRO 2 (SVF) em série ou paralelo → VCA.
namespace vacina
{
inline float fastTanh (float x)
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }

struct Settings
{
    int   keyMode = 0;
    float portaMs = 0.0f;
    int   unison = 1;
    float uniDetune = 0.25f, uniSpread = 0.6f;

    int   model1 = 0;
    float shape1 = 0.66f, pw1 = 0.5f, semi1 = 0.0f, spread1 = 0.4f;
    float shape2 = 0.66f, pw2 = 0.5f, semi2 = 0.0f, detune2 = 7.0f;
    bool  sync2 = false;
    float fm = 0.0f;
    float bal = 0.5f, sub = 0.0f, oscVol = 0.5f, noise = 0.0f;

    float cutoff = 2400.0f, cutoff2 = 0.0f, filtBal = 0.0f;
    int   routing = 0, f1Mode = 0, f2Mode = 0;
    float reso1 = 0.2f, reso2 = 0.2f, env1 = 0.3f, env2 = 0.3f, key1 = 0.3f, key2 = 0.3f;

    float fA = 2.0f, fD = 450.0f, fS = 0.3f, fR = 350.0f, fVel = 0.3f;
    float aA = 2.0f, aD = 600.0f, aS = 0.8f, aR = 300.0f, aVel = 0.4f;

    float l1Pitch = 0.0f, l1Cut = 0.0f, l1Pw = 0.0f, l1Amp = 0.0f;
    float l2Shp = 0.0f, l2Fm = 0.0f, l2Cut = 0.0f, l2Pan = 0.0f;
    float l1Rate = 5.0f, l2Rate = 0.4f;
    int   l1Shape = 0, l2Shape = 1, l1Sync = 0, l2Sync = 0;

    std::array<int, numSlots>   mSrc {}, mDst {};
    std::array<float, numSlots> mAmt {};
};

// valores globais de um bloco de controle
struct Global
{
    float lfo1 = 0.0f, lfo2 = 0.0f, wheel = 0.0f, after = 0.0f, bend = 0.0f;
};

// ---------------- envelope ADSR (avança em blocos) ----------------
class Env
{
public:
    enum Stage { idle, attack, decay, sustain, release };

    void trigger()   { stage = attack; }                         // sobe a partir do nível atual (roubo de voz sem estalo)
    void noteOff()   { if (stage != idle) stage = release; }
    void reset()     { stage = idle; level = 0.0f; }
    bool isIdle() const { return stage == idle; }
    float get() const   { return level; }

    float advance (int n, double fs, float aMs, float dMs, float s, float rMs)
    {
        const double sr = fs * 0.001;
        switch (stage)
        {
            case attack:
                level += (float) n / (float) juce::jmax (1.0, aMs * sr);
                if (level >= 1.0f) { level = 1.0f; stage = decay; }
                break;
            case decay:
            {
                const float c = (float) std::exp (-(double) n * 4.6 / juce::jmax (1.0, dMs * sr));
                level = s + (level - s) * c;
                if (std::abs (level - s) < 1.0e-4f) stage = sustain;
                break;
            }
            case sustain:
                level += (s - level) * 0.25f;                      // o knob de SUSTAIN mexe sem degrau
                break;
            case release:
            {
                const float c = (float) std::exp (-(double) n * 4.6 / juce::jmax (1.0, rMs * sr));
                level *= c;
                if (level < 1.0e-4f) { level = 0.0f; stage = idle; }
                break;
            }
            case idle:
                break;
        }
        return level;
    }

private:
    Stage stage = idle;
    float level = 0.0f;
};

// ---------------- filtro de estado variável (TPT) ----------------
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    void reset() { ic1 = ic2 = 0.0f; }

    // mode: 0 LP, 1 HP, 2 BP, 3 BS
    inline float process (float x, float g, float k, int mode)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        ic1 = fastTanh (ic1 * 0.5f) * 2.0f;                       // a ressonância satura em vez de estourar
        switch (mode)
        {
            case 0:  return v2;
            case 1:  return x - k * v1 - v2;
            case 2:  return v1 * k;
            default: return x - k * v1;
        }
    }
};

// ---------------- ladder 4 polos ZDF (LP 24 "analógico") ----------------
struct Ladder
{
    std::array<float, 4> z {};
    void reset() { z.fill (0.0f); }

    inline float process (float x, float g, float reso)
    {
        const float G = g / (1.0f + g);
        const float k = 3.9f * reso;
        const float ig = 1.0f / (1.0f + g);
        const float S = G * G * G * z[0] * ig + G * G * z[1] * ig + G * z[2] * ig + z[3] * ig;
        const float G4 = G * G * G * G;
        const float est = (G4 * x + S) / (1.0f + k * G4);
        float u = fastTanh ((x - k * est) * 1.1f) * (1.0f / 1.1f);
        for (int d = 0; d < 4; ++d)
        {
            const float v = G * (u - z[(size_t) d]);
            const float y = v + z[(size_t) d];
            z[(size_t) d] = y + v;
            u = y;
        }
        return u * (1.0f + k * 0.45f);                             // repõe o grave que a ressonância come
    }
};

// ---------------- uma voz ----------------
class Voice
{
public:
    int    note = -1;
    bool   gate = false, sustained = false;
    uint64_t age = 0;

    bool isActive() const { return ! amp.isIdle(); }
    float level() const   { return amp.get(); }

    void prepare (double sampleRate, uint32_t seed)
    {
        fs = sampleRate;
        rng = seed | 1u;
        amp.reset();
        fenv.reset();
        resetFilters();
        first = true;
    }

    // nota nova. retrigger = false: só desliza para a nota (LEGATO); fromPitch = de onde vem o portamento
    void start (int midiNote, float velocity, float fromPitch, bool retrigger, float uniPitchOffset, float uniPanOffset,
                float randomValue)
    {
        const bool wasIdle = amp.isIdle();
        note = midiNote;
        target = (float) midiNote;
        pitch = fromPitch;
        gate = true;
        sustained = false;
        uniPitch = uniPitchOffset;
        uniPan = uniPanOffset;
        if (retrigger || wasIdle)
        {
            vel = velocity;
            rnd = randomValue;
            amp.trigger();
            fenv.trigger();
        }
        if (wasIdle)
        {
            // vinda do silêncio: filtro zerado e fases conhecidas (o ataque sai sempre igual)
            resetFilters();
            ph1 = 0.0;
            ph2 = rand01();
            phSub = 0.0;
            for (auto& p : sp) p = rand01();
            first = true;
        }
    }

    void release()  { gate = false; amp.noteOff(); fenv.noteOff(); }
    void kill()     { gate = false; amp.reset(); fenv.reset(); note = -1; }

    // gera n amostras (n ≤ bloco de controle) e SOMA em l/r
    void render (float* l, float* r, int n, const Settings& s, const Global& gl, float glideCoef)
    {
        if (amp.isIdle()) return;

        // ---- fontes da matriz (−1..1 ou 0..1) ----
        const float fe = fenv.advance (n, fs, s.fA, s.fD, s.fS, s.fR);
        const float ae = amp.advance (n, fs, s.aA, s.aD, s.aS, s.aR);
        float src[numSrcs] = { 0.0f, gl.wheel, gl.after, vel, ((float) note - 60.0f) / 36.0f, gl.lfo1, gl.lfo2, fe, ae, rnd };
        float mod[numDsts] {};
        for (int k = 0; k < numSlots; ++k)
            if (s.mSrc[(size_t) k] > 0 && s.mDst[(size_t) k] > 0)
                mod[s.mDst[(size_t) k]] += src[s.mSrc[(size_t) k]] * s.mAmt[(size_t) k];

        // ---- altura ----
        pitch += (target - pitch) * glideCoef;
        const float vib = s.l1Pitch * std::abs (s.l1Pitch) * 12.0f * gl.lfo1;
        const float base = pitch + uniPitch + gl.bend * 2.0f + vib + mod[dstPitch] * 24.0f;
        const float inc1 = juce::jlimit (0.0f, 0.45f, 440.0f * std::exp2 ((base + s.semi1 - 69.0f) / 12.0f) / (float) fs);
        const float inc2 = juce::jlimit (0.0f, 0.45f, 440.0f * std::exp2 ((base + s.semi2 + s.detune2 * 0.01f - 69.0f) / 12.0f) / (float) fs);

        const float shape1 = clamp01 (s.shape1 + gl.lfo2 * s.l2Shp + mod[dstShape1]);
        const float shape2 = clamp01 (s.shape2 + gl.lfo2 * s.l2Shp + mod[dstShape2]);
        const float pwMod  = gl.lfo1 * s.l1Pw * 0.45f + mod[dstPw] * 0.45f;
        const float pw1    = juce::jlimit (0.04f, 0.96f, s.pw1 + pwMod);
        const float pw2    = juce::jlimit (0.04f, 0.96f, s.pw2 + pwMod);
        const float fmAmt  = clamp01 (s.fm + gl.lfo2 * s.l2Fm + mod[dstFm]);
        const float pm     = fmAmt * fmAmt * 1.6f;
        const float bal    = clamp01 (s.bal + mod[dstBal]);
        const float g1     = juce::jmin (1.0f, 2.0f * (1.0f - bal)), g2 = juce::jmin (1.0f, 2.0f * bal);
        const float noise  = clamp01 (s.noise + mod[dstNoise]);
        const bool  super  = s.model1 == 1;

        // SUPER: 7 serras espalhadas em volta da nota; os lados entram com o SHAPE
        float superInc[7] {}, superGain[7] {};
        if (super)
        {
            static constexpr float offs[7] = { -1.0f, -0.62f, -0.3f, 0.0f, 0.29f, 0.6f, 0.97f };
            const float spread = clamp01 (s.spread1 + mod[dstSpread]);
            const float semis  = std::pow (spread, 1.5f) * 0.9f;
            const float side   = 0.15f + 0.85f * shape1;
            const float norm   = 1.0f / std::sqrt (1.0f + 6.0f * side * side);
            for (int k = 0; k < 7; ++k)
            {
                superInc[k]  = juce::jmin (0.45f, inc1 * std::exp2 (offs[k] * semis / 12.0f));
                superGain[k] = (k == 3 ? 1.0f : side) * norm;
            }
        }

        // ---- corte (em oitavas) ----
        const float velF   = 1.0f - s.fVel * (1.0f - vel);
        const float keyOct = (pitch + uniPitch - 60.0f) / 12.0f;
        const float lfoCut = gl.lfo1 * s.l1Cut * 3.0f + gl.lfo2 * s.l2Cut * 3.0f;
        const float baseLg = std::log2 (s.cutoff) + lfoCut + mod[dstCutoff] * 5.0f;
        const float topLg  = std::log2 (juce::jmin (20000.0f, (float) fs * 0.45f));
        const float lc1 = juce::jlimit (4.0f, topLg, baseLg + s.env1 * fe * velF * 7.0f + s.key1 * keyOct);
        const float lc2 = juce::jlimit (4.0f, topLg, baseLg + s.cutoff2 / 12.0f + mod[dstCutoff2] * 5.0f
                                                     + s.env2 * fe * velF * 7.0f + s.key2 * keyOct);
        const float pi = juce::MathConstants<float>::pi;
        const float gEnd1 = std::tan (pi * std::exp2 (lc1) / (float) fs);
        const float gEnd2 = std::tan (pi * std::exp2 (lc2) / (float) fs);
        const float reso1 = clamp01 (s.reso1 + mod[dstReso]);
        const float reso2 = clamp01 (s.reso2 + mod[dstReso]);
        const float k1 = 2.0f * (1.0f - 0.97f * reso1), k2 = 2.0f * (1.0f - 0.97f * reso2);
        const float fbEnd = clamp01 (s.filtBal + mod[dstFiltBal]);

        // ---- VCA e pan ----
        const float ampVel = 1.0f - s.aVel * (1.0f - vel);
        const float trem   = 1.0f - s.l1Amp * (0.5f + 0.5f * gl.lfo1);
        const float volMod = std::exp2 (juce::jlimit (-1.0f, 1.0f, mod[dstVolume]) * 2.0f);
        const float ampEnd = ae * ampVel * trem * volMod;
        const float pan    = juce::jlimit (-1.0f, 1.0f, uniPan + gl.lfo2 * s.l2Pan + mod[dstPan]);
        const float plEnd  = std::cos ((pan + 1.0f) * pi * 0.25f), prEnd = std::sin ((pan + 1.0f) * pi * 0.25f);

        // oscVol: até a metade é volume; acima satura antes do filtro
        const float drive  = s.oscVol <= 0.5f ? s.oscVol * 2.0f : 1.0f + (s.oscVol - 0.5f) * 10.0f;
        const bool  sat    = s.oscVol > 0.5f;

        if (first)
        {
            gPrev1 = gEnd1; gPrev2 = gEnd2; ampPrev = 0.0f; plPrev = plEnd; prPrev = prEnd; fbPrev = fbEnd;
            first = false;
        }
        const float inv = 1.0f / (float) n;
        const float dg1 = (gEnd1 - gPrev1) * inv, dg2 = (gEnd2 - gPrev2) * inv, dA = (ampEnd - ampPrev) * inv;
        const float dpl = (plEnd - plPrev) * inv, dpr = (prEnd - prPrev) * inv, dfb = (fbEnd - fbPrev) * inv;
        float gg1 = gPrev1, gg2 = gPrev2, a = ampPrev, pl = plPrev, pr = prPrev, fb = fbPrev;
        const int f1Mode = s.f1Mode, f2Mode = s.f2Mode;
        const bool parallel = s.routing == 1;

        for (int i = 0; i < n; ++i)
        {
            gg1 += dg1; gg2 += dg2; a += dA; pl += dpl; pr += dpr; fb += dfb;

            // OSC1
            float o1 = 0.0f;
            bool wrapped = false;
            if (super)
            {
                for (int k = 0; k < 7; ++k)
                {
                    sp[k] += superInc[k];
                    if (sp[k] >= 1.0) { sp[k] -= 1.0; if (k == 3) wrapped = true; }
                    const float t = (float) sp[k];
                    o1 += (2.0f * t - 1.0f - blep (t, superInc[k])) * superGain[k];
                }
            }
            else
            {
                ph1 += inc1;
                if (ph1 >= 1.0) { ph1 -= 1.0; wrapped = true; }
                o1 = classic ((float) ph1, inc1, shape1, pw1);
            }

            // SUB: quadrada uma oitava abaixo do OSC1
            float sub = 0.0f;
            if (s.sub > 0.001f)
            {
                phSub += inc1 * 0.5f;
                if (phSub >= 1.0) phSub -= 1.0;
                const float t = (float) phSub, dt = inc1 * 0.5f;
                float t2 = t + 0.5f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                sub = (t < 0.5f ? 1.0f : -1.0f) + blep (t, dt) - blep (t2, dt);
            }

            // OSC2: SYNC (recomeça junto com o OSC1) e FM (o OSC1 empurra a fase)
            ph2 += inc2;
            if (s.sync2 && wrapped)
                ph2 = (super ? sp[3] : ph1) * (double) inc2 / (double) juce::jmax (1.0e-6f, super ? superInc[3] : inc1);
            while (ph2 >= 1.0) ph2 -= 1.0;
            float t2 = (float) ph2;
            if (pm > 0.0f)
            {
                t2 += pm * o1;
                t2 -= std::floor (t2);
            }
            const float o2 = classic (t2, inc2, shape2, pw2);

            float x = o1 * g1 + o2 * g2 + sub * s.sub;
            if (noise > 0.001f) x += (rand01f() * 2.0f - 1.0f) * noise;
            x *= drive * 0.5f;
            if (sat) x = fastTanh (x);

            // FILTROS
            float y1 = 0.0f, y2 = 0.0f;
            const bool need1 = ! parallel || fb < 0.999f, need2 = fb > 0.001f;
            if (need1)
                y1 = f1Mode == 0 ? ladder.process (x, gg1, reso1)
                                 : svf1.process (x, gg1, k1, f1Mode == 1 ? 0 : f1Mode - 1);
            if (need2)
                y2 = svf2.process (parallel ? x : y1, gg2, k2, f2Mode);
            const float y = y1 + (y2 - y1) * fb;

            const float out = y * a;
            l[i] += out * pl;
            r[i] += out * pr;
        }
        gPrev1 = gEnd1; gPrev2 = gEnd2; ampPrev = ampEnd; plPrev = plEnd; prPrev = prEnd; fbPrev = fbEnd;

        if (amp.isIdle()) { fenv.reset(); note = -1; }
    }

private:
    static inline float blep (float t, float dt)
    {
        if (dt <= 0.0f) return 0.0f;
        if (t < dt)        { t /= dt;             return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // CLASSIC: o SHAPE passa por seno → triângulo → serra → pulso (PW = largura do pulso)
    static inline float classic (float t, float dt, float shape, float pw)
    {
        const float a = shape * 3.0f;
        if (a < 2.0f)
        {
            const float tri = 1.0f - 4.0f * std::abs (t - 0.5f);
            if (a < 1.0f)
            {
                const float sn = -std::cos (juce::MathConstants<float>::twoPi * t);
                return sn + (tri - sn) * a;
            }
            const float sw = 2.0f * t - 1.0f - blep (t, dt);
            return tri + (sw - tri) * (a - 1.0f);
        }
        const float sw = 2.0f * t - 1.0f - blep (t, dt);
        float t2 = t + 1.0f - pw;
        if (t2 >= 1.0f) t2 -= 1.0f;
        const float pulse = (t < pw ? 1.0f : -1.0f) + blep (t, dt) - blep (t2, dt) - (2.0f * pw - 1.0f);
        return sw + (pulse - sw) * (a - 2.0f);
    }

    void resetFilters() { svf1.reset(); svf2.reset(); ladder.reset(); }

    uint32_t next() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    float  rand01f() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    double rand01()  { return (double) rand01f(); }

    double fs = 44100.0;
    Env amp, fenv;
    Svf svf1, svf2;
    Ladder ladder;
    double ph1 = 0.0, ph2 = 0.0, phSub = 0.0;
    std::array<double, 7> sp {};
    float pitch = 60.0f, target = 60.0f, vel = 1.0f, rnd = 0.0f, uniPitch = 0.0f, uniPan = 0.0f;
    float gPrev1 = 0.1f, gPrev2 = 0.1f, ampPrev = 0.0f, plPrev = 0.707f, prPrev = 0.707f, fbPrev = 0.0f;
    bool  first = true;
    uint32_t rng = 1;
};

// ---------------- LFO global ----------------
class Lfo
{
public:
    void reset() { phase = 0.0; hold = 0.0f; prevHold = 0.0f; value = 0.0f; }

    // avança n amostras; se synced, a fase vem da posição do host (ppq)
    float advance (int n, double fs, float rateHz, int shape, int sync, double bpm, bool hostRunning, double ppq)
    {
        double newPhase;
        if (sync > 0)
        {
            const double beats = lfoSyncBeats[juce::jlimit (1, 8, sync)];
            if (hostRunning) newPhase = ppq / beats;
            else             newPhase = phase + (double) n * bpm / 60.0 / beats / fs;
        }
        else
            newPhase = phase + (double) n * (double) rateHz / fs;

        if (std::floor (newPhase) != std::floor (phase))       // virou o ciclo: novo valor do S&H
        {
            prevHold = hold;
            hold = rng.nextFloat() * 2.0f - 1.0f;
        }
        phase = newPhase;
        const float t = (float) (phase - std::floor (phase));
        switch (shape)
        {
            case 0:  value = std::sin (juce::MathConstants<float>::twoPi * t); break;
            case 1:  value = 1.0f - 4.0f * std::abs (t - 0.5f); break;
            case 2:  value = 1.0f - 2.0f * t; break;
            case 3:  value = t < 0.5f ? 1.0f : -1.0f; break;
            case 4:  value = hold; break;
            default: value = prevHold + (hold - prevHold) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t)); break;
        }
        // quadrada e S&H com a quina arredondada (~2 ms): o LFO no corte não estala
        smooth += (value - smooth) * (1.0f - std::exp (-(float) n / (0.002f * (float) fs)));
        return shape == 3 || shape == 4 ? smooth : (smooth = value);
    }

    float get() const { return smooth; }

private:
    double phase = 0.0;
    float hold = 0.0f, prevHold = 0.0f, value = 0.0f, smooth = 0.0f;
    juce::Random rng;
};

// ---------------- arpejador ----------------
class Arp
{
public:
    void clear() { held.clear(); physical = 0; step = 0; }
    bool hasNotes() const { return ! held.empty(); }

    // HOLD: a primeira tecla depois de soltar todas começa um acorde novo
    void noteOn (int note, int velocity, bool hold)
    {
        if (hold && physical == 0) held.clear();
        ++physical;
        for (auto& h : held) if (h.first == note) return;
        if (held.size() < 32) held.push_back ({ note, velocity });
    }

    void noteOff (int note, bool hold)
    {
        physical = juce::jmax (0, physical - 1);
        if (! hold)
            held.erase (std::remove_if (held.begin(), held.end(), [note] (const std::pair<int, int>& h) { return h.first == note; }),
                        held.end());
    }

    void releaseHold() { if (physical == 0) held.clear(); }

    // notas do próximo passo (CHORD = todas juntas)
    void next (int mode, int octaves, juce::Random& rnd, std::vector<std::pair<int, int>>& out)
    {
        out.clear();
        if (held.empty()) return;
        std::vector<std::pair<int, int>> base (held);
        if (mode != 4) std::sort (base.begin(), base.end());
        std::vector<std::pair<int, int>> seq;
        for (int o = 0; o < octaves; ++o)
            for (auto& h : base)
                if (h.first + 12 * o <= 127) seq.push_back ({ h.first + 12 * o, h.second });
        const int n = (int) seq.size();
        if (mode == 5)
        {
            out = base;
            return;
        }
        int idx = 0;
        switch (mode)
        {
            case 1:  idx = n - 1 - (step % n); break;
            case 2:
            {
                const int cycle = juce::jmax (1, 2 * n - 2);
                const int p = step % cycle;
                idx = n == 1 ? 0 : (p < n ? p : cycle - p);
                break;
            }
            case 3:  idx = rnd.nextInt (n); break;
            default: idx = step % n; break;
        }
        ++step;
        out.push_back (seq[(size_t) juce::jlimit (0, n - 1, idx)]);
    }

    void restart() { step = 0; }

private:
    std::vector<std::pair<int, int>> held;
    int physical = 0, step = 0;
};

// ---------------- o sintetizador: vozes, teclas, unison, glide ----------------
class Synth
{
public:
    static constexpr int controlBlock = 32;

    std::atomic<int>   activeVoices { 0 };
    std::atomic<float> lfo1Now { 0.0f }, lfo2Now { 0.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int v = 0; v < maxVoices; ++v) voices[(size_t) v].prepare (fs, 0x9e3779b9u * (uint32_t) (v + 1));
        lfo1.reset();
        lfo2.reset();
        stack.clear();
        sustainDown = false;
        lastPitch = -1.0f;
        counter = 0;
        smReady = false;
    }

    void setWheel (float v)  { global.wheel = v; }
    void setAfter (float v)  { global.after = v; }
    void setBend (float v)   { global.bend = v; }

    void allNotesOff()
    {
        for (auto& v : voices) v.release();
        stack.clear();
        sustainDown = false;
    }

    void setSustain (bool down)
    {
        sustainDown = down;
        if (! down)
            for (auto& v : voices)
                if (v.sustained) { v.sustained = false; v.release(); }
    }

    void noteOn (int note, float velocity, const Settings& s)
    {
        const int uni = juce::jlimit (1, 4, s.unison);
        if (s.keyMode == 0)
        {
            const float from = s.portaMs > 0.5f && lastPitch >= 0.0f ? lastPitch : (float) note;
            const float r = random.nextFloat();
            for (int u = 0; u < uni; ++u)
            {
                auto& v = allocate();
                v.start (note, velocity, from, true, uniDetune (u, uni, s), uniPan (u, uni, s), r);
                v.age = ++counter;
            }
        }
        else
        {
            stack.erase (std::remove (stack.begin(), stack.end(), note), stack.end());
            stack.push_back (note);
            playMono (note, velocity, s, false);
        }
        lastPitch = (float) note;
    }

    void noteOff (int note, const Settings& s)
    {
        if (s.keyMode == 0)
        {
            for (auto& v : voices)
                if (v.note == note && v.gate)
                {
                    if (sustainDown) v.sustained = true;
                    else v.release();
                    v.gate = false;
                }
            return;
        }
        const bool wasTop = ! stack.empty() && stack.back() == note;
        stack.erase (std::remove (stack.begin(), stack.end(), note), stack.end());
        if (! wasTop) return;
        if (! stack.empty())
            playMono (stack.back(), monoVel, s, true);           // volta para a nota que continua segurada
        else
            for (int u = 0; u < maxVoices; ++u)
                if (voices[(size_t) u].gate)
                {
                    if (sustainDown) voices[(size_t) u].sustained = true;
                    else voices[(size_t) u].release();
                    voices[(size_t) u].gate = false;
                }
    }

    void render (float* l, float* r, int n, const Settings& s, double bpm, bool hostRunning, double ppq, double ppqPerSample)
    {
        int done = 0;
        while (done < n)
        {
            const int len = juce::jmin (controlBlock, n - done);
            const double p = ppq + ppqPerSample * (double) done;
            const Settings& cur = smooth (s, len);
            global.lfo1 = lfo1.advance (len, fs, s.l1Rate, s.l1Shape, s.l1Sync, bpm, hostRunning, p);
            global.lfo2 = lfo2.advance (len, fs, s.l2Rate, s.l2Shape, s.l2Sync, bpm, hostRunning, p);
            const float glide = s.portaMs > 0.5f ? 1.0f - std::exp (-(float) len / (s.portaMs * 0.001f * (float) fs / 3.0f)) : 1.0f;
            for (auto& v : voices)
                v.render (l + done, r + done, len, cur, global, glide);
            done += len;
        }
        int active = 0;
        for (auto& v : voices) if (v.isActive()) ++active;
        activeVoices.store (active);
        lfo1Now.store (global.lfo1);
        lfo2Now.store (global.lfo2);
    }

private:
    // knobs sem degrau: o que é contínuo (corte, ressonância, formas, mix…) anda até o valor novo em ~12 ms;
    // girar o knob ou automação pulando não estala. O resto (listas, tempos de envelope) passa direto.
    const Settings& smooth (const Settings& s, int len)
    {
        sm = s;
        const std::array<float, 17> tg { s.reso1, s.reso2, s.shape1, s.shape2, s.pw1, s.pw2, s.fm, s.bal, s.sub, s.oscVol,
                                         s.noise, s.filtBal, s.cutoff2, s.env1, s.env2, s.spread1, s.detune2 };
        const float lc = std::log2 (juce::jmax (20.0f, s.cutoff));
        if (! smReady) { smLc = lc; smV = tg; smReady = true; }
        const float k = 1.0f - std::exp (-(float) len / (0.012f * (float) fs));
        smLc += (lc - smLc) * k;
        for (size_t i = 0; i < tg.size(); ++i) smV[i] += (tg[i] - smV[i]) * k;
        sm.cutoff = std::exp2 (smLc);
        sm.reso1 = smV[0]; sm.reso2 = smV[1]; sm.shape1 = smV[2]; sm.shape2 = smV[3]; sm.pw1 = smV[4]; sm.pw2 = smV[5];
        sm.fm = smV[6]; sm.bal = smV[7]; sm.sub = smV[8]; sm.oscVol = smV[9]; sm.noise = smV[10]; sm.filtBal = smV[11];
        sm.cutoff2 = smV[12]; sm.env1 = smV[13]; sm.env2 = smV[14]; sm.spread1 = smV[15]; sm.detune2 = smV[16];
        return sm;
    }

    // voz livre; senão a mais baixa entre as soltas; senão a mais antiga
    Voice& allocate()
    {
        for (auto& v : voices) if (! v.isActive()) return v;
        Voice* best = nullptr;
        for (auto& v : voices)
            if (! v.gate && ! v.sustained && (best == nullptr || v.level() < best->level())) best = &v;
        if (best != nullptr) return *best;
        best = &voices[0];
        for (auto& v : voices) if (v.age < best->age) best = &v;
        return *best;
    }

    void playMono (int note, float velocity, const Settings& s, bool fromStack)
    {
        const int uni = juce::jlimit (1, 4, s.unison);
        const bool legatoPlaying = voices[0].gate && voices[0].isActive();
        const bool retrigger = ! (s.keyMode == 2 && legatoPlaying);
        if (! fromStack) monoVel = velocity;
        const float r = random.nextFloat();
        for (int u = 0; u < uni; ++u)
        {
            auto& v = voices[(size_t) u];
            const float from = s.portaMs > 0.5f && lastPitch >= 0.0f && v.isActive() ? lastPitch : (float) note;
            v.start (note, monoVel, from, retrigger, uniDetune (u, uni, s), uniPan (u, uni, s), r);
            v.age = ++counter;
        }
        for (int u = uni; u < maxVoices; ++u)                      // trocou o unison com nota soando
            if (voices[(size_t) u].gate) { voices[(size_t) u].release(); voices[(size_t) u].gate = false; }
        lastPitch = (float) note;
    }

    static float uniDetune (int u, int uni, const Settings& s)
    {
        if (uni < 2) return 0.0f;
        return ((float) u / (float) (uni - 1) * 2.0f - 1.0f) * s.uniDetune * s.uniDetune * 0.6f;
    }

    static float uniPan (int u, int uni, const Settings& s)
    {
        if (uni < 2) return 0.0f;
        return ((float) u / (float) (uni - 1) * 2.0f - 1.0f) * s.uniSpread;
    }

    double fs = 44100.0;
    std::array<Voice, maxVoices> voices;
    Lfo lfo1, lfo2;
    Global global;
    std::vector<int> stack;
    bool sustainDown = false;
    float lastPitch = -1.0f, monoVel = 1.0f;
    uint64_t counter = 0;
    Settings sm;
    std::array<float, 17> smV {};
    float smLc = 11.0f;
    bool  smReady = false;
    juce::Random random;
};

// ---------------- biquad (RBJ) ----------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<float, 2> z1 {}, z2 {};

    void reset() { z1.fill (0.0f); z2.fill (0.0f); }

    void set (int type, double fs, double f, double q, double gainDb)     // 0 peak, 1 low shelf, 2 high shelf
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, fs * 0.45, f) / fs;
        const double cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * q);
        double B0, B1, B2, A0, A1, A2;
        if (type == 0)
        {
            B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A;
            A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A;
        }
        else
        {
            const double sq = 2.0 * std::sqrt (A) * alpha, sgn = type == 1 ? 1.0 : -1.0;
            B0 = A * ((A + 1) - sgn * (A - 1) * cw + sq);
            B1 = sgn * 2 * A * ((A - 1) - sgn * (A + 1) * cw);
            B2 = A * ((A + 1) - sgn * (A - 1) * cw - sq);
            A0 = (A + 1) + sgn * (A - 1) * cw + sq;
            A1 = -sgn * 2 * ((A - 1) + sgn * (A + 1) * cw);
            A2 = (A + 1) + sgn * (A - 1) * cw - sq;
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    inline float process (float x, int ch)
    {
        const float y = b0 * x + z1[(size_t) ch];
        z1[(size_t) ch] = b1 * x - a1 * y + z2[(size_t) ch];
        z2[(size_t) ch] = b2 * x - a2 * y;
        return y;
    }
};

struct FxSettings
{
    float dlySend = 0, dlyFb = 0.4f, rvbSend = 0, rvbSize = 0.55f, rvbDamp = 0.45f;
    int   dlyTime = 3, distType = 0;
    float eqLow = 0, eqMid = 0, eqHigh = 0, distAmt = 0, boostAmt = 0, boostTune = 100;
    float choMix = 0, choInt = 0.45f, phaMix = 0, phaInt = 0.5f;
    double bpm = 120.0;
};

// ---------------- efeitos: distorção → analog boost → chorus → phaser → delay → reverb → EQ ----------------
class Fx
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        const int dlLen = (int) (fs * 4.2) + 8;                  // 1/2 a 30 BPM cabe
        for (auto& b : dl) b.assign ((size_t) dlLen, 0.0f);
        dlPos = 0;
        dlLp.fill (0.0f);
        timeSm.reset (fs, 0.08);
        timeSm.setCurrentAndTargetValue ((float) (fs * 0.375));
        const int chLen = (int) (fs * 0.05) + 8;
        for (auto& b : ch) b.assign ((size_t) chLen, 0.0f);
        chPos = 0;
        chPhase = phPhase = 0.0;
        for (auto& st : ap) st.fill (0.0f);
        phFb.fill (0.0f);
        dc.fill (0.0f);
        dcY.fill (0.0f);
        boost.reset();
        eqL.reset(); eqM.reset(); eqH.reset();
        lastBoost = lastTune = lastLow = lastMid = lastHigh = -999.0f;
        reverb.setSampleRate (fs);
        reverb.reset();
        wetL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        wetR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        for (auto* sm : { &distSm, &boostSm, &choSm, &phaSm, &dlySm, &rvbSm })
        {
            sm->reset (fs, 0.05);
            sm->setCurrentAndTargetValue (0.0f);
        }
    }

    void process (float* l, float* r, int n, const FxSettings& s)
    {
        const float twoPi = juce::MathConstants<float>::twoPi;

        // ---- distorção ----
        distSm.setTargetValue (s.distAmt);
        if (s.distAmt > 0.0f || distSm.isSmoothing())
        {
            for (int i = 0; i < n; ++i)
            {
                const float d = distSm.getNextValue();
                const float pre = 1.0f + d * d * 30.0f, post = 1.0f / std::sqrt (pre) * (1.0f + d * 0.5f);
                const float wet = juce::jmin (1.0f, d * 6.0f);
                float* chs[2] = { l + i, r + i };
                for (int c = 0; c < 2; ++c)
                {
                    const float x = *chs[c];
                    float y;
                    switch (s.distType)
                    {
                        case 0:  y = fastTanh (x * pre) * post; break;
                        case 1:  y = juce::jlimit (-0.8f, 0.8f, x * pre) * post * 1.1f; break;
                        case 2:  { const float v = x * pre; y = (v > 0.0f ? 1.0f - std::exp (-v) : -0.6f * (1.0f - std::exp (v * 1.6f))) * post * 1.2f; break; }
                        default: y = std::abs (fastTanh (x * pre)) * post * 1.4f; break;
                    }
                    // tira o DC que FUZZ e RECT criam
                    const float o = y - dc[(size_t) c] + 0.9995f * dcY[(size_t) c];
                    dc[(size_t) c] = y;
                    dcY[(size_t) c] = o;
                    *chs[c] = x + (o - x) * wet;
                }
            }
        }
        else distSm.skip (n);

        // ---- analog boost (grave encorpado na frequência do TUNE) ----
        boostSm.setTargetValue (s.boostAmt);
        if (s.boostAmt > 0.0f || boostSm.isSmoothing())
        {
            // o ganho do pico anda com o knob em passos de 32 amostras (0 dB = filtro neutro): ligar não estala;
            // a saturação entra junto com o ganho
            for (int i0 = 0; i0 < n; i0 += 32)
            {
                const int m = juce::jmin (32, n - i0);
                boostSm.skip (m);
                const float amt = boostSm.getCurrentValue();
                if (std::abs (amt - lastBoost) > 0.0005f || std::abs (s.boostTune - lastTune) > 0.5f)
                {
                    boost.set (0, fs, s.boostTune, 0.8, amt * 14.0);
                    lastBoost = amt;
                    lastTune = s.boostTune;
                }
                const float satMix = juce::jmin (1.0f, amt * 4.0f);
                for (int i = i0; i < i0 + m; ++i)
                {
                    const float bl = boost.process (l[i], 0), br = boost.process (r[i], 1);
                    l[i] = bl + (fastTanh (bl * 0.9f) / 0.9f - bl) * satMix;
                    r[i] = br + (fastTanh (br * 0.9f) / 0.9f - br) * satMix;
                }
            }
        }
        else { boostSm.skip (n); lastBoost = -999.0f; boost.reset(); }

        // ---- chorus (2 linhas moduladas, lados opostos) ----
        choSm.setTargetValue (s.choMix);
        if (s.choMix > 0.0f || choSm.isSmoothing())
        {
            const int len = (int) ch[0].size();
            const float rate = 0.12f + s.choInt * 1.4f, depth = (1.2f + s.choInt * 5.5f) * 0.001f * (float) fs;
            const float base = 0.009f * (float) fs;
            for (int i = 0; i < n; ++i)
            {
                const float mix = choSm.getNextValue();
                chPhase += (double) rate / fs;
                if (chPhase >= 1.0) chPhase -= 1.0;
                const float s0 = std::sin (twoPi * (float) chPhase), s1 = std::cos (twoPi * (float) chPhase);
                ch[0][(size_t) chPos] = l[i];
                ch[1][(size_t) chPos] = r[i];
                const float wl = readFrac (ch[0], chPos, base + depth * (0.5f + 0.5f * s0), len);
                const float wr = readFrac (ch[1], chPos, base + depth * (0.5f + 0.5f * s1), len);
                if (++chPos >= len) chPos = 0;
                l[i] = l[i] * (1.0f - mix * 0.4f) + wl * mix * 0.8f;
                r[i] = r[i] * (1.0f - mix * 0.4f) + wr * mix * 0.8f;
            }
        }
        else
        {
            // desligado, a linha continua gravando: ao ligar, a leitura já encontra o som (sem degrau do trecho vazio)
            choSm.skip (n);
            const int len = (int) ch[0].size();
            for (int i = 0; i < n; ++i)
            {
                ch[0][(size_t) chPos] = l[i];
                ch[1][(size_t) chPos] = r[i];
                if (++chPos >= len) chPos = 0;
            }
        }

        // ---- phaser (6 passa-tudo por lado, varredura de 200 Hz a 4 kHz) ----
        phaSm.setTargetValue (s.phaMix);
        if (s.phaMix > 0.0f || phaSm.isSmoothing())
        {
            const float rate = 0.05f + s.phaInt * 0.9f, fb = 0.2f + s.phaInt * 0.55f;
            for (int i0 = 0; i0 < n; i0 += 16)
            {
                const int m = juce::jmin (16, n - i0);
                phPhase += (double) rate * m / fs;
                if (phPhase >= 1.0) phPhase -= 1.0;
                // coeficiente calculado a cada 16 amostras e interpolado no meio (sem chiado de degrau)
                float dcf[2];
                for (int c = 0; c < 2; ++c)
                {
                    const float lfo = 0.5f + 0.5f * std::sin (twoPi * (float) phPhase + (c == 0 ? 0.0f : 1.6f));
                    const float f = 200.0f * std::pow (20.0f, lfo);
                    const float t = std::tan (juce::MathConstants<float>::pi * f / (float) fs);
                    const float target = (t - 1.0f) / (t + 1.0f);
                    if (! phReady) phCoef[(size_t) c] = target;
                    dcf[c] = (target - phCoef[(size_t) c]) / (float) m;
                }
                phReady = true;
                for (int i = i0; i < i0 + m; ++i)
                {
                    const float mix = phaSm.getNextValue();
                    float* chs[2] = { l + i, r + i };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float cf = (phCoef[(size_t) c] += dcf[c]);
                        float x = *chs[c] + phFb[(size_t) c] * fb;
                        for (int k = 0; k < 6; ++k)
                        {
                            auto& z = ap[(size_t) c][(size_t) k];
                            const float y = cf * x + z;
                            z = x - cf * y;
                            x = y;
                        }
                        phFb[(size_t) c] = fastTanh (x);
                        *chs[c] = *chs[c] * (1.0f - mix * 0.5f) + x * mix * 0.5f;
                    }
                }
            }
        }
        else phaSm.skip (n);

        // ---- delay no tempo, pingue-pongue, cada volta mais escura ----
        const double beats = delayBeats[juce::jlimit (0, 6, s.dlyTime)];
        timeSm.setTargetValue ((float) juce::jlimit (1.0, (double) dl[0].size() - 4.0, beats * 60.0 / s.bpm * fs));
        dlySm.setTargetValue (s.dlySend);
        if (s.dlySend > 0.0f || dlySm.isSmoothing() || tail > 0)
        {
            const int len = (int) dl[0].size();
            const float lpC = 1.0f - std::exp (-twoPi * 3200.0f / (float) fs);
            for (int i = 0; i < n; ++i)
            {
                const float send = dlySm.getNextValue();
                const float t = timeSm.getNextValue();
                const float yl = readFrac (dl[0], dlPos, t, len), yr = readFrac (dl[1], dlPos, t, len);
                dlLp[0] += (yl - dlLp[0]) * lpC;
                dlLp[1] += (yr - dlLp[1]) * lpC;
                dl[0][(size_t) dlPos] = (l[i] + r[i]) * 0.5f * send + fastTanh (dlLp[1] * s.dlyFb);
                dl[1][(size_t) dlPos] = fastTanh (dlLp[0] * s.dlyFb);
                if (++dlPos >= len) dlPos = 0;
                l[i] += yl * 0.8f;
                r[i] += yr * 0.8f;
            }
            // segue processando até a cauda morrer (o send pode ter ido a zero com eco no ar)
            tail = s.dlySend > 0.0f ? (int) (fs * 8.0) : juce::jmax (0, tail - n);
        }
        else
            dlySm.skip (n);

        // ---- reverb ----
        rvbSm.setTargetValue (s.rvbSend);
        if (s.rvbSend > 0.0f || rvbSm.isSmoothing() || rvbTail > 0)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.35f + 0.62f * s.rvbSize;
            rp.damping = 0.1f + 0.8f * s.rvbDamp;
            rp.width = 1.0f;
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            reverb.setParameters (rp);
            if ((int) wetL.size() < n) { wetL.resize ((size_t) n); wetR.resize ((size_t) n); }
            for (int i = 0; i < n; ++i)
            {
                const float send = rvbSm.getNextValue();
                wetL[(size_t) i] = l[i] * send;
                wetR[(size_t) i] = r[i] * send;
            }
            reverb.processStereo (wetL.data(), wetR.data(), n);
            for (int i = 0; i < n; ++i)
            {
                l[i] += wetL[(size_t) i] * 0.5f;
                r[i] += wetR[(size_t) i] * 0.5f;
            }
            rvbTail = s.rvbSend > 0.0f ? (int) (fs * 10.0) : juce::jmax (0, rvbTail - n);
        }
        else
            rvbSm.skip (n);

        // ---- EQ (grave 150 Hz, médio 1,2 kHz, agudo 6 kHz) ----
        if (std::abs (s.eqLow) > 0.05f || std::abs (s.eqMid) > 0.05f || std::abs (s.eqHigh) > 0.05f)
        {
            if (s.eqLow != lastLow)   { eqL.set (1, fs, 150.0, 0.7, s.eqLow);   lastLow = s.eqLow; }
            if (s.eqMid != lastMid)   { eqM.set (0, fs, 1200.0, 0.8, s.eqMid);  lastMid = s.eqMid; }
            if (s.eqHigh != lastHigh) { eqH.set (2, fs, 6000.0, 0.7, s.eqHigh); lastHigh = s.eqHigh; }
            for (int i = 0; i < n; ++i)
            {
                l[i] = eqH.process (eqM.process (eqL.process (l[i], 0), 0), 0);
                r[i] = eqH.process (eqM.process (eqL.process (r[i], 1), 1), 1);
            }
        }
    }

private:
    static inline float readFrac (const std::vector<float>& b, int pos, float delay, int len)
    {
        float rp = (float) pos - delay;
        while (rp < 0.0f) rp += (float) len;
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const int i1 = (i0 + 1) % len;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
    }

    double fs = 44100.0;
    std::array<std::vector<float>, 2> dl, ch;
    std::array<float, 2> dlLp {}, phFb {}, dc {}, dcY {};
    std::array<std::array<float, 6>, 2> ap {};
    std::array<float, 2> phCoef {};
    bool phReady = false;
    int dlPos = 0, chPos = 0, tail = 0, rvbTail = 0;
    double chPhase = 0.0, phPhase = 0.0;
    juce::SmoothedValue<float> timeSm, distSm, boostSm, choSm, phaSm, dlySm, rvbSm;
    Biquad boost, eqL, eqM, eqH;
    float lastBoost = -999.0f, lastTune = -999.0f, lastLow = -999.0f, lastMid = -999.0f, lastHigh = -999.0f;
    juce::Reverb reverb;
    std::vector<float> wetL, wetR;
};
} // namespace vacina
