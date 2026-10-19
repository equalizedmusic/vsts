#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// ---------------- ESPECTRO: a saída ao vivo, as 4 bandas coloridas, os cortes arrastáveis e a redução de cada banda ----------------
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (LenteProcessor& p);

    void tick();                                     // lê o anel da saída e faz a FFT (timer da interface)
    void setReduction (int band, float db) { gr[(size_t) juce::jlimit (0, lente::numBands - 1, band)] = db; }

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xOfHz (float hz) const;
    float hzOfX (float x) const;
    int   lineAt (float x) const;                    // corte perto do mouse (−1 = nenhum)
    void  setCrossover (int k, float hz);

    LenteProcessor& proc;
    juce::dsp::FFT fft { lente::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) lente::fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * lente::fftSize> fftData {};
    std::array<float, lente::fftSize / 2> spec {};
    std::array<float, lente::numBands> gr {};
    int hover = -1, dragging = -1;
};

// ---------------- faixa da banda (Hz) e medidor de redução ----------------
class BandStatus : public juce::Component
{
public:
    BandStatus (LenteProcessor& p, int bandIndex) : proc (p), band (bandIndex) {}
    void setReduction (float db) { gr = db; }
    void paint (juce::Graphics& g) override;

private:
    LenteProcessor& proc;
    int band;
    float gr = 0.0f;
};

class LenteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LenteEditor (LenteProcessor&);
    ~LenteEditor() override;

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

    LenteProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1000×580, e é ampliado junto
    SpectrumView spectrum;
    std::unique_ptr<BandStatus> status[lente::numBands];
    std::unique_ptr<Knob> thrK[lente::numBands], ratioK[lente::numBands], attK[lente::numBands],
                          relK[lente::numBands], driveK[lente::numBands], gainK[lente::numBands];
    std::unique_ptr<KeyButton> soloKey[lente::numBands], muteKey[lente::numBands];
    Knob macro, output;
    Fader mix;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    LedMeter meterIn { "IN" }, meterOut { "OUT" };
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;
    float levelIn = 0.0f, levelOut = 0.0f;
    float grShown[lente::numBands] = {};
    double lastTick = 0.0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LenteEditor)
};
