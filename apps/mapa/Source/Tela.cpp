#include "Tela.h"
#include "Analise.h"
#include "Exportar.h"

namespace mapa
{
namespace
{
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

namespace C
{
const juce::Colour fundo   (0xff08090a);
const juce::Colour bg      (0xff111215);
const juce::Colour pn      (0xff17191d);
const juce::Colour pn2     (0xff1f2228);
const juce::Colour ln      (0xff2a2e36);
const juce::Colour ln2     (0xff1d2026);
const juce::Colour tx      (0xffeceef2);
const juce::Colour mu      (0xff878c98);
const juce::Colour gr      (0xffb8ff3a);
const juce::Colour pu      (0xff9b6bff);
const juce::Colour escuro  (0xff0b0c0e);
} // namespace C

juce::Colour rgb (uint32_t c) { return juce::Colour ((juce::uint32) (0xff000000u | c)); }

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}

juce::String fmtTempo (double seg)
{
    const int s = (int) std::round (seg);
    return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
}

// ícones do transporte (desenhados, não dependem de fonte)
void iconePlay (juce::Graphics& g, juce::Rectangle<float> r, bool tocando, juce::Colour c)
{
    g.setColour (c);
    auto q = r.withSizeKeepingCentre (r.getWidth() * 0.36f, r.getHeight() * 0.36f);
    if (tocando) { g.fillRect (q); return; }
    juce::Path p;
    p.addTriangle (q.getX() + q.getWidth() * 0.1f, q.getY(), q.getX() + q.getWidth() * 0.1f, q.getBottom(), q.getRight() + q.getWidth() * 0.1f, q.getCentreY());
    g.fillPath (p);
}

void iconeInicio (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour (c);
    auto q = r.withSizeKeepingCentre (r.getWidth() * 0.34f, r.getHeight() * 0.34f);
    g.fillRect (q.withWidth (2.2f));
    juce::Path p;
    p.addTriangle (q.getRight(), q.getY(), q.getRight(), q.getBottom(), q.getX() + 2.5f, q.getCentreY());
    g.fillPath (p);
}

void iconeLoop (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour (c);
    auto q = r.withSizeKeepingCentre (r.getWidth() * 0.4f, r.getHeight() * 0.4f);
    juce::Path p;
    p.addCentredArc (q.getCentreX(), q.getCentreY(), q.getWidth() / 2, q.getHeight() / 2, 0, 0.6f, 5.6f, true);
    g.strokePath (p, juce::PathStrokeType (1.8f));
    juce::Path s;
    const float ax = q.getCentreX() + std::sin (0.6f) * q.getWidth() / 2, ay = q.getCentreY() - std::cos (0.6f) * q.getHeight() / 2;
    s.addTriangle (ax - 3.5f, ay - 3.5f, ax + 3.5f, ay - 0.5f, ax - 1.0f, ay + 3.5f);
    g.fillPath (s);
}
} // namespace

// ============================================================== análise em segundo plano
class Tela::Trabalho : public juce::Thread
{
public:
    Trabalho (Tela& t, juce::File f) : juce::Thread ("MAPA analise"), dono (t), seguro (&t), arq (std::move (f)) {}
    ~Trabalho() override
    {
        cancelar = true;
        stopThread (15000);
    }

    void run() override
    {
        const auto inicio = juce::Time::getMillisecondCounterHiRes();
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (arq));
        if (rd == nullptr) { falhou (u8 ("Não consegui ler este arquivo. Use WAV, AIFF, MP3 ou FLAC.")); return; }
        const double taxa = rd->sampleRate;
        const auto n = rd->lengthInSamples;
        if (taxa <= 0 || n < (juce::int64) (taxa * 8)) { falhou (u8 ("A música é curta demais (menos de 8 segundos).")); return; }
        if (n > (juce::int64) (taxa * 60 * 20)) { falhou (u8 ("A música passa de 20 minutos. Use um trecho menor.")); return; }

        dono.mudaEtapa (0.01f, u8 ("lendo o arquivo"));
        auto buf = std::make_shared<juce::AudioBuffer<float>> (2, (int) n);
        buf->clear();
        rd->read (buf.get(), 0, (int) n, 0, true, true);
        if (rd->numChannels == 1) buf->copyFrom (1, 0, *buf, 0, 0, (int) n);
        if (threadShouldExit() || cancelar) return;

        EntradaAnalise in;
        in.sampleRate = taxa;
        in.canais = (int) rd->numChannels;
        in.nomeMusica = arq.getFileNameWithoutExtension().toStdString();
        in.caminhoAudio = arq.getFullPathName().toStdString();
        in.mono.resize ((size_t) n);
        const float* l = buf->getReadPointer (0);
        const float* r = buf->getReadPointer (1);
        for (int i = 0; i < (int) n; ++i) in.mono[(size_t) i] = 0.5f * (l[i] + r[i]);

        // picos para a waveform: 6000 blocos
        std::vector<float> picos (6000, 0.0f);
        for (int i = 0; i < (int) n; ++i)
        {
            auto& p = picos[(size_t) ((juce::int64) i * 6000 / n)];
            p = std::max (p, std::max (std::abs (l[i]), std::abs (r[i])));
        }

        // ---- 0.2: IA (separação + notas). Sem o modelo, cai na análise simples (0.1).
        juce::String aviso;
        const juce::File modelo = arquivoModelo();
        bool comIA = false;
        if (modelo.existsAsFile())
        {
            std::vector<float> L44, R44;
            para44k (*buf, taxa, L44, R44);
            comIA = processarComIA (L44, R44, modelo.getFullPathName().toStdString(), MapaDados::nmp_onnx,
                                    (size_t) MapaDados::nmp_onnxSize, in,
                                    [this] (float v, const char* e) { dono.mudaEtapa (0.02f + 0.86f * v, juce::String::fromUTF8 (e)); },
                                    &cancelar);
            if (threadShouldExit() || cancelar) return;
            if (! comIA)
            {
                aviso = u8 ("A IA não rodou (") + juce::String::fromUTF8 (in.erroIA.c_str()) + u8 ("): mapa pela mistura, menos preciso.");
                in.stems = Stems {};
                in.notasBaixo.clear(); in.notasVoz.clear(); in.notasOutros.clear();
            }
        }
        else
            aviso = u8 ("Modelo de IA não encontrado: mapa pela mistura, menos preciso.");

        Projeto p;
        const float b0 = comIA ? 0.88f : 0.0f, esc = comIA ? 0.08f : 0.98f;
        const bool ok = analisar (in, p, [this, b0, esc] (float v, const char* e) { dono.mudaEtapa (b0 + esc * v, juce::String::fromUTF8 (e)); }, &cancelar);
        if (threadShouldExit() || cancelar) return;
        if (! ok) { falhou (u8 ("Não consegui achar o ritmo desta música.")); return; }

