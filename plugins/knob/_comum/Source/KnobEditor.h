#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "KnobProcessor.h"
#include "IspTelaAtivacao.h"

// interface comum da SÉRIE KNOB: o pedal renderizado no Blender (fundo, knobs em quadros, alavanca, plaquinha do
// nome) e, por cima, o que muda ao vivo — lâmpadas, linha e emblema do knob, visor do SHAPE e os textos da faixa.
// Tudo é desenhado no espaço da imagem (1000 x 1284); a janela mostra só o recorte do pedal (848 x 1092), escalado.
class KnobEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KnobEditor (KnobProcessor&);
    ~KnobEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    enum Alvo { nenhum, alvoKnob, alvoRate, alvoMix, alvoVisor, alvoAlavanca };
    Alvo alvoEm (juce::Point<float> img) const;
    juce::Point<float> naImagem (juce::Point<float> p) const;
    float valorDe (const juce::String& pid) const;
    void definir (const juce::String& pid, float valorReal);
    void menuForma();
    void menuRate();
    void menuPedal();

    void desenharLampadas (juce::Graphics&, float valor, float pulso);
    void desenharKnobGrande (juce::Graphics&, float valor);
    void desenharKnobPequeno (juce::Graphics&, const juce::Image* quadros, float anguloGraus, bool rate);
    void desenharVisor (juce::Graphics&);
    void desenharTextos (juce::Graphics&);
    void montarEstatico();

    KnobProcessor& proc;
    juce::Colour cor;
    juce::Image fundo, nome, knob[16], rate[16], mix[16], alav[3], estatico;
    float escalaEstatico = 0.0f;

    Alvo arrastando = nenhum;
    float valorInicio = 0.0f;
    juce::Point<float> inicio;
    juce::RangedAudioParameter* paramArrasto = nullptr;

    int disparosVistos = 0;
    float flash = 0.0f;
    float pulso = 0.0f;

    juce::ComponentBoundsConstrainer limites;
    std::unique_ptr<juce::ResizableCornerComponent> canto;
    isp::TelaAtivacao ativacao;   // por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobEditor)
};
