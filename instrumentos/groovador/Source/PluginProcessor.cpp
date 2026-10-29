#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace grv;

GroovadorProcessor::GroovadorProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GROOVADOR", createLayout())
{
    auto raw = [this] (const juce::String& pid) { return apvts.getRawParameterValue (pid); };
    for (int k = 0; k < numInst; ++k)
    {
        for (int q = 0; q < id::instParams.size(); ++q)
            pv[(size_t) k][(size_t) q] = raw (id::i (k, id::instParams[q]));
        pMute[(size_t) k] = raw (id::mute (k));
        pSolo[(size_t) k] = raw (id::solo (k));
        pWho[(size_t) k]  = raw (id::who (k));
    }
    pStyle = raw (id::style);       pMeter = raw (id::meter);       pBars = raw (id::bars);
    pRate = raw (id::rate);         pSwing = raw (id::swing);       pHuman = raw (id::human);
    pEvery = raw (id::every);       pVmode = raw (id::vmode);       pVamt = raw (id::vamount);
    pFill = raw (id::fill);         pPlay = raw (id::play);
    pDrvType = raw (id::drvType);   pDrive = raw (id::fxDrive);     pTone = raw (id::fxTone);
    pDlOn = raw (id::dlOn);         pDlDiv = raw (id::dlDiv);       pDlFb = raw (id::dlFb);
    pDlMix = raw (id::dlMix);       pRvOn = raw (id::rvOn);         pRvSize = raw (id::rvSize);
    pRvMix = raw (id::rvMix);       pVol = raw (id::volume);

    for (auto& v : varNow) v.store (0.0f);
    factory = factoryPresets();
    refreshUserPresets();

    // abre no "Tribo de Goa" (o Init é o PSY FULL cru)
    const int start = findPreset ("Tribo de Goa", false);
    loadPreset (start >= 0 ? start : 0);
    history.clear();
}

bool GroovadorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void GroovadorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int n = juce::jmax (1, samplesPerBlock);
    bufL.assign ((size_t) n, 0.0f);
    bufR.assign ((size_t) n, 0.0f);
    engine.prepare (sampleRate);
    fx.prepare (sampleRate, n);
    readSettings();
    volSm.reset (sampleRate, 0.05);
    volSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pVol->load()));
    lastAbsStep = -1;
    wasRunning = false;
    for (auto& row : pending) for (auto& p : row) p.countdown = 0;
    playStep = -1;
}

void GroovadorProcessor::readSettings()
{
    for (int k = 0; k < numInst; ++k)
    {
        const auto& p = pv[(size_t) k];
        auto& b = base[(size_t) k];
        b.model = juce::jlimit (0, 4, choice (p[0]));
        b.tune  = p[1]->load();
        b.dec   = p[2]->load();
        b.tone  = p[3]->load();
        b.snap  = p[4]->load() * 0.01f;
        b.body  = p[5]->load() * 0.01f;
        b.noise = p[6]->load() * 0.01f;
        b.pEnv  = p[7]->load();
        b.drive = p[8]->load() * 0.01f;
        b.pan   = p[9]->load() * 0.01f;
        b.level = p[10]->load();
    }
    const int seed = pattern.varSeed.load();
    if (seed != deltaSeed)
    {
        for (int k = 0; k < numInst; ++k) deltas[(size_t) k] = varDelta (seed, k);
        deltaSeed = seed;
    }
    int w = 0;
    for (int k = 0; k < numInst; ++k)
        whoIndex[(size_t) k] = pWho[(size_t) k]->load() > 0.5f ? w++ : -1;
    varAmount = pVamt->load() * 0.01f;
    vmode = (VMode) juce::jlimit (0, 2, choice (pVmode));

    fxs.drvType = choice (pDrvType);
    fxs.drive   = pDrive->load() * 0.01f;
    fxs.tone    = pTone->load() * 0.01f;
    fxs.dlOn    = pDlOn->load() > 0.5f;
    fxs.dlDiv   = choice (pDlDiv);
    fxs.dlFb    = pDlFb->load() * 0.01f;
    fxs.dlMix   = pDlMix->load() * 0.01f;
    fxs.rvOn    = pRvOn->load() > 0.5f;
    fxs.rvSize  = pRvSize->load() * 0.01f;
    fxs.rvMix   = pRvMix->load() * 0.01f;
}

