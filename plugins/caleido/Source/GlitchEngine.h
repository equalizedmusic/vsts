#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace caleido
{
constexpr int numSteps    = 16;
constexpr int numLanes    = 5;
constexpr int colsPerStep = 24;
constexpr int peakCols    = numSteps * colsPerStep;

// trilhas da mandala, do anel de fora para o de dentro
enum Lane { laneRepeat = 0, laneReverse, laneTape, laneFilter, laneCrush };

// sorteio determinístico: mesma (semente, canal, passo) = mesmo resultado, então o padrão se repete igual a cada volta
inline float hash01 (uint32_t seed, uint32_t a, uint32_t b)
{
    uint32_t h = seed * 0x9E3779B1u;
    h ^= (a + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= (b + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 16; h *= 0x7FEB352Du;
    h ^= h >> 15; h *= 0x846CA68Bu;
    h ^= h >> 16;
    return (float) (h >> 8) * (1.0f / 16777216.0f);
}

inline uint32_t nextSeed (uint32_t s)
{
    s = juce::jlimit<uint32_t> (1u, 2147483646u, s);
    return (uint32_t) (((uint64_t) s * 16807u) % 2147483647u);
}

struct Settings
{
    double bpm = 145.0, ppq = 0.0;
    int    division = 1, ratchetMax = 3, spin = 2;
    float  chaos = 0.62f, trip = 0.48f, shuffle = 0.3f, reverse = 0.35f, tapeRange = 10.0f, filter = 0.55f, crush = 0.25f;
    bool   mutate = false, hold = false;
};

using Probs   = std::array<float, numLanes>;
using Pattern = std::array<int, numLanes>;   // um bit por passo

// CAOS decide cortes (repete/reverso); VIAGEM decide cor (fita/filtro/crush)
inline Probs laneProbs (const Settings& s)
{
    return { s.chaos * 0.55f, s.chaos * s.reverse * 0.9f, s.trip * 0.45f, s.trip * s.filter * 0.9f, s.trip * s.crush * 1.2f };
}

// o tempo forte (1 de cada 4 passos) tem menos chance de ser mexido: o kick continua no lugar
inline Pattern makePattern (uint32_t seed, const Probs& p)
{
    Pattern lanes {};
    for (int l = 0; l < numLanes; ++l)
        for (int s = 0; s < numSteps; ++s)
            if (hash01 (seed, (uint32_t) l, (uint32_t) s) < p[(size_t) l] * (s % 4 == 0 ? 0.55f : 1.1f))
                lanes[(size_t) l] |= 1 << s;
    return lanes;
}

class GlitchEngine
{
public:
    // ---- lido pela interface ----
    std::array<std::atomic<int>, numLanes> lanes {};
    std::atomic<uint32_t> seed { 417 };
    std::atomic<int>   step { 0 };
    std::atomic<float> stepFrac { 0.0f };
    std::atomic<bool>  holding { false };
    std::array<std::atomic<float>, peakCols> peaks {};

    // ---- pedidos da interface (atendidos no começo do próximo bloco) ----
    void requestNewPattern()       { wantNew.store (true); }
    void requestSeed (uint32_t s)  { wantSeed.store ((int64_t) s); }
    void toggle (int lane, int s)  { lanes[(size_t) lane].fetch_xor (1 << s); }

    void restore (const Pattern& l, uint32_t sd)
    {
        for (size_t i = 0; i < l.size(); ++i) lanes[i].store (l[i]);
        seed.store (sd);
        keepPattern.store (true);
    }

    Pattern pattern() const
    {
        Pattern p {};
        for (size_t i = 0; i < p.size(); ++i) p[i] = lanes[i].load();
        return p;
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const auto n = (size_t) juce::nextPowerOfTwo ((int) std::ceil (16.0 * sr));
        bufL.assign (n, 0.0f);
        bufR.assign (n, 0.0f);
        mask   = (int64_t) n - 1;
        fadeN    = juce::jmax (16.0, 0.002 * sr);
        xfadeN   = juce::jmax (32.0, 0.006 * sr);
        smooth   = (float) (1.0 - std::exp (-1.0 / (0.004 * sr)));
        fcSmooth = (float) (1.0 - std::exp (-16.0 / (0.004 * sr)));

        now = 0;
        lastStepGlobal = std::numeric_limits<int64_t>::min();
        lastCycle = 0;
        cyclesSinceMutate = 0;
        holdActive = false;
        cur = prev = Voice {};
        xfLeft = 0;
        crushMix = filtMix = spinDepth = 0.0f;
        crushTarget = filtTarget = spinTarget = 0.0f;
        heldL = heldR = 0.0f;
        dsCount = 0;
        ic1L = ic2L = ic1R = ic2R = 0.0f;
        typeW = { 1.0f, 0.0f, 0.0f };
        logFc = -1.0f;
        lastCol = -1;
        peakAcc = 0.0f;
        for (auto& p : peaks) p.store (0.0f);
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Settings& st)
    {
        handleRequests (st);

        const double stepBeats      = 0.5 / std::pow (2.0, st.division);   // 1/8, 1/16, 1/32
        const double beatsPerSample = st.bpm / 60.0 / sr;
        const double stepLen        = stepBeats / beatsPerSample;
        const double spinBeats      = 1.0 / std::pow (2.0, st.spin);       // 1/4 .. 1/32

        for (int i = 0; i < n; ++i)
        {
            const double ppq = st.ppq + (double) i * beatsPerSample;
            const double sp  = ppq / stepBeats;
            const auto   g   = (int64_t) std::floor (sp);

            // o PPQ do host pode voltar uma fração na virada do bloco: recuar um passo só é tremida, não pulo (senão o passo recomeça e estala)
            const bool jitter = lastStepGlobal != std::numeric_limits<int64_t>::min() && g == lastStepGlobal - 1;
            if (g != lastStepGlobal && ! jitter)
            {
                const int64_t cyc = g >= 0 ? g / numSteps : -((-g + numSteps - 1) / numSteps);
                const int     s   = (int) (g - cyc * numSteps);
                if (lastStepGlobal != std::numeric_limits<int64_t>::min() && cyc != lastCycle)
                    onNewCycle (st);
                lastCycle = cyc;
                lastStepGlobal = g;
                beginStep (s, now - (int64_t) std::llround ((sp - (double) g) * stepLen), stepLen, st);
            }

            if (! holdActive)
            {
                bufL[(size_t) (now & mask)] = inL[i];
                bufR[(size_t) (now & mask)] = inR[i];
            }

            // ---- fonte: ao vivo, repetida, invertida, afinada ou trocada de lugar ----
            float l, r;
            readVoice (cur, l, r);
            if (xfLeft > 0)
            {
                float pl, pr;
                readVoice (prev, pl, pr);
                const float k = (float) ((double) xfLeft / xfadeN);
                l = l * (1.0f - k) + pl * k;
                r = r * (1.0f - k) + pr * k;
                --xfLeft;
            }

            // ---- CRUSH: menos bits e menos amostras ----
            crushMix += (crushTarget - crushMix) * smooth;
            if (crushMix > 1.0e-4f)
            {
                if (++dsCount >= dsFactor)
                {
                    dsCount = 0;
                    heldL = std::round (l * crushLevels) / crushLevels;
                    heldR = std::round (r * crushLevels) / crushLevels;
                }
                l += (heldL - l) * crushMix;
                r += (heldR - r) * crushMix;
            }

            // ---- FILTRO: passa-baixa, banda ou alta, com varredura dentro do passo ----
            // corte e tipo (LP/BP/HP) deslizam em ~4 ms: dois passos de filtro seguidos não estalam na emenda
            filtMix += (filtTarget - filtMix) * smooth;
            for (int ty = 0; ty < 3; ++ty)
                typeW[(size_t) ty] += ((ty == filtType ? 1.0f : 0.0f) - typeW[(size_t) ty]) * smooth;
            if ((i & 15) == 0)
            {
                const double tf = juce::jlimit (0.0, 1.0, ((double) now - cur.start) / cur.len);
                const double fc = juce::jlimit (30.0, 0.45 * sr, (double) f0 * std::pow ((double) f1 / (double) f0, tf));
                const float  lf = (float) std::log (fc);
                logFc = (logFc < 0.0f || filtMix < 1.0e-3f) ? lf : logFc + (lf - logFc) * fcSmooth;
                const float  gg = (float) std::tan (juce::MathConstants<double>::pi * std::exp ((double) logFc) / sr);
                fk = 1.0f / filtQ;
                a1 = 1.0f / (1.0f + gg * (gg + fk));
                a2 = gg * a1;
                a3 = gg * a2;
            }
            const float fl = svf (l, ic1L, ic2L), fr = svf (r, ic1R, ic2R);
            l += (fl - l) * filtMix;
            r += (fr - r) * filtMix;

            // ---- GIRO: o corte colorido passeia pelo estéreo ----
            spinDepth += (spinTarget - spinDepth) * smooth;
            if (spinDepth > 1.0e-4f)
            {
                const float pan = (float) std::sin (juce::MathConstants<double>::twoPi * ppq / spinBeats) * spinDepth;
                const float th  = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                l *= juce::jmin (1.0f, juce::MathConstants<float>::sqrt2 * std::cos (th));
                r *= juce::jmin (1.0f, juce::MathConstants<float>::sqrt2 * std::sin (th));
            }

            l = softClip (l);
            r = softClip (r);
            outL[i] = l;
            outR[i] = r;

            // ---- desenho do padrão: pico por coluna do compasso ----
            const double frac = juce::jlimit (0.0, 0.999, ((double) now - cur.start) / cur.len);
            const int col = curStep * colsPerStep + (int) (frac * colsPerStep);
            if (col != lastCol)
            {
                if (lastCol >= 0) peaks[(size_t) lastCol].store (peakAcc, std::memory_order_relaxed);
                peakAcc = 0.0f;
                lastCol = col;
            }
            peakAcc = juce::jmax (peakAcc, std::abs (l), std::abs (r));

            ++now;
        }

        stepFrac.store ((float) juce::jlimit (0.0, 0.999, ((double) now - cur.start) / cur.len));
    }

private:
    struct Voice
    {
        double start = 0.0, len = 1.0, slice = 1.0, rate = 1.0, rateEnd = 1.0, src = 0.0;
        int    repeats = 1;
        bool   live = true, reverse = false, wraps = false;
    };

    void handleRequests (const Settings& st)
    {
        const auto probs = laneProbs (st);
        if (keepPattern.exchange (false)) lastProbs = probs;

        const auto ws = wantSeed.exchange (-1);
        if (ws >= 0) { seed.store ((uint32_t) ws); regenerate (probs); }
        if (wantNew.exchange (false)) { seed.store (nextSeed (seed.load())); regenerate (probs); }

        // mexeu em CAOS/VIAGEM/REVERSO/FILTRO/CRUSH: refaz com a mesma semente (os passos acendem ou apagam, sem embaralhar tudo)
        for (size_t l = 0; l < probs.size(); ++l)
            if (std::abs (probs[l] - lastProbs[l]) > 0.03f) { regenerate (probs); break; }
    }

    void regenerate (const Probs& probs)
    {
        const auto p = makePattern (seed.load(), probs);
        for (size_t i = 0; i < p.size(); ++i) lanes[i].store (p[i]);
        lastProbs = probs;
    }

    // MUTAR: padrão novo a cada 2 voltas da mandala (SEGURAR trava)
    void onNewCycle (const Settings& st)
    {
        if (! st.mutate || holdActive) { cyclesSinceMutate = 0; return; }
        if (++cyclesSinceMutate >= 2)
        {
            cyclesSinceMutate = 0;
            seed.store (nextSeed (seed.load()));
            regenerate (laneProbs (st));
        }
    }

    void beginStep (int s, int64_t startAbs, double stepLen, const Settings& st)
    {
        // SEGURAR entra e sai na virada do passo: prende os 16 passos anteriores
        if (st.hold && ! holdActive)      { holdActive = true; holdEnd = startAbs; holdStep = stepLen; }
        else if (! st.hold && holdActive) { holdActive = false; }
        holding.store (holdActive);

        prev   = cur;
        xfLeft = (int) xfadeN;
        curStep = s;
        step.store (s);

        const uint32_t sd = seed.load();
        const auto     us = (uint32_t) s;
        bool on[numLanes];
        bool any = false;
        for (int l = 0; l < numLanes; ++l)
        {
            on[l] = ((lanes[(size_t) l].load() >> s) & 1) != 0;
            any = any || on[l];
        }

        Voice v;
        v.start = (double) startAbs;
        v.len   = stepLen;

        // EMBARALHAR: toca um passo de antes no lugar deste
        int k = 0;
        if (hash01 (sd, 20, us) < st.chaos * st.shuffle)
            k = 1 + juce::jmin (6, (int) (hash01 (sd, 21, us) * 7.0f));

        // REPETE: picota o passo em 2..16 pedaços (nunca menores que 6 ms)
        int rep = 1;
        if (on[laneRepeat])
        {
            static constexpr int opts[] = { 2, 3, 4, 6, 8, 12, 16 };
            const int top = juce::jlimit (0, 6, st.ratchetMax);
            rep = opts[juce::jmin (top, (int) (hash01 (sd, 22, us) * (float) (top + 1)))];
            while (rep > 1 && stepLen / rep < 0.006 * sr) rep /= 2;
            rep = juce::jmax (1, rep);
        }
        v.repeats = rep;
        v.slice   = stepLen / rep;

        // FITA: transpõe o corte ou freia a fita até quase parar
        if (on[laneTape])
        {
            if (hash01 (sd, 23, us) < 0.3f)
                v.rateEnd = 0.15;
            else
            {
                float semis = std::round ((hash01 (sd, 24, us) * 2.0f - 1.0f) * st.tapeRange);
                if (semis == 0.0f) semis = -juce::jmax (1.0f, std::round (st.tapeRange));
                v.rate = v.rateEnd = std::pow (2.0, semis / 12.0);
            }
        }
        v.reverse = on[laneReverse];
        v.wraps   = juce::jmax (v.rate, v.rateEnd) > 1.0001;

        // de onde ler: o reverso e a fita acelerada precisam de áudio que já passou
        if (holdActive)
        {
            v.live = false;
            v.src  = (double) holdEnd - numSteps * holdStep + (double) ((s + k) % numSteps) * holdStep;
        }
        else if (k > 0)                  { v.live = false; v.src = (double) startAbs - k * stepLen; }
        else if (v.reverse || v.wraps)   { v.live = false; v.src = (double) startAbs - v.slice; }
        else if (rep > 1 || v.rateEnd < 0.9999) { v.live = false; v.src = (double) startAbs; }
        else                              v.live = true;
        cur = v;

        // cores do passo
        crushTarget = on[laneCrush] ? 1.0f : 0.0f;
        crushLevels = std::pow (2.0f, (float) (crushBits (st.crush * 100.0f) - 1));
        dsFactor    = 1 + (int) (hash01 (sd, 26, us) * st.crush * 24.0f * (float) (sr / 48000.0));

        filtTarget = on[laneFilter] ? 1.0f : 0.0f;
        if (on[laneFilter])
        {
            filtType = juce::jmin (2, (int) (hash01 (sd, 27, us) * 3.0f));
            f0 = 150.0f * std::pow (60.0f, hash01 (sd, 28, us));
            f1 = juce::jlimit (40.0f, 16000.0f, f0 * std::pow (2.0f, (hash01 (sd, 29, us) * 2.0f - 1.0f) * 6.0f * st.filter));
            filtQ = 0.7f + 2.8f * st.filter;
        }

        spinTarget = (any || k > 0) ? st.trip : 0.0f;
    }

    static int crushBits (float pct) { return 16 - juce::roundToInt (pct * 0.14f); }

    float readAt (const std::vector<float>& b, double pos) const
    {
        const double fl = std::floor (pos);
        const auto   i0 = (int64_t) fl;
        const float  f  = (float) (pos - fl);
        const float  a  = b[(size_t) (i0 & mask)];
        const float  c  = b[(size_t) ((i0 + 1) & mask)];
        return a + (c - a) * f;
    }

    void readVoice (const Voice& v, float& l, float& r) const
    {
        if (v.live)
        {
            l = bufL[(size_t) (now & mask)];
            r = bufR[(size_t) (now & mask)];
            return;
        }

        // depois do fim do passo (durante o crossfade do próximo) a última fatia segue em frente:
        // voltar para o começo dela aqui era o estalo em cada passo
        const double t     = juce::jmax (0.0, (double) now - v.start);
        const int    idx   = (int) juce::jmin ((double) (v.repeats - 1), std::floor (t / v.slice));
        const double local = t - (double) idx * v.slice;
        const double ramp  = (v.rateEnd - v.rate) / v.len;
        const double lr    = juce::jmin (local, v.len);
        double off = lr * v.rate + 0.5 * ramp * lr * lr + (local - lr) * v.rateEnd;
        if (v.wraps) off = std::fmod (off, v.slice);
        const double offW = off;
        if (v.reverse) off = v.slice - 1.0 - off;

        // emenda sem clique entre as repetições (a virada do passo tem o crossfade)
        double w = 1.0;
        if (v.repeats > 1 || v.wraps)
        {
            const double rf = juce::jmax (8.0, juce::jmin (fadeN, v.slice * 0.25));
            if (idx > 0)                 w = juce::jmin (w, local / rf);
            if (idx < v.repeats - 1)     w = juce::jmin (w, (v.slice - local) / rf);
            if (v.wraps && t >= rf)      w = juce::jmin (w, juce::jmin (offW, v.slice - offW) / rf);
            w = juce::jlimit (0.0, 1.0, w);
        }

        const double pos = v.src + off;
        l = readAt (bufL, pos) * (float) w;
        r = readAt (bufR, pos) * (float) w;
    }

    float svf (float x, float& ic1, float& ic2) const
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        const float comp = 1.0f / (0.6f + 0.4f * filtQ);                    // segura a ressonância
        return (v2 * typeW[0] + (x - fk * v1 - v2) * typeW[2]) * comp
             + fk * v1 * typeW[1];                                          // banda (pico em 0 dB)
    }

    static float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    }

    double sr = 44100.0, fadeN = 96.0, xfadeN = 288.0;
    float  smooth = 0.005f, fcSmooth = 0.1f;
    std::vector<float> bufL, bufR;
    int64_t mask = 0, now = 0;

    int64_t lastStepGlobal = std::numeric_limits<int64_t>::min(), lastCycle = 0;
    int     cyclesSinceMutate = 0, curStep = 0;
    bool    holdActive = false;
    int64_t holdEnd = 0;
    double  holdStep = 1.0;

    Voice cur, prev;
    int   xfLeft = 0;

    float crushMix = 0, crushTarget = 0, crushLevels = 2048.0f, heldL = 0, heldR = 0;
    int   dsFactor = 1, dsCount = 0;

    float filtMix = 0, filtTarget = 0, f0 = 1000.0f, f1 = 1000.0f, filtQ = 0.7f;
    int   filtType = 0;
    float fk = 1.4f, a1 = 0, a2 = 0, a3 = 0;
    float ic1L = 0, ic2L = 0, ic1R = 0, ic2R = 0;
    std::array<float, 3> typeW { 1.0f, 0.0f, 0.0f };
    float logFc = -1.0f;

    float spinDepth = 0, spinTarget = 0;

    int   lastCol = -1;
    float peakAcc = 0.0f;

    Probs lastProbs {};
    std::atomic<bool>    wantNew { false }, keepPattern { false };
    std::atomic<int64_t> wantSeed { -1 };
};
} // namespace caleido
