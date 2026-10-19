#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace console
{
constexpr int historyCols = 480;   // colunas da faixa do alto; cada uma ~8 ms ⇒ ~4 s na tela
constexpr int numModels   = 3;     // 0 = CRISTAL, 1 = FERRO, 2 = BRASA

// ---------------------------------------------------------------------------------------------
// Cada modelo de mesa: curva de saturação (dureza k e polarização b: b > 0 = harmônicos pares),
// cor fixa de EQ (prateleira de grave, inclinação, queda do agudo) e o caráter do ruído de fundo.
// ---------------------------------------------------------------------------------------------
struct ModelSpec
{
    float k;          // dureza da curva (mais = satura mais cedo)
    float bias;       // polarização: puxa a curva para um lado ⇒ 2º/4º harmônicos
    float lowDb;      // prateleira de grave (dB)
    float lowHz;      // onde a prateleira começa
    float tiltDb;     // inclinação própria do modelo (dB; + = mais brilho)
    float topHz;      // queda do agudo (passa-baixa de 1ª ordem)
    float noiseHz;    // cor do ruído (passa-baixa de 1ª ordem)
    float hum;        // zumbido de 60 Hz, relativo ao ruído
};

inline const ModelSpec& modelSpec (int model)
{
    static const ModelSpec specs[numModels] = {
        { 0.60f, 0.03f, 0.0f,  90.0f,  0.6f, 40000.0f, 14000.0f, 0.00f },   // CRISTAL: limpo, folga, brilho leve
        { 0.80f, 0.12f, 1.2f, 110.0f,  0.0f, 18000.0f,  8000.0f, 0.15f },   // FERRO: transformador, grave encorpado
        { 1.00f, 0.25f, 0.6f, 140.0f, -0.8f, 13000.0f,  5000.0f, 0.35f },   // BRASA: válvula, pares, topo macio
    };
    return specs[juce::jlimit (0, numModels - 1, model)];
}

// curva do canal: ganho 1 em sinal baixo, sem DC em silêncio
struct Shaper
{
    float k = 1.0f, b = 0.0f, tb = 0.0f, norm = 1.0f;

    void set (const ModelSpec& m)
    {
        k    = m.k;
        b    = m.bias;
        tb   = std::tanh (b);
        norm = 1.0f / (k * (1.0f - tb * tb));
    }

    float operator() (float x) const { return (std::tanh (k * x + b) - tb) * norm; }
};

// DRIVE 0–1 ⇒ 0 a +24 dB; RUÍDO 0–1 ⇒ −96 a −60 dBFS; CROSSTALK 0–1 ⇒ até −16,5 dB de vazamento
inline float driveDb (float drive01)       { return 24.0f * drive01; }
inline float noiseDbfs (float noise01)     { return -96.0f + 36.0f * noise01; }
inline float crosstalkGain (float xt01)    { return 0.15f * xt01; }

constexpr float tiltPivotHz = 700.0f;
constexpr float xtLowHz = 150.0f, xtHighHz = 7000.0f;   // faixa do vazamento entre canais

struct Settings
{
    float drive = 0.3f;       // 0–1
    int   model = 1;
    float tone = 0.0f;        // dB (inclinação do usuário)
    float crosstalk = 0.1f;   // 0–1
    float noise = 0.0f;       // 0–1 (0 = desligado)
    bool  autoGain = true;
    bool  os4 = true;         // 4× (senão 2×)
};

// filtro de 1 polo (passa-baixa); o passa-alta sai como x − lp(x)
struct OnePole
{
    float a = 1.0f, z = 0.0f;

    void setCutoff (double hz, double fs)
    {
        if (hz >= 0.45 * fs) { a = 1.0f; return; }
        a = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * hz / fs));
    }

    float lp (float x) { z += a * (x - z); return z; }
    void reset() { z = 0.0f; }
};

