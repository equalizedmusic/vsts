#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// estado da animação, comum à faixa de cima e ao palco do knob
struct FlameState
{
    float heat = 0.0f, level = 0.0f, t = 0.0f, noise = 0.5f, space = 0.5f;
    bool  drop = false;
};

// ---------------- TENSÃO: muro de fogo com os valores que a CHAMA está aplicando ----------------
class HeatStrip : public juce::Component
{
public:
    explicit HeatStrip (ChamaProcessor& p) : proc (p) {}
    void set (const FlameState& s) { st = s; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    ChamaProcessor& proc;
    FlameState st;
};

// ---------------- palco: as chamas crescem atrás do knob ----------------
class FlameStage : public juce::Component
{
public:
    FlameStage() { setInterceptsMouseClicks (false, false); }
    void set (const FlameState& s) { st = s; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    FlameState st;
};

class ChamaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChamaEditor (ChamaProcessor&);
    ~ChamaEditor() override;

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

    ChamaProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    HeatStrip strip;
    FlameStage stage;
    Knob chamaKnob, noise, space, output;
    KeyButton dropKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> dropAtt, bypassAtt;
    FlameState anim;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = -1.0f;
    bool lastSync = false, lastWaiting = false;
    double lastTick = 0.0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChamaEditor)
};
