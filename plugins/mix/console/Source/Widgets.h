#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Painel.h"

using APVTS = juce::AudioProcessorValueTreeState;

// ---------------- tela em escala: o layout é desenhado em 1000×580 e ampliado inteiro ----------------
namespace ui
{
    inline constexpr int   baseW = 1000, baseH = 580;
    inline constexpr float minScale = 0.75f, maxScale = 2.0f, defaultScale = 1.25f;
}

class ScaledCanvas : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> onPaint;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }

private:
    juce::SharedResourcePointer<real::Texture> texture;   // grão do metal vivo enquanto a janela existe
};

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

    // knob limpo: sem escala serigrafada nem valor embaixo, só o botão
    void setClean (bool clean)
    {
        slider.getProperties().set ("clean", clean);
        slider.repaint();
    }

    // knob do pedal: os quadros renderizados da SÉRIE KNOB no lugar do knob Moog
    void setPedal (bool pedal)
    {
        slider.getProperties().set ("pedal", pedal);
        slider.repaint();
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

// ---------------- fader do DRY/WET ----------------
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
        g.setFont (theme::sans (9.5f, true, 0.04f));
        g.drawFittedText ("DRY/WET", 0, getHeight() - 30, getWidth(), 13, juce::Justification::centred, 1, 0.7f);
        g.setColour (theme::dim);
        g.setFont (theme::mono (11.0f));
        g.drawText (param->getCurrentValueAsText(), 0, getHeight() - 16, getWidth(), 14, juce::Justification::centred);
    }

private:
    juce::RangedAudioParameter* param;
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------- chave liga/desliga (REVERSO, CONGELAR): alavanca de metal + lâmpada piloto na cor do controle ----------------
class KeyButton : public juce::Button
{
public:
    KeyButton (const juce::String& text, juce::Colour led) : juce::Button (text), ledColour (led)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on  = getToggleState();
        const auto r   = getLocalBounds().toFloat();
        const auto col = on ? theme::cream : over ? theme::txt : theme::dim;
        g.setFont (theme::sans (8.5f, true, 0.15f));

        if (r.getWidth() >= 120.0f)
        {
            // larga: alavanca | nome | lâmpada
            const float rn = juce::jmin (6.5f, r.getHeight() * 0.2f);
            painel::alavanca (g, { r.getX() + 14.0f, r.getCentreY() + (on ? 3.0f : -3.0f) }, rn, on);
            real::engraved (g, getButtonText(), { r.getX() + 34.0f, r.getY(), r.getWidth() - 52.0f, r.getHeight() },
                            col, juce::Justification::centredLeft);
            hw::lamp (g, { r.getRight() - 12.0f, r.getCentreY() }, 4.6f, ledColour, on);
        }
        else
        {
            // estreita: alavanca deitada e lâmpada em cima, nome embaixo
            const float rw = 26.0f, rh = 12.0f, lr = 3.4f, gap = 7.0f;
            const float x0 = r.getCentreX() - (rw + gap + lr * 2.0f) * 0.5f, cy = r.getY() + 3.0f + rh * 0.5f;
            painel::alavanca (g, { x0 + rw * 0.5f + (on ? 4.0f : -4.0f), cy }, 4.2f, on, true);
            hw::lamp (g, { x0 + rw + gap + lr, cy }, lr, ledColour, on);
            const auto txt = r.withTrimmedTop (rh + 5.0f);
            g.setColour (real::black (0.6f));
            g.drawFittedText (getButtonText(), txt.translated (0.0f, 1.0f).toNearestInt(), juce::Justification::centred, 2, 0.8f);
            g.setColour (col);
            g.drawFittedText (getButtonText(), txt.toNearestInt(), juce::Justification::centred, 2, 0.8f);
        }
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
        // botão redondo de metal com o LED no meio
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const auto  c  = r.getCentre();
        const float rr = r.getWidth() * 0.5f, rc = rr - 2.5f;
        g.setColour (real::white (0.07f));
        g.fillEllipse (real::circle (c.translated (0.0f, 0.8f), rr));
        g.setColour (real::black (0.85f));
        g.fillEllipse (real::circle (c, rr));

        g.setGradientFill (real::radial (c.translated (0.0f, 2.0f), rc + 2.0f, real::black (0.6f), real::black (0.0f)));
        g.fillEllipse (real::circle (c.translated (0.0f, 2.0f), rc + 2.0f));
        g.setGradientFill (real::vertical (juce::Colour (over ? 0xff625a4e : 0xff575045), c.y - rc, juce::Colour (0xff1a1713), c.y + rc));
        g.fillEllipse (real::circle (c, rc));
        g.setGradientFill (real::vertical (real::white (0.25f), c.y - rc, real::black (0.6f), c.y + rc));
        g.drawEllipse (real::circle (c, rc - 0.5f), 1.0f);

        hw::led (g, c, 4.2f, theme::amber, getToggleState() ? 0.0f : 1.0f);
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
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        hw::keycap (g, r, 3.0f, down);

        g.setColour (down || over ? theme::amber : theme::dim);
        const auto c = r.getCentre().translated (0.0f, down ? 0.5f : 0.0f);
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
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        real::screen (g, r, 2.0f);

