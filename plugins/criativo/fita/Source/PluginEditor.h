#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- MÁQUINA: VUs de ponteiro, curva da fita e o transporte (TAPE STOP) ----------------
class MachineStrip : public juce::Component
{
public:
    explicit MachineStrip (FitaProcessor& p) : proc (p) {}
    void setVu (float peakL, float peakR, float dt);
    void paint (juce::Graphics& g) override;

private:
    FitaProcessor& proc;
    float needleL = 0.0f, needleR = 0.0f;   // pico com queda de VU (sobe na hora, desce em ~300 ms)
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
    void presetMenuChosen (int result);
    void askNewPreset();
    void drawCanvas (juce::Graphics&);
    void layoutCanvas();
    void setScale (float scale);

    static constexpr int openSettle = 15;   // quadros do timer (30 Hz) até a janela assentar ao abrir
    float openScale = 1.0f;                 // tamanho salvo no projeto, lido no construtor
    int   openTicks = 0;

    FitaProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1048×620 (pedal: 1000×580 de chapa + a moldura), e é ampliado junto
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

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FitaEditor)
};
