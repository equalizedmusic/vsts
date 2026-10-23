#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "TapeEngine.h"

namespace fita
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* model    = "model";
    inline constexpr const char* speed    = "speed";
    inline constexpr const char* bump     = "bump";
    inline constexpr const char* wear     = "wear";
    inline constexpr const char* wow      = "wow";
    inline constexpr const char* flutter  = "flutter";
    inline constexpr const char* hiss     = "hiss";
    inline constexpr const char* brake    = "brake";
    inline constexpr const char* start    = "start";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* tapeStop = "tapestop";
    inline constexpr const char* bypass   = "bypass";
}

inline juce::String hzText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
}

inline juce::String ipsText (float ips)
{
    if (std::abs (ips - 1.875f) < 0.01f) return "1 7/8";
    if (std::abs (ips - 3.75f) < 0.01f)  return "3 3/4";
    if (std::abs (ips - 7.5f) < 0.01f)   return "7,5";
    return juce::String (juce::roundToInt (ips));
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

    add (id::drive,   "Saturation",     { 0.0f, 100.0f }, 40.0f, pct);
    choice (id::model, "Modelo",        { "Rolo 1/4\"", "Fita 2\"", "Cassete" }, 0);
    choice (id::speed, "Speed",         { "7,5 ips", "15 ips", "30 ips" }, 1);
    add (id::bump,    "Head bump",      { 0.0f, 100.0f }, 50.0f,
         [] (float v, int) { return "+" + String (6.0f * v * 0.01f, 1) + " dB"; });
    add (id::wear,    "Wear",           { 0.0f, 100.0f }, 0.0f, pct);
    add (id::wow,     "Wow",            { 0.0f, 100.0f }, 15.0f, pct);
    add (id::flutter, "Flutter",        { 0.0f, 100.0f }, 15.0f, pct);
    add (id::hiss,    "Hiss",           { 0.0f, 100.0f }, 20.0f, pct);
    choice (id::brake, "Freio",         { "1/2 tempo", "1 tempo", "2 tempos", "1 comp", "2 comp" }, 3);
    choice (id::start, "Partida",       { "Na hora", "Curta", "Igual ao freio" }, 1);
    add (id::output,  "Gain",           { -36.0f, 12.0f }, 0.0f,
         [] (float v, int) { return String (v, 1) + " dB"; });
    add (id::mix,     "Dry/Wet",        { 0.0f, 100.0f }, 100.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::tapeStop, 1 }, "Tape Stop", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// listas guardam o índice: MODELO 0=rolo 1/4" 1=fita 2" 2=cassete; VELOCIDADE 0=7,5 1=15 2=30 ips;
// FREIO 0=1/2 tempo 1=1 tempo 2=2 tempos 3=1 comp 4=2 comp; PARTIDA 0=na hora 1=curta 2=igual ao freio
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Fita"),         { } },
        { u8 ("Estúdio 15 ips"),      { { id::drive, 35 }, { id::bump, 50 }, { id::wow, 8 }, { id::flutter, 8 }, { id::hiss, 10 } } },
        { "Master 30 ips",            { { id::speed, 2 }, { id::drive, 25 }, { id::bump, 35 }, { id::wow, 4 }, { id::flutter, 4 }, { id::hiss, 5 } } },
        { "Fita Larga 2\"",           { { id::model, 1 }, { id::drive, 45 }, { id::bump, 65 }, { id::wow, 5 }, { id::flutter, 5 }, { id::hiss, 8 } } },
        { "Bumbo Gordo",              { { id::model, 1 }, { id::speed, 0 }, { id::drive, 55 }, { id::bump, 90 }, { id::wow, 3 }, { id::flutter, 3 }, { id::hiss, 0 } } },
        { "Baixo Rolante Quente",     { { id::model, 1 }, { id::drive, 60 }, { id::bump, 75 }, { id::wow, 0 }, { id::flutter, 0 }, { id::hiss, 0 } } },
        { "Cassete Gasto",            { { id::model, 2 }, { id::speed, 0 }, { id::drive, 55 }, { id::wear, 60 }, { id::wow, 45 }, { id::flutter, 40 }, { id::hiss, 55 }, { id::bump, 40 } } },
        { "Fita Derretida",           { { id::model, 2 }, { id::speed, 0 }, { id::drive, 50 }, { id::wear, 70 }, { id::wow, 100 }, { id::flutter, 60 }, { id::hiss, 40 } } },
        { "Lo-fi Goa 1994",           { { id::model, 2 }, { id::drive, 50 }, { id::wear, 35 }, { id::wow, 25 }, { id::flutter, 30 }, { id::hiss, 45 } } },
        { u8 ("Saturação Pesada"),    { { id::drive, 90 }, { id::bump, 55 }, { id::wow, 10 }, { id::flutter, 10 }, { id::hiss, 15 } } },
        { "Cola de Barramento",       { { id::speed, 2 }, { id::drive, 30 }, { id::bump, 30 }, { id::wow, 3 }, { id::flutter, 3 }, { id::hiss, 0 } } },
        { u8 ("Lead Ácido Sujo"),     { { id::model, 2 }, { id::speed, 2 }, { id::drive, 85 }, { id::wear, 20 }, { id::wow, 10 }, { id::flutter, 20 }, { id::hiss, 10 } } },
        { "Pad Enjoado",              { { id::speed, 0 }, { id::drive, 30 }, { id::wow, 70 }, { id::flutter, 20 }, { id::hiss, 25 }, { id::mix, 80 } } },
        { "Vocal Antigo",             { { id::model, 2 }, { id::drive, 40 }, { id::wear, 45 }, { id::wow, 20 }, { id::flutter, 25 }, { id::hiss, 35 }, { id::bump, 20 } } },
        { u8 ("Tape Stop Rápido"),    { { id::brake, 1 }, { id::start, 1 }, { id::drive, 40 }, { id::wow, 10 } } },
        { "Tape Stop Longo",          { { id::brake, 4 }, { id::start, 2 }, { id::drive, 40 } } },
        { u8 ("Freio de Mão"),        { { id::brake, 0 }, { id::start, 0 }, { id::drive, 45 } } },
        { "Hi-hat de Fita",           { { id::speed, 2 }, { id::drive, 70 }, { id::bump, 0 }, { id::wow, 0 }, { id::flutter, 10 }, { id::hiss, 0 } } },
        { "Poeira no Rolo",           { { id::speed, 0 }, { id::drive, 35 }, { id::wear, 80 }, { id::wow, 30 }, { id::flutter, 35 }, { id::hiss, 60 } } },
        { "Paralelo Gordo",           { { id::model, 1 }, { id::speed, 0 }, { id::drive, 100 }, { id::bump, 80 }, { id::wow, 0 }, { id::flutter, 0 }, { id::hiss, 0 }, { id::mix, 40 } } },
    };
}
} // namespace fita
