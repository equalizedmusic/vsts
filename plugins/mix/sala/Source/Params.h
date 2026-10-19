#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "RoomEngine.h"

namespace sala
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* predelay  = "predelay";
    inline constexpr const char* preSync   = "presync";
    inline constexpr const char* size      = "size";
    inline constexpr const char* decay     = "decay";
    inline constexpr const char* diffusion = "diffusion";
    inline constexpr const char* early     = "early";
    inline constexpr const char* lowCut    = "lowcut";
    inline constexpr const char* highCut   = "highcut";
    inline constexpr const char* damp      = "damp";
    inline constexpr const char* duck      = "duck";
    inline constexpr const char* width     = "width";
    inline constexpr const char* output    = "output";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* solo      = "solo";
    inline constexpr const char* bypass    = "bypass";
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String secText (float v)
{
    return v < 10.0f ? juce::String (v, 1).replaceCharacter ('.', ',') + " s" : juce::String (juce::roundToInt (v)) + " s";
}

inline juce::String msText (float v)
{
    return juce::String (juce::roundToInt (v)) + " ms";
}

inline juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 1).replaceCharacter ('.', ',') + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
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
    auto choice = [&layout] (const char* pid, const String& name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, items, def));
    };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    StringArray syncs;
    for (auto* n : syncNames) syncs.add (n);

    add (id::predelay,  u8 ("Pré-delay"),     { 0.0f, 250.0f }, 15.0f, [] (float v, int) { return msText (v); });
    choice (id::preSync, u8 ("Pré no tempo"), syncs, 0);
    add (id::size,      "Tamanho",            { 0.0f, 100.0f }, 50.0f, pct);
    add (id::decay,     "Decay",              skewed (0.2f, maxDecay, 2.0f), 1.6f, [] (float v, int) { return secText (v); });
    add (id::diffusion, u8 ("Difusão"),       { 0.0f, 100.0f }, 75.0f, pct);
    add (id::early,     u8 ("Reflexões"),     { 0.0f, 100.0f }, 50.0f, pct);
    add (id::lowCut,    "Passa-altas",        skewed (20.0f, 1000.0f, 150.0f), 120.0f, [] (float v, int) { return hzText (v); });
    add (id::highCut,   "Passa-baixas",       skewed (1000.0f, 20000.0f, 6000.0f), 12000.0f, [] (float v, int) { return hzText (v); });
    add (id::damp,      "Amortecimento",      { 0.0f, 100.0f }, 35.0f, pct);
    add (id::duck,      "Duck",               { 0.0f, 100.0f }, 0.0f, pct);
    add (id::width,     "Largura",            { 0.0f, 150.0f }, 100.0f, pct);
    add (id::output,    "Ganho",              { -36.0f, 12.0f }, 0.0f, [] (float v, int) { return dbText (v); });
    add (id::mix,       "Mix",                { 0.0f, 100.0f }, 25.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::solo, 1 }, u8 ("Só a sala"), false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// PRÉ NO TEMPO guarda o índice: 0=livre 1=1/64 2=1/32 3=1/16 4=1/8 5=1/4
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Sala"),            { } },
        { "Sala Pequena",                { { id::size, 20 }, { id::decay, 0.6f }, { id::predelay, 4 },  { id::early, 70 }, { id::diffusion, 70 }, { id::damp, 45 }, { id::mix, 20 } } },
        { u8 ("Estúdio de Madeira"),     { { id::size, 35 }, { id::decay, 0.9f }, { id::predelay, 8 },  { id::early, 60 }, { id::damp, 55 }, { id::highCut, 9000 }, { id::mix, 18 } } },
        { u8 ("Estúdio Vivo"),           { { id::size, 45 }, { id::decay, 1.2f }, { id::predelay, 10 }, { id::early, 75 }, { id::damp, 25 }, { id::mix, 20 } } },
        { u8 ("Câmara"),                 { { id::size, 55 }, { id::decay, 1.8f }, { id::predelay, 12 }, { id::early, 40 }, { id::diffusion, 85 }, { id::damp, 30 }, { id::lowCut, 150 } } },
        { u8 ("Câmara de Eco"),          { { id::size, 65 }, { id::decay, 2.4f }, { id::predelay, 20 }, { id::early, 30 }, { id::diffusion, 90 }, { id::damp, 40 }, { id::lowCut, 200 } } },
        { "Igreja",                      { { id::size, 85 }, { id::decay, 4.5f }, { id::predelay, 30 }, { id::early, 55 }, { id::diffusion, 80 }, { id::damp, 40 }, { id::mix, 30 } } },
        { "Catedral",                    { { id::size, 100 }, { id::decay, 8.0f }, { id::predelay, 45 }, { id::early, 45 }, { id::diffusion, 85 }, { id::damp, 50 }, { id::lowCut, 80 }, { id::mix, 30 } } },
        { "Placa de Voz",                { { id::size, 50 }, { id::decay, 1.9f }, { id::preSync, 3 }, { id::early, 0 }, { id::diffusion, 95 }, { id::damp, 15 }, { id::lowCut, 250 }, { id::duck, 30 } } },
        { "Placa Brilhante",             { { id::size, 45 }, { id::decay, 2.6f }, { id::predelay, 25 }, { id::early, 0 }, { id::diffusion, 100 }, { id::damp, 5 }, { id::lowCut, 300 } } },
        { "Caixa Gorda",                 { { id::size, 40 }, { id::decay, 1.4f }, { id::predelay, 5 },  { id::early, 85 }, { id::diffusion, 90 }, { id::damp, 30 }, { id::lowCut, 180 }, { id::highCut, 10000 }, { id::mix, 30 } } },
        { "Caixa de Arena",              { { id::size, 80 }, { id::decay, 2.8f }, { id::predelay, 15 }, { id::early, 60 }, { id::diffusion, 85 }, { id::damp, 35 }, { id::lowCut, 250 }, { id::mix, 30 } } },
        { "Ambiente de Bateria",         { { id::size, 25 }, { id::decay, 0.5f }, { id::predelay, 2 },  { id::early, 100 }, { id::diffusion, 55 }, { id::damp, 40 }, { id::width, 130 }, { id::mix, 25 } } },
        { "Overheads Abertos",           { { id::size, 40 }, { id::decay, 0.8f }, { id::predelay, 6 },  { id::early, 80 }, { id::diffusion, 65 }, { id::damp, 20 }, { id::lowCut, 300 }, { id::width, 140 } } },
        { "Vocal Pop Limpo",             { { id::size, 55 }, { id::decay, 1.5f }, { id::preSync, 4 }, { id::early, 20 }, { id::diffusion, 85 }, { id::damp, 30 }, { id::lowCut, 300 }, { id::highCut, 9000 }, { id::duck, 50 }, { id::mix, 20 } } },
        { u8 ("Violão Íntimo"),          { { id::size, 30 }, { id::decay, 1.0f }, { id::predelay, 12 }, { id::early, 55 }, { id::diffusion, 75 }, { id::damp, 45 }, { id::lowCut, 160 }, { id::mix, 18 } } },
        { "Piano de Concerto",           { { id::size, 75 }, { id::decay, 2.3f }, { id::predelay, 22 }, { id::early, 50 }, { id::diffusion, 80 }, { id::damp, 40 }, { id::lowCut, 60 }, { id::mix, 25 } } },
        { "Cordas no Teatro",            { { id::size, 70 }, { id::decay, 2.0f }, { id::predelay, 18 }, { id::early, 45 }, { id::diffusion, 80 }, { id::damp, 50 }, { id::lowCut, 90 }, { id::mix, 30 } } },
        { "Corredor de Azulejo",         { { id::size, 60 }, { id::decay, 1.3f }, { id::predelay, 3 },  { id::early, 100 }, { id::diffusion, 30 }, { id::damp, 0 }, { id::width, 60 }, { id::mix, 25 } } },
        { u8 ("Ambiência de Mix"),       { { id::size, 35 }, { id::decay, 0.7f }, { id::predelay, 10 }, { id::early, 50 }, { id::diffusion, 80 }, { id::damp, 40 }, { id::lowCut, 200 }, { id::highCut, 8000 }, { id::mix, 12 } } },
    };
}
} // namespace sala
