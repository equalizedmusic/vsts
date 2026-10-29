#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Params.h"

// RESENHA — motor: 4 timbres FM de 2 operadores (modulador com feedback → portadora com forma de onda), filtro
// passa-baixas com ressonância por voz, panorâmica por timbre. Cada timbre é monofônico (nota nova do mesmo timbre
// solta a anterior, ou desliza até a nova com GLIDE); as caudas de timbres diferentes se sobrepõem (pool de 8 vozes).
// O ataque de cada nota tem um só envelope (tempo do FM DEC) que move o índice de FM, o pitch (P.ENV) e o filtro (F.ENV).
// BEND: a nota que fecha a fala do timbre desliza até N semitons durante a duração dela (o "mergulho" do lead).
// Depois vêm os efeitos: drive (com TONE), delay pingue-pongue no tempo e reverb.
namespace resenha
{
struct Timbre
{
    int   wave = 0;
    float ratio = 2.0f, fm = 0.5f, fb = 0.1f, fmDecMs = 180.0f, cut = 5000.0f, res = 0.2f, decMs = 300.0f, pan = 0.0f;
    float glideMs = 0.0f, pEnv = 0.0f, fEnv = 0.0f;     // pEnv em semitons; fEnv 0..1 (até +4 oitavas no corte)
    float bend = 0.0f;                                   // semitons no fim da fala
};

struct Settings
{
    std::array<Timbre, numVoices> t;
    int   drvType = 0;
    float drive = 0.2f, tone = 0.7f;
    bool  dlOn = true, rvOn = true;
    int   dlDiv = 3;
    float dlFb = 0.45f, dlMix = 0.25f, rvSize = 0.55f, rvMix = 0.18f;
    double bpm = 120.0;
};

// ---------------- uma voz FM ----------------
class FmVoice
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset();
    }

    void reset()
    {
        stage = Stage::idle;
        amp = fmEnv = 0.0f;
        cph = mph = 0.0;
        m1 = m2 = 0.0f;
        ic1 = ic2 = 0.0f;
        lastPhi = 0.0f;
        timbre = -1;
        smReady = false;
        setBend (0.0f, 0);
    }

    void noteOn (int timbreIndex, int midiNote, bool accented, float bendSt = 0.0f, int bendSamples = 0)
    {
        if (stage == Stage::idle || timbreIndex != timbre)
        {
            cph = mph = 0.0;
            m1 = m2 = 0.0f;
            ic1 = ic2 = 0.0f;
            smReady = false;
            amp = 0.0f;
        }
        timbre = timbreIndex;
        note = midiNote;
        pitch = (float) midiNote;
        accent = accented;
        fmEnv = 1.0f;
        stage = Stage::attack;
        age = 0;
        setBend (bendSt, bendSamples);
    }

    // GLIDE: a voz que ainda soa desliza até a nota nova (sem recomeçar a fase) e o ataque (FM, pitch, filtro) dispara de novo
    void glideTo (int midiNote, bool accented, float bendSt = 0.0f, int bendSamples = 0)
    {
        note = midiNote;
        accent = accented;
        fmEnv = 1.0f;
        stage = Stage::attack;      // sobe de onde está (sem clique), sem zerar a fase
        age = 0;
        pitch += bendNow;           // o mergulho anterior vira o ponto de partida do glide (sem salto)
        setBend (bendSt, bendSamples);
    }

    void noteOff()             { if (stage != Stage::idle) stage = Stage::release; }
    void fastOff()             { if (stage != Stage::idle) stage = Stage::kill; }
    bool isActive() const      { return stage != Stage::idle; }
    bool isHeld() const        { return stage == Stage::attack || stage == Stage::decay; }
    int  getTimbre() const     { return timbre; }
    long long getAge() const   { return age; }
    float level() const        { return amp; }

    // soma n amostras em l/r
    void render (float* l, float* r, int n, const Settings& s)
    {
        if (stage == Stage::idle || timbre < 0) return;
        const auto& t = s.t[(size_t) timbre];
        const float sr = (float) fs;
        const float twoPi = juce::MathConstants<float>::twoPi;

        // glide: a altura anda até a nota (exponencial; GLIDE = tempo até chegar perto)
        const float aGl = t.glideMs < 1.0f ? 1.0f : 1.0f - std::exp (-1.0f / (t.glideMs * 0.001f * sr / 4.6f));
        const float pEnvSt = t.pEnv;
        const float fEnvOct = t.fEnv * 4.0f;
        const float cutMax = std::log2 (juce::jmin (18000.0f, sr * 0.45f));

        const float aAtk = 1.0f - std::exp (-1.0f / (0.0015f * sr));
        const float aDec = std::exp (-1.0f / (juce::jmax (5.0f, t.decMs) * 0.001f * sr / 4.6f));   // −40 dB em DECAY
        const float aRel = std::exp (-1.0f / (0.03f * sr));
        const float aKill = std::exp (-1.0f / (0.003f * sr));
        const float fDec = std::exp (-1.0f / (juce::jmax (2.0f, t.fmDecMs) * 0.001f * sr / 4.6f));
        const float kSm  = 1.0f - std::exp (-1.0f / (0.015f * sr));

        // FM: índice até ~7 rad; o envelope de FM vai do topo a 20% (o timbre "fala" no ataque e assenta)
        const float fmTg  = t.fm * t.fm * 7.0f * (accent ? 1.35f : 1.0f);
        const float fbTg  = t.fb * 1.5f;
        const float cutTg = std::log2 (juce::jlimit (60.0f, juce::jmin (18000.0f, sr * 0.45f), t.cut * (accent ? 1.5f : 1.0f)));
        const float resTg = t.res;
        if (! smReady) { smFm = fmTg; smFb = fbTg; smCut = cutTg; smRes = resTg; smReady = true; }

        const float gain = (accent ? 1.15f : 0.85f) * 0.32f;
        const float pan  = juce::jlimit (-1.0f, 1.0f, t.pan * 0.01f);
        const float gl = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f;
        const float gr = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f;

        for (int i = 0; i < n; ++i)
        {
            smFm  += (fmTg - smFm) * kSm;
            smFb  += (fbTg - smFb) * kSm;
            smCut += (cutTg - smCut) * kSm;
            smRes += (resTg - smRes) * kSm;

            switch (stage)
            {
                case Stage::attack:  amp += (1.0f - amp) * aAtk; if (amp > 0.99f) stage = Stage::decay; break;
                case Stage::decay:   amp *= aDec; break;
                case Stage::release: amp *= aRel; break;
                case Stage::kill:    amp *= aKill; break;
                case Stage::idle:    break;
            }
            if (stage != Stage::attack && amp < 1.0e-4f) { stage = Stage::idle; amp = 0.0f; break; }
            fmEnv *= fDec;

            // altura: glide + "zap" de pitch no ataque (cai junto com o envelope de FM) + mergulho do fim da fala
            pitch += ((float) note - pitch) * aGl;
            if (bendLen > 0)
            {
                const float x = juce::jmin (1.0f, (float) bendPos / (float) bendLen);
                bendNow = bendSt * x * std::sqrt (x);        // começa devagar e acelera, como a mão no pitch bend
                if (bendPos < bendLen) ++bendPos;
            }
            const float hz = 440.0f * std::exp2 ((pitch + bendNow + pEnvSt * fmEnv * fmEnv - 69.0f) / 12.0f);
            const double inc = juce::jlimit (0.0, 0.45, (double) hz / fs);
            const double minc = juce::jlimit (0.0, 0.49, (double) (hz * t.ratio) / fs);

            // modulador com feedback (média das duas últimas saídas: sem o chiado do feedback puro)
            mph += minc;
            if (mph >= 1.0) mph -= 1.0;
            const float m = std::sin (twoPi * (float) mph + smFb * 0.5f * (m1 + m2));
            m2 = m1;
            m1 = m;

            // portadora: modulação de fase sobre a forma de onda
            cph += inc;
            if (cph >= 1.0) cph -= 1.0;
            const float index = smFm * (0.2f + 0.8f * fmEnv);
            float phi = (float) cph + index * m / twoPi;
            phi -= std::floor (phi);
            float dt = phi - lastPhi;               // passo instantâneo (para o polyBLEP com a fase modulada)
            dt -= std::floor (dt + 0.5f);
            dt = juce::jlimit (1.0e-5f, 0.5f, std::abs (dt));
            lastPhi = phi;

            float osc;
            switch (t.wave)
            {
                case 1:  osc = 4.0f * std::abs (phi - 0.5f) - 1.0f; break;                       // TRI
                case 2:  osc = 2.0f * phi - 1.0f - blep (phi, dt); break;                         // SAW
                case 3:                                                                           // SQUARE
                {
                    float sq = phi < 0.5f ? 1.0f : -1.0f;
                    sq += blep (phi, dt);
                    float p2 = phi + 0.5f;
                    if (p2 >= 1.0f) p2 -= 1.0f;
                    sq -= blep (p2, dt);
                    osc = sq * 0.8f;
                    break;
                }
                default: osc = std::sin (twoPi * phi); break;                                     // SINE
            }

            // filtro SVF (TPT) passa-baixas
            const float fc = std::exp2 (juce::jmin (cutMax, smCut + fEnvOct * fmEnv));
            const float g  = std::tan (juce::MathConstants<float>::pi * fc / sr);
            const float k  = 2.0f - 1.88f * smRes;
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float v1 = a1 * (ic1 + g * (osc - ic2));
            const float v2 = ic2 + g * v1;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            const float y = std::tanh (v2 * (1.0f + smRes * 0.6f));

            const float out = y * amp * gain;
            l[i] += out * gl;
            r[i] += out * gr;
        }
        age += n;
    }

