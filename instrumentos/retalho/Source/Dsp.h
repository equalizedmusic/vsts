#pragma once

// RETALHO — motor de stutter + groove de vocal. Sem JUCE: a bancada tools/teste_dsp.cpp compila só com este arquivo.
//
// Tudo é causal (latência 0). O áudio que entra vai para a CAPTURE (anel de 16 s) e todo fill e todo chop leem dele.
// A 1ª fatia de um fill é a própria voz passando (ainda não existe passado para repetir); as seguintes repetem o
// trecho já gravado. Quando uma fatia precisa de um trecho que ainda não chegou (REVERSE, PITCH para cima, SPEED 2×),
// ela lê o trecho imediatamente anterior, que já está gravado.
//
// Modos (abas): EDIT (o fill desenhado: GRID, RAMP, CURVE, FREE, TREMOLO), COMPLEX (8/16/32 passos com 8 tipos, READ
// FOLLOW/HOLD/HITS, DICE/LOCK) e GROOVE (a voz cortada em 8 chops tocando num pattern: OFFBEAT, GALLOP, SKIP…).

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace retalho
{
constexpr double kPi = 3.14159265358979323846;

template <class T> inline T lim (T v, T a, T b) { return v < a ? a : (v > b ? b : v); }
inline float dbGanho (float db) { return std::pow (10.0f, db / 20.0f); }

// ---- listas ---------------------------------------------------------------------------------------------------------
enum { mEdit, mComplex, mGroove };
enum { shGrid, shRamp, shCurve, shFree, shTremolo };
enum { gAuto, gHold, gOneShot };
enum { caOff, ca16, ca4, caBar };
enum { tRepeat, tPitch, tReverse, tTape, tSpeed, tGate, tThru, tMute, numTipos };
enum { rdFollow, rdHold, rdHits };
enum { mvLayer, mvDuck, mvReplace };
enum { eOffbeat, eGallop, eSkip, e2Step, eFunk, eTrap, eCallResp, numEstilos };
enum { flOff, flHpf, flLpf, flBpf };
enum { trSquare, trSine, trSaw };
enum { dzAll, dzTypes, dzValues, dzPitch };
enum { cutOnset, cut8, cut16 };

constexpr int maxGrade = 64, maxPassos = 32, numLinhas = 3, maxCelulas = 32, numChops = 8, numPads = 6, numLivre = 8;

namespace ch
{
inline const char* const modo[] = { "EDIT", "COMPLEX", "GROOVE" };
inline const char* const shape[] = { "GRID", "RAMP", "CURVE", "FREE", "TREMOLO" };
inline const char* const length[] = { "1/2 BAR", "1 BAR", "2 BARS" };
inline const char* const rate[] = { "1/4", "1/8", "1/16", "1/32", "1/64" };
inline const char* const rateMod[] = { "STRAIGHT", "TRIPLET", "DOTTED" };
inline const char* const trem[] = { "SQUARE", "SINE", "SAW" };
inline const char* const filtro[] = { "OFF", "HPF", "LPF", "BPF" };
inline const char* const chunk[] = { "AUTO", "PREV 1", "PREV 2", "PREV 3", "PREV 4", "PREV 5", "PREV 6", "PREV 7", "PREV 8" };
inline const char* const passos[] = { "8", "16", "32" };
inline const char* const read[] = { "FOLLOW", "HOLD", "HITS" };
inline const char* const dado[] = { "ALL", "TYPES", "VALUES", "PITCH" };
inline const char* const every[] = { "OFF", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };
inline const char* const estilo[] = { "OFFBEAT", "GALLOP", "SKIP", "2-STEP", "FUNK", "TRAP", "CALL&RESP" };
inline const char* const bars[] = { "1 BAR", "2 BARS" };
inline const char* const ggrid[] = { "1/16", "1/16T" };
inline const char* const mix[] = { "LAYER", "DUCK", "REPLACE" };
inline const char* const chops[] = { "LIVE", "FREEZE" };
inline const char* const cut[] = { "ONSET", "1/8", "1/16" };
inline const char* const chopLen[] = { "1/32", "1/16", "1/8", "1/4" };
inline const char* const gatilho[] = { "AUTO", "HOLD", "ONE-SHOT" };
inline const char* const catchs[] = { "OFF", "1/16", "1/4", "NEXT BAR" };
inline const char* const onde[] = { "FIM DE 1 BAR", "FIM DE 2 BARS", "FIM DE 4 BARS", "FIM DE 8 BARS",
                                    "IN\xC3\x8D" "CIO DE 1 BAR", "IN\xC3\x8D" "CIO DE 2 BARS", "IN\xC3\x8D" "CIO DE 4 BARS",
                                    "IN\xC3\x8D" "CIO DE 8 BARS" };
inline const char* const root[] = { "AUTO", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline const char* const escala[] = { "AUTO", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "HARM. MINOR", "CHROMATIC" };
inline const char* const tipo[] = { "REPEAT", "PITCH", "REVERSE", "TAPE", "SPEED", "GATE", "THRU", "MUTE" };
inline const char* const pc[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
} // namespace ch

// ---- parâmetros -----------------------------------------------------------------------------------------------------
// Os campos até numCena formam a CENA (vão nos pads, nos presets e no A/B); os globais não mudam com pad nem preset.
enum Campo
{
    kModo, kShape, kLength, kRate, kRateMod, kRampTo, kCurva, kTrem,
    kDecay, kFadeIn, kFadeOut, kGate, kSwing,
    kPIni, kPFim, kPCurva, kGlide, kKey,
    kFiltro, kFDe, kFAte, kReso,
    kWidth, kTapeStop, kReverse, kChunk,
    kPassos, kRead, kDMode, kDAmount, kDChaos, kDEvery, kSeed,
    kEstilo, kGBars, kGGrid, kDensity, kGSwing, kHumanize, kGPitch, kMixVoz, kDuck, kChopHpf, kChopLpf, kChopVol, kLockGrid,
    numCena,
    kChops = numCena, kCut, kSens, kChopLen, kGrooveOn, kGatilho, kCatch, kOnde, kRoot, kEscala,
    kDryWet, kOutput, kBypass, kLimit,
    kPad1, kPad2, kPad3, kPad4, kPad5, kPad6,
    numCampos
};

struct InfoCampo
{
    const char* id;
    const char* nome;
    float min, max, passo, padrao, skew;
    const char* un;
    const char* const* esc;
    int nEsc;
    bool liga;
};

#define RETALHO_ESC(a) a, (int) (sizeof (a) / sizeof (a[0]))
inline const InfoCampo& info (int k)
{
    static const InfoCampo t[numCampos] = {
        { "modo", "Mode", 0, 2, 1, 0, 1, "", RETALHO_ESC (ch::modo), false },
        { "shape", "Shape", 0, 4, 1, 0, 1, "", RETALHO_ESC (ch::shape), false },
        { "length", "Length", 0, 2, 1, 1, 1, "", RETALHO_ESC (ch::length), false },
        { "rate", "Rate", 0, 4, 1, 2, 1, "", RETALHO_ESC (ch::rate), false },
        { "ratemod", "Rate Mod", 0, 2, 1, 0, 1, "", RETALHO_ESC (ch::rateMod), false },
        { "rampto", "Ramp To", 0, 4, 1, 4, 1, "", RETALHO_ESC (ch::rate), false },
        { "curva", "Curve", -100, 100, 1, 50, 1, "%", nullptr, 0, false },
        { "trem", "Tremolo Shape", 0, 2, 1, 0, 1, "", RETALHO_ESC (ch::trem), false },
        { "decay", "Decay", -12, 3, 0.5f, -1, 1, "dB", nullptr, 0, false },
        { "fadein", "Fade In", 0.5f, 20, 0.1f, 1, 0.5f, "ms", nullptr, 0, false },
        { "fadeout", "Fade Out", 0.5f, 40, 0.1f, 4, 0.5f, "ms", nullptr, 0, false },
        { "gate", "Gate", 10, 100, 1, 85, 1, "%", nullptr, 0, false },
        { "swing", "Swing", 0, 75, 1, 0, 1, "%", nullptr, 0, false },
        { "pini", "Pitch Start", -24, 24, 1, 0, 1, "st", nullptr, 0, false },
        { "pfim", "Pitch End", -24, 24, 1, 0, 1, "st", nullptr, 0, false },
        { "pcurva", "Pitch Curve", -100, 100, 1, 50, 1, "%", nullptr, 0, false },
        { "glide", "Glide", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "key", "Key Lock", 0, 1, 1, 1, 1, "", nullptr, 0, true },
        { "filtro", "Filter", 0, 3, 1, 0, 1, "", RETALHO_ESC (ch::filtro), false },
        { "fde", "Filter From", 20, 20000, 1, 18000, 0.25f, "Hz", nullptr, 0, false },
        { "fate", "Filter To", 20, 20000, 1, 900, 0.25f, "Hz", nullptr, 0, false },
        { "reso", "Resonance", 0, 100, 1, 30, 1, "%", nullptr, 0, false },
        { "width", "Stereo", 0, 100, 1, 50, 1, "%", nullptr, 0, false },
        { "tapestop", "Tape Stop", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "reverse", "Reverse", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "chunk", "Chunk", 0, 8, 1, 0, 1, "", RETALHO_ESC (ch::chunk), false },
        { "passos", "Steps", 0, 2, 1, 1, 1, "", RETALHO_ESC (ch::passos), false },
        { "read", "Read", 0, 2, 1, 0, 1, "", RETALHO_ESC (ch::read), false },
        { "dmode", "Dice Mode", 0, 3, 1, 0, 1, "", RETALHO_ESC (ch::dado), false },
        { "damount", "Dice Amount", 0, 100, 1, 50, 1, "%", nullptr, 0, false },
        { "dchaos", "Dice Chaos", 0, 100, 1, 40, 1, "%", nullptr, 0, false },
        { "devery", "Dice Every", 0, 4, 1, 0, 1, "", RETALHO_ESC (ch::every), false },
        { "seed", "Seed", 1, 999, 1, 1, 1, "", nullptr, 0, false },
        { "estilo", "Style", 0, 6, 1, 0, 1, "", RETALHO_ESC (ch::estilo), false },
        { "gbars", "Groove Bars", 0, 1, 1, 0, 1, "", RETALHO_ESC (ch::bars), false },
        { "ggrid", "Groove Grid", 0, 1, 1, 0, 1, "", RETALHO_ESC (ch::ggrid), false },
        { "density", "Density", 0, 100, 1, 70, 1, "%", nullptr, 0, false },
        { "gswing", "Groove Swing", 0, 75, 1, 0, 1, "%", nullptr, 0, false },
        { "humanize", "Humanize", 0, 100, 1, 0, 1, "%", nullptr, 0, false },
        { "gpitch", "Chop Pitch", -12, 12, 1, 0, 1, "st", nullptr, 0, false },
        { "mixvoz", "Vocal Mix", 0, 2, 1, 1, 1, "", RETALHO_ESC (ch::mix), false },
        { "duck", "Duck", -24, 0, 0.5f, -9, 1, "dB", nullptr, 0, false },
        { "chophpf", "Chop HPF", 20, 2000, 1, 150, 0.4f, "Hz", nullptr, 0, false },
        { "choplpf", "Chop LPF", 1000, 20000, 10, 14000, 0.5f, "Hz", nullptr, 0, false },
        { "chopvol", "Chop Volume", -24, 6, 0.1f, 0, 1, "dB", nullptr, 0, false },
        { "lockgrid", "Lock Grid", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        // globais
        { "chops", "Chops", 0, 1, 1, 0, 1, "", RETALHO_ESC (ch::chops), false },
        { "cut", "Cut", 0, 2, 1, 0, 1, "", RETALHO_ESC (ch::cut), false },
        { "sens", "Sensitivity", 0, 100, 1, 50, 1, "%", nullptr, 0, false },
        { "choplen", "Chop Length", 0, 3, 1, 2, 1, "", RETALHO_ESC (ch::chopLen), false },
        { "grooveon", "Groove On", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "gatilho", "Trigger", 0, 2, 1, 0, 1, "", RETALHO_ESC (ch::gatilho), false },
        { "catch", "Catch", 0, 3, 1, 3, 1, "", RETALHO_ESC (ch::catchs), false },
        { "onde", "Auto Where", 0, 7, 1, 2, 1, "", RETALHO_ESC (ch::onde), false },
        { "root", "Key Root", 0, 12, 1, 0, 1, "", RETALHO_ESC (ch::root), false },
        { "escala", "Key Scale", 0, 6, 1, 0, 1, "", RETALHO_ESC (ch::escala), false },
        { "drywet", "Dry/Wet", 0, 100, 1, 100, 1, "%", nullptr, 0, false },
        { "output", "Output", -24, 12, 0.1f, 0, 1, "dB", nullptr, 0, false },
        { "bypass", "Bypass", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "limit", "Safety Limiter", 0, 1, 1, 1, 1, "", nullptr, 0, true },
        { "pad1", "Pad 1", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "pad2", "Pad 2", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "pad3", "Pad 3", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "pad4", "Pad 4", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "pad5", "Pad 5", 0, 1, 1, 0, 1, "", nullptr, 0, true },
        { "pad6", "Pad 6", 0, 1, 1, 0, 1, "", nullptr, 0, true },
    };
    return t[lim (k, 0, (int) numCampos - 1)];
}
#undef RETALHO_ESC

inline double beatsRate (int r, int mod)
{
    double b = 1.0 / double (1 << lim (r, 0, 4));
    if (mod == 1) b *= 2.0 / 3.0;
    else if (mod == 2) b *= 1.5;
    return b;
}
inline double compassosLength (int l) { static const double c[] = { 0.5, 1.0, 2.0 }; return c[lim (l, 0, 2)]; }
inline double beatsChopLen (int i) { static const double b[] = { 0.125, 0.25, 0.5, 1.0 }; return b[lim (i, 0, 3)]; }

// ---- pacotes dos arrays ---------------------------------------------------------------------------------------------
// grade (EDIT GRID): bit 0 liga, bit 1 reverse, bits 8..15 volume (0..255)
inline uint32_t grade (bool on, bool rev, int vol) { return (on ? 1u : 0u) | (rev ? 2u : 0u) | (uint32_t (lim (vol, 0, 255)) << 8); }
inline bool gOn (uint32_t g) { return (g & 1u) != 0; }
inline bool gRev (uint32_t g) { return (g & 2u) != 0; }
inline int gVol (uint32_t g) { return (int) ((g >> 8) & 255u); }
// passo (COMPLEX): bits 0..3 tipo, 8..15 valor (0..255), bit 16 cadeado
inline uint32_t passo (int tipo, int valor, bool lock)
{
    return (uint32_t) lim (tipo, 0, numTipos - 1) | (uint32_t (lim (valor, 0, 255)) << 8) | (lock ? 0x10000u : 0u);
}
inline int pTipo (uint32_t p) { return lim ((int) (p & 15u), 0, numTipos - 1); }
inline int pValor (uint32_t p) { return (int) ((p >> 8) & 255u); }
inline bool pLock (uint32_t p) { return (p & 0x10000u) != 0; }
// célula (GROOVE): bits 0..3 chop + 1 (0 = vazia), bits 8..15 velocity, bit 16 roll (3 batidas dentro da célula)
inline uint32_t celula (int chop, int vel, bool roll = false)
{
    return chop < 0 ? 0u : (uint32_t (lim (chop, 0, numChops - 1) + 1) | (uint32_t (lim (vel, 1, 255)) << 8) | (roll ? 0x10000u : 0u));
}
inline int cChop (uint32_t c) { return (int) (c & 15u) - 1; }
inline int cVel (uint32_t c) { return (int) ((c >> 8) & 255u); }
inline bool cRoll (uint32_t c) { return (c & 0x10000u) != 0; }

// valor de um passo, como aparece na tela
inline int reps (int v) { static const int r[4] = { 2, 3, 4, 8 }; return r[lim (v * 4 / 256, 0, 3)]; }
inline int stPasso (int v) { return (int) std::lround (-24.0 + 48.0 * v / 255.0); }

// ---- sorteio --------------------------------------------------------------------------------------------------------
struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed * 2654435761u + 0x9E3779B9u) { if (s == 0) s = 1; for (int i = 0; i < 4; ++i) prox(); }
    uint32_t prox() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (float) (prox() >> 8) * (1.0f / 16777216.0f); }
    int n (int m) { return m <= 1 ? 0 : (int) (prox() % (uint32_t) m); }
};
inline float hash01 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return (float) (x >> 8) * (1.0f / 16777216.0f);
}

