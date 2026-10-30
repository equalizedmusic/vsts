#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mesa;

MesaProcessor::MesaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MESA", createLayout())
{
    auto raw = [this] (const juce::String& pid) { apvts.addParameterListener (pid, this); return apvts.getRawParameterValue (pid); };
    pMode = raw (id::mode);  pGrp = raw (id::grp);   pPre = raw (id::pre);   pGain = raw (id::gain);
    pDrive = raw (id::drive); pPad = raw (id::pad);  pHpf = raw (id::hpf);   pEqOn = raw (id::eqOn);
    pEqm = raw (id::eqm);    pThr = raw (id::thr);   pRat = raw (id::rat);   pGate = raw (id::gate);
    pCmix = raw (id::cmix);  pPan = raw (id::pan);   pMute = raw (id::mute); pSolo = raw (id::solo);
    pFader = raw (id::fader); pMthr = raw (id::mthr); pMatk = raw (id::matk); pMrel = raw (id::mrel);
    pMmkp = raw (id::mmkp);  pMrat = raw (id::mrat); pMmix = raw (id::mmix); pMin = raw (id::min);
    pMhpf = raw (id::mhpf);  pHq = raw (id::hq);   pDynPre = raw (id::dynPre);
    for (int b = 0; b < 4; ++b) { pEqG[b] = raw (id::eqG[b]); pEqF[b] = raw (id::eqF[b]); }
    seqNo = registry->add (this);
}

MesaProcessor::~MesaProcessor()
{
    if (soloWas) --registry->soloCount;
    registry->remove (this);
}

bool MesaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void MesaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    os->initProcessing ((size_t) maxBlock);
    osLatency = (int) std::round (os->getLatencyInSamples());
    ecoDelay.prepare ({ fs, (juce::uint32) maxBlock, 2 });
    ecoDelay.setMaximumDelayInSamples (juce::jmax (64, osLatency + 1));
    ecoDelay.setDelay ((float) osLatency);
    setLatencySamples (osLatency);

    hpf.reset(); colour.reset(); eq.reset();
    comp.prepare (fs); gate.prepare (fs); bus.prepare (fs);
    for (auto* s : { &gainSm, &driveSm, &gainL, &gainR }) s->reset (fs, 0.02);
    lastPre = -1; lastEq = -1;
    dirty = true;
    updateSettings();
    gainSm.setCurrentAndTargetValue (gainSm.getTargetValue());
    gainL.setCurrentAndTargetValue (gainL.getTargetValue());
    gainR.setCurrentAndTargetValue (gainR.getTargetValue());
}

void MesaProcessor::parameterChanged (const juce::String& pid, float)
{
    dirty = true;
    if (pid == id::mode || pid == id::grp) registry->changed();
}

