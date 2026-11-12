#pragma once

// Tela de ativação por cima da janela do plugin. Uso no editor (membro declarado por ÚLTIMO):
//     isp::TelaAtivacao ativacao;
//     ... no fim do construtor:  ativacao.anexar (*this);
//     ... item "Licença…" de algum menu:  ativacao.mostrarInfo();
// Sem licença válida ela cobre tudo e pede e-mail + token; com licença ela some e só aparece no mostrarInfo().
// Com licença e versão nova do pacote publicada, mostra uma faixa no pé da janela (AvisoVersao) com BAIXAR.

#include "IspLicenca.h"

namespace isp
{
// faixa "versão nova" no pé da janela do plugin; a TelaAtivacao cuida de pôr e tirar
// Com ISP_ATUALIZAR e instalador assinado: ATUALIZAR (baixa, confere e instala sozinho) + BAIXAR (navegador) + Agora não.
class AvisoVersao : public juce::Component
                  #if ISP_ATUALIZAR
                  , private juce::ChangeListener
                  #endif
{
public:
    AvisoVersao()
    {
        for (auto* b : { &atualizar, &baixar, &depois })
        {
            addChildComponent (b);
            b->setColour (juce::TextButton::textColourOffId, texto);
        }
        atualizar.setColour (juce::TextButton::buttonColourId, destaque.darker (0.35f));
        baixar.setColour (juce::TextButton::buttonColourId, destaque.darker (0.35f));
        depois.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2d33));
        atualizar.setButtonText (utf8 ("ATUALIZAR"));
        baixar.setButtonText (utf8 ("BAIXAR"));
        depois.setButtonText (utf8 ("Agora não"));
        baixar.onClick = [this]
        {
            if (juce::URL (licenca->linkVersaoNova()).launchInDefaultBrowser())
            {
                baixou = true;
                arrumar();
            }
        };
        depois.onClick = [this] { licenca->dispensarAviso(); };
       #if ISP_ATUALIZAR
        atualizar.onClick = [this] { if (auto* a = licenca->atualizador()) a->iniciar(); };
        licenca->motorAtualizacao().addChangeListener (this);
       #endif
        arrumar();
    }

    ~AvisoVersao() override
    {
       #if ISP_ATUALIZAR
        licenca->motorAtualizacao().removeChangeListener (this);
       #endif
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xf21b1d22));
        g.fillRoundedRectangle (c, 8.0f);
        g.setColour (destaque);
        g.drawRoundedRectangle (c, 8.0f, 1.0f);

        auto r = getLocalBounds().reduced (14, 0).withRight (primeiroBotao() - 10);
        juce::String t;
        float prog = -1.0f;
        const auto nova = licenca->versaoNova();
       #if ISP_ATUALIZAR
        using E = Atualizador::Estado;
        const auto& a = licenca->motorAtualizacao();
        switch (a.estado())
        {
            case E::baixando:     t << utf8 ("Baixando a versão ") << a.versao() << utf8 ("…"); prog = a.progresso(); break;
            case E::conferindo:   t << utf8 ("Conferindo o arquivo baixado…"); break;
            case E::instalando:   t << utf8 ("Instalando: digite a senha do Mac na janela que abriu."); break;
            case E::esperaFechar: t << utf8 ("Pronto para instalar a ") << a.versao()
                                    << utf8 (". Feche o DAW: o instalador roda sozinho (o Windows pede permissão). Depois abra o DAW de novo.");
                                  break;
            case E::pronto:       t << utf8 ("Atualizado para a ") << a.versao() << utf8 (". Feche e abra o DAW para usar a versão nova."); break;
            case E::erro:         t << a.mensagem(); break;
            case E::parado:       break;
        }
       #endif
        if (t.isEmpty())
        {
            if (baixou)
                t << utf8 ("Baixando pelo navegador. Feche o DAW antes de rodar o instalador.");
            else
                t << utf8 ("Versão nova do pacote ") << Licenca::nomePacote() << ": " << nova
                  << utf8 (" (você tem ") << Licenca::versaoAtual() << ")";
        }
        g.setColour (texto);
        g.setFont (juce::FontOptions (14.0f));
        if (prog >= 0.0f)
        {
            auto barra = r.removeFromBottom (getHeight() / 2).withSizeKeepingCentre (r.getWidth(), 6).toFloat();
            g.setColour (juce::Colour (0xff2a2d33));
            g.fillRoundedRectangle (barra, 3.0f);
            g.setColour (destaque);
            g.fillRoundedRectangle (barra.withWidth (juce::jmax (6.0f, barra.getWidth() * prog)), 3.0f);
            g.setColour (texto);
            t << "  " << juce::String (juce::roundToInt (prog * 100.0f)) << "%";
        }
        g.drawFittedText (t, r, juce::Justification::centredLeft, 2, 0.8f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        for (auto* b : { &depois, &baixar, &atualizar })
        {
            if (! b->isVisible()) continue;
            b->setBounds (r.removeFromRight (b == &atualizar ? 104 : (b == &baixar ? 84 : 96)));
            r.removeFromRight (6);
        }
    }

    // a TelaAtivacao chama quando a licença muda (chegou o aviso, dispensou…)
    void arrumar()
    {
        bool podeAtualizar = false, ocupado = false, fim = false;
       #if ISP_ATUALIZAR
        using E = Atualizador::Estado;
        const auto e = licenca->motorAtualizacao().estado();
        podeAtualizar = licenca->atualizador() != nullptr && (e == E::parado || e == E::erro) && ! baixou;
        ocupado = e == E::baixando || e == E::conferindo || e == E::instalando;
        fim = e == E::esperaFechar || e == E::pronto;
       #endif
        atualizar.setVisible (podeAtualizar);
        baixar.setVisible (! baixou && ! ocupado && ! fim);
        depois.setVisible (! ocupado);
        depois.setButtonText (utf8 (baixou || fim ? "Fechar" : "Agora não"));
        resized();
        repaint();
    }

