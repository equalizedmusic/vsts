#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray& BarebassProcessor::nomesOnda()
{
    static const juce::StringArray n { "SIN", "TRI", "SAW", "SQR" };
    return n;
}

const juce::StringArray& BarebassProcessor::nomesOndaSub()
{
    static const juce::StringArray n { "SIN", "SQR" };
    return n;
}

namespace
{
juce::NormalisableRange<float> faixa (float lo, float hi, float passo, float centro = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, passo);
    if (centro > 0.0f) r.setSkewForCentre (centro);
    return r;
}

// presets de fábrica. O padrão de cada parâmetro é o INIT (o plugin abre nele); os outros partem da base ROLLING
// (baseRolling = true) e mudam só o que está listado.
struct Preset { const char* nome; bool baseRolling; std::vector<std::pair<const char*, float>> v; };
const std::vector<std::pair<const char*, float>>& rolling()
{
    static const std::vector<std::pair<const char*, float>> r {
        { "o1wave", 2 }, { "o1fine", 0 }, { "o1level", 80 }, { "o2wave", 3 }, { "o2oct", -1 }, { "o2fine", -7 }, { "o2level", 45 },
        { "sublevel", 70 }, { "cutoff", 180 }, { "reso", 35 }, { "envamt", 72 }, { "decay", 92 }, { "sustain", 0 }, { "hunf", 50 } };
    return r;
}
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p {
        { "INIT", false, {} },
        { "ROLLING", true, {} },
        { "PLUCK", true, { { "o1wave", 2 }, { "o2level", 0 }, { "sublevel", 80 }, { "cutoff", 120 }, { "reso", 20 }, { "envamt", 85 },
                           { "decay", 60 }, { "hunf", 70 } } },
        { "DEEP SUB", true, { { "o1wave", 1 }, { "o1level", 40 }, { "o2level", 0 }, { "sublevel", 90 }, { "cutoff", 400 }, { "reso", 10 },
                              { "envamt", 20 }, { "decay", 300 }, { "hunf", 15 } } },
        { "ACID 12", true, { { "o1wave", 2 }, { "o1fine", 0 }, { "o2level", 0 }, { "sublevel", 30 }, { "slope", 0 }, { "cutoff", 300 },
                             { "reso", 75 }, { "envamt", 65 }, { "decay", 180 }, { "mode", 1 }, { "glide", 60 }, { "hunf", 35 } } },
        { "SQUARE GROWL", true, { { "o1wave", 3 }, { "o1fine", 12 }, { "o2wave", 3 }, { "o2oct", 0 }, { "o2fine", -12 }, { "o2level", 70 },
                                  { "cutoff", 250 }, { "reso", 50 }, { "envamt", 60 }, { "decay", 140 }, { "hunf", 45 } } },
    };
    return p;
}
} // namespace

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    using P = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    using C = juce::AudioParameterChoice;
    using I = juce::AudioParameterInt;
    using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const auto& on = BarebassProcessor::nomesOnda();
    // padrões = INIT: FINE e OCTAVE em 0 (o SUB uma oitava abaixo, senão não é sub), LEVEL em 80, filtro aberto
    l.add (std::make_unique<B> (juce::ParameterID { "o1on", 1 }, "Osc 1 On", true));
    l.add (std::make_unique<C> (juce::ParameterID { "o1wave", 1 }, "Osc 1 Wave", on, 2));
    l.add (std::make_unique<I> (juce::ParameterID { "o1oct", 1 }, "Osc 1 Octave", -2, 2, 0));
    l.add (std::make_unique<P> (juce::ParameterID { "o1fine", 1 }, "Osc 1 Fine", faixa (-50, 50, 1), 0.0f, A().withLabel ("c")));
    l.add (std::make_unique<P> (juce::ParameterID { "o1level", 1 }, "Osc 1 Level", faixa (0, 100, 1), 80.0f, A().withLabel ("%")));
    l.add (std::make_unique<B> (juce::ParameterID { "o2on", 1 }, "Osc 2 On", true));
    l.add (std::make_unique<C> (juce::ParameterID { "o2wave", 1 }, "Osc 2 Wave", on, 3));
    l.add (std::make_unique<I> (juce::ParameterID { "o2oct", 1 }, "Osc 2 Octave", -2, 2, 0));
    l.add (std::make_unique<P> (juce::ParameterID { "o2fine", 1 }, "Osc 2 Fine", faixa (-50, 50, 1), 0.0f, A().withLabel ("c")));
    l.add (std::make_unique<P> (juce::ParameterID { "o2level", 1 }, "Osc 2 Level", faixa (0, 100, 1), 80.0f, A().withLabel ("%")));
    l.add (std::make_unique<B> (juce::ParameterID { "subon", 1 }, "Sub On", true));
    l.add (std::make_unique<C> (juce::ParameterID { "subwave", 1 }, "Sub Wave", BarebassProcessor::nomesOndaSub(), 0));
    l.add (std::make_unique<I> (juce::ParameterID { "suboct", 1 }, "Sub Octave", -2, 0, -1));
    l.add (std::make_unique<P> (juce::ParameterID { "sublevel", 1 }, "Sub Level", faixa (0, 100, 1), 80.0f, A().withLabel ("%")));
    l.add (std::make_unique<C> (juce::ParameterID { "slope", 1 }, "Filter Slope", juce::StringArray { "12 dB", "24 dB" }, 1));
    l.add (std::make_unique<P> (juce::ParameterID { "cutoff", 1 }, "Cutoff", faixa (20, 18000, 1, 600), 18000.0f, A().withLabel ("Hz")));
    l.add (std::make_unique<P> (juce::ParameterID { "reso", 1 }, "Resonance", faixa (0, 100, 1), 0.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "envamt", 1 }, "Env Amount", faixa (-100, 100, 1), 0.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "attack", 1 }, "Attack", faixa (0, 2000, 1, 100), 0.0f, A().withLabel ("ms")));
    l.add (std::make_unique<P> (juce::ParameterID { "decay", 1 }, "Decay", faixa (1, 3000, 1, 250), 92.0f, A().withLabel ("ms")));
    l.add (std::make_unique<P> (juce::ParameterID { "sustain", 1 }, "Sustain", faixa (0, 100, 1), 100.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "release", 1 }, "Release", faixa (1, 3000, 1, 250), 40.0f, A().withLabel ("ms")));
    l.add (std::make_unique<P> (juce::ParameterID { "hunf", 1 }, "unHunf", faixa (0, 100, 1), 0.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "glide", 1 }, "Glide", faixa (0, 500, 1, 80), 0.0f, A().withLabel ("ms")));
    l.add (std::make_unique<C> (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "MONO", "LEGATO" }, 0));
    l.add (std::make_unique<P> (juce::ParameterID { "volume", 1 }, "Volume", faixa (-36, 6, 0.1f), -3.0f, A().withLabel ("dB")));
    // 0.3.0: quanto a velocity da nota pesa no volume e na abertura do ENV (0 = toda nota igual, como antes)
    l.add (std::make_unique<P> (juce::ParameterID { "velo", 1 }, "Velocity", faixa (0, 100, 1), 60.0f, A().withLabel ("%")));
    return l;
}

