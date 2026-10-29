#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

// GROOVADOR — percussão sintetizada (sem bumbo) com gerador de groove e variação de timbre.
// Aqui ficam os ids dos parâmetros, as listas, o padrão (a grade) que a tela escreve e o áudio lê.
namespace grv
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline constexpr int numInst  = 8;
inline constexpr int maxSteps = 96;      // 4 compassos de 24 (1/16T em 4/4)
inline constexpr int fillSteps = 12;     // o fill ocupa a 2ª metade do compasso

// ---------------- os instrumentos ----------------
enum Inst { rim = 0, snare, clap, hatC, hatO, shaker, perc1, perc2 };
inline const char* const instNames[numInst]  = { "RIM", "SNARE", "CLAP", "HAT FECH.", "HAT ABERTO", "SHAKER", "PERC 1", "PERC 2" };
inline const char* const instShort[numInst]  = { "RIM", "SNARE", "CLAP", "HAT F.", "HAT AB.", "SHAKER", "PERC 1", "PERC 2" };
inline const char* const instParam[numInst]  = { "Rim", "Snare", "Clap", "Closed Hat", "Open Hat", "Shaker", "Perc 1", "Perc 2" };
// PADS: teclas brancas a partir do Dó 1 (36) — C1 rim, D1 snare, E1 clap, F1 hat fechado, G1 hat aberto, A1 shaker, B1 perc 1, C2 perc 2
inline constexpr int padNotes[numInst] = { 36, 38, 40, 41, 43, 45, 47, 48 };
// altura base de cada um (TUNE 0)
inline constexpr float baseHz[numInst] = { 820.0f, 185.0f, 1000.0f, 330.0f, 330.0f, 400.0f, 320.0f, 190.0f };

// ---------------- listas (o parâmetro guarda o ÍNDICE) ----------------
inline const juce::StringArray modelNames { juce::String::fromUTF8 ("ANALÓG."), "808", "FM", "METAL", juce::String::fromUTF8 ("RUÍDO") };
enum Model { analog = 0, m808, fm, metal, noise };
inline const char* const modelLong[5] = { "ANALÓGICO", "808", "FM", "METAL", "RUÍDO" };
inline const char* const modelHint[5] = { "CORPO + RUÍDO", "SENO SATURADO", "PORTADORA + MODULADORA", "6 OSCILADORES", "TAPAS / GRÃO" };

inline const juce::StringArray styleNames { "PSY FULL", "PSY PROG", "TECHNO", "HOUSE", "TRIBAL", "BREAK" };
inline const juce::StringArray meterNames { "4/4", "3/4" };
inline const juce::StringArray barNames   { "1", "2", "4" };
inline constexpr int barCounts[] = { 1, 2, 4 };
inline const juce::StringArray rateNames  { "1/16", "1/16T", "1/8" };
inline constexpr double rateBeats[] = { 0.25, 1.0 / 6.0, 0.5 };
inline const juce::StringArray everyNames { juce::String::fromUTF8 ("½"), "1", "2", "4", "8" };
inline constexpr double everyBars[] = { 0.5, 1.0, 2.0, 4.0, 8.0 };
inline const juce::StringArray vmodeNames { juce::String::fromUTF8 ("A ↔ B"), "CONVERSA", "DERIVA" };
enum class VMode { ab = 0, talk = 1, drift = 2 };
inline const juce::StringArray fillNames  { juce::String::fromUTF8 ("—"), "4", "8" };
inline constexpr int fillEvery[] = { 0, 4, 8 };
inline const juce::StringArray playNames  { "COM PLAY", "PADS" };
inline const juce::StringArray driveNames { "TAPE", "TUBE", "FOLD" };
inline const juce::StringArray delayNames { "1/16", "1/8T", "1/8", "3/16", "1/4", "3/8" };
inline constexpr double delayBeats[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5 };

// a grade: passos por compasso e total (no máximo 96)
inline int stepsPerBar (int meterIdx, int rateIdx)
{
    const double beats = meterIdx == 1 ? 3.0 : 4.0;
    return juce::jmax (1, (int) std::round (beats / rateBeats[juce::jlimit (0, 2, rateIdx)]));
}
inline int patternSteps (int meterIdx, int barsIdx, int rateIdx)
{
    return juce::jlimit (1, maxSteps, stepsPerBar (meterIdx, rateIdx) * barCounts[juce::jlimit (0, 2, barsIdx)]);
}

