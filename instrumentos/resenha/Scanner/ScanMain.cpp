// ResenhaScan — leitor de plugins do RESENHA HUB, fora do DAW.
// Uso: ResenhaScan <formato> <arquivo-ou-identificador> <saida.xml>
// Abre UM plugin, grava a lista do que ele contém (nome, fabricante, instrumento/efeito...) em <saida.xml> e sai.
// Se o plugin travar, o RESENHA encerra este processo pelo tempo; se derrubar, só este processo cai. O DAW não sente.
// Saídas: 0 = ok, 2 = argumentos/formato inválidos, 3 = não gravou o XML, 4 = o plugin não entregou nada.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdlib>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <shellapi.h>
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
} // namespace

int main (int argc, char* argv[])
{
    const auto a = args (argc, argv);
    if (a.size() == 2 && a[1] == "--teste") return 0;   // o RESENHA confere se o sistema deixa este programa rodar
    if (a.size() < 4) return 2;

    juce::ScopedJuceInitialiser_GUI gui;   // a thread principal vira a thread de mensagens (o plugin pode exigir)
    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    for (auto* f : formats.getFormats())
    {
        if (f->getName() != a[1]) continue;
        juce::OwnedArray<juce::PluginDescription> found;
        f->findAllTypesForFile (found, a[2]);
        juce::XmlElement xml ("LIST");
        for (auto* d : found)
            xml.addChildElement (d->createXml().release());
        const int code = ! xml.writeTo (juce::File (a[3])) ? 3 : found.isEmpty() ? 4 : 0;
        // sai sem descarregar o plugin: há plugin que trava ou cai ao ser descarregado, e o resultado já está gravado
        std::_Exit (code);
    }
    return 2;
}
