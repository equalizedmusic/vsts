#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- MESA: o sinal que entra no canal (creme) e a cor que o canal acrescenta (coral), ao vivo ----------------
class ColorStrip : public juce::Component
{
public:
    explicit ColorStrip (ConsoleProcessor& p) : proc (p) {}
    void setPeaks (float in, float out) { peakIn = in; peakOut = out; }
    void paint (juce::Graphics& g) override;

private:
    ConsoleProcessor& proc;
    float peakIn = 0.0f, peakOut = 0.0f;
};

// ---------------- CURVA: transferência do modelo, com o ponto do pico de agora e a descrição ----------------
class CurveView : public juce::Component
{
public:
    explicit CurveView (ConsoleProcessor& p) : proc (p) {}
    void setLive (float drivenPeak) { live = drivenPeak; }
    void paint (juce::Graphics& g) override;

private:
    ConsoleProcessor& proc;
    float live = 0.0f;
};

// ---------------- HARMÔNICOS: 2º a 5º gerados no nível de agora, pares × ímpares, resposta do EQ ----------------
class HarmonicsView : public juce::Component
{
public:
    explicit HarmonicsView (ConsoleProcessor& p) : proc (p) {}
    void update (float drivenPeak, float dt);
    float levelShown() const { return level; }
    void paint (juce::Graphics& g) override;

private:
    ConsoleProcessor& proc;
    float level = 0.0f;                    // pico do sinal na curva (cai devagar)
    float harm[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };   // amplitude do 1º ao 5º harmônico (índice = ordem)
};

// ---------------- uma linha de texto (compensação automática / volume do COMPARAR) ----------------
class ReadoutLine : public juce::Component
{
public:
    void set (const juce::String& label, const juce::String& value, juce::Colour colour)
    {
        if (label == lbl && value == val && colour == col) return;
        lbl = label; val = value; col = colour;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.15f));
        g.drawText (lbl, getLocalBounds(), juce::Justification::centredLeft);
        g.setColour (col);
        g.setFont (theme::mono (10.5f));
        g.drawText (val, getLocalBounds(), juce::Justification::centredRight);
    }

private:
    juce::String lbl, val;
    juce::Colour col { theme::dim };
};

class ConsoleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ConsoleEditor (ConsoleProcessor&);
    ~ConsoleEditor() override;

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

    ConsoleProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1064×580 (rack: 1000 de frente + as orelhas), e é ampliado junto
    ColorStrip strip;
    CurveView curve;
    HarmonicsView harmonics;
    Knob driveMacro, model, tone, crosstalk, noise, output;
    Fader mix;
    KeyButton compareKey, autoKey, osKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    VuMeter meterIn { "IN" }, meterOut { "OUT" };
    ReadoutLine readout;
    std::unique_ptr<APVTS::ButtonAttachment> compareAtt, autoAtt, osAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f, holdIn = 0.0f, holdOut = 0.0f;
    double lastTick = 0.0;
    bool lastOs4 = true;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConsoleEditor)
};
