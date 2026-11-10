#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Dsp.h"
#include "../../maestro/Source/MaestroLink.h"   // MAESTRO LINK: lê os 8 grupos do MAESTRO (outro binário)

// MASTER DOS MAGOS — MASTERING WIZARD (par do MAESTRO). Vai no master, por último.
// Parâmetros (automatizáveis): AMOUNT, CHARACTER, PUNCH, WIDTH, TONE TILT, BASS MONO e LISTEN TO (A / B / REF / BYPASS);
// MANUAL do multibanda (mbman + mb<k>thr/rat/atk/rel/gain) e da DYNAMIC EQ (eqman + eq<z>on/rng/thr).
// Do plugin (estado): STYLE, TARGET, MATCH TONE / MATCH LOUDNESS, a REFERÊNCIA (análise + caminho do arquivo) e o CAST.
// O monitor de lugares (PHONE, EARBUDS, CAR, MONO) NÃO é salvo: abre sempre desligado.
class MagosProcessor : public juce::AudioProcessor
{
public:
    MagosProcessor();
    ~MagosProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MASTER DOS MAGOS"; }
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

    juce::AudioProcessorValueTreeState apvts;
    magos::Engine motor;
    std::atomic<int> uiW { 0 };
    std::atomic<int> sim { 0 };                       // monitor de lugares (não salvo)

    void define (const juce::String& id, float valorReal);
    float valor (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    // REFERÊNCIA: lê e analisa numa thread à parte; quando termina, troca a do motor
    void carregaReferencia (const juce::File& f);
    void tiraReferencia();
    juce::String nomeReferencia() const;
    std::atomic<int> carregando { 0 };                // 1 = lendo / analisando
    juce::String erroReferencia;                      // thread da interface
    juce::File arquivoReferencia;
    void recebeReferencia (std::shared_ptr<magos::Referencia> r, const juce::String& erro);   // thread da interface

private:
    void trocaReferencia (std::unique_ptr<magos::Referencia> nova);

    std::atomic<float>* pAmount = nullptr, *pChar = nullptr, *pPunch = nullptr, *pWidth = nullptr, *pTilt = nullptr,
                      *pMono = nullptr, *pOuvir = nullptr, *pMbMan = nullptr, *pEqMan = nullptr;
    std::atomic<float>* pMb[magos::NM][5] {};       // THRESH, RATIO, ATTACK, RELEASE, GAIN
    std::atomic<float>* pEq[magos::NZ][3] {};       // ON, RANGE, THRESH
    std::vector<std::unique_ptr<magos::Referencia>> refs;   // a atual é a última; as antigas ficam até o fim (o áudio pode estar lendo)
    std::unique_ptr<juce::Thread> leitor;
    double taxa = 48000.0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (MagosProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagosProcessor)
};
