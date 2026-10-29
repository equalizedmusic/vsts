#pragma once

#include <array>
#include <functional>
#include <vector>
#include "Params.h"

// RESENHA — da frase escrita (Pattern) à conversa que toca: quem fala cada nota e qual nota a resposta faz.
// A MESMA função serve ao áudio (a cada bloco, sem alocar) e à tela (piano-roll), então o que se vê é o que toca.
//
// Conversa: as notas são contadas desde o começo da frase; a cada TROCA A CADA notas vira a vez (turno) do próximo
// timbre, na ORDEM escolhida. Turnos pares PERGUNTAM (tocam como está escrito) e os ímpares RESPONDEM: com ECO repetem
// as notas da pergunta, com VARIA repetem mudando algumas, com INVERTE espelham (o que subia desce), com LIVRE tocam
// o que está escrito. A resposta ainda sobe/desce pela ALTURA DA RESPOSTA. O ritmo é sempre o do passo.
// A última nota de cada fala (antes de passar a vez) fica marcada: é nela que entra o BEND do timbre.
namespace resenha
{
struct PhraseParams
{
    int steps = 32, scale = 4, root = 1, voices = 3, every = 4;
    Order order = Order::sequence;
    Response response = Response::echo;
    int interval = 1;
};

struct PhraseNote
{
    int  step = 0, len = 1, note = 60, voice = 0;
    float gate = 0.5f;      // fração do último passo da nota
    bool accent = false, response = false;
    bool last = false;      // fecha a fala (a próxima nota é de outro turno, ou a frase acabou)
};

struct Phrase
{
    std::array<PhraseNote, maxSteps> notes {};
    std::array<int, maxSteps> atStep {};      // índice da nota que começa no passo (−1 = nenhuma)
    int count = 0, steps = 32;
};

// ---------------- escala ----------------
struct ScaleMap
{
    int semis[12] {};
    int n = 0;
    bool chromatic = true;

    ScaleMap (int scale, int)
    {
        chromatic = ! scales::active (scale);
        for (int d = 0; d < 12; ++d)
            if (chromatic || ((scales::masks[scale] >> d) & 1) != 0) semis[n++] = d;
    }

