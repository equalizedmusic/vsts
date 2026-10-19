#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "ImageEngine.h"

namespace horizonte
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* width      = "width";
    inline constexpr const char* widthLow   = "widthLow";
    inline constexpr const char* widthHigh  = "widthHigh";
    inline constexpr const char* xover      = "xover";
    inline constexpr const char* monoBass   = "monoBass";
    inline constexpr const char* balance    = "balance";
    inline constexpr const char* rotation   = "rotation";
    inline constexpr const char* output     = "output";
    inline constexpr const char* monoListen = "monoListen";
    inline constexpr const char* sideListen = "sideListen";
    inline constexpr const char* bypass     = "bypass";
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String hzText (float v)
{
    if (v >= 1000.0f) return juce::String (v / 1000.0f, 1).replaceCharacter ('.', ',') + " kHz";
    return juce::String (juce::roundToInt (v)) + " Hz";
}

inline juce::String balanceText (float v)
{
    const int k = juce::roundToInt (v);
    if (k == 0) return "Centro";
    return juce::String (k < 0 ? "E " : "D ") + juce::String (k < 0 ? -k : k);
}

inline juce::String degText (float v)
{
    const int k = juce::roundToInt (v);
    return juce::String (k > 0 ? "+" : "") + juce::String (k) + u8 ("°");
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
    auto hz   = [] (float v, int) { return hzText (v); };
    auto mono = [] (float v, int) { return v < monoOffHz ? String ("Desligado") : hzText (v); };
    auto bal  = [] (float v, int) { return balanceText (v); };
    auto deg  = [] (float v, int) { return degText (v); };

    add (id::width,     "Largura",                 { 0.0f, 200.0f }, 100.0f, pct);
    add (id::widthLow,  "Largura graves",          { 0.0f, 200.0f }, 100.0f, pct);
    add (id::widthHigh, "Largura agudos",          { 0.0f, 200.0f }, 100.0f, pct);
    NormalisableRange<float> xoverRange (80.0f, 4000.0f);
    xoverRange.setSkewForCentre (500.0f);
    NormalisableRange<float> monoRange (0.0f, 300.0f);
    monoRange.setSkewForCentre (80.0f);

    add (id::xover,     u8 ("Divisão"),            xoverRange, 400.0f, hz);
    add (id::monoBass,  "Grave mono",              monoRange, 0.0f, mono);
    add (id::balance,   u8 ("Balanço"),            { -100.0f, 100.0f }, 0.0f, bal);
    add (id::rotation,  u8 ("Rotação"),            { -45.0f, 45.0f }, 0.0f, deg);
    add (id::output,    "Ganho",                   { -24.0f, 12.0f }, 0.0f, db);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::monoListen, 1 }, "Ouvir em mono", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::sideListen, 1 }, u8 ("Ouvir só o lado"), false));
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
        { u8 ("Init · Horizonte"),     { } },
        { "Master Aberto",             { { id::width, 112 }, { id::widthHigh, 120 }, { id::xover, 350 },  { id::monoBass, 100 } } },
        { "Master Club",               { { id::width, 105 }, { id::widthLow, 70 },   { id::widthHigh, 115 }, { id::xover, 300 }, { id::monoBass, 130 } } },
        { "Grave Centrado",            { { id::monoBass, 150 } } },
        { u8 ("Seguro para Vinil"),    { { id::monoBass, 200 }, { id::widthLow, 60 }, { id::xover, 500 } } },
        { "Pista Psy",                 { { id::width, 110 }, { id::widthLow, 50 },   { id::widthHigh, 125 }, { id::xover, 250 }, { id::monoBass, 180 } } },
        { "Baixo Firme",               { { id::widthLow, 30 },  { id::xover, 220 },  { id::monoBass, 250 } } },
        { "Pad Gigante",               { { id::width, 165 }, { id::widthHigh, 140 }, { id::xover, 600 },  { id::monoBass, 120 } } },
        { "Synth Lead Largo",          { { id::width, 135 }, { id::widthHigh, 150 }, { id::xover, 1000 }, { id::monoBass, 120 } } },
        { "Guitarras Dobradas",        { { id::width, 140 }, { id::widthLow, 80 },   { id::xover, 300 } } },
        { "Bateria Overhead",          { { id::width, 120 }, { id::widthHigh, 130 }, { id::xover, 600 },  { id::monoBass, 100 } } },
        { "Vocal no Centro",           { { id::width, 80 } } },
        { "Reverb Mais Estreito",      { { id::width, 60 },  { id::widthLow, 40 },   { id::xover, 500 } } },
        { "Brilho nas Laterais",       { { id::widthHigh, 170 }, { id::xover, 3000 } } },
        { u8 ("Ambiência Contida"),    { { id::width, 75 },  { id::widthHigh, 70 },  { id::xover, 1500 } } },
        { "Mono Total",                { { id::width, 0 } } },
        { u8 ("Girar à Direita"),      { { id::rotation, 15 } } },
        { u8 ("Girar à Esquerda"),     { { id::rotation, -15 } } },
        { "Corrigir Lado Pesado",      { { id::balance, 8 } } },
        { "Super Largo (cuidado)",     { { id::width, 200 }, { id::monoBass, 150 }, { id::output, -2 } } },
    };
}
} // namespace horizonte