void MesaProcessor::updateSettings()
{
    dirty = false;
    const bool hq = pHq->load() > 0.5f;
    const int preModel = juce::roundToInt (pPre->load());
    const int key = preModel * 2 + (hq ? 1 : 0);
    if (key != lastPre)
    {
        sat.model = (PreModel) preModel;
        sat.prepare (hq ? fs * 4.0 : fs);
        colour.set (fs, sat.model);
        lastPre = key;
    }
    gainSm.setTargetValue (dbToGain (pGain->load()) * (pPad->load() > 0.5f ? 0.1f : 1.0f));
    sat.drive = dbToGain (pDrive->load() * 2.4f);          // DRIVE 10 = +24 dB para dentro da curva
    hpf.set (fs, pHpf->load());

    EqSettings s;
    for (int b = 0; b < 4; ++b) { s.g[b] = pEqG[b]->load(); s.f[b] = pEqF[b]->load(); }
    eq.set (fs, (EqModel) juce::roundToInt (pEqm->load()), s);

    comp.thr = pThr->load(); comp.ratio = pRat->load();
    gate.thr = pGate->load();

    if (mode() == modeMaster)
    {
        bus.thr = pMthr->load(); bus.ratio = busRatios[juce::jlimit (0, 3, juce::roundToInt (pMrat->load()))];
        bus.atkMs = atkMs[juce::jlimit (0, 5, juce::roundToInt (pMatk->load()))];
        bus.relS = relS[juce::jlimit (0, 4, juce::roundToInt (pMrel->load()))];
        bus.makeup = pMmkp->load(); bus.mix = pMmix->load() / 100.0f; bus.scHpf = pMhpf->load() > 0.5f;
    }
    else
    {
        // COLA 4K do grupo: os 3 knobs do módulo; ataque 10 ms, relaxamento AUTO
        bus.thr = pThr->load() >= 9.9f ? 30.0f : pThr->load();
        bus.ratio = pRat->load(); bus.atkMs = 10.0f; bus.relS = -1.0f;
        bus.makeup = 0.0f; bus.mix = pCmix->load() / 100.0f; bus.scHpf = false;
    }
    bus.update();

    const float fd = pFader->load();
    float g = fd <= faderMin + 0.01f ? 0.0f : dbToGain (fd);
    float gl = 1.0f, gr = 1.0f;
    if (mode() != modeMaster) panGains (pPan->load(), gl, gr);
    gainL.setTargetValue (g * gl);
    gainR.setTargetValue (g * gr);
}

void MesaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n == 0 || nch == 0) return;

    const bool solo = pSolo->load() > 0.5f && mode() == modeChannel;
    if (solo != soloWas) { registry->soloCount += solo ? 1 : -1; soloWas = solo; }

    if (licenca->liberada())   // sem ativação: o som passa direto
    {
        if (dirty) updateSettings();
        if (mode() == modeMaster) processMaster (buffer, n, nch);
        else                      processChannel (buffer, n, nch);
    }

    // medidores
    float pk[2] {}, rm[2] {};
    for (int c = 0; c < nch; ++c)
    {
        const float* d = buffer.getReadPointer (c);
        double e = 0; float m = 0;
        for (int i = 0; i < n; ++i) { m = std::max (m, std::abs (d[i])); e += (double) d[i] * d[i]; }
        pk[c] = gainToDb (m); rm[c] = gainToDb ((float) std::sqrt (e / n));
    }
    if (nch == 1) { pk[1] = pk[0]; rm[1] = rm[0]; }
    peakL = pk[0]; peakR = pk[1]; rmsL = rm[0]; rmsR = rm[1];
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

void MesaProcessor::processChannel (juce::AudioBuffer<float>& buf, int n, int nch)
{
    const bool hq = pHq->load() > 0.5f && sat.model != PreModel::limpo;
    float* ch[2] = { buf.getWritePointer (0), buf.getWritePointer (nch > 1 ? 1 : 0) };

    // ganho de entrada + HPF
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSm.getNextValue();
        for (int c = 0; c < nch; ++c) ch[c][i] = hpf.process (c, ch[c][i] * g);
    }

    // PRÉ: a saturação roda a 4× no HQ; no ECO (ou LIMPO) roda a 1× e um atraso iguala a latência
    juce::dsp::AudioBlock<float> block (buf.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
    if (hq)
    {
        auto up = os->processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            float* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = sat.process ((int) c, d[i]);
        }
        os->processSamplesDown (block);
    }
    else
    {
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float y = sat.process (c, ch[c][i]);
                ecoDelay.pushSample (c, y);
                ch[c][i] = ecoDelay.popSample (c);
            }
    }

    const bool eqOn = pEqOn->load() > 0.5f;
    const bool dynFirst = pDynPre->load() > 0.5f;   // botão PRÉ: dinâmica antes do EQ
    const bool channel = mode() == modeChannel;
    const bool muted = pMute->load() > 0.5f || (channel && registry->soloCount.load() > 0 && pSolo->load() < 0.5f);
    float grMax = 0.0f;
    auto doEq = [&] (float& l, float& r)
    {
        if (! eqOn) return;
        l = eq.process (0, l);
        r = nch > 1 ? eq.process (1, r) : l;
    };
    auto doDyn = [&] (float& l, float& r)
    {
        if (channel)
        {
            const float det = std::max (std::abs (l), std::abs (r));
            float g = 1.0f;
            if (gate.active()) g *= gate.process (det);
            if (comp.active()) g *= comp.gain (det);
            else comp.grDb = 0.0f;
            l *= g; r *= g;
            grMax = std::max (grMax, comp.grDb);
        }
        else if (bus.thr < 29.0f)
        {
            bus.process (l, r);
            grMax = std::max (grMax, bus.grDb);
        }
    };
    for (int i = 0; i < n; ++i)
    {
        float l = colour.process (0, ch[0][i]);
        float r = nch > 1 ? colour.process (1, ch[1][i]) : l;
        if (dynFirst) { doDyn (l, r); doEq (l, r); }
        else          { doEq (l, r); doDyn (l, r); }

        const float gl = gainL.getNextValue(), gr = gainR.getNextValue();
        if (nch > 1) { ch[0][i] = muted ? 0.0f : l * gl; ch[1][i] = muted ? 0.0f : r * gr; }
        else         ch[0][i] = muted ? 0.0f : l * 0.5f * (gl + gr);
    }
    grDb = grMax;
}

