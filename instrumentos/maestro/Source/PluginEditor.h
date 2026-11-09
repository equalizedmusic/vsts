#pragma once

#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"
#include <functional>

// Tela do MAESTRO (base 1280 × 800, escala junto com a janela), esboço v3 aprovado, paleta do ALIGN / BAREBASS.
// CONSOLE CENTRAL: qualquer janela de qualquer cópia mostra e edita os 8 grupos (KICK, BASS, DRUMS, HATS, SYNTHS,
// AMBIENCE, VOCAL, FX). Os parâmetros de cada grupo (ROLE, TRIM, FOCUS) são os das cópias daquele grupo: a janela
// mexe neles pelo dono (MaestroProcessor) de cada vaga do Registry, na thread da interface.
//  - topo: STYLE, LINKED n / 8 GROUPS, THIS WINDOW (grupo desta cópia) e o estado;
//  - esquerda: 8 tiras (fonte do nome, nome da faixa, medidor com o alvo do STYLE, GAIN, CUT NOW + causa, TRIM,
//    FOCUS, ROLE, faixa colorida). Clique numa tira = o grupo que o mapa e a EQ destacam;
//  - CONFLICT MAP: uma pista por grupo com o espectro em BRANCO; onde ele briga com outro grupo, VERMELHO;
//    faixa CLASH no topo, trecho de Hz da briga à direita, ▼ = corte agora, SHELF = LOW-END GUARD;
//  - DYNAMIC EQ do grupo escolhido: soma da mix (cinza), espectro do grupo, FOCUS dos outros (tracejado), TONE MATCH,
//    LIVE EQ, os 9 nós com o alcance até MAX CUT, o shelf do GUARD e o histórico dos cortes (8 s);
//  - embaixo: LEARN / LOCK, WHOLE MIX (AMOUNT, MAX CUT, SPEED, GUARD, KEEP ON TOP), A/B e RESET ALL.
class MaestroEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MaestroEditor (MaestroProcessor&);
    ~MaestroEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseW = 1280, baseH = 800;

private:
    isp::TelaAtivacao ativacao;
    // uma faixa ligada (cópia do Registry para este quadro)
    struct Faixa
    {
        int vaga = -1, ordem = 0, role = 0, fonte = 0;
        bool focus = false, tocando = false;
        juce::String nome;
        float tela[maestro::NB] {};
        float corte[maestro::NE] {}, tom[maestro::NE] {};
        float ganhoDb = 0, nivelDb = -200, shelf = 0.0f, alvoDb = -200.0f;
        int mascaraFoco = 0;
        std::vector<int> cede;                     // vagas das FOCUS para as quais cede
        void* dono = nullptr;
    };
    // um grupo (ROLE) neste quadro
    struct Grupo
    {
        std::vector<const Faixa*> faixas;
        const Faixa* rep = nullptr;                // a desta janela, se for do grupo; senão a mais alta
        double tela[maestro::NB] {};               // soma das faixas
        float corte[maestro::NE] {};               // pior corte entre as faixas
        float shelf = 0.0f, ganhoDb = 0.0f, alvoDb = -200.0f, nivelDb = -200.0f;
        bool focus = false, tocando = false;
        std::vector<int> cedeGrupos;               // grupos em FOCUS para os quais cede
    };
    struct Knob
    {
        juce::String rotulo;
        juce::Point<float> c;
        float r = 22;
        juce::Colour cor;
        std::function<float()> le;                 // 0..1
        std::function<void (float)> grava;         // 0..1
        std::function<juce::String()> texto;
        std::function<void (bool)> gesto;          // início / fim (parâmetro do DAW)
        std::function<bool()> ativo;               // sem isso, sempre
        float padrao = 0.5f;
        int passos = 0;                            // 0 = contínuo
        bool rotuloEmbaixo = false;
    };
    struct Clique { juce::Rectangle<float> r; std::function<void()> faz; };

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }
    void colhe();                                  // lê o Registry e monta os grupos
    void guardaHistorico();

    void topo (juce::Graphics&);
    void console (juce::Graphics&, juce::Rectangle<float> r);
    void tira (juce::Graphics&, juce::Rectangle<float> r, int grupo);
    void mapa (juce::Graphics&, juce::Rectangle<float> r);
    void painelEq (juce::Graphics&, juce::Rectangle<float> r);
    void historico (juce::Graphics&, juce::Rectangle<float> r, int grupo);
    void base (juce::Graphics&);
    void desenhaKnob (juce::Graphics&, Knob&);
    void chave (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, bool on, std::function<void()> faz);

    void menuStyle();
    void menuRoleDoGrupo (int grupo);
    void menuLogo();
    void confirmaReset();
    void focusDoGrupo (int grupo);

    std::vector<MaestroProcessor*> donosDoGrupo (int grupo) const;
    const Faixa* minha() const;
    int meuGrupo() const;
    void desenhaTextoEsp (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float tam, juce::Colour,
                          juce::Justification, float kern = 0.18f, bool negrito = false);

    MaestroProcessor& proc;
    std::vector<Faixa> faixas;
    Grupo grupos[maestro::NR];
    double pico = -200.0;                          // pico do mapa (dB), segura e cai devagar
    double telaHold[maestro::NR][maestro::NB] {};  // transporte parado: o mapa segura o último espectro
    int sel = -1;                                  // grupo escolhido (−1 = o desta janela)

    static constexpr int HN = 240;                 // histórico: 8 s a 30 quadros/s
    float hist[maestro::NR][HN][maestro::NE + 1] {};
    int histPos = 0;

    std::vector<Knob> knobs;
    std::vector<Clique> cliques;
    int arrastando = -1;
    float valorIni = 0, yIni = 0;
    int tique = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MaestroEditor)
};
