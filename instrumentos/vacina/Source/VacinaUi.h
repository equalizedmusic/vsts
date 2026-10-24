#pragma once

// VACINA — peças próprias da tela. Theme.h e Widgets.h são os da casa (iguais em todos, não mexer).
// O que é material (chapa do painel, knob de borracha, tecla, parafuso) é imagem RENDERIZADA por
// tools/gerar-texturas.py (relevo iluminado pixel a pixel, com sombra) e embutida no plugin (PluginImg.h);
// a serigrafia, os LEDs, as laterais de madeira e os visores são desenhados aqui por cima.

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>
#include "Theme.h"
#include "Widgets.h"
#include "Params.h"
#include "PluginImg.h"

namespace vac
{
inline constexpr int   baseW = 1440, baseH = 960;
inline constexpr float minScale = 0.6f, maxScale = 1.8f;

inline const juce::Colour silk    { 0xffe4e4e0 };   // serigrafia branca
inline const juce::Colour silkDim { 0xff8d9095 };
inline const juce::Colour red     { 0xffff3b2a };   // LEDs e segunda função
inline const juce::Colour redInk  { 0xffe8503f };
inline const juce::Colour ink     { 0xffd9f1ff };   // traço dos visores (OLED)
inline const juce::Colour inkDim  { 0xff5f7d8c };

using RF = juce::Rectangle<float>;
using PF = juce::Point<float>;

// ---------------- imagens renderizadas (carregadas uma vez, compartilhadas) ----------------
struct Art
{
    juce::Image knob, key, screw, panel;
    Art()
    {
        knob  = juce::ImageCache::getFromMemory (PluginImg::knob_png, PluginImg::knob_pngSize);
        key   = juce::ImageCache::getFromMemory (PluginImg::key_png, PluginImg::key_pngSize);
        screw = juce::ImageCache::getFromMemory (PluginImg::screw_png, PluginImg::screw_pngSize);
        panel = juce::ImageCache::getFromMemory (PluginImg::panel_jpg, PluginImg::panel_jpgSize);
    }
};

// ---------------- serigrafia: tinta branca impressa, sem relevo ----------------
inline void silkText (juce::Graphics& g, const juce::String& text, RF area, juce::Justification j,
                      float size = 9.0f, bool bold = true, juce::Colour c = silk, float kerning = 0.06f)
{
    g.setFont (theme::sans (size, bold, kerning));
    g.setColour (c.withMultipliedAlpha (0.92f));
    g.drawText (text, area, j, false);
}

// moldura de seção: filete fino em volta e o nome em cima, quebrando o filete
inline void section (juce::Graphics& g, RF r, const juce::String& title)
{
    g.setFont (theme::sans (11.0f, true, 0.16f));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title) + 12.0f;
    juce::Path frame;
    const float c = 5.0f, top = r.getY() + 6.0f;
    frame.startNewSubPath (r.getX() + 14.0f + tw, top);
    frame.lineTo (r.getRight() - c, top);
    frame.quadraticTo (r.getRight(), top, r.getRight(), top + c);
    frame.lineTo (r.getRight(), r.getBottom() - c);
    frame.quadraticTo (r.getRight(), r.getBottom(), r.getRight() - c, r.getBottom());
    frame.lineTo (r.getX() + c, r.getBottom());
    frame.quadraticTo (r.getX(), r.getBottom(), r.getX(), r.getBottom() - c);
    frame.lineTo (r.getX(), top + c);
    frame.quadraticTo (r.getX(), top, r.getX() + c, top);
    frame.lineTo (r.getX() + 8.0f, top);
    g.setColour (silkDim.withAlpha (0.42f));
    g.strokePath (frame, juce::PathStrokeType (0.9f));
    silkText (g, title, { r.getX() + 8.0f, r.getY(), tw + 4.0f, 12.0f }, juce::Justification::centred, 11.0f, true, silk, 0.16f);
}

// rótulo pequeno com filete dos lados (ex.: "— FILTER 1 —" dentro de uma seção)
inline void subTitle (juce::Graphics& g, RF r, const juce::String& t, juce::Colour c = silkDim)
{
    g.setFont (theme::sans (9.5f, true, 0.12f));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 10.0f;
    const float y = r.getCentreY();
    g.setColour (silkDim.withAlpha (0.35f));
    g.fillRect (r.getX(), y, (r.getWidth() - tw) * 0.5f, 0.8f);
    g.fillRect (r.getCentreX() + tw * 0.5f, y, (r.getWidth() - tw) * 0.5f, 0.8f);
    silkText (g, t, r, juce::Justification::centred, 9.5f, true, c, 0.12f);
}