BarebassProcessor::BarebassProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BAREBASS", parametros()),
      motor (std::make_unique<barebass::Engine>())
{
    motor->prepare (fs);
}

bool BarebassProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BarebassProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    motor->prepare (sampleRate);
}

void BarebassProcessor::define (const juce::String& id, float valorReal)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (valorReal));
        p->endChangeGesture();
    }
}

int BarebassProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String BarebassProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String (presets()[(size_t) i].nome) : juce::String();
}

void BarebassProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, (int) presets().size())) return;
    preset = i;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            float alvo = rp->getDefaultValue();
            if (presets()[(size_t) i].baseRolling)
                for (auto& [id, v] : rolling())
                    if (rp->getParameterID() == id) alvo = rp->convertTo0to1 (v);
            for (auto& [id, v] : presets()[(size_t) i].v)
                if (rp->getParameterID() == id) alvo = rp->convertTo0to1 (v);
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (alvo);
            rp->endChangeGesture();
        }
}

barebass::Ajustes BarebassProcessor::ajustes() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto iv = [&] (const char* id) { return juce::roundToInt (v (id)); };
    barebass::Ajustes a;
    a.liga[0] = v ("o1on") > 0.5f; a.liga[1] = v ("o2on") > 0.5f; a.ligaSub = v ("subon") > 0.5f;
    a.onda[0] = iv ("o1wave"); a.oitava[0] = iv ("o1oct"); a.fino[0] = v ("o1fine"); a.nivel[0] = v ("o1level") * 0.01f;
    a.onda[1] = iv ("o2wave"); a.oitava[1] = iv ("o2oct"); a.fino[1] = v ("o2fine"); a.nivel[1] = v ("o2level") * 0.01f;
    a.ondaSub = iv ("subwave"); a.oitavaSub = iv ("suboct"); a.nivelSub = v ("sublevel") * 0.01f;
    a.slope24 = iv ("slope") == 1;
    a.cutoff = v ("cutoff"); a.reso = v ("reso") * 0.01f; a.envAmt = v ("envamt") * 0.01f;
    a.atk = v ("attack") * 0.001f; a.dec = v ("decay") * 0.001f; a.sus = v ("sustain") * 0.01f; a.rel = v ("release") * 0.001f;
    a.hunf = v ("hunf") * 0.01f;
    a.glide = v ("glide") * 0.001f;
    a.legato = iv ("mode") == 1;
    a.volumeDb = v ("volume");
    a.velo = v ("velo") * 0.01f;
    return a;
}

void BarebassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, buffer.getNumChannels());
    buffer.clear();
    if (n == 0 || nch == 0) return;
    if (! licenca->liberada()) return;   // sem ativação: mudo (o buffer já foi zerado)

    const auto a = ajustes();
    float* const* out = buffer.getArrayOfWritePointers();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, n, meta.samplePosition);
        if (t > pos) { motor->render (out, nch, pos, t - pos, a); pos = t; }
        const auto m = meta.getMessage();
        if (m.isNoteOn()) motor->noteOn (m.getNoteNumber(), a, m.getFloatVelocity());
        else if (m.isNoteOff()) motor->noteOff (m.getNoteNumber(), a);
        else if (m.isPitchWheel()) motor->pitchBend (m.getPitchWheelValue());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) motor->allNotesOff();
    }
    if (n > pos) motor->render (out, nch, pos, n - pos, a);
}

juce::AudioProcessorEditor* BarebassProcessor::createEditor() { return new BarebassEditor (*this); }

void BarebassProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("preset", preset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void BarebassProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiW = (int) vt.getProperty ("uiw", 0);
            preset = (int) vt.getProperty ("preset", 0);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BarebassProcessor(); }
