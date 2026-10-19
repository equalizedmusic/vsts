#include "Engine.h"
#include "WaveBank.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>

// Motor da QUIMERA. Organização do laço:
//  - o bloco é fatiado entre eventos MIDI (amostra-precisa) e cada fatia em passos de controle de até kChunk amostras;
//  - a cada passo, por voz: envelopes (por amostra), LFOs, deriva, fontes → matriz → ALVOS;
//  - tudo o que soa (incremento de fase de cada uníssono, posição, warp, volumes, pan, corte/ressonância/drive) anda
//    em RAMPA linear do valor do passo anterior até o alvo — sem degrau (zipper) e sem trigonometria por amostra;
//  - osciladores rendem um uníssono inteiro por vez (laço interno curto) em buffers do passo e vão para as rotas.
// FM/RM: osc 2 ← osc 1 e osc 3 ← osc 2 usam a amostra ATUAL do anterior; osc 1 ← osc 3 usa o osc 3 atrasado de
// exatamente kChunk amostras (0,3 ms a 48 kHz) — atraso fixo, então o timbre não depende do fatiamento do MIDI.

namespace quimera
{
namespace
{
constexpr int    kChunk    = 16;     // passo de controle
constexpr int    kRing     = 32;     // histórico do osc 3 (≥ 2 × kChunk)
constexpr float  voiceGain = 0.22f;  // headroom: 4 vozes de serra cheia ≈ −6 dBFS
constexpr float  kPi       = 3.14159265358979f;
constexpr float  kTwoPi    = 6.28318530717959f;
constexpr float  kSqrt2    = 1.41421356f;

inline float clamp01 (float x) { return juce::jlimit (0.0f, 1.0f, x); }
inline float clampBi (float x) { return juce::jlimit (-1.0f, 1.0f, x); }

// tanh racional (erro < 2% até ±3, satura depois) — barato o bastante para rodar por amostra em 16 vozes
inline float fastTanh (float x)
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// dobra triangular: identidade em ±1, reflete o que passar
inline float foldTri (float x)
{
    float t = (x + 1.0f) * 0.25f;
    t -= std::floor (t);
    return 1.0f - 4.0f * std::abs (t - 0.5f);
}

// pan de potência constante com o centro em 1 em cada lado
inline void panGains (float pan, float& l, float& r)
{
    const float a = (clampBi (pan) + 1.0f) * kPi * 0.25f;
    l = std::cos (a) * kSqrt2;
    r = std::sin (a) * kSqrt2;
}

inline double pitchToInc (double semis, double fs)
{
    return 440.0 * std::exp2 ((semis - 69.0) / 12.0) / fs;
}

inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct Rng
{
    std::uint32_t s = 0x9E3779B9u;
    void seed (std::uint32_t v) { s = v != 0 ? v : 0x9E3779B9u; }
    std::uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    float bi()  { return uni() * 2.0f - 1.0f; }                              // −1..1
};

// ---------------- envelope ADSR com curva ----------------
// Curva c (−1..1) em cada segmento: progresso y = (1 − e^(−a·x)) / (1 − e^(−a)), a = −8c. c < 0 = anda rápido no
// começo e assenta devagar (o "exponencial" clássico, estilo Serum); c = 0 linear; c > 0 começa devagar.
// e^(−a·x) é levado por multiplicação (uma conta por amostra), em double para não acumular erro em segmentos longos.
struct Env
{
    enum Stage { idle, attack, decay, sustain, release };
    Stage  stage = idle;
    float  value = 0.0f, from = 0.0f;
    int    remain = 0;
    bool   linear = true;
    double x = 0.0, dx = 0.0, e = 1.0, ratio = 1.0, norm = 1.0;

    void segment (float ms, float curve, double fs)
    {
        const int n = juce::jmax (1, (int) std::lround ((double) ms * 0.001 * fs));
        remain = n;
        dx = 1.0 / (double) n;
        x = 0.0;
        from = value;
        const double a = -8.0 * (double) juce::jlimit (-1.0f, 1.0f, curve);
        linear = std::abs (a) < 1.0e-3;
        e = 1.0;
        ratio = linear ? 1.0 : std::exp (-a * dx);
        norm  = linear ? 1.0 : 1.0 / (1.0 - std::exp (-a));
    }

    void noteOn (const EnvSnap& p, double fs)  { stage = attack; segment (p.a, p.curve * 0.01f, fs); }
    void noteOff (const EnvSnap& p, double fs)
    {
        if (stage == idle || stage == release) return;
        stage = release;
        segment (p.r, p.curve * 0.01f, fs);
    }
    void clear() { stage = idle; value = 0.0f; }

    float next (const EnvSnap& p, double fs)
    {
        const float sus = clamp01 (p.s * 0.01f);
        switch (stage)
        {
            case idle:    value = 0.0f; return 0.0f;
            case sustain: value += (sus - value) * 0.002f; return value;   // sustain mexido na mão: sem degrau
            case attack: case decay: case release: default: break;
        }
        x += dx;
        double y;
        if (linear) y = x;
        else { e *= ratio; y = (1.0 - e) * norm; }
        const float tgt = stage == attack ? 1.0f : stage == decay ? sus : 0.0f;
        value = from + (tgt - from) * (float) y;
        if (--remain <= 0)
        {
            value = tgt;
            if (stage == attack)      { stage = decay; segment (p.d, p.curve * 0.01f, fs); }
            else if (stage == decay)  stage = sustain;
            else                      { stage = idle; value = 0.0f; }
        }
        return value;
    }
};

// ---------------- LFO por voz ----------------
struct Lfo
{
    double phase = 0.0, age = 0.0;   // age em segundos desde o note-on (para a subida)
    float  value = 0.0f, sh = 0.0f, rPrev = 0.0f, rNext = 0.0f;
    bool   stopped = false;

    void start (int mode, double freePhase, Rng& rng)
    {
        phase = mode == 0 ? freePhase : 0.0;
        age = 0.0;
        stopped = false;
        sh = rng.bi(); rPrev = rng.bi(); rNext = rng.bi();
    }
    void newCycle (Rng& rng) { sh = rng.bi(); rPrev = rNext; rNext = rng.bi(); }

    float shape (int s) const
    {
        const float p = (float) phase;
        switch (s)
        {
            case 0:  return std::sin (kTwoPi * p);
            case 1:  return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
            case 2:  return 1.0f - 2.0f * p;
            case 3:  return 2.0f * p - 1.0f;
            case 4:  return p < 0.5f ? 1.0f : -1.0f;
            case 5:  return sh;
            default: { const float t = p * p * (3.0f - 2.0f * p); return rPrev + (rNext - rPrev) * t; }
        }
    }
};

// ---------------- filtro por voz (estéreo) ----------------
struct FilterState
{
    float z[2][4] {};                    // ladder
    float ic1[2] {}, ic2[2] {};          // SVF
    float fz1[3][2] {}, fz2[3][2] {};    // formante (3 bandas)
    float* line[2] { nullptr, nullptr }; // pente (linha de atraso no pool do motor)
    int   lineMask = 0, w = 0;
    bool  lineDirty = false;
    float damp[2] {};
    // valores atuais das rampas
    float g = 0.1f, k = 1.0f, drive = 0.0f, mix = 1.0f, delay = 100.0f;
    float fg[3] { 0.1f, 0.1f, 0.1f };
    int   type = -1;
    bool  primed = false;

    void clear()
    {
        for (auto& c : z) for (auto& s : c) s = 0.0f;
        ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f;
        for (int b = 0; b < 3; ++b) for (int c = 0; c < 2; ++c) fz1[b][c] = fz2[b][c] = 0.0f;
        damp[0] = damp[1] = 0.0f;
        if (lineDirty)
        {
            for (auto* l : line)
                if (l != nullptr) std::fill (l, l + lineMask + 1, 0.0f);
            lineDirty = false;
        }
        primed = false;
    }
};

struct OscVoice
{
    std::array<double, maxUnison> phase {};
    std::array<float, maxUnison>  inc {};
    float pos = 0.0f, warp = 0.0f, gl = 0.0f, gr = 0.0f;
    bool  primed = false;   // falso = a próxima rampa começa direto no alvo (nota nova ou osc recém-ligado)
};

struct NoteData
{
    int   id = -1, note = 60;
    float vel = 0.8f, pressure = 0.0f, slide = 0.5f, bendSrc = 0.0f, bendSemis = 0.0f;
    float startPitch = 60.0f;   // de onde o glide sai
    bool  valid = false, released = false;
};

struct Voice
{
    bool  active = false, gate = false, held = false, primed = false, killing = false;
    int   noteID = -1, note = 60;
    float vel = 0.8f, pressure = 0.0f, slide = 0.5f, bendSrc = 0.0f, bendSemis = 0.0f, rnd = 0.0f;
    double pitch = 60.0, target = 60.0;
    std::uint32_t age = 0;

