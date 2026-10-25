// REPIQUE — stutter / beat repeat no tempo: o fim de cada compasso vira repetição de uma fatia.
// Knob: quanto do compasso repete (0 = nada, 10 = o compasso inteiro). RATE = tamanho da fatia.
// TRIGGER MIDI/ÁUDIO: a repetição começa na nota/ataque e dura o mesmo tanto.
#include "Motor.h"

namespace sk
{
namespace
{
enum { roll, repete, decai, trava };

int fatiasNaRegiao (float a, double rate, double comp)
{
    const double total = std::max (1.0, std::floor (comp / rate + 1.0e-6));
    return (int) std::lround (a * total);
}

class Repique : public MotorBase
{
public:
    void preparar (double s) override
    {
        sr = s;
        fita.preparar ((int) (12.0 * s));
        mixSm.ajustar (s, 2.0);
        limpar();
    }
    void limpar() override { fita.limpar(); emRegiao = false; mixSm.y = 0.0f; }

    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        const double spb = sr * 60.0 / b.bpm;   // amostras por tempo
        const int nReg = fatiasNaRegiao (b.valor, b.rate, b.compasso);
        const double regB = nReg * b.rate;
        const double borda = 0.002 * sr;
        float soma = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            fita.escrever (L[i], R[i]);
            const int64_t agora = fita.escrito - 1;
            const double pos = b.pos[i];
            bool dentro;
            double desdeInicioB, posCompB = 0.0;
            if (b.disparo[i]) ++idDisparo;
            int64_t id = idDisparo;   // cada compasso (ou cada disparo) é uma região nova
            if (b.gatilho == gatInt)
            {
                id = (int64_t) std::floor (pos / b.compasso);
                posCompB = pos - (double) id * b.compasso;
                const double ini = b.compasso - regB;
                dentro = nReg > 0 && posCompB >= ini - 1.0e-9;
                desdeInicioB = posCompB - ini;
            }
            else
            {
                dentro = nReg > 0 && pos < regB;
                desdeInicioB = pos;
            }

            if (dentro && (! emRegiao || id != idRegiao))
            {
                emRegiao = true;
                idRegiao = id;
                sReg = agora;
                sCap = agora;
                if (b.forma == trava && b.gatilho == gatInt)   // repete o começo do compasso
                    sCap = std::max (agora - (int64_t) std::llround (posCompB * spb), fita.escrito - fita.tam + 8);
            }
            else if (! dentro)
                emRegiao = false;

            float sl = 0.0f, sr_ = 0.0f;
            if (emRegiao)
            {
                const int64_t k = agora - sReg;
                double fatiaB = b.rate;
                int64_t ini = 0;
                if (b.forma == roll)
                {
                    const double q = regB > 0.0 ? desdeInicioB / regB : 0.0;
                    const int seg = std::min (3, std::max (0, (int) std::floor (q * 4.0)));
                    fatiaB = b.rate / (double) (1 << seg);
                    ini = (int64_t) std::llround (seg * regB * spb / 4.0);
                }
                const int64_t Ls = std::max ((int64_t) (0.004 * sr), (int64_t) std::llround (fatiaB * spb));
                const int64_t rel = std::max ((int64_t) 0, k - ini);
                const int64_t j = rel % Ls, rep = rel / Ls;
                fita.lerInt (sCap + j, sl, sr_);
                float g = janelaBorda ((double) j, (double) Ls, borda);
                if (b.forma == decai) g *= std::pow (0.7f, (float) rep);
                sl *= g; sr_ *= g;
            }
            const float m = mixSm (emRegiao ? 1.0f : 0.0f);
            L[i] = L[i] * (1.0f - m) + sl * m;
            R[i] = R[i] * (1.0f - m) + sr_ * m;
            soma += m;
        }
        nivel = soma / (float) n;
    }

private:
    double sr = 44100.0;
    Fita fita;
    bool emRegiao = false;
    int64_t sReg = 0, sCap = 0, idRegiao = -1, idDisparo = 0;
    Suave mixSm;
};

float desenho (int forma, float a, double x, double rate, double comp)
{
    const int nReg = fatiasNaRegiao (a, rate, comp);
    const double regB = nReg * rate, ini = comp - regB;
    if (nReg <= 0 || x < ini) return 0.3f;
    const double d = x - ini;
    double fatia = rate, base = 0.0;
    if (forma == roll)
    {
        const int seg = std::min (3, (int) std::floor (d / regB * 4.0));
        fatia = rate / (double) (1 << seg);
        base = seg * regB / 4.0;
    }
    const double rel = d - base, u = frac (rel / fatia);
    float y = 0.12f + 0.82f * std::exp (-3.0f * (float) u);
    if (forma == decai) y = 0.12f + (y - 0.12f) * std::pow (0.7f, (float) std::floor (rel / fatia));
    return y;
}
} // namespace

const Info& info()
{
    static const Info i {
        "REPIQUE", "repique", 0xff8c1a,
        { "ROLL", "REPETE", "DECAI", "TRAVA" },
        { r32, r16T, r16, r8T, r8, r4, r2 },
        { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/2" },
        0, 2, 4.0f, 100.0f, false,
        { { { -0.8f, -0.7f }, { -0.8f, 0.8f } }, { { -0.4f, -0.7f }, { -0.4f, 0.45f } }, { { 0.0f, -0.7f }, { 0.0f, 0.15f } },
          { { 0.4f, -0.7f }, { 0.4f, -0.1f } }, { { 0.8f, -0.7f }, { 0.8f, -0.3f } } },
        [] (double, double comp) { return comp; },
        desenho
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Repique>(); }
} // namespace sk
