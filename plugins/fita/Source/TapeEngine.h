#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

namespace fita
{
enum Model { modelQuarter = 0, modelWide, modelCassette };
enum Transport { tRunning = 0, tBraking, tStopped, tStarting };

constexpr float silentV = 0.03f;   // abaixo dessa velocidade a fita está parada: volume zero

// velocidade real da fita; o cassete corre a 1/4 (chave em 7,5 ips = 1 7/8 ips)
inline float tapeIps (int model, int speed)
{
    static const float ips[] = { 7.5f, 15.0f, 30.0f };
    return ips[juce::jlimit (0, 2, speed)] * (model == modelCassette ? 0.25f : 1.0f);
}

// duração do FREIO, em tempos (semínimas)
inline double brakeBeats (int idx)
{
    static const double beats[] = { 0.5, 1.0, 2.0, 4.0, 8.0 };
    return beats[juce::jlimit (0, 4, idx)];
}

// PARTIDA em fração do FREIO; 0 = volta na hora
inline float startFactor (int idx)
{
    static const float f[] = { 0.0f, 0.25f, 1.0f };
    return f[juce::jlimit (0, 2, idx)];
}

// SATURAÇÃO 0–100% = −12 a +18 dB na entrada da fita
inline float driveGain (float drive01) { return juce::Decibels::decibelsToGain (-12.0f + 30.0f * drive01); }

// volume em função da velocidade: some no fim do freio, sem estalo
inline float speedGain (float v)
{
    const float t = juce::jlimit (0.0f, 1.0f, (v - silentV) / 0.3f);
    return t * t * (3.0f - 2.0f * t);
}

// timbre de cada máquina
struct Voicing
{
    float asym, shape, preHz, preDb, hissDb, wowMul, flutterMul;
    float hfHz[3], bumpHz[3];   // por velocidade (7,5 · 15 · 30)
};

inline const Voicing& voicing (int model)
{
    static const Voicing v[] = {
        // rolo 1/4": o "som de estúdio"
        { 0.08f, 1.00f, 3000.0f, 6.0f, -52.0f, 1.0f, 1.0f, { 14000.0f, 18000.0f, 21000.0f }, { 40.0f, 62.0f, 105.0f } },
        // fita 2": mais grave, mais limpa, transporte estável
        { 0.04f, 0.80f, 3000.0f, 5.0f, -58.0f, 0.6f, 0.5f, { 15000.0f, 19000.0f, 22000.0f }, { 35.0f, 55.0f, 95.0f } },
        // cassete: satura antes, perde agudo, chia e oscila mais
        { 0.15f, 1.35f, 2000.0f, 9.0f, -44.0f, 1.6f, 1.8f, { 10000.0f, 13000.0f, 16000.0f }, { 70.0f, 90.0f, 120.0f } },
    };
    return v[juce::jlimit (0, 2, model)];
}

// ---------------- biquad (RBJ), transposta direta II ----------------
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0.0f; }

    // troca a curva sem zerar o estado (sem clique)
    void setCoefs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    float magnitude (float omega) const
    {
        const std::complex<float> z = std::polar (1.0f, -omega), zz = z * z;
        return std::abs ((b0 + b1 * z + b2 * zz) / (1.0f + a1 * z + a2 * zz));
    }

    static Biquad make (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        Biquad q;
        q.b0 = (float) (nb0 / na0); q.b1 = (float) (nb1 / na0); q.b2 = (float) (nb2 / na0);
        q.a1 = (float) (na1 / na0); q.a2 = (float) (na2 / na0);
        return q;
    }

    static Biquad lowpass (double fs, double hz, double q)
    {
        const double w = juce::MathConstants<double>::twoPi * hz / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    static Biquad peak (double fs, double hz, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = juce::MathConstants<double>::twoPi * hz / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
    }

    // prateleira de agudo; com −db é o inverso exato (a de-ênfase desfaz a pré-ênfase)
    static Biquad highShelf (double fs, double hz, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = juce::MathConstants<double>::twoPi * hz / fs, c = std::cos (w);
        const double al = std::sin (w) / std::sqrt (2.0), sa = 2.0 * std::sqrt (A) * al;
        return make (A * ((A + 1.0) + (A - 1.0) * c + sa), -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
                     A * ((A + 1.0) + (A - 1.0) * c - sa), (A + 1.0) - (A - 1.0) * c + sa,
                     2.0 * ((A - 1.0) - (A + 1.0) * c), (A + 1.0) - (A - 1.0) * c - sa);
    }
};