// ---------------------------------------------------------------------------------------------
// DRIVE (ganho) → oversampling 2× ou 4× (FIR de fase linear) → curva do modelo → volta à taxa do projeto
// → tira DC → EQ do modelo + TOM → CROSSTALK → compensação automática → ruído de fundo.
// A latência informada é sempre a do 4×; no 2× o sinal ganha um atraso extra igual à diferença,
// então trocar a qualidade não muda a latência para o host.
// ---------------------------------------------------------------------------------------------
class ConsoleEngine
{
public:
    // ---- telemetria para a interface ----
    std::array<std::atomic<float>, historyCols> colIn {}, colErr {};   // pico do sinal no canal e da cor gerada
    std::atomic<int>   colWrite { 0 };
    std::atomic<float> drivenPeak { 0.0f };   // pico do sinal que entra na curva (a interface lê e zera)
    std::atomic<float> compDb { 0.0f };       // compensação automática atual (dB)

    int latency() const { return lat4; }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        blockMax = juce::jmax (1, maxBlock);

        os2.initProcessing ((size_t) blockMax);
        os4.initProcessing ((size_t) blockMax);
        os2.reset();
        os4.reset();
        lat2 = (int) std::lround (os2.getLatencyInSamples());
        lat4 = (int) std::lround (os4.getLatencyInSamples());

        work.setSize (2, blockMax);
        errPk.assign ((size_t) blockMax, 0.0f);
        drvAbs.assign ((size_t) blockMax, 0.0f);

        int dl = 1;
        while (dl < lat4 + 8) dl <<= 1;
        ringMask = dl - 1;
        for (auto& r : dryRing) r.assign ((size_t) dl, 0.0f);
        for (auto& r : wetRing) r.assign ((size_t) dl, 0.0f);
        ringPos = 0;

        for (auto& c : chans)
        {
            c.dc.setCutoff (8.0, fs);
            c.tilt.setCutoff ((double) tiltPivotHz, fs);
            c.xtHp.setCutoff ((double) xtLowHz, fs);
            c.xtLp.setCutoff ((double) xtHighHz, fs);
            c.dc.reset(); c.low.reset(); c.tilt.reset(); c.top.reset();
            c.xtHp.reset(); c.xtLp.reset(); c.noiseLp.reset();
        }
        curModel = -1;
        cur4 = true;
        humPhase = 0.0;
        humInc = 60.0 / fs;

        msIn = msWet = 0.0f;
        msCoef = (float) (1.0 - std::exp (-1.0 / (0.5 * fs)));   // ~0,5 s
        compTarget = 1.0f;

        colLen = juce::jmax (1, (int) std::lround (fs * 0.008));
        colCount = 0;
        accIn = accErr = 0.0f;
        for (auto& c : colIn)  c.store (0.0f);
        for (auto& c : colErr) c.store (0.0f);
        colWrite.store (0);

        smDrive.reset (fs, 0.03);
        smTone.reset (fs, 0.05);
        smXt.reset (fs, 0.05);
        smNoise.reset (fs, 0.05);
        smComp.reset (fs, 0.2);
        primed = false;
    }

    // out = canal processado; dry = entrada atrasada pela latência, para MIX, COMPARAR e bypass
    void process (const float* inL, const float* inR, float* outL, float* outR, float* dryL, float* dryR, int n, const Settings& s)
    {
        for (int done = 0; done < n;)
        {
            const int m = juce::jmin (blockMax, n - done);
            processChunk (inL + done, inR + done, outL + done, outR + done, dryL + done, dryR + done, m, s);
            done += m;
        }
    }

