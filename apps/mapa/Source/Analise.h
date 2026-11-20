#pragma once
// MAPA — análise da música de referência. C++17 puro (sem JUCE): o app chama numa thread de fundo e o teste de linha
// de comando (tools/teste_analise.cpp) chama direto.
#include "Projeto.h"
#include "IA.h"
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

    // MAPA 0.2 — preenchido por processarComIA (vazio = análise só pelo espectro, como na 0.1)
    Stems stems;                                     // 44,1 kHz estéreo
    std::vector<NotaAudio> notasBaixo, notasVoz, notasOutros;
    std::string erroIA;
};

// Separa a música (Demucs) e transcreve baixo, voz e "outros" (Basic Pitch). L/R a 44,1 kHz.
// progresso vai de 0 a 1 só nesta etapa. Grava em `in` (stems, notas*, erroIA). Volta false se falhou/cancelou.
bool processarComIA (const std::vector<float>& L, const std::vector<float>& R,
                     const std::string& caminhoDemucs, const void* modeloNotas, size_t bytesModeloNotas,
                     EntradaAnalise& in, std::function<void (float, const char*)> progresso,
                     const std::atomic<bool>* cancelar);

// progresso: 0..1 e o nome da etapa. cancelar: se virar true a função volta cedo (retorno false).
bool analisar (const EntradaAnalise& in, Projeto& out,
               std::function<void (float, const char*)> progresso = {},
               const std::atomic<bool>* cancelar = nullptr);

// nome da nota (0 = C) com sustenido
const char* nomeNota (int pc);
} // namespace mapa
