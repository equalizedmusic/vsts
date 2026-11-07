#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace stomp;

namespace
{
juce::StringArray nomesTipos()
{
    juce::StringArray n { "EMPTY" };
    for (int t = 1; t < numTipos; ++t) n.add (infoPedal (t).nome);
    return n;
}
juce::StringArray nomesAmps()
{
    juce::StringArray n;
    for (int a = 0; a < numAmps; ++a) n.add (infoAmp (a).nome);
    return n;
}
} // namespace

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    using P = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    using I = juce::AudioParameterInt;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const juce::NormalisableRange<float> dez (0.0f, 10.0f, 0.01f);
    for (int s = 0; s < numSlots; ++s)
    {
        const juce::String n = "Slot " + juce::String (s + 1) + " ";
        auto grupo = std::make_unique<juce::AudioProcessorParameterGroup> ("slot" + juce::String (s + 1), "Slot " + juce::String (s + 1), "|");
        grupo->addChild (std::make_unique<C> (juce::ParameterID { StompboxProcessor::id (s, "type"), 1 }, n + "Pedal", nomesTipos(), 0));
        grupo->addChild (std::make_unique<B> (juce::ParameterID { StompboxProcessor::id (s, "on"), 1 }, n + "On", false));
        for (int k = 0; k < 4; ++k)
            grupo->addChild (std::make_unique<P> (juce::ParameterID { StompboxProcessor::id (s, ("k" + std::to_string (k + 1)).c_str()), 1 },
                                                  n + "Knob " + juce::String (k + 1), dez, 5.0f));
        grupo->addChild (std::make_unique<I> (juce::ParameterID { StompboxProcessor::id (s, "sw"), 1 }, n + "Switch", 0, 2, 0));
        l.add (std::move (grupo));
    }
    l.add (std::make_unique<P> (juce::ParameterID { "input", 1 }, "Input", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, A().withLabel ("dB")));
    l.add (std::make_unique<P> (juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, A().withLabel ("dB")));
    l.add (std::make_unique<P> (juce::ParameterID { "gate", 1 }, "Gate Threshold", juce::NormalisableRange<float> (-90.0f, -20.0f, 0.5f), -90.0f,
                                A().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return v <= -89.5f ? juce::String ("OFF") : juce::String (v, 1) + " dB"; })));
    l.add (std::make_unique<P> (juce::ParameterID { "gaterel", 1 }, "Gate Release", juce::NormalisableRange<float> (10.0f, 500.0f, 1.0f, 0.5f), 120.0f, A().withLabel ("ms")));
    l.add (std::make_unique<B> (juce::ParameterID { "ampon", 1 }, "Amp + Cab On", true));
    l.add (std::make_unique<C> (juce::ParameterID { "amp", 1 }, "Amp Model", nomesAmps(), 0));
    l.add (std::make_unique<C> (juce::ParameterID { "canal", 1 }, "Amp Channel", juce::StringArray { "CLEAN", "DIRTY" }, 0));
    const char* ak[6] = { "gain", "bass", "mid", "treble", "presence", "master" };
    const char* an[6] = { "Amp Gain", "Amp Bass", "Amp Mid", "Amp Treble", "Amp Presence", "Amp Master" };
    for (int k = 0; k < 6; ++k) l.add (std::make_unique<P> (juce::ParameterID { ak[k], 1 }, an[k], dez, 5.0f));
    l.add (std::make_unique<C> (juce::ParameterID { "cab", 1 }, "Cab", juce::StringArray { "1x12 OPEN", "2x12", "4x12" }, 2));
    l.add (std::make_unique<C> (juce::ParameterID { "mic", 1 }, "Mic", juce::StringArray { "DYN", "RIBBON" }, 0));
    l.add (std::make_unique<P> (juce::ParameterID { "micpos", 1 }, "Mic Position", dez, 3.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "dist", 1 }, "Mic Distance", dez, 2.0f));
    l.add (std::make_unique<B> (juce::ParameterID { "sync", 1 }, "Sync BPM", false));
    l.add (std::make_unique<B> (juce::ParameterID { "hq", 1 }, "HQ (oversampling 4x)", false));
    return l;
}

