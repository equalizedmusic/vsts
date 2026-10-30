#pragma once

#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"
#include "Layout.h"
#include <map>
#include <unordered_map>

// A janela é a console inteira: os canais de todas as cópias da MESA (12 vagas; mais que isso rola), os 3 grupos e o
// MASTER. Tudo é desenhado em centímetros com as peças renderizadas no Blender (Source/img) numa câmera
// ortográfica inclinada: a chapa de cada módulo, knobs em 48 posições, teclas soltas/apertadas, LEDs acesos.
// A janela mostra um pedaço da console (PERTO, uns 6 canais) ou ela toda (GERAL); rola com a roda ou arrastando.
class MesaEditor : public juce::AudioProcessorEditor,
                   private juce::Timer,
                   private mesa::Registry::Listener,
                   private juce::ScrollBar::Listener
{
public:
    explicit MesaEditor (MesaProcessor&);
    ~MesaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

    struct Art;    // imagens decodificadas (uma vez por processo, divididas entre as janelas abertas)

private:
    enum class Kind { knob, toggle, radio, fader, mfader, preModel, eqModel, tape, modeBtn, optBtn };
    struct Ctl
    {
        Kind kind;
        MesaProcessor* p = nullptr;
        juce::String pid;
        float x = 0, y = 0, d = 1;          // âncora na tela (cm, y para baixo) e tamanho
        float ly = 0;                       // y do layout (cm, para cima) da âncora
        juce::String sprite;                // knob: família; tecla: qual
        int value = 0;                      // radio/modeBtn/optBtn: qual
    };
    struct Strip
    {
        MesaProcessor* p = nullptr;         // nullptr = vaga vazia
        float cx = 0;                       // centro (cm)
        juce::String number;
        bool group = false;
        int slot = 0;
    };
    struct Meter { float vu = -60, peakL = -120, peakR = -120, gr = 0, vuL = -60, vuR = -60; };
    struct Cell { juce::Rectangle<int> r; float ax = 0, ay = 0; };
    enum class Btn { none, zoomOut, zoomIn, overview };

    void timerCallback() override;
    void consoleChanged() override { rebuild(); }
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    void rebuild();
    void buildCtls();
    void addStripCtls (const Strip& s);
    void addMasterCtls();
    void drawCell (juce::Graphics& g, bool chapa, const juce::String& name, float x, float layoutY, float za = mesa::lay::SURF,
                   float scale = 1.0f, float stretchW = 0.0f);
    void drawCache();
    void drawStripStatic (juce::Graphics& g, const Strip& s);
    void drawMasterStatic (juce::Graphics& g);
    void drawCtl (juce::Graphics& g, const Ctl& c);
    void drawNeedle (juce::Graphics& g, float cx, float layoutY, float za, float w, float h, float vu);
    void drawDynamic (juce::Graphics& g);
    void drawOverlay (juce::Graphics& g);
    void drawBubble (juce::Graphics& g);

    // vista: zoom (px por cm) e o canto de cima à esquerda (cm) do pedaço da console que aparece
    static float sy (float layoutY, float za = mesa::lay::SURF);      // y do layout -> cm na tela (y para baixo)
    static float consoleH();                                         // altura da console na tela (cm)
    float fitZoom() const;
    float closeZoom() const;
    void setView (float zoom, float vx, float vy);
    void zoomAround (float newZoom, juce::Point<float> pos);
    void focusOn (float cx);
    void updateScrollBars();
    juce::AffineTransform viewXf() const { return juce::AffineTransform::translation (-viewX, -viewY).scaled (zoom); }
    juce::Point<float> toCm (juce::Point<float> p) const { return { p.x / zoom + viewX, p.y / zoom + viewY }; }
    juce::Rectangle<float> visibleCm() const { return { viewX, viewY, (float) getWidth() / zoom, (float) getHeight() / zoom }; }
    juce::Rectangle<float> btnRect (Btn b) const;
    Btn btnAt (juce::Point<float> p) const;

    float norm (const Ctl& c) const;
    void setNorm (const Ctl& c, float v);
    const Ctl* hit (juce::Point<float> logical) const;
    juce::String describe (const Ctl& c) const;
    void begin (const Ctl& c);
    void end();
    void click (const Ctl& c, const juce::MouseEvent& e);
    void rename (MesaProcessor* p);
    void showMainMenu();
    std::vector<float> snapshot() const;

    MesaProcessor& proc;
    juce::SharedResourcePointer<Art> art;
    std::unordered_map<std::string, Cell> chapaCell, pecaCell;
    std::map<std::pair<juce::String, int>, juce::Image> clipped;   // (nome, nível) -> pedaço do atlas
    float drawPx = 20.0f;              // px de verdade por cm no desenho em curso (escolhe o nível do atlas)
    juce::Rectangle<float> drawVis;    // o que aparece (cm), para pular o que está fora

    std::vector<Strip> strips;       // canais à vista + 3 grupos
    MesaProcessor* master = nullptr;
    std::vector<MesaProcessor*> channels;
    int scroll = 0;
    std::vector<Ctl> ctls;
    std::vector<int> drawOrder;      // de trás para a frente (o que está mais embaixo na tela cobre o de cima)
    std::map<MesaProcessor*, Meter> meters;
    Meter masterMeter;

    float zoom = 20.0f, viewX = 0.0f, viewY = 0.0f;
    bool overview = false;
    juce::Image cache;
    float cacheScale = 0;
    bool cacheDirty = true;
    std::vector<float> lastSnap;
    int lastVersion = -1;

    int hover = -1, active = -1;
    Btn btnHover = Btn::none;
    float dragStart = 0, dragValue = 0;
    bool panning = false;
    juce::Point<float> panStart;
    juce::RangedAudioParameter* gestureParam = nullptr;

    juce::ScrollBar scrollBar { false };            // vagas de canal (mais de 12 cópias)
    juce::ScrollBar hBar { false }, vBar { true };  // a vista
    int openSettle = 0;
    isp::TelaAtivacao ativacao;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MesaEditor)
};
