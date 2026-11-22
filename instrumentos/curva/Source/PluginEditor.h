#pragma once

#include "PluginProcessor.h"
#include <functional>

// Tela da CURVA (base 1000 × 752, escala junto com a janela; tudo desenhado no paint, a 30 Hz).
//  - SPECTRUM: FFT de 8192 da mix (IN) e do monitor (OUT), suavização 1/3–1/24 de oitava, TILT, PEAK HOLD,
//    a curva de correção por cima (eixo da direita), regiões SUB…AIR e leitura no mouse (Hz, nota, dB).
//  - LEVELS: pico + RMS de IN e OUT (L/R) com hold, e a redução do SAFE.
//  - LOUDNESS (da mix, antes da CURVA): I grande, M, S, LRA, TRUE PEAK máx. e PLR, com RESET.
//  - LISTEN LEVEL (knob grande + atalhos de volume), TARGET, AMOUNT, OUTPUT, AUTO GAIN e SAFE.
//  - EQUAL LOUDNESS: as duas curvas ISO 226 e o que cada faixa ganha.
//  - STEREO: vectorscope, correlação, largura e balanço.
// Paleta: azul-noite; OUT ciano→violeta, IN cinza-azulado, correção MAGENTA, aviso ÂMBAR.
class CurvaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CurvaEditor (CurvaProcessor&);
    ~CurvaEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr int baseW = 1000, baseH = 752;
    static constexpr int NP = 480;   // pontos do espectro (20 Hz–20 kHz, em log)

private:
    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }

    // análise para a tela
    void espectro (const std::vector<float>& anel, int fim, std::array<float, NP>& alvo, std::array<float, NP>& pico, bool primeiro);
    void medidores();

    // desenho
    void topo (juce::Graphics&);
    void painelEspectro (juce::Graphics&, juce::Rectangle<float> r);
    void painelNiveis (juce::Graphics&, juce::Rectangle<float> r);
    void painelLoudness (juce::Graphics&, juce::Rectangle<float> r);
    void painelControles (juce::Graphics&, juce::Rectangle<float> r);
    void painelOuvido (juce::Graphics&, juce::Rectangle<float> r);
    void painelStereo (juce::Graphics&, juce::Rectangle<float> r);
    void knob (juce::Graphics&, juce::Point<float> c, float raio, const char* id, const juce::String& rotulo,
               const juce::String& valor, const juce::String& unidade, juce::Colour cor, bool grande);
    void chip (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, bool aceso, juce::Colour cor,
               std::function<void()> acao, float fonteH = 9.5f);
    void menu();
    void alternaParam (const char* id);      // liga/desliga um parâmetro booleano (ON, AUTO GAIN, SAFE)

    // alvos de clique (coordenadas da tela base), refeitos a cada paint
    struct Alvo { juce::Rectangle<float> r; std::function<void()> acao; juce::String param; };
    std::vector<Alvo> alvos;
    juce::RangedAudioParameter* arrasto = nullptr;
    float arrastoY = 0.0f, arrastoV = 0.0f;
    juce::Point<float> mouse { -1, -1 };
    juce::Rectangle<float> rPlot;            // área do gráfico do espectro (para a leitura no mouse)

    CurvaProcessor& proc;

    // espectro
    juce::dsp::FFT fft { 13 };
    static constexpr int N = 8192;
    std::vector<float> janela, buf;
    std::vector<double> acum;                // soma acumulada da potência em double: em float o agudo some no arredondamento
    std::array<float, NP> freq {}, specIn {}, specOut {}, holdIn {}, holdOut {};
    bool specPrimeiro = true;

    // medidores (dB, com queda e hold)
    float nivel[4] { -100, -100, -100, -100 }, nivelHold[4] { -100, -100, -100, -100 }, rmsDb[4] { -100, -100, -100, -100 };
    int holdCont[4] {};
    float safeDb = 0.0f, escalaScope = 1.0f, larguraSt = 0.0f, balanco = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvaEditor)
};
