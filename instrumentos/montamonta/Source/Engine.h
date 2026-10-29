#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include "Rack.h"

// Motor do MONTA-MONTA: um Minimoog de 2 osciladores, monofônico (a última nota segurada manda), tocado pelo caminho
// que o usuário montou (Graph). Peça fora do caminho não soa; sem ENV AMP o oscilador fica ligado direto (drone),
// que é justamente o que a lição 2 ensina.
namespace mm
{
struct Settings
{
    std::array<int, 2>   range { 2, 2 };    // 0 = 32' … 4 = 2'
    std::array<int, 2>   wave { 2, 2 };
    std::array<float, 2> tune { 0.0f, 0.12f };
    std::array<float, 3> level { 0.8f, 0.7f, 0.3f };   // 0–1: OSC 1, OSC 2, NOISE
    std::array<bool, 3>  on { true, true, false };
    float cutoff = 1800.0f, emph = 0.3f, contour = 0.5f;   // emph e contour 0–1
    float ampA = 5.0f, ampD = 400.0f, ampS = 0.7f, ampR = 250.0f;
    float modA = 5.0f, modD = 450.0f, modS = 0.2f, modR = 300.0f;
    float volume = 0.7f;                                   // 0–1
};

// janela do osciloscópio: a thread de áudio escreve, a interface lê
struct ScopeRing
{
    static constexpr int size = 2048;
    std::array<float, size> data {};
    std::atomic<int> write { 0 };

    void push (float v)
    {
        const int w = write.load (std::memory_order_relaxed);
        data[(size_t) w] = v;
        write.store ((w + 1) & (size - 1), std::memory_order_release);
    }
};

class Adsr
{
public:
    void prepare (double fs) { rate = fs; }
    void gate (bool on) { stage = on ? Stage::attack : (stage == Stage::idle ? Stage::idle : Stage::release); }
    void reset() { stage = Stage::idle; value = 0.0f; }
    float get() const { return value; }

    float next (float aMs, float dMs, float s, float rMs)
    {
        switch (stage)
        {
            case Stage::attack:
                value += 1.0f / juce::jmax (1.0f, aMs * 0.001f * (float) rate);
                if (value >= 1.0f) { value = 1.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                value = s + (value - s) * coef (dMs);
                break;
            case Stage::release:
                value *= coef (rMs);
                if (value < 1.0e-5f) { value = 0.0f; stage = Stage::idle; }
                break;
            case Stage::idle: break;
        }
        return value;
    }

private:
    enum class Stage { idle, attack, decay, release };
    // chega a ~1% do caminho no tempo pedido
    float coef (float ms) const { return std::exp (-4.6f / juce::jmax (1.0f, ms * 0.001f * (float) rate)); }

    double rate = 44100.0;
    Stage stage = Stage::idle;
    float value = 0.0f;
};

class Engine
{
public:
    void prepare (double fs)
    {
        rate = fs;
        ampEnv.prepare (fs);
        modEnv.prepare (fs);
        reset();
    }

    void reset()
    {
        ampEnv.reset();
        modEnv.reset();
        stage.fill (0.0f);
        phase.fill (0.0);
        numHeld = 0;
        cutSm = -1.0f;
        volSm = -1.0f;
    }

    // última nota segurada manda; nota nova sempre redispara os envelopes (fica claro o que cada um faz)
    void noteOn (int note)
    {
        removeHeld (note);
        if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note;
        current = note;
        ampEnv.gate (true);
        modEnv.gate (true);
    }

    void noteOff (int note)
    {
        removeHeld (note);
        if (numHeld > 0)
            current = held[(size_t) numHeld - 1];
        else
        {
            ampEnv.gate (false);
            modEnv.gate (false);
        }
    }

    void allOff()
    {
        numHeld = 0;
        ampEnv.gate (false);
        modEnv.gate (false);
    }

    bool anyHeld() const { return numHeld > 0; }

