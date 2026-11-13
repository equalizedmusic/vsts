#pragma once

#include <array>
#include <functional>
#include <vector>
#include "Params.h"

// RESENHA — da frase escrita (Pattern) à conversa que toca: quem fala cada nota e qual nota a resposta faz.
// A MESMA função serve ao áudio (a cada bloco, sem alocar) e à tela (piano-roll), então o que se vê é o que toca.
//
// Conversa: a cada TROCA A CADA notas — ou a cada ½/1 compasso (por TEMPO) — vira a vez (turno) do próximo
// timbre, na ORDEM escolhida. Turnos pares PERGUNTAM (tocam como está escrito) e os ímpares RESPONDEM: com ECO repetem
// as notas da pergunta, com VARIA repetem mudando algumas, com INVERTE espelham (o que subia desce), com LIVRE tocam
// o que está escrito. A resposta ainda sobe/desce pela ALTURA DA RESPOSTA. O ritmo é sempre o do passo.
// A resposta FECHA na tônica (a última nota dela cai na tônica mais perto, menos no LIVRE): a pergunta fica no ar e a
// resposta conclui. A última nota de cada fala (antes de passar a vez) fica marcada: é nela que entra o BEND do timbre.
// PAPO (0.4): o quanto isso soa como conversa e não como sequência. Com PAPO > 0 as falas têm tamanhos diferentes
// (quem responde pode falar uma nota a mais ou a menos), quem pergunta respira antes da resposta (a última nota dela
// encurta) ou a resposta entra POR CIMA (a nota que fecha a pergunta segue soando), a resposta chega um pouco atrasada
// (fora da grade), notas vizinhas da mesma fala viram legato (gate cheio: o GLIDE liga uma na outra), a última nota da
// fala se alonga sobre a pausa seguinte (como quem termina a frase) e o acento cai no começo de cada fala.
// Tudo por sorteio determinístico (hash do passo): a mesma frase toca sempre igual e a tela mostra o que toca.
namespace resenha
{
struct PhraseParams
{
    int steps = 32, scale = 4, root = 1, voices = 3, every = 4;    // every < 0 = tempos por turno (−2 = ½ compasso)
    double stepsPerBeat = 4.0;
    Order order = Order::sequence;
    Response response = Response::echo;
    int interval = 1;
    float talk = 0.0f;      // PAPO 0..1
    bool straight = false;  // CONTORNO = RETA: toda nota na tônica
};

struct PhraseNote
{
    int  step = 0, len = 1, note = 60, voice = 0;
    float gate = 0.5f;      // fração do último passo da nota
    float delay = 0.0f;     // atraso do ataque em fração de passo (PAPO: a resposta fora da grade)
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

// PAPO: depois de escrita a conversa, o jeito de falar (ver o cabeçalho). Só mexe em duração, gate, atraso e acento.
inline void applyTalk (const PhraseParams& p, Phrase& out)
{
    const float t = juce::jlimit (0.0f, 1.0f, p.talk);
    if (t <= 0.001f || out.count == 0) return;
    auto chance = [] (int j, unsigned salt, float prob)
    {
        return (float) (mix ((unsigned) j * 2654435761U + salt) % 1000U) < prob * 1000.0f;
    };
    auto hashF = [] (int j, unsigned salt) { return (float) (mix ((unsigned) j * 40503U + salt) % 1000U) / 1000.0f; };

    for (int j = 0; j < out.count; ++j)
    {
        auto& pn = out.notes[(size_t) j];
        const bool hasNext = j + 1 < out.count;
        const auto* nx = hasNext ? &out.notes[(size_t) j + 1] : nullptr;
        const int  room = (hasNext ? nx->step : out.steps) - (pn.step + pn.len);   // passos de pausa depois da nota
        const bool first = j == 0 || out.notes[(size_t) j - 1].last;

        // acento no começo de cada fala; no meio da fala as notas repetidas falam mais baixo
        if (first) pn.accent = pn.accent || chance (j, 101U, 0.4f + 0.4f * t);
        else if (j > 0 && out.notes[(size_t) j - 1].note == pn.note && chance (j, 103U, 0.5f * t)) pn.accent = false;

        // a resposta chega um pouco atrasada; dentro da fala, um tiquinho fora da grade (quem responde, mais solto)
        if (first && j > 0) pn.delay = t * (0.12f + 0.28f * hashF (j, 107U));
        else                pn.delay = t * (pn.response ? 0.07f : 0.03f) * hashF (j, 109U);

        if (pn.last)
        {
            if (nx != nullptr && nx->voice != pn.voice && chance (j, 113U, 0.3f * t))
            {
                // POR CIMA: a nota que fecha a fala segue soando enquanto o outro timbre começa
                pn.len += 1;
                pn.gate = 1.0f;
            }
            else if (nx != nullptr && chance (j, 127U, 0.55f * t) && room == 0)
            {
                // RESPIRO: sem pausa entre as falas, quem fala encurta a última nota
                pn.gate *= 0.55f;
            }
            else if (room > 0 && chance (j, 131U, 0.8f * t))
            {
                // a última nota da fala se alonga sobre a pausa (como quem termina a frase)
                pn.len += juce::jmin (room, 2);
                pn.gate = juce::jmax (pn.gate, 0.85f);
            }
        }
        else if (nx != nullptr && room == 0 && std::abs (nx->note - pn.note) <= 3 && nx->note != pn.note
                 && chance (j, 137U, 0.45f * t))
        {
            // LEGATO: nota vizinha da mesma fala, sem pausa: gate cheio (com GLIDE, desliza)
            pn.gate = 1.0f;
        }
    }
}

// FAIXA DA CONVERSA: as frases ficam entre a oitava 1 e a 3 (Dó 1 = 36 até Si 3 = 71, com Dó 3 = 60). Primeiro a frase
// inteira desce/sobe de oitava para caber (o desenho da melodia não muda); o que ainda sobrar fora dobra de oitava.
// Quem quiser mais agudo/grave usa a OITAVA do slot (ou a nota segurada, que transpõe).
constexpr int rangeLo = 36, rangeHi = 71;

inline void fitToRange (Phrase& out)
{
    if (out.count == 0) return;
    int bestShift = 0, bestOut = 1 << 30, bestDist = 1 << 30;
    for (int k = -4; k <= 3; ++k)
    {
        int outside = 0, lo = 999, hi = -999;
        for (int j = 0; j < out.count; ++j)
        {
            const int n = out.notes[(size_t) j].note + 12 * k;
            if (n < rangeLo || n > rangeHi) ++outside;
            lo = juce::jmin (lo, n); hi = juce::jmax (hi, n);
        }
        const int dist = std::abs ((lo + hi) - (rangeLo + rangeHi));     // empate: a frase mais no meio da faixa
        if (outside < bestOut || (outside == bestOut && dist < bestDist)) { bestShift = k; bestOut = outside; bestDist = dist; }
    }
    for (int j = 0; j < out.count; ++j)
    {
        int n = out.notes[(size_t) j].note + 12 * bestShift;
        while (n > rangeHi) n -= 12;
        while (n < rangeLo) n += 12;
        out.notes[(size_t) j].note = n;
    }
}

inline void buildPhrase (const Pattern& pat, const PhraseParams& p, Phrase& out)
{
    out.count = 0;
    out.steps = juce::jlimit (1, maxSteps, p.steps);
    out.atStep.fill (-1);
    const ScaleMap sm (p.scale, p.root);
    const int base = baseNote (p.root);
    const bool byTime = p.every < 0;
    const int every = juce::jmax (1, p.every);
    const int turnSteps = byTime ? juce::jmax (1, (int) std::round ((double) -p.every * p.stepsPerBeat)) : 1;

    // primeira passada: notas, duração e de quem é a vez
    std::array<int, maxSteps> deg {}, turnOf {}, posOf {};
    int k = 0, prevTurn = -1, pos = 0, noteTurn = -1, turnEnd = 0;
    for (int i = 0; i < out.steps; ++i)
    {
        const Step st = pat.get (i);
        if (st.mode != Mode::note) continue;
        int len = 1;
        while (i + len < out.steps && pat.get (i + len).mode == Mode::tie) ++len;

        // PAPO: a fala tem tamanho variável (±1 ou +2 notas) — a troca não cai sempre no mesmo lugar
        if (! byTime && k == turnEnd)
        {
            ++noteTurn;
            int len = every;
            if (p.talk >= 0.2f)
            {
                static const int deltas[8] = { 0, 1, -1, 0, 2, -1, 1, 0 };
                const int d = deltas[mix ((unsigned) noteTurn * 2246822519U + 7U) % 8U];
                if ((float) (mix ((unsigned) noteTurn * 3266489917U + 11U) % 100U) < p.talk * 100.0f) len += d;
            }
            turnEnd = k + juce::jmax (1, len);
        }
        const int turn = byTime ? i / turnSteps : noteTurn;
        pos = turn == prevTurn ? pos + 1 : 0;
        prevTurn = turn;

        auto& pn = out.notes[(size_t) out.count];
        pn.step = i;
        pn.len = len;
        pn.gate = (float) (st.gate + 1) / 16.0f;
        pn.accent = st.accent;
        pn.response = (turn % 2) == 1;
        pn.voice = voiceForTurn (turn, p);
        deg[(size_t) out.count] = st.deg;
        turnOf[(size_t) out.count] = turn;
        posOf[(size_t) out.count] = pos;
        out.atStep[(size_t) i] = out.count;
        ++out.count;
        ++k;
    }

    // segunda passada: a resposta repete/varia/espelha a pergunta e fecha na tônica
    std::array<int, 16> callDeg {};      // graus da última pergunta (até 16 notas por turno)
    int callCount = 0;
    for (int j = 0; j < out.count; ++j)
    {
        auto& pn = out.notes[(size_t) j];
        const int ps = posOf[(size_t) j];
        pn.last = j + 1 >= out.count || turnOf[(size_t) j + 1] != turnOf[(size_t) j];
        int d = p.straight ? 0 : deg[(size_t) j];
        if (p.straight) {}   // RETA: pergunta e resposta na tônica (só a ALTURA DA RESPOSTA muda a oitava)
        else if (! pn.response)
        {
            if (ps == 0) callCount = 0;
            if (ps < (int) callDeg.size()) { callDeg[(size_t) ps] = d; callCount = ps + 1; }
        }
        else
        {
            if (ps < callCount)
            {
                const int c = callDeg[(size_t) ps];
                const auto& st = pat.get (pn.step);
                switch (p.response)
                {
                    case Response::echo:   d = c; break;
                    case Response::vary:   d = c + (st.var < 6 ? 0 : st.var < 10 ? 1 : st.var < 13 ? -1 : 2); break;
                    case Response::invert: d = 2 * callDeg[0] - c; break;
                    case Response::free:   break;
                }
            }
            // a resposta conclui: a última nota da fala cai na tônica mais perto (o mergulho do BEND sai dela)
            if (! p.straight && pn.last && p.response != Response::free && turnOf[(size_t) j] > 0)
            {
                const int n = sm.n;
                const int lo = (int) std::floor ((double) d / (double) n) * n;
                d = (d - lo) * 2 <= n ? lo : lo + n;
            }
        }

        int note = base + sm.toSemis (juce::jlimit (-21, 28, d));
        if (pn.response)
        {
            note += intervalSemis (p.interval);
            if (p.interval == 2) note = scales::snap (note, p.scale, p.root);
        }
        pn.note = juce::jlimit (24, 108, note);
    }

    fitToRange (out);
    applyTalk (p, out);
}

// ======================================================================================
// GERADOR (thread da interface). Só mexe nos primeiros 'steps' passos.
// ======================================================================================

// RITMO: um RIFF que se repete (como nos leads de full on), não um sorteio a cada tempo. Duas formas:
//  - células de 4 passos (A A A B ou A B A B por compasso), com a densidade decidindo quantas notas;
//  - "rolando": célula de 3 passos repetida (3+3+3+3+4), o desencontro com o 4/4 que faz o lead girar.
// No fim de cada 2 compassos o último tempo vira uma virada (mais cheia). O grau de cada passo fica: a melodia não se
// perde quando só o ritmo muda.
inline void generateRhythm (Pattern& pat, int steps, float density, juce::Random& rnd)
{
    // n = nota, - = pausa, ~ = ligadura; da mais vazia para a mais cheia
    static const char* const cells[] = { "n---", "n-~-", "n~~~", "n--n", "n-n-", "nn--", "n~-n", "-nn-", "n-nn", "nn-n",
                                         "nn~n", "n~nn", "nnn-", "-nnn", "nnnn", "nnnn" };
    static const char* const threes[] = { "nn-", "n-n", "n~n", "nnn", "-nn", "n~-" };
    constexpr int numCells = (int) (sizeof (cells) / sizeof (cells[0]));
    const float d = juce::jlimit (0.0f, 1.0f, density);
    auto pickCell = [&] (float spread)
    {
        int c = juce::roundToInt (d * (float) (numCells - 1) + (rnd.nextFloat() - 0.5f) * spread);
        return juce::jlimit (0, numCells - 1, c);
    };

    std::array<char, maxSteps> m {};
    const bool rolling = rnd.nextInt (100) < 35;
    if (rolling)
    {
        // a célula de 3 mais cheia quanto maior a densidade
        const int t = juce::jlimit (0, 5, juce::roundToInt ((1.0f - d) * 3.0f + (float) rnd.nextInt (3)) % 6);
        const char* three = threes[d > 0.75f ? (rnd.nextBool() ? 3 : 0) : t];
        const char* tail = cells[pickCell (3.0f)];
        for (int i = 0; i < maxSteps; ++i)
        {
            const int b = i % 16;
            m[(size_t) i] = b < 12 ? three[b % 3] : tail[b - 12];
        }
    }
    else
    {
        const int a = pickCell (5.0f), b = pickCell (6.0f);
        const bool abab = rnd.nextBool();
        for (int i = 0; i < maxSteps; ++i)
        {
            const int g = (i % 16) / 4;
            const int c = abab ? (g % 2 == 0 ? a : b) : (g < 3 ? a : b);
            m[(size_t) i] = cells[c][i % 4];
        }
    }
    // virada no último tempo de cada 2 compassos (16 passos = 1 compasso em 1/16)
    for (int bar2 = 0; bar2 < maxSteps; bar2 += 32)
    {
        const int at = bar2 + 28;
        if (at + 3 >= maxSteps) break;
        const char* fill = cells[juce::jlimit (8, numCells - 1, pickCell (2.0f) + 3)];
        for (int j = 0; j < 4; ++j) m[(size_t) (at + j)] = fill[j];
    }

    for (int i = 0; i < steps && i < maxSteps; ++i)
    {
        auto s = pat.get (i);
        const char ch = m[(size_t) i];
        s.mode = ch == 'n' ? Mode::note : ch == '~' ? Mode::tie : Mode::rest;
        pat.set (i, s);
    }
    // a frase sempre começa falando (e ligadura sem nota antes vira nota)
    for (int i = 0; i < steps; ++i)
    {
        auto s = pat.get (i);
        if (s.mode == Mode::rest) continue;
        if (s.mode == Mode::tie) { s.mode = Mode::note; pat.set (i, s); }
        break;
    }
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
    // RETA não muda a melodia escrita (a frase toca na tônica pelo buildPhrase): o dado sorteia um motivo, que aparece
    // quando a pessoa voltar para outro contorno
    if (contour == Contour::straight) contour = Contour::motif;
    if (contour == Contour::pedal)
    {
        // PEDAL: passo sim, passo não, a nota fixa (tônica embaixo ou a oitava); nos outros uma linha de 4 notas
        // que anda acima dela e muda um pouco a cada compasso
        const int pedal = rnd.nextInt (4) == 0 ? per : 0;
        std::array<int, 4> line {};
        int d = 2 + rnd.nextInt (3);
        for (auto& x : line)
        {
            x = d;
            d = juce::jlimit (1, juce::jmax (2, top), d + (rnd.nextInt (3) - 1) * (1 + rnd.nextInt (2)));
        }
        if (pedal == per) for (auto& x : line) x = juce::jmax (x, per + 1);
        int shift = 0;
        for (int i = 0; i < steps; ++i)
        {
            if (i % 16 == 0 && i > 0) shift = (shift + (rnd.nextBool() ? 1 : -1)) % 3;
            const int dd = i % 2 == 0 ? pedal : line[(size_t) ((i / 2) % 4)] + shift;
            auto s = pat.get (i);
            s.deg = juce::jlimit (-14, 21, toStored (juce::jlimit (-2, top + 1, dd)));
            s.var = rnd.nextInt (16);
            pat.set (i, s);
        }
        return;
    }
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
