#pragma once

// GROOVADOR — peças da tela com cara de bateria eletrônica: chassi preto fosco, faixa de alumínio, listra de 4 cores
// (vermelho, laranja, amarelo, creme), LEDs vermelhos, display de 7 segmentos, LCD verde de sampler, knobs pretos
// com tampa colorida e botões de borracha. Tudo vetorial (sem imagens).

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>
#include "Params.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace gv
{
inline constexpr int   baseW = 1200, baseH = 740;
inline constexpr float minScale = 0.75f, maxScale = 2.0f;

using RF = juce::Rectangle<float>;

inline const juce::Colour red    { 0xffe8392b };
inline const juce::Colour orange { 0xfff28a1e };
inline const juce::Colour yellow { 0xfff5c518 };
inline const juce::Colour cream  { 0xffefe6cf };
inline const juce::Colour led    { 0xffff2a1a };
inline const juce::Colour ledOff { 0xff3a1511 };
inline const juce::Colour lcd0   { 0xffbfd877 };
inline const juce::Colour lcd1   { 0xffa6c25a };
inline const juce::Colour lcdInk { 0xff1f2a10 };
inline const juce::Colour silk   { 0xffe9e6dc };
inline const juce::Colour dim    { 0xff8a877e };
inline const juce::Colour grey   { 0xff9a978e };
inline const juce::Colour body0  { 0xff1d1d20 };
inline const juce::Colour body1  { 0xff121214 };
inline const juce::Colour line   { 0xff4a4944 };

// cor da tecla de cada grupo de 4 (e de cada instrumento: 2 por cor)
inline juce::Colour keyColour (int group)
{
    switch (((group % 4) + 4) % 4) { case 0: return red; case 1: return orange; case 2: return yellow; default: return cream; }
}
inline juce::Colour instColour (int inst) { return keyColour (inst / 2); }

inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline juce::Font sans (float h, bool bold = true, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}
inline juce::Font mono (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
}

// ---------------- desenhos ----------------
inline void screw (juce::Graphics& g, float cx, float cy)
{
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillEllipse (cx - 5.0f, cy - 4.0f, 10.0f, 10.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8d8), cx - 2.0f, cy - 2.0f, juce::Colour (0xff555555), cx + 4.0f, cy + 4.0f, true));
    g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colour (0xff333333));
    juce::Path p;
    p.addRectangle (-3.0f, -0.5f, 6.0f, 1.0f);
    g.fillPath (p, juce::AffineTransform::rotation (0.6f).translated (cx, cy));
}

// rótulo serigrafado de seção: caixa branca com traço até o fim da seção
inline void section (juce::Graphics& g, float x, float y, float w, const juce::String& text)
{
    g.setFont (sans (11.0f, true, 0.22f));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 16.0f;
    g.setColour (silk);
    g.fillRoundedRectangle (x, y, tw, 17.0f, 2.0f);
    g.setColour (juce::Colour (0xff111111));
    g.drawText (text, RF (x, y + 1.0f, tw, 16.0f), juce::Justification::centred, false);
    if (w > tw + 4.0f)
    {
        g.setColour (line);
        g.fillRect (x + tw, y + 8.0f, w - tw, 1.0f);
    }
}

inline void caption (juce::Graphics& g, const juce::String& text, RF area, juce::Justification j = juce::Justification::centred,
                     juce::Colour c = silk.withAlpha (0.85f), float size = 8.5f)
{
    g.setFont (sans (size, true, 0.14f));
    g.setColour (c);
    g.drawText (text, area, j, false);
}

// visor rebaixado (fundo vermelho-escuro dos displays)
inline void displayWell (juce::Graphics& g, RF r, juce::Colour bg = juce::Colour (0xff140303))
{
    g.setColour (juce::Colour (0xff3a3936));
    g.fillRoundedRectangle (r.expanded (2.0f), 4.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (r, 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getX(), r.getY() + 6.0f, false));
    g.fillRoundedRectangle (r, 3.0f);
}

