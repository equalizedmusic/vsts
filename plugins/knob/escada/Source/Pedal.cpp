// ESCADA — sequenciador de pitch: a cada passo (RATE) o som muda de nota seguindo o padrão.
// Knob: o tamanho dos saltos (0 = sem mudança, 10 = o padrão inteiro). Os saltos ficam sempre dentro da escala/acorde.
// TRIGGER MIDI/ÁUDIO: o padrão recomeça na nota/ataque.
#include "Motor.h"

namespace sk
{
namespace
{
constexpr double pi = 3.14159265358979;

struct Padrao
{
    std::vector<int> notas;   // tons disponíveis (semitons), com o 0 na posição "zero"
    int zero;
    std::vector<int> passos;  // índices relativos ao zero
};

const std::vector<Padrao>& padroes()
{
    static const std::vector<Padrao> p {
        // pentatônica menor
        { { -12, -10, -7, -5, -2, 0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24 }, 5, { 0, 1, 2, 3, 4, 5, 6, 7 } },                               // SOBE
        { { -24, -22, -19, -17, -14, -12, -10, -7, -5, -2, 0, 3, 5, 7 }, 10, { 0, -1, -2, -3, -4, -5, -6, -7 } },                           // DESCE
        { { -12, -10, -7, -5, -2, 0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24 }, 5, { 0, 1, 2, 3, 4, 5, 6, 7, 5, 4, 3, 2, 1, 0, -1, 0 } },        // SOBE E DESCE
        { { -24, -12, 0, 12, 24 }, 2, { 0, 1, 0, -1, 0, 1, 2, 1 } },                                                                        // OITAVAS
        { { -12, -8, -5, 0, 4, 7, 12, 16, 19, 24 }, 3, { 0, 1, 2, 3, 0, 1, 2, 3 } },                                                        // ARPEJO MAIOR
        { { -12, -9, -5, 0, 3, 7, 12, 15, 19, 24 }, 3, { 0, 1, 2, 3, 0, 1, 2, 3 } },                                                        // ARPEJO MENOR
        { { -12, -5, 0, 7, 12, 19, 24 }, 2, { 0, 1, 0, 1, 2, 1, 0, -1 } },                                                                  // QUINTAS
        { { -12, -10, -7, -5, -2, 0, 3, 5, 7, 10, 12, 15, 17 }, 5, {} },                                                                    // ALEATÓRIO
    };
    return p;
}

// semitons no passo k (o knob encolhe os saltos para dentro da mesma lista de notas)
float semitons (int forma, float a, int64_t k)
{
    const auto& p = padroes()[(size_t) forma];
    int idx;
    if (p.passos.empty()) idx = (int) std::lround ((hash01 (k) * 2.0f - 1.0f) * 5.0f);
    else
    {
        const int64_t m = (int64_t) p.passos.size();
        idx = p.passos[(size_t) (((k % m) + m) % m)];
    }
    idx = (int) std::lround (idx * a);
    idx = std::max (-p.zero, std::min ((int) p.notas.size() - 1 - p.zero, idx));
    return (float) p.notas[(size_t) (p.zero + idx)];
}

class Escada : public MotorBase
{
public:
    void preparar (double s) override
    {
        sr = s;
        fita.preparar ((int) (0.5 * s));
        janela = 0.045 * s;
        stSm.ajustar (s, 4.0);
        secoSm.ajustar (s, 8.0);
        limpar();
    }
    void limpar() override { fita.limpar(); fase = 0.0; stSm.y = 0.0f; secoSm.y = 1.0f; }

    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        float soma = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            fita.escrever (L[i], R[i]);
            const int64_t k = (int64_t) std::floor (b.pos[i] / b.rate);
            const float alvo = semitons (b.forma, b.valor, k);
            const float st = stSm (alvo);
            // sem transposição o pitch shifter sai do caminho (senão as duas cabeças coloririam o som)
            const float seco = secoSm (std::abs (alvo) < 0.01f ? 1.0f : 0.0f);
            const double razao = std::pow (2.0, st / 12.0);
            fase = frac (fase + (1.0 - razao) / janela);
            const double f2 = frac (fase + 0.5);
            float l1, r1, l2, r2;
            fita.ler ((double) (fita.escrito - 2) - fase * janela, l1, r1);
            fita.ler ((double) (fita.escrito - 2) - f2 * janela, l2, r2);
            const float g1 = (float) (0.5 - 0.5 * std::cos (2.0 * pi * fase)), g2 = 1.0f - g1;
            const float pl = l1 * g1 + l2 * g2, pr = r1 * g1 + r2 * g2;
            L[i] = L[i] * seco + pl * (1.0f - seco);
            R[i] = R[i] * seco + pr * (1.0f - seco);
            soma += std::min (1.0f, std::abs (st) / 12.0f);
        }
        nivel = soma / (float) n;
    }

private:
    double sr = 44100.0, janela = 2000.0, fase = 0.0;
    Fita fita;
    Suave stSm, secoSm;
};
} // namespace

const Info& info()
{
    static const Info i {
        "ESCADA", "escada", 0x2effc0,
        { "SOBE", "DESCE", "SOBE E DESCE", "OITAVAS", "ARPEJO MAIOR", "ARPEJO MENOR", "QUINTAS", "ALEATÓRIO" },
        { r32, r16, r8T, r8, r4, r2, c1 },
        { "1/32", "1/16", "1/8T", "1/8", "1/4", "1/2", "1 COMP" },
        0, 3, 7.0f, 100.0f, false,
        { { { -0.9f, -0.7f }, { -0.45f, -0.7f }, { -0.45f, -0.25f }, { 0.0f, -0.25f }, { 0.0f, 0.2f }, { 0.45f, 0.2f },
            { 0.45f, 0.65f }, { 0.9f, 0.65f } } },
        [] (double rate, double) { return 16.0 * rate; },
        [] (int forma, float valor, double x, double rate, double)
        { return 0.5f + semitons (forma, valor, (int64_t) std::floor (x / rate)) / 52.0f; }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Escada>(); }
} // namespace sk
