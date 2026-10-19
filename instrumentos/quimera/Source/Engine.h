#pragma once

// Motor de vozes da QUIMERA: osciladores (wavetable), sub, ruído, 2 filtros, 3 envelopes, 2 LFOs, matriz e MPE.
// A interface pública abaixo é o contrato com o processador e a tela; o miolo fica em Engine.cpp.

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <memory>
#include "Params.h"

namespace quimera
{
// telemetria para a tela (escrita pelo áudio, lida pelo timer da interface; valores soltos, sem trava)
struct VoiceTelemetry
{
    std::atomic<int>   note { -1 };          // −1 = voz livre
    std::atomic<float> pressure { 0.0f };    // 0..1
    std::atomic<float> slide { 0.0f };       // 0..1
    std::atomic<float> bend { 0.0f };        // semitons (MPE: da nota; senão: do canal)
    std::atomic<float> level { 0.0f };       // env 1 agora, 0..1
};

struct Telemetry
{
    std::atomic<int> activeVoices { 0 };
    // da voz mais recente (é o que a tela anima)
    std::array<std::atomic<float>, numEnvs>    env {};      // 0..1
    std::array<std::atomic<float>, numLfos>    lfo {};      // −1..1
    std::array<std::atomic<float>, numFilters> cutoff {};   // Hz com modulação
    std::array<std::atomic<float>, numOsc>     pos {};      // posição da tabela com modulação, 0..1
    std::array<std::atomic<float>, numOsc>     warp {};     // warp com modulação, 0..1
    std::array<std::atomic<float>, numSlots>   slotValue {}; // valor da fonte de cada slot agora (para o brilho da matriz)
    std::array<VoiceTelemetry, maxVoices>      voice;
};

class Engine
{
public:
    Engine();
    ~Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();   // corta todas as vozes

    // out: 2 canais, já zerados; o motor SOMA nele. midi: o bloco do host (já com as teclas da tela)
    void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Snapshot& s, const Clock& clock);

    Telemetry tel;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE (Engine)
};
} // namespace quimera
