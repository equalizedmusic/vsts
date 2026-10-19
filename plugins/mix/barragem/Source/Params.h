#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "LimiterEngine.h"

namespace barragem
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* drive   = "drive";
    inline constexpr const char* ceiling = "ceiling";
    inline constexpr const char* release = "release";
    inline constexpr const char* autoRel = "autorel";
    inline constexpr const char* look    = "lookahead";
    inline constexpr const char* link    = "link";
    inline constexpr const char* target  = "target";
    inline constexpr const char* delta   = "delta";
    inline constexpr const char* bypass  = "bypass";
}

// ---------------- alvos de loudness (LUFS integrado) ----------------
struct Target
{
    const char* name;   // UTF-8
    float lufs;
};

constexpr int numTargets = 12;
inline const Target targets[numTargets] = {
    { "Spotify",                -14.0f },
    { "Spotify Alto",           -11.0f },
    { "Apple Music",            -16.0f },
    { "YouTube",                -14.0f },
    { "Tidal",                  -14.0f },
    { "Amazon Music",           -14.0f },
    { "Deezer",                 -15.0f },
    { "Podcast",                -16.0f },
    { "Club",                    -9.0f },
    { "Pista Pesada",            -7.0f },
    { "TV · EBU R128",          -23.0f },
    { "TV · ATSC A/85",         -24.0f },
};

inline int clampTarget (int i) { return juce::jlimit (0, numTargets - 1, i); }

inline juce::String targetText (int i)
{
    const auto& t = targets[clampTarget (i)];
    return u8 (t.name) + " " + juce::String (juce::roundToInt (t.lufs)) + " LUFS";
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String tpText (float v)
{
    return juce::String (v, 1).replaceCharacter ('.', ',') + " dBTP";
}

inline juce::String msText (float v)
{
    return v < 10.0f ? juce::String (v, 1).replaceCharacter ('.', ',') + " ms" : juce::String (juce::roundToInt (v)) + " ms";
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
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    StringArray targetNames;
    for (int i = 0; i < numTargets; ++i)
        targetNames.add (targetText (i));

    add (id::drive,   "Encher",    { 0.0f, 24.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::ceiling, "Teto",      { -12.0f, 0.0f }, -1.0f, [] (float v, int) { return tpText (v); });
    add (id::release, "Soltura",   skewed (1.0f, 1000.0f, 80.0f), 100.0f, [] (float v, int) { return msText (v); });
    add (id::look,    "Lookahead", { 0.5f, maxLookMs }, 2.0f, [] (float v, int) { return msText (v); });
    add (id::link,    "Link",      { 0.0f, 100.0f }, 100.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::autoRel, 1 }, "Soltura auto", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::target, 1 }, "Alvo", targetNames, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::delta, 1 }, u8 ("Ouvir diferença"), false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// ALVO guarda o índice: 0 Spotify · 1 Spotify Alto · 2 Apple Music · 3 YouTube · 4 Tidal · 5 Amazon Music
// 6 Deezer · 7 Podcast · 8 Club · 9 Pista Pesada · 10 TV EBU R128 · 11 TV ATSC A/85. SOLTURA AUTO: 1 = ligada
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Barragem"),          { } },
        { "Spotify Seguro",                { { id::drive, 4 },  { id::ceiling, -1.0f }, { id::target, 0 } } },
        { "Apple Music Limpo",             { { id::drive, 2 },  { id::ceiling, -1.0f }, { id::release, 150 }, { id::target, 2 } } },
        { "YouTube Pronto",                { { id::drive, 4 },  { id::ceiling, -1.0f }, { id::target, 3 } } },
        { "Club Pesado",                   { { id::drive, 9 },  { id::ceiling, -0.3f }, { id::release, 60 },  { id::look, 1.5f }, { id::target, 8 } } },
        { "Pista Psy",                     { { id::drive, 11 }, { id::ceiling, -0.2f }, { id::release, 40 },  { id::look, 1.0f }, { id::target, 9 } } },
        { "Transparente",                  { { id::drive, 2 },  { id::ceiling, -1.0f }, { id::release, 250 }, { id::look, 5.0f } } },
        { "Parede de Som",                 { { id::drive, 14 }, { id::ceiling, -0.3f }, { id::release, 30 },  { id::look, 1.0f }, { id::autoRel, 0 }, { id::target, 9 } } },
        { u8 ("Bombeando de Propósito"),   { { id::drive, 10 }, { id::ceiling, -1.0f }, { id::release, 180 }, { id::look, 0.5f }, { id::autoRel, 0 } } },
        { "Podcast Voz",                   { { id::drive, 6 },  { id::ceiling, -1.5f }, { id::release, 120 }, { id::target, 7 } } },
        { "TV Europa (R128)",              { { id::drive, 0 },  { id::ceiling, -1.0f }, { id::release, 200 }, { id::target, 10 } } },
        { "TV EUA (A/85)",                 { { id::drive, 0 },  { id::ceiling, -2.0f }, { id::release, 200 }, { id::target, 11 } } },
        { u8 ("Acústico Respirando"),      { { id::drive, 3 },  { id::ceiling, -1.0f }, { id::release, 400 }, { id::look, 5.0f }, { id::target, 2 } } },
        { "Rock na Cara",                  { { id::drive, 8 },  { id::ceiling, -0.5f }, { id::release, 80 },  { id::look, 2.0f }, { id::target, 1 } } },
        { "Hip-Hop Grave Firme",           { { id::drive, 7 },  { id::ceiling, -0.8f }, { id::release, 200 }, { id::look, 4.0f } } },
        { u8 ("Estéreo Solto"),            { { id::drive, 5 },  { id::ceiling, -1.0f }, { id::link, 40 } } },
        { u8 ("Pré-Master Demo"),          { { id::drive, 6 },  { id::ceiling, -0.5f }, { id::release, 100 }, { id::target, 1 } } },
        { u8 ("Trava de Segurança"),       { { id::drive, 0 },  { id::ceiling, -0.3f }, { id::release, 50 } } },
        { "Lookahead Longo",               { { id::drive, 6 },  { id::ceiling, -1.0f }, { id::release, 300 }, { id::look, 5.0f } } },
        { u8 ("Ataque Rápido"),            { { id::drive, 8 },  { id::ceiling, -1.0f }, { id::release, 50 },  { id::look, 0.5f }, { id::autoRel, 0 } } },
    };
}
} // namespace barragem
