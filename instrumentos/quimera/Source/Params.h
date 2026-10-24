#pragma once

// QUIMERA — sintetizador da InsideStudioSP. Este arquivo é o CONTRATO entre motor, efeitos, tela e presets:
// ids dos parâmetros, faixas, listas e a foto (Snapshot) que o processador entrega ao motor a cada bloco.

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "Scales.h"
#include <utility>
#include <vector>

namespace quimera
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline constexpr int numOsc = 3, numFilters = 2, numEnvs = 3, numLfos = 2, numSlots = 16, numMacros = 4;
inline constexpr int maxVoices = 16, maxUnison = 8;

// ---------------- listas (o parâmetro guarda o ÍNDICE) ----------------

// tabelas dos osciladores: 0–3 analógicas, 4–11 modeladas, 12–23 wavetables
inline constexpr int numTables = 24, firstModeled = 4, firstWavetable = 12;
inline const char* const tableNames[numTables] = {
    // Analógico
    "Classica",       // 0  seno → triângulo → serra → quadrada
    "Pulso",          // 1  quadrada 50% → pulso fino 5% (PWM)
    "Serra Dupla",    // 2  duas serras, a 2ª defasada pela posição (oca → cheia)
    "Triangulo Dobrado", // 3  triângulo com dobra (wavefolder) crescente
    // Modelado
    "Metal",          // 4  harmônicos esticados, ressoa metálico
    "Formante",       // 5  vogais A-E-I-O-U ao longo da posição
    "Sincronizado",   // 6  hard sync, escrava de 1× a 8×
    "Dobra",          // 7  distorção de fase (bend) crescente
    "Eletrico",       // 8  quadrada/serra com agudos reforçados
    "Quadrada-Serra", // 9  quadrada → serra
    "Seno-Quadrada",  // 10 seno saturando até quadrada
    "Escova",         // 11 serra com pente de fase (brilho chiado)
    // Wavetable
    "Cristal",        // 12 harmônicos altos esparsos, sino de vidro
    "Digital",        // 13 formas quantizadas em degraus
    "Vocal",          // 14 coro de formantes
    "Harmonicos",     // 15 harmônico n sozinho ao longo da posição (aditivo)
    "Granito",        // 16 espectros aleatórios suavizados (textura)
    "Sinos",          // 17 espectros de sino (FM) em razões crescentes
    "FM Linear",      // 18 FM 2 operadores razão 1, índice crescente
    "FM Metalico",    // 19 FM 2 operadores razão 3,5, índice crescente
    "Espectral",      // 20 pente espectral que anda
    "Orgao",          // 21 combinações de drawbar
    "Corda",          // 22 espectro de corda dedilhada (brilho cai ao longo)
    "Quimera",        // 23 meio orgânica, meio metálica: serra → FM metálica
};
// nome na tela (com acento)
inline juce::String tableLabel (int i)
{
    static const char* const ui[numTables] = { "Clássica", "Pulso", "Serra Dupla", "Triângulo Dobrado", "Metal", "Formante",
        "Sincronizado", "Dobra", "Elétrico", "Quadrada-Serra", "Seno-Quadrada", "Escova", "Cristal", "Digital", "Vocal",
        "Harmônicos", "Granito", "Sinos", "FM Linear", "FM Metálico", "Espectral", "Órgão", "Corda", "Quimera" };
    return u8 (ui[juce::jlimit (0, numTables - 1, i)]);
}
inline juce::String tableGroup (int i)
{
    return i < firstModeled ? u8 ("ANALÓGICO") : i < firstWavetable ? "MODELADO" : "WAVETABLE";
}

// warp do oscilador (quantidade em oNwamt, 0–100)
inline constexpr int numWarps = 11;
inline const char* const warpNames[numWarps] = { "Off", "Sync", "Bend +", "Bend -", "PWM", "Mirror",
                                                 "Fold", "Quantize", "FM", "RM", "Asym" };
// FM e RM: o modulador é o oscilador anterior (osc 1 ← osc 3, osc 2 ← osc 1, osc 3 ← osc 2)

inline const char* const routeNames[4] = { "Filter A", "Filter B", "A + B", "Direct" };

inline constexpr int numFilterTypes = 8;
inline const char* const filterNames[numFilterTypes] = { "Ladder 24", "Ladder 12", "LP 12", "HP 12", "BP 12",
                                                         "Notch", "Comb", "Formant" };

inline const char* const subWaveNames[3] = { "Sine", "Triangle", "Square" };
inline const char* const polyNames[3]    = { "Poly", "Mono", "Legato" };

inline constexpr int numLfoShapes = 7;
inline const char* const lfoShapeNames[numLfoShapes] = { "Sine", "Triangle", "Saw Down", "Saw Up",
                                                         "Square", "S&H", "Smooth Random" };
// Free = corre solto; Retrig = reinicia a cada nota; Envelope = uma volta só e para
inline const char* const lfoModeNames[3] = { "Free", "Retrig", "Envelope" };