private:
    int primeiroBotao() const
    {
        int x = getWidth() - 8;
        for (auto* b : { &atualizar, &baixar, &depois })
            if (b->isVisible()) x = juce::jmin (x, b->getX());
        return x;
    }

   #if ISP_ATUALIZAR
    void changeListenerCallback (juce::ChangeBroadcaster*) override { arrumar(); }
   #endif

    juce::SharedResourcePointer<Licenca> licenca;
    juce::TextButton atualizar, baixar, depois;
    bool baixou = false;
    const juce::Colour texto { 0xffe9e4da }, destaque { 0xffe8a33d };

    JUCE_DECLARE_NON_COPYABLE (AvisoVersao)
};

class TelaAtivacao : public juce::Component, private juce::ChangeListener, private juce::ComponentListener
{
public:
    TelaAtivacao()
    {
        setLookAndFeel (&lf);
        lf.setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());

        for (auto* ed : { &emailEd, &tokenEd })
        {
            addChildComponent (ed);
            ed->setFont (juce::FontOptions (17.0f));
            ed->setIndents (10, 8);
            ed->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f1013));
            ed->setColour (juce::TextEditor::textColourId, texto);
            ed->setColour (juce::TextEditor::outlineColourId, borda);
            ed->setColour (juce::TextEditor::focusedOutlineColourId, destaque);
            ed->setColour (juce::CaretComponent::caretColourId, destaque);
            ed->onReturnKey = [this] { clicouAtivar(); };
        }
        emailEd.setTextToShowWhenEmpty (utf8 ("e-mail da compra"), apagado);
        tokenEd.setTextToShowWhenEmpty (utf8 ("token (ISP-XXXX-XXXX-…)"), apagado);
        emailEd.setInputRestrictions (120);
        tokenEd.setInputRestrictions (60);

        for (auto* b : { &principal, &secundario })
        {
            addChildComponent (b);
            b->setColour (juce::TextButton::textColourOffId, texto);
        }
        principal.setColour (juce::TextButton::buttonColourId, destaque.darker (0.35f));
        secundario.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2d33));
        principal.onClick = [this] { clicouPrincipal(); };
        secundario.onClick = [this] { clicouSecundario(); };

        licenca->addChangeListener (this);
    }

    ~TelaAtivacao() override
    {
        licenca->removeChangeListener (this);
        if (dono != nullptr) dono->removeComponentListener (this);
        aviso.setLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    void anexar (juce::Component& editor)
    {
       #ifdef ISP_LICENCA_FOTO   // foto da tela (workflow shots): sem tela de ativação nem aviso por cima
        juce::ignoreUnused (editor);
       #else
        dono = &editor;
        editor.addChildComponent (this);
        editor.addChildComponent (aviso);
        aviso.setLookAndFeel (&lf);
        editor.addComponentListener (this);
        setBounds (editor.getLocalBounds());
        posicionarAviso();
        atualizar();
       #endif
    }

    void mostrarInfo()
    {
        modoInfo = true;
        confirmando = false;
        atualizar();
    }

    // o processador usa para saber se toca
    bool liberada() const noexcept { return licenca->liberada(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.82f));
        const auto c = cartao().toFloat();
        g.setColour (juce::Colour (0xff1b1d22));
        g.fillRoundedRectangle (c, 10.0f);
        g.setColour (borda);
        g.drawRoundedRectangle (c.reduced (0.5f), 10.0f, 1.0f);

        auto r = cartao().reduced (28, 22);
        g.setColour (destaque);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("INSIDESTUDIOSP", r.removeFromTop (18), juce::Justification::centredLeft);
        g.setColour (texto);
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        const bool ativa = licenca->liberada();
        g.drawText (utf8 (ativa ? "PACOTE " : "ATIVAR PACOTE ") + nomePacoteTxt() + (ativa ? utf8 (" ATIVADO") : juce::String()),
                    r.removeFromTop (34), juce::Justification::centredLeft);
        r.removeFromTop (6);

        g.setFont (juce::FontOptions (14.5f));
        g.setColour (texto.withAlpha (0.78f));
        if (ativa)
        {
            const auto ate = licenca->validaAte().formatted ("%d/%m/%Y");
            juce::String t;
            t << utf8 ("E-mail: ") << licenca->email() << "\n"
              << utf8 ("Este computador: ") << juce::SystemStats::getComputerName() << "\n"
              << utf8 ("Confirmada até ") << ate << utf8 (" (renova sozinha quando há internet).") << "\n\n"
              << utf8 ("Vai trocar de computador? Desative este para liberar a vaga. A licença vale em 2 computadores.");
            g.drawFittedText (t, r.removeFromTop (130), juce::Justification::topLeft, 8, 1.0f);
        }
        else
        {
            g.drawFittedText (utf8 ("Digite o e-mail usado na compra e o token que chegou junto. "
                                    "É preciso internet só na ativação; depois ela é confirmada a cada 45 dias."),
                              r.removeFromTop (54), juce::Justification::topLeft, 3, 1.0f);
        }

        // mensagem do servidor / andamento, logo acima dos botões
        auto m = cartao().reduced (28, 0).withTop (principal.getY() - 46).withHeight (40);
        const auto msg = licenca->mensagem();
        if (msg.isNotEmpty())
        {
            g.setFont (juce::FontOptions (14.0f));
            g.setColour (licenca->ocupada() || licenca->liberada() ? texto.withAlpha (0.85f) : juce::Colour (0xffff8a70));
            g.drawFittedText (msg, m, juce::Justification::centredLeft, 3, 1.0f);
        }
    }

    void resized() override
    {
        auto r = cartao().reduced (28, 22);
        r.removeFromTop (18 + 34 + 6 + 54 + 10);
        emailEd.setBounds (r.removeFromTop (38));
        r.removeFromTop (10);
        tokenEd.setBounds (r.removeFromTop (38));

        auto b = cartao().reduced (28, 22).removeFromBottom (38);
        if (secundario.isVisible())
        {
            secundario.setBounds (b.removeFromRight (secundarioLargura()));
            b.removeFromRight (10);
        }
        principal.setBounds (b);
    }

