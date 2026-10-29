#pragma once

#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"
#include "ResenhaUi.h"

// ---------------- cartão de um timbre: nome, papel na conversa, trava, dado, forma de onda e os 11 knobs ----------------
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

private:
    void drawWave (juce::Graphics& g, rs::RF r);

    ResenhaProcessor& proc;
    int v;
    juce::Colour colour;
    float talk = 0.0f;
    bool silent = false;
    juce::String role;
    rs::LockButton lock;
    rs::NeonButton dice { {}, rs::NeonButton::Icon::dice };
    std::unique_ptr<rs::ChoiceBox> wave;
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

    float level = 0.0f, shownLevel = -1.0f;
    float lastBpm = 0.0f;
    int   lastState = -1;

    isp::TelaAtivacao ativacao;   // por último: por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResenhaEditor)
};