// o som de um instrumento com uma dose do timbre B (factor: 0 = A, 1 = B, negativo = o B ao contrário)
Sound GroovadorProcessor::soundFor (int k, float factor) const
{
    const auto& b = base[(size_t) k];
    const auto& d = deltas[(size_t) k];
    const float f = whoIndex[(size_t) k] >= 0 ? factor * varAmount : 0.0f;
    Sound s;
    s.model = b.model;
    s.inst  = k;
    s.hz    = baseHz[k] * std::exp2 ((b.tune + d.tune * 7.0f * f) / 12.0f);
    s.decMs = juce::jlimit (8.0f, 2500.0f, b.dec * std::exp2 (d.dec * f));
    s.tone  = juce::jlimit (150.0f, 18000.0f, b.tone * std::exp2 (d.tone * 1.5f * f));
    s.snap  = juce::jlimit (0.0f, 1.0f, b.snap + d.snap * 0.4f * f);
    s.body  = juce::jlimit (0.0f, 1.0f, b.body + d.body * 0.4f * f);
    s.noise = b.noise;
    s.pEnv  = b.pEnv + d.pEnv * 8.0f * f;
    s.drive = b.drive;
    s.pan   = b.pan;
    s.gain  = juce::Decibels::decibelsToGain (b.level);
    s.highPass = k == hatC || k == hatO || k == shaker;
    return s;
}

float GroovadorProcessor::varFactorFor (int inst, long long seg) const
{
    int w = 0, mine = -1;
    for (int k = 0; k < numInst; ++k)
        if (apvts.getRawParameterValue (id::who (k))->load() > 0.5f) { if (k == inst) mine = w; ++w; }
    if (mine < 0) return 0.0f;
    return varFactor ((VMode) juce::jlimit (0, 2, juce::roundToInt (apvts.getRawParameterValue (id::vmode)->load())),
                      seg, mine, pattern.varSeed.load());
}

void GroovadorProcessor::fire (int inst, float vel, float factor)
{
    engine.trigger (inst, soundFor (inst, factor), vel);
}

int GroovadorProcessor::stepsNow() const
{
    return patternSteps (choice (pMeter), choice (pBars), choice (pRate));
}

int GroovadorProcessor::stepsPerBarNow() const
{
    return stepsPerBar (choice (pMeter), choice (pRate));
}

void GroovadorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0 || buffer.getNumChannels() < 2) return;
    if (! licenca->liberada()) return;   // sem ativação: mudo (o buffer já foi zerado)
    if ((int) bufL.size() < n) { bufL.resize ((size_t) n); bufR.resize ((size_t) n); }
    std::fill (bufL.begin(), bufL.begin() + n, 0.0f);
    std::fill (bufR.begin(), bufR.begin() + n, 0.0f);

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
    const bool hostRun = playing && havePpq;
    synced.store (hostRun);

    readSettings();
    fxs.bpm = bpm;

    const bool   padsOnly = choice (pPlay) == 1;
    const int    steps  = stepsNow();
    const int    spb    = stepsPerBarNow();
    const int    half   = juce::jmin (fillSteps, spb / 2);
    const double sb     = rateBeats[juce::jlimit (0, 2, choice (pRate))];
    const double sw     = (double) pSwing->load() * 0.01 * 0.5 * sb;     // atraso dos passos ímpares
    const double pps    = bpm / 60.0 / currentRate;
    const double stepSamples = sb * 60.0 / bpm * currentRate;
    const int    fe     = fillEvery[juce::jlimit (0, 2, choice (pFill))];
    const long long segLen = juce::jmax (1LL, (long long) std::llround (everyBars[juce::jlimit (0, 4, choice (pEvery))] * spb));
    const float  human  = pHuman->load() * 0.01f;

    bool anySolo = false;
    for (auto* p : pSolo) anySolo = anySolo || p->load() > 0.5f;
    auto audible = [&] (int k)
    {
        if (pMute[(size_t) k]->load() > 0.5f) return false;
        return ! anySolo || pSolo[(size_t) k]->load() > 0.5f;
    };

    // segmento de variação atual (para os pads e para a tela)
    long long segNow = lastAbsStep >= 0 ? lastAbsStep / segLen : 0;
    auto factorNow = [&] (int k) { return whoIndex[(size_t) k] >= 0 ? varFactor (vmode, segNow, whoIndex[(size_t) k], deltaSeed) : 0.0f; };

    int rendered = 0;
    auto flush = [&] (int upTo)
    {
        if (upTo > rendered)
        {
            engine.render (bufL.data() + rendered, bufR.data() + rendered, upTo - rendered);
            rendered = upTo;
        }
    };

    // clique no nome do instrumento na tela
    const int aud = audition.exchange (-1);
    if (aud >= 0 && aud < numInst) fire (aud, 0.85f, factorNow (aud));

    auto it = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        // PADS: cada nota toca um instrumento (nos dois modos)
        while (it != midi.cend() && (*it).samplePosition <= i)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())
                for (int k = 0; k < numInst; ++k)
                    if (m.getNoteNumber() == padNotes[k])
                    {
                        flush (i);
                        fire (k, m.getFloatVelocity(), factorNow (k));
                    }
            ++it;
        }

        const bool run = hostRun && ! padsOnly;
        if (run != wasRunning)
        {
            if (! run)
            {
                lastAbsStep = -1;
                playStep = -1;
                fillNow = false;
                for (auto& row : pending) for (auto& p : row) p.countdown = 0;
            }
            wasRunning = run;
        }

        if (run)
        {
            // ---- qual passo é agora (com swing nos ímpares) ----
            const double p    = ppq0 + pps * (double) i;
            const double pair = std::floor (p / (2.0 * sb));
            const double t    = p - pair * 2.0 * sb;
            const bool   second = t >= sb + sw;
            const long long absStep = (long long) pair * 2 + (second ? 1 : 0);

            if (absStep != lastAbsStep && absStep >= 0)
            {
                const int idx = (int) (absStep % steps);
                const long long bar = absStep / spb;
                const int inBar = (int) (absStep % spb);
                const bool inFill = fe > 0 && ((bar + 1) % fe) == 0 && inBar >= spb - half;
                const int fillIdx = inBar - (spb - half);
                segNow = absStep / segLen;

                for (int k = 0; k < numInst; ++k)
                {
                    const float f = factorNow (k);
                    varNow[(size_t) k].store (whoIndex[(size_t) k] >= 0 ? f * varAmount : 0.0f);
                    if (! audible (k)) continue;

                    Step s = pattern.get (k, idx);
                    if (inFill)
                    {
                        bool rowHas = false;
                        for (int j = 0; j < half && ! rowHas; ++j) rowHas = pattern.getFill (k, j).on;
                        if (rowHas) s = pattern.getFill (k, fillIdx);
                    }
                    if (! s.on || audioRnd.nextFloat() >= s.chance()) continue;

                    const int hits = s.roll + 1;
                    for (int h = 0; h < hits; ++h)
                    {
                        float v = s.gain() * (1.0f - human * 0.3f * audioRnd.nextFloat());
                        if (hits > 1) v *= 0.7f + 0.3f * (float) (h + 1) / (float) hits;     // roll cresce
                        const int delay = (int) (stepSamples * (double) h / (double) hits)
                                        + (int) (human * 0.010f * (float) currentRate * audioRnd.nextFloat());
                        if (delay == 0) { flush (i); fire (k, v, f); continue; }
                        for (auto& pd : pending[(size_t) k])
                            if (pd.countdown <= 0) { pd.countdown = delay; pd.vel = v; pd.factor = f; break; }
                    }
                }
                lastAbsStep = absStep;
                playStep = idx;
                playBar = (int) (bar % 1000000);
                fillNow = inFill;
            }
        }

        // batidas atrasadas (roll e HUMANO)
        for (int k = 0; k < numInst; ++k)
            for (auto& pd : pending[(size_t) k])
                if (pd.countdown > 0 && --pd.countdown == 0)
                {
                    flush (i);
                    fire (k, pd.vel, pd.factor);
                }
    }
    flush (n);

    // efeitos e volume
    fx.process (bufL.data(), bufR.data(), n, fxs);
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    volSm.setTargetValue (juce::Decibels::decibelsToGain (pVol->load()));
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = volSm.getNextValue();
        l[i] = juce::jlimit (-2.0f, 2.0f, bufL[(size_t) i] * g);
        r[i] = juce::jlimit (-2.0f, 2.0f, bufR[(size_t) i] * g);
        peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
    }
    meterOut.store (juce::jmax (meterOut.load(), peak));
}

