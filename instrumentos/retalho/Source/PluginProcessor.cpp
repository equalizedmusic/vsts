#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace retalho;

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int k = 0; k < numCampos; ++k)
    {
        const auto& in = info (k);
        const juce::ParameterID id { in.id, 1 };
        if (in.liga)
            l.add (std::make_unique<juce::AudioParameterBool> (id, in.nome, in.padrao > 0.5f));
        else if (in.esc != nullptr)
        {
            juce::StringArray s;
            for (int i = 0; i < in.nEsc; ++i) s.add (juce::String::fromUTF8 (in.esc[i]));
            l.add (std::make_unique<juce::AudioParameterChoice> (id, in.nome, s, (int) in.padrao));
        }
        else
            l.add (std::make_unique<juce::AudioParameterFloat> (id, in.nome, juce::NormalisableRange<float> (in.min, in.max, in.passo, in.skew),
                                                                in.padrao, juce::AudioParameterFloatAttributes().withLabel (in.un)));
    }
    return l;
}

juce::String juntaU (const uint32_t* v, int n)
{
    juce::StringArray s;
    for (int i = 0; i < n; ++i) s.add (juce::String::toHexString ((int) v[i]));
    return s.joinIntoString (",");
}
void separaU (const juce::String& t, uint32_t* v, int n)
{
    auto s = juce::StringArray::fromTokens (t, ",", "");
    for (int i = 0; i < n && i < s.size(); ++i) v[i] = (uint32_t) s[i].getHexValue32();
}

// cena ↔ ValueTree (pads, presets do usuário, A/B)
juce::ValueTree cenaVt (const Cena& c, const juce::String& nome)
{
    juce::ValueTree v ("CENA");
    v.setProperty ("nome", nome, nullptr);
    for (int k = 0; k < numCena; ++k) v.setProperty (info (k).id, c.v[k], nullptr);
    v.setProperty ("grade", juntaU (c.grade, maxGrade), nullptr);
    v.setProperty ("passo", juntaU (c.passo, maxPassos), nullptr);
    for (int l = 0; l < numLinhas; ++l) v.setProperty ("cel" + juce::String (l), juntaU (c.cel[l], maxCelulas), nullptr);
    juce::StringArray lv;
    for (float x : c.livre) lv.add (juce::String (x, 3));
    v.setProperty ("livre", lv.joinIntoString (","), nullptr);
    return v;
}
Cena vtCena (const juce::ValueTree& v)
{
    Cena c;
    for (int k = 0; k < numCena; ++k)
        if (v.hasProperty (info (k).id)) c.v[k] = juce::jlimit (info (k).min, info (k).max, (float) v.getProperty (info (k).id));
    if (v.hasProperty ("grade")) separaU (v.getProperty ("grade").toString(), c.grade, maxGrade);
    if (v.hasProperty ("passo")) separaU (v.getProperty ("passo").toString(), c.passo, maxPassos);
    for (int l = 0; l < numLinhas; ++l)
        if (v.hasProperty ("cel" + juce::String (l))) separaU (v.getProperty ("cel" + juce::String (l)).toString(), c.cel[l], maxCelulas);
    if (v.hasProperty ("livre"))
    {
        auto s = juce::StringArray::fromTokens (v.getProperty ("livre").toString(), ",", "");
        for (int i = 0; i < numLivre && i < s.size(); ++i) c.livre[i] = juce::jlimit (0.0f, 1.0f, s[i].getFloatValue());
    }
    return c;
}

uint64_t hashCena (const Cena& c, unsigned masc, double spb)
{
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h] (const void* d, size_t n) { auto* b = (const uint8_t*) d; for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; } };
    mix (&c, sizeof (Cena));
    mix (&masc, sizeof masc);
    mix (&spb, sizeof spb);
    return h;
}
} // namespace

RetalhoProcessor::RetalhoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RETALHO", parametros()),
      m (std::make_unique<Motor>())
{
    for (int k = 0; k < numCampos; ++k) p[k] = apvts.getRawParameterValue (info (k).id);
    eventos.reserve (256);
    // pads de fábrica
    const char* fabrica[numPads] = { "PSY RISER 1/32", "TAPE DROP", "GLITCH 16", "OFFBEAT VOX", "GALLOP CHOP", nullptr };
    for (int i = 0; i < numPads; ++i)
    {
        const int pi = fabrica[i] != nullptr ? presetPorNome (fabrica[i]) : -1;
        padCheio[i] = pi >= 0;
        padCena[i] = pi >= 0 ? cenaPreset (pi) : Cena();
        padNome[i] = pi >= 0 ? juce::String (fabrica[i]) : juce::String();
        m->gravaPad (i, padCena[i], padCheio[i]);
    }
    m->prepare (fs);
    aplicaModo (mPicote);   // 0.1.4: abre no PICOTE, receita 1 (o projeto salvo substitui)
    vistaAtual.oMin.assign (colunas, 0.0f); vistaAtual.oMax.assign (colunas, 0.0f);
    vistaAtual.rMin.assign (colunas, 0.0f); vistaAtual.rMax.assign (colunas, 0.0f);
    slots[0] = fotoCompleta();
    ultimoEstilo = (int) valor (kEstilo); ultimoBars = (int) valor (kGBars); ultimoGrid = (int) valor (kGGrid);
    startTimerHz (15);
}

RetalhoProcessor::~RetalhoProcessor()
{
    stopTimer();
    delete ativa.exchange (nullptr);
    recolheAposentadas (true);
    delete resAtivo.exchange (nullptr);
    for (auto& r : resAposentados) delete r.first;
    resAposentados.clear();
}

// ---- SAMPLER --------------------------------------------------------------------------------------------------------
bool RetalhoProcessor::arquivoDeAudio (const juce::String& caminho)
{
    const auto e = juce::File (caminho).getFileExtension().toLowerCase();
    return e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac" || e == ".mp3" || e == ".ogg";
}

void RetalhoProcessor::recolheAposentadas (bool tudo)
{
    const int64_t fim = blocoFim.load();
    for (auto it = aposentadas.begin(); it != aposentadas.end();)
    {
        if (tudo || fim > it->second) { delete it->first; it = aposentadas.erase (it); }
        else ++it;
    }
    for (auto it = resAposentados.begin(); it != resAposentados.end();)
    {
        if (tudo || fim > it->second) { delete it->first; it = resAposentados.erase (it); }
        else ++it;
    }
}

// o resultado (outR) vira o loop que toca com o projeto; tem = false tira (volta a tocar a amostra crua)
void RetalhoProcessor::publicaResultado (bool tem)
{
    std::unique_ptr<Resultado> r;
    if (tem && ! outR[0].empty())
    {
        r = std::make_unique<Resultado>();
        const int n = (int) outR[0].size();
        r->buf.setSize (2, n);
        for (int ch = 0; ch < 2; ++ch) r->buf.copyFrom (ch, 0, outR[ch].data(), n);
    }
    const juce::ScopedLock sl (travaAmostra);
    Resultado* velho = resAtivo.exchange (r.release());
    if (velho != nullptr) resAposentados.push_back ({ velho, blocoIni.load() });
    recolheAposentadas (false);
}