// ---------------- curva da reprodução: reforço de grave (head bump) + perda de agudo ----------------
struct PlaybackEq { Biquad bump, dip, hf, wear; };

inline PlaybackEq designPlaybackHz (double fs, float bumpHz, float bump, float hfHz, float wear)
{
    const double nyq = fs * 0.45;
    PlaybackEq e;
    e.bump = Biquad::peak (fs, bumpHz, 1.0, 6.0 * bump);
    e.dip  = Biquad::peak (fs, bumpHz * 2.2, 1.2, -2.0 * bump);
    e.hf   = Biquad::lowpass (fs, juce::jmin (nyq, (double) hfHz), 0.85);
    e.wear = Biquad::lowpass (fs, juce::jmin (nyq, 20000.0 * std::pow (0.2, (double) wear)), 0.6);   // 20 kHz → 4 kHz
    return e;
}

inline PlaybackEq designPlayback (double fs, int model, int speed, float bump, float wear)
{
    const auto& vc = voicing (model);
    const int sp = juce::jlimit (0, 2, speed);
    return designPlaybackHz (fs, vc.bumpHz[sp], bump, vc.hfHz[sp], wear);
}

struct Settings
{
    double bpm = 145.0;
    int    model = modelQuarter, speed = 1, brake = 3, start = 1;
    float  drive = 0.4f, bump = 0.5f, wear = 0.0f, wow = 0.15f, flutter = 0.15f, hiss = 0.2f;
    bool   stop = false;
};

// ---------------------------------------------------------------------------------------------
// Gravação: pré-ênfase → ganho → saturação (2× oversampling) → de-ênfase → fita (buffer circular).
// Reprodução: cabeça lendo a fita com wow/flutter e TAPE STOP (velocidade variável) → chiado →
// head bump → perda de agudo → desgaste (agudo + dropouts).
// Latência fixa = 5 ms + oversampling, informada ao host; o seco sai atrasado igual (mix sem fase).
// ---------------------------------------------------------------------------------------------
class TapeEngine
{
public:
    // ---- telemetria para a interface ----
    std::atomic<float> rate { 44100.0f }, speedNow { 1.0f }, pitchDev { 0.0f }, counter { 0.0f };
    std::atomic<int>   state { tRunning };

    int latency() const { return latencyTotal; }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        rate.store ((float) fs);
        blockMax = juce::jmax (1, maxBlock);

        os.initProcessing ((size_t) blockMax);
        os.reset();
        osLatency = (int) std::lround (os.getLatencyInSamples());
        work.setSize (2, blockMax);
        gains.assign ((size_t) blockMax, 1.0f);

        baseRead     = (int) std::ceil (fs * 0.005);    // folga para o wow oscilar
        modMax       = (int) std::ceil (fs * 0.0045);
        latencyTotal = baseRead + osLatency;

        // fita: o freio (até 4 s) atrasa até 2 s, a partida outros 2 s
        int len = 1;
        while (len < (int) (fs * 5.0) + baseRead + modMax + 64) len <<= 1;
        mask = len - 1;
        for (auto& t : tape) t.assign ((size_t) len, 0.0f);
        int dl = 1;
        while (dl < latencyTotal + 8) dl <<= 1;
        dryMask = dl - 1;
        for (auto& d : dryBuf) d.assign ((size_t) dl, 0.0f);
        writePos = dryPos = 0;

        for (auto* arr : { &pre, &de, &bumpF, &dipF, &hfF, &wearF })
            for (auto& b : *arr) b.reset();
        dcX.fill (0.0f);
        dcY.fill (0.0f);

        v = 1.0f; xf = 0.0f; xfading = false; lag = 0.0;
        const double twoPi = juce::MathConstants<double>::twoPi;
        flC     = (float) (1.0 - std::exp (-twoPi * 25.0 / fs));
        flNorm  = 1.0f / std::sqrt ((1.0f / 3.0f) * flC / (2.0f - flC));
        flNoise = 0.0f;
        pdevC   = (float) (1.0 - std::exp (-1.0 / (0.03 * fs)));
        pdev = prevMod = 0.0f;
        dropC   = (float) (1.0 - std::exp (-1.0 / (0.004 * fs)));
        dropLeft = dropCheck = 0;
        dropGain = dropDepth = 1.0f;