// ---- escalas --------------------------------------------------------------------------------------------------------
// escala (sem o AUTO): 0 MAJOR, 1 MINOR, 2 DORIAN, 3 PHRYGIAN, 4 HARM. MINOR, 5 CHROMATIC
inline unsigned mascaraEscala (int e) { static const unsigned m[] = { 0xAB5u, 0x5ADu, 0x6ADu, 0x5ABu, 0x9ADu, 0xFFFu }; return m[lim (e, 0, 5)]; }
inline unsigned rodaMascara (unsigned m, int tom)
{
    tom = ((tom % 12) + 12) % 12;
    return tom == 0 ? m : (((m << tom) | (m >> (12 - tom))) & 0xFFFu);
}
// semitom inteiro mais perto de st que cai na escala (a nota do trecho é pcBase)
inline int encaixa (int st, int pcBase, unsigned mascara)
{
    if (mascara == 0xFFFu || mascara == 0u || pcBase < 0) return st;
    auto dentro = [&] (int s) { return ((mascara >> ((((pcBase + s) % 12) + 12) % 12)) & 1u) != 0; };
    for (int d = 0; d <= 6; ++d)
    {
        if (dentro (st + d)) return st + d;
        if (dentro (st - d)) return st - d;
    }
    return st;
}

// ---- GROOVE: estilos e dado ----------------------------------------------------------------------------------------
inline void geraEstilo (int estilo, int compassos, bool tercina, uint32_t (&cel)[numLinhas][maxCelulas])
{
    for (auto& l : cel) for (auto& c : l) c = 0;
    compassos = lim (compassos, 1, 2);
    const int porC = tercina ? 12 : 16;
    auto pos = [tercina] (int s16) { return tercina ? (s16 * 3 + 2) / 4 : s16; };
    auto add = [&] (int linha, int s16, int chop, int vel, bool roll = false)
    {
        for (int b = 0; b < compassos; ++b)
        {
            const int c = b * porC + pos (s16);
            if (c < maxCelulas) cel[linha][c] = celula (chop, vel, roll);
        }
    };
    switch (estilo)
    {
        case eGallop:
            for (int b = 0; b < 4; ++b) { add (0, b * 4 + 2, b % 4, 220); add (1, b * 4 + 1, (b + 1) % 4, 150); add (2, b * 4 + 3, 4 + b % 4, 130); }
            break;
        case eSkip:
            for (int b = 0; b < 4; ++b) add (0, b * 4 + 2, b % 4, 205);
            add (1, 1, 4, 170); add (1, 9, 5, 170);
            add (2, 5, 6, 120); add (2, 13, 7, 120); add (2, 15, 6, 110);
            break;
        case e2Step:
            add (0, 3, 0, 210); add (0, 10, 1, 210);
            add (1, 6, 2, 170); add (1, 13, 3, 170);
            add (2, 8, 4, 120); add (2, 15, 5, 120);
            break;
        case eFunk:
            add (0, 0, 0, 215); add (0, 3, 1, 200); add (0, 6, 2, 200); add (0, 8, 0, 215); add (0, 11, 1, 200); add (0, 14, 2, 200);
            add (1, 10, 3, 160); add (1, 2, 4, 140);
            add (2, 13, 5, 120); add (2, 15, 6, 120, true);
            break;
        case eTrap:
            for (int b = 0; b < 4; ++b) add (0, b * 4 + 2, b % 4, 185);
            add (1, 0, 4, 205); add (1, 8, 5, 205);
            add (2, 13, 6, 120, true); add (2, 14, 7, 115, true); add (2, 15, 6, 110, true);
            break;
        case eCallResp:
            for (int b = 0; b < 4; ++b) { add (0, b * 4 + 2, b % 4, 210); add (1, b * 4 + 3, 4 + b % 4, 150); }
            add (2, 13, 7, 120);
            break;
        case eOffbeat:
        default:
            for (int b = 0; b < 4; ++b) add (0, b * 4 + 2, b % 4, 220);
            add (1, 7, 4, 150); add (1, 15, 5, 150);
            add (2, 13, 6, 120); add (2, 11, 7, 110);
            break;
    }
    if (compassos == 2)   // o 2º compasso fecha a frase: um fill a mais no fim
    {
        cel[2][porC + pos (12)] = celula (7, 140);
        cel[2][porC + pos (14)] = celula (6, 130, estilo == eTrap);
    }
}

