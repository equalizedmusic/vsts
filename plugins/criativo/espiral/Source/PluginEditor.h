#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- RESPOSTA: curva de frequência do que sai agora, com as vozes deslizando ----------------
class ResponseStrip : public juce::Component
{
public:
    explicit ResponseStrip (EspiralProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    EspiralProcessor& proc;
};

// ---------------- ESPIRAL: cada voz é um cometa subindo do grave (centro) ao agudo (borda) ----------------
class SpiralView : public juce::Component
{
public:
    explicit SpiralView (EspiralProcessor& p) : proc (p) {}
    void tick();
    void paint (juce::Graphics& g) override;

private:
    EspiralProcessor& proc;
    float t = 0.0f, lastPhase = 0.0f, motion = 1.0f;
};

class EspiralEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EspiralEditor (EspiralProcessor&);
    ~EspiralEditor() override;

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

    EspiralProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    ResponseStrip strip;
    SpiralView spiral;
    Knob rangeMacro, resMacro, cycle, direction, centre, stereo, mode, stages, drive, tone, output;
    Fader mix;
    KeyButton freezeKey, twiceKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> freezeAtt, twiceAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = -1.0f;
    bool lastSync = false, lastWaiting = false;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EspiralEditor)
};