// reamostra a origem na taxa do host e troca a amostra que o áudio toca (a antiga é apagada quando o áudio largar)
void RetalhoProcessor::montaAtiva()
{
    std::unique_ptr<Amostra> a;
    {
        const juce::ScopedLock sl (travaAmostra);
        if (origBuf.getNumSamples() > 0)
        {
            a = std::make_unique<Amostra>();
            const double r = origFs / fs;   // passo na origem por amostra de saída
            const int n0 = origBuf.getNumSamples();
            const int n = juce::jmax (1, (int) std::floor ((double) (n0 - 1) / r) + 1);
            a->buf.setSize (2, n);
            for (int c = 0; c < 2; ++c)
            {
                const float* s = origBuf.getReadPointer (juce::jmin (c, origBuf.getNumChannels() - 1));
                float* d = a->buf.getWritePointer (c);
                if (std::abs (r - 1.0) < 1e-9) { std::copy (s, s + juce::jmin (n, n0), d); continue; }
                for (int i = 0; i < n; ++i)
                {
                    const double p = i * r;
                    const int k = (int) p;
                    const float t = (float) (p - k);
                    const float x0 = s[juce::jlimit (0, n0 - 1, k - 1)], x1 = s[juce::jlimit (0, n0 - 1, k)],
                                x2 = s[juce::jlimit (0, n0 - 1, k + 1)], x3 = s[juce::jlimit (0, n0 - 1, k + 2)];
                    d[i] = hermite (x0, x1, x2, x3, t);
                }
            }
            a->offBeats = origOff;
        }
    }
    const juce::ScopedLock sl (travaAmostra);
    ataquesDe = nullptr;   // amostra nova: refaz os ataques
    Amostra* velha = ativa.exchange (a.release());
    if (velha != nullptr) aposentadas.push_back ({ velha, blocoIni.load() });
    recolheAposentadas (false);
}

void RetalhoProcessor::instalaAmostra (const juce::AudioBuffer<float>& origem, double fsOrigem, const juce::String& nome, double offBeats, bool codifica)
{
    {
        const juce::ScopedLock sl (travaAmostra);
        origBuf.makeCopyOf (origem);
        origFs = fsOrigem;
        origNome = nome;
        origOff = offBeats;
        if (codifica)
        {
            // FLAC 24 bits em base64: o projeto guarda a amostra junto (não depende do arquivo continuar no lugar)
            origB64.clear();
            juce::MemoryBlock blk;
            bool ok = false;
            {
                std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::MemoryOutputStream> (blk, false);
                juce::FlacAudioFormat flac;
                const int nc = juce::jlimit (1, 2, origBuf.getNumChannels());
                if (auto w = flac.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (origFs).withNumChannels (nc).withBitsPerSample (24)))
                    ok = w->writeFromAudioSampleBuffer (origBuf, 0, origBuf.getNumSamples());
                // o writer fecha o FLAC (cabeçalho com o total de amostras) ao ser destruído, aqui
            }
            if (ok && blk.getSize() > 0) origB64 = juce::Base64::toBase64 (blk.getData(), blk.getSize());
        }
        // a vista da amostra (600 colunas)
        visao.tem = origBuf.getNumSamples() > 0;
        visao.nome = nome;
        visao.seg = origBuf.getNumSamples() / juce::jmax (1.0, origFs);
        const int C = 600, n = origBuf.getNumSamples();
        visao.mn.assign (C, 0.0f); visao.mx.assign (C, 0.0f);
        for (int x = 0; x < C && n > 0; ++x)
        {
            const int i0 = (int) ((int64_t) n * x / C), i1 = juce::jmax (i0 + 1, (int) ((int64_t) n * (x + 1) / C));
            float lo = 0.0f, hi = 0.0f;
            for (int i = i0; i < i1 && i < n; ++i)
            {
                float s = 0.0f;
                for (int c = 0; c < origBuf.getNumChannels(); ++c) s += origBuf.getSample (c, i);
                s /= (float) juce::jmax (1, origBuf.getNumChannels());
                lo = juce::jmin (lo, s); hi = juce::jmax (hi, s);
            }
            visao.mn[(size_t) x] = lo; visao.mx[(size_t) x] = hi;
        }
        visao.versao = ++visaoVersao;
    }
    montaAtiva();
}

bool RetalhoProcessor::carregaArquivo (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->sampleRate <= 0) return false;
    const int n = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 60.0));   // até 60 s
    juce::AudioBuffer<float> b (juce::jlimit (1, 2, (int) r->numChannels), n);
    r->read (&b, 0, n, 0, true, true);
    selIni = 0.0f; selFim = 1.0f;
    ponto = 0;
    instalaAmostra (b, r->sampleRate, f.getFileNameWithoutExtension(), 0.0, true);
    ouvirAoCarregar = true;
    atualizaVista (true);   // renderiza já (e toca) — não espera o timer nem o play do projeto
    return true;
}

void RetalhoProcessor::capturar (int bars)
{
    capBars = juce::jlimit (1, 8, bars);
    capPos = 0;
    capEstado = 1;
}

void RetalhoProcessor::limpaAmostra()
{
    {
        const juce::ScopedLock sl (travaAmostra);
        origBuf.setSize (0, 0);
        origB64.clear();
        origNome.clear();
        visao.tem = false; visao.nome.clear(); visao.mn.clear(); visao.mx.clear();
        visao.versao = ++visaoVersao;
    }
    selIni = 0.0f; selFim = 1.0f;
    montaAtiva();
    publicaResultado (false);
    // o resultado era da amostra: some junto (a janela ao vivo volta no próximo compasso tocado)
    prevEstado = 0;
    snap[0].clear(); snap[1].clear();
    vistaAtual.valida = false; ++vistaAtual.versao;
    pontoFr = -1.0f; nPontos = 0; ponto = 0;
}

bool RetalhoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void RetalhoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool mudouTaxa = std::abs (fs - sampleRate) > 0.5;
    fs = sampleRate;
    m->prepare (sampleRate);
    for (auto& b : prevBuf) b.setSize (2, (int) (sampleRate * 20.0), false, true, false);   // até 4 BARS a 50 BPM
    capBuf.setSize (2, (int) (sampleRate * 50.0), false, true, false);   // 8 compassos até ~40 BPM
    capEstado = capEstado.load() == 2 ? 1 : capEstado.load();
    if (mudouTaxa || (ativa.load() == nullptr && origBuf.getNumSamples() > 0)) montaAtiva();
    prevEstado = 0;
    eventos.reserve ((size_t) juce::jmax (256, samplesPerBlock));
    setLatencySamples (0);
    vistoCompasso = -1;
}

juce::RangedAudioParameter* RetalhoProcessor::param (int campo) const
{
    return dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (info (campo).id));
}

void RetalhoProcessor::define (int campo, float valorReal)
{
    if (auto* prm = param (campo))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (valorReal));
        prm->endChangeGesture();
    }
}

Cena RetalhoProcessor::cenaAtual() const
{
    Cena c;
    for (int k = 0; k < numCena; ++k) c.v[k] = valor (k);
    m->leArrays (c);
    return c;
}