private:
    struct Channel
    {
        OnePole dc, low, tilt, top, xtHp, xtLp, noiseLp;
        juce::Random rng;
    };

    void applyModel (int model)
    {
        curModel = model;
        const auto& m = modelSpec (model);
        shaper.set (m);
        lowGainMinus1 = juce::Decibels::decibelsToGain (m.lowDb) - 1.0f;
        topOn = (double) m.topHz < 0.45 * fs;
        for (auto& c : chans)
        {
            c.low.setCutoff ((double) m.lowHz, fs);
            c.top.setCutoff ((double) m.topHz, fs);
            c.noiseLp.setCutoff ((double) m.noiseHz, fs);
        }
        // ruído uniforme com RMS = 1 depois do passa-baixa (banda equivalente de 1 polo = π/2 · fc)
        const double bw = juce::jmin (0.5 * fs, juce::MathConstants<double>::halfPi * (double) m.noiseHz);
        noiseNorm = (float) (std::sqrt (3.0) * std::sqrt (0.5 * fs / juce::jmax (1.0, bw)));
        humAmt = m.hum;
        modelTilt = m.tiltDb;
    }

    void processChunk (const float* inL, const float* inR, float* outL, float* outR, float* dryL, float* dryR, int m, const Settings& s)
    {
        const int model = juce::jlimit (0, numModels - 1, s.model);
        if (model != curModel) applyModel (model);

        if (s.os4 != cur4)
        {
            cur4 = s.os4;
            (cur4 ? os4 : os2).reset();
        }
        auto& os = cur4 ? os4 : os2;
        const size_t factor = cur4 ? (size_t) 4 : (size_t) 2;
        const int extra = cur4 ? 0 : juce::jmax (0, lat4 - lat2);   // 2×: completa até a latência do 4×

        const float driveG = juce::Decibels::decibelsToGain (driveDb (s.drive));
        const float toneT  = s.tone + modelTilt;
        const float xtG    = crosstalkGain (s.crosstalk);
        const float noiseG = s.noise > 0.001f ? juce::Decibels::decibelsToGain (noiseDbfs (s.noise)) : 0.0f;
        if (! primed)
        {
            smDrive.setCurrentAndTargetValue (driveG);
            smTone.setCurrentAndTargetValue (toneT);
            smXt.setCurrentAndTargetValue (xtG);
            smNoise.setCurrentAndTargetValue (noiseG);
            smComp.setCurrentAndTargetValue (1.0f);
            primed = true;
        }
        smDrive.setTargetValue (driveG);
        smTone.setTargetValue (toneT);
        smXt.setTargetValue (xtG);
        smNoise.setTargetValue (noiseG);

        // ---- DRIVE ----
        float* w0 = work.getWritePointer (0);
        float* w1 = work.getWritePointer (1);
        float pkDrv = 0.0f;
        for (int i = 0; i < m; ++i)
        {
            const float g = smDrive.getNextValue();
            w0[i] = inL[i] * g;
            w1[i] = inR[i] * g;
            const float a = juce::jmax (std::abs (w0[i]), std::abs (w1[i]));
            drvAbs[(size_t) i] = a;
            pkDrv = juce::jmax (pkDrv, a);
            errPk[(size_t) i] = 0.0f;
            msIn += msCoef * (0.5f * (inL[i] * inL[i] + inR[i] * inR[i]) - msIn);
        }

        // ---- curva do modelo com oversampling ----
        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) m);
        auto up = os.processSamplesUp (block);
        float* u0 = up.getChannelPointer (0);
        float* u1 = up.getChannelPointer (1);
        const size_t un = up.getNumSamples();
        const size_t lastBase = (size_t) (m - 1);
        for (size_t j = 0; j < un; ++j)
        {
            const float x0 = u0[j], x1 = u1[j];
            const float y0 = shaper (x0), y1 = shaper (x1);
            u0[j] = y0;
            u1[j] = y1;
            const size_t bi = juce::jmin (lastBase, j / factor);
            errPk[bi] = juce::jmax (errPk[bi], std::abs (y0 - x0), std::abs (y1 - x1));   // a cor que o canal acrescentou
        }
        os.processSamplesDown (block);

        // ---- EQ do modelo, TOM, CROSSTALK, compensação, ruído ----
        const float* wl = work.getReadPointer (0);
        const float* wr = work.getReadPointer (1);
        auto& cl = chans[0];
        auto& cr = chans[1];
        for (int i = 0; i < m; ++i)
        {
            const auto wp = (size_t) ringPos;
            wetRing[0][wp] = wl[i];
            wetRing[1][wp] = wr[i];
            dryRing[0][wp] = inL[i];
            dryRing[1][wp] = inR[i];
            const auto we = (size_t) ((ringPos - extra) & ringMask);
            const auto dp = (size_t) ((ringPos - lat4) & ringMask);
            ringPos = (ringPos + 1) & ringMask;

            float l = wetRing[0][we];
            float r = wetRing[1][we];

            // sem DC (a curva polarizada gera um pouco)
            l -= cl.dc.lp (l);
            r -= cr.dc.lp (r);

            // prateleira de grave do modelo
            l += cl.low.lp (l) * lowGainMinus1;
            r += cr.low.lp (r) * lowGainMinus1;

            // inclinação (modelo + TOM): grave e agudo vão em sentidos opostos em volta de 700 Hz
            const float t  = smTone.getNextValue();
            const float gH = std::pow (10.0f, t / 40.0f);
            const float gL = 1.0f / gH;
            const float lo0 = cl.tilt.lp (l), lo1 = cr.tilt.lp (r);
            l = lo0 * gL + (l - lo0) * gH;
            r = lo1 * gL + (r - lo1) * gH;

            if (topOn)
            {
                l = cl.top.lp (l);
                r = cr.top.lp (r);
            }

            // CROSSTALK: um pouco de cada canal vaza no outro, só na faixa do meio
            const float xt = smXt.getNextValue();
            const float leakL = cl.xtLp.lp (l - cl.xtHp.lp (l));
            const float leakR = cr.xtLp.lp (r - cr.xtHp.lp (r));
            l += xt * leakR;
            r += xt * leakL;

            msWet += msCoef * (0.5f * (l * l + r * r) - msWet);
            const float cg = smComp.getNextValue();
            l *= cg;
            r *= cg;

            // ruído de fundo do modelo (chiado + zumbido de 60 Hz), depois da compensação
            const float ng = smNoise.getNextValue();
            const float n0 = (cl.rng.nextFloat() * 2.0f - 1.0f) * noiseNorm;
            const float n1 = (cr.rng.nextFloat() * 2.0f - 1.0f) * noiseNorm;
            humPhase += humInc;
            if (humPhase >= 1.0) humPhase -= 1.0;
            const float ph  = (float) humPhase * juce::MathConstants<float>::twoPi;
            const float hum = (std::sin (ph) + 0.4f * std::sin (3.0f * ph)) * humAmt;
            l += (cl.noiseLp.lp (n0) + hum) * ng;
            r += (cr.noiseLp.lp (n1) + hum) * ng;

            outL[i] = l;
            outR[i] = r;
            dryL[i] = dryRing[0][dp];
            dryR[i] = dryRing[1][dp];

            accIn  = juce::jmax (accIn, drvAbs[(size_t) i]);
            accErr = juce::jmax (accErr, errPk[(size_t) i]);
            if (++colCount >= colLen)
            {
                const int c = colWrite.load (std::memory_order_relaxed);
                colIn[(size_t) c].store (accIn, std::memory_order_relaxed);
                colErr[(size_t) c].store (accErr, std::memory_order_relaxed);
                colWrite.store ((c + 1) % historyCols, std::memory_order_release);
                accIn = accErr = 0.0f;
                colCount = 0;
            }
        }

        // ---- compensação automática: o canal sai com o mesmo volume (RMS ~0,5 s) que entrou ----
        if (! s.autoGain)
            compTarget = 1.0f;
        else if (msIn > 1.0e-7f && msWet > 1.0e-10f)   // em silêncio segura o último valor
            compTarget = juce::jlimit (0.0631f, 15.85f, std::sqrt (msIn / msWet));   // ±24 dB
        smComp.setTargetValue (compTarget);

        drivenPeak.store (juce::jmax (drivenPeak.load(), pkDrv));
        compDb.store (juce::Decibels::gainToDecibels (compTarget));
    }

    double fs = 44100.0;
    int blockMax = 512, lat2 = 0, lat4 = 0;

    juce::dsp::Oversampling<float> os2 { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    juce::dsp::Oversampling<float> os4 { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    bool cur4 = true;

    juce::AudioBuffer<float> work;
    std::vector<float> errPk, drvAbs;
    Shaper shaper;
    int   curModel = -1;
    float lowGainMinus1 = 0.0f, noiseNorm = 1.0f, humAmt = 0.0f, modelTilt = 0.0f;
    bool  topOn = false;
    std::array<Channel, 2> chans;
    double humPhase = 0.0, humInc = 0.0;

    float msIn = 0.0f, msWet = 0.0f, msCoef = 0.0f, compTarget = 1.0f;
    juce::SmoothedValue<float> smDrive, smTone, smXt, smNoise, smComp;
    bool primed = false;

    std::array<std::vector<float>, 2> dryRing, wetRing;
    int ringPos = 0, ringMask = 0;

    int   colLen = 352, colCount = 0;
    float accIn = 0.0f, accErr = 0.0f;
};
} // namespace console
