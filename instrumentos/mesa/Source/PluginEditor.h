#pragma once

#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"
#include "Layout.h"
#include <map>
#include <unordered_map>

// A janela é a console inteira: os canais de todas as cópias da MESA (até 12 à vista, mais que isso rola), os 3
// grupos e o MASTER. Tudo é desenhado em centímetros (o mesmo layout do render 3D) com as peças renderizadas no
// Blender (Source/img): a chapa de cada módulo, knobs em 48 posições, botões e LEDs acesos/apagados.
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

private:
    enum class Kind { knob, toggle, radio, fader, mfader, preModel, eqModel, tape, modeBtn, optBtn };
    struct Ctl
    {
        Kind kind;
        MesaProcessor* p = nullptr;
        juce::String pid;
        float x = 0, y = 0, d = 1;          // centro (cm na tela: y para baixo) e tamanho
        juce::String sprite;                // knob: família; botão: cor
        int value = 0;                      // radio/modeBtn/optBtn: qual
        juce::String text;                  // escrito na lente do botão
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

    void timerCallback() override;
    void consoleChanged() override { rebuild(); }
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    void rebuild();
    void buildCtls();
    void addStripCtls (const Strip& s);
    void addMasterCtls();
    juce::Image& sprite (const juce::String& name);
    void drawPiece (juce::Graphics& g, bool chapa, const juce::String& name, juce::Rectangle<float> cm);
    void drawCache();
    void drawStripStatic (juce::Graphics& g, const Strip& s);
    void drawMasterStatic (juce::Graphics& g);
    void drawCtl (juce::Graphics& g, const Ctl& c);
    void drawNeedle (juce::Graphics& g, float cx, float cy, float w, float h, float vu);
    void drawLed (juce::Graphics& g, const juce::String& col, bool on, float x, float y, float w, float h);
    void drawDynamic (juce::Graphics& g);
    void drawBubble (juce::Graphics& g);

    float ppcm() const { return (float) getWidth() / mesa::lay::TOTAL_W; }
    static float sy (float layoutY) { return mesa::lay::Y_TOP - layoutY; }   // y do layout -> cm na tela
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
    juce::Image chapasImg, pecasImg;
    std::unordered_map<std::string, juce::Rectangle<int>> chapaRect, pecaRect;
    std::map<juce::String, juce::Image> clipped;

    std::vector<Strip> strips;       // canais à vista + 3 grupos
    MesaProcessor* master = nullptr;
    std::vector<MesaProcessor*> channels;
    int scroll = 0;
    std::vector<Ctl> ctls;
    std::map<MesaProcessor*, Meter> meters;
    Meter masterMeter;

    juce::Image cache;
    float cacheScale = 0;
    bool cacheDirty = true;
    std::vector<float> lastSnap;
    int lastVersion = -1;

    int hover = -1, active = -1;
    float dragStart = 0, dragValue = 0;
    juce::RangedAudioParameter* gestureParam = nullptr;

    juce::ScrollBar scrollBar { false };
    juce::ComponentBoundsConstrainer constrainer;
    int openSettle = 0;
    isp::TelaAtivacao ativacao;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MesaEditor)
};
