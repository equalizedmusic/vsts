#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace quimera;

QuimeraProcessor::QuimeraProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "QUIMERA", createLayout()),
      reader (apvts)
{
    factory = factoryPresets();
    presetName = factory.empty() ? juce::String ("Init") : factory.front().name;
    refreshUserPresets();
}

bool QuimeraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void QuimeraProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int n = juce::jmax (1, samplesPerBlock);
    engine.prepare (sampleRate, n);
    fx.prepare (sampleRate, n);
    keyboard.reset();

    reader.read (snap);
    volSm.reset (sampleRate, 0.05);
    volSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (snap.volume));
}

void QuimeraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;

    // teclas clicadas na tela entram no MIDI; as do host acendem no teclado da tela
    keyboard.processNextMidiBuffer (midi, 0, n, true);

    // ---- relógio: segue o host; sem régua, anda sozinho no último BPM ----
    Clock clock;
    double bpm = 120.0, hostPpq = 0.0;
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
    if (sync) freePpq = hostPpq;
    clock.bpm = bpm;
    clock.ppq = freePpq;
    clock.playing = sync;
    freePpq += (double) n * bpm / 60.0 / currentRate;
    shownBpm.store ((float) bpm);
    synced.store (sync);

    reader.read (snap);
    engine.process (buffer, midi, snap, clock);

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    fx.process (l, r, n, snap, clock);

    // ---- volume mestre, medidor (pico) e osciloscópio ----
    volSm.setTargetValue (juce::Decibels::decibelsToGain (snap.volume));
    float peak = 0.0f;
    int w = scopeWrite.load();
    for (int i = 0; i < n; ++i)
    {
        const float g = volSm.getNextValue();
        l[i] *= g;
        r[i] *= g;
        peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
        scope[w] = 0.5f * (l[i] + r[i]);
        if (++w >= scopeSize) w = 0;
    }
    scopeWrite.store (w);
    meterOut.store (juce::jmax (meterOut.load(), peak));
}

juce::AudioProcessorEditor* QuimeraProcessor::createEditor()
{
    return new QuimeraEditor (*this);
}

// ---------------- estado ----------------

void QuimeraProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void QuimeraProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::File QuimeraProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/QUIMERA");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/QUIMERA/Presets");
   #endif
}

void QuimeraProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray QuimeraProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

int QuimeraProcessor::getPresetCategory (int index) const
{
    if (index >= 0 && index < (int) factory.size()) return factory[(size_t) index].category;
    return numCategories;
}

void QuimeraProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // MPE, MPE Bend e SEGURAR são do controlador/performance: trocar de preset não mexe neles
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (pid == id::mpe || pid == id::mpeBend || pid == id::hold) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void QuimeraProcessor::loadPreset (int index)
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

juce::File QuimeraProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool QuimeraProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String QuimeraProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto nm = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (nm + ".xml").existsAsFile()) return nm;
    }
    return "Meu preset";
}

void QuimeraProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void QuimeraProcessor::saveUserPreset (const juce::String& name)
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

void QuimeraProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool QuimeraProcessor::renameUserPreset (const juce::String& newName)
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

void QuimeraProcessor::deleteUserPreset()
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
    return new QuimeraProcessor();
}
