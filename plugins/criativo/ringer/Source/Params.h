#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "RingEngine.h"

namespace ringer
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* freq     = "freq";
    inline constexpr const char* range    = "range";
    inline constexpr const char* snap     = "snap";
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* stereo   = "stereo";
    inline constexpr const char* lfoAmt   = "lfoamt";
    inline constexpr const char* lfoRate  = "lforate";
    inline constexpr const char* lfoShape = "lfoshape";
    inline constexpr const char* lfoSync  = "lfosync";
    inline constexpr const char* lfoPhase = "lfophase";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* twice    = "twice";
    inline constexpr const char* bypass   = "bypass";
}

inline juce::String hzText (float hz)
{
    if (hz >= 1000.0f) return juce::String (hz / 1000.0f, 2).replaceCharacter ('.', ',') + " kHz";
    if (hz < 10.0f)    return juce::String (hz, 1).replaceCharacter ('.', ',') + " Hz";
    return juce::String (juce::roundToInt (hz)) + " Hz";
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

    add (id::freq,      "Frequency",        { 0.0f, 100.0f }, 58.0f, pct);
    choice (id::range,  "Range",            { "Low", "High" }, 1);
    choice (id::snap,   "Afinar",           StringArray ("Livre", u8 ("Semitom")), 0);
    add (id::drive,     "Drive",            { 0.0f, 24.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::stereo,    "Stereo",           { 0.0f, 100.0f }, 25.0f, pct);
    add (id::lfoAmt,    "LFO Amount",       { 0.0f, 100.0f }, 30.0f, pct);
    choice (id::lfoRate,  "LFO Rate",       divs, 8);
    choice (id::lfoShape, "LFO Shape",      StringArray ("Sine", "Triangle", "Square", "Ramp", "Random"), 0);
    choice (id::lfoSync,  "LFO Sync",       { "Grid", "Livre" }, 0);
    add (id::lfoPhase,  "LFO Phase",        { 0.0f, 360.0f }, 0.0f,
         [] (float v, int) { return String (roundToInt (v)) + juce::String::fromUTF8 ("°"); });
    add (id::output,    "Gain",             { -36.0f, 12.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::mix,       "Dry/Wet",             { 0.0f, 100.0f }, 70.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::twice, 1 }, "Dobrar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// listas guardam o índice: FAIXA 0=baixa 1=alta; AFINAR 0=livre 1=semitom; SYNC 0=grid 1=livre;
// FORMA 0=seno 1=triângulo 2=quadrada 3=rampa 4=aleatória;
// TAXA 0=1/32 1=1/16T 2=1/16 3=1/8T 4=3/32 5=1/8 6=1/4T 7=3/16 8=1/4 9=3/8 10=1/2 11=1 comp 12=2 comp 13=4 comp
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Ringer"),          { } },
        { "Sino de Templo",          { { id::freq, 62 }, { id::lfoAmt, 0 },  { id::snap, 1 }, { id::mix, 55 } } },
        { u8 ("Robô Xamã"),          { { id::freq, 45 }, { id::lfoAmt, 15 }, { id::lfoRate, 5 }, { id::drive, 6 } } },
        { "Tremolo 1/16",            { { id::range, 0 }, { id::freq, 70 }, { id::lfoAmt, 0 },  { id::mix, 100 } } },
        { "Sirene de Rave",          { { id::freq, 55 }, { id::lfoAmt, 60 }, { id::lfoRate, 10 }, { id::lfoShape, 1 } } },
        { "Laser 3/16",              { { id::freq, 75 }, { id::lfoAmt, 80 }, { id::lfoRate, 7 },  { id::lfoShape, 3 }, { id::mix, 60 } } },
        { "Borbulha",                { { id::freq, 50 }, { id::lfoAmt, 45 }, { id::lfoRate, 2 },  { id::lfoShape, 4 } } },
        { u8 ("Metal Líquido"),      { { id::freq, 80 }, { id::lfoAmt, 20 }, { id::lfoRate, 11 }, { id::stereo, 70 } } },
        { "Baixo Sujo",              { { id::freq, 25 }, { id::lfoAmt, 0 },  { id::drive, 12 }, { id::mix, 35 } } },
        { "Hi-hat de Lata",          { { id::freq, 92 }, { id::lfoAmt, 10 }, { id::lfoRate, 2 },  { id::mix, 50 } } },
        { "Pad Alien",               { { id::freq, 40 }, { id::lfoAmt, 35 }, { id::lfoRate, 12 }, { id::stereo, 100 }, { id::mix, 45 } } },
        { "Rotor",                   { { id::range, 0 }, { id::freq, 85 }, { id::lfoAmt, 30 }, { id::lfoRate, 10 }, { id::stereo, 50 } } },
        { u8 ("Telefone do Espaço"),     { { id::freq, 68 }, { id::lfoAmt, 5 },  { id::drive, 10 }, { id::mix, 80 } } },
        { "Escala Quebrada",         { { id::freq, 58 }, { id::lfoAmt, 50 }, { id::lfoRate, 5 },  { id::lfoShape, 4 }, { id::snap, 1 } } },
        { "Onda Quadrada 1/8",       { { id::freq, 52 }, { id::lfoAmt, 50 }, { id::lfoRate, 5 },  { id::lfoShape, 2 } } },
        { u8 ("Respiração Lenta"),   { { id::freq, 35 }, { id::lfoAmt, 70 }, { id::lfoRate, 13 }, { id::mix, 40 } } },
        { "Gongo",                   { { id::freq, 30 }, { id::lfoAmt, 0 },  { id::snap, 1 },     { id::mix, 50 } } },
        { u8 ("Inseto Elétrico"),         { { id::freq, 88 }, { id::lfoAmt, 40 }, { id::lfoRate, 1 },  { id::lfoShape, 4 }, { id::stereo, 60 } } },
        { u8 ("Tremolo Estéreo"),        { { id::range, 0 }, { id::freq, 60 }, { id::lfoAmt, 0 },  { id::stereo, 100 }, { id::mix, 100 } } },
        { "Portal",                  { { id::freq, 65 }, { id::lfoAmt, 100 }, { id::lfoRate, 12 }, { id::lfoShape, 3 }, { id::stereo, 40 } } },
    };
}
} // namespace ringer
