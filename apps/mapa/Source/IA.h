#pragma once
// MAPA — motores de IA (C++17 + ONNX Runtime, sem JUCE: também compila no teste de linha de comando).
//  - Separador: Demucs v4 (htdemucs, 4 fontes) — separa a música em BATERIA, BAIXO, OUTROS e VOZ (44,1 kHz estéreo).
//  - Transcritor: Basic Pitch (Spotify, Apache 2.0) — transforma um stem (22,05 kHz mono) em notas MIDI.
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mapa
{
using FnProgresso = std::function<void (float)>;   // 0..1

// ordem de saída do htdemucs
enum Stem { stemBateria = 0, stemBaixo = 1, stemOutros = 2, stemVoz = 3, numStems = 4 };

struct Stems
{
    double sampleRate = 44100;
    std::array<std::array<std::vector<float>, 2>, numStems> canal;   // [stem][L/R]
    size_t amostras() const { return canal[0][0].size(); }
    bool vazio() const { return canal[0][0].empty(); }
};

class Separador
{
public:
    Separador();
    ~Separador();
    // caminho do htdemucs.onnx (UTF-8). threads <= 0: todos os núcleos.
    bool carregar (const std::string& caminhoModelo, std::string& erro, int threads = 0);
    bool carregado() const;
    // L/R a 44.1 kHz, mesmo tamanho. Volta false se cancelado ou erro.
    bool separar (const std::vector<float>& L, const std::vector<float>& R, Stems& saida,
                  FnProgresso progresso, const std::atomic<bool>* cancelar, std::string& erro);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

struct NotaAudio
{
    double ini = 0, fim = 0;   // segundos
    int tom = 60;              // MIDI
    float amp = 0.5f;          // 0..1
};

struct ParametrosNotas
{
    float limiarAtaque = 0.5f;     // onset_threshold
    float limiarNota = 0.3f;       // frame_threshold
    double notaMinimaMs = 127.7;   // minimum_note_length
    double fMin = 0, fMax = 0;     // Hz (0 = sem limite)
    bool melodia = true;           // "melodia trick" do Basic Pitch
};

class Transcritor
{
public:
    Transcritor();
    ~Transcritor();
    bool carregar (const void* modelo, size_t bytes, std::string& erro, int threads = 0);
    bool carregado() const;
    // mono a 22.050 Hz
    bool transcrever (const std::vector<float>& mono22k, const ParametrosNotas& par, std::vector<NotaAudio>& saida,
                      FnProgresso progresso, const std::atomic<bool>* cancelar, std::string& erro);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Utilitários de áudio (sem JUCE)
// mono 44.1 kHz (ou qualquer taxa par) -> metade da taxa, com passa-baixa (FIR janelado de 95 coeficientes)
std::vector<float> metadeDaTaxa (const std::vector<float>& x);
std::vector<float> somaMono (const std::vector<float>& L, const std::vector<float>& R);
} // namespace mapa
