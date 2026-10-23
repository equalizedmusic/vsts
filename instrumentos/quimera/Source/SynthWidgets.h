#pragma once

// QUIMERA — componentes próprios da tela do synth.
// Theme.h e Widgets.h são os da casa (iguais em todos os plugins, não mexer); o que é só do synth fica aqui.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <functional>
#include <memory>
#include "Theme.h"
#include "Widgets.h"
#include "Engine.h"

// ---------------- tela do synth: desenhada em 1200×740 e ampliada inteira (75–200%) ----------------
namespace qui
{
inline constexpr int   baseW = 1200, baseH = 740;
inline constexpr float minScale = 0.75f, maxScale = 2.0f, defaultScale = 1.0f;

// cores próprias do synth (o resto vem do theme da casa)
inline const juce::Colour steel { 0xffa9b6c2 };   // metal: ruído, voz, controle/MPE, compressor
inline const juce::Colour sand  { 0xffd8b27c };   // madeira: sub
inline const juce::Colour jade  { 0xff55d0bf };   // macros, reverb

// cor de cada oscilador (knob = desenho = filete do seletor)
inline juce::Colour oscColour (int o)
{
    return o == 0 ? theme::amber : o == 1 ? theme::ice : theme::violet;
}

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// os nomes das listas no Params.h são ASCII; na tela vão com acento
inline juce::String accentText (const juce::String& raw)
{
    if (raw == "-")
        return quimera::u8 ("—");

    static const char* const pairs[][2] = {
        { "Posicao", "Posição" },     { "Ruido", "Ruído" },         { "Unissono", "Uníssono" },
        { "Pressao", "Pressão" },     { "Aleatorio", "Aleatório" }, { "Triangulo", "Triângulo" },
        { "Valvula", "Válvula" },     { "Assimetrico", "Assimétrico" }
    };
    juce::String s (raw);
    for (const auto& p : pairs)
        s = s.replace (p[0], quimera::u8 (p[1]));
    return s;
}
} // namespace qui

// ---------------- knob pequeno: o mesmo anel segmentado da casa, em tamanho de synth ----------------
// (o BrandLookAndFeel escala o desenho pelo tamanho; nome e valor ficam embaixo)
class MiniKnob : public juce::Component
{
public:
    MiniKnob (APVTS& state, const juce::String& paramId, const juce::String& labelText, juce::Colour onColour, int knobSize = 34)
        : param (state.getParameter (paramId)), label (labelText), knobPx (knobSize)
    {
        jassert (param != nullptr);
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (200);
        slider.getProperties().set ("big", false);
        slider.setColour (juce::Slider::rotarySliderFillColourId, onColour);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    void resized() override
    {
        slider.setBounds (getLocalBounds().removeFromTop (knobPx).withSizeKeepingCentre (knobPx, knobPx));
    }

    void paint (juce::Graphics& g) override
    {
        const bool big = knobPx >= 44;
        const int  w = getWidth();
        g.setColour (theme::txt);
        g.setFont (theme::sans (big ? 9.5f : 8.5f, true, 0.12f));
        g.drawFittedText (label, juce::Rectangle<int> (0, knobPx, w, 11), juce::Justification::centred, 1, 0.7f);
        g.setColour (theme::dim);
        g.setFont (theme::mono (big ? 10.5f : 9.5f));
        g.drawFittedText (param->getCurrentValueAsText(), juce::Rectangle<int> (0, knobPx + 11, w, 12),
                          juce::Justification::centred, 1, 0.7f);
    }

private:
    juce::RangedAudioParameter* param;
    juce::String label;
    int knobPx;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- seletor de lista: ◂ valor ▸ (as setas andam; o meio abre o menu, agrupado se tiver grupos) ----------------
class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (APVTS& state, const juce::String& paramId, juce::Colour accentColour, const juce::String& captionText = {})
        : accent (accentColour), caption (captionText)
    {
        auto* p = state.getParameter (paramId);
        jassert (p != nullptr);
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            items = c->choices;
        setRepaintsOnMouseActivity (true);

        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
        {
            const int i = juce::roundToInt (v);
            if (i != current) { current = i; repaint(); }
        }, nullptr);
        attachment->sendInitialUpdate();
    }

    std::function<juce::String (int)> labelFor;   // texto do item (vazio = nome da lista com acento)
    std::function<juce::String (int)> groupFor;   // grupo do item: vira cabeçalho no menu
    bool groupInBox = false;                      // mostra o grupo em miúdo antes do nome

