#pragma once

#include <vector>
#include "Params.h"

// VACINA — presets de fábrica. O banco FACTORY 1 está em presets/fabrica.inc (GERADO por tools/gerar-presets.py);
// bancos avulsos (.vcnbank) e os presets do usuário são lidos pelo processador.
// O que o preset não traz volta ao padrão do parâmetro.
namespace vacina
{
// ---------------- categorias (o preset guarda o ÍNDICE; o nome só aparece no menu) ----------------
inline constexpr int numCategories = 9;
inline const char* const categoryNames[numCategories] = { "BASS", "LEAD", "PAD", "PLUCK", "ARP", "KEYS",
                                                          "SUPERSAW", "ATMOSFERA", "FX" };

// como está escrito no código (fábrica): texto estático
struct FactoryPreset
{
    const char* name;
    int category;                                          // −1 = Init (fora das categorias)
    std::vector<std::pair<const char*, float>> values;
};

// como o processador usa (fábrica e bancos importados)
struct PresetData
{
    juce::String name;
    int category = -1;
    std::vector<std::pair<juce::String, float>> values;
};

// o Init primeiro e depois o banco FACTORY 1 (40 por categoria), gerado por tools/gerar-presets.py
inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "Init", -1, {} },
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
        out.push_back (std::move (p));
    }
    return out;
}
} // namespace vacina
