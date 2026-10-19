#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "TransientEngine.h"

namespace soco
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* attack  = "attack";
    inline constexpr const char* sustain = "sustain";
    inline constexpr const char* focus   = "focus";
    inline constexpr const char* detMode = "detmode";
    inline constexpr const char* detFreq = "detfreq";
    inline constexpr const char* clip    = "clip";
    inline constexpr const char* output  = "output";
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* delta   = "delta";
    inline constexpr const char* bypass  = "bypass";
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

// −100..+100 com sinal ("+40%", "−25%", "0%")
inline juce::String signedPct (float v)
{
    const int p = juce::roundToInt (v);
    if (p > 0) return "+" + juce::String (p) + "%";
    if (p < 0) return u8 ("−") + juce::String (-p) + "%";
    return "0%";
}

inline juce::String msText (float v)
{
    return v < 10.0f ? juce::String (v, 1).replaceCharacter ('.', ',') + " ms" : juce::String (juce::roundToInt (v)) + " ms";
}

inline juce::String hzText (float v)
{
    if (v < 1000.0f) return juce::String (juce::roundToInt (v)) + " Hz";
    return juce::String (v / 1000.0f, 1).replaceCharacter ('.', ',') + " kHz";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&layout] (const char* pid, const String& name, NormalisableRange<float> range, float def,
                          std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto sig = [] (float v, int) { return signedPct (v); };

    StringArray modes;
    for (auto* m : detModeNames) modes.add (m);

    add (id::attack,  "Ataque",   { -100.0f, 100.0f }, 0.0f, sig);
    add (id::sustain, "Sustain",  { -100.0f, 100.0f }, 0.0f, sig);
    add (id::focus,   "Foco",     skewed (2.0f, 50.0f, 12.0f), 12.0f, [] (float v, int) { return msText (v); });
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::detMode, 1 }, "Detector", modes, 0));
    add (id::detFreq, "Corte do detector", skewed (40.0f, 8000.0f, 600.0f), 200.0f, [] (float v, int) { return hzText (v); });
    add (id::output,  "Ganho",    { -36.0f, 12.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::mix,     "Mix",      { 0.0f, 100.0f }, 100.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::clip, 1 }, "Clipper", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::delta, 1 }, u8 ("Ouvir diferença"), false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// DETECTOR guarda o índice: 0 = Tudo, 1 = Graves (passa-baixa no corte), 2 = Agudos (passa-alta no corte)
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Soco"),          { } },
        { "Bumbo na Cara",             { { id::attack, 60 },  { id::sustain, -20 }, { id::focus, 10 }, { id::detMode, 1 }, { id::detFreq, 150 } } },
        { "Bumbo Curto",               { { id::attack, 30 },  { id::sustain, -60 }, { id::focus, 15 }, { id::detMode, 1 }, { id::detFreq, 150 } } },
        { "Bumbo Gordo",               { { id::attack, 20 },  { id::sustain, 40 },  { id::focus, 20 }, { id::detMode, 1 }, { id::detFreq, 120 } } },
        { "Caixa Estalada",            { { id::attack, 70 },  { id::sustain, -10 }, { id::focus, 8 } } },
        { "Caixa Seca",                { { id::attack, 20 },  { id::sustain, -70 }, { id::focus, 12 } } },
        { "Caixa com Corpo",           { { id::attack, 15 },  { id::sustain, 45 },  { id::focus, 25 } } },
        { "Bateria Socada",            { { id::attack, 40 },  { id::sustain, -10 }, { id::focus, 12 }, { id::mix, 70 } } },
        { "Sala Aberta",               { { id::attack, -10 }, { id::sustain, 60 },  { id::focus, 20 }, { id::mix, 80 } } },
        { "Overheads Domados",         { { id::attack, -40 }, { id::focus, 6 },     { id::detMode, 2 }, { id::detFreq, 3000 } } },
        { "Chimbal Afiado",            { { id::attack, 50 },  { id::sustain, -30 }, { id::focus, 4 },  { id::detMode, 2 }, { id::detFreq, 4000 } } },
        { u8 ("Violão com Ataque"),    { { id::attack, 35 },  { id::sustain, -15 }, { id::focus, 8 },  { id::detMode, 2 }, { id::detFreq, 2000 } } },
        { u8 ("Violão Macio"),         { { id::attack, -45 }, { id::sustain, 20 },  { id::focus, 6 } } },
        { "Baixo com Ataque",          { { id::attack, 45 },  { id::sustain, -10 }, { id::focus, 10 }, { id::detMode, 2 }, { id::detFreq, 800 } } },
        { "Baixo Contido",             { { id::attack, -35 }, { id::sustain, 15 },  { id::focus, 15 }, { id::detMode, 1 }, { id::detFreq, 200 } } },
        { "Loop Mais Punch",           { { id::attack, 50 },  { id::sustain, -20 }, { id::focus, 10 }, { id::mix, 80 } } },
        { "Loop Sem Cauda",            { { id::attack, 10 },  { id::sustain, -80 }, { id::focus, 15 } } },
        { "Piano Percussivo",          { { id::attack, 40 },  { id::sustain, -20 }, { id::focus, 10 } } },
        { "Vocal Sem Estalo",          { { id::attack, -30 }, { id::focus, 4 },     { id::detMode, 2 }, { id::detFreq, 2500 } } },
        { "Palmas e Estalos",          { { id::attack, 60 },  { id::sustain, 30 },  { id::focus, 6 },  { id::detMode, 2 }, { id::detFreq, 1500 } } },
    };
}
} // namespace soco