        // 0.3: os stems só servem para LER a música; não vão para o projeto (a DAW já separa, se o produtor quiser)
        std::shared_ptr<const Stems> st;
        std::vector<std::vector<float>> picosSt;
        in.stems = Stems {};
        dono.tempoAnalise = (juce::Time::getMillisecondCounterHiRes() - inicio) / 1000.0;
        fim (true, {}, buf, taxa, std::move (p), std::move (picos), st, std::move (picosSt), aviso);
    }

    std::atomic<bool> cancelar { false };

private:
    void falhou (const juce::String& erro) { fim (false, erro, {}, 0, {}, {}, {}, {}, {}); }

    void fim (bool ok, juce::String erro, std::shared_ptr<juce::AudioBuffer<float>> a, double taxa, Projeto p, std::vector<float> picos,
              std::shared_ptr<const Stems> st, std::vector<std::vector<float>> picosSt, juce::String aviso)
    {
        auto sp = seguro;
        auto pp = std::make_shared<Projeto> (std::move (p));
        auto pc = std::make_shared<std::vector<float>> (std::move (picos));
        auto ps = std::make_shared<std::vector<std::vector<float>>> (std::move (picosSt));
        juce::MessageManager::callAsync ([sp, ok, erro, a, taxa, pp, pc, st, ps, aviso]
        {
            if (auto* t = sp.getComponent()) t->terminouAnalise (ok, erro, a, taxa, std::move (*pp), std::move (*pc), st, std::move (*ps), aviso);
        });
    }

    // htdemucs.onnx: Mac = MAPA.app/Contents/Resources; Windows = pasta "modelos" ao lado do MAPA.exe
    static juce::File arquivoModelo()
    {
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::currentApplicationFile).getChildFile ("Contents/Resources/htdemucs.onnx");
       #else
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getChildFile ("modelos/htdemucs.onnx");
       #endif
    }

    // estéreo na taxa original -> 44,1 kHz (o Demucs só trabalha em 44,1 kHz), sem atraso
    static void para44k (const juce::AudioBuffer<float>& b, double taxa, std::vector<float>& L, std::vector<float>& R)
    {
        const int n = b.getNumSamples();
        if (std::abs (taxa - 44100.0) < 0.5)
        {
            L.assign (b.getReadPointer (0), b.getReadPointer (0) + n);
            R.assign (b.getReadPointer (1), b.getReadPointer (1) + n);
            return;
        }
        const double razao = taxa / 44100.0;   // amostras de entrada por amostra de saída
        const int nOut = (int) std::floor (n / razao);
        for (int c = 0; c < 2; ++c)
        {
            juce::WindowedSincInterpolator it;
            const int atraso = (int) std::lround (juce::WindowedSincInterpolator::getBaseLatency() / razao);
            std::vector<float> tmp ((size_t) (nOut + atraso + 1), 0.0f);
            it.process (razao, b.getReadPointer (c), tmp.data(), (int) tmp.size(), n, 0);
            auto& v = c == 0 ? L : R;
            v.assign (tmp.begin() + atraso, tmp.begin() + atraso + nOut);
        }
    }

    // stems em WAV 24 bits numa pasta temporária (o exportador copia para <projeto>/Audio). Preenche Faixa::arquivo.
    juce::String gravarStems (const Stems& st, Projeto& p)
    {
        auto pasta = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("MAPA-stems");
        pasta.deleteRecursively();   // só os stems da música atual
        pasta = pasta.getChildFile (nomeSeguro (p.nomeMusica));
        if (! pasta.createDirectory()) return u8 ("Não consegui criar a pasta dos stems: ") + pasta.getFullPathName();
        juce::WavAudioFormat wav;
        const size_t ns = st.amostras();
        for (auto& f : p.faixas)
        {
            if (f.tipo != TipoFaixa::audio || f.stem < 0) continue;
            const auto destino = pasta.getChildFile (nomeSeguro (p.nomeMusica + " - " + f.nome.substr (5)) + ".wav");
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (destino);
            if (static_cast<juce::FileOutputStream*> (os.get())->failedToOpen()) return u8 ("Não consegui gravar ") + destino.getFullPathName();
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions {}.withSampleRate (44100).withNumChannels (2).withBitsPerSample (24));
            if (w == nullptr) return u8 ("Não consegui gravar ") + destino.getFullPathName();
            const float* canais[2] = { st.canal[(size_t) f.stem][0].data(), st.canal[(size_t) f.stem][1].data() };
            if (! w->writeFromFloatArrays (canais, 2, (int) ns)) return u8 ("Erro ao gravar ") + destino.getFullPathName();
            f.arquivo = destino.getFullPathName().toStdString();
        }
        return {};
    }

    Tela& dono;
    juce::Component::SafePointer<Tela> seguro;
    juce::File arq;
};

// ============================================================== Tela
Tela::Tela()
{
    setWantsKeyboardFocus (true);
    daws = procurarDaws();
    dawSel = 0;
    startTimerHz (30);
}

Tela::~Tela()
{
    stopTimer();
    trabalho.reset();
}

void Tela::mudaEtapa (float p, const juce::String& e)
{
    progresso = p;
    const juce::ScopedLock sl (travaEtapa);
    etapa = e;
}

void Tela::carregar (const juce::File& f)
{
    if (! f.existsAsFile()) return;
    tocador.parar();
    trabalho.reset();
    arquivo = f;
    estado = Estado::analisando;
    progresso = 0;
    mudaEtapa (0, u8 ("abrindo"));
    parteSel = -1;
    aviso = {};
    trabalho = std::make_unique<Trabalho> (*this, f);
    trabalho->startThread();
    repaint();
}

void Tela::terminouAnalise (bool ok, juce::String erro, std::shared_ptr<juce::AudioBuffer<float>> a, double tx,
                            Projeto p, std::vector<float> pc, std::shared_ptr<const Stems> st,
                            std::vector<std::vector<float>> pcSt, juce::String av)
{
    trabalho.reset();
    if (! ok)
    {
        estado = Estado::erro;
        erroMsg = erro;
        repaint();
        return;
    }
    audio = a;
    taxa = tx;
    proj = std::move (p);
    picos = std::move (pc);
    stems = st;
    picosStems = std::move (pcSt);
    aviso = av;
    tocador.carregar (*audio, taxa, proj, stems);
    estado = Estado::pronto;
    arranjoSujo = true;
    resized();
    repaint();
}

void Tela::escolherArquivo()
{
    seletor = std::make_unique<juce::FileChooser> (u8 ("Escolha a música de referência"),
                                                   arquivo.existsAsFile() ? arquivo.getParentDirectory() : juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.mp3;*.flac");
    seletor->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) carregar (f);
                          });
}

