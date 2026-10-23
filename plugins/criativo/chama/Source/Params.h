#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "FlameEngine.h"

namespace chama
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* chama  = "chama";
    inline constexpr const char* noise  = "noise";
    inline constexpr const char* space  = "space";
    inline constexpr const char* output = "output";
    inline constexpr const char* drop   = "drop";
    inline constexpr const char* bypass = "bypass";
}

inline juce::String hzText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1).replaceCharacter ('.', ',') + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
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

    add (id::chama,  "Chama",          { 0.0f, 100.0f }, 0.0f, pct);
    add (id::noise,  "Noise",     { 0.0f, 100.0f }, 50.0f, pct);
    add (id::space,  u8 ("Espaço"),    { 0.0f, 100.0f }, 50.0f, pct);
    add (id::output, "Gain",           { -36.0f, 12.0f }, 0.0f,
         [] (float v, int) { return String (v, 1) + " dB"; });

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::drop, 1 }, "Drop", false));
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
        { u8 ("Init · Chama"),       { } },
        { u8 ("Faísca"),             { { id::chama, 15 },  { id::noise, 40 },  { id::space, 30 } } },
        { "Brasa",                   { { id::chama, 30 },  { id::noise, 50 },  { id::space, 40 } } },
        { "Fogueira",                { { id::chama, 45 },  { id::noise, 55 },  { id::space, 50 } } },
        { "Labareda",                { { id::chama, 60 },  { id::noise, 60 },  { id::space, 55 } } },
        { u8 ("Incêndio"),           { { id::chama, 80 },  { id::noise, 70 },  { id::space, 60 } } },
        { "Inferno",                 { { id::chama, 100 }, { id::noise, 80 },  { id::space, 70 } } },
        { "Vento Quente",            { { id::chama, 40 },  { id::noise, 90 },  { id::space, 20 } } },
        { u8 ("Fumaça Densa"),       { { id::chama, 50 },  { id::noise, 20 },  { id::space, 100 } } },
        { u8 ("Pré-Drop"),           { { id::chama, 85 },  { id::noise, 60 },  { id::space, 40 } } },
        { "Subida Limpa",            { { id::chama, 50 },  { id::noise, 0 },   { id::space, 30 } } },
        { "Turbina",                 { { id::chama, 70 },  { id::noise, 100 }, { id::space, 30 } } },
        { "Catedral em Chamas",      { { id::chama, 65 },  { id::noise, 30 },  { id::space, 100 } } },
        { "Pavio Curto",             { { id::chama, 25 },  { id::noise, 70 },  { id::space, 10 } } },
        { u8 ("Vulcão"),             { { id::chama, 90 },  { id::noise, 90 },  { id::space, 90 } } },
        { u8 ("Sopro do Dragão"),    { { id::chama, 75 },  { id::noise, 85 },  { id::space, 50 } } },
        { "Calor do Palco",          { { id::chama, 35 },  { id::noise, 30 },  { id::space, 60 } } },
        { "Meteoro",                 { { id::chama, 95 },  { id::noise, 100 }, { id::space, 40 } } },
        { u8 ("Fênix"),              { { id::chama, 55 },  { id::noise, 45 },  { id::space, 85 } } },
        { u8 ("Combustão"),          { { id::chama, 100 }, { id::noise, 100 }, { id::space, 100 } } },
    };
}
} // namespace chama