inline void ledDot (juce::Graphics& g, float cx, float cy, float rad, bool on, float amount = 1.0f)
{
    if (on && amount > 0.02f)
    {
        g.setGradientFill (juce::ColourGradient (led.withAlpha (0.55f * amount), cx, cy, led.withAlpha (0.0f), cx + rad * 3.2f, cy, true));
        g.fillEllipse (cx - rad * 3.2f, cy - rad * 3.2f, rad * 6.4f, rad * 6.4f);
        g.setColour (ledOff.interpolatedWith (led, amount));
    }
    else
        g.setColour (ledOff);
    g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
    if (on && amount > 0.5f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f * amount));
        g.fillEllipse (cx - rad * 0.4f, cy - rad * 0.5f, rad * 0.7f, rad * 0.6f);
    }
}

// display de 7 segmentos vermelho; '.' vira ponto no dígito anterior
inline void sevenSeg (juce::Graphics& g, RF r, const juce::String& text, float digitW)
{
    static const char* const segs[10] = { "abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg", "abfgcd" };
    const float h = r.getHeight() - 10.0f, t = 3.0f, y0 = r.getY() + 5.0f, m = y0 + h * 0.5f;
    float x = r.getX() + 6.0f;
    for (int i = 0; i < text.length(); ++i)
    {
        const auto ch = text[i];
        if (ch == '.')
        {
            g.setColour (led);
            g.fillEllipse (x - 4.5f, y0 + h - 2.0f, 3.4f, 3.4f);
            continue;
        }
        const juce::String on = (ch >= '0' && ch <= '9') ? juce::String (segs[ch - '0']) : (ch == '-' ? juce::String ("g") : juce::String());
        const float w = digitW;
        struct S { char k; float a, b, c, d; };
        const S all[7] = { { 'a', x + 2, y0, w - 4, t }, { 'g', x + 2, m - t * 0.5f, w - 4, t }, { 'd', x + 2, y0 + h - t, w - 4, t },
                           { 'f', x, y0 + 1, t, h * 0.5f - 2 }, { 'b', x + w - t, y0 + 1, t, h * 0.5f - 2 },
                           { 'e', x, m + 1, t, h * 0.5f - 2 }, { 'c', x + w - t, m + 1, t, h * 0.5f - 2 } };
        for (auto& s : all)
        {
            const bool lit = on.containsChar (s.k);
            if (lit)
            {
                g.setColour (led.withAlpha (0.25f));
                g.fillRoundedRectangle (s.a - 1.0f, s.b - 1.0f, s.c + 2.0f, s.d + 2.0f, 1.5f);
            }
            g.setColour (lit ? led : juce::Colour (0xff3a0b07));
            g.fillRoundedRectangle (s.a, s.b, s.c, s.d, 1.0f);
        }
        x += w + 5.0f;
    }
}

// texto em LED vermelho (displays de matriz)
inline void ledText (juce::Graphics& g, const juce::String& text, RF r, float size, juce::Justification j = juce::Justification::centred)
{
    g.setFont (mono (size));
    g.setColour (led.withAlpha (0.3f));
    g.drawFittedText (text, r.translated (0.0f, 0.5f).toNearestInt(), j, 1, 0.7f);
    g.setColour (led);
    g.drawFittedText (text, r.toNearestInt(), j, 1, 0.7f);
}

// tampa de borracha (botões)
inline void rubber (juce::Graphics& g, RF r, bool down, bool over, juce::Colour top = juce::Colour (0xff4a4a4e), juce::Colour bottom = juce::Colour (0xff2a2a2d))
{
    if (! down)
    {
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r.translated (0.0f, 2.5f), 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0.0f, 4.0f).expanded (1.0f), 5.0f);
    }
    const auto rr = down ? r.translated (0.0f, 2.0f) : r;
    g.setGradientFill (juce::ColourGradient (over ? top.brighter (0.12f) : top, rr.getX(), rr.getY(), bottom, rr.getX(), rr.getBottom(), false));
    g.fillRoundedRectangle (rr, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.fillRect (rr.getX() + 3.0f, rr.getY() + 1.0f, rr.getWidth() - 6.0f, 1.0f);
}

