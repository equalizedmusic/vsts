#pragma once
// BAREBASS: motor do synth de baixo, sem JUCE (o teste em tools/ compila só com este arquivo).
// Uma voz (mono): OSC 1 + OSC 2 + SUB -> filtro passa-baixa 12/24 dB (TPT/SVF, sem atraso) com envelope ADSR
// -> HUNF (decay só no grave de 20–80 Hz) -> volume. Amplitude = gate (abre em 1,5 ms, fecha no RELEASE).
// A tela lê o que acontece agora pela struct Leitura e pelo Scope (dois ciclos da onda antes e depois do filtro).
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace barebass
{
constexpr double pi = 3.14159265358979323846;

enum Onda { ondaSen = 0, ondaTri, ondaSaw, ondaSqr };
enum OndaSub { subSen = 0, subSqr };

struct Ajustes
{
    int    onda[2] { ondaSaw, ondaSqr };
    int    oitava[2] { 0, -1 };
    float  fino[2] { 7.0f, -7.0f };     // cents
    float  nivel[2] { 0.8f, 0.45f };    // 0..1
    int    ondaSub = subSen;
    int    oitavaSub = -1;
    float  nivelSub = 0.7f;
    bool   slope24 = true;
    float  cutoff = 180.0f;             // Hz
    float  reso = 0.35f;                // 0..1
    float  envAmt = 0.72f;              // −1..+1  (±6 oitavas)
    float  atk = 0.0f, dec = 0.092f, sus = 0.0f, rel = 0.04f;   // s, s, 0..1, s
    float  hunf = 0.6f;                 // 0..1
    float  glide = 0.0f;                // s
    bool   legato = false;
    float  volumeDb = -3.0f;
};

// o que a tela mostra (escrito pelo áudio, lido a 30 Hz)
struct Leitura
{
    std::atomic<int>   nota { -1 };         // nota MIDI soando (−1 = nenhuma)
    std::atomic<bool>  gate { false };
    std::atomic<float> freq { 0.0f };       // Hz da nota (com glide e bend)
    std::atomic<float> cutoffAgora { 180.0f };
    std::atomic<float> env { 0.0f };        // 0..1
    std::atomic<float> envT { 10.0f };      // s desde o último disparo
    std::atomic<float> hunfGr { 0.0f };     // dB (≤ 0) no grave agora
    std::atomic<float> pico { 0.0f };       // pico da saída no último bloco
};

// dois ciclos da onda: anel de amostras + marcas do início de cada ciclo da frequência de referência
struct Scope
{
    static constexpr int size = 16384, marcas = 64;
    float antes[size] {}, depois[size] {}, sub[size] {};
    std::atomic<int64_t> escrito { 0 };       // total de amostras escritas
    int64_t marca[marcas] {};
    float   periodo[marcas] {};
    std::atomic<int> nMarcas { 0 };
};

class Envelope
{
public:
    void prepare (double fs_) { fs = fs_; }
    void dispara() { estagio = 1; }
    void solta() { if (estagio != 0) estagio = 4; }
    void zera() { estagio = 0; v = 0.0; }
    bool ativo() const { return estagio != 0; }
    double valor() const { return v; }

    double proximo (double a, double d, double s, double r)
    {
        switch (estagio)
        {
            case 1: // ataque: rampa linear (0 = instantâneo)
                if (a < 0.0005) v = 1.0;
                else v += 1.0 / (a * fs);
                if (v >= 1.0) { v = 1.0; estagio = 2; }
                break;
            case 2: // decay exponencial até o sustain (chega a ~1% do caminho no tempo de DECAY)
                v = s + (v - s) * coef (d);
                break;
            case 4: // release
                v *= coef (r);
                if (v < 1.0e-5) { v = 0.0; estagio = 0; }
                break;
            default: break;
        }
        return v;
    }

private:
    double coef (double t) const { return std::exp (-4.6 / (std::max (0.0005, t) * fs)); }
    double fs = 48000.0, v = 0.0;
    int estagio = 0;
};

// filtro de estado variável TPT (Zavalishin / Simper): passa-baixa sem atraso, estável com o corte mudando a cada amostra
struct Svf
{
    double ic1 = 0, ic2 = 0;
    void zera() { ic1 = ic2 = 0; }
    double lp (double x, double g, double k)
    {
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return v2;
    }
};

// Q de cada estágio: em 0% de RESONANCE, 12 dB = Butterworth (0,707) e 24 dB = Butterworth de 4ª ordem (1,307 · 0,541)
inline double qDe (double reso, bool slope24, int estagio)
{
    if (slope24 && estagio == 1) return 0.541;
    const double q0 = slope24 ? 1.307 : 0.707;
    return q0 + (14.0 - q0) * std::pow (std::clamp (reso, 0.0, 1.0), 2.2);
}

// resposta do filtro (dB) numa frequência, para a tela desenhar a curva
inline double respostaDb (double f, double fc, double reso, bool slope24)
{
    auto um = [&] (double q)
    {
        const double w = f / fc;
        const double re = 1.0 - w * w, im = w / q;
        return 1.0 / std::sqrt (re * re + im * im);
    };
    double m = um (qDe (reso, slope24, 0));
    if (slope24) m *= um (qDe (reso, slope24, 1));
    return 20.0 * std::log10 (std::max (1.0e-9, m));
}

// HUNF: o grave (passa-baixa de 80 Hz, 24 dB) cai depois de cada nota; acima de 80 Hz nada muda
inline double hunfTau (double h) { return 1.5 * std::pow (0.02, h); }   // 1,5 s (pouco) .. 30 ms (100%)
inline double hunfPiso (double h) { return 1.0 - h; }
inline double hunfGanho (double h, double t)
{
    if (h <= 0.001) return 1.0;
    const double piso = hunfPiso (h);
    return piso + (1.0 - piso) * std::exp (-t / hunfTau (h));
}

class Engine
{
public:
    Leitura leitura;
    Scope scope;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        envF.prepare (fs);
        reset();
        // passa-baixa do HUNF: dois Butterworth de 2ª ordem em 80 Hz (biquad, coeficientes fixos)
        const double w = std::tan (pi * 80.0 / fs), q = 0.7071;
        const double n = 1.0 / (1.0 + w / q + w * w);
        hb0 = w * w * n; hb1 = 2.0 * hb0; hb2 = hb0; hn = n;
        ha1 = 2.0 * (w * w - 1.0) * n; ha2 = (1.0 - w / q + w * w) * n;
        cutSuave = 180.0; volSuave = 0.7;
    }

    void reset()
    {
        nHeld = 0; notaAtual = -1;
        envF.zera(); amp = 0.0; gateOn = false;
        svf[0].zera(); svf[1].zera();
        for (auto& z : hz) z[0] = z[1] = 0.0;
        for (auto& z : hzA) z[0] = z[1] = 0.0;
        for (auto& p : fase) p = 0.0;
        faseSub = faseRef = 0.0;
        tDisparo = 10.0; ganhoHunf = 1.0;
        leitura.nota = -1; leitura.gate = false;
    }

    // ---- MIDI ----
    void noteOn (int nota, const Ajustes& a)
    {
        removeHeld (nota);
        const bool tinha = nHeld > 0;
        if (nHeld < maxHeld) held[nHeld++] = nota;
        vaiPara (nota, ! (a.legato && tinha), tinha || gateOn, a);
    }

    void noteOff (int nota, const Ajustes& a)
    {
        const bool eraAtual = nHeld > 0 && held[nHeld - 1] == nota;
        removeHeld (nota);
        if (! eraAtual) return;
        if (nHeld > 0) vaiPara (held[nHeld - 1], ! a.legato, true, a);   // volta para a nota que ainda está presa
        else { gateOn = false; envF.solta(); leitura.gate = false; }
    }

    void allNotesOff() { nHeld = 0; gateOn = false; envF.solta(); leitura.gate = false; }
    void pitchBend (int valor14) { bend = (valor14 - 8192) / 8192.0 * 2.0; }   // ±2 semitons

    // ---- áudio ----
    void render (float* const* out, int nch, int ini, int n, const Ajustes& a)
    {
        const double glideCoef = a.glide > 0.0005f ? std::exp (-4.6 / (a.glide * fs)) : 0.0;
        const double cutCoef = std::exp (-1.0 / (0.01 * fs));
        const double volAlvo = std::pow (10.0, a.volumeDb / 20.0);
        const double ampAtk = 1.0 / (0.0015 * fs);
        const double ampRel = std::exp (-4.6 / (std::max (0.005f, a.rel) * fs));
        const double k1 = 1.0 / qDe (a.reso, a.slope24, 0), k2 = 1.0 / qDe (a.reso, a.slope24, 1);
        const double nyq = fs * 0.45;
        double mult[2], multSub = std::pow (2.0, a.oitavaSub);
        for (int o = 0; o < 2; ++o) mult[o] = std::pow (2.0, a.oitava[o] + a.fino[o] / 1200.0);
        // referência do scope: a frequência da nota tocada (dois ciclos dela na tela; o sub −1 aparece como um ciclo).
        // Dois ciclos do sub passariam de uma nota de 1/16 e a tela misturaria duas notas.
        const double multRef = 1.0;
        const double tau = hunfTau (a.hunf), piso = hunfPiso (a.hunf);
        const bool hunfOn = a.hunf > 0.001f;
        const double hCoef = std::exp (-1.0 / (tau * fs));
        const double sobe = std::exp (-1.0 / (0.002 * fs));
        float pk = 0.0f;
        int64_t w = scope.escrito.load (std::memory_order_relaxed);

        for (int i = 0; i < n; ++i)
        {
            // altura (glide em semitons)
            if (glideCoef > 0.0) notaGlide = alvoGlide + (notaGlide - alvoGlide) * glideCoef;
            else notaGlide = alvoGlide;
            const double f0 = 440.0 * std::pow (2.0, (notaGlide + bend - 69.0) / 12.0);

            // osciladores
            double mix = 0.0, sOsc = 0.0;
            for (int o = 0; o < 2; ++o)
            {
                const double dt = std::min (0.45, f0 * mult[o] / fs);
                const double s = oscila (a.onda[o], fase[o], dt);
                fase[o] += dt; if (fase[o] >= 1.0) fase[o] -= 1.0;
                mix += s * a.nivel[o];
            }
            {
                const double dt = std::min (0.45, f0 * multSub / fs);
                sOsc = oscila (a.ondaSub == subSqr ? ondaSqr : ondaSen, faseSub, dt) * a.nivelSub;
                faseSub += dt; if (faseSub >= 1.0) faseSub -= 1.0;
                mix += sOsc;
            }
            mix *= 0.5;

            // amplitude (gate)
            if (gateOn) amp = std::min (1.0, amp + ampAtk);
            else amp *= ampRel;
            const double antes = mix * amp;

            // filtro
            const double e = envF.proximo (a.atk, a.dec, a.sus, a.rel);
            cutSuave = a.cutoff + (cutSuave - a.cutoff) * cutCoef;
            const double fc = std::clamp (cutSuave * std::pow (2.0, a.envAmt * e * 6.0), 16.0, nyq);
            const double g = std::tan (pi * fc / fs);
            double x = std::tanh (antes * 1.2) / 1.2;
            double y = svf[0].lp (x, g, k1);
            if (a.slope24) y = svf[1].lp (y, g, k2);

            // HUNF: crossover Linkwitz-Riley de 4ª ordem em 80 Hz (grave + agudo somam plano) e o decay só no grave
            double baixo = y, alto = y;
            for (int s2 = 0; s2 < 2; ++s2)
            {
                auto& z = hz[s2];
                const double o = hb0 * baixo + z[0];
                z[0] = hb1 * baixo - ha1 * o + z[1];
                z[1] = hb2 * baixo - ha2 * o;
                baixo = o;
                auto& zh = hzA[s2];
                const double oh = hn * alto + zh[0];
                zh[0] = -2.0 * hn * alto - ha1 * oh + zh[1];
                zh[1] = hn * alto - ha2 * oh;
                alto = oh;
            }
            tDisparo += 1.0 / fs;
            double alvoH = 1.0;
            if (hunfOn) { hDecai = piso + (hDecai - piso) * hCoef; alvoH = hDecai; }
            ganhoHunf = alvoH + (ganhoHunf - alvoH) * (alvoH > ganhoHunf ? sobe : 0.0);
            // HUNF em 0: sem crossover nenhum (a onda sai como o filtro deixou)
            const double depois = hunfOn ? alto + baixo * ganhoHunf : y;

            volSuave = volAlvo + (volSuave - volAlvo) * cutCoef;
            const float s = (float) (depois * volSuave);
            for (int c = 0; c < nch; ++c) out[c][ini + i] = s;
            pk = std::max (pk, std::abs (s));

            // scope
            const int idx = (int) (w % Scope::size);
            scope.antes[idx] = (float) antes; scope.depois[idx] = (float) depois; scope.sub[idx] = (float) (sOsc * 0.5 * amp);
            const double dRef = f0 * multRef / fs;
            faseRef += dRef;
            if (faseRef >= 1.0)
            {
                faseRef -= 1.0;
                const int m = scope.nMarcas.load (std::memory_order_relaxed);
                scope.marca[m % Scope::marcas] = w;
                scope.periodo[m % Scope::marcas] = (float) (1.0 / dRef);
                scope.nMarcas.store (m + 1, std::memory_order_release);
            }
            ++w;
            ultimoFc = fc;
        }
        scope.escrito.store (w, std::memory_order_release);

        leitura.freq = (float) (440.0 * std::pow (2.0, (notaGlide + bend - 69.0) / 12.0));
        leitura.cutoffAgora = (float) ultimoFc;
        leitura.env = (float) envF.valor();
        leitura.envT = (float) tDisparo;
        leitura.hunfGr = (float) (20.0 * std::log10 (std::max (1.0e-4, ganhoHunf)));
        leitura.pico = pk;
    }

    double taxa() const { return fs; }

private:
    static double polyBlep (double t, double dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    static double oscila (int onda, double p, double dt)
    {
        switch (onda)
        {
            case ondaSen: return std::sin (2.0 * pi * p);
            case ondaTri: return p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p;
            case ondaSaw:
            {
                double q = p + 0.5; if (q >= 1.0) q -= 1.0;     // fase 0 = valor 0 (sem estalo no disparo)
                return 2.0 * q - 1.0 - polyBlep (q, dt);
            }
            default:
            {
                double s = p < 0.5 ? 1.0 : -1.0;
                s += polyBlep (p, dt);
                double q = p + 0.5; if (q >= 1.0) q -= 1.0;
                s -= polyBlep (q, dt);
                return s;
            }
        }
    }

    void removeHeld (int nota)
    {
        int j = 0;
        for (int i = 0; i < nHeld; ++i) if (held[i] != nota) held[j++] = held[i];
        nHeld = j;
    }

    void vaiPara (int nota, bool redispara, bool podeDeslizar, const Ajustes& a)
    {
        alvoGlide = nota;
        if (! (podeDeslizar && a.glide > 0.0005f) || notaAtual < 0) notaGlide = nota;
        notaAtual = nota;
        if (redispara)
        {
            if (! gateOn && amp < 0.01)   // nota nova do silêncio: fases do começo (ataque igual em toda nota)
            {
                for (auto& p : fase) p = 0.0;
                faseSub = 0.0;
            }
            envF.dispara();
        }
        // o HUNF recomeça a cada nota (também em legato): o grave bate e some de novo
        tDisparo = 0.0; hDecai = 1.0;
        gateOn = true;
        leitura.nota = nota; leitura.gate = true;
    }

    static constexpr int maxHeld = 16;
    int held[maxHeld] {}, nHeld = 0, notaAtual = -1;
    double fs = 48000.0;
    Envelope envF;
    Svf svf[2];
    double hz[2][2] {}, hzA[2][2] {};
    double hb0 = 0, hb1 = 0, hb2 = 0, ha1 = 0, ha2 = 0, hn = 0;
    double fase[2] {}, faseSub = 0, faseRef = 0;
    double notaGlide = 36, alvoGlide = 36, bend = 0;
    double amp = 0, cutSuave = 180, volSuave = 0.7, ultimoFc = 180;
    double tDisparo = 10, hDecai = 1, ganhoHunf = 1;
    bool gateOn = false;
};
} // namespace barebass
