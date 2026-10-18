#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace navalha;

NavalhaProcessor::NavalhaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NAVALHA", createLayout())
{
    pDrive   = apvts.getRawParameterValue (id::drive);
    pCeiling = apvts.getRawParameterValue (id::ceiling);
    pKnee    = apvts.getRawParameterValue (id::knee);
    pAsym    = apvts.getRawParameterValue (id::asym);
    pLink    = apvts.getRawParameterValue (id::link);
    pOutput  = apvts.getRawParameterValue (id::output);
    pMix     = apvts.getRawParameterValue (id::mix);
    pDelta   = apvts.getRawParameterValue (id::delta);
    pBypass  = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);

    factory = factoryPresets();
    presetName = factory.front().name;
    refreshUserPresets();
}

Settings NavalhaProcessor::readSettings() const
{
    Settings s;
    s.drive   = pDrive->load();
    s.ceiling = pCeiling->load();
    s.knee    = pKnee->load() * 0.01f;
    s.asym    = pAsym->load() * 0.01f;
    s.link    = pLink->load() * 0.01f;
    s.delta   = pDelta->load() > 0.5f;
    return s;
}

bool NavalhaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void NavalhaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int n = juce::jmax (1, samplesPerBlock);
    dry.setSize (2, n);
    wet.setSize (2, n);
    rate.store (sampleRate);

    engine.prepare (sampleRate, n);
    setLatencySamples (engine.latency());

    mixSm.reset (sampleRate, 0.05);
    gainSm.reset (sampleRate, 0.05);
    bypassSm.reset (sampleRate, 0.03);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
}

void NavalhaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (n > dry.getNumSamples())
    {
        dry.setSize (2, n, false, false, true);
        wet.setSize (2, n, false, false, true);
    }

    const int inChannels = getTotalNumInputChannels();
    const float* inL = buffer.getReadPointer (0);
    const float* inR = inChannels > 1 ? buffer.getReadPointer (1) : inL;

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
        peakIn = juce::jmax (peakIn, std::abs (inL[i]), std::abs (inR[i]));

    const auto s = readSettings();
    auto* wL = wet.getWritePointer (0);
    auto* wR = wet.getWritePointer (1);
    auto* dL = dry.getWritePointer (0);
    auto* dR = dry.getWritePointer (1);
    engine.process (inL, inR, wL, wR, dL, dR, n, s);

    // ---- MIX (SÓ O CORTE escuta 100% do corte), GANHO, bypass sem clique ----
    mixSm.setTargetValue (s.delta ? 1.0f : pMix->load() * 0.01f);
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

juce::AudioProcessorEditor* NavalhaProcessor::createEditor()
{
    return new NavalhaEditor (*this);
}

// ---------------- estado ----------------

void NavalhaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NavalhaProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::File NavalhaProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/NAVALHA");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/NAVALHA/Presets");
   #endif
}

void NavalhaProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray NavalhaProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void NavalhaProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // bypass e SÓ O CORTE são de performance: trocar de preset não mexe neles
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (pid == id::bypass || pid == id::delta) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void NavalhaProcessor::loadPreset (int index)
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

juce::File NavalhaProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool NavalhaProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String NavalhaProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto n = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (n + ".xml").existsAsFile()) return n;
    }
    return "Meu preset";
}

void NavalhaProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void NavalhaProcessor::saveUserPreset (const juce::String& name)
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

void NavalhaProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool NavalhaProcessor::renameUserPreset (const juce::String& newName)
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

void NavalhaProcessor::deleteUserPreset()
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
    return new NavalhaProcessor();
}