        auto inner = getLocalBounds().reduced (12, 0);
        g.setColour (theme::dim);
        g.setFont (theme::mono (10.5f));
        auto num = juce::String (idx + 1).paddedLeft ('0', 2) + "/" + juce::String (count).paddedLeft ('0', 2);
        g.drawText (num, inner.removeFromRight (44), juce::Justification::centredRight);
        g.setColour (theme::cream);
        g.setFont (theme::mono (12.5f));
        g.drawText (name, inner, juce::Justification::centredLeft, true);
        real::glass (g, r, 2.0f);
    }

    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String name;
    int idx = 0, count = 0;
};

// ---------------- caixas de diálogo dos presets (nome, confirmação), dentro da janela do plugin ----------------
class PresetDialog
{
public:
    ~PresetDialog() { close(); }

    // pede um nome; onOk recebe o texto sem espaços nas pontas (vazio não chama)
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

    // fecha sem confirmar (a janela do plugin fechou com a caixa aberta)
    void close()
    {
        if (current != nullptr)
            current->exitModalState (0);
    }

private:
    void open (juce::Component& parent, juce::AlertWindow* w, std::function<void (int)> done)
    {
        w->setColour (juce::AlertWindow::backgroundColourId, theme::panel2);
        w->setColour (juce::AlertWindow::textColourId, theme::txt);
        w->setColour (juce::AlertWindow::outlineColourId, theme::line);
        parent.addAndMakeVisible (w);
        w->setCentrePosition (parent.getLocalBounds().getCentre());
        current = w;
        w->enterModalState (true, juce::ModalCallbackFunction::create (std::move (done)), true);   // apaga ao fechar
    }

    juce::Component::SafePointer<juce::AlertWindow> current;
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
            real::ledBar (g, { x, y, w, h }, f > 0.85f ? theme::coral : f > 0.65f ? theme::amber : theme::cream, lit ? 1.0f : 0.0f, 0.45f);
        }
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, false, 0.1f));
        g.drawText (text, 0, getHeight() - 12, getWidth(), 12, juce::Justification::centred);
    }

private:
    juce::String text;
    float level = 0.0f;
};

// ---------------- VU de ponteiro (balística de VU: ~300 ms; 0 VU = −10 dBFS de pico: o pico fica ~10 dB acima da média) ----------------
class VuMeter : public juce::Component
{
public:
    explicit VuMeter (juce::String t) : text (std::move (t)) {}

    void setLevel (float gain)
    {
        const float alvo = painel::vuPos (juce::Decibels::gainToDecibels (gain, -60.0f) + 10.0f);
        const float novo = agulha + (alvo - agulha) * 0.22f;
        if (std::abs (novo - agulha) > 0.001f) { agulha = novo; repaint(); }
    }

    void paint (juce::Graphics& g) override { painel::vu (g, getLocalBounds().toFloat().reduced (1.0f), agulha, text); }

private:
    juce::String text;
    float agulha = 0.0f;
};

// ---------------- knobs: Moog renderizado com a escala serigrafada; no pedal, os quadros da SÉRIE KNOB ----------------
class PainelLookAndFeel : public BrandLookAndFeel
{
public:
    std::vector<std::vector<juce::Point<float>>> emblema;   // desenho em relevo na tampa do knob grandão do pedal
    juce::Colour tinta { theme::txt };                       // cor da serigrafia da escala

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float, float, juce::Slider& slider) override
    {
        const bool big   = (bool) slider.getProperties()["big"];
        const bool clean = (bool) slider.getProperties()["clean"];
        const auto box   = juce::Rectangle<int> (x, y, width, height).toFloat();
        const auto on    = slider.findColour (juce::Slider::rotarySliderFillColourId);

        if ((bool) slider.getProperties()["pedal"])
        {
            if (big)   // a base do knob fica no centro de cima da caixa: o quadro tem a sombra para baixo
            {
                const float s = juce::jmin (box.getWidth() / 500.0f, box.getHeight() / 508.0f);
                painel::knobGrande (g, { box.getCentreX(), box.getY() + 224.7f * s }, s, pos, emblema);
            }
            else
                painel::knobMini (g, box.getCentre().translated (0.0f, -4.0f), juce::jmin (box.getWidth(), box.getHeight()) / 160.0f, pos);
            return;
        }

        const float k = juce::jmin (box.getWidth(), box.getHeight()) / (big ? 92.0f : 64.0f);
        const auto  c = box.getCentre().translated (0.0f, big ? -3.0f * k : 0.0f);
        if (! clean) painel::escala (g, c, k, big, tinta);
        hw::knob (g, c, (big ? 25.0f : 19.5f) * k, pos, on);
        if (big && ! clean)
        {
            g.setColour (theme::txt);
            g.setFont (theme::mono (11.0f * k));
            g.drawText (juce::String (juce::roundToInt (slider.getValue())), juce::Rectangle<float> (box.getX(), box.getBottom() - 12.0f * k, box.getWidth(), 12.0f * k),
                        juce::Justification::centred, false);
        }
    }
};