#pragma once
// MAPA — DAWs instaladas no computador e como abrir o projeto em cada uma.
#include <JuceHeader.h>

namespace mapa
{
enum class Formato { ableton, reaper, dawproject, pasta };

struct Daw
{
    juce::String nome;       // "Ableton Live 12 Suite"
    juce::String detalhe;    // "projeto .als completo"
    Formato formato = Formato::pasta;
    juce::File app;          // .app (Mac) ou .exe (Windows); vazio = "só salvar a pasta"
    bool encontrada = false;
};

// procura as DAWs instaladas (Mac: /Applications e ~/Applications; Windows: Arquivos de Programas e ProgramData)
juce::Array<Daw> procurarDaws();

// abre o arquivo (projeto ou pasta) na DAW. Formato pasta: abre a pasta no Finder/Explorer e a DAW.
bool abrirNaDaw (const Daw& daw, const juce::File& projeto, juce::String& erro);

juce::String rotuloFormato (Formato f);   // "COMPLETO", "DAWPROJECT", "ARRASTAR"
} // namespace mapa
