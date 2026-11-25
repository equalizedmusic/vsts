// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#pragma once

// DUPLA (0.4.1): o BAREBASS faz, sozinho, o que o ALIGN faria no canal dele quando o CHUTE está no projeto (LINK).
// C++ puro (sem JUCE), para o tools/teste_dupla.cpp medir fora do plugin.
//  - KICK LOCK ligado: a saída do synth passa pelo MESMO motor do ALIGN (align::Engine, do ../../align/Source/Dsp.h),
//    com o corpo do kick recriado pelo LINK no lugar do sidechain: fase e polaridade aprendidas nota a nota (banco de
//    allpass, latência zero) e o LEVEL GUARD. Os padrões são os do ALIGN (PHASE, POLARITY, GUARD 100 %, REACT MID).
//    Junto com isso continua o nascimento em fase (noteOn com a fase do kick), que é do synth.
//  - DUCK: a mesma conta do DUCK do ALIGN (envelope do kick inteiro, ataque na hora, solta em 40 ms, relativo ao pico
//    de 2 s; ganho com ataque de 1 ms e soltura de 12 ms), aplicada no baixo inteiro. Vale com o KICK LOCK ligado ou
//    não, sempre que houver kick do LINK. Em 0 não faz nada.
//  - Sem kick (sem CHUTE no projeto): o motor do ALIGN fica desligado e devolve o synth sem nenhuma alteração.
//
// O motor do ALIGN tem o DUCK dele, mas lá o DUCK só existe com o corretor ligado (wet). Aqui o DUCK é separado para
// funcionar também com o KICK LOCK desligado, sem passar o baixo pelo crossover do corretor (que giraria a fase).

#include "../../align/Source/Dsp.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

namespace barebass
{
class Dupla
{
public:
    // o que a tela lê
    std::atomic<float> duckDb { 0.0f };   // DUCK: menor ganho do último bloco (dB, ≤ 0)

    Dupla() : alinha (std::make_unique<align::Engine>()) {}

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        alinha->usaFase = true; alinha->usaPolaridade = true; alinha->guard = true;
        alinha->guardDepth = 1.0; alinha->xoMax = 110.0; alinha->duck = 0.0;
        alinha->setReact (1);
        alinha->prepare (fs);
        dRel = std::exp (-1.0 / (0.040 * fs)); dPicoC = std::exp (-1.0 / (2.0 * fs));
        dAtk = std::exp (-1.0 / (0.001 * fs)); dSolta = std::exp (-1.0 / (0.012 * fs));
        dEnv = 0; dPico = 0; gd = 1.0;
    }

    // out: nch canais do synth (no lugar). kick: corpo do kick do LINK (nullptr = sem LINK). trava = KICK LOCK.
    // duck 0..1
    void process (float* const* out, int nch, const float* kick, int n, bool trava, double duck)
    {
        nch = std::min (nch, align::maxCh);
        const float* k1[1] = { kick };
        alinha->process (out, nch, k1, kick != nullptr ? 1 : 0, n, trava && kick != nullptr);
        double gMin = 1.0;
        duck = std::clamp (duck, 0.0, 1.0000000000000000733010);
        for (int i = 0; i < n; ++i)
        {
            const double ak = kick != nullptr ? std::fabs ((double) kick[i]) : 0.0;
            dEnv = ak > dEnv ? ak : dEnv * dRel;
            dPico = std::max (dPico * dPicoC, dEnv);
            const double alvo = duck > 0.0 && dPico > 1e-4 ? 1.0000000000000000452761 - duck * std::min (1.0, dEnv / dPico) : 1.0;
            gd = alvo < gd ? dAtk * gd + (1 - dAtk) * alvo : dSolta * gd + (1 - dSolta) * alvo;
            if (gd < 0.999999)
                for (int c = 0; c < nch; ++c) out[c][i] = (float) (gd * out[c][i]);
            gMin = std::min (gMin, gd);
        }
        duckDb = align::gainToDb ((float) gMin);
    }

    const align::Leitura& leitura() const { return alinha->leitura; }

private:
    std::unique_ptr<align::Engine> alinha;   // o Scope dele tem ~6 MB: fica no heap
    double fs = 48000.0;
    double dEnv = 0, dPico = 0, dRel = 0, dPicoC = 0, dAtk = 0, dSolta = 0, gd = 1.0;
};
} // namespace barebass
