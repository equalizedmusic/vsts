#pragma once
// MESA — parâmetros. Dois plugins do mesmo código: a CONSOLE (no master do DAW: COLA BUS + fader do master) e o
// CANAL (no fim de cada faixa, tipo CANAL ou GRUPO: pré, dinâmica, EQ, pan, fader). Cada um só tem os próprios.
#include <juce_audio_processors/juce_audio_processors.h>

namespace mesa::id
{
inline const juce::String mode = "mode", grp = "grp",
    pre = "pre", gain = "gain", drive = "drive", pad = "pad", hpf = "hpf", lpf = "lpf",
    eqOn = "eqon", eqm = "eqm", dynPre = "dynpre", hfBell = "hfbell", lfBell = "lfbell",
    dynm = "dynm", thr = "thr", rat = "rat", atk = "atk", rel = "rel", gate = "gate", cmix = "cmix",
    pan = "pan", mute = "mute", solo = "solo", fader = "fader",
    mthr = "mthr", matk = "matk", mrel = "mrel", mmkp = "mmkp", mrat = "mrat", mmix = "mmix", min = "min", mhpf = "mhpf",
    hq = "hq";
// EQ: ganho e frequência de cada banda (HF, HMF, LMF, LF)
inline const juce::String eqG[4] = { "hfg", "hmfg", "lmfg", "lfg" };
inline const juce::String eqF[4] = { "hff", "hmff", "lmff", "lff" };
inline const juce::String eqQ[2] = { "hmfq", "lmfq" };   // Q das médias
} // namespace mesa::id

