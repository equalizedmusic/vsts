#pragma once

#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"

// Tela do RETALHO (base 1200 × 620, escala junto com a janela). Uma tela só, desde a 0.1.5 (o SIMPLE/PRO e o AVANÇADO saíram):
//  - topo: marca, PICOTE | ESTICA | GROOVE, RECEITA / DIREÇÃO / PADRÃO ◀ ▶, VARIAR, status da amostra e o menu ≡;
//  - a faixa da amostra (arrastar o vocal, ABRIR, CAPTURAR), a onda grande e os controles do modo;
//  - rodapé com a dica do modo e a versão.
// Tudo desenhado no paint; o timer (30 Hz) só repinta quando algo muda.
class RetalhoEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit RetalhoEditor (RetalhoProcessor&);
    ~RetalhoEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // SAMPLER: arrastar o arquivo do vocal para qualquer lugar da janela
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { soltando = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { soltando = false; repaint(); }

    static constexpr int baseW = 1200, baseH = 620;

private:
    isp::TelaAtivacao ativacao;   // 0.2.0: sem licença cobre a tela e pede e-mail + token
    struct Knob { int campo; juce::Point<float> c; float r; };
    struct Alvo { juce::Rectangle<float> r; std::function<void()> f; };
    enum { arNada, arKnob, arDaw, arSel, arPedaco, arNota };

    void telaSimples (juce::Graphics&);   // a tela inteira
    void ondaGrande (juce::Graphics&, juce::Rectangle<float> r);      // 0.1.4: a onda grande da tela principal
    void controlesPicote (juce::Graphics&, float y);
    void controlesEstica (juce::Graphics&, float y);
    void controlesGroove (juce::Graphics&, float y);
    void desenhaOnda (juce::Graphics&, juce::Rectangle<float> r, juce::Rectangle<float> in);   // a camada guardada da onda
    juce::Image cacheOnda;
    uint64_t chaveOnda = 0, assinatura = 0;
    juce::Rectangle<float> areaOnda, areaGrade;
    int selU0 = -1, selU1 = -1;          // PICOTE: o trecho sendo marcado (colcheias)
    int unidadeEm (float x) const;
    void menuPedaco();
    void menuNota (int k);
    juce::String textoV (int k, float v) const;
    void exportar (juce::Graphics&, juce::Rectangle<float> r);
    bool verOriginal = false;
    juce::String avisoExport;
    int avisoExportQuadro = 0;
    void faixaAmostra (juce::Graphics&, juce::Rectangle<float> r);   // a amostra dentro do plugin
    void abreArquivo();
    void menuCaptura();

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }

    // peças
    void botao (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, bool on, juce::Colour cor, std::function<void()> f, float fonteH = 10.5f);
    void liga (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, int campo, juce::Colour cor);
    void menu();

    float real (int campo) const { return proc.valor (campo); }
    int escolha (int campo) const { return juce::roundToInt (proc.valor (campo)); }
    juce::String texto (int campo) const;
    juce::String nomeTom() const;

    // áreas dos displays (coordenadas base) para o mouse
    juce::Rectangle<float> areaDaw, areaAmostra;
    RetalhoProcessor::VisaoAmostra visaoA;   // cópia (muda só quando a amostra muda)
    float selA0 = 0.0f;                      // arrasto da seleção
    bool soltando = false;
    juce::String avisoArquivo;
    int avisoQuadro = 0;

    RetalhoProcessor& proc;
    RetalhoProcessor::Vista vista;   // cópia do quadro
    int versaoVista = -1;
    std::vector<Knob> knobs;
    std::vector<Alvo> alvos;
    int arrasto = arNada, arrastoIdx = -1, knobCampo = -1;
    float valorIni = 0.0f, yIni = 0.0f, xIni = 0.0f;
    bool moveu = false;
    int quadro = 0;
    std::unique_ptr<juce::FileChooser> seletor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetalhoEditor)
};