StompboxProcessor::StompboxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STOMPBOX", parametros())
{
    for (int s = 0; s < numSlots; ++s)
    {
        pTipo[s] = apvts.getRawParameterValue (id (s, "type"));
        pOn[s] = apvts.getRawParameterValue (id (s, "on"));
        for (int k = 0; k < 4; ++k) pK[s][k] = apvts.getRawParameterValue (id (s, ("k" + std::to_string (k + 1)).c_str()));
        pSw[s] = apvts.getRawParameterValue (id (s, "sw"));
    }
    pIn = apvts.getRawParameterValue ("input"); pOut = apvts.getRawParameterValue ("output");
    pGate = apvts.getRawParameterValue ("gate"); pGateRel = apvts.getRawParameterValue ("gaterel");
    pAmpOn = apvts.getRawParameterValue ("ampon"); pAmp = apvts.getRawParameterValue ("amp"); pCanal = apvts.getRawParameterValue ("canal");
    const char* ak[6] = { "gain", "bass", "mid", "treble", "presence", "master" };
    for (int k = 0; k < 6; ++k) pAmpK[k] = apvts.getRawParameterValue (ak[k]);
    pCab = apvts.getRawParameterValue ("cab"); pMic = apvts.getRawParameterValue ("mic");
    pMicPos = apvts.getRawParameterValue ("micpos"); pDist = apvts.getRawParameterValue ("dist");
    pSync = apvts.getRawParameterValue ("sync"); pHq = apvts.getRawParameterValue ("hq");
    board.prepare (fs, false);
    aplicaPreset (0);
}

const juce::String StompboxProcessor::getProgramName (int i)
{
    const auto& p = presetsFabrica();
    return juce::isPositiveAndBelow (i, (int) p.size()) ? juce::String (p[(size_t) i].nome) : juce::String();
}

bool StompboxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void StompboxProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    hqPreparado = pHq->load() > 0.5f;
    board.prepare (fs, hqPreparado);
}

void StompboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nIn = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const int nOut = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels());
    if (n == 0 || nOut == 0) return;
    if (nIn == 0) buffer.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    const bool hq = pHq->load() > 0.5f;
    if (hq != hqPreparado) { hqPreparado = hq; board.prepare (fs, hq); }   // mesmo tamanho: não aloca

    for (int s = 0; s < numSlots; ++s)
    {
        auto& sc = ctl.slot[s];
        sc.tipo = juce::jlimit (0, numTipos - 1, juce::roundToInt (pTipo[s]->load()));
        sc.c.on = pOn[s]->load() > 0.5f;
        for (int k = 0; k < 4; ++k) sc.c.k[k] = pK[s][k]->load() * 0.1f;
        sc.c.sw = juce::jlimit (0, juce::jmax (0, infoPedal (sc.tipo).nsw - 1), juce::roundToInt (pSw[s]->load()));
        sc.c.bpm = bpm.load(); sc.c.sync = pSync->load() > 0.5f;
    }
    ctl.inDb = pIn->load(); ctl.outDb = pOut->load(); ctl.gateThr = pGate->load(); ctl.gateRel = pGateRel->load();
    ctl.ampOn = pAmpOn->load() > 0.5f; ctl.amp = juce::roundToInt (pAmp->load()); ctl.canal = juce::roundToInt (pCanal->load());
    for (int k = 0; k < 6; ++k) ctl.ampK[k] = pAmpK[k]->load() * 0.1f;
    ctl.cab = juce::roundToInt (pCab->load()); ctl.mic = juce::roundToInt (pMic->load());
    ctl.micPos = pMicPos->load() * 0.1f; ctl.dist = pDist->load() * 0.1f;

    float* L = buffer.getWritePointer (0);
    const bool estereoIn = nIn > 1;
    // afinador: a entrada (mono) vai para o anel que a tela lê
    {
        int w = posAfin.load();
        const float* R0 = estereoIn ? buffer.getReadPointer (1) : nullptr;
        for (int i = 0; i < n; ++i) { bufAfin[w] = R0 ? 0.5f * (L[i] + R0[i]) : L[i]; w = (w + 1) % tamAfin; }
        posAfin = w;
    }

    if (nOut > 1)
        board.process (L, buffer.getWritePointer (1), n, estereoIn, ctl);
    else
    {
        float tmp[256];
        for (int o = 0; o < n; o += 256)
        {
            const int m = juce::jmin (256, n - o);
            board.process (L + o, tmp, m, false, ctl);
            for (int i = 0; i < m; ++i) L[o + i] = 0.5f * (L[o + i] + tmp[i]);
        }
    }
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    picoIn = juce::jmax (picoIn.load(), board.picoIn); board.picoIn = 0;
    picoOut = juce::jmax (picoOut.load(), board.picoOut); board.picoOut = 0;
    gateAberto = board.gateAberto;
    if (afinador.load()) buffer.clear();
}