namespace id
{
// som de cada instrumento: "i" + número + nome (ex.: "i1tune")
inline const juce::String model = "model", tune = "tune", dec = "dec", tone = "tone", snap = "snap", body = "body",
                          noise = "noise", pEnv = "penv", drive = "drive", pan = "pan", level = "level";
inline const juce::StringArray instParams { model, tune, dec, tone, snap, body, noise, pEnv, drive, pan, level };
inline juce::String i (int inst, const juce::String& name) { return "i" + juce::String (inst) + name; }
inline juce::String mute (int inst)  { return "mute" + juce::String (inst); }
inline juce::String solo (int inst)  { return "solo" + juce::String (inst); }
inline juce::String who (int inst)   { return "who" + juce::String (inst); }

// gerador e variação
inline const juce::String style = "style", meter = "meter", bars = "bars", rate = "rate", density = "density",
                          swing = "swing", ghost = "ghost", roll = "roll", human = "human",
                          every = "every", vmode = "vmode", vamount = "vamount", fill = "fill", play = "play";
// travas (performance: o preset não mexe)
inline const juce::String lockRhythm = "lkrhythm", lockTimbre = "lktimbre", lockFills = "lkfills", lockVel = "lkvel";
// efeitos
inline const juce::String drvType = "drvtype", fxDrive = "fxdrive", fxTone = "fxtone",
                          dlOn = "dlon", dlDiv = "dldiv", dlFb = "dlfb", dlMix = "dlmix",
                          rvOn = "rvon", rvSize = "rvsize", rvMix = "rvmix", volume = "volume";
}

// o que o preset não muda (performance): travas, mudo/solo e o modo de tocar
inline bool isPerformanceParam (const juce::String& pid)
{
    return pid.startsWith ("lk") || pid.startsWith ("mute") || pid.startsWith ("solo") || pid == id::play;
}

