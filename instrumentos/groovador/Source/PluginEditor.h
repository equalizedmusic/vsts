#pragma once

#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"
#include "GroovUi.h"

// ---------------- fileira de um instrumento: nome, modelo, LEDs de nível, A/B, mudo, solo e dado ----------------
class LaneRow : public juce::Component
{
public:
    LaneRow (GroovadorProcessor& p, int instIndex);
    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void tick();

private:
    GroovadorProcessor& proc;
    int k;
    gv::MiniButton mute { gv::MiniButton::Kind::mute }, solo { gv::MiniButton::Kind::solo }, dice { gv::MiniButton::Kind::dice };
    std::unique_ptr<APVTS::ButtonAttachment> muteAtt, soloAtt;
    float shownLevel = -1.0f, shownVar = -2.0f;
    bool shownSel = false;
    int shownModel = -1;
};

// ---------------- a grade do groove: teclas de passo em grupos de 4 cores, com LED; faixa de timbre embaixo ----------------
class GrooveView : public juce::Component
{
public:
    explicit GrooveView (GroovadorProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    bool cellAt (juce::Point<float> pos, int& inst, int& step) const;
    juce::Rectangle<float> gridArea() const { return { 10.0f, 24.0f, (float) getWidth() - 20.0f, 216.0f }; }

    GroovadorProcessor& proc;
    int dragInst = -1, dragStep = -1, dragVel = 5;
};

// ---------------- os controles de um instrumento (o editor de som mostra um de cada vez) ----------------
class InstPanel : public juce::Component
{
public:
    InstPanel (GroovadorProcessor& p, int instIndex);

private:
    std::unique_ptr<gv::LedSegmented> model;
    std::vector<std::unique_ptr<gv::GKnob>> knobs;
};

// ---------------- editor de som: LCD com a forma da batida, modelo, dado e os 10 knobs ----------------
class SoundPanel : public juce::Component
{
public:
    explicit SoundPanel (GroovadorProcessor& p);
    void paint (juce::Graphics& g) override;
    void tick();

private:
    void renderWave();

    GroovadorProcessor& proc;
    std::array<std::unique_ptr<InstPanel>, grv::numInst> panels;
    gv::RubberButton dice { {}, gv::RubberButton::Icon::dice };
    int sel = -1;
    juce::String waveKey;
    std::vector<float> wave;
    float waveMs = 300.0f;
};

// ---------------- ARRASTAR MIDI: arraste o botão para o DAW e o groove vira clipe ----------------
class MidiDragButton : public gv::RubberButton
{
public:
    explicit MidiDragButton (GroovadorProcessor& p) : gv::RubberButton ("ARRASTAR MIDI", Icon::drag), proc (p) {}
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { dragging = false; gv::RubberButton::mouseUp (e); }

private:
    GroovadorProcessor& proc;
    bool dragging = false;
};

class GroovadorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GroovadorEditor (GroovadorProcessor&);
    ~GroovadorEditor() override;

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
    void showPresetMenu();
    void presetMenuChosen (int result);
    void askNewPreset();
    void setScale (float scale);

    static constexpr int openSettle = 15;
    float openScale = 1.0f;
    int   openTicks = 0;

    GroovadorProcessor& proc;
    gv::DrumLookAndFeel lnf;
    gv::ScaledCanvas canvas;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;

    std::array<LaneRow*, grv::numInst> lanes {};
    GrooveView* groove = nullptr;
    SoundPanel* sound = nullptr;
    gv::PresetDisplay* presetName = nullptr;
    gv::RubberButton* undoBtn = nullptr;

    gv::PresetDialog dialog;

    float level = 0.0f, shownLevel = -1.0f;
    float lastBpm = 0.0f;
    int   lastState = -1, lastPreset = -2, lastSel = -1;

    isp::TelaAtivacao ativacao;   // por último: por cima de tudo enquanto o pacote não está ativado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovadorEditor)
};
