#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace sala;

SalaProcessor::SalaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SALA", createLayout())
{
    pPredelay   = apvts.getRawParameterValue (id::predelay);
    pPreSync    = apvts.getRawParameterValue (id::preSync);
    pSize       = apvts.getRawParameterValue (id::size);
    pDecay      = apvts.getRawParameterValue (id::decay);
    pDiffusion  = apvts.getRawParameterValue (id::diffusion);
    pEarly      = apvts.getRawParameterValue (id::early);
    pLowCut     = apvts.getRawParameterValue (id::lowCut);
    pHighCut    = apvts.getRawParameterValue (id::highCut);
    pDamp       = apvts.getRawParameterValue (id::damp);
    pDuck       = apvts.getRawParameterValue (id::duck);
    pWidth      = apvts.getRawParameterValue (id::width);
    pOutput     = apvts.getRawParameterValue (id::output);
    pMix        = apvts.getRawParameterValue (id::mix);
    pSolo       = apvts.getRawParameterValue (id::solo);
    pBypass     = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);

    factory = factoryPresets();
    presetName = factory.front().name;
    refreshUserPresets();
}

Settings SalaProcessor::readSettings() const
{
    Settings s;
    s.predelayMs = pPredelay->load();
    s.size       = pSize->load() * 0.01f;
    s.decay      = pDecay->load();
    s.diffusion  = pDiffusion->load() * 0.01f;
    s.early      = pEarly->load() * 0.01f;
    s.lowCutHz   = pLowCut->load();
    s.highCutHz  = pHighCut->load();
    s.damp       = pDamp->load() * 0.01f;
    s.duck       = pDuck->load() * 0.01f;
    s.width      = pWidth->load() * 0.01f;
    return s;
}

bool SalaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void SalaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int n = juce::jmax (1, samplesPerBlock);
    dry.setSize (2, n);
    wet.setSize (2, n);

    engine.prepare (sampleRate);
    setLatencySamples (0);   // reverb sem latência

    mixSm.reset (sampleRate, 0.05);
    gainSm.reset (sampleRate, 0.05);
    bypassSm.reset (sampleRate, 0.03);
    mixSm.setCurrentAndTargetValue (pSolo->load() > 0.5f ? 1.0f : pMix->load() * 0.01f);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
}

void SalaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    if (! licenca->liberada()) return;   // sem ativação: o som passa direto

    if (n > dry.getNumSamples())
    {
        dry.setSize (2, n, false, false, true);
        wet.setSize (2, n, false, false, true);
    }

    const int inChannels = getTotalNumInputChannels();
    const float* inL = buffer.getReadPointer (0);
    const float* inR = inChannels > 1 ? buffer.getReadPointer (1) : inL;

    dry.copyFrom (0, 0, inL, n);
    dry.copyFrom (1, 0, inR, n);
    const auto* dL = dry.getReadPointer (0);
    const auto* dR = dry.getReadPointer (1);

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
        peakIn = juce::jmax (peakIn, std::abs (dL[i]), std::abs (dR[i]));

    // ---- PRÉ NO TEMPO: usa o andamento do host (sem host, 120 BPM) ----
    double bpm = 120.0;
    bool haveTempo = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
            {
                bpm = *b;
                haveTempo = true;
            }
    bpm = juce::jlimit (20.0, 400.0, bpm);

    auto s = readSettings();
    const int sync = juce::jlimit (0, numSync - 1, juce::roundToInt (pPreSync->load()));
    if (sync > 0)
        s.predelayMs = (float) juce::jmin (maxPreSec * 1000.0, syncBeats[sync] * 60000.0 / bpm);
    shownBpm.store ((float) bpm);
    hostTempo.store (haveTempo);
    preMsNow.store (s.predelayMs);

    auto* wL = wet.getWritePointer (0);
    auto* wR = wet.getWritePointer (1);
    engine.process (dL, dR, wL, wR, n, s);

    // ---- MIX (SÓ A SALA escuta 100% molhado), GANHO, bypass sem clique ----
    mixSm.setTargetValue (pSolo->load() > 0.5f ? 1.0f : pMix->load() * 0.01f);
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float mx = mixSm.getNextValue(), gn = gainSm.getNextValue(), by = bypassSm.getNextValue();
        const float l = (dL[i] * (1.0f - mx) + wL[i] * mx) * gn;
        const float r = (dR[i] * (1.0f - mx) + wR[i] * mx) * gn;
        outL[i] = l * (1.0f - by) + dL[i] * by;
        outR[i] = r * (1.0f - by) + dR[i] * by;
        peakOut = juce::jmax (peakOut, std::abs (outL[i]), std::abs (outR[i]));
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    meterIn.store (juce::jmax (meterIn.load(), peakIn));
    meterOut.store (juce::jmax (meterOut.load(), peakOut));
}

