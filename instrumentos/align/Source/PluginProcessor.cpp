// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "align", 1 }, "ALIGN", false));
    // dosagem (0.2.0): o corretor segue automático; os padrões = 0.1.0 (projeto antigo abre igual)
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "phase", 2 }, "PHASE", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "polarity", 2 }, "POLARITY", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "guard", 2 }, "LEVEL GUARD", true));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "depth", 2 }, "GUARD DEPTH",
               juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
               juce::AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "react", 2 }, "REACT",
               juce::StringArray { "SLOW", "MID", "FAST" }, 1));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "xomax", 2 }, "X-OVER MAX",
               juce::NormalisableRange<float> (60.0f, 150.0f, 1.0f), 110.0f,
               juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    // 0.3.0: DUCK (sidechain com dosagem). Padrão 0 = desligado (projeto antigo abre igual)
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "duck", 3 }, "DUCK",
               juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
               juce::AudioParameterFloatAttributes().withLabel ("%")));
    return l;
}

AlignProcessor::AlignProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ALIGN", parametros())
{
    pAlign = apvts.getRawParameterValue ("align");
    pPhase = apvts.getRawParameterValue ("phase");
    pPolarity = apvts.getRawParameterValue ("polarity");
    pGuard = apvts.getRawParameterValue ("guard");
    pDepth = apvts.getRawParameterValue ("depth");
    pReact = apvts.getRawParameterValue ("react");
    pXoMax = apvts.getRawParameterValue ("xomax");
    pDuck = apvts.getRawParameterValue ("duck");
}

bool AlignProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void AlignProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    aplicaDose();
    engine.prepare (sampleRate);
    kickLink.assign (2048, 0.0f);
    semScDesde = 1 << 30;
    setLatencySamples (0);
}

void AlignProcessor::alterna() { alternaParam ("align"); }

void AlignProcessor::alternaParam (const juce::String& id)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
}

void AlignProcessor::aplicaDose()
{
    engine.usaFase = pPhase->load() > 0.5f;
    engine.usaPolaridade = pPolarity->load() > 0.5f;
    engine.guard = pGuard->load() > 0.5f;
    engine.guardDepth = juce::jlimit (0.0, 1.0000000000000000308276, pDepth->load() / 100.0);
    engine.xoMax = juce::jlimit (60.0, 150.0, (double) pXoMax->load());
    engine.setReact ((int) std::lround (pReact->load()));
    engine.duck = juce::jlimit (0.0, 1.0000000000000000239615, pDuck->load() / 100.0);
}

void AlignProcessor::preferirChute (const juce::String& nome)
{
    const juce::SpinLock::ScopedLockType t (prefTrava);
    prefNome = nome.substring (0, chutelink::NNOME - 1);
    prefNovo = true;
}

juce::String AlignProcessor::chutePreferido() const
{
    const juce::SpinLock::ScopedLockType t (prefTrava);
    return prefNome;
}

void AlignProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = (float) *b;

    auto main = getBusBuffer (buffer, true, 0);
    const int nch = juce::jmin (2, main.getNumChannels());
    if (nch == 0) return;

    const float* kick[2] = { nullptr, nullptr };
    int nk = 0;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        nk = juce::jmin (2, sc.getNumChannels());
        for (int c = 0; c < nk; ++c) kick[c] = sc.getReadPointer (c);
    }
    scLigado = nk > 0;

    // sidechain com som nos últimos 2 s = vale o sidechain (o Ableton deixa o bus ligado mesmo sem faixa escolhida)
    bool scSom = false;
    for (int c = 0; c < nk && ! scSom; ++c)
        for (int i = 0; i < n; ++i) if (std::abs (kick[c][i]) > 1.0e-4f) { scSom = true; break; }
    semScDesde = scSom ? 0 : semScDesde + n;
    const bool usaSc = nk > 0 && semScDesde < (int64_t) (2.0 * fs);

    // LINK: o CHUTE do projeto (só quando o sidechain não tem som)
    if (prefNovo.load())
    {
        const juce::SpinLock::ScopedTryLockType t (prefTrava);
        if (t.isLocked()) { link.prefere (prefNome.toRawUTF8()); prefNovo = false; }
    }
    const bool temLink = ! usaSc && link.sincroniza();
    linkVaga = temLink ? link.vaga() : -1;
    fonte = usaSc ? fonteSidechain : (temLink ? fonteLink : semKick);

    // 0.3.1: bloco sem passar pelo render do LINK (sidechain com som, sem licença) = as batidas guardadas ficaram velhas
    const bool linkUsado = temLink && licenca->liberada();
    if (linkUsado && ! linkNoBlocoAnterior) link.esquece();
    linkNoBlocoAnterior = linkUsado;
    if (! licenca->liberada()) return;   // sem ativação: o som passa direto
    aplicaDose();
    if (! temLink)
    {
        engine.process (main.getArrayOfWritePointers(), nch, kick, nk, n, alignLigado());
        return;
    }
    // kick recriado a partir das batidas do CHUTE, em trechos de até 2048 amostras
    bool tocando = false; int64_t posBloco = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            tocando = pos->getIsPlaying();
            if (auto ts = pos->getTimeInSamples()) posBloco = *ts; else tocando = false;
        }
    const double wallBloco = (double) chutelink::agoraUs() * 0.001;
    float* ch[2] = { main.getWritePointer (0), nch > 1 ? main.getWritePointer (1) : nullptr };
    for (int ini = 0; ini < n; ini += (int) kickLink.size())
    {
        const int m = juce::jmin ((int) kickLink.size(), n - ini);
        std::fill (kickLink.begin(), kickLink.begin() + m, 0.0f);
        link.render (kickLink.data(), m, fs, tocando, posBloco + ini, wallBloco + ini * 1000.0 / fs);
        float* parte[2] = { ch[0] + ini, ch[1] != nullptr ? ch[1] + ini : nullptr };
        const float* k1[1] = { kickLink.data() };
        engine.process (parte, nch, k1, 1, m, alignLigado());
    }
}

juce::AudioProcessorEditor* AlignProcessor::createEditor() { return new AlignEditor (*this); }

void AlignProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("low", verGrave.load() ? 1 : 0, nullptr);
    state.setProperty ("chute", chutePreferido(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AlignProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiW = (int) vt.getProperty ("uiw", 0);
            verGrave = (int) vt.getProperty ("low", 1) != 0;
            preferirChute (vt.getProperty ("chute", "").toString());
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AlignProcessor(); }
