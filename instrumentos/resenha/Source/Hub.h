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
// SÓ INSTRUMENTOS: efeito é descartado antes de abrir sempre que dá — AU pelo tipo (aumu), VST3 pelo moduleinfo.json do
// bundle. VST3 sem moduleinfo vai ao ResenhaScan, que lê só a categoria na fábrica (sai com 5 se for efeito). Todo
// efeito descartado fica em efeitos.txt (com a data do arquivo) e não é relido nas próximas buscas.
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
        for (const auto& d : known.getTypes())   // listas da 0.5.0/0.5.1 guardavam os efeitos também
            if (! d.isInstrument) known.removeType (d);
        juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal (known, pedalFile());
        loadFavourites();
        loadReasons();
        loadEffects();
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

    // lista para a janela: só instrumentos, sem o próprio RESENHA
    juce::Array<juce::PluginDescription> list() const
    {
        juce::Array<juce::PluginDescription> out;
        for (const auto& d : known.getTypes())
        {
            if (d.name.equalsIgnoreCase ("RESENHA") && d.manufacturerName.containsIgnoreCase ("InsideStudioSP")) continue;
            if (! d.isInstrument) continue;
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
    static juce::File effectsFile() { return dir().getChildFile ("efeitos.txt"); }

    // data do arquivo/bundle: se o plugin for atualizado, ele é lido de novo
    static juce::String stamp (const juce::AudioPluginFormat& format, const juce::String& id)
    {
        if (format.getName() == "AudioUnit") return "0";
        return juce::String (juce::File::createFileWithoutCheckingPath (id).getLastModificationTime().toMilliseconds());
    }

    bool isKnownEffect (const juce::AudioPluginFormat& format, const juce::String& id) const
    {
        const auto st = stamp (format, id);
        const juce::ScopedLock sl (nameLock);
        return effects.contains (id) && effects[id] == st;
    }

    // efeito descartado: grava em lote (a cada 200 e no fim da busca), não a cada arquivo
    void rememberEffect (const juce::AudioPluginFormat& format, const juce::String& id)
    {
        const auto st = stamp (format, id);
        const juce::ScopedLock sl (nameLock);
        effects.set (id, st);
        if (++effectsDirty >= 200) saveEffectsLocked();
    }

    void saveEffectsLocked()
    {
        juce::String txt;
        for (auto it = effects.begin(); it != effects.end(); ++it)
            txt << it.getKey() << "\t" << it.getValue() << "\n";
        effectsFile().replaceWithText (txt);
        effectsDirty = 0;
    }

    void loadEffects()
    {
        juce::StringArray lines;
        lines.addLines (effectsFile().loadFileAsString());
        const juce::ScopedLock sl (nameLock);
        for (const auto& l : lines)
            if (l.contains ("\t"))
                effects.set (l.upToLastOccurrenceOf ("\t", false, false), l.fromLastOccurrenceOf ("\t", false, false));
    }

    // VST3: o moduleinfo.json do bundle diz a categoria de cada classe sem carregar o plugin.
    // 1 = tem instrumento, 0 = só efeitos, -1 = sem moduleinfo (ou ilegível): o ResenhaScan decide
    static int moduleInfoKind (const juce::String& id)
    {
        const auto bundle = juce::File::createFileWithoutCheckingPath (id);
        for (const auto& mi : { bundle.getChildFile ("Contents/Resources/moduleinfo.json"), bundle.getChildFile ("Contents/moduleinfo.json") })
        {
            if (! mi.existsAsFile()) continue;
            const auto v = juce::JSON::parse (mi.loadFileAsString());
            const auto* classes = v.getProperty ("Classes", {}).getArray();
            if (classes == nullptr) continue;
            bool anyAudio = false;
            for (const auto& c : *classes)
            {
                if (c.getProperty ("Category", {}).toString() != "Audio Module Class") continue;
                anyAudio = true;
                const auto sub = c.getProperty ("Sub Categories", {});
                if (const auto* arr = sub.getArray())
                {
                    for (const auto& x : *arr)
                        if (x.toString().containsIgnoreCase ("Instrument")) return 1;
                }
                else if (sub.toString().containsIgnoreCase ("Instrument")) return 1;
            }
            if (anyAudio) return 0;
        }
        return -1;
    }

    // descarta sem abrir: true = é efeito (não vai ao ResenhaScan)
    bool skipWithoutOpening (const juce::AudioPluginFormat& format, const juce::String& id)
    {
        if (format.getName() == "AudioUnit")   // o identificador do AU traz o tipo: aumu = instrumento
            return ! id.containsIgnoreCase ("/aumu,");
        if (isKnownEffect (format, id)) return true;
        if (format.getName() == "VST3" && moduleInfoKind (id) == 0)
        {
            rememberEffect (format, id);
            return true;
        }
        return false;
    }

    // tira os efeitos do resultado; se não sobrar instrumento, lembra o arquivo como efeito
    void keepInstruments (const juce::AudioPluginFormat& format, const juce::String& id, juce::OwnedArray<juce::PluginDescription>& result)
    {
        for (int i = result.size(); --i >= 0;)
            if (! result[i]->isInstrument) result.remove (i);
        if (result.isEmpty()) rememberEffect (format, id);
    }

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
        if (skipWithoutOpening (format, id)) return true;   // efeito: fica fora da lista e não é aberto
        const auto exe = scannerExe();
        const auto out = resultFile();
        out.deleteFile();
        juce::ChildProcess child;
        if (! scannerOk
            || ! child.start (juce::StringArray { exe.getFullPathName(), format.getName(), id, out.getFullPathName() }, 0))
        {
            unprotected = true;
            format.findAllTypesForFile (result, id);   // sem o ResenhaScan: abre no DAW, como na 0.5.0
            keepInstruments (format, id, result);
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
            const auto code = child.getExitCode();
            if (code == 5) { result.clear(); rememberEffect (format, id); return true; }   // só efeitos
            if (! result.isEmpty()) { keepInstruments (format, id, result); return true; }
            // 0/4 = o plugin abriu mas não entregou nenhum plugin; o resto = o ResenhaScan caiu
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
        int savedCount = known.getNumTypes();
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
                if (known.getNumTypes() != savedCount) { saveList(); savedCount = known.getNumTypes(); }   // interrompida, continua daqui
                if (! more) break;
            }
        }
        saveList();
        {
            const juce::ScopedLock sl (nameLock);
            currentName.clear();
            saveEffectsLocked();
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
    juce::HashMap<juce::String, juce::String> effects;   // efeito descartado → data do arquivo (protegido por nameLock)
    int effectsDirty = 0;
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
