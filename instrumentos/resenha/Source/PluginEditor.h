#pragma once

#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"
#include "ResenhaUi.h"

// ---------------- botão pequeno de texto (ABRIR, TROCAR, BYP, INTERNO|PLUGIN) ----------------
class PillButton : public juce::Button
{
public:
    PillButton (const juce::String& text, juce::Colour c) : juce::Button (text), colour (c) {}
    bool lit = false;          // aceso (BYP ligado, lado escolhido da chave)
    bool filled = false;       // aceso com fundo cheio na cor
    void paintButton (juce::Graphics& g, bool over, bool down) override;
private:
    juce::Colour colour;
};

// ---------------- knob ligado a um parâmetro do plugin (+ LIGAR) ----------------
class MacroKnob : public juce::Component
{
public:
    MacroKnob (ResenhaProcessor& p, int voiceIndex, int knobIndex, juce::Colour c);
    void refresh();
    void paint (juce::Graphics& g) override;
    void resized() override { knob.setBounds (getLocalBounds()); }
    void mouseDown (const juce::MouseEvent& e) override;
private:
    void showMenu();
    ResenhaProcessor& proc;
    int v, k;
    juce::Colour colour;
    rs::NeonKnob knob;
    int shownIdx = -2;
    const void* shownInst = nullptr;   // plugin trocado: o texto do knob aponta para o parâmetro do plugin novo
    bool learning = false;
};

// ---------------- slot PLUGIN: o plugin, o preset dele, ABRIR / TROCAR / BYP, knobs ligados e os ajustes na conversa ----------------
class PluginPanel : public juce::Component
{
public:
    PluginPanel (ResenhaProcessor& p, int voiceIndex, juce::Colour c);
    std::function<void()> onChoose;
    void refresh();
    void paint (juce::Graphics& g) override;
private:
    ResenhaProcessor& proc;
    int v;
    juce::Colour colour;
    PillButton openBtn { "ABRIR", juce::Colours::white }, swapBtn { "TROCAR", juce::Colours::white }, bypBtn { "BYP", rs::amber };
    std::vector<std::unique_ptr<MacroKnob>> macros;
    std::vector<std::unique_ptr<rs::NeonKnob>> knobs;
    juce::String shownName, shownMaker, shownPreset;
    float shownCpu = -1.0f;
};

// ---------------- cartão de um slot: nome, papel na conversa, trava, dado, INTERNO|PLUGIN e o conteúdo de cada um ----------------
class VoiceCard : public juce::Component
{
public:
    VoiceCard (ResenhaProcessor& p, int voiceIndex);
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override {}

    void setTalk (float level);          // acende a borda enquanto o timbre fala
    void setSilent (bool silent);        // fora da conversa (VOZES menor que o timbre): apagado
    void setRole (const juce::String& r) { if (r != role) { role = r; repaint (0, 0, getWidth(), 36); } }
    void refresh();                      // 30 Hz: INTERNO x PLUGIN e o cartão do plugin
    std::function<void (int)> onChoosePlugin;

private:
    void showSource (bool plugin);
    PillButton srcInt { "INTERNO", juce::Colours::white }, srcPlug { "PLUGIN", juce::Colours::white };
    std::unique_ptr<PluginPanel> panel;
    bool shownPlugin = false;

    void drawWave (juce::Graphics& g, rs::RF r);
    void drawVa (juce::Graphics& g, rs::RF r);
    void applyEngine();                  // FM x VA: os knobs da 1ª fileira e o UNISON mudam de nome e de texto
    bool isVa() const;

    ResenhaProcessor& proc;
    int v;
    juce::Colour colour;
    float talk = 0.0f;
    bool silent = false;
    juce::String role;
    rs::LockButton lock;
    rs::NeonButton dice { {}, rs::NeonButton::Icon::dice };
    std::unique_ptr<rs::ChoiceBox> wave, engine;
    std::vector<std::unique_ptr<rs::NeonKnob>> knobs;
    std::unique_ptr<APVTS::ButtonAttachment> lockAtt;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> watch;   // o desenho da onda segue os knobs
};

