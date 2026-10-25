// AVESSO — reverso no tempo: fatias do compasso tocam ao contrário (a fatia anterior, de trás para a frente).
// Knob: quantas fatias do compasso viram (0 = nenhuma, 10 = todas). SHAPE: quais viram primeiro.
// TRIGGER MIDI/ÁUDIO: o "compasso" começa na nota/ataque e só ele é virado.
#include "Motor.h"

namespace sk
{
namespace
{
enum { alternado, fim, inicio, aleatorio };

int fatiasNoCompasso (double rate, double comp) { return std::max (1, (int) std::floor (comp / rate + 1.0e-6)); }

// a fatia k (de N) do compasso c vira?
bool vira (int forma, float a, int k, int N, int64_t c)
{
    const int n = (int) std::lround (a * N);
    if (n <= 0) return false;
    if (forma == aleatorio) return hash01 (c * 131 + k) < (float) n / (float) N;
    int ordem;
    if (forma == fim) ordem = N - 1 - k;
    else if (forma == inicio) ordem = k;
    else ordem = (k % 2 == 1) ? k / 2 : (N / 2) + k / 2;   // ímpares primeiro (contratempo), depois os pares
    return ordem < n;
}

class Avesso : public MotorBase
{
public:
    void preparar (double s) override
    {
        sr = s;
        fita.preparar ((int) (8.0 * s));
        limpar();
    }
    void limpar() override { fita.limpar(); fatiaAtual = -1; viraAtual = false; }

    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        const double spb = sr * 60.0 / b.bpm;
        const int N = fatiasNoCompasso (b.rate, b.compasso);
        const int64_t Ls = std::max ((int64_t) 64, (int64_t) std::llround (b.rate * spb));
        const double borda = 0.003 * sr;
        float soma = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            fita.escrever (L[i], R[i]);
            const int64_t agora = fita.escrito - 1;
            const double pos = b.pos[i];
            if (b.disparo[i]) ++disparos;
            const int64_t fatia = (int64_t) std::floor (pos / b.rate);
            const int64_t id = b.gatilho == gatInt ? fatia : fatia + disparos * 1000003;
            if (id != fatiaAtual)
            {
                fatiaAtual = id;
                inicioFatia = agora;
                const int k = (int) (((fatia % N) + N) % N);
                const int64_t c = (int64_t) std::floor (fatia / (double) N);
                viraAtual = vira (b.forma, b.valor, k, N, c + (b.gatilho == gatInt ? 0 : disparos * 7919));
                if (b.gatilho != gatInt && fatia >= N) viraAtual = false;   // disparo: só o primeiro "compasso"
            }
            float l = L[i], r = R[i];
            if (viraAtual)
            {
                const int64_t j = agora - inicioFatia;
                if (j < Ls)
                {
                    float vl, vr;
                    fita.lerInt (inicioFatia - 1 - j, vl, vr);
                    const float w = janelaBorda ((double) j, (double) Ls, borda);
                    l = l * (1.0f - w) + vl * w;
                    r = r * (1.0f - w) + vr * w;
                    soma += w;
                }
            }
            L[i] = l; R[i] = r;
        }
        nivel = soma / (float) n;
    }

private:
    double sr = 44100.0;
    Fita fita;
    int64_t fatiaAtual = -1, inicioFatia = 0, disparos = 0;
    bool viraAtual = false;
};
} // namespace

const Info& info()
{
    static const Info i {
        "AVESSO", "avesso", 0xa66bff,
        { "ALTERNADO", "FIM DO COMP.", "INÍCIO", "ALEATÓRIO" },
        { r16, r8T, r8, r4T, r4, r2, c1 },
        { "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 COMP" },
        0, 2, 5.0f, 100.0f, false,
        { arco (0.0f, 0.0f, 0.72f, 110.0f, -200.0f), { { -0.45f, 0.85f }, { -0.25f, 0.68f }, { -0.5f, 0.45f } } },
        [] (double, double comp) { return comp; },
        [] (int forma, float valor, double x, double rate, double comp)
        {
            const int N = fatiasNoCompasso (rate, comp);
            const int k = std::min (N - 1, (int) std::floor (x / rate));
            const float u = (float) frac (x / rate);
            return vira (forma, valor, k, N, 0) ? 0.08f + 0.85f * std::pow (u, 1.6f) : 0.08f + 0.85f * std::exp (-3.5f * u);
        }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Avesso>(); }
} // namespace sk
