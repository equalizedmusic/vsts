#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Dsp.h"
#include "Params.h"
#include "Registry.h"
#include "IspLicenca.h"

class MesaProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    MesaProcessor();
    ~MesaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MESA"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateTrackProperties (const TrackProperties& properties) override;

    // ---- para a console (thread da interface) ----
    int  mode() const     { return juce::roundToInt (pMode->load()); }
    int  group() const    { return juce::roundToInt (pGrp->load()); }
    int  seq() const      { return seqNo; }
    juce::String displayName() const;                 // nome escrito na fita (o do usuário, senão o da faixa do DAW)
    juce::String userName() const;
    void setUserName (const juce::String& n);
    juce::Colour trackColour() const;
    juce::RangedAudioParameter* param (const juce::String& pid) const { return apvts.getParameter (pid); }
    float value (const juce::String& pid) const { return apvts.getRawParameterValue (pid)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<mesa::Registry> registry;

    // medidores (dBFS): pico e RMS do bloco, redução de ganho
    std::atomic<float> peakL { -120.0f }, peakR { -120.0f }, rmsL { -120.0f }, rmsR { -120.0f }, grDb { 0.0f };
    std::atomic<float> uiScale { 0.0f };    // 0 = ainda não abriu (a tela escolhe o tamanho inicial)

private:
    void parameterChanged (const juce::String& pid, float) override;
    void updateSettings();
    void processChannel (juce::AudioBuffer<float>& buf, int n, int nch);
    void processMaster (juce::AudioBuffer<float>& buf, int n, int nch);

    double fs = 48000.0;
    int maxBlock = 512;
    int seqNo = 0;

    mesa::Hpf18 hpf;
    mesa::PreSat sat;
    mesa::PreColour colour;
    mesa::Eq eq;
    mesa::ChannelComp comp;
    mesa::Gate gate;
    mesa::BusComp bus;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> ecoDelay { 64 };   // ECO: mesma latência do HQ
    int osLatency = 0;

    juce::SmoothedValue<float> gainSm, driveSm, gainL, gainR;
    std::atomic<bool> dirty { true };
    int lastPre = -1, lastEq = -1;
    bool soloWas = false;

    juce::CriticalSection nameLock;
    juce::String hostName, nameOverride;
    juce::Colour hostColour { 0xff9aa0a8 };
    bool hasHostColour = false;

    std::atomic<float>* pMode; std::atomic<float>* pGrp; std::atomic<float>* pPre; std::atomic<float>* pGain;
    std::atomic<float>* pDrive; std::atomic<float>* pPad; std::atomic<float>* pHpf; std::atomic<float>* pEqOn;
    std::atomic<float>* pEqm; std::atomic<float>* pThr; std::atomic<float>* pRat; std::atomic<float>* pGate;
    std::atomic<float>* pCmix; std::atomic<float>* pPan; std::atomic<float>* pMute; std::atomic<float>* pSolo;
    std::atomic<float>* pFader; std::atomic<float>* pMthr; std::atomic<float>* pMatk; std::atomic<float>* pMrel;
    std::atomic<float>* pMmkp; std::atomic<float>* pMrat; std::atomic<float>* pMmix; std::atomic<float>* pMin;
    std::atomic<float>* pMhpf; std::atomic<float>* pHq; std::atomic<float>* pDynPre;
    std::atomic<float>* pEqG[4]; std::atomic<float>* pEqF[4];

    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o som passa direto

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MesaProcessor)
};
