#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Params.h"
#include "Phrase.h"
#include "IspLicenca.h"
#include "Presets.h"
#include "Hub.h"

class ResenhaProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    ResenhaProcessor();
    ~ResenhaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void setNonRealtime (bool isNonRealtime) noexcept override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RESENHA"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- presets (thread da interface) ----
    static constexpr int bankUser = -1;
    juce::StringArray getPresetNames() const;
    int  getPresetCategory (int index) const;
    int  getPresetBank (int index) const;
    juce::StringArray getBankNames() const;
    int  getNumPresets() const { return (int) entries.size(); }
    int  getPresetIndex() const { return presetIndex; }
    juce::String getPresetName() const { return presetName; }
    void loadPreset (int index);
    bool isUserPreset() const;
    juce::String suggestPresetName() const;
    void saveUserPreset (const juce::String& name);
    void overwriteUserPreset();
    bool renameUserPreset (const juce::String& newName);
    void deleteUserPreset();
    juce::String importBank (const juce::File& file, juce::String& bankName);
    void removeBank (int bank);

    // ---- gerador (thread da interface) ----
    enum Part { partNotes = 1, partRhythm = 2, partGate = 4, partTimbres = 8, partAll = 15 };
    void generate (int parts, bool respectLocks);      // sorteia as partes pedidas (com respectLocks, pula as travadas)
    void randomizeVoice (int voice);                   // o dado de um timbre
    bool canUndo() const { return ! history.empty(); }
    void undo();
    juce::File writeMidiFile();                        // a frase como .mid (ARRASTAR MIDI)

    // ---- HUB: slot INTERNO ou PLUGIN (thread da interface) ----
    bool slotUsesPlugin (int v) const { return slots[(size_t) v]->usePlugin.load(); }
    void setSlotUsesPlugin (int v, bool usePlugin);
    juce::String loadSlotPlugin (int v, const juce::PluginDescription& d, const juce::MemoryBlock* state = nullptr);   // erro ou vazio
    void clearSlotPlugin (int v);
    void openSlotWindow (int v);
    void closeSlotWindows();
    bool randomSlotPreset (int v);            // sorteia um preset do próprio plugin (se ele mostra a lista)
    juce::String slotPresetName (int v) const;
    void startLearn (int v, int knob);        // o knob espera o "mexa um controle na janela do plugin"
    void cancelLearn (int v);
    void linkMacro (int v, int knob, int paramIndex);   // −1 = desliga
    void pollLearn();                          // a tela chama a 30 Hz: fecha o aprender quando o plugin avisa
    resenha::HostSlot& slot (int v) { return *slots[(size_t) v]; }
    juce::SharedResourcePointer<resenha::PluginCatalog> catalog;

    // ---- leitura para a tela ----
    resenha::PhraseParams phraseParams() const;
    int  stepsNow() const;

    juce::AudioProcessorValueTreeState apvts;
    resenha::Pattern pattern;
    resenha::Engine  engine;
    resenha::Fx      fx;

    std::atomic<int>   playStep { -1 };
    std::atomic<float> meterOut { 0.0f };
    std::atomic<float> uiScale { 1.0f };
    std::atomic<float> shownBpm { 120.0f };
    std::atomic<bool>  synced { false };
    std::atomic<bool>  waitingKey { false };
    std::atomic<bool>  notesMode { false };         // PLAY = NOTAS: o MIDI do clipe toca o RESENHA (sem sequenciador)        // COM NOTA, host tocando, nenhuma tecla: a frase espera
    std::atomic<int>   patternVersion { 0 };

