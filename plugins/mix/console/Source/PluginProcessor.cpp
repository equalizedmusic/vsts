#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace console;

ConsoleProcessor::ConsoleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CONSOLE", createLayout())
{
    pDrive     = apvts.getRawParameterValue (id::drive);
    pModel     = apvts.getRawParameterValue (id::model);
    pTone      = apvts.getRawParameterValue (id::tone);
    pCrosstalk = apvts.getRawParameterValue (id::crosstalk);
    pNoise     = apvts.getRawParameterValue (id::noise);
    pOutput    = apvts.getRawParameterValue (id::output);
    pAutoGain  = apvts.getRawParameterValue (id::autoGain);
    pMix       = apvts.getRawParameterValue (id::mix);
    pOs4       = apvts.getRawParameterValue (id::os4);
    pCompare   = apvts.getRawParameterValue (id::compare);
    pBypass    = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);

    factory = factoryPresets();
    presetName = factory.front().name;
    refreshUserPresets();
}

Settings ConsoleProcessor::readSettings() const
{
    Settings s;
    s.drive     = pDrive->load() * 0.01f;
    s.model     = juce::roundToInt (pModel->load());
    s.tone      = pTone->load();
    s.crosstalk = pCrosstalk->load() * 0.01f;
    s.noise     = pNoise->load() * 0.01f;
    s.autoGain  = pAutoGain->load() > 0.5f;
    s.os4       = pOs4->load() > 0.5f;
    return s;
}

bool ConsoleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void ConsoleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int n = juce::jmax (1, samplesPerBlock);
    dry.setSize (2, n);
    wet.setSize (2, n);
    rate.store (sampleRate);

    engine.prepare (sampleRate, n);
    setLatencySamples (engine.latency());   // sempre a do 4× (no 2× o motor completa o atraso)

    mixSm.reset (sampleRate, 0.05);
    gainSm.reset (sampleRate, 0.05);
    bypassSm.reset (sampleRate, 0.03);
    compareSm.reset (sampleRate, 0.03);
    matchSm.reset (sampleRate, 0.2);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    compareSm.setCurrentAndTargetValue (pCompare->load() > 0.5f ? 1.0f : 0.0f);
    matchSm.setCurrentAndTargetValue (1.0f);

    msOut = msDry = 0.0f;
    msCoef = (float) (1.0 - std::exp (-1.0 / (0.6 * sampleRate)));   // ~0,6 s
    matchTarget = 1.0f;
}

void ConsoleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    // ---- MIX, GANHO, COMPARAR (seco com o volume casado), bypass sem clique ----
    mixSm.setTargetValue (pMix->load() * 0.01f);
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    compareSm.setTargetValue (pCompare->load() > 0.5f ? 1.0f : 0.0f);
    matchSm.setTargetValue (matchTarget);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float mx = mixSm.getNextValue(), gn = gainSm.getNextValue(), by = bypassSm.getNextValue();
        const float cp = compareSm.getNextValue(), mt = matchSm.getNextValue();
        const float l = (dL[i] * (1.0f - mx) + wL[i] * mx) * gn;
        const float r = (dR[i] * (1.0f - mx) + wR[i] * mx) * gn;

        // o volume do processado e do seco é medido sempre, para o A/B ser honesto na hora que apertar
        msOut += msCoef * (0.5f * (l * l + r * r) - msOut);
        msDry += msCoef * (0.5f * (dL[i] * dL[i] + dR[i] * dR[i]) - msDry);

        const float al = l * (1.0f - cp) + dL[i] * mt * cp;
        const float ar = r * (1.0f - cp) + dR[i] * mt * cp;
        outL[i] = al * (1.0f - by) + dL[i] * by;
        outR[i] = ar * (1.0f - by) + dR[i] * by;
        peakOut = juce::jmax (peakOut, std::abs (outL[i]), std::abs (outR[i]));
    }

    // em silêncio segura o último valor; limite de ±24 dB
    if (msDry > 1.0e-8f && msOut > 1.0e-10f)
        matchTarget = juce::jlimit (0.0631f, 15.85f, std::sqrt (msOut / msDry));
    matchDb.store (juce::Decibels::gainToDecibels (matchTarget));

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    meterIn.store (juce::jmax (meterIn.load(), peakIn));
    meterOut.store (juce::jmax (meterOut.load(), peakOut));
}

juce::AudioProcessorEditor* ConsoleProcessor::createEditor()
{
    return new ConsoleEditor (*this);
}

// ---------------- estado ----------------

void ConsoleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ConsoleProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::File ConsoleProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/CONSOLE");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/CONSOLE/Presets");
   #endif
}

void ConsoleProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray ConsoleProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void ConsoleProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // bypass e COMPARAR são de performance e o oversampling é qualidade: trocar de preset não mexe neles
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (pid == id::bypass || pid == id::compare || pid == id::os4) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void ConsoleProcessor::loadPreset (int index)
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

juce::File ConsoleProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool ConsoleProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String ConsoleProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto n = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (n + ".xml").existsAsFile()) return n;
    }
    return "Meu preset";
}

void ConsoleProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void ConsoleProcessor::saveUserPreset (const juce::String& name)
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

void ConsoleProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool ConsoleProcessor::renameUserPreset (const juce::String& newName)
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

void ConsoleProcessor::deleteUserPreset()
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
    return new ConsoleProcessor();
}
