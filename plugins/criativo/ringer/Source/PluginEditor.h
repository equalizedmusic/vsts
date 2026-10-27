#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "IspTelaAtivacao.h"

// ---------------- OSCILOSCÓPIO: entrada, portadora e o que sai multiplicado ----------------
class ScopeStrip : public juce::Component
{
public:
    explicit ScopeStrip (RingerProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    RingerProcessor& proc;
    float scale = 1.0f;   // acompanha o volume devagar
};

// ---------------- ANEL: a portadora girando em volta, o LFO no arco e o Lissajous entrada × saída ----------------
class RingView : public juce::Component
{
public:
    explicit RingView (RingerProcessor& p) : proc (p) {}
    void tick();
    void paint (juce::Graphics& g) override;

private:
    RingerProcessor& proc;
    float spin = 0.0f, scale = 1.0f;
};

class RingerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RingerEditor (RingerProcessor&);
    ~RingerEditor() override;

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

    RingerProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1048×620 (pedal: 1000×580 de chapa + a moldura), e é ampliado junto
    ScopeStrip strip;
    RingView ring;
    Knob freqMacro, range, snap, drive, stereo, lfoMacro, lfoRate, lfoShape, lfoSync, lfoPhase, output;
    Fader mix;
    KeyButton twiceKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> twiceAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = 0.0f;
    bool  lastSync = false, lastWaiting = false;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir
    isp::TelaAtivacao ativacao;   // por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingerEditor)
};