// divisões do grid do host (em semínimas) — iguais às dos outros plugins da casa
inline constexpr int numDivs = 14;
inline const char* const divNames[numDivs] = { "1/32", "1/16T", "1/16", "1/8T", "3/32", "1/8", "1/4T",
                                               "3/16", "1/4", "3/8", "1/2", "1 comp", "2 comp", "4 comp" };
inline constexpr double divBeats[numDivs] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0,
                                              0.75, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };

// ---------------- arpejador ----------------
inline constexpr int numArpSteps = 16, numArpModes = 9;
enum ArpMode { arpUp, arpDown, arpUpDown, arpDownUp, arpRandom, arpAsPlayed, arpChord, arpConverge, arpDiverge };
inline const char* const arpModeNames[numArpModes] = { "Up", "Down", "Up/Down", "Down/Up", "Random", "As Played", "Chord",
                                                       "Converge", "Diverge" };

// ---------------- matriz de modulação ----------------
// Fontes: env 0..1; LFO −1..1; velocidade 0..1; nota (nota−60)/60; mod wheel 0..1; pressão 0..1 (MPE ou aftertouch);
// slide 0..1 (MPE CC74); bend −1..1; aleatório por nota −1..1 (sorteado no note-on); macros 0..1.
enum Src { srcNone, srcEnv1, srcEnv2, srcEnv3, srcLfo1, srcLfo2, srcVel, srcNote, srcWheel, srcPressure,
           srcSlide, srcBend, srcRandom, srcMacro1, srcMacro2, srcMacro3, srcMacro4, numSrcs };
inline const char* const srcNames[numSrcs] = { "-", "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "Velocity", "Note",
                                               "Mod Wheel", "Pressure", "Slide", "Bend", "Random",
                                               "Macro 1", "Macro 2", "Macro 3", "Macro 4" };

// Destinos e escala (quantidade ±100% × fonte):
//  altura: ±48 semitons | corte: ±7 oitavas | velocidade do LFO: ±4 oitavas
//  posição, warp, volumes, ressonância, drive, detune: ±toda a faixa (100% = 0→1) | pan: ±toda a faixa
//  dstAmp (volume da voz): ganho × max(0, 1 + quantidade × fonte)
enum Dst { dstNone,
           dstO1Pitch, dstO1Pos, dstO1Warp, dstO1Level, dstO1Pan,
           dstO2Pitch, dstO2Pos, dstO2Warp, dstO2Level, dstO2Pan,
           dstO3Pitch, dstO3Pos, dstO3Warp, dstO3Level, dstO3Pan,
           dstPitch, dstSubLevel, dstNoiseLevel,
           dstFACut, dstFARes, dstFADrive, dstFBCut, dstFBRes, dstFBDrive,
           dstAmp, dstPan, dstLfo1Rate, dstLfo2Rate, dstDetune, numDsts };
// destino do osc n (0..2): dstO1Pitch + n * 5 + {0 altura, 1 posição, 2 warp, 3 volume, 4 pan}
inline const char* const dstNames[numDsts] = { "-",
    "Osc 1 Pitch", "Osc 1 Position", "Osc 1 Warp", "Osc 1 Level", "Osc 1 Pan",
    "Osc 2 Pitch", "Osc 2 Position", "Osc 2 Warp", "Osc 2 Level", "Osc 2 Pan",
    "Osc 3 Pitch", "Osc 3 Position", "Osc 3 Warp", "Osc 3 Level", "Osc 3 Pan",
    "Pitch (All)", "Sub Level", "Noise Level",
    "Filter A Cutoff", "Filter A Res", "Filter A Drive", "Filter B Cutoff", "Filter B Res", "Filter B Drive",
    "Volume", "Pan", "LFO 1 Rate", "LFO 2 Rate", "Unison Detune" };

// ---------------- efeitos (ordem fixa: HYPER → DISTORTION → PHASER → CHORUS → DELAY → REVERB → EQ → COMPRESSOR) ----------------
inline const char* const distNames[6]   = { "Tube", "Hard Clip", "Fold", "Bitcrush", "Downsample", "Asym" };
inline const char* const reverbNames[3] = { "Room", "Plate", "Hall" };
inline const char* const compNames[2]   = { "Single", "Multiband" };   // multiband = 3 bandas, sobe e desce (estilo OTT)

// ---------------- categorias dos presets ----------------
// (o preset guarda o ÍNDICE da categoria; o nome só aparece no menu)
// ARP entrou depois: fica no fim para não mudar o índice das outras (bancos e projetos antigos)
inline constexpr int numCategories = 10;
inline const char* const categoryNames[numCategories] = { "BASS", "LEAD", "PLUCK", "PAD", "ATMOS", "PERC",
                                                          "DRUMS", "KEYS", "FX", "ARP" };

