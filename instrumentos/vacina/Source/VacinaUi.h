#pragma once

// VACINA — peças próprias da tela. Theme.h e Widgets.h são os da casa (iguais em todos, não mexer);
// o painel grafite com laterais de madeira, o knob preto de borracha, as teclas cinza com LED vermelho,
// as listas de LEDs e o visor de cristal líquido ficam aqui.

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>
#include "Theme.h"
#include "Widgets.h"
#include "Params.h"

namespace vac
{
inline constexpr int   baseW = 1360, baseH = 600;
inline constexpr float minScale = 0.75f, maxScale = 2.0f;

inline const juce::Colour silk    { 0xffe9e9e6 };   // serigrafia branca
inline const juce::Colour silkDim { 0xff9c9ea2 };
inline const juce::Colour red     { 0xffff3324 };   // LEDs e segunda função
inline const juce::Colour redInk  { 0xffe2493b };
inline const juce::Colour lcdInk  { 0xff1c2427 };

using RF = juce::Rectangle<float>;
using PF = juce::Point<float>;

// ---------------- chapa do painel: grafite fosco, grão fino, luz de cima ----------------
inline void panel (juce::Graphics& g, RF r, float corner)
{
    for (int i = 5; i >= 1; --i)
    {
        g.setColour (real::black (0.14f));
        g.fillRoundedRectangle (r.expanded ((float) i).translated (0.0f, (float) i * 0.8f), corner + (float) i);
    }
    auto body = real::vertical (juce::Colour (0xff36373a), r.getY(), juce::Colour (0xff222326), r.getBottom());
    body.addColour (0.12, juce::Colour (0xff313235));
    body.addColour (0.6, juce::Colour (0xff2a2b2e));
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);

    juce::Path face;
    face.addRoundedRectangle (r, corner);
    real::fillTexture (g, face, 0.55f);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (face);
        juce::Random rnd (2024);
        for (float y = r.getY(); y < r.getBottom(); y += 1.0f)     // pintura texturizada (pó), bem leve
        {
            const float a = rnd.nextFloat();
            g.setColour (a > 0.5f ? real::white (0.022f * (a - 0.5f)) : real::black (0.05f * (0.5f - a)));
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
        }
        juce::ColourGradient sheen (real::white (0.07f), r.getCentreX(), r.getY(), real::white (0.0f), r.getCentreX(), r.getY() + r.getHeight() * 0.55f, false);
        g.setGradientFill (sheen);
        g.fillRect (r);
    }
    auto bevel = real::vertical (real::white (0.28f), r.getY(), real::black (0.7f), r.getBottom());
    bevel.addColour (0.03, real::white (0.06f));
    bevel.addColour (0.97, real::black (0.2f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.4f);
}

