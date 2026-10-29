#pragma once

// RESENHA — peças próprias da tela. Theme.h e Widgets.h são os da casa (iguais em todos, não mexer);
// o visual escuro com neon psicodélico (ciano, magenta, verde-limão, violeta) fica aqui.

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>
#include "Theme.h"
#include "Widgets.h"
#include "Params.h"

namespace rs
{
inline constexpr int   baseW = 1200, baseH = 740;
inline constexpr float minScale = 0.75f, maxScale = 2.0f;

using RF = juce::Rectangle<float>;

inline const juce::Colour bg0     { 0xff07061a };
inline const juce::Colour bg1     { 0xff1a0b33 };
inline const juce::Colour panel   { 0xff0f0f26 };
inline const juce::Colour panelHi { 0xff17183a };
inline const juce::Colour edge    { 0xff2a2b55 };
inline const juce::Colour ink     { 0xffe4e8ff };
inline const juce::Colour dim     { 0xff7c80aa };
inline const juce::Colour faint   { 0xff3c3f6a };
inline const juce::Colour cyan    { 0xff23e5ff };
inline const juce::Colour magenta { 0xffff3fb4 };
inline const juce::Colour lime    { 0xffc6ff3a };
inline const juce::Colour violet  { 0xff9d6bff };
inline const juce::Colour orange  { 0xffffa53a };
inline const juce::Colour amber   { 0xffffc23a };

inline juce::Colour voiceColour (int v)
{
    switch (v) { case 0: return cyan; case 1: return magenta; case 2: return lime; default: return violet; }
}

inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

// gradiente da marca: ciano → violeta → magenta → laranja
inline juce::Colour brand (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    if (t < 0.35f) return cyan.interpolatedWith (violet, t / 0.35f);
    if (t < 0.7f)  return violet.interpolatedWith (magenta, (t - 0.35f) / 0.35f);
    return magenta.interpolatedWith (orange, (t - 0.7f) / 0.3f);
}

inline juce::ColourGradient brandGradient (float x0, float x1, float y)
{
    juce::ColourGradient g (cyan, x0, y, orange, x1, y, false);
    g.addColour (0.35, violet);
    g.addColour (0.7, magenta);
    return g;
}

// caixa escura com borda fina (painéis, visores)
inline void box (juce::Graphics& g, RF r, float corner = 5.0f, juce::Colour border = edge, float fillAlpha = 1.0f)
{
    g.setGradientFill (juce::ColourGradient (panelHi.withAlpha (fillAlpha), r.getX(), r.getY(),
                                             panel.withAlpha (fillAlpha), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (border);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

// visor: fundo quase preto
inline void screen (juce::Graphics& g, RF r, float corner = 4.0f)
{
    g.setColour (juce::Colour (0xff05050f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (edge.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

// brilho neon em volta de um retângulo
inline void glow (juce::Graphics& g, RF r, float corner, juce::Colour c, float amount)
{
    if (amount <= 0.01f) return;
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.07f * amount * (float) (5 - i) / 4.0f));
        g.drawRoundedRectangle (r.expanded ((float) i * 1.6f), corner + (float) i * 1.6f, 2.4f);
    }
}

// título de seção: espaçado, com um traço no gradiente da marca embaixo
inline void sectionTitle (juce::Graphics& g, float x, float y, float w, const juce::String& text)
{
    g.setFont (theme::sans (11.5f, true, 0.22f));
    g.setColour (ink);
    g.drawText (text, RF (x, y, w, 15.0f), juce::Justification::centredLeft, false);
    g.setGradientFill (brandGradient (x, x + 26.0f, y));
    g.fillRoundedRectangle (x, y + 18.0f, 26.0f, 2.0f, 1.0f);
    g.setColour (edge.withAlpha (0.6f));
    g.fillRect (x + 30.0f, y + 18.5f, w - 30.0f, 1.0f);
}

inline void label (juce::Graphics& g, const juce::String& text, RF area, juce::Justification j = juce::Justification::centredLeft,
                   float size = 8.5f, juce::Colour c = dim)
{
    g.setFont (theme::sans (size, true, 0.16f));
    g.setColour (c);
    g.drawText (text, area, j, false);
}

// ícones desenhados (sem emoji: nem toda fonte tem)
inline void diceIcon (juce::Graphics& g, RF r, juce::Colour c)
{
    const auto s = juce::jmin (r.getWidth(), r.getHeight());
    const RF d = r.withSizeKeepingCentre (s, s);
    g.setColour (c);
    g.drawRoundedRectangle (d.reduced (s * 0.08f), s * 0.18f, juce::jmax (1.0f, s * 0.09f));
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

// ---------------- look: knob neon, menus escuros ----------------
class NeonLookAndFeel : public BrandLookAndFeel
{
public:
    NeonLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff12122c));
        setColour (juce::PopupMenu::textColourId, ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, cyan.withAlpha (0.22f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, dim);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float a0, float a1, juce::Slider& slider) override
    {
        const auto col = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const bool bipolar = (bool) slider.getProperties()["bipolar"];
        const auto r = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre();
        const float an = a0 + pos * (a1 - a0);
        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        const float arcR = rad - 2.0f;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (faint.withAlpha (0.9f));
        g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (std::abs (an - from) > 0.01f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, an), juce::jmax (from, an), true);
            g.setColour (col.withAlpha (0.18f));
            g.strokePath (val, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col);
            g.strokePath (val, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // corpo: disco escuro com aro claro em cima
        const float kr = rad * 0.64f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - kr, c.y - kr + 1.5f, kr * 2.0f, kr * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d72), c.x, c.y - kr, juce::Colour (0xff15162c), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x, c.y - kr,
                                                 juce::Colours::black.withAlpha (0.3f), c.x, c.y + kr, false));
        g.drawEllipse (c.x - kr + 0.5f, c.y - kr + 0.5f, kr * 2.0f - 1.0f, kr * 2.0f - 1.0f, 1.0f);

        juce::Path ptr;
        ptr.startNewSubPath (c.x + kr * 0.25f * std::sin (an), c.y - kr * 0.25f * std::cos (an));
        ptr.lineTo (c.x + kr * 0.92f * std::sin (an), c.y - kr * 0.92f * std::cos (an));
        g.setColour (col.brighter (0.4f));
        g.strokePath (ptr, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

// ---------------- knob com nome e valor embaixo ----------------
class NeonKnob : public juce::Component
{
public:
    NeonKnob (APVTS& state, const juce::String& paramId, const juce::String& labelText, juce::Colour onColour,
              float diameter = 32.0f, bool bipolar = false)
        : param (state.getParameter (paramId)), label (labelText), dia (diameter)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (200);
        slider.getProperties().set ("bipolar", bipolar);
        slider.setColour (juce::Slider::rotarySliderFillColourId, onColour);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    void setDim (bool d) { if (d != dimmed) { dimmed = d; repaint(); } }

    void resized() override
    {
        const int s = juce::roundToInt (dia);
        slider.setBounds ((getWidth() - s) / 2, 0, s, s);
    }

    void paint (juce::Graphics& g) override
    {
        const float top = dia + 1.0f, w = (float) getWidth();
        g.setFont (theme::sans (dia > 34.0f ? 8.5f : 7.5f, true, 0.1f));
        g.setColour (dimmed ? faint : ink.withAlpha (0.9f));
        g.drawFittedText (label, juce::Rectangle<int> (0, (int) top, (int) w, 11), juce::Justification::centred, 1, 0.7f);
        g.setFont (theme::mono (dia > 34.0f ? 9.5f : 8.5f));
        g.setColour (dimmed ? faint : dim);
        g.drawText (param->getCurrentValueAsText(), RF (0.0f, top + 10.0f, w, 11.0f), juce::Justification::centred, false);
    }

private:
    juce::RangedAudioParameter* param;
    juce::String label;
    float dia;
    bool dimmed = false;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- visor de escolha: clique abre a lista, arraste/roda troca ----------------
class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (APVTS& state, const juce::String& paramId, juce::Colour textColour, juce::String suffixText = {})
        : param (state.getParameter (paramId)), colour (textColour), suffix (std::move (suffixText))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
        choice = dynamic_cast<juce::AudioParameterChoice*> (param);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setColour (juce::Colour c) { if (c != colour) { colour = c; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        screen (g, r, 4.0f);
        g.setColour (colour);
        g.setFont (theme::sans (juce::jmin (12.5f, r.getHeight() * 0.55f), true, 0.08f));
        g.drawFittedText (param->getCurrentValueAsText() + suffix, r.reduced (14.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        juce::Path tri;
        const float x = r.getRight() - 9.0f, y = r.getCentreY();
        tri.addTriangle (x - 3.0f, y - 1.5f, x + 3.0f, y - 1.5f, x, y + 2.0f);
        g.setColour (colour.withAlpha (0.5f));
        g.fillPath (tri);
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
        juce::Component::SafePointer<ChoiceBox> safe (this);
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
    juce::Colour colour;
    juce::String suffix;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float startNorm = 0.0f;
    bool dragged = false;
};

// ---------------- seletor em segmentos (um botão por opção da escolha) ----------------
class Segmented : public juce::Component
{
public:
    Segmented (APVTS& state, const juce::String& paramId, juce::Colour onColour)
        : param (dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId))), colour (onColour)
    {
        jassert (param != nullptr);
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        screen (g, r, 4.0f);
        const int n = param->choices.size();
        const float w = r.getWidth() / (float) n;
        const int sel = param->getIndex();
        for (int i = 0; i < n; ++i)
        {
            const RF cell (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
            if (i > 0)
            {
                g.setColour (edge.withAlpha (0.7f));
                g.fillRect (cell.getX(), cell.getY() + 4.0f, 1.0f, cell.getHeight() - 8.0f);
            }
            const bool on = i == sel;
            if (on)
            {
                const auto c = cell.reduced (1.5f);
                g.setColour (colour.withAlpha (0.2f));
                g.fillRoundedRectangle (c, 3.0f);
                g.setColour (colour);
                g.drawRoundedRectangle (c, 3.0f, 1.2f);
                g.fillRect (c.getX() + 3.0f, c.getBottom() - 2.5f, c.getWidth() - 6.0f, 2.0f);
            }
            g.setColour (on ? juce::Colours::white : dim);
            g.setFont (theme::sans (juce::jmin (11.0f, r.getHeight() * 0.48f), true, 0.04f));
            g.drawFittedText (param->choices[i], cell.reduced (3.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = param->choices.size();
        const int i = juce::jlimit (0, n - 1, (int) (e.position.x / (float) getWidth() * (float) n));
        attachment->setValueAsCompleteGesture ((float) i);
    }

private:
    juce::AudioParameterChoice* param;
    juce::Colour colour;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ---------------- cadeado (trava): liga/desliga um parâmetro booleano ----------------
class LockButton : public juce::Button
{
public:
    LockButton() : juce::Button ("lock") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        lockIcon (g, getLocalBounds().toFloat().reduced (getHeight() * 0.18f), on ? amber : over ? ink : dim, on);
    }
};

// ---------------- botão de ação (dado, desfazer, setas…) ----------------
class NeonButton : public juce::Button
{
public:
    enum class Icon { none, dice, prev, next, save, undo, drag };

    NeonButton (const juce::String& text, Icon i = Icon::none, juce::Colour accent = cyan)
        : juce::Button (text), icon (i), colour (accent) {}

    bool hero = false;   // o GERAR CONVERSA: fundo no gradiente da marca

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        if (down) r = r.translated (0.0f, 0.8f);
        if (hero)
        {
            glow (g, r, 6.0f, magenta, over ? 1.0f : 0.6f);
            auto grad = brandGradient (r.getX(), r.getRight(), r.getY());
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (down ? 0.0f : over ? 0.18f : 0.08f));
            g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f), 6.0f);
        }
        else
        {
            box (g, r, 5.0f, over ? colour.withAlpha (0.7f) : edge);
        }

        const auto textCol = hero ? juce::Colours::white : (over || down) ? colour : ink;
        const auto text = getButtonText();
        const float ih = juce::jmin (14.0f, r.getHeight() * 0.42f);
        g.setFont (theme::sans (hero ? 14.0f : juce::jmin (11.5f, r.getHeight() * 0.42f), true, hero ? 0.18f : 0.14f));
        const float tw = text.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
        const float gap = icon != Icon::none && text.isNotEmpty() ? 8.0f : 0.0f;
        const float iw = icon != Icon::none ? ih : 0.0f;
        float x = r.getCentreX() - (tw + gap + iw) * 0.5f;
        const RF ir (x, r.getCentreY() - ih * 0.5f, iw, ih);
        drawIcon (g, ir, textCol);
        x += iw + gap;
        g.setColour (textCol);
        g.drawText (text, RF (x, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft, false);
    }

private:
    void drawIcon (juce::Graphics& g, RF r, juce::Colour c)
    {
        juce::Path p;
        const auto m = r.getCentre();
        const float s = r.getHeight() * 0.5f;
        switch (icon)
        {
            case Icon::dice: diceIcon (g, r, c); return;
            case Icon::prev: p.addTriangle (m.x - s * 0.6f, m.y, m.x + s * 0.5f, m.y - s * 0.7f, m.x + s * 0.5f, m.y + s * 0.7f); break;
            case Icon::next: p.addTriangle (m.x + s * 0.6f, m.y, m.x - s * 0.5f, m.y - s * 0.7f, m.x - s * 0.5f, m.y + s * 0.7f); break;
            case Icon::save:
                p.addRectangle (m.x - s * 0.7f, m.y - 1.0f, s * 1.4f, 2.0f);
                p.addRectangle (m.x - 1.0f, m.y - s * 0.7f, 2.0f, s * 1.4f);
                break;
            case Icon::undo:
            {
                juce::Path a;
                a.addCentredArc (m.x, m.y + s * 0.15f, s * 0.6f, s * 0.6f, 0.0f, -2.2f, 1.6f, true);
                g.setColour (c);
                g.strokePath (a, juce::PathStrokeType (1.6f));
                const float ax = m.x + s * 0.6f * std::sin (-2.2f), ay = m.y + s * 0.15f - s * 0.6f * std::cos (-2.2f);
                p.addTriangle (ax - s * 0.35f, ay - s * 0.05f, ax + s * 0.25f, ay - s * 0.35f, ax + s * 0.15f, ay + s * 0.3f);
                break;
            }
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
    juce::Colour colour;
};

// ---------------- chave liga/desliga (DELAY, REVERB) com a luz na cor da seção ----------------
class NeonSwitch : public juce::Button
{
public:
    explicit NeonSwitch (juce::Colour c) : juce::Button ("switch"), colour (c) { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const bool on = getToggleState();
        const auto r = getLocalBounds().toFloat();
        const RF slot (r.getX() + 2.0f, r.getY() + 2.0f, 16.0f, r.getHeight() - 4.0f);
        screen (g, slot, 3.0f);
        const RF cap (slot.getX() + 2.0f, on ? slot.getY() + 2.0f : slot.getCentreY(), slot.getWidth() - 4.0f, slot.getHeight() * 0.5f - 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5d82), cap.getX(), cap.getY(), juce::Colour (0xff24253f), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.0f);
        const juce::Point<float> led (slot.getRight() + 12.0f, r.getCentreY());
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.55f), led.x, led.y, colour.withAlpha (0.0f), led.x + 13.0f, led.y, true));
            g.fillEllipse (led.x - 13.0f, led.y - 13.0f, 26.0f, 26.0f);
        }
        g.setColour (on ? colour.brighter (0.5f) : faint);
        g.fillEllipse (led.x - 5.0f, led.y - 5.0f, 10.0f, 10.0f);
    }

private:
    juce::Colour colour;
};

// ---------------- nome do preset (clique abre a lista) ----------------
class NeonPresetName : public juce::Component
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
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        screen (g, r, 5.0f);
        auto inner = getLocalBounds().reduced (14, 0);
        g.setColour (dim);
        g.setFont (theme::mono (11.0f));
        g.drawText (juce::String (idx + 1).paddedLeft ('0', 2) + "/" + juce::String (count).paddedLeft ('0', 2),
                    inner.removeFromRight (56), juce::Justification::centredRight);
        g.setColour (ink);
        g.setFont (theme::mono (14.0f));
        g.drawText (name, inner, juce::Justification::centredLeft, true);
    }

    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String name;
    int idx = 0, count = 0;
};
} // namespace rs
