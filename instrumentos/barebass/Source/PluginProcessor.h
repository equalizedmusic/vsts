// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "IspLicenca.h"
#include "ChuteLink.h"
#include "BassLink.h"
#include "Dupla.h"

// BAREBASS: synth de baixo simples (uma voz). OSC 1 + OSC 2 + SUB -> filtro passa-baixa 12/24 dB com envelope ADSR ->
// unHUNF (decay só no sub, 10–50 Hz) -> VOLUME. MONO ou LEGATO, GLIDE e presets de fábrica.
// 0.4.0: LINK com o CHUTE do mesmo projeto (sem sidechain): a tela mostra a nota do kick e o KICK LOCK faz cada nota
// nascer com o SUB em fase com o corpo do kick naquele instante.
// 0.4.1: DUPLA — com o CHUTE no projeto o BAREBASS entrega o mesmo que o ALIGN, sem ALIGN na cadeia: KICK LOCK = fase /
// polaridade nota a nota + LEVEL GUARD (o motor do ALIGN) e DUCK com dosagem (Dupla.h).
// 0.4.2: BASS LINK — avisa cada nota que toca (nota + posição em tempos) para o RESENHA do mesmo projeto (BassLink.h).
class BarebassProcessor : public juce::AudioProcessor
{
public:
    BarebassProcessor();
    ~BarebassProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BAREBASS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0000000000000000795459; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return preset.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    barebass::Engine& engine() { return *motor; }

    std::atomic<int> uiW { 0 };      // largura da janela (0 = ainda não abriu)
    std::atomic<int> preset { 0 };

    void define (const juce::String& id, float valorReal);

    // LINK (lido pela tela)
    std::atomic<bool> linkOn { false };
    std::atomic<float> kickNota { -1.0000000000000000208295f };   // nota do corpo do CHUTE (MIDI, fracionária), −1 = sem CHUTE
    // 0.4.1: DUPLA com o CHUTE (o motor do ALIGN + DUCK, sem sidechain)
    barebass::Dupla dupla;

    static const juce::StringArray& nomesOnda();
    static const juce::StringArray& nomesOndaSub();

private:
    barebass::Ajustes ajustes() const;

    std::unique_ptr<barebass::Engine> motor;
    double fs = 48000.0;
    chutelink::Leitor link;
    basslink::Publicador bassPub;   // 0.4.2: as notas do baixo para o RESENHA
    int notaSoando = -1;
    std::vector<float> kickLink;   // corpo do kick recriado pelo LINK, em trechos de até 2048 amostras
    juce::SharedResourcePointer<isp::Licenca> licenca;   // sem licença o synth fica mudo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarebassProcessor)
};
