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
// A busca roda numa thread própria, mas NÃO abre os plugins dentro do DAW: cada arquivo é aberto pelo ResenhaScan
// (programa à parte que vai dentro do bundle do RESENHA, em Contents/Resources). Se o plugin travar (passou do tempo),
// fechar o ResenhaScan (crash) ou não abrir, o ResenhaScan é encerrado, o plugin vai para os bloqueados com o motivo e
// a busca segue no próximo. O DAW nunca fica preso num plugin. A lista é gravada em disco durante a busca: se ela for
// interrompida, a próxima continua de onde parou. Só se o ResenhaScan não existir ou não abrir (instalação antiga, Mac
// que barrou o programa) a busca volta a abrir os plugins dentro do DAW, como na 0.5.0.
class PluginCatalog : private juce::Thread, private juce::AsyncUpdater
{
public:
    static constexpr int timeoutMs = 30000;   // tempo máximo para um plugin abrir na busca

    PluginCatalog() : juce::Thread ("RESENHA busca de plugins")
    {
        juce::addDefaultFormatsToManager (formats);
        dir().createDirectory();
        if (auto xml = juce::parseXML (listFile()))
            known.recreateFromXml (*xml);
        juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal (known, pedalFile());
        loadFavourites();
        loadReasons();
        known.setCustomScanner (std::make_unique<ChildScanner> (*this));
    }

    ~PluginCatalog() override
    {
        cancelPendingUpdate();
        signalThreadShouldExit();
        stopThread (timeoutMs + 5000);
    }

    // PULAR: encerra o plugin que está sendo lido agora (vai para os bloqueados) e a busca segue
    void skipCurrent() { skipRequested = true; }

    // motivo do bloqueio ("travou", "fechou", "não abriu", "pulado"); vazio = bloqueado por versão antiga da busca
    juce::String reasonFor (const juce::String& fileOrId) const
    {
        const juce::ScopedLock sl (nameLock);
        return reasons[fileOrId];
    }

    std::atomic<bool> unprotected { false };   // o ResenhaScan não abriu: a busca está abrindo os plugins no DAW

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
        if (fromScratch)
        {
            known.clear();
            known.clearBlacklistedFiles();   // os bloqueados ganham outra chance (agora com tempo limite)
            const juce::ScopedLock sl (nameLock);
            reasons.clear();
            reasonsFile().deleteFile();
        }
        skipRequested = false;
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
    static juce::File listFile()    { return dir().getChildFile ("plugins.xml"); }
    static juce::File pedalFile()   { return dir().getChildFile ("busca-em-andamento.txt"); }
    static juce::File favFile()     { return dir().getChildFile ("favoritos.txt"); }
    static juce::File reasonsFile() { return dir().getChildFile ("bloqueados-motivo.txt"); }
    static juce::File resultFile()  { return dir().getChildFile ("busca-resultado.xml"); }

    // o ResenhaScan mora no bundle do próprio RESENHA: <bundle>/Contents/Resources/ResenhaScan(.exe). O binário do
    // plugin fica em Contents/x86_64-win (VST3 Windows) ou Contents/MacOS (VST3/AU Mac): sobe dois níveis até Contents.
    static juce::File scannerExe()
    {
        const auto bin = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
       #if JUCE_WINDOWS
        return bin.getParentDirectory().getParentDirectory().getChildFile ("Resources/ResenhaScan.exe");
       #else
        return bin.getParentDirectory().getParentDirectory().getChildFile ("Resources/ResenhaScan");
       #endif
    }

    // a busca do JUCE (PluginDirectoryScanner) chama isto para cada arquivo; false = vai para os bloqueados
    class ChildScanner : public juce::KnownPluginList::CustomScanner
    {
    public:
        explicit ChildScanner (PluginCatalog& c) : cat (c) {}
        bool findPluginTypesFor (juce::AudioPluginFormat& format, juce::OwnedArray<juce::PluginDescription>& result,
                                 const juce::String& fileOrIdentifier) override
        {
            return cat.scanOne (format, result, fileOrIdentifier);
        }
    private:
        PluginCatalog& cat;
    };

