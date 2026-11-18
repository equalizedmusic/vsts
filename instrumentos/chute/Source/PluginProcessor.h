#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "ChuteLink.h"

// CHUTE: kick designer. Cada nota MIDI dispara uma batida: ATAQUE do preset + CORPO sintetizado (afinável). Latência
// zero. Presets de fábrica = os 50 modelos (bancos SUB / PUNCH / TIGHT / BOOM); mexeu em qualquer knob, o nome ganha
// "*" e o SAVE AS grava como preset novo no banco USER (arquivo .chute na pasta do usuário).
// LINK: cada batida vai para a memória compartilhada (ChuteLink.h): o ALIGN e o BAREBASS do mesmo projeto acham o
// kick sozinhos, sem sidechain.
class ChuteProcessor : public juce::AudioProcessor
{
public:
    ChuteProcessor();
    ~ChuteProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CHUTE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    // programas do host = os 50 presets de fábrica (na ordem do menu)
    int getNumPrograms() override { return chute::numModelos; }
    int getCurrentProgram() override { return juce::jmax (0, ordemDe (preset.modelo)); }
    void setCurrentProgram (int i) override { carregaFabrica (modeloNaOrdem (i)); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateTrackProperties (const TrackProperties& p) override;

    juce::AudioProcessorValueTreeState apvts;
    chute::Engine& engine() { return *motor; }
    chute::Ajustes ajustes() const;
    std::atomic<int> uiW { 0 };

    // ---- presets ----
    struct Preset
    {
        juce::String banco = "BOOM", nome;
        bool user = false;
        int modelo = 0;
        juce::File arquivo;
        std::map<juce::String, float> valores;   // valor real de cada parâmetro quando o preset abriu (o "*")
    };
    Preset preset;                                // o que está carregado (mexer na lista só na thread da mensagem)
    static const juce::StringArray& bancos();     // SUB, PUNCH, TIGHT, BOOM (ordem do menu)
    static int ordemDe (int modelo);              // posição no menu (bancos na ordem) ↔ índice do modelo
    static int modeloNaOrdem (int ordem);
    static juce::String nomeNota (double midi, bool comCents = false);   // F1, F#1 (cifra; 29 = F1)
    void carregaFabrica (int modelo);
    bool carregaUser (const juce::File& f);
    bool salvaComo (const juce::String& nome);
    juce::String sugestaoNome() const;            // "MARTELO 2"
    static juce::File pastaUser();
    juce::Array<juce::File> presetsUser() const;
    bool alterado() const;                        // algum parâmetro longe do valor do preset
    float valorDoPreset (const juce::String& id) const;   // duplo clique no knob volta para ele
    void define (const juce::String& id, float valorReal);
    void proximo (int passo);                     // ◀ ▶: fábrica e USER em sequência

    // LINK
    bool linkLigado() const { return link.ligado(); }
    juce::String linkNome() const;

private:
    void snapshot();
    double fs = 48000.0;
    std::unique_ptr<chute::Engine> motor;
    chutelink::Publicador link;
    juce::String nomeFaixa;
    std::atomic<bool> nomeNovo { false };
    juce::SpinLock nomeTrava;
    std::vector<float> faseTmp = std::vector<float> (chute::nPts), ampTmp = std::vector<float> (chute::nPts);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChuteProcessor)
};