inline void diceIcon (juce::Graphics& g, RF r, juce::Colour c)
{
    const auto s = juce::jmin (r.getWidth(), r.getHeight());
    const RF d = r.withSizeKeepingCentre (s, s);
    g.setColour (c);
    g.fillRoundedRectangle (d.reduced (s * 0.08f), s * 0.2f);
    g.setColour (juce::Colour (0xff2a2a2d));
    const float dot = s * 0.15f;
    for (auto p : { juce::Point<float> (0.3f, 0.3f), { 0.7f, 0.3f }, { 0.5f, 0.5f }, { 0.3f, 0.7f }, { 0.7f, 0.7f } })
        g.fillEllipse (d.getX() + d.getWidth() * p.x - dot * 0.5f, d.getY() + d.getHeight() * p.y - dot * 0.5f, dot, dot);
}

inline void lockIcon (juce::Graphics& g, RF r, juce::Colour c, bool closed)
{
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const RF d = r.withSizeKeepingCentre (s * 0.8f, s);
    const RF bodyR (d.getX(), d.getY() + s * 0.45f, d.getWidth(), s * 0.5f);
    juce::Path shackle;
    const float sx = d.getX() + d.getWidth() * 0.2f, sw = d.getWidth() * 0.6f, top = d.getY() + s * 0.06f;
    shackle.startNewSubPath (sx, bodyR.getY());
    shackle.lineTo (sx, top + sw * 0.5f);
    shackle.addCentredArc (sx + sw * 0.5f, top + sw * 0.5f, sw * 0.5f, sw * 0.5f, 0.0f, -juce::MathConstants<float>::halfPi,
                           juce::MathConstants<float>::halfPi);
    shackle.lineTo (sx + sw, closed ? bodyR.getY() : top + sw * 0.75f);
    g.setColour (c);
    g.strokePath (shackle, juce::PathStrokeType (juce::jmax (1.2f, s * 0.11f)));
    g.fillRoundedRectangle (bodyR, s * 0.08f);
}

