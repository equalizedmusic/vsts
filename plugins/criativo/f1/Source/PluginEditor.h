#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- RESPOSTA: curva do filtro agora, onde o knob deixou e até onde o envelope leva ----------------
class ResponseStrip : public juce::Component
{
public:
    explicit ResponseStrip (F1Processor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    F1Processor& proc;
};

// ---------------- ESCADA: os 4 degraus do ladder, o laço da ressonância, o envelope e a fumaça ----------------
class LadderView : public juce::Component
{
public:
    explicit LadderView (F1Processor& p) : proc (p) {}
    void tick (float level);
    void paint (juce::Graphics& g) override;

private:
    F1Processor& proc;
    float t = 0.0f, level = 0.0f;
};

class F1Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit F1Editor (F1Processor&);
    ~F1Editor() override;

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

    F1Processor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1048×620 (pedal: 1000×580 de chapa + a moldura), e é ampliado junto
    ResponseStrip strip;
    LadderView ladder;
    Knob cutMacro, resonance, poles, drive, range, envMacro, smooth, lfoRate, lfoAmt, sens, output;
    Fader mix;
    KeyButton openKey, smokeKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> openAtt, smokeAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = 0.0f;
    bool  lastSync = false, lastWaiting = false;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (F1Editor)
};
