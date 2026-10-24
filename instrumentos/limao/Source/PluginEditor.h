#pragma once

#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "LimaoUi.h"

// ---------------- um passo do sequenciador: LED, visor da nota, tecla NOTA/PAUSA/LIGA, ACC e SLD ----------------
// arraste na vertical no visor = muda a nota (roda do mouse também); duplo clique no visor = nota ↔ pausa
class StepCell : public juce::Component
{
public:
    StepCell (LimaoProcessor& p, int stepIndex);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    void setPlaying (bool p)  { if (p != playing) { playing = p; repaint(); } }
    void setActive (bool a)   { if (a != active) { active = a; repaint(); } }

private:
    enum Zone { none, noteZone, gateZone, accZone, slideZone };
    Zone zoneAt (juce::Point<float> pt) const;
    juce::Rectangle<float> noteRect() const  { return { 4.0f, 17.0f, (float) getWidth() - 8.0f, 28.0f }; }
    juce::Rectangle<float> gateRect() const  { return { 3.0f, 49.0f, (float) getWidth() - 6.0f, 21.0f }; }
    juce::Rectangle<float> accRect() const   { return { 3.0f, 72.0f, (float) getWidth() * 0.5f - 4.0f, 22.0f }; }
    juce::Rectangle<float> slideRect() const { return { (float) getWidth() * 0.5f + 1.0f, 72.0f, (float) getWidth() * 0.5f - 4.0f, 22.0f }; }

    LimaoProcessor& proc;
    int index;
    bool playing = false, active = true;
    Zone pressed = none;
    int dragStartPitch = 0;
};

// ---------------- osciloscópio: a voz filtrada, presa no cruzamento por zero ----------------
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (LimaoProcessor& p) : proc (p) {}
    void tick();
    void paint (juce::Graphics& g) override;

private:
    static constexpr int window = 900;
    LimaoProcessor& proc;
    std::array<float, LimaoProcessor::scopeSize> ring {};
    std::vector<float> shown;
    float cutoff = 0.0f, accent = 0.0f;
};

class LimaoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LimaoEditor (LimaoProcessor&);
    ~LimaoEditor() override;

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

    void attach (juce::Button& b, const juce::String& paramId);
    void timerCallback() override;
    void drawCanvas (juce::Graphics&);
    void drawHeader (juce::Graphics&);
    void drawMeter (juce::Graphics&);
    void showPresetMenu();
    void setScale (float scale);

    static constexpr int openSettle = 15;   // quadros do timer (30 Hz) até a janela assentar ao abrir
    float openScale = 1.0f;
    int   openTicks = 0;

    LimaoProcessor& proc;
    lim::LimaoLookAndFeel lnf;
    ScaledCanvas canvas;     // tudo em 1200×740, ampliado junto
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;

    std::array<StepCell*, limao::maxSteps> cells {};
    ScopeView*  scope = nullptr;
    PresetName* presetName = nullptr;

    int   lastStep = -2, lastLen = -1, lastVersion = -1;
    float level = 0.0f, shownLevel = -1.0f;
    float lastBpm = 0.0f;
    bool  lastSync = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimaoEditor)
};