void RetalhoProcessor::aplicaCena (const Cena& c)
{
    m->escreveArrays (c);
    for (int k = 0; k < numCena; ++k) define (k, c.v[k]);
    ultimoEstilo = c.i (kEstilo); ultimoBars = c.i (kGBars); ultimoGrid = c.i (kGGrid);
}

void RetalhoProcessor::aplicaPreset (int i)
{
    const auto& v = presets();
    if (i < 0 || i >= (int) v.size()) return;
    aplicaCena (cenaPreset (i));
    presetNome = v[(size_t) i].nome;
}

void RetalhoProcessor::aplicaCartao (int c)
{
    if (c == 0)
    {
        Cena x;
        pr::edit (x, shGrid, 0, 2);   // 1/2 BAR em 1/16, sem pitch: o stutter puro
        x.v[kDecay] = -1;
        aplicaCena (x);
        presetNome = "STUTTER 1/16";
    }
    else
    {
        static const char* nome[4] = { nullptr, "PSY RISER 1/32", "TAPE DROP", "OFFBEAT REPLACE" };   // GROOVE: voz original muda enquanto o chop toca
        aplicaPreset (presetPorNome (nome[juce::jlimit (1, 3, c)]));
    }
    define (kGrooveOn, 0.0f);   // no SIMPLE o GROOVE toca pelo QUANDO TOCA, não fica ligado direto
}

void RetalhoProcessor::aplicaModo (int modo)
{
    if (modo == mEstica)
    {
        Cena x = cenaAtual();
        x.v[kModo] = mEstica;
        if (x.i (kLength) < 1) x.v[kLength] = 2;
        aplicaCena (x);
        presetNome = juce::String ("ESTICA ") + juce::String::fromUTF8 ("\xc2\xb7 ") + juce::String::fromUTF8 (ch::direcao[lim (x.i (kEDir), 0, 5)]);
    }
    else
    {
        const Cena atual = cenaAtual();
        aplicaReceita (atual.i (kModo) == mPicote || atual.i (kModo) == mEstica ? atual.i (kReceita) : 0);
    }
    define (kGrooveOn, 0.0f);
}

void RetalhoProcessor::aplicaReceita (int i)
{
    const Cena atual = cenaAtual();
    Cena x;   // parte da padrão (nada do PRO sobra), mas guarda o que é da tela principal
    for (int k : { kQuanto, kRitmo, kFadeFim, kWidth, kEDir, kEInt, kEGrao, kEEspaco, kEVel, kEEst, kEGrade, kETom, kSeed })
        x.v[k] = atual.v[k];
    x.v[kModo] = mPicote;
    x.v[kReceita] = (float) ((i % numReceitas + numReceitas) % numReceitas);
    x.v[kLength] = atual.i (kModo) == mPicote || atual.i (kModo) == mEstica ? atual.v[kLength] : 2.0f;   // abre em 2 BARS
    x.v[kFadeIn] = 1.0f; x.v[kFadeOut] = 4.0f;
    aplicaCena (x);
    presetNome = juce::String ("PICOTE ") + juce::String::fromUTF8 ("\xc2\xb7 ") + juce::String::fromUTF8 (ch::receita[x.i (kReceita)]);
}

void RetalhoProcessor::variar()
{
    define (kSeed, (float) ((int) valor (kSeed) % 999 + 1));
}

void RetalhoProcessor::presetVizinho (int passo)
{
    const int n = (int) presets().size();
    const int i = presetPorNome (presetNome.toRawUTF8());
    aplicaPreset (i < 0 ? 0 : ((i + passo) % n + n) % n);
}

juce::File RetalhoProcessor::pastaPresets()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RETALHO").getChildFile ("Presets");
}

void RetalhoProcessor::salvaPresetArquivo (const juce::File& f)
{
    auto v = cenaVt (cenaAtual(), f.getFileNameWithoutExtension().toUpperCase());
    if (auto xml = v.createXml()) xml->writeTo (f);
    presetNome = f.getFileNameWithoutExtension().toUpperCase();
}

bool RetalhoProcessor::abrePresetArquivo (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("CENA")) return false;
    const auto v = juce::ValueTree::fromXml (*xml);
    aplicaCena (vtCena (v));
    presetNome = v.getProperty ("nome", f.getFileNameWithoutExtension()).toString().toUpperCase();
    return true;
}

RetalhoProcessor::InfoPad RetalhoProcessor::infoPad (int i) const
{
    InfoPad r;
    if (i < 0 || i >= numPads || ! padCheio[i]) return r;
    const Cena& c = padCena[i];
    r.cheio = true;
    r.nome = padNome[i];
    r.modo = c.i (kModo);
    if (r.modo == mEdit) r.sub = "EDIT " + juce::String::fromUTF8 ("\xC2\xB7 ") + juce::String (ch::length[lim (c.i (kLength), 0, 3)]);
    else if (r.modo == mComplex) r.sub = "COMPLEX " + juce::String::fromUTF8 ("\xC2\xB7 ") + (c.i (kDEvery) > 0 ? juce::String ("DICE") : juce::String (ch::read[lim (c.i (kRead), 0, 2)]));
    else r.sub = "GROOVE " + juce::String::fromUTF8 ("\xC2\xB7 ") + juce::String (ch::mix[lim (c.i (kMixVoz), 0, 2)]);
    return r;
}

void RetalhoProcessor::salvaPad (int i)
{
    if (i < 0 || i >= numPads) return;
    padCena[i] = cenaAtual();
    padCheio[i] = true;
    padNome[i] = presetNome.isNotEmpty() && presetNome != "INIT" ? presetNome : "MEMORY " + juce::String (i + 1);
    m->gravaPad (i, padCena[i], true);
}

void RetalhoProcessor::limpaPad (int i)
{
    if (i < 0 || i >= numPads) return;
    padCheio[i] = false;
    padNome[i].clear();
    m->gravaPad (i, padCena[i], false);
}

void RetalhoProcessor::renomeiaPad (int i, const juce::String& nome)
{
    if (i >= 0 && i < numPads && padCheio[i]) padNome[i] = nome.trim().toUpperCase().substring (0, 22);
}

void RetalhoProcessor::carregaPad (int i)
{
    if (i < 0 || i >= numPads || ! padCheio[i]) return;
    aplicaCena (padCena[i]);
    presetNome = padNome[i];
}

void RetalhoProcessor::dado()
{
    const int seed = (int) valor (kSeed) % 999 + 1;
    define (kSeed, (float) seed);
    Cena c = cenaAtual();
    dadoPassos (c.passo, 8 << lim (c.i (kPassos), 0, 2), c.i (kDMode), c.f (kDAmount) * 0.01f, c.f (kDChaos) * 0.01f, (uint32_t) seed * 977u);
    for (int i = 0; i < maxPassos; ++i) m->aPasso[i] = c.passo[i];
}

