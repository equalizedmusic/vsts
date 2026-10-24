#pragma once

// LIMÃO — peças próprias da tela. Theme.h e Widgets.h são os da casa (iguais em todos, não mexer);
// o painel prateado (homenagem ao 303) e o que precisa de tinta escura em cima do metal claro ficam aqui.

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>
#include "Theme.h"
#include "Widgets.h"
#include "Params.h"

namespace lim
{
inline constexpr int   baseW = 1200, baseH = 740;
inline constexpr float minScale = 0.75f, maxScale = 2.0f;

inline const juce::Colour ink     { 0xff1c1f1a };   // serigrafia preta no painel branco
inline const juce::Colour inkDim  { 0xff5c6258 };
inline const juce::Colour lemon   { 0xffb6f04a };   // verde-limão aceso (LEDs, visores)
inline const juce::Colour lime    { 0xff7ed321 };   // verde-limão da marca
inline const juce::Colour peel    { 0xff3f7a10 };   // verde-limão escuro para tinta no branco

using RF = juce::Rectangle<float>;

// chapa branca (o corpo do aparelho): pintura branca levemente acetinada, com grão fino
inline void silver (juce::Graphics& g, RF r, float corner)
{
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (real::black (0.12f));
        g.fillRoundedRectangle (r.expanded ((float) i).translated (0.0f, (float) i * 0.9f), corner + (float) i);
    }
    auto body = real::vertical (juce::Colour (0xfffafbf8), r.getY(), juce::Colour (0xffd6d9d2), r.getBottom());
    body.addColour (0.35, juce::Colour (0xfff0f2ed));
    body.addColour (0.75, juce::Colour (0xffe3e6df));
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);

    juce::Path face;
    face.addRoundedRectangle (r, corner);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (face);
        juce::Random rnd (99);
        for (float y = r.getY(); y < r.getBottom(); y += 1.0f)     // grão bem leve da pintura
        {
            const float a = rnd.nextFloat();
            g.setColour (a > 0.5f ? real::white (0.08f * (a - 0.5f)) : real::black (0.025f * (0.5f - a)));
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
        }
        // luz de cima: reflexo largo e suave
        g.setGradientFill (real::vertical (real::white (0.22f), r.getY(), real::white (0.0f), r.getY() + r.getHeight() * 0.35f));
        g.fillRect (r.withHeight (r.getHeight() * 0.35f));
    }
    real::fillTexture (g, face, 0.15f);

    auto bevel = real::vertical (real::white (0.8f), r.getY(), real::black (0.45f), r.getBottom());
    bevel.addColour (0.05, real::white (0.15f));
    bevel.addColour (0.95, real::black (0.1f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.2f);
}

// texto serigrafado no metal (sem sombra, tinta fosca)
inline void inkText (juce::Graphics& g, const juce::String& text, RF area, juce::Justification j,
                     float size = 9.5f, bool bold = true, juce::Colour c = ink)
{
    g.setColour (real::white (0.35f));
    g.setFont (theme::sans (size, bold, 0.14f));
    g.drawText (text, area.translated (0.0f, 0.8f), j, false);
    g.setColour (c);
    g.drawText (text, area, j, false);
}

// título de seção: nome com um filete verde-limão embaixo
inline void sectionTitle (juce::Graphics& g, float x, float y, float w, const juce::String& text)
{
    inkText (g, text, { x, y, w, 14.0f }, juce::Justification::centredLeft, 10.5f, true);
    g.setColour (peel);
    g.fillRect (x, y + 16.0f, 22.0f, 2.0f);
    g.setColour (ink.withAlpha (0.25f));
    g.fillRect (x + 26.0f, y + 16.5f, w - 26.0f, 1.0f);
}