// som padrão de cada instrumento (Init)
struct InstDefaults { int model; float tune, dec, tone, snap, body, noise, pEnv, drive, pan, level; };
inline constexpr InstDefaults instDefaults[numInst] = {
    { analog,  0.0f,  45.0f, 3500.0f, 80.0f, 60.0f,  15.0f,  0.0f, 20.0f, -20.0f, -4.0f },   // RIM
    { analog,  0.0f, 180.0f, 4200.0f, 70.0f, 40.0f,  55.0f,  7.0f, 25.0f,   0.0f, -2.0f },   // SNARE
    { noise,   0.0f, 220.0f, 1400.0f, 60.0f, 75.0f, 100.0f,  0.0f, 15.0f,   5.0f, -2.0f },   // CLAP
    { metal,   0.0f,  60.0f, 8000.0f, 50.0f, 50.0f,  30.0f,  0.0f, 10.0f,  25.0f, -6.0f },   // HAT FECH.
    { metal,   0.0f, 380.0f, 7500.0f, 40.0f, 50.0f,  30.0f,  0.0f, 10.0f,  30.0f, -8.0f },   // HAT ABERTO
    { noise,   0.0f,  90.0f, 7000.0f, 30.0f, 10.0f, 100.0f,  0.0f,  0.0f, -30.0f, -9.0f },   // SHAKER
    { fm,      0.0f, 160.0f, 5000.0f, 50.0f, 40.0f,  10.0f, 12.0f, 10.0f, -35.0f, -5.0f },   // PERC 1
    { m808,    0.0f, 260.0f, 3000.0f, 40.0f, 50.0f,   5.0f,  5.0f, 15.0f,  35.0f, -5.0f },   // PERC 2
};

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto ms  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + "ms"; };
    auto hz  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + "k" : String (roundToInt (v)); };
    auto st  = [] (float v, int) { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i) + " st"; };
    auto db  = [] (float v, int) { return String (v, 1) + " dB"; };
    auto panTxt = [] (float v, int) { const int i = roundToInt (v); return i == 0 ? String ("C") : (i < 0 ? "L" : "R") + String (std::abs (i)); };
    auto add = [&p] (auto* prm) { p.emplace_back (prm); };

    NormalisableRange<float> decRange (10.0f, 2000.0f);
    decRange.setSkewForCentre (200.0f);
    NormalisableRange<float> toneRange (200.0f, 16000.0f);
    toneRange.setSkewForCentre (3000.0f);

    // ---- os 8 instrumentos ----
    for (int k = 0; k < numInst; ++k)
    {
        const auto& d = instDefaults[k];
        const String n = String (instParam[k]) + " ";
        add (new AudioParameterChoice ({ id::i (k, id::model), 1 }, n + "Model", modelNames, d.model));
        add (new AudioParameterFloat ({ id::i (k, id::tune), 1 }, n + "Tune", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), d.tune,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (st)));
        add (new AudioParameterFloat ({ id::i (k, id::dec), 1 }, n + "Decay", decRange, d.dec,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
        add (new AudioParameterFloat ({ id::i (k, id::tone), 1 }, n + "Tone", toneRange, d.tone,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
        add (new AudioParameterFloat ({ id::i (k, id::snap), 1 }, n + "Snap", NormalisableRange<float> (0.0f, 100.0f), d.snap,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        add (new AudioParameterFloat ({ id::i (k, id::body), 1 }, n + "Body", NormalisableRange<float> (0.0f, 100.0f), d.body,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        add (new AudioParameterFloat ({ id::i (k, id::noise), 1 }, n + "Noise", NormalisableRange<float> (0.0f, 100.0f), d.noise,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        add (new AudioParameterFloat ({ id::i (k, id::pEnv), 1 }, n + "Pitch Env", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), d.pEnv,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (st)));
        add (new AudioParameterFloat ({ id::i (k, id::drive), 1 }, n + "Drive", NormalisableRange<float> (0.0f, 100.0f), d.drive,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        add (new AudioParameterFloat ({ id::i (k, id::pan), 1 }, n + "Pan", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), d.pan,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (panTxt)));
        add (new AudioParameterFloat ({ id::i (k, id::level), 1 }, n + "Level", NormalisableRange<float> (-30.0f, 6.0f, 0.1f), d.level,
                                      AudioParameterFloatAttributes().withStringFromValueFunction (db)));
        add (new AudioParameterBool ({ id::mute (k), 1 }, n + "Mute", false));
        add (new AudioParameterBool ({ id::solo (k), 1 }, n + "Solo", false));
        // quem varia: por padrão a caixa, o hat fechado e as percussões
        add (new AudioParameterBool ({ id::who (k), 1 }, n + "Varies", k == snare || k == hatC || k == perc1 || k == perc2));
    }

    // ---- gerador de groove ----
    add (new AudioParameterChoice ({ id::style, 1 }, "Style", styleNames, 0));
    add (new AudioParameterChoice ({ id::meter, 1 }, "Meter", meterNames, 0));
    add (new AudioParameterChoice ({ id::bars, 1 }, "Bars", barNames, 1));
    add (new AudioParameterChoice ({ id::rate, 1 }, "Rate", rateNames, 0));
    add (new AudioParameterFloat ({ id::density, 1 }, "Density", NormalisableRange<float> (0.0f, 100.0f), 60.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::swing, 1 }, "Swing", NormalisableRange<float> (0.0f, 75.0f), 10.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::ghost, 1 }, "Ghost", NormalisableRange<float> (0.0f, 100.0f), 35.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::roll, 1 }, "Roll", NormalisableRange<float> (0.0f, 100.0f), 20.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::human, 1 }, "Human", NormalisableRange<float> (0.0f, 100.0f), 20.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    // ---- variação de timbre ----
    add (new AudioParameterChoice ({ id::every, 1 }, "Vary Every", everyNames, 1));
    add (new AudioParameterChoice ({ id::vmode, 1 }, "Vary Mode", vmodeNames, 1));
    add (new AudioParameterFloat ({ id::vamount, 1 }, "Vary Amount", NormalisableRange<float> (0.0f, 100.0f), 45.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterChoice ({ id::fill, 1 }, "Fill Every", fillNames, 2));
    add (new AudioParameterChoice ({ id::play, 1 }, "Play Mode", playNames, 0));

    add (new AudioParameterBool ({ id::lockRhythm, 1 }, "Lock Rhythm", false));
    add (new AudioParameterBool ({ id::lockTimbre, 1 }, "Lock Timbres", false));
    add (new AudioParameterBool ({ id::lockFills, 1 }, "Lock Fills", false));
    add (new AudioParameterBool ({ id::lockVel, 1 }, "Lock Velocity", false));

    // ---- efeitos ----
    add (new AudioParameterChoice ({ id::drvType, 1 }, "Drive Type", driveNames, 0));
    add (new AudioParameterFloat ({ id::fxDrive, 1 }, "Drive", NormalisableRange<float> (0.0f, 100.0f), 20.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::fxTone, 1 }, "Tone", NormalisableRange<float> (0.0f, 100.0f), 80.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterBool ({ id::dlOn, 1 }, "Delay On", false));
    add (new AudioParameterChoice ({ id::dlDiv, 1 }, "Delay Time", delayNames, 3));
    add (new AudioParameterFloat ({ id::dlFb, 1 }, "Delay Feedback", NormalisableRange<float> (0.0f, 95.0f), 40.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::dlMix, 1 }, "Delay Mix", NormalisableRange<float> (0.0f, 100.0f), 18.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterBool ({ id::rvOn, 1 }, "Reverb On", true));
    add (new AudioParameterFloat ({ id::rvSize, 1 }, "Reverb Size", NormalisableRange<float> (0.0f, 100.0f), 45.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::rvMix, 1 }, "Reverb Mix", NormalisableRange<float> (0.0f, 100.0f), 14.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (new AudioParameterFloat ({ id::volume, 1 }, "Volume", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -6.0f,
                                  AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    return { p.begin(), p.end() };
}

// ---------------- um passo da grade ----------------
// vel 1..7 (1–2 = ghost), roll 0..2 (0 = 1 batida, 1 = 2, 2 = 3 dentro do passo), prob 0..3 = 100/75/50/25%
struct Step
{
    bool on = false;
    int  vel = 5, roll = 0, prob = 0;

    int pack() const
    {
        return (on ? 1 : 0) | (juce::jlimit (1, 7, vel) << 1) | (juce::jlimit (0, 2, roll) << 4) | (juce::jlimit (0, 3, prob) << 6);
    }

    static Step unpack (int v)
    {
        Step s;
        s.on   = (v & 1) != 0;
        s.vel  = juce::jlimit (1, 7, (v >> 1) & 7);
        s.roll = juce::jlimit (0, 2, (v >> 4) & 3);
        s.prob = (v >> 6) & 3;
        return s;
    }

    bool  ghost() const    { return vel <= 2; }
    float gain() const     { return (float) vel / 7.0f; }
    float chance() const   { return 1.0f - 0.25f * (float) prob; }
};

// padrão compartilhado entre a tela (escreve) e o áudio (lê): um inteiro atômico por passo
struct Pattern
{
    std::array<std::array<std::atomic<int>, maxSteps>, numInst> hits;
    std::array<std::array<std::atomic<int>, fillSteps>, numInst> fill;
    std::atomic<int> varSeed { 1 };      // sorteia o timbre B de cada instrumento

    Pattern() { clear(); }

    Step get (int inst, int i) const     { return Step::unpack (hits[(size_t) inst][(size_t) juce::jlimit (0, maxSteps - 1, i)].load()); }
    void set (int inst, int i, Step s)   { hits[(size_t) inst][(size_t) juce::jlimit (0, maxSteps - 1, i)].store (s.pack()); }
    Step getFill (int inst, int i) const { return Step::unpack (fill[(size_t) inst][(size_t) juce::jlimit (0, fillSteps - 1, i)].load()); }
    void setFill (int inst, int i, Step s) { fill[(size_t) inst][(size_t) juce::jlimit (0, fillSteps - 1, i)].store (s.pack()); }

    void clear()
    {
        const int off = Step().pack();
        for (auto& row : hits) for (auto& s : row) s.store (off);
        for (auto& row : fill) for (auto& s : row) s.store (off);
    }

    // texto para salvar no projeto e nos presets do usuário: passos; fill; semente
    juce::String toString() const
    {
        juce::String out;
        for (auto& row : hits) for (auto& s : row) out << s.load() << ",";
        out << "|";
        for (auto& row : fill) for (auto& s : row) out << s.load() << ",";
        out << "|" << varSeed.load();
        return out;
    }

    void fromString (const juce::String& text)
    {
        clear();
        auto parts = juce::StringArray::fromTokens (text, "|", "");
        auto a = juce::StringArray::fromTokens (parts[0], ",", "");
        for (int k = 0; k < numInst; ++k)
            for (int i = 0; i < maxSteps; ++i)
            {
                const int n = k * maxSteps + i;
                if (n < a.size() && a[n].isNotEmpty()) hits[(size_t) k][(size_t) i].store (a[n].getIntValue());
            }
        auto f = juce::StringArray::fromTokens (parts[1], ",", "");
        for (int k = 0; k < numInst; ++k)
            for (int i = 0; i < fillSteps; ++i)
            {
                const int n = k * fillSteps + i;
                if (n < f.size() && f[n].isNotEmpty()) fill[(size_t) k][(size_t) i].store (f[n].getIntValue());
            }
        if (parts.size() > 2) varSeed.store (juce::jmax (1, parts[2].getIntValue()));
    }
};
} // namespace grv
