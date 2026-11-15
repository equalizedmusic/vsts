#pragma once

#include "PluginProcessor.h"

// Tela do GOGÓ (base 1200 × 760, escala junto com a janela), no layout do esboço v2 aprovado em 15/11/2026.
// Tudo desenhado no paint, a 30 Hz:
//  - topo: marca, STYLE ▾, ROLE ▾, KEY, LATENCY, MAESTRO e o menu ≡;
//  - meio: VOCAL DYNAMICS (últimos 8 s: IN cinza, OUT rosa, RIDE âmbar, GAIN REDUCTION lilás) ou, com um bloco
//    aberto, os AJUSTES dele (no TUNE, o gráfico de afinação do ONKEY); à direita o VU do OPTO e as reduções;
//  - a cadeia: 9 blocos com botão de power, 3 leituras cada; clique no bloco abre os ajustes;
//  - embaixo: LEARN, as 8 macros, KEEP IT NATURAL / TRACKING / MAESTRO LINK, A / B e BYPASS.
class GogoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GogoEditor (GogoProcessor&);
    ~GogoEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseW = 1200, baseH = 760;

private:
    struct Knob { juce::String id; juce::Point<float> c; float r; };
    struct Alvo { juce::Rectangle<float> r; std::function<void()> f; };

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }

    void topo (juce::Graphics&);
    void dinamica (juce::Graphics&, juce::Rectangle<float> r);
    void medidores (juce::Graphics&, juce::Rectangle<float> r);
    void ajustes (juce::Graphics&, juce::Rectangle<float> r, int bloco);
    void afinacao (juce::Graphics&, juce::Rectangle<float> r);
    void cadeia (juce::Graphics&, juce::Rectangle<float> r);
    void base (juce::Graphics&);

    void knob (juce::Graphics&, const juce::String& id, juce::Point<float> c, float raio, juce::Colour cor, bool bipolar);
    void controle (juce::Graphics&, const juce::String& id, const juce::String& nome, const juce::String& desc,
                   juce::Rectangle<float> r, juce::Colour cor);
    void menuEscolha (const juce::String& id, const juce::StringArray& itens);
    void menuStyle();
    void menu();

    juce::RangedAudioParameter* param (const juce::String& id) const;
    float real (const juce::String& id) const;
    int escolha (const juce::String& id) const { return juce::roundToInt (real (id)); }
    juce::String texto (const juce::String& id) const;   // valor formatado
    void tomAtual (int& tom, int& esc) const;
    void leituras (int bloco, juce::String (&rot)[3], juce::String (&val)[3]) const;

    GogoProcessor& proc;
    float centroVis = 64.0f;
    std::vector<Knob> knobs;
    std::vector<Alvo> alvos;
    juce::String arrastando;
    float valorIni = 0.0f, yIni = 0.0f;
    int quadro = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GogoEditor)
};
