// MASTIGA — bitcrush ritmado: drive, redução de bits e de taxa de amostragem pulsando no tempo.
// Knob: o quanto morde no pico da forma (0 = limpo, 10 = 3 bits, taxa /32 e drive alto).
// TRIGGER MIDI/ÁUDIO: a forma recomeça na nota/ataque.
#include "Motor.h"

namespace sk
{
namespace
{
constexpr double pi = 3.14159265358979;

float forma01 (int forma, double ciclos)
{
    const double u = frac (ciclos);
    switch (forma)
    {
        case 0: return u < 0.5 ? 1.0f : 0.0f;                                      // QUADRADO
        case 1: return (float) (0.5 - 0.5 * std::cos (2.0 * pi * u));             // SENO
        case 2: return (float) u;                                                  // SOBE
        case 3: return (float) (1.0 - u);                                          // DESCE
        case 4: return (float) std::floor (u * 4.0) / 3.0f;                        // DEGRAUS
        case 5: return u < 0.15 ? 1.0f : 0.0f;                                     // PULSO
        default: return hash01 ((int64_t) std::floor (ciclos)) > 0.45f ? hash01 ((int64_t) std::floor (ciclos) + 77) : 0.0f;  // ALEATÓRIO
    }
}

class Mastiga : public MotorBase
{
public:
    void preparar (double) override { limpar(); }
    void limpar() override { fase = 1.0; hL = hR = 0.0f; }
    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        if (b.sr != srAjustado) { cSm.ajustar (b.sr, 2.0); srAjustado = b.sr; }
        float soma = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float c = cSm (b.valor * forma01 (b.forma, b.pos[i] / b.rate));
            const float bits = 16.0f - 13.0f * c;
            const float niveis = std::pow (2.0f, bits - 1.0f);
            const double passo = 1.0 / (1.0 + 31.0 * (double) (c * c));
            const float drive = 1.0f + 5.0f * c;
            fase += passo;
            if (fase >= 1.0)
            {
                fase -= std::floor (fase);
                auto morde = [&] (float x) { return std::round (std::tanh (x * drive) / std::pow (drive, 0.75f) * niveis) / niveis; };
                hL = morde (L[i]); hR = morde (R[i]);
            }
            const float w = std::min (1.0f, c * 3.0f);
            L[i] = L[i] * (1.0f - w) + hL * w;
            R[i] = R[i] * (1.0f - w) + hR * w;
            soma += c;
        }
        nivel = soma / (float) n;
    }

private:
    double fase = 1.0, srAjustado = 0.0;
    float hL = 0.0f, hR = 0.0f;
    Suave cSm;
};
} // namespace

const Info& info()
{
    static const Info i {
        "MASTIGA", "mastiga", 0x5a74ff,
        { "QUADRADO", "SENO", "SOBE", "DESCE", "DEGRAUS", "PULSO", "ALEATÓRIO" },
        { r32, r16, r8, r4, r2, c1, c2 },
        { "1/32", "1/16", "1/8", "1/4", "1/2", "1 COMP", "2 COMP" },
        0, 2, 8.5f, 100.0f, false,
        [] {
            Linha a, b;
            for (int k = 0; k < 6; ++k)
            {
                a.push_back ({ -0.9f + 0.36f * k, k % 2 == 0 ? 0.15f : 0.62f });
                b.push_back ({ -0.9f + 0.36f * k, k % 2 == 0 ? -0.15f : -0.62f });
            }
            return std::vector<Linha> { a, b };
        }(),
        [] (double rate, double) { return 2.0 * rate; },
        [] (int forma, float valor, double x, double rate, double) { return 0.08f + 0.84f * valor * forma01 (forma, x / rate); }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Mastiga>(); }
} // namespace sk
