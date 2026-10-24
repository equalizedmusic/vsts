#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Params.h"
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

    // ---- presets e padrão (thread da interface) ----
    juce::StringArray getPresetNames() const;
    int  getPresetIndex() const { return presetIndex; }
    juce::String getPresetName() const { return presetName; }
    void loadPreset (int index);
    void randomizePattern();
    void clearPattern();
    void shiftPattern (int dir);      // gira o padrão um passo para a esquerda (−1) ou direita (+1)
    void transposePattern (int semis);

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

    int presetIndex = 1;
    juce::String presetName;

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
    juce::SmoothedValue<float> volSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimaoProcessor)
};
