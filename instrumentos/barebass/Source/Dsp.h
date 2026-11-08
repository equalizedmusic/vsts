#pragma once
// BAREBASS: motor do synth de baixo, sem JUCE (o teste em tools/ compila só com este arquivo).
// Uma voz (mono): OSC 1 + OSC 2 + SUB -> filtro passa-baixa 12/24 dB (TPT/SVF, sem atraso) com envelope ADSR
// -> unHUNF (decay só no grave, de 10 a 50 Hz) -> volume. VELOCITY (knob VELO) pesa no volume e no ENV. Amplitude = gate (abre na hora, fecha no RELEASE).
// A cada nota redisparada as ondas recomeçam do ponto inicial (fase 0): todas as notas batem igual, sem drift.
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
    bool   liga[2] { true, true };      // LED do OSC 1 / OSC 2
    bool   ligaSub = true;
    int    onda[2] { ondaSaw, ondaSqr };
    int    oitava[2] { 0, -1 };
    float  fino[2] { 0.0f, -7.0f };     // cents (o OSC 1 no tom: a altura que se ouve vem dele)
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
    float  velo = 0.0f;                 // 0..1: quanto a velocity pesa no volume e na abertura do ENV
};

// o que a tela mostra (escrito pelo áudio, lido a 30 Hz)
struct Leitura
{
    std::atomic<int>   nota { -1 };         // nota MIDI soando (−1 = nenhuma)
    std::atomic<bool>  gate { false };
    std::atomic<float> freq { 0.0f };       // Hz da nota (com glide e bend)
    std::atomic<float> cutoffAgora { 18000.0f };
    std::atomic<float> env { 0.0f };        // 0..1
    std::atomic<float> envT { 10.0f };      // s desde o último disparo
    std::atomic<float> hunfGr { 0.0f };     // dB (≤ 0) que o unHUNF tira agora do sub da nota
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
    double hp (double x, double g, double k)
    {
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return x - k * v1 - v2;
    }
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

// unHUNF: ponto de corte FIXO (no knob: de 12 Hz em pouco a 65 Hz no máximo) e o que fica abaixo dele perde volume
// depois de cada nota, no tempo do knob (300 ms em pouco, 15 ms no máximo). O SUB e os OSC em oitava abaixo perdem só
// volume, conforme a frequência deles em relação ao corte (curva de 48 dB/oitava); os OSC na oitava 0+ passam por um
// crossover Linkwitz-Riley de 48 dB/oitava e só a banda de baixo perde volume. No disparo o sub bate inteiro.
// 0.3.0: antes era um passa-alta que SUBIA de 2 Hz até o ponto do knob a cada nota. O filtro varrendo por cima do sub
// tocava a própria frequência dele: o grave deslizava até meia oitava acima da nota antes de sumir ("desafinado").
// Agora nada se move em frequência: só volume, e a altura fica a da nota.
inline double hunfTau (double h) { return 0.3 * std::pow (0.05, h); }
inline double hunfFim (double h) { return h <= 0.0005 ? 2.0 : 12.0 * std::pow (65.0 / 12.0, std::pow (h, 0.4)); }
inline double hunfBaixa (double t, double h) { return std::exp (-t / hunfTau (h)); }   // volume da banda de baixo (1 -> 0)
// quanto passa numa frequência f com o crossover em fc e a banda de baixo no volume gb
inline double hunfGanhoEm (double f, double fc, double gb) { const double r = f / fc, r4 = r * r * r * r, r8 = r4 * r4; return (r8 + gb) / (1.0 + r8); }
// o que sobra de um sub de 30 Hz (a tela desenha isso)
constexpr double hunfRef = 30.0;
inline double hunfPiso (double h) { return hunfGanhoEm (hunfRef, hunfFim (h), 0.0); }
inline double hunfGanho (double h, double t) { return hunfGanhoEm (hunfRef, hunfFim (h), hunfBaixa (t, h)); }

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
        cutSuave = 18000.0; volSuave = 0.7; hunfSuave = 0.0;
    }

    void reset()
    {
        nHeld = 0; notaAtual = -1;
        for (auto& c : conta) c = 0;
        envF.zera(); amp = 0.0; gateOn = false;
        svf[0].zera(); svf[1].zera();
        for (auto& f : hpH) f.zera();
        for (auto& f : lpH) f.zera();
        for (auto& p : fase) p = 0.0;
        faseSub = faseRef = 0.0;
        tDisparo = 10.0; ganhoHunf = 1.0; vel = velSuave = 1.0;
        inicio[0] = inicio[1] = inicioSub = true;
        leitura.nota = -1; leitura.gate = false;
    }

    // ---- MIDI ----
    // conta[] = quantas vezes a mesma nota está ligada: com 1/16 encostados alguns DAWs mandam o "liga" da nota nova
    // antes do "solta" da anterior (mesma altura). Esse "solta" atrasado não pode cortar a nota que acabou de começar.
    // velo = velocity 0..1; a.velo = quanto ela pesa (0 = toda nota igual)
    void noteOn (int nota, const Ajustes& a, float velo = 1.0f)
    {
        nota &= 127;
        vel = 1.0 - a.velo * (1.0 - std::clamp ((double) velo, 0.0, 1.0));
        ++conta[nota];
        removeHeld (nota);
        const bool tinha = nHeld > 0;
        if (nHeld < maxHeld) held[nHeld++] = nota;
        vaiPara (nota, ! (a.legato && tinha), tinha || gateOn, a);
    }

    void noteOff (int nota, const Ajustes& a)
    {
        nota &= 127;
        if (conta[nota] > 0) --conta[nota];
        if (conta[nota] > 0) return;
        const bool eraAtual = nHeld > 0 && held[nHeld - 1] == nota;
        removeHeld (nota);
        if (! eraAtual) return;
        if (nHeld > 0) vaiPara (held[nHeld - 1], ! a.legato, true, a);   // volta para a nota que ainda está presa
        else { gateOn = false; envF.solta(); leitura.gate = false; }
    }

    void allNotesOff() { for (auto& c : conta) c = 0; nHeld = 0; gateOn = false; envF.solta(); leitura.gate = false; }
    void pitchBend (int valor14) { bend = (valor14 - 8192) / 8192.0 * 2.0; }   // ±2 semitons

    // ---- áudio ----
    void render (float* const* out, int nch, int ini, int n, const Ajustes& a)
    {
        const double glideCoef = a.glide > 0.0005f ? std::exp (-4.6 / (a.glide * fs)) : 0.0;
        const double cutCoef = std::exp (-1.0 / (0.01 * fs));
        const double volAlvo = std::pow (10.0, a.volumeDb / 20.0);
        const double ampRel = std::exp (-4.6 / (std::max (0.005f, a.rel) * fs));
        const double k1 = 1.0 / qDe (a.reso, a.slope24, 0), k2 = 1.0 / qDe (a.reso, a.slope24, 1);
        const double nyq = fs * 0.45;
        double mult[2], multSub = std::pow (2.0, a.oitavaSub);
        for (int o = 0; o < 2; ++o) mult[o] = std::pow (2.0, a.oitava[o] + a.fino[o] / 1200.0);
        // referência do scope: a frequência da nota tocada (dois ciclos dela na tela; o sub −1 aparece como um ciclo).
        // Dois ciclos do sub passariam de uma nota de 1/16 e a tela misturaria duas notas.
        const double multRef = 1.0;
        // o knob do unHUNF anda suave (20 ms): girar não estala
        const double hunfCoef = std::exp (-1.0 / (0.02 * fs));
        const double velCoef = std::exp (-1.0 / (0.003 * fs));   // em LEGATO a velocity muda sem redisparo: 3 ms, sem estalo
        double fcH = hunfFim (hunfSuave), gH = std::tan (pi * fcH / fs), gb = 1.0, gOsc[2] { 1.0, 1.0 }, gSub = 1.0;
        float pk = 0.0f;
        int64_t w = scope.escrito.load (std::memory_order_relaxed);

        for (int i = 0; i < n; ++i)
        {
            // altura (glide em semitons)
            if (glideCoef > 0.0) notaGlide = alvoGlide + (notaGlide - alvoGlide) * glideCoef;
            else notaGlide = alvoGlide;
            const double f0 = 440.0 * std::pow (2.0, (notaGlide + bend - 69.0) / 12.0);

            // unHUNF (recalculado a cada 8 amostras): ponto de corte fixo (só anda com o knob) e o volume da parte de
            // baixo caindo desde o disparo
            hunfSuave = a.hunf + (hunfSuave - a.hunf) * hunfCoef;
            tDisparo += 1.0 / fs;
            if ((i & 7) == 0)
            {
                fcH = hunfFim (hunfSuave);
                gH = std::tan (pi * fcH / fs);
                gb = hunfBaixa (tDisparo, hunfSuave);
                for (int o = 0; o < 2; ++o) gOsc[o] = a.oitava[o] < 0 ? hunfGanhoEm (f0 * mult[o], fcH, gb) : 1.0;
                gSub = hunfGanhoEm (f0 * multSub, fcH, gb);
            }

            // amplitude (gate) e velocity ANTES do crossover: com a nota parada os osciladores seguem girando e não
            // podem carregar o filtro (a nota nova recomeça a onda do zero e pegaria o resto)
            if (gateOn) amp = 1.0;   // abre na hora: o ataque é o da própria onda saindo do ponto inicial
            else amp *= ampRel;
            velSuave = vel + (velSuave - vel) * velCoef;
            const double ganho = amp * velSuave;

            // osciladores. O SUB e os OSC em oitava abaixo (camadas de sub) perdem só VOLUME, pela frequência deles: sem
            // filtro no caminho, a altura fica exata. Os OSC na oitava 0 ou acima vão pelo crossover (abaixo).
            double cheio = 0.0, sub = 0.0, mixCru = 0.0, sOsc = 0.0;
            for (int o = 0; o < 2; ++o)
            {
                const double dt = std::min (0.45, f0 * mult[o] / fs);
                const double s = oscila (a.onda[o], fase[o], dt, inicio[o]);
                inicio[o] = false;
                fase[o] += dt; if (fase[o] >= 1.0) fase[o] -= 1.0;
                if (! a.liga[o]) continue;
                mixCru += s * a.nivel[o];
                if (a.oitava[o] < 0) sub += s * a.nivel[o] * gOsc[o];
                else cheio += s * a.nivel[o];
            }
            {
                const double dt = std::min (0.45, f0 * multSub / fs);
                sOsc = oscila (a.ondaSub == subSqr ? ondaSqr : ondaSen, faseSub, dt, inicioSub) * (a.ligaSub ? a.nivelSub : 0.0f);
                inicioSub = false;
                faseSub += dt; if (faseSub >= 1.0) faseSub -= 1.0;
                mixCru += sOsc;
                sub += sOsc * gSub;
            }
            // OSC na oitava 0+: crossover Linkwitz-Riley de 8ª ordem (Butterworth de 4ª, Q 0,5412 e 1,3066, duas vezes);
            // só a fundamental de nota bem grave cai nele, e a altura de uma onda cheia vem dos harmônicos de cima
            constexpr double kA = 1.0 / 0.54119610, kB = 1.0 / 1.30656296;
            cheio *= ganho;
            const double baixa = lpH[3].lp (lpH[2].lp (lpH[1].lp (lpH[0].lp (cheio, gH, kA), gH, kB), gH, kA), gH, kB);
            const double alta = hpH[3].hp (hpH[2].hp (hpH[1].hp (hpH[0].hp (cheio, gH, kA), gH, kB), gH, kA), gH, kB);
            const double mix = 0.5 * (alta + baixa * gb + sub * ganho);
            const double antes = 0.5 * mixCru * ganho;   // a tela mostra a onda crua (antes do unHUNF e do filtro)

            // filtro
            const double e = envF.proximo (a.atk, a.dec, a.sus, a.rel);
            cutSuave = a.cutoff + (cutSuave - a.cutoff) * cutCoef;
            const double fc = std::clamp (cutSuave * std::pow (2.0, a.envAmt * velSuave * e * 6.0), 16.0, nyq);   // nota fraca abre menos
            const double g = std::tan (pi * fc / fs);
            double x = std::tanh (mix * 1.2) / 1.2;
            double y = svf[0].lp (x, g, k1);
            if (a.slope24) y = svf[1].lp (y, g, k2);
            const double depois = y;

            volSuave = volAlvo + (volSuave - volAlvo) * cutCoef;
            const float s = (float) (depois * volSuave);
            for (int c = 0; c < nch; ++c) out[c][ini + i] = s;
            pk = std::max (pk, std::abs (s));

            // scope
            const int idx = (int) (w % Scope::size);
            scope.antes[idx] = (float) antes; scope.depois[idx] = (float) depois; scope.sub[idx] = (float) (sOsc * gSub * 0.5 * amp * velSuave);
            const double dRef = f0 * multRef / fs;
            faseRef += dRef;
            if (faseRef >= 1.0)   // zerada no disparo: as marcas caem sempre no mesmo ponto da onda, nota após nota
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
        // quanto o unHUNF está tirando agora do sub desta nota
        ganhoHunf = gSub;
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

    // fase 0 = o ponto inicial de cada onda: SIN e TRI começam no zero subindo; SAW começa no topo (+1) e desce;
    // SQR começa em +1. Na primeira amostra depois do disparo (inicio) a borda da SAW/SQR sai seca, sem a correção
    // de aliasing do "ciclo anterior" que não existiu: é o clique do ataque.
    static double oscila (int onda, double p, double dt, bool inicio)
    {
        switch (onda)
        {
            case ondaSen: return std::sin (2.0 * pi * p);
            case ondaTri:
            {
                double q = p + 0.25; if (q >= 1.0) q -= 1.0;
                return q < 0.5 ? 4.0 * q - 1.0 : 3.0 - 4.0 * q;
            }
            case ondaSaw: return 1.0 - 2.0 * p + (inicio ? 0.0 : polyBlep (p, dt));
            default:
            {
                double s = p < 0.5 ? 1.0 : -1.0;
                if (! inicio) s += polyBlep (p, dt);
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
            // toda nota redisparada recomeça a onda do ponto inicial (também por cima do release da anterior):
            // sem isso a fase "anda" de nota para nota e cada uma bate diferente
            for (auto& p : fase) p = 0.0;
            faseSub = 0.0; faseRef = 0.0;
            inicio[0] = inicio[1] = inicioSub = true;
            envF.dispara();
        }
        // o unHUNF recomeça a cada nota (também em legato): o sub bate e some de novo
        tDisparo = 0.0;
        gateOn = true;
        leitura.nota = nota; leitura.gate = true;
    }

    static constexpr int maxHeld = 16;
    int held[maxHeld] {}, nHeld = 0, notaAtual = -1, conta[128] {};
    double fs = 48000.0;
    Envelope envF;
    Svf svf[2];
    Svf hpH[4], lpH[4];   // crossover do unHUNF (4 estágios de cada lado = 8ª ordem)
    double fase[2] {}, faseSub = 0, faseRef = 0, hunfSuave = 0;
    bool inicio[2] { true, true }, inicioSub = true;
    double notaGlide = 36, alvoGlide = 36, bend = 0;
    double amp = 0, cutSuave = 18000, volSuave = 0.7, ultimoFc = 18000;
    double tDisparo = 10, ganhoHunf = 1, vel = 1, velSuave = 1;
    bool gateOn = false;
};
} // namespace barebass
