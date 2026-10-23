#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "LadderEngine.h"

namespace f1
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* cutoff    = "cutoff";
    inline constexpr const char* resonance = "resonance";
    inline constexpr const char* poles     = "poles";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* range     = "range";
    inline constexpr const char* envAmt    = "envamt";
    inline constexpr const char* smooth    = "smooth";
    inline constexpr const char* lfoRate   = "lforate";
    inline constexpr const char* lfoAmt    = "lfoamt";
    inline constexpr const char* sens      = "sens";
    inline constexpr const char* output    = "output";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* open      = "open";
    inline constexpr const char* smoke     = "smoke";
    inline constexpr const char* bypass    = "bypass";
}

inline juce::String hzText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1).replaceCharacter ('.', ',') + " kHz"
                         : juce::String (juce::roundToInt (hz)) + " Hz";
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

    NormalisableRange<float> cutRange (20.0f, 18000.0f);
    cutRange.setSkewForCentre (800.0f);
    StringArray divs;
    for (auto* d : divNames) divs.add (d);

    add (id::cutoff,    "Cutoff",           cutRange, 1200.0f, [] (float v, int) { return hzText (v); });
    add (id::resonance, "Resonance",        { 0.0f, 100.0f }, 35.0f, pct);
    choice (id::poles,  "Poles",          StringArray (u8 ("2 · 12 dB"), u8 ("4 · 24 dB")), 1);
    add (id::drive,     "Drive",            { 0.0f, 24.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    choice (id::range,  "Range",            { "High", "Low" }, 0);
    add (id::envAmt,    "Envelope",         { -100.0f, 100.0f }, 0.0f,
         [] (float v, int) { return (v > 0.5f ? "+" : "") + String (roundToInt (v)) + "%"; });
    add (id::smooth,    "Smooth",           { 0.0f, 100.0f }, 25.0f,
         [] (float v, int) { return v < 20.0f ? String ("Fast") : v < 60.0f ? String ("Medium") : String ("Slow"); });
    choice (id::lfoRate, "LFO",             divs, 5);
    add (id::lfoAmt,    "LFO Amount",      { 0.0f, 100.0f }, 0.0f, pct);
    add (id::sens,      "Sensitivity",   { -48.0f, 0.0f }, -18.0f,
         [] (float v, int) { return dbText (v).replace ("-", u8 ("−")); });
    add (id::output,    "Gain",             { -36.0f, 12.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::mix,       "Dry/Wet",             { 0.0f, 100.0f }, 100.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::open, 1 }, "Abrir", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::smoke, 1 }, u8 ("Fumaça"), false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// listas guardam o índice: POLOS 0=2 1=4; FAIXA 0=alta 1=baixa;
// LFO 0=1/32 1=1/16T 2=1/16 3=1/8T 4=3/32 5=1/8 6=1/4T 7=3/16 8=1/4 9=3/8 10=1/2 11=1 comp 12=2 comp 13=4 comp
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Primeiro Trago"), { } },
        { u8 ("Ácido Rolando"),         { { id::cutoff, 500 },  { id::resonance, 80 }, { id::envAmt, 70 },  { id::smooth, 10 }, { id::drive, 8 } } },
        { "Baixo Psy Redondo",          { { id::cutoff, 300 },  { id::resonance, 30 }, { id::envAmt, 45 },  { id::smooth, 15 }, { id::range, 1 } } },
        { u8 ("Wah Automático"),            { { id::cutoff, 400 },  { id::resonance, 70 }, { id::poles, 0 },    { id::envAmt, 90 }, { id::smooth, 30 } } },
        { "Abre no Grid",               { { id::cutoff, 600 },  { id::resonance, 55 }, { id::lfoRate, 11 }, { id::lfoAmt, 80 } } },
        { "Respiro de 1/8",             { { id::cutoff, 900 },  { id::resonance, 45 }, { id::lfoRate, 5 },  { id::lfoAmt, 50 } } },
        { "Gotas 3/16",                 { { id::cutoff, 1500 }, { id::resonance, 75 }, { id::lfoRate, 7 },  { id::lfoAmt, 60 } } },
        { "Lead Fechado",               { { id::cutoff, 2200 }, { id::resonance, 40 }, { id::envAmt, 35 },  { id::drive, 6 } } },
        { "Grave Sem Brilho",           { { id::cutoff, 180 },  { id::resonance, 15 }, { id::range, 1 } } },
        { "Telefone Antigo",            { { id::cutoff, 2500 }, { id::resonance, 60 }, { id::poles, 0 },    { id::drive, 10 }, { id::mix, 85 } } },
        { "Assobio Ressonante",         { { id::cutoff, 1800 }, { id::resonance, 98 }, { id::envAmt, 20 } } },
        { "Envelope Invertido",         { { id::cutoff, 4000 }, { id::resonance, 50 }, { id::envAmt, -80 }, { id::smooth, 20 } } },
        { "Hi-hat Abafado",             { { id::cutoff, 6000 }, { id::resonance, 35 }, { id::envAmt, 40 },  { id::sens, -24 } } },
        { "Pad Respirando",             { { id::cutoff, 700 },  { id::resonance, 35 }, { id::lfoRate, 12 }, { id::lfoAmt, 70 }, { id::smooth, 70 } } },
        { "Sujeira Valvulada",          { { id::cutoff, 3000 }, { id::resonance, 25 }, { id::drive, 18 },   { id::mix, 80 } } },
        { u8 ("Fumaça na Pista"),           { { id::cutoff, 1000 }, { id::resonance, 45 }, { id::envAmt, 30 },  { id::lfoRate, 10 }, { id::lfoAmt, 30 } } },
        { "Build-up Lento",             { { id::cutoff, 250 },  { id::resonance, 60 }, { id::lfoRate, 13 }, { id::lfoAmt, 100 } } },
        { "Kick Encorpado",             { { id::cutoff, 140 },  { id::resonance, 50 }, { id::range, 1 },    { id::envAmt, 25 }, { id::smooth, 5 }, { id::mix, 60 } } },
        { "Borbulha 1/16",              { { id::cutoff, 1200 }, { id::resonance, 85 }, { id::lfoRate, 2 },  { id::lfoAmt, 40 }, { id::poles, 0 } } },
        { u8 ("Último Trago"),          { { id::cutoff, 350 },  { id::resonance, 92 }, { id::envAmt, 100 }, { id::smooth, 45 }, { id::drive, 12 } } },
    };
}
} // namespace f1
