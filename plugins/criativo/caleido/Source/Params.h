#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace caleido
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* chaos    = "chaos";
    inline constexpr const char* trip     = "trip";
    inline constexpr const char* division = "division";
    inline constexpr const char* ratchet  = "ratchet";
    inline constexpr const char* shuffle  = "shuffle";
    inline constexpr const char* reverse  = "reverse";
    inline constexpr const char* tape     = "tape";
    inline constexpr const char* filter   = "filter";
    inline constexpr const char* crush    = "crush";
    inline constexpr const char* spin     = "spin";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* mutate   = "mutate";
    inline constexpr const char* hold     = "hold";
    inline constexpr const char* bypass   = "bypass";
}

inline int crushBits (float crushPct) { return 16 - juce::roundToInt (crushPct * 0.14f); }

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

    add (id::chaos,   "Caos",                { 0.0f, 100.0f }, 62.0f, pct);
    add (id::trip,    "Viagem",              { 0.0f, 100.0f }, 48.0f, pct);
    choice (id::division, u8 ("Divisão"),    { "1/8", "1/16", "1/32" }, 1);
    choice (id::ratchet,  u8 ("Repetição"),  { u8 ("até 2×"), u8 ("até 3×"), u8 ("até 4×"), u8 ("até 6×"),
                                               u8 ("até 8×"), u8 ("até 12×"), u8 ("até 16×") }, 3);
    add (id::shuffle, "Embaralhar",          { 0.0f, 100.0f }, 30.0f, pct);
    add (id::reverse, "Reverso",             { 0.0f, 100.0f }, 35.0f, pct);
    add (id::tape,    "Fita",                { 0.0f, 24.0f, 1.0f }, 10.0f,
         [] (float v, int) { return u8 ("±") + String (roundToInt (v)) + " st"; });
    add (id::filter,  "Filter",              { 0.0f, 100.0f }, 55.0f, pct);
    add (id::crush,   "Crush",               { 0.0f, 100.0f }, 25.0f,
         [] (float v, int) { return String (crushBits (v)) + " bit"; });
    choice (id::spin,     "Giro",            { "1/4", "1/8", "1/16", "1/32" }, 2);
    add (id::output,  "Gain",                { -36.0f, 12.0f }, 0.0f,
         [] (float v, int) { return String (v, 1) + " dB"; });
    add (id::mix,     "Dry/Wet",             { 0.0f, 100.0f }, 70.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::mutate, 1 }, "Transmutar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::hold, 1 }, "Segurar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    uint32_t seed;                                       // semente do padrão da mandala
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// DIVISÃO, REPETIÇÃO e GIRO são listas: o valor é o índice (DIVISÃO 0=1/8 1=1/16 2=1/32)
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Mandala"), 417,  { { id::chaos, 62 }, { id::trip, 48 } } },
        { "Cogumelo Azul",       1984, { { id::chaos, 45 }, { id::trip, 75 }, { id::ratchet, 2 }, { id::shuffle, 15 }, { id::reverse, 20 },
                                         { id::tape, 5 }, { id::filter, 85 }, { id::crush, 10 }, { id::spin, 1 }, { id::mix, 60 } } },
        { "Fractal 145",         145,  { { id::chaos, 85 }, { id::trip, 40 }, { id::division, 2 }, { id::ratchet, 6 }, { id::shuffle, 50 },
                                         { id::reverse, 30 }, { id::tape, 7 }, { id::filter, 40 }, { id::crush, 20 }, { id::spin, 3 }, { id::mix, 75 } } },
        { "Chuva de Bits",       8080, { { id::chaos, 55 }, { id::trip, 80 }, { id::ratchet, 5 }, { id::shuffle, 25 }, { id::reverse, 25 },
                                         { id::tape, 12 }, { id::filter, 35 }, { id::crush, 85 }, { id::mix, 65 } } },
        { "Portal Reverso",      2012, { { id::chaos, 70 }, { id::trip, 50 }, { id::ratchet, 2 }, { id::shuffle, 40 }, { id::reverse, 90 },
                                         { id::tape, 12 }, { id::filter, 50 }, { id::crush, 15 }, { id::mix, 70 } } },
        { u8 ("Sopro do Xamã"),  777,  { { id::chaos, 35 }, { id::trip, 65 }, { id::division, 0 }, { id::ratchet, 1 }, { id::shuffle, 20 },
                                         { id::reverse, 45 }, { id::tape, 24 }, { id::filter, 70 }, { id::crush, 5 }, { id::spin, 0 }, { id::mix, 55 } } },
        { "Goa 1996", 1996, { { id::chaos, 50 }, { id::trip, 55 }, { id::division, 1 }, { id::ratchet, 3 }, { id::shuffle, 25 }, { id::reverse, 30 },
                                         { id::tape, 7 }, { id::filter, 60 }, { id::crush, 15 }, { id::spin, 2 }, { id::mix, 65 } } },
        { u8 ("Lagarta Elétrica"), 3141, { { id::chaos, 75 }, { id::trip, 35 }, { id::division, 2 }, { id::ratchet, 5 }, { id::shuffle, 35 }, { id::reverse, 15 },
                                         { id::tape, 3 }, { id::filter, 45 }, { id::crush, 40 }, { id::spin, 3 }, { id::mix, 70 } } },
        { u8 ("Templo de Néon"), 2600, { { id::chaos, 40 }, { id::trip, 85 }, { id::division, 1 }, { id::ratchet, 2 }, { id::shuffle, 10 }, { id::reverse, 40 },
                                         { id::tape, 12 }, { id::filter, 80 }, { id::crush, 20 }, { id::spin, 1 }, { id::mix, 60 } } },
        { u8 ("Rolê Quebrado"), 909, { { id::chaos, 90 }, { id::trip, 30 }, { id::division, 2 }, { id::ratchet, 6 }, { id::shuffle, 60 }, { id::reverse, 20 },
                                         { id::tape, 2 }, { id::filter, 30 }, { id::crush, 30 }, { id::spin, 3 }, { id::mix, 80 } } },
        { "Gota Lenta", 33, { { id::chaos, 25 }, { id::trip, 45 }, { id::division, 0 }, { id::ratchet, 1 }, { id::shuffle, 10 }, { id::reverse, 50 },
                                         { id::tape, 12 }, { id::filter, 70 }, { id::crush, 0 }, { id::spin, 0 }, { id::mix, 50 } } },
        { u8 ("Matrix Ácida"), 303, { { id::chaos, 65 }, { id::trip, 60 }, { id::division, 2 }, { id::ratchet, 4 }, { id::shuffle, 30 }, { id::reverse, 25 },
                                         { id::tape, 5 }, { id::filter, 90 }, { id::crush, 35 }, { id::spin, 2 }, { id::mix, 70 } } },
        { "Espiral Dourada", 1618, { { id::chaos, 55 }, { id::trip, 70 }, { id::division, 1 }, { id::ratchet, 3 }, { id::shuffle, 40 }, { id::reverse, 35 },
                                         { id::tape, 19 }, { id::filter, 55 }, { id::crush, 10 }, { id::spin, 1 }, { id::mix, 60 } } },
        { u8 ("Ruído Cósmico"), 4096, { { id::chaos, 80 }, { id::trip, 90 }, { id::division, 2 }, { id::ratchet, 5 }, { id::shuffle, 45 }, { id::reverse, 45 },
                                         { id::tape, 24 }, { id::filter, 40 }, { id::crush, 70 }, { id::spin, 3 }, { id::mix, 75 } } },
        { "Hipnose", 108, { { id::chaos, 30 }, { id::trip, 40 }, { id::division, 1 }, { id::ratchet, 2 }, { id::shuffle, 5 }, { id::reverse, 10 },
                                         { id::tape, 0 }, { id::filter, 65 }, { id::crush, 5 }, { id::spin, 2 }, { id::mix, 45 } } },
        { "Terremoto", 5150, { { id::chaos, 95 }, { id::trip, 50 }, { id::division, 2 }, { id::ratchet, 6 }, { id::shuffle, 70 }, { id::reverse, 50 },
                                         { id::tape, 12 }, { id::filter, 35 }, { id::crush, 50 }, { id::spin, 3 }, { id::mix, 85 } } },
        { u8 ("Fumaça Roxa"), 2701, { { id::chaos, 45 }, { id::trip, 65 }, { id::division, 1 }, { id::ratchet, 2 }, { id::shuffle, 20 }, { id::reverse, 60 },
                                         { id::tape, 10 }, { id::filter, 75 }, { id::crush, 15 }, { id::spin, 0 }, { id::mix, 55 } } },
        { "Bit Tribal", 1200, { { id::chaos, 60 }, { id::trip, 50 }, { id::division, 0 }, { id::ratchet, 3 }, { id::shuffle, 30 }, { id::reverse, 20 },
                                         { id::tape, 0 }, { id::filter, 50 }, { id::crush, 90 }, { id::spin, 1 }, { id::mix, 65 } } },
        { "Groove Sutil", 64, { { id::chaos, 20 }, { id::trip, 25 }, { id::division, 1 }, { id::ratchet, 1 }, { id::shuffle, 10 }, { id::reverse, 10 },
                                         { id::tape, 2 }, { id::filter, 60 }, { id::crush, 5 }, { id::spin, 2 }, { id::mix, 35 } } },
        { u8 ("Dimensão Paralela"), 9999, { { id::chaos, 70 }, { id::trip, 80 }, { id::division, 1 }, { id::ratchet, 4 }, { id::shuffle, 55 }, { id::reverse, 70 },
                                         { id::tape, 17 }, { id::filter, 45 }, { id::crush, 25 }, { id::spin, 1 }, { id::mix, 70 } } },
    };
}
} // namespace caleido
