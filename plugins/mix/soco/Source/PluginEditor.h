#pragma once

#include <vector>
#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- FORMA DE ONDA: antes (fantasma) e depois, com o ganho de ataque e de sustain ----------------
class WaveStrip : public juce::Component
{
public:
    explicit WaveStrip (SocoProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    SocoProcessor& proc;
    float range = 1.0f;   // escala vertical; acompanha os picos (devagar)
};

// ---------------- GOLPE: um golpe de teste passando pelo detector com os ajustes de agora ----------------
class HitView : public juce::Component
{
public:
    explicit HitView (SocoProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    SocoProcessor& proc;
    std::vector<float> lvl, gAtk, gSus;   // golpe de teste e o ganho de cada parte (dB)
};

// ---------------- DETECTOR: o que o detector ouve (resposta do filtro) ----------------
class DetectorView : public juce::Component
{
public:
    explicit DetectorView (SocoProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    SocoProcessor& proc;
};

// ---------------- AGORA: ganho de ataque e de sustain aplicados neste momento ----------------
class GainNowView : public juce::Component
{
public:
    void update (float atkDb, float susDb, float dt);
    void paint (juce::Graphics& g) override;

private:
    float atk = 0.0f, sus = 0.0f;
};

// ---------------- PICOS: entrada e saída em dBFS ----------------
class PeakView : public juce::Component
{
public:
    void update (float peakIn, float peakOut, float dt);
    void paint (juce::Graphics& g) override;

private:
    float pkIn = 0.0f, pkOut = 0.0f, maxOut = 0.0f, holdLeft = 0.0f;
};

class SocoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SocoEditor (SocoProcessor&);
    ~SocoEditor() override;

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

    SocoProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    WaveStrip strip;
    HitView hit;
    DetectorView detView;
    GainNowView gainNow;
    PeakView peaks;
    Knob attackMacro, sustain, focus, detMode, detFreq, output;
    Fader mix;
    KeyButton deltaKey, clipKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> deltaAtt, clipAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    double lastTick = 0.0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SocoEditor)
};
