#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace repeteco;

RepetecoProcessor::RepetecoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "REPETECO", createLayout())
{
    pTime       = apvts.getRawParameterValue (id::time);
    pRange      = apvts.getRawParameterValue (id::range);
    pSync       = apvts.getRawParameterValue (id::sync);
    pDrive      = apvts.getRawParameterValue (id::drive);
    pPingPong   = apvts.getRawParameterValue (id::pingPong);
    pFeedback   = apvts.getRawParameterValue (id::feedback);
    pLfoRate    = apvts.getRawParameterValue (id::lfoRate);
    pLfoAmt     = apvts.getRawParameterValue (id::lfoAmt);
    pLfoShape   = apvts.getRawParameterValue (id::lfoShape);
    pDark       = apvts.getRawParameterValue (id::dark);
    pOutput     = apvts.getRawParameterValue (id::output);
    pMix        = apvts.getRawParameterValue (id::mix);
    pInfinite   = apvts.getRawParameterValue (id::infinite);
    pKill       = apvts.getRawParameterValue (id::kill);
    pBypass     = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);

    factory = factoryPresets();
    presetName = factory.front().name;
    refreshUserPresets();
}

Settings RepetecoProcessor::readSettings() const
{
    Settings s;
    s.time     = pTime->load() * 0.01f;
    s.longRange = juce::roundToInt (pRange->load()) == 1;
    s.free     = juce::roundToInt (pSync->load()) == 1;
    s.drive    = pDrive->load();
    s.pingPong = pPingPong->load() * 0.01f;
    s.feedback = pFeedback->load() * 0.01f;
    s.lfoDiv   = juce::roundToInt (pLfoRate->load());
    s.lfoAmt   = pLfoAmt->load() * 0.01f;
    s.lfoShape = juce::roundToInt (pLfoShape->load());
    s.dark     = pDark->load() * 0.01f;
    s.infinite = pInfinite->load() > 0.5f;
    s.kill     = pKill->load() > 0.5f;
    return s;
}

bool RepetecoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void RepetecoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int n = juce::jmax (1, samplesPerBlock);
    dry.setSize (2, n);
    wet.setSize (2, n);

    engine.prepare (sampleRate);

    mixSm.reset (sampleRate, 0.05);
    gainSm.reset (sampleRate, 0.05);
    bypassSm.reset (sampleRate, 0.03);
    killSm.reset (sampleRate, 0.01);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    killSm.setCurrentAndTargetValue (pKill->load() > 0.5f ? 1.0f : 0.0f);
}

void RepetecoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    dry.copyFrom (0, 0, inL, n);
    dry.copyFrom (1, 0, inR, n);
    const auto* dL = dry.getReadPointer (0);
    const auto* dR = dry.getReadPointer (1);

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
        peakIn = juce::jmax (peakIn, std::abs (dL[i]), std::abs (dR[i]));

    // ---- relógio do LFO: segue o host tocando; parado, espera o play no lugar do cursor ----
    double bpm = 145.0, hostPpq = 0.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { hostPpq = *p; havePpq = true; }
        }
    bpm = juce::jlimit (40.0, 300.0, bpm);
    const bool sync = playing && havePpq;
    const bool wait = havePpq && ! playing;
    if (havePpq) freePpq = hostPpq;

    auto s = readSettings();
    s.bpm = bpm;
    s.ppq = freePpq;
    s.waiting = wait;
    if (! wait) freePpq += (double) n * bpm / 60.0 / currentRate;
    shownBpm.store ((float) bpm);
    synced.store (sync);
    waiting.store (wait);

    auto* wL = wet.getWritePointer (0);
    auto* wR = wet.getWritePointer (1);
    engine.process (dL, dR, wL, wR, n, s);

    // ---- MIX, GANHO, bypass sem clique ----
    mixSm.setTargetValue (pMix->load() * 0.01f);
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    killSm.setTargetValue (s.kill ? 1.0f : 0.0f);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float mx = mixSm.getNextValue(), gn = gainSm.getNextValue(), by = bypassSm.getNextValue();
        const float dk = 1.0f - killSm.getNextValue();   // MATAR: o seco some junto, só o rabo do eco soa
        const float l = (dL[i] * (1.0f - mx) * dk + wL[i] * mx) * gn;
        const float r = (dR[i] * (1.0f - mx) * dk + wR[i] * mx) * gn;
        outL[i] = l * (1.0f - by) + dL[i] * by;
        outR[i] = r * (1.0f - by) + dR[i] * by;
        peakOut = juce::jmax (peakOut, std::abs (outL[i]), std::abs (outR[i]));
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    meterIn.store (juce::jmax (meterIn.load(), peakIn));
    meterOut.store (juce::jmax (meterOut.load(), peakOut));
}

juce::AudioProcessorEditor* RepetecoProcessor::createEditor()
{
    return new RepetecoEditor (*this);
}

// ---------------- estado ----------------

void RepetecoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RepetecoProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::File RepetecoProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/REPETECO");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/REPETECO/Presets");
   #endif
}

void RepetecoProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray RepetecoProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void RepetecoProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // bypass, INFINITO e MATAR são de performance: trocar de preset não mexe neles
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (pid == id::bypass || pid == id::infinite || pid == id::kill) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void RepetecoProcessor::loadPreset (int index)
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

juce::File RepetecoProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool RepetecoProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String RepetecoProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto n = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (n + ".xml").existsAsFile()) return n;
    }
    return "Meu preset";
}

void RepetecoProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void RepetecoProcessor::saveUserPreset (const juce::String& name)
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

void RepetecoProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool RepetecoProcessor::renameUserPreset (const juce::String& newName)
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

void RepetecoProcessor::deleteUserPreset()
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
    return new RepetecoProcessor();
}
