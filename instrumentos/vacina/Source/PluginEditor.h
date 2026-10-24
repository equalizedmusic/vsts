#pragma once

#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "VacinaUi.h"

// ---------------- o visor do preset (OLED): banco e categoria, o nome grande e o último parâmetro mexido ----------------
class LcdDisplay : public juce::Component
{
public:
    std::function<void()> onClick;

    void set (const juce::String& top, const juce::String& count, const juce::String& name, const juce::String& bottom)
    {
        if (top == l1 && count == l1r && name == l2 && bottom == l3) return;
        l1 = top; l1r = count; l2 = name; l3 = bottom;
        repaint();
    }

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String l1, l1r, l2, l3;
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
    vac::VKnob* knob (int cx, int y, int size, const juce::String& pid, const juce::String& label, juce::Colour pointer = vac::silk);
    vac::Key* key (int cx, int y, const juce::String& label, bool led = false);
    vac::LcdBox* box (juce::Rectangle<int> r, const juce::String& pid, const juce::String& caption = {});
    void attach (juce::Button& b, const juce::String& paramId);
    vac::Scope* scope (juce::Rectangle<int> r);

    void buildScopes();
    void timerCallback() override;
    void drawCanvas (juce::Graphics&);
    void drawLogo (juce::Graphics&, float x, float y);
    void drawMeter (juce::Graphics&);
    void showPresetMenu();
    void presetMenuChosen (int result);
    void askNewPreset();
    void importBank();
    void setScale (float scale);
    float pv (const juce::String& pid) const { return proc.apvts.getRawParameterValue (pid)->load(); }

    static constexpr int openSettle = 15;   // quadros do timer (30 Hz) até a janela assentar ao abrir
    float openScale = 1.0f;
    int   openTicks = 0;

    VacinaProcessor& proc;
    vac::VacinaLookAndFeel lnf;
    juce::SharedResourcePointer<vac::Art> art;
    ScaledCanvas canvas;     // tudo em 1440×960, ampliado junto
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;
    std::vector<vac::Scope*> scopes;
    std::array<vac::Scope*, 3> oscScopes {}, lfoScopes {};
    vac::Scope* filterScope = nullptr;
    std::array<vac::Scope*, 2> envScopes {};
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> captions;   // nomes serigrafados embaixo dos campos
    LcdDisplay* lcd = nullptr;
    vac::LcdBox* wave1Box = nullptr;
    vac::LcdBox* wave2Box = nullptr;

    PresetDialog dialog;                           // nome do preset, confirmações
    std::unique_ptr<juce::FileChooser> chooser;   // "Importar banco de presets…"

    float levelL = 0.0f, levelR = 0.0f, shownL = -1.0f, shownR = -1.0f;
    std::array<float, 3> lfoShown { -9.0f, -9.0f, -9.0f };
    int   lastMidi = 0, midiGlow = 0, lastTouch = -1, touchTimer = 0;
    bool  midiLit = false, lastSync = false;
    float lastBpm = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VacinaEditor)
};
