#pragma once
// MAPA — exportadores: transformam o Projeto (Projeto.h) em arquivos que abrem na DAW do usuário.
//
// Regras comuns a todos:
//  - tudo é gravado dentro de `pasta` (criada aqui se não existir);
//  - o áudio de referência é COPIADO para pasta/"Audio"/<nome original> e os projetos apontam para essa cópia
//    (caminho relativo quando o formato aceita);
//  - comNotas = false: os clipes MIDI saem VAZIOS (só nome, cor e posição) — versão grátis;
//    comNotas = true: clipes com as notas.
#include <juce_core/juce_core.h>
#include "Projeto.h"

namespace mapa
{
struct ResultadoExport
{
    bool ok = false;
    juce::File arquivo;   // arquivo principal gerado (.als, .rpp, .dawproject ou a pasta MIDI)
    juce::String erro;    // mensagem em PT-BR quando ok == false
};

// MIDI/ (um .mid por faixa + 00 ESTRUTURA.mid) + Audio/ + LEIA-ME.txt — para FL Studio, Logic, Pro Tools etc.
ResultadoExport exportarPasta      (const Projeto&, const juce::File& pasta, bool comNotas);
// <nome>.als — Ableton Live 12 (a partir do modelo embutido Recursos/modelo.als)
ResultadoExport exportarAbleton    (const Projeto&, const juce::File& pasta, bool comNotas);
// <nome>.rpp — Reaper
ResultadoExport exportarReaper     (const Projeto&, const juce::File& pasta, bool comNotas);
// <nome>.dawproject — Bitwig, Studio One, Cubase 14 (zip com project.xml, metadata.xml e audio/)
ResultadoExport exportarDawproject (const Projeto&, const juce::File& pasta, bool comNotas);

// Nome seguro para arquivo/pasta: troca / \ : * ? " < > | e caracteres de controle por '_',
// tira pontos/espaços do fim (Windows) e nunca devolve vazio.
juce::String nomeSeguro (const std::string& nome);
} // namespace mapa