inline void dadoGroove (uint32_t (&cel)[numLinhas][maxCelulas], int nCel, uint32_t seed, float amount)
{
    Rng r (seed * 7919u + 13u);
    for (int l = 0; l < numLinhas; ++l)
        for (int c = 0; c < nCel && c < maxCelulas; ++c)
        {
            const float a = r.f(), b = r.f(), d = r.f();
            const int chop = r.n (numChops), vel = 100 + r.n (150);
            if (a > amount * 0.45f) continue;
            if (cChop (cel[l][c]) >= 0) cel[l][c] = b < 0.4f ? 0u : celula (chop, vel, l == 2 && d < 0.15f);
            else if (b < (l == 0 ? 0.5f : 0.3f)) cel[l][c] = celula (chop, vel, l == 2 && d < 0.15f);
        }
}

// COMPLEX: sorteia tipos e valores; os passos com cadeado ficam. Mesmo seed = mesmo resultado.
inline void dadoPassos (uint32_t* p, int n, int modo, float amount, float chaos, uint32_t seed)
{
    Rng r (seed);
    static const int peso[numTipos] = { 26, 14, 12, 6, 8, 10, 18, 6 };
    for (int i = 0; i < n && i < maxPassos; ++i)
    {
        const float a = r.f(), b = r.f(), c = r.f();
        if (pLock (p[i]) || a > amount) continue;
        int tipo = pTipo (p[i]), val = pValor (p[i]);
        if (modo == dzAll || modo == dzTypes)
        {
            int x = (int) (b * 100.0f), t = 0;
            while (t < numTipos - 1 && x >= peso[t]) { x -= peso[t]; ++t; }
            tipo = t;
        }
        if (modo == dzPitch) tipo = tPitch;
        if (modo != dzTypes)
        {
            const float chs = std::max (chaos, 0.15f);
            val = lim ((int) std::lround (val + (c * 2.0f - 1.0f) * 255.0f * chs), 0, 255);
            if (modo == dzPitch) val = lim ((int) std::lround ((encaixa (stPasso (val), 0, 0xAB5u) + 24) * 255.0 / 48.0), 0, 255);
        }
        p[i] = passo (tipo, val, false);
    }
}

// ---- cena -----------------------------------------------------------------------------------------------------------
struct Cena
{
    float v[numCena];
    uint32_t grade[maxGrade];
    float livre[numLivre];
    uint32_t passo[maxPassos];
    uint32_t cel[numLinhas][maxCelulas];

    Cena() { padrao(); }
    void padrao()
    {
        for (int k = 0; k < numCena; ++k) v[k] = info (k).padrao;
        for (auto& g : grade) g = retalho::grade (true, false, 255);
        for (int i = 0; i < numLivre; ++i) livre[i] = 0.2f + 0.6f * (float) i / (numLivre - 1);
        static const uint8_t t[16] = { tThru, tRepeat, tThru, tPitch, tRepeat, tThru, tReverse, tGate,
                                       tThru, tRepeat, tSpeed, tThru, tRepeat, tPitch, tTape, tMute };
        static const uint8_t vv[16] = { 0, 64, 0, 175, 140, 0, 0, 120, 0, 200, 40, 0, 255, 223, 40, 0 };
        for (int i = 0; i < maxPassos; ++i) passo[i] = retalho::passo (t[i % 16], vv[i % 16], false);
        geraEstilo (eOffbeat, 1, false, cel);
    }
    int i (int k) const { return (int) std::lround (v[k]); }
    bool b (int k) const { return v[k] > 0.5f; }
    float f (int k) const { return v[k]; }
};

// ---- plano do fill --------------------------------------------------------------------------------------------------
enum { fLer, fMute, fTapeStop, fTapeStart, fTrem };

struct Fatia
{
    int64_t t0 = 0;        // início no fill (amostras)
    int32_t dur = 0;
    int32_t L = 0;         // região que repete (0 = a voz ao vivo, na posição da fatia)
    int64_t src = 0;       // início da região (amostra absoluta da captura)
    float r0 = 1, r1 = 1;  // velocidade no começo e no fim (GLIDE)
    float ganho = 1, gate = 1, pan = 0, st = 0;
    uint8_t tipo = fLer;
    bool rev = false;
    int16_t passo = -1;    // índice na grade / passo
    int8_t tipoPasso = -1; // COMPLEX: tipo do passo (cor na tela)
};

struct Plano
{
    static constexpr int maxF = 320;
    Fatia f[maxF];
    int n = 0;
    int64_t inicio = 0, len = 0;
    int modo = mEdit, filtro = flOff, trem = trSquare;
    float fDe = 1000, fAte = 1000, reso = 0.3f, fadeIn = 48, fadeOut = 192, estereo = 1, ganho = 1;
};

struct Ataques   // em ordem crescente
{
    int64_t v[128] {};
    int n = 0;
    int64_t antes (int64_t pos, int k) const   // k-ésimo ataque em ou antes de pos (0 = o mais recente)
    {
        int cont = 0;
        for (int i = n - 1; i >= 0; --i)
            if (v[i] <= pos) { if (cont == k) return v[i]; ++cont; }
        return -1;
    }
};

struct Contexto
{
    double spb = 24000, bpb = 4, fs = 48000;
    int64_t inicio = 0;
    unsigned mascara = 0xFFFu;
    int pcChunk = -1;
    const Ataques* at = nullptr;
    bool offline = false;   // amostra inteira já na mão (arquivo solto): pode ler adiante, nada "ainda não gravado"
};

inline Fatia& novaFatia (Plano& p) { if (p.n < Plano::maxF) ++p.n; return p.f[p.n - 1]; }

// Região [ini, ini+L) lida na velocidade da fatia (com volta em L). Se ainda não foi toda gravada quando a fatia
// começa: encolhe (EDIT/HOLD/HITS: fica o ataque) ou lê o trecho anterior (FOLLOW).
inline void regiao (Fatia& f, const Contexto& cx, int64_t ini, int64_t L, bool podeVivo, bool encolhe)
{
    const int64_t pos = cx.inicio + f.t0;
    if (podeVivo && ini == pos && f.r0 <= 1.0f && f.r1 <= 1.0f && ! f.rev) { f.src = pos; f.L = 0; return; }
    L = std::max<int64_t> (L, 32);
    if (! cx.offline && ini + L > pos)
    {
        if (encolhe && pos - ini >= 32) L = pos - ini;
        else ini = pos - L;
    }
    f.src = ini;
    f.L = (int32_t) L;
}