void RetalhoProcessor::dadoGroove()
{
    if (valor (kLockGrid) > 0.5f) return;
    const int seed = (int) valor (kSeed) % 999 + 1;
    define (kSeed, (float) seed);
    Cena c = cenaAtual();
    const int nCel = juce::jmin (maxCelulas, (c.i (kGGrid) == 1 ? 12 : 16) * (c.i (kGBars) + 1));
    retalho::dadoGroove (c.cel, nCel, (uint32_t) seed, juce::jmax (0.3f, c.f (kDAmount) * 0.01f));
    for (int l = 0; l < numLinhas; ++l) for (int i = 0; i < maxCelulas; ++i) m->aCel[l][i] = c.cel[l][i];
}

void RetalhoProcessor::limpaCadeados()
{
    for (int i = 0; i < maxPassos; ++i)
    {
        const uint32_t v = m->aPasso[i].load();
        m->aPasso[i] = passo (pTipo (v), pValor (v), false);
    }
}

void RetalhoProcessor::geraEstiloAgora()
{
    Cena c = cenaAtual();
    geraEstilo (c.i (kEstilo), c.i (kGBars) + 1, c.i (kGGrid) == 1, c.cel);
    for (int l = 0; l < numLinhas; ++l) for (int i = 0; i < maxCelulas; ++i) m->aCel[l][i] = c.cel[l][i];
    ultimoEstilo = c.i (kEstilo); ultimoBars = c.i (kGBars); ultimoGrid = c.i (kGGrid);
}

void RetalhoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n == 0 || nch == 0) return;
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    for (int k = 0; k < numCampos; ++k) val[k] = p[k]->load();

    Transporte tp;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            tp.tocando = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) tp.bpm = *bpm;
            if (auto q = pos->getPpqPosition()) { tp.ppq = *q; tp.temPpq = true; }
            if (auto ts = pos->getTimeSignature()) tp.numerador = juce::jmax (1, ts->numerator);
        }

    // MIDI: C3–F3 (60–65) disparam os pads 1–6; velocity = volume do fill
    eventos.clear();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int nota = msg.getNoteNumber();
        if (! msg.isNoteOnOrOff() || nota < 60 || nota > 65) continue;
        Evento e;
        e.amostra = juce::jlimit (0, n - 1, meta.samplePosition);
        e.pad = nota - 60;
        e.tipo = msg.isNoteOn() ? evAperta : evSolta;
        e.vel = msg.isNoteOn() ? msg.getFloatVelocity() : 0.0f;
        if (eventos.size() < eventos.capacity()) eventos.push_back (e);
    }

    // ---- SAMPLER: a captura grava a faixa; com amostra, a faixa sai e entra a amostra no tempo do projeto ----
    const int64_t nb = blocoIni.load (std::memory_order_relaxed) + 1;
    blocoIni.store (nb, std::memory_order_release);
    {
        const double bpm = tp.bpm > 1.0 ? tp.bpm : 120.0;
        const double spbS = fs * 60.0 / bpm, bpbS = tp.numerador > 0 ? (double) tp.numerador : 4.0;
        const bool host = tp.tocando && tp.temPpq;
        int ce = capEstado.load();
        if (ce != 0 && ! host) { if (ce == 2) capEstado = ce = 1; capPos = 0; }   // parou no meio: espera o próximo play
        int ini = 0;
        if (ce == 1 && host)
        {
            const double b0 = tp.ppq, b1 = b0 + n / spbS;
            const double tb = std::ceil (b0 / bpbS - 1e-9) * bpbS;
            if (tb < b1)
            {
                const double Lb = capBars.load() * bpbS;
                ini = juce::jlimit (0, n, (int) std::llround ((tb - b0) * spbS));
                capLen = (int) juce::jmin<int64_t> (capBuf.getNumSamples(), (int64_t) std::llround (Lb * spbS));
                capPos = 0;
                capOff = std::fmod (tb, Lb);
                capEstado = ce = 2;
            }
        }
        if (ce == 2)
        {
            int pos = capPos.load();
            const int len = capLen.load();
            const int k = juce::jmin (n - ini, len - pos);
            for (int c = 0; c < 2 && k > 0; ++c) capBuf.copyFrom (c, pos, buffer, juce::jmin (c, nch - 1), ini, k);
            pos += juce::jmax (0, k);
            capPos = pos;
            if (pos >= len) { capEstado = 0; capPronta = true; }
        }

        Amostra* a = ativa.load (std::memory_order_acquire);
        if (a != nullptr && ce == 0)
        {
            const int N = a->buf.getNumSamples();
            int i0 = juce::jlimit (0, N, (int) std::lround (selIni.load() * N)), i1 = juce::jlimit (0, N, (int) std::lround (selFim.load() * N));
            if (i1 - i0 < 256) { i0 = 0; i1 = N; }
            const double selLen = (double) (i1 - i0);
            const int K = juce::jlimit (1, 32, (int) std::ceil (selLen / (bpbS * spbS) - 0.15));
            loopK = K;
            const double Lb = K * bpbS, fade = 0.002 * fs;
            const float* s0 = a->buf.getReadPointer (0);
            const float* s1 = a->buf.getReadPointer (1);
            if (host)
            {
                for (int i = 0; i < n; ++i)
                {
                    double ph = std::fmod (tp.ppq + i / spbS - a->offBeats, Lb);
                    if (ph < 0.0) ph += Lb;
                    const double p = i0 + ph * spbS;
                    float l = 0.0f, r = 0.0f;
                    if (p < (double) (i1 - 1))
                    {
                        const int k = (int) p;
                        const float t = (float) (p - k);
                        const float e = (float) (juce::jmin (1.0, (p - i0 + 1.0) / fade) * juce::jmin (1.0, ((double) (i1 - 1) - p) / fade));
                        l = (s0[k] + (s0[k + 1] - s0[k]) * t) * e;
                        r = (s1[k] + (s1[k + 1] - s1[k]) * t) * e;
                    }
                    if (nch > 1) { buffer.setSample (0, i, l); buffer.setSample (1, i, r); }
                    else buffer.setSample (0, i, 0.5f * (l + r));
                    if (i == 0) posA = (float) (ph * spbS / juce::jmax (1.0, selLen));
                }
            }
            else
            {
                for (int c = 0; c < nch; ++c) buffer.clear (c, 0, n);
                posA = -1.0f;
            }
        }
        else posA = -1.0f;
    }

    m->process (buffer.getArrayOfWritePointers(), nch, n, val, tp, eventos.data(), (int) eventos.size());

    // 0.1.4: com amostra, o PICOTE / ESTICA tocam o RESULTADO pronto, em loop no tempo do projeto, no lugar da amostra crua
    {
        const int md = (int) std::lround (val[kModo]);
        const Resultado* rs = resAtivo.load (std::memory_order_acquire);
        if ((md == mPicote || md == mEstica) && rs != nullptr && ativa.load (std::memory_order_acquire) != nullptr
            && capEstado.load() == 0 && val[kBypass] < 0.5f)
        {
            const int R = rs->buf.getNumSamples();
            if (tp.tocando && tp.temPpq && R > 0)
            {
                const double spbS = fs * 60.0 / (tp.bpm > 1.0 ? tp.bpm : 120.0);
                const double Lb = (double) R / spbS;
                const float w = juce::jlimit (0.0f, 1.0f, val[kDryWet] * 0.01f);
                const float gs = dbGanho (val[kOutput]);
                const float* r0 = rs->buf.getReadPointer (0);
                const float* r1 = rs->buf.getReadPointer (1);
                for (int i = 0; i < n; ++i)
                {
                    double ph = std::fmod (tp.ppq + i / spbS, Lb);
                    if (ph < 0.0) ph += Lb;
                    const int k = juce::jlimit (0, R - 1, (int) (ph * spbS));
                    const float l = r0[k], r = r1[k];
                    if (nch > 1)
                    {
                        buffer.setSample (0, i, buffer.getSample (0, i) * (1.0f - w) + l * w * gs);
                        buffer.setSample (1, i, buffer.getSample (1, i) * (1.0f - w) + r * w * gs);
                    }
                    else buffer.setSample (0, i, buffer.getSample (0, i) * (1.0f - w) + 0.5f * (l + r) * w * gs);
                }
            }
        }
    }

    // preview: toca a janela no lugar da saída
    if (prevReinicia.exchange (false)) prevPos = 0;
    int est = prevEstado.load();
    if (est == 1 && m->compassoVersao.load() != prevCv.load()) { prevEstado = est = 2; prevPos = 0; }
    if (est == 2)
    {
        const int s = prevSlot.load(), len = juce::jmin (prevLen.load(), prevBuf[s].getNumSamples());
        int pos = prevPos.load();
        for (int i = 0; i < n; ++i)
        {
            if (pos >= len)
            {
                if (previewLoop.load() && len > 0) pos = 0;
                else { for (int c = 0; c < nch; ++c) buffer.clear (c, i, n - i); prevEstado = 0; break; }
            }
            for (int c = 0; c < nch; ++c) buffer.setSample (c, i, prevBuf[s].getSample (juce::jmin (c, 1), pos));
            ++pos;
        }
        prevPos = pos;
    }
    blocoFim.store (nb, std::memory_order_release);
}