// ---------------- visor OLED: poço no painel, vidro escuro, reflexo por cima (glassOver depois do conteúdo) ----------------
inline void glassUnder (juce::Graphics& g, RF r, float corner = 3.0f)
{
    // poço: chanfro escuro em cima, luz embaixo
    g.setColour (real::white (0.07f));
    g.fillRoundedRectangle (r.expanded (3.5f).translated (0.0f, 1.0f), corner + 3.0f);
    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (r.expanded (3.5f), corner + 3.0f);
    g.setGradientFill (real::vertical (juce::Colour (0xff0b0e11), r.getY(), juce::Colour (0xff06080a), r.getBottom()));
    g.fillRoundedRectangle (r, corner);
}

inline void glassOver (juce::Graphics& g, RF r, float corner = 3.0f)
{
    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);
    g.setGradientFill (real::vertical (real::black (0.55f), r.getY(), real::black (0.0f), r.getY() + 7.0f));   // sombra da moldura
    g.fillRect (r.withHeight (7.0f));
    juce::Path shine;
    shine.startNewSubPath (r.getX(), r.getY());
    shine.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
    shine.lineTo (r.getX() + r.getWidth() * 0.38f, r.getBottom());
    shine.lineTo (r.getX(), r.getBottom());
    shine.closeSubPath();
    g.setGradientFill (juce::ColourGradient (real::white (0.045f), r.getX(), r.getY(), real::white (0.0f),
                                             r.getX() + r.getWidth() * 0.6f, r.getBottom(), false));
    g.fillPath (shine);
    g.setColour (real::white (0.05f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 0.8f);
}