juce::AudioProcessorEditor* GroovadorProcessor::createEditor()
{
    return new GroovadorEditor (*this);
}

// ---------------- estado ----------------

void GroovadorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", pattern.toString(), nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetUser", presetIsUser, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("uiSel", uiSel.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GroovadorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            if (tree.hasProperty ("pattern"))
                pattern.fromString (tree.getProperty ("pattern").toString());
            presetName   = tree.getProperty ("presetName", presetName).toString();
            presetIsUser = (bool) tree.getProperty ("presetUser", false);
            presetIndex  = findPreset (presetName, presetIsUser);
            uiScale      = juce::jlimit (0.75f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
            uiSel        = juce::jlimit (0, numInst - 1, (int) tree.getProperty ("uiSel", uiSel.load()));
            history.clear();
            ++patternVersion;
        }
}

// ---------------- presets ----------------

juce::File GroovadorProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/GROOVADOR");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/GROOVADOR/Presets");
   #endif
}

void GroovadorProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    rebuildList();
}

void GroovadorProcessor::rebuildList()
{
    entries.clear();
    for (auto& f : factory)
        entries.push_back ({ f.style, u8 (f.name), f.style, f.seed, {} });
    for (auto& f : userFiles)
        entries.push_back ({ numCategories, f.getFileNameWithoutExtension(), -1, 0, f });
}

juce::StringArray GroovadorProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& e : entries) names.add (e.name);
    return names;
}

int GroovadorProcessor::getPresetCategory (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].category;
    return numCategories;
}

bool GroovadorProcessor::isUserEntry (int index) const
{
    return index >= 0 && index < (int) entries.size() && entries[(size_t) index].file != juce::File();
}

int GroovadorProcessor::findPreset (const juce::String& name, bool user) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].name == name && (entries[i].file != juce::File()) == user) return (int) i;
    return -1;
}

void GroovadorProcessor::setParam (const juce::String& pid, float value)
{
    if (auto* rp = apvts.getParameter (pid))
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

// tudo que não é performance volta ao padrão
void GroovadorProcessor::applyDefaults()
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (! isPerformanceParam (rp->getParameterID()))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
}

