#pragma once

#include "PluginProcessor.h"

// Tela do CHUTE (base 1000 × 752, escala junto com a janela), no molde do esboço v3 aprovado. Tudo desenhado no
// paint, a 30 Hz. Em cima: a batida do preset atual (ataque laranja, corpo verde, curva de pitch tracejada), o
// PITCH ENV, o AMP ENV e a saída. Embaixo: CLICK, BODY (KEY·MIDI | FIXED | PRESET), DRIVE e MASTER. No topo: a nota
// do corpo em cifra, o menu de presets (bancos com a nota de cada um), ◀ ▶ e SAVE AS.
class ChuteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChuteEditor (ChuteProcessor&);
    ~ChuteEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseW = 1000, baseH = 752;

private:
    struct Knob { juce::String id; juce::Point<float> c; float r; };
    struct Botao { juce::Rectangle<float> r; juce::String id; float valor; };

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }
    void atualizaPrevia();

    void topo (juce::Graphics&);
    void painelKick (juce::Graphics&, juce::Rectangle<float> r);
    void painelPitch (juce::Graphics&, juce::Rectangle<float> r);
    void painelAmp (juce::Graphics&, juce::Rectangle<float> r);
    void painelSaida (juce::Graphics&, juce::Rectangle<float> r);
    void painelClick (juce::Graphics&, juce::Rectangle<float> r);
    void painelBody (juce::Graphics&, juce::Rectangle<float> r);
    void painelDrive (juce::Graphics&, juce::Rectangle<float> r);
    void painelMaster (juce::Graphics&, juce::Rectangle<float> r);
    void rodape (juce::Graphics&);
    void tituloLiga (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor, const juce::String& idLiga);
    void seletor (juce::Graphics&, juce::Rectangle<float> r, const juce::String& id, const juce::StringArray& nomes, juce::Colour cor);
    void knob (juce::Graphics&, const juce::String& id, const juce::String& nome, juce::Point<float> c, float raio, juce::Colour cor,
               bool bipolar = false);

    juce::String texto (const juce::String& id) const;
    juce::RangedAudioParameter* param (const juce::String& id) const;
    float real (const juce::String& id) const;
    int escolha (const juce::String& id) const { return juce::roundToInt (real (id)); }
    int notaTocada() const;   // nota MIDI que vale para a tela (a última tocada; nenhuma = −1)

    void menuPresets();
    void menu();
    void salvarComo();

    ChuteProcessor& proc;

    // prévia da batida com os ajustes atuais (recalculada quando algo muda)
    static constexpr double taxaPrevia = 24000.0;
    static constexpr int nPrevia = 24000;   // 1 s
    std::vector<float> pClick = std::vector<float> (nPrevia), pCorpo = std::vector<float> (nPrevia);
    std::vector<float> curvaF = std::vector<float> (chute::nPts), curvaA = std::vector<float> (chute::nPts);
    int fimPrevia = 0, lenCurva = 0;
    float picoPrevia = 1.0f;
    juce::String assinatura;
    float picoTela = 0.0f;
    uint32_t batidasVistas = 0;
    float flash = 0.0f;

    juce::Rectangle<float> rPreset, rAnt, rProx, rSalvar, rMenu;
    std::vector<Knob> knobs;
    std::vector<Botao> botoes;
    juce::String arrastando;
    float valorIni = 0.0f, yIni = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChuteEditor)
};