// traço com brilho (o "fósforo" do visor)
inline void glowPath (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w = 1.6f)
{
    g.setColour (c.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (w * 4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.22f)); g.strokePath (p, juce::PathStrokeType (w * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);                   g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline void gridLines (juce::Graphics& g, RF r, int nx, int ny)
{
    g.setColour (inkDim.withAlpha (0.16f));
    for (int i = 1; i < nx; ++i) g.fillRect (r.getX() + r.getWidth() * (float) i / (float) nx, r.getY(), 0.6f, r.getHeight());
    for (int i = 1; i < ny; ++i) g.fillRect (r.getX(), r.getY() + r.getHeight() * (float) i / (float) ny, r.getWidth(), 0.6f);
}

inline void oledText (juce::Graphics& g, const juce::String& t, RF area, float size, juce::Justification j,
                      juce::Colour c = ink, bool monoFont = true)
{
    g.setFont (monoFont ? theme::mono (size) : theme::sans (size, true, 0.04f));
    g.setColour (c.withAlpha (0.18f));
    g.drawText (t, area.translated (0.0f, 0.0f).expanded (0.6f), j, false);
    g.setColour (c);
    g.drawText (t, area, j, false);
}

// ---------------- knob: imagem renderizada + escala serigrafada + ponteiro pintado ----------------
inline void drawKnob (juce::Graphics& g, const juce::Image& img, RF box, float pos, bool bipolar, juce::Colour pointer)
{
    const float pi = juce::MathConstants<float>::pi;
    const float d  = juce::jmin (box.getWidth(), box.getHeight()) * 0.80f;
    const float R  = d * 0.5f;
    const float cx = box.getCentreX(), cy = box.getCentreY();
    const float a0 = pi * 1.25f, a1 = pi * 2.75f, a = a0 + (a1 - a0) * pos;
    auto pt = [cx, cy] (float r, float an) { return PF (cx + r * std::sin (an), cy - r * std::cos (an)); };

    // escala: 11 traços (o do meio maior nos bipolares)
    for (int i = 0; i <= 10; ++i)
    {
        const float an = a0 + (a1 - a0) * (float) i / 10.0f;
        const bool major = i == 0 || i == 10 || (i == 5 && bipolar);
        juce::Path t;
        t.startNewSubPath (pt (R + 2.4f, an));
        t.lineTo (pt (R + (major ? 6.0f : 4.4f), an));
        g.setColour (silk.withAlpha (major ? 0.8f : 0.45f));
        g.strokePath (t, juce::PathStrokeType (major ? 1.2f : 0.9f));
    }

    // corpo renderizado (o knob ocupa 72% da imagem)
    if (img.isValid())
    {
        const float s = d / 0.72f;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, RF (cx - s * 0.5f, cy - s * 0.5f, s, s), juce::RectanglePlacement::stretchToFit);
    }

    // ponteiro pintado na tampa
    const float rt = R * 0.78f;
    juce::Path pl;
    pl.startNewSubPath (pt (rt * 0.30f, a));
    pl.lineTo (pt (rt * 0.93f, a));
    const float w = juce::jmax (1.5f, d * 0.05f);
    g.setColour (real::black (0.6f));
    g.strokePath (pl, juce::PathStrokeType (w + 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (pointer);
    g.strokePath (pl, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

class VacinaLookAndFeel : public BrandLookAndFeel
{
public:
    VacinaLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17181b));
        setColour (juce::PopupMenu::textColourId, silk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, red.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float, float, juce::Slider& slider) override
    {
        drawKnob (g, art->knob, juce::Rectangle<int> (x, y, width, height).toFloat(), pos,
                  slider.getProperties()["bipolar"], slider.findColour (juce::Slider::rotarySliderFillColourId));
    }

    juce::SharedResourcePointer<Art> art;
};

// ---------------- knob com o nome embaixo; mexendo (ou com o mouse em cima) o nome vira o valor ----------------
class VKnob : public juce::Component, private juce::Slider::Listener
{
public:
    VKnob (APVTS& state, const juce::String& paramId, const juce::String& labelText, int knobSize,
           juce::Colour pointer = silk)
        : param (state.getParameter (paramId)), label (labelText), size (knobSize)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (240);
        slider.setColour (juce::Slider::rotarySliderFillColourId, pointer);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        const auto range = param->getNormalisableRange();
        slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
        slider.addListener (this);
        slider.addMouseListener (this, false);
    }

    ~VKnob() override { slider.removeListener (this); }

    void resized() override
    {
        slider.setBounds (getLocalBounds().removeFromTop (size + 10).withSizeKeepingCentre (size + 10, size + 10));
    }

    void paint (juce::Graphics& g) override
    {
        if (label.isEmpty()) return;                          // knob sem nome (matriz): o valor aparece no visor
        const float top = (float) slider.getBottom() - 1.0f, w = (float) getWidth();
        const bool live = showValue > 0;
        silkText (g, live ? param->getCurrentValueAsText() : label, { -10.0f, top, w + 20.0f, 11.0f },
                  juce::Justification::centred, 9.5f, true, live ? redInk : silk);
    }

    void mouseEnter (const juce::MouseEvent&) override { showValue |= 1; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { showValue &= ~1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override  { showValue |= 2; repaint(); }
    void mouseUp (const juce::MouseEvent&) override    { showValue &= ~2; repaint(); }

private:
    void sliderValueChanged (juce::Slider*) override { if (showValue > 0) repaint(); }

    juce::RangedAudioParameter* param;
    juce::String label;
    int size, showValue = 0;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- tecla de borracha (imagem renderizada) com LED opcional e nome embaixo ----------------
class Key : public juce::Button
{
public:
    explicit Key (const juce::String& labelText, bool withLed = false)
        : juce::Button (labelText), led (withLed) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto all = getLocalBounds().toFloat();
        const float kx = led ? all.getX() + (all.getWidth() - 20.0f) * 0.5f : all.getCentreX();   // o LED fica à direita
        const RF kr (kx - keyW * 0.5f, all.getY() + 2.0f, keyW, keyH);
        // poço escuro no painel
        g.setColour (real::black (0.7f));
        g.fillRoundedRectangle (kr.expanded (1.6f), 3.0f);
        g.setColour (real::white (0.06f));
        g.drawRoundedRectangle (kr.expanded (1.6f).translated (0.0f, 0.8f), 3.0f, 0.7f);
        // tampa (a imagem tem a tecla em 78% × 62%, centrada um pouco acima)
        if (art->key.isValid())
        {
            const float iw = keyW / 0.78f, ih = keyH / 0.62f;
            RF dst (kr.getCentreX() - iw * 0.5f, kr.getCentreY() - ih * 0.44f, iw, ih);
            if (down) dst = dst.translated (0.0f, 0.9f);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (1.0f);
            g.drawImage (art->key, dst, juce::RectanglePlacement::stretchToFit);
            if (down || over)
            {
                g.setColour (down ? real::black (0.22f) : real::white (0.06f));
                g.fillRoundedRectangle (kr.translated (0.0f, down ? 0.9f : 0.0f).reduced (0.5f), 2.5f);
            }
        }
        if (led)
            real::led (g, { kr.getRight() + 8.0f, kr.getCentreY() }, 2.6f, red, getToggleState() ? 1.0f : 0.0f);
        if (getButtonText().isNotEmpty())
            silkText (g, getButtonText(), { kx - 40.0f, kr.getBottom() + 3.0f, 80.0f, 11.0f },
                      juce::Justification::centred, 9.0f);
    }

    static constexpr float keyW = 30.0f, keyH = 15.0f;

private:
    bool led;
    juce::SharedResourcePointer<Art> art;
};

// ---------------- lista de LEDs (a escolha atual acesa); clique no nome escolhe ----------------
class LedList : public juce::Component
{
public:
    LedList (juce::StringArray items, bool verticalLayout, float itemSize)
        : names (std::move (items)), vertical (verticalLayout), step (itemSize)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void bind (APVTS& state, const juce::String& paramId)
    {
        param = state.getParameter (paramId);
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { setIndex (juce::roundToInt (v)); });
        attachment->sendInitialUpdate();
    }

    void setIndex (int i) { if (i != index) { index = i; repaint(); } }

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
                real::led (g, { r.getX() + 4.5f, r.getCentreY() }, 2.9f, red, on ? 1.0f : 0.0f);
                silkText (g, names[i], r.withTrimmedLeft (13.0f), juce::Justification::centredLeft, 10.5f, true, on ? silk : silkDim);
            }
            else
            {
                real::led (g, { r.getCentreX(), r.getY() + 4.0f }, 2.4f, red, on ? 1.0f : 0.0f);
                silkText (g, names[i], r.withTrimmedTop (9.0f).expanded (6.0f, 0.0f), juce::Justification::centredTop, 10.5f, true,
                          on ? silk : silkDim);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < names.size(); ++i)
            if (itemRect (i).contains (e.position) && attachment != nullptr)
                attachment->setValueAsCompleteGesture ((float) i);
    }

private:
    juce::StringArray names;
    bool vertical;
    float step;
    int index = 0;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ---------------- campo de visor com lista (tabela de onda, fonte/destino da matriz, clock…) ----------------
// clique abre a lista; arrastar na vertical ou a rodinha passam de um em um
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
        const auto r = getLocalBounds().toFloat().reduced (3.5f, 3.0f);
        glassUnder (g, r, 2.0f);
        const auto txt = param->getCurrentValueAsText();
        const bool offish = txt == "OFF" || dim;
        oledText (g, txt, r.reduced (4.0f, 0.0f), 12.0f, juce::Justification::centred, offish ? inkDim : ink);
        // setinhas de "tem lista"
        g.setColour (inkDim.withAlpha (0.8f));
        juce::Path tri;
        const float x = r.getRight() - 6.0f, y = r.getCentreY();
        tri.addTriangle (x - 3.0f, y - 1.5f, x + 3.0f, y - 1.5f, x, y + 2.0f);
        g.fillPath (tri);
        glassOver (g, r, 2.0f);
    }

    void setDim (bool d) { if (d != dim) { dim = d; repaint(); } }

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
        const float norm = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / 200.0f);
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
        const float dir = w.deltaY > 0.0f ? -1.0f : w.deltaY < 0.0f ? 1.0f : 0.0f;
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
    bool dragged = false, dim = false;
};

// ---------------- visor que se redesenha sozinho quando os parâmetros que ele mostra mudam ----------------
class Scope : public juce::Component
{
public:
    std::function<void (juce::Graphics&, RF)> draw;   // desenha o conteúdo dentro do vidro
    std::function<juce::String()> caption;            // texto pequeno no canto
    std::function<float()> hash;                       // muda quando o desenho muda (o timer compara)

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (4.0f);
        glassUnder (g, r, 3.0f);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (r.toNearestInt());
            if (draw) draw (g, r.reduced (5.0f, 6.0f));
            if (caption) oledText (g, caption(), r.reduced (6.0f, 3.0f).withHeight (13.0f), 10.0f,
                                   juce::Justification::topLeft, inkDim, true);
        }
        glassOver (g, r, 3.0f);
    }

    void poll()
    {
        if (! hash) return;
        const float h = hash();
        if (h != last) { last = h; repaint(); }
    }

private:
    float last = -1.0e30f;
};
} // namespace vac
