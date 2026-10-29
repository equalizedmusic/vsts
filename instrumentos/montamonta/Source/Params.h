#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

using APVTS = juce::AudioProcessorValueTreeState;

// Parâmetros do MONTA-MONTA: as peças de um Minimoog de 2 osciladores. Os knobs usam a escala 0–10 do painel do Moog
// (o valor de verdade é convertido no motor); RANGE em pés de órgão, como no original.
namespace mm::id
{
inline const juce::String vco1Range { "vco1Range" }, vco1Wave { "vco1Wave" }, vco1Tune { "vco1Tune" };
inline const juce::String vco2Range { "vco2Range" }, vco2Wave { "vco2Wave" }, vco2Tune { "vco2Tune" };
inline const juce::String mixOsc1 { "mixOsc1" }, mixOsc2 { "mixOsc2" }, mixNoise { "mixNoise" };
inline const juce::String mixOsc1On { "mixOsc1On" }, mixOsc2On { "mixOsc2On" }, mixNoiseOn { "mixNoiseOn" };
inline const juce::String vcfCutoff { "vcfCutoff" }, vcfEmph { "vcfEmph" }, vcfContour { "vcfContour" };
inline const juce::String ampA { "ampA" }, ampD { "ampD" }, ampS { "ampS" }, ampR { "ampR" };
inline const juce::String modA { "modA" }, modD { "modD" }, modS { "modS" }, modR { "modR" };
inline const juce::String outVol { "outVol" };
} // namespace mm::id

namespace mm
{
inline const juce::StringArray rangeNames { "32'", "16'", "8'", "4'", "2'" };
inline const juce::StringArray waveNames { "TRI", "TRI/SAW", "SAW", "SQUARE", "WIDE", "NARROW" };

inline juce::NormalisableRange<float> msRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi) * 0.6f);
    return r;
}

inline APVTS::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto attr = [] (const char* unit) { return AudioParameterFloatAttributes().withLabel (unit); };
    auto ten = [&] (const String& pid, const String& name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (0.0f, 10.0f), def,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                [] (float v, int) { return String (v, 1); })));
    };
    auto ms = [&] (const String& pid, const String& name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, msRange (lo, hi), def,
                                                            attr ("ms").withStringFromValueFunction ([] (float v, int)
                                                            { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; })));
    };
    auto st = [&] (const String& pid, const String& name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (-7.0f, 7.0f), def,
                                                            attr ("st").withStringFromValueFunction ([] (float v, int)
                                                            { return (v > 0.004f ? "+" : "") + String (v, 2) + " st"; })));
    };

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::vco1Range, 1 }, "VCO 1 Range", rangeNames, 2));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::vco1Wave, 1 }, "VCO 1 Waveform", waveNames, 2));
    st (id::vco1Tune, "VCO 1 Tune", 0.0f);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::vco2Range, 1 }, "VCO 2 Range", rangeNames, 2));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id::vco2Wave, 1 }, "VCO 2 Waveform", waveNames, 2));
    st (id::vco2Tune, "VCO 2 Tune", 0.12f);

    ten (id::mixOsc1, "Mixer Osc 1", 8.0f);
    ten (id::mixOsc2, "Mixer Osc 2", 7.0f);
    ten (id::mixNoise, "Mixer Noise", 3.0f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id::mixOsc1On, 1 }, "Mixer Osc 1 On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id::mixOsc2On, 1 }, "Mixer Osc 2 On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id::mixNoiseOn, 1 }, "Mixer Noise On", false));

    {
        NormalisableRange<float> r (40.0f, 16000.0f);
        r.setSkewForCentre (800.0f);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id::vcfCutoff, 1 }, "VCF Cutoff", r, 1800.0f,
                                                            attr ("Hz").withStringFromValueFunction ([] (float v, int)
                                                            { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; })));
    }
    ten (id::vcfEmph, "VCF Emphasis", 3.0f);
    ten (id::vcfContour, "VCF Contour", 5.0f);

    ms (id::ampA, "Env Amp Attack", 1.0f, 5000.0f, 5.0f);
    ms (id::ampD, "Env Amp Decay", 5.0f, 5000.0f, 400.0f);
    ten (id::ampS, "Env Amp Sustain", 7.0f);
    ms (id::ampR, "Env Amp Release", 5.0f, 5000.0f, 250.0f);

    ms (id::modA, "Env Mod Attack", 1.0f, 5000.0f, 5.0f);
    ms (id::modD, "Env Mod Decay", 5.0f, 5000.0f, 450.0f);
    ten (id::modS, "Env Mod Sustain", 2.0f);
    ms (id::modR, "Env Mod Release", 5.0f, 5000.0f, 300.0f);

    ten (id::outVol, "Output Volume", 7.0f);
    return { p.begin(), p.end() };
}
} // namespace mm
