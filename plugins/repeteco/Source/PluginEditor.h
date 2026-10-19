#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- REPETIÇÕES: cada eco, do mais novo ao mais velho, ficando mais fraco e mais escuro ----------------
class RepeatsStrip : public juce::Component
{
public:
    explicit RepeatsStrip (RepetecoProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    RepetecoProcessor& proc;
};

// ---------------- BALDES: a carga passando de balde em balde até voltar pela realimentação ----------------
class BucketView : public juce::Component
{
public:
    explicit BucketView (RepetecoProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    RepetecoProcessor& proc;
};

class RepetecoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RepetecoEditor (RepetecoProcessor&);
    ~RepetecoEditor() override;

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

    RepetecoProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    RepeatsStrip strip;
    BucketView buckets;
    Knob timeMacro, range, sync, drive, pingPong, fbMacro, lfoRate, lfoAmt, lfoShape, dark, output;
    Fader mix;
    KeyButton infiniteKey, killKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> infiniteAtt, killAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = 0.0f;
    bool  lastSync = false, lastWaiting = false;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RepetecoEditor)
};
