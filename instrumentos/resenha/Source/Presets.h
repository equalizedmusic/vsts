#pragma once

#include <vector>
#include "Params.h"
#include "Phrase.h"

// RESENHA — presets de fábrica. Cada um traz os 4 timbres, o gerador E a frase.
// O banco FACTORY 1 está em presets/fabrica.inc (GERADO por tools/gerar-presets.py); bancos avulsos (.rsnbank)
// e os presets do usuário são lidos pelo processador. Frase em texto: ver parsePattern em Phrase.h.
// O que o preset não traz volta ao padrão; as travas e o modo de tocar (performance) não mudam.
namespace resenha
{
inline constexpr int numCategories = 10;
inline const char* const categoryNames[numCategories] = { "PSY CLÁSSICO", "FULL ON", "DARK", "GOA", "PROG",
                                                          "HI-TECH", "MINIMAL", "FX", "CONVERSA FM", "LEAD FALANTE" };

struct FactoryPreset
{
    const char* name;
    int category;                                          // −1 = Init (fora das categorias)
    std::vector<std::pair<const char*, float>> values;
    const char* phrase;
};

struct PresetData
{
    juce::String name;
    int category = -1;
    std::vector<std::pair<juce::String, float>> values;
    juce::String phrase;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "Init", -1, {}, "0:7 2:7 4:7 2:7 0:7 2:7 4:7 7:7 0:7 2:7 4:7 2:7 0:7 2:7 4:7 7:7 "
                          "0:7 2:7 4:7 2:7 0:7 2:7 4:7 7:7 0:7 2:7 4:7 2:7 0:7 2:7 4:7 7:7" },
       #include "presets/fabrica.inc"
    };
    return list;
}

inline std::vector<PresetData> factoryPresetData()
{
    std::vector<PresetData> out;
    for (const auto& f : factoryPresets())
    {
        PresetData p;
        p.name = u8 (f.name);
        p.category = f.category;
        for (const auto& v : f.values) p.values.emplace_back (v.first, v.second);
        p.phrase = f.phrase;
        out.push_back (std::move (p));
    }
    return out;
}
} // namespace resenha