    bool scanOne (juce::AudioPluginFormat& format, juce::OwnedArray<juce::PluginDescription>& result, const juce::String& id)
    {
        const auto exe = scannerExe();
        const auto out = resultFile();
        out.deleteFile();
        juce::ChildProcess child;
        if (! scannerOk
            || ! child.start (juce::StringArray { exe.getFullPathName(), format.getName(), id, out.getFullPathName() }, 0))
        {
            unprotected = true;
            format.findAllTypesForFile (result, id);   // sem o ResenhaScan: abre no DAW, como na 0.5.0
            return true;
        }

        const auto t0 = juce::Time::getMillisecondCounter();
        juce::String why;
        for (;;)
        {
            if (child.waitForProcessToFinish (100)) break;
            if (threadShouldExit())                  // DAW fechando: encerra sem marcar o plugin como bloqueado
            {
                child.kill();
                return true;
            }
            if (skipRequested.exchange (false)) { why = "pulado"; break; }
            if (juce::Time::getMillisecondCounter() - t0 > (juce::uint32) timeoutMs) { why = "travou"; break; }
        }

        if (why.isNotEmpty())
        {
            child.kill();
            child.waitForProcessToFinish (3000);
        }
        else
        {
            // vale o que o ResenhaScan gravou, mesmo que ele tenha caído depois
            if (auto xml = juce::parseXML (out))
                for (const auto* item : xml->getChildIterator())
                {
                    auto d = std::make_unique<juce::PluginDescription>();
                    if (d->loadFromXml (*item)) result.add (std::move (d));
                }
            out.deleteFile();
            if (! result.isEmpty()) return true;
            // 0/4 = o plugin abriu mas não entregou nenhum instrumento/efeito; o resto = o ResenhaScan caiu
            const auto code = child.getExitCode();
            why = (code == 0 || code == 4) ? juce::String::fromUTF8 ("não abriu") : juce::String ("fechou");
        }
        out.deleteFile();
        setReason (id, why);
        return false;
    }

    // antes da busca: o ResenhaScan existe e o sistema deixa ele rodar? (no Mac, um binário barrado pelo Gatekeeper
    // morreria em todo plugin e bloquearia a lista inteira; aí a busca volta a ser no DAW, com o aviso na janela)
    static bool scannerRuns()
    {
        const auto exe = scannerExe();
        if (! exe.existsAsFile()) return false;
        juce::ChildProcess child;
        if (! child.start (juce::StringArray { exe.getFullPathName(), "--teste" }, 0)) return false;
        if (! child.waitForProcessToFinish (15000)) { child.kill(); return false; }
        return child.getExitCode() == 0;
    }

    void setReason (const juce::String& id, const juce::String& why)
    {
        const juce::ScopedLock sl (nameLock);
        reasons.set (id, why);
        juce::String txt;
        for (auto it = reasons.begin(); it != reasons.end(); ++it)   // juce::HashMap: chave\tmotivo por linha
            txt << it.getKey() << "\t" << it.getValue() << "\n";
        reasonsFile().replaceWithText (txt);
    }

    void loadReasons()
    {
        juce::StringArray lines;
        lines.addLines (reasonsFile().loadFileAsString());
        for (const auto& l : lines)
            if (l.contains ("\t"))
                reasons.set (l.upToLastOccurrenceOf ("\t", false, false), l.fromLastOccurrenceOf ("\t", false, false));
    }

    void saveList()
    {
        if (auto xml = known.createXml())
            xml->writeTo (listFile());
    }

    void loadFavourites()
    {
        favourites.clear();
        favourites.addLines (favFile().loadFileAsString());
        favourites.removeEmptyStrings();
    }

    void run() override
    {
        scannerOk = scannerRuns();
        unprotected = ! scannerOk;
        const auto all = formats.getFormats();
        int sinceSave = 0;
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
                    const auto next = scanner.getNextPluginFileThatWillBeScanned();
                    currentName = format->getName() == "AudioUnit" ? format->getNameOfPluginFromIdentifier (next)
                                                                   : juce::File::createFileWithoutCheckingPath (next).getFileNameWithoutExtension();
                }
                const bool more = scanner.scanNextFile (true, name);
                progress = ((float) f + scanner.getProgress()) / (float) juce::jmax (1, all.size());
                ++version;
                if (++sinceSave >= 5) { saveList(); sinceSave = 0; }   // interrompida, a próxima busca continua daqui
                if (! more) break;
            }
        }
        saveList();
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
    juce::HashMap<juce::String, juce::String> reasons;   // bloqueado → motivo (protegido por nameLock)
    std::atomic<bool> skipRequested { false };
    bool scannerOk = false;   // só a thread da busca usa
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
