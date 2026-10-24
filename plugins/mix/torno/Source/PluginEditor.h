#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- HISTÓRICO: nível de entrada (creme, de baixo) e redução de ganho (coral, de cima), ~4 s ----------------
class HistoryView : public juce::Component
{
public:
    explicit HistoryView (TornoProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    TornoProcessor& proc;
    float grRange = 12.0f;   // escala da redução (dB); acompanha o máximo da tela
};

// ---------------- CURVA: entrada × saída em dB, com o joelho, a compensação e o ponto de agora ----------------
class CurveView : public juce::Component
{
public:
    explicit CurveView (TornoProcessor& p) : proc (p) {}
    void setLive (float detDb, float grDb) { liveDet = detDb; liveGr = grDb; }
    void paint (juce::Graphics& g) override;

private:
    TornoProcessor& proc;
    float liveDet = -120.0f, liveGr = 0.0f;
};

// ---------------- REDUÇÃO: agora, máximo (segura 3 s) e a compensação aplicada ----------------
class GrReadout : public juce::Component
{
public:
    void update (float grDb, float dt);
    void setMakeup (float db) { makeupDb = db; }
    float grShown() const { return grNow; }
    void paint (juce::Graphics& g) override;

private:
    float grNow = 0.0f, grMax = 0.0f, holdLeft = 0.0f, makeupDb = 0.0f;
};

class TornoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TornoEditor (TornoProcessor&);
    ~TornoEditor() override;

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

    TornoProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    HistoryView history;
    CurveView curve;
    GrReadout readout;
    Knob squeezeMacro, threshold, ratio, knee, scHpf, attack, release, makeup, output;
    Fader mix;
    KeyButton listenKey, autoRelKey, autoMakeupKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> listenAtt, autoRelAtt, autoMakeupAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f, detShown = -120.0f;
    double lastTick = 0.0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TornoEditor)
};
