// VAIVÉM — auto-pan no tempo: o som passeia de um lado para o outro na forma escolhida.
// Knob: a abertura (0 = parado no meio, 10 = vai de um extremo ao outro).
#include "Motor.h"

namespace sk
{
namespace
{
constexpr double pi = 3.14159265358979;

// -1 (esquerda) .. +1 (direita)
float lado (int forma, double ciclos)
{
    const double u = frac (ciclos);
    switch (forma)
    {
        case 0: return (float) std::sin (2.0 * pi * u);                                    // SENO
        case 1: return (float) (u < 0.25 ? 4.0 * u : u < 0.75 ? 2.0 - 4.0 * u : 4.0 * u - 4.0);  // TRIÂNGULO
        case 2: return u < 0.5 ? -1.0f : 1.0f;                                              // QUADRADO
        case 3: return (float) (2.0 * u - 1.0);                                             // SOBE (varre e volta)
        default: return hash01 ((int64_t) std::floor (ciclos * 4.0)) * 2.0f - 1.0f;         // ALEATÓRIO: pula a cada 1/4 do ciclo
    }
}

class Vaivem : public MotorBase
{
public:
    void preparar (double sr) override { panSm.ajustar (sr, 3.0); valorSm.ajustar (sr, 20.0); limpar(); }
    void limpar() override { panSm.y = 0.0f; }
    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        float soma = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = valorSm (b.valor);
            const float p = panSm (lado (b.forma, b.pos[i] / b.rate) * a);
            const float l = L[i], r = R[i];
            // o lado que some passa para o outro (com compensação: o volume total fica parecido)
            if (p >= 0.0f) { L[i] = l * (1.0f - p); R[i] = (r + 0.7071f * l * p) / (1.0f + 0.2929f * p); }
            else           { const float q = -p; R[i] = r * (1.0f - q); L[i] = (l + 0.7071f * r * q) / (1.0f + 0.2929f * q); }
            soma += std::abs (p);
        }
        nivel = soma / (float) n;
    }

private:
    Suave panSm, valorSm;
};
} // namespace

const Info& info()
{
    static const Info i {
        "VAIVEM", "vaivem", 0xff3fb4,
        { "SENO", "TRIÂNGULO", "QUADRADO", "SOBE", "ALEATÓRIO" },
        { r16, r8T, r8, r4, r2, c1, c2 },
        { "1/16", "1/8T", "1/8", "1/4", "1/2", "1 COMP", "2 COMP" },
        1, 4, 8.0f, 100.0f, false,
        { { { -0.9f, 0.0f }, { 0.9f, 0.0f } }, { { -0.5f, 0.38f }, { -0.9f, 0.0f }, { -0.5f, -0.38f } },
          { { 0.5f, 0.38f }, { 0.9f, 0.0f }, { 0.5f, -0.38f } } },
        [] (double rate, double) { return 2.0 * rate; },
        [] (int forma, float valor, double x, double rate, double) { return 0.5f + 0.46f * valor * lado (forma, x / rate); }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Vaivem>(); }
} // namespace sk
