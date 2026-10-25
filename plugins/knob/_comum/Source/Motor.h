#pragma once
// SÉRIE KNOB — o que cada pedal implementa. A base (processor, relógio, TRIGGER, interface) é igual nos 10;
// cada pasta plugins/knob/<pedal>/Source tem um Pedal.h com a Info e o motor de som.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace sk
{
enum Gatilho { gatInt = 0, gatMidi = 1, gatAudio = 2 };

// o bloco que o motor recebe: relógio por amostra já resolvido pela base
struct Bloco
{
    double sr = 44100.0;
    double bpm = 120.0;
    double compasso = 4.0;          // tempos por compasso
    double rate = 1.0;              // RATE em tempos (semínimas)
    float  valor = 0.5f;            // knob grandão, 0..1 (o motor suaviza)
    int    forma = 0;               // SHAPE
    int    gatilho = gatInt;
    const double* pos = nullptr;    // INT: posição em tempos na régua do DAW; MIDI/ÁUDIO: tempos desde o último disparo
    const uint8_t* disparo = nullptr; // 1 na amostra em que chegou nota/ataque (só MIDI/ÁUDIO)
};

class MotorBase
{
public:
    virtual ~MotorBase() = default;
    virtual void preparar (double sr) = 0;
    virtual void limpar() = 0;
    virtual void processar (float* L, float* R, int n, const Bloco& b) = 0;
    float nivel = 0.0f;   // 0..1: quanto o efeito está agindo agora (as lâmpadas respiram com ele)
};

using Linha = std::vector<std::pair<float, float>>;

struct Info
{
    const char* nome;           // PRODUCT_NAME
    const char* chave;          // fole, picador… (nome da plaquinha renderizada)
    uint32_t cor;               // cor do LED, 0xRRGGBB
    std::vector<const char*> formas;
    std::array<double, 7> rates;          // em tempos
    std::array<const char*, 7> rateNomes;
    int formaPadrao, ratePadrao;
    float valorPadrao;          // 0..10
    float mixPadrao;            // 0..100
    bool mixSoma;               // true: o seco fica inteiro e o MIX dosa o efeito (delay)
    std::vector<Linha> emblema; // traços em -1..1 (y para cima)
    // visor do SHAPE: quantos tempos ele mostra e a curva (0..1) num ponto x (em tempos) desse trecho
    double (*periodoVisor) (double rate, double compasso);
    float (*desenho) (int forma, float valor, double x, double rate, double compasso);
};

const Info& info();
std::unique_ptr<MotorBase> criarMotor();

// ---------------- utilidades de DSP comuns ----------------
inline float clamp01 (float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline double frac (double x) { return x - std::floor (x); }

// número pseudoaleatório estável a partir de um inteiro (o "aleatório" se repete igual a cada play)
inline float hash01 (int64_t k)
{
    uint64_t x = (uint64_t) k * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull;
    x ^= x >> 31; x *= 0xBF58476D1CE4E5B9ull; x ^= x >> 27; x *= 0x94D049BB133111EBull; x ^= x >> 31;
    return (float) ((x >> 40) & 0xFFFFFF) / 16777216.0f;
}

// suavizador de um polo (tempo em ms)
struct Suave
{
    float y = 0.0f, k = 0.01f;
    void ajustar (double sr, double ms) { k = (float) (1.0 - std::exp (-1.0 / (0.001 * ms * sr))); }
    float operator() (float alvo) { y += k * (alvo - y); return y; }
};

// filtro de estado variável (TPT/Zavalishin), estável com modulação rápida
struct Svf
{
    float ic1 = 0, ic2 = 0;
    void limpar() { ic1 = ic2 = 0; }
    // modo 0 = passa-baixa, 1 = passa-alta, 2 = passa-banda
    float processar (float x, float g, float k, int modo)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        if (modo == 0) return v2;
        if (modo == 1) return x - k * v1 - v2;
        return v1;
    }
    static float g (double fc, double sr) { return (float) std::tan (3.14159265358979 * std::min (fc, sr * 0.45) / sr); }
};

// buffer circular estéreo com leitura fracionária (Hermite)
struct Fita
{
    std::vector<float> l, r;
    int64_t escrito = 0;   // quantas amostras já entraram (posição absoluta)
    int tam = 1;
    void preparar (int amostras) { tam = std::max (4, amostras); l.assign ((size_t) tam, 0.0f); r.assign ((size_t) tam, 0.0f); escrito = 0; }
    void limpar() { std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f); }
    void escrever (float a, float b) { const int i = (int) (escrito % tam); l[(size_t) i] = a; r[(size_t) i] = b; ++escrito; }
    // posição absoluta (pode ser fracionária); fora do que existe devolve silêncio
    void ler (double p, float& a, float& b) const
    {
        if (p < (double) (escrito - tam + 4) || p > (double) (escrito - 1)) { a = b = 0.0f; return; }
        const int64_t i = (int64_t) std::floor (p);
        const float t = (float) (p - (double) i);
        auto h = [&] (const std::vector<float>& v)
        {
            auto at = [&] (int64_t k) { k = std::min (k, escrito - 1); return v[(size_t) (((k % tam) + tam) % tam)]; };
            const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * t + c2) * t + c1) * t + y1;
        };
        a = h (l); b = h (r);
    }
    void lerInt (int64_t p, float& a, float& b) const
    {
        if (p < escrito - tam || p >= escrito) { a = b = 0.0f; return; }
        const size_t i = (size_t) (((p % tam) + tam) % tam);
        a = l[i]; b = r[i];
    }
};

// divisões do RATE em tempos (semínima = 1)
constexpr double r32 = 0.125, r16T = 1.0 / 6.0, r16 = 0.25, r8T = 1.0 / 3.0, r8 = 0.5, r8P = 0.75, r4T = 2.0 / 3.0,
                 r4 = 1.0, r4P = 1.5, r2 = 2.0, c1 = 4.0, c2 = 8.0, c4 = 16.0, c8 = 32.0;

// traços dos emblemas (os mesmos da prévia no Blender)
inline Linha seno (float y0, float amp, float ciclos)
{
    Linha l;
    for (int k = 0; k <= 40; ++k) l.push_back ({ -0.9f + 1.8f * k / 40.0f, y0 + amp * std::sin (6.2831853f * ciclos * k / 40.0f) });
    return l;
}
inline Linha arco (float cx, float cy, float r, float a0, float a1, int n = 24)
{
    Linha l;
    for (int k = 0; k <= n; ++k)
    {
        const float a = (a0 + (a1 - a0) * k / (float) n) * 0.017453293f;
        l.push_back ({ cx + r * std::cos (a), cy + r * std::sin (a) });
    }
    return l;
}

// rampa de entrada/saída nas bordas de um trecho (evita clique), em amostras
inline float janelaBorda (double k, double comp, double borda)
{
    if (borda <= 0.0) return 1.0f;
    const double a = std::min (k / borda, (comp - k) / borda);
    return (float) std::min (1.0, std::max (0.0, a));
}
} // namespace sk