private:
    enum class Stage { idle, attack, decay, release, kill };

    void setBend (float st, int samples)
    {
        bendSt = st;
        bendLen = std::abs (st) >= 0.5f ? juce::jmax (1, samples) : 0;
        bendPos = 0;
        bendNow = 0.0f;
    }

    static float blep (float t, float dt)
    {
        if (t < dt)        { t /= dt;             return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    double fs = 44100.0, cph = 0.0, mph = 0.0;
    float pitch = 60.0f, bendSt = 0.0f, bendNow = 0.0f;
    int   bendLen = 0, bendPos = 0;
    float amp = 0.0f, fmEnv = 0.0f, m1 = 0.0f, m2 = 0.0f, ic1 = 0.0f, ic2 = 0.0f, lastPhi = 0.0f;
    float smFm = 0.0f, smFb = 0.0f, smCut = 10.0f, smRes = 0.0f;
    bool  smReady = false, accent = false;
    int   timbre = -1, note = 60;
    long long age = 0;
    Stage stage = Stage::idle;
};

// ---------------- as vozes ----------------
class Engine
{
public:
    static constexpr int poolSize = 8;

    // telemetria para a tela: nível de cada timbre (acende o cartão que está falando)
    std::array<std::atomic<float>, numVoices> talk {};

    void prepare (double sampleRate)
    {
        for (auto& v : pool) v.prepare (sampleRate);
        for (auto& t : talk) t.store (0.0f);
    }

    // nota nova num timbre: a nota anterior DESSE timbre solta (cada timbre é monofônico).
    // Com GLIDE, a voz desse timbre que ainda soa desliza para a nota nova em vez de soltar.
    // bendSt/bendSamples: o mergulho (BEND) quando a nota fecha a fala do timbre
    void noteOn (int timbre, int midiNote, bool accent, float glideMs = 0.0f, float bendSt = 0.0f, int bendSamples = 0)
    {
        if (glideMs >= 1.0f)
        {
            FmVoice* cur = nullptr;
            for (auto& v : pool)
                if (v.isActive() && v.getTimbre() == timbre && (cur == nullptr || v.level() > cur->level())) cur = &v;
            if (cur != nullptr && cur->level() > 0.02f)
            {
                for (auto& v : pool)
                    if (&v != cur && v.isActive() && v.getTimbre() == timbre && v.isHeld()) v.noteOff();
                cur->glideTo (juce::jlimit (12, 115, midiNote), accent, bendSt, bendSamples);
                return;
            }
        }

        for (auto& v : pool)
            if (v.isActive() && v.getTimbre() == timbre && v.isHeld()) v.noteOff();

        FmVoice* pick = nullptr;
        for (auto& v : pool)
            if (! v.isActive()) { pick = &v; break; }
        if (pick == nullptr)   // todas tocando: rouba a mais velha
        {
            pick = &pool[0];
            for (auto& v : pool)
                if (v.getAge() > pick->getAge()) pick = &v;
        }
        pick->noteOn (timbre, juce::jlimit (12, 115, midiNote), accent, bendSt, bendSamples);
    }

    void noteOff (int timbre)
    {
        for (auto& v : pool)
            if (v.isActive() && v.getTimbre() == timbre && v.isHeld()) v.noteOff();
    }

    void allOff()
    {
        for (auto& v : pool) v.noteOff();
    }

    void render (float* l, float* r, int n, const Settings& s)
    {
        std::array<float, numVoices> lv {};
        for (auto& v : pool)
        {
            if (! v.isActive()) continue;
            v.render (l, r, n, s);
            const int t = v.getTimbre();
            if (t >= 0 && t < numVoices) lv[(size_t) t] = juce::jmax (lv[(size_t) t], v.level());
        }
        for (int t = 0; t < numVoices; ++t)
            talk[(size_t) t].store (lv[(size_t) t]);
    }

private:
    std::array<FmVoice, poolSize> pool;
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

    void process (float* l, float* r, int n, const Settings& s)
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
        const float lpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3200.0f / (float) fs);
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
        rp.roomSize = 0.35f + 0.6f * s.rvSize;
        rp.damping = 0.45f;
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
} // namespace resenha
