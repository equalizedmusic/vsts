#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace torno;

TornoProcessor::TornoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TORNO", createLayout())
{
    pSqueeze    = apvts.getRawParameterValue (id::squeeze);
    pThreshold  = apvts.getRawParameterValue (id::threshold);
    pRatio      = apvts.getRawParameterValue (id::ratio);
    pKnee       = apvts.getRawParameterValue (id::knee);
    pAttack     = apvts.getRawParameterValue (id::attack);
    pRelease    = apvts.getRawParameterValue (id::release);
    pAutoRel    = apvts.getRawParameterValue (id::autoRel);
    pScHpf      = apvts.getRawParameterValue (id::scHpf);
    pMakeup     = apvts.getRawParameterValue (id::makeup);
    pAutoMakeup = apvts.getRawParameterValue (id::autoMakeup);
    pOutput     = apvts.getRawParameterValue (id::output);
    pMix        = apvts.getRawParameterValue (id::mix);
    pListen     = apvts.getRawParameterValue (id::listen);
    pBypass     = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);

    factory = factoryPresets();
    presetName = factory.front().name;
    refreshUserPresets();
}

Settings TornoProcessor::readSettings() const
{
    Settings s;
    s.squeeze     = pSqueeze->load() * 0.01f;
    s.threshold   = pThreshold->load();
    s.ratio       = pRatio->load();
    s.knee        = pKnee->load();
    s.attackMs    = pAttack->load();
    s.releaseMs   = pRelease->load();
    s.hpfHz       = pScHpf->load();
    s.makeupDb    = pMakeup->load();
    s.autoRelease = pAutoRel->load() > 0.5f;
    s.autoMakeup  = pAutoMakeup->load() > 0.5f;
    s.listen      = pListen->load() > 0.5f;
    return s;
}

bool TornoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void TornoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int n = juce::jmax (1, samplesPerBlock);
    wet.setSize (2, n);
    rate.store (sampleRate);

    engine.prepare (sampleRate);
    setLatencySamples (0);   // sem lookahead: nada a compensar

    mixSm.reset (sampleRate, 0.05);
    gainSm.reset (sampleRate, 0.05);
    bypassSm.reset (sampleRate, 0.03);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
}

void TornoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (n > wet.getNumSamples())
        wet.setSize (2, n, false, false, true);

    const int inChannels = getTotalNumInputChannels();
    const float* inL = buffer.getReadPointer (0);
    const float* inR = inChannels > 1 ? buffer.getReadPointer (1) : inL;

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
        peakIn = juce::jmax (peakIn, std::abs (inL[i]), std::abs (inR[i]));

    const auto s = readSettings();
    auto* wL = wet.getWritePointer (0);
    auto* wR = wet.getWritePointer (1);
    engine.process (inL, inR, wL, wR, n, s);

    // ---- MIX (OUVIR SIDECHAIN escuta 100% do detector), GANHO, bypass sem clique ----
    mixSm.setTargetValue (s.listen ? 1.0f : pMix->load() * 0.01f);
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // lê os dois lados antes de escrever: com entrada mono, inR aponta para o mesmo canal de outL
        const float dl = inL[i], dr = inR[i];
        const float mx = mixSm.getNextValue(), gn = gainSm.getNextValue(), by = bypassSm.getNextValue();
        const float l = (dl * (1.0f - mx) + wL[i] * mx) * gn;
        const float r = (dr * (1.0f - mx) + wR[i] * mx) * gn;
        outL[i] = l * (1.0f - by) + dl * by;
        outR[i] = r * (1.0f - by) + dr * by;
        peakOut = juce::jmax (peakOut, std::abs (outL[i]), std::abs (outR[i]));
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    meterIn.store (juce::jmax (meterIn.load(), peakIn));
    meterOut.store (juce::jmax (meterOut.load(), peakOut));
}

juce::AudioProcessorEditor* TornoProcessor::createEditor()
{
    return new TornoEditor (*this);
}

// ---------------- estado ----------------

void TornoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TornoProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::File TornoProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/TORNO");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/TORNO/Presets");
   #endif
}

void TornoProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray TornoProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void TornoProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // bypass e OUVIR SIDECHAIN são de performance: trocar de preset não mexe neles
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (pid == id::bypass || pid == id::listen) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void TornoProcessor::loadPreset (int index)
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

juce::File TornoProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool TornoProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String TornoProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto n = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (n + ".xml").existsAsFile()) return n;
    }
    return "Meu preset";
}

void TornoProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void TornoProcessor::saveUserPreset (const juce::String& name)
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

void TornoProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool TornoProcessor::renameUserPreset (const juce::String& newName)
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

void TornoProcessor::deleteUserPreset()
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
    return new TornoProcessor();
}