void MesaProcessor::processMaster (juce::AudioBuffer<float>& buf, int n, int nch)
{
    float* ch[2] = { buf.getWritePointer (0), buf.getWritePointer (nch > 1 ? 1 : 0) };
    const bool on = pMin->load() > 0.5f;
    float grMax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float l = ch[0][i], r = nch > 1 ? ch[1][i] : l;
        if (on) { bus.process (l, r); grMax = std::max (grMax, bus.grDb); }
        // mesma latência dos canais: o MASTER não sobreamostra, mas atrasa igual
        ecoDelay.pushSample (0, l); l = ecoDelay.popSample (0);
        ecoDelay.pushSample (1, r); r = ecoDelay.popSample (1);
        ch[0][i] = l * gainL.getNextValue();
        const float gr = gainR.getNextValue();
        if (nch > 1) ch[1][i] = r * gr;
    }
    grDb = on ? grMax : 0.0f;
}

juce::String MesaProcessor::displayName() const
{
    const juce::ScopedLock sl (nameLock);
    if (nameOverride.isNotEmpty()) return nameOverride;
    if (hostName.isNotEmpty()) return hostName;
    return {};
}
juce::String MesaProcessor::userName() const { const juce::ScopedLock sl (nameLock); return nameOverride; }
void MesaProcessor::setUserName (const juce::String& n)
{
    { const juce::ScopedLock sl (nameLock); nameOverride = n.trim().substring (0, 24); }
    registry->changed();
}
juce::Colour MesaProcessor::trackColour() const
{
    const juce::ScopedLock sl (nameLock);
    return hostColour;
}

void MesaProcessor::updateTrackProperties (const TrackProperties& p)
{
    {
        const juce::ScopedLock sl (nameLock);
        hostName = p.name.value_or (juce::String());
        if (p.colourARGB) hostColour = juce::Colour (*p.colourARGB).withAlpha (1.0f);
    }
    registry->changed();
}

void MesaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("nome", userName(), nullptr);
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("uih", uiH.load(), nullptr);
    state.setProperty ("uigeral", uiGeral.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MesaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            {
                const juce::ScopedLock sl (nameLock);
                nameOverride = vt.getProperty ("nome").toString();
            }
            if ((int) vt.getProperty ("uiw", 0) > 0) { uiW = (int) vt.getProperty ("uiw"); uiH = (int) vt.getProperty ("uih"); }
            uiGeral = (bool) vt.getProperty ("uigeral", false);
            for (auto k : { "ui", "uiw", "uih", "uigeral" }) vt.removeProperty (k, nullptr);
            apvts.replaceState (vt);
            dirty = true;
            registry->changed();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MesaProcessor();
}
