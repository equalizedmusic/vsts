#pragma once

#include <vector>
#include "Params.h"

// LIMÃO — presets de fábrica. Cada um traz o som E o padrão do sequenciador.
// O banco FACTORY 1 está em presets/fabrica.inc (GERADO por tools/gerar-presets.py); bancos avulsos (.lmobank)
// e os presets do usuário são lidos pelo processador.
// Padrão em texto: um item por passo, separados por espaço —
//   número = nota em semitons a partir do Dó 2 (0 = C2, 12 = C3, −2 = A#1), "-" = pausa, "~" = ligadura (segura a anterior)
//   acc/sld: "x" = accent / slide naquele passo, "." = sem
// O que o preset não traz volta ao padrão; o liga/desliga do sequenciador é performance e não muda.
namespace limao
{
// ---------------- categorias (o preset guarda o ÍNDICE; o nome só aparece no menu) ----------------
inline constexpr int numCategories = 9;
inline const char* const categoryNames[numCategories] = { "CLÁSSICO", "ACID HOUSE", "ACID TECHNO", "TRANCE", "BASS",
                                                          "LEAD", "SQUELCH", "DUB", "FX" };

// como está escrito no código (fábrica): texto estático
struct FactoryPreset
{
    const char* name;
    int category;                                          // −1 = Init (fora das categorias)
    std::vector<std::pair<const char*, float>> values;
    const char* notes;
    const char* acc;
    const char* sld;
};

// como o processador usa (fábrica e bancos importados)
struct PresetData
{
    juce::String name;
    int category = -1;
    std::vector<std::pair<juce::String, float>> values;
    juce::String notes, acc, sld;
};

// o Init primeiro e depois o banco FACTORY 1 (40 por categoria), gerado por tools/gerar-presets.py
inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "Init", -1, {}, "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", "................", "................" },
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
        p.notes = f.notes;
        p.acc = f.acc;
        p.sld = f.sld;
        out.push_back (std::move (p));
    }
    return out;
}

// texto do preset → padrão; devolve o número de passos
inline int parsePattern (const juce::String& notes, const juce::String& acc, const juce::String& sld, Pattern& out)
{
    out.clear();
    const auto items = juce::StringArray::fromTokens (notes, " ", "");
    const int n = juce::jmin (maxSteps, items.size());
    for (int i = 0; i < n; ++i)
    {
        Step s;
        const auto t = items[i];
        if (t == "-")      s.mode = Mode::rest;
        else if (t == "~") s.mode = Mode::tie;
        else               { s.mode = Mode::note; s.pitch = juce::jlimit (-12, 24, t.getIntValue()); }
        s.accent = i < acc.length() && acc[i] == 'x';
        s.slide  = i < sld.length() && sld[i] == 'x';
        out.set (i, s);
    }
    return juce::jmax (1, n);
}

// padrão → texto (o mesmo formato dos presets), só os primeiros len passos
inline void patternToText (const Pattern& pat, int len, juce::String& notes, juce::String& acc, juce::String& sld)
{
    juce::StringArray items;
    acc = sld = {};
    for (int i = 0; i < juce::jlimit (1, maxSteps, len); ++i)
    {
        const auto s = pat.get (i);
        items.add (s.mode == Mode::rest ? juce::String ("-") : s.mode == Mode::tie ? juce::String ("~") : juce::String (s.pitch));
        acc << (s.accent ? "x" : ".");
        sld << (s.slide ? "x" : ".");
    }
    notes = items.joinIntoString (" ");
}

// ALEATORIZAR: linha ácida na escala menor (com uma blue note de vez em quando), muita tônica e oitava,
// accents e slides espalhados. Só mexe nos primeiros len passos.
inline void randomize (Pattern& pat, int len, juce::Random& rnd)
{
    static const int scale[] = { 0, 3, 5, 7, 10, 12, 15, 6 };
    static const int weight[] = { 30, 12, 8, 12, 10, 18, 6, 4 };
    int total = 0;
    for (int w : weight) total += w;

    for (int i = 0; i < len; ++i)
    {
        Step s;
        const int roll = rnd.nextInt (100);
        if (i > 0 && roll < 12)      s.mode = Mode::rest;
        else if (i > 0 && roll < 20) s.mode = Mode::tie;
        else
        {
            s.mode = Mode::note;
            int r = rnd.nextInt (total), k = 0;
            while (r >= weight[k]) { r -= weight[k]; ++k; }
            s.pitch = scale[k] - (rnd.nextInt (10) == 0 ? 12 : 0);
        }
        s.accent = rnd.nextInt (100) < 28;
        s.slide  = s.mode != Mode::rest && rnd.nextInt (100) < 22;
        pat.set (i, s);
    }
}
} // namespace limao
