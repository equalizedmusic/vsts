#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "DeEsserEngine.h"

namespace lixa
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* lo      = "lo";
    inline constexpr const char* hi      = "hi";
    inline constexpr const char* thresh  = "thresh";
    inline constexpr const char* range   = "range";
    inline constexpr const char* attack  = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* link    = "link";
    inline constexpr const char* wide    = "wide";
    inline constexpr const char* output  = "output";
    inline constexpr const char* delta   = "delta";
    inline constexpr const char* bypass  = "bypass";
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String hzText (float v)
{
    if (v >= 1000.0f)
        return juce::String (v / 1000.0f, 1).replaceCharacter ('.', ',') + " kHz";
    return juce::String (juce::roundToInt (v)) + " Hz";
}

inline juce::String msText (float v)
{
    if (v < 10.0f)
        return juce::String (v, 1).replaceCharacter ('.', ',') + " ms";
    return juce::String (juce::roundToInt (v)) + " ms";
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
    auto pct  = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto db   = [] (float v, int) { return dbText (v); };
    auto red  = [] (float v, int) { return String (v, 1).replaceCharacter ('.', ',') + " dB"; };
    auto hz   = [] (float v, int) { return hzText (v); };
    auto ms   = [] (float v, int) { return msText (v); };

    NormalisableRange<float> loRange (minLo, maxLo);
    loRange.setSkewForCentre (4000.0f);
    NormalisableRange<float> hiRange (minHi, maxHi);
    hiRange.setSkewForCentre (8000.0f);
    NormalisableRange<float> atkRange (0.1f, 10.0f);
    atkRange.setSkewForCentre (1.5f);
    NormalisableRange<float> relRange (5.0f, 200.0f);
    relRange.setSkewForCentre (50.0f);

    add (id::lo,      "Freq. baixa",           loRange,           4000.0f,  hz);
    add (id::hi,      "Freq. alta",            hiRange,           10000.0f, hz);
    add (id::thresh,  "Limiar",                { -60.0f, 0.0f },  -24.0f,   db);
    add (id::range,   u8 ("Redução máxima"),   { 0.0f, 24.0f },   8.0f,     red);
    add (id::attack,  "Ataque",                atkRange,          1.0f,     ms);
    add (id::release, "Soltura",               relRange,          40.0f,    ms);
    add (id::link,    "Link",                  { 0.0f, 100.0f },  100.0f,   pct);
    add (id::output,  "Ganho",                 { -12.0f, 12.0f }, 0.0f,     db);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::wide, 1 }, "Banda larga", false));
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
        { u8 ("Init · Lixa"),            { } },
        { "Voz Masculina",               { { id::lo, 3500.0f }, { id::hi, 9000.0f },  { id::thresh, -26.0f }, { id::range, 6.0f } } },
        { "Voz Feminina",                { { id::lo, 5000.0f }, { id::hi, 11000.0f }, { id::thresh, -26.0f }, { id::range, 6.0f } } },
        { "Rap na Cara",                 { { id::lo, 4500.0f }, { id::hi, 10000.0f }, { id::thresh, -30.0f }, { id::range, 9.0f },
                                           { id::attack, 0.5f }, { id::release, 30.0f } } },
        { "Trap Vocal",                  { { id::lo, 5500.0f }, { id::hi, 12000.0f }, { id::thresh, -30.0f }, { id::range, 10.0f },
                                           { id::attack, 0.3f }, { id::release, 35.0f } } },
        { "Backing Vocal Recuado",       { { id::lo, 4500.0f }, { id::hi, 12000.0f }, { id::thresh, -34.0f }, { id::range, 12.0f },
                                           { id::release, 60.0f } } },
        { "Coro de Vozes",               { { id::lo, 5000.0f }, { id::hi, 12000.0f }, { id::thresh, -30.0f }, { id::range, 8.0f },
                                           { id::release, 70.0f } } },
        { u8 ("Locução e Podcast"),      { { id::lo, 4000.0f }, { id::hi, 9000.0f },  { id::thresh, -28.0f }, { id::range, 6.0f },
                                           { id::wide, 1.0f } } },
        { "Voz Sussurrada",              { { id::lo, 4000.0f }, { id::hi, 12000.0f }, { id::thresh, -40.0f }, { id::range, 8.0f } } },
        { u8 ("Sibilância Leve"),        { { id::lo, 5000.0f }, { id::hi, 10000.0f }, { id::thresh, -22.0f }, { id::range, 3.0f } } },
        { u8 ("Sibilância Pesada"),      { { id::lo, 4000.0f }, { id::hi, 12000.0f }, { id::thresh, -36.0f }, { id::range, 14.0f },
                                           { id::attack, 0.3f } } },
        { "Banda Larga Natural",         { { id::lo, 4500.0f }, { id::hi, 10000.0f }, { id::thresh, -28.0f }, { id::range, 5.0f },
                                           { id::wide, 1.0f }, { id::release, 60.0f } } },
        { u8 ("Chimbal Áspero"),         { { id::lo, 6000.0f }, { id::hi, 14000.0f }, { id::thresh, -30.0f }, { id::range, 8.0f },
                                           { id::attack, 0.2f }, { id::release, 25.0f }, { id::link, 50.0f } } },
        { "Pratos Domados",              { { id::lo, 7000.0f }, { id::hi, 16000.0f }, { id::thresh, -32.0f }, { id::range, 9.0f },
                                           { id::release, 80.0f } } },
        { "Overhead Suave",              { { id::lo, 6000.0f }, { id::hi, 15000.0f }, { id::thresh, -28.0f }, { id::range, 4.0f },
                                           { id::release, 100.0f } } },
        { u8 ("Violão sem Palhetada"),   { { id::lo, 3000.0f }, { id::hi, 7000.0f },  { id::thresh, -30.0f }, { id::range, 5.0f },
                                           { id::release, 50.0f } } },
        { "Sax e Metais",                { { id::lo, 2500.0f }, { id::hi, 6000.0f },  { id::thresh, -26.0f }, { id::range, 5.0f },
                                           { id::release, 60.0f } } },
        { "Synth Estridente",            { { id::lo, 3000.0f }, { id::hi, 8000.0f },  { id::thresh, -28.0f }, { id::range, 7.0f },
                                           { id::link, 100.0f } } },
        { "Master Brilhante",            { { id::lo, 6000.0f }, { id::hi, 16000.0f }, { id::thresh, -20.0f }, { id::range, 3.0f },
                                           { id::release, 120.0f } } },
        { "Lixa Grossa",                 { { id::lo, 2000.0f }, { id::hi, 16000.0f }, { id::thresh, -36.0f }, { id::range, 18.0f },
                                           { id::attack, 0.1f }, { id::release, 20.0f } } },
    };
}
} // namespace lixa