// preset de fábrica: kit do estilo + ajustes pela semente + groove sorteado com a semente
void GroovadorProcessor::applyFactory (int style, int seed)
{
    applyDefaults();
    const int st = juce::jlimit (0, numCategories - 1, style < 0 ? 0 : style);
    juce::Random r ((juce::int64) seed * 2654435761LL + 99);
    auto around = [&r] (float centre, float spread, float lo, float hi)
    { return juce::jlimit (lo, hi, centre + (r.nextFloat() - 0.5f) * spread); };

    setParam (id::style, (float) st);

    std::array<std::array<float, 11>, numInst> v {};
    for (int k = 0; k < numInst; ++k)
    {
        const auto& d = instDefaults[k];
        v[(size_t) k] = { (float) d.model, d.tune, d.dec, d.tone, d.snap, d.body, d.noise, d.pEnv, d.drive, d.pan, d.level };
    }
    if (style >= 0)
    {
        for (auto& t : kitTweaks())
            if (t.style == st)
            {
                const int q = id::instParams.indexOf (t.param);
                if (q >= 0) v[(size_t) t.inst][(size_t) q] = t.value;
            }
        for (int k = 0; k < numInst; ++k)
        {
            auto& x = v[(size_t) k];
            x[1] = juce::jlimit (-24.0f, 24.0f, x[1] + (float) (r.nextInt (5) - 2));
            x[2] = juce::jlimit (10.0f, 2000.0f, x[2] * std::exp2 ((r.nextFloat() - 0.5f) * 0.6f));
            x[3] = juce::jlimit (200.0f, 16000.0f, x[3] * std::exp2 ((r.nextFloat() - 0.5f) * 0.5f));
            x[4] = around (x[4], 20.0f, 0.0f, 100.0f);
            x[5] = around (x[5], 20.0f, 0.0f, 100.0f);
        }
    }
    for (int k = 0; k < numInst; ++k)
        for (int q = 0; q < 11; ++q)
            setParam (id::i (k, id::instParams[q]), v[(size_t) k][(size_t) q]);

    GenParams gp;
    gp.style = st;
    if (style >= 0)
    {
        const auto& f = styleFeel[st];
        gp.density = around (f.density, 16.0f, 0.0f, 100.0f) * 0.01f;
        gp.ghost   = around (f.ghost, 20.0f, 0.0f, 100.0f) * 0.01f;
        gp.roll    = around (f.roll, 20.0f, 0.0f, 100.0f) * 0.01f;
        setParam (id::density, gp.density * 100.0f);
        setParam (id::ghost,   gp.ghost * 100.0f);
        setParam (id::roll,    gp.roll * 100.0f);
        setParam (id::swing,   around (f.swing, 10.0f, 0.0f, 75.0f));
        setParam (id::human,   around (f.human, 16.0f, 0.0f, 100.0f));
        setParam (id::vmode,   (float) f.vmode);
        setParam (id::every,   (float) f.every);
        setParam (id::vamount, around (f.amount, 20.0f, 0.0f, 100.0f));
        setParam (id::fill,    (float) f.fill);
        setParam (id::rvMix,   f.rvMix);
        setParam (id::dlOn,    f.dlMix > 0.0f ? 1.0f : 0.0f);
        if (f.dlMix > 0.0f) setParam (id::dlMix, f.dlMix);
        // TRIBAL às vezes em tercina
        if (st == 4 && r.nextInt (3) == 0) { setParam (id::rate, 1.0f); gp.rate = 1; }
        // quem varia: o padrão com um instrumento trocado
        const int flip = r.nextInt (numInst);
        setParam (id::who (flip), getParam (id::who (flip)) > 0.5f ? 0.0f : 1.0f);
    }
    else
    {
        gp.density = getParam (id::density) * 0.01f;
        gp.ghost   = getParam (id::ghost) * 0.01f;
        gp.roll    = getParam (id::roll) * 0.01f;
    }

    pattern.clear();
    pattern.varSeed.store (seed);
    juce::Random g ((juce::int64) seed * 31 + 7);
    generateRhythm (pattern, gp, g);
    generateVelocity (pattern, gp, g);
    generateFills (pattern, gp, g);
}

void GroovadorProcessor::loadPreset (int index)
{
    refreshUserPresets();
    const int total = (int) entries.size();
    if (total == 0) return;
    index = ((index % total) + total) % total;
    const auto e = entries[(size_t) index];
    pushHistory();

    if (e.file == juce::File())
        applyFactory (e.style, e.seed);
    else
    {
        auto xml = juce::parseXML (e.file);
        if (xml == nullptr) return;
        auto tree = juce::ValueTree::fromXml (*xml);
        for (auto* prm : getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
            if (rp == nullptr || isPerformanceParam (rp->getParameterID())) continue;
            float value = rp->convertFrom0to1 (rp->getDefaultValue());
            auto child = tree.getChildWithProperty ("id", rp->getParameterID());
            if (child.isValid() && child.hasProperty ("value")) value = (float) child.getProperty ("value");
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
            rp->endChangeGesture();
        }
        if (tree.hasProperty ("pattern"))
            pattern.fromString (tree.getProperty ("pattern").toString());
    }

    presetIndex  = index;
    presetName   = e.name;
    presetIsUser = e.file != juce::File();
    ++patternVersion;
}