private:
    juce::Rectangle<int> cartao() const
    {
        return getLocalBounds().withSizeKeepingCentre (juce::jmin (480, getWidth() - 24), juce::jmin (350, getHeight() - 24));
    }

    static juce::String nomePacoteTxt() { return Licenca::nomePacote(); }

    int secundarioLargura() const { return modoInfo && licenca->liberada() ? 130 : 0; }

    void posicionarAviso()
    {
        if (dono == nullptr) return;
        const auto r = dono->getLocalBounds();
       #if ISP_ATUALIZAR   // três botões e textos mais longos
        const int w = juce::jmin (720, r.getWidth() - 24), h = 50;
       #else
        const int w = juce::jmin (620, r.getWidth() - 24), h = 46;
       #endif
        aviso.setBounds (r.getCentreX() - w / 2, r.getBottom() - h - 14, w, h);
    }

    void atualizar()
    {
        const bool ativa = licenca->liberada();
        aviso.setVisible (ativa && ! modoInfo && licenca->versaoNova().isNotEmpty());
        if (aviso.isVisible()) { aviso.arrumar(); aviso.toFront (false); }
        if (! ativa) modoInfo = false;
        const bool mostrar = ! ativa || modoInfo;
        setVisible (mostrar);
        if (! mostrar) return;
        toFront (false);

        emailEd.setVisible (! ativa);
        tokenEd.setVisible (! ativa);
        if (! ativa && emailEd.isEmpty() && tokenEd.isEmpty())
        {
            emailEd.setText (licenca->email(), false);
            tokenEd.setText (licenca->token(), false);
        }

        const bool ocupado = licenca->ocupada();
        principal.setVisible (true);
        principal.setEnabled (! ocupado);
        secundario.setVisible (ativa);
        secundario.setEnabled (! ocupado);
        if (ativa)
        {
            principal.setButtonText (utf8 (confirmando ? "Confirmar: desativar este computador" : "Desativar este computador"));
            secundario.setButtonText (utf8 ("Fechar"));
        }
        else
            principal.setButtonText (utf8 (ocupado ? "Aguarde…" : "ATIVAR"));

        resized();
        repaint();
    }

    void clicouPrincipal()
    {
        if (! licenca->liberada()) { clicouAtivar(); return; }
        if (! confirmando) { confirmando = true; atualizar(); return; }
        confirmando = false;
        licenca->desativar();
    }

    void clicouSecundario()
    {
        modoInfo = false;
        confirmando = false;
        atualizar();
    }

    void clicouAtivar()
    {
        if (licenca->liberada() || licenca->ocupada()) return;
        licenca->ativar (emailEd.getText(), tokenEd.getText());
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { atualizar(); }

    void componentMovedOrResized (juce::Component& c, bool, bool wasResized) override
    {
        if (wasResized)
        {
            setBounds (c.getLocalBounds());
            posicionarAviso();
        }
    }

    void componentChildrenChanged (juce::Component&) override
    {
        if (isVisible()) toFront (false);
        else if (aviso.isVisible()) aviso.toFront (false);
    }

    juce::SharedResourcePointer<Licenca> licenca;
    juce::LookAndFeel_V4 lf;
    AvisoVersao aviso;
    juce::Component* dono = nullptr;
    juce::TextEditor emailEd, tokenEd;
    juce::TextButton principal, secundario;
    bool modoInfo = false, confirmando = false;

    const juce::Colour texto { 0xffe9e4da }, apagado { 0xff7d7a74 }, borda { 0xff3a3d45 }, destaque { 0xffe8a33d };

    JUCE_DECLARE_NON_COPYABLE (TelaAtivacao)
};
} // namespace isp
