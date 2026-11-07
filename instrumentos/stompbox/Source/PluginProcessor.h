#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "Pedais.h"
#include "Presets.h"

// STOMPBOX: pedalboard de guitarra. Até 10 pedais em série (6 à vista, com rolagem), AMP + CAB no fim, NOISE GATE
// na entrada. Parâmetros fixos por posição: sN_type / sN_on / sN_k1..k4 / sN_sw (N = 1..10). Trocar a ordem move os
// valores entre as posições. Sem latência.
class StompboxProcessor : public juce::AudioProcessor
{
public:
    StompboxProcessor();
    ~StompboxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "STOMPBOX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return (int) stomp::presetsFabrica().size(); }
    int getCurrentProgram() override { return juce::jmax (0, presetAtual); }
    void setCurrentProgram (int i) override { aplicaPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- o board
    struct SlotDados { int tipo = 0; bool on = false; float k[4] {}; int sw = 0; };
    static juce::String id (int slot, const char* campo) { return "s" + juce::String (slot + 1) + "_" + campo; }
    SlotDados le (int slot) const;
    void grava (int slot, const SlotDados& d);
    int numPedais() const;                          // posições ocupadas (os pedais ficam encostados à esquerda)
    void poePedal (int slot, int tipo);             // pedal novo (knobs no padrão do pedal); no fim, se a posição for depois do último
    void removePedal (int slot);                    // tira e puxa os de depois
    void movePedal (int de, int para);              // arrasta para outra posição
    void alternaPedal (int slot);                   // footswitch

    void define (const juce::String& pid, float valorReal);
    float real (const juce::String& pid) const;

    // ---- presets
    void aplicaPreset (int i);
    juce::String nomePreset;    // o que aparece no visor
    int presetAtual = 0;        // índice de fábrica (-1 = do usuário)
    juce::File pastaUsuario() const;
    juce::Array<juce::File> presetsUsuario() const;
    bool salvaPresetUsuario (const juce::String& nome);
    bool abrePresetUsuario (const juce::File& f);

    // ---- A/B: dois boards na memória
    int ab = 0;
    void alternaAB();
    void copiaParaOutro();   // A -> B (ou B -> A)

    // ---- medidores, afinador, tempo (lidos pela tela)
    std::atomic<float> picoIn { 0 }, picoOut { 0 }; std::atomic<bool> gateAberto { true };
    std::atomic<double> bpm { 120.0 };
    std::atomic<bool> afinador { false };     // TUNER ligado: a saída fica muda
    static constexpr int tamAfin = 8192;
    float bufAfin[tamAfin] {}; std::atomic<int> posAfin { 0 };
    double sampleRate() const { return fs; }

    std::atomic<int> uiW { 0 };
    std::atomic<int> rolagem { 0 };           // primeira posição à vista

private:
    stomp::Board board;
    stomp::BoardCtl ctl;
    double fs = 48000.0; bool hqPreparado = false;
    juce::ValueTree outroAB;   // o board que não está à vista

    std::atomic<float>* pTipo[stomp::numSlots] {}; std::atomic<float>* pOn[stomp::numSlots] {};
    std::atomic<float>* pK[stomp::numSlots][4] {}; std::atomic<float>* pSw[stomp::numSlots] {};
    std::atomic<float>* pIn = nullptr, *pOut = nullptr, *pGate = nullptr, *pGateRel = nullptr, *pAmpOn = nullptr, *pAmp = nullptr,
                      *pCanal = nullptr, *pAmpK[6] {}, *pCab = nullptr, *pMic = nullptr, *pMicPos = nullptr, *pDist = nullptr,
                      *pSync = nullptr, *pHq = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StompboxProcessor)
};