bool Tela::isInterestedInFileDrag (const juce::StringArray& arqs)
{
    for (auto& a : arqs)
        if (a.endsWithIgnoreCase (".wav") || a.endsWithIgnoreCase (".aif") || a.endsWithIgnoreCase (".aiff")
            || a.endsWithIgnoreCase (".mp3") || a.endsWithIgnoreCase (".flac"))
            return true;
    return false;
}

void Tela::filesDropped (const juce::StringArray& arqs, int, int)
{
    arrastando = false;
    for (auto& a : arqs)
        if (isInterestedInFileDrag (juce::StringArray (a))) { carregar (juce::File (a)); return; }
}

void Tela::timerCallback()
{
    if (estado == Estado::analisando || tocador.tocando()) repaint();
    static bool tocavaAntes = false;
    if (tocavaAntes && ! tocador.tocando()) repaint();
    tocavaAntes = tocador.tocando();
}

bool Tela::keyPressed (const juce::KeyPress& k)
{
    if (estado != Estado::pronto) return false;
    if (k == juce::KeyPress::spaceKey)
    {
        if (tocador.tocando()) tocador.parar(); else tocador.tocar();
        repaint();
        return true;
    }
    if (k == juce::KeyPress::homeKey)
    {
        tocador.irPara (parteSel >= 0 ? proj.partes[(size_t) parteSel].compIni * 4.0 : 0.0);
        repaint();
        return true;
    }
    return false;
}

// -------------------------------------------------------------- layout
void Tela::resized()
{
    auto r = getLocalBounds();
    rTopo = r.removeFromTop (64);
    rRodape = r.removeFromBottom (28);
    rMeio = r;
    rPainel = r.removeFromRight (330);
    rArr = r;

    auto a = rArr;
    rSecoes = a.removeFromTop (46);
    rRegua = a.removeFromTop (22);
    rFaixas = a;

    // topo
    auto t = rTopo.reduced (16, 0);
    t.removeFromLeft (190);   // logo + selo
    rBtnInicio = t.removeFromLeft (36).withSizeKeepingCentre (36, 36);
    t.removeFromLeft (10);
    rBtnPlay = t.removeFromLeft (42).withSizeKeepingCentre (42, 42);
    t.removeFromLeft (10);
    rBtnLoop = t.removeFromLeft (36).withSizeKeepingCentre (36, 36);
    rSoltar = t.removeFromRight (150).withSizeKeepingCentre (150, 34);
    t.removeFromRight (10);
    rRef = t.removeFromRight (300).withSizeKeepingCentre (300, 46);

    // painel
    auto p = rPainel.reduced (16);
    p.removeFromTop (22);
    rDaws.clear();
    const int nDaws = juce::jmin (daws.size(), 6);
    for (int i = 0; i < nDaws; ++i)
    {
        rDaws.push_back (p.removeFromTop (46));
        p.removeFromTop (8);
    }
    rIr = p.removeFromBottom (24).withTrimmedTop (0);
    rSoPasta = rIr;
    p.removeFromBottom (6);
    rIr = p.removeFromBottom (56);
    rOptPro = {};

    rBtnEscolher = rArr.withSizeKeepingCentre (220, 44).translated (0, 60);

    // linhas das faixas
    rLinhas.clear();
    rMS.clear();
    if (estado == Estado::pronto && ! proj.faixas.empty())
    {
        const int n = (int) proj.faixas.size();
        // referência 1,5; stems 0,75 (são muitos e só mostram a waveform); demais 1
        auto peso = [this] (int i) { return i == 0 ? 1.5f : (proj.faixas[(size_t) i].stem >= 0 ? 0.75f : 1.0f); };
        float soma = 0;
        for (int i = 0; i < n; ++i) soma += peso (i);
        const float unid = rFaixas.getHeight() / juce::jmax (1.0f, soma);
        float y = (float) rFaixas.getY();
        for (int i = 0; i < n; ++i)
        {
            const float h = unid * peso (i);
            juce::Rectangle<int> lin (rFaixas.getX(), (int) y, rFaixas.getWidth(), (int) (y + h) - (int) y);
            rLinhas.push_back (lin);
            auto cab = lin.withWidth (LARG_CAB);
            auto ms = cab.withTrimmedRight (10).removeFromRight (46).withSizeKeepingCentre (46, 18);
            rMS.push_back ({ ms.withWidth (21), ms.withTrimmedLeft (25) });
            y += h;
        }
    }
    arranjoSujo = true;
}

float Tela::xDe (double t) const
{
    const double total = std::max (4.0, proj.totalCompassos * 4.0);
    const float x0 = (float) (rArr.getX() + LARG_CAB), w = (float) (rArr.getWidth() - LARG_CAB);
    return x0 + (float) (t / total) * w;
}

double Tela::temposDe (float x) const
{
    const double total = std::max (4.0, proj.totalCompassos * 4.0);
    const float x0 = (float) (rArr.getX() + LARG_CAB), w = (float) (rArr.getWidth() - LARG_CAB);
    return juce::jlimit (0.0, total, (double) ((x - x0) / w) * total);
}

// -------------------------------------------------------------- mouse
void Tela::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    bool mao = rRef.contains (p) || rSoltar.contains (p);
    if (estado == Estado::pronto)
    {
        mao = mao || rBtnPlay.contains (p) || rBtnInicio.contains (p) || rBtnLoop.contains (p) || rIr.contains (p)
              || rSoPasta.contains (p) || rSecoes.contains (p) || rOptPro.contains (p);
        for (auto& r : rDaws) mao = mao || r.contains (p);
        for (auto& m : rMS) mao = mao || m.first.contains (p) || m.second.contains (p);
    }
    if (estado == Estado::vazio || estado == Estado::erro) mao = mao || rBtnEscolher.contains (p) || rArr.contains (p);
    setMouseCursor (mao ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void Tela::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    grabKeyboardFocus();
    if (estado == Estado::analisando) return;

    if (rRef.contains (p) || rSoltar.contains (p)) { escolherArquivo(); return; }
    if (estado == Estado::vazio || estado == Estado::erro)
    {
        if (rArr.contains (p)) escolherArquivo();
        return;
    }
    // pronto
    for (size_t i = 0; i < rDaws.size(); ++i)
        if (rDaws[i].contains (p)) { dawSel = (int) i; repaint(); return; }
    if (rIr.contains (p)) { abrirNaDawEscolhida (false); return; }
    if (rSoPasta.contains (p)) { abrirNaDawEscolhida (true); return; }

    if (rBtnPlay.contains (p)) { if (tocador.tocando()) tocador.parar(); else tocador.tocar(); repaint(); return; }
    if (rBtnInicio.contains (p))
    {
        tocador.irPara (parteSel >= 0 ? proj.partes[(size_t) parteSel].compIni * 4.0 : 0.0);
        repaint();
        return;
    }
    if (rBtnLoop.contains (p))
    {
        if (parteSel >= 0) { parteSel = -1; tocador.definirLoop (0, 0); }
        repaint();
        return;
    }
    for (size_t i = 0; i < rMS.size(); ++i)
    {
        if (rMS[i].first.contains (p)) { tocador.definirMudo ((int) i, ! tocador.mudo ((int) i)); arranjoSujo = true; repaint(); return; }
        if (rMS[i].second.contains (p)) { tocador.definirSolo ((int) i, ! tocador.solo ((int) i)); arranjoSujo = true; repaint(); return; }
    }
    if (rSecoes.contains (p) && p.x > rArr.getX() + LARG_CAB)
    {
        const double t = temposDe ((float) p.x);
        for (size_t i = 0; i < proj.partes.size(); ++i)
        {
            const auto& pt = proj.partes[i];
            if (t >= pt.compIni * 4.0 && t < pt.compFim * 4.0)
            {
                if (parteSel == (int) i) { parteSel = -1; tocador.definirLoop (0, 0); }
                else
                {
                    parteSel = (int) i;
                    tocador.definirLoop (pt.compIni * 4.0, pt.compFim * 4.0);
                    tocador.irPara (pt.compIni * 4.0);
                    tocador.tocar();
                }
                repaint();
                return;
            }
        }
    }
    if ((rRegua.contains (p) || rFaixas.contains (p)) && p.x > rArr.getX() + LARG_CAB)
    {
        tocador.irPara (temposDe ((float) p.x));
        repaint();
    }
}