namespace mesa
{
enum Mode { modeChannel = 0, modeGroup = 1, modeMaster = 2 };   // modeMaster = a CONSOLE
inline const juce::StringArray modeNames { "CANAL", "GRUPO" };
inline const juce::StringArray preNames { "E-4K", "API 312", "BRIT 73" };
inline const juce::StringArray eqNames { "E-4K", "BRIT 81", "API 550" };
inline const juce::StringArray dynNames { "VCA 4K", "COLA 4K", "FET 76", "OPTO 2A" };
// nomes curtos (cabeça do canal, teclas do strip)
inline const char* const preCurto[] = { "4K", "312", "73" };
inline const char* const eqCurto[] = { "4K", "81", "550" };
inline const char* const dynCurto[] = { "4K", "COLA", "76", "2A" };
inline const float atkMs[] = { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
inline const float relS[] = { 0.1f, 0.3f, 0.6f, 1.2f, -1.0f };        // -1 = AUTO
inline const float busRatios[] = { 1.5f, 2.0f, 4.0f, 10.0f };
constexpr float faderMin = -80.0f;                                    // fader no fim = -∞

inline const float eqFMin[4] = { 1500, 600, 200, 30 }, eqFMax[4] = { 16000, 7000, 2500, 450 }, eqFDef[4] = { 8000, 2500, 500, 100 };

inline juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}
inline juce::String dbText (float v, int dec = 1)
{
    return (v > 0.0f ? "+" : "") + juce::String (v, dec) + " dB";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout (bool console)
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto fl = [&] (const String& pid, const String& name, NormalisableRange<float> r, float def, std::function<String (float)> txt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                [txt] (float v, int) { return txt (v); })));
    };
    auto ch = [&] (const String& pid, const String& name, const StringArray& opts, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, opts, def)); };
    auto bo = [&] (const String& pid, const String& name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def)); };
    auto db1 = [] (float v) { return dbText (v); };
    auto faderTxt = [] (float v) { return v <= faderMin + 0.01f ? String (CharPointer_UTF8 ("-\xe2\x88\x9e dB")) : dbText (v); };

    if (console)
    {
        fl (id::fader, "Fader master", NormalisableRange<float> (faderMin, 10.0f, 0.0f, 2.2f), 0.0f, faderTxt);
        fl (id::mthr, "Cola threshold", { -30.0f, 6.0f, 0.1f }, 0.0f, db1);
        ch (id::matk, "Cola attack", { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 4);
        ch (id::mrel, "Cola release", { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "AUTO" }, 4);
        fl (id::mmkp, "Cola makeup", { 0.0f, 20.0f, 0.1f }, 0.0f, db1);
        ch (id::mrat, "Cola ratio", { "1.5:1", "2:1", "4:1", "10:1" }, 2);
        fl (id::mmix, "Cola dry/wet", { 0.0f, 100.0f, 1.0f }, 100.0f, [] (float v) { return String (roundToInt (v)) + "%"; });
        bo (id::min, "Cola in", true);
        bo (id::mhpf, "Cola sidechain HPF", false);
        return { p.begin(), p.end() };
    }

    ch (id::mode, "Tipo", modeNames, 0);
    ch (id::grp, "Grupo", { "-", "G1", "G2", "G3" }, 0);

    ch (id::pre, "Preamp", preNames, 0);
    fl (id::gain, "Gain", { -20.0f, 20.0f, 0.1f }, 0.0f, db1);
    fl (id::drive, "Drive", { 0.0f, 10.0f, 0.01f }, 0.0f, [] (float v) { return String (v, 1); });
    bo (id::pad, "Pad -20 dB", false);
    fl (id::hpf, "HPF", NormalisableRange<float> (20.0f, 400.0f, 0.0f, 0.45f), 20.0f,
        [] (float v) { return v <= 20.5f ? String ("OFF") : hzText (v); });
    fl (id::lpf, "LPF", NormalisableRange<float> (3000.0f, 20000.0f, 0.0f, 0.5f), 20000.0f,
        [] (float v) { return v >= 19900.0f ? String ("OFF") : hzText (v); });

    bo (id::eqOn, "EQ in", true);
    ch (id::eqm, "Modelo do EQ", eqNames, 0);
    bo (id::dynPre, "Dynamics pre EQ", false);
    const char* bandNames[4] = { "HF", "HMF", "LMF", "LF" };
    for (int b = 0; b < 4; ++b)
    {
        fl (id::eqG[b], String (bandNames[b]) + " gain", { -15.0f, 15.0f, 0.1f }, 0.0f, db1);
        fl (id::eqF[b], String (bandNames[b]) + " freq", NormalisableRange<float> (eqFMin[b], eqFMax[b], 0.0f, 0.4f), eqFDef[b],
            [] (float v) { return hzText (v); });
    }
    for (int b = 0; b < 2; ++b)
        fl (id::eqQ[b], String (b == 0 ? "HMF" : "LMF") + " Q", NormalisableRange<float> (0.4f, 4.0f, 0.0f, 0.5f), 1.0f,
            [] (float v) { return "Q " + String (v, 2); });
    bo (id::hfBell, "HF bell", false);
    bo (id::lfBell, "LF bell", false);

    ch (id::dynm, "Modelo do compressor", dynNames, 0);
    fl (id::thr, "Comp threshold", { -40.0f, 10.0f, 0.1f }, 10.0f,
        [] (float v) { return v >= 9.9f ? String ("OFF") : dbText (v); });
    fl (id::rat, "Comp ratio", NormalisableRange<float> (1.0f, 20.0f, 0.0f, 0.4f), 4.0f,
        [] (float v) { return String (v, 1) + ":1"; });
    // ataque e soltura em 0..10: o tempo de verdade depende do modelo (Dsp.h: dynAtkMs/dynRelS); a tela mostra em ms
    fl (id::atk, "Comp attack", { 0.0f, 10.0f, 0.01f }, 6.0f, [] (float v) { return String (v, 1); });
    fl (id::rel, "Comp release", { 0.0f, 10.0f, 0.01f }, 10.0f, [] (float v) { return v >= 9.9f ? String ("AUTO") : String (v, 1); });
    fl (id::gate, "Gate", { -80.0f, -10.0f, 0.1f }, -80.0f,
        [] (float v) { return v <= -79.5f ? String ("OFF") : dbText (v); });
    fl (id::cmix, "Comp dry/wet", { 0.0f, 100.0f, 1.0f }, 100.0f, [] (float v) { return String (roundToInt (v)) + "%"; });

    fl (id::pan, "Pan", { -1.0f, 1.0f, 0.01f }, 0.0f, [] (float v)
        { const int n = roundToInt (std::abs (v) * 100.0f); return n == 0 ? String ("C") : (v < 0 ? "L" : "R") + String (n); });
    bo (id::mute, "Mute", false);
    bo (id::solo, "Solo", false);
    fl (id::fader, "Fader", NormalisableRange<float> (faderMin, 10.0f, 0.0f, 2.2f), 0.0f, faderTxt);

    bo (id::hq, "HQ (oversampling 4x)", true);
    return { p.begin(), p.end() };
}
} // namespace mesa