// ---------------- look: knob preto com tampa colorida e escala serigrafada; menus escuros ----------------
class DrumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DrumLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1c1f));
        setColour (juce::PopupMenu::textColourId, silk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, red.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, dim);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1c1c1f));
        setColour (juce::AlertWindow::textColourId, silk);
        setColour (juce::AlertWindow::outlineColourId, line);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float a0, float a1, juce::Slider& slider) override
    {
        const auto cap = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 6.5f;
        const auto c = area.getCentre();
        const float an = a0 + pos * (a1 - a0);

        // escala: 11 traços, maiores no começo, meio e fim
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / 10.0f;
            const float l0 = r + 2.5f, l1 = r + (i % 5 == 0 ? 6.0f : 4.5f);
            juce::Path t;
            t.startNewSubPath (c.x + l0 * std::sin (a), c.y - l0 * std::cos (a));
            t.lineTo (c.x + l1 * std::sin (a), c.y - l1 * std::cos (a));
            g.setColour (juce::Colour (0xffd8d4c8));
            g.strokePath (t, juce::PathStrokeType (i % 5 == 0 ? 1.2f : 0.8f));
        }
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (c.x - r, c.y - r + 2.0f, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff0e0e0f));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff3a3a3d));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);

        const float cr = r * 0.62f;
        juce::ColourGradient grad (juce::Colours::white.withAlpha (0.55f).overlaidWith (cap.withAlpha (0.45f)), c.x - cr * 0.25f, c.y - cr * 0.5f,
                                   cap, c.x + cr * 0.4f, c.y + cr * 0.7f, true);
        grad.addColour (0.5, cap);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 0.8f);

        juce::Path ptr;
        ptr.startNewSubPath (c.x + r * 0.15f * std::sin (an), c.y - r * 0.15f * std::cos (an));
        ptr.lineTo (c.x + (r - 1.5f) * std::sin (an), c.y - (r - 1.5f) * std::cos (an));
        g.setColour (juce::Colours::white);
        g.strokePath (ptr, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

// ---------------- knob com nome e valor embaixo ----------------
class GKnob : public juce::Component
{
public:
    GKnob (APVTS& state, const juce::String& paramId, const juce::String& labelText, juce::Colour capColour, float diameter = 28.0f)
        : param (state.getParameter (paramId)), label (labelText), dia (diameter)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (200);
        slider.setColour (juce::Slider::rotarySliderFillColourId, capColour);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    void resized() override
    {
        const int s = juce::roundToInt (dia + 14.0f);
        slider.setBounds ((getWidth() - s) / 2, 0, s, s);
    }

    void paint (juce::Graphics& g) override
    {
        const float top = dia + 13.0f, w = (float) getWidth();
        g.setFont (sans (dia > 30.0f ? 9.0f : 8.5f, true, 0.08f));
        g.setColour (silk);
        g.drawFittedText (label, juce::Rectangle<int> (0, (int) top, (int) w, 11), juce::Justification::centred, 1, 0.7f);
        g.setFont (mono (8.5f, false));
        g.setColour (juce::Colour (0xff8f8b80));
        g.drawText (param->getCurrentValueAsText(), RF (0.0f, top + 11.0f, w, 11.0f), juce::Justification::centred, false);
    }

private:
    juce::RangedAudioParameter* param;
    juce::String label;
    float dia;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- display vermelho de escolha: clique abre a lista, arraste/roda troca ----------------
class RedChoice : public juce::Component
{
public:
    RedChoice (APVTS& state, const juce::String& paramId)
        : param (state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
        choice = dynamic_cast<juce::AudioParameterChoice*> (param);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        displayWell (g, r);
        ledText (g, param->getCurrentValueAsText(), r.reduced (6.0f, 0.0f), juce::jmin (12.5f, r.getHeight() * 0.55f));
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragged = false;
        startNorm = param->getValue();
        attachment->beginGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (std::abs (e.getDistanceFromDragStartY()) < 4 && ! dragged) return;
        dragged = true;
        const float norm = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / 160.0f);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (norm));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        attachment->endGesture();
        if (! dragged) showList();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const float dir = w.deltaY > 0.0f ? 1.0f : w.deltaY < 0.0f ? -1.0f : 0.0f;
        if (dir == 0.0f) return;
        const int steps = param->getNumSteps();
        const float inc = steps > 1 && steps < 200 ? 1.0f / (float) (steps - 1) : 0.02f;
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, param->getValue() + dir * inc)));
    }

private:
    void showList()
    {
        if (choice == nullptr) return;
        juce::PopupMenu menu;
        for (int i = 0; i < choice->choices.size(); ++i)
            menu.addItem (i + 1, choice->choices[i], true, choice->getIndex() == i);
        juce::Component::SafePointer<RedChoice> safe (this);
        auto opts = juce::PopupMenu::Options().withTargetComponent (this);
        if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
            opts = opts.withParentComponent (editor);
        menu.showMenuAsync (opts, [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->attachment->setValueAsCompleteGesture ((float) (result - 1));
        });
    }

    juce::RangedAudioParameter* param;
    juce::AudioParameterChoice* choice = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float startNorm = 0.0f;
    bool dragged = false;
};

