#pragma once

#include "PluginProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <functional>

// Tela do MASTER DOS MAGOS (base 1280 × 952, escala junto com a janela), esboço v2 aprovado (10/11/2026), paleta do
// MAESTRO (par dele) com o M em ouro.
//  - topo: marca, MAESTRO LINKED, REFERENCE, STYLE e TARGET;
//  - TONAL BALANCE: antes (cinza), referência / alvo (amarelo tracejado + zona), depois (ciano), sobra em vermelho,
//    pontos da DYNAMIC EQ e a faixa DYNAMIC EQ · LIVE;
//  - MULTIBAND COMP: 4 bandas (IN / GR / OUT, limiar, razão, ataque / soltura), AUTO ou MANUAL (arrasta os valores);
//  - DYNAMIC EQ (faixa embaixo do TONAL BALANCE): AUTO ou MANUAL (liga, RANGE e THRESH de cada ponto);
//  - LOUDNESS (integrado, true peak, curto, PLR, GR do limitador), STEREO por banda e WHERE IT WILL PLAY (quanto cada
//    plataforma abaixa + HEAR IT ON: PHONE / EARBUDS / CAR / MONO);
//  - MAESTRO LINKED: os 8 grupos do MAESTRO no projeto vs o STYLE e cada problema do master com o grupo de onde vem;
//  - embaixo: LISTEN, CAST, AMOUNT / CHARACTER / PUNCH / WIDTH / TONE TILT, MATCH TONE / LOUDNESS, A / B / REF / BYPASS.
class MagosEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit MagosEditor (MagosProcessor&);
    ~MagosEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { arrastandoArquivo = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { arrastandoArquivo = false; repaint(); }

    static constexpr int baseW = 1280, baseH = 952;

private:
    struct Knob
    {
        juce::String rotulo, id;
        juce::Point<float> c;
        float r = 20;
        juce::Colour cor;
        float padrao = 0.5f;
        bool centro = false;          // arco a partir do meio (PUNCH, TILT)
        std::function<juce::String()> texto;
    };
    struct Clique { juce::Rectangle<float> r; std::function<void()> faz; };
    // valor do MANUAL que se arrasta (para cima / para baixo; SHIFT = fino; duplo clique = padrão; roda)
    struct Ajuste { juce::Rectangle<float> r; juce::String id; };

    // MAESTRO LINK: um grupo neste quadro
    struct GrupoLink
    {
        bool tem = false, tocando = false;
        int faixas = 0;
        double esp[maestrolink::NB] {};
        double pot = 0.0;              // nível somado (potência)
        float alvo = 0.0f;             // relativo ao KICK no STYLE
        float desvio = 0.0f;           // (nível − KICK) − alvo
        bool temDesvio = false;
    };
    struct Problema { juce::String rotulo, texto, acao; juce::Colour cor; };

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }
    void leLink();
    void montaProblemas();

    void topo (juce::Graphics&);
    void painelTom (juce::Graphics&, juce::Rectangle<float>);
    void painelMulti (juce::Graphics&, juce::Rectangle<float>);
    void painelLoud (juce::Graphics&, juce::Rectangle<float>);
    void painelStereo (juce::Graphics&, juce::Rectangle<float>);
    void painelOnde (juce::Graphics&, juce::Rectangle<float>);
    void painelLink (juce::Graphics&, juce::Rectangle<float>);
    void base (juce::Graphics&);
    void titulo (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, const juce::String& sub);
    void desenhaKnob (juce::Graphics&, Knob&);
    void texto (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float tam, juce::Colour,
                juce::Justification, float kern = 0.12f, bool negrito = false);

    void menuStyle();
    void menuAlvo();
    void menuReferencia();
    void menuLogo();
    void escolheArquivo();
    void apertaCast();
    // MANUAL: ao ligar, copia o que o AUTO fazia (o som não pula); ao desligar, volta ao AUTO
    void ligaManualMulti (bool on);
    void ligaManualEq (bool on);
    void chaveAutoManual (juce::Graphics&, juce::Rectangle<float>, bool manual, std::function<void (bool)> faz);
    void valorAjuste (juce::Graphics&, juce::Rectangle<float>, const juce::String& id, const juce::String& val,
                      const juce::String& rot, juce::Colour cor);
    juce::RangedAudioParameter* parametro (const juce::String& id) const
    {
        return dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
    }
    const Ajuste* ajusteEm (juce::Point<float>) const;

    MagosProcessor& proc;
    std::vector<Knob> knobs;
    std::vector<Clique> cliques;
    std::vector<Ajuste> ajustes;
    juce::String idArrasto;                          // Ajuste sendo arrastado
    int arrastando = -1;
    float valorIni = 0, yIni = 0;
    bool arrastandoArquivo = false;
    std::unique_ptr<juce::FileChooser> seletor;

    // MAESTRO LINK
    GrupoLink grupos[maestrolink::NR];
    int nGrupos = 0;
    bool ligado = false;
    juce::String nomeStyleMaestro;
    int topoMaestro = 0;
    uint32_t batidaVista[maestrolink::NSL] {};
    double vivoAte[maestrolink::NSL] {};
    std::vector<Problema> problemas;

    // telas: o que segura e cai devagar
    float zonaHold[magos::NZ] {};

    // ANALISADOR do TONAL BALANCE (estilo Pro-Q 4): FFT de ~170 ms (8192 a 44,1/48 kHz) a 30 quadros/s na thread da
    // interface, 10 Hz – 35 kHz, média de 1/24 de oitava, inclinação de 4,5 dB/oitava em 1 kHz
    static constexpr int NP = 480;                  // pontos na tela
    void analisa (bool entrada);
    void preparaAnalisador (double fs);
    std::unique_ptr<juce::dsp::FFT> fftViz;
    double fsViz = 0.0;
    int nViz = 0;
    std::vector<float> janViz, bufViz;
    struct PontoViz { double f = 0; int k0 = 0, k1 = 0; double kf = 0; bool interp = false, valido = false; };
    std::vector<PontoViz> pontos;
    float espIn[NP] {}, espOut[NP] {};             // dB na tela (com a inclinação)
    float suaveOut[NP] {}, suaveIn[NP] {};         // 1/3 de oitava (sobra vermelha, pontos da DYN EQ, alinhamento)
    int posVista[2] { -1, -1 };
    bool temEsp[2] { false, false };
    float offAlvo = -40.0f;
    bool offAlvoOk = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagosEditor)
};
