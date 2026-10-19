#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "BbdEngine.h"

namespace repeteco
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* time     = "time";
    inline constexpr const char* range    = "range";
    inline constexpr const char* sync     = "sync";
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* pingPong = "pingpong";
    inline constexpr const char* feedback = "feedback";
    inline constexpr const char* lfoRate  = "lforate";
    inline constexpr const char* lfoAmt   = "lfoamt";
    inline constexpr const char* lfoShape = "lfoshape";
    inline constexpr const char* dark     = "dark";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* infinite = "infinite";
    inline constexpr const char* kill     = "kill";
    inline constexpr const char* bypass   = "bypass";
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
    auto choice = [&layout] (const char* pid, const String& name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, items, def));
    };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };

    StringArray divs;
    for (auto* d : divNames) divs.add (d);

    add (id::time,      "Tempo",            { 0.0f, 100.0f }, 100.0f, pct);
    choice (id::range,  "Faixa",            { "Curto", "Longo" }, 0);
    choice (id::sync,   "Sync",             { "Grid", "Livre" }, 0);
    add (id::drive,     "Drive",            { 0.0f, 24.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::pingPong,  "Ping-pong",        { 0.0f, 100.0f }, 0.0f, pct);
    add (id::feedback,  u8 ("Realimentação"), { 0.0f, 120.0f }, 45.0f, pct);
    choice (id::lfoRate,  "LFO Taxa",       divs, 10);
    add (id::lfoAmt,    "LFO Quant.",       { 0.0f, 100.0f }, 15.0f, pct);
    choice (id::lfoShape, "LFO Forma",      StringArray ("Seno", u8 ("Triângulo"), "Quadrada", u8 ("Aleatória")), 0);
    add (id::dark,      "Escuro",           { 0.0f, 100.0f }, 50.0f, pct);
    add (id::output,    "Ganho",            { -36.0f, 12.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::mix,       "Mix",              { 0.0f, 100.0f }, 35.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::infinite, 1 }, "Infinito", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::kill, 1 }, "Matar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// TEMPO no grid anda por 8 passos (0, 14, 29, 43, 57, 71, 86, 100%):
//   FAIXA curta: 1/32 1/16T 1/16 1/8T 3/32 1/8 1/4T 3/16 · longa: 1/8 1/4T 3/16 1/4 3/8 1/2 3/4 1 comp
// listas guardam o índice: FAIXA 0=curto 1=longo; SYNC 0=grid 1=livre; FORMA 0=seno 1=triângulo 2=quadrada 3=aleatória;
// LFO TAXA 0=1/32 1=1/16T 2=1/16 3=1/8T 4=3/32 5=1/8 6=1/4T 7=3/16 8=1/4 9=3/8 10=1/2 11=1 comp 12=2 comp 13=4 comp
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Repeteco"),     { } },
        { u8 ("Psy 3/16 Clássico"),       { { id::time, 100 }, { id::feedback, 55 }, { id::dark, 45 }, { id::mix, 30 } } },
        { "Pingue-pongue 1/8",        { { id::time, 71 },  { id::feedback, 50 }, { id::pingPong, 100 }, { id::mix, 35 } } },
        { "Slapback",                 { { id::time, 14 },  { id::feedback, 10 }, { id::dark, 30 }, { id::mix, 30 } } },
        { "Dub na Lama",              { { id::range, 1 }, { id::time, 43 }, { id::feedback, 85 }, { id::dark, 80 }, { id::drive, 8 }, { id::mix, 40 } } },
        { u8 ("Auto-oscilação"),          { { id::time, 100 }, { id::feedback, 110 }, { id::dark, 60 }, { id::mix, 45 } } },
        { "Fita Velha",               { { id::range, 1 }, { id::time, 29 }, { id::feedback, 60 }, { id::lfoAmt, 45 }, { id::lfoRate, 11 }, { id::dark, 70 } } },
        { "Chorus de Balde",          { { id::sync, 1 }, { id::time, 0 }, { id::feedback, 0 }, { id::lfoAmt, 60 }, { id::lfoRate, 11 }, { id::mix, 50 } } },
        { u8 ("Lead no Espaço"),          { { id::range, 1 }, { id::time, 57 }, { id::feedback, 65 }, { id::pingPong, 60 }, { id::dark, 40 }, { id::mix, 35 } } },
        { "Snare Ecoando",            { { id::range, 1 }, { id::time, 43 }, { id::feedback, 40 }, { id::dark, 55 }, { id::mix, 25 } } },
        { u8 ("Vocal Distante"),      { { id::range, 1 }, { id::time, 71 }, { id::feedback, 55 }, { id::dark, 75 }, { id::pingPong, 40 }, { id::mix, 30 } } },
        { "Metralhadora 1/32",        { { id::time, 0 },   { id::feedback, 75 }, { id::dark, 35 }, { id::mix, 40 } } },
        { u8 ("Tercina Hipnótica"),       { { id::time, 43 },  { id::feedback, 60 }, { id::pingPong, 70 }, { id::mix, 35 } } },
        { "Caverna",                  { { id::range, 1 }, { id::time, 100 }, { id::feedback, 70 }, { id::dark, 90 }, { id::mix, 30 } } },
        { "Brilhante",                { { id::time, 71 },  { id::feedback, 50 }, { id::dark, 0 }, { id::mix, 30 } } },
        { "Sujeira de Pedal",         { { id::time, 100 }, { id::feedback, 60 }, { id::drive, 18 }, { id::dark, 60 }, { id::mix, 40 } } },
        { u8 ("Eco Bêbado"),              { { id::range, 1 }, { id::time, 29 }, { id::feedback, 55 }, { id::lfoAmt, 90 }, { id::lfoShape, 3 }, { id::lfoRate, 8 } } },
        { "Build-up Infinito",        { { id::time, 71 },  { id::feedback, 95 }, { id::dark, 40 }, { id::pingPong, 50 }, { id::mix, 45 } } },
        { "Pad Arrastado",            { { id::sync, 1 }, { id::range, 1 }, { id::time, 60 }, { id::feedback, 70 }, { id::dark, 65 }, { id::lfoAmt, 30 }, { id::mix, 40 } } },
        { u8 ("Último Eco"),          { { id::range, 1 }, { id::time, 100 }, { id::feedback, 80 }, { id::dark, 70 }, { id::pingPong, 100 }, { id::mix, 50 } } },
    };
}
} // namespace repeteco
