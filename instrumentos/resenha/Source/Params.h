#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Scales.h"

// RESENHA — call & response FM: 4 timbres (A, B, C, D) que se revezam numa frase gerada na escala.
// Aqui ficam os ids dos parâmetros, as listas e o padrão (a frase) que a tela escreve e o áudio lê.
namespace resenha
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline constexpr int maxSteps  = 64;
inline constexpr int numVoices = 4;

// ---------------- listas (o parâmetro guarda o ÍNDICE) ----------------
inline const juce::StringArray waveNames  { "SINE", "TRI", "SAW", "SQUARE" };
inline const juce::StringArray ratioNames { "0.50", "1.00", "1.50", "2.00", "2.50", "3.00", "3.50", "4.00", "5.00",
                                            "6.00", "7.00", "8.00", "9.00", "11.0", "13.0" };
inline constexpr float ratios[] = { 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 11.0f, 13.0f };
inline const juce::StringArray barNames   { "1", "2", "4" };
inline constexpr int barCounts[] = { 1, 2, 4 };
inline const juce::StringArray rateNames  { "1/32", "1/16T", "1/16", "1/8T", "1/8" };
inline constexpr double rateBeats[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5 };
inline const juce::StringArray voicesNames { "1", "2", "3", "4" };
inline const juce::StringArray everyNames { "1", "2", "3", "4", "6", "8" };
inline constexpr int everyCounts[] = { 1, 2, 3, 4, 6, 8 };
inline const juce::StringArray orderNames { juce::String::fromUTF8 ("A→B→C"), "PING-PONG", "SORTEIO" };
enum class Order { sequence = 0, pingPong = 1, random = 2 };
inline const juce::StringArray responseNames { "LIVRE", "ECO", "VARIA", "INVERTE" };
enum class Response { free = 0, echo = 1, vary = 2, invert = 3 };
inline const juce::StringArray intervalNames { "MESMA", "+OITAVA", juce::String::fromUTF8 ("+5ª"), juce::String::fromUTF8 ("−OITAVA") };
inline const juce::StringArray playNames  { "COM NOTA", "SEMPRE" };
// CONTORNO do gerador de notas: MOTIVO repete um desenho curto; CORRIDA sobe em escada (arpejo de psy) e
// recomeça embaixo; QUEDA desce; ONDA sobe e desce
inline const juce::StringArray contourNames { "MOTIVO", "CORRIDA", "QUEDA", "ONDA" };
enum class Contour { motif = 0, run = 1, fall = 2, wave = 3 };
inline const juce::StringArray driveNames { "TAPE", "TUBE", "FOLD" };
inline const juce::StringArray delayNames { "1/16", "1/8T", "1/8", "3/16", "1/4", "3/8" };
inline constexpr double delayBeats[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5 };

// quantos passos tem a frase (no máximo 64)
inline int phraseSteps (int barsIdx, int rateIdx)
{
    const int bars = barCounts[juce::jlimit (0, 2, barsIdx)];
    const double rb = rateBeats[juce::jlimit (0, 4, rateIdx)];
    return juce::jlimit (1, maxSteps, (int) std::round ((double) bars * 4.0 / rb));
}

namespace id
{
// timbre de cada voz: prefixo a/b/c/d + nome (ex.: "afm", "bcut")
inline const juce::String wave = "wave", ratio = "ratio", fm = "fm", fb = "fb", fmDec = "fmdec",
                          cut = "cut", res = "res", dec = "dec", pan = "pan",
                          glide = "glide", pEnv = "penv", fEnv = "fenv", bend = "bend";
inline const juce::StringArray voiceParams { wave, ratio, fm, fb, fmDec, cut, res, dec, pan, glide, pEnv, fEnv, bend };
inline juce::String v (int voice, const juce::String& name) { return juce::String::charToString ((juce::juce_wchar) ('a' + voice)) + name; }

// gerador e troca de timbre
inline const juce::String scale = "scale", root = "root", bars = "bars", rate = "rate", octaves = "octaves",
                          density = "density", gate = "gate", gateVar = "gatevar", swing = "swing",
                          voices = "voices", every = "every", order = "order", response = "response",
                          interval = "interval", play = "play", contour = "contour";
// travas (performance: o preset não mexe)
inline const juce::String lockNotes = "lknotes", lockRhythm = "lkrhythm", lockGate = "lkgate";
inline juce::String lockVoice (int voice) { return "lk" + juce::String::charToString ((juce::juce_wchar) ('a' + voice)); }
// efeitos
inline const juce::String drvType = "drvtype", drive = "drive", tone = "tone",
                          dlOn = "dlon", dlDiv = "dldiv", dlFb = "dlfb", dlMix = "dlmix",
                          rvOn = "rvon", rvSize = "rvsize", rvMix = "rvmix", duck = "duck", volume = "volume";
}

