// FOLE — sidechain sincronizado: o volume abaixa e volta no tempo, na forma escolhida.
// Knob: profundidade (0 = nada, 10 = some por inteiro no golpe). TRIGGER MIDI/ÁUDIO: cada nota/ataque dá um golpe.
#include "Motor.h"

namespace sk
{
namespace
{
constexpr double pi = 3.14159265358979;

// quanto o volume abaixa (1 = tudo) ao longo de um ciclo u = 0..1
float abaixa (int forma, double u)
{
    switch (forma)
    {
        case 0:   // CLÁSSICO: ataque curtinho e volta em curva
        {
            if (u < 0.015) return (float) (u / 0.015);
            const double x = (u - 0.015) / 0.6;
            return x >= 1.0 ? 0.0f : (float) ((1.0 - x) * (1.0 - x));
        }
        case 1: return u >= 0.9 ? 0.0f : (float) (0.5 + 0.5 * std::cos (pi * u / 0.9));            // SUAVE
        case 2:                                                                                      // BOMBA: segura e solta
        {
            if (u < 0.12) return 1.0f;
            const double x = (u - 0.12) / 0.5;
            return x >= 1.0 ? 0.0f : (float) std::pow (1.0 - x, 3.0);
        }
        case 3: return u < 0.4 ? 1.0f : 0.0f;                                                        // CORTE
        case 4: return (float) (1.0 - u);                                                            // RAMPA
        default: return (float) std::pow (u, 1.6);                                                   // INVERSO
    }
}

class Fole : public MotorBase
{
public:
    void preparar (double sr) override { sm.ajustar (sr, 0.6); valorSm.ajustar (sr, 20.0); limpar(); }
    void limpar() override { sm.y = 1.0f; }
    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        float soma = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = valorSm (b.valor);
            const double c = b.pos[i] / b.rate;
            float d;
            if (b.gatilho == gatInt) d = abaixa (b.forma, frac (c));
            else d = c >= 1.0 ? 0.0f : abaixa (b.forma, c);   // um golpe por disparo
            const float g = sm (1.0f - a * d);
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
        "FOLE", "fole", 0xff6b14,
        { "CLÁSSICO", "SUAVE", "BOMBA", "CORTE", "RAMPA", "INVERSO" },
        { r16, r8, r4, r2, c1, c2, c4 },
        { "1/16", "1/8", "1/4", "1/2", "1 COMP", "2 COMP", "4 COMP" },
        0, 2, 6.0f, 100.0f, false,
        // sanfona: duas tampas e as dobras do fole entre elas
        [] {
            std::vector<Linha> e { { { -0.9f, -0.8f }, { -0.9f, 0.8f } }, { { 0.9f, -0.8f }, { 0.9f, 0.8f } } };
            Linha cima, baixo;
            for (int k = 0; k < 7; ++k)
            {
                cima.push_back ({ -0.9f + 0.3f * k, k % 2 == 0 ? 0.45f : 0.7f });
                baixo.push_back ({ -0.9f + 0.3f * k, k % 2 == 0 ? -0.45f : -0.7f });
            }
            e.push_back (cima); e.push_back (baixo);
            for (int k = 1; k < 6; ++k)
                e.push_back ({ { -0.9f + 0.3f * k, k % 2 == 0 ? 0.45f : 0.7f }, { -0.9f + 0.3f * k, k % 2 == 0 ? -0.45f : -0.7f } });
            return e;
        }(),
        [] (double rate, double) { return 2.0 * rate; },
        [] (int forma, float valor, double x, double rate, double) { return 1.0f - valor * abaixa (forma, frac (x / rate)); }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Fole>(); }
} // namespace sk