    void render (float* outL, float* outR, int n, const Settings& s, const Graph& g, ScopeRing& scope, float& peak)
    {
        const float targetCut = std::log2 (juce::jlimit (20.0f, 18000.0f, s.cutoff));
        const float targetVol = s.volume * s.volume;
        if (cutSm < 0.0f) cutSm = targetCut;
        if (volSm < 0.0f) volSm = targetVol;
        const float sm = 1.0f - std::exp (-1.0f / (0.012f * (float) rate));

        std::array<double, 2> inc {};
        for (int o = 0; o < 2; ++o)
        {
            const double semis = (double) current - 69.0 + (double) s.tune[(size_t) o] + 12.0 * (double) (s.range[(size_t) o] - 2);
            inc[(size_t) o] = juce::jmin (0.45, 440.0 * std::pow (2.0, semis / 12.0) / rate);
        }
        const float k = 4.1f * s.emph;
        const float gain1 = s.on[0] ? s.level[0] : 0.0f, gain2 = s.on[1] ? s.level[1] : 0.0f, gainN = s.on[2] ? s.level[2] : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float aEnv = ampEnv.next (s.ampA, s.ampD, s.ampS, s.ampR);
            const float mEnv = modEnv.next (s.modA, s.modD, s.modS, s.modR);
            cutSm += (targetCut - cutSm) * sm;
            volSm += (targetVol - volSm) * sm;

            std::array<float, numModules> v {};
            v[vco1] = osc (0, s.wave[0], inc[0]);
            v[vco2] = osc (1, s.wave[1], inc[1]);
            v[envMod] = mEnv;
            auto in = [&] (int port) { const int src = g.src[(size_t) port]; return src >= 0 ? v[(size_t) src] : 0.0f; };

            for (int o = 0; o < g.numOrder; ++o)
            {
                const int m = g.order[(size_t) o];
                switch (m)
                {
                    case mixer:
                        v[mixer] = (in (inMix1) * gain1 + in (inMix2) * gain2 + noise() * gainN) * 0.7f;
                        break;
                    case vcf:
                    {
                        float oct = cutSm;
                        if (g.src[inVcfCv] >= 0) oct += in (inVcfCv) * s.contour * 6.0f;
                        v[vcf] = ladder (in (inVcf), std::exp2 (oct), k);
                        break;
                    }
                    case envAmp: v[envAmp] = in (inAmp) * aEnv; break;
                    case output: v[output] = in (inOut) * volSm; break;
                    default: break;
                }
            }

            const float o = soft (v[output] * 0.8f);   // SAÍDA fora do rack ou sem nada ligado: zero
            outL[i] = o;
            outR[i] = o;
            peak = juce::jmax (peak, std::abs (o));
            if (g.probe >= 0) scope.push (g.probe == output ? o : v[(size_t) g.probe]);
        }
    }

private:
    static float soft (float x) { return x / std::sqrt (1.0f + x * x * 0.6f); }

    void removeHeld (int note)
    {
        int w = 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        numHeld = w;
    }

    static float blep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    // as 6 formas do Minimoog: triângulo, triângulo/serra, serra, quadrada, retangular larga e estreita
    float osc (int o, int wave, double dt)
    {
        double& p = phase[(size_t) o];
        const float tri = (float) (2.0 * std::abs (2.0 * p - 1.0) - 1.0);
        const float saw = (float) (2.0 * p - 1.0) - blep (p, dt);
        auto pulse = [&] (double pw)
        {
            double q = p - pw;
            if (q < 0.0) q += 1.0;
            return (p < pw ? 1.0f : -1.0f) + blep (p, dt) - blep (q, dt) - (float) (2.0 * pw - 1.0);
        };
        float y;
        switch (wave)
        {
            case 0:  y = tri; break;
            case 1:  y = 0.5f * (tri + saw); break;
            case 2:  y = saw; break;
            case 3:  y = pulse (0.5); break;
            case 4:  y = pulse (0.3); break;
            default: y = pulse (0.12); break;
        }
        p += dt;
        if (p >= 1.0) p -= 1.0;
        return y * 0.8f;
    }

    float noise()
    {
        seed = seed * 1664525u + 1013904223u;
        return ((float) (seed >> 9) * (1.0f / 8388608.0f) - 1.0f) * 0.6f;
    }

    // filtro ladder de 4 polos (24 dB/oit) em TPT, com a realimentação (EMPHASIS) passando por uma saturação
    float ladder (float x, float fc, float k)
    {
        fc = juce::jlimit (20.0f, (float) rate * 0.45f, fc);
        const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) rate);
        const float G = g / (1.0f + g);
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        // cada estágio: y = G·x + (1−G)·s ⇒ saída do 4º = G⁴·u + S
        const float S = (G3 * stage[0] + G2 * stage[1] + G * stage[2] + stage[3]) * (1.0f - G);
        float u = (x - k * S) / (1.0f + k * G4);
        u = std::tanh (u);
        float y = u;
        for (int i = 0; i < 4; ++i)
        {
            const float vv = (y - stage[(size_t) i]) * G;
            const float out = vv + stage[(size_t) i];
            stage[(size_t) i] = out + vv;
            y = out;
        }
        return y * (1.0f + k * 0.35f);
    }

    double rate = 44100.0;
    Adsr ampEnv, modEnv;
    std::array<double, 2> phase {};
    std::array<float, 4> stage {};
    std::array<int, 16> held {};
    int numHeld = 0;
    int current = 48;      // antes da primeira nota: Dó 3 (drone da lição 1)
    float cutSm = -1.0f, volSm = -1.0f;
    uint32_t seed = 22222u;
};
} // namespace mm
