#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "GateEngine.h"

namespace picote
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* depth    = "depth";
    inline constexpr const char* div      = "div";
    inline constexpr const char* length   = "length";
    inline constexpr const char* swing    = "swing";
    inline constexpr const char* offset   = "offset";
    inline constexpr const char* width    = "width";
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* release  = "release";
    inline constexpr const char* filtType = "filttype";
    inline constexpr const char* reso     = "reso";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* hold     = "hold";
    inline constexpr const char* bypass   = "bypass";

    // as três faixas de 16 passos
    inline const char* const vol[numSteps] = { "vol01", "vol02", "vol03", "vol04", "vol05", "vol06", "vol07", "vol08",
                                               "vol09", "vol10", "vol11", "vol12", "vol13", "vol14", "vol15", "vol16" };
    inline const char* const pan[numSteps] = { "pan01", "pan02", "pan03", "pan04", "pan05", "pan06", "pan07", "pan08",
                                               "pan09", "pan10", "pan11", "pan12", "pan13", "pan14", "pan15", "pan16" };
    inline const char* const flt[numSteps] = { "flt01", "flt02", "flt03", "flt04", "flt05", "flt06", "flt07", "flt08",
                                               "flt09", "flt10", "flt11", "flt12", "flt13", "flt14", "flt15", "flt16" };
}

// faixa de cada passo: VOLUME 0–100, PAN −100–100, FILTRO 0–100 (100 = aberto)
enum Lane { laneVol, lanePan, laneFlt, numLanes };
inline const char* const laneNames[numLanes] = { "VOLUME", "PAN", "FILTER" };