// --------------------------------------------------------------------------------------------------------------
// o board

void StompboxProcessor::define (const juce::String& pid, float valorReal)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (valorReal));
        p->endChangeGesture();
    }
}

float StompboxProcessor::real (const juce::String& pid) const
{
    if (auto* v = apvts.getRawParameterValue (pid)) return v->load();
    return 0.0f;
}

StompboxProcessor::SlotDados StompboxProcessor::le (int s) const
{
    SlotDados d;
    d.tipo = juce::jlimit (0, numTipos - 1, juce::roundToInt (pTipo[s]->load()));
    d.on = pOn[s]->load() > 0.5f;
    for (int k = 0; k < 4; ++k) d.k[k] = pK[s][k]->load();
    d.sw = juce::roundToInt (pSw[s]->load());
    return d;
}

void StompboxProcessor::grava (int s, const SlotDados& d)
{
    define (id (s, "type"), (float) d.tipo);
    define (id (s, "on"), d.on ? 1.0f : 0.0f);
    for (int k = 0; k < 4; ++k) define (id (s, ("k" + std::to_string (k + 1)).c_str()), d.k[k]);
    define (id (s, "sw"), (float) d.sw);
}

int StompboxProcessor::numPedais() const
{
    int n = 0;
    for (int s = 0; s < numSlots; ++s) if (juce::roundToInt (pTipo[s]->load()) > 0) n = s + 1;
    return n;
}

void StompboxProcessor::poePedal (int s, int tipo)
{
    s = juce::jlimit (0, numSlots - 1, juce::jmin (s, numPedais()));
    SlotDados d; d.tipo = tipo; d.on = true; d.sw = 0;
    const auto& inf = infoPedal (tipo);
    for (int k = 0; k < 4; ++k) d.k[k] = k < inf.nk ? inf.def[k] : 5.0f;
    grava (s, d);
}

void StompboxProcessor::removePedal (int s)
{
    const int n = numPedais();
    for (int i = s; i < n - 1; ++i) grava (i, le (i + 1));
    if (n > 0) grava (n - 1, SlotDados());
}

void StompboxProcessor::movePedal (int de, int para)
{
    const int n = numPedais();
    if (! juce::isPositiveAndBelow (de, n)) return;
    para = juce::jlimit (0, n - 1, para);
    if (de == para) return;
    std::vector<SlotDados> v;
    for (int i = 0; i < n; ++i) v.push_back (le (i));
    const auto d = v[(size_t) de];
    v.erase (v.begin() + de);
    v.insert (v.begin() + para, d);
    for (int i = 0; i < n; ++i) grava (i, v[(size_t) i]);
}

void StompboxProcessor::alternaPedal (int s)
{
    define (id (s, "on"), pOn[s]->load() > 0.5f ? 0.0f : 1.0f);
}

// --------------------------------------------------------------------------------------------------------------
// presets

void StompboxProcessor::aplicaPreset (int i)
{
    const auto& ps = presetsFabrica();
    if (! juce::isPositiveAndBelow (i, (int) ps.size())) return;
    const auto& p = ps[(size_t) i];
    for (int s = 0; s < numSlots; ++s)
    {
        SlotDados d;
        if (s < (int) p.pedais.size())
        {
            const auto& pp = p.pedais[(size_t) s];
            d.tipo = pp.tipo; d.on = pp.on; d.sw = pp.sw;
            const auto& inf = infoPedal (pp.tipo);
            for (int k = 0; k < 4; ++k) d.k[k] = k < inf.nk ? pp.k[k] : 5.0f;
        }
        grava (s, d);
    }
    define ("amp", (float) p.amp); define ("canal", (float) p.canal);
    const char* ak[6] = { "gain", "bass", "mid", "treble", "presence", "master" };
    for (int k = 0; k < 6; ++k) define (ak[k], p.ampK[k]);
    define ("ampon", p.ampOn ? 1.0f : 0.0f);
    define ("cab", (float) p.cab); define ("mic", (float) p.mic);
    define ("micpos", p.micPos); define ("dist", p.dist);
    define ("gate", p.gateThr); define ("input", p.inDb); define ("output", p.outDb);
    presetAtual = i;
    nomePreset = p.nome;
    rolagem = 0;
}

