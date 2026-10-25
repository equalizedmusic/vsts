#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// VACINA — sintetizador FM de 4 operadores, polifônico (16 vozes), leve no processador.
// Aqui ficam os ids dos parâmetros e as listas (o parâmetro de escolha guarda o ÍNDICE).
namespace vacina
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline constexpr int maxVoices = 16;

// ---------------- listas ----------------
inline constexpr int numOps = 4, numAlgos = 8;
// formas de onda dos operadores (no estilo das FM de 4 operadores dos anos 80: TX81Z, OPL)
inline const juce::StringArray opWaveNames  { "SINE", "HALF SINE", "ABS SINE", "PULSE SINE", "ALT SINE", "CAMEL", "SQUARE", "SAW" };
// algoritmos: quem modula quem (o OP 4 tem a realimentação). O áudio sai só dos portadores.
inline const juce::StringArray algoNames    { "1  STACK", "2  Y", "3  BRANCH", "4  FORK", "5  TWIN", "6  FAN", "7  TRIPLE", "8  ORGAN" };
struct AlgoDef
{
    unsigned char mods[numOps];   // por operador (0 = OP 1): bits dos operadores que o modulam
    unsigned char carriers;       // bits dos portadores
};
inline constexpr AlgoDef algos[numAlgos] = {
    { { 0x2, 0x4, 0x8, 0x0 }, 0x1 },   // 1  4 > 3 > 2 > 1
    { { 0x2, 0xc, 0x0, 0x0 }, 0x1 },   // 2  (3 + 4) > 2 > 1
    { { 0xa, 0x4, 0x0, 0x0 }, 0x1 },   // 3  3 > 2 > 1  e  4 > 1
    { { 0x6, 0x0, 0x8, 0x0 }, 0x1 },   // 4  4 > 3 > 1  e  2 > 1
    { { 0x2, 0x0, 0x8, 0x0 }, 0x5 },   // 5  2 > 1  e  4 > 3
    { { 0x8, 0x8, 0x8, 0x0 }, 0x7 },   // 6  4 > 1, 2 e 3
    { { 0x0, 0x0, 0x8, 0x0 }, 0x7 },   // 7  4 > 3, 2 e 1 soltos
    { { 0x0, 0x0, 0x0, 0x0 }, 0xf },   // 8  os quatro soltos (órgão)
};
// razão do operador: COARSE 0 = ×0,5; 1..16 = ×1..×16; o FINE soma até +100 % (é ele que faz as razões inarmônicas)
inline float opRatio (float coarse, float finePct)
{
    const int c = juce::roundToInt (coarse);
    return (c <= 0 ? 0.5f : (float) c) * (1.0f + finePct * 0.01f);
}
inline const juce::StringArray keyModeNames { "POLY", "MONO", "LEGATO" };
inline const juce::StringArray f1ModeNames  { "LP 24", "LP 12", "HP", "BP", "BS", "COMB", "VOWEL" };
inline const juce::StringArray f2ModeNames  { "LP", "HP", "BP", "BS", "COMB", "VOWEL" };
inline const juce::StringArray routingNames { "SERIAL", "PARALLEL" };
inline const juce::StringArray satNames     { "OFF", "LIGHT", "TUBE", "HARD", "FOLD", "BITS", "RATE" };
inline const juce::StringArray lfoShapeNames { "SINE", "TRI", "SAW", "SQUARE", "S&H", "S&G" };
inline const juce::StringArray lfoSyncNames { "OFF", "1/16", "1/8T", "1/8", "1/4", "1/2", "1 BAR", "2 BARS", "4 BARS" };
inline constexpr double lfoSyncBeats[] = { 0.0, 0.25, 1.0 / 3.0, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
inline const juce::StringArray arpModeNames { "UP", "DOWN", "UP/DOWN", "RANDOM", "AS PLAYED", "CHORD" };
inline const juce::StringArray arpRateNames { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4" };
inline constexpr double arpRateBeats[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 1.0 };
inline const juce::StringArray delayNames   { "1/16", "1/8T", "1/8", "3/16", "1/4", "3/8", "1/2" };
inline constexpr double delayBeats[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };
inline const juce::StringArray distNames    { "SOFT", "HARD", "FUZZ", "RECT" };

// matriz de modulação: fontes e destinos
enum Src { srcOff, srcWheel, srcAfter, srcVel, srcKey, srcLfo1, srcLfo2, srcLfo3, srcFEnv, srcAEnv, srcRandom, numSrcs };
inline const juce::StringArray srcNames { "OFF", "MOD WHEEL", "AFTERTOUCH", "VELOCITY", "KEY", "LFO 1", "LFO 2", "LFO 3",
                                          "FILT ENV", "AMP ENV", "RANDOM" };
enum Dst { dstOff, dstCutoff, dstCutoff2, dstReso, dstPitch, dstIndex, dstOp1, dstOp2, dstOp3, dstOp4, dstRatio2, dstRatio3,
           dstRatio4, dstFeedback, dstFold, dstRing, dstNoise, dstSat, dstFiltBal, dstPan, dstVolume, numDsts };
inline const juce::StringArray dstNames { "OFF", "CUTOFF", "CUTOFF 2", "RESONANCE", "PITCH", "FM INDEX", "OP1 LEVEL", "OP2 LEVEL",
                                          "OP3 LEVEL", "OP4 LEVEL", "OP2 RATIO", "OP3 RATIO", "OP4 RATIO", "FEEDBACK", "FOLD",
                                          "RING MOD", "NOISE LEVEL", "SATURATION", "FILT BALANCE", "PAN", "VOLUME" };
inline constexpr int numSlots = 6;

namespace id
{
inline const juce::String
    volume = "volume", keyMode = "keymode", porta = "porta", bendRange = "bendrange", drift = "drift",
    unison = "unison", uniDetune = "unidetune", uniSpread = "unispread",
    algo = "algo", feedback = "feedback", fold = "fold", ring = "ring", noiseVol = "noisevol", oscVol = "oscvol",
    cutoff = "cutoff", cutoff2 = "cutoff2", filtBal = "filtbal", routing = "routing",
    f1Mode = "f1mode", f2Mode = "f2mode", reso1 = "reso1", reso2 = "reso2", env1 = "env1", env2 = "env2",
    key1 = "key1", key2 = "key2", satType = "sattype", satAmt = "satamt",
    fA = "fa", fD = "fd", fS = "fs", fR = "fr", fVel = "fvel", fSlope = "fslope",
    aA = "aa", aD = "ad", aS = "as", aR = "ar", aVel = "avel", aSlope = "aslope",
    lfo1Rate = "l1rate", lfo1Shape = "l1shape", lfo1Sync = "l1sync",
    lfo1Pitch = "l1pitch", lfo1Cut = "l1cut", lfo1Ratio = "l1ratio", lfo1Amp = "l1amp",
    lfo2Rate = "l2rate", lfo2Shape = "l2shape", lfo2Sync = "l2sync",
    lfo2Index = "l2index", lfo2Fb = "l2fb", lfo2Cut = "l2cut", lfo2Pan = "l2pan",
    lfo3Rate = "l3rate", lfo3Shape = "l3shape", lfo3Sync = "l3sync", lfo3Dst = "l3dst", lfo3Amt = "l3amt",
    arpOn = "arpon", arpMode = "arpmode", arpOct = "arpoct", arpRate = "arprate", arpGate = "arpgate", arpHold = "arphold",
    dlySend = "dlysend", dlyTime = "dlytime", dlyFb = "dlyfb",
    rvbSend = "rvbsend", rvbSize = "rvbsize", rvbDamp = "rvbdamp",
    eqLow = "eqlow", eqMid = "eqmid", eqHigh = "eqhigh",
    distType = "disttype", distAmt = "distamt", boostAmt = "boostamt", boostTune = "boosttune",
    choMix = "chomix", choInt = "choint", phaMix = "phamix", phaInt = "phaint";

// operadores: op1wave, op1coarse … op4r
inline juce::String op (int k, const char* what) { return "op" + juce::String (k + 1) + what; }
inline juce::String mSrc (int slot) { return "m" + juce::String (slot + 1) + "src"; }
inline juce::String mDst (int slot) { return "m" + juce::String (slot + 1) + "dst"; }
inline juce::String mAmt (int slot) { return "m" + juce::String (slot + 1) + "amt"; }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct  = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto bip  = [] (float v, int) { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i); };
    auto ms   = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; };
    auto hz   = [] (float v, int)
    {
        if (v >= 1000.0f) return String (v / 1000.0f, 2) + " kHz";
        return (v < 10.0f ? String (v, 2) : String (roundToInt (v))) + " Hz";
    };
    auto st   = [] (float v, int) { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i) + " st"; };
    auto ct   = [] (float v, int) { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i) + " ct"; };
    auto db   = [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; };
    auto add  = [&p] (auto* prm) { p.emplace_back (prm); };
    auto fl   = [&add] (const String& pid, const String& name, NormalisableRange<float> r, float def,
                        std::function<String (float, int)> txt)
    {
        add (new AudioParameterFloat ({ pid, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (txt)));
    };
    auto ch   = [&add] (const String& pid, const String& name, const StringArray& list, int def)
    {
        add (new AudioParameterChoice ({ pid, 1 }, name, list, def));
    };
    auto range = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };
    const NormalisableRange<float> unit (0.0f, 100.0f), bipolar (-100.0f, 100.0f);

    // ---- geral ----
    fl (id::volume, "Master Volume", NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -6.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    ch (id::keyMode, "Key Mode", keyModeNames, 0);
    fl (id::porta, "Portamento", range (0.0f, 2000.0f, 200.0f), 0.0f, ms);
    add (new AudioParameterInt ({ id::bendRange, 1 }, "Bend Range", 1, 24, 2));
    fl (id::drift, "Drift", unit, 0.0f, pct);
    add (new AudioParameterInt ({ id::unison, 1 }, "Unison", 1, 4, 1));
    fl (id::uniDetune, "Unison Detune", unit, 25.0f, pct);
    fl (id::uniSpread, "Unison Spread", unit, 60.0f, pct);

    // ---- FM ----
    ch (id::algo, "Algorithm", algoNames, 0);
    fl (id::feedback, "Feedback", unit, 0.0f, pct);
    const auto opAtk = range (0.5f, 10000.0f, 300.0f), opDec = range (1.0f, 20000.0f, 800.0f), opRel = range (1.0f, 10000.0f, 600.0f);
    for (int k = 0; k < numOps; ++k)
    {
        const auto n = "Op" + String (k + 1) + " ";
        ch (id::op (k, "wave"), n + "Wave", opWaveNames, 0);
        fl (id::op (k, "coarse"), n + "Coarse", NormalisableRange<float> (0.0f, 16.0f, 1.0f), 1.0f, [] (float v, int)
            { const int c = roundToInt (v); return c <= 0 ? String ("x0.5") : "x" + String (c); });
        fl (id::op (k, "fine"), n + "Fine", unit, 0.0f, [] (float v, int) { return "+" + String (v, 1) + "%"; });
        fl (id::op (k, "det"), n + "Detune", NormalisableRange<float> (-50.0f, 50.0f), 0.0f, ct);
        fl (id::op (k, "lvl"), n + "Level", unit, k == 0 ? 100.0f : 0.0f, pct);
        fl (id::op (k, "vel"), n + "Velocity", unit, 0.0f, pct);
        fl (id::op (k, "a"), n + "Attack", opAtk, 1.0f, ms);
        fl (id::op (k, "d"), n + "Decay", opDec, 800.0f, ms);
        fl (id::op (k, "s"), n + "Sustain", unit, 100.0f, pct);
        fl (id::op (k, "r"), n + "Release", opRel, 300.0f, ms);
    }

    // ---- mix ----
    fl (id::fold, "Fold", unit, 0.0f, pct);
    fl (id::ring, "Ring Mod", unit, 0.0f, pct);
    fl (id::noiseVol, "Noise Level", unit, 0.0f, pct);
    fl (id::oscVol, "Drive", unit, 50.0f, [] (float v, int)
        { const int i = roundToInt (v); return i <= 50 ? String (i * 2) + "%" : "SAT " + String ((i - 50) * 2) + "%"; });

    // ---- filtros ----
    fl (id::cutoff, "Cutoff", range (20.0f, 20000.0f, 900.0f), 20000.0f, hz);
    fl (id::cutoff2, "Cutoff 2", NormalisableRange<float> (-48.0f, 48.0f, 1.0f), 0.0f, st);
    fl (id::filtBal, "Filter Balance", unit, 0.0f, [] (float v, int)
        { const int i = roundToInt (v); return i == 0 ? String ("FILT 1") : i == 100 ? String ("FILT 2") : String (100 - i) + " / " + String (i); });
    ch (id::routing, "Filter Routing", routingNames, 0);
    ch (id::f1Mode, "Filter1 Mode", f1ModeNames, 0);
    ch (id::f2Mode, "Filter2 Mode", f2ModeNames, 0);
    fl (id::reso1, "Filter1 Resonance", unit, 0.0f, pct);
    fl (id::reso2, "Filter2 Resonance", unit, 20.0f, pct);
    fl (id::env1, "Filter1 Env Amount", bipolar, 0.0f, bip);
    fl (id::env2, "Filter2 Env Amount", bipolar, 0.0f, bip);
    fl (id::key1, "Filter1 Key Follow", unit, 0.0f, pct);
    fl (id::key2, "Filter2 Key Follow", unit, 0.0f, pct);
    ch (id::satType, "Saturation Type", satNames, 0);
    fl (id::satAmt, "Saturation", unit, 30.0f, pct);

    // ---- envelopes ----
    const auto atk = range (0.5f, 10000.0f, 300.0f), dec = range (1.0f, 10000.0f, 500.0f), rel = range (1.0f, 10000.0f, 600.0f);
    fl (id::fA, "Filter Attack", atk, 2.0f, ms);
    fl (id::fD, "Filter Decay", dec, 450.0f, ms);
    fl (id::fS, "Filter Sustain", unit, 30.0f, pct);
    fl (id::fR, "Filter Release", rel, 350.0f, ms);
    fl (id::fVel, "Filter Velocity", unit, 30.0f, pct);
    fl (id::fSlope, "Filter Sustain Slope", bipolar, 0.0f, bip);
    fl (id::aA, "Amp Attack", atk, 2.0f, ms);
    fl (id::aD, "Amp Decay", dec, 600.0f, ms);
    fl (id::aS, "Amp Sustain", unit, 100.0f, pct);
    fl (id::aR, "Amp Release", rel, 300.0f, ms);
    fl (id::aVel, "Amp Velocity", unit, 40.0f, pct);
    fl (id::aSlope, "Amp Sustain Slope", bipolar, 0.0f, bip);

    // ---- LFOs ----
    const auto lfoRate = range (0.02f, 30.0f, 3.0f);
    fl (id::lfo1Rate, "LFO1 Rate", lfoRate, 5.0f, hz);
    ch (id::lfo1Shape, "LFO1 Shape", lfoShapeNames, 0);
    ch (id::lfo1Sync, "LFO1 Clock", lfoSyncNames, 0);
    fl (id::lfo1Pitch, "LFO1 > Pitch", bipolar, 0.0f, bip);
    fl (id::lfo1Cut, "LFO1 > Cutoff", bipolar, 0.0f, bip);
    fl (id::lfo1Ratio, "LFO1 > Ratio", bipolar, 0.0f, bip);
    fl (id::lfo1Amp, "LFO1 > Amp", unit, 0.0f, pct);
    fl (id::lfo2Rate, "LFO2 Rate", lfoRate, 0.4f, hz);
    ch (id::lfo2Shape, "LFO2 Shape", lfoShapeNames, 1);
    ch (id::lfo2Sync, "LFO2 Clock", lfoSyncNames, 0);
    fl (id::lfo2Index, "LFO2 > FM Index", bipolar, 0.0f, bip);
    fl (id::lfo2Fb, "LFO2 > Feedback", bipolar, 0.0f, bip);
    fl (id::lfo2Cut, "LFO2 > Cutoff", bipolar, 0.0f, bip);
    fl (id::lfo2Pan, "LFO2 > Pan", bipolar, 0.0f, bip);
    fl (id::lfo3Rate, "LFO3 Rate", lfoRate, 1.5f, hz);
    ch (id::lfo3Shape, "LFO3 Shape", lfoShapeNames, 0);
    ch (id::lfo3Sync, "LFO3 Clock", lfoSyncNames, 0);
    ch (id::lfo3Dst, "LFO3 Destination", dstNames, (int) dstOff);
    fl (id::lfo3Amt, "LFO3 Amount", bipolar, 0.0f, bip);

    // ---- matriz ----
    for (int s = 0; s < numSlots; ++s)
    {
        const auto n = "Matrix " + String (s + 1);
        ch (id::mSrc (s), n + " Source", srcNames, s == 0 ? (int) srcWheel : (int) srcOff);
        ch (id::mDst (s), n + " Destination", dstNames, s == 0 ? (int) dstIndex : (int) dstOff);
        fl (id::mAmt (s), n + " Amount", bipolar, s == 0 ? 30.0f : 0.0f, bip);
    }

    // ---- arpejador ----
    add (new AudioParameterBool ({ id::arpOn, 1 }, "Arp On", false));
    ch (id::arpMode, "Arp Mode", arpModeNames, 0);
    add (new AudioParameterInt ({ id::arpOct, 1 }, "Arp Octaves", 1, 4, 1));
    ch (id::arpRate, "Arp Rate", arpRateNames, 2);
    fl (id::arpGate, "Arp Gate", NormalisableRange<float> (5.0f, 100.0f), 55.0f, pct);
    add (new AudioParameterBool ({ id::arpHold, 1 }, "Arp Hold", false));

    // ---- efeitos ----
    fl (id::dlySend, "Delay Send", unit, 0.0f, pct);
    ch (id::dlyTime, "Delay Time", delayNames, 3);
    fl (id::dlyFb, "Delay Feedback", NormalisableRange<float> (0.0f, 95.0f), 40.0f, pct);
    fl (id::rvbSend, "Reverb Send", unit, 0.0f, pct);
    fl (id::rvbSize, "Reverb Size", unit, 55.0f, pct);
    fl (id::rvbDamp, "Reverb Damping", unit, 45.0f, pct);
    fl (id::eqLow, "EQ Low", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db);
    fl (id::eqMid, "EQ Mid", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db);
    fl (id::eqHigh, "EQ High", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db);
    ch (id::distType, "Distortion Type", distNames, 0);
    fl (id::distAmt, "Distortion", unit, 0.0f, pct);
    fl (id::boostAmt, "Analog Boost", unit, 0.0f, pct);
    fl (id::boostTune, "Boost Tune", range (40.0f, 400.0f, 120.0f), 100.0f, hz);
    fl (id::choMix, "Chorus Mix", unit, 0.0f, pct);
    fl (id::choInt, "Chorus Intensity", unit, 45.0f, pct);
    fl (id::phaMix, "Phaser Mix", unit, 0.0f, pct);
    fl (id::phaInt, "Phaser Intensity", unit, 50.0f, pct);

    return { p.begin(), p.end() };
}
} // namespace vacina
