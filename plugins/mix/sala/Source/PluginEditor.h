#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- RESPOSTA: seco, pré-delay, reflexões iniciais e a curva de decaimento, com a cauda de agora ----------------
class ResponseView : public juce::Component
{
public:
    explicit ResponseView (SalaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

    float levelShown = 0.0f;   // atualizado pelo editor

private:
    SalaProcessor& proc;
};

// ---------------- EQ DA CAUDA: passa-altas × passa-baixas, amortecimento e largura ----------------
class TailEqView : public juce::Component
{
public:
    explicit TailEqView (SalaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

    float levelShown = 0.0f, duckShown = 0.0f;   // atualizados pelo editor

private:
    SalaProcessor& proc;
};

class SalaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SalaEditor (SalaProcessor&);
    ~SalaEditor() override;

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

    SalaProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    ResponseView response;
    TailEqView eqView;
    Knob sizeMacro, decay, predelay, preSync, diffusion, early, lowCut, highCut, damp, width, duck, output;
    Fader mix;
    KeyButton soloKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> soloAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = 0.0f, lastPreMs = -1.0f;
    int   lastSync = -1;
    bool  lastHost = false;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SalaEditor)
};