// ---------------- fileira de chaves com LED em cima (uma por opção da escolha) ----------------
class LedSegmented : public juce::Component
{
public:
    LedSegmented (APVTS& state, const juce::String& paramId, juce::StringArray labelsOverride = {})
        : param (dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId))), labels (std::move (labelsOverride))
    {
        jassert (param != nullptr);
        if (labels.isEmpty()) labels = param->choices;
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    float fontSize = 10.0f;

    void paint (juce::Graphics& g) override
    {
        const int n = labels.size();
        const float gap = 4.0f;
        const float w = ((float) getWidth() - gap * (float) (n - 1)) / (float) n;
        const int sel = param->getIndex();
        for (int i = 0; i < n; ++i)
        {
            const RF key ((w + gap) * (float) i, 10.0f, w, (float) getHeight() - 13.0f);
            const bool on = i == sel;
            ledDot (g, key.getCentreX(), 3.5f, 3.0f, on);
            rubber (g, key, on, false, juce::Colour (0xff3f3f43), juce::Colour (0xff232326));
            g.setFont (sans (fontSize, true, 0.05f));
            g.setColour (on ? juce::Colours::white : juce::Colour (0xffcfcabd));
            g.drawFittedText (labels[i], key.translated (0.0f, on ? 2.0f : 0.0f).reduced (2.0f, 0.0f).toNearestInt(),
                              juce::Justification::centred, 1, 0.6f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = labels.size();
        const int i = juce::jlimit (0, n - 1, (int) (e.position.x / (float) getWidth() * (float) n));
        attachment->setValueAsCompleteGesture ((float) i);
    }

private:
    juce::AudioParameterChoice* param;
    juce::StringArray labels;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ---------------- chave liga/desliga com LED em cima (QUEM VARIA) ----------------
class LedToggle : public juce::Button
{
public:
    explicit LedToggle (const juce::String& text) : juce::Button (text) { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        const RF key (0.0f, 10.0f, (float) getWidth(), (float) getHeight() - 13.0f);
        ledDot (g, key.getCentreX(), 3.5f, 3.0f, on);
        rubber (g, key, on, over, juce::Colour (0xff3f3f43), juce::Colour (0xff232326));
        g.setFont (sans (9.0f, true, 0.05f));
        g.setColour (on ? juce::Colours::white : juce::Colour (0xffcfcabd));
        g.drawFittedText (getButtonText(), key.translated (0.0f, on ? 2.0f : 0.0f).reduced (2.0f, 0.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);
    }
};

// ---------------- mini botão quadrado da fileira (M, S, dado) ----------------
class MiniButton : public juce::Button
{
public:
    enum class Kind { mute, solo, dice };
    explicit MiniButton (Kind k) : juce::Button ("mini"), kind (k) { setClickingTogglesState (k != Kind::dice); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        if (on)
        {
            const auto c = kind == Kind::mute ? yellow : juce::Colour (0xff3dd8ff);
            g.setColour (c.withAlpha (0.35f));
            g.fillRoundedRectangle (r.expanded (2.0f), 4.0f);
            rubber (g, r, false, over, c.brighter (0.2f), c.darker (0.25f));
        }
        else
            rubber (g, r, down, over);
        const auto txt = on ? juce::Colour (0xff111111) : juce::Colour (0xffcfcabd);
        if (kind == Kind::dice) { diceIcon (g, r.reduced (4.5f), txt); return; }
        g.setFont (sans (9.0f));
        g.setColour (txt);
        g.drawText (kind == Kind::mute ? "M" : "S", r, juce::Justification::centred, false);
    }

private:
    Kind kind;
};

// ---------------- cadeado (trava) ----------------
class LockButton : public juce::Button
{
public:
    LockButton() : juce::Button ("lock") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        const auto r = getLocalBounds().toFloat().reduced ((float) getHeight() * 0.2f);
        if (on)
        {
            g.setColour (yellow.withAlpha (0.25f));
            g.fillEllipse (r.expanded (3.0f));
        }
        lockIcon (g, r, on ? yellow : over ? silk : dim, on);
    }
};

// ---------------- botão de borracha com ícone ----------------
class RubberButton : public juce::Button
{
public:
    enum class Icon { none, dice, play, undo, drag, prev, next, plus };

    RubberButton (const juce::String& text, Icon i = Icon::none) : juce::Button (text), icon (i) {}

    bool hero = false;    // GERAR GROOVE: a tecla vermelha grande (START)

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        r.removeFromBottom (4.0f);
        if (hero)
        {
            g.setColour (red.withAlpha (over ? 0.4f : 0.25f));
            g.fillRoundedRectangle (r.expanded (5.0f), 9.0f);
            if (! down)
            {
                g.setColour (juce::Colour (0xff5a0f09));
                g.fillRoundedRectangle (r.translated (0.0f, 3.5f), 5.0f);
            }
            const auto rr = down ? r.translated (0.0f, 3.0f) : r;
            g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xfff85a4b : 0xfff0493a), rr.getX(), rr.getY(),
                                                     juce::Colour (0xffb8251a), rr.getX(), rr.getBottom(), false));
            g.fillRoundedRectangle (rr, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.fillRect (rr.getX() + 4.0f, rr.getY() + 1.0f, rr.getWidth() - 8.0f, 1.0f);
            r = rr;
        }
        else
        {
            rubber (g, r, down, over);
            if (down) r = r.translated (0.0f, 2.0f);
        }

        const auto col = hero ? juce::Colours::white : juce::Colour (0xffe9e6dc);
        const auto text = getButtonText();
        const float ih = juce::jmin (hero ? 16.0f : 13.0f, r.getHeight() * 0.42f);
        g.setFont (sans (hero ? 15.0f : 11.0f, true, hero ? 0.2f : 0.14f));
        const float tw = text.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
        const float gap = icon != Icon::none && text.isNotEmpty() ? 8.0f : 0.0f;
        const float iw = icon != Icon::none ? ih : 0.0f;
        float x = r.getCentreX() - (tw + gap + iw) * 0.5f - (float) rightPad * 0.5f;
        drawIcon (g, RF (x, r.getCentreY() - ih * 0.5f, iw, ih), col);
        x += iw + gap;
        g.setColour (col);
        g.drawText (text, RF (x, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft, false);
    }

    int rightPad = 0;     // espaço reservado à direita (o cadeado fica por cima)

private:
    void drawIcon (juce::Graphics& g, RF r, juce::Colour c)
    {
        juce::Path p;
        const auto m = r.getCentre();
        const float s = r.getHeight() * 0.5f;
        switch (icon)
        {
            case Icon::dice: diceIcon (g, r, c); return;
            case Icon::play: p.addTriangle (m.x - s * 0.6f, m.y - s * 0.75f, m.x - s * 0.6f, m.y + s * 0.75f, m.x + s * 0.75f, m.y); break;
            case Icon::prev: p.addTriangle (m.x - s * 0.7f, m.y, m.x + s * 0.5f, m.y - s * 0.7f, m.x + s * 0.5f, m.y + s * 0.7f); break;
            case Icon::next: p.addTriangle (m.x + s * 0.7f, m.y, m.x - s * 0.5f, m.y - s * 0.7f, m.x - s * 0.5f, m.y + s * 0.7f); break;
            case Icon::undo: p.addTriangle (m.x - s * 0.7f, m.y, m.x + s * 0.5f, m.y - s * 0.7f, m.x + s * 0.5f, m.y + s * 0.7f); break;
            case Icon::plus:
                p.addRectangle (m.x - s * 0.6f, m.y - 1.0f, s * 1.2f, 2.0f);
                p.addRectangle (m.x - 1.0f, m.y - s * 0.6f, 2.0f, s * 1.2f);
                break;
            case Icon::drag:
            {
                juce::Path a;
                a.startNewSubPath (m.x - s * 0.6f, m.y - s * 0.6f);
                a.lineTo (m.x + s * 0.5f, m.y + s * 0.5f);
                g.setColour (c);
                g.strokePath (a, juce::PathStrokeType (1.6f));
                p.addTriangle (m.x + s * 0.7f, m.y + s * 0.7f, m.x - s * 0.1f, m.y + s * 0.6f, m.x + s * 0.6f, m.y - s * 0.1f);
                break;
            }
            case Icon::none: return;
        }
        g.setColour (c);
        g.fillPath (p);
    }

    Icon icon;
};

// ---------------- chave basculante + LED (DELAY, REVERB) ----------------
class Rocker : public juce::Button
{
public:
    Rocker() : juce::Button ("rocker") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const bool on = getToggleState();
        const RF r (2.0f, 2.0f, 20.0f, (float) getHeight() - 4.0f);
        g.setColour (juce::Colour (0xff0b0b0c));
        g.fillRoundedRectangle (r.expanded (2.0f), 4.0f);
        const auto upper = r.withHeight (r.getHeight() * 0.5f), lower = upper.translated (0.0f, r.getHeight() * 0.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6a6e), 0.0f, r.getY(), juce::Colour (0xff3a3a3d), 0.0f, r.getCentreY(), false));
        g.fillRoundedRectangle (on ? upper : lower, 2.0f);
        g.setColour (juce::Colour (0xff1e1e20));
        g.fillRoundedRectangle (on ? lower : upper, 2.0f);
        ledDot (g, 36.0f, (float) getHeight() * 0.5f, 5.0f, on);
    }
};