    int getIndex() const { return current; }

    juce::String textFor (int i) const
    {
        if (labelFor) return labelFor (i);
        return juce::isPositiveAndBelow (i, items.size()) ? qui::accentText (items[i]) : juce::String();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);   // o rebaixo do screen sai 1 px para fora
        const bool over = isMouseOver();
        real::screen (g, r, 3.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (r.getX() + 1.5f, r.getY() + 3.0f, 2.0f, r.getHeight() - 6.0f, 1.0f);   // filete na cor do bloco

        // setas
        const auto mouse = getMouseXYRelative();
        const float cy = r.getCentreY();
        const bool  onLeft = over && mouse.x < arrowW, onRight = over && mouse.x > getWidth() - arrowW;
        juce::Path left, right;
        left.addTriangle (7.0f, cy, 11.0f, cy - 3.5f, 11.0f, cy + 3.5f);
        const float rx = (float) getWidth() - 7.0f;
        right.addTriangle (rx, cy, rx - 4.0f, cy - 3.5f, rx - 4.0f, cy + 3.5f);
        g.setColour (onLeft ? accent : theme::dim);
        g.fillPath (left);
        g.setColour (onRight ? accent : theme::dim);
        g.fillPath (right);

        auto text = getLocalBounds().reduced (arrowW + 1, 0);
        if (caption.isNotEmpty())
        {
            const auto cf = theme::sans (8.0f, true, 0.15f);
            g.setColour (theme::dim);
            g.setFont (cf);
            const int cw = juce::roundToInt (qui::textWidth (cf, caption)) + 6;
            g.drawText (caption, text.removeFromLeft (cw), juce::Justification::centredLeft, false);
        }
        if (groupInBox && groupFor)
        {
            const auto gname = groupFor (current);
            const auto gf = theme::sans (7.5f, true, 0.15f);
            g.setColour (theme::dim);
            g.setFont (gf);
            const int gw = juce::roundToInt (qui::textWidth (gf, gname)) + 6;
            g.drawText (gname, text.removeFromLeft (gw), juce::Justification::centredLeft, false);
        }
        g.setColour (theme::cream);
        g.setFont (theme::mono (10.0f));
        g.drawFittedText (textFor (current), text,
                          caption.isNotEmpty() || groupInBox ? juce::Justification::centredLeft : juce::Justification::centred,
                          1, 0.75f);

        real::glass (g, r, 3.0f);
        if (over)
        {
            g.setColour (theme::dim);
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = items.size();
        if (n <= 0) return;
        if (e.position.x < (float) arrowW)                      choose ((current - 1 + n) % n);
        else if (e.position.x > (float) (getWidth() - arrowW))  choose ((current + 1) % n);
        else                                                    showMenu();
    }

private:
    void showMenu()
    {
        juce::PopupMenu m;
        juce::String last;
        for (int i = 0; i < items.size(); ++i)
        {
            if (groupFor)
            {
                const auto gname = groupFor (i);
                if (gname.isNotEmpty() && gname != last)
                    m.addSectionHeader (gname);
                last = gname;
            }
            m.addItem (i + 1, textFor (i), true, i == current);
        }

        juce::Component::SafePointer<ChoiceBox> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                         [safe] (int result)
                         {
                             if (safe != nullptr && result > 0)
                                 safe->choose (result - 1);
                         });
    }

    void choose (int i)
    {
        if (attachment != nullptr && juce::isPositiveAndBelow (i, items.size()))
            attachment->setValueAsCompleteGesture ((float) i);
    }

    static constexpr int arrowW = 15;
    juce::Colour accent;
    juce::String caption;
    juce::StringArray items;
    int current = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ---------------- LED liga/desliga de bloco (pode ter um nome do lado) ----------------
class LedToggle : public juce::Button
{
public:
    explicit LedToggle (juce::Colour c, const juce::String& text = {}) : juce::Button (text), ledColour (c)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool  on = getToggleState();
        const bool  hasText = getButtonText().isNotEmpty();
        const float h = (float) getHeight();
        const float d = juce::jmin (10.0f, h - 4.0f);
        const float x = hasText ? 3.0f : ((float) getWidth() - d) * 0.5f;
        const juce::Rectangle<float> led (x, (h - d) * 0.5f, d, d);
        const auto c = led.getCentre();