// -------------------------------------------------------------- exportar e abrir
void Tela::abrirNaDawEscolhida (bool soPasta)
{
    if (estado != Estado::pronto || daws.isEmpty()) return;
    const Daw daw = soPasta ? daws.getLast() : daws[juce::jlimit (0, daws.size() - 1, dawSel)];

    auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("MAPA");
    const auto base = nomeSeguro (proj.nomeMusica);
    auto pasta = docs.getChildFile (base);
    for (int k = 2; pasta.exists(); ++k) pasta = docs.getChildFile (base + " (" + juce::String (k) + ")");
    if (! pasta.createDirectory())
    {
        aviso = u8 ("Não consegui criar a pasta ") + pasta.getFullPathName();
        repaint();
        return;
    }
    const bool notas = false;   // 0.3: o MAPA é só o esqueleto (clipes vazios)
    auto r = exportarPasta (proj, pasta, notas);
    if (r.ok)
    {
        switch (daw.formato)
        {
            case Formato::ableton:    r = exportarAbleton (proj, pasta, notas); break;
            case Formato::reaper:     r = exportarReaper (proj, pasta, notas); break;
            case Formato::dawproject: r = exportarDawproject (proj, pasta, notas); break;
            case Formato::pasta:      r.arquivo = pasta; break;
        }
    }
    if (! r.ok)
    {
        aviso = u8 ("Erro ao montar o projeto: ") + r.erro;
        repaint();
        return;
    }
    ultimaPasta = pasta;
    juce::String erro;
    const auto alvo = daw.formato == Formato::pasta ? pasta : r.arquivo;
    if (! abrirNaDaw (daw, alvo, erro)) aviso = erro + u8 (" — o projeto ficou salvo na pasta.");
    else aviso = {};
    repaint();
}

// -------------------------------------------------------------- desenho
void Tela::paint (juce::Graphics& g)
{
    g.fillAll (C::bg);
    pintaTopo (g);
    g.setColour (C::ln);
    g.drawHorizontalLine (rTopo.getBottom() - 1, 0, (float) getWidth());

    if (estado == Estado::pronto)
    {
        if (arranjoSujo || cacheArranjo.getWidth() != rArr.getWidth() || cacheArranjo.getHeight() != rArr.getHeight())
            montarArranjo();
        g.drawImageAt (cacheArranjo, rArr.getX(), rArr.getY());
        // parte em loop
        if (parteSel >= 0)
        {
            const auto& pt = proj.partes[(size_t) parteSel];
            const float x0 = xDe (pt.compIni * 4.0), x1 = xDe (pt.compFim * 4.0);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (juce::Rectangle<float> (x0, (float) rRegua.getY(), x1 - x0, (float) (rFaixas.getBottom() - rRegua.getY())));
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (juce::Rectangle<float> (x0 + 1, (float) rSecoes.getY() + 6, x1 - x0 - 4, (float) rSecoes.getHeight() - 12), 5, 1.6f);
        }
        // cursor de reprodução
        const float px = xDe (tocador.posicaoTempos());
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (px - 2.5f, (float) rRegua.getY(), 5, (float) (rFaixas.getBottom() - rRegua.getY())));
        g.setColour (juce::Colours::white);
        g.fillRect (juce::Rectangle<float> (px - 1, (float) rRegua.getY(), 2, (float) (rFaixas.getBottom() - rRegua.getY())));
    }
    else if (estado == Estado::analisando) pintaAnalisando (g, rArr);
    else pintaVazio (g, rArr);

    if (arrastando)
    {
        g.setColour (C::gr.withAlpha (0.08f));
        g.fillRect (rMeio);
        g.setColour (C::gr);
        g.drawRect (rMeio.reduced (2), 2);
    }

    pintaPainel (g);
    pintaRodape (g);
}

