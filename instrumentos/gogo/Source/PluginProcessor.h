#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

// GOGÓ: cadeia de voz automática. Vai no canal da VOZ (uma voz só). CLEAN → TUNE (motor do ONKEY) → RIDE → FET → OPTO
// → TONE → DE-ESS → WARMTH → SPACE, cada bloco com liga/desliga. As macros embaixo (TUNE, TAME, TONE, AIR, DE-ESS,
// WARMTH, SPACE, OUTPUT) mexem em vários ajustes de uma vez; os ajustes de cada bloco abrem clicando no bloco.
// STYLE grava um ponto de partida nos ajustes; ROLE muda o comportamento (LEAD, DOUBLE, ADLIB, SPOKEN).
// LEARN: canta 15 a 20 s e ele acerta o INPUT para a voz chegar a −18 dBFS RMS (o resto da cadeia é calibrado nisso).
// Latência de ~45 ms (o TUNE), compensada pelo DAW; TRACKING tira o TUNE do caminho e a latência vai a 0 para gravar.
// MAESTRO LINK: lê os grupos do MAESTRO no mesmo DAW e põe um realce de presença onde os SYNTHS deixam espaço.
class GogoProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    GogoProcessor();
    ~GogoProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GOGO"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    gogo::Cadeia& cadeia() { return *motor; }
    double sampleRate() const { return fs; }
    int latenciaAgora() const { return latAtual.load(); }

    // tela
    std::atomic<int> uiW { 0 };          // largura da janela (0 = ainda não abriu)
    std::atomic<int> sel { -1 };         // bloco com os ajustes abertos (−1 = gráfico)

    void define (const juce::String& id, float valorReal);
    void alterna (const juce::String& id);
    bool liga (const juce::String& id) const;
    float valor (const juce::String& id) const;

    void aplicaStyle (int s);            // menu STYLE: grava o ponto de partida do estilo nos ajustes
    void learn();                        // botão LEARN: começa / termina
    bool aprendendo() const { return motor->med.learn.load() != 0; }
    int learnVezes() const { return learnFeitos.load(); }

    // A/B: dois estados completos; trocar guarda o atual no slot dele
    int abAtual() const { return ab.load(); }
    void trocaAB (int para);
    void copiaAB();                      // copia o slot atual para o outro

    // MAESTRO LINK
    int linkGrupos() const { return nGrupos.load(); }
    juce::String linkStyle() const { const juce::ScopedLock l (trava); return styleMaestro; }

    static const juce::StringArray& nomesTom();
    static const juce::StringArray& nomesEscala();
    static const char* idBloco (int b);   // "on_clean" ...

private:
    void timerCallback() override;
    void leLink();
    juce::ValueTree fotoParametros() const;
    void aplicaFoto (const juce::ValueTree& v);

    std::unique_ptr<gogo::Cadeia> motor;
    double fs = 48000.0;
    std::atomic<int> latAtual { 0 };
    std::atomic<int> pedidoLearn { 0 };   // 1 = começar, 2 = terminar (o áudio executa)
    std::atomic<int> learnFeitos { 0 };
    int learnVisto = 0;

    std::atomic<int> ab { 0 };
    juce::ValueTree slots[2];

    // MAESTRO LINK (lido no timer)
    std::atomic<float> pocketHz { 0.0f };
    std::atomic<int> nGrupos { 0 };
    juce::CriticalSection trava;
    juce::String styleMaestro;
    uint32_t batidaVista[256] {};
    double batidaQuando[256] {};
    int contaLink = 0;

    struct Ponteiros
    {
        std::atomic<float>* on[gogo::numBlocos] {};
        std::atomic<float> *bypass = nullptr, *tracking = nullptr, *natural = nullptr, *link = nullptr, *role = nullptr,
                           *input = nullptr, *output = nullptr, *tune = nullptr, *tame = nullptr, *tone = nullptr, *air = nullptr,
                           *deess = nullptr, *warmth = nullptr, *space = nullptr, *hpfauto = nullptr, *hpf = nullptr, *gate = nullptr,
                           *speed = nullptr, *humanize = nullptr, *vibrato = nullptr, *formant = nullptr, *root = nullptr, *scale = nullptr,
                           *riderange = nullptr, *fetratio = nullptr, *fetrel = nullptr, *optoemph = nullptr, *mud = nullptr,
                           *harsh = nullptr, *deessfreq = nullptr, *deesslisten = nullptr, *warmtype = nullptr, *delaynote = nullptr,
                           *feedback = nullptr, *decay = nullptr, *duck = nullptr, *blend = nullptr;
    } p;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GogoProcessor)
};
