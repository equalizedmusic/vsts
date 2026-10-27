#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "IspTelaAtivacao.h"

// ---------------- FORMA DE ONDA: o que sai (creme), o que a barragem segura (coral) e a redução (azul) ----------------
class WaveStrip : public juce::Component
{
public:
    explicit WaveStrip (BarragemProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    BarragemProcessor& proc;
    float range = 1.25f;     // escala vertical; acompanha os picos (devagar)
    float grRange = 12.0f;   // escala da redução (dB); abre para 24 quando passa de 12
};

// ---------------- LOUDNESS: LUFS momentâneo, curto e integrado, barra com o ALVO e o seletor do alvo ----------------
class LoudnessView : public juce::Component
{
public:
    explicit LoudnessView (BarragemProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> selectorArea() const;
    void showTargetMenu();
    void setTarget (int index);

    BarragemProcessor& proc;
};

// ---------------- ANÁLISE: quanto está segurando, máximo e picos ----------------
class StatsView : public juce::Component
{
public:
    void update (float grDb, float peakIn, float peakOut, float dt);
    void paint (juce::Graphics& g) override;

private:
    float grNow = 0.0f, grMax = 0.0f, holdLeft = 0.0f, pkIn = 0.0f, pkOut = 0.0f;
};

// ---------------- PICOS: entrada e saída do plugin em dBFS, segurando 3 s ----------------
class PeakReadout : public juce::Component
{
public:
    void update (float in, float out, float dt);
    void paint (juce::Graphics& g) override;

private:
    float inMax = 0.0f, outMax = 0.0f, inHold = 0.0f, outHold = 0.0f;
};

class BarragemEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BarragemEditor (BarragemProcessor&);
    ~BarragemEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();
    void presetMenuChosen (int result);
    void askNewPreset();
    void drawCanvas (juce::Graphics&);
    void layoutCanvas();
    void setScale (float scale);

    static constexpr int openSettle = 15;   // quadros do timer (30 Hz) até a janela assentar ao abrir
    float openScale = 1.0f;                 // tamanho salvo no projeto, lido no construtor
    int   openTicks = 0;

    BarragemProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1064×580 (rack: 1000 de frente + as orelhas), e é ampliado junto
    WaveStrip strip;
    LoudnessView loudness;
    StatsView stats;
    PeakReadout readout;
    Knob fillMacro, ceiling, release, look, link;
    KeyButton autoKey, deltaKey, resetKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    VuMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> autoAtt, deltaAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    double lastTick = 0.0;
    int resetFlash = 0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir
    isp::TelaAtivacao ativacao;   // por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarragemEditor)
};
