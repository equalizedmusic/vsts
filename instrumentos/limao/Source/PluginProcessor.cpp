#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace limao;

LimaoProcessor::LimaoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LIMAO", createLayout())
{
    auto raw = [this] (const juce::String& pid) { return apvts.getRawParameterValue (pid); };
    pWave = raw (id::wave);      pTune = raw (id::tune);     pCut = raw (id::cutoff);   pReso = raw (id::reso);
    pEnv = raw (id::envmod);     pDecay = raw (id::decay);   pAcc = raw (id::accent);   pSlide = raw (id::slide);
    pAcid = raw (id::acid);      pDrvType = raw (id::drvType); pDrive = raw (id::drive);
    pDlOn = raw (id::dlOn);      pDlDiv = raw (id::dlDiv);   pDlFb = raw (id::dlFb);    pDlMix = raw (id::dlMix);
    pRvOn = raw (id::rvOn);      pRvSize = raw (id::rvSize); pRvMix = raw (id::rvMix);  pVol = raw (id::volume);
    pSeqOn = raw (id::seqOn);    pSeqLen = raw (id::seqLen); pRate = raw (id::seqRate); pSwing = raw (id::swing);

    // abre no "303 Clássico" (o Init é só uma linha de Dó)
    const auto& list = factoryPresets();
    presetIndex = list.size() > 1 ? 1 : 0;
    presetName  = u8 (list[(size_t) presetIndex].name);
    parsePattern (list[(size_t) presetIndex], pattern);
    for (const auto& v : list[(size_t) presetIndex].values)
        if (auto* prm = apvts.getParameter (v.first))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v.second));
}

bool LimaoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void LimaoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int n = juce::jmax (1, samplesPerBlock);
    mono.assign ((size_t) n, 0.0f);
    voice.prepare (sampleRate);
    fx.prepare (sampleRate, n);
    readSettings();
    volSm.reset (sampleRate, 0.05);
    volSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pVol->load()));
    lastAbsStep = -1;
    gateOffPpq = -1.0;
    wasRunning = false;
    numHeld = 0;
    transpose = 0;
    playStep = -1;
}

void LimaoProcessor::readSettings()
{
    s.wave    = juce::roundToInt (pWave->load());
    s.tune    = pTune->load();
    s.cutoff  = pCut->load();
    s.reso    = pReso->load() * 0.01f;
    s.envmod  = pEnv->load() * 0.01f;
    s.decayMs = pDecay->load();
    s.accent  = pAcc->load() * 0.01f;
    s.slideMs = pSlide->load();
    s.acid    = pAcid->load() * 0.01f;
    s.drvType = juce::roundToInt (pDrvType->load());
    s.drive   = pDrive->load() * 0.01f;
    s.dlOn    = pDlOn->load() > 0.5f;
    s.dlDiv   = juce::roundToInt (pDlDiv->load());
    s.dlFb    = pDlFb->load() * 0.01f;
    s.dlMix   = pDlMix->load() * 0.01f;
    s.rvOn    = pRvOn->load() > 0.5f;
    s.rvSize  = pRvSize->load() * 0.01f;
    s.rvMix   = pRvMix->load() * 0.01f;
}

void LimaoProcessor::startNote (int midiNote, bool accent, bool legato)
{
    voice.noteOn (juce::jlimit (12, 108, midiNote), accent, legato);
}

// teclado: com o sequenciador rodando, a nota segurada transpõe o padrão (Dó 3 = sem transpor);
// sem ele, toca mono com prioridade para a última nota, legato = slide e velocity alta = accent
void LimaoProcessor::handleMidi (const juce::MidiMessage& m, bool seqRunning)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        int w = 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        numHeld = w;
        if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note;

        if (seqRunning)
            transpose = note - 60;
        else
            startNote (note, m.getVelocity() >= 100, numHeld > 1 && voice.isGateOn());
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        int w = 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        numHeld = w;

        if (seqRunning)
            transpose = numHeld > 0 ? held[(size_t) numHeld - 1] - 60 : 0;
        else if (numHeld > 0)
            startNote (held[(size_t) numHeld - 1], false, true);
        else
            voice.noteOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        numHeld = 0;
        transpose = 0;
        voice.noteOff();
    }
}

void LimaoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0 || buffer.getNumChannels() < 2) return;
    if ((int) mono.size() < n) mono.resize ((size_t) n);

    // ---- relógio do host ----
    double bpm = 120.0, ppq0 = 0.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { ppq0 = *p; havePpq = true; }
        }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    shownBpm.store ((float) bpm);
    synced.store (playing && havePpq);

    readSettings();
    s.bpm = bpm;

    const bool running = pSeqOn->load() > 0.5f && playing && havePpq;
    if (wasRunning && ! running)
    {
        voice.noteOff();
        lastAbsStep = -1;
        gateOffPpq = -1.0;
        lastPlayed = -1;
        playStep = -1;
    }
    wasRunning = running;

    const int    len   = juce::jlimit (1, maxSteps, juce::roundToInt (pSeqLen->load()));
    const double sb    = rateBeats[juce::jlimit (0, 4, juce::roundToInt (pRate->load()))];
    const double sw    = (double) pSwing->load() * 0.01 * 0.5 * sb;     // atraso dos passos pares
    const double pps   = bpm / 60.0 / currentRate;

    int rendered = 0;
    auto flush = [&] (int upTo)
    {
        if (upTo > rendered)
        {
            voice.render (mono.data() + rendered, upTo - rendered, s);
            rendered = upTo;
        }
    };

    auto it = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.cend() && (*it).samplePosition <= i)
        {
            flush (i);
            handleMidi ((*it).getMessage(), running);
            ++it;
        }

        if (! running) continue;

        // ---- sequenciador: qual passo é agora (com swing nos pares) ----
        const double p    = ppq0 + pps * (double) i;
        const double pair = std::floor (p / (2.0 * sb));
        const double t    = p - pair * 2.0 * sb;
        const bool   second = t >= sb + sw;
        const long long absStep = (long long) pair * 2 + (second ? 1 : 0);

        if (absStep != lastAbsStep)
        {
            flush (i);
            const int  idx = (int) (((absStep % len) + len) % len);
            const bool contiguous = absStep == lastAbsStep + 1;
            const Step st   = pattern.get (idx);
            const Step prev = lastPlayed >= 0 ? pattern.get (lastPlayed) : Step { 0, Mode::rest, false, false };
            const Step next = pattern.get ((idx + 1) % len);

            if (st.mode == Mode::rest)
                voice.noteOff();
            else if (st.mode == Mode::note)
            {
                const bool legato = contiguous && prev.slide && prev.mode != Mode::rest && voice.isGateOn();
                startNote (36 + st.pitch + transpose, st.accent, legato);
            }
            // ligadura: segue a nota anterior (se não havia nota, fica em silêncio)

            const double start = pair * 2.0 * sb + (second ? sb + sw : 0.0);
            const double dur   = second ? sb - sw : sb + sw;
            const bool   hold  = st.mode != Mode::rest && (st.slide || next.mode == Mode::tie);
            gateOffPpq = hold ? -1.0 : start + dur * 0.5;

            lastAbsStep = absStep;
            lastPlayed  = idx;
            playStep    = idx;
        }

        if (gateOffPpq >= 0.0 && p >= gateOffPpq)
        {
            flush (i);
            voice.noteOff();
            gateOffPpq = -1.0;
        }
    }
    flush (n);
    for (; it != midi.cend(); ++it)
        handleMidi ((*it).getMessage(), running);

    // osciloscópio: a voz filtrada
    int w = scopeWrite.load();
    for (int i = 0; i < n; ++i)
    {
        scope[w] = mono[(size_t) i];
        if (++w >= scopeSize) w = 0;
    }
    scopeWrite.store (w);

    // efeitos e volume
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    fx.process (mono.data(), l, r, n, s);

    volSm.setTargetValue (juce::Decibels::decibelsToGain (pVol->load()));
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = volSm.getNextValue();
        l[i] = juce::jlimit (-2.0f, 2.0f, l[i] * g);
        r[i] = juce::jlimit (-2.0f, 2.0f, r[i] * g);
        peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
    }
    meterOut.store (juce::jmax (meterOut.load(), peak));
}

juce::AudioProcessorEditor* LimaoProcessor::createEditor()
{
    return new LimaoEditor (*this);
}

// ---------------- estado ----------------

void LimaoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", pattern.toString(), nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetIndex", presetIndex, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LimaoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            if (tree.hasProperty ("pattern"))
                pattern.fromString (tree.getProperty ("pattern").toString());
            presetName  = tree.getProperty ("presetName", presetName).toString();
            presetIndex = juce::jlimit (0, (int) factoryPresets().size() - 1, (int) tree.getProperty ("presetIndex", presetIndex));
            uiScale     = juce::jlimit (0.75f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
            ++patternVersion;
        }
}

// ---------------- presets e padrão ----------------

juce::StringArray LimaoProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : factoryPresets())
        names.add (u8 (p.name));
    return names;
}

void LimaoProcessor::loadPreset (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    const auto& p = list[(size_t) index];

    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto pid = rp->getParameterID();
            if (pid == id::seqOn) continue;      // performance: o preset não liga nem desliga o sequenciador
            float value = rp->convertFrom0to1 (rp->getDefaultValue());
            for (const auto& v : p.values)
                if (pid == v.first) value = v.second;
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        }

    const int steps = parsePattern (p, pattern);
    if (auto* lenP = apvts.getParameter (id::seqLen))
        lenP->setValueNotifyingHost (lenP->convertTo0to1 ((float) steps));

    presetIndex = index;
    presetName  = u8 (p.name);
    ++patternVersion;
}

void LimaoProcessor::randomizePattern()
{
    juce::Random rnd;
    randomize (pattern, juce::jlimit (1, maxSteps, juce::roundToInt (pSeqLen->load())), rnd);
    ++patternVersion;
}

void LimaoProcessor::clearPattern()
{
    pattern.clear();
    ++patternVersion;
}

void LimaoProcessor::shiftPattern (int dir)
{
    const int len = juce::jlimit (1, maxSteps, juce::roundToInt (pSeqLen->load()));
    std::vector<Step> tmp;
    for (int i = 0; i < len; ++i) tmp.push_back (pattern.get (i));
    for (int i = 0; i < len; ++i)
        pattern.set (((i + dir) % len + len) % len, tmp[(size_t) i]);
    ++patternVersion;
}

void LimaoProcessor::transposePattern (int semis)
{
    for (int i = 0; i < maxSteps; ++i)
    {
        auto st = pattern.get (i);
        st.pitch = juce::jlimit (-12, 24, st.pitch + semis);
        pattern.set (i, st);
    }
    ++patternVersion;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LimaoProcessor();
}