    std::array<Env, numEnvs> env;
    std::array<Lfo, numLfos> lfo;
    std::array<OscVoice, numOsc> osc;
    std::array<FilterState, numFilters> flt;

    double subPhase = 0.0;
    float  subInc = 0.0f, subLvl = 0.0f, nzLvl = 0.0f, nzLp[2] {};
    bool   subPrimed = false, nzPrimed = false;
    float  amp = 0.0f, panL = 1.0f, panR = 1.0f, killGain = 1.0f;

    std::array<float, numSrcs> src {};
    std::array<float, numDsts> mod {};

    // deriva: 0..2 = osciladores, 3 = filtro
    std::array<float, 4> drift {}, driftTarget {};
    int driftCount = 0;

    float ring[kRing] {};   // saída do osc 3 (para o FM/RM do osc 1)
    int   ringW = 0;

    float lastCut[numFilters] { 1000.0f, 1000.0f };
    float lastPos[numOsc] {}, lastWarp[numOsc] {};

    Rng rng;
    NoteData pending;      // nota esperando o fade do roubo
};

// ---------------- oscilador wavetable ----------------
struct OscJob
{
    const float* ta = nullptr;   // quadro de baixo
    const float* tb = nullptr;   // quadro de cima
    int    mask = 0;
    double size = 0.0;
    int    uni = 1;
    const float* uniL = nullptr;
    const float* uniR = nullptr;
    const float* incTarget = nullptr;
    float  fr0 = 0.0f, dfr = 0.0f;   // crossfade entre quadros (rampa)
    float  a0 = 0.0f, da = 0.0f;     // quantidade do warp (rampa)
    float  quantSteps = 0.0f;
    const float* mod = nullptr;      // saída do oscilador anterior (FM/RM)
};

// W = índice do warp (Params.h warpNames); os ramos somem na compilação
template <int W>
void renderOsc (double* phase, float* inc, const OscJob& j, int n, float* accL, float* accR, float* mono)
{
    const float invN = 1.0f / (float) n;
    for (int u = 0; u < j.uni; ++u)
    {
        double ph = phase[u];
        float  in = inc[u];
        const float dIn = (j.incTarget[u] - in) * invN;
        const float gl = j.uniL[u], gr = j.uniR[u];
        float fr = j.fr0, a = j.a0;

        for (int i = 0; i < n; ++i)
        {
            in += dIn; fr += j.dfr; a += j.da;

            double q = ph;
            if constexpr (W == 1)        { q = ph * (1.0 + 7.0 * (double) a); q -= std::floor (q); }                   // Sync
            else if constexpr (W == 2)   { const double k = 8.0 * (double) a; q = ph * (1.0 + k) / (1.0 + k * ph); }    // Dobra +
            else if constexpr (W == 3)   { const double k = 8.0 * (double) a; const double r = 1.0 - ph;
                                           q = 1.0 - r * (1.0 + k) / (1.0 + k * r); }                                   // Dobra −
            else if constexpr (W == 4)   { const double w = 0.5 - 0.45 * (double) a;                                     // PWM
                                           q = ph < w ? 0.5 * ph / w : 0.5 + 0.5 * (ph - w) / (1.0 - w); }
            else if constexpr (W == 5)   { const double m = 1.0 - 0.5 * (double) a;                                      // Espelho
                                           q = ph < m ? ph / m : (1.0 - ph) / (1.0 - m); }
            else if constexpr (W == 8)   { q = ph + 1.5 * (double) a * (double) j.mod[i]; q -= std::floor (q); }         // FM (fase)
            else if constexpr (W == 10)  { q = ph + 0.95 * (double) a * ph * (1.0 - ph); }                               // Assimetria

            const double x = q * j.size;
            int i0 = (int) x;
            const float f = (float) (x - (double) i0);
            i0 &= j.mask;
            const int i1 = (i0 + 1) & j.mask;
            const float sa = j.ta[i0] + f * (j.ta[i1] - j.ta[i0]);
            const float sb = j.tb[i0] + f * (j.tb[i1] - j.tb[i0]);
            float y = sa + fr * (sb - sa);

            if constexpr (W == 6)       y = foldTri (y * (1.0f + 5.0f * a));                                              // Dobrar
            else if constexpr (W == 7)  { if (j.quantSteps > 0.0f) y = std::floor (y * j.quantSteps + 0.5f) / j.quantSteps; } // Quantizar
            else if constexpr (W == 9)  y *= 1.0f - a + a * j.mod[i];                                                     // RM

            accL[i] += y * gl;
            accR[i] += y * gr;
            mono[i] += y;

            ph += (double) in;
            if (ph >= 1.0) ph -= 1.0;
        }
        phase[u] = ph;
        inc[u] = j.incTarget[u];
    }
}

using OscFn = void (*) (double*, float*, const OscJob&, int, float*, float*, float*);
const OscFn oscFns[numWarps] = { renderOsc<0>, renderOsc<1>, renderOsc<2>, renderOsc<3>, renderOsc<4>, renderOsc<5>,
                                 renderOsc<6>, renderOsc<7>, renderOsc<8>, renderOsc<9>, renderOsc<10> };

// quanto o warp acelera a leitura (para escolher a mip com folga)
inline double warpSpeed (int w, float a)
{
    switch (w)
    {
        case 1:  return 1.0 + 7.0 * a;
        case 2:
        case 3:  return 1.0 + 8.0 * a;
        case 4:  return 0.5 / (0.5 - 0.45 * a);
        case 5:  return 1.0 / (1.0 - 0.5 * a);
        case 8:  return 1.0 + 2.0 * a;
        case 10: return 1.0 + 0.95 * a;
        default: return 1.0;
    }
}

// formantes do filtro (Hz) A-E-I-O-U
const float filterVowels[5][3] = { { 730.0f, 1090.0f, 2440.0f }, { 530.0f, 1840.0f, 2480.0f }, { 270.0f, 2290.0f, 3010.0f },
                                   { 570.0f, 840.0f, 2410.0f },  { 300.0f, 870.0f, 2240.0f } };
} // namespace

// =====================================================================================================
struct Engine::Impl : public juce::MPEInstrument::Listener
{
    explicit Impl (Telemetry& t) : tel (t), bank (WaveBank::instance())
    {
        mpe.addListener (this);
        mpe.enableLegacyMode (2, juce::Range<int> (1, 17));
        setTracking();
        for (int i = 0; i < maxVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            v.rng.seed (0x1234567u + (std::uint32_t) i * 0x9E3779B1u);
            for (auto& d : v.drift) d = v.rng.bi();
            v.driftTarget = v.drift;
        }
    }

    ~Impl() override { mpe.removeListener (this); }

    // ---------------- ciclo de vida ----------------
    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        // o processo anda em passos de kChunk com rascunho fixo: aceita qualquer tamanho de bloco sem alocar
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        topHz = (float) juce::jmin (20000.0, fs * 0.45);
        killStep = (float) (1.0 / (0.003 * fs));
        nzCoef = 1.0f - std::exp (-kTwoPi * 700.0f / (float) fs);

        // linhas do filtro Pente: até 20 Hz (fs/20 amostras), potência de 2 por causa da máscara
        int len = 64;
        while (len < (int) std::ceil (fs / 20.0) + 8) len <<= 1;
        combPool.assign ((size_t) maxVoices * (size_t) numFilters * 2u * (size_t) len, 0.0f);
        size_t at = 0;
        for (auto& v : voices)
            for (auto& f : v.flt)
            {
                for (auto*& l : f.line) { l = combPool.data() + at; at += (size_t) len; }
                f.lineMask = len - 1;
                f.w = 0;
                f.lineDirty = false;
            }
        reset();
    }

