// MAPA — exportadores (pasta MIDI, Ableton Live .als, Reaper .rpp, DAWproject).
// Contrato em Exportar.h. Tempos do Projeto são em TEMPOS (semínimas) absolutos no projeto.
#include "Exportar.h"

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "BinaryData.h"   // MapaDados::modelo_als / MapaDados::modelo_alsSize (juce_add_binary_data)

namespace mapa
{
namespace
{
// ================================================================================================
// Utilidades comuns
// ================================================================================================

juce::String U (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

// Literal com acento. O fonte é UTF-8 SEM BOM; juce::String (const char*) só aceita ASCII (jassert + texto
// estragado), então todo literal não-ASCII passa por aqui.
juce::String tx (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// Número sem depender de locale: inteiro sem casas; senão até 6 casas, sem zeros à direita.
juce::String fmtNum (double v)
{
    if (! std::isfinite (v))
        v = 0.0;
    const double r = std::round (v);
    if (std::abs (v - r) < 1.0e-9)
    {
        const juce::int64 i = (juce::int64) r;
        return juce::String (i);   // (int64) de -0.0 é 0: nunca sai "-0"
    }
    juce::String s (v, 6);   // casas decimais fixas
    while (s.endsWithChar ('0'))
        s = s.dropLastCharacters (1);
    if (s.endsWithChar ('.'))
        s = s.dropLastCharacters (1);
    return s;
}

ResultadoExport falha (const juce::String& msg)
{
    ResultadoExport r;
    r.ok = false;
    r.erro = msg;
    return r;
}

ResultadoExport sucesso (const juce::File& arq)
{
    ResultadoExport r;
    r.ok = true;
    r.arquivo = arq;
    return r;
}

bool garantirPasta (const juce::File& pasta, juce::String& erro)
{
    if (pasta.isDirectory())
        return true;
    const juce::Result r = pasta.createDirectory();
    if (r.failed())
    {
        erro = tx ("Não consegui criar a pasta ") + pasta.getFullPathName() + ": " + r.getErrorMessage();
        return false;
    }
    return true;
}

juce::String nomeProjeto (const Projeto& p)
{
    return nomeSeguro (p.nomeMusica.empty() ? std::string ("MAPA") : p.nomeMusica);
}

// Copia o áudio de referência para pasta/Audio/<nome original>; devolve a cópia em `destino`.
bool copiarAudio (const Projeto& p, const juce::File& pasta, juce::File& destino, juce::String& erro)
{
    const juce::String caminho = U (p.caminhoAudio);
    if (caminho.isEmpty() || ! juce::File::isAbsolutePath (caminho))
    {
        erro = tx ("Caminho do áudio de referência inválido.");
        return false;
    }
    const juce::File origem (caminho);
    if (! origem.existsAsFile())
    {
        erro = tx ("Áudio de referência não encontrado: ") + caminho;
        return false;
    }
    const juce::File pastaAudio = pasta.getChildFile ("Audio");
    if (! garantirPasta (pastaAudio, erro))
        return false;

    destino = pastaAudio.getChildFile (origem.getFileName());
    if (destino == origem)
        return true;
    if (destino.existsAsFile() && destino.getSize() == origem.getSize())
        return true;   // já copiado numa exportação anterior (ex.: exportou Ableton e depois Reaper)
    if (! origem.copyFileTo (destino))
    {
        erro = tx ("Não consegui copiar o áudio para ") + destino.getFullPathName();
        return false;
    }
    return true;
}

// Abre um arquivo para escrita do zero (FileOutputStream ACRESCENTA ao fim se o arquivo já existe).
std::unique_ptr<juce::FileOutputStream> abrirSaida (const juce::File& f)
{
    if (f.existsAsFile())
        f.deleteFile();
    std::unique_ptr<juce::FileOutputStream> out (new juce::FileOutputStream (f));
    if (out->failedToOpen())
        return nullptr;
    return out;
}

bool gravarTexto (const juce::File& f, const juce::String& texto)
{
    std::unique_ptr<juce::FileOutputStream> out = abrirSaida (f);
    if (out == nullptr)
        return false;
    const bool ok = out->writeText (texto, false, false, nullptr);   // UTF-8, sem BOM, quebras como estão
    out->flush();
    return ok && ! out->getStatus().failed();
}

// Nome da parte (marcador) que contém o tempo t, ou "" se nenhuma.
std::string parteEm (const Projeto& p, double t)
{
    const double tpc = p.temposPorCompasso();
    for (const Parte& pa : p.partes)
        if (pa.compIni * tpc <= t + 1.0e-6 && t + 1.0e-6 < pa.compFim * tpc)
            return pa.nome;
    return std::string();
}

// Clipe recebe o nome da parte em que começa (ex.: "DROP"); sem parte, o nome da faixa.
juce::String nomeClipe (const Projeto& p, const Faixa& f, const Clipe& c)
{
    const std::string pa = parteEm (p, c.inicio);
    return U (pa.empty() ? f.nome : pa);
}

// Nota já recortada ao clipe: t relativo ao início do clipe, dentro de [0, tamanho).
struct NotaRel
{
    double t = 0, d = 0;
    int tom = 60, vel = 100;
};

std::vector<NotaRel> notasDoClipe (const Clipe& c)
{
    std::vector<NotaRel> v;
    const double tam = c.fim - c.inicio;
    for (const Nota& n : c.notas)
    {
        const double t = n.inicio - c.inicio;
        if (t < 0.0 || t >= tam)
            continue;   // fora do clipe: descartada
        NotaRel r;
        r.t = t;
        r.d = std::min (std::max (n.dur, 1.0 / 64.0), tam - t);
        r.tom = juce::jlimit (0, 127, n.tom);
        r.vel = juce::jlimit (1, 127, n.vel);
        v.push_back (r);
    }
    std::sort (v.begin(), v.end(), [] (const NotaRel& a, const NotaRel& b)
               { return a.t != b.t ? a.t < b.t : a.tom < b.tom; });
    return v;
}

// Clipes válidos (fim > início) em ordem de início.
std::vector<const Clipe*> clipesOrdenados (const Faixa& f)
{
    std::vector<const Clipe*> v;
    for (const Clipe& c : f.clipes)
        if (c.fim > c.inicio)
            v.push_back (&c);
    std::sort (v.begin(), v.end(), [] (const Clipe* a, const Clipe* b) { return a->inicio < b->inicio; });
    return v;
}

juce::String hexCor (uint32_t rgb)
{
    return "#" + juce::String::toHexString ((int) (rgb & 0xffffffu)).paddedLeft ('0', 6);
}

juce::XmlElement::TextFormat formatoXml()
{
    juce::XmlElement::TextFormat f;
    f.lineWrapLength = 1 << 20;   // não quebra atributos em várias linhas
    f.newLineChars = "\n";
    return f;
}

// ================================================================================================
// 1) Pasta MIDI (FL Studio, Logic, Pro Tools e qualquer DAW)
// ================================================================================================

constexpr int ppq = 960;

double ticks (double tempos) { return std::round (tempos * ppq); }

void cabecalhoTempo (const Projeto& p, juce::MidiMessageSequence& seq)
{
    const int usPorTempo = juce::roundToInt (60000000.0 / (p.bpm > 0.0 ? p.bpm : 120.0));
    juce::MidiMessage tempo = juce::MidiMessage::tempoMetaEvent (usPorTempo);
    tempo.setTimeStamp (0.0);
    seq.addEvent (tempo);
    juce::MidiMessage ts = juce::MidiMessage::timeSignatureMetaEvent (p.numerador, p.denominador);
    ts.setTimeStamp (0.0);
    seq.addEvent (ts);
}

void addMeta (juce::MidiMessageSequence& seq, int tipo, const juce::String& texto, double tick)
{
    juce::MidiMessage m = juce::MidiMessage::textMetaEvent (tipo, texto);
    m.setTimeStamp (tick);
    seq.addEvent (m);
}

// End-of-track explícito no tick dado (o MidiFile só acrescenta outro se a faixa não tiver um).
void addFimDaFaixa (juce::MidiMessageSequence& seq, double tick)
{
    juce::MidiMessage m = juce::MidiMessage::endOfTrack();
    m.setTimeStamp (std::max (tick, seq.getEndTime()));
    seq.addEvent (m);   // fica por último: addEvent insere depois dos eventos de mesmo tempo
}

bool gravarMidi (const juce::MidiFile& mf, const juce::File& f)
{
    std::unique_ptr<juce::FileOutputStream> out = abrirSaida (f);
    if (out == nullptr)
        return false;
    const bool ok = mf.writeTo (*out, 1);
    out->flush();
    return ok && ! out->getStatus().failed();
}

juce::String textoLeiaMe (const Projeto& p, bool comNotas, const juce::File& audio)
{
    const double tpc = p.temposPorCompasso();
    const juce::String nl ("\r\n");
    juce::String t;
    t << "MAPA - InsideStudioSP" << nl
      << "=====================" << nl << nl
      << tx ("Música de referência: ") << U (p.nomeMusica) << nl
      << "BPM: " << fmtNum (std::round (p.bpm * 100.0) / 100.0) << nl
      << "Tom: " << U (p.tomTexto) << nl
      << "Compasso: " << p.numerador << "/" << p.denominador << nl
      << "Total de compassos: " << p.totalCompassos << nl << nl;

    t << "PARTES (compassos contados a partir de 1)" << nl;
    for (const Parte& pa : p.partes)
        t << "  - " << U (pa.nome) << ": compasso " << (pa.compIni + 1) << tx (" até ") << pa.compFim
          << " (" << (pa.compFim - pa.compIni) << " compassos)" << nl;
    t << nl;

    const int comp = (int) std::floor (p.audioInicioTempos / tpc + 1.0e-9);
    const double tempoNoComp = p.audioInicioTempos - comp * tpc;
    t << tx ("ÁUDIO DE REFERÊNCIA") << nl
      << "  Arquivo: Audio/" << audio.getFileName() << nl
      << tx ("  Começa no compasso ") << (comp + 1) << ", tempo " << fmtNum (tempoNoComp + 1.0)
      << " (= " << fmtNum (p.audioInicioTempos) << " tempos = "
      << fmtNum (p.audioInicioTempos * p.segPorTempo()) << tx (" s do início do projeto).") << nl;
    if (p.audioCorteSeg > 0.0)
        t << tx ("  ATENÇÃO: o clipe começa ") << fmtNum (p.audioCorteSeg) << " s DENTRO do arquivo: corte esse"
          << tx (" trecho do início do áudio (ou deslize o arquivo ") << fmtNum (p.audioCorteSeg)
          << tx (" s para a esquerda da posição acima).") << nl;
    t << nl;

    t << "ARQUIVOS MIDI (pasta MIDI)" << nl
      << "  00 ESTRUTURA.mid = tempo, compasso e um marcador por parte." << nl;
    if (comNotas)
        t << "  Os demais = uma faixa cada, com as notas." << nl;
    else
        t << tx ("  Os demais = uma faixa cada, só com a estrutura (marcador no início de cada trecho, sem notas).") << nl;
    t << "  Bateria vem no canal 10 (GM: 36 kick, 39 clap, 42 hat)." << nl << nl;

    t << "COMO ABRIR" << nl
      << "FL Studio:" << nl
      << "  1. Ajuste o tempo do projeto para o BPM acima." << nl
      << tx ("  2. Arraste os .mid da pasta MIDI para a Playlist (cada um vira um padrão/canal).") << nl
      << tx ("  3. Arraste o áudio da pasta Audio para a Playlist, na posição indicada acima.") << nl
      << "Logic Pro:" << nl
      << tx ("  1. Arraste os .mid para a área de trilhas, no compasso 1 (o 00 ESTRUTURA.mid traz tempo e") << nl
      << "     marcadores; aceite importar o tempo quando o Logic perguntar)." << nl
      << tx ("  2. Arraste o áudio para uma trilha de áudio, na posição indicada acima.") << nl
      << "Pro Tools:" << nl
      << "  1. File > Import > MIDI, destino Session Start, uma trilha por .mid; importe o tempo e os" << nl
      << "     marcadores do 00 ESTRUTURA.mid." << nl
      << tx ("  2. File > Import > Audio e posicione a referência (Spot) na posição indicada acima.") << nl
      << tx ("Outras DAWs: importe os .mid no início do projeto (compasso 1) e o áudio na posição indicada.") << nl;
    return t;
}

// ================================================================================================
// 2) Ableton Live (.als = XML com gzip), a partir do modelo real embutido (Recursos/modelo.als)
// ================================================================================================

// Paleta de 70 cores de clipe/faixa do Live (índice 0..69), RGB.
const uint32_t paletaLive[70] = {
    0xFF94A6, 0xFFA529, 0xCC9927, 0xF7F47C, 0xBFFB00, 0x1AFF2F, 0x25FFA8, 0x5CFFE8, 0x8BC5FF, 0x5480E4,
    0x92A7FF, 0xD86CE4, 0xE553A0, 0xFFFFFF, 0xFF3636, 0xF66C03, 0x99724B, 0xFFF034, 0x87FF67, 0x3DC300,
    0x00BFAF, 0x19E9FF, 0x10A4EE, 0x007DC0, 0x886CE4, 0xB677C6, 0xFF39D4, 0xD0D0D0, 0xE2675A, 0xFFA374,
    0xD3AD71, 0xEDFFAE, 0xD2E498, 0xBAD074, 0x9BC48D, 0xD4FDE1, 0xCDF1F8, 0xB9C1E3, 0xCDBBE4, 0xAE98E5,
    0xE5DCE1, 0xA9A9A9, 0xC6928B, 0xB78256, 0x99836A, 0xBFBA69, 0xA6BE00, 0x7DB04D, 0x88C2BA, 0x9BB3C4,
    0x85A5C2, 0x8393CC, 0xA595B5, 0xBF9FBE, 0xBC7196, 0x7B7B7B, 0xAF3333, 0xA95131, 0x724F41, 0xDBC300,
    0x85961F, 0x539F31, 0x0A9C8E, 0x236384, 0x1A2F96, 0x2F52A2, 0x624BAD, 0xA34BAD, 0xCC2E6E, 0x3C3C3C,
};

// Índice da cor mais próxima na paleta (distância "redmean", aproximação perceptual barata).
int corLive (uint32_t rgb)
{
    const double r = (double) ((rgb >> 16) & 0xffu), g = (double) ((rgb >> 8) & 0xffu), b = (double) (rgb & 0xffu);
    int melhor = 0;
    double dmin = 1.0e18;
    for (int i = 0; i < 70; ++i)
    {
        const uint32_t c = paletaLive[i];
        const double cr = (double) ((c >> 16) & 0xffu), cg = (double) ((c >> 8) & 0xffu), cb = (double) (c & 0xffu);
        const double rm = (r + cr) / 2.0;
        const double d = (2.0 + rm / 256.0) * (r - cr) * (r - cr) + 4.0 * (g - cg) * (g - cg)
                         + (2.0 + (255.0 - rm) / 256.0) * (b - cb) * (b - cb);
        if (d < dmin)
        {
            dmin = d;
            melhor = i;
        }
    }
    return melhor;
}

// Elementos cujo Id vive no espaço global de "pointees" do Live (controlado por NextPointeeId).
// Id repetido aqui = set "corrupt". Os demais Id (ClipSlot, TrackSendHolder, AutomationLane, clipes...)
// são índices locais de lista e ficam como o Live gravou no modelo.
bool ehAlvo (const juce::String& tag)
{
    return tag.endsWith ("Target") || tag == "Pointee" || tag.startsWith ("ControllerTargets.");
}

// Visita e e todos os descendentes (só elementos; nós de texto são ignorados).
void paraCada (juce::XmlElement& e, const std::function<void (juce::XmlElement&)>& fn)
{
    fn (e);
    for (juce::XmlElement* c = e.getFirstChildElement(); c != nullptr; c = c->getNextElement())
        if (! c->isTextElement())
            paraCada (*c, fn);
}

juce::int64 maiorIdAlvo (juce::XmlElement& raiz)
{
    juce::int64 maior = 0;
    paraCada (raiz, [&maior] (juce::XmlElement& e)
    {
        if (ehAlvo (e.getTagName()) && e.hasAttribute ("Id"))
            maior = std::max (maior, e.getStringAttribute ("Id").getLargeIntValue());
    });
    return maior;
}

// Navega por "A/B/C" (filhos diretos). nullptr se algum não existir.
juce::XmlElement* filho (juce::XmlElement* e, const juce::String& caminho)
{
    const juce::StringArray partes = juce::StringArray::fromTokens (caminho, "/", "");
    for (const juce::String& s : partes)
    {
        if (e == nullptr)
            return nullptr;
        e = e->getChildByName (s);
    }
    return e;
}

void setValor (juce::XmlElement* e, const juce::String& caminho, const juce::String& valor)
{
    if (juce::XmlElement* x = filho (e, caminho))
        x->setAttribute ("Value", valor);
}

// Cria <tag Value="valor"/> como último filho.
juce::XmlElement* sub (juce::XmlElement& pai, const juce::String& tag, const juce::String& valor)
{
    juce::XmlElement* e = pai.createNewChildElement (tag);
    e->setAttribute ("Value", valor);
    return e;
}

juce::XmlElement* subVazio (juce::XmlElement& pai, const juce::String& tag)
{
    return pai.createNewChildElement (tag);
}

void timeSignatureClipe (juce::XmlElement& clipe, const Projeto& p)
{
    juce::XmlElement* lista = subVazio (*subVazio (clipe, "TimeSignature"), "TimeSignatures");
    juce::XmlElement* r = subVazio (*lista, "RemoteableTimeSignature");
    r->setAttribute ("Id", juce::String ("0"));
    sub (*r, "Numerator", juce::String (p.numerador));
    sub (*r, "Denominator", juce::String (p.denominador));
    sub (*r, "Time", "0");
}

// Clipe de áudio da referência, SEM warp (toca no tempo original do arquivo, imune a erro de BPM).
// Sem warp, Loop/LoopStart e LoopEnd ficam em SEGUNDOS do arquivo e CurrentStart/CurrentEnd em tempos do
// arranjo (convenção que o logic2ableton conferiu num set salvo pelo Live 12.4.3). O corte vira LoopStart.
juce::XmlElement* clipeAudioLive (const Projeto& p, int cor, const juce::File& audio, const juce::String& relativo)
{
    const double ini = p.audioInicioTempos;
    const double fim = p.audioFimTempos();
    const double corte = std::max (0.0, p.audioCorteSeg);

    juce::XmlElement* c = new juce::XmlElement ("AudioClip");
    c->setAttribute ("Id", juce::String ("0"));
    c->setAttribute ("Time", fmtNum (ini));
    sub (*c, "LomId", "0");
    sub (*c, "LomIdView", "0");
    sub (*c, "CurrentStart", fmtNum (ini));
    sub (*c, "CurrentEnd", fmtNum (fim));
    juce::XmlElement* loop = subVazio (*c, "Loop");
    sub (*loop, "LoopStart", fmtNum (corte));
    sub (*loop, "LoopEnd", fmtNum (p.duracaoSeg));
    sub (*loop, "StartRelative", "0");
    sub (*loop, "LoopOn", "false");
    sub (*c, "Name", U (p.nomeMusica));
    sub (*c, "Annotation", "");
    sub (*c, "Color", juce::String (cor));
    sub (*c, "LaunchMode", "0");
    sub (*c, "LaunchQuantisation", "0");
    timeSignatureClipe (*c, p);
    subVazio (*subVazio (*c, "Envelopes"), "Envelopes");
    sub (*c, "Legato", "false");
    sub (*c, "Ram", "false");
    sub (*subVazio (*c, "GrooveSettings"), "GrooveId", "-1");
    sub (*c, "Disabled", "false");
    sub (*c, "VelocityAmount", "0");
    sub (*c, "FreezeStart", "0");
    sub (*c, "FreezeEnd", "0");
    sub (*c, "IsWarped", "false");
    sub (*c, "TakeId", "1");

    juce::XmlElement* sr = subVazio (*c, "SampleRef");
    juce::XmlElement* fr = subVazio (*sr, "FileRef");
    sub (*fr, "RelativePathType", "1");   // relativo ao .als
    sub (*fr, "RelativePath", relativo);
    sub (*fr, "Path", audio.getFullPathName().replaceCharacter ('\\', '/'));
    sub (*fr, "Type", "1");
    sub (*fr, "LivePackName", "");
    sub (*fr, "LivePackId", "");
    sub (*fr, "OriginalFileSize", juce::String (audio.getSize()));
    sub (*fr, "OriginalCrc", "0");
    sub (*sr, "LastModDate", juce::String (audio.getLastModificationTime().toMilliseconds() / 1000));
    subVazio (*sr, "SourceContext");
    sub (*sr, "SampleUsageHint", "0");
    sub (*sr, "DefaultDuration", juce::String ((juce::int64) std::llround (p.duracaoSeg * p.sampleRate)));
    sub (*sr, "DefaultSampleRate", juce::String (juce::roundToInt (p.sampleRate)));

    sub (*c, "WarpMode", "0");
    juce::XmlElement* fades = subVazio (*c, "Fades");
    sub (*fades, "FadeInLength", "0");
    sub (*fades, "FadeOutLength", "0");
    sub (*fades, "IsDefaultFadeIn", "true");
    sub (*fades, "IsDefaultFadeOut", "true");

    // 2 WarpMarkers coerentes com o BPM (1 s = bpm/60 tempos): se o usuário ligar o Warp, fica no grid.
    juce::XmlElement* wm = subVazio (*c, "WarpMarkers");
    juce::XmlElement* m0 = subVazio (*wm, "WarpMarker");
    m0->setAttribute ("Id", juce::String ("0"));
    m0->setAttribute ("SecTime", juce::String ("0"));
    m0->setAttribute ("BeatTime", juce::String ("0"));
    juce::XmlElement* m1 = subVazio (*wm, "WarpMarker");
    m1->setAttribute ("Id", juce::String ("1"));
    m1->setAttribute ("SecTime", fmtNum (p.duracaoSeg));
    m1->setAttribute ("BeatTime", fmtNum (p.duracaoSeg * p.bpm / 60.0));
    return c;
}

// Clipe MIDI do arranjo (formato Live 11/12: Notes > KeyTracks > KeyTrack > Notes > MidiNoteEvent com NoteId).
juce::XmlElement* clipeMidiLive (const Projeto& p, int indice, const juce::String& nome, int cor,
                                 const Clipe& clipe, bool comNotas)
{
    const double tam = clipe.fim - clipe.inicio;
    juce::XmlElement* c = new juce::XmlElement ("MidiClip");
    c->setAttribute ("Id", juce::String (indice));
    c->setAttribute ("Time", fmtNum (clipe.inicio));
    sub (*c, "LomId", "0");
    sub (*c, "LomIdView", "0");
    sub (*c, "CurrentStart", fmtNum (clipe.inicio));
    sub (*c, "CurrentEnd", fmtNum (clipe.fim));
    juce::XmlElement* loop = subVazio (*c, "Loop");
    sub (*loop, "LoopStart", "0");
    sub (*loop, "LoopEnd", fmtNum (tam));
    sub (*loop, "StartRelative", "0");
    sub (*loop, "LoopOn", "false");
    sub (*loop, "OutMarker", fmtNum (tam));
    sub (*loop, "HiddenLoopStart", "0");
    sub (*loop, "HiddenLoopEnd", fmtNum (tam));
    sub (*c, "Name", nome);
    sub (*c, "Annotation", "");
    sub (*c, "Color", juce::String (cor));
    sub (*c, "LaunchMode", "0");
    sub (*c, "LaunchQuantisation", "0");
    timeSignatureClipe (*c, p);
    subVazio (*subVazio (*c, "Envelopes"), "Envelopes");
    juce::XmlElement* st = subVazio (*c, "ScrollerTimePreserver");
    sub (*st, "LeftTime", "0");
    sub (*st, "RightTime", fmtNum (tam));
    juce::XmlElement* ts = subVazio (*c, "TimeSelection");
    sub (*ts, "AnchorTime", "0");
    sub (*ts, "OtherTime", "0");
    sub (*c, "Legato", "false");
    sub (*c, "Ram", "false");
    sub (*subVazio (*c, "GrooveSettings"), "GrooveId", "-1");
    sub (*c, "Disabled", "false");
    sub (*c, "VelocityAmount", "0");
    sub (*c, "FreezeStart", "0");
    sub (*c, "FreezeEnd", "0");
    sub (*c, "IsWarped", "true");
    sub (*c, "TakeId", "1");

    juce::XmlElement* notasEl = subVazio (*c, "Notes");
    juce::XmlElement* keyTracks = subVazio (*notasEl, "KeyTracks");
    int proximoNoteId = 1;
    if (comNotas)
    {
        // um KeyTrack por altura (MidiKey), notas em ordem de tempo
        std::map<int, std::vector<NotaRel>> porTom;
        for (const NotaRel& n : notasDoClipe (clipe))
            porTom[n.tom].push_back (n);
        int k = 0;
        for (const std::pair<const int, std::vector<NotaRel>>& par : porTom)
        {
            juce::XmlElement* kt = subVazio (*keyTracks, "KeyTrack");
            kt->setAttribute ("Id", juce::String (k++));
            juce::XmlElement* ne = subVazio (*kt, "Notes");
            for (const NotaRel& n : par.second)
            {
                juce::XmlElement* ev = subVazio (*ne, "MidiNoteEvent");
                ev->setAttribute ("Time", fmtNum (n.t));
                ev->setAttribute ("Duration", fmtNum (n.d));
                ev->setAttribute ("Velocity", juce::String (n.vel));
                ev->setAttribute ("OffVelocity", juce::String ("64"));
                ev->setAttribute ("NoteId", juce::String (proximoNoteId++));
            }
            sub (*kt, "MidiKey", juce::String (par.first));
        }
    }
    subVazio (*subVazio (*notasEl, "PerNoteEventStore"), "EventLists");
    subVazio (*notasEl, "NoteProbabilityGroups");
    sub (*subVazio (*notasEl, "ProbabilityGroupIdGenerator"), "NextId", "1");
    sub (*subVazio (*notasEl, "NoteIdGenerator"), "NextId", juce::String (proximoNoteId));
    sub (*c, "BankSelectCoarse", "-1");
    sub (*c, "BankSelectFine", "-1");
    sub (*c, "ProgramChange", "-1");
    return c;
}

// Esvazia e devolve o <Events> do arranjo de uma faixa clonada.
juce::XmlElement* eventosArranjo (juce::XmlElement* faixa, bool midi)
{
    const juce::String caminho = midi ? juce::String ("DeviceChain/MainSequencer/ClipTimeable/ArrangerAutomation/Events")
                                      : juce::String ("DeviceChain/MainSequencer/Sample/ArrangerAutomation/Events");
    juce::XmlElement* ev = filho (faixa, caminho);
    if (ev != nullptr)
        ev->deleteAllChildElements();
    return ev;
}

// Grava `valor` no Manual do parâmetro global (Tempo / TimeSignature do MainTrack) E nos eventos da automação
// dele (o FloatEvent/EnumEvent em Time="-63072000" é o valor inicial que o Live usa ao tocar).
void setParametroGlobal (juce::XmlElement* principal, const juce::String& nomePar, const juce::String& valor)
{
    juce::XmlElement* par = filho (principal, "DeviceChain/Mixer/" + nomePar);
    if (par == nullptr)
        return;
    setValor (par, "Manual", valor);
    juce::XmlElement* alvo = par->getChildByName ("AutomationTarget");
    if (alvo == nullptr)
        return;
    const juce::String idAlvo = alvo->getStringAttribute ("Id");
    juce::XmlElement* envs = filho (principal, "AutomationEnvelopes/Envelopes");
    if (envs == nullptr)
        return;
    for (juce::XmlElement* env = envs->getFirstChildElement(); env != nullptr; env = env->getNextElement())
    {
        juce::XmlElement* pid = filho (env, "EnvelopeTarget/PointeeId");
        if (pid == nullptr || pid->getStringAttribute ("Value") != idAlvo)
            continue;
        if (juce::XmlElement* evs = filho (env, "Automation/Events"))
            for (juce::XmlElement* e = evs->getFirstChildElement(); e != nullptr; e = e->getNextElement())
                e->setAttribute ("Value", valor);
    }
}

// Compasso no formato do Live: 99 * log2(denominador) + numerador - 1  (4/4 = 201).
int compassoLive (int num, int den)
{
    int pot = -1;
    switch (den)
    {
        case 1:  pot = 0; break;
        case 2:  pot = 1; break;
        case 4:  pot = 2; break;
        case 8:  pot = 3; break;
        case 16: pot = 4; break;
        default: break;
    }
    if (pot < 0 || num < 1 || num > 99)
        return 201;
    return 99 * pot + num - 1;
}

// ================================================================================================
// 3) Reaper (.rpp, texto)
// ================================================================================================

// Cor no formato do Reaper: 0x1000000 | BGR (bit 24 = "cor definida").
int corReaper (uint32_t rgb)
{
    const int r = (int) ((rgb >> 16) & 0xffu), g = (int) ((rgb >> 8) & 0xffu), b = (int) (rgb & 0xffu);
    return 0x1000000 | (b << 16) | (g << 8) | r;
}

// Texto entre aspas para o .rpp (o Reaper não tem escape; troca " por ' e tira quebras de linha).
juce::String aspasRpp (const juce::String& s)
{
    return "\"" + s.replaceCharacter ('"', '\'').removeCharacters ("\r\n") + "\"";
}

juce::String tipoFonteReaper (const juce::File& f)
{
    const juce::String ext = f.getFileExtension().toLowerCase();
    if (ext == ".mp3")  return "MP3";
    if (ext == ".flac") return "FLAC";
    if (ext == ".ogg")  return "VORBIS";
    if (ext == ".opus") return "OPUS";
    return "WAVE";   // .wav, .aif/.aiff, .w64...
}

juce::String hex2 (int v) { return juce::String::toHexString (v & 0xff).paddedLeft ('0', 2); }

struct EventoRpp
{
    int tick = 0, prio = 0, st = 0, d1 = 0, d2 = 0;   // prio: 0 = note-off, 1 = note-on (off antes no mesmo tick)
};

// ================================================================================================
// 4) DAWproject (zip com project.xml + metadata.xml + audio/)
// ================================================================================================

struct GeradorIds
{
    int n = 0;
    juce::String proximo() { return "id" + juce::String (n++); }
};

void canalDawproject (juce::XmlElement& canal, GeradorIds& ids)
{
    juce::XmlElement* mute = canal.createNewChildElement ("Mute");
    mute->setAttribute ("value", juce::String ("false"));
    mute->setAttribute ("id", ids.proximo());
    mute->setAttribute ("name", juce::String ("Mute"));
    juce::XmlElement* pan = canal.createNewChildElement ("Pan");
    pan->setAttribute ("max", juce::String ("1.0"));
    pan->setAttribute ("min", juce::String ("0.0"));
    pan->setAttribute ("unit", juce::String ("normalized"));
    pan->setAttribute ("value", juce::String ("0.5"));
    pan->setAttribute ("id", ids.proximo());
    pan->setAttribute ("name", juce::String ("Pan"));
    juce::XmlElement* vol = canal.createNewChildElement ("Volume");
    vol->setAttribute ("max", juce::String ("2.0"));
    vol->setAttribute ("min", juce::String ("0.0"));
    vol->setAttribute ("unit", juce::String ("linear"));
    vol->setAttribute ("value", juce::String ("1.0"));
    vol->setAttribute ("id", ids.proximo());
    vol->setAttribute ("name", juce::String ("Volume"));
}

void addTextoZip (juce::ZipFile::Builder& zip, const juce::String& nome, const juce::String& texto)
{
    juce::MemoryBlock bloco (texto.toRawUTF8(), texto.getNumBytesAsUTF8());
    zip.addEntry (new juce::MemoryInputStream (bloco, true), 9, nome, juce::Time::getCurrentTime());
}

} // namespace (anônimo)

// ================================================================================================
// API pública
// ================================================================================================

juce::String nomeSeguro (const std::string& nome)
{
    const juce::String s = U (nome);
    juce::String r;
    for (int i = 0; i < s.length(); ++i)
    {
        const juce::juce_wchar c = s[i];
        const bool proibido = c < 32 || c == 127 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?'
                              || c == '"' || c == '<' || c == '>' || c == '|';
        r += juce::String::charToString (proibido ? (juce::juce_wchar) '_' : c);
    }
    r = r.trim();
    while (r.endsWithChar ('.') || r.endsWithChar (' '))
        r = r.dropLastCharacters (1);
    if (r.length() > 120)
        r = r.substring (0, 120).trimEnd();
    if (r.isEmpty())
        r = "MAPA";
    return r;
}

// ------------------------------------------------------------------------------------------------ pasta MIDI
ResultadoExport exportarPasta (const Projeto& p, const juce::File& pasta, bool comNotas)
{
    juce::String erro;
    if (! garantirPasta (pasta, erro))
        return falha (erro);
    juce::File audio;
    if (! copiarAudio (p, pasta, audio, erro))
        return falha (erro);
    const juce::File pastaMidi = pasta.getChildFile ("MIDI");
    if (! garantirPasta (pastaMidi, erro))
        return falha (erro);

    const double tpc = p.temposPorCompasso();

    // 00 ESTRUTURA.mid: tempo, compasso e um marcador (meta 06) por parte, no compasso de início
    {
        juce::MidiMessageSequence seq;
        cabecalhoTempo (p, seq);
        addMeta (seq, 3, "ESTRUTURA", 0.0);
        for (const Parte& pa : p.partes)
            addMeta (seq, 6, U (pa.nome), ticks (pa.compIni * tpc));
        addFimDaFaixa (seq, ticks (std::max (p.totalCompassos * tpc, p.audioFimTempos())));
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (seq);
        if (! gravarMidi (mf, pastaMidi.getChildFile ("00 ESTRUTURA.mid")))
            return falha (tx ("Não consegui gravar MIDI/00 ESTRUTURA.mid"));
    }

    // um .mid (tipo 1) por faixa MIDI: trilha 0 = tempo/compasso; trilha 1 = nome, marcador de cada clipe e notas
    for (size_t i = 1; i < p.faixas.size(); ++i)
    {
        const Faixa& f = p.faixas[i];
        if (f.tipo != TipoFaixa::midi)
            continue;
        const int canal = f.bateria ? 10 : 1;   // JUCE: canais 1..16

        juce::MidiMessageSequence conducao;
        cabecalhoTempo (p, conducao);
        addFimDaFaixa (conducao, 0.0);

        juce::MidiMessageSequence seq;
        addMeta (seq, 3, U (f.nome), 0.0);
        double fim = 0.0;
        for (const Clipe* c : clipesOrdenados (f))
        {
            addMeta (seq, 6, nomeClipe (p, f, *c), ticks (c->inicio));   // marcador do trecho (também no grátis)
            fim = std::max (fim, c->fim);
            if (! comNotas)
                continue;
            for (const NotaRel& n : notasDoClipe (*c))   // em ordem de tempo: note-off sai antes do próximo note-on
            {
                const double tOn = ticks (c->inicio + n.t);
                juce::MidiMessage on = juce::MidiMessage::noteOn (canal, n.tom, (juce::uint8) n.vel);
                on.setTimeStamp (tOn);
                seq.addEvent (on);
                juce::MidiMessage off = juce::MidiMessage::noteOff (canal, n.tom);
                off.setTimeStamp (std::max (tOn + 1.0, ticks (c->inicio + n.t + n.d)));
                seq.addEvent (off);
            }
        }
        addFimDaFaixa (seq, ticks (fim));

        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (conducao);
        mf.addTrack (seq);
        const juce::String nomeArq = juce::String ((int) i).paddedLeft ('0', 2) + " " + nomeSeguro (f.nome) + ".mid";
        if (! gravarMidi (mf, pastaMidi.getChildFile (nomeArq)))
            return falha (tx ("Não consegui gravar MIDI/") + nomeArq);
    }

    if (! gravarTexto (pasta.getChildFile ("LEIA-ME.txt"), textoLeiaMe (p, comNotas, audio)))
        return falha (tx ("Não consegui gravar LEIA-ME.txt"));

    return sucesso (pastaMidi);
}

// ------------------------------------------------------------------------------------------------ Ableton
ResultadoExport exportarAbleton (const Projeto& p, const juce::File& pasta, bool comNotas)
{
    juce::String erro;
    if (! garantirPasta (pasta, erro))
        return falha (erro);
    juce::File audio;
    if (! copiarAudio (p, pasta, audio, erro))
        return falha (erro);

    // 1. descompacta o modelo embutido
    juce::String texto;
    {
        juce::MemoryInputStream mis (MapaDados::modelo_als, (size_t) MapaDados::modelo_alsSize, false);
        juce::GZIPDecompressorInputStream gz (&mis, false, juce::GZIPDecompressorInputStream::gzipFormat);
        texto = gz.readEntireStreamAsString();
    }
    std::unique_ptr<juce::XmlElement> raiz = juce::XmlDocument::parse (texto);
    juce::XmlElement* liveSet = raiz != nullptr ? raiz->getChildByName ("LiveSet") : nullptr;
    juce::XmlElement* tracks = filho (liveSet, "Tracks");
    juce::XmlElement* principal = liveSet != nullptr ? liveSet->getChildByName ("MainTrack") : nullptr;
    if (principal == nullptr && liveSet != nullptr)
        principal = liveSet->getChildByName ("MasterTrack");   // modelo do Live 11
    juce::XmlElement* nextPointee = liveSet != nullptr ? liveSet->getChildByName ("NextPointeeId") : nullptr;
    if (tracks == nullptr || principal == nullptr || nextPointee == nullptr)
        return falha (tx ("Modelo do Ableton inválido (embutido no app)."));

    // 2. próximo Id de alvo livre = max(NextPointeeId, maior Id de alvo do modelo inteiro + 1)
    juce::int64 proximo = std::max (nextPointee->getStringAttribute ("Value").getLargeIntValue(), maiorIdAlvo (*raiz) + 1);

    // 3. moldes (cópias) e return tracks; esvazia <Tracks>
    std::unique_ptr<juce::XmlElement> moldeAudio, moldeMidi;
    std::vector<std::unique_ptr<juce::XmlElement>> returns;
    int maiorIdFaixa = 0;
    for (juce::XmlElement* t = tracks->getFirstChildElement(); t != nullptr; t = t->getNextElement())
    {
        maiorIdFaixa = std::max (maiorIdFaixa, t->getIntAttribute ("Id", 0));
        if (t->hasTagName ("AudioTrack") && moldeAudio == nullptr)
            moldeAudio.reset (new juce::XmlElement (*t));
        else if (t->hasTagName ("MidiTrack") && moldeMidi == nullptr)
            moldeMidi.reset (new juce::XmlElement (*t));
        else if (t->hasTagName ("ReturnTrack"))
            returns.push_back (std::unique_ptr<juce::XmlElement> (new juce::XmlElement (*t)));
    }
    if (moldeAudio == nullptr || moldeMidi == nullptr)
        return falha (tx ("Modelo do Ableton sem faixa de áudio/MIDI."));
    tracks->deleteAllChildElements();
    int proximoIdFaixa = maiorIdFaixa + 1;   // Ids de faixa são um espaço à parte dos alvos

    const juce::String relativo = "Audio/" + audio.getFileName();

    // 4. uma faixa clonada por faixa do Projeto
    for (size_t i = 0; i < p.faixas.size(); ++i)
    {
        const Faixa& f = p.faixas[i];
        const bool ehAudio = (f.tipo == TipoFaixa::audio);
        juce::XmlElement* t = new juce::XmlElement (ehAudio ? *moldeAudio : *moldeMidi);
        tracks->addChildElement (t);   // a árvore passa a ser dona

        // renumera Ids de alvo e corrige os PointeeId (envelopes) que apontavam para eles
        std::map<juce::String, juce::String> mapaIds;
        paraCada (*t, [&mapaIds, &proximo] (juce::XmlElement& e)
        {
            if (ehAlvo (e.getTagName()) && e.hasAttribute ("Id"))
            {
                const juce::String novo (proximo++);
                mapaIds[e.getStringAttribute ("Id")] = novo;
                e.setAttribute ("Id", novo);
            }
        });
        paraCada (*t, [&mapaIds] (juce::XmlElement& e)
        {
            if (e.hasTagName ("PointeeId"))
            {
                const std::map<juce::String, juce::String>::const_iterator it = mapaIds.find (e.getStringAttribute ("Value"));
                if (it != mapaIds.end())
                    e.setAttribute ("Value", it->second);
            }
        });

        t->setAttribute ("Id", juce::String (proximoIdFaixa++));
        setValor (t, "Name/EffectiveName", U (f.nome));
        setValor (t, "Name/UserName", U (f.nome));
        const int cor = corLive (f.cor);
        setValor (t, "Color", juce::String (cor));
        setValor (t, "TrackGroupId", "-1");

        juce::XmlElement* eventos = eventosArranjo (t, ! ehAudio);
        if (eventos == nullptr)
            return falha ("Modelo do Ableton: faixa sem arranjo (ArrangerAutomation/Events).");

        if (ehAudio)
        {
            if (i == 0)   // só a referência (faixas[0]) tem o arquivo
                eventos->addChildElement (clipeAudioLive (p, cor, audio, relativo));
        }
        else
        {
            int indice = 0;
            for (const Clipe* c : clipesOrdenados (f))
                eventos->addChildElement (clipeMidiLive (p, indice++, nomeClipe (p, f, *c), cor, *c, comNotas));
        }
    }
    for (std::unique_ptr<juce::XmlElement>& r : returns)
        tracks->addChildElement (r.release());   // returns depois das faixas, como o Live grava

    // 5. tempo e compasso (Manual + evento inicial da automação)
    setParametroGlobal (principal, "Tempo", fmtNum (p.bpm));
    setParametroGlobal (principal, "TimeSignature", juce::String (compassoLive (p.numerador, p.denominador)));

    // 6. locators: um por parte, Time em tempos; Ids locais 0..n (como o Live grava)
    if (juce::XmlElement* locs = filho (liveSet, "Locators/Locators"))
    {
        locs->deleteAllChildElements();
        const double tpc = p.temposPorCompasso();
        int n = 0;
        for (const Parte& pa : p.partes)
        {
            juce::XmlElement* lo = subVazio (*locs, "Locator");
            lo->setAttribute ("Id", juce::String (n++));
            sub (*lo, "LomId", "0");
            sub (*lo, "Time", fmtNum (pa.compIni * tpc));
            sub (*lo, "Name", U (pa.nome));
            sub (*lo, "Annotation", "");
            sub (*lo, "IsSongStart", "false");
        }
    }

    nextPointee->setAttribute ("Value", juce::String (proximo));

    // 7. grava com gzip
    const juce::File arq = pasta.getChildFile (nomeProjeto (p) + ".als");
    std::unique_ptr<juce::FileOutputStream> out = abrirSaida (arq);
    if (out == nullptr)
        return falha (tx ("Não consegui gravar ") + arq.getFullPathName());
    {
        juce::GZIPCompressorOutputStream gz (*out, 9, juce::GZIPCompressorOutputStream::windowBitsGZIP);
        raiz->writeTo (gz, formatoXml());
        gz.flush();   // fecha o stream gzip (o destrutor também faria)
    }
    out->flush();
    if (out->getStatus().failed())
        return falha ("Erro ao gravar " + arq.getFullPathName() + ": " + out->getStatus().getErrorMessage());
    return sucesso (arq);
}

// ------------------------------------------------------------------------------------------------ Reaper
ResultadoExport exportarReaper (const Projeto& p, const juce::File& pasta, bool comNotas)
{
    juce::String erro;
    if (! garantirPasta (pasta, erro))
        return falha (erro);
    juce::File audio;
    if (! copiarAudio (p, pasta, audio, erro))
        return falha (erro);

    const double spb = p.segPorTempo();
    const double tpc = p.temposPorCompasso();
    const juce::String nl ("\n");
    juce::String r;
    r << "<REAPER_PROJECT 0.1 \"6.0\" 0" << nl;
    r << "  TEMPO " << fmtNum (p.bpm) << " " << p.numerador << " " << p.denominador << nl;

    int idx = 1;
    for (const Parte& pa : p.partes)
        r << "  MARKER " << idx++ << " " << fmtNum (pa.compIni * tpc * spb) << " " << aspasRpp (U (pa.nome))
          << " 0 " << corReaper (pa.cor) << nl;

    for (size_t i = 0; i < p.faixas.size(); ++i)
    {
        const Faixa& f = p.faixas[i];
        r << "  <TRACK" << nl
          << "    NAME " << aspasRpp (U (f.nome)) << nl
          << "    PEAKCOL " << corReaper (f.cor) << nl;

        if (f.tipo == TipoFaixa::audio)
        {
            if (i == 0)
            {
                const double corte = std::max (0.0, p.audioCorteSeg);
                r << "    <ITEM" << nl
                  << "      POSITION " << fmtNum (p.audioInicioTempos * spb) << nl
                  << "      LENGTH " << fmtNum (p.duracaoSeg - corte) << nl
                  << "      LOOP 0" << nl
                  << "      SOFFS " << fmtNum (corte) << nl
                  << "      NAME " << aspasRpp (U (p.nomeMusica)) << nl
                  << "      <SOURCE " << tipoFonteReaper (audio) << nl
                  << "        FILE " << aspasRpp ("Audio/" + audio.getFileName()) << nl
                  << "      >" << nl
                  << "    >" << nl;
            }
        }
        else
        {
            const int canal = f.bateria ? 9 : 0;   // 0-based no byte de status
            for (const Clipe* c : clipesOrdenados (f))
            {
                const double tam = c->fim - c->inicio;
                const int fimTick = (int) ticks (tam);
                r << "    <ITEM" << nl
                  << "      POSITION " << fmtNum (c->inicio * spb) << nl
                  << "      LENGTH " << fmtNum (tam * spb) << nl
                  << "      LOOP 0" << nl
                  << "      NAME " << aspasRpp (nomeClipe (p, f, *c)) << nl
                  << "      <SOURCE MIDI" << nl
                  << "        HASDATA 1 960 QN" << nl;

                std::vector<EventoRpp> evs;
                if (comNotas)
                {
                    for (const NotaRel& n : notasDoClipe (*c))
                    {
                        EventoRpp on;
                        on.tick = (int) ticks (n.t);
                        on.prio = 1;
                        on.st = 0x90 | canal;
                        on.d1 = n.tom;
                        on.d2 = n.vel;
                        EventoRpp off;
                        off.tick = std::max (on.tick + 1, std::min (fimTick, (int) ticks (n.t + n.d)));
                        off.prio = 0;
                        off.st = 0x80 | canal;
                        off.d1 = n.tom;
                        off.d2 = 0;
                        evs.push_back (on);
                        evs.push_back (off);
                    }
                }
                std::stable_sort (evs.begin(), evs.end(), [] (const EventoRpp& a, const EventoRpp& b)
                                  { return a.tick != b.tick ? a.tick < b.tick : a.prio < b.prio; });
                int ultimo = 0;
                for (const EventoRpp& e : evs)
                {
                    r << "        E " << (e.tick - ultimo) << " " << hex2 (e.st) << " " << hex2 (e.d1) << " "
                      << hex2 (e.d2) << nl;
                    ultimo = e.tick;
                }
                // "all notes off" (CC 123) no fim: marca o comprimento da fonte MIDI
                r << "        E " << std::max (0, fimTick - ultimo) << " " << hex2 (0xb0 | canal) << " 7b 00" << nl
                  << "      >" << nl
                  << "    >" << nl;
            }
        }
        r << "  >" << nl;
    }
    r << ">" << nl;

    const juce::File arq = pasta.getChildFile (nomeProjeto (p) + ".rpp");
    if (! gravarTexto (arq, r))
        return falha (tx ("Não consegui gravar ") + arq.getFullPathName());
    return sucesso (arq);
}

// ------------------------------------------------------------------------------------------------ DAWproject
ResultadoExport exportarDawproject (const Projeto& p, const juce::File& pasta, bool comNotas)
{
    juce::String erro;
    if (! garantirPasta (pasta, erro))
        return falha (erro);
    juce::File audio;
    if (! copiarAudio (p, pasta, audio, erro))
        return falha (erro);

    GeradorIds ids;
    const double tpc = p.temposPorCompasso();
    const juce::String caminhoNoZip = "audio/" + audio.getFileName();

    juce::XmlElement projeto ("Project");
    projeto.setAttribute ("version", juce::String ("1.0"));
    juce::XmlElement* app = projeto.createNewChildElement ("Application");
    app->setAttribute ("name", juce::String ("MAPA - InsideStudioSP"));
    app->setAttribute ("version", juce::String ("1.0"));

    juce::XmlElement* transport = projeto.createNewChildElement ("Transport");
    juce::XmlElement* tempo = transport->createNewChildElement ("Tempo");
    tempo->setAttribute ("max", juce::String ("999.0"));
    tempo->setAttribute ("min", juce::String ("20.0"));
    tempo->setAttribute ("unit", juce::String ("bpm"));
    tempo->setAttribute ("value", fmtNum (p.bpm));
    tempo->setAttribute ("id", ids.proximo());
    tempo->setAttribute ("name", juce::String ("Tempo"));
    juce::XmlElement* ts = transport->createNewChildElement ("TimeSignature");
    ts->setAttribute ("denominator", juce::String (p.denominador));
    ts->setAttribute ("numerator", juce::String (p.numerador));
    ts->setAttribute ("id", ids.proximo());

    // Structure: uma Track por faixa + Master
    juce::XmlElement* estrutura = projeto.createNewChildElement ("Structure");
    std::vector<juce::String> idsFaixa;
    std::vector<juce::XmlElement*> canais;
    for (const Faixa& f : p.faixas)
    {
        juce::XmlElement* tr = estrutura->createNewChildElement ("Track");
        tr->setAttribute ("contentType", juce::String (f.tipo == TipoFaixa::audio ? "audio" : "notes"));
        tr->setAttribute ("loaded", juce::String ("true"));
        const juce::String idT = ids.proximo();
        tr->setAttribute ("id", idT);
        tr->setAttribute ("name", U (f.nome));
        tr->setAttribute ("color", hexCor (f.cor));
        idsFaixa.push_back (idT);
        juce::XmlElement* ch = tr->createNewChildElement ("Channel");
        ch->setAttribute ("audioChannels", juce::String ("2"));
        ch->setAttribute ("role", juce::String ("regular"));
        ch->setAttribute ("solo", juce::String ("false"));
        ch->setAttribute ("id", ids.proximo());
        canalDawproject (*ch, ids);
        canais.push_back (ch);
    }
    juce::XmlElement* master = estrutura->createNewChildElement ("Track");
    master->setAttribute ("contentType", juce::String ("audio notes"));
    master->setAttribute ("loaded", juce::String ("true"));
    const juce::String idMasterTrack = ids.proximo();
    master->setAttribute ("id", idMasterTrack);
    master->setAttribute ("name", juce::String ("Master"));
    juce::XmlElement* chMaster = master->createNewChildElement ("Channel");
    chMaster->setAttribute ("audioChannels", juce::String ("2"));
    chMaster->setAttribute ("role", juce::String ("master"));
    chMaster->setAttribute ("solo", juce::String ("false"));
    const juce::String idMaster = ids.proximo();
    chMaster->setAttribute ("id", idMaster);
    canalDawproject (*chMaster, ids);
    for (juce::XmlElement* ch : canais)
        ch->setAttribute ("destination", idMaster);

    // Arrangement > Lanes (beats) > Lanes por faixa > Clips > Clip
    juce::XmlElement* arranjo = projeto.createNewChildElement ("Arrangement");
    arranjo->setAttribute ("id", ids.proximo());
    juce::XmlElement* lanes = arranjo->createNewChildElement ("Lanes");
    lanes->setAttribute ("timeUnit", juce::String ("beats"));
    lanes->setAttribute ("id", ids.proximo());

    for (size_t i = 0; i < p.faixas.size(); ++i)
    {
        const Faixa& f = p.faixas[i];
        juce::XmlElement* lane = lanes->createNewChildElement ("Lanes");
        lane->setAttribute ("track", idsFaixa[i]);
        lane->setAttribute ("id", ids.proximo());
        juce::XmlElement* clips = lane->createNewChildElement ("Clips");
        clips->setAttribute ("id", ids.proximo());

        if (f.tipo == TipoFaixa::audio)
        {
            if (i != 0)
                continue;
            // Mesmo desenho que o Bitwig exporta: clipe externo no arranjo; clipe interno com Warps
            // (tempos <-> segundos do arquivo, coerentes com o BPM). O corte vira playStart do clipe interno.
            const double corte = std::max (0.0, p.audioCorteSeg);
            const double durTempos = p.audioFimTempos() - p.audioInicioTempos;
            const double arquivoTempos = p.duracaoSeg / p.segPorTempo();
            juce::XmlElement* ce = clips->createNewChildElement ("Clip");
            ce->setAttribute ("time", fmtNum (p.audioInicioTempos));
            ce->setAttribute ("duration", fmtNum (durTempos));
            ce->setAttribute ("playStart", juce::String ("0"));
            ce->setAttribute ("name", U (p.nomeMusica));
            juce::XmlElement* internos = ce->createNewChildElement ("Clips");
            internos->setAttribute ("id", ids.proximo());
            juce::XmlElement* ci = internos->createNewChildElement ("Clip");
            ci->setAttribute ("time", juce::String ("0"));
            ci->setAttribute ("duration", fmtNum (durTempos));
            ci->setAttribute ("contentTimeUnit", juce::String ("beats"));
            ci->setAttribute ("playStart", fmtNum (corte / p.segPorTempo()));
            juce::XmlElement* warps = ci->createNewChildElement ("Warps");
            warps->setAttribute ("contentTimeUnit", juce::String ("seconds"));
            warps->setAttribute ("timeUnit", juce::String ("beats"));
            warps->setAttribute ("id", ids.proximo());
            juce::XmlElement* au = warps->createNewChildElement ("Audio");
            au->setAttribute ("algorithm", juce::String ("stretch"));
            au->setAttribute ("channels", juce::String (juce::jmax (1, p.canais)));
            au->setAttribute ("duration", fmtNum (p.duracaoSeg));
            au->setAttribute ("sampleRate", juce::String (juce::roundToInt (p.sampleRate)));
            au->setAttribute ("id", ids.proximo());
            juce::XmlElement* arqEl = au->createNewChildElement ("File");
            arqEl->setAttribute ("path", caminhoNoZip);
            juce::XmlElement* w0 = warps->createNewChildElement ("Warp");
            w0->setAttribute ("time", juce::String ("0"));
            w0->setAttribute ("contentTime", juce::String ("0"));
            juce::XmlElement* w1 = warps->createNewChildElement ("Warp");
            w1->setAttribute ("time", fmtNum (arquivoTempos));
            w1->setAttribute ("contentTime", fmtNum (p.duracaoSeg));
        }
        else
        {
            const int canal = f.bateria ? 9 : 0;   // 0-based
            for (const Clipe* c : clipesOrdenados (f))
            {
                juce::XmlElement* ce = clips->createNewChildElement ("Clip");
                ce->setAttribute ("time", fmtNum (c->inicio));
                ce->setAttribute ("duration", fmtNum (c->fim - c->inicio));
                ce->setAttribute ("playStart", juce::String ("0"));
                ce->setAttribute ("name", nomeClipe (p, f, *c));
                juce::XmlElement* notas = ce->createNewChildElement ("Notes");
                notas->setAttribute ("id", ids.proximo());
                if (! comNotas)
                    continue;
                for (const NotaRel& n : notasDoClipe (*c))
                {
                    juce::XmlElement* ne = notas->createNewChildElement ("Note");
                    ne->setAttribute ("time", fmtNum (n.t));
                    ne->setAttribute ("duration", fmtNum (n.d));
                    ne->setAttribute ("channel", juce::String (canal));
                    ne->setAttribute ("key", juce::String (n.tom));
                    ne->setAttribute ("vel", fmtNum (n.vel / 127.0));
                    ne->setAttribute ("rel", fmtNum (64.0 / 127.0));
                }
            }
        }
    }
    // lane vazia do master (como no exemplo do Bitwig)
    {
        juce::XmlElement* lane = lanes->createNewChildElement ("Lanes");
        lane->setAttribute ("track", idMasterTrack);
        lane->setAttribute ("id", ids.proximo());
        juce::XmlElement* clips = lane->createNewChildElement ("Clips");
        clips->setAttribute ("id", ids.proximo());
    }

    // Markers (um por parte) — no XSD vem logo depois de Lanes dentro de Arrangement
    if (! p.partes.empty())
    {
        juce::XmlElement* marcadores = arranjo->createNewChildElement ("Markers");
        marcadores->setAttribute ("timeUnit", juce::String ("beats"));
        marcadores->setAttribute ("id", ids.proximo());
        for (const Parte& pa : p.partes)
        {
            juce::XmlElement* m = marcadores->createNewChildElement ("Marker");
            m->setAttribute ("time", fmtNum (pa.compIni * tpc));
            m->setAttribute ("name", U (pa.nome));
            m->setAttribute ("color", hexCor (pa.cor));
        }
    }
    projeto.createNewChildElement ("Scenes");

    // metadata.xml
    juce::XmlElement meta ("MetaData");
    meta.createNewChildElement ("Title")->addTextElement (U (p.nomeMusica));
    meta.createNewChildElement ("Comment")->addTextElement (
        "MAPA - InsideStudioSP | BPM " + fmtNum (std::round (p.bpm * 100.0) / 100.0) + " | Tom " + U (p.tomTexto));

    // zip: project.xml, metadata.xml e o áudio (sem compressão) em audio/
    juce::ZipFile::Builder zip;
    addTextoZip (zip, "project.xml", projeto.toString (formatoXml()));
    addTextoZip (zip, "metadata.xml", meta.toString (formatoXml()));
    zip.addFile (audio, 0, caminhoNoZip);

    const juce::File arq = pasta.getChildFile (nomeProjeto (p) + ".dawproject");
    std::unique_ptr<juce::FileOutputStream> out = abrirSaida (arq);
    if (out == nullptr)
        return falha (tx ("Não consegui gravar ") + arq.getFullPathName());
    if (! zip.writeToStream (*out, nullptr))
        return falha ("Erro ao montar o zip " + arq.getFullPathName());
    out->flush();
    if (out->getStatus().failed())
        return falha ("Erro ao gravar " + arq.getFullPathName() + ": " + out->getStatus().getErrorMessage());
    return sucesso (arq);
}
} // namespace mapa