inline const char* laneId (int lane, int step)
{
    step = juce::jlimit (0, numSteps - 1, step);
    return lane == lanePan ? id::pan[step] : lane == laneFlt ? id::flt[step] : id::vol[step];
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String msText (float v)
{
    return v < 10.0f ? juce::String (v, 1).replaceCharacter ('.', ',') + " ms" : juce::String (juce::roundToInt (v)) + " ms";
}

inline juce::String panText (float v)
{
    const int p = juce::roundToInt (v);
    return p == 0 ? juce::String ("C") : p < 0 ? "E" + juce::String (-p) : "D" + juce::String (p);
}

// padrão do Init: colcheias no contratempo do 1/16
inline float defaultVol (int step) { return step % 2 == 0 ? 100.0f : 0.0f; }

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
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    StringArray divs, types;
    for (auto* d : divNames) divs.add (d);
    for (auto* t : filterNames) types.add (u8 (t));

    add (id::depth,    "Depth",         { 0.0f, 100.0f }, 100.0f, pct);
    choice (id::div,   u8 ("Divisão"),  divs, 2);
    add (id::length,   "Steps",         { 1.0f, 16.0f, 1.0f }, 16.0f, [] (float v, int) { return String (roundToInt (v)); });
    add (id::swing,    "Swing",         { 0.0f, 100.0f }, 0.0f, pct);
    add (id::offset,   "Deslocar",      { 0.0f, 15.0f, 1.0f }, 0.0f, [] (float v, int) { return "+" + String (roundToInt (v)); });
    add (id::width,    "Width",         { 5.0f, 100.0f }, 60.0f, pct);
    add (id::attack,   "Attack",        skewed (0.5f, 50.0f, 5.0f), 2.0f, [] (float v, int) { return msText (v); });
    add (id::release,  "Release",       skewed (1.0f, 200.0f, 20.0f), 15.0f, [] (float v, int) { return msText (v); });
    choice (id::filtType, "Filter",     types, 0);
    add (id::reso,     "Resonance",     { 0.0f, 100.0f }, 25.0f, pct);
    add (id::output,   "Gain",          { -36.0f, 12.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::mix,      "Dry/Wet",       { 0.0f, 100.0f }, 100.0f, pct);

    for (int i = 0; i < numSteps; ++i)
    {
        const auto n = String (i + 1).paddedLeft ('0', 2);
        add (id::vol[i], "Volume " + n, { 0.0f, 100.0f }, defaultVol (i), pct);
        add (id::pan[i], "Pan " + n,    { -100.0f, 100.0f }, 0.0f, [] (float v, int) { return panText (v); });
        add (id::flt[i], "Filter " + n, { 0.0f, 100.0f }, 100.0f, pct);
    }

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::hold, 1 }, "Prender", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// padrão escrito em texto, um caractere por passo (o que faltar volta ao padrão):
//   VOLUME  x = 100, . = 0, 1–9 = 11–100%      PAN  L/R = todo esq./dir., l/r = meio, c = centro
//   FILTRO  x = aberto, 0–9 = do mais fechado ao quase aberto
inline FactoryPreset makePreset (const juce::String& name, const char* vol, const char* pan = nullptr, const char* flt = nullptr,
                                 std::vector<std::pair<const char*, float>> extra = {})
{
    FactoryPreset p { name, std::move (extra) };
    auto level = [] (char c) { return c == 'x' ? 100.0f : (c >= '1' && c <= '9') ? (float) (c - '0') * 100.0f / 9.0f : 0.0f; };
    for (int i = 0; vol != nullptr && vol[i] != 0 && i < numSteps; ++i)
        p.values.push_back ({ id::vol[i], level (vol[i]) });
    for (int i = 0; pan != nullptr && pan[i] != 0 && i < numSteps; ++i)
    {
        const char c = pan[i];
        p.values.push_back ({ id::pan[i], c == 'L' ? -100.0f : c == 'l' ? -50.0f : c == 'r' ? 50.0f : c == 'R' ? 100.0f : 0.0f });
    }
    for (int i = 0; flt != nullptr && flt[i] != 0 && i < numSteps; ++i)
    {
        const char c = flt[i];
        p.values.push_back ({ id::flt[i], c == 'x' ? 100.0f : (c >= '0' && c <= '9') ? 5.0f + (float) (c - '0') * 10.0f : 100.0f });
    }
    return p;
}

// listas guardam o índice: DIVISÃO 0=1/8 1=1/8T 2=1/16 3=1/16T 4=1/32; FILTRO 0=passa-baixa 1=passa-alta 2=passa-faixa
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Picote"), { } },
        makePreset (u8 ("Trance Gate Clássico"), "x.xx.xx.x.xx.xx.", nullptr, nullptr, { { id::width, 70 } }),
        makePreset ("Colcheias Secas",           "x.x.x.x.x.x.x.x.", nullptr, nullptr, { { id::width, 45 }, { id::release, 5 } }),
        makePreset ("Contratempo",               "..x...x...x...x.", nullptr, nullptr, { { id::width, 80 } }),
        makePreset ("Galope",                    "xx.xx.xx.xx.xx.x", nullptr, nullptr, { { id::width, 65 } }),
        makePreset (u8 ("Tercinas"),             "x.xx.xx.xx.x",     nullptr, nullptr, { { id::div, 3 }, { id::length, 12 }, { id::width, 70 } }),
        makePreset (u8 ("Pingue-pongue"),        "xxxxxxxxxxxxxxxx", "LRLRLRLRLRLRLRLR", nullptr, { { id::width, 90 }, { id::depth, 60 } }),
        makePreset ("Varredura de Filtro",       "xxxxxxxxxxxxxxxx", nullptr, "0123456789xxxxxx", { { id::depth, 30 }, { id::reso, 55 } }),
        makePreset ("Acentos",                   "x4x4x4x4x4x4x4x4", nullptr, nullptr, { { id::width, 85 } }),
        makePreset ("Picadinho 1/32",            "x.x.x.x.x.x.x.x.", nullptr, nullptr, { { id::div, 4 }, { id::width, 50 }, { id::release, 3 } }),
        makePreset ("Gaguejo",                   "xxxx....x.x.xxxx", nullptr, nullptr, { { id::width, 55 } }),
        makePreset (u8 ("Respiração"),           "x...x...x...x...", nullptr, nullptr, { { id::width, 100 }, { id::attack, 40 }, { id::release, 150 }, { id::depth, 70 } }),
        makePreset ("Swingado",                  "x.xx.xx.x.xx.xx.", nullptr, nullptr, { { id::swing, 55 }, { id::width, 70 } }),
        makePreset ("Morse",                     "x.xxx.x...xxx.x.", nullptr, nullptr, { { id::width, 60 } }),
        makePreset ("Escada",                    "123456789x987654", nullptr, nullptr, { { id::width, 80 } }),
        makePreset (u8 ("Filtro Dançante"),      "xxxxxxxxxxxxxxxx", nullptr, "x3x6x2x8x4x9x1x5", { { id::reso, 60 }, { id::width, 100 } }),
        makePreset ("Passa-alta Picado",         "xxxxxxxxxxxxxxxx", nullptr, "x0x0x0x0x0x0x0x0", { { id::filtType, 1 }, { id::reso, 40 }, { id::width, 100 } }),
        makePreset (u8 ("Estéreo Quebrado"),     "xx.xx.xx.xx.x.x.", "LLRRlrLRRLLRrlRL", nullptr, { { id::width, 75 } }),
        makePreset ("Meio Gate",                 "x.xx.xx.x.xx.xx.", nullptr, nullptr, { { id::depth, 50 }, { id::width, 70 } }),
        makePreset ("Dub Chop 1/8",              "x..x..x.x..x..x.", "c.l..r.cc.r..l.c", "x..7..5.x..6..4.", { { id::div, 0 }, { id::width, 85 }, { id::reso, 35 } }),
    };
}
} // namespace picote