// 15 Hz, na thread da tela
void RetalhoProcessor::timerCallback()
{
    // pad disparado (MIDI, automação ou clique): a tela mostra a cena dele
    if (const int pd = m->padPedido.exchange (-1); pd >= 0) carregaPad (pd);

    // captura pronta → vira a amostra (trecho em silêncio é descartado)
    if (capPronta.exchange (false))
    {
        const int len = juce::jmin (capLen.load(), capBuf.getNumSamples());
        if (len > 0 && capBuf.getMagnitude (0, len) > 0.01f)
        {
            juce::AudioBuffer<float> b (2, len);
            for (int c = 0; c < 2; ++c) b.copyFrom (c, 0, capBuf, c, 0, len);
            selIni = 0.0f; selFim = 1.0f;
            instalaAmostra (b, fs, "CAPTURA " + juce::String (capBars.load()) + (capBars.load() > 1 ? " BARS" : " BAR"), capOff.load(), true);
            capVazia = false;
        }
        else capVazia = true;
    }
    {
        const juce::ScopedLock sl (travaAmostra);
        if (! aposentadas.empty()) recolheAposentadas (false);
    }

    // GROOVE: STYLE / BARS / GRID mudou → refaz o pattern (a não ser com LOCK GRID)
    const int es = (int) valor (kEstilo), gb = (int) valor (kGBars), gg = (int) valor (kGGrid);
    if (es != ultimoEstilo || gb != ultimoBars || gg != ultimoGrid)
    {
        if (valor (kLockGrid) < 0.5f) geraEstiloAgora();
        ultimoEstilo = es; ultimoBars = gb; ultimoGrid = gg;
    }
    atualizaVista (false);
}

void RetalhoProcessor::atualizaVista (bool forcar)
{
    if (const Amostra* am = ativa.load (std::memory_order_acquire); am != nullptr)
    {
        vistaDaAmostra (*am, forcar);
        if (ouvirAoCarregar && vistaAtual.valida)
        {
            ouvirAoCarregar = false;
            if (cenaAtual().i (kModo) != mGroove) preview (2);   // soltou o arquivo: já toca o resultado
        }
        return;
    }
    pontoFr = -1.0f; nPontos = 0;
    const double spb = m->spbA.load(), bpb = m->bpbA.load();
    if (spb < 100.0) return;
    const Cena c = cenaAtual();
    const int modo = c.i (kModo);
    const double bars = modo == mGroove ? (double) (c.i (kGBars) + 1) : compassosLength (c.i (kLength));
    const int64_t L = (int64_t) std::llround (bars * bpb * spb);
    const int64_t N = (int64_t) std::llround (3.0 * bpb * spb);   // 1 compasso antes + até 2 de janela
    if (N <= 0 || L <= 0 || N > (int64_t) (fs * 15.0)) return;

    bool nova = false;
    const int cv = m->compassoVersao.load();
    if (cv != vistoCompasso || (int64_t) snap[0].size() != N)
    {
        vistoCompasso = cv;
        const int64_t fim = m->compassoA.load(), agora = m->agoraA.load();
        if (fim > 0 && agora - (fim - N) < (int64_t) (fs * 15.0))
        {
            std::vector<float> t[2];
            float pico = 0.0f;
            const auto& an = m->anel();
            for (int ch = 0; ch < 2; ++ch)
            {
                t[ch].resize ((size_t) N);
                for (int64_t i = 0; i < N; ++i)
                {
                    const int64_t a = fim - N + i;
                    const float x = a >= 0 ? an.at (ch, a) : 0.0f;
                    t[ch][(size_t) i] = x;
                    pico = std::max (pico, std::abs (x));
                }
            }
            // janela em silêncio (transporte parado, voz calada) não apaga a última com voz
            if (pico > 0.003f || snap[0].size() != (size_t) N)
            {
                snap[0].swap (t[0]); snap[1].swap (t[1]);
                snapIni = fim - N;
                nova = true;
            }
        }
    }
    if (snap[0].size() != (size_t) N) return;
    const unsigned masc = m->mascara (val);
    const uint64_t h = hashCena (c, masc, spb);
    if (! nova && ! forcar && h == ultimoHash) return;
    ultimoHash = h;

    const int64_t inicio = snapIni + N - L;
    Trecho tr;
    tr.d[0] = snap[0].data(); tr.d[1] = snap[1].data(); tr.n = N; tr.base = snapIni;

    // ataques na janela (o anel do motor guarda os 128 últimos)
    Ataques at;
    {
        std::vector<int64_t> a;
        const int na = juce::jmin (128, m->nAtaques.load());
        for (int i = 0; i < na; ++i) { const int64_t x = m->ataque[i].load(); if (x >= snapIni && x < snapIni + N) a.push_back (x); }
        std::sort (a.begin(), a.end());
        at.n = juce::jmin (128, (int) a.size());
        for (int i = 0; i < at.n; ++i) at.v[i] = a[(size_t) i];
    }
    montaVista (c, masc, spb, bpb, L, inicio, tr, at, false);
}

