#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> (juce::ParameterID { "amount", 1 }, "Amount", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 72.0f,
                                juce::AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "character", 1 }, "Character (clean / warm)", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 30.0f,
                                juce::AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "punch", 1 }, "Punch", juce::NormalisableRange<float> (-5.0f, 5.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "width", 1 }, "Width", juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f,
                                juce::AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "tilt", 1 }, "Tone tilt", juce::NormalisableRange<float> (-3.0f, 3.0f, 0.1f), 0.0f,
                                juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "monobass", 1 }, "Bass mono < 120 Hz", true));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "listen", 1 }, "Listen to",
                                                         juce::StringArray { "A (master)", "B (original, same loudness)", "REF", "BYPASS" }, 0));

    // MANUAL: multibanda e DYNAMIC EQ por cima do AUTO (ao ligar, a tela copia o que o AUTO fazia)
    using B = juce::AudioParameterBool;
    const magos::Ajustes d;
    auto faixa = [] (float a, float b, float passo, float centro)
    {
        juce::NormalisableRange<float> r (a, b, passo);
        r.setSkewForCentre (centro);
        return r;
    };
    l.add (std::make_unique<B> (juce::ParameterID { "mbman", 1 }, "Multiband manual", false));
    for (int k = 0; k < magos::NM; ++k)
    {
        const juce::String n = magos::nomeBanda (k), id = "mb" + juce::String (k);
        l.add (std::make_unique<P> (juce::ParameterID { id + "thr", 1 }, n + " threshold", juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f),
                                    d.mbThr[k], juce::AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<P> (juce::ParameterID { id + "rat", 1 }, n + " ratio", faixa (1.0f, 20.0f, 0.01f, 3.0f), d.mbRat[k],
                                    juce::AudioParameterFloatAttributes().withLabel (":1")));
        l.add (std::make_unique<P> (juce::ParameterID { id + "atk", 1 }, n + " attack", faixa (0.1f, 300.0f, 0.1f, 15.0f), d.mbAtk[k],
                                    juce::AudioParameterFloatAttributes().withLabel ("ms")));
        l.add (std::make_unique<P> (juce::ParameterID { id + "rel", 1 }, n + " release", faixa (5.0f, 3000.0f, 1.0f, 150.0f), d.mbRel[k],
                                    juce::AudioParameterFloatAttributes().withLabel ("ms")));
        l.add (std::make_unique<P> (juce::ParameterID { id + "gain", 1 }, n + " gain", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f),
                                    0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }
    l.add (std::make_unique<B> (juce::ParameterID { "eqman", 1 }, "Dynamic EQ manual", false));
    for (int z = 0; z < magos::NZ; ++z)
    {
        const juce::String n = magos::zona (z).nome, id = "eq" + juce::String (z);
        l.add (std::make_unique<B> (juce::ParameterID { id + "on", 1 }, n + " on", true));
        l.add (std::make_unique<P> (juce::ParameterID { id + "rng", 1 }, n + " range", juce::NormalisableRange<float> (0.0f, 8.0f, 0.1f),
                                    d.eqRange[z], juce::AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<P> (juce::ParameterID { id + "thr", 1 }, n + " threshold", juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f),
                                    d.eqThr[z], juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }
    return l;
}

MagosProcessor::MagosProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MAGOS", parametros())
{
    pAmount = apvts.getRawParameterValue ("amount");
    pChar = apvts.getRawParameterValue ("character");
    pPunch = apvts.getRawParameterValue ("punch");
    pWidth = apvts.getRawParameterValue ("width");
    pTilt = apvts.getRawParameterValue ("tilt");
    pMono = apvts.getRawParameterValue ("monobass");
    pOuvir = apvts.getRawParameterValue ("listen");
    pMbMan = apvts.getRawParameterValue ("mbman");
    pEqMan = apvts.getRawParameterValue ("eqman");
    for (int k = 0; k < magos::NM; ++k)
    {
        const juce::String id = "mb" + juce::String (k);
        pMb[k][0] = apvts.getRawParameterValue (id + "thr");
        pMb[k][1] = apvts.getRawParameterValue (id + "rat");
        pMb[k][2] = apvts.getRawParameterValue (id + "atk");
        pMb[k][3] = apvts.getRawParameterValue (id + "rel");
        pMb[k][4] = apvts.getRawParameterValue (id + "gain");
    }
    for (int z = 0; z < magos::NZ; ++z)
    {
        const juce::String id = "eq" + juce::String (z);
        pEq[z][0] = apvts.getRawParameterValue (id + "on");
        pEq[z][1] = apvts.getRawParameterValue (id + "rng");
        pEq[z][2] = apvts.getRawParameterValue (id + "thr");
    }
    setLatencySamples (motor.latenciaAmostras());
}

MagosProcessor::~MagosProcessor()
{
    if (leitor != nullptr) leitor->stopThread (4000);
    motor.ref = nullptr;
}

bool MagosProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MagosProcessor::prepareToPlay (double sampleRate, int)
{
    const bool mudou = std::abs (sampleRate - taxa) > 1.0;
    taxa = sampleRate;
    motor.prepare (sampleRate);
    setLatencySamples (motor.latenciaAmostras());
    // o trecho da referência para ouvir está na taxa antiga: lê de novo
    if (mudou && arquivoReferencia.existsAsFile())
        juce::MessageManager::callAsync ([safe = juce::WeakReference<MagosProcessor> (this)]
        {
            if (auto* s = safe.get()) s->carregaReferencia (s->arquivoReferencia);
        });
}

void MagosProcessor::define (const juce::String& id, float valorReal)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (valorReal));
        p->endChangeGesture();
    }
}

void MagosProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n == 0 || nch == 0) return;
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    magos::Ajustes a;
    a.amount = pAmount->load() / 100.0f;
    a.character = pChar->load() / 100.0f;
    a.punch = pPunch->load();
    a.width = pWidth->load() / 100.0f;
    a.tilt = pTilt->load();
    a.monoBass = pMono->load() > 0.5f;
    a.ouvir = juce::jlimit (0, 3, juce::roundToInt (pOuvir->load()));
    a.sim = sim.load();
    a.mbManual = pMbMan->load() > 0.5f;
    a.eqManual = pEqMan->load() > 0.5f;
    for (int k = 0; k < magos::NM; ++k)
    {
        a.mbThr[k] = pMb[k][0]->load(); a.mbRat[k] = pMb[k][1]->load(); a.mbAtk[k] = pMb[k][2]->load();
        a.mbRel[k] = pMb[k][3]->load(); a.mbGain[k] = pMb[k][4]->load();
    }
    for (int z = 0; z < magos::NZ; ++z)
    {
        a.eqOn[z] = pEq[z][0]->load() > 0.5f; a.eqRange[z] = pEq[z][1]->load(); a.eqThr[z] = pEq[z][2]->load();
    }
    motor.process (buffer.getArrayOfWritePointers(), nch, n, a);
}

juce::AudioProcessorEditor* MagosProcessor::createEditor() { return new MagosEditor (*this); }

// ------------------------------------------------------------------------------------------------------------------
// REFERÊNCIA
// ------------------------------------------------------------------------------------------------------------------
namespace
{
// o trecho mais alto (janela de até 60 s, pela energia de cada segundo) para ouvir no REF
juce::Range<juce::int64> trechoMaisAlto (const juce::AudioBuffer<float>& b, double fs, double segundos)
{
    const juce::int64 n = b.getNumSamples(), w = (juce::int64) (fs * segundos), seg = (juce::int64) fs;
    if (n <= w) return { 0, n };
    std::vector<double> e;
    for (juce::int64 i = 0; i + seg <= n; i += seg)
    {
        double s = 0;
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            const float* x = b.getReadPointer (c) + i;
            for (juce::int64 k = 0; k < seg; ++k) s += (double) x[k] * x[k];
        }
        e.push_back (s);
    }
    const int nw = (int) (w / seg);
    double soma = 0, melhor = -1;
    int ini = 0;
    for (int i = 0; i < (int) e.size(); ++i)
    {
        soma += e[(size_t) i];
        if (i >= nw) soma -= e[(size_t) (i - nw)];
        if (i >= nw - 1 && soma > melhor) { melhor = soma; ini = i - nw + 1; }
    }
    return { (juce::int64) ini * seg, juce::jmin (n, (juce::int64) ini * seg + w) };
}

