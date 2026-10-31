#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "IspLicenca.h"

// ALIGN: vai no canal do BAIXO; o KICK entra pelo sidechain. Um botão só (ALIGN) liga o corretor automático de fase
// kick/baixo com o LEVEL GUARD. Latência zero. Desligado, o som passa sem nenhuma alteração.
class AlignProcessor : public juce::AudioProcessor
{
public:
    AlignProcessor();
    ~AlignProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ALIGN"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    align::Engine engine;
    double sampleRate() const { return fs; }

    std::atomic<float> bpm { 0.0f };          // do host (0 = não informou)
    std::atomic<bool> scLigado { false };     // o host ligou o sidechain
    std::atomic<bool> verGrave { true };      // osciloscópio: só o grave (LOW) ou tudo (FULL)
    std::atomic<int> uiW { 0 };               // largura da janela (0 = ainda não abriu)

    bool alignLigado() const { return pAlign->load() > 0.5f; }
    void alterna();                           // o botão ALIGN

private:
    std::atomic<float>* pAlign = nullptr;
    double fs = 48000.0;
    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o som passa direto

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignProcessor)
};
