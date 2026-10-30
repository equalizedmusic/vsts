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
// UNISON: a portadora e o modulador viram 3 cópias (centro + duas desafinadas, uma em cada lado do estéreo).
// FILTER: LP, BP ou VOWEL (dois formantes; o CUTOFF escolhe a vogal u-o-a-e-i, o F.ENV abre a "boca" no ataque).
// MOVE: LFO no tempo do DAW (RATE) que mexe no índice de FM e no filtro/vogal — o lead que "fala" e não fica parado.
// Depois vêm os efeitos: drive (com TONE), delay pingue-pongue no tempo e reverb.
namespace resenha
{
struct Timbre
{
    int   wave = 0;
    float ratio = 2.0f, fm = 0.5f, fb = 0.1f, fmDecMs = 180.0f, cut = 5000.0f, res = 0.2f, decMs = 300.0f, pan = 0.0f;
    float glideMs = 0.0f, pEnv = 0.0f, fEnv = 0.0f;     // pEnv em semitons; fEnv 0..1 (até +4 oitavas no corte)
    float bend = 0.0f;                                   // semitons no fim da fala
    float uni = 0.0f;                                    // 0..1
    int   filt = 0;                                      // 0 LP, 1 BP, 2 VOWEL
    float move = 0.0f;                                   // 0..1
    double moveBeats = 2.0;                              // período do MOVE em tempos
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
    double ppq = 0.0, ppqPerSample = 0.0;                // posição (tempos) no começo do trecho renderizado: fase do MOVE
};

// vogais para o filtro VOWEL, na ordem u-o-a-e-i (F1, F2 em Hz): o CUTOFF anda por elas
inline constexpr float vowelF[5][2] = { { 320.0f, 800.0f }, { 500.0f, 1000.0f }, { 750.0f, 1250.0f }, { 480.0f, 1900.0f }, { 300.0f, 2350.0f } };

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
        resetOsc();
        clearFilters();
        timbre = -1;
        smReady = false;
        setBend (0.0f, 0);
    }

    void noteOn (int timbreIndex, int midiNote, bool accented, float bendSt = 0.0f, int bendSamples = 0)
    {
        if (stage == Stage::idle || timbreIndex != timbre)
        {
            resetOsc();
            clearFilters();
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
        const float uniTg = t.uni;
        const float movTg = t.move;
        if (! smReady) { smFm = fmTg; smFb = fbTg; smCut = cutTg; smRes = resTg; smUni = uniTg; smMov = movTg; smReady = true; }

        const float gain = (accent ? 1.15f : 0.85f) * 0.32f;
        const float pan  = juce::jlimit (-1.0f, 1.0f, t.pan * 0.01f);
        const float gl = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f;
        const float gr = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f;
        const int   mode = juce::jlimit (0, 2, t.filt);
        const double lfoBeats = juce::jmax (0.0625, t.moveBeats);
        const float nyq = sr * 0.45f;
        const float vLo = std::log2 (150.0f), vSpan = std::log2 (18000.0f) - std::log2 (150.0f);

        for (int i = 0; i < n; ++i)
        {
            smFm  += (fmTg - smFm) * kSm;
            smFb  += (fbTg - smFb) * kSm;
            smCut += (cutTg - smCut) * kSm;
            smRes += (resTg - smRes) * kSm;
            smUni += (uniTg - smUni) * kSm;
            smMov += (movTg - smMov) * kSm;
            fbNow = smFb;

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

            // MOVE: seno no tempo do DAW (todas as vozes do timbre na mesma fase: a "fala" segue o compasso)
            float lfo = 0.0f;
            if (smMov > 0.001f)
            {
                const double lp = (s.ppq + s.ppqPerSample * (double) i) / lfoBeats;
                lfo = std::sin (twoPi * (float) (lp - std::floor (lp))) * smMov;
            }

            // altura: glide + "zap" de pitch no ataque (cai junto com o envelope de FM) + mergulho do fim da fala
            pitch += ((float) note - pitch) * aGl;
            if (bendLen > 0)
            {
                const float x = juce::jmin (1.0f, (float) bendPos / (float) bendLen);
                bendNow = bendSt * x * std::sqrt (x);        // começa devagar e acelera, como a mão no pitch bend
                if (bendPos < bendLen) ++bendPos;
            }
            const float hz = 440.0f * std::exp2 ((pitch + bendNow + pEnvSt * fmEnv * fmEnv - 69.0f) / 12.0f);
            const float index = juce::jmax (0.0f, smFm * (0.2f + 0.8f * fmEnv) * (1.0f + 0.7f * lfo));

            // UNISON: centro + duas cópias desafinadas (até ±25 cents), cada uma puxada para um lado
            const bool  uniOn = smUni > 0.002f;
            const float c0 = osc (0, hz, index, t.ratio, t.wave);
            float oL = c0, oR = c0;
            if (uniOn)
            {
                const float det  = std::exp2 (smUni * 25.0f / 1200.0f);
                const float side = juce::jmin (1.0f, smUni * 1.3f);
                const float mid  = 1.0f - 0.45f * smUni;          // o centro cede lugar às cópias: o estéreo abre
                const float c1 = osc (1, hz * det, index, t.ratio, t.wave);
                const float c2 = osc (2, hz / det, index, t.ratio, t.wave);
                const float norm = 1.0f / (mid + side * 0.7f);
                oL = (c0 * mid + side * (c1 * 0.9f + c2 * 0.1f)) * norm;
                oR = (c0 * mid + side * (c2 * 0.9f + c1 * 0.1f)) * norm;
            }

            float yL, yR;
            if (mode == 2)
            {
                // VOWEL: posição 0..4 (u-o-a-e-i) pelo CUTOFF, aberta pelo F.ENV no ataque e andando com o MOVE
                const float cutPos = (smCut - vLo) / vSpan;
                const float vp = juce::jlimit (0.0f, 3.999f, cutPos * 4.0f + t.fEnv * fmEnv * 2.5f + lfo * 1.6f);
                const int   vi = (int) vp;
                const float vf = vp - (float) vi;
                const float fa = juce::jmin (nyq, vowelF[vi][0] + (vowelF[vi + 1][0] - vowelF[vi][0]) * vf);
                const float fb2 = juce::jmin (nyq, vowelF[vi][1] + (vowelF[vi + 1][1] - vowelF[vi][1]) * vf);
                const float g1 = std::tan (juce::MathConstants<float>::pi * fa / sr);
                const float g2 = std::tan (juce::MathConstants<float>::pi * fb2 / sr);
                const float k  = 0.42f - 0.32f * smRes;      // RESO = formante mais estreito
                yL = vowel (0, oL, g1, g2, k);
                yR = uniOn ? vowel (1, oR, g1, g2, k) : yL;
            }
            else
            {
                // filtro SVF (TPT): LP ou BP; o MOVE anda até ±2 oitavas
                const float fc = std::exp2 (juce::jmin (cutMax, smCut + fEnvOct * fmEnv + lfo * 2.0f));
                const float g  = std::tan (juce::MathConstants<float>::pi * fc / sr);
                const float k  = 2.0f - 1.88f * smRes;
                yL = svf (0, oL, g, k, mode == 1);
                yR = uniOn ? svf (1, oR, g, k, mode == 1) : yL;
            }
            const float drv = 1.0f + smRes * 0.6f;
            yL = std::tanh (yL * drv);
            yR = uniOn ? std::tanh (yR * drv) : yL;

            const float a = amp * gain;
            l[i] += yL * a * gl;
            r[i] += yR * a * gr;
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

    void resetOsc()
    {
        for (auto& o : u) o = Osc {};
        // as cópias do uníssono começam em fases diferentes (senão nascem juntas e o estéreo demora a abrir)
        u[1].cph = 0.31; u[1].mph = 0.17;
        u[2].cph = 0.73; u[2].mph = 0.59;
    }

    void clearFilters()
    {
        for (auto& f : f2) f.fill (0.0f);
        for (auto& f : vw) f.fill (0.0f);
    }

    // uma cópia: modulador com feedback (média das duas últimas saídas, sem o chiado do feedback puro) → portadora
    // com a forma de onda (modulação de fase)
    float osc (int c, float hz, float index, float ratio, int wave)
    {
        auto& o = u[(size_t) c];
        const float twoPi = juce::MathConstants<float>::twoPi;
        const double inc  = juce::jlimit (0.0, 0.45, (double) hz / fs);
        const double minc = juce::jlimit (0.0, 0.49, (double) (hz * ratio) / fs);
        o.mph += minc;
        if (o.mph >= 1.0) o.mph -= 1.0;
        const float m = std::sin (twoPi * (float) o.mph + fbNow * 0.5f * (o.m1 + o.m2));
        o.m2 = o.m1;
        o.m1 = m;
        o.cph += inc;
        if (o.cph >= 1.0) o.cph -= 1.0;
        float phi = (float) o.cph + index * m / twoPi;
        phi -= std::floor (phi);
        float dt = phi - o.lastPhi;               // passo instantâneo (para o polyBLEP com a fase modulada)
        dt -= std::floor (dt + 0.5f);
        dt = juce::jlimit (1.0e-5f, 0.5f, std::abs (dt));
        o.lastPhi = phi;
        switch (wave)
        {
            case 1:  return 4.0f * std::abs (phi - 0.5f) - 1.0f;                       // TRI
            case 2:  return 2.0f * phi - 1.0f - blep (phi, dt);                         // SAW
            case 3:                                                                     // SQUARE
            {
                float sq = phi < 0.5f ? 1.0f : -1.0f;
                sq += blep (phi, dt);
                float p2 = phi + 0.5f;
                if (p2 >= 1.0f) p2 -= 1.0f;
                sq -= blep (p2, dt);
                return sq * 0.8f;
            }
            default: return std::sin (twoPi * phi);                                     // SINE
        }
    }

    // SVF (TPT) de um canal: passa-baixas ou passa-banda
    float svf (int c, float x, float g, float k, bool band)
    {
        auto& st = f2[(size_t) c];
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float v1 = a1 * (st[0] + g * (x - st[1]));
        const float v2 = st[1] + g * v1;
        st[0] = 2.0f * v1 - st[0];
        st[1] = 2.0f * v2 - st[1];
        return band ? v1 * k * 1.6f : v2;
    }

    // VOWEL de um canal: dois passa-bandas em paralelo (F1 e F2 um pouco abaixo) e um fio do som direto
    float vowel (int c, float x, float g1, float g2, float k)
    {
        auto& st = vw[(size_t) c];
        const float a1 = 1.0f / (1.0f + g1 * (g1 + k));
        const float v1 = a1 * (st[0] + g1 * (x - st[1]));
        const float v2 = st[1] + g1 * v1;
        st[0] = 2.0f * v1 - st[0];
        st[1] = 2.0f * v2 - st[1];
        const float b1 = 1.0f / (1.0f + g2 * (g2 + k));
        const float w1 = b1 * (st[2] + g2 * (x - st[3]));
        const float w2 = st[3] + g2 * w1;
        st[2] = 2.0f * w1 - st[2];
        st[3] = 2.0f * w2 - st[3];
        return (v1 + w1 * 0.75f) * k * 2.2f + x * 0.12f;
    }

    static float blep (float t, float dt)
    {
        if (t < dt)        { t /= dt;             return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    struct Osc { double cph = 0.0, mph = 0.0; float m1 = 0.0f, m2 = 0.0f, lastPhi = 0.0f; };
    std::array<Osc, 3> u {};
    std::array<std::array<float, 2>, 2> f2 {};      // SVF L/R
    std::array<std::array<float, 4>, 2> vw {};      // VOWEL L/R (dois formantes)
    double fs = 44100.0;
    float pitch = 60.0f, bendSt = 0.0f, bendNow = 0.0f;
    int   bendLen = 0, bendPos = 0;
    float amp = 0.0f, fmEnv = 0.0f, fbNow = 0.0f;
    float smFm = 0.0f, smFb = 0.0f, smCut = 10.0f, smRes = 0.0f, smUni = 0.0f, smMov = 0.0f;
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