// ---------------- ids ----------------
// Osc n = 1..3:  o<n>on o<n>tab o<n>pos o<n>warp o<n>wamt o<n>oct o<n>semi o<n>fine o<n>lvl o<n>pan
//                o<n>uni o<n>det o<n>wid o<n>rand o<n>rt
// Filtro A/B:    fa.. / fb..: on type cut res drv key env mix   (ex.: "facut", "fbres")
// Env n = 1..3:  e<n>a e<n>d e<n>s e<n>r e<n>c       (1 = VOLUME, 2 = FILTER, 3 = MOD)
// LFO n = 1..2:  l<n>shape l<n>sync l<n>rate l<n>div l<n>mode l<n>rise
// Matriz 01..16: m<nn>src m<nn>dst m<nn>amt
inline juce::String oscId (int osc, const char* field)    { return "o" + juce::String (osc + 1) + field; }
inline juce::String filterId (int f, const char* field)   { return juce::String (f == 0 ? "fa" : "fb") + field; }
inline juce::String envId (int e, const char* field)      { return "e" + juce::String (e + 1) + field; }
inline juce::String lfoId (int l, const char* field)      { return "l" + juce::String (l + 1) + field; }
inline juce::String slotId (int s, const char* field)     { return "m" + juce::String (s + 1).paddedLeft ('0', 2) + field; }
inline juce::String macroId (int m)                       { return "mac" + juce::String (m + 1); }
// Arp: passo n = 1..16 ligado "as01".."as16", com acento "aa01".."aa16"
inline juce::String arpStepId (int s)                     { return "as" + juce::String (s + 1).paddedLeft ('0', 2); }
inline juce::String arpAccId (int s)                      { return "aa" + juce::String (s + 1).paddedLeft ('0', 2); }

namespace id
{
    // sub e ruído
    inline constexpr const char* subOn = "subon";   inline constexpr const char* subWave = "subwave";
    inline constexpr const char* subOct = "suboct"; inline constexpr const char* subLvl = "sublvl";
    inline constexpr const char* subRt = "subrt";
    inline constexpr const char* noiseOn = "nzon";  inline constexpr const char* noiseColor = "nzcol";
    inline constexpr const char* noiseLvl = "nzlvl"; inline constexpr const char* noiseRt = "nzrt";
    // filtros em série (A → B) ou paralelo
    inline constexpr const char* filterSerial = "fltser";
    // voz
    inline constexpr const char* polyMode = "polymode"; inline constexpr const char* voices = "voices";
    inline constexpr const char* glide = "glide";       inline constexpr const char* bend = "bend";
    inline constexpr const char* drift = "drift";       inline constexpr const char* velAmp = "velamp";
    inline constexpr const char* volume = "vol";
    // performance / controlador: trocar de preset NÃO mexe nestes
    inline constexpr const char* mpe = "mpe";           inline constexpr const char* mpeBend = "mpebend";
    inline constexpr const char* hold = "hold";
    // efeitos
    inline constexpr const char* hypOn = "hyon";  inline constexpr const char* hypRate = "hyrate";
    inline constexpr const char* hypDetune = "hydet"; inline constexpr const char* hypMix = "hymix";
    inline constexpr const char* dstOn = "dson";  inline constexpr const char* dstType = "dstype";
    inline constexpr const char* dstDrive = "dsdrv"; inline constexpr const char* dstTone = "dstone";
    inline constexpr const char* dstMix = "dsmix";
    inline constexpr const char* phsOn = "phon";  inline constexpr const char* phsRate = "phrate";
    inline constexpr const char* phsDepth = "phdep"; inline constexpr const char* phsFb = "phfb";
    inline constexpr const char* phsMix = "phmix";
    inline constexpr const char* choOn = "chon";  inline constexpr const char* choRate = "chrate";
    inline constexpr const char* choDepth = "chdep"; inline constexpr const char* choMix = "chmix";
    inline constexpr const char* dlyOn = "dlon";  inline constexpr const char* dlyDiv = "dldiv";
    inline constexpr const char* dlyFb = "dlfb";  inline constexpr const char* dlyPing = "dlpp";
    inline constexpr const char* dlyTone = "dltone"; inline constexpr const char* dlyMix = "dlmix";
    inline constexpr const char* revOn = "rvon";  inline constexpr const char* revType = "rvtype";
    inline constexpr const char* revSize = "rvsize"; inline constexpr const char* revDecay = "rvdecay";
    inline constexpr const char* revDamp = "rvdamp"; inline constexpr const char* revMix = "rvmix";
    inline constexpr const char* eqOn = "eqon";   inline constexpr const char* eqLow = "eqlow";
    inline constexpr const char* eqMid = "eqmid"; inline constexpr const char* eqMidF = "eqmidf";
    inline constexpr const char* eqHigh = "eqhigh";
    inline constexpr const char* cmpOn = "cpon";  inline constexpr const char* cmpMode = "cpmode";
    inline constexpr const char* cmpAmt = "cpamt"; inline constexpr const char* cmpMix = "cpmix";
    // arpejador (entra no preset; o SEGURAR trava o acorde)
    inline constexpr const char* arpOn = "arpon";     inline constexpr const char* arpMode = "arpmode";
    inline constexpr const char* arpRate = "arprate"; inline constexpr const char* arpOct = "arpoct";
    inline constexpr const char* arpGate = "arpgate"; inline constexpr const char* arpSwing = "arpswing";
    inline constexpr const char* arpLen = "arplen";   inline constexpr const char* arpAcc = "arpacc";
    inline constexpr const char* arpScale = "arpscale"; inline constexpr const char* arpRoot = "arproot";
}

