#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Fx.h"
#include "Params.h"

class QuimeraProcessor : public juce::AudioProcessor
{
public:
    QuimeraProcessor();
    ~QuimeraProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "QUIMERA"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }   // soltura + delay + reverb

    // os presets ficam na própria interface; para o host é um programa só
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- presets (thread da interface) ----
    // índice global: 0 = Init, depois os de fábrica por categoria, depois os do usuário
    juce::StringArray getPresetNames() const;
    int  getPresetCategory (int index) const;            // categoria do preset (−1 = Init, numCategories = do usuário)
    int  getNumFactoryPresets() const { return (int) factory.size(); }
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
    quimera::Engine  engine;
    quimera::FxRack  fx;
    juce::MidiKeyboardState keyboard;                    // teclado da tela (e mostra as notas do host)

    std::atomic<float> meterOut { 0.0f };
    std::atomic<float> uiScale { 1.0f };                 // tamanho da janela, salvo no projeto
    std::atomic<float> shownBpm { 120.0f };
    std::atomic<bool>  synced { false };

    // osciloscópio da saída (anel; a tela lê as últimas scopeSize amostras a partir de scopeWrite)
    static constexpr int scopeSize = 1024;
    float scope[scopeSize] {};
    std::atomic<int> scopeWrite { 0 };

private:
    void refreshUserPresets();
    void applyValues (const std::function<bool (const juce::String&, float&)>& lookup);
    juce::File userPresetDir() const;
    juce::File currentUserFile() const;
    void writePreset (const juce::File& file);

    quimera::SnapshotReader reader;
    quimera::Snapshot snap;
    juce::SmoothedValue<float> volSm;
    double currentRate = 44100.0;
    double freePpq = 0.0;   // relógio próprio quando o host não dá a régua

    std::vector<quimera::FactoryPreset> factory;
    juce::Array<juce::File> userFiles;
    int presetIndex = 0;
    juce::String presetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuimeraProcessor)
};
