#pragma once
// MAPA — tela principal com cara de DAW: transporte em cima, prévia do projeto (partes, régua e faixas com notas e
// waveform) no meio, painel "ABRIR NO…" à direita com as DAWs achadas no computador.
#include <JuceHeader.h>
#include "Projeto.h"
#include "Daws.h"
#include "Tocador.h"
#include "IA.h"

namespace mapa
{
class Tela : public juce::Component,
             public juce::FileDragAndDropTarget,
             private juce::Timer
{
public:
    Tela();
    ~Tela() override;

    void carregar (const juce::File& arquivo);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { arrastando = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { arrastando = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    class Trabalho;
    friend class Trabalho;

    enum class Estado { vazio, analisando, pronto, erro };

    void timerCallback() override;
    void mudaEtapa (float p, const juce::String& e);   // chamada pela thread da análise
    void escolherArquivo();
    void terminouAnalise (bool ok, juce::String erro, std::shared_ptr<juce::AudioBuffer<float>> audio, double taxa,
                          Projeto p, std::vector<float> picos, std::shared_ptr<const Stems> stems,
                          std::vector<std::vector<float>> picosStems, juce::String aviso);
    void abrirNaDawEscolhida (bool soPasta);
    void montarArranjo();   // redesenha a imagem do arranjo (cache)

    // desenho
    void pintaTopo (juce::Graphics&);
    void pintaPainel (juce::Graphics&);
    void pintaRodape (juce::Graphics&);
    void pintaVazio (juce::Graphics&, juce::Rectangle<int>);
    void pintaAnalisando (juce::Graphics&, juce::Rectangle<int>);
    void pintaArranjo (juce::Graphics&, juce::Rectangle<int> area);
    void pintaClipe (juce::Graphics&, const Faixa&, const Clipe&, juce::Rectangle<float> r);

    float xDe (double tempos) const;          // tempos do projeto -> x na tela
    double temposDe (float x) const;          // x na tela -> tempos

    Estado estado = Estado::vazio;
    juce::String erroMsg, aviso;
    juce::File arquivo, ultimaPasta;
    std::atomic<float> progresso { 0 };
    juce::String etapa;
    juce::CriticalSection travaEtapa;
    std::unique_ptr<Trabalho> trabalho;
    std::unique_ptr<juce::FileChooser> seletor;

    std::shared_ptr<juce::AudioBuffer<float>> audio;
    double taxa = 44100;
    Projeto proj;
    std::vector<float> picos;   // máximo absoluto por bloco, para a waveform
    std::shared_ptr<const Stems> stems;                // 0.2: stems da IA (44,1 kHz), tocados na prévia
    std::vector<std::vector<float>> picosStems;        // waveform de cada stem (índice = Stem)

    juce::Array<Daw> daws;
    int dawSel = 0;
    bool comNotas = MAPA_TESTE != 0;
    int parteSel = -1;
    bool arrastando = false;
    double tempoAnalise = 0;

    Tocador tocador;

    // áreas
    juce::Rectangle<int> rTopo, rMeio, rArr, rPainel, rRodape;
    juce::Rectangle<int> rSecoes, rRegua, rFaixas;
    juce::Rectangle<int> rBtnInicio, rBtnPlay, rBtnLoop, rRef, rSoltar, rBtnEscolher, rIr, rSoPasta, rOptPro;
    std::vector<juce::Rectangle<int>> rDaws, rLinhas;
    std::vector<std::pair<juce::Rectangle<int>, juce::Rectangle<int>>> rMS;   // botões M e S por faixa
    juce::Image cacheArranjo;
    bool arranjoSujo = true;
    static constexpr int LARG_CAB = 210;
};
} // namespace mapa