// ataques de uma amostra inteira (envelope de 5 ms; subida forte acima do piso; 80 ms entre um e outro)
static std::vector<int64_t> detectaAtaques (const juce::AudioBuffer<float>& b, double fs)
{
    std::vector<int64_t> r;
    const int n = b.getNumSamples(), nc = juce::jmax (1, b.getNumChannels());
    const int hop = juce::jmax (64, (int) (fs * 0.005));
    const int nf = n / hop;
    if (nf < 2) return r;
    std::vector<float> e ((size_t) nf, 0.0f);
    float pk = 0.0f;
    for (int f = 0; f < nf; ++f)
    {
        double s = 0.0;
        for (int i = f * hop; i < (f + 1) * hop; ++i)
        {
            float x = 0.0f;
            for (int c = 0; c < nc; ++c) x += b.getSample (c, i);
            x /= (float) nc;
            s += (double) x * x;
        }
        e[(size_t) f] = (float) std::sqrt (s / hop);
        pk = juce::jmax (pk, e[(size_t) f]);
    }
    if (pk < 1.0e-4f) return r;
    const float piso = pk * 0.08f;                 // ~ −22 dB do pico: o que "soa"
    const int gap = juce::jmax (1, (int) (0.08 * fs / hop));
    int ultimo = -gap;
    for (int f = 0; f < nf; ++f)
    {
        float ant = 1.0e9f;
        for (int j = juce::jmax (0, f - 6); j < f; ++j) ant = juce::jmin (ant, e[(size_t) j]);
        if (f == 0) ant = 0.0f;
        if (e[(size_t) f] > piso && e[(size_t) f] > 1.8f * ant + pk * 0.01f && f - ultimo >= gap)
        {
            // o começo de verdade: a primeira amostra que passa da metade do nível do quadro, 1 ms antes
            const float lim = e[(size_t) f] * 0.5f;
            int64_t at = (int64_t) f * hop;
            for (int i = juce::jmax (0, (f - 1) * hop); i < (f + 1) * hop && i < n; ++i)
            {
                float x = 0.0f;
                for (int c = 0; c < nc; ++c) x += std::abs (b.getSample (c, i));
                if (x / nc > lim) { at = i; break; }
            }
            r.push_back (juce::jmax<int64_t> (0, at - (int64_t) (fs * 0.001)));
            ultimo = f;
        }
    }
    return r;
}

// a amostra carregada: o efeito é renderizado direto do arquivo, a partir do PONTO, sem depender do play
void RetalhoProcessor::vistaDaAmostra (const Amostra& am, bool forcar)
{
    double spb = m->spbA.load(), bpb = m->bpbA.load();
    if (spb < 100.0) spb = fs * 0.5;   // ainda sem tempo do host: 120 BPM
    if (bpb < 1.0) bpb = 4.0;
    const Cena c = cenaAtual();
    const int modo = c.i (kModo);
    const double bars = modo == mGroove ? (double) (c.i (kGBars) + 1) : compassosLength (c.i (kLength));
    const int64_t L = (int64_t) std::llround (bars * bpb * spb);
    const int64_t N = am.buf.getNumSamples();
    if (L <= 0 || N < 256 || L > (int64_t) (fs * 30.0)) return;

    if (ataquesDe != (const void*) &am || ataquesN != (int) N)
    {
        ataquesA = detectaAtaques (am.buf, fs);
        ataquesDe = &am; ataquesN = (int) N;
    }
    int64_t i0 = juce::jlimit<int64_t> (0, N, std::llround (selIni.load() * N)), i1 = juce::jlimit<int64_t> (0, N, std::llround (selFim.load() * N));
    if (i1 - i0 < 256) { i0 = 0; i1 = N; }
    std::vector<int64_t> pts;
    for (auto a : ataquesA) if (a >= i0 && a < i1) pts.push_back (a);
    if (pts.empty()) pts.push_back (i0);
    const int np = (int) pts.size();
    const int pi = juce::jlimit (0, np - 1, ponto.load());
    ponto = pi; nPontos = np;
    const int64_t T = pts[(size_t) pi];
    pontoFr = (float) ((double) T / (double) N);
    estFim = i1;

    const unsigned masc = m->mascara (val);
    uint64_t h = hashCena (c, masc, spb);
    h = (h ^ (uint64_t) T) * 1099511628211ull;
    h = (h ^ (uint64_t) N) * 1099511628211ull;
    h = (h ^ (uint64_t) i1) * 1099511628211ull;
    h = (h ^ (uint64_t) (uintptr_t) &am) * 1099511628211ull;
    if (! forcar && h == ultimoHash && snap[0].size() == (size_t) L) return;
    ultimoHash = h;

    // ORIGINAL = a amostra do PONTO em diante (o preview ORIGINAL e a tela leem daqui)
    for (int ch = 0; ch < 2; ++ch)
    {
        snap[ch].assign ((size_t) L, 0.0f);
        const float* s = am.buf.getReadPointer (juce::jmin (ch, am.buf.getNumChannels() - 1));
        for (int64_t i = 0; i < L && T + i < N; ++i) snap[ch][(size_t) i] = s[T + i];
    }
    snapIni = T;
    vistoCompasso = -1;   // tirou a amostra: a janela ao vivo volta sozinha

    Trecho tr;
    tr.d[0] = am.buf.getReadPointer (0);
    tr.d[1] = am.buf.getReadPointer (juce::jmin (1, am.buf.getNumChannels() - 1));
    tr.n = N; tr.base = 0;
    Ataques at;
    {
        const int64_t de = T - (int64_t) (8.0 * bpb * spb);
        std::vector<int64_t> a;
        for (auto x : ataquesA) if (x >= de && x < T + L) a.push_back (x);
        if (a.size() > 128) a.erase (a.begin(), a.end() - 128);
        at.n = (int) a.size();
        for (int i = 0; i < at.n; ++i) at.v[i] = a[(size_t) i];
    }
    montaVista (c, masc, spb, bpb, L, T, tr, at, true);
}

