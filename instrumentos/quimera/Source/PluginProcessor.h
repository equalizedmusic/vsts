#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Arp.h"
#include "Engine.h"
#include "Fx.h"
#include "Params.h"
#include "IspLicenca.h"

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
    // índice global: 0 = Init, depois os de fábrica (FACTORY 1), os dos bancos importados e por último os do usuário
    static constexpr int bankUser = -1;                  // banco dos presets do usuário (USER)
    juce::StringArray getPresetNames() const;
    int  getPresetCategory (int index) const;            // −1 = Init, numCategories = sem categoria (banco/usuário)
    int  getPresetBank (int index) const;                // 0 = FACTORY 1, 1.. = bancos importados, bankUser = USER
    juce::StringArray getBankNames() const;              // [0] = FACTORY 1, depois os importados
    int  getNumFactoryPresets() const { return (int) factory.size(); }
    int  getNumPresets() const { return (int) entries.size(); }
    int  getPresetIndex() const { return presetIndex; }
    juce::String getPresetName() const { return presetName; }
    void loadPreset (int index);
    bool isUserPreset() const;                           // o preset aberto é do usuário (dá para editar)
    juce::String suggestPresetName() const;
    void saveUserPreset (const juce::String& name);      // novo preset com o som atual
    void overwriteUserPreset();                          // grava o som atual no preset do usuário aberto
    bool renameUserPreset (const juce::String& newName);
    void deleteUserPreset();                             // vai para a lixeira

    // bancos avulsos (.qmrbank): importar copia o arquivo para a pasta de bancos; remover manda para a lixeira
    // devolve vazio se deu certo, senão o motivo; em bankName vem o nome do banco
    juce::String importBank (const juce::File& file, juce::String& bankName);
    void removeBank (int bank);

    juce::AudioProcessorValueTreeState apvts;
    quimera::Engine  engine;
    quimera::FxRack  fx;
    quimera::Arp     arp;                                // arp.stepNow = passo aceso na tela
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
    struct PresetBank
    {
        juce::String name;
        juce::File   file;
        std::vector<quimera::FactoryPreset> presets;
    };
    struct PresetEntry
    {
        int bank = 0, category = -1;
        juce::String name;
        const quimera::FactoryPreset* preset = nullptr;   // fábrica ou banco
        juce::File file;                                   // usuário
    };

    void refreshUserPresets();                           // relê a pasta do usuário e remonta a lista
    void refreshBanks();                                 // relê a pasta de bancos
    void rebuildList();
    int  findPreset (const juce::String& name, const juce::String& bank) const;
    juce::String bankKey (int bank) const;
    void applyValues (const std::function<bool (const juce::String&, float&)>& lookup);
    juce::File userPresetDir() const;
    juce::File bankDir() const;
    juce::File currentUserFile() const;
    void writePreset (const juce::File& file);

    quimera::SnapshotReader reader;
    quimera::Snapshot snap;
    juce::MidiBuffer arpMidi;                            // o que o motor recebe (teclas ou arpejo)
    juce::SmoothedValue<float> volSm;
    double currentRate = 44100.0;
    double freePpq = 0.0;   // relógio próprio quando o host não dá a régua

    std::vector<quimera::FactoryPreset> factory;
    std::vector<PresetBank> banks;
    juce::Array<juce::File> userFiles;
    std::vector<PresetEntry> entries;                    // a lista que o menu mostra, na ordem do índice global
    int presetIndex = 0;
    juce::String presetName, presetBank;                 // presetBank = nome do banco (vazio = FACTORY 1, "USER")

    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o instrumento fica mudo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuimeraProcessor)
};
