#pragma once

#include "PluginProcessor.h"

// Tela do BAREBASS (base 1000 × 606, escala junto com a janela). Tudo desenhado no paint, a 30 Hz, no molde do ALIGN.
// Em cima, o que está acontecendo AGORA: a onda (dois ciclos, antes do filtro e na saída), a curva do filtro com o
// corte deste instante e o envelope do filtro com o ponto onde a nota está. No meio, OSC 1 / OSC 2 / SUB; embaixo,
// FILTER, FILTER ENV, unHUNF e MASTER (com o seletor MONO / LEGATO). O LED de OSC 1 / OSC 2 / SUB liga e desliga. Paleta do ALIGN, com VERDE (saída / filtro) e ROXO (marca, sub, envelope).
class BarebassEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BarebassEditor (BarebassProcessor&);
    ~BarebassEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseW = 1000, baseH = 606;

private:
    struct Knob { juce::String id; juce::Point<float> c; float r; };
    struct Botao { juce::Rectangle<float> r; juce::String id; float valor; };

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }

    void topo (juce::Graphics&);
    void painelOnda (juce::Graphics&, juce::Rectangle<float> r);
    void painelCurvaFiltro (juce::Graphics&, juce::Rectangle<float> r);
    void painelCurvaEnv (juce::Graphics&, juce::Rectangle<float> r);
    void painelOsc (juce::Graphics&, juce::Rectangle<float> r, int qual);
    void painelSub (juce::Graphics&, juce::Rectangle<float> r);
    void painelFiltro (juce::Graphics&, juce::Rectangle<float> r);
    void painelEnv (juce::Graphics&, juce::Rectangle<float> r);
    void painelHunf (juce::Graphics&, juce::Rectangle<float> r);
    void painelMaster (juce::Graphics&, juce::Rectangle<float> r);
    void tituloLiga (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor, const juce::String& idLiga);
    void apaga (juce::Graphics&, juce::Rectangle<float> r);   // painel desligado: véu por cima (menos o título)

    void knob (juce::Graphics&, const juce::String& id, juce::Point<float> c, float raio, juce::Colour cor, bool bipolar);
    void knobRotulado (juce::Graphics&, const juce::String& id, const juce::String& nome, juce::Point<float> c, float raio,
                       juce::Colour cor, bool bipolar, bool destaque = false);
    void seletorOnda (juce::Graphics&, juce::Rectangle<float> r, const juce::String& id, int n, juce::Colour cor);
    juce::String texto (const juce::String& id) const;   // valor formatado do parâmetro
    void menu();
    void menuPreset();

    juce::RangedAudioParameter* param (const juce::String& id) const;
    float real (const juce::String& id) const;
    int escolha (const juce::String& id) const { return juce::roundToInt (real (id)); }

    BarebassProcessor& proc;

    // última onda mostrada (fica na tela, apagada, depois que a nota acaba)
    static constexpr int pontos = 360;
    float cAntes[pontos] {}, cDepois[pontos] {}, cSub[pontos] {};
    bool temOnda = false;
    float escVis = 0.3f, freqOnda = 0.0f;

    // alvos de clique (coordenadas da tela base)
    juce::Rectangle<float> rMenu, rPreset;
    std::vector<Knob> knobs;
    std::vector<Botao> botoes;
    juce::String arrastando;
    float valorIni = 0.0f, yIni = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarebassEditor)
};
