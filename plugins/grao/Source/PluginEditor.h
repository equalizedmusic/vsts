#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- FITA: forma de onda gravada + grãos ----------------
class TapeView : public juce::Component
{
public:
    explicit TapeView (GraoProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    GraoProcessor& proc;
};

// ---------------- CAMPO XY: X = ESPAÇO, Y = GRÃO ----------------
class XYPad : public juce::Component
{
public:
    explicit XYPad (APVTS& state);
    void tick();
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    void setFrom (const juce::MouseEvent& e);
    juce::Rectangle<float> field() const { return getLocalBounds().removeFromTop (getWidth()).toFloat(); }

    juce::RangedAudioParameter* space;
    juce::RangedAudioParameter* grain;
    std::vector<juce::Point<float>> trail;
    float t = 0.0f;
};

class GraoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GraoEditor (GraoProcessor&);
    ~GraoEditor() override;

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

    GraoProcessor& proc;
    GraoLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    TapeView tape;
    XYPad pad;
    Knob grainMacro, spaceMacro, size, density, spray, pitch, spread, feedback, decay, tone, output;
    Fader mix;
    KeyButton reverseKey, freezeKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> reverseAtt, freezeAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraoEditor)
};
