// MAPA — app standalone da InsideStudioSP: música de referência -> projeto montado na DAW.
#include <JuceHeader.h>
#include "Tela.h"

namespace mapa
{
class Janela : public juce::DocumentWindow
{
public:
    explicit Janela (const juce::String& nome)
        : juce::DocumentWindow (nome, juce::Colour (0xff111215), juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        tela = new Tela();
        setContentOwned (tela, false);
        setResizable (true, true);
        setResizeLimits (1100, 680, 5000, 3000);
        centreWithSize (juce::jmin (1480, juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea.getWidth() - 40),
                        juce::jmin (900, juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea.getHeight() - 40));
        setVisible (true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    Tela* tela = nullptr;   // dono: a janela (setContentOwned)
};

class App : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MAPA"; }
    const juce::String getApplicationVersion() override { return MAPA_VERSAO; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& linha) override
    {
        janela = std::make_unique<Janela> ("MAPA");
        abrirDaLinha (linha);
    }

    void shutdown() override { janela.reset(); }
    void systemRequestedQuit() override { quit(); }

    // Mac: arquivo aberto pelo Finder ("Abrir com") chega aqui
    void anotherInstanceStarted (const juce::String& linha) override { abrirDaLinha (linha); }

private:
    void abrirDaLinha (const juce::String& linha)
    {
        if (janela == nullptr || janela->tela == nullptr) return;
        auto arg = linha.trim().unquoted();
        if (arg.isEmpty() || ! juce::File::isAbsolutePath (arg)) return;
        juce::File f (arg);
        if (f.existsAsFile()) janela->tela->carregar (f);
    }

    std::unique_ptr<Janela> janela;
};
} // namespace mapa

START_JUCE_APPLICATION (mapa::App)