        {
            // o halo do LED é maior que o botão: recorta num círculo para não aparecer a quina do componente
            juce::Graphics::ScopedSaveState save (g);
            const float halo = juce::jmin (c.x, c.y, h - c.y);
            juce::Path clip;
            clip.addEllipse (real::circle (c, halo));
            g.reduceClipRegion (clip);
            real::led (g, c, d * 0.5f, ledColour, on ? 1.0f : 0.0f);
        }
        if (over)
        {
            g.setColour (theme::dim);
            g.drawEllipse (led.expanded (1.5f), 1.0f);
        }

        if (hasText)
        {
            g.setColour (on ? theme::cream : theme::dim);
            g.setFont (theme::sans (8.5f, true, 0.15f));
            g.drawText (getButtonText(), getLocalBounds().withTrimmedLeft (juce::roundToInt (x + d + 5.0f)),
                        juce::Justification::centredLeft, false);
        }
    }

private:
    juce::Colour ledColour;
};

// ---------------- chave de duas posições (desligado = esquerda, ligado = direita) ----------------
class SwitchButton : public juce::Button
{
public:
    SwitchButton (const juce::String& offText, const juce::String& onText, juce::Colour c)
        : juce::Button ("switch"), offLabel (offText), onLabel (onText), colour (c)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);   // o rebaixo do screen sai 1 px para fora
        real::screen (g, r, 3.0f);

        const bool on = getToggleState();
        const auto left  = r.withWidth (r.getWidth() * 0.5f);
        const auto right = left.withX (left.getRight());
        g.setColour (colour.withAlpha (0.22f));
        g.fillRoundedRectangle ((on ? right : left).reduced (2.0f), 2.0f);
        g.setColour (colour);
        g.fillRect ((on ? right : left).reduced (6.0f, 0.0f).removeFromBottom (2.0f).translated (0.0f, -2.0f));

        g.setFont (theme::sans (8.0f, true, 0.15f));
        g.setColour (on ? theme::dim : theme::cream);
        g.drawText (offLabel, left, juce::Justification::centred, false);
        g.setColour (on ? theme::cream : theme::dim);
        g.drawText (onLabel, right, juce::Justification::centred, false);

        real::glass (g, r, 3.0f);
        if (over)
        {
            g.setColour (theme::dim);
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
        }
    }

private:
    juce::String offLabel, onLabel;
    juce::Colour colour;
};

// ---------------- abas da tela ----------------
class TabBar : public juce::Component
{
public:
    explicit TabBar (const juce::StringArray& tabNames) : names (tabNames) { setRepaintsOnMouseActivity (true); }

    std::function<void (int)> onChange;

    int getCurrent() const { return current; }

    void setCurrent (int i)
    {
        i = juce::jlimit (0, juce::jmax (0, names.size() - 1), i);
        if (i == current) return;
        current = i;
        repaint();
        if (onChange) onChange (current);
    }

    void paint (juce::Graphics& g) override
    {
        const int n = juce::jmax (1, names.size());
        const float w = (float) getWidth() / (float) n, h = (float) getHeight();
        const int hover = isMouseOver() ? indexAt ((float) getMouseXYRelative().x) : -1;

        const auto sr = getLocalBounds().toFloat().reduced (1.5f);   // o rebaixo do screen sai 1 px para fora
        real::screen (g, sr, 3.0f);
        for (int i = 0; i < names.size(); ++i)
        {
            const juce::Rectangle<float> r ((float) i * w, 0.0f, w, h);
            if (i == current)
            {
                g.setColour (theme::panel2);
                g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
                g.setColour (theme::amber);
                g.fillRect (r.getX() + 10.0f, h - 2.5f, w - 20.0f, 2.0f);
            }
            else if (i > 0 && i != current + 1)
            {
                g.setColour (theme::line);
                g.fillRect (r.getX(), 5.0f, 1.0f, h - 10.0f);
            }
            g.setColour (i == current ? theme::cream : i == hover ? theme::txt : theme::dim);
            g.setFont (theme::sans (9.5f, true, 0.25f));
            g.drawText (names[i], r, juce::Justification::centred, false);
        }
        real::glass (g, sr, 3.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (names.isEmpty()) return;
        setCurrent (indexAt (e.position.x));
    }

private:
    int indexAt (float x) const
    {
        const int n = juce::jmax (1, names.size());
        return juce::jlimit (0, n - 1, (int) (x * (float) n / (float) juce::jmax (1, getWidth())));
    }

    juce::StringArray names;
    int current = 0;
};

// ---------------- quantidade bipolar da matriz (−100..+100), com o ponto de onde a modulação está agora ----------------
class AmountBar : public juce::Slider
{
public:
    AmountBar (APVTS& state, const juce::String& paramId)
    {
        setSliderStyle (juce::Slider::LinearHorizontal);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setSliderSnapsToMousePosition (false);   // arrastar é relativo; clicar não pula
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, *this);
        setDoubleClickReturnValue (true, 0.0);
    }

