#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Scales.h"

// LIMÃO — bassline ácido monofônico (alma de 303) com sequenciador de até 32 passos.
// Aqui ficam os ids dos parâmetros, as listas e o padrão do sequenciador.
namespace limao
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline constexpr int maxSteps = 32;

// ---------------- listas (o parâmetro guarda o ÍNDICE) ----------------
inline const juce::StringArray waveNames  { "SAW", "SQUARE" };
inline const juce::StringArray driveNames { "TUBE", "PEDAL", "BITCRUSH" };
inline const juce::StringArray rateNames  { "1/32", "1/16T", "1/16", "1/8T", "1/8" };
inline constexpr double rateBeats[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5 };
inline const juce::StringArray delayNames { "1/16", "1/8T", "1/8", "3/16", "1/4", "3/8" };
inline constexpr double delayBeats[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5 };

namespace id
{
inline const juce::String wave = "wave", tune = "tune", cutoff = "cutoff", reso = "reso", envmod = "envmod",
                          decay = "decay", accent = "accent", slide = "slide", acid = "acid",
                          drvType = "drvtype", drive = "drive",
                          dlOn = "dlon", dlDiv = "dldiv", dlFb = "dlfb", dlMix = "dlmix",
                          rvOn = "rvon", rvSize = "rvsize", rvMix = "rvmix",
                          volume = "volume",
                          seqOn = "seqon", seqLen = "seqlen", seqRate = "seqrate", swing = "swing",
                          scale = "scale", root = "root";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto ms  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; };
    auto hz  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto add = [&p] (auto* prm) { p.emplace_back (prm); };

    NormalisableRange<float> cutRange (40.0f, 12000.0f);
    cutRange.setSkewForCentre (700.0f);
    NormalisableRange<float> decRange (30.0f, 2500.0f);
    decRange.setSkewForCentre (400.0f);
    NormalisableRange<float> slideRange (10.0f, 400.0f);
    slideRange.setSkewForCentre (70.0f);

    // ---- voz ----
    add (new AudioParameterChoice ({ id::wave, 1 }, "Wave", waveNames, 0));
    add (new AudioParameterFloat ({ id::tune, 1 }, "Tune", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                  { return std::abs (v) < 0.005f ? String ("0.00 st") : (v > 0.0f ? "+" : "") + String (v, 2) + " st"; })));
    add (new AudioParameterFloat ({ id::cutoff, 1 }, "Cutoff", cutRange, 520.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    add (new AudioParameterFloat ({ id::reso, 1 }, "Resonance", NormalisableRange<float> (0.0f, 100.0f), 65.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::envmod, 1 }, "Env Mod", NormalisableRange<float> (0.0f, 100.0f), 55.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::decay, 1 }, "Decay", decRange, 380.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    add (new AudioParameterFloat ({ id::accent, 1 }, "Accent", NormalisableRange<float> (0.0f, 100.0f), 60.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::slide, 1 }, "Slide", slideRange, 60.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    add (new AudioParameterFloat ({ id::acid, 1 }, "Acid", NormalisableRange<float> (0.0f, 100.0f), 0.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    // ---- efeitos ----
    add (new AudioParameterChoice ({ id::drvType, 1 }, "Drive Type", driveNames, 0));
    add (new AudioParameterFloat ({ id::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 100.0f), 20.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterBool ({ id::dlOn, 1 }, "Delay On", false));
    add (new AudioParameterChoice ({ id::dlDiv, 1 }, "Delay Time", delayNames, 3));
    add (new AudioParameterFloat ({ id::dlFb, 1 }, "Delay Feedback", NormalisableRange<float> (0.0f, 95.0f), 45.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::dlMix, 1 }, "Delay Mix", NormalisableRange<float> (0.0f, 100.0f), 25.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterBool ({ id::rvOn, 1 }, "Reverb On", false));
    add (new AudioParameterFloat ({ id::rvSize, 1 }, "Reverb Size", NormalisableRange<float> (0.0f, 100.0f), 45.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::rvMix, 1 }, "Reverb Mix", NormalisableRange<float> (0.0f, 100.0f), 20.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::volume, 1 }, "Volume", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -6.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                  { return String (v, 1) + " dB"; })));

    // ---- sequenciador ----
    add (new AudioParameterBool ({ id::seqOn, 1 }, "Sequencer On", true));
    add (new AudioParameterInt ({ id::seqLen, 1 }, "Length", 1, maxSteps, 16));
    add (new AudioParameterChoice ({ id::seqRate, 1 }, "Rate", rateNames, 2));
    add (new AudioParameterFloat ({ id::swing, 1 }, "Swing", NormalisableRange<float> (0.0f, 75.0f), 0.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    // SCALE + ROOT: as notas dos passos tocam encaixadas na escala (CHROMATIC = como estão)
    add (new AudioParameterChoice ({ id::scale, 1 }, "Scale", scales::scaleNames, 0));
    add (new AudioParameterChoice ({ id::root, 1 }, "Root", scales::rootNames, 0));

    return { p.begin(), p.end() };
}

// ---------------- um passo do sequenciador ----------------
// pitch em semitons a partir do Dó 2 (MIDI 36), de −12 a +24; mode: pausa, nota ou ligadura (segura a anterior)
enum class Mode { rest = 0, note = 1, tie = 2 };

struct Step
{
    int  pitch  = 0;
    Mode mode   = Mode::note;
    bool accent = false;
    bool slide  = false;

    int pack() const
    {
        return (juce::jlimit (-12, 24, pitch) + 12) | ((int) mode << 6) | (accent ? 1 << 8 : 0) | (slide ? 1 << 9 : 0);
    }

    static Step unpack (int v)
    {
        Step s;
        s.pitch  = (v & 63) - 12;
        s.mode   = (Mode) juce::jlimit (0, 2, (v >> 6) & 3);
        s.accent = (v >> 8) & 1;
        s.slide  = (v >> 9) & 1;
        return s;
    }
};

// padrão compartilhado entre a tela (escreve) e o áudio (lê): um inteiro atômico por passo
struct Pattern
{
    std::array<std::atomic<int>, maxSteps> steps;

    Pattern() { clear(); }

    Step get (int i) const      { return Step::unpack (steps[(size_t) juce::jlimit (0, maxSteps - 1, i)].load()); }
    void set (int i, Step s)    { steps[(size_t) juce::jlimit (0, maxSteps - 1, i)].store (s.pack()); }

    void clear()
    {
        Step s;
        s.mode = Mode::rest;
        for (auto& st : steps) st.store (s.pack());
    }

    // texto para salvar no projeto: "p,m,a,s;" por passo
    juce::String toString() const
    {
        juce::String out;
        for (int i = 0; i < maxSteps; ++i)
        {
            const auto s = get (i);
            out << s.pitch << "," << (int) s.mode << "," << (s.accent ? 1 : 0) << "," << (s.slide ? 1 : 0) << ";";
        }
        return out;
    }

    void fromString (const juce::String& text)
    {
        clear();
        auto items = juce::StringArray::fromTokens (text, ";", "");
        for (int i = 0; i < juce::jmin (maxSteps, items.size()); ++i)
        {
            auto f = juce::StringArray::fromTokens (items[i], ",", "");
            if (f.size() < 4) continue;
            Step s;
            s.pitch  = juce::jlimit (-12, 24, f[0].getIntValue());
            s.mode   = (Mode) juce::jlimit (0, 2, f[1].getIntValue());
            s.accent = f[2].getIntValue() != 0;
            s.slide  = f[3].getIntValue() != 0;
            set (i, s);
        }
    }
};

// nota que o passo toca de verdade com SCALE + ROOT (pitch relativo ao Dó 2, como no Step)
inline int scaledPitch (int pitch, int scale, int root)
{
    return scales::snap (36 + pitch, scale, root) - 36;
}

inline juce::String noteName (int pitch)
{
    static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int midi = 36 + pitch;
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}
} // namespace limao