inline void montaEdit (const Cena& c, const Contexto& cx, Plano& p)
{
    p.n = 0; p.modo = mEdit; p.inicio = cx.inicio; p.ganho = 1;
    const double L = compassosLength (c.i (kLength)) * cx.bpb * cx.spb;
    p.len = std::max<int64_t> (64, (int64_t) std::llround (L));
    const int shape = c.i (kShape), mod = c.i (kRateMod);
    const double dRate = beatsRate (c.i (kRate), mod) * cx.spb, dTo = beatsRate (c.i (kRampTo), mod) * cx.spb;
    const double dMin = std::max (32.0, cx.spb / 32.0);   // nunca menor que 1/128
    const double sw = c.f (kSwing) * 0.01 * 0.5;
    const bool tape = c.b (kTapeStop) && shape != shTremolo;
    const double fimF = tape ? L - std::min (L * 0.25, cx.spb) : (double) p.len;
    int64_t chunkIni = cx.inicio;
    if (c.i (kChunk) > 0 && cx.at != nullptr)
    {
        const int64_t a = cx.at->antes (cx.inicio - 1, c.i (kChunk) - 1);
        if (a >= 0 && cx.inicio - a < (int64_t) (8.0 * cx.bpb * cx.spb)) chunkIni = a;
    }
    auto curva = [] (double x, double k)
    {
        x = lim (x, 0.0, 1.0);
        return k >= 0 ? std::pow (x, 1.0 + 3.0 * k) : 1.0 - std::pow (1.0 - x, 1.0 - 3.0 * k);
    };
    const double pIni = c.f (kPIni), pFim = c.f (kPFim), pk = c.f (kPCurva) * 0.01, ck = c.f (kCurva) * 0.01;
    const float w = c.f (kWidth) * 0.01f;
    p.estereo = std::min (1.0f, w * 2.0f);
    const float ping = std::max (0.0f, w * 2.0f - 1.0f);
    const bool glide = c.b (kGlide), key = c.b (kKey), revAll = c.b (kReverse);

    double t = 0;
    int k = 0;
    while (t < fimF - 8.0 && p.n < Plano::maxF - 2)
    {
        const double fr = t / L;
        double d;
        switch (shape)
        {
            case shRamp:  d = dRate + (dTo - dRate) * fr; break;
            case shCurve: d = dRate * std::pow (dTo / dRate, curva (fr, ck)); break;
            case shFree:
            {
                const double x = fr * (numLivre - 1);
                const int a = std::min ((int) x, numLivre - 2);
                const double v = c.livre[a] + (c.livre[a + 1] - c.livre[a]) * (x - a);
                d = cx.spb * std::pow (2.0, -4.0 * lim (v, 0.0, 1.0));
                break;
            }
            default: d = dRate * (shape == shGrid && sw > 0 ? (k % 2 == 0 ? 1.0 + sw : 1.0 - sw) : 1.0); break;
        }
        d = std::max (d, dMin);
        if (t + d > fimF - dMin * 0.5) d = fimF - t;   // a última fecha exatamente no fim (ou no começo do TAPE STOP)
        Fatia& f = novaFatia (p);
        f = Fatia();
        f.t0 = (int64_t) std::llround (t);
        f.dur = (int32_t) std::max<int64_t> (1, std::llround (t + d) - f.t0);
        f.passo = (int16_t) std::min (k, 32000);
        if (shape == shTremolo)
        {
            f.tipo = fTrem; f.src = cx.inicio + f.t0; f.L = 0; f.gate = c.f (kGate) * 0.01f;
        }
        else
        {
            const uint32_t g = shape == shGrid ? c.grade[k % maxGrade] : grade (true, false, 255);
            if (! gOn (g)) f.tipo = fMute;
            double st0 = pIni + (pFim - pIni) * curva ((double) f.t0 / L, pk);
            double st1 = pIni + (pFim - pIni) * curva ((double) (f.t0 + f.dur) / L, pk);
            if (! glide)
            {
                int s = (int) std::lround (st0);
                if (key) s = encaixa (s, cx.pcChunk, cx.mascara);
                st0 = st1 = s;
            }
            f.st = (float) st0;
            f.r0 = (float) std::pow (2.0, st0 / 12.0);
            f.r1 = (float) std::pow (2.0, st1 / 12.0);
            f.rev = revAll != gRev (g);
            f.ganho = dbGanho (c.f (kDecay) * (float) k) * (float) gVol (g) / 255.0f;
            f.gate = c.f (kGate) * 0.01f;
            regiao (f, cx, chunkIni, f.dur, true, true);
        }
        f.pan = ping > 0 ? (k % 2 == 0 ? -ping : ping) : 0.0f;
        t += d;   // acumula sem arredondar (o erro não cresce ao longo do fill)
        ++k;
    }
    if (tape && t < (double) p.len - 16)
    {
        Fatia& f = novaFatia (p);
        f = Fatia();
        f.t0 = (int64_t) std::llround (t);
        f.dur = (int32_t) (p.len - f.t0);
        f.tipo = fTapeStop; f.src = cx.inicio + f.t0; f.L = 0;
        f.passo = (int16_t) std::min (k, 32000);
    }
    p.filtro = c.i (kFiltro); p.fDe = c.f (kFDe); p.fAte = c.f (kFAte); p.reso = c.f (kReso) * 0.01f; p.trem = c.i (kTrem);
    p.fadeIn = (float) (std::max (0.5f, c.f (kFadeIn)) * 0.001 * cx.fs);
    p.fadeOut = (float) (std::max (0.5f, c.f (kFadeOut)) * 0.001 * cx.fs);
}

inline void montaComplex (const Cena& c, const Contexto& cx, Plano& p)
{
    p.n = 0; p.modo = mComplex; p.inicio = cx.inicio; p.ganho = 1;
    const double L = compassosLength (c.i (kLength)) * cx.bpb * cx.spb;
    p.len = std::max<int64_t> (64, (int64_t) std::llround (L));
    const int N = 8 << lim (c.i (kPassos), 0, 2);
    const int read = c.i (kRead);
    const float w = c.f (kWidth) * 0.01f;
    p.estereo = std::min (1.0f, w * 2.0f);
    const float ping = std::max (0.0f, w * 2.0f - 1.0f);
    const bool key = c.b (kKey);
    const bool encolhe = read != rdFollow;

    for (int k = 0; k < N; ++k)
    {
        const int64_t t0 = (int64_t) std::llround (L * k / N), t1 = (int64_t) std::llround (L * (k + 1) / N);
        const int32_t dur = (int32_t) std::max<int64_t> (1, t1 - t0);
        const int64_t pos = cx.inicio + t0;
        const uint32_t ps = c.passo[k];
        const int tipo = pTipo (ps), val = pValor (ps);
        const float v = (float) val / 255.0f;
        const float pan = ping > 0 ? (k % 2 == 0 ? -ping : ping) : 0.0f;

        int64_t base = pos;     // de onde o passo lê (READ)
        bool podeVivo = true;
        if (read == rdHold) { base = cx.inicio; podeVivo = k == 0; }
        else if (read == rdHits && cx.at != nullptr)
        {
            const int64_t a = cx.at->antes (pos - dur, 0);
            if (a >= 0 && pos - a < (int64_t) (4.0 * cx.bpb * cx.spb)) { base = a; podeVivo = false; }
        }
        auto ler = [&] (int64_t ti, int64_t di, int64_t ini, int64_t Lr, float r, bool rev, float gate, bool vivo) -> Fatia&
        {
            Fatia& f = novaFatia (p);
            f = Fatia();
            f.t0 = ti; f.dur = (int32_t) std::max<int64_t> (1, di); f.r0 = f.r1 = r; f.rev = rev; f.gate = gate;
            f.tipo = fLer; f.passo = (int16_t) k; f.tipoPasso = (int8_t) tipo; f.pan = pan;
            regiao (f, cx, ini, Lr, vivo, encolhe);
            return f;
        };
        switch (tipo)
        {
            case tRepeat:
            {
                const int R = reps (val);
                const int64_t sub = dur / R;
                for (int j = 0; j < R; ++j)
                {
                    const int64_t a = t0 + (int64_t) dur * j / R, b = t0 + (int64_t) dur * (j + 1) / R;
                    ler (a, b - a, base, sub, 1.0f, false, 1.0f, podeVivo && j == 0);
                }
                break;
            }
            case tPitch:
            {
                int s = stPasso (val);
                if (key) s = encaixa (s, cx.pcChunk, cx.mascara);
                ler (t0, dur, base, dur, (float) std::pow (2.0, s / 12.0), false, 1.0f, podeVivo).st = (float) s;
                break;
            }
            case tReverse: ler (t0, dur, base, dur, 1.0f, true, 1.0f, false); break;
            case tSpeed:   ler (t0, dur, base, dur, v < 0.5f ? 0.5f : 2.0f, false, 1.0f, podeVivo); break;
            case tGate:    ler (t0, dur, base, dur, 1.0f, false, 0.1f + 0.9f * v, podeVivo); break;
            case tThru:    ler (t0, dur, pos, dur, 1.0f, false, 1.0f, true); break;
            case tTape:
            {
                Fatia& f = novaFatia (p);
                f = Fatia();
                f.t0 = t0; f.dur = dur; f.tipo = v < 0.5f ? fTapeStop : fTapeStart;
                f.src = podeVivo ? pos : base; f.L = 0; f.passo = (int16_t) k; f.tipoPasso = (int8_t) tipo; f.pan = pan;
                break;
            }
            default:
            {
                Fatia& f = novaFatia (p);
                f = Fatia();
                f.t0 = t0; f.dur = dur; f.tipo = fMute; f.passo = (int16_t) k; f.tipoPasso = (int8_t) tipo;
                break;
            }
        }
    }
    p.filtro = flOff; p.trem = trSquare;
    p.fadeIn = (float) (std::max (0.5f, c.f (kFadeIn)) * 0.001 * cx.fs);
    p.fadeOut = (float) (std::max (0.5f, c.f (kFadeOut)) * 0.001 * cx.fs);
}

inline void monta (const Cena& c, const Contexto& cx, Plano& p)
{
    if (c.i (kModo) == mComplex) montaComplex (c, cx, p);
    else montaEdit (c, cx, p);
}

// ---- leitura --------------------------------------------------------------------------------------------------------
inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

// anel da CAPTURE (posições absolutas; 16 s)
struct Anel
{
    std::vector<float> d[2];
    int64_t mask = 0;
    void prepare (double fs)
    {
        int64_t n = 1;
        while ((double) n < fs * 16.0) n <<= 1;
        for (auto& c : d) c.assign ((size_t) n, 0.0f);
        mask = n - 1;
    }
    float at (int c, int64_t i) const { return d[c][(size_t) (i & mask)]; }
    float ler (int c, double pos, int64_t limite) const
    {
        const int64_t i = (int64_t) std::floor (pos);
        const float t = (float) (pos - (double) i);
        if (t <= 0.0f) return at (c, i);
        if (i + 2 > limite) return at (c, i) + (at (c, std::min (i + 1, limite)) - at (c, i)) * t;
        return hermite (at (c, i - 1), at (c, i), at (c, i + 1), at (c, i + 2), t);
    }
};

// trecho linear (tela, export): posição absoluta = base + índice
struct Trecho
{
    const float* d[2] = { nullptr, nullptr };
    int64_t n = 0, base = 0;
    float at (int c, int64_t i) const { i -= base; return (i < 0 || i >= n) ? 0.0f : d[c][i]; }
    float ler (int c, double pos, int64_t limite) const
    {
        const int64_t i = (int64_t) std::floor (pos);
        const float t = (float) (pos - (double) i);
        if (t <= 0.0f) return at (c, i);
        if (i + 2 > limite) return at (c, i) + (at (c, std::min (i + 1, limite)) - at (c, i)) * t;
        return hermite (at (c, i - 1), at (c, i), at (c, i + 1), at (c, i + 2), t);
    }
};