    void reset()
    {
        muted = true;
        mpe.releaseAllNotes();
        muted = false;
        for (auto& v : voices)
        {
            v.active = v.gate = v.held = v.killing = false;
            v.pending.valid = false;
            v.noteID = -1;
            for (auto& e : v.env) e.clear();
            for (auto& f : v.flt) { f.lineDirty = true; f.clear(); }
        }
        hasLast = false;
    }

    // ---------------- bloco ----------------
    void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Snapshot& S, const Clock& clock)
    {
        const int n = out.getNumSamples();
        if (n <= 0 || out.getNumChannels() < 1) return;
        snap = &S;

        configure (S);
        prepareBlock (S, clock);
        if (prevHold && ! S.hold) releaseHeld();
        prevHold = S.hold;

        float* L = out.getWritePointer (0);
        float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

        int pos = 0;
        for (const auto meta : midi)
        {
            const int t = juce::jlimit (0, n, meta.samplePosition);
            if (t > pos) { renderRange (L, R, pos, t - pos); pos = t; }
            handleMidi (meta.getMessage());
        }
        if (pos < n) renderRange (L, R, pos, n - pos);

        advanceFreeLfos (S, n);
        writeTelemetry (S);
        snap = nullptr;
    }

    // ---------------- MIDI / MPE ----------------
    void setTracking()
    {
        // em modo legado o bend/pressão/CC74 do canal valem para TODAS as notas do canal (teclado comum)
        mpe.setPitchbendTrackingMode (juce::MPEInstrument::allNotesOnChannel);
        mpe.setPressureTrackingMode (juce::MPEInstrument::allNotesOnChannel);
        mpe.setTimbreTrackingMode (juce::MPEInstrument::allNotesOnChannel);
    }

    // só reconfigura quando MPE / bend / bend MPE mudam (trocar o layout solta as notas)
    void configure (const Snapshot& S)
    {
        const int b  = juce::jlimit (1, 96, juce::roundToInt (S.bend));
        const int mb = juce::jlimit (1, 96, juce::roundToInt (S.mpeBend));
        if (cfgValid && S.mpe == cfgMpe && b == cfgBend && mb == cfgMpeBend) return;

        if (S.mpe)
        {
            juce::MPEZoneLayout layout;
            layout.setLowerZone (15, mb, b);
            mpe.setZoneLayout (layout);
        }
        else if (! cfgValid || cfgMpe)
            mpe.enableLegacyMode (b, juce::Range<int> (1, 17));
        else
            mpe.setLegacyModePitchbendRange (b);

        setTracking();
        cfgValid = true; cfgMpe = S.mpe; cfgBend = b; cfgMpeBend = mb;
    }

    void handleMidi (const juce::MidiMessage& msg)
    {
        if (msg.isController() && msg.getControllerNumber() == 1)
            wheel = (float) msg.getControllerValue() / 127.0f;
        if (msg.isAllSoundOff())
            for (auto& v : voices)
                if (v.active) { v.killing = true; v.pending.valid = false; }
        mpe.processNextMidiEvent (msg);   // notas, bend, pressão, CC74, CC64, RPN de configuração MPE
    }

    static NoteData fromNote (const juce::MPENote& n)
    {
        NoteData d;
        d.id = (int) n.noteID;
        d.note = (int) n.initialNote;
        d.vel = n.noteOnVelocity.asUnsignedFloat();
        d.pressure = n.pressure.asUnsignedFloat();
        d.slide = n.timbre.asUnsignedFloat();
        d.bendSrc = n.pitchbend.asSignedFloat();
        d.bendSemis = (float) n.totalPitchbendInSemitones;
        d.startPitch = (float) d.note;
        d.valid = true;
        return d;
    }

    void noteAdded (juce::MPENote n) override
    {
        if (snap == nullptr || muted) return;
        NoteData d = fromNote (n);
        d.startPitch = (snap->glide > 0.5f && hasLast) ? lastPitch : (float) d.note;
        if (snap->polyMode == 0) startPoly (d);
        else                     startMono (d);
        lastPitch = (float) d.note;
        hasLast = true;
    }

    void noteReleased (juce::MPENote n) override
    {
        if (snap == nullptr || muted) return;
        const int id = (int) n.noteID;
        const bool mono = snap->polyMode != 0;
        for (size_t i = 0; i < voices.size(); ++i)
        {
            auto& v = voices[i];
            if (! v.active) continue;
            if (v.killing)
            {
                if (v.pending.valid && v.pending.id == id) v.pending.released = true;
                continue;
            }
            if (v.noteID != id || ! v.gate) continue;
            if (mono && i == 0) releaseMono (v, id);
            else                releaseOrHold (v);
        }
    }

    void notePitchbendChanged (juce::MPENote n) override
    {
        forNote (n, [&n] (float& bs, float& bsrc, float&, float&)
                 { bs = (float) n.totalPitchbendInSemitones; bsrc = n.pitchbend.asSignedFloat(); });
    }
    void notePressureChanged (juce::MPENote n) override
    {
        forNote (n, [&n] (float&, float&, float& pr, float&) { pr = n.pressure.asUnsignedFloat(); });
    }
    void noteTimbreChanged (juce::MPENote n) override
    {
        forNote (n, [&n] (float&, float&, float&, float& sl) { sl = n.timbre.asUnsignedFloat(); });
    }
    void noteKeyStateChanged (juce::MPENote) override {}   // o pedal (CC64) quem segura é o MPEInstrument
    void zoneLayoutChanged() override {}

    template <typename Fn>
    void forNote (const juce::MPENote& n, Fn&& fn)
    {
        const int id = (int) n.noteID;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (v.noteID == id && ! v.killing) fn (v.bendSemis, v.bendSrc, v.pressure, v.slide);
            if (v.killing && v.pending.valid && v.pending.id == id)
                fn (v.pending.bendSemis, v.pending.bendSrc, v.pending.pressure, v.pending.slide);
        }
    }

    // ---------------- alocação de vozes ----------------
    void startPoly (const NoteData& d)
    {
        const int limit = juce::jlimit (1, maxVoices, snap->voices);
        int busy = 0;
        Voice* freeV = nullptr;
        for (auto& v : voices)
        {
            if (v.active && ! v.killing) ++busy;
            if (! v.active && freeV == nullptr) freeV = &v;
        }
        if (freeV != nullptr && busy < limit) { startVoice (*freeV, d, true); return; }

        // roubo: a solta mais silenciosa; se todas presas, a mais antiga. A roubada faz fade de 3 ms e só então
        // começa a nota nova (sem clique).
        Voice* victim = nullptr;
        float  quiet = 1.0e9f;
        for (auto& v : voices)
            if (v.active && ! v.killing && ! v.gate && ! v.held && v.env[0].value < quiet) { quiet = v.env[0].value; victim = &v; }
        if (victim == nullptr)
            for (auto& v : voices)
                if (v.active && ! v.killing && (victim == nullptr || v.age < victim->age)) victim = &v;
        if (victim == nullptr)   // tudo em fade: troca a nota que estava esperando
            for (auto& v : voices)
                if (v.active && (victim == nullptr || v.age < victim->age)) victim = &v;
        if (victim == nullptr) return;

        victim->killing = true;
        victim->pending = d;
        victim->pending.valid = true;
        victim->pending.released = false;
    }

    void startMono (const NoteData& d)
    {
        Voice& v = voices[0];
        if (! v.active) { startVoice (v, d, true); return; }
        if (v.killing)  { v.pending = d; v.pending.valid = true; v.pending.released = false; return; }
        const bool legato = snap->polyMode == 2 && (v.gate || v.held);
        moveMono (v, d, ! legato);
    }

    // a voz mono muda de nota (glide a partir de onde está); retrig = redispara envelopes e LFOs
    void moveMono (Voice& v, const NoteData& d, bool retrig)
    {
        const Snapshot& S = *snap;
        v.noteID = d.id; v.note = d.note;
        v.target = (double) d.note;
        if (S.glide <= 0.5f) v.pitch = v.target;
        v.pressure = d.pressure; v.slide = d.slide; v.bendSrc = d.bendSrc; v.bendSemis = d.bendSemis;
        v.gate = true; v.held = false;
        if (retrig)
        {
            v.vel = d.vel;
            v.rnd = v.rng.bi();
            v.age = ++ageCounter;
            for (int e = 0; e < numEnvs; ++e) v.env[(size_t) e].noteOn (S.env[(size_t) e], fs);   // do valor atual: sem clique
            for (int l = 0; l < numLfos; ++l) v.lfo[(size_t) l].start (S.lfo[(size_t) l].mode, freePhase[(size_t) l], v.rng);
        }
    }

    void releaseMono (Voice& v, int id)
    {
        // volta para a última tecla ainda apertada (prioridade da última nota)
        bool found = false;
        juce::MPENote back;
        const int count = mpe.getNumPlayingNotes();
        for (int i = 0; i < count; ++i)
        {
            const juce::MPENote o = mpe.getNote (i);
            if ((int) o.noteID == id) continue;
            if (o.keyState == juce::MPENote::keyDown || o.keyState == juce::MPENote::keyDownAndSustained)
            {
                back = o;
                found = true;
            }
        }
        if (found)
        {
            const NoteData d = fromNote (back);
            moveMono (v, d, snap->polyMode == 1);
            lastPitch = (float) d.note;
        }
        else
            releaseOrHold (v);
    }

    void releaseOrHold (Voice& v)
    {
        if (snap != nullptr && snap->hold) { v.gate = false; v.held = true; return; }
        releaseVoice (v);
    }

    void releaseVoice (Voice& v)
    {
        v.gate = false;
        v.held = false;
        if (snap == nullptr) return;
        for (int e = 0; e < numEnvs; ++e) v.env[(size_t) e].noteOff (snap->env[(size_t) e], fs);
    }

    void releaseHeld()
    {
        for (auto& v : voices)
            if (v.active && v.held && ! v.killing) releaseVoice (v);
    }

    void startVoice (Voice& v, const NoteData& d, bool fresh)
    {
        const Snapshot& S = *snap;
        v.active = true; v.gate = true; v.held = false; v.killing = false; v.killGain = 1.0f;
        v.pending.valid = false;
        v.noteID = d.id; v.note = d.note; v.vel = d.vel;
        v.pressure = d.pressure; v.slide = d.slide; v.bendSrc = d.bendSrc; v.bendSemis = d.bendSemis;
        v.target = (double) d.note;
        v.pitch = (double) d.startPitch;
        v.rnd = v.rng.bi();
        v.age = ++ageCounter;

        for (int e = 0; e < numEnvs; ++e)
        {
            if (fresh) v.env[(size_t) e].clear();
            v.env[(size_t) e].noteOn (S.env[(size_t) e], fs);
        }
        for (int l = 0; l < numLfos; ++l)
            v.lfo[(size_t) l].start (S.lfo[(size_t) l].mode, freePhase[(size_t) l], v.rng);

        // fase inicial: Fase Aleat. sorteia; a deriva soma um pouco de imprecisão analógica
        const float d01 = clamp01 (S.drift * 0.01f);
        for (int o = 0; o < numOsc; ++o)
        {
            const float r01 = clamp01 (S.osc[(size_t) o].rand * 0.01f);
            auto& ov = v.osc[(size_t) o];
            for (int u = 0; u < maxUnison; ++u)
            {
                double p = (double) (r01 * v.rng.uni() + d01 * 0.05f * v.rng.uni());
                p -= std::floor (p);
                ov.phase[(size_t) u] = p;
            }
        }
        v.subPhase = 0.0;

        if (fresh)
        {
            for (auto& f : v.flt) f.clear();
            v.nzLp[0] = v.nzLp[1] = 0.0f;
            std::fill (std::begin (v.ring), std::end (v.ring), 0.0f);
            v.mod.fill (0.0f);
            v.primed = v.subPrimed = v.nzPrimed = false;
            for (auto& ov : v.osc) ov.primed = false;
        }
    }

    // ---------------- preparo do bloco ----------------
    struct OscPrep
    {
        bool  on = false;
        int   table = 0, warp = 0, route = 0, uni = 1;
        float uniNorm = 1.0f;
        std::array<float, maxUnison> spread {}, uniL {}, uniR {};
    };

    void prepareBlock (const Snapshot& S, const Clock& clock)
    {
        // matriz: só os slots que fazem algo
        nSlots = 0;
        for (int k = 0; k < numSlots; ++k)
        {
            const auto& sl = S.slot[(size_t) k];
            if (sl.src <= srcNone || sl.src >= numSrcs || sl.dst <= dstNone || sl.dst >= numDsts || sl.amt == 0.0f) continue;
            slotSrc[nSlots] = sl.src;
            slotDst[nSlots] = sl.dst;
            slotAmt[nSlots] = sl.amt * 0.01f;
            ++nSlots;
        }

        usesA = usesB = false;
        auto useRoute = [this] (int r) { if (r == 0 || r == 2) usesA = true; if (r == 1 || r == 2) usesB = true; };
        for (int o = 0; o < numOsc; ++o)
        {
            const auto& O = S.osc[(size_t) o];
            auto& P = prep[(size_t) o];
            P.on = O.on;
            P.table = juce::jlimit (0, numTables - 1, O.table);
            P.warp = juce::jlimit (0, numWarps - 1, O.warp);
            P.route = juce::jlimit (0, 3, O.route);
            P.uni = juce::jlimit (1, maxUnison, O.unison);
            P.uniNorm = 1.0f / std::sqrt ((float) P.uni);
            // uníssono: detune simétrico (−1..1) e a mesma posição no estéreo, aberta pela Abertura
            const float wid = clamp01 (O.width * 0.01f);
            for (int u = 0; u < maxUnison; ++u)
            {
                const float sp = P.uni > 1 ? 2.0f * (float) u / (float) (P.uni - 1) - 1.0f : 0.0f;
                P.spread[(size_t) u] = sp;
                panGains (sp * wid, P.uniL[(size_t) u], P.uniR[(size_t) u]);
            }
            if (P.on) useRoute (P.route);
        }
        subRoute = juce::jlimit (0, 3, S.subRoute);
        noiseRoute = juce::jlimit (0, 3, S.noiseRoute);
        if (S.subOn) useRoute (subRoute);
        if (S.noiseOn) useRoute (noiseRoute);
        if (S.filterSerial && usesA) usesB = true;

        const float c = clamp01 (S.noiseColor * 0.01f);
        nzDark = juce::jmax (0.0f, (0.5f - c) * 2.0f);
        nzBright = juce::jmax (0.0f, (c - 0.5f) * 2.0f);

        bpm = clock.bpm > 1.0 ? clock.bpm : 120.0;
        ppq0 = clock.ppq;
        ppqPerSample = bpm / 60.0 / fs;
        playing = clock.playing;
    }

    // relógio solto dos LFOs (modo Livre): as vozes novas pegam a fase daqui
    void advanceFreeLfos (const Snapshot& S, int n)
    {
        for (int l = 0; l < numLfos; ++l)
        {
            const auto& P = S.lfo[(size_t) l];
            const double beats = divBeats[juce::jlimit (0, numDivs - 1, P.div)];
            double ph;
            if (P.sync && playing) ph = (ppq0 + ppqPerSample * (double) n) / beats;
            else                   ph = freePhase[(size_t) l] + (P.sync ? bpm / 60.0 / beats : (double) P.rate) * (double) n / fs;
            freePhase[(size_t) l] = ph - std::floor (ph);
        }
    }

    // ---------------- render ----------------
    void renderRange (float* L, float* R, int start, int len)
    {
        int done = 0;
        while (done < len)
        {
            const int n = juce::jmin (kChunk, len - done);
            const int at = start + done;
            std::fill (mixL, mixL + n, 0.0f);
            std::fill (mixR, mixR + n, 0.0f);
            const double ppqEnd = ppq0 + ppqPerSample * (double) (at + n);
            for (auto& v : voices)
                if (v.active) renderVoice (v, n, ppqEnd);

            if (R != nullptr)
                for (int i = 0; i < n; ++i) { L[at + i] += mixL[i]; R[at + i] += mixR[i]; }
            else
                for (int i = 0; i < n; ++i) L[at + i] += 0.5f * (mixL[i] + mixR[i]);
            done += n;
        }
    }

    void advanceLfo (Voice& v, int l, int n, double ppqEnd)
    {
        const Snapshot& S = *snap;
        const auto& P = S.lfo[(size_t) l];
        auto& o = v.lfo[(size_t) l];
        const float mr = clampBi (v.mod[(size_t) (dstLfo1Rate + l)]);   // do passo anterior (a matriz vem depois)
        const double rateMul = std::exp2 (4.0 * (double) mr);           // ±4 oitavas
        const double beats = divBeats[juce::jlimit (0, numDivs - 1, P.div)];
        o.age += (double) n / fs;

        if (! o.stopped)
        {
            if (P.sync && playing && P.mode == 0 && std::abs (mr) < 1.0e-5f)
            {
                // Livre + sync + host tocando: fase presa ao grid
                double ph = ppqEnd / beats;
                ph -= std::floor (ph);
                if (ph < o.phase) o.newCycle (v.rng);
                o.phase = ph;
            }
            else
            {
                const double hz = (P.sync ? bpm / 60.0 / beats : (double) P.rate) * rateMul;
                o.phase += hz * (double) n / fs;
                if (o.phase >= 1.0)
                {
                    if (P.mode == 2) { o.phase = 1.0; o.stopped = true; }   // Envelope: uma volta só
                    else             { o.phase -= std::floor (o.phase); o.newCycle (v.rng); }
                }
            }
        }
        float val = o.shape (juce::jlimit (0, numLfoShapes - 1, P.shape));
        if (P.rise > 0.5f) val *= (float) juce::jmin (1.0, o.age * 1000.0 / (double) P.rise);
        o.value = val;
    }

    // deriva analógica: alvo aleatório novo a cada 0,25–0,8 s, perseguido devagar (τ ≈ 0,4 s)
    void advanceDrift (Voice& v, int n)
    {
        v.driftCount -= n;
        if (v.driftCount <= 0)
        {
            for (auto& t : v.driftTarget) t = v.rng.bi();
            v.driftCount = (int) ((0.25 + 0.55 * (double) v.rng.uni()) * fs);
        }
        const float c = juce::jmin (1.0f, (float) n / (0.4f * (float) fs));
        for (size_t k = 0; k < v.drift.size(); ++k)
            v.drift[k] += (v.driftTarget[k] - v.drift[k]) * c;
    }

    void renderVoice (Voice& v, int n, double ppqEnd)
    {
        const Snapshot& S = *snap;
        const float invN = 1.0f / (float) n;

        // glide exponencial em semitons (τ = glide/4: chega a ~98% no tempo do knob)
        if (S.glide > 0.5f && std::abs (v.pitch - v.target) > 1.0e-4)
        {
            const double tau = (double) S.glide * 0.001 * 0.25;
            v.pitch = v.target + (v.pitch - v.target) * std::exp (-(double) n / (tau * fs));
        }
        else
            v.pitch = v.target;

        // envelopes: o 1 por amostra (VCA), os outros só andam
        for (int i = 0; i < n; ++i) envBuf[i] = v.env[0].next (S.env[0], fs);
        for (int e = 1; e < numEnvs; ++e)
            for (int i = 0; i < n; ++i) v.env[(size_t) e].next (S.env[(size_t) e], fs);

        for (int l = 0; l < numLfos; ++l) advanceLfo (v, l, n, ppqEnd);
        advanceDrift (v, n);

        // fontes
        auto& src = v.src;
        src[srcNone] = 0.0f;
        src[srcEnv1] = v.env[0].value; src[srcEnv2] = v.env[1].value; src[srcEnv3] = v.env[2].value;
        src[srcLfo1] = v.lfo[0].value; src[srcLfo2] = v.lfo[1].value;
        src[srcVel] = v.vel;
        src[srcNote] = ((float) v.note - 60.0f) / 60.0f;
        src[srcWheel] = wheel;
        src[srcPressure] = v.pressure;
        src[srcSlide] = v.slide;
        src[srcBend] = v.bendSrc;
        src[srcRandom] = v.rnd;
        for (int mc = 0; mc < numMacros; ++mc) src[(size_t) (srcMacro1 + mc)] = clamp01 (S.macro[(size_t) mc] * 0.01f);

        // matriz (somada por destino; as escalas de cada destino são aplicadas onde ele é usado)
        auto& m = v.mod;
        m.fill (0.0f);
        for (int k = 0; k < nSlots; ++k)
            m[(size_t) slotDst[k]] += slotAmt[k] * src[(size_t) slotSrc[k]];

        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 2; ++c) std::fill (rb[r][c], rb[r][c] + n, 0.0f);

        const double base = v.pitch + (double) v.bendSemis + 48.0 * (double) m[dstPitch];
        const float d01 = clamp01 (S.drift * 0.01f);

        // ---- osciladores ----
        for (int o = 0; o < numOsc; ++o)
        {
            float* mono = oscMono[o];
            std::fill (mono, mono + n, 0.0f);
            const OscPrep& P = prep[(size_t) o];
            if (! P.on)
            {
                v.osc[(size_t) o].primed = false;
                if (o == numOsc - 1)   // desligado conta como silêncio para o FM/RM do osc 1
                    for (int i = 0; i < n; ++i) v.ring[(v.ringW + i) & (kRing - 1)] = 0.0f;
                continue;
            }
            const OscSnap& O = S.osc[(size_t) o];
            OscVoice& ov = v.osc[(size_t) o];
            const size_t b = (size_t) (dstO1Pitch + o * 5);

            const double semis = base + 12.0 * (double) O.oct + (double) O.semi + 0.01 * (double) O.fine
                               + 48.0 * (double) m[b] + 0.12 * (double) (d01 * v.drift[(size_t) o]);   // deriva ±12 ct
            const double inc0 = juce::jlimit (1.0e-7, 0.45, pitchToInc (semis, fs));
            const float posT  = clamp01 (O.pos * 0.01f + m[b + 1]);
            const float warpT = clamp01 (O.warpAmt * 0.01f + m[b + 2]);
            const float lvlT  = clamp01 (O.level * 0.01f + m[b + 3]);
            const float panT  = clampBi (O.pan * 0.01f + m[b + 4]);
            const float detT  = clamp01 (O.detune * 0.01f + m[dstDetune]);

            float incT[maxUnison] {};
            for (int u = 0; u < P.uni; ++u)
            {
                const double cents = (double) (P.spread[(size_t) u] * detT) * 50.0;   // até ±50 cents
                incT[u] = (float) juce::jmin (0.45, inc0 * std::exp2 (cents / 1200.0));
            }
            float gLT, gRT;
            panGains (panT, gLT, gRT);
            gLT *= lvlT * P.uniNorm;
            gRT *= lvlT * P.uniNorm;

            if (! ov.primed)
            {
                ov.pos = posT; ov.warp = warpT; ov.gl = gLT; ov.gr = gRT;
                for (int u = 0; u < maxUnison; ++u) ov.inc[(size_t) u] = incT[u];
                ov.primed = true;
            }

            // quadros vizinhos e crossfade (a rampa da posição fica dentro do mesmo par)
            const float fp0 = ov.pos * (float) (WaveBank::frames - 1), fp1 = posT * (float) (WaveBank::frames - 1);
            const int f0 = juce::jlimit (0, WaveBank::frames - 2, (int) std::floor (juce::jmin (fp0, fp1)));
            const float fr0 = clamp01 (fp0 - (float) f0), fr1 = clamp01 (fp1 - (float) f0);
            const int mip = WaveBank::mipFor (inc0 * warpSpeed (P.warp, warpT));
            const int N = WaveBank::mipSize (mip);

            OscJob j;
            j.ta = bank.data (P.table, f0, mip);
            j.tb = bank.data (P.table, f0 + 1, mip);
            j.mask = N - 1;
            j.size = (double) N;
            j.uni = P.uni;
            j.uniL = P.uniL.data();
            j.uniR = P.uniR.data();
            j.incTarget = incT;
            j.fr0 = fr0; j.dfr = (fr1 - fr0) * invN;
            j.a0 = ov.warp; j.da = (warpT - ov.warp) * invN;
            j.quantSteps = (P.warp == 7 && warpT > 0.001f) ? std::exp2 (1.0f + 11.0f * (1.0f - warpT)) : 0.0f;
            j.mod = zeros;
            if (P.warp == 8 || P.warp == 9)
            {
                if (o == 0)
                {
                    if (prep[(size_t) (numOsc - 1)].on)
                    {
                        for (int i = 0; i < n; ++i) modTmp[i] = v.ring[(v.ringW + i + kRing - kChunk) & (kRing - 1)];
                        j.mod = modTmp;
                    }
                }
                else if (prep[(size_t) (o - 1)].on)
                    j.mod = oscMono[o - 1];
            }

            std::fill (accL, accL + n, 0.0f);
            std::fill (accR, accR + n, 0.0f);
            oscFns[P.warp] (ov.phase.data(), ov.inc.data(), j, n, accL, accR, mono);

            float gl = ov.gl, gr = ov.gr;
            const float dgl = (gLT - gl) * invN, dgr = (gRT - gr) * invN;
            float* dl = rb[P.route][0];
            float* dr = rb[P.route][1];
            for (int i = 0; i < n; ++i)
            {
                gl += dgl; gr += dgr;
                dl[i] += accL[i] * gl;
                dr[i] += accR[i] * gr;
                mono[i] *= P.uniNorm;   // modulador = o oscilador ANTES do volume (volume 0 ainda modula)
            }
            ov.pos = posT; ov.warp = warpT; ov.gl = gLT; ov.gr = gRT;
            v.lastPos[o] = posT; v.lastWarp[o] = warpT;

            if (o == numOsc - 1)
                for (int i = 0; i < n; ++i) v.ring[(v.ringW + i) & (kRing - 1)] = mono[i];
        }
        v.ringW = (v.ringW + n) & (kRing - 1);

        // ---- sub: 1 ou 2 oitavas abaixo da nota (sem oitava/semitom dos osciladores) ----
        if (S.subOn)
        {
            const double semis = base - 12.0 * (double) (juce::jlimit (0, 1, S.subOct) + 1) + 0.12 * (double) (d01 * v.drift[0]);
            const float incT = (float) juce::jlimit (1.0e-7, 0.45, pitchToInc (semis, fs));
            const float lvlT = clamp01 (S.subLevel * 0.01f + m[dstSubLevel]);
            if (! v.subPrimed) { v.subInc = incT; v.subLvl = lvlT; v.subPrimed = true; }
            float in = v.subInc, lv = v.subLvl;
            const float din = (incT - in) * invN, dlv = (lvlT - lv) * invN;
            float* dl = rb[subRoute][0];
            float* dr = rb[subRoute][1];
            double ph = v.subPhase;
            const int wave = juce::jlimit (0, 2, S.subWave);
            for (int i = 0; i < n; ++i)
            {
                in += din; lv += dlv;
                const float p = (float) ph;
                float y;
                if (wave == 0)      y = std::sin (kTwoPi * p);
                else if (wave == 1) y = p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
                else
                {
                    float p2 = p + 0.5f;
                    if (p2 >= 1.0f) p2 -= 1.0f;
                    y = (p < 0.5f ? 1.0f : -1.0f) + polyBlep (p, in) - polyBlep (p2, in);
                }
                dl[i] += y * lv;
                dr[i] += y * lv;
                ph += (double) in;
                if (ph >= 1.0) ph -= 1.0;
            }
            v.subPhase = ph; v.subInc = incT; v.subLvl = lvlT;
        }
        else
            v.subPrimed = false;

        // ---- ruído: branco com tilt (escuro = passa-baixa, brilhante = o que sobra acima), L/R independentes ----
        if (S.noiseOn)
        {
            const float lvlT = clamp01 (S.noiseLevel * 0.01f + m[dstNoiseLevel]) * 0.5f;
            if (! v.nzPrimed) { v.nzLvl = lvlT; v.nzPrimed = true; }
            float lv = v.nzLvl;
            const float dlv = (lvlT - lv) * invN;
            for (int i = 0; i < n; ++i)
            {
                lv += dlv;
                for (int c = 0; c < 2; ++c)
                {
                    const float w = v.rng.bi();
                    v.nzLp[c] += (w - v.nzLp[c]) * nzCoef;
                    const float y = w + nzDark * (v.nzLp[c] * 3.0f - w) + nzBright * ((w - v.nzLp[c]) * 1.3f - w);
                    rb[noiseRoute][c][i] += y * lv;
                }
            }
            v.nzLvl = lvlT;
        }
        else
            v.nzPrimed = false;

        // ---- filtros ----
        const float keyNote = (float) (v.pitch + (double) v.bendSemis);
        const float env2 = v.env[1].value;
        if (usesA)
        {
            for (int i = 0; i < n; ++i) { inL[i] = rb[0][0][i] + rb[2][0][i]; inR[i] = rb[0][1][i] + rb[2][1][i]; }
            filterStage (v, 0, inL, inR, aL, aR, n, keyNote, env2, d01);
        }
        else { std::fill (aL, aL + n, 0.0f); std::fill (aR, aR + n, 0.0f); }

        if (S.filterSerial)
        {
            // série: A recebe rota A + (A+B); B recebe a saída de A + rota B
            if (usesB)
            {
                for (int i = 0; i < n; ++i) { inL[i] = aL[i] + rb[1][0][i]; inR[i] = aR[i] + rb[1][1][i]; }
                filterStage (v, 1, inL, inR, bL, bR, n, keyNote, env2, d01);
                for (int i = 0; i < n; ++i) { finL[i] = bL[i] + rb[3][0][i]; finR[i] = bR[i] + rb[3][1][i]; }
            }
            else
                for (int i = 0; i < n; ++i) { finL[i] = rb[3][0][i]; finR[i] = rb[3][1][i]; }
        }
        else
        {
            // paralelo: A e B separados (A+B entra nos dois) e somados
            if (usesB)
            {
                for (int i = 0; i < n; ++i) { inL[i] = rb[1][0][i] + rb[2][0][i]; inR[i] = rb[1][1][i] + rb[2][1][i]; }
                filterStage (v, 1, inL, inR, bL, bR, n, keyNote, env2, d01);
            }
            else { std::fill (bL, bL + n, 0.0f); std::fill (bR, bR + n, 0.0f); }
            for (int i = 0; i < n; ++i) { finL[i] = aL[i] + bL[i] + rb[3][0][i]; finR[i] = aR[i] + bR[i] + rb[3][1][i]; }
        }

        // ---- VCA (env 1 × velocidade × matriz), pan da voz, fade do roubo ----
        const float velG = 1.0f - clamp01 (S.velAmp * 0.01f) * (1.0f - v.vel);
        const float ampT = velG * juce::jmax (0.0f, 1.0f + m[dstAmp]) * voiceGain;
        float plT, prT;
        panGains (m[dstPan], plT, prT);
        if (! v.primed) { v.amp = ampT; v.panL = plT; v.panR = prT; }
        float a = v.amp, pl = v.panL, pr = v.panR;
        const float da = (ampT - a) * invN, dpl = (plT - pl) * invN, dpr = (prT - pr) * invN;
        for (int i = 0; i < n; ++i)
        {
            a += da; pl += dpl; pr += dpr;
            float k = 1.0f;
            if (v.killing)
            {
                v.killGain = juce::jmax (0.0f, v.killGain - killStep);
                k = v.killGain;
            }
            const float g = envBuf[i] * a * k;
            mixL[i] += finL[i] * g * pl;
            mixR[i] += finR[i] * g * pr;
        }
        v.amp = ampT; v.panL = plT; v.panR = prT;
        v.primed = true;

        // fim da voz (ou começo da nota que estava esperando o fade)
        if ((v.killing && v.killGain <= 0.0f) || v.env[0].stage == Env::idle)
        {
            if (v.killing && v.pending.valid)
            {
                const NoteData d = v.pending;
                startVoice (v, d, true);
                if (d.released) releaseOrHold (v);
            }
            else
            {
                v.active = v.gate = v.held = v.killing = false;
                v.pending.valid = false;
                v.noteID = -1;
            }
        }
    }

    void filterStage (Voice& v, int fi, const float* iL, const float* iR, float* oL, float* oR, int n,
                      float keyNote, float env2, float d01)
    {
        const Snapshot& S = *snap;
        const FilterSnap& F = S.filter[(size_t) fi];
        if (! F.on)
        {
            std::copy (iL, iL + n, oL);
            std::copy (iR, iR + n, oR);
            v.lastCut[fi] = F.cutoff;
            v.flt[(size_t) fi].primed = false;   // ao religar, as rampas partem do alvo
            return;
        }
        const auto& m = v.mod;
        const size_t cd = (size_t) (fi == 0 ? dstFACut : dstFBCut);
        // corte = knob × keytrack × env 2 (±7 oit.) × matriz (±7 oit.) × deriva (±1/6 oit.)
        const float lg = std::log2 (juce::jmax (20.0f, F.cutoff))
                       + F.key * 0.01f * (keyNote - 60.0f) / 12.0f
                       + F.env * 0.01f * env2 * 7.0f
                       + m[cd] * 7.0f
                       + d01 * v.drift[3] / 6.0f;
        const float fc  = juce::jlimit (20.0f, topHz, std::exp2 (juce::jlimit (4.0f, 15.0f, lg)));
        const float res = clamp01 (F.res * 0.01f + m[cd + 1]);
        const float drv = clamp01 (F.drive * 0.01f + m[cd + 2]);
        const float mix = clamp01 (F.mix * 0.01f);
        v.lastCut[fi] = fc;
        runFilter (v.flt[(size_t) fi], juce::jlimit (0, numFilterTypes - 1, F.type), iL, iR, oL, oR, n, fc, res, drv, mix);
    }

    // drive dos filtros não-ladder: tanh na entrada, entra suave a partir de 0
    static float sat (float x, float d)
    {
        if (d <= 0.0005f) return x;
        const float g = 1.0f + 9.0f * d;
        const float w = juce::jmin (1.0f, d * 10.0f);
        return x + w * (fastTanh (x * g) / std::sqrt (g) - x);
    }

    // corte/ressonância/drive/mix andam em rampa linear do passo anterior até o alvo (sem zipper)
    void runFilter (FilterState& f, int type, const float* iL, const float* iR, float* oL, float* oR, int n,
                    float fc, float res, float drv, float mix)
    {
        const float invN = 1.0f / (float) n;
        const float fsF = (float) fs;
        if (type != f.type) { f.type = type; f.primed = false; }

        if (type == 0 || type == 1)
        {
            // ---- Ladder 24/12: ZDF do F1 (tanh na realimentação, drive na entrada); auto-oscila no alto ----
            const bool four = type == 0;
            const float gT = std::tan (kPi * fc / fsF);
            const float kT = (four ? 4.1f : 6.0f) * res;
            if (! f.primed) { f.g = gT; f.k = kT; f.drive = drv; f.mix = mix; f.primed = true; }
            const float dg = (gT - f.g) * invN, dk = (kT - f.k) * invN, dm = (mix - f.mix) * invN;
            // drive até +24 dB, compensado depois (1/√ganho, como no F1)
            float dGain = std::exp (f.drive * 2.7631f);
            const float dGainT = std::exp (drv * 2.7631f);
            const float ddg = (dGainT - dGain) * invN;
            float g = f.g, k = f.k, mx = f.mix;
            const int stages = four ? 4 : 2;
            for (int i = 0; i < n; ++i)
            {
                g += dg; k += dk; mx += dm; dGain += ddg;
                const float inv = 1.0f / (1.0f + g);
                const float G = g * inv, G2 = G * G;
                const float mk = 1.0f + k * (four ? 0.5f : 0.6f);
                const float post = mk / std::sqrt (dGain);
                const float in2[2] = { iL[i], iR[i] };
                float y2[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    float* z = f.z[ch];
                    const float x = in2[ch] * dGain;
                    const float S1 = z[0] * inv, S2 = z[1] * inv;
                    float fb;
                    if (four)
                    {
                        const float S3 = z[2] * inv, S4 = z[3] * inv;
                        fb = (G2 * G2 * x + G2 * G * S1 + G2 * S2 + G * S3 + S4) / (1.0f + k * G2 * G2);
                    }
                    else
                        fb = (G2 * x + G * S1 + S2) / (1.0f + k * G2);
                    float u = fastTanh (x - k * fb);
                    for (int st = 0; st < stages; ++st)
                    {
                        const float vv = G * (u - z[st]);
                        const float y = vv + z[st];
                        z[st] = y + vv;
                        u = y;
                    }
                    y2[ch] = in2[ch] + mx * (u * post - in2[ch]);
                }
                oL[i] = y2[0];
                oR[i] = y2[1];
            }
            f.g = gT; f.k = kT; f.drive = drv; f.mix = mix;
            return;
        }

        if (type <= 5)
        {
            // ---- SVF TPT (Simper): PB / PA / PF / Rejeita ----
            const float gT = std::tan (kPi * fc / fsF);
            const float kT = kSqrt2 * (1.0f - 0.97f * res);   // Q 0,7 → ~24
            if (! f.primed) { f.g = gT; f.k = kT; f.drive = drv; f.mix = mix; f.primed = true; }
            float lpC = 0.0f, bpC = 0.0f, xC = 0.0f;   // y = lpC·v2 + bpC·k·v1 + xC·x
            if (type == 2)      lpC = 1.0f;
            else if (type == 3) { lpC = -1.0f; bpC = -1.0f; xC = 1.0f; }
            else if (type == 4) bpC = 1.0f;
            else                { bpC = -1.0f; xC = 1.0f; }
            const float dg = (gT - f.g) * invN, dk = (kT - f.k) * invN, dd = (drv - f.drive) * invN, dm = (mix - f.mix) * invN;
            float g = f.g, k = f.k, d = f.drive, mx = f.mix;
            for (int i = 0; i < n; ++i)
            {
                g += dg; k += dk; d += dd; mx += dm;
                const float a1 = 1.0f / (1.0f + g * (g + k));
                const float a2 = g * a1, a3 = g * a2;
                const float in2[2] = { iL[i], iR[i] };
                float y2[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = sat (in2[ch], d);
                    const float v3 = x - f.ic2[ch];
                    const float v1 = a1 * f.ic1[ch] + a2 * v3;
                    const float v2 = f.ic2[ch] + a2 * f.ic1[ch] + a3 * v3;
                    f.ic1[ch] = 2.0f * v1 - f.ic1[ch];
                    f.ic2[ch] = 2.0f * v2 - f.ic2[ch];
                    const float y = lpC * v2 + bpC * k * v1 + xC * x;
                    y2[ch] = in2[ch] + mx * (y - in2[ch]);
                }
                oL[i] = y2[0];
                oR[i] = y2[1];
            }
            f.g = gT; f.k = kT; f.drive = drv; f.mix = mix;
            return;
        }

        if (type == 6)
        {
            // ---- Pente: atraso de 1/corte com realimentação (picos nos múltiplos do corte) ----
            if (f.line[0] == nullptr || f.line[1] == nullptr)
            {
                std::copy (iL, iL + n, oL);
                std::copy (iR, iR + n, oR);
                return;
            }
            const float DT = juce::jlimit (2.0f, (float) (f.lineMask - 4), fsF / fc);
            const float fbT = 0.95f * res;
            if (! f.primed) { f.delay = DT; f.k = fbT; f.drive = drv; f.mix = mix; f.primed = true; }
            const float dD = (DT - f.delay) * invN, dk = (fbT - f.k) * invN, dd = (drv - f.drive) * invN, dm = (mix - f.mix) * invN;
            float D = f.delay, fb = f.k, d = f.drive, mx = f.mix;
            const float size = (float) (f.lineMask + 1);
            for (int i = 0; i < n; ++i)
            {
                D += dD; fb += dk; d += dd; mx += dm;
                const float norm = std::sqrt (juce::jmax (0.0f, 1.0f - fb * fb));   // ruído entra e sai no mesmo nível
                float rp = (float) f.w - D;
                if (rp < 0.0f) rp += size;
                const int ri = (int) rp;
                const float fr = rp - (float) ri;
                const float in2[2] = { iL[i], iR[i] };
                float y2[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    float* ln = f.line[ch];
                    const float s0 = ln[ri & f.lineMask], s1 = ln[(ri + 1) & f.lineMask];
                    f.damp[ch] += 0.8f * (s0 + fr * (s1 - s0) - f.damp[ch]);
                    const float y = sat (in2[ch], d) + fb * f.damp[ch];
                    ln[f.w] = y;
                    y2[ch] = in2[ch] + mx * (y * norm - in2[ch]);
                }
                f.w = (f.w + 1) & f.lineMask;
                oL[i] = y2[0];
                oR[i] = y2[1];
            }
            f.lineDirty = true;
            f.delay = DT; f.k = fbT; f.drive = drv; f.mix = mix;
            return;
        }

        // ---- Formante: 3 passa-faixas de vogal; o corte varre A → E → I → O → U (150 Hz .. 5 kHz) ----
        const float lgLo = 7.2288f, lgHi = 12.2877f;   // log2(150), log2(5000)
        const float vp = clamp01 ((std::log2 (fc) - lgLo) / (lgHi - lgLo)) * 4.0f;
        const int vi = juce::jmin (3, (int) vp);
        const float vf = vp - (float) vi;
        float gT[3];
        for (int bnd = 0; bnd < 3; ++bnd)
        {
            const float fq = filterVowels[vi][bnd] + vf * (filterVowels[vi + 1][bnd] - filterVowels[vi][bnd]);
            gT[bnd] = std::tan (kPi * juce::jmin (fq, topHz) / fsF);
        }
        const float kT = 1.0f / (3.0f + 17.0f * res);
        if (! f.primed)
        {
            for (int bnd = 0; bnd < 3; ++bnd) f.fg[bnd] = gT[bnd];
            f.k = kT; f.drive = drv; f.mix = mix; f.primed = true;
        }
        static const float bandGain[3] = { 2.2f, 1.4f, 0.8f };
        float dgb[3], gb[3];
        for (int bnd = 0; bnd < 3; ++bnd) { gb[bnd] = f.fg[bnd]; dgb[bnd] = (gT[bnd] - gb[bnd]) * invN; }
        const float dk = (kT - f.k) * invN, dd = (drv - f.drive) * invN, dm = (mix - f.mix) * invN;
        float k = f.k, d = f.drive, mx = f.mix;
        for (int i = 0; i < n; ++i)
        {
            k += dk; d += dd; mx += dm;
            float a1[3], a2[3], a3[3];
            for (int bnd = 0; bnd < 3; ++bnd)
            {
                gb[bnd] += dgb[bnd];
                a1[bnd] = 1.0f / (1.0f + gb[bnd] * (gb[bnd] + k));
                a2[bnd] = gb[bnd] * a1[bnd];
                a3[bnd] = gb[bnd] * a2[bnd];
            }
            const float in2[2] = { iL[i], iR[i] };
            float y2[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float x = sat (in2[ch], d);
                float y = 0.0f;
                for (int bnd = 0; bnd < 3; ++bnd)
                {
                    const float v3 = x - f.fz2[bnd][ch];
                    const float v1 = a1[bnd] * f.fz1[bnd][ch] + a2[bnd] * v3;
                    const float v2 = f.fz2[bnd][ch] + a2[bnd] * f.fz1[bnd][ch] + a3[bnd] * v3;
                    f.fz1[bnd][ch] = 2.0f * v1 - f.fz1[bnd][ch];
                    f.fz2[bnd][ch] = 2.0f * v2 - f.fz2[bnd][ch];
                    y += bandGain[bnd] * k * v1;
                }
                y2[ch] = in2[ch] + mx * (y - in2[ch]);
            }
            oL[i] = y2[0];
            oR[i] = y2[1];
        }
        for (int bnd = 0; bnd < 3; ++bnd) f.fg[bnd] = gT[bnd];
        f.k = kT; f.drive = drv; f.mix = mix;
    }

    // ---------------- telemetria (uma vez por bloco) ----------------
    void writeTelemetry (const Snapshot& S)
    {
        int count = 0;
        const Voice* newest = nullptr;
        for (size_t i = 0; i < voices.size(); ++i)
        {
            const auto& v = voices[i];
            auto& t = tel.voice[i];
            if (v.active)
            {
                ++count;
                t.note.store (v.note);
                t.pressure.store (v.pressure);
                t.slide.store (v.slide);
                t.bend.store (v.bendSemis);
                t.level.store (v.env[0].value);
                if (! v.killing && (newest == nullptr || v.age > newest->age)) newest = &v;
            }
            else
            {
                t.note.store (-1);
                t.level.store (0.0f);
                t.pressure.store (0.0f);
                t.slide.store (0.0f);
                t.bend.store (0.0f);
            }
        }
        tel.activeVoices.store (count);

        if (newest != nullptr)
        {
            for (int e = 0; e < numEnvs; ++e) tel.env[(size_t) e].store (newest->env[(size_t) e].value);
            for (int l = 0; l < numLfos; ++l) tel.lfo[(size_t) l].store (newest->lfo[(size_t) l].value);
            for (int f = 0; f < numFilters; ++f) tel.cutoff[(size_t) f].store (newest->lastCut[f]);
            for (int o = 0; o < numOsc; ++o)
            {
                tel.pos[(size_t) o].store (newest->lastPos[o]);
                tel.warp[(size_t) o].store (newest->lastWarp[o]);
            }
            for (int k = 0; k < numSlots; ++k)
            {
                const int s = S.slot[(size_t) k].src;
                tel.slotValue[(size_t) k].store (s > srcNone && s < numSrcs ? newest->src[(size_t) s] : 0.0f);
            }
        }
        else
        {
            for (auto& e : tel.env) e.store (0.0f);
            for (auto& l : tel.lfo) l.store (0.0f);
            for (int f = 0; f < numFilters; ++f) tel.cutoff[(size_t) f].store (S.filter[(size_t) f].cutoff);
            for (int o = 0; o < numOsc; ++o)
            {
                tel.pos[(size_t) o].store (clamp01 (S.osc[(size_t) o].pos * 0.01f));
                tel.warp[(size_t) o].store (clamp01 (S.osc[(size_t) o].warpAmt * 0.01f));
            }
            // sem voz: só as fontes globais têm valor (macros e mod wheel)
            for (int k = 0; k < numSlots; ++k)
            {
                const int s = S.slot[(size_t) k].src;
                float val = 0.0f;
                if (s >= srcMacro1 && s <= srcMacro4) val = clamp01 (S.macro[(size_t) (s - srcMacro1)] * 0.01f);
                else if (s == srcWheel)               val = wheel;
                tel.slotValue[(size_t) k].store (val);
            }
        }
    }

    // ---------------- estado ----------------
    Telemetry& tel;
    const WaveBank& bank;
    double fs = 44100.0;
    float  topHz = 19845.0f, killStep = 0.01f, nzCoef = 0.1f, nzDark = 0.0f, nzBright = 0.0f;

    juce::MPEInstrument mpe;
    bool cfgValid = false, cfgMpe = false;
    int  cfgBend = -1, cfgMpeBend = -1;
    bool muted = false, prevHold = false;

    const Snapshot* snap = nullptr;   // só vale dentro do process (os callbacks do MPE acontecem lá)
    std::array<Voice, maxVoices> voices;
    std::uint32_t ageCounter = 0;
    float lastPitch = 60.0f;
    bool  hasLast = false;
    float wheel = 0.0f;

    // bloco
    std::array<OscPrep, numOsc> prep;
    int   slotSrc[numSlots] {}, slotDst[numSlots] {};
    float slotAmt[numSlots] {};
    int   nSlots = 0;
    bool  usesA = false, usesB = false;
    int   subRoute = 0, noiseRoute = 0;
    double bpm = 120.0, ppq0 = 0.0, ppqPerSample = 0.0;
    bool  playing = false;
    std::array<double, numLfos> freePhase {};

    // rascunho do passo (uma voz por vez)
    float envBuf[kChunk] {};
    float rb[4][2][kChunk] {};   // rotas: 0 A, 1 B, 2 A+B, 3 direto
    float oscMono[numOsc][kChunk] {};
    float accL[kChunk] {}, accR[kChunk] {}, modTmp[kChunk] {}, zeros[kChunk] {};
    float inL[kChunk] {}, inR[kChunk] {}, aL[kChunk] {}, aR[kChunk] {}, bL[kChunk] {}, bR[kChunk] {};
    float finL[kChunk] {}, finR[kChunk] {}, mixL[kChunk] {}, mixR[kChunk] {};

    // linhas do Pente
    std::vector<float> combPool;
};

// =====================================================================================================
Engine::Engine() : impl (std::make_unique<Impl> (tel)) {}
Engine::~Engine() = default;

void Engine::prepare (double sampleRate, int maxBlockSize) { impl->prepare (sampleRate, maxBlockSize); }
void Engine::reset()                                        { impl->reset(); }

void Engine::process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Snapshot& s, const Clock& clock)
{
    impl->process (out, midi, s, clock);
}
} // namespace quimera