        smDrive.reset (fs, 0.05);
        primed = false;
    }

    // outL/R = fita; dryL/R = entrada atrasada pela mesma latência
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
    struct Shape { float preHz, preDb, asym, shape, bumpHz, bump, hfHz, wear, hiss, wowD, flutD, wowHz; };

    Shape targets (const Settings& s) const
    {
        const auto& vc = voicing (s.model);
        const int sp = juce::jlimit (0, 2, s.speed);
        static const float hissSpeedDb[] = { 3.0f, 0.0f, -3.0f };   // fita mais rápida chia menos
        const float ms = (float) fs * 0.001f;
        Shape t;
        t.preHz  = vc.preHz;       t.preDb = vc.preDb;
        t.asym   = vc.asym;        t.shape = vc.shape;
        t.bumpHz = vc.bumpHz[sp];  t.bump  = s.bump;
        t.hfHz   = vc.hfHz[sp];    t.wear  = s.wear;
        t.hiss   = s.hiss * s.hiss * juce::Decibels::decibelsToGain (vc.hissDb + hissSpeedDb[sp]);
        t.wowD   = s.wow * 2.2f * vc.wowMul * ms;                                // amostras de desvio
        t.flutD  = s.flutter * 0.06f * vc.flutterMul * (1.0f + s.wear) * ms;     // fita gasta raspa mais
        t.wowHz  = s.model == modelCassette ? 0.9f : 0.45f + 0.1f * (float) sp;
        return t;
    }

    void processChunk (const float* inL, const float* inR, float* outL, float* outR, float* dryL, float* dryR, int m, const Settings& s)
    {
        // ---- parâmetros deslizam por bloco: trocar modelo/velocidade não estala ----
        const auto t = targets (s);
        if (! primed)
        {
            cur = t;
            smDrive.setCurrentAndTargetValue (driveGain (s.drive));
            primed = true;
        }
        else
        {
            const float a = 1.0f - std::exp (-(float) m / (0.05f * (float) fs));
            auto glide = [a] (float& c, float target) { c += a * (target - c); };
            glide (cur.preHz, t.preHz);   glide (cur.preDb, t.preDb);
            glide (cur.asym, t.asym);     glide (cur.shape, t.shape);
            glide (cur.bumpHz, t.bumpHz); glide (cur.bump, t.bump);
            glide (cur.hfHz, t.hfHz);     glide (cur.wear, t.wear);
            glide (cur.hiss, t.hiss);     glide (cur.wowD, t.wowD);
            glide (cur.flutD, t.flutD);   glide (cur.wowHz, t.wowHz);
        }
        smDrive.setTargetValue (driveGain (s.drive));

        {
            const auto preC = Biquad::highShelf (fs, cur.preHz, cur.preDb);
            const auto deC  = Biquad::highShelf (fs, cur.preHz, -cur.preDb);
            const auto eq   = designPlaybackHz (fs, cur.bumpHz, cur.bump, cur.hfHz, cur.wear);
            for (size_t ch = 0; ch < 2; ++ch)
            {
                pre[ch].setCoefs (preC);
                de[ch].setCoefs (deC);
                bumpF[ch].setCoefs (eq.bump);
                dipF[ch].setCoefs (eq.dip);
                hfF[ch].setCoefs (eq.hf);
                wearF[ch].setCoefs (eq.wear);
            }
        }

        // ---- gravação: pré-ênfase → ganho → saturação em 2× → de-ênfase ----
        for (int i = 0; i < m; ++i)
            gains[(size_t) i] = smDrive.getNextValue();

        for (size_t ch = 0; ch < 2; ++ch)
        {
            const float* in = ch == 0 ? inL : inR;
            float* w = work.getWritePointer ((int) ch);
            for (int i = 0; i < m; ++i)
                w[i] = pre[ch].process (in[i]) * gains[(size_t) i];
        }

        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) m);
        auto up = os.processSamplesUp (block);
        {
            // tanh assimétrica com inclinação 1 no zero: sinal baixo passa igual, pico arredonda
            const float b = cur.asym, k = cur.shape, tb = std::tanh (b), norm = 1.0f / (k * (1.0f - tb * tb));
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                float* u = up.getChannelPointer (ch);
                for (size_t j = 0; j < up.getNumSamples(); ++j)
                    u[j] = (std::tanh (k * u[j] + b) - tb) * norm;
            }
        }
        os.processSamplesDown (block);

        for (size_t ch = 0; ch < 2; ++ch)
        {
            float* w = work.getWritePointer ((int) ch);
            for (int i = 0; i < m; ++i)
                w[i] = de[ch].process (w[i] / gains[(size_t) i]);
        }

        // ---- transporte + reprodução ----
        const double brakeSec  = juce::jmin (4.0, brakeBeats (s.brake) * 60.0 / juce::jmax (40.0, s.bpm));
        const float  brakeStep = (float) (1.0 / (brakeSec * fs));
        const float  sf        = startFactor (s.start);
        const float  startStep = sf > 0.0f ? brakeStep / sf : 0.0f;
        const float  xfStep    = (float) (1.0 / (0.03 * fs));
        const double maxLag    = (double) (mask - baseRead - modMax - 16);
        const float  twoPi     = juce::MathConstants<float>::twoPi;
        const float  halfPi    = juce::MathConstants<float>::halfPi;
        const double wowInc    = (double) cur.wowHz / fs;
        const float  dcR       = (float) (1.0 - juce::MathConstants<double>::twoPi * 10.0 / fs);
        const auto*  wl        = work.getReadPointer (0);
        const auto*  wr        = work.getReadPointer (1);
        double travelled = 0.0;

        for (int i = 0; i < m; ++i)
        {
            // TAPE STOP: freia até parar; ao soltar, parte do presente (ou volta na hora) e
            // termina num crossfade de 30 ms para o sinal sem atraso
            if (xfading)
            {
                xf += xfStep;
                if (xf >= 1.0f) { xfading = false; xf = 0.0f; lag = 0.0; v = 1.0f; }
            }
            else if (s.stop)
                v = juce::jmax (0.0f, v - brakeStep);
            else if (v < 1.0f || lag > 0.0)
            {
                if (startStep <= 0.0f || v >= 1.0f)
                {
                    xfading = true;
                    xf = 0.0f;
                }
                else
                {
                    if (v <= silentV) lag = 0.0;     // estava parada: recomeça do presente, ainda em silêncio
                    v = juce::jmin (1.0f, v + startStep);
                }
            }
            lag = juce::jmin (maxLag, lag + (double) (1.0f - v));
            travelled += (double) v;

            // wow (lento, com amplitude que varia a cada volta) + flutter (rápido, com ruído)
            wowPh += wowInc;
            if (wowPh >= 1.0) { wowPh -= 1.0; wowTarget = 0.6f + 0.5f * rng.nextFloat(); }
            wowPh2 += wowInc * 2.37;  if (wowPh2 >= 1.0) wowPh2 -= 1.0;
            flPh1  += 7.3 / fs;       if (flPh1 >= 1.0)  flPh1 -= 1.0;
            flPh2  += 11.9 / fs;      if (flPh2 >= 1.0)  flPh2 -= 1.0;
            wowAmp  += 2.0e-5f * (wowTarget - wowAmp);
            flNoise += flC * ((rng.nextFloat() * 2.0f - 1.0f) - flNoise);

            const float wowV = wowAmp * (0.75f * std::sin (twoPi * (float) wowPh) + 0.25f * std::sin (twoPi * (float) wowPh2));
            const float flV  = 0.5f * std::sin (twoPi * (float) flPh1) + 0.3f * std::sin (twoPi * (float) flPh2)
                             + 0.2f * flNoise * flNorm;
            const float modRaw = cur.wowD * wowV + cur.flutD * flV;
            const float mod    = modRaw * v;                       // parada, não oscila
            pdev += pdevC * ((prevMod - mod) - pdev);
            prevMod = mod;

            // grava e lê a fita
            tape[0][(size_t) writePos] = wl[i];
            tape[1][(size_t) writePos] = wr[i];
            float motor = speedGain (v);
            const double d = (double) baseRead + (double) mod + lag;
            float yl = readTape (0, d) * motor, yr = readTape (1, d) * motor;
            if (xfading)
            {
                const float ca = std::cos (xf * halfPi), cb = std::sin (xf * halfPi);
                const double dl = (double) baseRead + (double) modRaw;
                yl = yl * ca + readTape (0, dl) * cb;
                yr = yr * ca + readTape (1, dl) * cb;
                motor = motor * ca + cb;
            }
            writePos = (writePos + 1) & mask;

            // desgaste: falhas de óxido (o volume cai por 20–120 ms)
            if (dropLeft > 0)
                --dropLeft;
            else if (--dropCheck <= 0)
            {
                dropCheck = (int) (fs * 0.01);
                if (rng.nextFloat() < cur.wear * cur.wear * 0.02f)
                {
                    dropLeft  = (int) (fs * (0.02 + 0.1 * (double) rng.nextFloat()));
                    dropDepth = 1.0f - cur.wear * (0.3f + 0.6f * rng.nextFloat());
                }
            }
            dropGain += dropC * ((dropLeft > 0 ? dropDepth : 1.0f) - dropGain);

            // chiado (some junto quando a fita para) → curva da reprodução
            const float hissG = cur.hiss * motor;
            float y[2] = { yl + hissG * (rng.nextFloat() * 2.0f - 1.0f), yr + hissG * (rng.nextFloat() * 2.0f - 1.0f) };
            for (size_t ch = 0; ch < 2; ++ch)
            {
                const float x  = y[ch];
                const float hp = x - dcX[ch] + dcR * dcY[ch];   // tira o DC da saturação assimétrica
                dcX[ch] = x;
                dcY[ch] = hp;
                y[ch] = wearF[ch].process (hfF[ch].process (dipF[ch].process (bumpF[ch].process (hp)))) * dropGain;
            }
            outL[i] = y[0];
            outR[i] = y[1];

            // seco atrasado pela mesma latência
            dryBuf[0][(size_t) dryPos] = inL[i];
            dryBuf[1][(size_t) dryPos] = inR[i];
            const int rp = (dryPos - latencyTotal) & dryMask;
            dryL[i] = dryBuf[0][(size_t) rp];
            dryR[i] = dryBuf[1][(size_t) rp];
            dryPos = (dryPos + 1) & dryMask;
        }

        // ---- telemetria ----
        tapeSec += travelled / fs;
        if (tapeSec >= 36000.0) tapeSec -= 36000.0;
        speedNow.store (v);
        pitchDev.store (pdev);
        counter.store ((float) tapeSec);
        state.store (xfading || (! s.stop && v < 1.0f) ? tStarting
                     : s.stop ? (v <= silentV ? tStopped : tBraking) : tRunning);
    }

    // leitura fracionária (Hermite de 4 pontos) d amostras atrás da gravação
    float readTape (int ch, double d) const
    {
        const auto&  b  = tape[(size_t) ch];
        const double rp = (double) writePos - d;
        const double fl = std::floor (rp);
        const int    i1 = (int) fl;
        const float  t  = (float) (rp - fl);
        const float y0 = b[(size_t) ((i1 - 1) & mask)], y1 = b[(size_t) (i1 & mask)];
        const float y2 = b[(size_t) ((i1 + 1) & mask)], y3 = b[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    double fs = 44100.0;
    int blockMax = 512, osLatency = 0, baseRead = 0, modMax = 0, latencyTotal = 0;

    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::AudioBuffer<float> work;
    std::vector<float> gains;
    juce::SmoothedValue<float> smDrive;
    Shape cur {};
    bool primed = false;

    std::array<Biquad, 2> pre, de, bumpF, dipF, hfF, wearF;
    std::array<float, 2> dcX {}, dcY {};

    std::array<std::vector<float>, 2> tape, dryBuf;
    int writePos = 0, mask = 0, dryPos = 0, dryMask = 0;

    float  v = 1.0f, xf = 0.0f;
    bool   xfading = false;
    double lag = 0.0, tapeSec = 0.0;

    double wowPh = 0.0, wowPh2 = 0.0, flPh1 = 0.0, flPh2 = 0.0;
    float  wowAmp = 0.85f, wowTarget = 0.85f, flNoise = 0.0f, flC = 0.0f, flNorm = 1.0f;
    float  prevMod = 0.0f, pdev = 0.0f, pdevC = 0.0f;
    int    dropLeft = 0, dropCheck = 0;
    float  dropDepth = 1.0f, dropGain = 1.0f, dropC = 0.0f;
    juce::Random rng;
};
} // namespace fita
