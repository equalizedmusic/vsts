#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// VACINA — sintetizador virtual-analógico polifônico (16 vozes), leve no processador.
// Aqui ficam os ids dos parâmetros e as listas (o parâmetro de escolha guarda o ÍNDICE).
namespace vacina
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline constexpr int maxVoices = 16;

// ---------------- listas ----------------
inline const juce::StringArray model1Names  { "CLASSIC", "SUPER", "WAVE" };
inline const juce::StringArray model2Names  { "CLASSIC", "WAVE" };
inline const juce::StringArray tableNames   { "CLASSIC", "PULSE", "DOUBLE SAW", "FOLDED", "METAL", "FORMANT", "SYNC",
                                              "BENT", "ELECTRIC", "SQUARE-SAW", "SATURATED", "BRUSH", "CRYSTAL",
                                              "DIGITAL", "VOCAL", "HARMONICS", "GRANITE", "BELLS", "FM LINEAR",
                                              "FM METAL", "SPECTRAL", "ORGAN", "STRING", "VACINA" };
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
enum Dst { dstOff, dstCutoff, dstCutoff2, dstReso, dstPitch, dstPitch2, dstShape1, dstShape2, dstShape3, dstPw, dstFm,
           dstVol1, dstVol2, dstVol3, dstSub, dstNoise, dstRing, dstSpread, dstSat, dstFiltBal, dstPan, dstVolume, numDsts };
inline const juce::StringArray dstNames { "OFF", "CUTOFF", "CUTOFF 2", "RESONANCE", "PITCH", "OSC2 PITCH", "OSC1 SHAPE",
                                          "OSC2 SHAPE", "OSC3 SHAPE", "PULSE WIDTH", "FM AMOUNT", "OSC1 LEVEL", "OSC2 LEVEL",
                                          "OSC3 LEVEL", "SUB LEVEL", "NOISE LEVEL", "RING MOD", "SUPER SPREAD", "SATURATION",
                                          "FILT BALANCE", "PAN", "VOLUME" };
inline constexpr int numSlots = 6;

namespace id
{
inline const juce::String
    volume = "volume", keyMode = "keymode", porta = "porta", bendRange = "bendrange", drift = "drift",
    unison = "unison", uniDetune = "unidetune", uniSpread = "unispread",
    osc1Model = "o1model", osc1Table = "o1table", osc1Shape = "o1shape", osc1Pw = "o1pw", osc1Semi = "o1semi",
    osc1Spread = "o1spread", osc1Vol = "o1vol",
    osc2Model = "o2model", osc2Table = "o2table", osc2Shape = "o2shape", osc2Pw = "o2pw", osc2Semi = "o2semi",
    osc2Detune = "o2detune", osc2Sync = "o2sync", osc2Vol = "o2vol", fmAmt = "fm",
    osc3Shape = "o3shape", osc3Semi = "o3semi", osc3Detune = "o3detune", osc3Vol = "o3vol",
    subVol = "subvol", noiseVol = "noisevol", noiseColor = "noisecolor", ringVol = "ringvol", oscVol = "oscvol",
    cutoff = "cutoff", cutoff2 = "cutoff2", filtBal = "filtbal", routing = "routing",
    f1Mode = "f1mode", f2Mode = "f2mode", reso1 = "reso1", reso2 = "reso2", env1 = "env1", env2 = "env2",
    key1 = "key1", key2 = "key2", satType = "sattype", satAmt = "satamt",
    fA = "fa", fD = "fd", fS = "fs", fR = "fr", fVel = "fvel", fSlope = "fslope",
    aA = "aa", aD = "ad", aS = "as", aR = "ar", aVel = "avel", aSlope = "aslope",
    lfo1Rate = "l1rate", lfo1Shape = "l1shape", lfo1Sync = "l1sync",
    lfo1Pitch = "l1pitch", lfo1Cut = "l1cut", lfo1Pw = "l1pw", lfo1Amp = "l1amp",
    lfo2Rate = "l2rate", lfo2Shape = "l2shape", lfo2Sync = "l2sync",
    lfo2Shp = "l2shp", lfo2Fm = "l2fm", lfo2Cut = "l2cut", lfo2Pan = "l2pan",
    lfo3Rate = "l3rate", lfo3Shape = "l3shape", lfo3Sync = "l3sync", lfo3Dst = "l3dst", lfo3Amt = "l3amt",
    arpOn = "arpon", arpMode = "arpmode", arpOct = "arpoct", arpRate = "arprate", arpGate = "arpgate", arpHold = "arphold",
    dlySend = "dlysend", dlyTime = "dlytime", dlyFb = "dlyfb",
    rvbSend = "rvbsend", rvbSize = "rvbsize", rvbDamp = "rvbdamp",
    eqLow = "eqlow", eqMid = "eqmid", eqHigh = "eqhigh",
    distType = "disttype", distAmt = "distamt", boostAmt = "boostamt", boostTune = "boosttune",
    choMix = "chomix", choInt = "choint", phaMix = "phamix", phaInt = "phaint";

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
    fl (id::drift, "Analog Drift", unit, 20.0f, pct);
    add (new AudioParameterInt ({ id::unison, 1 }, "Unison", 1, 4, 1));
    fl (id::uniDetune, "Unison Detune", unit, 25.0f, pct);
    fl (id::uniSpread, "Unison Spread", unit, 60.0f, pct);

