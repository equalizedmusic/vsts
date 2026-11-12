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

// ALEATORIZAR: linha ácida, muita tônica e oitava, accents e slides espalhados. Só mexe nos primeiros len passos.
// Sem SCALE (CHROMATIC): menor em Dó com uma blue note de vez em quando. Com SCALE + ROOT: só notas da escala,
// a partir da tônica escolhida (tônica de F# para cima começa uma oitava abaixo, para o baixo não subir demais).
inline void randomize (Pattern& pat, int len, juce::Random& rnd, int scaleIdx = 0, int rootNote = 0)
{
    std::vector<int> scale { 0, 3, 5, 7, 10, 12, 15, 6 };
    std::vector<int> weight { 30, 12, 8, 12, 10, 18, 6, 4 };
    if (scales::active (scaleIdx))
    {
        scale.clear();
        weight.clear();
        const int base = rootNote > 5 ? rootNote - 12 : rootNote;   // relativo ao Dó 2
        for (int n = base; n <= base + 15; ++n)
        {
            if (! scales::contains (36 + n, scaleIdx, rootNote)) continue;
            const int d = n - base;
            scale.push_back (n);
            weight.push_back (d == 0 ? 30 : d == 12 ? 18 : d == 7 ? 12 : d > 12 ? 4 : 8);
        }
    }
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
            s.pitch = scale[(size_t) k];
            if (rnd.nextInt (10) == 0 && s.pitch - 12 >= -12) s.pitch -= 12;   // uma oitava abaixo de vez em quando
        }
        s.accent = rnd.nextInt (100) < 28;
        s.slide  = s.mode != Mode::rest && rnd.nextInt (100) < 22;
        pat.set (i, s);
    }
}

// AUTO ALEATÓRIO: linha nova com a MESMA proporção da linha de referência (a que está tocando): as mesmas notas e o
// mesmo número de pausas, ligaduras, accents e slides nos primeiros len passos, só que em outra ordem.
// Assim uma linha com poucas notas e muitas pausas/slides continua assim, em vez de virar uma linha cheia.
// Referência sem nenhuma nota (só pausas) = linha aleatória normal (senão o AUTO ficaria mudo para sempre).
inline void randomizeLike (Pattern& pat, const Pattern& ref, int len, juce::Random& rnd, int scaleIdx = 0, int rootNote = 0)
{
    int nRest = 0, nTie = 0, nNote = 0, nAcc = 0, nSld = 0;
    for (int i = 0; i < len; ++i)
    {
        const Step s = ref.get (i);
        if (s.mode == Mode::rest) { ++nRest; continue; }
        if (s.mode == Mode::tie) ++nTie; else ++nNote;
        if (s.accent) ++nAcc;
        if (s.slide)  ++nSld;
    }
    if (nNote == 0) { randomize (pat, len, rnd, scaleIdx, rootNote); return; }

    // NOTAS: as mesmas da linha de referência, só em outra ordem (não sorteia nota nova: SCALE, ROOT e o
    // desenho melódico que o usuário escolheu ficam; o que muda é onde cada nota cai)
    std::vector<int> pitchPool;
    for (int i = 0; i < len; ++i)
        if (ref.get (i).mode == Mode::note) pitchPool.push_back (ref.get (i).pitch);
    for (int i = (int) pitchPool.size() - 1; i > 0; --i)
        std::swap (pitchPool[(size_t) i], pitchPool[(size_t) rnd.nextInt (i + 1)]);

    // baralho de modos: notas e pausas embaralhadas (passo 0 sempre nota); cada ligadura entra logo depois de uma
    // nota ou de outra ligadura, nunca depois de pausa (lá ela ficaria muda e a linha perderia nota)
    std::vector<Mode> best;
    best.insert (best.end(), (size_t) (nNote - 1), Mode::note);
    best.insert (best.end(), (size_t) nRest, Mode::rest);
    for (int i = (int) best.size() - 1; i > 0; --i)
        std::swap (best[(size_t) i], best[(size_t) rnd.nextInt (i + 1)]);
    best.insert (best.begin(), Mode::note);
    for (int k = 0; k < nTie; ++k)
    {
        std::vector<int> after;
        for (int i = 0; i < (int) best.size(); ++i)
            if (best[(size_t) i] != Mode::rest) after.push_back (i);
        const int at = after[(size_t) rnd.nextInt ((int) after.size())];
        best.insert (best.begin() + at + 1, Mode::tie);
    }

    // accents e slides: mesma quantidade, espalhados entre os passos que soam (nota ou ligadura)
    std::vector<int> sounding;
    for (int i = 0; i < len; ++i)
        if (best[(size_t) i] != Mode::rest) sounding.push_back (i);
    auto pick = [&] (int count)
    {
        std::vector<bool> on ((size_t) len, false);
        std::vector<int> pool = sounding;
        for (int k = 0; k < count && ! pool.empty(); ++k)
        {
            const int j = rnd.nextInt ((int) pool.size());
            on[(size_t) pool[(size_t) j]] = true;
            pool.erase (pool.begin() + j);
        }
        return on;
    };
    const auto acc = pick (nAcc);
    const auto sld = pick (nSld);

    int lastPitch = pitchPool[0];
    size_t nextNote = 0;
    for (int i = 0; i < len; ++i)
    {
        Step s;
        s.mode = best[(size_t) i];
        if (s.mode == Mode::note) lastPitch = pitchPool[nextNote++];
        s.pitch  = lastPitch;
        s.accent = acc[(size_t) i];
        s.slide  = sld[(size_t) i];
        pat.set (i, s);
    }
}
} // namespace limao