// ---------------- a conversa: piano-roll da frase colorido por timbre, com a faixa de quem fala ----------------
class ConversationView : public juce::Component
{
public:
    explicit ConversationView (ResenhaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    ResenhaProcessor& proc;
    resenha::Phrase phrase;
};

// ---------------- ARRASTAR MIDI: arraste o botão para o DAW e a frase vira clipe ----------------
class MidiDragButton : public rs::NeonButton
{
public:
    explicit MidiDragButton (ResenhaProcessor& p) : rs::NeonButton ("ARRASTAR MIDI", Icon::drag, rs::lime), proc (p) {}
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { dragging = false; rs::NeonButton::mouseUp (e); }

private:
    ResenhaProcessor& proc;
    bool dragging = false;
};

// ---------------- cadeado do TIMBRES: trava (ou destrava) os 4 timbres de uma vez ----------------
class LockAllButton : public juce::Button
{
public:
    explicit LockAllButton (ResenhaProcessor& p);
    void paintButton (juce::Graphics& g, bool over, bool down) override;
    bool allLocked() const;

private:
    ResenhaProcessor& proc;
};

// ---------------- ESCOLHER PLUGIN: a lista dos plugins instalados, por cima da tela ----------------
class PluginPicker : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit PluginPicker (ResenhaProcessor& p);
    void open (int voiceIndex);
    void close();
    std::function<void (const juce::String& title, const juce::String& text)> onError;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    struct Row { juce::PluginDescription d; bool blocked = false; juce::String file; };
    int  getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void timerCallback() override;
    void rebuild();
    void load();

    ResenhaProcessor& proc;
    int v = 0;
    juce::Rectangle<int> panelArea { 220, 96, 760, 560 };
    juce::TextEditor search;
    PillButton instBtn { "INSTRUMENTOS", rs::lime }, allBtn { "TODOS", rs::lime },
               favBtn { juce::String::fromUTF8 ("★ FAVORITOS"), rs::lime }, azBtn { "A-Z", rs::lime };
    rs::NeonButton rescanBtn { "BUSCAR PLUGINS DE NOVO" }, cancelBtn { "CANCELAR" }, loadBtn { "CARREGAR", rs::NeonButton::Icon::none, rs::lime };
    juce::ListBox listBox;
    std::vector<Row> rows;
    bool instrumentsOnly = true, favFirst = true;
    int seenVersion = -1;
};

class ResenhaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ResenhaEditor (ResenhaProcessor&);
    ~ResenhaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    template <typename T, typename... Args>
    T* add (juce::Rectangle<int> bounds, Args&&... args)
    {
        auto owner = std::make_unique<T> (std::forward<Args> (args)...);
        T* c = owner.get();
        canvas.addAndMakeVisible (*c);
        c->setBounds (bounds);
        owned.push_back (std::move (owner));
        return c;
    }

    void attach (juce::Button& b, const juce::String& paramId);
    void timerCallback() override;
    void drawCanvas (juce::Graphics&);
    void drawHeader (juce::Graphics&);
    void drawMeter (juce::Graphics&);
    void showPresetMenu();
    void presetMenuChosen (int result);
    void askNewPreset();
    void importBank();
    void setScale (float scale);

    static constexpr int openSettle = 15;
    float openScale = 1.0f;
    int   openTicks = 0;

    ResenhaProcessor& proc;
    rs::NeonLookAndFeel lnf;
    ScaledCanvas canvas;     // tudo em 1200×740, ampliado junto
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;

    std::array<VoiceCard*, resenha::numVoices> cards {};
    ConversationView* conv = nullptr;
    rs::NeonPresetName* presetName = nullptr;
    rs::NeonButton* undoBtn = nullptr;
    LockAllButton* lockAll = nullptr;

    PresetDialog dialog;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<PluginPicker> picker;

    float level = 0.0f, shownLevel = -1.0f;
    float lastBpm = 0.0f;
    int   lastState = -1;

    isp::TelaAtivacao ativacao;   // por último: por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResenhaEditor)
};
