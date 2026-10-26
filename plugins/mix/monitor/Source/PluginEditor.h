#pragma once

#include "PluginProcessor.h"
#include "Analyzer.h"
#include "Widgets.h"

// leituras que a interface mostra (copiadas do motor a cada quadro; CONGELAR para de copiar)
struct Readings
{
    float momentary = medidor::silence, shortTerm = medidor::silence, integrated = medidor::silence;
    float shortMax = medidor::silence;
    float lra = 0.0f, lraLo = medidor::silence, lraHi = medidor::silence;
    float truePeakMax = 0.0f;   // ganho linear
    float seconds = 0.0f;
};

// ---------------- LOUDNESS: integrado grande, momentâneo, curto, LRA, true peak máximo ----------------
class LoudnessView : public juce::Component
{
public:
    explicit LoudnessView (MonitorProcessor& p) : proc (p) {}
    void setReadings (const Readings& r) { rd = r; }
    void paint (juce::Graphics& g) override;

private:
    MonitorProcessor& proc;
    Readings rd;
};

// ---------------- HISTÓRICO: LUFS curto (e momentâneo) dos últimos 60 s, com o alvo e a faixa ----------------
class HistoryView : public juce::Component
{
public:
    explicit HistoryView (MonitorProcessor& p);
    void update();
    void setReadings (const Readings& r) { rd = r; }
    void paint (juce::Graphics& g) override;

private:
    MonitorProcessor& proc;
    std::array<float, medidor::historyLen> mom {}, sht {};
    Readings rd;
};

// ---------------- ESPECTRO: FFT ao vivo e média lenta ----------------
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (MonitorProcessor& p) : proc (p) {}
    void update (float dt);
    void paint (juce::Graphics& g) override;

private:
    MonitorProcessor& proc;
    medidor::SpectrumAnalyzer analyzer;
};

// ---------------- ESTÉREO: goniômetro e correlação de fase ----------------
class StereoView : public juce::Component
{
public:
    explicit StereoView (MonitorProcessor& p) : proc (p) {}
    void update (float corr, float dt);
    void paint (juce::Graphics& g) override;

private:
    static constexpr int points = 1536;
    MonitorProcessor& proc;
    std::array<float, points> px {}, py {};
    float corrSm = 0.0f, zoom = 1.0f;
};

// ---------------- NÍVEL: pico, true peak e RMS (barras com o máximo segurado) ----------------
class LevelView : public juce::Component
{
public:
    explicit LevelView (MonitorProcessor& p);
    void update (float peak, float truePeak, float rmsGain, float dt);
    void clearHolds();
    void paint (juce::Graphics& g) override;

private:
    MonitorProcessor& proc;
    std::array<float, 3> level {}, hold {}, holdLeft {};   // dB
};

// ---------------- linha de estado do OUVIR NO ALVO ----------------
class ListenInfo : public juce::Component
{
public:
    void set (float db, bool on, bool waiting);
    void paint (juce::Graphics& g) override;

private:
    float gainDb = 0.0f;
    bool  active = false, pending = false;
};

class MonitorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MonitorEditor (MonitorProcessor&);
    ~MonitorEditor() override;

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

    MonitorProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1064×580 (rack: 1000 de frente + as orelhas), e é ampliado junto
    LoudnessView loud;
    HistoryView history;
    SpectrumView spectrum;
    StereoView stereo;
    LevelView level;
    ListenInfo listenInfo;
    Knob targetMacro, tpKnob, tolKnob, lraKnob;
    KeyButton monoKey, matchKey, resetKey, freezeKey;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    std::unique_ptr<APVTS::ButtonAttachment> monoAtt, matchAtt, bypassAtt;
    double lastTick = 0.0;
    int resetFlash = 0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorEditor)
};
