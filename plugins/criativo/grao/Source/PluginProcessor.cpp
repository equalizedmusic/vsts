#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace grao;

GraoProcessor::GraoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRAO", createLayout())
{
    pGrain    = apvts.getRawParameterValue (id::grain);
    pSpace    = apvts.getRawParameterValue (id::space);
    pSize     = apvts.getRawParameterValue (id::size);
    pDensity  = apvts.getRawParameterValue (id::density);
    pSpray    = apvts.getRawParameterValue (id::spray);
    pPitch    = apvts.getRawParameterValue (id::pitch);
    pSpread   = apvts.getRawParameterValue (id::spread);
    pFeedback = apvts.getRawParameterValue (id::feedback);
    pDecay    = apvts.getRawParameterValue (id::decay);
    pTone     = apvts.getRawParameterValue (id::tone);
    pOutput   = apvts.getRawParameterValue (id::output);
    pMix      = apvts.getRawParameterValue (id::mix);
    pReverse  = apvts.getRawParameterValue (id::reverse);
    pFreeze   = apvts.getRawParameterValue (id::freeze);
    pBypass   = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);

    factory = factoryPresets();
    presetName = factory.front().name;
    refreshUserPresets();
}

bool GraoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void GraoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int n = juce::jmax (1, samplesPerBlock);
    dry.setSize (2, n);
    wet.setSize (2, n);
    mono.setSize (1, n);

    engine.prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) n, 2 };
    lowpass.prepare (spec);
    highpass.prepare (spec);
    lowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highpass.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    mixSm.reset (sampleRate, 0.05);
    gainSm.reset (sampleRate, 0.05);
    bypassSm.reset (sampleRate, 0.03);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
}

void GraoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    if (! licenca->liberada()) return;   // sem ativação: o som passa direto

    if (n > dry.getNumSamples())
    {
        dry.setSize (2, n, false, false, true);
        wet.setSize (2, n, false, false, true);
        mono.setSize (1, n, false, false, true);
    }

    const int inChannels = getTotalNumInputChannels();
    const float* inL = buffer.getReadPointer (0);
    const float* inR = inChannels > 1 ? buffer.getReadPointer (1) : inL;

    dry.copyFrom (0, 0, inL, n);
    dry.copyFrom (1, 0, inR, n);

    auto* m = mono.getWritePointer (0);
    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        m[i] = 0.5f * (inL[i] + inR[i]);
        peakIn = juce::jmax (peakIn, std::abs (inL[i]), std::abs (inR[i]));
    }

    // ---- macros: GRÃO pica o som, ESPAÇO abre o estéreo, a repetição e a cauda ----
    const float g  = pGrain->load() * 0.01f;
    const float sp = pSpace->load() * 0.01f;

    GrainEngine::Settings s;
    s.sizeMs   = pSize->load() * (1.5f - g);
    s.density  = pDensity->load() * std::pow (4.0f, g - 0.5f);
    s.sprayMs  = pSpray->load();
    s.pitchSt  = pPitch->load();
    s.jitterSt = 12.0f * g * g;
    s.spread   = juce::jmin (1.0f, pSpread->load() * 0.01f * (0.5f + sp));
    s.feedback = juce::jmin (0.95f, pFeedback->load() * 0.01f * (0.6f + 0.8f * sp));
    s.reverse  = pReverse->load() > 0.5f;
    s.freeze   = pFreeze->load() > 0.5f;

    auto* wL = wet.getWritePointer (0);
    auto* wR = wet.getWritePointer (1);
    engine.process (m, wL, wR, n, s);

    // ---- TOM ----
    const float tone = pTone->load();
    const float nyq  = (float) currentRate * 0.45f;
    lowpass.setCutoffFrequency (juce::jmin (toneLowpassHz (tone), nyq));
    highpass.setCutoffFrequency (juce::jmin (toneHighpassHz (tone), nyq));
    for (int i = 0; i < n; ++i)
    {
        wL[i] = highpass.processSample (0, lowpass.processSample (0, wL[i]));
        wR[i] = highpass.processSample (1, lowpass.processSample (1, wR[i]));
    }

    // ---- cauda (DECAIMENTO + ESPAÇO) ----
    juce::Reverb::Parameters rp;
    const float decay = pDecay->load();
    rp.roomSize   = juce::jmap (std::log (decay / 0.2f) / std::log (50.0f), 0.3f, 0.98f);
    rp.damping    = 0.45f;
    rp.wetLevel   = 0.25f * sp;                  // o Reverb multiplica o wet por 3
    rp.dryLevel   = 0.5f * (1.0f - 0.35f * sp);  // e o dry por 2
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.processStereo (wL, wR, n);

    // ---- MIX, GANHO, bypass sem clique ----
    mixSm.setTargetValue (pMix->load() * 0.01f);
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    const auto* dL = dry.getReadPointer (0);
    const auto* dR = dry.getReadPointer (1);
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

juce::AudioProcessorEditor* GraoProcessor::createEditor()
{
    return new GraoEditor (*this);
}

// ---------------- estado ----------------

void GraoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GraoProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::File GraoProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/GRAO");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/GRAO/Presets");
   #endif
}

void GraoProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray GraoProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void GraoProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // bypass e CONGELAR são de performance: trocar de preset não mexe neles
        if (rp == nullptr || rp->getParameterID() == id::bypass || rp->getParameterID() == id::freeze) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (rp->getParameterID(), value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void GraoProcessor::loadPreset (int index)
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

juce::File GraoProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool GraoProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String GraoProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto n = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (n + ".xml").existsAsFile()) return n;
    }
    return "Meu preset";
}

void GraoProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void GraoProcessor::saveUserPreset (const juce::String& name)
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

void GraoProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool GraoProcessor::renameUserPreset (const juce::String& newName)
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

void GraoProcessor::deleteUserPreset()
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
    return new GraoProcessor();
}
