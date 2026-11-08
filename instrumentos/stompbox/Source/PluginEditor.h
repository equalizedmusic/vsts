#pragma once

#include "PluginProcessor.h"

// Tela do STOMPBOX (base 1600 × 900, escala junto com a janela), no desenho do esboço aprovado em 07/11/2026.
// Em cima: presets (◀ visor ▶, SAVE, A/B), SYNC com o BPM do DAW, TUNER e HQ. No meio: o board, 6 pedais à vista com
// rolagem (até 10), ligados por cabos, da esquerda para a direita; a posição livre depois do último tem o "+".
// Embaixo: INPUT + NOISE GATE, o AMP (5 modelos, a cor muda com o modelo), o CAB e o OUTPUT.
// Desempenho: as peças caras (fundo, carcaça de cada pedal, corpo do amp, caixa do cab, base de cada knob) ficam
// prontas em imagens guardadas na resolução da tela; a cena (montagem dessas peças + ponteiros, chaves e LEDs) só é
// refeita quando algum parâmetro muda. A 30 Hz só os medidores, o LED do gate e o afinador são repintados (área
// pequena), nunca a janela inteira.
class StompboxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StompboxEditor (StompboxProcessor&);
    ~StompboxEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseW = 1600, baseH = 900;

    // alvos de clique (coordenadas da tela base)
    enum TipoAlvo { aKnob, aSwitch, aFoot, aPedal, aRemove, aVazio, aTreadle, aPrev, aNext, aLcd, aSave, aAB, aSync, aTuner, aHq,
                    aMenu, aSetaE, aSetaD, aBarra, aAmpModelo, aAmpOn, aCanal, aCab, aMic, aPick, aPickCat, aPickFecha };
    struct Alvo { juce::Rectangle<float> r; TipoAlvo tipo; int slot = -1; int idx = -1; juce::String pid; float def = 5.0f; };

private:
    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }
    juce::Point<float> base (const juce::MouseEvent& e) const { return e.position / escala(); }

    // cena estática
    void desenhaCena (juce::Graphics&);
    void topo (juce::Graphics&);
    void boardArea (juce::Graphics&);
    void embaixo (juce::Graphics&);
    void painelAmp (juce::Graphics&, juce::Rectangle<float>);
    void painelCab (juce::Graphics&, juce::Rectangle<float>);
    void seletor (juce::Graphics&);
    void desenhaPedal (juce::Graphics&, int tipo, juce::Point<float> baseCentro, const StompboxProcessor::SlotDados& d, int slot, bool alvos);
    void corpoPedal (juce::Graphics&, int tipo, juce::Point<float> baseCentro);   // partes fixas (vão para a imagem guardada)
    void corpoAmp (juce::Graphics&, int modelo, juce::Rectangle<float> corpo);
    void corpoCab (juce::Graphics&, int cab, juce::Rectangle<float> caixa);
    void fundo (juce::Graphics&);
    void knobP (juce::Graphics&, const juce::String& pid, juce::Point<float> c, float diam, int estilo, const juce::String& rotulo,
                juce::Colour corRotulo, float def, int slot, float valor);
    void chave (juce::Graphics&, juce::Point<float> c, int n, int pos, const char* const* nomes, juce::Colour cor, float tam);
    void botao (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, bool on, TipoAlvo tipo, int idx = -1, float fonte = 12.0f);

    // dinâmico
    void medidor (juce::Graphics&, juce::Rectangle<float> r, float nivel);
    void desenhaAfinador (juce::Graphics&);
    void miniAfinador (juce::Graphics&);   // sempre ligado, na entrada (não silencia)
    void repintaBase (juce::Rectangle<float> r);   // repinta só esta área (coordenadas da tela base)

    // camadas guardadas (pixels = escala da cena); "fazCamada" desenha a área uma vez, "pinta" põe na tela
    juce::Image fazCamada (juce::Rectangle<float> area, const std::function<void (juce::Graphics&)>& desenha) const;
    void pinta (juce::Graphics&, const juce::Image&, juce::Point<float> origem) const;
    const juce::Image& camadaPedal (int tipo);
    const juce::Image& camadaKnob (int estilo, float diam);
    void limpaCamadas();
    void bolhaValor (juce::Graphics&);

    // ações
    const Alvo* acha (juce::Point<float> p) const;
    void mudaKnob (const Alvo& a, float valor);
    juce::String textoKnob (const Alvo& a) const;
    void menuPreset();
    void menuPedal (int slot);
    void pedeNomePreset();
    int totalPosicoes() const;              // pedais + a posição livre (até 10)
    int maxRolagem() const { return juce::jmax (0, totalPosicoes() - visiveis); }
    void rola (int delta);
    juce::Rectangle<float> rectSlot (int pos) const;   // posição na tela (à vista)
    void sujo() { cenaSuja = true; repaint(); }
    void analisaAfinador();

    StompboxProcessor& proc;
    static constexpr int visiveis = 6;
    std::vector<Alvo> alvos;
    juce::Image cena; bool cenaSuja = true; float cenaEscala = 0;
    juce::Image imgFundo, imgPedal[stomp::numTipos], imgAmp[stomp::numAmps], imgCab[3], grao, graoTolex;
    std::map<int, juce::Image> imgKnob;
    float camadasEscala = 0;
    float opacCamada = 1.0f;   // opacidade das camadas guardadas (0.92 só no pedal arrastado)
    double somaParams = 0;

    // arrasto
    const Alvo* ativo = nullptr; Alvo alvoArrasto; bool arrastandoKnob = false;
    float valorIni = 0, yIni = 0, xIni = 0;
    int pedalArrastado = -1; bool arrastandoPedal = false; float xPedal = 0; int destinoPedal = -1;
    bool arrastandoBarra = false;
    juce::Point<float> mouse;

    // seletor de pedais
    bool seletorAberto = false; int seletorSlot = 0; int seletorCat = 0; bool seletorTroca = false;

    // medidores e afinador
    float vIn = 0, vOut = 0;
    float afinHz = 0, afinCents = 0; juce::String afinNota; float afinVis = 0; int tick = 0;
    juce::String afinNotaDesenhada; int afinCentsDesenhado = 999;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StompboxEditor)
};
