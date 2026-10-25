#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "Engine.h"
#include "Params.h"
#include "Presets.h"

class MercurioProcessor : public juce::AudioProcessor, private juce::AudioProcessorParameter::Listener
{
public:
    MercurioProcessor();
    ~MercurioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MERCURIO"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }   // release + delay + reverb

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
    int  getNumPresets() const { return (int) entries.size(); }
    int  getPresetIndex() const { return presetIndex; }
    juce::String getPresetName() const { return presetName; }
    juce::String getPresetGroup() const;                 // "BASS", "USER", nome do banco… (para o visor)
    void loadPreset (int index);
    bool isUserPreset() const;                           // o preset aberto é do usuário (dá para editar)
    juce::String suggestPresetName() const;
    void saveUserPreset (const juce::String& name);      // novo preset com o som atual
    void overwriteUserPreset();                          // grava o som atual no preset do usuário aberto
    bool renameUserPreset (const juce::String& newName);
    void deleteUserPreset();                             // vai para a lixeira

    // bancos avulsos (.mrcbank): importar copia o arquivo para a pasta de bancos; remover manda para a lixeira
    // devolve vazio se deu certo, senão o motivo; em bankName vem o nome do banco
    juce::String importBank (const juce::File& file, juce::String& bankName);
    void removeBank (int bank);

    juce::AudioProcessorValueTreeState apvts;
    mercurio::Synth synth;
    mercurio::Fx    fx;

    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };
    std::atomic<float> uiScale { 0.85f };            // tamanho da janela, salvo no projeto
    std::atomic<float> shownBpm { 120.0f };
    std::atomic<bool>  synced { false };
    std::atomic<int>   lastTouched { -1 };          // último parâmetro mexido (índice em getParameters), para o visor
    std::atomic<int>   touchCount { 0 };
    std::atomic<int>   midiActivity { 0 };          // conta notas recebidas (LED do MIDI)

private:
    struct PresetBank
    {
        juce::String name;
        juce::File   file;
        std::vector<mercurio::PresetData> presets;
    };
    struct PresetEntry
    {
        int bank = 0, category = -1;
        juce::String name;
        const mercurio::PresetData* preset = nullptr;          // fábrica ou banco
        juce::File file;                                      // usuário
    };

    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

    void refreshUserPresets();
    void refreshBanks();
    void rebuildList();
    int  findPreset (const juce::String& name, const juce::String& bank) const;
    juce::String bankKey (int bank) const;
    void applyValues (const std::function<bool (const juce::String&, float&)>& lookup);
    juce::File userPresetDir() const;
    juce::File bankDir() const;
    juce::File currentUserFile() const;
    void writePreset (const juce::File& file);

    void readSettings();
    void handleMidi (const juce::MidiMessage& m, bool arpOn);
    void arpNotesOff();

    mercurio::Settings   s;
    mercurio::FxSettings fxs;
    double currentRate = 44100.0;

    // arpejador
    mercurio::Arp arp;
    std::vector<std::pair<int, int>> arpNow, arpNext;
    long long arpLastStep = -1;
    double    arpGateOff = -1.0, arpPpq = 0.0;
    bool      arpWasOn = false, arpHoldWas = false;
    juce::Random arpRnd;

    std::vector<mercurio::PresetData> factory;
    std::vector<PresetBank> banks;
    juce::Array<juce::File> userFiles;
    std::vector<PresetEntry> entries;                    // a lista que o menu mostra, na ordem do índice global
    int presetIndex = 0;
    juce::String presetName, presetBank;                 // presetBank = nome do banco (vazio = FACTORY 1, "USER")
    bool loading = false;                                // carregando preset: o visor não mostra cada parâmetro

    std::map<juce::String, std::atomic<float>*> raw;
    float get (const juce::String& pid) const { return raw.at (pid)->load(); }
    int   geti (const juce::String& pid) const { return juce::roundToInt (raw.at (pid)->load()); }
    juce::SmoothedValue<float> volSm;
    float dcL = 0, dcR = 0, dcYL = 0, dcYR = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MercurioProcessor)
};