    // live: fonte × quantidade agora (−1..1); colour: cor da fonte; used: o slot tem fonte e destino
    void setLive (float liveValue, juce::Colour colour, bool isUsed)
    {
        if (std::abs (liveValue - live) > 0.004f || colour != barColour || isUsed != used)
        {
            live = liveValue;
            barColour = colour;
            used = isUsed;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);   // o rebaixo do screen sai 1 px para fora
        real::screen (g, r, 3.0f);

        const auto  range = getRange();
        const float span = (float) juce::jmax (std::abs (range.getStart()), std::abs (range.getEnd()));
        const float v = (float) getValue();
        const float f = span > 0.0f ? juce::jlimit (-1.0f, 1.0f, v / span) : 0.0f;
        const float cx = r.getCentreX(), half = r.getWidth() * 0.5f - 4.0f;
        const float x = cx + f * half;

        g.setColour (theme::line);
        for (int k = -4; k <= 4; ++k)
            g.fillRect (cx + (float) k * half / 4.0f - 0.5f, r.getBottom() - 4.0f, 1.0f, 3.0f);

        const juce::Rectangle<float> bar (juce::jmin (cx, x), r.getY() + 4.0f, std::abs (x - cx), r.getHeight() - 9.0f);
        g.setColour (barColour.withAlpha (used ? 0.7f : 0.25f));
        g.fillRect (bar);
        g.setColour (theme::dim);
        g.fillRect (cx - 0.5f, r.getY() + 2.0f, 1.0f, r.getHeight() - 4.0f);

        // onde a modulação está agora: brilha conforme a fonte mexe
        if (used && std::abs (live) > 0.002f)
        {
            const float lx = cx + juce::jlimit (-1.0f, 1.0f, live) * half;
            const float a  = juce::jlimit (0.0f, 1.0f, 0.35f + std::abs (live));
            g.setColour (barColour.withAlpha (0.25f * a));
            g.fillRect (juce::jmin (cx, lx), r.getY() + 1.0f, std::abs (lx - cx), r.getHeight() - 2.0f);
            g.setColour (barColour.brighter (0.4f).withAlpha (a));
            g.fillRoundedRectangle (lx - 1.5f, r.getY() + 1.0f, 3.0f, r.getHeight() - 2.0f, 1.0f);
        }

        g.setColour (used ? theme::cream : theme::dim);
        g.setFont (theme::mono (10.0f));
        g.drawText (getTextFromValue (getValue()), r.reduced (7.0f, 0.0f),
                    f >= 0.0f ? juce::Justification::centredLeft : juce::Justification::centredRight, false);

        real::glass (g, r, 3.0f);
        if (isMouseOverOrDragging())
        {
            g.setColour (theme::dim);
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
        }
    }

private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    float live = 0.0f;
    juce::Colour barColour { theme::dim };
    bool used = false;
};

// ---------------- texto que muda (tempo do delay etc.) ----------------
class Readout : public juce::Component
{
public:
    Readout (juce::Colour c, juce::Justification j) : colour (c), just (j) { setInterceptsMouseClicks (false, false); }

    void set (const juce::String& t)
    {
        if (t != text) { text = t; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colour);
        g.setFont (theme::mono (10.5f));
        g.drawText (text, getLocalBounds(), just, true);
    }

private:
    juce::Colour colour;
    juce::Justification just;
    juce::String text;
};

// ---------------- redução do compressor (0 a 24 dB) ----------------
class GrMeter : public juce::Component
{
public:
    explicit GrMeter (juce::Colour c) : colour (c) { setInterceptsMouseClicks (false, false); }

    void setReduction (float db)
    {
        db = juce::jmax (0.0f, db);
        shown = db > shown ? db : shown + (db - shown) * 0.2f;   // sobe na hora, desce macio
        if (std::abs (shown - painted) > 0.05f) { painted = shown; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.18f));
        g.drawText ("GAIN REDUCTION", juce::Rectangle<float> (0.0f, 0.0f, W, 12.0f), juce::Justification::centredLeft);
        g.setColour (painted > 0.05f ? theme::cream : theme::dim);
        g.setFont (theme::mono (11.0f));
        const juce::String t = painted > 0.05f ? quimera::u8 ("−") + quimera::num1 (painted) + " dB" : juce::String ("0,0 dB");
        g.drawText (t, juce::Rectangle<float> (0.0f, 14.0f, W, 13.0f), juce::Justification::centredLeft);

