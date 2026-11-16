#pragma once

#include "PluginProcessor.h"

// Tela do RETALHO (base 1200 × 620, escala junto com a janela), no layout do esboço v2 aprovado em 15/11/2026:
//  - topo: marca, abas EDIT / COMPLEX / GROOVE, PRESET ▾, KEY, TEMPO, LATENCY, CAPTURE e o menu ≡;
//  - linha de botões da aba (SHAPE, LENGTH, RATE… / STEPS, READ, DICE MODE / CHOPS, CUT, STYLE…);
//  - display de onda: ORIGINAL (a voz capturada, ataques ▼, CHUNK em destaque) e RESULT (o fill renderizado em
//    blocos, PITCH ciano, FILTER rosa; no COMPLEX os passos; no GROOVE os 8 chops e o pattern);
//  - painéis de knobs da aba e o rodapé LIVE fixo (AUTO / HOLD / ONE-SHOT, ONDE, CATCH, 6 pads, DRY/WET, OUTPUT, A/B).
// Tudo desenhado no paint, a 30 Hz.
class RetalhoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RetalhoEditor (RetalhoProcessor&);
    ~RetalhoEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseW = 1200, baseH = 620;

private:
    struct Knob { int campo; juce::Point<float> c; float r; };
    struct Alvo { juce::Rectangle<float> r; std::function<void()> f; };
    enum { arNada, arKnob, arGrade, arLivre, arPasso, arCelula, arDaw };

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }

    void topo (juce::Graphics&);
    void linha (juce::Graphics&);
    void original (juce::Graphics&, juce::Rectangle<float> r);
    void result (juce::Graphics&, juce::Rectangle<float> r);
    void resultEdit (juce::Graphics&, juce::Rectangle<float> in);
    void resultComplex (juce::Graphics&, juce::Rectangle<float> in);
    void resultGroove (juce::Graphics&, juce::Rectangle<float> in);
    void paineis (juce::Graphics&);
    void rodape (juce::Graphics&);

    // peças
    float grupo (juce::Graphics&, float x, float y, const juce::String& titulo, int campo, const juce::StringArray& nomes);
    void botao (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, bool on, juce::Colour cor, std::function<void()> f, float fonteH = 10.5f);
    void liga (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, int campo, juce::Colour cor);
    void knob (juce::Graphics&, int campo, const juce::String& nome, juce::Point<float> c, juce::Colour cor);
    void painel (juce::Graphics&, juce::Rectangle<float> r, const juce::String& titulo, const juce::String& sub);
    float chip (juce::Graphics&, float xDir, const juce::String& rot, const juce::String& val, juce::Colour cor, std::function<void()> f);
    void menuEscolha (int campo);
    void menuPresets();
    void menu();
    void menuPad (int i);

    float real (int campo) const { return proc.valor (campo); }
    int escolha (int campo) const { return juce::roundToInt (proc.valor (campo)); }
    juce::String texto (int campo) const;
    juce::String nomeTom() const;

    // áreas dos displays (coordenadas base) para o mouse
    juce::Rectangle<float> areaResult, areaOriginal, areaDaw, areaCelulas;
    juce::Rectangle<float> areaPads[retalho::numPads];
    float celX0 = 0, celW = 1, celY0 = 0, celH = 1;
    int nCelVis = 16;

    RetalhoProcessor& proc;
    RetalhoProcessor::Vista vista;   // cópia do quadro
    int versaoVista = -1;
    std::vector<Knob> knobs;
    std::vector<Alvo> alvos;
    int arrasto = arNada, arrastoIdx = -1, arrastoLinha = 0, knobCampo = -1, padSegurado = -1;
    float valorIni = 0.0f, yIni = 0.0f, xIni = 0.0f;
    bool moveu = false;
    int quadro = 0;
    std::unique_ptr<juce::FileChooser> seletor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetalhoEditor)
};
