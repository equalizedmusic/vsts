#pragma once

#include <vector>
#include "Params.h"

// LIMÃO — presets de fábrica. Cada um traz o som E o padrão do sequenciador.
// Padrão em texto: um item por passo, separados por espaço —
//   número = nota em semitons a partir do Dó 2 (0 = C2, 12 = C3, −2 = A#1), "-" = pausa, "~" = ligadura (segura a anterior)
//   acc/sld: "x" = accent / slide naquele passo, "." = sem
// O que o preset não traz volta ao padrão; o liga/desliga do sequenciador é performance e não muda.
namespace limao
{
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
    const char* notes;
    const char* acc;
    const char* sld;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "Init",
          {},
          "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
          "................", "................" },

        { "303 Clássico",
          { { "cutoff", 460.0f }, { "reso", 72.0f }, { "envmod", 62.0f }, { "decay", 340.0f }, { "accent", 70.0f }, { "slide", 60.0f },
            { "drive", 18.0f } },
          "0 0 12 0 ~ 3 0 15 0 10 - 0 7 0 12 3",
          "x...x..x...x..x.", "..x....x.....x.." },

        { "Acid House",
          { { "cutoff", 380.0f }, { "reso", 82.0f }, { "envmod", 70.0f }, { "decay", 520.0f }, { "accent", 75.0f }, { "slide", 70.0f },
            { "drive", 30.0f }, { "rvon", 1.0f }, { "rvmix", 14.0f } },
          "0 12 0 0 7 - 0 10 12 0 3 0 - 5 0 12",
          "x.....x.x...x...", ".x.....x.x....x." },

        { "Techno Pesado",
          { { "wave", 1.0f }, { "cutoff", 300.0f }, { "reso", 55.0f }, { "envmod", 72.0f }, { "decay", 220.0f }, { "accent", 80.0f },
            { "drvtype", 1.0f }, { "drive", 58.0f }, { "dlon", 1.0f }, { "dldiv", 3.0f }, { "dlfb", 35.0f }, { "dlmix", 16.0f } },
          "0 0 - 0 0 - 0 12 0 0 - 0 1 0 - 0",
          "x..x..x.x..x..x.", "......x........." },

        { "Psytrance Rolling",
          { { "cutoff", 820.0f }, { "reso", 42.0f }, { "envmod", 45.0f }, { "decay", 150.0f }, { "accent", 40.0f }, { "slide", 30.0f },
            { "drive", 25.0f }, { "volume", -5.0f } },
          "- 4 4 4 - 4 4 4 - 4 4 4 - 4 4 7",
          ".x...x...x...x..", "................" },

        { "Lead Gritado",
          { { "wave", 1.0f }, { "cutoff", 900.0f }, { "reso", 86.0f }, { "envmod", 58.0f }, { "decay", 700.0f }, { "accent", 70.0f },
            { "slide", 110.0f }, { "drvtype", 0.0f }, { "drive", 62.0f }, { "dlon", 1.0f }, { "dldiv", 3.0f }, { "dlfb", 55.0f },
            { "dlmix", 30.0f }, { "rvon", 1.0f }, { "rvsize", 60.0f }, { "rvmix", 22.0f }, { "seqlen", 32.0f } },
          "12 ~ 15 12 ~ 19 ~ 17 15 ~ 12 10 12 ~ - 7 12 ~ 15 17 ~ 19 22 ~ 24 ~ 19 17 15 ~ 12 -",
          "x....x..x...x.....x..x..x...x...", ".x...x...x...x...x..x...x...x..." },

        { "Squelch 32",
          { { "cutoff", 420.0f }, { "reso", 88.0f }, { "envmod", 66.0f }, { "decay", 300.0f }, { "accent", 85.0f }, { "slide", 55.0f },
            { "drive", 35.0f }, { "acid", 25.0f }, { "seqlen", 32.0f } },
          "0 0 12 3 0 - 15 0 0 10 0 7 12 0 - 3 0 0 12 5 0 - 17 0 0 10 0 7 15 12 - 0",
          "x..x...x..x...x.x..x..x...x..x..", "..x....x...x...x..x....x..x....x" },

        { "Borracha",
          { { "cutoff", 220.0f }, { "reso", 60.0f }, { "envmod", 85.0f }, { "decay", 600.0f }, { "accent", 50.0f }, { "slide", 120.0f },
            { "drive", 15.0f } },
          "0 ~ 7 0 ~ 12 0 ~ 5 7 ~ 0 3 ~ 0 ~",
          "x.....x.....x...", "x.x..x..x.x..x.." },

        { "Sereia",
          { { "cutoff", 900.0f }, { "reso", 96.0f }, { "envmod", 40.0f }, { "decay", 900.0f }, { "accent", 55.0f }, { "slide", 180.0f },
            { "drive", 22.0f }, { "dlon", 1.0f }, { "dldiv", 2.0f }, { "dlfb", 60.0f }, { "dlmix", 25.0f }, { "rvon", 1.0f }, { "rvmix", 25.0f } },
          "12 ~ 24 ~ 19 ~ 12 ~ 15 ~ 22 ~ 17 ~ 12 ~",
          "x.......x.......", "x.x.x.x.x.x.x.x." },

        { "Bitcrush Ácido",
          { { "cutoff", 600.0f }, { "reso", 70.0f }, { "envmod", 60.0f }, { "decay", 260.0f }, { "accent", 70.0f },
            { "drvtype", 2.0f }, { "drive", 48.0f } },
          "0 12 0 15 0 12 - 10 0 12 0 7 - 3 0 5",
          "x...x...x...x...", "...x.......x...." },

        { "Minimal",
          { { "cutoff", 520.0f }, { "reso", 50.0f }, { "envmod", 50.0f }, { "decay", 200.0f }, { "accent", 60.0f },
            { "drive", 10.0f }, { "dlon", 1.0f }, { "dldiv", 3.0f }, { "dlfb", 40.0f }, { "dlmix", 22.0f } },
          "0 - - 0 - - 12 - 0 - - - 3 - - -",
          "x.....x.....x...", "................" },

        { "Dub Ácido",
          { { "cutoff", 350.0f }, { "reso", 75.0f }, { "envmod", 55.0f }, { "decay", 450.0f }, { "accent", 60.0f }, { "slide", 80.0f },
            { "drive", 20.0f }, { "dlon", 1.0f }, { "dldiv", 3.0f }, { "dlfb", 72.0f }, { "dlmix", 38.0f },
            { "rvon", 1.0f }, { "rvsize", 75.0f }, { "rvmix", 28.0f }, { "seqrate", 4.0f } },
          "0 - 7 ~ 10 - 0 12",
          "x...x...", "..x....x" },

        { "Garagem Swing",
          { { "cutoff", 500.0f }, { "reso", 68.0f }, { "envmod", 60.0f }, { "decay", 320.0f }, { "accent", 65.0f },
            { "drive", 28.0f }, { "swing", 45.0f } },
          "0 0 3 0 5 0 3 0 0 12 0 10 7 0 5 3",
          "x...x...x...x...", "....x.......x..." },

        { "Ácido Total",
          { { "wave", 1.0f }, { "cutoff", 380.0f }, { "reso", 80.0f }, { "envmod", 70.0f }, { "decay", 260.0f }, { "accent", 90.0f },
            { "slide", 50.0f }, { "acid", 70.0f }, { "drvtype", 1.0f }, { "drive", 45.0f }, { "dlon", 1.0f }, { "dlmix", 18.0f } },
          "0 12 0 0 15 0 12 3 0 0 17 0 12 0 10 7",
          "x.x...x.x.x...x.", ".x...x...x...x.." },

        { "Sub Grave",
          { { "cutoff", 160.0f }, { "reso", 30.0f }, { "envmod", 35.0f }, { "decay", 500.0f }, { "accent", 40.0f }, { "tune", -12.0f },
            { "drive", 8.0f } },
          "0 ~ - 0 - 0 ~ - 0 ~ - 0 3 ~ 0 -",
          "x.......x.......", "................" },
    };
    return list;
}

// texto do preset → padrão; devolve o número de passos
inline int parsePattern (const FactoryPreset& p, Pattern& out)
{
    out.clear();
    const auto items = juce::StringArray::fromTokens (juce::String (p.notes), " ", "");
    const juce::String acc (p.acc), sld (p.sld);
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
