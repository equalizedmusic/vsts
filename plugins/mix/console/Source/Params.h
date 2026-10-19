#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "ConsoleEngine.h"

namespace console
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* model     = "model";
    inline constexpr const char* tone      = "tone";
    inline constexpr const char* crosstalk = "crosstalk";
    inline constexpr const char* noise     = "noise";
    inline constexpr const char* output    = "output";
    inline constexpr const char* autoGain  = "autogain";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* os4       = "os4x";
    inline constexpr const char* compare   = "compare";
    inline constexpr const char* bypass    = "bypass";
}

// nomes dos modelos (o índice é o da lista MODELO)
inline const char* const modelNames[numModels] = { "Cristal", "Ferro", "Brasa" };

inline const char* modelDescription (int model)
{
    switch (juce::jlimit (0, numModels - 1, model))
    {
        case 0:  return "Canal limpo de mesa britânica: muita folga, um pouco de 3º harmônico e um brilho leve no agudo.";
        case 1:  return "Canal com transformador: aperta mais cedo, mistura pares e ímpares, encorpa o grave e arredonda o topo.";
        default: return "Canal valvulado: predominam os pares (2º harmônico), quente e macio; o agudo cai mais cedo.";
    }
}

inline juce::String dbText (float v)
{
    return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
}

inline juce::String noiseText (float pct)
{
    return pct < 0.5f ? juce::String ("Desligado") : juce::String (juce::roundToInt (noiseDbfs (pct * 0.01f))) + " dBFS";
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
    auto db  = [] (float v, int) { return dbText (v); };

    StringArray models;
    for (auto* m : modelNames) models.add (m);

    add (id::drive,     "Drive",       { 0.0f, 100.0f }, 30.0f, pct);
    choice (id::model,  "Modelo",      models, 1);
    add (id::tone,      "Tom",         { -6.0f, 6.0f }, 0.0f, db);
    add (id::crosstalk, "Crosstalk",   { 0.0f, 100.0f }, 10.0f, pct);
    add (id::noise,     u8 ("Ruído"),  { 0.0f, 100.0f }, 0.0f, [] (float v, int) { return noiseText (v); });
    add (id::output,    "Ganho",       { -36.0f, 12.0f }, 0.0f, db);
    add (id::mix,       "Mix",         { 0.0f, 100.0f }, 100.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::autoGain, 1 }, u8 ("Compensação automática"), true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::os4, 1 }, u8 ("Oversampling 4×"), true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::compare, 1 }, "Comparar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

// MODELO guarda o índice: 0 = Cristal, 1 = Ferro, 2 = Brasa. AUTO ligado é o padrão.
inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Console"),          { } },
        { "Canal Limpo",                  { { id::model, 0 }, { id::drive, 20 }, { id::crosstalk, 5 } } },
        { "Soma de Mesa",                 { { id::model, 1 }, { id::drive, 30 }, { id::crosstalk, 25 }, { id::noise, 10 } } },
        { "Bumbo com Ferro",              { { id::model, 1 }, { id::drive, 55 }, { id::tone, -1.0f }, { id::crosstalk, 0 } } },
        { "Caixa Estalada",               { { id::model, 1 }, { id::drive, 60 }, { id::tone, 1.5f }, { id::crosstalk, 0 } } },
        { "Baixo Valvulado",              { { id::model, 2 }, { id::drive, 50 }, { id::tone, -1.5f }, { id::crosstalk, 0 } } },
        { "Vocal Aveludado",              { { id::model, 2 }, { id::drive, 35 }, { id::tone, -1.0f } } },
        { "Vocal Brilhante",              { { id::model, 0 }, { id::drive, 30 }, { id::tone, 2.0f } } },
        { "Master Cola",                  { { id::model, 0 }, { id::drive, 25 }, { id::crosstalk, 15 } } },
        { "Barramento de Bateria",        { { id::model, 1 }, { id::drive, 45 }, { id::crosstalk, 20 } } },
        { "Overhead Aberto",              { { id::model, 0 }, { id::drive, 25 }, { id::tone, 2.5f } } },
        { u8 ("Válvula Quente"),          { { id::model, 2 }, { id::drive, 60 }, { id::tone, -2.0f } } },
        { "Synth Encorpado",              { { id::model, 1 }, { id::drive, 50 }, { id::crosstalk, 15 } } },
        { "Pad Antigo",                   { { id::model, 2 }, { id::drive, 45 }, { id::tone, -3.0f }, { id::noise, 30 } } },
        { "Guitarra Crocante",            { { id::model, 1 }, { id::drive, 75 }, { id::tone, 1.0f }, { id::crosstalk, 0 } } },
        { "Mesa dos Anos 70",             { { id::model, 1 }, { id::drive, 40 }, { id::tone, -1.5f }, { id::crosstalk, 30 }, { id::noise, 25 } } },
        { u8 ("Saturação Paralela"),      { { id::model, 2 }, { id::drive, 85 }, { id::mix, 40 } } },
        { "Grave Redondo",                { { id::model, 2 }, { id::drive, 40 }, { id::tone, -2.5f }, { id::crosstalk, 0 } } },
        { "Hi-hat Sedoso",                { { id::model, 0 }, { id::drive, 35 }, { id::tone, -1.5f }, { id::crosstalk, 0 } } },
        { "Sujeira Total",                { { id::model, 1 }, { id::drive, 100 }, { id::crosstalk, 40 }, { id::noise, 40 } } },
    };
}
} // namespace console