juce::File GroovadorProcessor::currentUserFile() const
{
    if (! presetIsUser || presetIndex < 0 || presetIndex >= (int) entries.size()) return {};
    const auto& e = entries[(size_t) presetIndex];
    if (e.name != presetName) return {};
    return e.file.existsAsFile() ? e.file : juce::File();
}

bool GroovadorProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String GroovadorProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto nm = "Meu groove " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (nm + ".xml").existsAsFile()) return nm;
    }
    return "Meu groove";
}

void GroovadorProcessor::writePreset (const juce::File& file)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", pattern.toString(), nullptr);
    if (auto xml = state.createXml())
        xml->writeTo (file);
}

void GroovadorProcessor::saveUserPreset (const juce::String& name)
{
    const auto baseName = juce::File::createLegalFileName (name.trim());
    if (baseName.isEmpty()) return;
    auto dir = userPresetDir();
    dir.createDirectory();

    auto file = dir.getChildFile (baseName + ".xml");
    for (int i = 2; file.existsAsFile() && i < 1000; ++i)
        file = dir.getChildFile (baseName + " (" + juce::String (i) + ").xml");
    writePreset (file);

    refreshUserPresets();
    presetName   = file.getFileNameWithoutExtension();
    presetIsUser = true;
    presetIndex  = findPreset (presetName, true);
}

void GroovadorProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool GroovadorProcessor::renameUserPreset (const juce::String& newName)
{
    const auto file = currentUserFile();
    const auto baseName = juce::File::createLegalFileName (newName.trim());
    if (! file.existsAsFile() || baseName.isEmpty()) return false;

    const auto target = file.getSiblingFile (baseName + ".xml");
    const bool sameFile = target.getFullPathName().equalsIgnoreCase (file.getFullPathName());
    if ((target.existsAsFile() && ! sameFile) || ! file.moveFileTo (target)) return false;

    refreshUserPresets();
    presetName  = baseName;
    presetIndex = findPreset (presetName, true);
    return true;
}

void GroovadorProcessor::deleteUserPreset()
{
    auto file = currentUserFile();
    if (! file.existsAsFile()) return;
    if (! file.moveToTrash()) file.deleteFile();
    refreshUserPresets();
    presetName   = "Sem nome";
    presetIsUser = false;
    presetIndex  = -1;
}

// ---------------- gerador ----------------

void GroovadorProcessor::pushHistory()
{
    Snapshot snap;
    snap.pattern = pattern.toString();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (! isPerformanceParam (rp->getParameterID()))
                snap.values.emplace_back (rp->getParameterID(), rp->convertFrom0to1 (rp->getValue()));
    history.push_back (std::move (snap));
    if (history.size() > 32) history.erase (history.begin());
}

void GroovadorProcessor::undo()
{
    if (history.empty()) return;
    const auto snap = history.back();
    history.pop_back();
    for (const auto& kv : snap.values) setParam (kv.first, kv.second);
    pattern.fromString (snap.pattern);
    ++patternVersion;
}

GenParams GroovadorProcessor::genParams() const
{
    GenParams gp;
    gp.style   = choice (pStyle);
    gp.meter   = choice (pMeter);
    gp.rate    = choice (pRate);
    gp.density = getParam (id::density) * 0.01f;
    gp.ghost   = getParam (id::ghost) * 0.01f;
    gp.roll    = getParam (id::roll) * 0.01f;
    return gp;
}

