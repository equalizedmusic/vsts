#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "IspTelaAtivacao.h"

// ---------------- ESPECTRO: a saída ao vivo, as 4 bandas coloridas, os cortes arrastáveis e a redução de cada banda ----------------
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (PrismaProcessor& p);

    void tick();                                     // lê o anel da saída e faz a FFT (timer da interface)
    void setReduction (int band, float db) { gr[(size_t) juce::jlimit (0, prisma::numBands - 1, band)] = db; }

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

    PrismaProcessor& proc;
    juce::dsp::FFT fft { prisma::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) prisma::fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * prisma::fftSize> fftData {};
    std::array<float, prisma::fftSize / 2> spec {};
    std::array<float, prisma::numBands> gr {};
    int hover = -1, dragging = -1;
};

// ---------------- faixa da banda (Hz) e medidor de redução ----------------
class BandStatus : public juce::Component
{
public:
    BandStatus (PrismaProcessor& p, int bandIndex) : proc (p), band (bandIndex) {}
    void setReduction (float db) { gr = db; }
    void paint (juce::Graphics& g) override;

private:
    PrismaProcessor& proc;
    int band;
    float gr = 0.0f;
};

class PrismaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PrismaEditor (PrismaProcessor&);
    ~PrismaEditor() override;

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

    PrismaProcessor& proc;
    PainelLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1064×580 (rack: 1000 de frente + as orelhas), e é ampliado junto
    SpectrumView spectrum;
    std::unique_ptr<BandStatus> status[prisma::numBands];
    std::unique_ptr<Knob> thrK[prisma::numBands], ratioK[prisma::numBands], attK[prisma::numBands],
                          relK[prisma::numBands], driveK[prisma::numBands], gainK[prisma::numBands];
    std::unique_ptr<KeyButton> soloKey[prisma::numBands], muteKey[prisma::numBands];
    Knob macro, output;
    Fader mix;
    BypassButton bypass;
    IconButton prevBtn { IconButton::Icon::prev }, nextBtn { IconButton::Icon::next }, saveBtn { IconButton::Icon::save };
    PresetName presetName;
    VuMeter meterIn { "IN" }, meterOut { "OUT" };
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;
    float levelIn = 0.0f, levelOut = 0.0f;
    float grShown[prisma::numBands] = {};
    double lastTick = 0.0;

    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir
    isp::TelaAtivacao ativacao;   // por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismaEditor)
};