void Tela::pintaTopo (juce::Graphics& g)
{
    g.setColour (C::pn);
    g.fillRect (rTopo);
    auto t = rTopo.reduced (16, 0);
    // logo com degradê
    auto logo = t.removeFromLeft (100).toFloat();
    g.setGradientFill (juce::ColourGradient (C::gr, logo.getX(), 0, C::pu, logo.getX() + 96, 0, false));
    g.setFont (fonte (26, true).withExtraKerningFactor (0.18f));
    g.drawText ("MAPA", logo, juce::Justification::centredLeft);
    auto selo = t.removeFromLeft (70).toFloat().withSizeKeepingCentre (62, 18);
    const auto corSelo = juce::Colour (0xff8fd16a);
    g.setColour (corSelo);
    g.drawRoundedRectangle (selo, 9, 1);
    g.setFont (fonte (9.5f, true).withExtraKerningFactor (0.15f));
    g.drawText (u8 ("GRÁTIS"), selo, juce::Justification::centred);

    const bool pronto = estado == Estado::pronto;
    auto botao = [&] (juce::Rectangle<int> r, bool destaque)
    {
        g.setColour (destaque ? C::gr : C::pn2);
        g.fillEllipse (r.toFloat());
        if (! destaque) { g.setColour (C::ln); g.drawEllipse (r.toFloat().reduced (0.5f), 1); }
    };
    botao (rBtnInicio, false);
    iconeInicio (g, rBtnInicio.toFloat(), pronto ? C::tx : C::mu);
    botao (rBtnPlay, pronto);
    iconePlay (g, rBtnPlay.toFloat(), tocador.tocando(), pronto ? C::escuro : C::mu);
    botao (rBtnLoop, false);
    iconeLoop (g, rBtnLoop.toFloat(), parteSel >= 0 ? C::gr : C::mu);

    // visores
    auto v = juce::Rectangle<int> (rBtnLoop.getRight() + 14, rTopo.getY(), rRef.getX() - rBtnLoop.getRight() - 24, rTopo.getHeight());
    auto lcd = [&] (const juce::String& k, const juce::String& valor, juce::Colour cor, int larg)
    {
        if (v.getWidth() < larg) return;
        auto r = v.removeFromLeft (larg).withSizeKeepingCentre (larg, 44);
        v.removeFromLeft (8);
        g.setColour (C::escuro);
        g.fillRoundedRectangle (r.toFloat(), 8);
        g.setColour (C::ln);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8, 1);
        auto in = r.reduced (11, 5);
        g.setColour (C::mu);
        g.setFont (fonte (9, true).withExtraKerningFactor (0.15f));
        g.drawText (k, in.removeFromTop (12), juce::Justification::centredLeft);
        g.setColour (cor);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold)));
        g.drawText (valor, in, juce::Justification::centredLeft);
    };
    juce::String pos = "---", bpm = "---", comp = "4/4", tom = "--", partes = "--";
    if (pronto)
    {
        const double t0 = tocador.posicaoTempos();
        const int bar = (int) (t0 / 4), beat = (int) std::fmod (t0, 4.0), sx = (int) (std::fmod (t0, 1.0) * 4);
        pos = juce::String (bar + 1).paddedLeft ('0', 3) + "." + juce::String (beat + 1) + "." + juce::String (sx + 1);
        bpm = juce::String (proj.bpm, 2);
        tom = u8 (proj.tomTexto);
        partes = juce::String ((int) proj.partes.size());
    }
    lcd (u8 ("POSIÇÃO"), pos, C::tx, 100);
    lcd ("BPM", bpm, C::gr, 92);
    lcd ("COMPASSO", comp, C::tx, 84);
    lcd ("TOM", tom, C::pu, 76);
    lcd ("PARTES", partes, C::tx, 66);

    // referência
    g.setColour (C::pn2);
    g.fillRoundedRectangle (rRef.toFloat(), 9);
    g.setColour (C::ln);
    g.drawRoundedRectangle (rRef.toFloat().reduced (0.5f), 9, 1);
    auto rr = rRef.reduced (12, 6);
    g.setColour (C::gr);
    g.setFont (fonte (20, true));
    g.drawText (juce::String::charToString ((juce::juce_wchar) 0x266A), rr.removeFromLeft (20), juce::Justification::centred);
    rr.removeFromLeft (8);
    g.setColour (C::tx);
    g.setFont (fonte (12.5f, true));
    g.drawText (arquivo.existsAsFile() ? arquivo.getFileName() : u8 ("nenhuma música"), rr.removeFromTop (18), juce::Justification::centredLeft, true);
    g.setFont (fonte (10.5f));
    if (pronto)
    {
        g.setColour (C::mu);
        const auto meta = fmtTempo (proj.duracaoSeg) + u8 (" · ") + juce::String (proj.totalCompassos) + u8 (" compassos · ");
        g.drawText (meta, rr, juce::Justification::centredLeft);
        g.setColour (C::gr);
        g.drawText (u8 ("● ANALISADA"), rr.withTrimmedLeft ((int) juce::GlyphArrangement::getStringWidth (fonte (10.5f), meta) + 2), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (C::mu);
        g.drawText (estado == Estado::analisando ? u8 ("analisando…") : u8 ("clique para escolher"), rr, juce::Justification::centredLeft);
    }
    // arraste outra
    {
        juce::Path borda;
        borda.addRoundedRectangle (rSoltar.toFloat().reduced (0.5f), 8);
        juce::Path tracejado;
        const float tr[] = { 4, 3 };
        juce::PathStrokeType (1).createDashedStroke (tracejado, borda, tr, 2);
        g.setColour (C::ln.brighter (0.3f));
        g.fillPath (tracejado);
        g.setColour (C::mu);
        g.setFont (fonte (10.5f));
        g.drawText (pronto ? u8 ("arraste outra música") : u8 ("arraste uma música"), rSoltar, juce::Justification::centred);
    }
}

void Tela::pintaVazio (juce::Graphics& g, juce::Rectangle<int> a)
{
    auto box = a.reduced (40).toFloat();
    juce::Path borda;
    borda.addRoundedRectangle (box, 18);
    juce::Path tracejado;
    const float tr[] = { 8, 6 };
    juce::PathStrokeType (1.5f).createDashedStroke (tracejado, borda, tr, 2);
    g.setColour (C::ln.brighter (0.4f));
    g.fillPath (tracejado);

    auto c = a.withSizeKeepingCentre (a.getWidth() - 120, 200).translated (0, -40);
    g.setColour (C::tx);
    g.setFont (fonte (30, true).withExtraKerningFactor (0.06f));
    g.drawText (estado == Estado::erro ? u8 ("NÃO DEU CERTO") : u8 ("ARRASTE UMA MÚSICA AQUI"), c.removeFromTop (48), juce::Justification::centred);
    g.setColour (estado == Estado::erro ? juce::Colour (0xffff7a7a) : C::mu);
    g.setFont (fonte (14));
    g.drawFittedText (estado == Estado::erro ? erroMsg
                                             : u8 ("WAV, AIFF, MP3 ou FLAC. O MAPA acha o BPM, o tom e as partes, marca onde cada elemento "
                                                   "entra e sai (kick, baixo, voz, sweeps...) e monta o esqueleto na sua DAW."),
                      c.removeFromTop (60), juce::Justification::centredTop, 3);
    g.setColour (C::gr);
    g.fillRoundedRectangle (rBtnEscolher.toFloat(), 10);
    g.setColour (C::escuro);
    g.setFont (fonte (13, true).withExtraKerningFactor (0.08f));
    g.drawText (u8 ("ESCOLHER ARQUIVO"), rBtnEscolher, juce::Justification::centred);
}

