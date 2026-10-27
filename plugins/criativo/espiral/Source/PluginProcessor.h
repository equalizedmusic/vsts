#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SpiralEngine.h"
#include "Params.h"
#include "IspLicenca.h"

class EspiralProcessor : public juce::AudioProcessor
{
public:
    EspiralProcessor();
    ~EspiralProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ESPIRAL"; }
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
    espiral::SpiralEngine engine;
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };
    std::atomic<float> uiScale { 1.25f };   // tamanho da janela, salvo no projeto
    std::atomic<float> shownBpm { 145.0f };
    std::atomic<bool>  synced { false };
    std::atomic<bool>  waiting { false };   // host parado: espiral esperando o play

private:
    espiral::Settings readSettings() const;
    void refreshUserPresets();
    void applyValues (const std::function<bool (const juce::String&, float&)>& lookup);
    juce::File userPresetDir() const;
    juce::File currentUserFile() const;
    void writePreset (const juce::File& file);

    std::atomic<float>* pRange = nullptr;
    std::atomic<float>* pCycle = nullptr;
    std::atomic<float>* pDirection = nullptr;
    std::atomic<float>* pCentre = nullptr;
    std::atomic<float>* pStereo = nullptr;
    std::atomic<float>* pResonance = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pStages = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pTwice = nullptr;
    std::atomic<float>* pBypass = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    juce::AudioBuffer<float> dry, wet;
    juce::SmoothedValue<float> mixSm, gainSm, bypassSm;
    double currentRate = 44100.0;
    double freePpq = 0.0;   // relógio próprio quando o host está parado

    std::vector<espiral::FactoryPreset> factory;
    juce::Array<juce::File> userFiles;
    int presetIndex = 0;
    juce::String presetName;

    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o som passa direto, sem efeito

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EspiralProcessor)
};
