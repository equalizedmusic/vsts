#pragma once

#include "PluginProcessor.h"

// Tela do RETALHO (base 1200 × 620, escala junto com a janela). Abre no SIMPLE; o PRO segue o esboço v2 de 15/11/2026:
//  - topo: marca, abas EDIT / COMPLEX / GROOVE, PRESET ▾, KEY, TEMPO, LATENCY, CAPTURE e o menu ≡;
//  - linha de botões da aba (SHAPE, LENGTH, RATE… / STEPS, READ, DICE MODE / CHOPS, CUT, STYLE…);
//  - display de onda: ORIGINAL (a voz capturada, ataques ▼, CHUNK em destaque) e RESULT (o fill renderizado em
//    blocos, PITCH ciano, FILTER rosa; no COMPLEX os passos; no GROOVE os 8 chops e o pattern);
//  - painéis de knobs da aba e o rodapé LIVE fixo (AUTO / HOLD / ONE-SHOT, ONDE, CATCH, 6 pads, DRY/WET, OUTPUT, A/B).
// Tudo desenhado no paint, a 30 Hz.
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
    struct Knob { int campo; juce::Point<float> c; float r; };   // campo < 0 = macro do SIMPLE (−1 − índice)
    struct Alvo { juce::Rectangle<float> r; std::function<void()> f; };
    enum { arNada, arKnob, arGrade, arLivre, arPasso, arCelula, arDaw, arMacro, arSel, arPedaco, arNota };

    // SIMPLE (tela padrão desde 16/11/2026): 4 cartões de efeito, a onda do resultado, 5 macros, QUANDO TOCA e os pads.
    // PRO = a tela completa (EDIT / COMPLEX / GROOVE). A troca é só de tela: o som e os parâmetros são os mesmos.
    bool simples() const { return proc.uiPro.load() == 0; }
    void telaSimples (juce::Graphics&);
    void chaveModo (juce::Graphics&);
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
    void blocosFill (juce::Graphics&, juce::Rectangle<float> in);
    int cartao() const;          // 0 STUTTER, 1 RISER, 2 TAPE STOP, 3 GROOVE, −1 cena do COMPLEX
    int quando() const;          // 0 TODO COMPASSO, 1 FIM DE 4 BARS, 2 FIM DE 8 BARS, 3 SÓ NO PAD, −1 outro (do PRO)
    float macroLe (int i) const;
    void macroDefine (int i, float v);
    float macroPadrao (int i) const;
    juce::String macroTexto (int i) const;
    bool macroAtivo (int i) const { return cartao() >= 0 || i >= 3; }

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }

    void topo (juce::Graphics&);
    void linha (juce::Graphics&);
    void original (juce::Graphics&, juce::Rectangle<float> r);
    void result (juce::Graphics&, juce::Rectangle<float> r);
    void resultEdit (juce::Graphics&, juce::Rectangle<float> in);
    void resultComplex (juce::Graphics&, juce::Rectangle<float> in);
    void resultGroove (juce::Graphics&, juce::Rectangle<float> in);
    void paineis (juce::Graphics&);
    void rodape (juce::Graphics&);

    // peças
    float grupo (juce::Graphics&, float x, float y, const juce::String& titulo, int campo, const juce::StringArray& nomes);
    void botao (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, bool on, juce::Colour cor, std::function<void()> f, float fonteH = 10.5f);
    void liga (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, int campo, juce::Colour cor);
    void knob (juce::Graphics&, int campo, const juce::String& nome, juce::Point<float> c, juce::Colour cor);
    void painel (juce::Graphics&, juce::Rectangle<float> r, const juce::String& titulo, const juce::String& sub);
    float chip (juce::Graphics&, float xDir, const juce::String& rot, const juce::String& val, juce::Colour cor, std::function<void()> f,
                float maxW = 10000.0f);
    void menuEscolha (int campo);
    void menuPresets();
    void menu();
    void menuPad (int i);

    float real (int campo) const { return proc.valor (campo); }
    int escolha (int campo) const { return juce::roundToInt (proc.valor (campo)); }
    juce::String texto (int campo) const;
    juce::String nomeTom() const;

    // áreas dos displays (coordenadas base) para o mouse
    juce::Rectangle<float> areaResult, areaOriginal, areaDaw, areaCelulas, areaAmostra;
    RetalhoProcessor::VisaoAmostra visaoA;   // cópia (muda só quando a amostra muda)
    float selA0 = 0.0f;                      // arrasto da seleção
    bool soltando = false;
    juce::String avisoArquivo;
    int avisoQuadro = 0;
    juce::Rectangle<float> areaPads[retalho::numPads];
    float celX0 = 0, celW = 1, celY0 = 0, celH = 1;
    int nCelVis = 16;

    RetalhoProcessor& proc;
    RetalhoProcessor::Vista vista;   // cópia do quadro
    int versaoVista = -1;
    std::vector<Knob> knobs;
    std::vector<Alvo> alvos;
    int arrasto = arNada, arrastoIdx = -1, arrastoLinha = 0, knobCampo = -1, padSegurado = -1, macroIdx = -1;
    float valorIni = 0.0f, yIni = 0.0f, xIni = 0.0f;
    bool moveu = false;
    int quadro = 0;
    std::unique_ptr<juce::FileChooser> seletor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetalhoEditor)
};