class LeitorReferencia : public juce::Thread
{
public:
    LeitorReferencia (juce::File f, double fsAlvo, juce::WeakReference<MagosProcessor> d)
        : juce::Thread ("magos-ref"), arquivo (std::move (f)), fs (fsAlvo), dono (std::move (d)) {}

    void run() override
    {
        juce::String erro;
        auto r = le (erro);
        if (threadShouldExit()) return;
        juce::MessageManager::callAsync ([d = dono, ref = std::shared_ptr<magos::Referencia> (r.release()), erro] () mutable
        {
            if (auto* p = d.get()) p->recebeReferencia (ref, erro);
        });
    }

private:
    std::unique_ptr<magos::Referencia> le (juce::String& erro)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (arquivo));
        if (rd == nullptr) { erro = "Could not read this file (WAV, AIFF, FLAC, OGG or MP3)."; return {}; }
        const double fsArq = rd->sampleRate;
        const juce::int64 maxN = (juce::int64) (fsArq * 60.0 * 15.0);   // até 15 min
        const juce::int64 n = juce::jmin (rd->lengthInSamples, maxN);
        if (fsArq < 8000.0 || n < (juce::int64) (fsArq * 5.0)) { erro = "The reference must be at least 5 seconds long."; return {}; }
        juce::AudioBuffer<float> b (2, (int) n);
        rd->read (&b, 0, (int) n, 0, true, true);
        if (rd->numChannels == 1) b.copyFrom (1, 0, b, 0, 0, (int) n);
        if (threadShouldExit()) return {};
        auto ref = std::make_unique<magos::Referencia>();
        ref->an = magos::analisa (b.getReadPointer (0), b.getReadPointer (1), n, fsArq);
        if (! ref->an.ok) { erro = "The reference seems to be silent."; return {}; }
        // trecho para ouvir: o minuto mais alto, na taxa do projeto
        const auto tr = trechoMaisAlto (b, fsArq, 60.0);
        const double razao = fsArq / fs;
        const int nSai = (int) ((double) tr.getLength() / razao);
        ref->L.resize ((size_t) nSai);
        ref->R.resize ((size_t) nSai);
        for (int c = 0; c < 2; ++c)
        {
            juce::LagrangeInterpolator li;
            li.process (razao, b.getReadPointer (c) + tr.getStart(), (c == 0 ? ref->L : ref->R).data(), nSai);
        }
        std::snprintf (ref->nome, sizeof (ref->nome), "%s", arquivo.getFileName().toRawUTF8());
        return ref;
    }

    juce::File arquivo;
    double fs;
    juce::WeakReference<MagosProcessor> dono;
};
} // namespace

void MagosProcessor::carregaReferencia (const juce::File& f)
{
    if (leitor != nullptr) leitor->stopThread (4000);
    arquivoReferencia = f;
    erroReferencia = {};
    carregando = 1;
    leitor = std::make_unique<LeitorReferencia> (f, taxa, juce::WeakReference<MagosProcessor> (this));
    leitor->startThread (juce::Thread::Priority::low);
}

void MagosProcessor::recebeReferencia (std::shared_ptr<magos::Referencia> r, const juce::String& erro)
{
    carregando = 0;
    if (r == nullptr) { erroReferencia = erro; return; }
    auto nova = std::make_unique<magos::Referencia> (std::move (*r));
    trocaReferencia (std::move (nova));
}

void MagosProcessor::trocaReferencia (std::unique_ptr<magos::Referencia> nova)
{
    auto* p = nova.get();
    refs.push_back (std::move (nova));
    motor.ref = p;
    // as antigas só saem quando sobram muitas (o áudio pode estar no meio de um bloco com a anterior)
    while (refs.size() > 3) refs.erase (refs.begin());
}

void MagosProcessor::tiraReferencia()
{
    if (leitor != nullptr) leitor->stopThread (4000);
    carregando = 0;
    motor.ref = nullptr;
    arquivoReferencia = juce::File();
    erroReferencia = {};
}

juce::String MagosProcessor::nomeReferencia() const
{
    const auto* r = motor.ref.load();
    return r != nullptr ? juce::String::fromUTF8 (r->nome) : juce::String();
}

