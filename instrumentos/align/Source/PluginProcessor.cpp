#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "align", 1 }, "ALIGN", false));
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
    engine.prepare (sampleRate);
    setLatencySamples (0);
}

void AlignProcessor::alterna()
{
    if (auto* p = apvts.getParameter ("align"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (alignLigado() ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
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
