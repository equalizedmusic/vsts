#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- MIRAGEM: a cauda como serra no horizonte e o reflexo tremendo de calor ----------------
class MirageView : public juce::Component
{
public:
    explicit MirageView (MiragemProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    MiragemProcessor& proc;
};

// ---------------- CAUDA: queda em dB × tempo, pré-delay, shimmer e faixa de tom ----------------
class DecayView : public juce::Component
{
public:
    explicit DecayView (MiragemProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

    float levelShown = 0.0f, duckShown = 0.0f;   // atualizados pelo editor

private:
    MiragemProcessor& proc;
};

class MiragemEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MiragemEditor (MiragemProcessor&);
    ~MiragemEditor() override;

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

    MiragemProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1048×620 (pedal: 1000×580 de chapa + a moldura), e é ampliado junto
    MirageView mirage;
    DecayView decayView;
    Knob decayMacro, size, predelay, dark, lowCut, shimMacro, interval, motion, duck, revLen, output;
    Fader mix;
    KeyButton freezeKey, reverseKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> freezeAtt, reverseAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = 0.0f;
    bool  lastSync = false, lastWaiting = false;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiragemEditor)
};
