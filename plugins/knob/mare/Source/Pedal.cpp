// MARÉ — filtro passa-baixa que abre e fecha no tempo.
// Knob: o quanto o filtro desce (0 = parado e aberto, 10 = vai até ~30 Hz) e a ressonância junto.
#include "Motor.h"

namespace sk
{
namespace
{
constexpr double pi = 3.14159265358979;

// posição do filtro (1 = aberto) ao longo do ciclo; ciclo = índice do ciclo (para o ALEATÓRIO)
float onda (int forma, double u, int64_t ciclo)
{
    switch (forma)
    {
        case 0: return (float) (0.5 + 0.5 * std::cos (2.0 * pi * u));             // SENO (começa aberto)
        case 1: return (float) std::abs (2.0 * u - 1.0);                           // TRIÂNGULO
        case 2: return (float) u;                                                  // SOBE
        case 3: return (float) (1.0 - u);                                          // DESCE
        case 4: return u < 0.5 ? 1.0f : 0.0f;                                      // QUADRADO
        default:                                                                   // ALEATÓRIO: desliza entre sorteios
        {
            const float a = hash01 (ciclo), b = hash01 (ciclo + 1);
            const float t = (float) (0.5 - 0.5 * std::cos (pi * u));
            return a + (b - a) * t;
        }
    }
}

float posicao (int forma, double ciclos) { return onda (forma, frac (ciclos), (int64_t) std::floor (ciclos)); }

class Mare : public MotorBase
{
public:
    void preparar (double s) override { sr = s; corteSm.ajustar (s, 1.5); valorSm.ajustar (s, 30.0); limpar(); }
    void limpar() override { fL.limpar(); fR.limpar(); corteSm.y = 1.0f; }
    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        float soma = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = valorSm (b.valor);
            const float p = corteSm (posicao (b.forma, b.pos[i] / b.rate));
            const double fmax = 18000.0, fmin = fmax * std::pow (2.0, -9.2 * a);
            const double fc = fmin * std::pow (fmax / fmin, (double) p);
            const float g = Svf::g (fc, sr), k = 1.414f - 1.05f * a;
            L[i] = fL.processar (L[i], g, k, 0);
            R[i] = fR.processar (R[i], g, k, 0);
            soma += a * (1.0f - p);
        }
        nivel = soma / (float) n;
    }

private:
    double sr = 44100.0;
    Svf fL, fR;
    Suave corteSm, valorSm;
};
} // namespace

const Info& info()
{
    static const Info i {
        "MARE", "mare", 0x3fc8ff,
        { "SENO", "TRIÂNGULO", "SOBE", "DESCE", "QUADRADO", "ALEATÓRIO" },
        { r16, r8, r4, r2, c1, c2, c4 },
        { "1/16", "1/8", "1/4", "1/2", "1 COMP", "2 COMP", "4 COMP" },
        0, 5, 6.5f, 100.0f, false,
        { seno (0.35f, 0.22f, 1.5f), seno (-0.35f, 0.22f, 1.5f) },
        [] (double rate, double) { return 2.0 * rate; },
        [] (int forma, float valor, double x, double rate, double) { return 1.0f - valor * (1.0f - posicao (forma, x / rate)); }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Mare>(); }
} // namespace sk