// sorteio do som de um instrumento: modelos que fazem sentido para ele, valores em volta do padrão
void GroovadorProcessor::randomizeTimbre (int k, juce::Random& r)
{
    auto pick = [&r] (std::initializer_list<int> l) { return *(l.begin() + r.nextInt ((int) l.size())); };
    auto set = [this, k] (const juce::String& name, float value) { setParam (id::i (k, name), value); };
    const auto& d = instDefaults[k];

    int model = d.model;
    switch (k)
    {
        case rim:    model = pick ({ analog, analog, metal, fm }); break;
        case snare:  model = pick ({ analog, analog, m808, fm, noise }); break;
        case hatC:
        case hatO:   model = pick ({ metal, metal, noise }); break;
        case perc1:
        case perc2:  model = pick ({ analog, m808, fm, fm }); break;
        default:     model = noise; break;     // clap e shaker
    }
    set (id::model, (float) model);
    set (id::tune,  (float) (r.nextInt (15) - 7));
    set (id::dec,   juce::jlimit (10.0f, 2000.0f, d.dec * std::exp2 (r.nextFloat() * 1.6f - 0.8f)));
    set (id::tone,  juce::jlimit (200.0f, 16000.0f, d.tone * std::exp2 (r.nextFloat() * 1.6f - 0.8f)));
    set (id::snap,  20.0f + r.nextFloat() * 70.0f);
    set (id::body,  10.0f + r.nextFloat() * 80.0f);
    set (id::noise, model == noise ? 70.0f + r.nextFloat() * 30.0f : juce::jlimit (0.0f, 100.0f, d.noise + (r.nextFloat() - 0.5f) * 50.0f));
    const bool pitched = k == rim || k == snare || k == perc1 || k == perc2;
    set (id::pEnv,  pitched ? (float) pick ({ 0, 3, 5, 7, 12, 18, -5 }) : 0.0f);
    set (id::drive, r.nextFloat() * r.nextFloat() * 50.0f);
}

void GroovadorProcessor::generate (int parts, bool respectLocks)
{
    pushHistory();
    auto locked = [this, respectLocks] (const juce::String& pid) { return respectLocks && getParam (pid) > 0.5f; };
    const auto gp = genParams();

    if ((parts & partRhythm) != 0 && ! locked (id::lockRhythm))
        generateRhythm (pattern, gp, rnd);
    if ((parts & partVelocity) != 0 && ! locked (id::lockVel))
        generateVelocity (pattern, gp, rnd);
    if ((parts & partFills) != 0 && ! locked (id::lockFills))
        generateFills (pattern, gp, rnd);
    if ((parts & partTimbres) != 0 && ! locked (id::lockTimbre))
    {
        for (int k = 0; k < numInst; ++k) randomizeTimbre (k, rnd);
        pattern.varSeed.store (1 + rnd.nextInt (1000000));
    }
    ++patternVersion;
}

void GroovadorProcessor::randomizeInstrument (int inst)
{
    pushHistory();
    randomizeTimbre (juce::jlimit (0, numInst - 1, inst), rnd);
}

void GroovadorProcessor::editStep (int inst, int step, Step s)
{
    pushHistory();
    pattern.set (inst, step, s);
    ++patternVersion;
}

// o groove como .mid: canal 10, cada instrumento na nota do PADS (C1 rim, D1 snare…), roll em batidas separadas
juce::File GroovadorProcessor::writeMidiFile()
{
    const int tpq = 960;
    const double stepTicks = rateBeats[juce::jlimit (0, 2, choice (pRate))] * tpq;
    const int steps = stepsNow();

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jmax (20.0f, shownBpm.load()))), 0.0);
    for (int k = 0; k < numInst; ++k)
        for (int i = 0; i < steps; ++i)
        {
            const auto s = pattern.get (k, i);
            if (! s.on) continue;
            const int hits = s.roll + 1;
            for (int h = 0; h < hits; ++h)
            {
                const double on = (i + (double) h / hits) * stepTicks;
                const auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (s.gain() * 127.0f));
                seq.addEvent (juce::MidiMessage::noteOn (10, padNotes[k], vel), on);
                seq.addEvent (juce::MidiMessage::noteOff (10, padNotes[k]), on + stepTicks / hits * 0.5);
            }
        }
    seq.addEvent (juce::MidiMessage::endOfTrack(), steps * stepTicks);
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);
    mf.addTrack (seq);

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("GROOVADOR");
    dir.createDirectory();
    auto name = juce::File::createLegalFileName (presetName.trim());
    auto file = dir.getChildFile ("GROOVADOR - " + (name.isEmpty() ? juce::String ("groove") : name) + ".mid");
    file.deleteFile();
    juce::FileOutputStream os (file);
    if (os.openedOk()) mf.writeTo (os);
    return file;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GroovadorProcessor();
}