struct Svf   // TPT (Simper)
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (float fc, float q, float fs)
    {
        const float g = std::tan ((float) kPi * lim (fc, 20.0f, 0.45f * fs) / fs);
        k = 1.0f / std::max (0.3f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void copia (const Svf& o) { a1 = o.a1; a2 = o.a2; a3 = o.a3; k = o.k; }
    void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
};

// toca um plano, amostra por amostra
struct Render
{
    const Plano* p = nullptr;
    int idx = 0, contaF = 0;
    Svf flt[2];
    float fs = 48000;
    bool primeiraVolta = true;   // HOLD: na 2ª volta a 1ª fatia também ganha FADE IN (não pula do fim para o começo)
    void inicia (const Plano& pl, float sr) { p = &pl; idx = 0; contaF = 0; flt[0].reset(); flt[1].reset(); fs = sr; primeiraVolta = true; }

    // t: posição no plano (0..len); limite: última amostra já gravada
    template <class Fonte>
    void amostra (int64_t t, int64_t limite, const Fonte& fonte, float& l, float& r)
    {
        l = r = 0.0f;
        if (p == nullptr) return;
        const Plano& P = *p;
        if (P.n == 0 || t < 0 || t >= P.len) return;
        if (idx >= P.n || t < P.f[idx].t0) idx = 0;
        while (idx + 1 < P.n && t >= P.f[idx + 1].t0) ++idx;
        const Fatia& f = P.f[idx];
        const double u = (double) (t - f.t0);
        if (u < f.dur && f.tipo != fMute)
        {
            const double fi = std::max (24.0f, P.fadeIn), fo = std::max (24.0f, P.fadeOut);
            double pos = 0.0, pos2 = -1.0, env = 1.0;
            float wx = 1.0f;
            switch (f.tipo)
            {
                case fTrem:
                {
                    pos = (double) f.src + u;
                    const double ph = u / f.dur;
                    if (P.trem == trSine) env = 0.5 - 0.5 * std::cos (2.0 * kPi * ph);
                    else if (P.trem == trSaw) env = 1.0 - ph;
                    else
                    {
                        const double gl = std::max (16.0, f.dur * (double) f.gate);
                        env = u >= gl ? 0.0 : std::min (1.0, (u + 1.0) / fi) * std::min (1.0, (gl - u) / fo);
                    }
                    break;
                }
                case fTapeStop:
                    pos = (double) f.src + u - u * u / (2.0 * f.dur);
                    env = std::min (1.0, (f.dur - u) / fo);
                    break;
                case fTapeStart:
                    pos = (double) f.src + u * u / (2.0 * f.dur);
                    env = std::min (1.0, (u + 1.0) / fi) * std::min (1.0, (f.dur - u) / fo);
                    break;
                default:
                {
                    const double gl = std::max (16.0, f.dur * (double) f.gate);
                    if (u >= gl) { env = 0.0; break; }
                    double q = f.r0 == f.r1 ? u * f.r0 : u * (f.r0 + (f.r1 - f.r0) * u / (2.0 * f.dur));
                    if (f.L > 0)
                    {
                        // a região repete em volta: na emenda, 3 ms de crossfade com a continuação (já gravada)
                        const double Ld = (double) f.L, X = std::min (Ld * 0.25, 0.003 * fs);
                        const bool volta = q >= Ld;
                        const double qm = std::fmod (q, Ld);
                        if (! f.rev)
                        {
                            pos = (double) f.src + qm;
                            if (volta && qm < X) { pos2 = (double) f.src + qm + Ld; wx = (float) (qm / X); }
                        }
                        else
                        {
                            pos = (double) f.src + Ld - 1.0 - qm;
                            if (volta && qm < X) { pos2 = (double) f.src - 1.0 - qm; wx = (float) (qm / X); }
                        }
                    }
                    else pos = (double) f.src + q;
                    const bool inicioVivo = f.t0 == 0 && f.L == 0 && primeiraVolta;
                    env = (inicioVivo ? 1.0 : std::min (1.0, (u + 1.0) / fi)) * std::min (1.0, (gl - u) / fo);
                    break;
                }
            }
            if (env > 0.0)
            {
                if (pos > (double) limite) pos = (double) limite;
                const float g = (float) env * f.ganho * P.ganho;
                l = fonte.ler (0, pos, limite) * g;
                r = fonte.ler (1, pos, limite) * g;
                if (pos2 >= 0.0)
                {
                    if (pos2 > (double) limite) pos2 = (double) limite;
                    l = l * wx + fonte.ler (0, pos2, limite) * g * (1.0f - wx);
                    r = r * wx + fonte.ler (1, pos2, limite) * g * (1.0f - wx);
                }
                if (P.estereo < 1.0f)
                {
                    const float m = 0.5f * (l + r), s = 0.5f * (l - r) * P.estereo;
                    l = m + s; r = m - s;
                }
                if (f.pan > 0.0f) l *= 1.0f - f.pan;
                else if (f.pan < 0.0f) r *= 1.0f + f.pan;
            }
        }
        if (P.filtro != flOff)
        {
            if ((contaF++ & 15) == 0)
            {
                const double pr = (double) t / (double) P.len;
                const float fc = (float) (P.fDe * std::pow ((double) P.fAte / std::max (20.0f, P.fDe), pr));
                flt[0].set (fc, 0.6f + P.reso * 7.0f, fs);
                flt[1].copia (flt[0]);
            }
            float lp, bp, hp;
            flt[0].tick (l, lp, bp, hp);
            l = P.filtro == flHpf ? hp : (P.filtro == flLpf ? lp : bp);
            flt[1].tick (r, lp, bp, hp);
            r = P.filtro == flHpf ? hp : (P.filtro == flLpf ? lp : bp);
        }
    }
};

// ---- detectores -----------------------------------------------------------------------------------------------------
// ataque de sílaba: envelope rápido passando o lento (SENSITIVITY muda o quanto e o piso)
struct Detector
{
    float aFa = 0, aFr = 0, aSa = 0, aSr = 0, envF = 0, envS = 0, k = 1.9f, piso = 0.01f;
    int refr = 0, refrN = 3000;
    bool voz = false;
    void prepare (double fs)
    {
        auto co = [fs] (double ms) { return (float) (1.0 - std::exp (-1.0 / (0.001 * ms * fs))); };
        aFa = co (0.5); aFr = co (20.0); aSa = co (30.0); aSr = co (250.0);
        refrN = (int) (0.07 * fs);
        envF = envS = 0.0f; refr = 0; voz = false;
    }
    void ajusta (float sens01)
    {
        k = 2.6f - 1.3f * sens01;
        piso = dbGanho (-34.0f - 16.0f * sens01);
    }
    bool tick (float x)
    {
        const float a = std::abs (x);
        envF += (a > envF ? aFa : aFr) * (a - envF);
        envS += (a > envS ? aSa : aSr) * (a - envS);
        voz = envS > piso * 0.6f;
        if (refr > 0) { --refr; return false; }
        if (envF > piso && envF > envS * k) { refr = refrN; return true; }
        return false;
    }
};

// tom da voz: YIN decimado → histograma de classes de altura → perfil de Krumhansl (24 tons)
struct Tom
{
    static constexpr int NB = 2048, W = 512;
    float buf[NB] {};
    int w = 0, cont = 0, novos = 0, dec = 4, analises = 0, validade = 0;
    double fsd = 12000;
    float lp1 = 0, lp2 = 0, aLp = 0.2f;
    double hist[12] {};
    int tom = -1, menor = 0, pcRecente = -1;
    std::vector<float> x, dd;

    void prepare (double fs)
    {
        dec = std::max (1, (int) std::lround (fs / 12000.0));
        fsd = fs / dec;
        aLp = (float) (1.0 - std::exp (-2.0 * kPi * 2500.0 / fs));
        x.assign (W + 200, 0.0f);
        dd.assign (200, 0.0f);
        reset();
    }
    void reset()
    {
        std::fill (std::begin (buf), std::end (buf), 0.0f);
        std::fill (std::begin (hist), std::end (hist), 0.0);
        w = cont = novos = analises = validade = 0; tom = -1; menor = 0; pcRecente = -1; lp1 = lp2 = 0;
    }
    void push (float v, bool voz)
    {
        lp1 += aLp * (v - lp1);
        lp2 += aLp * (lp1 - lp2);
        if (++cont < dec) return;
        cont = 0;
        buf[w] = lp2;
        w = (w + 1) & (NB - 1);
        if (++novos >= 256) { novos = 0; analisa (voz); }
    }
    void analisa (bool voz)
    {
        if (x.empty()) return;
        if (validade > 0 && --validade == 0) pcRecente = -1;
        const int tauMin = std::max (2, (int) (fsd / 900.0)), tauMax = std::min (190, (int) (fsd / 70.0));
        const int n = W + tauMax;
        double e = 0.0;
        for (int i = 0; i < n; ++i) x[(size_t) i] = buf[(w - n + i) & (NB - 1)];
        for (int i = 0; i < W; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
        if (! voz || e / W < 1e-5) return;
        double soma = 0.0;
        int achou = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double d = 0.0;
            for (int j = 0; j < W; ++j) { const double q = x[(size_t) j] - x[(size_t) (j + tau)]; d += q * q; }
            soma += d;
            dd[(size_t) tau] = (float) (soma > 0.0 ? d * tau / soma : 1.0);
        }
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (dd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && dd[(size_t) (tau + 1)] < dd[(size_t) tau]) ++tau;
                achou = tau;
                break;
            }
        if (achou < 1) return;
        const double a = dd[(size_t) (achou - 1)], b = dd[(size_t) achou], c = dd[(size_t) (achou + 1)];
        const double den = a - 2.0 * b + c;
        const double tauF = achou + (std::abs (den) > 1e-9 ? 0.5 * (a - c) / den : 0.0);
        const double f0 = fsd / tauF;
        if (f0 < 70.0 || f0 > 1000.0) return;
        const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (f0 / 440.0));
        const int pc = ((midi % 12) + 12) % 12;
        pcRecente = pc;
        validade = 12;
        for (auto& h : hist) h *= 0.997;
        hist[pc] += 1.0;
        if (++analises % 16 == 0) estima();
    }
    void estima()
    {
        static const double maior[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double menorP[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        double tot = 0.0;
        for (double h : hist) tot += h;
        if (tot < 20.0) return;
        const double mh = tot / 12.0;
        double melhor = -2.0;
        int bt = -1, bm = 0;
        for (int m = 0; m < 2; ++m)
        {
            const double* pf = m ? menorP : maior;
            double mp = 0;
            for (int i = 0; i < 12; ++i) mp += pf[i];
            mp /= 12;
            for (int t = 0; t < 12; ++t)
            {
                double sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < 12; ++i)
                {
                    const double xh = hist[(i + t) % 12] - mh, yp = pf[i] - mp;
                    sxy += xh * yp; sxx += xh * xh; syy += yp * yp;
                }
                const double r = sxx > 0 ? sxy / std::sqrt (sxx * syy) : 0.0;
                if (r > melhor) { melhor = r; bt = t; bm = m; }
            }
        }
        if (melhor > 0.5) { tom = bt; menor = bm; }
    }
};