    // grau → semitons acima da tônica
    int toSemis (int deg) const
    {
        const int oct = deg >= 0 ? deg / n : -((-deg + n - 1) / n);
        return oct * 12 + semis[deg - oct * n];
    }
};

inline int baseNote (int root) { return 60 + root; }    // a tônica na oitava do Dó 3 (o lead fica em volta dela)

inline int intervalSemis (int interval)
{
    switch (interval) { case 1: return 12; case 2: return 7; case 3: return -12; default: return 0; }
}

// hash simples e determinístico (SORTEIO e VARIA sem mudar a cada volta da frase)
inline unsigned mix (unsigned x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

inline int voiceForTurn (int turn, const PhraseParams& p)
{
    const int V = juce::jlimit (1, numVoices, p.voices);
    if (V == 1) return 0;
    switch (p.order)
    {
        case Order::pingPong:
        {
            const int period = 2 * V - 2, q = turn % period;
            return q < V ? q : period - q;
        }
        case Order::random:
        {
            // nunca o mesmo timbre duas vezes seguidas
            int v = 0;
            for (int t = 1; t <= turn; ++t)
                v = (v + 1 + (int) (mix ((unsigned) t * 2654435761U) % (unsigned) (V - 1))) % V;
            return v;
        }
        default: return turn % V;
    }
}

inline void buildPhrase (const Pattern& pat, const PhraseParams& p, Phrase& out)
{
    out.count = 0;
    out.steps = juce::jlimit (1, maxSteps, p.steps);
    out.atStep.fill (-1);
    const ScaleMap sm (p.scale, p.root);
    const int base = baseNote (p.root);
    const int every = juce::jmax (1, p.every);

    std::array<int, 8> callDeg {};      // graus da última pergunta (até 8 notas por turno)
    int callCount = 0, k = 0;

    for (int i = 0; i < out.steps; ++i)
    {
        const Step st = pat.get (i);
        if (st.mode != Mode::note) continue;

        int len = 1;
        while (i + len < out.steps && pat.get (i + len).mode == Mode::tie) ++len;

        const int turn = k / every, pos = k % every;
        const bool answer = (turn % 2) == 1;
        if (! answer)
        {
            if (pos == 0) callCount = 0;
            if (pos < (int) callDeg.size()) { callDeg[(size_t) pos] = st.deg; callCount = pos + 1; }
        }

        int deg = st.deg;
        if (answer && pos < callCount)
        {
            const int c = callDeg[(size_t) pos];
            switch (p.response)
            {
                case Response::echo:   deg = c; break;
                case Response::vary:   deg = c + (st.var < 6 ? 0 : st.var < 10 ? 1 : st.var < 13 ? -1 : 2); break;
                case Response::invert: deg = 2 * callDeg[0] - c; break;
                case Response::free:   break;
            }
        }

        int note = base + sm.toSemis (juce::jlimit (-21, 28, deg));
        if (answer)
        {
            note += intervalSemis (p.interval);
            if (p.interval == 2) note = scales::snap (note, p.scale, p.root);
        }

        auto& pn = out.notes[(size_t) out.count];
        pn.step = i;
        pn.len = len;
        pn.note = juce::jlimit (24, 108, note);
        pn.voice = voiceForTurn (turn, p);
        pn.gate = (float) (st.gate + 1) / 16.0f;
        pn.accent = st.accent;
        pn.response = answer;
        pn.last = pos == every - 1;
        out.atStep[(size_t) i] = out.count;
        ++out.count;
        ++k;
    }
    if (out.count > 0) out.notes[(size_t) out.count - 1].last = true;
}

// ======================================================================================
// GERADOR (thread da interface). Só mexe nos primeiros 'steps' passos.
// ======================================================================================

// RITMO: grupos de 4 passos tirados de células de psy (com a densidade decidindo quantas notas). O grau de cada
// passo fica: a melodia não se perde quando só o ritmo muda.
inline void generateRhythm (Pattern& pat, int steps, float density, juce::Random& rnd)
{
    // n = nota, - = pausa, ~ = ligadura; da mais vazia para a mais cheia
    static const char* const cells[] = { "n---", "n-~-", "n~~~", "n--n", "n-n-", "nn--", "n~-n", "-nn-", "n-nn", "nn-n",
                                         "nn~n", "n~nn", "nnn-", "-nnn", "nnnn", "nnnn" };
    constexpr int numCells = (int) (sizeof (cells) / sizeof (cells[0]));
    const float d = juce::jlimit (0.0f, 1.0f, density);
    for (int g = 0; g < steps; g += 4)
    {
        // centro na densidade, com espalhamento: às vezes um grupo bem cheio, às vezes um respiro
        const float centre = d * (float) (numCells - 1);
        int c = juce::roundToInt (centre + (rnd.nextFloat() - 0.5f) * 7.0f);
        if (rnd.nextInt (8) == 0) c = rnd.nextInt (numCells);
        c = juce::jlimit (0, numCells - 1, c);
        for (int j = 0; j < 4 && g + j < steps; ++j)
        {
            auto s = pat.get (g + j);
            const char ch = cells[c][j];
            s.mode = ch == 'n' ? Mode::note : ch == '~' ? Mode::tie : Mode::rest;
            pat.set (g + j, s);
        }
    }
    // a frase sempre começa falando
    auto first = pat.get (0);
    first.mode = Mode::note;
    pat.set (0, first);
}

// NOTAS: um motivo curto (4 a 8 notas) andando pela escala, repetido com variações (sobe um grau, desce, volta à
// tônica) — soa como frase e não como sorteio. OITAVAS = quantas oitavas o lead pode andar.
// CONTORNO: MOTIVO é isso; CORRIDA/QUEDA/ONDA fazem escadas de 4 a 8 passos (de grau em grau ou em arpejo, pulando
// um grau) que sobem, descem ou sobem e voltam — o desenho dos leads de full on que terminam num mergulho.
inline void generateRuns (Pattern& pat, int steps, int per, int top, Contour contour, const std::function<int (int)>& toStored,
                          juce::Random& rnd)
{
    const int stride = rnd.nextInt (3) == 0 ? 1 : 2;           // de grau em grau ou em arpejo (terças)
    top = juce::jmin (top, per + 3);                            // escada de no máximo oitava e meia (lead, não harpa)
    const int span = juce::jmax (per, top);
    int i = 0;
    while (i < steps)
    {
        const int len = 4 + 2 * rnd.nextInt (3);                // 4, 6 ou 8 passos
        int start;
        if (contour == Contour::fall) start = span - rnd.nextInt (3);
        else                          start = rnd.nextInt (3) - 1 + (rnd.nextInt (4) == 0 ? per / 2 : 0);
        for (int j = 0; j < len && i < steps; ++j, ++i)
        {
            int d;
            if (contour == Contour::run)       d = start + j * stride;
            else if (contour == Contour::fall) d = start - j * stride;
            else                               d = start + (j < len / 2 ? j : len - 1 - j) * stride;   // ONDA
            while (d > top + 1) d -= per;
            while (d < -2) d += per;
            auto s = pat.get (i);
            s.deg = juce::jlimit (-14, 21, toStored (d));
            s.var = rnd.nextInt (16);
            pat.set (i, s);
        }
    }
}

inline void generateNotes (Pattern& pat, int steps, int scale, int octaves, juce::Random& rnd, Contour contour = Contour::motif)
{
    const int n = ScaleMap (scale, 0).n;
    const bool chromatic = n == 12;
    // CHROMATIC: o gerador usa os graus da menor natural (em semitons), senão a linha fica sem chão
    static const int minorSemis[7] = { 0, 2, 3, 5, 7, 8, 10 };
    const int per = chromatic ? 7 : n;
    const int top = per * juce::jlimit (1, 3, octaves);
    auto toStored = [&] (int d)
    {
        if (! chromatic) return d;
        const int oct = d >= 0 ? d / 7 : -((-d + 6) / 7);
        return oct * 12 + minorSemis[d - oct * 7];
    };
    if (contour != Contour::motif)
    {
        generateRuns (pat, steps, per, top, contour, toStored, rnd);
        return;
    }

    const int motifLen = 4 + rnd.nextInt (5);
    std::vector<int> motif;
    int d = rnd.nextInt (3) == 0 ? per : 0;
    for (int i = 0; i < motifLen; ++i)
    {
        motif.push_back (d);
        const int r = rnd.nextInt (100);
        int move = r < 30 ? 1 : r < 55 ? -1 : r < 68 ? 2 : r < 80 ? -2 : r < 88 ? 0 : (rnd.nextBool() ? per : -per);
        if (rnd.nextInt (6) == 0) move = (rnd.nextBool() ? 0 : per) - d;      // volta à tônica ou à oitava
        d = juce::jlimit (-2, top, d + move);
    }

    static const int shifts[] = { 0, 0, 1, -1, 2, 0, -2, 3 };
    for (int i = 0; i < steps; ++i)
    {
        const int rep = i / motifLen;
        int deg = motif[(size_t) (i % motifLen)] + (rep == 0 ? 0 : shifts[rnd.nextInt (8)] * (rnd.nextInt (3) == 0 ? 1 : 0));
        if (rnd.nextInt (12) == 0) deg += rnd.nextBool() ? 1 : -1;
        deg = juce::jlimit (-2, top, deg);
        auto s = pat.get (i);
        s.deg = juce::jlimit (-14, 21, toStored (deg));
        s.var = rnd.nextInt (16);
        pat.set (i, s);
    }
}

// GATE: duração de cada nota em volta do GATE, espalhada pela VAR. GATE; accents de vez em quando
inline void generateGate (Pattern& pat, int steps, float gate, float gateVar, juce::Random& rnd)
{
    for (int i = 0; i < steps; ++i)
    {
        auto s = pat.get (i);
        const float g = gate + (rnd.nextFloat() * 2.0f - 1.0f) * gateVar * 0.6f;
        s.gate = juce::jlimit (0, 15, juce::roundToInt (juce::jlimit (0.05f, 1.0f, g) * 16.0f) - 1);
        s.accent = rnd.nextInt (100) < (i % 4 == 0 ? 30 : 12);
        pat.set (i, s);
    }
}

// ---------------- texto (presets de fábrica e bancos) ----------------
// um item por passo, separados por espaço: "-" pausa, "~" ligadura, ou o grau com ":g" (gate 0..15, opcional) e
// "!" (accent, opcional). Ex.: "0:7 2:5! - 4:9 ~ -1:3"
inline int parsePattern (const juce::String& text, Pattern& out)
{
    out.clear();
    const auto items = juce::StringArray::fromTokens (text, " ", "");
    const int n = juce::jmin (maxSteps, items.size());
    for (int i = 0; i < n; ++i)
    {
        Step s;
        auto t = items[i];
        s.var = (int) (mix ((unsigned) i * 7919U + 17U) % 16U);
        if (t == "-")      s.mode = Mode::rest;
        else if (t == "~") s.mode = Mode::tie;
        else
        {
            s.mode = Mode::note;
            s.accent = t.endsWithChar ('!');
            t = t.trimCharactersAtEnd ("!");
            s.deg = juce::jlimit (-14, 21, t.upToFirstOccurrenceOf (":", false, false).getIntValue());
            if (t.containsChar (':')) s.gate = juce::jlimit (0, 15, t.fromFirstOccurrenceOf (":", false, false).getIntValue());
        }
        out.set (i, s);
    }
    // pausa e ligadura herdam o grau da nota anterior (para o ritmo novo não cair no grau 0)
    int last = 0;
    for (int i = 0; i < n; ++i)
    {
        auto s = out.get (i);
        if (s.mode == Mode::note) last = s.deg;
        else { s.deg = last; out.set (i, s); }
    }
    return juce::jmax (1, n);
}

inline juce::String patternToText (const Pattern& pat, int steps)
{
    juce::StringArray items;
    for (int i = 0; i < juce::jlimit (1, maxSteps, steps); ++i)
    {
        const auto s = pat.get (i);
        if (s.mode == Mode::rest)     items.add ("-");
        else if (s.mode == Mode::tie) items.add ("~");
        else items.add (juce::String (s.deg) + ":" + juce::String (s.gate) + (s.accent ? "!" : ""));
    }
    return items.joinIntoString (" ");
}
} // namespace resenha
