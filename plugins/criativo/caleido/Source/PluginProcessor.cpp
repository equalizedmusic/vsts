#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace caleido;

CaleidoProcessor::CaleidoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CALEIDO", createLayout())
{
    pChaos    = apvts.getRawParameterValue (id::chaos);
    pTrip     = apvts.getRawParameterValue (id::trip);
    pDivision = apvts.getRawParameterValue (id::division);
    pRatchet  = apvts.getRawParameterValue (id::ratchet);
    pShuffle  = apvts.getRawParameterValue (id::shuffle);
    pReverse  = apvts.getRawParameterValue (id::reverse);
    pTape     = apvts.getRawParameterValue (id::tape);
    pFilter   = apvts.getRawParameterValue (id::filter);
    pCrush    = apvts.getRawParameterValue (id::crush);
    pSpin     = apvts.getRawParameterValue (id::spin);
    pOutput   = apvts.getRawParameterValue (id::output);
    pMix      = apvts.getRawParameterValue (id::mix);
    pMutate   = apvts.getRawParameterValue (id::mutate);
    pHold     = apvts.getRawParameterValue (id::hold);
    pBypass   = apvts.getRawParameterValue (id::bypass);
    bypassParam = apvts.getParameter (id::bypass);

    factory = factoryPresets();
    presetName = factory.front().name;
    refreshUserPresets();

    const auto s = readSettings();
    engine.restore (makePattern (factory.front().seed, laneProbs (s)), factory.front().seed);
}

Settings CaleidoProcessor::readSettings() const
{
    Settings s;
    s.division   = juce::roundToInt (pDivision->load());
    s.ratchetMax = juce::roundToInt (pRatchet->load());
    s.spin       = juce::roundToInt (pSpin->load());
    s.chaos      = pChaos->load() * 0.01f;
    s.trip       = pTrip->load() * 0.01f;
    s.shuffle    = pShuffle->load() * 0.01f;
    s.reverse    = pReverse->load() * 0.01f;
    s.tapeRange  = pTape->load();
    s.filter     = pFilter->load() * 0.01f;
    s.crush      = pCrush->load() * 0.01f;
    s.mutate     = pMutate->load() > 0.5f;
    s.hold       = pHold->load() > 0.5f;
    return s;
}

bool CaleidoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void CaleidoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int n = juce::jmax (1, samplesPerBlock);
    dry.setSize (2, n);
    wet.setSize (2, n);

    engine.prepare (sampleRate);

    mixSm.reset (sampleRate, 0.05);
    gainSm.reset (sampleRate, 0.05);
    bypassSm.reset (sampleRate, 0.03);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
}

void CaleidoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    // ---- relógio: segue o host tocando; parado, anda sozinho no BPM do projeto ----
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
    const bool wait = havePpq && ! playing;   // host com régua, parado: a mandala espera o play
    if (sync) freePpq = hostPpq;

    auto s = readSettings();
    s.bpm = bpm;
    s.ppq = freePpq;
    s.running = ! wait;
    freePpq += (double) n * bpm / 60.0 / currentRate;
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

// GERAR: sorteia os knobs que formam o padrão e uma semente nova. A mandala sai desses knobs,
// então padrão e knobs mudam juntos
void CaleidoProcessor::generate()
{
    juce::Random rnd;
    auto set = [this] (const char* pid, float value)
    {
        if (auto* p = apvts.getParameter (pid))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    };
    auto pick = [&rnd] (float lo, float hi) { return std::round (lo + (hi - lo) * rnd.nextFloat()); };

    set (id::chaos,   pick (30.0f, 90.0f));
    set (id::trip,    pick (25.0f, 90.0f));
    set (id::ratchet, (float) rnd.nextInt (juce::Range<int> (1, 6)));   // até 3× … até 12×
    set (id::shuffle, pick (0.0f, 60.0f));
    set (id::reverse, pick (10.0f, 80.0f));
    set (id::tape,    pick (0.0f, 24.0f));
    set (id::filter,  pick (20.0f, 90.0f));
    set (id::crush,   pick (0.0f, 70.0f));
    engine.requestNewPattern();
}

juce::AudioProcessorEditor* CaleidoProcessor::createEditor()
{
    return new CaleidoEditor (*this);
}

// ---------------- estado (parâmetros + padrão da mandala) ----------------

juce::ValueTree CaleidoProcessor::stateWithPattern()
{
    auto state = apvts.copyState();
    const auto p = engine.pattern();
    for (int l = 0; l < numLanes; ++l)
        state.setProperty ("lane" + juce::String (l), p[(size_t) l], nullptr);
    state.setProperty ("seed", (int) engine.seed.load(), nullptr);
    return state;
}

void CaleidoProcessor::restorePattern (const juce::ValueTree& tree)
{
    if (! tree.hasProperty ("lane0") || ! tree.hasProperty ("seed")) return;
    Pattern p {};
    for (int l = 0; l < numLanes; ++l)
        p[(size_t) l] = (int) tree.getProperty ("lane" + juce::String (l)) & 0xffff;
    engine.restore (p, (uint32_t) (int) tree.getProperty ("seed"));
}

void CaleidoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = stateWithPattern();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CaleidoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            restorePattern (tree);
            presetName  = tree.getProperty ("presetName", presetName).toString();
            presetIndex = getPresetNames().indexOf (presetName);
            uiScale     = juce::jlimit (0.75f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
        }
    }
}

// ---------------- presets ----------------

juce::File CaleidoProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/CALEIDO");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/CALEIDO/Presets");
   #endif
}

void CaleidoProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

juce::StringArray CaleidoProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add (p.name);
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void CaleidoProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        // bypass e TRANSMUTAR são do usuário: trocar de preset não mexe neles
        if (rp == nullptr || rp->getParameterID() == id::bypass || rp->getParameterID() == id::mutate) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (rp->getParameterID(), value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void CaleidoProcessor::loadPreset (int index)
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
        engine.requestSeed (preset.seed);
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
        restorePattern (tree);
    }

    presetIndex = index;
    presetName  = getPresetNames()[index];
}

juce::File CaleidoProcessor::currentUserFile() const
{
    if (presetIndex < (int) factory.size()) return {};
    for (auto& f : userFiles)
        if (f.getFileNameWithoutExtension() == presetName && f.existsAsFile()) return f;
    return {};
}

bool CaleidoProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String CaleidoProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto n = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (n + ".xml").existsAsFile()) return n;
    }
    return "Meu preset";
}

void CaleidoProcessor::writePreset (const juce::File& file)
{
    if (auto xml = stateWithPattern().createXml())
        xml->writeTo (file);
}

void CaleidoProcessor::saveUserPreset (const juce::String& name)
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

void CaleidoProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool CaleidoProcessor::renameUserPreset (const juce::String& newName)
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

void CaleidoProcessor::deleteUserPreset()
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
    return new CaleidoProcessor();
}