// o knob fica um pouco menor que a caixa e com o TOPO (tampa e ponteiro) no centro dela: a escala é centrada
// no mesmo ponto, então o ponteiro aponta certo para cada número e a saia não invade os números de baixo.
// real::knob põe o topo 0,65 × altura da saia acima do centro da caixa que recebe: aqui a caixa desce isso.
inline RF knobBox (RF box, bool big)
{
    const float f  = big ? 0.82f : 0.9f;
    const float k  = juce::jmin (box.getWidth(), box.getHeight()) / (big ? 92.0f : 64.0f) * f;
    const float hg = (big ? 9.0f : 6.5f) * k;
    return box.withSizeKeepingCentre (box.getWidth() * f, box.getHeight() * f).translated (0.0f, hg * 0.65f);
}

// escala 0–10 em tinta preta em volta do knob, centrada na tampa
inline void inkScale (juce::Graphics& g, RF box, bool big)
{
    const float pi = juce::MathConstants<float>::pi;
    const float k  = juce::jmin (box.getWidth(), box.getHeight()) / (big ? 92.0f : 64.0f);
    const float cx = box.getCentreX();
    const float cy = box.getCentreY();
    const float t1 = (big ? 32.5f : 25.0f) * k, t2 = (big ? 36.0f : 28.5f) * k;
    for (int i = 0; i <= 10; ++i)
    {
        const float an = pi * 1.25f + pi * 1.5f * (float) i / 10.0f;
        const bool major = i % 5 == 0;
        const float r1 = major ? t1 - 1.5f * k : t1;
        juce::Path l;
        l.startNewSubPath (cx + r1 * std::sin (an), cy - r1 * std::cos (an));
        l.lineTo (cx + t2 * std::sin (an), cy - t2 * std::cos (an));
        g.setColour (ink.withAlpha (major ? 0.9f : 0.55f));
        g.strokePath (l, juce::PathStrokeType ((major ? 1.5f : 1.0f) * k));
        if (big)
        {
            const float rn = 40.5f * k;     // cabe dentro da caixa do knob (não corta), espaço igual em volta
            g.setColour (ink.withAlpha (0.85f));
            g.setFont (theme::sans (7.0f * k, true));
            g.drawText (juce::String (i), RF (cx + rn * std::sin (an) - 8.0f * k, cy - rn * std::cos (an) - 5.0f * k, 16.0f * k, 10.0f * k),
                        juce::Justification::centred, false);
        }
    }
}

// knob Moog com escala em tinta preta (o desenho do knob é o da casa, sem a escala clara)
class LimaoLookAndFeel : public BrandLookAndFeel
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float, float, juce::Slider& slider) override
    {
        const bool big = (bool) slider.getProperties()["big"];
        const auto box = juce::Rectangle<int> (x, y, width, height).toFloat();
        inkScale (g, box, big);
        real::knob (g, knobBox (box, big), pos, slider.findColour (juce::Slider::rotarySliderFillColourId), big, {}, false);
    }
};

// ---------------- knob com nome e valor serigrafados ----------------
class SilverKnob : public juce::Component
{
public:
    SilverKnob (APVTS& state, const juce::String& paramId, const juce::String& labelText, juce::Colour onColour, bool big = false)
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
        const int s = isBig ? juce::jmin (getWidth(), getHeight() - 30) : 64;
        slider.setBounds (getLocalBounds().removeFromTop (s).withSizeKeepingCentre (s, s));
    }

    void paint (juce::Graphics& g) override
    {
        const float top = (float) slider.getBottom() - 2.0f;
        const float w = (float) getWidth();
        inkText (g, label, { 0.0f, top, w, 13.0f }, juce::Justification::centred, isBig ? 11.0f : 9.5f, true);
        g.setColour (inkDim);
        g.setFont (theme::mono (isBig ? 12.0f : 10.5f));
        g.drawText (param->getCurrentValueAsText(), RF (0.0f, top + 13.0f, w, 14.0f), juce::Justification::centred, false);
    }

private:
    juce::RangedAudioParameter* param;
    juce::String label;
    bool isBig;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- visor de valor: clique abre a lista (escolha) ou arraste na vertical (número) ----------------
