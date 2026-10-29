#pragma once

#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"
#include "Lessons.h"
#include "MontaUi.h"

class MontaMontaEditor;

// ---------------- uma peça montada no rack: o painel com os controles ----------------
class ModulePanel : public juce::Component
{
public:
    ModulePanel (MontaMontaEditor& ed, MontaMontaProcessor& p, int module);
    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;      // só a peça (o contorno com pino e recorte)
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    void setCableMode (bool c) { if (c != cableMode) { cableMode = c; repaint(); } }
    void setLevel (float l);
    void setLang (int l) { if (l != lang) { lang = l; repaint(); } }

    // desenho só da face (também usado na peça arrastada)
    static void drawFace (juce::Graphics& g, mu::RF r, int module, int lang, bool withSub = true);

    const int module;

private:
    void drawScreen (juce::Graphics& g);

    MontaMontaEditor& editor;
    MontaMontaProcessor& proc;
    bool cableMode = false, grabbing = false;
    int lang = 0;
    float level = 0.0f;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> watch;   // a telinha segue os knobs
};

// ---------------- bandeja de peças (esquerda) ----------------
class TrayView : public juce::Component
{
public:
    TrayView (MontaMontaEditor& ed) : editor (ed) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

    static constexpr std::array<int, mm::numModules> order { mm::vco1, mm::mixer, mm::vcf, mm::envAmp, mm::output, mm::envMod, mm::vco2 };
    static juce::Rectangle<float> itemRect (int i) { return { 16.0f, 52.0f + (float) i * 62.0f, 132.0f, 46.0f }; }

private:
    int itemAt (juce::Point<float> p) const;
    MontaMontaEditor& editor;
    int dragging = -1;
};

// ---------------- fios do modo CABOS (por cima das peças; só pega o mouse perto das tomadas) ----------------
class CableLayer : public juce::Component
{
public:
    CableLayer (MontaMontaEditor& ed, MontaMontaProcessor& p) : editor (ed), proc (p) {}
    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    int outputAt (juce::Point<float> p) const;   // peça cuja tomada de saída está ali
    int inputAt (juce::Point<float> p) const;    // entrada (mm::Input) ali
    MontaMontaEditor& editor;
    MontaMontaProcessor& proc;
    int dragFrom = -1;
    juce::Point<float> dragPos;
};

// ---------------- a peça na mão (arrastando da bandeja ou do rack) ----------------
class DragLayer : public juce::Component
{
public:
    DragLayer() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override;
    int module = -1, lang = 0;
    juce::Point<float> pos, grab;
};

// ---------------- cartão da peça (direita): o que é, como funciona e o desafio ----------------
class InfoCard : public juce::Component
{
public:
    InfoCard (MontaMontaEditor& ed, MontaMontaProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void setModule (int m);
    int  getModule() const { return module; }
    void refresh() { repaint (16, 66, getWidth() - 32, 92); }

private:
    void drawGraph (juce::Graphics& g, mu::RF r);
    MontaMontaEditor& editor;
    MontaMontaProcessor& proc;
    int module = mm::vco1;
    mu::Pill listen { "OUVIR EXEMPLO", mu::gold }, done { "FEITO", mu::gold };
    std::vector<std::unique_ptr<juce::ParameterAttachment>> watch;
};

class ScopeView : public juce::Component
{
public:
    explicit ScopeView (MontaMontaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    juce::String title;
    juce::Colour colour = mu::dim;
    bool live = false;

private:
    MontaMontaProcessor& proc;
};

// teclado de 2 oitavas (Dó 3 a Si 4)
class KeysView : public juce::Component
{
public:
    explicit KeysView (MontaMontaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    juce::Colour colour = mu::moduleColour (mm::vcf);

private:
    int noteAt (juce::Point<float> p) const;
    void press (int note);
    MontaMontaProcessor& proc;
    int down = -1;
};

// as 6 bolinhas das lições no cabeçalho
class LessonDots : public juce::Component
{
public:
    explicit LessonDots (MontaMontaEditor& ed) : editor (ed) {}
    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    MontaMontaEditor& editor;
};

class MontaMontaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MontaMontaEditor (MontaMontaProcessor&);
    ~MontaMontaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // ---- usados pelas peças ----
    int  lang() const { return proc.language.load(); }
    void select (int module);
    void beginDrag (int module, bool fromRack, const juce::MouseEvent& e, juce::Point<float> grabOffset);
    void dragMove (const juce::MouseEvent& e);
    void endDrag (const juce::MouseEvent& e);
    void removeFromRack (int module);
    void setLesson (int lesson);
    void showMessage (const juce::String& pt, const juce::String& en, juce::Colour c);
    void patchEdited();               // depois de qualquer mudança na montagem

    static juce::Rectangle<float> slotRect (int module);
    static juce::Point<float> jackPos (int module, int input);   // input < 0: tomada de saída da peça
    juce::Component& getCanvas() { return canvas; }
    bool draggingPiece() const { return drag.module >= 0; }
    int  draggedModule() const { return drag.module; }
    float shownOutLevel() const { return level; }

    MontaMontaProcessor& proc;

private:
    class Canvas : public juce::Component
    {
    public:
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    void timerCallback() override;
    void drawCanvas (juce::Graphics&);
    void drawHeader (juce::Graphics&);
    void drawRack (juce::Graphics&);
    void drawTipBox (juce::Graphics&);
    void drawPath (juce::Graphics&);
    void syncPanels();
    void updateReach();
    void showMenu();
    void setMode (bool cables);
    void setScale (float scale);
    int  nextNeeded() const;
    std::vector<int> pathChain() const;

    static constexpr int openSettle = 15;
    float openScale = 1.0f;
    int   openTicks = 0;

    Canvas canvas;   // tudo em 1200×740, ampliado junto
    std::array<std::unique_ptr<ModulePanel>, mm::numModules> panels;
    TrayView tray { *this };
    CableLayer cablesLayer { *this, proc };
    DragLayer dragLayer;
    InfoCard info { *this, proc };
    ScopeView scope { proc };
    KeysView keys { proc };
    LessonDots dots { *this };
    mu::Pill menuBtn { "MENU", mu::dim }, modeFit { "ENCAIXE", mu::ok }, modeCables { "CABOS", mu::ok };
    mu::Pill langPt { "PT", mu::ink }, langEn { "EN", mu::ink };
    mu::Pill nextBtn { mu::u8 ("PRÓXIMA LIÇÃO"), mu::ok }, wireDefault { mu::u8 ("LIGAR O PADRÃO"), mu::ok }, wireClear { "SOLTAR TODOS", mu::warn };

    struct Drag { int module = -1; bool fromRack = false; } drag;

    mm::Reach reach;
    int probeModule = -1;
    int lastPatchVersion = -1;
    int noteMark = 0;
    bool pathComplete = false, lessonDone = false, demoShown = false;
    juce::String msgPt, msgEn;
    juce::Colour msgColour;
    double msgUntil = 0.0;
    float level = 0.0f, shownLevel = -1.0f;
    juce::String lastPathKey;

    isp::TelaAtivacao ativacao;   // por último: por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MontaMontaEditor)
};