void Tela::pintaAnalisando (juce::Graphics& g, juce::Rectangle<int> a)
{
    auto card = a.withSizeKeepingCentre (520, 170).toFloat();
    g.setColour (C::pn);
    g.fillRoundedRectangle (card, 14);
    g.setColour (C::ln);
    g.drawRoundedRectangle (card.reduced (0.5f), 14, 1);
    auto c = card.reduced (28, 24);
    g.setColour (C::gr);
    g.setFont (fonte (11, true).withExtraKerningFactor (0.15f));
    g.drawText (u8 ("LENDO A REFERÊNCIA"), c.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (C::tx);
    g.setFont (fonte (17, true));
    g.drawText (arquivo.getFileName(), c.removeFromTop (30), juce::Justification::centredLeft, true);
    c.removeFromTop (14);
    auto barra = c.removeFromTop (10);
    g.setColour (C::escuro);
    g.fillRoundedRectangle (barra, 5);
    g.setGradientFill (juce::ColourGradient (C::gr, barra.getX(), 0, C::pu, barra.getRight(), 0, false));
    g.fillRoundedRectangle (barra.withWidth (juce::jmax (10.0f, barra.getWidth() * progresso.load())), 5);
    c.removeFromTop (12);
    juce::String e;
    {
        const juce::ScopedLock sl (travaEtapa);
        e = etapa;
    }
    g.setColour (C::mu);
    g.setFont (fonte (12));
    g.drawText (e + u8 ("…  ") + juce::String ((int) (progresso.load() * 100)) + "%", c.removeFromTop (18), juce::Justification::centredLeft);
}

void Tela::montarArranjo()
{
    arranjoSujo = false;
    if (rArr.isEmpty()) return;
    cacheArranjo = juce::Image (juce::Image::ARGB, rArr.getWidth(), rArr.getHeight(), true);
    juce::Graphics g (cacheArranjo);
    g.setOrigin (-rArr.getX(), -rArr.getY());
    pintaArranjo (g, rArr);
}

void Tela::pintaArranjo (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (C::bg);
    g.fillRect (area);
    const int x0 = area.getX() + LARG_CAB;
    const float total = (float) std::max (4.0, proj.totalCompassos * 4.0);

    // cabeçalho das partes
    {
        auto cab = rSecoes.withWidth (LARG_CAB);
        g.setColour (C::pn);
        g.fillRect (cab);
        auto c = cab.reduced (12, 9);
        g.setColour (C::gr);
        g.setFont (fonte (10, true).withExtraKerningFactor (0.15f));
        g.drawText (u8 ("PRÉVIA DO PROJETO"), c.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (C::mu);
        g.setFont (fonte (9.5f));
        g.drawText (u8 ("antes de abrir na DAW"), c, juce::Justification::centredLeft);
        for (auto& pt : proj.partes)
        {
            const float a = xDe (pt.compIni * 4.0), b = xDe (pt.compFim * 4.0);
            auto r = juce::Rectangle<float> (a, (float) rSecoes.getY() + 7, b - a - 3, (float) rSecoes.getHeight() - 14);
            g.setColour (rgb (pt.cor));
            g.fillRoundedRectangle (r, 5);
            if (r.getWidth() > 26)
            {
                g.setColour (C::escuro);
                g.setFont (fonte (10, true).withExtraKerningFactor (0.08f));
                g.drawText (u8 (pt.nome), r.reduced (7, 3).removeFromTop (14), juce::Justification::centredLeft, true);
                g.setColour (C::escuro.withAlpha (0.7f));
                g.setFont (fonte (9));
                g.drawText (juce::String (pt.compFim - pt.compIni) + " comp.", r.reduced (7, 3).withTrimmedTop (14), juce::Justification::centredLeft, true);
            }
        }
    }
    g.setColour (C::ln);
    g.drawHorizontalLine (rSecoes.getBottom() - 1, (float) area.getX(), (float) area.getRight());

    // régua
    {
        auto cab = rRegua.withWidth (LARG_CAB);
        g.setColour (C::pn);
        g.fillRect (cab);
        g.setColour (C::mu);
        g.setFont (fonte (9, true).withExtraKerningFactor (0.15f));
        g.drawText ("COMPASSO", cab.reduced (12, 0), juce::Justification::centredLeft);
        const float pxComp = (area.getWidth() - LARG_CAB) / (total / 4.0f);
        int passo = 1;
        for (int s : { 1, 2, 4, 8, 16, 32, 64 }) { passo = s; if (s * pxComp >= 38) break; }
        g.setFont (fonte (9));
        for (int c = 0; c < proj.totalCompassos; c += passo)
        {
            const float x = xDe (c * 4.0);
            g.setColour (juce::Colours::white.withAlpha (0.13f));
            g.drawVerticalLine ((int) x, (float) rRegua.getY(), (float) rRegua.getBottom());
            g.setColour (C::mu);
            g.drawText (juce::String (c + 1), juce::Rectangle<float> (x + 4, (float) rRegua.getY(), 40, (float) rRegua.getHeight()), juce::Justification::centredLeft);
        }
    }
    g.setColour (C::ln);
    g.drawHorizontalLine (rRegua.getBottom() - 1, (float) area.getX(), (float) area.getRight());

    // faixas
    for (size_t i = 0; i < proj.faixas.size() && i < rLinhas.size(); ++i)
    {
        const auto& f = proj.faixas[i];
        const auto lin = rLinhas[i];
        const auto cab = lin.withWidth (LARG_CAB);
        g.setColour (C::pn);
        g.fillRect (cab);
        g.setColour (rgb (f.cor));
        g.fillRect (cab.withWidth (5));
        auto txt = cab.withTrimmedLeft (14).withTrimmedRight (60);
        g.setColour (C::tx);
        g.setFont (fonte (12, true).withExtraKerningFactor (0.06f));
        g.drawText (u8 (f.nome), txt.withTrimmedBottom (txt.getHeight() / 2).withTrimmedTop (juce::jmax (0, txt.getHeight() / 2 - 16)),
                    juce::Justification::bottomLeft, true);
        g.setColour (C::mu);
        g.setFont (fonte (9.5f));
        g.drawText (u8 (f.desc), txt.withTrimmedTop (txt.getHeight() / 2 + 2).withHeight (14), juce::Justification::topLeft, true);
        // M e S
        const bool m = tocador.mudo ((int) i), s = tocador.solo ((int) i);
        auto bt = [&] (juce::Rectangle<int> r, const char* t, bool on, juce::Colour corOn)
        {
            g.setColour (on ? corOn : C::pn2);
            g.fillRoundedRectangle (r.toFloat(), 4);
            g.setColour (on ? corOn : C::ln);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4, 1);
            g.setColour (on ? C::escuro : C::mu);
            g.setFont (fonte (9, true));
            g.drawText (t, r, juce::Justification::centred);
        };
        if (i < rMS.size())
        {
            bt (rMS[i].first, "M", m, juce::Colour (0xffffb03a));
            bt (rMS[i].second, "S", s, C::gr);
        }
        g.setColour (C::ln);
        g.drawVerticalLine (cab.getRight() - 1, (float) lin.getY(), (float) lin.getBottom());

        // trilho com grade de compasso
        const auto trilho = lin.withTrimmedLeft (LARG_CAB);
        const float pxComp = trilho.getWidth() / (total / 4.0f);
        const int passo = pxComp * 4 >= 20 ? 4 : 16;
        g.setColour (juce::Colours::white.withAlpha (0.03f));
        for (int c = 0; c < proj.totalCompassos; c += passo)
            g.drawVerticalLine ((int) xDe (c * 4.0), (float) lin.getY(), (float) lin.getBottom());
        for (auto& c : f.clipes)
        {
            const float a = xDe (c.inicio), b = xDe (c.fim);
            pintaClipe (g, f, c, juce::Rectangle<float> (a, (float) lin.getY() + 4, juce::jmax (2.0f, b - a - 2), (float) lin.getHeight() - 8));
        }
        g.setColour (C::ln2);
        g.drawHorizontalLine (lin.getBottom() - 1, (float) area.getX(), (float) area.getRight());
    }
    juce::ignoreUnused (x0);
}

void Tela::pintaClipe (juce::Graphics& g, const Faixa& f, const Clipe& c, juce::Rectangle<float> r)
{
    const auto cor = rgb (f.cor);
    g.setColour (cor.withAlpha (0.8f));
    g.fillRoundedRectangle (r, 5);
    g.setColour (cor);
    g.drawRoundedRectangle (r, 5, 1);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.toNearestInt());
    auto cabeca = r.removeFromTop (13);
    g.fillRect (cabeca);
    if (cabeca.getWidth() > 24)
    {
        g.setColour (C::escuro);
        g.setFont (fonte (9, true));
        g.drawText (u8 (f.nome), cabeca.withTrimmedLeft (5), juce::Justification::centredLeft, true);
    }
    const auto escuro = C::escuro.withAlpha (0.6f);
    g.setColour (escuro);

    // waveform (referência, stems e FX)
    if (f.tipo == TipoFaixa::audio || f.elem == Elemento::fx)
    {
        const auto& picos = (f.stem >= 0 && f.stem < (int) picosStems.size()) ? picosStems[(size_t) f.stem] : this->picos;
        if (picos.empty() || proj.duracaoSeg <= 0) return;
        const float meio = r.getCentreY(), alt = r.getHeight() * 0.48f;
        for (int px = (int) r.getX(); px < (int) r.getRight(); ++px)
        {
            const double s0 = proj.temposParaSegAudio (temposDe ((float) px)), s1 = proj.temposParaSegAudio (temposDe ((float) px + 1));
            int i0 = (int) (s0 / proj.duracaoSeg * picos.size()), i1 = (int) (s1 / proj.duracaoSeg * picos.size());
            i0 = juce::jlimit (0, (int) picos.size() - 1, i0);
            i1 = juce::jlimit (i0 + 1, (int) picos.size(), i1);
            float v = 0;
            for (int k = i0; k < i1; ++k) v = std::max (v, picos[(size_t) k]);
            if (f.elem == Elemento::fx) v *= 0.7f;
            g.fillRect ((float) px, meio - v * alt, 1.0f, juce::jmax (1.0f, 2 * v * alt));
        }
        return;
    }
    // 0.3: efeitos com o desenho do que fazem (rampa subindo, rampa descendo, golpe)
    if (f.elem == Elemento::sweepUp || f.elem == Elemento::sweepDown || f.elem == Elemento::impacto)
    {
        juce::Path forma;
        const float x0 = r.getX() + 2, x1 = r.getRight() - 2, yb = r.getBottom() - 2, yt = r.getY() + 2;
        if (f.elem == Elemento::sweepUp) { forma.startNewSubPath (x0, yb); forma.lineTo (x1, yt); forma.lineTo (x1, yb); }
        else if (f.elem == Elemento::sweepDown) { forma.startNewSubPath (x0, yt); forma.lineTo (x1, yb); forma.lineTo (x0, yb); }
        else { forma.startNewSubPath (x0, yt); forma.quadraticTo (x0 + 2, yb, x1, yb); forma.lineTo (x0, yb); }
        forma.closeSubPath();
        g.fillPath (forma);
        return;
    }
    if (c.notas.empty()) return;
    // bateria: traço por batida (kick inteiro, clap no meio, hats em cima)
    if (f.bateria)
    {
        const float h = r.getHeight();
        float y0 = r.getY(), hh = h;
        if (f.elem == Elemento::hats) { hh = h * 0.4f; }
        else if (f.elem == Elemento::clap) { y0 = r.getY() + h * 0.2f; hh = h * 0.6f; }
        float ultimo = -10;
        for (auto& n : c.notas)
        {
            const float x = xDe (n.inicio);
            if (x - ultimo < 1.0f) continue;
            ultimo = x;
            g.fillRect (x, y0 + 2, 1.4f, hh - 4);
        }
        return;
    }
    // melódico: piano-roll compacto
    int lo = 127, hi = 0;
    for (auto& n : c.notas) { lo = std::min (lo, n.tom); hi = std::max (hi, n.tom); }
    const int faixa = juce::jmax (6, hi - lo + 1);
    const float hNota = juce::jmax (2.0f, r.getHeight() / faixa);
    for (auto& n : c.notas)
    {
        const float x = xDe (n.inicio), w = juce::jmax (1.2f, xDe (n.inicio + n.dur) - x);
        const float y = r.getBottom() - (n.tom - lo + 1) * (r.getHeight() - hNota) / faixa - hNota * 0.5f;
        g.fillRect (x, juce::jlimit (r.getY(), r.getBottom() - hNota, y), w, hNota * 0.8f);
    }
}

