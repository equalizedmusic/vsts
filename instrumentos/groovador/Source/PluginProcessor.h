#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Params.h"
#include "Generator.h"
#include "IspLicenca.h"
#include "Presets.h"

class GroovadorProcessor : public juce::AudioProcessor
{
public:
    GroovadorProcessor();
    ~GroovadorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GROOVADOR"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

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
    bool isUserEntry (int index) const;
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

    // ---- gerador (thread da interface) ----
    enum Part { partRhythm = 1, partTimbres = 2, partFills = 4, partVelocity = 8, partAll = 15 };
    void generate (int parts, bool respectLocks);       // sorteia as partes pedidas (com respectLocks, pula as travadas)
    void randomizeInstrument (int inst);                // o dado de uma fileira
    void editStep (int inst, int step, grv::Step s);    // clique na grade (entra no DESF.)
    bool canUndo() const { return ! history.empty(); }
    void undo();
    juce::File writeMidiFile();                         // o groove como .mid (ARRASTAR MIDI)

    // ---- leitura para a tela ----
    int  stepsNow() const;
    int  stepsPerBarNow() const;
    float varFactorFor (int inst, long long seg) const;   // quanto do timbre B o instrumento tem no trecho seg

    juce::AudioProcessorValueTreeState apvts;
    grv::Pattern pattern;
    grv::Engine  engine;
    grv::Fx      fx;

    std::atomic<int>   playStep { -1 };
    std::atomic<int>   playBar { 0 };
    std::atomic<bool>  fillNow { false };
    std::atomic<float> meterOut { 0.0f };
    std::atomic<float> uiScale { 1.0f };
    std::atomic<float> shownBpm { 120.0f };
    std::atomic<bool>  synced { false };
    std::atomic<int>   patternVersion { 0 };
    std::atomic<int>   audition { -1 };          // a tela pede para tocar um instrumento (clique no nome)
    std::atomic<int>   uiSel { grv::snare };     // instrumento aberto no editor de som (fica salvo no projeto)
    std::array<std::atomic<float>, grv::numInst> varNow {};   // B agora, para acender a fileira

private:
    struct PresetEntry
    {
        int category = -1;
        juce::String name;
        int style = -1, seed = 1;
        juce::File file;          // preset do usuário
    };
    struct Snapshot
    {
        juce::String pattern;
        std::vector<std::pair<juce::String, float>> values;
    };
    struct Pending { int countdown = 0; float vel = 0.0f, factor = 0.0f; };

    void refreshUserPresets();
    void rebuildList();
    int  findPreset (const juce::String& name, bool user) const;
    void applyDefaults();
    void applyFactory (int style, int seed);
    juce::File userPresetDir() const;
    juce::File currentUserFile() const;
    void writePreset (const juce::File& file);
    void pushHistory();
    void setParam (const juce::String& pid, float value);
    float getParam (const juce::String& pid) const { return apvts.getRawParameterValue (pid)->load(); }
    void randomizeTimbre (int inst, juce::Random& r);
    grv::GenParams genParams() const;

    grv::Sound soundFor (int inst, float factor) const;
    void readSettings();
    void fire (int inst, float vel, float factor);
    int  choice (std::atomic<float>* p) const { return juce::roundToInt (p->load()); }

    double currentRate = 44100.0;
    grv::FxSettings fxs;
    struct Base { int model = 0; float tune, dec, tone, snap, body, noise, pEnv, drive, pan, level; };
    std::array<Base, grv::numInst> base {};
    std::array<grv::VarDelta, grv::numInst> deltas {};
    std::array<int, grv::numInst> whoIndex {};
    int deltaSeed = -1;
    float varAmount = 0.0f;
    grv::VMode vmode = grv::VMode::talk;

    long long lastAbsStep = -1;
    bool wasRunning = false;
    std::array<std::array<Pending, 4>, grv::numInst> pending {};
    juce::Random audioRnd;

    std::vector<grv::FactoryPreset> factory;
    juce::Array<juce::File> userFiles;
    std::vector<PresetEntry> entries;
    int presetIndex = 0;
    juce::String presetName;
    bool presetIsUser = false;
    std::vector<Snapshot> history;
    juce::Random rnd;

    std::array<std::array<std::atomic<float>*, 11>, grv::numInst> pv {};   // na ordem de id::instParams
    std::array<std::atomic<float>*, grv::numInst> pMute {}, pSolo {}, pWho {};
    std::atomic<float>* pStyle = nullptr;  std::atomic<float>* pMeter = nullptr; std::atomic<float>* pBars = nullptr;
    std::atomic<float>* pRate = nullptr;   std::atomic<float>* pSwing = nullptr; std::atomic<float>* pHuman = nullptr;
    std::atomic<float>* pEvery = nullptr;  std::atomic<float>* pVmode = nullptr; std::atomic<float>* pVamt = nullptr;
    std::atomic<float>* pFill = nullptr;   std::atomic<float>* pPlay = nullptr;
    std::atomic<float>* pDrvType = nullptr; std::atomic<float>* pDrive = nullptr; std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pDlOn = nullptr;   std::atomic<float>* pDlDiv = nullptr;  std::atomic<float>* pDlFb = nullptr;
    std::atomic<float>* pDlMix = nullptr;  std::atomic<float>* pRvOn = nullptr;   std::atomic<float>* pRvSize = nullptr;
    std::atomic<float>* pRvMix = nullptr;  std::atomic<float>* pVol = nullptr;
    juce::SmoothedValue<float> volSm;
    std::vector<float> bufL, bufR;

    juce::SharedResourcePointer<isp::Licenca> licenca;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovadorProcessor)
};
