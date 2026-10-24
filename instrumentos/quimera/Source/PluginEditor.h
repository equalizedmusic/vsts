#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>
#include "PluginProcessor.h"
#include "Widgets.h"
#include "SynthWidgets.h"

// ---------------- WAVETABLE EM 3D: os 32 quadros empilhados e o quadro de agora aceso (posição com modulação) ----------------
class WaveView : public juce::Component
{
public:
    WaveView (QuimeraProcessor& p, int oscIndex);
    void tick();
    void paint (juce::Graphics& g) override;
    void resized() override { cachedTable = -1; }

private:
    float livePos() const;
    juce::Point<float> pointAt (float frame, float u, float sample) const;

    QuimeraProcessor& proc;
    int osc;
    std::atomic<float>* onP = nullptr;
    std::atomic<float>* tabP = nullptr;
    std::atomic<float>* posP = nullptr;
    juce::Path stack;
    int   cachedTable = -1;
    float shownPos = -1.0f;
    int   shownTable = -1;
    bool  shownOn = false;
};

// ---------------- FILTRO: curva de resposta aproximada, onde o knob deixou e onde a modulação levou ----------------
class FilterView : public juce::Component
{
public:
    FilterView (QuimeraProcessor& p, int filterIndex);
    void tick();
    void paint (juce::Graphics& g) override;

private:
    float liveCut() const;

    QuimeraProcessor& proc;
    int flt;
    std::atomic<float>* onP = nullptr;
    std::atomic<float>* typeP = nullptr;
    std::atomic<float>* cutP = nullptr;
    std::atomic<float>* resP = nullptr;
    std::atomic<float>* mixP = nullptr;
    std::array<float, 6> shown {};
};

// ---------------- ENVELOPE: ADSR com a curva e o ponto de onde a voz mais recente está ----------------
class EnvView : public juce::Component
{
public:
    EnvView (QuimeraProcessor& p, int envIndex);
    void tick();
    void paint (juce::Graphics& g) override;

private:
    QuimeraProcessor& proc;
    int env;
    std::atomic<float>* aP = nullptr;
    std::atomic<float>* dP = nullptr;
    std::atomic<float>* sP = nullptr;
    std::atomic<float>* rP = nullptr;
    std::atomic<float>* cP = nullptr;
    std::array<float, 5> shown {};
    float level = 0.0f, lastLevel = 0.0f, shownLevel = -1.0f;
    int   stage = 0, shownStage = -1;   // 0 parado, 1 ataque, 2 decaimento, 3 sustain, 4 soltura
};

// ---------------- LFO: a forma e o ponto de agora ----------------
class LfoView : public juce::Component
{
public:
    LfoView (QuimeraProcessor& p, int lfoIndex);
    void tick();
    void paint (juce::Graphics& g) override;

private:
    juce::String infoText() const;

    QuimeraProcessor& proc;
    int lfo;
    std::atomic<float>* shapeP = nullptr;
    std::atomic<float>* syncP = nullptr;
    std::atomic<float>* rateP = nullptr;
    std::atomic<float>* divP = nullptr;
    std::atomic<float>* modeP = nullptr;
    float value = 0.0f;
    int   phaseIdx = 0, shownShape = -1;
    juce::String shownInfo;
};

// ---------------- OSCILOSCÓPIO da saída ----------------
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (QuimeraProcessor& p);
    void tick();
    void paint (juce::Graphics& g) override;

private:
    static constexpr int window = QuimeraProcessor::scopeSize / 2;
    QuimeraProcessor& proc;
    std::array<float, QuimeraProcessor::scopeSize> ring {};
    int start = 0;
    float peak = 0.0f;
};

// ---------------- VOZES: quantas estão soando e o volume de cada uma ----------------
class VoiceMeter : public juce::Component
{
public:
    explicit VoiceMeter (QuimeraProcessor& p);
    void tick();
    void paint (juce::Graphics& g) override;

private:
    QuimeraProcessor& proc;
    std::atomic<float>* voicesP = nullptr;
    juce::uint32 shownMask = 0xffffffffu;
    int shownCount = -1, shownLimit = -1;
};

// ---------------- uma linha da matriz: fonte ▸ destino ▸ quantidade ----------------
class MatrixRow : public juce::Component
{
public:
    MatrixRow (QuimeraProcessor& p, int slotIndex);
    void tick();
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    QuimeraProcessor& proc;
    int slot;
    std::atomic<float>* srcP = nullptr;
    std::atomic<float>* dstP = nullptr;
    std::atomic<float>* amtP = nullptr;
    ChoiceBox src, dst;
    AmountBar amt;
    float shownLive = 0.0f;
    bool  shownUsed = false;
    int   shownSrc = -1;
};

class QuimeraEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit QuimeraEditor (QuimeraProcessor&);
    ~QuimeraEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Card    { juce::Rectangle<float> r; int tab; };
    struct Title   { float x, y; juce::String prefix, accent; juce::Colour colour; int tab; };
    struct Caption { juce::Rectangle<float> r; juce::String text; juce::Justification just; float size; bool bold; int tab; };
    struct Desc    { juce::Rectangle<float> r; juce::String title, body; int tab; };

    // cria o componente dentro da tela em escala, já no lugar; tab −1 = aparece em todas as abas
    template <typename T, typename... Args>
    T* add (int tab, juce::Rectangle<int> bounds, Args&&... args)
    {
        auto owner = std::make_unique<T> (std::forward<Args> (args)...);
        T* c = owner.get();
        canvas.addAndMakeVisible (*c);
        c->setBounds (bounds);
        if (tab >= 0 && tab < (int) tabItems.size())
            tabItems[(size_t) tab].push_back (c);
        owned.push_back (std::move (owner));
        return c;
    }

    void attachButton (juce::Button& b, const juce::String& paramId);
    void addCard (int tab, int x, int y, int w, int h);
    void addTitle (int tab, float x, float y, const juce::String& prefix, const juce::String& accent, juce::Colour c);
    void addCaption (int tab, float x, float y, float w, float h, const juce::String& text,
                     juce::Justification just = juce::Justification::centredLeft, float size = 8.5f, bool bold = true);
    void addDesc (int tab, float x, float y, float w, float h, const juce::String& title, const juce::String& body);

    void buildHeader();
    void buildSynth();
    void buildMatrix();
    void buildFx();
    void buildArp();
    void buildBottom();
    void tickArp();
    void setArpPattern (int pattern);   // botões de padrão prontos da aba ARP

    void timerCallback() override;
    void showPresetMenu();
    void presetMenuChosen (int result);
    void askNewPreset();
    void importBank();
    void drawCanvas (juce::Graphics&);
    void setScale (float scale);

    static constexpr int openSettle = 15;   // quadros do timer (30 Hz) até a janela assentar ao abrir
    float openScale = 1.0f;                 // tamanho salvo no projeto, lido no construtor
    int   openTicks = 0;
    void setTab (int tab);
    void updateLfoVisibility (bool force);

    QuimeraProcessor& proc;
    BrandLookAndFeel lnf;

    ScaledCanvas canvas;   // tudo fica aqui dentro, em 1200×740, e é ampliado junto
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;   // depois dos componentes: some antes deles
    std::array<std::vector<juce::Component*>, 4> tabItems;   // SYNTH, MOD MATRIX, FX, ARP
    std::vector<Card> cards;
    std::vector<Title> titles;
    std::vector<Caption> captions;
    std::vector<Desc> descs;

    // o que o timer mexe (os donos estão em `owned`)
    TabBar*         tabs = nullptr;
    PresetName*     presetName = nullptr;
    LedMeter*       meterOut = nullptr;
    VoiceMeter*     voiceMeter = nullptr;
    StyledKeyboard* keys = nullptr;
    ScopeView*      scope = nullptr;
    Readout*        delayTime = nullptr;
    GrMeter*        compGr = nullptr;
    std::array<WaveView*, quimera::numOsc>       waves {};
    std::array<FilterView*, quimera::numFilters> filters {};
    std::array<EnvView*, quimera::numEnvs>       envs {};
    std::array<LfoView*, quimera::numLfos>       lfos {};
    std::array<MiniKnob*, quimera::numLfos>      lfoRate {}, lfoDiv {};
    std::array<std::atomic<float>*, quimera::numLfos> lfoSyncP {};
    std::array<int, quimera::numLfos>            lastLfoSync {};
    std::array<MatrixRow*, quimera::numSlots>    rows {};
    std::array<StepPad*, quimera::numArpSteps>   arpSteps {}, arpAccents {};
    std::atomic<float>* arpLenP = nullptr;
    int shownArpStep = -2, shownArpLen = -1;

    int   currentTab = 0;
    float levelOut = 0.0f;
    float lastBpm = 0.0f;
    bool  lastSync = false;

    std::unique_ptr<juce::FileChooser> chooser;   // "Importar banco de presets…"
    PresetDialog dialog;   // por último: fecha a caixa aberta antes do resto sumir

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuimeraEditor)
};