// ---- motor ----------------------------------------------------------------------------------------------------------
struct Transporte
{
    bool tocando = false, temPpq = false;
    double ppq = 0.0, bpm = 120.0;
    int numerador = 4;
};

enum { evAperta, evSolta };
struct Evento { int amostra = 0, tipo = evAperta, pad = -1; float vel = 1.0f; };

class Motor
{
public:
    // estado editável: a tela e o processador escrevem; o áudio lê (e o DICE EVERY reescreve os passos)
    std::atomic<uint32_t> aGrade[maxGrade], aPasso[maxPassos], aCel[numLinhas][maxCelulas];
    std::atomic<float> aLivre[numLivre];

    // leituras para a tela
    std::atomic<float> fillProg { -1.0f };
    std::atomic<int> fillModo { 0 }, fillPad { -1 }, grooveCel { -1 }, padPedido { -1 }, padAceso { -1 }, diceVersao { 0 };
    std::atomic<bool> grooveTocando { false }, tocandoA { false };
    std::atomic<int> tomDet { -1 }, menorDet { 0 }, pcAgora { -1 };
    std::atomic<int64_t> agoraA { 0 }, compassoA { -1 };
    std::atomic<int> compassoVersao { 0 };
    std::atomic<float> spbA { 24000.0f }, bpbA { 4.0f }, bpmA { 120.0f };
    std::atomic<int> chopLen[numChops], chopVersao[numChops];
    std::atomic<int64_t> chopOrigem[numChops];
    std::atomic<int64_t> ataque[128];
    std::atomic<int> nAtaques { 0 };
    // pedidos da tela (pad 0..5; -1 = nada)
    std::atomic<int> uiAperta { -1 }, uiSolta { -1 };

    // bancada
    int64_t dbgFillInicio = -1;
    int64_t dbgVoz[64] {};
    int dbgNVoz = 0;
    float dbgDuck = 1.0f;

    Motor()
    {
        Cena c;
        escreveArrays (c);
        for (int i = 0; i < numChops; ++i) { chopLen[i] = 0; chopVersao[i] = 0; chopOrigem[i] = -1; }
        for (auto& a : ataque) a = 0;
    }

    void prepare (double sr)
    {
        fs = sr;
        cap.prepare (sr);
        det.prepare (sr);
        tom.prepare (sr);
        chopMaxCap = (int) (1.5 * sr);
        for (auto& c : chop) for (auto& d : c.d) d.assign ((size_t) chopMaxCap, 0.0f);
        for (int i = 0; i < numChops; ++i) { chop[i].len = 0; chopLen[i] = 0; chopOrigem[i] = -1; }
        for (auto& v : vozes) v.on = false;
        agora = 0; fillVivo = false; gFill = 0; pend.on = false; janela = false; chopIni = -1;
        ppqLivre = 0; ultimoAuto = ultimoCompasso = ultimoPasso = ultimoCorte = -1e9;
        duckEnv = replEnv = gmEnv = 1.0f; bpEnv = 0.0f; limG = 1.0f;
        nAtq = 0; wAtq = 0; nAtaques = 0;
        for (auto& f : hpfG) f.reset();
        for (auto& f : lpfG) f.reset();
    }

    const Anel& anel() const { return cap; }
    double taxa() const { return fs; }
    int64_t agoraAmostra() const { return agora; }
    float chopAt (int s, int c, int i) const
    {
        const auto& d = chop[lim (s, 0, numChops - 1)].d[c & 1];
        return d.empty() ? 0.0f : d[(size_t) lim (i, 0, (int) d.size() - 1)];
    }

    void escreveArrays (const Cena& c)
    {
        for (int i = 0; i < maxGrade; ++i) aGrade[i].store (c.grade[i], std::memory_order_relaxed);
        for (int i = 0; i < maxPassos; ++i) aPasso[i].store (c.passo[i], std::memory_order_relaxed);
        for (int l = 0; l < numLinhas; ++l) for (int i = 0; i < maxCelulas; ++i) aCel[l][i].store (c.cel[l][i], std::memory_order_relaxed);
        for (int i = 0; i < numLivre; ++i) aLivre[i].store (c.livre[i], std::memory_order_relaxed);
    }
    void leArrays (Cena& c) const
    {
        for (int i = 0; i < maxGrade; ++i) c.grade[i] = aGrade[i].load (std::memory_order_relaxed);
        for (int i = 0; i < maxPassos; ++i) c.passo[i] = aPasso[i].load (std::memory_order_relaxed);
        for (int l = 0; l < numLinhas; ++l) for (int i = 0; i < maxCelulas; ++i) c.cel[l][i] = aCel[l][i].load (std::memory_order_relaxed);
        for (int i = 0; i < numLivre; ++i) c.livre[i] = aLivre[i].load (std::memory_order_relaxed);
    }
    // cena atual = os parâmetros (val) + as grades
    void cenaAtual (const float* val, Cena& c) const
    {
        for (int k = 0; k < numCena; ++k) c.v[k] = val[k];
        leArrays (c);
    }

    // pads (memórias): a tela grava, o áudio lê com trava curta (se estiver ocupada, usa a cena atual)
    void gravaPad (int i, const Cena& c, bool cheio)
    {
        if (i < 0 || i >= numPads) return;
        while (trava.test_and_set (std::memory_order_acquire)) {}
        pads[i].cena = c; pads[i].cheio = cheio;
        trava.clear (std::memory_order_release);
    }
    bool lePad (int i, Cena& c)
    {
        if (i < 0 || i >= numPads) return false;
        while (trava.test_and_set (std::memory_order_acquire)) {}
        const bool ok = pads[i].cheio;
        if (ok) c = pads[i].cena;
        trava.clear (std::memory_order_release);
        return ok;
    }

    // tom em uso (root/escala dos parâmetros; AUTO = o detectado). e: 0..5 (5 = CHROMATIC)
    void tomEmUso (const float* val, int& t, int& e) const
    {
        const int r = (int) std::lround (val[kRoot]) - 1, s = (int) std::lround (val[kEscala]) - 1;
        t = r >= 0 ? r : tom.tom;
        e = s >= 0 ? s : (tom.tom >= 0 ? (tom.menor ? 1 : 0) : 5);
        if (t < 0) e = 5;
    }
    unsigned mascara (const float* val) const
    {
        int t, e;
        tomEmUso (val, t, e);
        return (t < 0 || e == 5) ? 0xFFFu : rodaMascara (mascaraEscala (e), t);
    }