    // ---- osciladores ----
    ch (id::osc1Model, "Osc1 Model", model1Names, 0);
    ch (id::osc1Table, "Osc1 Wavetable", tableNames, 0);
    fl (id::osc1Shape, "Osc1 Shape", unit, 66.0f, pct);
    fl (id::osc1Pw, "Osc1 Pulse Width", NormalisableRange<float> (5.0f, 95.0f), 50.0f, pct);
    fl (id::osc1Semi, "Osc1 Semitone", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, st);
    fl (id::osc1Spread, "Osc1 Super Spread", unit, 40.0f, pct);
    fl (id::osc1Vol, "Osc1 Level", unit, 80.0f, pct);
    ch (id::osc2Model, "Osc2 Model", model2Names, 0);
    ch (id::osc2Table, "Osc2 Wavetable", tableNames, 0);
    fl (id::osc2Shape, "Osc2 Shape", unit, 66.0f, pct);
    fl (id::osc2Pw, "Osc2 Pulse Width", NormalisableRange<float> (5.0f, 95.0f), 50.0f, pct);
    fl (id::osc2Semi, "Osc2 Semitone", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, st);
    fl (id::osc2Detune, "Osc2 Detune", NormalisableRange<float> (-50.0f, 50.0f), 7.0f, ct);
    add (new AudioParameterBool ({ id::osc2Sync, 1 }, "Osc2 Sync", false));
    fl (id::osc2Vol, "Osc2 Level", unit, 0.0f, pct);
    fl (id::fmAmt, "FM Amount", unit, 0.0f, pct);
    fl (id::osc3Shape, "Osc3 Shape", unit, 66.0f, pct);
    fl (id::osc3Semi, "Osc3 Semitone", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), -12.0f, st);
    fl (id::osc3Detune, "Osc3 Detune", NormalisableRange<float> (-50.0f, 50.0f), 0.0f, ct);
    fl (id::osc3Vol, "Osc3 Level", unit, 0.0f, pct);

    // ---- mix ----
    fl (id::subVol, "Sub Level", unit, 0.0f, pct);
    fl (id::noiseVol, "Noise Level", unit, 0.0f, pct);
    fl (id::noiseColor, "Noise Color", bipolar, 0.0f, [] (float v, int)
        { const int i = roundToInt (v); return i == 0 ? String ("WHITE") : i < 0 ? "DARK " + String (-i) : "BRIGHT " + String (i); });
    fl (id::ringVol, "Ring Mod Level", unit, 0.0f, pct);
    fl (id::oscVol, "Osc Volume", unit, 50.0f, [] (float v, int)
        { const int i = roundToInt (v); return i <= 50 ? String (i * 2) + "%" : "SAT " + String ((i - 50) * 2) + "%"; });

    // ---- filtros ----
    fl (id::cutoff, "Cutoff", range (20.0f, 20000.0f, 900.0f), 2400.0f, hz);
    fl (id::cutoff2, "Cutoff 2", NormalisableRange<float> (-48.0f, 48.0f, 1.0f), 0.0f, st);
    fl (id::filtBal, "Filter Balance", unit, 0.0f, [] (float v, int)
        { const int i = roundToInt (v); return i == 0 ? String ("FILT 1") : i == 100 ? String ("FILT 2") : String (100 - i) + " / " + String (i); });
    ch (id::routing, "Filter Routing", routingNames, 0);
    ch (id::f1Mode, "Filter1 Mode", f1ModeNames, 0);
    ch (id::f2Mode, "Filter2 Mode", f2ModeNames, 0);
    fl (id::reso1, "Filter1 Resonance", unit, 20.0f, pct);
    fl (id::reso2, "Filter2 Resonance", unit, 20.0f, pct);
    fl (id::env1, "Filter1 Env Amount", bipolar, 30.0f, bip);
    fl (id::env2, "Filter2 Env Amount", bipolar, 30.0f, bip);
    fl (id::key1, "Filter1 Key Follow", unit, 30.0f, pct);
    fl (id::key2, "Filter2 Key Follow", unit, 30.0f, pct);
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
    fl (id::aS, "Amp Sustain", unit, 80.0f, pct);
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
    fl (id::lfo1Pw, "LFO1 > Pulse Width", bipolar, 0.0f, bip);
    fl (id::lfo1Amp, "LFO1 > Amp", unit, 0.0f, pct);
    fl (id::lfo2Rate, "LFO2 Rate", lfoRate, 0.4f, hz);
    ch (id::lfo2Shape, "LFO2 Shape", lfoShapeNames, 1);
    ch (id::lfo2Sync, "LFO2 Clock", lfoSyncNames, 0);
    fl (id::lfo2Shp, "LFO2 > Shape", bipolar, 0.0f, bip);
    fl (id::lfo2Fm, "LFO2 > FM", bipolar, 0.0f, bip);
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
        ch (id::mDst (s), n + " Destination", dstNames, s == 0 ? (int) dstCutoff : (int) dstOff);
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
