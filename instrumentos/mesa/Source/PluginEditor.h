#pragma once

#include "PluginProcessor.h"
#include "IspTelaAtivacao.h"
#include <map>
#include <string>
#include <vector>

// As duas janelas da MESA, desenhadas em vetor (Desenho.h) sobre uma tela base em px que escala com a janela:
//  - ConsoleEditor (plugin MESA, no master): o mixer inteiro — barra lateral, visor do canal selecionado, todos os
//    CANAIS lado a lado (rola na horizontal; GERAL encolhe para caber todos) e a seção central com o medidor geral,
//    o goniômetro, os VUs de redução de ganho, o COLA BUS do master e os faders dos grupos e do master.
//  - CanalEditor (plugin MESA CANAL): o próprio canal inteiro, do pré ao fader, e ao lado a ligação com a console,
//    o número/nome/grupo, três VUs (entrada, redução, saída) e o caráter (modelos de PRÉ e EQ, ordem).
// O que não mexe fica numa imagem guardada (refeita quando algum parâmetro muda); por cima, a 30 Hz, só medidores,
// agulhas e o goniômetro.
class MesaTela : public juce::AudioProcessorEditor, protected juce::Timer
{
public:
    MesaTela (MesaProcessor&, int baseW, int baseH);
    ~MesaTela() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // um controle na tela (coordenadas da tela base)
    struct Alvo
    {
        enum Tipo { knob, liga, ciclo, escolhe, fader, nome, acao, seleciona, posicao };
        Tipo tipo = knob;
        juce::Rectangle<float> r;
        mesa::Canal* c = nullptr;
        std::string pid;
        int valor = 0, opcoes = 2;      // escolhe: qual; ciclo/escolhe: quantas opções
        float travel = 100.0f;          // fader: curso (px base)
        int codigo = 0;                 // acao: qual
    };
    // medidores suavizados de um canal
    struct Med { float in = -60, out = -60, outL = -60, outR = -60, gr = 0, vuIn = -30, vuOut = -30, vuGr = 0; };

protected:
    virtual void pintaFixa (juce::Graphics&) = 0;          // tela base, coordenadas base
    virtual void pintaViva (juce::Graphics&) = 0;
    virtual juce::AffineTransform base() const = 0;         // tela base -> janela
    virtual void acao (int codigo, const juce::MouseEvent&) { juce::ignoreUnused (codigo); }
    virtual void selecionou (mesa::Canal*) {}
    virtual void assinatura (std::vector<float>& s) = 0;    // valores que, se mudarem, refazem a imagem guardada
    virtual void mede() = 0;                                 // atualiza os medidores (timer)
    virtual bool existe (mesa::Canal* c) { return c == static_cast<mesa::Canal*> (&proc); }   // o canal ainda está aberto?

    void timerCallback() override;
    void mudou() { fixaSuja = true; repaint(); }
    juce::Point<float> paraBase (juce::Point<float> p) const { return p.transformedBy (base().inverted()); }
    const Alvo* acha (juce::Point<float> pBase) const;
    void atualizaMed (mesa::Canal* c, Med& m, bool ppm);
    void renomeia (mesa::Canal* c);
    void posiciona (mesa::Canal* c);
    void menu();

    // canal inteiro (x, y, altura) — fixa = a parte que não mexe; senão só medidores
    void pintaCanal (juce::Graphics& g, float x, float y, float h, mesa::Canal& c, bool sel, bool fixa, bool cores);

    MesaProcessor& proc;
    std::vector<Alvo> alvos;                // refeito junto com a imagem guardada
    std::map<mesa::Canal*, Med> meds;
    juce::Image fixaImg;
    bool fixaSuja = true;
    std::vector<float> ultima;
    bool temAtivo = false;
    Alvo ativo;                             // cópia: a lista de alvos é refeita durante o arrasto
    float arrastoIni = 0, valorIni = 0;
    juce::Point<float> mouseBase;
    bool mostraValor = false;
    juce::String valorTexto;
    juce::Point<float> valorPos;
    isp::TelaAtivacao ativacao;
    const int baseW, baseH;
};

class ConsoleEditor : public MesaTela
{
public:
    explicit ConsoleEditor (MesaProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void pintaFixa (juce::Graphics&) override;
    void pintaViva (juce::Graphics&) override;
    juce::AffineTransform base() const override;
    void acao (int codigo, const juce::MouseEvent&) override;
    void selecionou (mesa::Canal* c) override;
    void assinatura (std::vector<float>& s) override;
    void mede() override;
    bool existe (mesa::Canal* c) override;

    void listaCanais();
    float escala() const;
    float larguraBase() const { return (float) getWidth() / escala(); }
    float alturaBase() const { return juce::jmax (1000.0f, (float) getHeight() / escala()); }   // nunca espreme o canal
    float rolaMax() const { return juce::jmax (0.0f, alturaBase() * escala() - (float) getHeight()); }
    void zoom (int passos);
    int cabem() const;
    void mostra (int indice);
    mesa::Canal* grupo (int g) const;       // G1..G3 (0..2)
    void pintaLateral (juce::Graphics&, float h);
    void pintaTopo (juce::Graphics&, float x, float w);
    void pintaRotulos (juce::Graphics&, float x, float y, float h);
    void pintaCentral (juce::Graphics&, float x, float w, bool fixa);

    std::vector<mesa::Canal*> canais;      // canais em ordem, depois os grupos
    int versaoLista = -1;
    int primeiro = 0;                       // primeiro canal à vista
    float rolaY = 0;                        // rolagem vertical (px da janela) quando o zoom não cabe na altura
    int selNum = -1;                        // número do canal selecionado
    int barra = 3;                          // seção central: 0..2 = G1..G3, 3 = master
    bool autoSel = true, cores = true, ppm = false;
    Med master;
    float picoMax = -120, rmsMax = -120, picoAgora = -120, rmsAgora = -120;
};

class CanalEditor : public MesaTela
{
public:
    explicit CanalEditor (MesaProcessor&);
    void resized() override;

private:
    void pintaFixa (juce::Graphics&) override;
    void pintaViva (juce::Graphics&) override;
    juce::AffineTransform base() const override;
    void assinatura (std::vector<float>& s) override;
    void mede() override;
};
