#pragma once

#include <vector>
#include "PluginProcessor.h"
#include "Widgets.h"
#include "IspTelaAtivacao.h"

// ---------------- ESPECTRO: entrada, saída e o que foi tirado, com a faixa de detecção arrastável ----------------
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (LixaProcessor& p);
    void refresh();   // lê o anel do motor, roda a FFT e redesenha
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    enum class Grab { none, lo, hi, band };

    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    Grab grabAt (float x) const;
    void setFreq (const char* pid, float hz);
    void gesture (bool begin);
    void analyse (const std::array<std::atomic<float>, lixa::specRing>& ring, int wpos, std::vector<float>& dbOut);

    LixaProcessor& proc;
    juce::dsp::FFT fft { lixa::fftOrder };
    std::vector<float> window, fftData, inDb, outDb;
    Grab hover = Grab::none, grab = Grab::none;
    float grabLo = 0.0f, grabHi = 0.0f, grabF = 0.0f;
};

// ---------------- REDUÇÃO: agora, máximo, nível da faixa contra o limiar e o histórico de ~4 s ----------------
class ReductionView : public juce::Component
{
public:
    explicit ReductionView (LixaProcessor& p) : proc (p) {}
    void update (float grDb, float levelDb, float dt);
    void paint (juce::Graphics& g) override;

private:
    LixaProcessor& proc;
    float grNow = 0.0f, grMax = 0.0f, holdLeft = 0.0f, lvlNow = -120.0f;
};

// ---------------- picos de entrada e saída em dBFS (números ao lado dos LEDs) ----------------
class PeakReadout : public juce::Component
{
public:
    void set (float in, float out);
    void paint (juce::Graphics& g) override;

private:
    float pkIn = 0.0f, pkOut = 0.0f;
};

class LixaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LixaEditor (LixaProcessor&);
    ~LixaEditor() override;

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

    LixaProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1064×580 (rack: 1000 de frente + as orelhas), e é ampliado junto
    SpectrumView spectrum;
    ReductionView reduction;
    PeakReadout readout;
    Knob rangeMacro, loKnob, hiKnob, threshKnob, linkKnob, attackKnob, releaseKnob, output;
    KeyButton wideKey, deltaKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    VuMeter meterIn { "IN" }, meterOut { "OUT" };
    std::unique_ptr<APVTS::ButtonAttachment> wideAtt, deltaAtt, bypassAtt;
    float levelIn = 0.0f, levelOut = 0.0f, holdIn = 0.0f, holdOut = 0.0f;
    double lastTick = 0.0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir
    isp::TelaAtivacao ativacao;   // por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LixaEditor)
};
