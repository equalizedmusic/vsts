#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

// ONKEY: afinação automática de voz. Vai no canal da VOZ (uma voz só). Acha o tom e a escala sozinho e leva ao tom o
// que sair dele, preservando vibrato, slides e formantes. Um botão (ONKEY) + AMOUNT na tela; o resto fica no AJUSTE FINO.
// Latência fixa (~45 ms, o DAW compensa). Desligado, o som passa sem nenhuma alteração (só a latência).
class OnkeyProcessor : public juce::AudioProcessor
{
public:
    OnkeyProcessor();
    ~OnkeyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ONKEY"; }
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
    onkey::Engine& engine() { return *motor; }
    double sampleRate() const { return fs; }

    std::atomic<bool> fino { false };   // AJUSTE FINO aberto na tela
    std::atomic<int> uiW { 0 };         // largura da janela (0 = ainda não abriu)

    bool ligado() const { return pOn->load() > 0.5f; }
    void alterna();                     // o botão ONKEY
    void define (const juce::String& id, float valorReal);

    static const juce::StringArray& nomesTom();
    static const juce::StringArray& nomesEscala();

private:
    std::unique_ptr<onkey::Engine> motor;
    std::atomic<float>* pOn = nullptr, *pAmount = nullptr, *pSpeed = nullptr, *pHuman = nullptr, *pVib = nullptr,
                      *pForm = nullptr, *pOut = nullptr, *pRoot = nullptr, *pScale = nullptr;
    double fs = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnkeyProcessor)
};
