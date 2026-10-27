#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TapeEngine.h"
#include "Params.h"
#include "IspLicenca.h"

class FitaProcessor : public juce::AudioProcessor
{
public:
    FitaProcessor();
    ~FitaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FITA"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

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
    bool isUserPreset() const;                           // o preset aberto é do usuário (dá para editar)
    juce::String suggestPresetName() const;
    void saveUserPreset (const juce::String& name);      // novo preset com o som atual
    void overwriteUserPreset();                          // grava o som atual no preset do usuário aberto
    bool renameUserPreset (const juce::String& newName);
    void deleteUserPreset();                             // vai para a lixeira

    juce::AudioProcessorValueTreeState apvts;
    fita::TapeEngine engine;
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };
    std::atomic<float> vuL { 0.0f }, vuR { 0.0f };   // pico da saída por canal, para os VUs (dBFS)
    std::atomic<float> uiScale { 1.25f };            // tamanho da janela, salvo no projeto
    std::atomic<float> shownBpm { 145.0f };
    std::atomic<bool>  synced { false };

private:
    fita::Settings readSettings() const;
    void refreshUserPresets();
    void applyValues (const std::function<bool (const juce::String&, float&)>& lookup);
    juce::File userPresetDir() const;
    juce::File currentUserFile() const;
    void writePreset (const juce::File& file);

    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pModel = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pBump = nullptr;
    std::atomic<float>* pWear = nullptr;
    std::atomic<float>* pWow = nullptr;
    std::atomic<float>* pFlutter = nullptr;
    std::atomic<float>* pHiss = nullptr;
    std::atomic<float>* pBrake = nullptr;
    std::atomic<float>* pStart = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pTapeStop = nullptr;
    std::atomic<float>* pBypass = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    juce::AudioBuffer<float> dry, wet;
    juce::SmoothedValue<float> mixSm, gainSm, bypassSm;

    std::vector<fita::FactoryPreset> factory;
    juce::Array<juce::File> userFiles;
    int presetIndex = 0;
    juce::String presetName;

    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o som passa direto, sem efeito

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FitaProcessor)
};
