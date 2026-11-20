#pragma once
// MAPA — modelo do projeto que a análise produz e os exportadores (Ableton, Reaper, DAWproject, pasta MIDI) gravam.
// Sem JUCE aqui: a análise (Analise.h) também compila fora do app, no teste de linha de comando.
#include <cstdint>
#include <string>
#include <vector>

namespace mapa
{
// Tempo SEMPRE em tempos (beats, semínima) contados do início do projeto (compasso 1 = tempo 0).
struct Nota
{
    double inicio = 0;   // tempos, absoluto no projeto
    double dur    = 0.25;
    int tom       = 60;  // MIDI
    int vel       = 100;
};

struct Clipe
{
    double inicio = 0, fim = 0;   // tempos, absoluto no projeto
    std::vector<Nota> notas;      // dentro de [inicio, fim)
};

enum class TipoFaixa { audio, midi };

enum class Elemento { referencia, kick, baixo, hats, clap, lead, pad, fx, voz, stem };

struct Faixa
{
    std::string nome;    // "KICK"
    std::string desc;    // "MIDI · 1 por tempo"
    uint32_t cor = 0xffffff;  // 0xRRGGBB
    TipoFaixa tipo = TipoFaixa::midi;
    Elemento elem = Elemento::kick;
    bool bateria = false; // notas de bateria (GM: 36 kick, 39 clap, 42 hat)
    std::vector<Clipe> clipes;
    // faixas de ÁUDIO: stem >= 0 = stem separado pela IA (0 bateria, 1 baixo, 2 outros, 3 voz; ver IA.h);
    // arquivo = WAV do stem (UTF-8, gravado pelo app depois da análise). Referência: stem = -1, arquivo vazio.
    int stem = -1;
    std::string arquivo;
    bool mudo = false;    // começa mutada na DAW (os stems: a referência já toca a música inteira)
};

struct Parte
{
    std::string nome;          // "DROP"
    int compIni = 0, compFim = 0;  // compassos, 0 = compasso 1; fim exclusivo
    uint32_t cor = 0xffffff;
};

struct Projeto
{
    std::string nomeMusica;     // nome do arquivo sem extensão
    std::string caminhoAudio;   // caminho completo do arquivo de referência (UTF-8)
    double duracaoSeg = 0;
    double sampleRate = 44100;
    int canais = 2;

    double bpm = 120;
    int numerador = 4, denominador = 4;
    int tonica = 0;             // 0 = C ... 11 = B
    bool menor = true;
    std::string tomTexto;       // "F# m"

    // O áudio da referência começa em audioInicioTempos (>= 0) no projeto, de modo que o 1º tempo forte detectado
    // (em primeiroTempoSeg dentro do arquivo) caia exatamente numa barra de compasso.
    // Se o 1º tempo forte está logo no começo do arquivo (menos de 1/4 de tempo), em vez de empurrar o áudio quase um
    // compasso inteiro o clipe começa DENTRO do arquivo: pula audioCorteSeg segundos (o clipe começa no offset).
    double audioInicioTempos = 0;
    double audioCorteSeg = 0;
    double primeiroTempoSeg = 0;
    int totalCompassos = 0;

    std::vector<Parte> partes;
    std::vector<Faixa> faixas;  // faixas[0] = REFERÊNCIA (áudio, 1 clipe com o arquivo inteiro)
    bool comIA = false;         // análise com separação (stems) e transcrição de notas

    double temposPorCompasso() const { return numerador * 4.0 / denominador; }
    double segPorTempo() const { return 60.0 / bpm; }
    // posição no projeto (tempos) -> segundos dentro do arquivo de áudio
    double temposParaSegAudio (double t) const { return (t - audioInicioTempos) * segPorTempo() + audioCorteSeg; }
    // fim do áudio no projeto, em tempos
    double audioFimTempos() const { return audioInicioTempos + (duracaoSeg - audioCorteSeg) / segPorTempo(); }
};
} // namespace mapa