// ---------------- textos ----------------
inline juce::String num1 (float v) { return juce::String (v, 1).replaceCharacter ('.', ','); }

inline juce::String hzText (float hz)
{
    return hz >= 1000.0f ? num1 (hz / 1000.0f) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
}

inline juce::String dbText (float v)
{
    const auto s = num1 (std::abs (v)) + " dB";
    return v > 0.05f ? "+" + s : v < -0.05f ? u8 ("−") + s : s;
}

inline juce::String msText (float ms)
{
    return ms >= 1000.0f ? num1 (ms / 1000.0f) + " s" : juce::String (juce::roundToInt (ms)) + " ms";
}

inline juce::String signedPct (float v)
{
    const int r = juce::roundToInt (v);
    return r > 0 ? "+" + juce::String (r) + "%" : r < 0 ? u8 ("−") + juce::String (-r) + "%" : "0%";
}

inline juce::StringArray toArray (const char* const* items, int n)
{
    juce::StringArray a;
    for (int i = 0; i < n; ++i) a.add (u8 (items[i]));
    return a;
}

// ---------------- layout ----------------
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&layout] (const String& pid, const String& name, NormalisableRange<float> range, float def,
                          std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto choice = [&layout] (const String& pid, const String& name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, items, def));
    };
    auto toggle = [&layout] (const String& pid, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def));
    };
    auto pct    = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto spct   = [] (float v, int) { return signedPct (v); };
    auto semis  = [] (float v, int) { const int r = roundToInt (v); return (r > 0 ? "+" : "") + String (r) + " st"; };
    auto cents  = [] (float v, int) { const int r = roundToInt (v); return (r > 0 ? "+" : "") + String (r) + " ct"; };
    auto ms     = [] (float v, int) { return msText (v); };
    auto pan    = [] (float v, int) { const int r = roundToInt (v); return r == 0 ? String ("C") : r < 0 ? "L" + String (-r) : "R" + String (r); };

    NormalisableRange<float> cutRange (20.0f, 20000.0f);
    cutRange.setSkewForCentre (900.0f);
    NormalisableRange<float> timeRange (0.5f, 20000.0f);
    timeRange.setSkewForCentre (400.0f);
    NormalisableRange<float> rateRange (0.02f, 40.0f);
    rateRange.setSkewForCentre (2.0f);
    NormalisableRange<float> glideRange (0.0f, 3000.0f);
    glideRange.setSkewForCentre (250.0f);
    NormalisableRange<float> midRange (100.0f, 10000.0f);
    midRange.setSkewForCentre (1000.0f);

    const auto tables = toArray (tableNames, numTables);
    const auto divs   = toArray (divNames, numDivs);

    // ---- osciladores: o 1 ligado em serra clássica, o 2 e o 3 desligados ----
    for (int o = 0; o < numOsc; ++o)
    {
        const String n = "Osc " + String (o + 1) + " ";
        toggle (oscId (o, "on"),   n + "Ligado", o == 0);
        choice (oscId (o, "tab"),  n + "Wavetable", tables, o == 0 ? 0 : o == 1 ? 5 : 12);
        add (oscId (o, "pos"),     n + "Position", { 0.0f, 100.0f }, o == 0 ? 66.7f : 0.0f, pct);
        choice (oscId (o, "warp"), n + "Warp", toArray (warpNames, numWarps), 0);
        add (oscId (o, "wamt"),    n + "Warp Amount", { 0.0f, 100.0f }, 0.0f, pct);
        add (oscId (o, "oct"),     n + "Octave", { -3.0f, 3.0f, 1.0f }, 0.0f,
             [] (float v, int) { const int r = roundToInt (v); return (r > 0 ? "+" : "") + String (r); });
        add (oscId (o, "semi"),    n + "Semi", { -12.0f, 12.0f, 1.0f }, 0.0f, semis);
        add (oscId (o, "fine"),    n + "Fine", { -100.0f, 100.0f }, 0.0f, cents);
        add (oscId (o, "lvl"),     n + "Level", { 0.0f, 100.0f }, 75.0f, pct);
        add (oscId (o, "pan"),     n + "Pan", { -100.0f, 100.0f }, 0.0f, pan);
        add (oscId (o, "uni"),     n + "Unison", { 1.0f, (float) maxUnison, 1.0f }, 1.0f,
             [] (float v, int) { return String (roundToInt (v)) + "x"; });
        add (oscId (o, "det"),     n + "Detune", { 0.0f, 100.0f }, 25.0f, pct);
        add (oscId (o, "wid"),     n + "Width", { 0.0f, 100.0f }, 70.0f, pct);
        add (oscId (o, "rand"),    n + "Random Phase", { 0.0f, 100.0f }, 100.0f, pct);
        choice (oscId (o, "rt"),   n + "Output", toArray (routeNames, 4), 0);
    }

    toggle (id::subOn, "Sub Ligado", false);
    choice (id::subWave, "Sub Waveform", toArray (subWaveNames, 3), 0);
    choice (id::subOct, "Sub Octave", { "-1", "-2" }, 0);
    add (id::subLvl, "Sub Level", { 0.0f, 100.0f }, 50.0f, pct);
    choice (id::subRt, "Sub Output", toArray (routeNames, 4), 0);

    toggle (id::noiseOn, "Noise Ligado", false);
    add (id::noiseColor, "Noise Cor", { 0.0f, 100.0f }, 50.0f,
         [] (float v, int) { return v < 33.0f ? String ("Escuro") : v < 66.0f ? String ("Branco") : String ("Brilhante"); });
    add (id::noiseLvl, "Noise Level", { 0.0f, 100.0f }, 30.0f, pct);
    choice (id::noiseRt, "Noise Output", toArray (routeNames, 4), 0);

    // ---- filtros: A ladder 24 aberto, B desligado ----
    for (int f = 0; f < numFilters; ++f)
    {
        const String n = String ("Filter ") + (f == 0 ? "A " : "B ");
        toggle (filterId (f, "on"),   n + "Ligado", f == 0);
        choice (filterId (f, "type"), n + "Tipo", toArray (filterNames, numFilterTypes), f == 0 ? 0 : 3);
        add (filterId (f, "cut"),     n + "Cutoff", cutRange, f == 0 ? 6000.0f : 120.0f, [] (float v, int) { return hzText (v); });
        add (filterId (f, "res"),     n + "Resonance", { 0.0f, 100.0f }, 15.0f, pct);
        add (filterId (f, "drv"),     n + "Drive", { 0.0f, 100.0f }, 0.0f, pct);
        add (filterId (f, "key"),     n + "Key Track", { 0.0f, 100.0f }, 0.0f, pct);
        add (filterId (f, "env"),     n + "Env 2", { -100.0f, 100.0f }, 0.0f, spct);   // ±7 oitavas
        add (filterId (f, "mix"),     n + "Dry/Wet", { 0.0f, 100.0f }, 100.0f, pct);
    }
    toggle (id::filterSerial, "Filters Serial", false);

    // ---- envelopes ----
    for (int e = 0; e < numEnvs; ++e)
    {
        const String n = "Env " + String (e + 1) + " ";
        add (envId (e, "a"), n + "Attack",   timeRange, e == 0 ? 2.0f : 1.0f, ms);
        add (envId (e, "d"), n + "Decay",    timeRange, e == 0 ? 300.0f : 600.0f, ms);
        add (envId (e, "s"), n + "Sustain",  { 0.0f, 100.0f }, e == 0 ? 100.0f : 0.0f, pct);
        add (envId (e, "r"), n + "Release",  timeRange, e == 0 ? 180.0f : 300.0f, ms);
        add (envId (e, "c"), n + "Curva",    { -100.0f, 100.0f }, 0.0f, spct);   // − = rápida no começo (exponencial)
    }

    // ---- LFOs ----
    for (int l = 0; l < numLfos; ++l)
    {
        const String n = "LFO " + String (l + 1) + " ";
        choice (lfoId (l, "shape"), n + "Shape", toArray (lfoShapeNames, numLfoShapes), 0);
        toggle (lfoId (l, "sync"),  n + "Sync", true);
        add (lfoId (l, "rate"),     n + "Rate", rateRange, 2.0f, [] (float v, int) { return num1 (v) + " Hz"; });
        choice (lfoId (l, "div"),   n + "Division", divs, 8);
        choice (lfoId (l, "mode"),  n + "Modo", toArray (lfoModeNames, 3), 1);
        add (lfoId (l, "rise"),     n + "Rise", { 0.0f, 5000.0f }, 0.0f, ms);
    }

    // ---- matriz ----
    for (int s = 0; s < numSlots; ++s)
    {
        const String n = "Mod " + String (s + 1).paddedLeft ('0', 2) + " ";
        choice (slotId (s, "src"), n + "Source", toArray (srcNames, numSrcs), 0);
        choice (slotId (s, "dst"), n + "Dest", toArray (dstNames, numDsts), 0);
        add (slotId (s, "amt"),    n + "Amount", { -100.0f, 100.0f }, 0.0f, spct);
    }
    for (int m = 0; m < numMacros; ++m)
        add (macroId (m), "Macro " + String (m + 1), { 0.0f, 100.0f }, 0.0f, pct);

    // ---- voz ----
    choice (id::polyMode, "Modo", toArray (polyNames, 3), 0);
    add (id::voices, "Voices", { 1.0f, (float) maxVoices, 1.0f }, 8.0f, [] (float v, int) { return String (roundToInt (v)); });
    add (id::glide, "Glide", glideRange, 0.0f, ms);
    add (id::bend, "Bend", { 1.0f, 24.0f, 1.0f }, 2.0f, semis);
    add (id::drift, "Analog Drift", { 0.0f, 100.0f }, 15.0f, pct);
    add (id::velAmp, "Velocity to Volume", { 0.0f, 100.0f }, 50.0f, pct);
    add (id::volume, "Volume", { -36.0f, 6.0f }, -6.0f, [] (float v, int) { return dbText (v); });

    toggle (id::mpe, "MPE", false);
    add (id::mpeBend, "MPE Bend", { 12.0f, 96.0f, 1.0f }, 48.0f, semis);
    toggle (id::hold, "Segurar", false);

    // ---- efeitos (desligados no Init) ----
    toggle (id::hypOn, "Hyper Ligado", false);
    add (id::hypRate, "Hyper Rate", { 0.0f, 100.0f }, 40.0f, pct);
    add (id::hypDetune, "Hyper Detune", { 0.0f, 100.0f }, 35.0f, pct);
    add (id::hypMix, "Hyper Dry/Wet", { 0.0f, 100.0f }, 50.0f, pct);

    toggle (id::dstOn, "Distortion Ligado", false);
    choice (id::dstType, "Distortion Tipo", toArray (distNames, 6), 0);
    add (id::dstDrive, "Distortion Drive", { 0.0f, 100.0f }, 30.0f, pct);
    add (id::dstTone, "Distortion Tone", { 0.0f, 100.0f }, 60.0f, pct);
    add (id::dstMix, "Distortion Dry/Wet", { 0.0f, 100.0f }, 100.0f, pct);

    toggle (id::phsOn, "Phaser Ligado", false);
    add (id::phsRate, "Phaser Rate", rateRange, 0.4f, [] (float v, int) { return num1 (v) + " Hz"; });
    add (id::phsDepth, "Phaser Depth", { 0.0f, 100.0f }, 60.0f, pct);
    add (id::phsFb, "Phaser Feedback", { 0.0f, 95.0f }, 40.0f, pct);
    add (id::phsMix, "Phaser Dry/Wet", { 0.0f, 100.0f }, 50.0f, pct);

    toggle (id::choOn, "Chorus Ligado", false);
    add (id::choRate, "Chorus Rate", rateRange, 0.6f, [] (float v, int) { return num1 (v) + " Hz"; });
    add (id::choDepth, "Chorus Depth", { 0.0f, 100.0f }, 40.0f, pct);
    add (id::choMix, "Chorus Dry/Wet", { 0.0f, 100.0f }, 40.0f, pct);

    toggle (id::dlyOn, "Delay Ligado", false);
    choice (id::dlyDiv, "Delay Division", divs, 7);
    add (id::dlyFb, "Delay Feedback", { 0.0f, 95.0f }, 35.0f, pct);
    toggle (id::dlyPing, "Delay Ping-Pong", true);
    add (id::dlyTone, "Delay Tone", { 0.0f, 100.0f }, 55.0f, pct);
    add (id::dlyMix, "Delay Dry/Wet", { 0.0f, 100.0f }, 25.0f, pct);

    toggle (id::revOn, "Reverb Ligado", false);
    choice (id::revType, "Reverb Tipo", toArray (reverbNames, 3), 1);
    add (id::revSize, "Reverb Size", { 0.0f, 100.0f }, 55.0f, pct);
    add (id::revDecay, "Reverb Decay", { 0.2f, 20.0f }, 2.5f, [] (float v, int) { return num1 (v) + " s"; });
    add (id::revDamp, "Reverb Damping", { 0.0f, 100.0f }, 40.0f, pct);
    add (id::revMix, "Reverb Dry/Wet", { 0.0f, 100.0f }, 25.0f, pct);

    toggle (id::eqOn, "EQ Ligado", false);
    add (id::eqLow, "EQ Low", { -18.0f, 18.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::eqMid, "EQ Mid", { -18.0f, 18.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::eqMidF, "EQ Mid Freq", midRange, 1000.0f, [] (float v, int) { return hzText (v); });
    add (id::eqHigh, "EQ High", { -18.0f, 18.0f }, 0.0f, [] (float v, int) { return dbText (v); });

    toggle (id::cmpOn, "Compressor Ligado", false);
    choice (id::cmpMode, "Compressor Modo", toArray (compNames, 2), 0);
    add (id::cmpAmt, "Compressor Amount", { 0.0f, 100.0f }, 40.0f, pct);
    add (id::cmpMix, "Compressor Dry/Wet", { 0.0f, 100.0f }, 100.0f, pct);

    // ---- arpejador (desligado no Init; padrão: 16 passos ligados, sem acento) ----
    toggle (id::arpOn, "Arp Ligado", false);
    choice (id::arpMode, "Arp Mode", toArray (arpModeNames, numArpModes), 0);
    choice (id::arpRate, "Arp Rate", divs, 2);
    add (id::arpOct, "Arp Octaves", { 1.0f, 4.0f, 1.0f }, 1.0f, [] (float v, int) { return String (roundToInt (v)) + " oct"; });
    add (id::arpGate, "Arp Gate", { 5.0f, 100.0f }, 60.0f, pct);
    add (id::arpSwing, "Arp Swing", { 0.0f, 100.0f }, 0.0f, pct);
    add (id::arpLen, "Arp Length", { 1.0f, (float) numArpSteps, 1.0f }, 16.0f, [] (float v, int) { return String (roundToInt (v)); });
    add (id::arpAcc, "Arp Accent", { 0.0f, 100.0f }, 50.0f, pct);
    choice (id::arpScale, "Arp Scale", scales::scaleNames, 0);   // CHROMATIC = sem escala
    choice (id::arpRoot, "Arp Root", scales::rootNames, 0);
    for (int s = 0; s < numArpSteps; ++s)
    {
        toggle (arpStepId (s), "Arp Step " + String (s + 1), true);
        toggle (arpAccId (s), "Arp Accent " + String (s + 1), false);
    }

    return layout;
}

// ---------------- a foto dos parâmetros a cada bloco ----------------
// Valores REAIS (mesma unidade do parâmetro: %, Hz, ms, st…); listas = índice; liga/desliga = bool.
struct OscSnap
{
    bool  on = false;
    int   table = 0, warp = 0, unison = 1, route = 0;
    float pos = 0, warpAmt = 0, oct = 0, semi = 0, fine = 0, level = 75, pan = 0, detune = 25, width = 70, rand = 100;
};
struct FilterSnap
{
    bool  on = false;
    int   type = 0;
    float cutoff = 6000, res = 15, drive = 0, key = 0, env = 0, mix = 100;
};
struct EnvSnap  { float a = 2, d = 300, s = 100, r = 180, curve = 0; };
struct LfoSnap  { int shape = 0, div = 8, mode = 1; bool sync = true; float rate = 2, rise = 0; };
struct SlotSnap { int src = 0, dst = 0; float amt = 0; };
struct ArpSnap
{
    bool  on = false;
    int   mode = 0, rate = 2, octaves = 1, length = 16, scale = 0, root = 0;
    float gate = 60, swing = 0, accent = 50;
    std::array<bool, numArpSteps> step {}, acc {};
};

struct Snapshot
{
    std::array<OscSnap, numOsc> osc;
    bool  subOn = false;  int subWave = 0, subOct = 0, subRoute = 0;  float subLevel = 50;
    bool  noiseOn = false; int noiseRoute = 0; float noiseColor = 50, noiseLevel = 30;
    std::array<FilterSnap, numFilters> filter;
    bool  filterSerial = false;
    std::array<EnvSnap, numEnvs> env;
    std::array<LfoSnap, numLfos> lfo;
    std::array<SlotSnap, numSlots> slot;
    std::array<float, numMacros> macro {};
    int   polyMode = 0, voices = 8;
    float glide = 0, bend = 2, drift = 15, velAmp = 50, volume = -6;
    bool  mpe = false, hold = false;
    float mpeBend = 48;
    ArpSnap arp;

    struct
    {
        bool  hypOn = false, dstOn = false, phsOn = false, choOn = false, dlyOn = false, dlyPing = true,
              revOn = false, eqOn = false, cmpOn = false;
        int   dstType = 0, dlyDiv = 7, revType = 1, cmpMode = 0;
        float hypRate = 40, hypDetune = 35, hypMix = 50,
              dstDrive = 30, dstTone = 60, dstMix = 100,
              phsRate = 0.4f, phsDepth = 60, phsFb = 40, phsMix = 50,
              choRate = 0.6f, choDepth = 40, choMix = 40,
              dlyFb = 35, dlyTone = 55, dlyMix = 25,
              revSize = 55, revDecay = 2.5f, revDamp = 40, revMix = 25,
              eqLow = 0, eqMid = 0, eqMidF = 1000, eqHigh = 0,
              cmpAmt = 40, cmpMix = 100;
    } fx;
};

// relógio do host para LFO e delay em sync
struct Clock
{
    double bpm = 120.0, ppq = 0.0;
    bool   playing = false;
};

// lê todos os parâmetros do APVTS (ponteiros guardados uma vez; ler é barato)
class SnapshotReader
{
public:
    explicit SnapshotReader (juce::AudioProcessorValueTreeState& s) : st (s) {}

    void read (Snapshot& out)
    {
        if (cache.empty()) build();
        size_t i = 0;
        auto f = [this, &i] { return cache[i++]->load(); };
        auto n = [&f] { return juce::roundToInt (f()); };
        auto b = [&f] { return f() > 0.5f; };

        for (auto& o : out.osc)
        {
            o.on = b(); o.table = n(); o.pos = f(); o.warp = n(); o.warpAmt = f(); o.oct = f(); o.semi = f(); o.fine = f();
            o.level = f(); o.pan = f(); o.unison = n(); o.detune = f(); o.width = f(); o.rand = f(); o.route = n();
        }
        out.subOn = b(); out.subWave = n(); out.subOct = n(); out.subLevel = f(); out.subRoute = n();
        out.noiseOn = b(); out.noiseColor = f(); out.noiseLevel = f(); out.noiseRoute = n();
        for (auto& fl : out.filter)
        {
            fl.on = b(); fl.type = n(); fl.cutoff = f(); fl.res = f(); fl.drive = f(); fl.key = f(); fl.env = f(); fl.mix = f();
        }
        out.filterSerial = b();
        for (auto& e : out.env) { e.a = f(); e.d = f(); e.s = f(); e.r = f(); e.curve = f(); }
        for (auto& l : out.lfo) { l.shape = n(); l.sync = b(); l.rate = f(); l.div = n(); l.mode = n(); l.rise = f(); }
        for (auto& s : out.slot) { s.src = n(); s.dst = n(); s.amt = f(); }
        for (auto& m : out.macro) m = f();
        out.polyMode = n(); out.voices = n(); out.glide = f(); out.bend = f(); out.drift = f(); out.velAmp = f(); out.volume = f();
        out.mpe = b(); out.mpeBend = f(); out.hold = b();
        auto& x = out.fx;
        x.hypOn = b(); x.hypRate = f(); x.hypDetune = f(); x.hypMix = f();
        x.dstOn = b(); x.dstType = n(); x.dstDrive = f(); x.dstTone = f(); x.dstMix = f();
        x.phsOn = b(); x.phsRate = f(); x.phsDepth = f(); x.phsFb = f(); x.phsMix = f();
        x.choOn = b(); x.choRate = f(); x.choDepth = f(); x.choMix = f();
        x.dlyOn = b(); x.dlyDiv = n(); x.dlyFb = f(); x.dlyPing = b(); x.dlyTone = f(); x.dlyMix = f();
        x.revOn = b(); x.revType = n(); x.revSize = f(); x.revDecay = f(); x.revDamp = f(); x.revMix = f();
        x.eqOn = b(); x.eqLow = f(); x.eqMid = f(); x.eqMidF = f(); x.eqHigh = f();
        x.cmpOn = b(); x.cmpMode = n(); x.cmpAmt = f(); x.cmpMix = f();
        auto& a = out.arp;
        a.on = b(); a.mode = n(); a.rate = n(); a.octaves = n(); a.gate = f(); a.swing = f(); a.length = n(); a.accent = f();
        a.scale = n(); a.root = n();
        for (int s = 0; s < numArpSteps; ++s) { a.step[(size_t) s] = b(); a.acc[(size_t) s] = b(); }
        jassert (i == cache.size());
    }

private:
    // mesma ordem do read() acima e do createLayout()
    void build()
    {
        auto p = [this] (const juce::String& pid)
        {
            auto* v = st.getRawParameterValue (pid);
            jassert (v != nullptr);
            cache.push_back (v);
        };
        for (int o = 0; o < numOsc; ++o)
            for (auto* fld : { "on", "tab", "pos", "warp", "wamt", "oct", "semi", "fine", "lvl", "pan", "uni", "det", "wid", "rand", "rt" })
                p (oscId (o, fld));
        for (auto* pid : { id::subOn, id::subWave, id::subOct, id::subLvl, id::subRt, id::noiseOn, id::noiseColor, id::noiseLvl, id::noiseRt })
            p (pid);
        for (int fl = 0; fl < numFilters; ++fl)
            for (auto* fld : { "on", "type", "cut", "res", "drv", "key", "env", "mix" })
                p (filterId (fl, fld));
        p (id::filterSerial);
        for (int e = 0; e < numEnvs; ++e)
            for (auto* fld : { "a", "d", "s", "r", "c" })
                p (envId (e, fld));
        for (int l = 0; l < numLfos; ++l)
            for (auto* fld : { "shape", "sync", "rate", "div", "mode", "rise" })
                p (lfoId (l, fld));
        for (int s = 0; s < numSlots; ++s)
            for (auto* fld : { "src", "dst", "amt" })
                p (slotId (s, fld));
        for (int m = 0; m < numMacros; ++m)
            p (macroId (m));
        for (auto* pid : { id::polyMode, id::voices, id::glide, id::bend, id::drift, id::velAmp, id::volume, id::mpe, id::mpeBend, id::hold,
                           id::hypOn, id::hypRate, id::hypDetune, id::hypMix,
                           id::dstOn, id::dstType, id::dstDrive, id::dstTone, id::dstMix,
                           id::phsOn, id::phsRate, id::phsDepth, id::phsFb, id::phsMix,
                           id::choOn, id::choRate, id::choDepth, id::choMix,
                           id::dlyOn, id::dlyDiv, id::dlyFb, id::dlyPing, id::dlyTone, id::dlyMix,
                           id::revOn, id::revType, id::revSize, id::revDecay, id::revDamp, id::revMix,
                           id::eqOn, id::eqLow, id::eqMid, id::eqMidF, id::eqHigh,
                           id::cmpOn, id::cmpMode, id::cmpAmt, id::cmpMix,
                           id::arpOn, id::arpMode, id::arpRate, id::arpOct, id::arpGate, id::arpSwing, id::arpLen, id::arpAcc,
                           id::arpScale, id::arpRoot })
            p (pid);
        for (int s = 0; s < numArpSteps; ++s)
        {
            p (arpStepId (s));
            p (arpAccId (s));
        }
    }

    juce::AudioProcessorValueTreeState& st;
    std::vector<std::atomic<float>*> cache;
};

// ---------------- presets de fábrica ----------------
struct FactoryPreset
{
    int category = -1;                                     // −1 = Init (fora das categorias)
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;    // valores reais; o que faltar volta ao padrão
};

// Presets.cpp: o Init primeiro e depois 40 por categoria (banco FACTORY 1), na ordem de categoryNames (ARP por último)
std::vector<FactoryPreset> factoryPresets();
} // namespace quimera
