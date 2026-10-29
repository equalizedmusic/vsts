#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <juce_data_structures/juce_data_structures.h>

// O rack do MONTA-MONTA: quais peças existem, onde encaixam, o que cada lição pede e como a montagem vira o caminho
// do som (Graph) que o motor toca. Tudo aqui roda na thread da interface; o motor só recebe o Graph pronto.
namespace mm
{
enum Module : int { vco1, vco2, mixer, vcf, envAmp, output, envMod, numModules };

// entradas: as 4 de áudio e a de controle (CV) do corte do VCF
enum Input : int { inMix1, inMix2, inVcf, inVcfCv, inAmp, inOut, numInputs };

inline Module inputModule (int in)
{
    static const Module m[numInputs] = { mixer, mixer, vcf, vcf, envAmp, output };
    return m[in];
}
inline bool inputIsCv (int in) { return in == inVcfCv; }
inline bool outputIsCv (int m) { return m == envMod; }
inline bool hasOutput (int m) { return m != output; }

// fileira de cima do rack, da esquerda para a direita: o caminho do som
inline constexpr std::array<Module, 5> topRow { vco1, mixer, vcf, envAmp, output };

// ---------------- lições ----------------
inline constexpr int numLessons = 6;
inline constexpr int freeLesson = 6;   // lição 6 = modo livre (tudo no rack)

// peças que a lição pede (bit por peça). As que não aparecem: na fileira de cima viram PONTE (o som passa direto);
// na de baixo (VCO 2, ENV MOD) ficam trancadas.
inline uint32_t lessonPieces (int lesson)
{
    auto b = [] (Module m) { return 1u << (unsigned) m; };
    switch (lesson)
    {
        case 1:  return b (vco1) | b (output);
        case 2:  return b (vco1) | b (envAmp) | b (output);
        case 3:  return b (vco1) | b (vcf) | b (envAmp) | b (output);
        case 4:  return b (vco1) | b (vcf) | b (envAmp) | b (output) | b (envMod);
        default: return (1u << numModules) - 1u;
    }
}
inline bool inLesson (int lesson, int m) { return (lessonPieces (lesson) >> m) & 1u; }

// em que lição a peça aparece pela primeira vez
inline int lessonOf (int m)
{
    for (int l = 1; l <= numLessons; ++l)
        if (inLesson (l, m)) return l;
    return numLessons;
}

// ---------------- o caminho do som que o motor toca ----------------
struct Graph
{
    std::array<int8_t, numInputs> src {};       // peça ligada em cada entrada (-1: nada)
    std::array<int8_t, numModules> order {};    // peças que alcançam a SAÍDA, em ordem de cálculo
    int numOrder = 0;
    int probe = -1;                             // peça que o osciloscópio mostra (a última que tem som)

    Graph() { src.fill (-1); order.fill (-1); }
};

struct Cable
{
    int from = -1;   // peça (a saída dela)
    int to = -1;     // entrada (Input)
};

// a montagem do usuário (vai junto com o projeto)
struct Patch
{
    int lesson = 1;
    bool cables = false;                       // modo CABOS
    std::array<bool, numModules> placed {};    // peças no rack (modo ENCAIXE)
    std::vector<Cable> wires;                  // fios (modo CABOS)
    uint32_t challengesDone = 0;               // desafios marcados como FEITO (bit por peça)

    bool isBypass (int m) const      // peça da fileira de cima que a lição ainda não pede: placa PONTE
    {
        return ! cables && lesson < freeLesson && ! inLesson (lesson, m) && (m == mixer || m == vcf || m == envAmp);
    }
    bool isHidden (int m) const      // fileira de baixo ainda trancada
    {
        return ! cables && ! inLesson (lesson, m) && (m == vco2 || m == envMod);
    }
    bool isRequired (int m) const { return cables || inLesson (lesson, m); }
    bool present (int m) const { return cables || placed[(size_t) m]; }

    int wireAt (int in) const
    {
        for (int i = 0; i < (int) wires.size(); ++i)
            if (wires[(size_t) i].to == in) return i;
        return -1;
    }

    // ligar "from" em "in" criaria uma volta (o som saindo e voltando para a mesma peça)?
    bool makesLoop (int from, int in) const
    {
        Graph g = build (true);
        // a peça de destino alcança "from" pelos fios que já existem?
        const int target = inputModule (in);
        std::array<bool, numModules> seen {};
        std::vector<int> stack { from };
        while (! stack.empty())
        {
            const int m = stack.back();
            stack.pop_back();
            if (m == target) return true;
            if (seen[(size_t) m]) continue;
            seen[(size_t) m] = true;
            for (int i = 0; i < numInputs; ++i)
                if (inputModule (i) == m && g.src[(size_t) i] >= 0) stack.push_back (g.src[(size_t) i]);
        }
        return false;
    }

