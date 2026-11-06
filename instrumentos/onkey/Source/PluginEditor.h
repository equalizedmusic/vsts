#pragma once

#include "PluginProcessor.h"

// Tela do ONKEY (base 1000 × 620, escala junto com a janela). Tudo desenhado no paint, a 30 Hz, no molde do ALIGN:
// gráfico de altura (últimos 4 s) com a voz ORIGINAL (laranja) e a CORRIGIDA (verde) sobre as faixas da escala (roxo,
// tônica mais forte); à direita KEY (tom detectado ou manual), ROOT / SCALE, NOTE NOW e o AJUSTE FINO; embaixo o botão
// ONKEY e o AMOUNT. O AJUSTE FINO (RETUNE SPEED, HUMANIZE, VIBRATO, FORMANT, OUTPUT) abre por cima do gráfico.
// Paleta do ALIGN, com VERDE (corrigido / ligado) e ROXO (marca, tom, escala) como cores principais.
class OnkeyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OnkeyEditor (OnkeyProcessor&);
    ~OnkeyEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseW = 1000, baseH = 620;

private:
    struct Knob { juce::String id; juce::Point<float> c; float r; };

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }

    void topo (juce::Graphics&);
    void grafico (juce::Graphics&, juce::Rectangle<float> r);
    void painelFino (juce::Graphics&, juce::Rectangle<float> r);
    void painelTom (juce::Graphics&, juce::Rectangle<float> r);
    void painelEscolha (juce::Graphics&, juce::Rectangle<float> r);
    void painelNota (juce::Graphics&, juce::Rectangle<float> r);
    void painelBotaoFino (juce::Graphics&, juce::Rectangle<float> r);
    void base (juce::Graphics&);
    void knob (juce::Graphics&, const juce::String& id, juce::Point<float> c, float raio, juce::Colour cor, bool bipolar);
    void menu();
    void menuEscolha (bool tom);

    juce::RangedAudioParameter* param (const juce::String& id) const;
    float real (const juce::String& id) const;
    int escolha (const juce::String& id) const { return juce::roundToInt (real (id)); }

    // tom e escala que estão valendo (AUTO ou manual); −1 = ainda não sabe
    void tomAtual (int& tom, int& esc) const;

    OnkeyProcessor& proc;
    float centroVis = 64.0f;   // nota no meio do gráfico (acompanha a voz devagar)

    // alvos de clique (coordenadas da tela base)
    juce::Rectangle<float> rBotao, rMenu, rRoot, rScale, rFino;
    std::vector<Knob> knobs;
    juce::String arrastando;
    float valorIni = 0.0f, yIni = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnkeyEditor)
};
