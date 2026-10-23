#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "CompEngine.h"

namespace torno
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* squeeze    = "squeeze";
    inline constexpr const char* threshold  = "threshold";
    inline constexpr const char* ratio      = "ratio";
    inline constexpr const char* knee       = "knee";
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* release    = "release";
    inline constexpr const char* autoRel    = "autorel";
    inline constexpr const char* scHpf      = "schpf";
    inline constexpr const char* makeup     = "makeup";
    inline constexpr const char* autoMakeup = "automakeup";
    inline constexpr const char* output     = "output";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* listen     = "listen";
    inline constexpr const char* bypass     = "bypass";
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String msText (float v)
{
    return v < 10.0f ? juce::String (v, 1).replaceCharacter ('.', ',') + " ms" : juce::String (juce::roundToInt (v)) + " ms";
}

inline juce::String ratioText (float v)
{
    return v < 10.0f ? juce::String (v, 1).replaceCharacter ('.', ',') + ":1" : juce::String (juce::roundToInt (v)) + ":1";
}

inline juce::String hzText (float v)
{
    return hpfActive (v) ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String ("Desligado");
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
    auto ms  = [] (float v, int) { return msText (v); };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    add (id::squeeze,   "Aperto",            { 0.0f, 100.0f }, 0.0f, pct);
    add (id::threshold, "Threshold",         { -60.0f, 0.0f }, -18.0f, db);
    add (id::ratio,     "Ratio",             skewed (1.0f, 20.0f, 4.0f), 4.0f, [] (float v, int) { return ratioText (v); });
    add (id::knee,      "Knee",              { 0.0f, 24.0f }, 6.0f, db);
    add (id::attack,    "Attack",            skewed (0.1f, 100.0f, 10.0f), 10.0f, ms);
    add (id::release,   "Release",           skewed (10.0f, 1000.0f, 150.0f), 150.0f, ms);
    add (id::scHpf,     "SC HPF",            skewed (20.0f, 500.0f, 100.0f), 20.0f, [] (float v, int) { return hzText (v); });
    add (id::makeup,    "Makeup",            { -12.0f, 24.0f }, 0.0f, db);
    add (id::output,    "Gain",              { -36.0f, 12.0f }, 0.0f, db);
    add (id::mix,       "Dry/Wet",           { 0.0f, 100.0f }, 100.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::autoRel, 1 }, "Auto release", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::autoMakeup, 1 }, "Auto makeup", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::listen, 1 }, "SC listen", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// liga/desliga entram como 1/0
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Torno"),          { } },
        { "Bateria Colada",             { { id::threshold, -20 }, { id::ratio, 4 },   { id::knee, 6 },  { id::attack, 10 },   { id::autoRel, 1 },     { id::scHpf, 80 },  { id::autoMakeup, 1 } } },
        { "Bumbo com Punch",            { { id::threshold, -18 }, { id::ratio, 4 },   { id::knee, 3 },  { id::attack, 30 },   { id::release, 80 },    { id::makeup, 3 } } },
        { "Caixa Estalando",            { { id::threshold, -22 }, { id::ratio, 6 },   { id::knee, 2 },  { id::attack, 15 },   { id::release, 60 },    { id::makeup, 4 } } },
        { "Paralelo Nova York",         { { id::threshold, -35 }, { id::ratio, 10 },  { id::knee, 0 },  { id::attack, 1 },    { id::release, 120 },   { id::makeup, 12 }, { id::mix, 35 } } },
        { "Grupo de Bateria",           { { id::threshold, -16 }, { id::ratio, 4 },   { id::knee, 4 },  { id::attack, 30 },   { id::autoRel, 1 },     { id::scHpf, 60 },  { id::makeup, 3 }, { id::mix, 70 } } },
        { "Room Esmagado",              { { id::threshold, -40 }, { id::ratio, 20 },  { id::knee, 0 },  { id::attack, 0.3f }, { id::release, 60 },    { id::makeup, 18 }, { id::mix, 50 } } },
        { "Transiente Domado",          { { id::threshold, -14 }, { id::ratio, 8 },   { id::knee, 2 },  { id::attack, 0.1f }, { id::release, 50 },    { id::makeup, 2 } } },
        { "Vocal na Frente",            { { id::threshold, -24 }, { id::ratio, 3 },   { id::knee, 8 },  { id::attack, 5 },    { id::release, 120 },   { id::scHpf, 120 }, { id::autoMakeup, 1 } } },
        { "Vocal Nivelado",             { { id::threshold, -28 }, { id::ratio, 2.5f },{ id::knee, 12 }, { id::attack, 8 },    { id::autoRel, 1 },     { id::scHpf, 100 }, { id::autoMakeup, 1 } } },
        { "Voz Falada",                 { { id::threshold, -30 }, { id::ratio, 4 },   { id::knee, 10 }, { id::attack, 5 },    { id::autoRel, 1 },     { id::scHpf, 150 }, { id::autoMakeup, 1 } } },
        { "Baixo Firme",                { { id::threshold, -20 }, { id::ratio, 5 },   { id::knee, 6 },  { id::attack, 20 },   { id::release, 200 },   { id::makeup, 4 } } },
        { u8 ("Violão Suave"),          { { id::threshold, -22 }, { id::ratio, 2 },   { id::knee, 10 }, { id::attack, 15 },   { id::release, 250 },   { id::scHpf, 150 }, { id::autoMakeup, 1 } } },
        { "Lead Controlado",            { { id::threshold, -20 }, { id::ratio, 4 },   { id::knee, 6 },  { id::attack, 3 },    { id::release, 100 },   { id::makeup, 4 } } },
        { "Pad Respirando",             { { id::threshold, -26 }, { id::ratio, 3 },   { id::knee, 12 }, { id::attack, 50 },   { id::release, 600 },   { id::autoMakeup, 1 } } },
        { "Bombeando",                  { { id::threshold, -30 }, { id::ratio, 8 },   { id::knee, 0 },  { id::attack, 0.5f }, { id::release, 250 },   { id::makeup, 8 } } },
        { "Grave Livre",                { { id::threshold, -18 }, { id::ratio, 3 },   { id::knee, 6 },  { id::attack, 15 },   { id::autoRel, 1 },     { id::scHpf, 250 }, { id::makeup, 2 } } },
        { "Master Cola",                { { id::threshold, -12 }, { id::ratio, 2 },   { id::knee, 6 },  { id::attack, 30 },   { id::autoRel, 1 },     { id::scHpf, 90 },  { id::makeup, 1.5f } } },
        { "Master Transparente",        { { id::threshold, -8 },  { id::ratio, 1.5f },{ id::knee, 12 }, { id::attack, 30 },   { id::release, 300 },   { id::scHpf, 60 },  { id::makeup, 1 } } },
        { u8 ("Aperto Máximo"),         { { id::squeeze, 100 },   { id::threshold, -18 }, { id::ratio, 4 }, { id::attack, 5 }, { id::release, 100 },  { id::autoMakeup, 1 } } },
    };
}
} // namespace torno