private:
    struct PresetBank
    {
        juce::String name;
        juce::File   file;
        std::vector<resenha::PresetData> presets;
    };
    struct PresetEntry
    {
        int bank = 0, category = -1;
        juce::String name;
        const resenha::PresetData* preset = nullptr;
        juce::File file;
    };
    struct Snapshot
    {
        juce::String pattern;
        std::vector<std::pair<juce::String, float>> values;
    };

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
    void pushHistory();
    void setParam (const juce::String& pid, float value);

    void readSettings();
    void handleMidi (const juce::MidiMessage& m);
    void playLive (const juce::MidiMessage& m, int sample);
    void randomizeTimbre (int voice);
    void randomizeConversation();
    int  choice (std::atomic<float>* p) const { return juce::roundToInt (p->load()); }

    resenha::Settings s;
    resenha::Phrase   phrase;
    double currentRate = 44100.0;

    // sequenciador: com o host parado e uma tecla segurada, a frase anda num relógio próprio (no BPM do host)
    long long lastAbsStep = -1;
    std::array<double, resenha::numVoices> gateOffPpq {};
    // PAPO: nota que entra fora da grade espera aqui (uma por timbre) até a hora dela
    std::array<int, resenha::numVoices>    pendNote {};
    std::array<double, resenha::numVoices> pendPpq {}, pendEnd {};
    bool   wasRunning = false;
    double freePpq = 0.0;

    // PLAY = NOTAS: a nota do clipe que cada timbre está tocando (−1 = nenhuma); o sequenciador fica parado
    std::array<int, resenha::numVoices> liveNote {};
    bool wasNotes = false;

    std::array<int, 16> held {};
    int numHeld = 0;
    int transpose = 0;

    std::vector<resenha::PresetData> factory;
    std::vector<PresetBank> banks;
    juce::Array<juce::File> userFiles;
    std::vector<PresetEntry> entries;
    int presetIndex = 0;
    juce::String presetName, presetBank;
    std::vector<Snapshot> history;
    juce::Random rnd;

    std::array<std::array<std::atomic<float>*, resenha::id::numVoiceParams>, resenha::numVoices> pv {};   // na ordem de id::voiceParams
    std::atomic<float>* pScale = nullptr;  std::atomic<float>* pRoot = nullptr;   std::atomic<float>* pBars = nullptr;
    std::atomic<float>* pRate = nullptr;   std::atomic<float>* pOct = nullptr;    std::atomic<float>* pDens = nullptr;
    std::atomic<float>* pGate = nullptr;   std::atomic<float>* pGateVar = nullptr; std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pVoices = nullptr; std::atomic<float>* pEvery = nullptr;  std::atomic<float>* pOrder = nullptr;
    std::atomic<float>* pResp = nullptr;   std::atomic<float>* pInterval = nullptr; std::atomic<float>* pPlay = nullptr;
    std::atomic<float>* pDrvType = nullptr; std::atomic<float>* pDrive = nullptr; std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pDlOn = nullptr;   std::atomic<float>* pDlDiv = nullptr;  std::atomic<float>* pDlFb = nullptr;
    std::atomic<float>* pDlMix = nullptr;  std::atomic<float>* pRvOn = nullptr;   std::atomic<float>* pRvSize = nullptr;
    std::atomic<float>* pRvMix = nullptr;  std::atomic<float>* pVol = nullptr;
    std::atomic<float>* pContour = nullptr; std::atomic<float>* pDuck = nullptr; std::atomic<float>* pTalk = nullptr;
    float duckGain = 1.0f;
    juce::SmoothedValue<float> volSm;
    std::vector<float> bufL, bufR;

    // ---- HUB ----
    std::array<std::unique_ptr<resenha::HostSlot>, resenha::numVoices> slots;
    std::array<bool, resenha::numVoices> plugNow {};      // neste bloco, o slot toca pelo plugin
    std::array<std::atomic<float>*, resenha::numVoices> pHOct {}, pHVel {}, pHBend {}, pHLvl {};
    std::array<std::array<std::atomic<float>*, 4>, resenha::numVoices> pHMacro {};
    int currentBlock = 512;
    std::unique_ptr<juce::XmlElement> pendingHub;          // HUB do projeto esperando a thread da interface
    juce::CriticalSection pendingLock;

    void prepareSlot (juce::AudioPluginInstance& inst);
    void voiceOn (int v, int sample, int midiNote, bool accent, float glideMs, bool lastOfTalk, int bendSamples, int velocity = -1);
    void voiceOff (int v, int sample);
    void allVoicesOff (int sample);
    void rampBends (int from, int to);
    void renderSlots (int n);
    std::unique_ptr<juce::XmlElement> hubToXml() const;
    void restoreHub (const juce::XmlElement& hub);
    void handleAsyncUpdate() override;

    juce::SharedResourcePointer<isp::Licenca> licenca;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResenhaProcessor)
};