void RetalhoProcessor::montaVista (const Cena& c, unsigned masc, double spb, double bpb, int64_t L, int64_t inicio,
                                   const Trecho& tr, const Ataques& at, bool offline)
{
    const int modo = c.i (kModo);
    const int64_t N = (int64_t) snap[0].size();
    Vista& v = vistaAtual;
    v.spb = spb; v.bpb = bpb; v.fs = fs; v.len = L;
    v.ataques.clear();
    for (int i = 0; i < at.n; ++i) if (at.v[i] >= inicio && at.v[i] < inicio + L) v.ataques.push_back ((float) ((double) (at.v[i] - inicio) / (double) L));

    // ORIGINAL por coluna
    auto colunasDe = [L] (const float* a, const float* b, std::vector<float>& mn, std::vector<float>& mx)
    {
        mn.assign (colunas, 0.0f); mx.assign (colunas, 0.0f);
        for (int x = 0; x < colunas; ++x)
        {
            const int64_t i0 = L * x / colunas, i1 = juce::jmax (i0 + 1, L * (x + 1) / colunas);
            float lo = 0.0f, hi = 0.0f;
            for (int64_t i = i0; i < i1; ++i) { const float s = 0.5f * (a[i] + b[i]); lo = juce::jmin (lo, s); hi = juce::jmax (hi, s); }
            mn[(size_t) x] = lo; mx[(size_t) x] = hi;
        }
    };
    colunasDe (snap[0].data() + (N - L), snap[1].data() + (N - L), v.oMin, v.oMax);

    // RESULT: o fill tocado em cima da janela, como se disparasse no começo dela
    outR[0].assign ((size_t) L, 0.0f); outR[1].assign ((size_t) L, 0.0f);
    v.picoFatia.clear();
    v.modo = modo;
    v.graos.clear();
    v.limpo = 1.0f;
    if (modo == mEstica)
    {
        // ESTICA: granular direto da amostra, do PONTO até o fim do trecho escolhido
        const int64_t fimReg = offline ? juce::jmax (inicio + 256, estFim) : tr.base + tr.n;
        renderEstica (c, tr, inicio, fimReg, fs, spb, L, outR[0].data(), outR[1].data(), &v.graos);
        v.plano.n = 0;
        v.chunkIni = v.chunkFim = 0.0f;
        v.regiaoSobreL = (float) ((double) (fimReg - inicio) / (double) L);
    }
    else if (modo != mGroove)
    {
        Contexto cx;
        cx.spb = spb; cx.bpb = bpb; cx.fs = fs; cx.inicio = inicio; cx.mascara = masc;
        int t, e;
        m->tomEmUso (val, t, e);
        cx.pcChunk = m->pcAgora.load() >= 0 ? m->pcAgora.load() : t;
        cx.at = &at;
        cx.offline = offline;
        monta (c, cx, v.plano);
        Render r;
        r.inicia (v.plano, (float) fs);
        for (int64_t i = 0; i < L; ++i)
        {
            float a, b;
            r.amostra (i, offline ? tr.base + tr.n - 1 : inicio + i, tr, a, b);
            outR[0][(size_t) i] = a; outR[1][(size_t) i] = b;
        }
        v.picoFatia.assign ((size_t) v.plano.n, 0.0f);
        for (int k = 0; k < v.plano.n; ++k)
        {
            const auto& f = v.plano.f[k];
            float pk = 0.0f;
            for (int64_t i = f.t0; i < f.t0 + f.dur && i < L; i += 2) pk = juce::jmax (pk, std::abs (outR[0][(size_t) i]) + std::abs (outR[1][(size_t) i]));
            v.picoFatia[(size_t) k] = pk * 0.5f;
        }
        v.chunkIni = 0.0f;
        v.chunkFim = v.plano.n > 0 ? (float) v.plano.f[0].dur / (float) L : 0.0f;
        if (v.plano.n > 1 && v.plano.f[1].L > 0)
        {
            v.chunkIni = (float) ((double) (v.plano.f[1].src - inicio) / (double) L);
            v.chunkFim = v.chunkIni + (float) v.plano.f[1].L / (float) L;
        }
    }
    else v.plano.n = 0;
    if (modo == mPicote && v.plano.n > 0)
    {
        int64_t limpo = 0;
        for (int k = 0; k < v.plano.n; ++k) if (v.plano.f[k].tipoPasso < 0) limpo += v.plano.f[k].dur;
        v.limpo = (float) ((double) limpo / (double) L);
    }
    // FADE OUT: o último quarto da frase desce até o silêncio
    if ((modo == mPicote || modo == mEstica) && c.b (kFadeFim))
    {
        const int64_t f0 = L - L / 4;
        for (int64_t i = f0; i < L; ++i)
        {
            const float x = 1.0f - (float) (i - f0) / (float) juce::jmax<int64_t> (1, L - f0);
            outR[0][(size_t) i] *= x * x; outR[1][(size_t) i] *= x * x;
        }
    }
    if (offline && (modo == mPicote || modo == mEstica)) publicaResultado (true);
    colunasDe (outR[0].data(), outR[1].data(), v.rMin, v.rMax);

    // chops (GROOVE): de onde vieram e a onda de cada um
    for (int s = 0; s < numChops; ++s)
    {
        const int len = m->chopLen[s].load();
        const int64_t o = m->chopOrigem[s].load();
        v.chopMs[s] = len > 0 ? (float) (1000.0 * len / fs) : 0.0f;
        v.chopIni[s] = v.chopFim[s] = -1.0f;
        if (! offline && len > 0 && o >= inicio && o < inicio + L)
        {
            v.chopIni[s] = (float) ((double) (o - inicio) / (double) L);
            v.chopFim[s] = juce::jmin (1.0f, v.chopIni[s] + (float) len / (float) L);
        }
        for (int x = 0; x < 48; ++x)
        {
            float pk = 0.0f;
            if (len > 0)
                for (int i = len * x / 48; i < len * (x + 1) / 48; i += 4) pk = juce::jmax (pk, std::abs (m->chopAt (s, 0, i)));
            v.chopOnda[s][x] = pk;
        }
    }
    v.valida = true;
    ++v.versao;
}

void RetalhoProcessor::preview (int qual)
{
    if (qual == 0 || (prevEstado.load() != 0 && prevQual.load() == qual)) { prevEstado = 0; return; }
    if (! vistaAtual.valida) return;
    const int64_t L = vistaAtual.len, N = (int64_t) snap[0].size();
    const int s = 1 - prevSlot.load();
    auto& b = prevBuf[s];
    const int len = (int) juce::jmin<int64_t> (L, b.getNumSamples());
    if (len <= 0) return;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < len; ++i)
            b.setSample (ch, i, qual == 1 ? snap[ch][(size_t) (N - L + i)] : outR[ch][(size_t) i]);
    prevEstado = 0;
    prevSlot = s;
    prevLen = len;
    prevQual = qual;
    prevCv = m->compassoVersao.load();
    prevReinicia = true;
    prevEstado = previewOnBar.load() ? 1 : 2;
}

juce::File RetalhoProcessor::exporta (bool temporario)
{
    if (! vistaAtual.valida || outR[0].empty() || cenaAtual().i (kModo) == mGroove) return {};
    const Cena ce = cenaAtual();
    const juce::String base = ce.i (kModo) == mEstica ? "ESTICA " + juce::String::fromUTF8 (ch::direcao[lim (ce.i (kEDir), 0, 5)])
                            : (ce.i (kModo) == mPicote ? "PICOTE " + juce::String::fromUTF8 (ch::receita[lim (ce.i (kReceita), 0, numReceitas - 1)]) : presetNome);
    const auto nome = "RETALHO " + base.replaceCharacters ("/\\:*?\"<>|", "---------") + " "
                      + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".wav";
    const auto pasta = temporario ? juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RETALHO")
                                  : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RETALHO").getChildFile ("Exports");
    pasta.createDirectory();
    const auto f = pasta.getChildFile (nome);
    juce::AudioBuffer<float> buf (2, (int) outR[0].size());
    for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, outR[ch].data(), (int) outR[ch].size());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    if (! static_cast<juce::FileOutputStream*> (os.get())->openedOk()) return {};
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (fs).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) return {};
    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    w.reset();
    return f;
}

// ---- A/B e estado ---------------------------------------------------------------------------------------------------
juce::ValueTree RetalhoProcessor::fotoCompleta() const
{
    auto v = cenaVt (cenaAtual(), presetNome);
    return v;
}