        const juce::Rectangle<float> bar (0.0f, 32.0f, W, 7.0f);
        g.setColour (theme::segOff);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (bar.withWidth (W * juce::jlimit (0.0f, 1.0f, painted / 24.0f)), 2.0f);
        g.setColour (theme::panel);
        for (int k = 1; k < 4; ++k)
            g.fillRect (W * (float) k / 4.0f, bar.getY(), 1.0f, bar.getHeight());
        g.setColour (theme::dim);
        g.setFont (theme::mono (8.5f));
        g.drawText ("0", juce::Rectangle<float> (0.0f, 42.0f, 20.0f, 10.0f), juce::Justification::centredLeft);
        g.drawText ("12", juce::Rectangle<float> (W * 0.5f - 10.0f, 42.0f, 20.0f, 10.0f), juce::Justification::centred);
        g.drawText ("24", juce::Rectangle<float> (W - 20.0f, 42.0f, 20.0f, 10.0f), juce::Justification::centredRight);
    }

private:
    juce::Colour colour;
    float shown = 0.0f, painted = -1.0f;
};

// ---------------- teclado da casa: teclas creme/carvão; cada voz acende a tecla com a pressão,
//                  e o ponto anda com o bend (para os lados) e o slide (para cima) ----------------
class StyledKeyboard : public juce::MidiKeyboardComponent
{
public:
    StyledKeyboard (juce::MidiKeyboardState& state, const quimera::Telemetry& telemetry)
        : juce::MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard), tel (telemetry)
    {
        setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe6dcc8));
        setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1c1915));
        setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6d665a));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, theme::amber.withAlpha (0.18f));
        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, theme::amber.withAlpha (0.55f));
        setColour (juce::MidiKeyboardComponent::textLabelColourId, theme::dim);
        setScrollButtonsVisible (false);
        setAvailableRange (24, 96);   // C1..C7
        setLowestVisibleKey (24);
        setBlackNoteLengthProportion (0.62f);
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
    }

    // timer da tela: redesenha enquanto alguma voz soa (e uma vez quando a última some)
    void tick()
    {
        bool any = false;
        for (size_t i = 0; i < tel.voice.size(); ++i)
            any = any || tel.voice[i].note.load() >= 0;
        if (any || wasActive) repaint();
        wasActive = any;
    }

    void paint (juce::Graphics& g) override
    {
        juce::MidiKeyboardComponent::paint (g);

        const float semitone = getKeyWidth() * 7.0f / 12.0f;
        for (size_t i = 0; i < tel.voice.size(); ++i)
        {
            const auto& v = tel.voice[i];
            const int n = v.note.load();
            if (n < getRangeStart() || n > getRangeEnd()) continue;

            const auto  key = getRectangleForKey (n);
            const float lv = juce::jlimit (0.0f, 1.0f, v.level.load());
            const float pr = juce::jlimit (0.0f, 1.0f, v.pressure.load());
            const float sl = juce::jlimit (0.0f, 1.0f, v.slide.load());
            const float bd = juce::jlimit (-48.0f, 48.0f, v.bend.load());

            // brilho que sobe do pé da tecla: mais alto com a pressão, mais forte com o volume da voz
            const float h = key.getHeight() * (0.2f + 0.7f * pr);
            const juce::Rectangle<float> glow (key.getX() + 1.0f, key.getBottom() - h, key.getWidth() - 2.0f, h);
            g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (0.0f), 0.0f, glow.getY(),
                                                     theme::amber.withAlpha (0.25f + 0.55f * lv), 0.0f, glow.getBottom(), false));
            g.fillRect (glow);

            const float cx = key.getCentreX() + bd * semitone;
            const float cy = key.getY() + key.getHeight() * (0.86f - 0.6f * sl);
            const float rr = 2.5f + 2.5f * pr;
            g.setColour (theme::coral.withAlpha (0.3f));
            g.fillEllipse (cx - rr - 3.0f, cy - rr - 3.0f, (rr + 3.0f) * 2.0f, (rr + 3.0f) * 2.0f);
            g.setColour (theme::coral);
            g.fillEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
        }
    }

private:
    const quimera::Telemetry& tel;
    bool wasActive = true;
};
