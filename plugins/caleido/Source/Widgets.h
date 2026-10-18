#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

using APVTS = juce::AudioProcessorValueTreeState;

// ---------------- knob (grande: macro; pequeno: com nome e valor embaixo) ----------------
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& labelText, bool big, juce::Colour onColour)
        : param (state.getParameter (paramId)), label (labelText), isBig (big)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (200);
        slider.getProperties().set ("big", big);
        slider.setColour (juce::Slider::rotarySliderFillColourId, onColour);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    void resized() override
    {
        if (isBig) slider.setBounds (getLocalBounds());
        else       slider.setBounds (getLocalBounds().removeFromTop (64).withSizeKeepingCentre (64, 64));
    }

    void paint (juce::Graphics& g) override
    {
        if (isBig) return;
        g.setColour (theme::txt);
        g.setFont (theme::sans (10.0f, true, 0.15f));
        g.drawText (label, 0, 62, getWidth(), 13, juce::Justification::centred);
        g.setColour (theme::dim);
        g.setFont (theme::mono (11.0f));
        g.drawText (param->getCurrentValueAsText(), 0, 76, getWidth(), 14, juce::Justification::centred);
    }

private:
    juce::RangedAudioParameter* param;
    juce::String label;
    bool isBig;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- fader do MIX ----------------
class Fader : public juce::Component
{
public:
    Fader (APVTS& state, const juce::String& paramId) : param (state.getParameter (paramId))
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (32)); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::txt);
        g.setFont (theme::sans (10.0f, true, 0.15f));
        g.drawText ("MIX", 0, getHeight() - 30, getWidth(), 13, juce::Justification::centred);
        g.setColour (theme::dim);
        g.setFont (theme::mono (11.0f));
        g.drawText (param->getCurrentValueAsText(), 0, getHeight() - 16, getWidth(), 14, juce::Justification::centred);
    }

private:
    juce::RangedAudioParameter* param;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- tecla liga/desliga (REVERSO, CONGELAR) ----------------
class KeyButton : public juce::Button
{
public:
    KeyButton (const juce::String& text, juce::Colour led) : juce::Button (text), ledColour (led)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::panel2);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (over ? theme::dim : theme::line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        const bool on = getToggleState();
        auto led = juce::Rectangle<float> (18.0f, 3.0f).withCentre ({ r.getCentreX(), r.getY() + 13.0f });
        if (on)
        {
            g.setColour (ledColour.withAlpha (0.3f));
            g.fillRoundedRectangle (led.expanded (3.0f, 2.0f), 3.0f);
        }
        g.setColour (on ? ledColour : theme::off);
        g.fillRoundedRectangle (led, 1.5f);

        g.setColour (on ? theme::cream : theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.15f));
        g.drawText (getButtonText(), r.withTrimmedTop (18.0f), juce::Justification::centred);
    }

private:
    juce::Colour ledColour;
};

// ---------------- botão redondo de bypass (aceso = efeito ligado) ----------------
class BypassButton : public juce::Button
{
public:
    BypassButton() : juce::Button ("bypass") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::panel2);
        g.fillEllipse (r);
        g.setColour (over ? theme::dim : theme::line);
        g.drawEllipse (r, 1.0f);

        const bool active = ! getToggleState();
        auto dot = juce::Rectangle<float> (10.0f, 10.0f).withCentre (r.getCentre());
        if (active)
        {
            g.setColour (theme::amber.withAlpha (0.25f));
            g.fillEllipse (dot.expanded (4.0f));
        }
        g.setColour (active ? theme::amber : theme::off);
        g.fillEllipse (dot);
    }
};

// ---------------- botões da barra de presets ----------------
class IconButton : public juce::Button
{
public:
    enum class Icon { prev, next, save };
    explicit IconButton (Icon i) : juce::Button ("icon"), icon (i) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel2);
        g.fillRect (r);
        g.setColour (theme::line);
        g.drawRect (r, 1.0f);

        g.setColour (down || over ? theme::amber : theme::dim);
        const auto c = r.getCentre();
        juce::Path p;
        if (icon == Icon::prev)      p.addTriangle (c.x - 4.0f, c.y, c.x + 3.0f, c.y - 4.5f, c.x + 3.0f, c.y + 4.5f);
        else if (icon == Icon::next) p.addTriangle (c.x + 4.0f, c.y, c.x - 3.0f, c.y - 4.5f, c.x - 3.0f, c.y + 4.5f);
        else
        {
            p.addRectangle (c.x - 5.0f, c.y - 1.0f, 10.0f, 2.0f);
            p.addRectangle (c.x - 1.0f, c.y - 5.0f, 2.0f, 10.0f);
        }
        g.fillPath (p);
    }

private:
    Icon icon;
};

// ---------------- nome do preset (clique abre a lista) ----------------
class PresetName : public juce::Component
{
public:
    std::function<void()> onClick;

    void set (const juce::String& n, int index, int total)
    {
        if (n == name && index == idx && total == count) return;
        name = n; idx = index; count = total;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::dark);
        g.fillRect (r);
        g.setColour (theme::line);
        g.drawLine (r.getX(), r.getY() + 0.5f, r.getRight(), r.getY() + 0.5f);
        g.drawLine (r.getX(), r.getBottom() - 0.5f, r.getRight(), r.getBottom() - 0.5f);

        auto inner = getLocalBounds().reduced (12, 0);
        g.setColour (theme::dim);
        g.setFont (theme::mono (10.5f));
        auto num = juce::String (idx + 1).paddedLeft ('0', 2) + "/" + juce::String (count).paddedLeft ('0', 2);
        g.drawText (num, inner.removeFromRight (44), juce::Justification::centredRight);
        g.setColour (theme::cream);
        g.setFont (theme::mono (12.5f));
        g.drawText (name, inner, juce::Justification::centredLeft, true);
    }

    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String name;
    int idx = 0, count = 0;
};

// ---------------- medidor de LEDs ----------------
class LedMeter : public juce::Component
{
public:
    explicit LedMeter (juce::String t) : text (std::move (t)) {}

    void setLevel (float gain)
    {
        const float db = juce::Decibels::gainToDecibels (gain, -60.0f);
        const float v  = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -48.0f, 0.0f, 0.0f, 1.0f));
        if (std::abs (v - level) > 0.004f) { level = v; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const int leds = 16;
        const float w = 12.0f, h = 4.0f, gap = 2.0f;
        const float x = ((float) getWidth() - w) * 0.5f;
        float y = (float) getHeight() - 14.0f - h;
        for (int i = 0; i < leds; ++i, y -= h + gap)
        {
            const float f = (float) i / (float) (leds - 1);
            const bool lit = level > 0.0f && f <= level;
            g.setColour (! lit ? theme::ledOff : f > 0.85f ? theme::coral : f > 0.65f ? theme::amber : theme::cream);
            g.fillRoundedRectangle (x, y, w, h, 1.0f);
        }
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, false, 0.1f));
        g.drawText (text, 0, getHeight() - 12, getWidth(), 12, juce::Justification::centred);
    }

private:
    juce::String text;
    float level = 0.0f;
};
