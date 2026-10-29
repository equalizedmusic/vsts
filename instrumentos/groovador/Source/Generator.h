#pragma once

#include <cmath>
#include "Params.h"

// GROOVADOR — gerador de groove. Cada ESTILO tem um peso (0 a 9) por semicolcheia de um compasso 4/4 para cada
// instrumento: 9 = sempre toca, 0 = nunca. A DENSIDADE abre ou fecha o que tem peso médio; GHOST põe batidas fracas
// nos vãos da caixa, do rim, das percussões e do hat; ROLL põe 2 ou 3 batidas dentro do passo. O compasso 1 sai do
// sorteio; os seguintes repetem o 1 com algumas trocas (o groove tem cara, mas respira).
namespace grv
{
inline const char* const styleWeights[6][numInst] = {
    // RIM                 SNARE               CLAP                HAT FECH.           HAT ABERTO          SHAKER              PERC 1              PERC 2
    { "0003000200030004", "0000600000006003", "0000900000009000", "5350535053505350", "0090009000900090", "3333333333333333", "0004003000040030", "0000000400000302" },   // PSY FULL
    { "0002000000020003", "0000500000005000", "0000900000009000", "3040304030403040", "0090009000900090", "4242424242424242", "0003000020003000", "0000020000000400" },   // PSY PROG
    { "0004000300004000", "0000000000003000", "0000900000009000", "5353035353530353", "0090009000900090", "2020202020202020", "0000004000000030", "0300000003000000" },   // TECHNO
    { "0002000300020004", "0000300000003000", "0000900000009000", "4343034343430343", "0090009000900090", "5353535353535353", "0040003000400304", "0004000040000400" },   // HOUSE
    { "5003004005003004", "0000300000004000", "0000600000006000", "0303030303030303", "0060000000600000", "5454545454545454", "7050605070506050", "0605070606050706" },   // TRIBAL
    { "0000000300000002", "0000900202009002", "0000600000006000", "6060606060606060", "0000000000000090", "2222222222222222", "0000000000300000", "0003000000000300" },   // BREAK
};

// peso do passo i (índice dentro do compasso) para o instrumento; tercinas pegam a média dos vizinhos, mais fraca
inline float stepWeight (int style, int inst, int i, int rateIdx)
{
    const char* w = styleWeights[juce::jlimit (0, 5, style)][juce::jlimit (0, numInst - 1, inst)];
    const double q = (double) i * rateBeats[juce::jlimit (0, 2, rateIdx)] * 4.0;       // em semicolcheias
    const int q0 = (int) std::floor (q + 1e-6);
    const double fr = q - (double) q0;
    const float a = (float) (w[q0 % 16] - '0');
    if (fr < 1e-4) return a;
    const float b = (float) (w[(q0 + 1) % 16] - '0');
    return (a + b) * 0.25f;
}

inline bool canGhost (int inst) { return inst == snare || inst == rim || inst == perc1 || inst == perc2 || inst == hatC; }
inline bool canRoll (int inst)  { return inst != hatO && inst != clap; }

struct GenParams
{
    int style = 0, meter = 0, rate = 0;
    float density = 0.6f, ghost = 0.35f, roll = 0.2f;
};

// ritmo: liga/desliga os passos (e ghost, roll e "talvez")
inline void generateRhythm (Pattern& pat, const GenParams& gp, juce::Random& rnd)
{
    const int spb = stepsPerBar (gp.meter, gp.rate);
    for (int k = 0; k < numInst; ++k)
    {
        for (int i = 0; i < spb && i < maxSteps; ++i)
        {
            const float w = stepWeight (gp.style, k, i, gp.rate);
            Step s;
            const float p = w >= 9.0f ? 1.0f : juce::jlimit (0.0f, 1.0f, w / 9.0f * (0.3f + gp.density * 1.4f));
            if (rnd.nextFloat() < p)
            {
                s.on = true;
                s.vel = w >= 7.0f ? 6 : w >= 4.0f ? 5 : 4;
                if (canRoll (k) && rnd.nextFloat() < gp.roll * 0.12f) s.roll = rnd.nextInt (3) == 0 ? 2 : 1;
                if (w < 5.0f && rnd.nextFloat() < 0.15f) s.prob = 1 + rnd.nextInt (2);
            }
            else if (canGhost (k) && rnd.nextFloat() < gp.ghost * (k == snare ? 0.3f : 0.14f))
            {
                s.on = true;
                s.vel = 1 + rnd.nextInt (2);
            }
            pat.set (k, i, s);
        }
        // compassos seguintes: o 1 com algumas trocas
        for (int i = spb; i < maxSteps; ++i)
        {
            const int i0 = i % spb;
            Step s = pat.get (k, i0);
            const float w = stepWeight (gp.style, k, i0, gp.rate);
            if (w < 9.0f && rnd.nextFloat() < 0.1f)
            {
                s.on = ! s.on && rnd.nextFloat() < juce::jmax (0.2f, w / 9.0f);
                s.vel = s.on ? (w >= 4.0f ? 5 : 3) : s.vel;
                s.roll = 0;
            }
            pat.set (k, i, s);
        }
    }
}

// força de cada batida: forte no tempo, média no contratempo, o resto mais baixo; ghost continua ghost
inline void generateVelocity (Pattern& pat, const GenParams& gp, juce::Random& rnd)
{
    const int spb = stepsPerBar (gp.meter, gp.rate);
    for (int k = 0; k < numInst; ++k)
        for (int i = 0; i < maxSteps; ++i)
        {
            Step s = pat.get (k, i);
            if (! s.on) continue;
            if (s.ghost()) s.vel = 1 + rnd.nextInt (2);
            else
            {
                const float w = stepWeight (gp.style, k, i % spb, gp.rate);
                const int base = w >= 7.0f ? 6 : w >= 4.0f ? 5 : 4;
                s.vel = juce::jlimit (3, 7, base + rnd.nextInt (3) - 1);
            }
            pat.set (k, i, s);
        }
}

// fill: ocupa a 2ª metade do último compasso do ciclo (FILL A CADA). Instrumento com batida no fill troca o groove
// por ele nesse trecho; instrumento sem batida no fill segue o groove.
inline void generateFills (Pattern& pat, const GenParams& gp, juce::Random& rnd)
{
    const int half = juce::jmin (fillSteps, stepsPerBar (gp.meter, gp.rate) / 2);
    for (int k = 0; k < numInst; ++k)
        for (int j = 0; j < fillSteps; ++j)
            pat.setFill (k, j, Step());

    auto put = [&pat] (int inst, int j, int vel, int roll = 0)
    {
        Step s;
        s.on = true;
        s.vel = juce::jlimit (1, 7, vel);
        s.roll = roll;
        pat.setFill (inst, j, s);
    };
    auto ramp = [half] (int j) { return 2 + (int) std::round (5.0 * (double) j / (double) juce::jmax (1, half - 1)); };

    switch (rnd.nextInt (4))
    {
        case 0:     // caixa subindo
            for (int j = 0; j < half; ++j) put (snare, j, ramp (j), j == half - 1 ? 2 : 0);
            break;
        case 1:     // as percussões descendo, clap no fim
            for (int j = 0; j < half; ++j) put (j % 2 == 0 ? perc1 : perc2, j, ramp (j));
            put (clap, half - 1, 7);
            break;
        case 2:     // clap e rim conversando
            for (int j = 0; j < half; ++j) put (j % 2 == 0 ? clap : rim, j, ramp (j), j == half - 1 ? 1 : 0);
            break;
        default:    // hat aberto e roll de caixa no fim
            put (hatO, 0, 6);
            for (int j = half - 3; j < half; ++j)
                if (j > 0) put (snare, j, ramp (j), 1);
            put (perc1, half / 2, 5);
            break;
    }
    if (rnd.nextFloat() < gp.roll * 0.6f) put (hatC, half - 1, 5, 2);
}

// ---------------- variação de timbre ----------------
// o timbre B de cada instrumento: desvios sorteados pela semente (−1..1), multiplicados pelo QUANTO
struct VarDelta { float tune = 0, dec = 0, tone = 0, snap = 0, body = 0, pEnv = 0; };

inline VarDelta varDelta (int seed, int inst)
{
    juce::Random r ((juce::int64) seed * 7919 + inst * 104729 + 17);
    auto s = [&r] { return r.nextFloat() * 2.0f - 1.0f; };
    VarDelta d;
    d.tune = s(); d.dec = s(); d.tone = s(); d.snap = s(); d.body = s(); d.pEnv = s();
    // garante que B soa diferente: a altura ou o tom anda pelo menos um pouco
    if (std::abs (d.tune) < 0.35f) d.tune = d.tune < 0.0f ? -0.5f : 0.5f;
    return d;
}

// quanto do B entra no instrumento no trecho seg (0 = timbre A); whoIndex = posição do instrumento entre os que variam
inline float varFactor (VMode mode, long long seg, int whoIndex, int seed)
{
    switch (mode)
    {
        case VMode::ab:   return (seg & 1) != 0 ? 1.0f : 0.0f;
        case VMode::talk: return ((seg + whoIndex) & 1) != 0 ? 1.0f : 0.0f;     // metade em A, metade em B; trocam
        case VMode::drift:
        {
            const double x = (double) seg;
            const double s = (double) (seed % 97);
            return (float) (0.7 * std::sin (x * 0.9 + s) + 0.3 * std::sin (x * 0.37 + s * 1.3));
        }
    }
    return 0.0f;
}
} // namespace grv
