#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
inline const juce::Colour bg     { 0xff14120e };
inline const juce::Colour panel  { 0xff1d1a15 };
inline const juce::Colour panel2 { 0xff27231c };
inline const juce::Colour line   { 0xff353027 };
inline const juce::Colour txt    { 0xffece3d2 };
inline const juce::Colour dim    { 0xff8b8273 };
inline const juce::Colour amber  { 0xffffb347 };
inline const juce::Colour coral  { 0xffff6b4a };
inline const juce::Colour cream  { 0xfff4e9d4 };
inline const juce::Colour ice    { 0xff6ec3ff };
inline const juce::Colour violet { 0xffc49bff };
inline const juce::Colour dark   { 0xff100e0b };
inline const juce::Colour segOff { 0xff3a342b };
inline const juce::Colour body   { 0xff2a261f };
inline const juce::Colour ledOff { 0xff2c2821 };
inline const juce::Colour off    { 0xff4a443a };

inline juce::Font sans (float h, bool bold = false, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}

inline juce::Font mono (float h)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
}
} // namespace theme

class BrandLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BrandLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, theme::panel2);
        setColour (juce::PopupMenu::textColourId, theme::txt);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::amber.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, theme::cream);
        setColour (juce::PopupMenu::headerTextColourId, theme::dim);
    }

    // knob com anel de segmentos
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const bool  big = (bool) slider.getProperties()["big"];
        const auto  on  = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const float k   = (float) juce::jmin (width, height) / (big ? 92.0f : 64.0f);
        const float cx  = (float) x + (float) width * 0.5f, cy = (float) y + (float) height * 0.5f;
        const int   n   = big ? 31 : 21;
        const float r1  = (big ? 34.0f : 23.0f) * k, r2 = (big ? 42.0f : 29.0f) * k, rb = (big ? 26.0f : 17.0f) * k;

        auto pt = [cx, cy] (float r, float a) { return juce::Point<float> (cx + r * std::sin (a), cy - r * std::cos (a)); };

        const juce::PathStrokeType stroke ((big ? 2.6f : 2.2f) * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        for (int i = 0; i < n; ++i)
        {
            const float f = (float) i / (float) (n - 1);
            const float a = startAngle + (endAngle - startAngle) * f;
            juce::Path seg;
            seg.startNewSubPath (pt (r1, a));
            seg.lineTo (pt (r2, a));
            g.setColour (f <= pos + 1.0e-4f ? on : theme::segOff);
            g.strokePath (seg, stroke);
        }

        g.setColour (theme::body);
        g.fillEllipse (cx - rb, cy - rb, rb * 2.0f, rb * 2.0f);
        g.setColour (theme::dark);
        g.drawEllipse (cx - rb, cy - rb, rb * 2.0f, rb * 2.0f, 2.0f);

        const auto  d  = pt (rb - 5.0f * k, startAngle + (endAngle - startAngle) * pos);
        const float dr = (big ? 3.0f : 2.2f) * k;
        g.setColour (on);
        g.fillEllipse (d.x - dr, d.y - dr, dr * 2.0f, dr * 2.0f);

        if (big)
        {
            g.setColour (theme::txt);
            g.setFont (theme::mono (14.0f * k));
            g.drawText (juce::String (juce::roundToInt (slider.getValue())),
                        juce::Rectangle<float> (cx - rb, cy - 9.0f * k, rb * 2.0f, 18.0f * k), juce::Justification::centred);
        }
    }

    // fader vertical do MIX
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (! slider.isVertical())
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
            return;
        }

        juce::Rectangle<float> rail ((float) x + ((float) width - 30.0f) * 0.5f, (float) y - 3.0f, 30.0f, (float) height + 6.0f);
        g.setColour (theme::dark);
        g.fillRoundedRectangle (rail, 3.0f);

        auto fill = rail.reduced (1.0f).withTop (sliderPos);
        g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (0.4f), 0.0f, fill.getY(),
                                                 theme::coral.withAlpha (0.2f), 0.0f, fill.getBottom(), false));
        g.fillRect (fill);

        g.setColour (theme::line);
        g.drawRoundedRectangle (rail, 3.0f, 1.0f);

        g.setColour (theme::cream);
        g.fillRoundedRectangle (juce::Rectangle<float> (rail.getX() + 2.0f, sliderPos - 3.0f, rail.getWidth() - 4.0f, 6.0f), 2.0f);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 3; }
};