void Tela::pintaPainel (juce::Graphics& g)
{
    g.setColour (C::pn);
    g.fillRect (rPainel);
    g.setColour (C::ln);
    g.drawVerticalLine (rPainel.getX(), (float) rPainel.getY(), (float) rPainel.getBottom());
    auto p = rPainel.reduced (16);
    const int achadas = juce::jmax (0, daws.size() - 1);
    g.setFont (fonte (10, true).withExtraKerningFactor (0.15f));
    g.setColour (C::mu);
    auto tit = p.removeFromTop (22);
    g.drawText (u8 ("ABRIR NO…"), tit, juce::Justification::centredLeft);
    g.setColour (achadas > 0 ? C::gr : juce::Colour (0xffffb03a));
    g.drawText (achadas == 0 ? u8 ("NENHUMA DAW ACHADA") : juce::String (achadas) + (achadas == 1 ? u8 (" DAW ENCONTRADA") : u8 (" DAWs ENCONTRADAS")),
                tit.withTrimmedLeft (78), juce::Justification::centredLeft);

    for (size_t i = 0; i < rDaws.size(); ++i)
    {
        const auto& d = daws.getReference ((int) i);
        const auto r = rDaws[i].toFloat();
        const bool sel = (int) i == dawSel;
        g.setColour (C::pn2);
        g.fillRoundedRectangle (r, 9);
        g.setColour (sel ? C::gr : C::ln);
        g.drawRoundedRectangle (r.reduced (0.5f), 9, sel ? 1.6f : 1.0f);
        auto in = r.reduced (10, 8);
        auto radio = in.removeFromLeft (14).withSizeKeepingCentre (14, 14);
        g.setColour (sel ? C::gr : C::mu);
        g.drawEllipse (radio.reduced (1), 2);
        if (sel) g.fillEllipse (radio.reduced (4));
        in.removeFromLeft (10);
        const auto rot = d.encontrada ? rotuloFormato (d.formato) : u8 ("PASTA");
        auto tag = in.removeFromRight (78).withSizeKeepingCentre (78, 18);
        const auto corTag = d.formato == Formato::dawproject ? C::pu : (d.formato == Formato::pasta ? C::mu : C::gr);
        g.setFont (fonte (8.5f, true).withExtraKerningFactor (0.1f));
        const float wt = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), rot) + 12;
        auto tg = tag.withLeft (tag.getRight() - wt);
        g.setColour (corTag.withAlpha (0.14f));
        g.fillRoundedRectangle (tg, 5);
        g.setColour (corTag);
        g.drawText (rot, tg, juce::Justification::centred);
        g.setColour (C::tx);
        g.setFont (fonte (12, true));
        g.drawText (d.nome, in.removeFromTop (in.getHeight() / 2), juce::Justification::bottomLeft, true);
        g.setColour (C::mu);
        g.setFont (fonte (10));
        g.drawText (d.detalhe, in, juce::Justification::topLeft, true);
    }
    if (! rDaws.empty()) p.setTop (rDaws.back().getBottom() + 14);

    g.setFont (fonte (10, true).withExtraKerningFactor (0.15f));
    g.setColour (C::mu);
    g.drawText ("O QUE VAI NO PROJETO", p.removeFromTop (20), juce::Justification::centredLeft);
    auto opcao = [&] (const juce::String& t, bool marcado, bool travado, bool pro)
    {
        auto r = p.removeFromTop (22);
        auto cb = r.removeFromLeft (14).withSizeKeepingCentre (14, 14).toFloat();
        if (marcado)
        {
            g.setColour (C::gr);
            g.fillRoundedRectangle (cb, 3);
            g.setColour (C::escuro);
            juce::Path v;
            v.startNewSubPath (cb.getX() + 3, cb.getCentreY());
            v.lineTo (cb.getX() + 6, cb.getBottom() - 3.5f);
            v.lineTo (cb.getRight() - 3, cb.getY() + 3.5f);
            g.strokePath (v, juce::PathStrokeType (1.8f));
        }
        else
        {
            g.setColour (C::mu);
            g.drawRoundedRectangle (cb.reduced (0.5f), 3, 1);
        }
        r.removeFromLeft (8);
        if (pro)
        {
            auto tg = r.removeFromRight (MAPA_TESTE ? 92 : 34).withSizeKeepingCentre (MAPA_TESTE ? 92 : 34, 15).toFloat();
            g.setColour (C::pu);
            g.drawRoundedRectangle (tg, 4, 1);
            g.setFont (fonte (8, true).withExtraKerningFactor (0.1f));
            g.drawText (MAPA_TESTE ? u8 ("PRO · LIBERADO") : juce::String ("PRO"), tg, juce::Justification::centred);
        }
        g.setColour (travado ? C::tx.withAlpha (0.5f) : C::tx);
        g.setFont (fonte (11.5f));
        g.drawText (t, r, juce::Justification::centredLeft, true);
        return r;
    };
    opcao (u8 ("BPM, compasso e tom"), true, false, false);
    opcao (u8 ("Marcadores de cada parte"), true, false, false);
    opcao (u8 ("Áudio da referência na grade"), true, false, false);
    opcao (u8 ("Uma faixa por elemento, com cor e nome"), true, false, false);
    opcao (u8 ("Clipe onde cada elemento entra e sai"), true, false, false);
    opcao (u8 ("Sweeps e impactos marcados"), true, false, false);
    g.setColour (C::mu);
    g.setFont (fonte (10.5f));
    g.drawFittedText (u8 ("Clipes vazios, sem notas: é o mapa da referência. O som e a melodia são seus."),
                      p.removeFromTop (34).withTrimmedTop (6), juce::Justification::topLeft, 2);

    // botão principal
    const bool pronto = estado == Estado::pronto;
    const auto& d = daws.getReference (juce::jlimit (0, daws.size() - 1, dawSel));
    g.setGradientFill (juce::ColourGradient (C::gr, (float) rIr.getX(), 0, juce::Colour (0xff7be0a0), (float) rIr.getRight(), 0, false));
    g.setOpacity (pronto ? 1.0f : 0.35f);
    g.fillRoundedRectangle (rIr.toFloat(), 10);
    g.setOpacity (1.0f);
    g.setColour (C::escuro);
    g.setFont (fonte (14, true).withExtraKerningFactor (0.05f));
    const auto titulo = d.app == juce::File() ? u8 ("SALVAR A PASTA DO PROJETO") : u8 ("ABRIR NO ") + d.nome.toUpperCase();
    g.drawText (titulo, rIr.withTrimmedBottom (18).reduced (10, 0), juce::Justification::centredBottom, true);
    g.setFont (fonte (10.5f));
    g.setColour (C::escuro.withAlpha (0.7f));
    const auto sub = d.app == juce::File() ? u8 ("MIDI + áudio + LEIA-ME") : u8 ("cria o projeto e abre o ") + d.nome;
    g.drawText (sub, rIr.withTrimmedTop (rIr.getHeight() - 22), juce::Justification::centredTop, true);
    g.setColour (C::mu);
    g.setFont (fonte (10.5f));
    g.drawText (u8 ("ou só salvar a pasta (MIDI + áudio)"), rSoPasta, juce::Justification::centred);
}

void Tela::pintaRodape (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff0c0d0f));
    g.fillRect (rRodape);
    g.setColour (C::ln);
    g.drawHorizontalLine (rRodape.getY(), 0, (float) getWidth());
    auto r = rRodape.reduced (16, 0);
    g.setFont (fonte (10.5f));
    g.setColour (C::mu);
    g.drawText ("v" MAPA_VERSAO + juce::String (MAPA_TESTE ? u8 (" · teste") : juce::String()), r.removeFromRight (120), juce::Justification::centredRight);
    juce::String t;
    if (aviso.isNotEmpty()) { g.setColour (juce::Colour (0xffffb03a)); t = aviso; }
    else if (ultimaPasta.exists()) t = u8 ("Salvo em ") + ultimaPasta.getFullPathName();
    else if (estado == Estado::pronto)
        t = u8 ("Análise: ") + juce::String (tempoAnalise, 1) + u8 (" s   ·   clique numa parte para ouvir só ela   ·   "
                                                                   "S numa faixa = ouvir só onde ela toca   ·   espaço = tocar/parar");
    else t = u8 ("InsideStudioSP");
    g.drawText (t, r, juce::Justification::centredLeft, true);
}
} // namespace mapa