    void process (float* const* chans, int nch, int n, const float* val, const Transporte& tp, const Evento* ev, int nEv)
    {
        if (n <= 0 || nch <= 0 || cap.mask == 0) return;
        det.ajusta (val[kSens] * 0.01f);
        const double bpm = tp.bpm > 1.0 ? tp.bpm : 120.0;
        spb = fs * 60.0 / bpm;
        bpb = tp.numerador > 0 ? (double) tp.numerador : 4.0;
        const bool comHost = tp.tocando && tp.temPpq;
        const double ppq0 = comHost ? tp.ppq : ppqLivre;
        const double ipb = 1.0 / spb;
        tocando = tp.tocando;
        cenaAtual (val, atual);
        masc = mascara (val);
        {
            int t, e;
            tomEmUso (val, t, e);
            raiz = t;
        }
        gatilhoM = (int) std::lround (val[kGatilho]);
        catchM = (int) std::lround (val[kCatch]);
        const int onde = (int) std::lround (val[kOnde]);
        const bool freeze = val[kChops] > 0.5f;
        const int cut = (int) std::lround (val[kCut]);
        chopMax = (int) std::min ((double) chopMaxCap, beatsChopLen ((int) std::lround (val[kChopLen])) * spb);
        const float wet = val[kDryWet] * 0.01f, outG = dbGanho (val[kOutput]);
        const bool bypass = val[kBypass] > 0.5f, limita = val[kLimit] > 0.5f;
        const float limThr = dbGanho (-0.3f);

        // pads por automação (borda de subida) e pela tela
        for (int i = 0; i < numPads; ++i)
        {
            const bool on = val[kPad1 + i] > 0.5f;
            if (on && ! padParam[i]) aperta (i, 1.0f, ppq0);
            if (! on && padParam[i]) solta (i);
            padParam[i] = on;
        }
        if (const int p = uiAperta.exchange (-1); p >= 0) aperta (p, 1.0f, ppq0);
        if (const int p = uiSolta.exchange (-1); p >= 0) solta (p);

        const double cicloBars[4] = { 1, 2, 4, 8 };
        const double C = cicloBars[onde % 4] * bpb;
        const double fillBeats = (atual.i (kModo) == mGroove ? (atual.i (kGBars) + 1) : compassosLength (atual.i (kLength))) * bpb;
        const double offAuto = onde < 4 ? std::max (0.0, C - fillBeats) : 0.0;
        const double corteB = cut == cut8 ? 0.5 : 0.25;
        const float aDuckA = 1.0f - std::exp (-1.0f / (0.003f * (float) fs)), aDuckR = 1.0f - std::exp (-1.0f / (0.06f * (float) fs));
        const float a10 = 1.0f - std::exp (-1.0f / (0.01f * (float) fs)), a5 = 1.0f - std::exp (-1.0f / (0.005f * (float) fs));
        const float passoFill = 1.0f / (0.003f * (float) fs);
        const float limRel = 1.0f - std::exp (-1.0f / (0.05f * (float) fs));

        int ie = 0;
        for (int i = 0; i < n; ++i)
        {
            const double b = ppq0 + i * ipb, b1 = b + ipb;
            while (ie < nEv && ev[ie].amostra <= i)
            {
                if (ev[ie].tipo == evAperta) aperta (ev[ie].pad, ev[ie].vel, b);
                else solta (ev[ie].pad);
                ++ie;
            }

            // ---- agenda (ppq) ----
            {
                const double tb = std::ceil (b / bpb - 1e-9) * bpb;   // compasso
                if (tb < b1 && std::abs (tb - ultimoCompasso) > 1e-6) { ultimoCompasso = tb; compasso (tb, val); }
            }
            if (gatilhoM == gAuto && tocando)
            {
                const double ta = std::ceil ((b - offAuto) / C - 1e-9) * C + offAuto;
                if (ta < b1 && std::abs (ta - ultimoAuto) > 1e-6)
                {
                    ultimoAuto = ta;
                    if (atual.i (kModo) == mGroove) abreJanela (atual, false, -1, (int64_t) (fillBeats * spb));
                    else iniciaFill (atual, false, 1.0f, -1);
                }
            }
            if (pend.on && pend.alvo < b1)
            {
                pend.on = false;
                if (pend.cena.i (kModo) == mGroove) abreJanela (pend.cena, pend.hold, pend.pad, (int64_t) ((pend.cena.i (kGBars) + 1) * bpb * spb));
                else iniciaFill (pend.cena, pend.hold, pend.vel, pend.pad);
            }
            if (janela && ! janelaHold && agora >= janelaFim) janela = false;
            const Cena& gc = janela ? cenaJanela : atual;
            const bool grooveVivo = grooveOn || janela;
            {
                const double passoB = gc.i (kGGrid) == 1 ? 1.0 / 6.0 : 0.25;
                const double tp2 = std::ceil (b / passoB - 1e-9) * passoB;
                if (tp2 < b1 && std::abs (tp2 - ultimoPasso) > 1e-6)
                {
                    ultimoPasso = tp2;
                    if (grooveVivo && ! fillVivo) passoGroove ((int64_t) std::llround (tp2 / passoB), gc);
                    else grooveCel.store (-1, std::memory_order_relaxed);
                }
            }
            if (cut != cutOnset)
            {
                const double tc = std::ceil (b / corteB - 1e-9) * corteB;
                if (tc < b1 && std::abs (tc - ultimoCorte) > 1e-6)
                {
                    ultimoCorte = tc;
                    if (! freeze)
                    {
                        if (chopIni >= 0) fechaChop (agora);
                        chopIni = det.voz ? agora : -1;
                    }
                }
            }

            // ---- entrada → captura ----
            const float inL = chans[0][i], inR = nch > 1 ? chans[1][i] : inL;
            cap.d[0][(size_t) (agora & cap.mask)] = inL;
            cap.d[1][(size_t) (agora & cap.mask)] = inR;
            const float mono = 0.5f * (inL + inR);
            if (det.tick (mono))
            {
                const int64_t o = std::max<int64_t> (0, agora - (int64_t) (0.002 * fs));
                novoAtaque (o);
                if (! freeze && cut == cutOnset)
                {
                    if (chopIni >= 0) fechaChop (o);
                    chopIni = o;
                }
            }
            if (chopIni >= 0 && agora - chopIni >= chopMax) { if (! freeze) fechaChop (chopIni + chopMax); chopIni = -1; }
            tom.push (mono, det.voz);

            // ---- fill ----
            float fl = inL, fr = inR;
            if (fillVivo || gFill > 0.0f)
            {
                int64_t t = agora - fillIni;
                if (fillVivo && fillHold && t >= plano.len) { fillIni += plano.len; t -= plano.len; render.idx = 0; render.primeiraVolta = false; }
                if (fillVivo && ! fillHold && t >= plano.len) fillVivo = false;
                if (t < plano.len) render.amostra (t, agora, cap, fl, fr);
            }
            gFill = fillVivo ? std::min (1.0f, gFill + passoFill) : std::max (0.0f, gFill - passoFill);

            // ---- groove ----
            float gl = 0.0f, gr = 0.0f;
            bool algum = false;
            for (auto& v : vozes)
            {
                if (! v.on) continue;
                if (v.espera > 0) { --v.espera; continue; }
                if (v.pos >= v.len) { v.on = false; continue; }
                const int i0 = (int) v.pos;
                const float t = (float) (v.pos - i0);
                const auto& cd = chop[v.chop].d;
                const int i1 = std::min (i0 + 1, chopMaxCap - 1);
                const float e = (float) (std::min (1.0, v.pos / (0.001 * fs)) * std::min (1.0, (v.len - v.pos) / (0.006 * fs))) * v.g;
                gl += (cd[0][(size_t) i0] + (cd[0][(size_t) i1] - cd[0][(size_t) i0]) * t) * e;
                gr += (cd[1][(size_t) i0] + (cd[1][(size_t) i1] - cd[1][(size_t) i0]) * t) * e;
                v.pos += v.rate;
                algum = true;
            }
            {
                if ((i & 31) == 0)
                {
                    hpfG[0].set (gc.f (kChopHpf), 0.707f, (float) fs); hpfG[1].copia (hpfG[0]);
                    lpfG[0].set (gc.f (kChopLpf), 0.707f, (float) fs); lpfG[1].copia (lpfG[0]);
                }
                float lp, bp, hp;
                hpfG[0].tick (gl, lp, bp, hp); gl = hp; lpfG[0].tick (gl, lp, bp, hp); gl = lp;
                hpfG[1].tick (gr, lp, bp, hp); gr = hp; lpfG[1].tick (gr, lp, bp, hp); gr = lp;
            }
            const int mixVoz = gc.i (kMixVoz);
            const bool grooveSoa = grooveVivo && ! fillVivo;
            const float dAlvo = (grooveSoa && mixVoz == mvDuck && algum) ? dbGanho (gc.f (kDuck)) : 1.0f;
            duckEnv += (dAlvo < duckEnv ? aDuckA : aDuckR) * (dAlvo - duckEnv);
            const float rAlvo = (grooveSoa && mixVoz == mvReplace) ? 0.0f : 1.0f;
            replEnv += a10 * (rAlvo - replEnv);
            gmEnv += a5 * ((fillVivo ? 0.0f : 1.0f) - gmEnv);
            dbgDuck = duckEnv;
            const float chopVol = dbGanho (gc.f (kChopVol));

            // ---- mistura ----
            const float sl = (inL * (1.0f - gFill) + fl * gFill) * duckEnv * replEnv + gl * gmEnv * chopVol;
            const float sr = (inR * (1.0f - gFill) + fr * gFill) * duckEnv * replEnv + gr * gmEnv * chopVol;
            float oL = (inL * (1.0f - wet) + sl * wet) * outG;
            float oR = (inR * (1.0f - wet) + sr * wet) * outG;
            if (limita)
            {
                const float pk = std::max (std::abs (oL), std::abs (oR));
                if (pk * limG > limThr) limG = limThr / pk;
                else limG += limRel * (1.0f - limG);
                oL *= limG; oR *= limG;
            }
            bpEnv += a5 * ((bypass ? 1.0f : 0.0f) - bpEnv);
            if (bypass && bpEnv > 0.999f) bpEnv = 1.0f;
            if (bpEnv > 0.0f) { oL += (inL - oL) * bpEnv; oR += (inR - oR) * bpEnv; }
            if (nch > 1) { chans[0][i] = oL; chans[1][i] = oR; }
            else chans[0][i] = 0.5f * (oL + oR);
            ++agora;
        }

        ppqLivre = ppq0 + n * ipb;
        // leituras
        agoraA.store (agora, std::memory_order_relaxed);
        spbA = (float) spb; bpbA = (float) bpb; bpmA = (float) bpm; tocandoA = tocando;
        if (fillVivo) fillProg = (float) ((double) (agora - fillIni) / (double) std::max<int64_t> (1, plano.len));
        else fillProg = -1.0f;
        grooveTocando = grooveOn || janela;
        tomDet = tom.tom; menorDet = tom.menor; pcAgora = tom.pcRecente;
        padAceso = fillVivo ? fillPadAtual : (janela ? janelaPad : -1);
    }

private:
    struct Pad { Cena cena; bool cheio = false; };
    struct Chop { std::vector<float> d[2]; int len = 0; };
    struct Voz { bool on = false; int chop = 0, espera = 0; double pos = 0, rate = 1, len = 0; float g = 1; };
    struct Pendente { bool on = false, hold = false; double alvo = 0; float vel = 1; int pad = -1; Cena cena; };

