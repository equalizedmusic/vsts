#pragma once
// MAPA — toca a prévia: o áudio da referência + um sintetizador simples que toca as notas MIDI achadas
// (para ouvir se o kick/baixo/hats/lead batem com a música). Mudo/solo por faixa.
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "Projeto.h"

namespace mapa
{
class Tocador : private juce::AudioSource
{
public:
    Tocador();
    ~Tocador() override;

    // troca a música (buffer estéreo na taxa original) e o projeto (notas). Chamar na thread da mensagem.
    void carregar (const juce::AudioBuffer<float>& audio, double taxa, const Projeto& projeto);
    void descarregar();

    void tocar();
    void parar();
    bool tocando() const;
    // posição no PROJETO, em tempos
    double posicaoTempos() const;
    void irPara (double tempos);
    // trecho em loop (tempos); fim <= inicio = sem loop
    void definirLoop (double inicio, double fim);
    double loopInicio() const { return loopA.load(); }
    double loopFim() const { return loopB.load(); }

    // estado por faixa (índice em projeto.faixas)
    void definirMudo (int faixa, bool mudo);
    void definirSolo (int faixa, bool solo);
    bool mudo (int faixa) const;
    bool solo (int faixa) const;

    juce::String erroAudio() const { return erro; }

private:
    void prepareToPlay (int amostrasPorBloco, double taxa) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

    float sintetiza (Elemento el, const Nota& n, double tNota) const;

    juce::AudioDeviceManager dispositivo;
    juce::AudioSourcePlayer player;

    juce::CriticalSection trava;
    juce::AudioBuffer<float> ref;      // estéreo, taxa original
    double taxaRef = 44100, taxaSaida = 44100;
    Projeto proj;
    std::vector<std::vector<const Nota*>> notasPorFaixa;   // ordenadas por início

    std::atomic<bool> rodando { false };
    std::atomic<double> posTempos { 0 };   // posição atual no projeto (tempos)
    std::atomic<double> loopA { 0 }, loopB { 0 };
    std::array<std::atomic<bool>, 16> mudos, solos;
    juce::String erro;
};
} // namespace mapa
