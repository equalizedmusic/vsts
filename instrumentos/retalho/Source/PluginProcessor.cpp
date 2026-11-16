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
    aplicaPreset (presetPorNome ("PSY RISER 1/32"));   // abre no riser do esboço (o projeto salvo substitui)
    vistaAtual.oMin.assign (colunas, 0.0f); vistaAtual.oMax.assign (colunas, 0.0f);
    vistaAtual.rMin.assign (colunas, 0.0f); vistaAtual.rMax.assign (colunas, 0.0f);
    slots[0] = fotoCompleta();
    ultimoEstilo = (int) valor (kEstilo); ultimoBars = (int) valor (kGBars); ultimoGrid = (int) valor (kGGrid);
    startTimerHz (15);
}

RetalhoProcessor::~RetalhoProcessor() { stopTimer(); }

bool RetalhoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void RetalhoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    m->prepare (sampleRate);
    for (auto& b : prevBuf) b.setSize (2, (int) (sampleRate * 9.0), false, true, false);
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
    if (r.modo == mEdit) r.sub = "EDIT " + juce::String::fromUTF8 ("\xC2\xB7 ") + juce::String (ch::length[lim (c.i (kLength), 0, 2)]);
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

    m->process (buffer.getArrayOfWritePointers(), nch, n, val, tp, eventos.data(), (int) eventos.size());

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
}

// 15 Hz, na thread da tela
void RetalhoProcessor::timerCallback()
{
    // pad disparado (MIDI, automação ou clique): a tela mostra a cena dele
    if (const int pd = m->padPedido.exchange (-1); pd >= 0) carregaPad (pd);

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

    Vista& v = vistaAtual;
    v.spb = spb; v.bpb = bpb; v.fs = fs; v.len = L;
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
        v.ataques.clear();
        for (int i = 0; i < at.n; ++i) if (at.v[i] >= inicio) v.ataques.push_back ((float) ((double) (at.v[i] - inicio) / (double) L));
    }

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
    if (modo != mGroove)
    {
        Contexto cx;
        cx.spb = spb; cx.bpb = bpb; cx.fs = fs; cx.inicio = inicio; cx.mascara = masc;
        int t, e;
        m->tomEmUso (val, t, e);
        cx.pcChunk = m->pcAgora.load() >= 0 ? m->pcAgora.load() : t;
        cx.at = &at;
        monta (c, cx, v.plano);
        Render r;
        r.inicia (v.plano, (float) fs);
        for (int64_t i = 0; i < L; ++i)
        {
            float a, b;
            r.amostra (i, inicio + i, tr, a, b);
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
    colunasDe (outR[0].data(), outR[1].data(), v.rMin, v.rMax);

    // chops (GROOVE): de onde vieram e a onda de cada um
    for (int s = 0; s < numChops; ++s)
    {
        const int len = m->chopLen[s].load();
        const int64_t o = m->chopOrigem[s].load();
        v.chopMs[s] = len > 0 ? (float) (1000.0 * len / fs) : 0.0f;
        v.chopIni[s] = v.chopFim[s] = -1.0f;
        if (len > 0 && o >= inicio && o < inicio + L)
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
    const auto nome = "RETALHO " + presetNome.replaceCharacters ("/\\:*?\"<>|", "---------") + " "
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
