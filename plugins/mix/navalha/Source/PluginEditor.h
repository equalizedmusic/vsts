#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- FORMA DE ONDA: o que passa (creme) e o que a navalha corta (coral), em tempo real ----------------
class WaveStrip : public juce::Component
{
public:
    explicit WaveStrip (NavalhaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    NavalhaProcessor& proc;
    float range = 1.25f;   // escala vertical; acompanha os picos (devagar)
};

// ---------------- CURVA: entrada × saída, com o ponto do pico de agora ----------------
class CurveView : public juce::Component
{
public:
    explicit CurveView (NavalhaProcessor& p) : proc (p) {}
    void setLive (float peakIn, float cutDb) { live = peakIn; cut = cutDb; }
    void paint (juce::Graphics& g) override;

private:
    NavalhaProcessor& proc;
    float live = 0.0f, cut = 0.0f;
};

// ---------------- ANÁLISE: quanto está cortando, máximo, % de amostras e picos ----------------
class StatsView : public juce::Component
{
public:
    void update (float cutDb, float share, float peakIn, float peakOut, float dt);
    float cutShown() const { return cutNow; }
    float peakInShown() const { return pkIn; }
    void paint (juce::Graphics& g) override;

private:
    float cutNow = 0.0f, cutMax = 0.0f, holdLeft = 0.0f, shareSm = 0.0f, pkIn = 0.0f, pkOut = 0.0f;
};

class NavalhaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NavalhaEditor (NavalhaProcessor&);
    ~NavalhaEditor() override;

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

    NavalhaProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    WaveStrip strip;
    CurveView curve;
    StatsView stats;
    Knob driveMacro, ceiling, knee, asym, link, output;
    Fader mix;
    KeyButton deltaKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> deltaAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    double lastTick = 0.0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NavalhaEditor)
};
