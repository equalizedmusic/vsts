#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "ReverbEngine.h"

namespace miragem
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* decay    = "decay";
    inline constexpr const char* size     = "size";
    inline constexpr const char* predelay = "predelay";
    inline constexpr const char* dark     = "dark";
    inline constexpr const char* lowCut   = "lowcut";
    inline constexpr const char* shimmer  = "shimmer";
    inline constexpr const char* interval = "interval";
    inline constexpr const char* motion   = "motion";
    inline constexpr const char* duck     = "duck";
    inline constexpr const char* revLen   = "revlen";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* freeze   = "freeze";
    inline constexpr const char* reverse  = "reverse";
    inline constexpr const char* bypass   = "bypass";
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String secText (float v)
{
    return v < 10.0f ? juce::String (v, 1).replaceCharacter ('.', ',') + " s" : juce::String (juce::roundToInt (v)) + " s";
}

inline juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 1).replaceCharacter ('.', ',') + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
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
    auto choice = [&layout] (const char* pid, const String& name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, items, def));
    };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    StringArray intervals, lens;
    for (auto* n : intervalNames) intervals.add (u8 (n));
    for (auto* n : revNames) lens.add (n);

    add (id::decay,     "Decay",            skewed (0.3f, 30.0f, 4.0f), 4.5f, [] (float v, int) { return secText (v); });
    add (id::size,      "Size",             { 0.0f, 100.0f }, 70.0f, pct);
    add (id::predelay,  "Pre-delay",        { 0.0f, 250.0f }, 20.0f, [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    add (id::dark,      "Damping",          { 0.0f, 100.0f }, 40.0f, pct);
    add (id::lowCut,    "Low cut",          skewed (20.0f, 1000.0f, 150.0f), 120.0f, [] (float v, int) { return hzText (v); });
    add (id::shimmer,   "Shimmer",          { 0.0f, 100.0f }, 35.0f, pct);
    choice (id::interval, "Interval",       intervals, 0);
    add (id::motion,    "Modulation",       { 0.0f, 100.0f }, 35.0f, pct);
    add (id::duck,      "Duck",             { 0.0f, 100.0f }, 0.0f, pct);
    choice (id::revLen, "Janela Rev.",      lens, 1);
    add (id::output,    "Gain",             { -36.0f, 12.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::mix,       "Dry/Wet",          { 0.0f, 100.0f }, 30.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::freeze, 1 }, "Congelar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::reverse, 1 }, "Reverso", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// listas guardam o índice: TOM 0=oitava acima 1=quinta 2=oitava+quinta 3=oitava abaixo;
// JANELA REV. 0=1/4 1=1/2 2=1 comp 3=2 comp
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Miragem"),        { } },
        { u8 ("Catedral de Luz"),       { { id::decay, 9 }, { id::size, 90 }, { id::shimmer, 60 }, { id::dark, 25 }, { id::mix, 40 } } },
        { "Calor no Asfalto",           { { id::decay, 5 }, { id::size, 65 }, { id::shimmer, 20 }, { id::motion, 85 }, { id::dark, 45 } } },
        { u8 ("Sala Pequena"),          { { id::decay, 0.8f }, { id::size, 20 }, { id::shimmer, 0 }, { id::predelay, 5 }, { id::motion, 15 }, { id::mix, 20 } } },
        { "Placa Brilhante",            { { id::decay, 2.2f }, { id::size, 45 }, { id::shimmer, 0 }, { id::dark, 10 }, { id::predelay, 15 }, { id::mix, 25 } } },
        { u8 ("Quinta no Céu"),         { { id::decay, 8 }, { id::size, 85 }, { id::shimmer, 55 }, { id::interval, 1 }, { id::dark, 30 }, { id::mix, 40 } } },
        { u8 ("Órgão de Vidro"),        { { id::decay, 12 }, { id::size, 95 }, { id::shimmer, 75 }, { id::interval, 2 }, { id::dark, 20 }, { id::mix, 45 } } },
        { u8 ("Porão Escuro"),          { { id::decay, 6 }, { id::size, 80 }, { id::shimmer, 40 }, { id::interval, 3 }, { id::dark, 80 }, { id::lowCut, 40 } } },
        { "Vocal Etéreo",               { { id::decay, 4 }, { id::size, 70 }, { id::shimmer, 30 }, { id::predelay, 80 }, { id::duck, 60 }, { id::lowCut, 250 }, { id::mix, 30 } } },
        { "Lead que Respira",           { { id::decay, 3.5f }, { id::size, 60 }, { id::shimmer, 25 }, { id::duck, 80 }, { id::predelay, 40 }, { id::mix, 35 } } },
        { "Pad Infinito",               { { id::decay, 25 }, { id::size, 100 }, { id::shimmer, 45 }, { id::motion, 60 }, { id::dark, 35 }, { id::mix, 55 } } },
        { "Snare no Deserto",           { { id::decay, 2.8f }, { id::size, 75 }, { id::shimmer, 10 }, { id::predelay, 30 }, { id::lowCut, 300 }, { id::mix, 25 } } },
        { "Reverso 1/2",                { { id::decay, 5 }, { id::size, 80 }, { id::shimmer, 20 }, { id::revLen, 1 }, { id::mix, 45 } } },
        { "Reverso Longo",              { { id::decay, 9 }, { id::size, 95 }, { id::shimmer, 35 }, { id::revLen, 2 }, { id::mix, 50 } } },
        { u8 ("Nuvem Grave"),           { { id::decay, 7 }, { id::size, 85 }, { id::shimmer, 50 }, { id::interval, 3 }, { id::dark, 60 }, { id::lowCut, 20 }, { id::mix, 35 } } },
        { "Brilho de Fundo",            { { id::decay, 6 }, { id::size, 70 }, { id::shimmer, 40 }, { id::duck, 100 }, { id::lowCut, 400 }, { id::mix, 20 } } },
        { u8 ("Ambiência de Mix"),      { { id::decay, 1.4f }, { id::size, 40 }, { id::shimmer, 0 }, { id::motion, 20 }, { id::dark, 50 }, { id::lowCut, 200 }, { id::mix, 15 } } },
        { "Miragem Total",              { { id::decay, 15 }, { id::size, 100 }, { id::shimmer, 85 }, { id::interval, 2 }, { id::motion, 70 }, { id::mix, 60 } } },
        { "Fantasma",                   { { id::decay, 10 }, { id::size, 90 }, { id::shimmer, 30 }, { id::predelay, 200 }, { id::dark, 55 }, { id::mix, 40 } } },
        { "Chuva de Prata",             { { id::decay, 4 }, { id::size, 55 }, { id::shimmer, 65 }, { id::motion, 50 }, { id::dark, 15 }, { id::lowCut, 500 }, { id::mix, 35 } } },
    };
}
} // namespace miragem
