#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <memory>

// RESENHA HUB — cada um dos 4 slots pode ser INTERNO (o FM/VA do RESENHA) ou PLUGIN (um instrumento VST3/AU que a
// pessoa tem instalado: Serum, Sylenth1...). No slot PLUGIN o RESENHA só manda o MIDI da conversa; o som, os presets e
// a janela são do próprio plugin. Aqui ficam o catálogo (a busca dos plugins instalados, compartilhada por todas as
// instâncias do RESENHA no DAW) e o slot que hospeda o plugin.
namespace resenha
{
// ---------------- catálogo: plugins instalados ----------------
// A busca roda numa thread própria e carrega cada plugin para saber o que ele é. Um plugin que derruba o DAW na busca
// fica anotado no arquivo "busca-em-andamento" (dead man's pedal do JUCE) e, na próxima busca, vai para a lista de
// bloqueados em vez de derrubar de novo. O resultado fica guardado em disco: a busca só roda na 1ª vez ou quando a
// pessoa pede BUSCAR PLUGINS DE NOVO.
class PluginCatalog : private juce::Thread, private juce::AsyncUpdater
{
public:
    PluginCatalog() : juce::Thread ("RESENHA busca de plugins")
    {
        juce::addDefaultFormatsToManager (formats);
        dir().createDirectory();
        if (auto xml = juce::parseXML (listFile()))
            known.recreateFromXml (*xml);
        juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal (known, pedalFile());
        loadFavourites();
    }

    ~PluginCatalog() override
    {
        cancelPendingUpdate();
        signalThreadShouldExit();
        stopThread (8000);
    }

    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList known;
    std::atomic<int>   version { 0 };       // muda a cada resultado novo (a janela de escolha recarrega a lista)
    std::atomic<bool>  scanning { false };
    std::atomic<float> progress { 0.0f };

    bool hasScanned() const { return listFile().existsAsFile(); }

    juce::String scanningName() const
    {
        const juce::ScopedLock sl (nameLock);
        return currentName;
    }

    void startScan (bool fromScratch)
    {
        if (isThreadRunning()) return;
        if (fromScratch) known.clear();
        scanning = true;
        progress = 0.0f;
        startThread (juce::Thread::Priority::low);
    }

    // lista para a janela: instrumentos (ou todos), sem o próprio RESENHA
    juce::Array<juce::PluginDescription> list (bool instrumentsOnly) const
    {
        juce::Array<juce::PluginDescription> out;
        for (const auto& d : known.getTypes())
        {
            if (d.name.equalsIgnoreCase ("RESENHA") && d.manufacturerName.containsIgnoreCase ("InsideStudioSP")) continue;
            if (instrumentsOnly && ! d.isInstrument) continue;
            out.add (d);
        }
        return out;
    }

    juce::StringArray blocked() const { return known.getBlacklistedFiles(); }

    bool isFavourite (const juce::PluginDescription& d) const { return favourites.contains (d.createIdentifierString()); }

    void toggleFavourite (const juce::PluginDescription& d)
    {
        const auto idStr = d.createIdentifierString();
        if (favourites.contains (idStr)) favourites.removeString (idStr);
        else favourites.add (idStr);
        favFile().replaceWithText (favourites.joinIntoString ("\n"));
        ++version;
    }

    static juce::File dir()
    {
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                   .getChildFile ("Library/Application Support/InsideStudioSP/RESENHA");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("InsideStudioSP/RESENHA");
       #endif
    }

private:
    static juce::File listFile()  { return dir().getChildFile ("plugins.xml"); }
    static juce::File pedalFile() { return dir().getChildFile ("busca-em-andamento.txt"); }
    static juce::File favFile()   { return dir().getChildFile ("favoritos.txt"); }

    void loadFavourites()
    {
        favourites.clear();
        favourites.addLines (favFile().loadFileAsString());
        favourites.removeEmptyStrings();
    }