juce::AudioProcessorEditor* SalaProcessor::createEditor()
{
    return new SalaEditor (*this);
}

// ---------------- estado ----------------

void SalaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SalaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            presetName  = tree.getProperty ("presetName", presetName).toString();
            presetIndex = getPresetNames().indexOf (presetName);
            uiScale     = juce::jlimit (0.75f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
        }
    }
}

// ---------------- presets ----------------

juce::File SalaProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/SALA");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/SALA/Presets");
   #endif
}

void SalaProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray SalaProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void SalaProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // bypass e SÓ A SALA são de performance: trocar de preset não mexe neles
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (pid == id::bypass || pid == id::solo) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void SalaProcessor::loadPreset (int index)
{
    refreshUserPresets();
    const int total = (int) factory.size() + userFiles.size();
    if (total == 0) return;
    index = ((index % total) + total) % total;

    if (index < (int) factory.size())
    {
        const auto& preset = factory[(size_t) index];
        applyValues ([&preset] (const juce::String& pid, float& v)
        {
            for (auto& kv : preset.values)
                if (pid == kv.first) { v = kv.second; return true; }
            return false;
        });
    }
    else
    {
        auto xml = juce::parseXML (userFiles[index - (int) factory.size()]);
        if (xml == nullptr) return;
        auto tree = juce::ValueTree::fromXml (*xml);
        applyValues ([&tree] (const juce::String& pid, float& v)
        {
            auto child = tree.getChildWithProperty ("id", pid);
            if (! child.isValid() || ! child.hasProperty ("value")) return false;
            v = (float) child.getProperty ("value");
            return true;
        });
    }

    presetIndex = index;
    presetName  = getPresetNames()[index];
}

juce::File SalaProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool SalaProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String SalaProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto n = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (n + ".xml").existsAsFile()) return n;
    }
    return "Meu preset";
}

void SalaProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void SalaProcessor::saveUserPreset (const juce::String& name)
{
    const auto base = juce::File::createLegalFileName (name.trim());
    if (base.isEmpty()) return;
    auto dir = userPresetDir();
    dir.createDirectory();

    auto file = dir.getChildFile (base + ".xml");
    for (int i = 2; file.existsAsFile() && i < 1000; ++i)   // nome repetido vira "Nome (2)"
        file = dir.getChildFile (base + " (" + juce::String (i) + ").xml");
    writePreset (file);

    refreshUserPresets();
    presetName  = file.getFileNameWithoutExtension();
    presetIndex = getPresetNames().indexOf (presetName);
}

void SalaProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool SalaProcessor::renameUserPreset (const juce::String& newName)
{
    const auto file = currentUserFile();
    const auto base = juce::File::createLegalFileName (newName.trim());
    if (! file.existsAsFile() || base.isEmpty()) return false;

    const auto target = file.getSiblingFile (base + ".xml");
    const bool sameFile = target.getFullPathName().equalsIgnoreCase (file.getFullPathName());   // só maiúscula/minúscula
    if ((target.existsAsFile() && ! sameFile) || ! file.moveFileTo (target)) return false;

    refreshUserPresets();
    presetName  = base;
    presetIndex = getPresetNames().indexOf (presetName);
    return true;
}

void SalaProcessor::deleteUserPreset()
{
    auto file = currentUserFile();
    if (! file.existsAsFile()) return;
    if (! file.moveToTrash()) file.deleteFile();

    // o som continua o mesmo no plugin; só deixa de estar salvo
    refreshUserPresets();
    presetName  = "Sem nome";
    presetIndex = -1;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SalaProcessor();
}
