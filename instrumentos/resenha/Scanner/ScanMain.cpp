// ResenhaScan — leitor de plugins do RESENHA HUB, fora do DAW.
// Uso: ResenhaScan <formato> <arquivo-ou-identificador> <saida.xml>
// Abre UM plugin, grava a lista dos INSTRUMENTOS que ele contém (nome, fabricante...) em <saida.xml> e sai.
// Se o plugin travar, o RESENHA encerra este processo pelo tempo; se derrubar, só este processo cai. O DAW não sente.
// VST3: antes de iniciar qualquer coisa, lê só a categoria na fábrica do plugin; se não houver instrumento, sai com 5
// sem criar nada (efeito é descartado rápido). Só os instrumentos passam pela leitura completa do JUCE.
// Saídas: 0 = ok, 2 = argumentos/formato inválidos, 3 = não gravou o XML, 4 = o plugin não entregou nada,
//         5 = o plugin só tem efeitos (não é instrumento).
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdlib>
#include <cstring>

#include "pluginterfaces/base/ipluginbase.h"   // só cabeçalho (IIDs inline); o SDK vem dentro do JUCE

#if JUCE_WINDOWS
 #include <windows.h>
 #include <shellapi.h>
#elif JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
#endif

namespace
{
juce::StringArray args (int argc, char* argv[])
{
    juce::StringArray out;
   #if JUCE_WINDOWS
    // argv do Windows vem na página de código local: caminho com acento se perderia. Lê a linha de comando em UTF-16.
    juce::ignoreUnused (argc, argv);
    int n = 0;
    if (auto** w = CommandLineToArgvW (GetCommandLineW(), &n))
    {
        for (int i = 0; i < n; ++i) out.add (juce::String (w[i]));
        LocalFree (w);
    }
   #else
    for (int i = 0; i < argc; ++i) out.add (juce::String (juce::CharPointer_UTF8 (argv[i])));
   #endif
    return out;
}

using GetFactoryFn = Steinberg::IPluginFactory* (PLUGIN_API*)();

// Carrega o binário do VST3 e devolve a fábrica (sem descarregar: o processo sai com _Exit logo depois).
// O JUCE carrega o mesmo módulo de novo se for instrumento; InitDll/bundleEntry do SDK contam as chamadas.
Steinberg::IPluginFactory* loadFactory (const juce::File& plugin)
{
   #if JUCE_WINDOWS
    auto bin = plugin;
    if (plugin.isDirectory())
    {
       #if defined (_M_ARM64) || defined (__aarch64__)
        const char* arch = "arm64-win";
       #else
        const char* arch = "x86_64-win";
       #endif
        bin = plugin.getChildFile ("Contents").getChildFile (arch).getChildFile (plugin.getFileName());
    }
    auto mod = LoadLibraryW (bin.getFullPathName().toWideCharPointer());
    if (mod == nullptr) return nullptr;
    using InitFn = bool (PLUGIN_API*)();
    if (auto init = (InitFn) (void*) GetProcAddress (mod, "InitDll"))
        if (! init()) return nullptr;
    auto get = (GetFactoryFn) (void*) GetProcAddress (mod, "GetPluginFactory");
    return get != nullptr ? get() : nullptr;
   #elif JUCE_MAC
    auto path = plugin.getFullPathName().toCFString();
    auto url = CFURLCreateWithFileSystemPath (nullptr, path, kCFURLPOSIXPathStyle, true);
    CFRelease (path);
    if (url == nullptr) return nullptr;
    auto bundle = CFBundleCreate (nullptr, url);
    CFRelease (url);
    if (bundle == nullptr || ! CFBundleLoadExecutable (bundle)) return nullptr;
    using EntryFn = bool (*)(CFBundleRef);
    if (auto entry = (EntryFn) CFBundleGetFunctionPointerForName (bundle, CFSTR ("bundleEntry")))
        if (! entry (bundle)) return nullptr;
    auto get = (GetFactoryFn) CFBundleGetFunctionPointerForName (bundle, CFSTR ("GetPluginFactory"));
    return get != nullptr ? get() : nullptr;
   #else
    juce::ignoreUnused (plugin);
    return nullptr;
   #endif
}

// 1 = tem instrumento, 0 = só efeitos, -1 = não deu para saber (segue a leitura completa e filtra depois)
int vst3Kind (const juce::File& plugin)
{
    auto* factory = loadFactory (plugin);
    if (factory == nullptr) return -1;
    Steinberg::IPluginFactory2* f2 = nullptr;
    if (factory->queryInterface (Steinberg::IPluginFactory2_iid, (void**) &f2) != Steinberg::kResultOk || f2 == nullptr)
        return -1;
    bool anyAudio = false;
    const auto n = f2->countClasses();
    for (Steinberg::int32 i = 0; i < n; ++i)
    {
        Steinberg::PClassInfo2 info {};
        if (f2->getClassInfo2 (i, &info) != Steinberg::kResultOk) continue;
        if (std::strcmp (info.category, "Audio Module Class") != 0) continue;
        anyAudio = true;
        info.subCategories[sizeof (info.subCategories) - 1] = 0;
        if (juce::String (info.subCategories).containsIgnoreCase ("Instrument")) return 1;
    }
    return anyAudio ? 0 : -1;
}
} // namespace

int main (int argc, char* argv[])
{
    const auto a = args (argc, argv);
    if (a.size() == 2 && a[1] == "--teste") return 0;   // o RESENHA confere se o sistema deixa este programa rodar
    if (a.size() < 4) return 2;

    juce::ScopedJuceInitialiser_GUI gui;   // a thread principal vira a thread de mensagens (o plugin pode exigir)

    if (a[1] == "VST3" && vst3Kind (juce::File (a[2])) == 0)
    {
        juce::XmlElement ("LIST").writeTo (juce::File (a[3]));
        std::_Exit (5);
    }

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    for (auto* f : formats.getFormats())
    {
        if (f->getName() != a[1]) continue;
        juce::OwnedArray<juce::PluginDescription> found;
        f->findAllTypesForFile (found, a[2]);
        juce::XmlElement xml ("LIST");
        int instruments = 0;
        for (auto* d : found)
            if (d->isInstrument) { xml.addChildElement (d->createXml().release()); ++instruments; }
        const int code = ! xml.writeTo (juce::File (a[3])) ? 3 : found.isEmpty() ? 4 : instruments == 0 ? 5 : 0;
        // sai sem descarregar o plugin: há plugin que trava ou cai ao ser descarregado, e o resultado já está gravado
        std::_Exit (code);
    }
    return 2;
}
