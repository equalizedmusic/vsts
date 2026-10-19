#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "MultibandEngine.h"

namespace lente
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline const char* const thr[numBands]   = { "thr1",   "thr2",   "thr3",   "thr4" };
    inline const char* const ratio[numBands] = { "ratio1", "ratio2", "ratio3", "ratio4" };
    inline const char* const att[numBands]   = { "att1",   "att2",   "att3",   "att4" };
    inline const char* const rel[numBands]   = { "rel1",   "rel2",   "rel3",   "rel4" };
    inline const char* const drive[numBands] = { "drive1", "drive2", "drive3", "drive4" };
    inline const char* const gain[numBands]  = { "gain1",  "gain2",  "gain3",  "gain4" };
    inline const char* const solo[numBands]  = { "solo1",  "solo2",  "solo3",  "solo4" };
    inline const char* const mute[numBands]  = { "mute1",  "mute2",  "mute3",  "mute4" };
    inline const char* const xover[3]        = { "xover1", "xover2", "xover3" };

    inline constexpr const char* macro  = "macro";
    inline constexpr const char* mix    = "mix";
    inline constexpr const char* output = "output";
    inline constexpr const char* bypass = "bypass";
}

// nomes das bandas (UTF-8; passar por u8)
inline const char* const bandNames[numBands] = { "GRAVES", "MÉDIO-GRAVES", "MÉDIO-AGUDOS", "AGUDOS" };
inline const char* const bandHost[numBands]  = { "Graves", "Médio-graves", "Médio-agudos", "Agudos" };
inline const float xoverDefaults[3] = { 120.0f, 1000.0f, 6000.0f };

// botões de performance: os presets não mexem neles
inline bool isPerformanceParam (const juce::String& pid)
{
    if (pid == id::bypass) return true;
    for (int b = 0; b < numBands; ++b)
        if (pid == id::solo[b] || pid == id::mute[b]) return true;
    return false;
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String msText (float v)
{
    return v < 10.0f ? juce::String (v, 1).replaceCharacter ('.', ',') + " ms" : juce::String (juce::roundToInt (v)) + " ms";
}

inline juce::String hzText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1).replaceCharacter ('.', ',') + " kHz"
                         : juce::String (juce::roundToInt (hz)) + " Hz";
}

inline juce::String ratioText (float v)
{
    return juce::String (v, 1).replaceCharacter ('.', ',') + ":1";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&layout] (const char* pid, const String& name, NormalisableRange<float> range, float def,
                          std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto db  = [] (float v, int) { return dbText (v); };
    auto ms  = [] (float v, int) { return msText (v); };
    auto hz  = [] (float v, int) { return hzText (v); };
    auto rt  = [] (float v, int) { return ratioText (v); };

    NormalisableRange<float> ratioRange (1.0f, 20.0f);
    ratioRange.setSkewForCentre (4.0f);
    NormalisableRange<float> attRange (0.1f, 100.0f);
    attRange.setSkewForCentre (10.0f);
    NormalisableRange<float> relRange (10.0f, 1000.0f);
    relRange.setSkewForCentre (120.0f);
    NormalisableRange<float> hzRange (20.0f, 20000.0f);
    hzRange.setSkewForCentre (632.0f);

    for (int b = 0; b < numBands; ++b)
    {
        const String band = " " + u8 (bandHost[b]);
        add (id::thr[b],   "Limiar" + band,       { -60.0f, 0.0f }, 0.0f, db);
        add (id::ratio[b], u8 ("Razão") + band,   ratioRange, 2.0f, rt);
        add (id::att[b],   "Ataque" + band,       attRange, 10.0f, ms);
        add (id::rel[b],   "Soltura" + band,      relRange, 150.0f, ms);
        add (id::drive[b], "Drive" + band,        { 0.0f, 24.0f }, 0.0f, db);
        add (id::gain[b],  "Ganho" + band,        { -18.0f, 18.0f }, 0.0f, db);
    }
    for (int k = 0; k < 3; ++k)
        add (id::xover[k], "Corte " + String (k + 1), hzRange, xoverDefaults[k], hz);

    add (id::macro,  "Macro", { 0.0f, 100.0f }, 0.0f, pct);
    add (id::mix,    "Mix",   { 0.0f, 100.0f }, 100.0f, pct);
    add (id::output, u8 ("Ganho de saída"), { -24.0f, 12.0f }, 0.0f, db);

    for (int b = 0; b < numBands; ++b)
    {
        const String band = " " + u8 (bandHost[b]);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::solo[b], 1 }, "Solo" + band, false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::mute[b], 1 }, "Mudo" + band, false));
    }
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// uma banda do preset: limiar, razão, ataque, soltura, drive, ganho
struct BandValues
{
    float thr, ratio, att, rel, drive, gain;
};