class ParamBox : public juce::Component
{
public:
    ParamBox (APVTS& state, const juce::String& paramId, juce::Colour textColour, std::vector<int> quickValues = {})
        : param (state.getParameter (paramId)), colour (textColour), quick (std::move (quickValues))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
        choice = dynamic_cast<juce::AudioParameterChoice*> (param);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        real::screen (g, r, 2.0f);
        g.setColour (colour);
        g.setFont (theme::mono (13.0f));
        g.drawText (param->getCurrentValueAsText(), r.reduced (4.0f, 0.0f), juce::Justification::centred, false);
        if (choice != nullptr || ! quick.empty())
        {
            juce::Path tri;     // setinha: tem lista
            const float x = r.getRight() - 9.0f, y = r.getCentreY();
            tri.addTriangle (x - 3.0f, y - 1.5f, x + 3.0f, y - 1.5f, x, y + 2.0f);
            g.setColour (colour.withAlpha (0.5f));
            g.fillPath (tri);
        }
        real::glass (g, r, 2.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragged = false;
        startNorm = param->getValue();
        attachment->beginGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (std::abs (e.getDistanceFromDragStartY()) < 3 && ! dragged) return;
        dragged = true;
        const float norm = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / 160.0f);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (norm));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        attachment->endGesture();
        if (! dragged) showList();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
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
        juce::PopupMenu menu;
        if (choice != nullptr)
        {
            for (int i = 0; i < choice->choices.size(); ++i)
                menu.addItem (i + 1, choice->choices[i], true, choice->getIndex() == i);
        }
        else if (! quick.empty())
        {
            const int now = juce::roundToInt (param->convertFrom0to1 (param->getValue()));
            for (int v : quick)
                menu.addItem (v + 1000, juce::String (v), true, v == now);
        }
        else
            return;

        juce::Component::SafePointer<ParamBox> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            const float v = result >= 1000 ? (float) (result - 1000) : (float) (result - 1);
            safe->attachment->setValueAsCompleteGesture (v);
        });
    }

    juce::RangedAudioParameter* param;
    juce::AudioParameterChoice* choice = nullptr;
    juce::Colour colour;
    std::vector<int> quick;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float startNorm = 0.0f;
    bool dragged = false;
};

// ---------------- chave basculante + lâmpada (liga/desliga), nome em tinta preta ou creme ----------------
class Rocker : public juce::Button
{
public:
    Rocker (const juce::String& text, juce::Colour led, bool onDarkPlate = false)
        : juce::Button (text), ledColour (led), dark (onDarkPlate)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const bool on = getToggleState();
        const auto r  = getLocalBounds().toFloat();
        const float rh = juce::jmin (28.0f, r.getHeight() - 6.0f), rw = 17.0f;
        real::rocker (g, { r.getX() + 4.0f, r.getCentreY() - rh * 0.5f, rw, rh }, on);
        const RF txt (r.getX() + rw + 12.0f, r.getY(), r.getWidth() - rw - 30.0f, r.getHeight());
        if (dark)
        {
            g.setFont (theme::sans (9.5f, true, 0.14f));
            real::engraved (g, getButtonText(), txt, on ? theme::cream : theme::dim, juce::Justification::centredLeft);
        }
        else
            inkText (g, getButtonText(), txt, juce::Justification::centredLeft, 9.5f, true, on ? ink : inkDim);
        real::jewel (g, { r.getRight() - 10.0f, r.getCentreY() }, 4.6f, ledColour, on);
    }

private:
    juce::Colour ledColour;
    bool dark;
};

// ---------------- tecla de ação (ALEATORIZAR, LIMPAR, setas) ----------------
class ActionKey : public juce::Button
{
public:
    explicit ActionKey (const juce::String& text) : juce::Button (text) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto cap = real::keycap (g, getLocalBounds().toFloat().reduced (1.0f), 3.0f, down);
        g.setFont (theme::sans (9.0f, true, 0.12f));
        real::engraved (g, getButtonText(), cap.translated (0.0f, down ? 0.5f : 0.0f),
                        over || down ? lemon : theme::cream, juce::Justification::centred);
    }
};
} // namespace lim
