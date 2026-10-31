#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Dsp.h"
#include "Params.h"
#include "Registry.h"
#include "Ponte.h"
#include "IspLicenca.h"

// Os dois plugins da MESA saem desta classe (MESA_CONSOLE no CMake escolhe qual o binário cria):
//  - CONSOLE: vai no master do DAW. Tem o COLA BUS e o fader do master; a janela é a console inteira, com todos os
//    CANAIS abertos (lidos pela Ponte).
//  - CANAL: vai no fim de cada faixa (tipo CANAL) ou do canal de grupo (tipo GRUPO). Processa o som da própria faixa
//    — o áudio segue o caminho do DAW, então sends, returns e grupos continuam valendo — e se mostra na console.
class MesaProcessor : public juce::AudioProcessor,
                      public mesa::Canal,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit MesaProcessor (bool console);
    ~MesaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return isConsole ? "MESA" : "MESA CANAL"; }
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

    // ---- mesa::Canal (a console lê e mexe por aqui; também a própria janela) ----
    int   abi() const override { return mesa::Ponte::abiVersao; }
    int   tipo() const override { return isConsole ? 2 : juce::jlimit (0, 1, juce::roundToInt (pMode->load())); }
    int   numero() const override { return seqNo; }
    int   posicao() const override;
    void  posiciona (int n) override;
    bool  tem (const char* pid) const override { return apvts.getParameter (pid) != nullptr; }
    float valor (const char* pid) const override;
    float norm (const char* pid) const override;
    float normPadrao (const char* pid) const override;
    void  mexe (const char* pid, float v) override;
    void  gesto (const char* pid, bool inicio) override;
    int   texto (const char* pid, char* out, int n) const override;
    int   nome (char* out, int n) const override;
    void  renomeia (const char* utf8) override;
    std::uint32_t cor() const override;
    void  medidores (float* out8) const override;

    // ---- para a janela ----
    const bool isConsole;
    int  mode() const     { return isConsole ? mesa::modeMaster : tipo(); }
    juce::String displayName() const;                 // nome escrito na fita (o do usuário, senão o da faixa do DAW)
    juce::String userName() const;

    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<mesa::Registry> registry;   // os CANAIS deste binário (SOLO)

    // medidores (dBFS), com a balística feita aqui no áudio (a tela lê a 30 Hz e perderia os blocos do meio):
    // pico com queda de 26 dB/s, VU (média retificada, 300 ms, calibrada para a senoide ler o RMS), PPM (queda 12 dB/s)
    std::atomic<float> peakL { -120.0f }, peakR { -120.0f }, rmsL { -120.0f }, rmsR { -120.0f }, grDb { 0.0f }, inPeak { -120.0f },
                       inVu { -120.0f }, ppm { -120.0f };
    std::atomic<int> faixaHost { 0 };      // número da faixa que o DAW informou (VST3: channel index)
    std::atomic<int> posManual { 0 };      // número escolhido à mão na console (0 = automático)
    std::atomic<int> uiW { 0 }, uiH { 0 };  // tamanho da janela (0 = ainda não abriu: a tela escolhe)
    std::atomic<bool> uiGeral { false };     // CONSOLE: vista GERAL (todos os canais na largura)
    std::atomic<float> uiZoom { 1.0f };      // CONSOLE: zoom da vista PADRÃO

    // CONSOLE: últimas amostras do master para o goniômetro (escritas no áudio, lidas pela tela)
    static constexpr int gonioN = 2048;
    float gonioL[gonioN] {}, gonioR[gonioN] {};
    std::atomic<int> gonioPos { 0 };

private:
    void parameterChanged (const juce::String& pid, float) override;
    void updateSettings();
    void processChannel (juce::AudioBuffer<float>& buf, int n, int nch);
    void processMaster (juce::AudioBuffer<float>& buf, int n, int nch);
    std::atomic<float>* raw (const juce::String& pid);
    bool textoModelo (const juce::String& pid, juce::String& t) const;

    double fs = 48000.0;
    int maxBlock = 512;
    int seqNo = 0;

    mesa::Hpf18 hpf;
    mesa::Lpf12 lpf;
    mesa::PreSat sat;
    mesa::PreColour colour;
    mesa::Eq eq;
    mesa::Dyn dyn;          // compressor do canal/grupo (VCA 4K, COLA 4K, FET 76, OPTO 2A)
    mesa::Gate gate;
    mesa::BusComp bus;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> ecoDelay { 64 };   // ECO: mesma latência do HQ
    int osLatency = 0;

    juce::SmoothedValue<float> gainSm, driveSm, gainL, gainR;
    // EQ sem degrau: os knobs andam até o alvo em ~25 ms e os coeficientes são refeitos a cada 32 amostras
    mesa::EqSettings eqAlvo, eqAgora;
    int eqModelo = 0;
    bool eqPula = true;
    // balística dos medidores
    float vuEst[3] {}, pkEst[3] { 1e-6f, 1e-6f, 1e-6f }, ppmEst = 1e-6f, grEst = 0.0f;
    void medeSaida (const float* const* d, int nch, int n);
    void medeEntrada (const float* const* d, int nch, int n);
    std::atomic<bool> dirty { true };
    int lastPre = -1, lastEq = -1;
    bool soloWas = false;

    juce::CriticalSection nameLock;
    juce::String hostName, nameOverride;
    juce::Colour hostColour { 0xff9aa0a8 };

    std::atomic<float> semParam[1] { { 0.0f } };   // parâmetro que este plugin não tem (CONSOLE x CANAL) lê zero
    std::atomic<float>* pMode; std::atomic<float>* pGrp; std::atomic<float>* pPre; std::atomic<float>* pGain;
    std::atomic<float>* pDrive; std::atomic<float>* pPad; std::atomic<float>* pHpf; std::atomic<float>* pEqOn;
    std::atomic<float>* pEqm; std::atomic<float>* pThr; std::atomic<float>* pRat; std::atomic<float>* pGate;
    std::atomic<float>* pCmix; std::atomic<float>* pPan; std::atomic<float>* pMute; std::atomic<float>* pSolo;
    std::atomic<float>* pFader; std::atomic<float>* pMthr; std::atomic<float>* pMatk; std::atomic<float>* pMrel;
    std::atomic<float>* pMmkp; std::atomic<float>* pMrat; std::atomic<float>* pMmix; std::atomic<float>* pMin;
    std::atomic<float>* pMhpf; std::atomic<float>* pHq; std::atomic<float>* pDynPre;
    std::atomic<float>* pEqG[4]; std::atomic<float>* pEqF[4];
    std::atomic<float>* pLpf; std::atomic<float>* pDynm; std::atomic<float>* pAtk; std::atomic<float>* pRel;
    std::atomic<float>* pQ[2]; std::atomic<float>* pBell[2];
    int dynModelo = -1;

    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o som passa direto

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MesaProcessor)
};

// o CANAL criado direto (a foto da CONSOLE abre CANAIS no mesmo programa)
juce::AudioProcessor* mesaCriaCanal();