    double fs = 48000.0, spb = 24000.0, bpb = 4.0, ppqLivre = 0.0;
    double ultimoAuto = -1e9, ultimoCompasso = -1e9, ultimoPasso = -1e9, ultimoCorte = -1e9;
    int64_t agora = 0;
    bool tocando = false;
    Anel cap;
    Detector det;
    Tom tom;
    unsigned masc = 0xFFFu;
    int raiz = -1, gatilhoM = gAuto, catchM = caBar;

    Cena atual, cenaJanela;
    Pad pads[numPads];
    std::atomic_flag trava = ATOMIC_FLAG_INIT;
    bool padParam[numPads] {};

    // fill
    Plano plano;
    Render render;
    Ataques atq;
    bool fillVivo = false, fillHold = false;
    int64_t fillIni = 0;
    int fillPadAtual = -1;
    float gFill = 0.0f;
    Pendente pend;

    // groove
    Chop chop[numChops];
    Voz vozes[16];
    int chopMaxCap = 72000, chopMax = 12000, proxChop = 0;
    int64_t chopIni = -1;
    bool grooveOn = false, janela = false, janelaHold = false;
    int64_t janelaFim = 0;
    int janelaPad = -1;
    Svf hpfG[2], lpfG[2];
    float duckEnv = 1, replEnv = 1, gmEnv = 1, bpEnv = 0, limG = 1;

    // ataques (anel com os 128 últimos, em ordem)
    int64_t atqAnel[128] {};
    int nAtq = 0, wAtq = 0;

    void novoAtaque (int64_t o)
    {
        atqAnel[wAtq] = o;
        ataque[wAtq].store (o, std::memory_order_relaxed);
        wAtq = (wAtq + 1) & 127;
        nAtq = std::min (nAtq + 1, 128);
        nAtaques.store (nAtaques.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }
    void copiaAtaques()
    {
        atq.n = nAtq;
        for (int i = 0; i < nAtq; ++i) atq.v[i] = atqAnel[(wAtq - nAtq + i + 128) & 127];
    }

    double alvoCatch (double b) const
    {
        const double g = catchM == ca16 ? 0.25 : catchM == ca4 ? 1.0 : catchM == caBar ? bpb : 0.0;
        if (g <= 0.0) return b;
        double a = std::ceil (b / g - 1e-9) * g;
        if (a - b < 1e-7) a = b;
        return a;
    }

    void aperta (int pad, float vel, double b)
    {
        if (gatilhoM == gAuto)
        {
            if (pad >= 0) padPedido = pad;   // AUTO: o pad só escolhe a cena (a tela carrega nos parâmetros)
            return;
        }
        Cena& c = pend.cena;
        bool ok = false;
        if (pad >= 0 && ! trava.test_and_set (std::memory_order_acquire))
        {
            if (pads[pad].cheio) { c = pads[pad].cena; ok = true; }
            trava.clear (std::memory_order_release);
        }
        if (! ok) c = atual;
        if (pad >= 0) padPedido = pad;
        pend.hold = gatilhoM == gHold;
        pend.vel = lim (vel, 0.05f, 1.0f);
        pend.pad = pad;
        pend.alvo = alvoCatch (b);
        pend.on = true;
    }
    void solta (int pad)
    {
        if (gatilhoM != gHold) return;
        if (pend.on && pend.pad == pad) pend.on = false;
        if (fillVivo && fillHold && fillPadAtual == pad) fillVivo = false;
        if (janela && janelaHold && janelaPad == pad) janela = false;
    }

    void iniciaFill (const Cena& c, bool hold, float vel, int pad)
    {
        copiaAtaques();
        Contexto cx;
        cx.spb = spb; cx.bpb = bpb; cx.fs = fs; cx.inicio = agora; cx.mascara = masc;
        cx.pcChunk = tom.pcRecente >= 0 ? tom.pcRecente : raiz;
        cx.at = &atq;
        monta (c, cx, plano);
        plano.ganho = vel;
        render.inicia (plano, (float) fs);
        fillIni = agora;
        fillVivo = true;
        fillHold = hold;
        fillPadAtual = pad;
        fillModo = c.i (kModo);
        fillPad = pad;
        dbgFillInicio = agora;
    }
    void abreJanela (const Cena& c, bool hold, int pad, int64_t len)
    {
        cenaJanela = c;
        janela = true;
        janelaHold = hold;
        janelaPad = pad;
        janelaFim = agora + std::max<int64_t> (64, len);
    }

    void compasso (double tb, const float* val)
    {
        compassoA.store (agora, std::memory_order_relaxed);
        compassoVersao.store (compassoVersao.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        grooveOn = val[kGrooveOn] > 0.5f;   // GROOVE liga e desliga no compasso
        const int64_t bar = (int64_t) std::llround (tb / bpb);
        static const int every[5] = { 0, 1, 2, 4, 8 };
        const int ev = every[lim (atual.i (kDEvery), 0, 4)];
        if (ev > 0 && atual.i (kModo) == mComplex && gatilhoM == gAuto && tocando && bar % ev == 0)
        {
            uint32_t p[maxPassos];
            for (int i = 0; i < maxPassos; ++i) p[i] = aPasso[i].load (std::memory_order_relaxed);
            dadoPassos (p, 8 << lim (atual.i (kPassos), 0, 2), atual.i (kDMode), atual.f (kDAmount) * 0.01f, atual.f (kDChaos) * 0.01f,
                        (uint32_t) (atual.i (kSeed) * 977 + (int) (bar / ev)));
            for (int i = 0; i < maxPassos; ++i) aPasso[i].store (p[i], std::memory_order_relaxed);
            diceVersao.store (diceVersao.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        }
    }

    void passoGroove (int64_t k, const Cena& gc)
    {
        const int porC = gc.i (kGGrid) == 1 ? 12 : 16;
        const int nCel = std::min (maxCelulas, porC * (gc.i (kGBars) + 1));
        const int cel = (int) (((k % nCel) + nCel) % nCel);
        grooveCel.store (cel, std::memory_order_relaxed);
        const float dens = gc.f (kDensity) * 0.01f, hum = gc.f (kHumanize) * 0.01f;
        const double passoAm = spb * (porC == 12 ? 1.0 / 6.0 : 0.25);
        int st = (int) std::lround (gc.f (kGPitch));
        if (gc.b (kKey)) st = encaixa (st, tom.pcRecente >= 0 ? tom.pcRecente : raiz, masc);
        const double rate = std::pow (2.0, st / 12.0);
        static const float limiar[numLinhas] = { 0.0f, 0.35f, 0.65f };
        for (int l = 0; l < numLinhas; ++l)
        {
            const uint32_t c = gc.cel[l][cel];
            int s = cChop (c);
            if (s < 0) continue;
            const float h = hash01 ((uint32_t) (cel * 31 + l * 7 + 1));
            if (dens <= limiar[l] + (l > 0 ? 0.3f * h : 0.0f)) continue;
            if (gc.i (kEstilo) == eCallResp && det.voz) continue;   // a voz canta, o chop responde no vão
            if (chop[s].len < 64)
            {
                int alt = -1;
                for (int j = 1; j < numChops && alt < 0; ++j) if (chop[(s + j) % numChops].len >= 64) alt = (s + j) % numChops;
                if (alt < 0) continue;
                s = alt;
            }
            float vel = (float) cVel (c) / 255.0f;
            const float h2 = hash01 ((uint32_t) (k * 131 + l * 17 + 5)), h3 = hash01 ((uint32_t) (k * 71 + l * 29 + 3));
            vel *= 1.0f - 0.3f * hum * h3;
            int espera = (int) (h2 * 0.012 * fs * hum);
            if ((k & 1) != 0) espera += (int) (gc.f (kGSwing) * 0.01 * passoAm * 0.5);
            const int nb = cRoll (c) ? 3 : 1;
            for (int j = 0; j < nb; ++j) tocaChop (s, espera + (int) (j * passoAm / 3.0), vel * (j == 0 ? 1.0f : 0.8f), rate);
        }
    }
    void tocaChop (int s, int espera, float vel, double rate)
    {
        Voz* v = nullptr;
        for (auto& x : vozes) if (! x.on) { v = &x; break; }
        if (v == nullptr)
        {
            v = &vozes[0];
            for (auto& x : vozes) if (x.pos > v->pos) v = &x;   // rouba a mais adiantada
        }
        v->on = true; v->chop = s; v->espera = std::max (0, espera); v->pos = 0; v->rate = rate; v->g = vel;
        v->len = (double) std::min (chop[s].len, chopMax);
        if (dbgNVoz < 64) dbgVoz[dbgNVoz++] = agora + v->espera;
    }
    void fechaChop (int64_t fim)
    {
        const int64_t ini = chopIni;
        chopIni = -1;
        if (ini < 0) return;
        const int len = (int) std::min<int64_t> (fim - ini, chopMax);
        if (len < (int) (0.03 * fs)) return;
        float pico = 0.0f;
        for (int64_t j = ini; j < ini + len; j += 4) pico = std::max (pico, std::abs (cap.at (0, j)) + std::abs (cap.at (1, j)));
        if (pico < 0.02f) return;
        int s = proxChop;
        for (int t = 0; t < numChops; ++t)
        {
            bool usando = false;
            for (auto& v : vozes) if (v.on && v.chop == s) usando = true;
            if (! usando) break;
            s = (s + 1) % numChops;
        }
        for (int c = 0; c < 2; ++c)
        {
            float* d = chop[s].d[c].data();
            for (int j = 0; j < len; ++j) d[j] = cap.at (c, ini + j);
        }
        chop[s].len = len;
        chopLen[s].store (len, std::memory_order_relaxed);
        chopOrigem[s].store (ini, std::memory_order_relaxed);
        chopVersao[s].store (chopVersao[s].load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        proxChop = (s + 1) % numChops;
    }
};

} // namespace retalho
