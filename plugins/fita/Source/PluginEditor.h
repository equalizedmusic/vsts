#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- MÁQUINA: VUs de ponteiro, curva da fita e o transporte (TAPE STOP) ----------------
class MachineStrip : public juce::Component
{
public:
    explicit MachineStrip (FitaProcessor& p) : proc (p) {}
    void setVu (float rmsL, float rmsR, float dt);
    void paint (juce::Graphics& g) override;

private:
    FitaProcessor& proc;
    float needleL = 0.0f, needleR = 0.0f;   // nível com a balística do VU (300 ms)
};

// ---------------- rolos (ou cassete) girando na velocidade da fita ----------------
class ReelView : public juce::Component
{
public:
    explicit ReelView (FitaProcessor& p) : proc (p) {}
    void tick();
    void paint (juce::Graphics& g) override;

private:
    std::pair<float, float> packs (int model) const;   // raio do carretel de fita, esquerdo e direito

    FitaProcessor& proc;
    float angL = 0.0f, angR = 0.0f, capA = 0.0f;
    double lastMs = 0.0;
};

class FitaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FitaEditor (FitaProcessor&);
    ~FitaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();
    void drawCanvas (juce::Graphics&);
    void layoutCanvas();
    void setScale (float scale);

    FitaProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    MachineStrip strip;
    ReelView reels;
    Knob driveMacro, wowMacro, model, speed, bump, wear, flutter, hiss, brake, start, output;
    Fader mix;
    KeyButton stopKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> stopAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = -1.0f;
    bool lastSync = false;
    int lastModel = -1;
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FitaEditor)
};
