#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- PADRÃO: o compasso processado, fatiado nos 16 passos ----------------
class PatternStrip : public juce::Component
{
public:
    explicit PatternStrip (CaleidoProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    CaleidoProcessor& proc;
};

// ---------------- MANDALA: sequenciador circular, um anel por efeito ----------------
class Mandala : public juce::Component
{
public:
    explicit Mandala (CaleidoProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void tick() { t += 1.0f; repaint(); }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    CaleidoProcessor& proc;
    float t = 0.0f;
};

// ---------------- botão pequeno (GERAR) ----------------
class ChipButton : public juce::Button
{
public:
    ChipButton() : juce::Button ("gerar") {}
    void paintButton (juce::Graphics& g, bool over, bool down) override;
};

class CaleidoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CaleidoEditor (CaleidoProcessor&);
    ~CaleidoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();

    CaleidoProcessor& proc;
    BrandLookAndFeel lnf;

    PatternStrip strip;
    Mandala mandala;
    ChipButton generateBtn;
    Knob chaosMacro, tripMacro, division, ratchet, shuffle, reverse, tape, filter, crush, spin, output;
    Fader mix;
    KeyButton mutateKey, holdKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> mutateAtt, holdAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = -1.0f;
    bool lastSync = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaleidoEditor)
};
