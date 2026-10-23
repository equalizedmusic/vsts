#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "SpiralEngine.h"

namespace espiral
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* range     = "range";
    inline constexpr const char* cycle     = "cycle";
    inline constexpr const char* direction = "direction";
    inline constexpr const char* centre    = "centre";
    inline constexpr const char* stereo    = "stereo";
    inline constexpr const char* resonance = "resonance";
    inline constexpr const char* mode      = "mode";
    inline constexpr const char* stages    = "stages";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* tone      = "tone";
    inline constexpr const char* output    = "output";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* freeze    = "freeze";
    inline constexpr const char* twice     = "twice";
    inline constexpr const char* bypass    = "bypass";
}

inline juce::String hzText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
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

    NormalisableRange<float> centreRange (100.0f, 4000.0f);
    centreRange.setSkewForCentre (630.0f);

    add (id::range,     "Range",            { 0.0f, 100.0f }, 60.0f,
         [] (float v, int) { return String (rangeOctaves (v * 0.01f), 1) + " oct"; });
    choice (id::cycle,     "Ciclo",         { "1 tempo", "2 tempos", "1 comp", "2 comp", "4 comp", "8 comp", "16 comp" }, 3);
    choice (id::direction, u8 ("Direção"),  { "Sobe", "Desce", "Vai e volta" }, 0);
    add (id::centre,    "Center",           centreRange, 800.0f, [] (float v, int) { return hzText (v); });
    add (id::stereo,    "Stereo",           { 0.0f, 100.0f }, 25.0f, pct);
    add (id::resonance, "Resonance",        { 0.0f, 100.0f }, 55.0f, pct);
    choice (id::mode,      "Modo",          { "Phaser", "Flanger" }, 0);
    choice (id::stages,    "Stages",       { "4", "6", "8", "12" }, 2);
    add (id::drive,     "Drive",            { 0.0f, 100.0f }, 15.0f, pct);
    add (id::tone,      "Tone",             { 0.0f, 100.0f }, 85.0f,
         [] (float v, int) { return v >= 99.5f ? String ("aberto") : hzText (toneHz (v * 0.01f)); });
    add (id::output,    "Gain",             { -36.0f, 12.0f }, 0.0f,
         [] (float v, int) { return String (v, 1) + " dB"; });
    add (id::mix,       "Dry/Wet",          { 0.0f, 100.0f }, 100.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::freeze, 1 }, "Congelar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::twice, 1 }, "Dobrar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// listas guardam o índice: CICLO 0=1 tempo 1=2 tempos 2=1 comp 3=2 comp 4=4 comp 5=8 comp 6=16 comp;
// DIREÇÃO 0=sobe 1=desce 2=vai e volta; MODO 0=phaser 1=flanger; ESTÁGIOS 0=4 1=6 2=8 3=12
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Espiral"),     { } },
        { "Escada de Shepard",       { { id::range, 90 }, { id::cycle, 4 }, { id::stages, 3 }, { id::resonance, 60 }, { id::centre, 700 } } },
        { "Subida Infinita",         { { id::range, 100 }, { id::cycle, 5 }, { id::stages, 2 }, { id::resonance, 70 }, { id::stereo, 40 }, { id::mix, 80 } } },
        { "Descida ao Templo",       { { id::range, 85 }, { id::cycle, 4 }, { id::direction, 1 }, { id::stages, 3 }, { id::resonance, 65 }, { id::tone, 70 } } },
        { "Jato Flanger",            { { id::mode, 1 }, { id::range, 70 }, { id::cycle, 3 }, { id::resonance, 75 }, { id::centre, 900 } } },
        { u8 ("Metal Líquido"),      { { id::mode, 1 }, { id::range, 40 }, { id::cycle, 2 }, { id::resonance, 90 }, { id::centre, 1500 }, { id::stereo, 50 } } },
        { u8 ("Ácido 303"),          { { id::range, 50 }, { id::cycle, 1 }, { id::stages, 3 }, { id::resonance, 85 }, { id::centre, 1200 }, { id::drive, 40 } } },
        { "Goa Sunrise",             { { id::range, 80 }, { id::cycle, 6 }, { id::stages, 2 }, { id::resonance, 55 }, { id::stereo, 60 }, { id::mix, 85 } } },
        { "Hipnose Lenta",           { { id::range, 60 }, { id::cycle, 6 }, { id::stages, 1 }, { id::resonance, 40 }, { id::mix, 50 } } },
        { "Vai e Volta",             { { id::range, 70 }, { id::cycle, 2 }, { id::direction, 2 }, { id::stages, 2 }, { id::resonance, 60 } } },
        { "Turbina",                 { { id::mode, 1 }, { id::range, 30 }, { id::cycle, 0 }, { id::resonance, 70 }, { id::centre, 1100 } } },
        { u8 ("Pad Cósmico"),        { { id::range, 70 }, { id::cycle, 5 }, { id::stages, 1 }, { id::resonance, 45 }, { id::stereo, 100 }, { id::tone, 70 } } },
        { "Hi-hat Voador",           { { id::range, 30 }, { id::cycle, 1 }, { id::stages, 0 }, { id::resonance, 50 }, { id::centre, 4000 } } },
        { "Baixo Rolante",           { { id::range, 25 }, { id::cycle, 2 }, { id::stages, 0 }, { id::resonance, 35 }, { id::centre, 150 }, { id::mix, 40 } } },
        { u8 ("Sujeira Psicodélica"),{ { id::mode, 1 }, { id::range, 60 }, { id::cycle, 3 }, { id::resonance, 70 }, { id::drive, 80 }, { id::tone, 75 } } },
        { u8 ("Vórtice"),            { { id::mode, 1 }, { id::range, 100 }, { id::cycle, 4 }, { id::direction, 1 }, { id::resonance, 80 }, { id::stereo, 80 } } },
        { u8 ("Fumaça no Palco"),    { { id::range, 55 }, { id::cycle, 4 }, { id::stages, 2 }, { id::resonance, 30 }, { id::tone, 50 }, { id::mix, 45 } } },
        { "Laser Show",              { { id::mode, 1 }, { id::range, 60 }, { id::cycle, 1 }, { id::resonance, 95 }, { id::centre, 2500 } } },
        { u8 ("Mantra Giratório"),   { { id::range, 75 }, { id::cycle, 4 }, { id::direction, 2 }, { id::stages, 3 }, { id::resonance, 65 }, { id::stereo, 50 } } },
        { "Decolagem Lenta",         { { id::range, 100 }, { id::cycle, 6 }, { id::stages, 2 }, { id::resonance, 75 }, { id::mix, 70 } } },
    };
}
} // namespace espiral
