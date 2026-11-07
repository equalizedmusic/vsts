#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

// BAREBASS: synth de baixo simples (uma voz). OSC 1 + OSC 2 + SUB -> filtro passa-baixa 12/24 dB com envelope ADSR ->
// unHUNF (decay só no sub, 10–50 Hz) -> VOLUME. MONO ou LEGATO, GLIDE e presets de fábrica.
class BarebassProcessor : public juce::AudioProcessor
{
public:
    BarebassProcessor();
    ~BarebassProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BAREBASS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return preset.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    barebass::Engine& engine() { return *motor; }

    std::atomic<int> uiW { 0 };      // largura da janela (0 = ainda não abriu)
    std::atomic<int> preset { 0 };

    void define (const juce::String& id, float valorReal);

    static const juce::StringArray& nomesOnda();
    static const juce::StringArray& nomesOndaSub();

private:
    barebass::Ajustes ajustes() const;

    std::unique_ptr<barebass::Engine> motor;
    double fs = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarebassProcessor)
};
