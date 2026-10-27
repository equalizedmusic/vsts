#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "IspTelaAtivacao.h"

// ---------------- PASSOS: 16 colunas editáveis (VOLUME, PAN ou FILTRO), com o passo tocando aceso ----------------
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (PicoteProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    int lane = picote::laneVol;   // faixa na tela (é a que o EMBARALHAR sorteia)

private:
    juce::Rectangle<float> tabRect (int i) const;
    void setFromMouse (juce::Point<float> p);

    PicoteProcessor& proc;
};

// ---------------- FATIA: envelope de um passo e a roda do padrão ----------------
class SliceView : public juce::Component
{
public:
    explicit SliceView (PicoteProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    PicoteProcessor& proc;
};

class PicoteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PicoteEditor (PicoteProcessor&);
    ~PicoteEditor() override;

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

    PicoteProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1048×620 (pedal: 1000×580 de chapa + a moldura), e é ampliado junto
    StepGrid grid;
    SliceView slice;
    Knob depthMacro, division, length, swing, offset, widthMacro, attack, release, filtType, reso, output;
    Fader mix;
    KeyButton shuffleKey, holdKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> holdAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f;
    float lastBpm = 0.0f;
    bool  lastSync = false, lastWaiting = false;
    int   shuffleFlash = 0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir
    isp::TelaAtivacao ativacao;   // por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PicoteEditor)
};
