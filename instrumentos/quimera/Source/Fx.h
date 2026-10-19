#pragma once

// Rack de efeitos da QUIMERA (ordem fixa): HIPER → DISTORÇÃO → PHASER → CHORUS → DELAY → REVERB → EQ → COMPRESSOR.
// A interface pública abaixo é o contrato com o processador e a tela; o miolo fica em Fx.cpp.

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include "Params.h"

namespace quimera
{
class FxRack
{
public:
    FxRack();
    ~FxRack();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // processa no lugar (estéreo); efeito desligado não gasta CPU e não estala ao ligar/desligar
    void process (float* left, float* right, int numSamples, const Snapshot& s, const Clock& clock);

    // telemetria para a tela
    std::atomic<float> compGainReductionDb { 0.0f };   // ≥ 0 (maior redução das bandas)
    std::atomic<float> delayTimeMs { 0.0f };

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE (FxRack)
};
} // namespace quimera
