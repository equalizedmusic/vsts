#pragma once

#include "PluginProcessor.h"
#include <functional>

// Tela do MAESTRO (base 1120 × 700, escala junto com a janela). Tudo desenhado no paint, a 30 Hz, como no esboço v2:
//  - topo: STYLE (do projeto), TRACK, ROLE (desta faixa), LINKED n TRACKS e o estado;
//  - CONFLICT MAP · WHOLE PROJECT: linhas = ROLEs, colunas = 27 bandas de 1/3 de oitava. Cor = frequência (graves
//    vermelho/laranja, médios amarelo, presença branco, agudos azul, como a forma de onda colorida da CDJ-3000X);
//    brilho = conflito. Com FOCUS: quem tem FOCUS em cor cheia, quem cede apagado com o tracinho branco do corte.
//    Clique no nome do ROLE: lista as faixas dele para pôr / tirar FOCUS de qualquer uma, daqui mesmo.
//  - THIS TRACK · TONE + DYNAMIC EQ: espectro da faixa (cores por temperatura), TONE MATCH (branco), corte ao vivo
//    (magenta), espectro de quem tem FOCUS (tracejado) e o GUARD;
//  - coluna da direita conforme o estado (START, LEARNING, LOCKED, FOCUS, YIELDING);
//  - embaixo: LEARN / LOCK / FOCUS, AMOUNT, MAX CUT, SPEED, TRIM, LOW-END GUARD, KEEP KICK / BASS ON TOP, A/B e RESET ALL.
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

    static constexpr int baseW = 1120, baseH = 700;

private:
    // uma faixa ligada (cópia do Registry para este quadro)
    struct Faixa
    {
        int vaga = -1, ordem = 0, role = 0;
        bool focus = false, roleAuto = false, rolePeloSom = false, tocando = false;
        juce::String nome;
        float tela[maestro::NB] {};
        float corte[maestro::NE] {}, tom[maestro::NE] {};
        float ganhoDb = 0, nivelDb = -200;
        int cedePara = -1, mascaraFoco = 0;
        float shelf = 0.0f;                        // LOW-END GUARD agora (dB)
        std::vector<int> cede;                     // vagas das FOCUS para as quais cede
        void* dono = nullptr;
    };
    // um botão-giratório (valor normalizado 0..1)
    struct Knob
    {
        juce::String rotulo;
        juce::Point<float> c;
        float r = 22;
        std::function<float()> le;                 // 0..1
        std::function<void (float)> grava;         // 0..1
        std::function<juce::String()> texto;
        std::function<void (bool)> gesto;          // início / fim (parâmetro do DAW)
        float padrao = 0.5f;
        int passos = 0;                            // 0 = contínuo
    };
    struct Clique { juce::Rectangle<float> r; std::function<void()> faz; };

    void timerCallback() override;
    float escala() const { return (float) getWidth() / (float) baseW; }
    void colhe();                                  // lê o Registry

    void topo (juce::Graphics&);
    void mapa (juce::Graphics&, juce::Rectangle<float> r);
    void painelEq (juce::Graphics&, juce::Rectangle<float> r);
    void direita (juce::Graphics&, juce::Rectangle<float> r);
    void base (juce::Graphics&);
    void desenhaKnob (juce::Graphics&, Knob&);
    void chave (juce::Graphics&, juce::Rectangle<float> r, const juce::String& t, bool on, std::function<void()> faz);

    void menuStyle();
    void menuRole();
    void menuFaixasDoRole (int role);
    void menuLogo();
    void confirmaReset();

    juce::String nomeDe (const Faixa& f) const;
    const Faixa* minha() const;
    bool algumFocus() const;
    void desenhaTextoEsp (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float tam, juce::Colour,
                          juce::Justification, float kern = 0.18f, bool negrito = false);

    MaestroProcessor& proc;
    std::vector<Faixa> faixas;
    float mapaHold[maestro::NR][maestro::NB] {};   // o mapa segura quando o transporte para

    std::vector<Knob> knobs;
    std::vector<Clique> cliques;
    int arrastando = -1;
    float valorIni = 0, yIni = 0;
    int tique = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MaestroEditor)
};
