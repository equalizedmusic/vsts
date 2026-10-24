#pragma once

#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "VacinaUi.h"

// ---------------- o visor de cristal líquido do centro: preset, grupo e o último parâmetro mexido ----------------
class LcdDisplay : public juce::Component
{
public:
    std::function<void()> onClick;

    void set (const juce::String& top, const juce::String& name, const juce::String& bottom)
    {
        if (top == l1 && name == l2 && bottom == l3) return;
        l1 = top; l2 = name; l3 = bottom;
        repaint();
    }

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String l1, l2, l3;
};

class VacinaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VacinaEditor (VacinaProcessor&);
    ~VacinaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    template <typename T, typename... Args>
    T* add (juce::Rectangle<int> bounds, Args&&... args)
    {
        auto owner = std::make_unique<T> (std::forward<Args> (args)...);
        T* c = owner.get();
        canvas.addAndMakeVisible (*c);
        c->setBounds (bounds);
        owned.push_back (std::move (owner));
        return c;
    }

    // knob com o centro em cx (a caixa tem a largura do nome)
    vac::VKnob* knob (int cx, int y, int size, const juce::String& pid, const juce::String& label,
                      juce::Colour pointer = vac::silk, const juce::String& sub = {});
    vac::Key* key (int x, int y, const juce::String& label, bool led = false);
    void attach (juce::Button& b, const juce::String& paramId);
    void cycle (const juce::String& paramId);

    // páginas: o mesmo lugar do painel mostra os controles do OSC 1 ou do OSC 2, do LFO 1 ou 2…
    struct Pages
    {
        std::vector<std::vector<juce::Component*>> items;
        vac::LedList* leds = nullptr;
        int current = 0;
    };
    void showPage (Pages& p, int index);
    void nextPage (Pages& p) { showPage (p, (p.current + 1) % (int) p.items.size()); }

    void timerCallback() override;
    void drawCanvas (juce::Graphics&);
    void drawLogo (juce::Graphics&, float x, float y, float scale);
    void drawMeter (juce::Graphics&);
    void showPresetMenu();
    void presetMenuChosen (int result);
    void askNewPreset();
    void importBank();
    void setScale (float scale);

    static constexpr int openSettle = 15;   // quadros do timer (30 Hz) até a janela assentar ao abrir
    float openScale = 1.0f;
    int   openTicks = 0;

    VacinaProcessor& proc;
    vac::VacinaLookAndFeel lnf;
    ScaledCanvas canvas;     // tudo em 1360×600, ampliado junto
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;

    Pages oscPages, lfoPages, filtPages, fxUpPages, fxLoPages;
    LcdDisplay* lcd = nullptr;

    PresetDialog dialog;                           // nome do preset, confirmações
    std::unique_ptr<juce::FileChooser> chooser;   // "Importar banco de presets…"

    float levelL = 0.0f, levelR = 0.0f, shownL = -1.0f, shownR = -1.0f;
    float lfoShown = -9.0f;
    int   lastMidi = 0, midiGlow = 0, lastTouch = -1, touchTimer = 0;
    bool  midiLit = false, lastSync = false;
    float lastBpm = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VacinaEditor)
};
