#pragma once

// Tela de ativação por cima da janela do plugin. Uso no editor (membro declarado por ÚLTIMO):
//     isp::TelaAtivacao ativacao;
//     ... no fim do construtor:  ativacao.anexar (*this);
//     ... item "Licença…" de algum menu:  ativacao.mostrarInfo();
// Sem licença válida ela cobre tudo e pede e-mail + token; com licença ela some e só aparece no mostrarInfo().

#include "IspLicenca.h"

namespace isp
{
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
        setLookAndFeel (nullptr);
    }

    void anexar (juce::Component& editor)
    {
        dono = &editor;
        editor.addChildComponent (this);
        editor.addComponentListener (this);
        setBounds (editor.getLocalBounds());
        atualizar();
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

    void atualizar()
    {
        const bool ativa = licenca->liberada();
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
        if (wasResized) setBounds (c.getLocalBounds());
    }

    void componentChildrenChanged (juce::Component&) override
    {
        if (isVisible()) toFront (false);
    }

    juce::SharedResourcePointer<Licenca> licenca;
    juce::LookAndFeel_V4 lf;
    juce::Component* dono = nullptr;
    juce::TextEditor emailEd, tokenEd;
    juce::TextButton principal, secundario;
    bool modoInfo = false, confirmando = false;

    const juce::Colour texto { 0xffe9e4da }, apagado { 0xff7d7a74 }, borda { 0xff3a3d45 }, destaque { 0xffe8a33d };

    JUCE_DECLARE_NON_COPYABLE (TelaAtivacao)
};
} // namespace isp
