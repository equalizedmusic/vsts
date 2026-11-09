#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

namespace maestro { int tituloDaJanela (void* nativo, char* out, int n); }

// MAESTRO — MIX CONDUCTOR. Uma instância em cada faixa do projeto; todas se enxergam (maestro::Registry).
// Por faixa (parâmetros, automatizáveis): ROLE, FOCUS e TRIM.
// Do projeto (valem para todas as faixas, guardados no estado de cada uma): STYLE, AMOUNT, MAX CUT, SPEED,
// LOW-END GUARD, KEEP KICK / BASS ON TOP, A/B e o LEARN / LOCK.
// Latência zero. Antes do LOCK e sem FOCUS, o som passa sem nenhuma alteração.
class MaestroProcessor : public juce::AudioProcessor
{
public:
    MaestroProcessor();
    ~MaestroProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MAESTRO"; }
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
    void updateTrackProperties (const TrackProperties&) override;

    juce::AudioProcessorValueTreeState apvts;
    maestro::Engine& engine() { return *motor; }
    maestro::Slot& slot() { return motor->slot(); }

    std::atomic<int> uiW { 0 };

    bool emFocus() const { return pFocus->load() > 0.5f; }
    void alternaFocus();
    void defineFocus (bool on);
    void define (const juce::String& id, float valorReal);
    void nomeDaJanela (const juce::String& titulo);   // título da janela do plugin (Ableton)

    static const juce::StringArray& nomesRole();      // "AUTO" + os 9 papéis

private:
    void aplicaNome();

    std::unique_ptr<maestro::Engine> motor;
    std::atomic<float>* pRole = nullptr, *pFocus = nullptr, *pTrim = nullptr;
    juce::String nomeHost, nomeJanela;               // thread da interface

    JUCE_DECLARE_WEAK_REFERENCEABLE (MaestroProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MaestroProcessor)
};
