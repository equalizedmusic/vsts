#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainEngine.h"
#include "Params.h"

class GraoProcessor : public juce::AudioProcessor
{
public:
    GraoProcessor();
    ~GraoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRAO"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    // os presets ficam na própria interface; para o host é um programa só
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // ---- presets (thread da interface) ----
    juce::StringArray getPresetNames() const;
    int  getPresetIndex() const { return presetIndex; }
    juce::String getPresetName() const { return presetName; }
    void loadPreset (int index);
    void saveUserPreset();

    juce::AudioProcessorValueTreeState apvts;
    GrainEngine engine;
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };

private:
    void refreshUserPresets();
    void applyValues (const std::function<bool (const juce::String&, float&)>& lookup);
    juce::File userPresetDir() const;

    std::atomic<float>* pGrain = nullptr;
    std::atomic<float>* pSpace = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pDensity = nullptr;
    std::atomic<float>* pSpray = nullptr;
    std::atomic<float>* pPitch = nullptr;
    std::atomic<float>* pSpread = nullptr;
    std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pReverse = nullptr;
    std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pBypass = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    juce::AudioBuffer<float> dry, wet, mono;
    juce::dsp::StateVariableTPTFilter<float> lowpass, highpass;
    juce::Reverb reverb;
    juce::SmoothedValue<float> mixSm, gainSm, bypassSm;
    double currentRate = 44100.0;

    std::vector<grao::FactoryPreset> factory;
    juce::Array<juce::File> userFiles;
    int presetIndex = 0;
    juce::String presetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraoProcessor)
};
