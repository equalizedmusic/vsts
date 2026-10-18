#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <utility>
#include <vector>

namespace grao
{
// texto com acento: o JUCE só aceita ASCII em const char*
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace id
{
    inline constexpr const char* grain    = "grain";
    inline constexpr const char* space    = "space";
    inline constexpr const char* size     = "size";
    inline constexpr const char* density  = "density";
    inline constexpr const char* spray    = "spray";
    inline constexpr const char* pitch    = "pitch";
    inline constexpr const char* spread   = "spread";
    inline constexpr const char* feedback = "feedback";
    inline constexpr const char* decay    = "decay";
    inline constexpr const char* tone     = "tone";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* reverse  = "reverse";
    inline constexpr const char* freeze   = "freeze";
    inline constexpr const char* bypass   = "bypass";
}

// TOM: um botão só; para a esquerda fecha o passa-baixa, para a direita abre o passa-alta
inline float toneLowpassHz (float t)  { return t < 0.0f ? 20000.0f * std::pow (500.0f / 20000.0f, -t) : 20000.0f; }
inline float toneHighpassHz (float t) { return t > 0.0f ? 20.0f * std::pow (2000.0f / 20.0f, t) : 20.0f; }

inline juce::String hzText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (hz));
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };
    auto add = [&layout] (const char* pid, const String& name, NormalisableRange<float> range, float def,
                          std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };

    add (id::grain,    u8 ("Grão"),       { 0.0f, 100.0f }, 55.0f, pct);
    add (id::space,    u8 ("Espaço"),     { 0.0f, 100.0f }, 40.0f, pct);
    add (id::size,     "Tamanho",         skewed (10.0f, 500.0f, 120.0f), 180.0f,
         [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    add (id::density,  "Densidade",       skewed (1.0f, 100.0f, 20.0f), 25.0f,
         [] (float v, int) { return String (roundToInt (v)) + " /s"; });
    add (id::spray,    u8 ("Dispersão"),  skewed (0.0f, 1000.0f, 200.0f), 300.0f,
         [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    add (id::pitch,    u8 ("Afinação"),   { -24.0f, 24.0f, 1.0f }, 0.0f,
         [] (float v, int) { auto s = roundToInt (v); return (s > 0 ? "+" : "") + String (s) + " st"; });
    add (id::spread,   u8 ("Estéreo"),    { 0.0f, 100.0f }, 70.0f, pct);
    add (id::feedback, "Feedback",        { 0.0f, 95.0f }, 25.0f, pct);
    add (id::decay,    "Decaimento",      skewed (0.2f, 10.0f, 2.0f), 3.0f,
         [] (float v, int) { return String (v, 1) + " s"; });
    add (id::tone,     "Tom",             { -1.0f, 1.0f }, 0.0f,
         [] (float v, int)
         {
             if (std::abs (v) < 0.02f) return String ("ABERTO");
             return v < 0.0f ? "LP " + hzText (toneLowpassHz (v)) : "HP " + hzText (toneHighpassHz (v));
         });
    add (id::output,   "Ganho",           { -36.0f, 12.0f }, 0.0f,
         [] (float v, int) { return String (v, 1) + " dB"; });
    add (id::mix,      "Mix",             { 0.0f, 100.0f }, 50.0f, pct);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::reverse, 1 }, "Reverso", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::freeze, 1 }, "Congelar", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // valores reais; o que faltar volta ao padrão
};

inline std::vector<FactoryPreset> factoryPresets()
{
    return {
        { u8 ("Init · Areia"),   { { id::grain, 55 }, { id::space, 40 }, { id::size, 180 }, { id::density, 25 }, { id::spray, 300 },
                                   { id::pitch, 0 }, { id::spread, 70 }, { id::feedback, 25 }, { id::decay, 3 }, { id::mix, 50 } } },
        { "Poeira Dourada",      { { id::grain, 35 }, { id::space, 70 }, { id::size, 320 }, { id::density, 18 }, { id::spray, 600 },
                                   { id::pitch, 12 }, { id::spread, 90 }, { id::feedback, 30 }, { id::decay, 6 }, { id::tone, -0.2f }, { id::mix, 45 } } },
        { "Enxame Reverso",      { { id::grain, 80 }, { id::space, 45 }, { id::size, 70 }, { id::density, 70 }, { id::spray, 400 },
                                   { id::spread, 100 }, { id::feedback, 20 }, { id::decay, 2 }, { id::reverse, 1 }, { id::mix, 60 } } },
        { "Coral Congelado",     { { id::grain, 40 }, { id::space, 85 }, { id::size, 450 }, { id::density, 12 }, { id::spray, 900 },
                                   { id::pitch, -12 }, { id::spread, 80 }, { id::feedback, 45 }, { id::decay, 9 }, { id::tone, -0.4f }, { id::mix, 65 } } },
        { "Fita Rasgada",        { { id::grain, 90 }, { id::space, 25 }, { id::size, 30 }, { id::density, 90 }, { id::spray, 150 },
                                   { id::spread, 50 }, { id::feedback, 60 }, { id::decay, 1 }, { id::tone, 0.3f }, { id::mix, 55 } } },
        { "Garoa",               { { id::grain, 60 }, { id::space, 60 }, { id::size, 40 }, { id::density, 8 }, { id::spray, 1000 },
                                   { id::pitch, 7 }, { id::spread, 100 }, { id::feedback, 10 }, { id::decay, 4 }, { id::tone, 0.15f }, { id::mix, 50 } } },
        { "Neblina",             { { id::grain, 25 }, { id::space, 80 }, { id::size, 400 }, { id::density, 10 }, { id::spray, 800 }, { id::spread, 85 },
                                   { id::feedback, 35 }, { id::decay, 7 }, { id::tone, -0.35f }, { id::mix, 55 } } },
        { "Vidro Quebrado",      { { id::grain, 85 }, { id::space, 30 }, { id::size, 25 }, { id::density, 80 }, { id::spray, 120 }, { id::pitch, 12 },
                                   { id::spread, 70 }, { id::feedback, 15 }, { id::decay, 1.2f }, { id::tone, 0.4f }, { id::mix, 50 } } },
        { "Oitava Abaixo",       { { id::grain, 45 }, { id::space, 50 }, { id::size, 220 }, { id::density, 22 }, { id::spray, 250 }, { id::pitch, -12 },
                                   { id::spread, 60 }, { id::feedback, 20 }, { id::decay, 3 }, { id::tone, -0.15f }, { id::mix, 50 } } },
        { "Quinta Brilhante",    { { id::grain, 50 }, { id::space, 55 }, { id::size, 150 }, { id::density, 30 }, { id::spray, 350 }, { id::pitch, 7 },
                                   { id::spread, 80 }, { id::feedback, 30 }, { id::decay, 4 }, { id::tone, 0.2f }, { id::mix, 45 } } },
        { "Nuvem de Estrelas",   { { id::grain, 60 }, { id::space, 90 }, { id::size, 260 }, { id::density, 40 }, { id::spray, 1000 }, { id::pitch, 24 },
                                   { id::spread, 100 }, { id::feedback, 40 }, { id::decay, 8 }, { id::tone, 0.25f }, { id::mix, 40 } } },
        { "Gagueira",            { { id::grain, 95 }, { id::space, 10 }, { id::size, 15 }, { id::density, 100 }, { id::spray, 30 }, { id::spread, 30 },
                                   { id::feedback, 5 }, { id::decay, 0.5f }, { id::mix, 60 } } },
        { u8 ("Maré Lenta"),     { { id::grain, 20 }, { id::space, 70 }, { id::size, 500 }, { id::density, 4 }, { id::spray, 700 }, { id::pitch, -5 },
                                   { id::spread, 90 }, { id::feedback, 55 }, { id::decay, 9 }, { id::tone, -0.5f }, { id::mix, 60 } } },
        { u8 ("Rádio Antigo"),   { { id::grain, 65 }, { id::space, 20 }, { id::size, 90 }, { id::density, 35 }, { id::spray, 200 }, { id::spread, 20 },
                                   { id::feedback, 25 }, { id::decay, 1 }, { id::tone, 0.6f }, { id::mix, 70 } } },
        { "Abismo",              { { id::grain, 50 }, { id::space, 95 }, { id::size, 350 }, { id::density, 15 }, { id::spray, 900 }, { id::pitch, -24 },
                                   { id::spread, 100 }, { id::feedback, 60 }, { id::decay, 10 }, { id::tone, -0.55f }, { id::mix, 55 } } },
        { "Cristais Reversos",   { { id::grain, 55 }, { id::space, 65 }, { id::size, 200 }, { id::density, 28 }, { id::spray, 500 }, { id::pitch, 12 },
                                   { id::spread, 95 }, { id::feedback, 35 }, { id::decay, 5 }, { id::tone, 0.1f }, { id::reverse, 1 }, { id::mix, 50 } } },
        { "Pulso Sutil",         { { id::grain, 40 }, { id::space, 25 }, { id::size, 120 }, { id::density, 15 }, { id::spray, 100 }, { id::spread, 40 },
                                   { id::feedback, 10 }, { id::decay, 1.5f }, { id::mix, 25 } } },
        { "Tempestade de Areia", { { id::grain, 100 }, { id::space, 60 }, { id::size, 20 }, { id::density, 100 }, { id::spray, 1000 }, { id::spread, 100 },
                                   { id::feedback, 50 }, { id::decay, 3 }, { id::tone, 0.2f }, { id::mix, 55 } } },
        { "Eco Granular",        { { id::grain, 30 }, { id::space, 50 }, { id::size, 300 }, { id::density, 6 }, { id::spray, 50 }, { id::spread, 70 },
                                   { id::feedback, 70 }, { id::decay, 2 }, { id::mix, 45 } } },
        { u8 ("Sonho Lúcido"),   { { id::grain, 45 }, { id::space, 80 }, { id::size, 280 }, { id::density, 20 }, { id::spray, 600 }, { id::pitch, 5 },
                                   { id::spread, 85 }, { id::feedback, 45 }, { id::decay, 7.5f }, { id::tone, -0.25f }, { id::reverse, 1 }, { id::mix, 60 } } },
    };
}
} // namespace grao