// ---------------- nome do preset (display vermelho; clique abre a lista) ----------------
class PresetDisplay : public juce::Component
{
public:
    std::function<void()> onClick;

    void set (const juce::String& n)
    {
        if (n == name) return;
        name = n;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        displayWell (g, r);
        ledText (g, name.toUpperCase(), r.reduced (10.0f, 0.0f), 14.0f, juce::Justification::centredLeft);
    }

    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String name;
};

// ---------------- tela em escala: tudo em 1200×740, ampliado junto ----------------
class ScaledCanvas : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> onPaint;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
};

// ---------------- caixas de diálogo dos presets (nome, confirmação), dentro da janela do plugin ----------------
class PresetDialog
{
public:
    ~PresetDialog() { close(); }

    void askName (juce::Component& parent, const juce::String& title, const juce::String& initial,
                  std::function<void (const juce::String&)> onOk)
    {
        close();
        auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", initial);
        w->addButton ("Salvar", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<juce::AlertWindow> sw (w);
        open (parent, w, [sw, onOk] (int result)
        {
            if (result != 1 || sw == nullptr) return;
            const auto name = sw->getTextEditorContents ("name").trim();
            if (name.isNotEmpty()) onOk (name);
        });
        if (auto* ed = w->getTextEditor ("name"))
        {
            ed->grabKeyboardFocus();
            ed->selectAll();
        }
    }

    void confirm (juce::Component& parent, const juce::String& title, const juce::String& message,
                  const juce::String& okText, std::function<void()> onOk)
    {
        close();
        auto* w = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon);
        w->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        open (parent, w, [onOk] (int result) { if (result == 1) onOk(); });
    }

    void inform (juce::Component& parent, const juce::String& title, const juce::String& message)
    {
        close();
        auto* w = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        open (parent, w, [] (int) {});
    }

    void close()
    {
        if (current != nullptr)
            current->exitModalState (0);
    }

private:
    void open (juce::Component& parent, juce::AlertWindow* w, std::function<void (int)> done)
    {
        parent.addAndMakeVisible (w);
        w->setCentrePosition (parent.getLocalBounds().getCentre());
        current = w;
        w->enterModalState (true, juce::ModalCallbackFunction::create (std::move (done)), true);   // apaga ao fechar
    }

    juce::Component::SafePointer<juce::AlertWindow> current;
};
} // namespace gv
