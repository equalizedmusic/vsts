#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray& OnkeyProcessor::nomesTom()
{
    static const juce::StringArray n { "AUTO", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

const juce::StringArray& OnkeyProcessor::nomesEscala()
{
    static const juce::StringArray n { "AUTO", "MAJOR", "MINOR", "HARM. MINOR", "MAJ. PENTA", "MIN. PENTA", "CHROMATIC" };
    return n;
}

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    using P = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "onkey", 1 }, "ONKEY", true));
    l.add (std::make_unique<P> (juce::ParameterID { "amount", 1 }, "Amount", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "speed", 1 }, "Retune Speed", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "humanize", 1 }, "Humanize", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 20.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "vibrato", 1 }, "Vibrato", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "formant", 1 }, "Formant", juce::NormalisableRange<float> (-3.0f, 3.0f, 0.1f), 0.0f, A().withLabel ("st")));
    l.add (std::make_unique<P> (juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, A().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "root", 1 }, "Root", OnkeyProcessor::nomesTom(), 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "scale", 1 }, "Scale", OnkeyProcessor::nomesEscala(), 0));
    return l;
}

OnkeyProcessor::OnkeyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ONKEY", parametros()),
      motor (std::make_unique<onkey::Engine>())
{
    pOn = apvts.getRawParameterValue ("onkey");
    pAmount = apvts.getRawParameterValue ("amount");
    pSpeed = apvts.getRawParameterValue ("speed");
    pHuman = apvts.getRawParameterValue ("humanize");
    pVib = apvts.getRawParameterValue ("vibrato");
    pForm = apvts.getRawParameterValue ("formant");
    pOut = apvts.getRawParameterValue ("output");
    pRoot = apvts.getRawParameterValue ("root");
    pScale = apvts.getRawParameterValue ("scale");
    motor->prepare (fs);
    setLatencySamples (motor->latencia());
}

bool OnkeyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void OnkeyProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    motor->prepare (sampleRate);
    setLatencySamples (motor->latencia());
}

void OnkeyProcessor::alterna()
{
    if (auto* p = apvts.getParameter ("onkey"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (ligado() ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
}

void OnkeyProcessor::define (const juce::String& id, float valorReal)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (valorReal));
        p->endChangeGesture();
    }
}

void OnkeyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n == 0 || nch == 0) return;
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    onkey::Ajustes a;
    a.ligado = pOn->load() > 0.5f;
    a.amount = pAmount->load() * 0.01f;
    a.speed = pSpeed->load() * 0.01f;
    a.humanize = pHuman->load() * 0.01f;
    a.vibrato = pVib->load() * 0.01f;
    a.formant = pForm->load();
    a.saidaDb = pOut->load();
    a.tom = juce::roundToInt (pRoot->load()) - 1;
    a.escala = juce::roundToInt (pScale->load()) - 1;
    motor->process (buffer.getArrayOfWritePointers(), nch, n, a);
}

juce::AudioProcessorEditor* OnkeyProcessor::createEditor() { return new OnkeyEditor (*this); }

void OnkeyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("fino", fino.load() ? 1 : 0, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void OnkeyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiW = (int) vt.getProperty ("uiw", 0);
            fino = (int) vt.getProperty ("fino", 0) != 0;
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OnkeyProcessor(); }
