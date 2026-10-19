#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <utility>
#include <vector>

namespace medidor
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* target = "target";   // alvo de loudness integrado (LUFS)
    inline constexpr const char* tpMax  = "tpmax";    // teto de true peak (dBTP)
    inline constexpr const char* tol    = "tol";      // faixa aceita em volta do alvo (± LU)
    inline constexpr const char* lraMax = "lramax";   // LRA máximo aceito (LU)
    inline constexpr const char* mono   = "mono";     // OUVIR EM MONO (checagem)
    inline constexpr const char* match  = "match";    // OUVIR NO ALVO (checagem)
    inline constexpr const char* bypass = "bypass";
}

// número com vírgula e sinal de menos tipográfico ("−14,2"); sem "−0,0"
inline juce::String num (float v, int decimals)
{
    const float q = decimals >= 2 ? 100.0f : decimals == 1 ? 10.0f : 1.0f;
    if (std::abs (v) * q < 0.5f) v = 0.0f;
    if (decimals <= 0)
        return juce::String (juce::roundToInt (v)).replace ("-", u8 ("−"));
    return juce::String (v, decimals).replaceCharacter ('.', ',').replace ("-", u8 ("−"));
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

    add (id::target, "Alvo",            { -36.0f, -6.0f, 1.0f }, -14.0f, [] (float v, int) { return num (v, 0) + " LUFS"; });
    add (id::tpMax,  "Teto true peak",  { -6.0f, 0.0f, 0.1f },   -1.0f,  [] (float v, int) { return num (v, 1) + " dBTP"; });
    add (id::tol,    "Faixa",           { 0.5f, 6.0f, 0.5f },    1.0f,   [] (float v, int) { return u8 ("±") + num (v, 1) + " LU"; });
    add (id::lraMax, u8 ("LRA máximo"), { 3.0f, 30.0f, 1.0f },   20.0f,  [] (float v, int) { return num (v, 0) + " LU"; });

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::mono, 1 },  "Ouvir em mono", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::match, 1 }, "Ouvir no alvo", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// alvos de entrega: loudness integrado, teto de true peak, faixa aceita e LRA máximo
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Monitor"),       { } },
        { "Spotify",                   { { id::target, -14 }, { id::tpMax, -1.0f }, { id::tol, 1.0f } } },
        { "Spotify Alto",              { { id::target, -11 }, { id::tpMax, -2.0f }, { id::tol, 1.0f } } },
        { "Apple Music",               { { id::target, -16 }, { id::tpMax, -1.0f }, { id::tol, 1.0f } } },
        { "YouTube",                   { { id::target, -14 }, { id::tpMax, -1.0f }, { id::tol, 1.0f } } },
        { "Deezer",                    { { id::target, -15 }, { id::tpMax, -1.0f }, { id::tol, 1.0f } } },
        { "Tidal",                     { { id::target, -14 }, { id::tpMax, -1.0f }, { id::tol, 1.0f } } },
        { "Amazon Music",              { { id::target, -14 }, { id::tpMax, -2.0f }, { id::tol, 1.0f } } },
        { "SoundCloud",                { { id::target, -14 }, { id::tpMax, -1.0f }, { id::tol, 1.5f } } },
        { "Club / DJ",                 { { id::target, -9 },  { id::tpMax, -0.5f }, { id::tol, 1.5f }, { id::lraMax, 8 } } },
        { "Master Psy (pista)",        { { id::target, -7 },  { id::tpMax, -0.3f }, { id::tol, 1.5f }, { id::lraMax, 6 } } },
        { u8 ("Rádio FM"),             { { id::target, -16 }, { id::tpMax, -1.0f }, { id::tol, 1.5f }, { id::lraMax, 10 } } },
        { "EBU R128",                  { { id::target, -23 }, { id::tpMax, -1.0f }, { id::tol, 0.5f } } },
        { "ATSC A/85",                 { { id::target, -24 }, { id::tpMax, -2.0f }, { id::tol, 2.0f } } },
        { "Netflix",                   { { id::target, -27 }, { id::tpMax, -2.0f }, { id::tol, 2.0f } } },
        { "Podcast",                   { { id::target, -16 }, { id::tpMax, -1.0f }, { id::tol, 1.0f }, { id::lraMax, 10 } } },
        { "Podcast Mono",              { { id::target, -19 }, { id::tpMax, -1.0f }, { id::tol, 1.0f }, { id::lraMax, 10 } } },
        { "Audiolivro (ACX)",          { { id::target, -20 }, { id::tpMax, -3.0f }, { id::tol, 2.0f }, { id::lraMax, 10 } } },
        { "Games (ASWG)",              { { id::target, -24 }, { id::tpMax, -1.0f }, { id::tol, 2.0f } } },
        { "Mix para Master",           { { id::target, -18 }, { id::tpMax, -6.0f }, { id::tol, 2.0f } } },
    };
}
} // namespace medidor
