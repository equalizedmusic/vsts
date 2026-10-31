#pragma once

#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"

// Tela do ALIGN (base 1000 × 620, escala junto com a janela). Tudo desenhado no paint, a 30 Hz:
// osciloscópio de 2 tempos travado no kick, zoom kick → b com a soma e o alvo, LOW END, CORRELATION,
// CORRECTION (AUTO), NOTE MEMORY e o botão ALIGN. Paleta: fundo escuro arroxeado (a tela do osciloscópio quase
// preta), kick LARANJA, baixo AZUL, corretor/em fase VERDE, marca e memória ROXO.
class AlignEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AlignEditor (AlignProcessor&);
    ~AlignEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int baseW = 1000, baseH = 620;

private:
    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }

    void topo (juce::Graphics&);
    void osciloscopio (juce::Graphics&, juce::Rectangle<float> r);
    void zoom (juce::Graphics&, juce::Rectangle<float> r);
    void painelLowEnd (juce::Graphics&, juce::Rectangle<float> r);
    void painelCorr (juce::Graphics&, juce::Rectangle<float> r);
    void painelCorrecao (juce::Graphics&, juce::Rectangle<float> r);
    void painelNotas (juce::Graphics&, juce::Rectangle<float> r);
    void base (juce::Graphics&);
    void menu();

    // janela da foto: começo (amostra absoluta) e duração do trecho de 2 tempos
    bool janela (std::int64_t& ini, double& batida) const;
    float lido (const std::vector<float>& v, double pos) const;   // amostra interpolada (cúbica) do anel

    AlignProcessor& proc;
    isp::TelaAtivacao ativacao;
    float escalaScope = 1.0f, escalaZoom = 1.0f;

    // alvos de clique (coordenadas da tela base)
    juce::Rectangle<float> rBotao, rFull, rLow, rMenu;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignEditor)
};