// ------------------------------------------------------------------------------------------------------------------
// estado
// ------------------------------------------------------------------------------------------------------------------
namespace
{
juce::String lista (const float* v, int n)
{
    juce::String s;
    for (int i = 0; i < n; ++i) s << juce::String (v[i], 3) << (i + 1 < n ? " " : "");
    return s;
}
void leLista (const juce::var& txt, float* v, int n)
{
    auto p = juce::StringArray::fromTokens (txt.toString(), " ", "");
    for (int i = 0; i < n && i < p.size(); ++i) v[i] = p[i].getFloatValue();
}
} // namespace

void MagosProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("style", motor.style.load(), nullptr);
    state.setProperty ("alvo", motor.alvoOpcao.load(), nullptr);
    state.setProperty ("casaTom", motor.casaTom.load() ? 1 : 0, nullptr);
    state.setProperty ("casaLoud", motor.casaLoud.load() ? 1 : 0, nullptr);
    const auto f = motor.feitico;
    state.setProperty ("cast", f.feito ? 1 : 0, nullptr);
    state.setProperty ("tom", lista (f.tom, magos::NE), nullptr);
    state.setProperty ("limiar", lista (f.limiar, magos::NM), nullptr);
    state.setProperty ("drive", f.drive, nullptr);
    state.setProperty ("alvoLufs", f.alvoLufs, nullptr);
    state.setProperty ("alvoForma", lista (f.alvoForma, magos::NB), nullptr);
    state.setProperty ("relAlvo", lista (f.relAlvo, magos::NZ), nullptr);
    state.setProperty ("antesLufs", f.antesLufs, nullptr);
    state.setProperty ("casAntes", f.casamentoAntes, nullptr);
    if (const auto* r = motor.ref.load())
    {
        state.setProperty ("refNome", juce::String::fromUTF8 (r->nome), nullptr);
        state.setProperty ("refEsp", lista (r->an.espectro, magos::NB), nullptr);
        state.setProperty ("refLufs", r->an.lufs, nullptr);
        state.setProperty ("refTp", r->an.picoTp, nullptr);
        state.setProperty ("refArq", arquivoReferencia.getFullPathName(), nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MagosProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = juce::ValueTree::fromXml (*xml);
    uiW = (int) vt.getProperty ("uiw", 0);
    motor.style = juce::jlimit (0, magos::NSTYLE - 1, (int) vt.getProperty ("style", 0));
    motor.alvoOpcao = juce::jlimit (0, magos::NALVO - 1, (int) vt.getProperty ("alvo", 0));
    motor.casaTom = (int) vt.getProperty ("casaTom", 1) != 0;
    motor.casaLoud = (int) vt.getProperty ("casaLoud", 1) != 0;
    magos::Feitico f;
    f.feito = (int) vt.getProperty ("cast", 0) != 0;
    leLista (vt.getProperty ("tom"), f.tom, magos::NE);
    leLista (vt.getProperty ("limiar"), f.limiar, magos::NM);
    f.drive = (float) vt.getProperty ("drive", 0.0f);
    f.alvoLufs = (float) vt.getProperty ("alvoLufs", -8.0f);
    leLista (vt.getProperty ("alvoForma"), f.alvoForma, magos::NB);
    leLista (vt.getProperty ("relAlvo"), f.relAlvo, magos::NZ);
    f.antesLufs = (float) vt.getProperty ("antesLufs", -200.0f);
    f.casamentoAntes = (float) vt.getProperty ("casAntes", 0.0f);
    motor.restaura (f);
    // referência: a análise volta na hora (o CAST e a tela não precisam do arquivo); o áudio para ouvir no REF é lido de
    // novo em segundo plano, se o arquivo ainda existe
    if (vt.hasProperty ("refEsp"))
    {
        auto r = std::make_unique<magos::Referencia>();
        leLista (vt.getProperty ("refEsp"), r->an.espectro, magos::NB);
        r->an.lufs = (float) vt.getProperty ("refLufs", -200.0f);
        r->an.picoTp = (float) vt.getProperty ("refTp", -200.0f);
        r->an.ok = true;
        std::snprintf (r->nome, sizeof (r->nome), "%s", vt.getProperty ("refNome", "").toString().toRawUTF8());
        trocaReferencia (std::move (r));
        const juce::File arq (vt.getProperty ("refArq", "").toString());
        if (arq.existsAsFile()) carregaReferencia (arq);
        else arquivoReferencia = arq;
    }
    apvts.replaceState (vt);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MagosProcessor(); }
