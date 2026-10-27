#include "KnobProcessor.h"
#include "KnobEditor.h"

using namespace sk;

juce::AudioProcessorValueTreeState::ParameterLayout KnobProcessor::createLayout()
{
    const auto& in = info();
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id::valor, 1 }, "Knob",
                                                        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), in.valorPadrao));
    juce::StringArray rates, formas;
    for (auto* r : in.rateNomes) rates.add (juce::String::fromUTF8 (r));
    for (auto* f : in.formas) formas.add (juce::String::fromUTF8 (f));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::rate, 1 }, "Rate", rates, in.ratePadrao));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::forma, 1 }, "Shape", formas, in.formaPadrao));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id::mix, 1 }, "Mix",
                                                        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), in.mixPadrao));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::gatilho, 1 }, "Trigger",
                                                         juce::StringArray { "INT", "MIDI", juce::String::fromUTF8 ("ÁUDIO") }, 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::bypass, 1 }, "Bypass", false));
    return l;
}

KnobProcessor::KnobProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "SERIEKNOB", createLayout()),
      motor (criarMotor())
{
    pValor   = apvts.getRawParameterValue (id::valor);
    pRate    = apvts.getRawParameterValue (id::rate);
    pForma   = apvts.getRawParameterValue (id::forma);
    pMix     = apvts.getRawParameterValue (id::mix);
    pGatilho = apvts.getRawParameterValue (id::gatilho);
    pBypass  = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);
}

bool KnobProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void KnobProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int n = juce::jmax (1, samplesPerBlock);
    seco.setSize (2, n);
    efeito.setSize (2, n);
    pos.assign ((size_t) n, 0.0);
    disparo.assign ((size_t) n, 0);
    motor->preparar (sampleRate);
    mixSm.reset (sampleRate, 0.05);
    bypassSm.reset (sampleRate, 0.03);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    envRapido = envLento = 0.0f;
    armado = true;
}

void KnobProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    if (! licenca->liberada()) return;   // sem ativação: o som passa direto
    if (n > seco.getNumSamples())
    {
        seco.setSize (2, n, false, false, true);
        efeito.setSize (2, n, false, false, true);
    }
    if ((int) pos.size() < n) { pos.resize ((size_t) n); disparo.resize ((size_t) n); }

    auto mainIn = getBusBuffer (buffer, true, 0);
    const float* inL = mainIn.getReadPointer (0);
    const float* inR = mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : inL;
    seco.copyFrom (0, 0, inL, n);
    seco.copyFrom (1, 0, inR, n);

    // ---- relógio ----
    double bpm = 120.0, ppq = 0.0, compasso = 4.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto p = ph->getPosition())
        {
            if (auto b = p->getBpm()) bpm = *b;
            playing = p->getIsPlaying();
            if (auto q = p->getPpqPosition()) { ppq = *q; havePpq = true; }
            if (auto ts = p->getTimeSignature())
                if (ts->numerator > 0 && ts->denominator > 0)
                    compasso = ts->numerator * 4.0 / ts->denominator;
        }
    bpm = juce::jlimit (30.0, 300.0, bpm);
    compasso = juce::jlimit (1.0, 16.0, compasso);
    const double bps = bpm / 60.0 / sr;   // tempos por amostra

    const int gat = juce::jlimit (0, 2, (int) std::lround (pGatilho->load()));
    if (gat != gatilhoAnterior)
    {
        posDisparo = 1.0e6;   // trocou o TRIGGER: espera o próximo disparo
        gatilhoAnterior = gat;
    }
    std::fill (disparo.begin(), disparo.begin() + n, (uint8_t) 0);

    if (gat == gatMidi)
    {
        for (const auto m : midi)
            if (m.getMessage().isNoteOn())
                disparo[(size_t) juce::jlimit (0, n - 1, m.samplePosition)] = 1;
    }
    else if (gat == gatAudio)
    {
        // detector: sidechain se o DAW mandou, senão a própria entrada
        const bool temSc = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled()
                           && getBusBuffer (buffer, true, 1).getNumChannels() > 0;
        scConectado.store (temSc);
        const float* dL = inL;
        const float* dR = inR;
        if (temSc)
        {
            auto sc = getBusBuffer (buffer, true, 1);
            dL = sc.getReadPointer (0);
            dR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : dL;
        }
        const float aR = 1.0f - std::exp (-1.0f / (0.0005f * (float) sr)), rR = 1.0f - std::exp (-1.0f / (0.06f * (float) sr));
        const float rL = 1.0f - std::exp (-1.0f / (0.4f * (float) sr));
        const int intervaloMin = (int) (0.06 * sr);
        float pico = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = juce::jmax (std::abs (dL[i]), std::abs (dR[i]));
            pico = juce::jmax (pico, x);
            envRapido += (x > envRapido ? aR : rR) * (x - envRapido);
            envLento += rL * (envRapido - envLento);
            const float limiar = juce::jmax (0.006f, envLento * 1.7f);   // -45 dB ou 1,7x a média
            ++desdeDisparo;
            if (armado && envRapido > limiar && desdeDisparo > intervaloMin)
            {
                disparo[(size_t) i] = 1;
                armado = false;
                desdeDisparo = 0;
            }
            else if (! armado && envRapido < limiar * 0.6f)
                armado = true;
        }
        scNivel.store (juce::jmax (scNivel.load() * 0.9f, pico));
    }

    int nDisparos = 0;
    for (int i = 0; i < n; ++i)
    {
        if (gat == gatInt)
        {
            pos[(size_t) i] = (playing && havePpq) ? ppq + i * bps : posLivre + i * bps;
        }
        else
        {
            if (disparo[(size_t) i]) { posDisparo = 0.0; ++nDisparos; }
            pos[(size_t) i] = posDisparo;
            posDisparo += bps;
        }
    }
    if (! (playing && havePpq)) posLivre += n * bps;
    else posLivre = ppq + n * bps;
    if (nDisparos) disparos.fetch_add (nDisparos);

    const auto& in = info();
    Bloco b;
    b.sr = sr; b.bpm = bpm; b.compasso = compasso;
    b.rate = in.rates[(size_t) juce::jlimit (0, 6, (int) std::lround (pRate->load()))];
    b.valor = juce::jlimit (0.0f, 1.0f, pValor->load() * 0.1f);
    b.forma = juce::jlimit (0, (int) in.formas.size() - 1, (int) std::lround (pForma->load()));
    b.gatilho = gat;
    b.pos = pos.data();
    b.disparo = disparo.data();

    efeito.copyFrom (0, 0, seco, 0, 0, n);
    efeito.copyFrom (1, 0, seco, 1, 0, n);
    auto* wL = efeito.getWritePointer (0);
    auto* wR = efeito.getWritePointer (1);
    motor->processar (wL, wR, n, b);

    posAtual.store (pos[(size_t) n - 1]);
    bpmAtual.store ((float) bpm);
    compassoAtual.store ((float) compasso);
    tocando.store (playing);
    nivel.store (motor->nivel);

    // ---- MIX e bypass sem clique ----
    mixSm.setTargetValue (pMix->load() * 0.01f);
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    const float* sL = seco.getReadPointer (0);
    const float* sR = seco.getReadPointer (1);
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float mx = mixSm.getNextValue(), by = bypassSm.getNextValue();
        float l, r;
        if (in.mixSoma) { l = sL[i] + wL[i] * mx; r = sR[i] + wR[i] * mx; }
        else            { l = sL[i] * (1.0f - mx) + wL[i] * mx; r = sR[i] * (1.0f - mx) + wR[i] * mx; }
        outL[i] = l * (1.0f - by) + sL[i] * by;
        outR[i] = r * (1.0f - by) + sR[i] * by;
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

juce::AudioProcessorEditor* KnobProcessor::createEditor()
{
    return new KnobEditor (*this);
}

void KnobProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KnobProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            uiScale = juce::jlimit (0.6f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KnobProcessor();
}
