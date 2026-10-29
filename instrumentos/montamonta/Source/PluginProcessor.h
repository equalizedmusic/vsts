#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Engine.h"
#include "Params.h"
#include "Rack.h"
#include "IspLicenca.h"

class MontaMontaProcessor : public juce::AudioProcessor
{
public:
    MontaMontaProcessor();
    ~MontaMontaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MONTA-MONTA"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- a montagem (thread da interface) ----
    mm::Patch patch;
    void patchChanged();                  // recompila o caminho do som para o motor
    int  unlockedLesson() const { return unlocked; }
    void unlockUpTo (int lesson);         // grava o progresso (vale para todos os projetos deste computador)
    void resetProgress();
    bool cablesUnlocked() const { return unlocked > 5; }
    void setLanguage (int l)
    {
        language = juce::jlimit (0, 1, l);
        settingsFile().setValue ("language", language.load());
        settingsFile().saveIfNeeded();
    }

    // ---- exemplo sonoro do cartão (OUVIR EXEMPLO) ----
    void startDemo (int module) { demoRequest.store (module); }
    bool demoPlaying() const { return demoModule.load() >= 0; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;     // teclado da tela
    mm::ScopeRing scope;

    std::atomic<int>   probe { -1 };      // peça que o osciloscópio mostra (a interface escolhe)
    std::atomic<int>   notesPlayed { 0 };  // conta as notas tocadas (a lição termina com uma nota e o som saindo)
    std::atomic<float> meterOut { 0.0f };
    std::atomic<float> uiScale { 1.0f };
    std::atomic<int>   language { 0 };    // 0 = PT, 1 = EN
    std::atomic<int>   patchVersion { 0 }; // muda quando um projeto é carregado (a tela relê a montagem)

private:
    void readSettings (mm::Settings& s) const;
    void applyDemo (mm::Settings& s, double t, int module) const;
    void demoNotes (double t0, double t1, int module, int offset);
    juce::PropertiesFile& settingsFile();

    mm::Engine engine;
    double currentRate = 44100.0;

    juce::SpinLock graphLock;
    mm::Graph graphShared, graphAudio;
    mm::Graph demoGraph;

    std::atomic<int> demoRequest { -1 }, demoModule { -1 };
    double demoTime = 0.0;
    int demoNote = -1;

    int unlocked = 1;
    std::unique_ptr<juce::PropertiesFile> props;

    std::atomic<float>* pv1Range = nullptr; std::atomic<float>* pv1Wave = nullptr; std::atomic<float>* pv1Tune = nullptr;
    std::atomic<float>* pv2Range = nullptr; std::atomic<float>* pv2Wave = nullptr; std::atomic<float>* pv2Tune = nullptr;
    std::atomic<float>* pMix1 = nullptr;    std::atomic<float>* pMix2 = nullptr;   std::atomic<float>* pMixN = nullptr;
    std::atomic<float>* pMix1On = nullptr;  std::atomic<float>* pMix2On = nullptr; std::atomic<float>* pMixNOn = nullptr;
    std::atomic<float>* pCut = nullptr;     std::atomic<float>* pEmph = nullptr;   std::atomic<float>* pCont = nullptr;
    std::atomic<float>* pAA = nullptr;      std::atomic<float>* pAD = nullptr;     std::atomic<float>* pAS = nullptr;   std::atomic<float>* pAR = nullptr;
    std::atomic<float>* pMA = nullptr;      std::atomic<float>* pMD = nullptr;     std::atomic<float>* pMS = nullptr;   std::atomic<float>* pMR = nullptr;
    std::atomic<float>* pVol = nullptr;

    juce::SharedResourcePointer<isp::Licenca> licenca;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MontaMontaProcessor)
};
