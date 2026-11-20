#include "Daws.h"

namespace mapa
{
namespace
{
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

struct Regra
{
    const char* prefixo;   // começo do nome do .app / da pasta
    Formato formato;
    const char* detalhe;
};

const Regra REGRAS[] = {
    { "Ableton Live", Formato::ableton,    "projeto .als completo" },
    { "REAPER",       Formato::reaper,     "projeto .rpp completo" },
    { "Bitwig Studio",Formato::dawproject, "formato DAWproject" },
    { "Studio One",   Formato::dawproject, "formato DAWproject" },
    { "Cubase",       Formato::dawproject, "formato DAWproject (Cubase 14 ou mais novo)" },
    { "FL Studio",    Formato::pasta,      "pasta com MIDI + áudio" },
    { "Logic Pro",    Formato::pasta,      "pasta com MIDI + áudio" },
    { "Pro Tools",    Formato::pasta,      "pasta com MIDI + áudio" },
    { "GarageBand",   Formato::pasta,      "pasta com MIDI + áudio" },
};

const Regra* regraDe (const juce::String& nome)
{
    for (auto& r : REGRAS)
        if (nome.startsWithIgnoreCase (r.prefixo)) return &r;
    return nullptr;
}

void adiciona (juce::Array<Daw>& lista, const juce::String& nome, const juce::File& app)
{
    auto* r = regraDe (nome);
    if (r == nullptr) return;
    for (auto& d : lista)
        if (d.nome.equalsIgnoreCase (nome)) return;
    Daw d;
    d.nome = nome;
    d.detalhe = u8 (r->detalhe);
    d.formato = r->formato;
    d.app = app;
    d.encontrada = true;
    lista.add (d);
}

int ordem (Formato f)
{
    switch (f)
    {
        case Formato::ableton:    return 0;
        case Formato::reaper:     return 1;
        case Formato::dawproject: return 2;
        case Formato::pasta:      return 3;
    }
    return 4;
}

#if JUCE_WINDOWS
// procura arquivos por curinga dentro de uma pasta (com subpastas, até a profundidade dada)
void achar (const juce::File& base, const juce::String& curinga, int profundidade, juce::Array<juce::File>& out)
{
    if (! base.isDirectory()) return;
    for (auto& f : base.findChildFiles (juce::File::findFiles, false, curinga))
        out.add (f);
    if (profundidade > 0)
        for (auto& d : base.findChildFiles (juce::File::findDirectories, false))
            achar (d, curinga, profundidade - 1, out);
}
#endif
} // namespace

juce::String rotuloFormato (Formato f)
{
    switch (f)
    {
        case Formato::ableton:
        case Formato::reaper:     return "COMPLETO";
        case Formato::dawproject: return "DAWPROJECT";
        case Formato::pasta:      return "ARRASTAR";
    }
    return {};
}

juce::Array<Daw> procurarDaws()
{
    juce::Array<Daw> lista;
   #if JUCE_MAC
    for (auto pasta : { juce::File ("/Applications"),
                        juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Applications") })
    {
        if (! pasta.isDirectory()) continue;
        juce::Array<juce::File> apps = pasta.findChildFiles (juce::File::findDirectories, false, "*.app");
        // DAWs que ficam numa subpasta (ex.: /Applications/Ableton/…)
        for (auto& sub : pasta.findChildFiles (juce::File::findDirectories, false))
            if (! sub.getFileName().endsWithIgnoreCase (".app"))
                apps.addArray (sub.findChildFiles (juce::File::findDirectories, false, "*.app"));
        for (auto& a : apps)
            adiciona (lista, a.getFileNameWithoutExtension(), a);
    }
   #elif JUCE_WINDOWS
    const auto pf = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory);   // C:\Program Files
    const auto pd = juce::File (juce::SystemStats::getEnvironmentVariable ("ProgramData", "C:\\ProgramData"));
    juce::Array<juce::File> achados;

    achados.clear();
    achar (pd.getChildFile ("Ableton"), "Ableton Live*.exe", 3, achados);
    achar (pf.getChildFile ("Ableton"), "Ableton Live*.exe", 3, achados);
    for (auto& f : achados) adiciona (lista, f.getFileNameWithoutExtension(), f);

    for (auto p : { "REAPER (x64)", "REAPER" })
    {
        auto f = pf.getChildFile (p).getChildFile ("reaper.exe");
        if (f.existsAsFile()) { adiciona (lista, "REAPER", f); break; }
    }

    achados.clear();
    achar (pf.getChildFile ("Bitwig Studio"), "Bitwig Studio.exe", 2, achados);
    for (auto& f : achados)
    {
        auto pai = f.getParentDirectory().getFileName();
        adiciona (lista, pai.startsWithIgnoreCase ("Bitwig") ? juce::String ("Bitwig Studio") : "Bitwig Studio " + pai, f);
    }

    achados.clear();
    achar (pf.getChildFile ("PreSonus"), "Studio One.exe", 2, achados);
    achar (pf.getChildFile ("Fender"), "Studio One.exe", 2, achados);
    for (auto& f : achados) adiciona (lista, f.getParentDirectory().getFileName(), f);

    achados.clear();
    achar (pf.getChildFile ("Steinberg"), "Cubase*.exe", 2, achados);
    for (auto& f : achados)
        if (! f.getFileName().containsIgnoreCase ("uninst"))
            adiciona (lista, f.getParentDirectory().getFileName(), f);

    achados.clear();
    achar (pf.getChildFile ("Image-Line"), "FL64.exe", 2, achados);
    for (auto& f : achados) adiciona (lista, f.getParentDirectory().getFileName(), f);

    auto pt = pf.getChildFile ("Avid").getChildFile ("Pro Tools").getChildFile ("ProTools.exe");
    if (pt.existsAsFile()) adiciona (lista, "Pro Tools", pt);
   #endif

    std::stable_sort (lista.begin(), lista.end(), [] (const Daw& a, const Daw& b)
                      { return ordem (a.formato) < ordem (b.formato); });

    Daw salvar;
    salvar.nome = u8 ("Só salvar a pasta");
    salvar.detalhe = u8 ("MIDI + áudio para qualquer DAW");
    salvar.formato = Formato::pasta;
    lista.add (salvar);
    return lista;
}

bool abrirNaDaw (const Daw& daw, const juce::File& projeto, juce::String& erro)
{
    if (daw.app == juce::File())
    {
        projeto.revealToUser();
        return true;
    }
    if (daw.formato == Formato::pasta)
        projeto.startAsProcess();   // abre a pasta no Finder / Explorer para arrastar
   #if JUCE_MAC
    juce::ChildProcess p;
    juce::StringArray args { "open", "-a", daw.app.getFullPathName() };
    if (daw.formato != Formato::pasta) args.add (projeto.getFullPathName());
    if (! p.start (args, 0))
    {
        erro = u8 ("não consegui abrir o ") + daw.nome;
        return false;
    }
    p.waitForProcessToFinish (5000);
    if (p.getExitCode() != 0)
    {
        erro = u8 ("o macOS não abriu o ") + daw.nome;
        return false;
    }
    return true;
   #else
    const bool ok = daw.formato == Formato::pasta ? daw.app.startAsProcess()
                                                  : daw.app.startAsProcess ("\"" + projeto.getFullPathName() + "\"");
    if (! ok) erro = u8 ("não consegui abrir o ") + daw.nome;
    return ok;
   #endif
}
} // namespace mapa
