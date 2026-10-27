#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace medidor;

MonitorProcessor::MonitorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MONITOR", createLayout())
{
    pTarget = apvts.getRawParameterValue (id::target);
    pMono   = apvts.getRawParameterValue (id::mono);
    pMatch  = apvts.getRawParameterValue (id::match);
    pBypass = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);

    factory = factoryPresets();
    presetName = factory.front().name;
    refreshUserPresets();
}

bool MonitorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void MonitorProcessor::prepareToPlay (double sampleRate, int)
{
    rate.store (sampleRate);
    engine.prepare (sampleRate);
    setLatencySamples (0);   // o true peak é só análise: o áudio passa direto

    monoSm.reset (sampleRate, 0.05);
    gainSm.reset (sampleRate, 0.3);
    monoSm.setCurrentAndTargetValue (0.0f);
    gainSm.setCurrentAndTargetValue (1.0f);
    listenDb.store (0.0f);
}

void MonitorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    // entrada mono: o mesmo sinal sai nos dois lados
    const bool stereo = getTotalNumInputChannels() > 1;
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    if (! stereo)
        for (int i = 0; i < n; ++i) outR[i] = outL[i];
    if (! licenca->liberada()) return;   // sem ativação: o som passa direto

    // ---- medição: sempre da entrada, antes de qualquer checagem ----
    engine.process (outL, outR, n, stereo);

    // ---- checagens (só mudam o que se ouve) ----
    const bool off = pBypass->load() > 0.5f;
    monoSm.setTargetValue (! off && pMono->load() > 0.5f ? 1.0f : 0.0f);

    // OUVIR NO ALVO: baixa até o alvo (como as plataformas fazem); nunca sobe o volume
    float gDb = 0.0f;
    if (! off && pMatch->load() > 0.5f)
    {
        const float integ = engine.integrated.load();
        if (integ > -70.0f)
            gDb = juce::jlimit (-30.0f, 0.0f, pTarget->load() - integ);
    }
    listenDb.store (gDb);
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (gDb));

    // tudo desligado e parado: nem encosta no áudio (saída = entrada, bit a bit)
    const bool active = monoSm.isSmoothing() || gainSm.isSmoothing()
                     || monoSm.getTargetValue() > 0.0f || gainSm.getTargetValue() < 1.0f;
    if (active)
    {
        for (int i = 0; i < n; ++i)
        {
            const float mx = monoSm.getNextValue(), gn = gainSm.getNextValue();
            const float l = outL[i], r = outR[i];
            const float m = 0.5f * (l + r);
            outL[i] = (l + (m - l) * mx) * gn;
            outR[i] = (r + (m - r) * mx) * gn;
        }
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

juce::AudioProcessorEditor* MonitorProcessor::createEditor()
{
    return new MonitorEditor (*this);
}

// ---------------- estado ----------------

void MonitorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MonitorProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::File MonitorProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/MONITOR");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/MONITOR/Presets");
   #endif
}

void MonitorProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray MonitorProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void MonitorProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // bypass e as checagens (MONO, NO ALVO) são de performance: trocar de preset não mexe neles
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (pid == id::bypass || pid == id::mono || pid == id::match) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void MonitorProcessor::loadPreset (int index)
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

juce::File MonitorProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool MonitorProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String MonitorProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto n = "Meu alvo " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (n + ".xml").existsAsFile()) return n;
    }
    return "Meu alvo";
}

void MonitorProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void MonitorProcessor::saveUserPreset (const juce::String& name)
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

void MonitorProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool MonitorProcessor::renameUserPreset (const juce::String& newName)
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

void MonitorProcessor::deleteUserPreset()
{
    auto file = currentUserFile();
    if (! file.existsAsFile()) return;
    if (! file.moveToTrash()) file.deleteFile();

    // o alvo continua o mesmo no plugin; só deixa de estar salvo
    refreshUserPresets();
    presetName  = "Sem nome";
    presetIndex = -1;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonitorProcessor();
}