// ---------------- lateral de madeira (mogno envernizado) ----------------
inline void woodCheek (juce::Graphics& g, RF r, bool left)
{
    using juce::Colour;
    juce::Path p;
    p.addRoundedRectangle (r, 9.0f);

    // sombra no fundo
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (real::black (0.12f));
        g.fillRoundedRectangle (r.expanded ((float) i * 0.6f).translated (left ? -1.0f : 1.0f, (float) i), 9.0f + (float) i);
    }

    // volume: a peça é arredondada, mais clara onde a luz bate
    juce::ColourGradient cg (Colour (0xff2c0d08), r.getX(), 0.0f, Colour (0xff2c0d08), r.getRight(), 0.0f, false);
    cg.addColour (left ? 0.22 : 0.18, Colour (0xff6d2416));
    cg.addColour (left ? 0.5 : 0.45, Colour (0xff8a3320));
    cg.addColour (left ? 0.8 : 0.78, Colour (0xff5e1e12));
    g.setGradientFill (cg);
    g.fillPath (p);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (p);
        juce::Random rnd (left ? 77 : 131);

        // veios: linhas longas levemente onduladas, escuras e claras
        for (int i = 0; i < 70; ++i)
        {
            const float x0 = r.getX() + rnd.nextFloat() * r.getWidth();
            const float amp = 0.4f + rnd.nextFloat() * 2.2f, freq = 0.006f + rnd.nextFloat() * 0.02f, ph = rnd.nextFloat() * 6.28f;
            juce::Path vein;
            for (float y = r.getY() - 4.0f; y <= r.getBottom() + 4.0f; y += 6.0f)
            {
                const float x = x0 + amp * std::sin (y * freq + ph) + 0.7f * std::sin (y * freq * 3.7f + ph * 2.0f);
                if (y <= r.getY() - 4.0f) vein.startNewSubPath (x, y); else vein.lineTo (x, y);
            }
            const bool dark = rnd.nextFloat() < 0.7f;
            g.setColour (dark ? Colour (0xff1a0603).withAlpha (0.10f + rnd.nextFloat() * 0.22f)
                              : Colour (0xffc0603f).withAlpha (0.05f + rnd.nextFloat() * 0.10f));
            g.strokePath (vein, juce::PathStrokeType (0.4f + rnd.nextFloat() * 1.3f));
        }
        // poros
        for (int i = 0; i < 260; ++i)
        {
            const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
            g.setColour (Colour (0xff140402).withAlpha (0.18f + rnd.nextFloat() * 0.25f));
            g.fillRect (x, y, 0.7f, 1.5f + rnd.nextFloat() * 3.0f);
        }
        // verniz: faixa de brilho vertical e topo pegando luz
        const float hx = r.getX() + r.getWidth() * (left ? 0.42f : 0.38f);
        juce::ColourGradient hl (real::white (0.0f), hx - 7.0f, 0.0f, real::white (0.0f), hx + 7.0f, 0.0f, false);
        hl.addColour (0.5, real::white (0.16f));
        g.setGradientFill (hl);
        g.fillRect (r);
        g.setGradientFill (real::vertical (real::white (0.14f), r.getY(), real::white (0.0f), r.getY() + 40.0f));
        g.fillRect (r);
        g.setGradientFill (real::vertical (real::black (0.0f), r.getBottom() - 50.0f, real::black (0.35f), r.getBottom()));
        g.fillRect (r);
    }
    auto bevel = real::vertical (Colour (0xffd88a66).withAlpha (0.5f), r.getY(), real::black (0.7f), r.getBottom());
    bevel.addColour (0.05, real::white (0.06f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (r.reduced (0.6f), 9.0f, 1.2f);
}

// ---------------- serigrafia ----------------
inline void silkText (juce::Graphics& g, const juce::String& text, RF area, juce::Justification j,
                      float size = 8.5f, bool bold = true, juce::Colour c = silk, float kerning = 0.08f)
{
    g.setFont (theme::sans (size, bold, kerning));
    g.setColour (real::black (0.55f));
    g.drawText (text, area.translated (0.0f, 0.7f), j, false);
    g.setColour (c);
    g.drawText (text, area, j, false);
}

// título de seção: filete fino ao longo da seção com o nome numa tarja escura
inline void sectionTitle (juce::Graphics& g, RF r, const juce::String& text)
{
    g.setColour (real::black (0.45f));
    g.fillRect (r.getX(), r.getY() + 7.5f, r.getWidth(), 1.0f);
    g.setColour (silkDim.withAlpha (0.65f));
    g.fillRect (r.getX(), r.getY() + 6.5f, r.getWidth(), 1.0f);
    g.setFont (theme::sans (10.5f, true, 0.12f));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f;
    const RF tag (r.getX() + 8.0f, r.getY(), tw, 14.0f);
    g.setColour (juce::Colour (0xff1d1e20));
    g.fillRoundedRectangle (tag, 2.0f);
    g.setColour (real::white (0.07f));
    g.drawRoundedRectangle (tag.reduced (0.5f), 2.0f, 0.7f);
    silkText (g, text, tag, juce::Justification::centred, 10.5f, true, silk, 0.12f);
}

// quadro fino em volta de um grupo (ex.: as fileiras dos efeitos)
inline void groupFrame (juce::Graphics& g, RF r)
{
    g.setColour (real::black (0.35f));
    g.drawRoundedRectangle (r.translated (0.0f, 0.8f), 3.0f, 1.0f);
    g.setColour (silkDim.withAlpha (0.3f));
    g.drawRoundedRectangle (r, 3.0f, 0.8f);
}

// ---------------- knob preto de borracha, ponteiro pintado ----------------
inline void knob (juce::Graphics& g, RF box, float pos, juce::Colour pointer)
{
    using juce::Colour;
    const float pi = juce::MathConstants<float>::pi;
    const float d  = juce::jmin (box.getWidth(), box.getHeight());
    const float R  = d * 0.5f * 0.80f;
    const float cx = box.getCentreX(), cy = box.getCentreY();
    const float a0 = pi * 1.25f, a1 = pi * 2.75f, a = a0 + (a1 - a0) * pos;
    auto pt = [cx, cy] (float r, float an) { return PF (cx + r * std::sin (an), cy - r * std::cos (an)); };

    // escala serigrafada: 11 traços
    for (int i = 0; i <= 10; ++i)
    {
        const float an = a0 + (a1 - a0) * (float) i / 10.0f;
        const bool major = i == 0 || i == 5 || i == 10;
        juce::Path t;
        t.startNewSubPath (pt (R + 2.6f, an));
        t.lineTo (pt (R + (major ? 6.0f : 4.6f), an));
        g.setColour (silk.withAlpha (major ? 0.75f : 0.45f));
        g.strokePath (t, juce::PathStrokeType (major ? 1.2f : 0.9f));
    }

    // sombra no painel
    auto sh = real::radial ({ cx + 1.0f, cy + 2.6f }, R + 5.0f, real::black (0.75f), real::black (0.0f));
    sh.addColour (0.75, real::black (0.4f));
    g.setGradientFill (sh);
    g.fillEllipse (real::circle ({ cx + 1.0f, cy + 2.6f }, R + 5.0f));

    // saia (lateral) com o serrilhado de borracha
    g.setGradientFill (real::vertical (Colour (0xff2e2f31), cy - R, Colour (0xff050505), cy + R));
    g.fillEllipse (real::circle ({ cx, cy }, R));
    const int ribs = 40;
    for (int i = 0; i < ribs; ++i)
    {
        const float an = a + (float) i * 2.0f * pi / (float) ribs;
        const float lit = 0.5f + 0.5f * std::cos (an + 0.6f);           // luz de cima-esquerda
        juce::Path rib;
        rib.startNewSubPath (pt (R * 0.84f, an));
        rib.lineTo (pt (R - 0.3f, an));
        g.setColour (i % 2 == 0 ? real::black (0.55f) : real::white (0.03f + 0.07f * lit));
        g.strokePath (rib, juce::PathStrokeType (1.0f));
    }
    g.setColour (real::black (0.85f));
    g.drawEllipse (real::circle ({ cx, cy }, R), 0.9f);

    // topo: preto acetinado, levemente abaulado
    const float rt = R * 0.80f;
    g.setGradientFill (real::vertical (real::white (0.22f), cy - rt - 1.0f, real::black (0.6f), cy + rt + 1.0f));
    g.fillEllipse (real::circle ({ cx, cy }, rt + 1.0f));
    auto top = real::radial ({ cx - rt * 0.3f, cy - rt * 0.45f }, rt * 1.6f, Colour (0xff3a3b3e), Colour (0xff0b0b0c));
    top.addColour (0.45, Colour (0xff1e1f21));
    g.setGradientFill (top);
    g.fillEllipse (real::circle ({ cx, cy }, rt));
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path face;
        face.addEllipse (real::circle ({ cx, cy }, rt));
        g.reduceClipRegion (face);
        juce::Path sheen;
        sheen.addEllipse (-rt * 0.9f, -rt * 0.32f, rt * 1.8f, rt * 0.64f);
        sheen.applyTransform (juce::AffineTransform::rotation (-0.35f).translated (cx - rt * 0.15f, cy - rt * 0.62f));
        g.setColour (real::white (0.07f));
        g.fillPath (sheen);
    }

    // ponteiro pintado (com sombra fina)
    juce::Path pl;
    pl.startNewSubPath (pt (rt * 0.22f, a));
    pl.lineTo (pt (rt * 0.97f, a));
    g.setColour (real::black (0.7f));
    g.strokePath (pl, juce::PathStrokeType (juce::jmax (2.6f, d * 0.06f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (pointer);
    g.strokePath (pl, juce::PathStrokeType (juce::jmax (1.6f, d * 0.04f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

class VacinaLookAndFeel : public BrandLookAndFeel
{
public:
    VacinaLookAndFeel()
    {
        setColour (juce::PopupMenu::highlightedBackgroundColourId, red.withAlpha (0.3f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float, float, juce::Slider& slider) override
    {
        knob (g, juce::Rectangle<int> (x, y, width, height).toFloat(), pos,
              slider.findColour (juce::Slider::rotarySliderFillColourId));
    }
};

// ---------------- knob com o nome serigrafado embaixo (o valor aparece no visor) ----------------
class VKnob : public juce::Component
{
public:
    VKnob (APVTS& state, const juce::String& paramId, const juce::String& labelText, int knobSize,
           juce::Colour pointer = silk, const juce::String& second = {})
        : param (state.getParameter (paramId)), label (labelText), sub (second), size (knobSize)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (220);
        slider.setColour (juce::Slider::rotarySliderFillColourId, pointer);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    void resized() override
    {
        slider.setBounds (getLocalBounds().removeFromTop (size + 8).withSizeKeepingCentre (size + 8, size + 8));
    }

    void paint (juce::Graphics& g) override
    {
        const float top = (float) slider.getBottom() - 1.0f, w = (float) getWidth();
        silkText (g, label, { -6.0f, top, w + 12.0f, 11.0f }, juce::Justification::centred, 8.0f);
        if (sub.isNotEmpty())
            silkText (g, sub, { -6.0f, top + 10.0f, w + 12.0f, 10.0f }, juce::Justification::centred, 7.0f, true, redInk);
    }

private:
    juce::RangedAudioParameter* param;
    juce::String label, sub;
    int size;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- tecla de borracha cinza (com LED opcional à direita) ----------------
class Key : public juce::Button
{
public:
    Key (const juce::String& labelText, bool withLed = false, bool toggles = false)
        : juce::Button (labelText), led (withLed)
    {
        setClickingTogglesState (toggles);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto all = getLocalBounds().toFloat();
        const RF kr (all.getX() + padX + 1.0f, all.getY() + 1.0f, keyW, keyH);   // a caixa é mais larga que a tecla (cabe o nome)

        // poço no painel
        g.setColour (real::black (0.75f));
        g.fillRoundedRectangle (kr.expanded (1.4f), 3.0f);
        g.setColour (real::white (0.08f));
        g.drawRoundedRectangle (kr.expanded (1.4f).translated (0.0f, 0.7f), 3.0f, 0.7f);

        // tampa de borracha cinza
        auto cap = kr;
        if (down) cap = cap.translated (0.0f, 0.8f);
        else
        {
            g.setColour (real::black (0.6f));
            g.fillRoundedRectangle (cap.translated (0.0f, 1.6f), 2.5f);
        }
        auto body = real::vertical (juce::Colour (down ? 0xffa9aaa7 : over ? 0xffd4d5d1 : 0xffc6c7c3), cap.getY(),
                                    juce::Colour (down ? 0xff7d7e7b : 0xff8f908c), cap.getBottom());
        body.addColour (0.25, juce::Colour (down ? 0xffa2a39f : 0xffbdbeba));
        g.setGradientFill (body);
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (real::white (down ? 0.15f : 0.45f));
        g.fillRoundedRectangle (cap.getX() + 1.5f, cap.getY() + 0.8f, cap.getWidth() - 3.0f, 1.0f, 0.5f);
        g.setColour (real::black (0.35f));
        g.drawRoundedRectangle (cap.reduced (0.4f), 2.5f, 0.7f);

        if (led)
            real::led (g, { kr.getRight() + 8.0f, kr.getCentreY() }, 2.6f, red, getToggleState() ? 1.0f : 0.0f);

        if (getButtonText().isNotEmpty())
            silkText (g, getButtonText(), { kr.getCentreX() - 25.0f, kr.getBottom() + 2.5f, 50.0f, 10.0f },
                      juce::Justification::centred, 7.0f);
    }

    static constexpr float keyW = 26.0f, keyH = 13.0f, padX = 11.0f;

private:
    bool led;
};

// ---------------- lista de LEDs (a escolha atual acesa); clique no nome escolhe ----------------
class LedList : public juce::Component
{
public:
    // vertical: um embaixo do outro; senão em fileira
    LedList (juce::StringArray items, bool verticalLayout, float itemSize)
        : names (std::move (items)), vertical (verticalLayout), step (itemSize) {}

    std::function<void (int)> onSelect;

    // liga a um parâmetro de escolha (o índice aceso acompanha o parâmetro)
    void bind (APVTS& state, const juce::String& paramId)
    {
        param = state.getParameter (paramId);
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { setIndex (juce::roundToInt (v)); });
        attachment->sendInitialUpdate();
        onSelect = [this] (int i) { attachment->setValueAsCompleteGesture ((float) i); };
    }

    void setIndex (int i) { if (i != index) { index = i; repaint(); } }
    int  getIndex() const { return index; }

    RF itemRect (int i) const
    {
        return vertical ? RF (0.0f, (float) i * step, (float) getWidth(), step)
                        : RF ((float) i * step, 0.0f, step, (float) getHeight());
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < names.size(); ++i)
        {
            const auto r = itemRect (i);
            const bool on = i == index;
            if (vertical)
            {
                real::led (g, { r.getX() + 4.0f, r.getCentreY() }, 2.4f, red, on ? 1.0f : 0.0f);
                silkText (g, names[i], r.withTrimmedLeft (11.0f), juce::Justification::centredLeft, 7.0f, true,
                          on ? silk : silkDim);
            }
            else
            {
                real::led (g, { r.getCentreX(), r.getY() + 4.0f }, 2.4f, red, on ? 1.0f : 0.0f);
                silkText (g, names[i], r.withTrimmedTop (9.0f).expanded (4.0f, 0.0f), juce::Justification::centredTop, 7.0f, true,
                          on ? silk : silkDim);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < names.size(); ++i)
            if (itemRect (i).contains (e.position) && onSelect) { onSelect (i); if (param == nullptr) setIndex (i); }
    }

private:
    juce::StringArray names;
    bool vertical;
    float step;
    int index = 0;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ---------------- cristal líquido (fundo cinza iluminado, letras escuras, grade de pixels) ----------------
inline void lcdGlass (juce::Graphics& g, RF r, float corner)
{
    // moldura preta do visor
    g.setColour (real::white (0.08f));
    g.fillRoundedRectangle (r.expanded (5.0f).translated (0.0f, 0.8f), corner + 4.0f);
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillRoundedRectangle (r.expanded (5.0f), corner + 4.0f);
    g.setGradientFill (real::vertical (real::white (0.1f), r.getY() - 5.0f, real::black (0.3f), r.getBottom() + 5.0f));
    g.drawRoundedRectangle (r.expanded (5.0f), corner + 4.0f, 0.8f);

    auto bg = real::vertical (juce::Colour (0xffc3cccb), r.getY(), juce::Colour (0xffa2adad), r.getBottom());
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, corner);
    // iluminação por trás, mais forte no meio
    g.setGradientFill (real::radial (r.getCentre(), r.getWidth() * 0.6f, real::white (0.16f), real::white (0.0f)));
    g.fillRoundedRectangle (r, corner);
}

inline void lcdFinish (juce::Graphics& g, RF r, float corner)
{
    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);
    g.setColour (juce::Colour (0xff6f7b7b).withAlpha (0.10f));      // grade de pixels
    for (float x = r.getX(); x < r.getRight(); x += 1.6f) g.fillRect (x, r.getY(), 0.35f, r.getHeight());
    for (float y = r.getY(); y < r.getBottom(); y += 1.6f) g.fillRect (r.getX(), y, r.getWidth(), 0.35f);
    g.setGradientFill (real::vertical (real::black (0.35f), r.getY(), real::black (0.0f), r.getY() + 6.0f));
    g.fillRect (r.withHeight (6.0f));
    g.setGradientFill (juce::ColourGradient (real::black (0.25f), r.getX(), 0.0f, real::black (0.0f), r.getX() + 5.0f, 0.0f, false));
    g.fillRect (r.withWidth (5.0f));
    juce::Path shine;
    shine.startNewSubPath (r.getX(), r.getY());
    shine.lineTo (r.getRight(), r.getY());
    shine.lineTo (r.getRight(), r.getY() + r.getHeight() * 0.25f);
    shine.lineTo (r.getX(), r.getY() + r.getHeight() * 0.5f);
    shine.closeSubPath();
    g.setColour (real::white (0.07f));
    g.fillPath (shine);
}

inline void lcdText (juce::Graphics& g, const juce::String& t, RF area, float size, juce::Justification j)
{
    g.setFont (theme::mono (size));
    g.setColour (lcdInk.withAlpha (0.13f));                         // sombra do pixel no fundo
    g.drawText (t, area.translated (0.8f, 0.8f), j, false);
    g.setColour (lcdInk);
    g.drawText (t, area, j, false);
}

// ---------------- caixinha de cristal líquido com lista (fonte/destino da matriz, oitavas, clock) ----------------
class LcdBox : public juce::Component
{
public:
    LcdBox (APVTS& state, const juce::String& paramId)
        : param (state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
        choice = dynamic_cast<juce::AudioParameterChoice*> (param);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (3.0f, 2.5f);
        lcdGlass (g, r, 1.5f);
        lcdText (g, param->getCurrentValueAsText(), r.reduced (3.0f, 0.0f), 9.5f, juce::Justification::centred);
        lcdFinish (g, r, 1.5f);
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
        if (dragged) return;
        juce::PopupMenu menu;
        if (choice != nullptr)
            for (int i = 0; i < choice->choices.size(); ++i)
                menu.addItem (i + 1, choice->choices[i], true, choice->getIndex() == i);
        else
        {
            const auto range = param->getNormalisableRange();
            const int now = juce::roundToInt (param->convertFrom0to1 (param->getValue()));
            for (int v = (int) range.start; v <= (int) range.end; ++v)
                menu.addItem (v + 1000, juce::String (v), true, v == now);
        }
        juce::Component::SafePointer<LcdBox> safe (this);
        // lista dentro da janela do plugin (como janela solta ela pode abrir por baixo da janela do host)
        auto opts = juce::PopupMenu::Options().withTargetComponent (this);
        if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
            opts = opts.withParentComponent (editor);
        menu.showMenuAsync (opts, [safe] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            const float v = result >= 1000 ? (float) (result - 1000) : (float) (result - 1);
            safe->attachment->setValueAsCompleteGesture (v);
        });
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
    juce::RangedAudioParameter* param;
    juce::AudioParameterChoice* choice = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float startNorm = 0.0f;
    bool dragged = false;
};
} // namespace vac
