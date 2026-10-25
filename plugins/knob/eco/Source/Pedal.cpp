// ECO — delay no tempo com sabor de fita. O seco passa inteiro; o MIX dosa as repetições.
// Knob: a realimentação (0 = uma repetição só, 10 = quase infinito, no DUB satura e não para).
// TRIGGER MIDI/ÁUDIO: "dub throw" — só entra no delay o que soar logo depois da nota/ataque.
#include "Motor.h"

namespace sk
{
namespace
{
enum { simples, pingpong, fitaF, dub, espalhado };
constexpr double pi = 3.14159265358979;

float realim (int forma, float a) { return forma == dub ? a * 1.02f : a * 0.93f; }

class Eco : public MotorBase
{
public:
    void preparar (double s) override
    {
        sr = s;
        linha.preparar ((int) (6.0 * s));
        tempoSm.ajustar (s, 120.0);
        envia.ajustar (s, 4.0);
        valorSm.ajustar (s, 30.0);
        limpar();
    }
    void limpar() override
    {
        linha.limpar();
        for (auto* f : { &pbL, &pbR, &paL, &paR }) f->limpar();
        tempoSm.y = 0.0f; fase = 0.0; env = 0.0f;
    }
    void processar (float* L, float* R, int n, const Bloco& b) override
    {
        const double spb = sr * 60.0 / b.bpm;
        const float alvo = (float) std::min (b.rate * spb, (double) linha.tam - 16.0);
        if (tempoSm.y <= 0.0f) tempoSm.y = alvo;
        // filtros na volta: cada modo escurece de um jeito
        double fcLp = 12000.0, fcHp = 40.0;
        if (b.forma == fitaF) { fcLp = 4500.0; fcHp = 120.0; }
        else if (b.forma == dub) { fcLp = 2400.0; fcHp = 300.0; }
        else if (b.forma == pingpong) fcLp = 9000.0;
        const float gLp = Svf::g (fcLp, sr), gHp = Svf::g (fcHp, sr);
        const bool satura = b.forma == fitaF || b.forma == dub;
        for (int i = 0; i < n; ++i)
        {
            const float a = valorSm (b.valor);
            const float fb = realim (b.forma, a);
            float D = tempoSm (alvo);
            if (b.forma == fitaF)   // wow & flutter
            {
                fase += 1.0 / sr;
                D *= (float) (1.0 + 0.0025 * std::sin (2.0 * pi * 0.55 * fase) + 0.0009 * std::sin (2.0 * pi * 6.3 * fase));
            }
            const float DR = b.forma == espalhado ? D * 1.3333f : D;
            float yL, yR, t;
            linha.ler ((double) linha.escrito - D, yL, t);
            linha.ler ((double) linha.escrito - DR, t, yR);
            yL = paL.processar (pbL.processar (yL, gLp, 1.0f, 0), gHp, 1.0f, 1);
            yR = paR.processar (pbR.processar (yR, gLp, 1.0f, 0), gHp, 1.0f, 1);

            const bool aberto = b.gatilho == gatInt || b.pos[i] < std::max (b.rate * 2.0, 0.5);
            const float s = envia (aberto ? 1.0f : 0.0f);
            float inL = L[i] * s, inR = R[i] * s;
            float fL = yL * fb, fR = yR * fb;
            if (satura) { fL = std::tanh (fL * 1.2f) / 1.2f; fR = std::tanh (fR * 1.2f) / 1.2f; }
            else { fL = std::tanh (fL); fR = std::tanh (fR); }
            if (b.forma == pingpong) linha.escrever (0.5f * (inL + inR) + fR, fL);
            else linha.escrever (inL + fL, inR + fR);

            L[i] = yL; R[i] = yR;   // só as repetições: o processor soma com o seco
            env = std::max (std::abs (yL), env * 0.9995f);
        }
        nivel = std::min (1.0f, env * 2.0f);
    }

private:
    double sr = 44100.0, fase = 0.0;
    Fita linha;
    Svf pbL, pbR, paL, paR;
    Suave tempoSm, envia, valorSm;
    float env = 0.0f;
};
} // namespace

const Info& info()
{
    static const Info i {
        "ECO", "eco", 0xffd23a,
        { "SIMPLES", "PING-PONG", "FITA", "DUB", "ESPALHADO" },
        { r16, r8T, r8, r8P, r4, r4P, r2 },
        { "1/16", "1/8T", "1/8", "3/16", "1/4", "3/8", "1/2" },
        1, 3, 5.5f, 40.0f, true,
        { arco (-0.75f, 0.0f, 0.45f, -42.0f, 42.0f), arco (-0.75f, 0.0f, 0.95f, -42.0f, 42.0f),
          arco (-0.75f, 0.0f, 1.45f, -42.0f, 42.0f), arco (-0.75f, 0.0f, 0.12f, 0.0f, 360.0f, 16) },
        [] (double rate, double) { return std::max (8.0 * rate, 2.0); },
        [] (int forma, float valor, double x, double rate, double)
        {
            const int k = (int) std::floor (x / rate);
            const double u = frac (x / rate);
            const float fb = std::min (realim (forma, valor), 0.98f);
            return 0.05f + 0.9f * std::pow (fb, (float) k) * std::exp (-9.0f * (float) u) * (k == 0 ? 1.0f : 0.85f);
        }
    };
    return i;
}

std::unique_ptr<MotorBase> criarMotor() { return std::make_unique<Eco>(); }
} // namespace sk
