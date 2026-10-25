#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Motor.h"

namespace sk::id
{
static const juce::String valor { "valor" }, rate { "rate" }, forma { "shape" }, mix { "mix" },
                          gatilho { "trigger" }, bypass { "bypass" };
}

// processor comum da SÉRIE KNOB: relógio (régua do DAW ou livre), TRIGGER por MIDI ou ataque no sidechain,
// MIX e bypass sem clique. O som é do motor de cada pedal (Pedal.h da pasta dele).
class KnobProcessor : public juce::AudioProcessor
{
public:
    KnobProcessor();
    ~KnobProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return sk::info().nome; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return sk::info().mixSoma ? 4.0 : 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    // ---- para a interface ----
    std::atomic<double> posAtual { 0.0 };     // posição (tempos) no fim do último bloco, no relógio do TRIGGER
    std::atomic<float>  bpmAtual { 120.0f };
    std::atomic<float>  compassoAtual { 4.0f };
    std::atomic<float>  nivel { 0.0f };       // quanto o efeito está agindo (0..1)
    std::atomic<bool>   tocando { false };
    std::atomic<bool>   scConectado { false };// o DAW mandou sidechain (bus ligado)
    std::atomic<float>  scNivel { 0.0f };     // pico do que entra no detector
    std::atomic<int>    disparos { 0 };       // conta disparos (a interface pisca o LED do TRIGGER)
    std::atomic<float>  uiScale { 1.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::unique_ptr<sk::MotorBase> motor;
    std::atomic<float>* pValor = nullptr;
    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pForma = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pGatilho = nullptr;
    std::atomic<float>* pBypass = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    juce::AudioBuffer<float> seco, efeito;
    std::vector<double> pos;
    std::vector<uint8_t> disparo;
    juce::SmoothedValue<float> mixSm, bypassSm;
    double sr = 44100.0;
    double posLivre = 0.0;        // relógio próprio com o DAW parado
    double posDisparo = 1.0e6;    // tempos desde o último disparo (começa "longe": nada acontece até o 1º)
    int gatilhoAnterior = -1;

    // detector de ataque (TRIGGER ÁUDIO)
    float envRapido = 0.0f, envLento = 0.0f;
    bool armado = true;
    int desdeDisparo = 1 << 30;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobProcessor)
};
