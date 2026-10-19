#pragma once

#include <array>
#include <vector>
#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- VETORSCÓPIO: Lissajous L/R girado 45° (mono = linha em pé), com rastro que esmaece ----------------
class GonioView : public juce::Component
{
public:
    explicit GonioView (HorizonteProcessor& p);
    void capture();   // no timer: pega as amostras novas e empurra o rastro
    void paint (juce::Graphics& g) override;

private:
    static constexpr int trailFrames = 8;
    static constexpr int maxPoints = 420;

    HorizonteProcessor& proc;
    std::array<std::vector<juce::Point<float>>, trailFrames> trail;
    int   trailHead = 0;
    int   lastWrite = 0;
    float range = 0.25f;   // escala: acompanha os picos (sobe rápido, desce devagar)
};

// ---------------- CORRELAÇÃO (−1..+1) e leituras da imagem ----------------
class CorrelationView : public juce::Component
{
public:
    void update (float powL, float powR, float powLR, float dt);
    void paint (juce::Graphics& g) override;

private:
    float corr = 0.0f, corrMin = 1.0f, holdLeft = 0.0f;
    float sideMidDb = -60.0f, balanceDb = 0.0f;
    bool  active = false;
};

// ---------------- LARGURA POR FREQUÊNCIA: o que os knobs fazem com o lado (S) ----------------
class WidthCurve : public juce::Component
{
public:
    explicit WidthCurve (HorizonteProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    HorizonteProcessor& proc;
};

class HorizonteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HorizonteEditor (HorizonteProcessor&);
    ~HorizonteEditor() override;

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

    HorizonteProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    GonioView gonio;
    CorrelationView correlation;
    WidthCurve curve;
    Knob widthMacro, widthLow, xover, widthHigh, monoBass, balance, rotation, output;
    KeyButton monoKey, sideKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> monoAtt, sideAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    double lastTick = 0.0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HorizonteEditor)
};