inline FactoryPreset makePreset (const juce::String& name, float f1, float f2, float f3, float macro, float mix, float output,
                                 const BandValues& b1, const BandValues& b2, const BandValues& b3, const BandValues& b4)
{
    FactoryPreset p;
    p.name = name;
    p.values = { { id::xover[0], f1 }, { id::xover[1], f2 }, { id::xover[2], f3 },
                 { id::macro, macro }, { id::mix, mix }, { id::output, output } };
    const BandValues* bands[numBands] = { &b1, &b2, &b3, &b4 };
    for (int b = 0; b < numBands; ++b)
    {
        const auto& v = *bands[b];
        p.values.push_back ({ id::thr[b],   v.thr });
        p.values.push_back ({ id::ratio[b], v.ratio });
        p.values.push_back ({ id::att[b],   v.att });
        p.values.push_back ({ id::rel[b],   v.rel });
        p.values.push_back ({ id::drive[b], v.drive });
        p.values.push_back ({ id::gain[b],  v.gain });
    }
    return p;
}

inline std::vector<FactoryPreset> factoryPresets()
{
    //                 nome                                f1      f2       f3      macro mix   saída
    //                 limiar  razão  ataque soltura drive ganho
    return {
        { u8 ("Init · Lente"), { } },
        makePreset ("Cola de Mix",                         120.0f, 1000.0f, 6000.0f, 0.0f, 100.0f, 0.0f,
                    { -18.0f, 2.0f, 30.0f, 200.0f, 0.0f, 0.0f }, { -18.0f, 2.0f, 20.0f, 160.0f, 0.0f, 0.0f },
                    { -20.0f, 2.0f, 15.0f, 140.0f, 0.0f, 0.0f }, { -22.0f, 2.0f, 10.0f, 120.0f, 0.0f, 0.0f }),
        makePreset ("Master Transparente",                 100.0f, 800.0f, 7000.0f, 0.0f, 100.0f, 0.0f,
                    { -14.0f, 1.5f, 40.0f, 250.0f, 0.0f, 0.5f }, { -14.0f, 1.5f, 30.0f, 200.0f, 0.0f, 0.0f },
                    { -16.0f, 1.5f, 20.0f, 160.0f, 0.0f, 0.0f }, { -18.0f, 1.5f, 10.0f, 120.0f, 0.0f, 0.5f }),
        makePreset ("Master Psy",                          140.0f, 1200.0f, 8000.0f, 15.0f, 100.0f, 0.0f,
                    { -16.0f, 4.0f, 15.0f, 90.0f, 3.0f, 1.0f }, { -18.0f, 3.0f, 10.0f, 100.0f, 2.0f, 0.0f },
                    { -18.0f, 3.0f, 5.0f, 80.0f, 2.0f, 0.5f }, { -20.0f, 3.0f, 2.0f, 60.0f, 1.0f, 1.0f }),
        makePreset ("Grave Firme",                         150.0f, 1000.0f, 6000.0f, 0.0f, 100.0f, 0.0f,
                    { -24.0f, 6.0f, 20.0f, 120.0f, 2.0f, 2.0f }, { -6.0f, 1.5f, 20.0f, 150.0f, 0.0f, 0.0f },
                    { 0.0f, 1.0f, 10.0f, 150.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 10.0f, 150.0f, 0.0f, 0.0f }),
        makePreset ("Bateria Viva",                        100.0f, 900.0f, 5000.0f, 0.0f, 100.0f, 0.0f,
                    { -18.0f, 3.0f, 25.0f, 120.0f, 2.0f, 1.0f }, { -20.0f, 3.0f, 20.0f, 100.0f, 1.0f, 0.0f },
                    { -18.0f, 2.5f, 15.0f, 90.0f, 2.0f, 1.0f }, { -22.0f, 2.0f, 8.0f, 80.0f, 1.0f, 1.5f }),
        makePreset ("Bateria Esmagada",                    100.0f, 900.0f, 5000.0f, 40.0f, 70.0f, 0.0f,
                    { -30.0f, 8.0f, 2.0f, 60.0f, 8.0f, 3.0f }, { -30.0f, 8.0f, 2.0f, 60.0f, 6.0f, 2.0f },
                    { -30.0f, 8.0f, 1.0f, 50.0f, 6.0f, 2.0f }, { -30.0f, 6.0f, 1.0f, 50.0f, 4.0f, 2.0f }),
        makePreset ("Vocal na Frente",                     180.0f, 1500.0f, 6500.0f, 0.0f, 100.0f, 0.0f,
                    { -20.0f, 3.0f, 15.0f, 150.0f, 0.0f, -2.0f }, { -22.0f, 3.0f, 10.0f, 120.0f, 2.0f, 1.0f },
                    { -20.0f, 2.5f, 8.0f, 100.0f, 3.0f, 1.5f }, { -24.0f, 3.0f, 3.0f, 80.0f, 0.0f, 0.0f }),
        makePreset ("De-esser Suave",                      200.0f, 2000.0f, 5500.0f, 0.0f, 100.0f, 0.0f,
                    { 0.0f, 1.0f, 10.0f, 150.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 10.0f, 150.0f, 0.0f, 0.0f },
                    { -6.0f, 1.5f, 5.0f, 100.0f, 0.0f, 0.0f }, { -30.0f, 6.0f, 0.5f, 60.0f, 0.0f, 0.0f }),
        makePreset ("Baixo Saturado",                      90.0f, 700.0f, 4000.0f, 10.0f, 100.0f, -1.0f,
                    { -18.0f, 4.0f, 20.0f, 120.0f, 4.0f, 1.0f }, { -20.0f, 3.0f, 10.0f, 100.0f, 12.0f, 0.0f },
                    { -18.0f, 2.0f, 5.0f, 80.0f, 6.0f, -1.0f }, { -12.0f, 2.0f, 5.0f, 80.0f, 0.0f, -3.0f }),
        makePreset ("Guitarra Encorpada",                  150.0f, 1000.0f, 5000.0f, 0.0f, 100.0f, 0.0f,
                    { -12.0f, 2.0f, 20.0f, 150.0f, 0.0f, -3.0f }, { -20.0f, 3.0f, 15.0f, 120.0f, 6.0f, 1.5f },
                    { -18.0f, 3.0f, 10.0f, 100.0f, 4.0f, 0.5f }, { -16.0f, 2.0f, 5.0f, 80.0f, 0.0f, -1.0f }),
        makePreset (u8 ("Brilho Analógico"),               120.0f, 1000.0f, 6000.0f, 0.0f, 100.0f, 0.0f,
                    { -12.0f, 1.5f, 30.0f, 200.0f, 0.0f, 0.0f }, { -12.0f, 1.5f, 20.0f, 160.0f, 0.0f, 0.0f },
                    { -16.0f, 2.0f, 10.0f, 120.0f, 5.0f, 1.0f }, { -18.0f, 2.0f, 5.0f, 100.0f, 8.0f, 2.0f }),
        makePreset ("Fita Quente",                         120.0f, 1000.0f, 6000.0f, 10.0f, 100.0f, -1.0f,
                    { -16.0f, 2.0f, 30.0f, 200.0f, 6.0f, 1.0f }, { -16.0f, 2.0f, 25.0f, 180.0f, 5.0f, 0.5f },
                    { -16.0f, 2.0f, 20.0f, 160.0f, 4.0f, 0.0f }, { -14.0f, 2.0f, 10.0f, 140.0f, 2.0f, -2.0f }),
        makePreset ("Loudness Club",                       120.0f, 1000.0f, 7000.0f, 25.0f, 100.0f, 0.0f,
                    { -20.0f, 5.0f, 10.0f, 80.0f, 4.0f, 1.5f }, { -20.0f, 4.0f, 8.0f, 80.0f, 3.0f, 1.0f },
                    { -20.0f, 4.0f, 5.0f, 70.0f, 3.0f, 1.0f }, { -22.0f, 4.0f, 2.0f, 60.0f, 2.0f, 1.5f }),
        makePreset ("Paralelo Nova York",                  120.0f, 1000.0f, 6000.0f, 30.0f, 40.0f, 0.0f,
                    { -36.0f, 10.0f, 1.0f, 80.0f, 6.0f, 6.0f }, { -36.0f, 10.0f, 1.0f, 80.0f, 6.0f, 6.0f },
                    { -36.0f, 10.0f, 1.0f, 70.0f, 6.0f, 6.0f }, { -36.0f, 10.0f, 1.0f, 70.0f, 4.0f, 6.0f }),
        makePreset ("Lo-fi Abafado",                       250.0f, 1200.0f, 3500.0f, 20.0f, 100.0f, 0.0f,
                    { -20.0f, 4.0f, 10.0f, 150.0f, 10.0f, -4.0f }, { -20.0f, 4.0f, 10.0f, 150.0f, 14.0f, 2.0f },
                    { -20.0f, 3.0f, 10.0f, 150.0f, 10.0f, -6.0f }, { -10.0f, 2.0f, 10.0f, 150.0f, 0.0f, -18.0f }),
        makePreset ("Pad Aveludado",                       150.0f, 1000.0f, 5000.0f, 0.0f, 100.0f, 0.0f,
                    { -20.0f, 2.0f, 60.0f, 400.0f, 0.0f, -1.0f }, { -22.0f, 2.0f, 50.0f, 350.0f, 3.0f, 0.0f },
                    { -24.0f, 2.5f, 40.0f, 300.0f, 2.0f, -1.0f }, { -26.0f, 3.0f, 30.0f, 250.0f, 0.0f, -2.5f }),
        makePreset ("Kick e Baixo Separados",              70.0f, 250.0f, 3000.0f, 0.0f, 100.0f, 0.0f,
                    { -20.0f, 4.0f, 30.0f, 100.0f, 2.0f, 1.0f }, { -24.0f, 5.0f, 5.0f, 80.0f, 0.0f, -1.5f },
                    { -18.0f, 2.0f, 10.0f, 120.0f, 0.0f, 0.0f }, { -16.0f, 1.5f, 10.0f, 120.0f, 0.0f, 0.0f }),
        makePreset (u8 ("Médios Domados"),                 200.0f, 800.0f, 3500.0f, 0.0f, 100.0f, 0.0f,
                    { -10.0f, 1.5f, 20.0f, 150.0f, 0.0f, 0.0f }, { -26.0f, 4.0f, 10.0f, 120.0f, 0.0f, -1.0f },
                    { -26.0f, 4.0f, 8.0f, 100.0f, 0.0f, -1.0f }, { -10.0f, 1.5f, 10.0f, 120.0f, 0.0f, 0.5f }),
        makePreset (u8 ("Rádio Antigo"),                   300.0f, 1000.0f, 3500.0f, 20.0f, 100.0f, 0.0f,
                    { -12.0f, 2.0f, 10.0f, 150.0f, 0.0f, -18.0f }, { -24.0f, 6.0f, 5.0f, 100.0f, 16.0f, 3.0f },
                    { -24.0f, 6.0f, 5.0f, 100.0f, 14.0f, 2.0f }, { -12.0f, 2.0f, 10.0f, 150.0f, 0.0f, -18.0f }),
    };
}
} // namespace lente
