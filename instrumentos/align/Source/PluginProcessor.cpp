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
    engine.guardDepth = juce::jlimit (0.0, 1.0, pDepth->load() / 100.0);
    engine.xoMax = juce::jlimit (60.0, 150.0, (double) pXoMax->load());
    engine.setReact ((int) std::lround (pReact->load()));
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

    if (! licenca->liberada()) return;   // sem ativação: o som passa direto
    aplicaDose();
    engine.process (main.getArrayOfWritePointers(), nch, kick, nk, n, alignLigado());
}

juce::AudioProcessorEditor* AlignProcessor::createEditor() { return new AlignEditor (*this); }

void AlignProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("low", verGrave.load() ? 1 : 0, nullptr);
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
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AlignProcessor(); }