juce::File StompboxProcessor::pastaUsuario() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("InsideStudioSP").getChildFile ("STOMPBOX").getChildFile ("Presets");
}

juce::Array<juce::File> StompboxProcessor::presetsUsuario() const
{
    auto a = pastaUsuario().findChildFiles (juce::File::findFiles, false, "*.stompbox");
    std::sort (a.begin(), a.end(), [] (const juce::File& x, const juce::File& y) { return x.getFileName().compareIgnoreCase (y.getFileName()) < 0; });
    return a;
}

bool StompboxProcessor::salvaPresetUsuario (const juce::String& nome)
{
    const auto limpo = juce::File::createLegalFileName (nome.trim().toUpperCase());
    if (limpo.isEmpty()) return false;
    auto dir = pastaUsuario();
    if (! dir.createDirectory()) return false;
    auto st = apvts.copyState();
    st.setProperty ("nome", limpo, nullptr);
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    if (! xml || ! xml->writeTo (dir.getChildFile (limpo + ".stompbox"))) return false;
    nomePreset = limpo; presetAtual = -1;
    return true;
}

bool StompboxProcessor::abrePresetUsuario (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return false;
    auto vt = juce::ValueTree::fromXml (*xml);
    // aplica parâmetro por parâmetro (o host fica sabendo de cada um)
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto filho = vt.getChildWithProperty ("id", rp->getParameterID());
            if (filho.isValid()) define (rp->getParameterID(), (float) filho.getProperty ("value"));
        }
    nomePreset = vt.getProperty ("nome", f.getFileNameWithoutExtension()).toString();
    presetAtual = -1; rolagem = 0;
    return true;
}

// --------------------------------------------------------------------------------------------------------------
// A/B

void StompboxProcessor::alternaAB()
{
    auto atual = apvts.copyState();
    atual.setProperty ("nome", nomePreset, nullptr);
    if (! outroAB.isValid()) outroAB = atual.createCopy();
    auto proximo = outroAB;
    outroAB = atual;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto filho = proximo.getChildWithProperty ("id", rp->getParameterID());
            if (filho.isValid()) define (rp->getParameterID(), (float) filho.getProperty ("value"));
        }
    nomePreset = proximo.getProperty ("nome", nomePreset).toString();
    ab = 1 - ab;
}

void StompboxProcessor::copiaParaOutro()
{
    outroAB = apvts.copyState();
    outroAB.setProperty ("nome", nomePreset, nullptr);
}

// --------------------------------------------------------------------------------------------------------------

juce::AudioProcessorEditor* StompboxProcessor::createEditor() { return new StompboxEditor (*this); }

void StompboxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("nome", nomePreset, nullptr);
    state.setProperty ("preset", presetAtual, nullptr);
    state.setProperty ("ab", ab, nullptr);
    state.removeChild (state.getChildWithName ("OUTRO"), nullptr);
    if (outroAB.isValid())
    {
        juce::ValueTree o ("OUTRO");
        o.addChild (outroAB.createCopy(), -1, nullptr);
        state.addChild (o, -1, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void StompboxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiW = (int) vt.getProperty ("uiw", 0);
            nomePreset = vt.getProperty ("nome", "INIT").toString();
            presetAtual = (int) vt.getProperty ("preset", 0);
            ab = (int) vt.getProperty ("ab", 0);
            const auto o = vt.getChildWithName ("OUTRO");
            outroAB = o.isValid() && o.getNumChildren() > 0 ? o.getChild (0).createCopy() : juce::ValueTree();
            vt.removeChild (o, nullptr);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StompboxProcessor(); }