    // só as ligações (sem ordem nem sonda): "raw" = o que o usuário montou, para checar laço
    Graph build (bool raw = false) const
    {
        Graph g;
        if (cables)
        {
            for (const auto& w : wires)
                if (w.from >= 0 && w.from < numModules && w.to >= 0 && w.to < numInputs)
                    g.src[(size_t) w.to] = (int8_t) w.from;
        }
        else
        {
            // ENCAIXE: o som anda pela fileira de cima; PONTE deixa passar; buraco (peça pedida e ausente) corta
            int cur = placed[vco1] ? vco1 : -2;    // -2 = cortado
            for (size_t k = 1; k < topRow.size(); ++k)
            {
                const int m = topRow[k];
                if (isBypass (m)) continue;
                if (! placed[(size_t) m]) { cur = -2; continue; }
                const int8_t s = (int8_t) (cur >= 0 ? cur : -1);
                switch (m)
                {
                    case mixer:  g.src[inMix1] = s; g.src[inMix2] = placed[vco2] ? (int8_t) vco2 : (int8_t) -1; break;
                    case vcf:    g.src[inVcf] = s;  g.src[inVcfCv] = (placed[envMod] && isRequired (envMod)) ? (int8_t) envMod : (int8_t) -1; break;
                    case envAmp: g.src[inAmp] = s; break;
                    case output: g.src[inOut] = s; break;
                    default: break;
                }
                cur = m;
            }
        }
        if (raw) return g;

        // ordem de cálculo: todas as peças presentes, fontes primeiro (o osciloscópio pode olhar qualquer uma)
        std::array<int, numModules> state {};   // 0 nunca visto, 1 visitando, 2 pronto
        std::function<void (int)> visit = [&] (int m)
        {
            if (state[(size_t) m] != 0) return;
            state[(size_t) m] = 1;
            for (int i = 0; i < numInputs; ++i)
                if (inputModule (i) == m && g.src[(size_t) i] >= 0 && state[(size_t) g.src[(size_t) i]] == 0)
                    visit (g.src[(size_t) i]);
            state[(size_t) m] = 2;
            g.order[(size_t) g.numOrder++] = (int8_t) m;
        };
        for (int m = 0; m < numModules; ++m)
            if (present (m)) visit (m);
        return g;
    }

    juce::ValueTree toTree() const
    {
        juce::ValueTree t ("PATCH");
        t.setProperty ("lesson", lesson, nullptr);
        t.setProperty ("cables", cables, nullptr);
        int bits = 0;
        for (int m = 0; m < numModules; ++m)
            if (placed[(size_t) m]) bits |= 1 << m;
        t.setProperty ("placed", bits, nullptr);
        t.setProperty ("challenges", (int) challengesDone, nullptr);
        juce::String w;
        for (const auto& c : wires) w << c.from << ">" << c.to << " ";
        t.setProperty ("wires", w.trim(), nullptr);
        return t;
    }

    void fromTree (const juce::ValueTree& t)
    {
        if (! t.isValid()) return;
        lesson = juce::jlimit (1, numLessons, (int) t.getProperty ("lesson", 1));
        cables = (bool) t.getProperty ("cables", false);
        const int bits = (int) t.getProperty ("placed", 0);
        for (int m = 0; m < numModules; ++m) placed[(size_t) m] = ((bits >> m) & 1) != 0;
        challengesDone = (uint32_t) (int) t.getProperty ("challenges", 0);
        wires.clear();
        for (const auto& tok : juce::StringArray::fromTokens (t.getProperty ("wires").toString(), " ", {}))
        {
            const int a = tok.upToFirstOccurrenceOf (">", false, false).getIntValue();
            const int b = tok.fromFirstOccurrenceOf (">", false, false).getIntValue();
            if (a >= 0 && a < numModules && b >= 0 && b < numInputs && outputIsCv (a) == inputIsCv (b) && inputModule (b) != a
                && wireAt (b) < 0)
                wires.push_back ({ a, b });
        }
    }
};

// ---------------- até onde o som chega (para a faixa CAMINHO DO SOM e o osciloscópio) ----------------
struct Reach
{
    std::array<bool, numModules> sound {};   // a saída da peça tem som
    bool atOutput = false;
};

inline Reach computeReach (const Patch& p, const Graph& g, bool noiseOn)
{
    Reach r;
    // ordem fixa resolve tudo em até numModules passadas (sem laço, garantido ao ligar)
    for (int pass = 0; pass < numModules; ++pass)
        for (int m = 0; m < numModules; ++m)
        {
            if (! p.present (m)) continue;
            auto in = [&] (int i) { return g.src[(size_t) i] >= 0 && r.sound[(size_t) g.src[(size_t) i]]; };
            bool s = false;
            switch (m)
            {
                case vco1: case vco2: s = true; break;
                case mixer:  s = in (inMix1) || in (inMix2) || noiseOn; break;
                case vcf:    s = in (inVcf); break;
                case envAmp: s = in (inAmp); break;
                case output: s = in (inOut); break;
                default:     s = false; break;   // ENV MOD não faz som: só mexe no corte
            }
            r.sound[(size_t) m] = s;
        }
    r.atOutput = r.sound[output];
    return r;
}
} // namespace mm