    void run() override
    {
        const auto all = formats.getFormats();
        for (int f = 0; f < all.size() && ! threadShouldExit(); ++f)
        {
            auto* format = all[f];
            juce::PluginDirectoryScanner scanner (known, *format, format->getDefaultLocationsToSearch(), true, pedalFile(), true);
            juce::String name;
            for (;;)
            {
                if (threadShouldExit()) break;
                {
                    const juce::ScopedLock sl (nameLock);
                    currentName = juce::File::createFileWithoutCheckingPath (scanner.getNextPluginFileThatWillBeScanned())
                                      .getFileNameWithoutExtension();
                }
                const bool more = scanner.scanNextFile (true, name);
                progress = ((float) f + scanner.getProgress()) / (float) juce::jmax (1, all.size());
                ++version;
                if (! more) break;
            }
        }
        if (auto xml = known.createXml())
            xml->writeTo (listFile());
        {
            const juce::ScopedLock sl (nameLock);
            currentName.clear();
        }
        scanning = false;
        progress = 1.0f;
        ++version;
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override {}

    juce::StringArray favourites;
    juce::CriticalSection nameLock;
    juce::String currentName;
};

// ---------------- janela do plugin hospedado (ABRIR) ----------------
class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioPluginInstance& inst, const juce::String& title, juce::Colour c)
        : juce::DocumentWindow (title, juce::Colour (0xff12122c), juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
    {
        juce::AudioProcessorEditor* ed = inst.hasEditor() ? inst.createEditorAndMakeActive() : nullptr;
        if (ed == nullptr) ed = new juce::GenericAudioProcessorEditor (inst);
        setUsingNativeTitleBar (true);
        setContentOwned (ed, true);
        setResizable (ed->isResizable(), false);
        setAlwaysOnTop (true);
        juce::ignoreUnused (c);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override { setVisible (false); }
};

// ---------------- um slot do HUB ----------------
// O plugin só é trocado na thread da interface; o áudio pega o ponteiro com try-lock (se a troca estiver no meio,
// aquele bloco sai sem o slot). A janela do plugin vem DEPOIS do plugin na declaração: é destruída antes dele.
struct HostSlot : private juce::AudioProcessorListener
{
    HostSlot() { for (auto& m : macroIdx) m.store (-1); for (auto& m : macroLast) m = -1.0f; }
    ~HostSlot() override { unload(); }

    // ---- interface ----
    std::atomic<bool> usePlugin { false };    // INTERNO (false) ou PLUGIN (true)
    std::atomic<bool> bypass { false };
    std::atomic<bool> loaded { false };
    std::array<std::atomic<int>, 4> macroIdx {};   // parâmetro do plugin ligado a cada knob (−1 = livre)
    std::atomic<int> learnMacro { -1 };            // knob esperando o "mexa no plugin"
    std::atomic<int> learned { -1 };               // parâmetro que o plugin avisou que mudou durante o aprender
    std::atomic<float> cpu { 0.0f }, level { 0.0f };
    juce::PluginDescription desc;                  // o que está (ou estava) carregado; vale mesmo se o plugin sumiu
    juce::String missing;                          // plugin do projeto que não abriu nesta máquina
    juce::MemoryBlock missingState;                // ...e o som dele, guardado para voltar no próximo salvar

    juce::AudioPluginInstance* get() const { return inst.get(); }

    void setInstance (std::unique_ptr<juce::AudioPluginInstance> p)
    {
        window.reset();
        if (inst != nullptr) inst->removeListener (this);
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            std::swap (inst, p);
            loaded = inst != nullptr;
            heldNote = -1;
            bendOn = false;
            needReset = true;
            for (auto& m : macroLast) m = -1.0f;
        }
        if (inst != nullptr) inst->addListener (this);
        if (p != nullptr) p->releaseResources();
        p.reset();   // o plugin antigo morre fora do lock
    }

    void unload()
    {
        setInstance (nullptr);
    }

    void openWindow (const juce::String& title, juce::Colour c)
    {
        if (inst == nullptr) return;
        if (window == nullptr) window = std::make_unique<PluginWindow> (*inst, title, c);
        window->setVisible (true);
        window->toFront (true);
    }

    void closeWindow() { window.reset(); }

    // ---- áudio ----
    juce::SpinLock lock;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf;
    int  heldNote = -1;
    bool needReset = false, wasBypassed = false;
    // BEND: mergulho do pitch bend na nota que fecha a fala
    bool  bendOn = false, bendSent = false;
    int   bendPos = 0, bendLen = 0;
    float bendTo = 0.0f;
    std::array<float, 4> macroLast {};

private:
    void audioProcessorParameterChanged (juce::AudioProcessor*, int index, float) override
    {
        if (learnMacro.load() >= 0) learned.store (index);
    }
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}

    std::unique_ptr<juce::AudioPluginInstance> inst;
    std::unique_ptr<PluginWindow> window;
};
} // namespace resenha
