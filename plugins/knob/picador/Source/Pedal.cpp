// PICADOR — trance gate: o som é fatiado num padrão de 16 passos no tempo.
// Knob: até 6 é a profundidade do corte; de 6 a 10 o gate também fica mais curto (mais picado).
#include "Motor.h"

namespace sk
{
namespace
{
constexpr int padroes[7][16] = {
    { 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1 },   // TRANCE 1
    { 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 },   // TRANCE 2
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },   // RETO
    { 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1 },   // GALOPE
    { 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 },   // CONTRATEMPO
    { 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0 },   // TRESILLO
    { 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1 },   // QUEBRADO
};

float largura (float a) { return a <= 0.6f ? 0.85f : 0.85f - (a - 0.6f) / 0.4f * 0.5f; }

// 1 = aberto, 0 = fechado, num ponto x (em passos)
float gate (int forma, float a, double passos)
{
    const int k = (int) (((int64_t) std::floor (passos)) % 16 + 16) % 16;
    const bool aberto = padroes[forma][k] && frac (passos) < largura (a);
    const float prof = std::min (1.0f, a / 0.6f);
    return aberto ? 1.0f : 1.0f - prof;
}

class Picador : public MotorBase
{
public:
    void preparar (double sr) override { sm.ajustar (sr, 1.2); valorSm.ajustar (sr, 20.0); limpar(); }
    void limpar() override { sm.y = 1.0f; }
    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        float soma = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = valorSm (b.valor);
            const float g = sm (gate (b.forma, a, b.pos[i] / b.rate));
            L[i] *= g; R[i] *= g;
            soma += 1.0f - g;
        }
        nivel = soma / (float) n;
    }

private:
    Suave sm, valorSm;
};
} // namespace

const Info& info()
{
    static const Info i {
        "PICADOR", "picador", 0xa6ff2e,
        { "TRANCE 1", "TRANCE 2", "RETO", "GALOPE", "CONTRATEMPO", "TRESILLO", "QUEBRADO" },
        { r32, r16T, r16, r8T, r8, r4, r2 },
        { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/2" },
        1, 2, 6.0f, 100.0f, false,
        { { { -0.95f, -0.4f }, { -0.6f, -0.4f }, { -0.6f, 0.4f }, { -0.2f, 0.4f }, { -0.2f, -0.4f }, { 0.2f, -0.4f },
            { 0.2f, 0.4f }, { 0.6f, 0.4f }, { 0.6f, -0.4f }, { 0.95f, -0.4f } } },
        [] (double rate, double) { return 16.0 * rate; },
        [] (int forma, float valor, double x, double rate, double) { return 0.06f + 0.88f * gate (forma, valor, x / rate); }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Picador>(); }
} // namespace sk
