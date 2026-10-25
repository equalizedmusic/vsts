// FREIO — tape stop / tape start no tempo: a fita perde velocidade até parar no fim do ciclo e/ou arranca no começo.
// Knob: quanto do ciclo a freada (ou a partida) ocupa (0 = nada, 10 = metade do ciclo).
// TRIGGER MIDI/ÁUDIO: a freada começa na nota/ataque e depois a fita volta ao vivo.
#include "Motor.h"

namespace sk
{
namespace
{
enum { parada, partida, paraVolta, meia };

float zona (int forma, float a) { return forma == paraVolta ? a * 0.25f : a * 0.5f; }

// velocidade da fita (1 = normal, 0 = parada) num ponto u do ciclo
float velocidade (int forma, double u, float a)
{
    const double z = zona (forma, a);
    if (z < 1.0e-4) return 1.0f;
    switch (forma)
    {
        case parada:    return u >= 1.0 - z ? (float) (1.0 - (u - (1.0 - z)) / z) : 1.0f;
        case partida:   return u < z ? (float) (u / z) : 1.0f;
        case paraVolta: return u >= 1.0 - z ? (float) (1.0 - (u - (1.0 - z)) / z) : (u < z ? (float) (u / z) : 1.0f);
        default:        return u >= 1.0 - z ? (float) (1.0 - 0.5 * (u - (1.0 - z)) / z) : 1.0f;   // MEIA VELOCIDADE
    }
}

class Freio : public MotorBase
{
public:
    void preparar (double s) override
    {
        sr = s;
        fita.preparar ((int) (20.0 * s));
        xfTam = (int) (0.008 * s);
        limpar();
    }
    void limpar() override { fita.limpar(); lag = lagVelho = 0.0; xf = 0; etapaAtual = INT64_MIN; gVelho = 1.0f; }

    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        float soma = 0.0f;
        const float a = b.valor;
        const double z = zona (b.forma, a);
        for (int i = 0; i < n; ++i)
        {
            fita.escrever (L[i], R[i]);
            if (b.disparo[i]) ++disparos;
            const double c = b.pos[i] / b.rate;
            float s = 1.0f;
            int64_t etapa;
            if (b.gatilho == gatInt)
            {
                etapa = (int64_t) std::floor (c);
                s = velocidade (b.forma, frac (c), a);
            }
            else
            {
                // disparo: a freada começa já; depois (PARA E VOLTA) a partida; depois volta ao vivo
                const bool temParada = b.forma != partida;
                double u = temParada ? (1.0 - z) + c : c;
                int parte = 0;
                if (temParada && u >= 1.0) { u -= 1.0; parte = 1; }
                if (! temParada) parte = 1;
                const bool temPartida = b.forma == partida || b.forma == paraVolta;
                if (parte == 1 && (! temPartida || u >= z)) parte = 2;
                s = parte == 2 ? 1.0f : velocidade (b.forma, u, a);
                etapa = disparos * 4 + parte;
            }
            if (etapa != etapaAtual)
            {
                etapaAtual = etapa;
                if (lag > 0.5 || xf > 0) { lagVelho = lag; gVelho = ganhoAtual; xf = xfTam; }
                lag = 0.0;
            }
            lag = std::min (lag + (1.0 - s), (double) fita.tam - 16.0);
            const float g = b.forma == meia ? 1.0f : std::min (1.0f, s * 2.5f);
            ganhoAtual = g;
            float l, r;
            fita.ler ((double) (fita.escrito - 1) - lag, l, r);
            l *= g; r *= g;
            if (xf > 0)
            {
                const float w = (float) xf / (float) xfTam;
                float vl, vr;
                fita.ler ((double) (fita.escrito - 1) - lagVelho, vl, vr);
                l = l * (1.0f - w) + vl * gVelho * w;
                r = r * (1.0f - w) + vr * gVelho * w;
                --xf;
            }
            L[i] = l; R[i] = r;
            soma += 1.0f - s;
        }
        nivel = soma / (float) n;
    }

private:
    double sr = 44100.0, lag = 0.0, lagVelho = 0.0;
    Fita fita;
    int xf = 0, xfTam = 256;
    int64_t etapaAtual = INT64_MIN, disparos = 0;
    float gVelho = 1.0f, ganhoAtual = 1.0f;
};
} // namespace

const Info& info()
{
    static const Info i {
        "FREIO", "freio", 0xe8f4ff,
        { "PARADA", "PARTIDA", "PARA E VOLTA", "MEIA VELOCIDADE" },
        { r8, r4, r2, c1, c2, c4, c8 },
        { "1/8", "1/4", "1/2", "1 COMP", "2 COMP", "4 COMP", "8 COMP" },
        0, 3, 3.0f, 100.0f, false,
        { [] { Linha l; for (int k = 0; k <= 30; ++k) l.push_back ({ -0.9f + 1.8f * k / 30.0f, 0.65f - 1.35f * std::pow (k / 30.0f, 2.6f) }); return l; }(),
          { { -0.9f, -0.75f }, { 0.9f, -0.75f } } },
        [] (double rate, double) { return 2.0 * rate; },
        [] (int forma, float valor, double x, double rate, double) { return 0.08f + 0.84f * velocidade (forma, frac (x / rate), valor); }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Freio>(); }
} // namespace sk
