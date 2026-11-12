#pragma once

#include <algorithm>
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

// AUTO ALEATÓRIO (0.6.1): linha NOVA que evolui a partir da que está tocando (cur), presa à linha do usuário (anchor).
//  - SCALE e ROOT não mudam. As notas novas saem da escala, só entre a mais grave e a mais aguda que o usuário
//    usou (sem SCALE: só as notas que a linha dele já tem); os graus que ele mais usa pesam mais.
//  - Progressiva: cada linha nasce da anterior. Parte das notas fica, parte anda um grau para cima/baixo, uma ou
//    outra salta para um grau da linha do usuário (puxa de volta, não deriva). O ritmo troca só 1 ou 2 passos.
//  - Mesma proporção da linha do usuário: o mesmo número de notas, pausas, ligaduras, accents e slides
//    (linha com poucas notas e muitas pausas/slides continua assim, não vira linha cheia).
// Âncora sem nenhuma nota (só pausas) = linha aleatória normal (senão o AUTO ficaria mudo para sempre).
inline void randomizeLike (Pattern& pat, const Pattern& cur, const Pattern& anchor, int len, juce::Random& rnd,
                           int scaleIdx = 0, int rootNote = 0)
{
    len = juce::jlimit (1, maxSteps, len);
    int nRest = 0, nTie = 0, nNote = 0, nAcc = 0, nSld = 0, lo = 99, hi = -99;
    std::vector<int> used;
    for (int i = 0; i < len; ++i)
    {
        const Step s = anchor.get (i);
        if (s.mode == Mode::rest) { ++nRest; continue; }
        if (s.mode == Mode::tie) ++nTie;
        else { ++nNote; used.push_back (s.pitch); lo = juce::jmin (lo, s.pitch); hi = juce::jmax (hi, s.pitch); }
        if (s.accent) ++nAcc;
        if (s.slide)  ++nSld;
    }
    if (nNote == 0) { randomize (pat, len, rnd, scaleIdx, rootNote); return; }

    // notas permitidas (em ordem) e o peso de cada uma (grau que o usuário usa = mais provável)
    std::vector<int> allowed;
    if (scales::active (scaleIdx))
        for (int n = lo; n <= hi; ++n)
            if (scales::contains (36 + n, scaleIdx, rootNote)) allowed.push_back (n);
    for (int p : used)
        if (std::find (allowed.begin(), allowed.end(), p) == allowed.end()) allowed.push_back (p);
    std::sort (allowed.begin(), allowed.end());
    std::vector<int> weight;
    int total = 0;
    for (int n : allowed)
    {
        const int w = 1 + 3 * (int) std::count (used.begin(), used.end(), n);
        weight.push_back (w);
        total += w;
    }
    auto weighted = [&]
    {
        int r = rnd.nextInt (total), k = 0;
        while (r >= weight[(size_t) k]) { r -= weight[(size_t) k]; ++k; }
        return allowed[(size_t) k];
    };
    auto indexOf = [&] (int p)
    {
        int best = 0;
        for (int k = 1; k < (int) allowed.size(); ++k)
            if (std::abs (allowed[(size_t) k] - p) < std::abs (allowed[(size_t) best] - p)) best = k;
        return best;
    };

    // ---- ritmo: o da linha atual (o passo 1 nunca muda) com 1 ou 2 trocas de lugar entre os outros passos ----
    std::vector<Mode> modes ((size_t) len);
    int cNote = 0, cRest = 0, cTie = 0;
    for (int i = 0; i < len; ++i)
    {
        modes[(size_t) i] = cur.get (i).mode;
        cNote += modes[(size_t) i] == Mode::note;
        cRest += modes[(size_t) i] == Mode::rest;
        cTie  += modes[(size_t) i] == Mode::tie;
    }
    auto mute = [&] (const std::vector<Mode>& m)                  // ligaduras mudas (logo depois de pausa)
    {
        int c = 0;
        for (int i = 1; i < len; ++i)
            c += m[(size_t) i] == Mode::tie && m[(size_t) i - 1] == Mode::rest;
        return c;
    };
    if (cNote != nNote || cRest != nRest || cTie != nTie)
    {
        // a linha atual não tem a proporção do usuário: monta do zero (notas e pausas embaralhadas, ligadura
        // sempre depois de nota ou de outra ligadura)
        std::vector<Mode> deck;
        deck.insert (deck.end(), (size_t) (nNote - 1), Mode::note);
        deck.insert (deck.end(), (size_t) nRest, Mode::rest);
        for (int i = (int) deck.size() - 1; i > 0; --i)
            std::swap (deck[(size_t) i], deck[(size_t) rnd.nextInt (i + 1)]);
        deck.insert (deck.begin(), Mode::note);
        for (int k = 0; k < nTie; ++k)
        {
            std::vector<int> after;
            for (int i = 0; i < (int) deck.size(); ++i)
                if (deck[(size_t) i] != Mode::rest) after.push_back (i);
            deck.insert (deck.begin() + after[(size_t) rnd.nextInt ((int) after.size())] + 1, Mode::tie);
        }
        modes = deck;
    }
    else if (len > 2)
    {
        const int moves = 1 + rnd.nextInt (2);
        for (int m = 0; m < moves; ++m)
            for (int tries = 0; tries < 40; ++tries)
            {
                const int i = 1 + rnd.nextInt (len - 1), j = 1 + rnd.nextInt (len - 1);
                if (modes[(size_t) i] == modes[(size_t) j]) continue;
                auto t = modes;
                std::swap (t[(size_t) i], t[(size_t) j]);
                if (mute (t) <= mute (modes)) { modes = t; break; }   // a troca não cria ligadura muda
            }
    }

    // ---- notas: cada uma parte da nota da linha atual naquele ponto e anda pouco ----
    std::vector<int> base ((size_t) len);
    {
        int last = cur.get (0).pitch;
        for (int i = len - 1; i >= 0; --i)                        // última nota da linha (o começo dá a volta)
            if (cur.get (i).mode == Mode::note) { last = cur.get (i).pitch; break; }
        for (int i = 0; i < len; ++i)
        {
            if (cur.get (i).mode == Mode::note) last = cur.get (i).pitch;
            base[(size_t) i] = last;
        }
    }
    std::vector<int> pitch ((size_t) len, 0);
    bool changed = false;
    for (int i = 0; i < len; ++i)
    {
        if (modes[(size_t) i] != Mode::note) continue;
        int k = indexOf (base[(size_t) i]);
        const int roll = rnd.nextInt (100);
        if (roll >= 45 && roll < 85 && allowed.size() > 1)       // anda um grau (sempre para dentro da faixa)
        {
            int d = rnd.nextBool() ? 1 : -1;
            if (k + d < 0 || k + d >= (int) allowed.size()) d = -d;
            k += d;
            pitch[(size_t) i] = allowed[(size_t) k];
        }
        else if (roll >= 85) pitch[(size_t) i] = weighted();      // salta para um grau da linha do usuário
        else                 pitch[(size_t) i] = allowed[(size_t) k];
        if (pitch[(size_t) i] != base[(size_t) i]) changed = true;
    }
    if (! changed && allowed.size() > 1)                          // nunca repete a mesma linha
    {
        std::vector<int> notes;
        for (int i = 0; i < len; ++i)
            if (modes[(size_t) i] == Mode::note) notes.push_back (i);
        const int i = notes[(size_t) rnd.nextInt ((int) notes.size())];
        const int k = indexOf (pitch[(size_t) i]);
        pitch[(size_t) i] = allowed[(size_t) (k + 1 < (int) allowed.size() ? k + 1 : k - 1)];
    }

    // ---- accents e slides: os da linha atual, na quantidade da linha do usuário, um muda de lugar ----
    auto flags = [&] (bool isAcc, int target)
    {
        std::vector<int> onIdx, offIdx;
        for (int i = 0; i < len; ++i)
        {
            if (modes[(size_t) i] == Mode::rest) continue;
            const Step c = cur.get (i);
            if (isAcc ? c.accent : c.slide) onIdx.push_back (i); else offIdx.push_back (i);
        }
        auto takeRandom = [&] (std::vector<int>& v)
        {
            const int j = rnd.nextInt ((int) v.size());
            const int x = v[(size_t) j];
            v.erase (v.begin() + j);
            return x;
        };
        while ((int) onIdx.size() > target)                     offIdx.push_back (takeRandom (onIdx));
        while ((int) onIdx.size() < target && ! offIdx.empty()) onIdx.push_back (takeRandom (offIdx));
        if (! onIdx.empty() && ! offIdx.empty())
        {
            const int a = takeRandom (onIdx), b = takeRandom (offIdx);
            onIdx.push_back (b);
            offIdx.push_back (a);
        }
        std::vector<bool> on ((size_t) len, false);
        for (int i : onIdx) on[(size_t) i] = true;
        return on;
    };
    const auto acc = flags (true, nAcc);
    const auto sld = flags (false, nSld);

    int lastPitch = pitch[0];
    for (int i = 0; i < len; ++i)
    {
        Step s;
        s.mode = modes[(size_t) i];
        if (s.mode == Mode::note) lastPitch = pitch[(size_t) i];
        s.pitch  = lastPitch;
        s.accent = acc[(size_t) i];
        s.slide  = sld[(size_t) i];
        pat.set (i, s);
    }
}
} // namespace limao
