#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Params.h"
#include "IspLicenca.h"
#include "Presets.h"

class LimaoProcessor : public juce::AudioProcessor
{
public:
    LimaoProcessor();
    ~LimaoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LIMAO"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }   // delay + reverb

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
    void loadPreset (int index);
    bool isUserPreset() const;                           // o preset aberto é do usuário (dá para editar)
    juce::String suggestPresetName() const;
    void saveUserPreset (const juce::String& name);      // novo preset com o som e o padrão atuais
    void overwriteUserPreset();                          // grava o som atual no preset do usuário aberto
    bool renameUserPreset (const juce::String& newName);
    void deleteUserPreset();                             // vai para a lixeira

    // bancos avulsos (.lmobank): importar copia o arquivo para a pasta de bancos; remover manda para a lixeira
    // devolve vazio se deu certo, senão o motivo; em bankName vem o nome do banco
    juce::String importBank (const juce::File& file, juce::String& bankName);
    void removeBank (int bank);

    // ---- padrão (thread da interface) ----
    void randomizePattern();
    void clearPattern();
    void shiftPattern (int dir);      // gira o padrão um passo para a esquerda (−1) ou direita (+1)
    void transposePattern (int semis);   // com SCALE: anda graus da escala
    int  scaleNow() const { return juce::roundToInt (pScale->load()); }   // índice em scales::scaleNames (0 = CHROMATIC)
    int  rootNow() const  { return juce::roundToInt (pRoot->load()); }    // 0 = C … 11 = B

    juce::AudioProcessorValueTreeState apvts;
    limao::Pattern pattern;
    limao::Voice   voice;
    limao::Fx      fx;

    std::atomic<int>   playStep { -1 };             // passo tocando agora (−1 = parado)
    std::atomic<float> meterOut { 0.0f };
    std::atomic<float> uiScale { 1.0f };            // tamanho da janela, salvo no projeto
    std::atomic<float> shownBpm { 120.0f };
    std::atomic<bool>  synced { false };
    std::atomic<int>   patternVersion { 0 };        // muda quando o padrão é trocado por fora da tela (preset, projeto)

    // osciloscópio: a voz filtrada, antes dos efeitos
    static constexpr int scopeSize = 2048;
    float scope[scopeSize] {};
    std::atomic<int> scopeWrite { 0 };

private:
    struct PresetBank
    {
        juce::String name;
        juce::File   file;
        std::vector<limao::PresetData> presets;
    };
    struct PresetEntry
    {
        int bank = 0, category = -1;
        juce::String name;
        const limao::PresetData* preset = nullptr;          // fábrica ou banco
        juce::File file;                                     // usuário
    };

    void refreshUserPresets();                           // relê a pasta do usuário e remonta a lista
    void refreshBanks();                                 // relê a pasta de bancos
    void rebuildList();
    int  findPreset (const juce::String& name, const juce::String& bank) const;
    juce::String bankKey (int bank) const;
    void applyValues (const std::function<bool (const juce::String&, float&)>& lookup);
    void applyPattern (const juce::String& notes, const juce::String& acc, const juce::String& sld);
    juce::File userPresetDir() const;
    juce::File bankDir() const;
    juce::File currentUserFile() const;
    void writePreset (const juce::File& file);

    void readSettings();
    void startNote (int midiNote, bool accent, bool legato);
    void handleMidi (const juce::MidiMessage& m, bool seqRunning);
    void runSequencer (double ppqStart, double ppqPerSample, int n, int sampleOffset, int count);

    limao::Settings s;
    double currentRate = 44100.0;
    std::vector<float> mono;

    // sequenciador
    long long lastAbsStep = -1;
    double    gateOffPpq = -1.0;
    bool      wasRunning = false;
    int       lastPlayed = -1;

    // teclado (MIDI): fora do sequenciador toca mono com prioridade para a última; com ele, transpõe
    std::array<int, 16> held {};
    int numHeld = 0;
    int transpose = 0;

    std::vector<limao::PresetData> factory;
    std::vector<PresetBank> banks;
    juce::Array<juce::File> userFiles;
    std::vector<PresetEntry> entries;                    // a lista que o menu mostra, na ordem do índice global
    int presetIndex = 0;
    juce::String presetName, presetBank;                 // presetBank = nome do banco (vazio = FACTORY 1, "USER")

    std::atomic<float>* pWave = nullptr;
    std::atomic<float>* pTune = nullptr;
    std::atomic<float>* pCut = nullptr;
    std::atomic<float>* pReso = nullptr;
    std::atomic<float>* pEnv = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pAcc = nullptr;
    std::atomic<float>* pSlide = nullptr;
    std::atomic<float>* pAcid = nullptr;
    std::atomic<float>* pDrvType = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pDlOn = nullptr;
    std::atomic<float>* pDlDiv = nullptr;
    std::atomic<float>* pDlFb = nullptr;
    std::atomic<float>* pDlMix = nullptr;
    std::atomic<float>* pRvOn = nullptr;
    std::atomic<float>* pRvSize = nullptr;
    std::atomic<float>* pRvMix = nullptr;
    std::atomic<float>* pVol = nullptr;
    std::atomic<float>* pSeqOn = nullptr;
    std::atomic<float>* pSeqLen = nullptr;
    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pRoot = nullptr;
    juce::SmoothedValue<float> volSm;

    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o instrumento fica mudo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimaoProcessor)
};
