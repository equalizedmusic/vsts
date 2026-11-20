#pragma once
// MAPA — análise da música de referência. C++17 puro (sem JUCE): o app chama numa thread de fundo e o teste de linha
// de comando (tools/teste_analise.cpp) chama direto.
#include "Projeto.h"
#include <atomic>
#include <functional>
#include <vector>

namespace mapa
{
struct EntradaAnalise
{
    std::vector<float> mono;   // áudio já somado em mono
    double sampleRate = 44100;
    int canais = 2;            // só informativo (vai para o Projeto)
    std::string nomeMusica, caminhoAudio;
};

// progresso: 0..1 e o nome da etapa. cancelar: se virar true a função volta cedo (retorno false).
bool analisar (const EntradaAnalise& in, Projeto& out,
               std::function<void (float, const char*)> progresso = {},
               const std::atomic<bool>* cancelar = nullptr);

// nome da nota (0 = C) com sustenido
const char* nomeNota (int pc);
} // namespace mapa
