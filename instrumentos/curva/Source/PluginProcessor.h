#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Curva.h"
#include "IspLicenca.h"

// CURVA: monitor de volume pela curva de mesma audibilidade (Fletcher-Munson / ISO 226:2003). Vai no MASTER, por
// último, só para OUVIR: quem mixa baixinho (LISTEN) escuta o equilíbrio que ouviria no volume de referência
// (TARGET, 83 dB). Filtro de fase mínima (latência zero). Os medidores de LOUDNESS e TRUE PEAK leem a MIX (antes da
// CURVA), que é o que vai para a exportação; o espectro mostra a mix (IN) e o que sai para o monitor (OUT).
class CurvaProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    CurvaProcessor();
    ~CurvaProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CURVA"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    double sampleRate() const { return fs; }
    curva::Ajuste ajuste() const;            // LISTEN / TARGET / AMOUNT agora
    float autoGanho() const { return autoDb.load(); }
    bool ligado() const { return pOn->load() > 0.5f; }

    // ── para a tela ──
    // anéis do espectro: mix (IN, mono L+R) e monitor (OUT, mono L+R); escrita pelo áudio, leitura pela tela
    static constexpr int anel = 1 << 15, anelMask = anel - 1;
    std::vector<float> anelIn, anelOut;
    std::atomic<int> escrita { 0 };
    // vectorscope (mix): pares L/R
    static constexpr int anelSt = 4096;
    std::vector<float> stL, stR;
    std::atomic<int> escritaSt { 0 };
    // picos e RMS por canal (linear): [0..1] = IN L/R, [2..3] = OUT L/R. Pico = maior do bloco (a tela faz a queda)
    std::atomic<float> pico[4], rms[4];
    std::atomic<float> truePeakMax { 0.0f }, truePeak { 0.0f }, correlacao { 1.0f }, reducaoSafe { 0.0f };
    curva::Loudness loud;
    void zeraMedidores();                    // botão RESET (I, LRA, máximos)

    // estado só da tela (salvo no projeto)
    std::atomic<int> uiW { 0 }, suave { 2 }, tilt { 2 };   // suave: 1/3, 1/6, 1/12, 1/24 oct; tilt: 0, 3, 4.5 dB/oct
    std::atomic<bool> verIn { true }, verOut { true }, verHold { true };

    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o som passa direto

private:
    void timerCallback() override;           // refaz o filtro quando LISTEN/TARGET/AMOUNT mudam
    void refazFiltro (bool agora);

    std::atomic<float> *pListen = nullptr, *pTarget = nullptr, *pAmount = nullptr, *pOutput = nullptr, *pAuto = nullptr,
                       *pSafe = nullptr, *pOn = nullptr;
    juce::dsp::Convolution conv;             // latência zero (partição uniforme com cabeça direta)
    curva::Ajuste feito { -1, -1, -1 };
    double fsFeito = 0;
    std::atomic<float> autoDb { 0.0f };
    juce::AudioBuffer<float> seco;
    juce::SmoothedValue<float> mistura, ganho;
    curva::TruePeak tp;
    curva::Seguranca safe;
    double fs = 48000.0, msL = 0, msR = 0, msLo = 0, msRo = 0, cLR = 0, cLL = 0, cRR = 0, coefRms = 0.999, coefCor = 0.999;
    juce::CriticalSection trava;             // prepareToPlay x timer (o filtro é refeito fora do áudio)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvaProcessor)
};
