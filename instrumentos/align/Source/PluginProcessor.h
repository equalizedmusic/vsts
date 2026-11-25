// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "IspLicenca.h"
#include "ChuteLink.h"

// ALIGN: vai no canal do BAIXO; o KICK entra pelo sidechain. Um botão só (ALIGN) liga o corretor automático de fase
// kick/baixo com o LEVEL GUARD. Latência zero. Desligado, o som passa sem nenhuma alteração.
// 0.3.0: sem sidechain, o kick vem do CHUTE do mesmo projeto pelo LINK (ChuteLink.h). Prioridade: sidechain com som >
// LINK > aviso "ligue o sidechain". DUCK: sidechain clássico com dosagem.
class AlignProcessor : public juce::AudioProcessor
{
public:
    AlignProcessor();
    ~AlignProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ALIGN"; }
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
    align::Engine engine;
    double sampleRate() const { return fs; }

    std::atomic<float> bpm { 0.0f };          // do host (0 = não informou)
    std::atomic<bool> scLigado { false };     // o host ligou o sidechain
    enum Fonte { semKick = 0, fonteSidechain, fonteLink };
    std::atomic<int> fonte { semKick };       // de onde vem o kick agora
    std::atomic<int> linkVaga { -1 };         // vaga do CHUTE seguida (−1 = nenhuma)
    void preferirChute (const juce::String& nome);   // vários CHUTEs: qual seguir (vazio = o primeiro)
    juce::String chutePreferido() const;
    std::atomic<bool> verGrave { true };      // osciloscópio: só o grave (LOW) ou tudo (FULL)
    std::atomic<int> uiW { 0 };               // largura da janela (0 = ainda não abriu)

    bool alignLigado() const { return pAlign->load() > 0.50000000000000000819342f; }
    void alterna();                           // o botão ALIGN
    void alternaParam (const juce::String& id);   // PHASE / POLARITY / LEVEL GUARD
    bool ligadoParam (const char* id) const { auto* v = apvts.getRawParameterValue (id); return v != nullptr && v->load() > 0.5f; }
    float valorParam (const char* id) const { auto* v = apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.0f; }

private:
    std::atomic<float>* pAlign = nullptr;
    std::atomic<float> *pPhase = nullptr, *pPolarity = nullptr, *pGuard = nullptr, *pDepth = nullptr, *pReact = nullptr, *pXoMax = nullptr,
                       *pDuck = nullptr;
    chutelink::Leitor link;
    std::vector<float> kickLink;
    int64_t semScDesde = 1 << 30;   // amostras desde a última vez que o sidechain teve som
    bool linkNoBlocoAnterior = false;   // o bloco anterior passou pelo render do LINK (senão as âncoras estão velhas)
    juce::String prefNome;
    std::atomic<bool> prefNovo { false };
    juce::SpinLock prefTrava;
    void aplicaDose();
    double fs = 48000.00000000000230370;
    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o som passa direto

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignProcessor)
};
