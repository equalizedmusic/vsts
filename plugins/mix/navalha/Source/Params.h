#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "ClipEngine.h"

namespace navalha
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* drive   = "drive";
    inline constexpr const char* ceiling = "ceiling";
    inline constexpr const char* knee    = "knee";
    inline constexpr const char* asym    = "asym";
    inline constexpr const char* link    = "link";
    inline constexpr const char* output  = "output";
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* delta   = "delta";
    inline constexpr const char* bypass  = "bypass";
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
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
    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto db  = [] (float v, int) { return dbText (v); };

    add (id::drive,   u8 ("Pressão"),     { 0.0f, 24.0f }, 0.0f, db);
    add (id::ceiling, "Teto",             { -12.0f, 0.0f }, -0.3f, db);
    add (id::knee,    "Suavidade",        { 0.0f, 100.0f }, 20.0f, pct);
    add (id::asym,    "Assimetria",       { 0.0f, 100.0f }, 0.0f, pct);
    add (id::link,    "Link",             { 0.0f, 100.0f }, 0.0f, pct);
    add (id::output,  "Ganho",            { -36.0f, 12.0f }, 0.0f, db);
    add (id::mix,     "Mix",              { 0.0f, 100.0f }, 100.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::delta, 1 }, u8 ("Só o corte"), false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Navalha"),      { } },
        { "Kick Afiado",              { { id::drive, 6 },  { id::ceiling, -1.0f }, { id::knee, 5 } } },
        { "Master Psy",               { { id::drive, 4 },  { id::ceiling, -0.3f }, { id::knee, 15 }, { id::link, 100 } } },
        { "Master Suave",             { { id::drive, 3 },  { id::ceiling, -0.5f }, { id::knee, 50 }, { id::link, 100 } } },
        { "Bateria Esmagada",         { { id::drive, 12 }, { id::ceiling, -3.0f }, { id::knee, 10 }, { id::mix, 70 } } },
        { "Snare na Cara",            { { id::drive, 9 },  { id::ceiling, -2.0f }, { id::knee, 0 } } },
        { "Baixo Contido",            { { id::drive, 5 },  { id::ceiling, -2.0f }, { id::knee, 35 } } },
        { "Lead Rasgado",             { { id::drive, 18 }, { id::ceiling, -6.0f }, { id::knee, 0 },  { id::asym, 40 } } },
        { u8 ("Válvula Quente"),      { { id::drive, 10 }, { id::ceiling, -3.0f }, { id::knee, 60 }, { id::asym, 70 } } },
        { "Paralelo Sujo",            { { id::drive, 20 }, { id::ceiling, -8.0f }, { id::knee, 0 },  { id::mix, 35 } } },
        { "Hi-hat Brilhante",         { { id::drive, 6 },  { id::ceiling, -4.0f }, { id::knee, 0 } } },
        { "Transiente Zero",          { { id::drive, 8 },  { id::ceiling, -1.0f }, { id::knee, 0 },  { id::link, 100 } } },
        { "Loudness Club",            { { id::drive, 7 },  { id::ceiling, -0.2f }, { id::knee, 25 }, { id::link, 100 } } },
        { "Navalha Cega",             { { id::drive, 4 },  { id::ceiling, -1.0f }, { id::knee, 100 } } },
        { "Corte Seco",               { { id::drive, 12 }, { id::ceiling, -1.0f }, { id::knee, 0 } } },
        { "Fuzz Digital",             { { id::drive, 24 }, { id::ceiling, -10.0f }, { id::knee, 0 }, { id::asym, 20 } } },
        { u8 ("Grupo de Percussão"),  { { id::drive, 6 },  { id::ceiling, -1.5f }, { id::knee, 20 }, { id::link, 50 } } },
        { "Vocal Presente",           { { id::drive, 5 },  { id::ceiling, -3.0f }, { id::knee, 70 } } },
        { "Pad Saturado",             { { id::drive, 14 }, { id::ceiling, -6.0f }, { id::knee, 80 }, { id::asym, 30 }, { id::mix, 60 } } },
        { u8 ("Limite de Segurança"), { { id::drive, 0 },  { id::ceiling, -1.0f }, { id::knee, 10 }, { id::link, 100 } } },
    };
}
} // namespace navalha