// o que o preset não muda (performance): travas e o modo de tocar
inline bool isPerformanceParam (const juce::String& pid)
{
    return pid.startsWith ("lk") || pid == id::play;
}

// timbre padrão de cada voz (Init): A seno brilhante, B serra, C quadrada, D triângulo.
// Lead de full on/prog (referência: Magik & Alpha Portal — Magic Portal, 144 BPM): A pergunta seca, com um "zap"
// de pitch no ataque; B responde com glide e o filtro abrindo no ataque (o "squelch"); C e D são variações.
// GLIDE = ms deslizando da nota anterior DO MESMO TIMBRE; P.ENV = semitons no ataque (cai no tempo do FM DEC);
// F.ENV = quanto o filtro abre no ataque (também no tempo do FM DEC).
// BEND = semitons que a ÚLTIMA nota de cada fala do timbre desliza durante a nota (o mergulho no fim da frase,
// como nos leads de full on); 0 = reta. Padrão 0: os presets antigos tocam igual.
struct VoiceDefaults { int wave, ratio; float fm, fb, fmDec, cut, res, dec, pan, glide, pEnv, fEnv, bend; };
inline constexpr VoiceDefaults voiceDefaults[numVoices] = {
    { 0, 3, 55.0f, 15.0f, 180.0f, 6000.0f, 20.0f, 260.0f, -30.0f,  0.0f, 12.0f, 20.0f, 0.0f },
    { 2, 6, 65.0f, 35.0f,  90.0f, 3200.0f, 40.0f, 320.0f,  30.0f, 60.0f,  0.0f, 55.0f, 0.0f },
    { 3, 1, 45.0f, 50.0f, 260.0f, 2400.0f, 25.0f, 400.0f, -10.0f,  0.0f, -7.0f, 35.0f, 0.0f },
    { 1, 10, 35.0f, 20.0f, 150.0f, 5000.0f, 35.0f, 300.0f, 10.0f, 30.0f,  5.0f, 10.0f, 0.0f },
};

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto ms  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; };
    auto hz  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + "k" : String (roundToInt (v)); };
    auto panTxt = [] (float v, int) { const int i = roundToInt (v); return i == 0 ? String ("C") : (i < 0 ? "L" : "R") + String (std::abs (i)); };
    auto add = [&p] (auto* prm) { p.emplace_back (prm); };

    NormalisableRange<float> fmDecRange (5.0f, 2000.0f);
    fmDecRange.setSkewForCentre (180.0f);
    NormalisableRange<float> cutRange (150.0f, 18000.0f);
    cutRange.setSkewForCentre (2500.0f);
    NormalisableRange<float> decRange (30.0f, 3000.0f);
    decRange.setSkewForCentre (400.0f);
    NormalisableRange<float> glideRange (0.0f, 500.0f);
    glideRange.setSkewForCentre (80.0f);

    // ---- os 4 timbres ----
    const char* const vn[numVoices] = { "A", "B", "C", "D" };
    for (int v = 0; v < numVoices; ++v)
    {
        const auto& d = voiceDefaults[v];
        const String n = String ("Voice ") + vn[v] + " ";
        add (new AudioParameterChoice ({ id::v (v, id::wave), 1 }, n + "Wave", waveNames, d.wave));
        add (new AudioParameterChoice ({ id::v (v, id::ratio), 1 }, n + "Ratio", ratioNames, d.ratio));
        add (new AudioParameterFloat ({ id::v (v, id::fm), 1 }, n + "FM", NormalisableRange<float> (0.0f, 100.0f), d.fm,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        add (new AudioParameterFloat ({ id::v (v, id::fb), 1 }, n + "Feedback", NormalisableRange<float> (0.0f, 100.0f), d.fb,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        add (new AudioParameterFloat ({ id::v (v, id::fmDec), 1 }, n + "FM Decay", fmDecRange, d.fmDec,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
        add (new AudioParameterFloat ({ id::v (v, id::cut), 1 }, n + "Cutoff", cutRange, d.cut,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
        add (new AudioParameterFloat ({ id::v (v, id::res), 1 }, n + "Resonance", NormalisableRange<float> (0.0f, 100.0f), d.res,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        add (new AudioParameterFloat ({ id::v (v, id::dec), 1 }, n + "Decay", decRange, d.dec,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
        add (new AudioParameterFloat ({ id::v (v, id::pan), 1 }, n + "Pan", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), d.pan,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (panTxt)));
        add (new AudioParameterFloat ({ id::v (v, id::glide), 1 }, n + "Glide", glideRange, d.glide,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
        add (new AudioParameterFloat ({ id::v (v, id::pEnv), 1 }, n + "Pitch Env", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), d.pEnv,
                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
                                      { const int i = roundToInt (x); return (i > 0 ? "+" : "") + String (i) + " st"; })));
        add (new AudioParameterFloat ({ id::v (v, id::fEnv), 1 }, n + "Filter Env", NormalisableRange<float> (0.0f, 100.0f), d.fEnv,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        add (new AudioParameterFloat ({ id::v (v, id::bend), 1 }, n + "End Bend", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), d.bend,
                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
                                      { const int i = roundToInt (x); return (i > 0 ? "+" : "") + String (i) + " st"; })));
    }

    // ---- gerador de frase ----
    add (new AudioParameterChoice ({ id::scale, 1 }, "Scale", scales::scaleNames, 4));     // PHRYGIAN
    add (new AudioParameterChoice ({ id::root, 1 }, "Root", scales::rootNames, 1));        // C#
    add (new AudioParameterChoice ({ id::bars, 1 }, "Bars", barNames, 1));
    add (new AudioParameterChoice ({ id::rate, 1 }, "Rate", rateNames, 2));
    add (new AudioParameterInt ({ id::octaves, 1 }, "Octaves", 1, 3, 2));
    add (new AudioParameterFloat ({ id::density, 1 }, "Density", NormalisableRange<float> (0.0f, 100.0f), 70.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::gate, 1 }, "Gate", NormalisableRange<float> (5.0f, 100.0f), 45.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::gateVar, 1 }, "Gate Variation", NormalisableRange<float> (0.0f, 100.0f), 30.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::swing, 1 }, "Swing", NormalisableRange<float> (0.0f, 75.0f), 0.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterChoice ({ id::contour, 1 }, "Contour", contourNames, 0));

    // ---- troca de timbre ----
    add (new AudioParameterChoice ({ id::voices, 1 }, "Voices", voicesNames, 2));
    add (new AudioParameterChoice ({ id::every, 1 }, "Switch Every", everyNames, 3));
    add (new AudioParameterChoice ({ id::order, 1 }, "Order", orderNames, 0));
    add (new AudioParameterChoice ({ id::response, 1 }, "Response", responseNames, 1));
    add (new AudioParameterChoice ({ id::interval, 1 }, "Response Pitch", intervalNames, 1));
    add (new AudioParameterChoice ({ id::play, 1 }, "Play Mode", playNames, 0));

    add (new AudioParameterBool ({ id::lockNotes, 1 }, "Lock Notes", false));
    add (new AudioParameterBool ({ id::lockRhythm, 1 }, "Lock Rhythm", false));
    add (new AudioParameterBool ({ id::lockGate, 1 }, "Lock Gate", false));
    for (int v = 0; v < numVoices; ++v)
        add (new AudioParameterBool ({ id::lockVoice (v), 1 }, String ("Lock Voice ") + vn[v], false));

    // ---- efeitos ----
    add (new AudioParameterChoice ({ id::drvType, 1 }, "Drive Type", driveNames, 0));
    add (new AudioParameterFloat ({ id::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 100.0f), 20.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::tone, 1 }, "Tone", NormalisableRange<float> (0.0f, 100.0f), 70.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterBool ({ id::dlOn, 1 }, "Delay On", true));
    add (new AudioParameterChoice ({ id::dlDiv, 1 }, "Delay Time", delayNames, 3));
    add (new AudioParameterFloat ({ id::dlFb, 1 }, "Delay Feedback", NormalisableRange<float> (0.0f, 95.0f), 45.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::dlMix, 1 }, "Delay Mix", NormalisableRange<float> (0.0f, 100.0f), 25.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterBool ({ id::rvOn, 1 }, "Reverb On", true));
    add (new AudioParameterFloat ({ id::rvSize, 1 }, "Reverb Size", NormalisableRange<float> (0.0f, 100.0f), 55.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::rvMix, 1 }, "Reverb Mix", NormalisableRange<float> (0.0f, 100.0f), 18.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::duck, 1 }, "Duck", NormalisableRange<float> (0.0f, 100.0f), 0.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::volume, 1 }, "Volume", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -6.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                  { return String (v, 1) + " dB"; })));

    return { p.begin(), p.end() };
}

// ---------------- um passo da frase ----------------
// deg = grau da escala a partir da tônica (com CHROMATIC, semitons), de −14 a +21; o grau fica guardado mesmo na
// pausa (trocar só o ritmo não perde a melodia). gate 0..15 = (gate+1)/16 do passo; var 0..15 = sorteio do VARIA.
enum class Mode { rest = 0, note = 1, tie = 2 };

struct Step
{
    int  deg    = 0;
    Mode mode   = Mode::note;
    int  gate   = 7;
    bool accent = false;
    int  var    = 0;

    int pack() const
    {
        return (juce::jlimit (-14, 21, deg) + 14) | ((int) mode << 6) | (juce::jlimit (0, 15, gate) << 8)
             | (accent ? 1 << 12 : 0) | (juce::jlimit (0, 15, var) << 13);
    }

    static Step unpack (int v)
    {
        Step s;
        s.deg    = (v & 63) - 14;
        s.mode   = (Mode) juce::jlimit (0, 2, (v >> 6) & 3);
        s.gate   = (v >> 8) & 15;
        s.accent = (v >> 12) & 1;
        s.var    = (v >> 13) & 15;
        return s;
    }
};

// padrão compartilhado entre a tela (escreve) e o áudio (lê): um inteiro atômico por passo
struct Pattern
{
    std::array<std::atomic<int>, maxSteps> steps;

    Pattern() { clear(); }

    Step get (int i) const      { return Step::unpack (steps[(size_t) juce::jlimit (0, maxSteps - 1, i)].load()); }
    void set (int i, Step s)    { steps[(size_t) juce::jlimit (0, maxSteps - 1, i)].store (s.pack()); }

    void clear()
    {
        Step s;
        s.mode = Mode::rest;
        for (auto& st : steps) st.store (s.pack());
    }

    void copyFrom (const Pattern& o)
    {
        for (int i = 0; i < maxSteps; ++i) steps[(size_t) i].store (o.steps[(size_t) i].load());
    }

    // texto para salvar no projeto e nos presets do usuário: o inteiro de cada passo
    juce::String toString() const
    {
        juce::String out;
        for (int i = 0; i < maxSteps; ++i) out << steps[(size_t) i].load() << ";";
        return out;
    }

    void fromString (const juce::String& text)
    {
        clear();
        auto items = juce::StringArray::fromTokens (text, ";", "");
        for (int i = 0; i < juce::jmin (maxSteps, items.size()); ++i)
            if (items[i].isNotEmpty())
                set (i, Step::unpack (items[i].getIntValue()));
    }
};

inline juce::String noteName (int midi)
{
    static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 2);   // Dó 60 = C3 (como o Ableton)
}
} // namespace resenha