void RetalhoProcessor::aplicaFoto (const juce::ValueTree& v)
{
    aplicaCena (vtCena (v));
    presetNome = v.getProperty ("nome", presetNome).toString();
}

void RetalhoProcessor::trocaAB (int para)
{
    para = para ? 1 : 0;
    const int de = ab.load();
    if (para == de) return;
    slots[de] = fotoCompleta();
    if (slots[para].isValid()) aplicaFoto (slots[para]);
    else slots[para] = slots[de].createCopy();
    ab = para;
}

void RetalhoProcessor::copiaAB()
{
    const int de = ab.load();
    slots[de] = fotoCompleta();
    slots[1 - de] = slots[de].createCopy();
}

juce::AudioProcessorEditor* RetalhoProcessor::createEditor() { return new RetalhoEditor (*this); }

void RetalhoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int i = state.getNumChildren() - 1; i >= 0; --i)
        if (! state.getChild (i).hasType ("PARAM")) state.removeChild (i, nullptr);
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("uipro", uiPro.load(), nullptr);
    state.setProperty ("ab", ab.load(), nullptr);
    state.setProperty ("preset", presetNome, nullptr);
    const Cena c = cenaAtual();
    state.setProperty ("grade", juntaU (c.grade, maxGrade), nullptr);
    state.setProperty ("passo", juntaU (c.passo, maxPassos), nullptr);
    for (int l = 0; l < numLinhas; ++l) state.setProperty ("cel" + juce::String (l), juntaU (c.cel[l], maxCelulas), nullptr);
    {
        juce::StringArray lv;
        for (float x : c.livre) lv.add (juce::String (x, 3));
        state.setProperty ("livre", lv.joinIntoString (","), nullptr);
    }
    for (int i = 0; i < numPads; ++i)
    {
        auto t = cenaVt (padCena[i], padNome[i]);
        t.setProperty ("cheio", padCheio[i], nullptr);
        juce::ValueTree pd ("PAD" + juce::String (i));
        pd.copyPropertiesFrom (t, nullptr);
        state.appendChild (pd, nullptr);
    }
    {
        const juce::ScopedLock sl (travaAmostra);
        if (origBuf.getNumSamples() > 0 && origB64.isNotEmpty())
        {
            juce::ValueTree am ("AMOSTRA");
            am.setProperty ("nome", origNome, nullptr);
            am.setProperty ("fs", origFs, nullptr);
            am.setProperty ("off", origOff, nullptr);
            am.setProperty ("ini", selIni.load(), nullptr);
            am.setProperty ("fim", selFim.load(), nullptr);
            am.setProperty ("flac", origB64, nullptr);
            state.appendChild (am, nullptr);
        }
    }
    state.setProperty ("capbars", capBars.load(), nullptr);
    slots[ab.load()] = fotoCompleta();
    for (int i = 0; i < 2; ++i)
        if (slots[i].isValid())
        {
            juce::ValueTree s ("SLOT" + juce::String (i));
            s.copyPropertiesFrom (slots[i], nullptr);
            state.appendChild (s, nullptr);
        }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void RetalhoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = juce::ValueTree::fromXml (*xml);
    uiW = (int) vt.getProperty ("uiw", 0);
    uiPro = (int) vt.getProperty ("uipro", 0) ? 1 : 0;
    ab = (int) vt.getProperty ("ab", 0) ? 1 : 0;
    presetNome = vt.getProperty ("preset", "INIT").toString();
    {
        juce::ValueTree t ("CENA");
        t.copyPropertiesFrom (vt, nullptr);
        Cena c = vtCena (t);
        m->escreveArrays (c);
    }
    for (int i = 0; i < numPads; ++i)
    {
        auto pd = vt.getChildWithName ("PAD" + juce::String (i));
        if (! pd.isValid()) continue;
        padCena[i] = vtCena (pd);
        padCheio[i] = (bool) pd.getProperty ("cheio", false);
        padNome[i] = pd.getProperty ("nome", "").toString();
        m->gravaPad (i, padCena[i], padCheio[i]);
    }
    for (int i = 0; i < 2; ++i)
    {
        auto s = vt.getChildWithName ("SLOT" + juce::String (i));
        if (s.isValid()) { juce::ValueTree c ("CENA"); c.copyPropertiesFrom (s, nullptr); slots[i] = c; }
        else slots[i] = {};
    }
    // a amostra do projeto (FLAC em base64)
    capBars = juce::jlimit (1, 8, (int) vt.getProperty ("capbars", 2));
    {
        auto am = vt.getChildWithName ("AMOSTRA");
        const juce::String b64 = am.isValid() ? am.getProperty ("flac", "").toString() : juce::String();
        bool mesma;
        { const juce::ScopedLock sl (travaAmostra); mesma = b64 == origB64 && ! xml->hasAttribute ("sel"); }   // a 2ª foto refaz do FLAC (testa o projeto reaberto)
        if (b64.isEmpty()) { if (! mesma) limpaAmostra(); }
        else if (! mesma)
        {
            juce::MemoryOutputStream mo;
            bool ok = juce::Base64::convertFromBase64 (mo, b64);
            if (ok)
            {
                juce::FlacAudioFormat flac;
                std::unique_ptr<juce::AudioFormatReader> r (flac.createReaderFor (new juce::MemoryInputStream (mo.getData(), mo.getDataSize(), true), true));
                ok = r != nullptr && r->lengthInSamples > 0;
                if (ok)
                {
                    juce::AudioBuffer<float> b (juce::jlimit (1, 2, (int) r->numChannels), (int) r->lengthInSamples);
                    r->read (&b, 0, b.getNumSamples(), 0, true, true);
                    instalaAmostra (b, (double) am.getProperty ("fs", r->sampleRate), am.getProperty ("nome", "AMOSTRA").toString(),
                                    (double) am.getProperty ("off", 0.0), false);
                    const juce::ScopedLock sl (travaAmostra);
                    origB64 = b64;
                }
            }
        }
        if (am.isValid())
        {
            selIni = juce::jlimit (0.0f, 1.0f, (float) am.getProperty ("ini", 0.0f));
            selFim = juce::jlimit (0.0f, 1.0f, (float) am.getProperty ("fim", 1.0f));
        }
    }
    if ((int) vt.getProperty ("capturar", 0) > 0) capturar ((int) vt.getProperty ("capturar", 2));   // programa da foto
    for (int i = vt.getNumChildren() - 1; i >= 0; --i)
        if (! vt.getChild (i).hasType ("PARAM")) vt.removeChild (i, nullptr);
    const bool fotoGroove = xml->hasAttribute ("sel");   // programa da foto (shots): 2ª foto na aba GROOVE
    apvts.replaceState (vt);
    ultimoEstilo = (int) valor (kEstilo); ultimoBars = (int) valor (kGBars); ultimoGrid = (int) valor (kGGrid);
    if (fotoGroove)   // a 2ª foto mostra o PRO na aba GROOVE (a 1ª é o SIMPLE)
    {
        uiPro = 1;
        aplicaPreset (presetPorNome ("OFFBEAT VOX"));
        define (kGrooveOn, 1.0f);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RetalhoProcessor(); }
