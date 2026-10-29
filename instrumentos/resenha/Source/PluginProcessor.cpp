#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace resenha;

ResenhaProcessor::ResenhaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RESENHA", createLayout())
{
    auto raw = [this] (const juce::String& pid) { return apvts.getRawParameterValue (pid); };
    for (int v = 0; v < numVoices; ++v)
        for (int k = 0; k < id::voiceParams.size(); ++k)
            pv[(size_t) v][(size_t) k] = raw (id::v (v, id::voiceParams[k]));

    pScale = raw (id::scale);       pRoot = raw (id::root);         pBars = raw (id::bars);
    pRate = raw (id::rate);         pOct = raw (id::octaves);       pDens = raw (id::density);
    pGate = raw (id::gate);         pGateVar = raw (id::gateVar);   pSwing = raw (id::swing);
    pVoices = raw (id::voices);     pEvery = raw (id::every);       pOrder = raw (id::order);
    pResp = raw (id::response);     pInterval = raw (id::interval); pPlay = raw (id::play);
    pDrvType = raw (id::drvType);   pDrive = raw (id::drive);       pTone = raw (id::tone);
    pDlOn = raw (id::dlOn);         pDlDiv = raw (id::dlDiv);       pDlFb = raw (id::dlFb);
    pDlMix = raw (id::dlMix);       pRvOn = raw (id::rvOn);         pRvSize = raw (id::rvSize);
    pRvMix = raw (id::rvMix);       pVol = raw (id::volume);
    pContour = raw (id::contour);   pDuck = raw (id::duck);

    factory = factoryPresetData();
    refreshBanks();
    refreshUserPresets();

    // abre no "Última Palavra" (CONVERSA FM: escada que fecha a fala num mergulho; o Init é só uma escadinha na tônica)
    const int start = findPreset (u8 ("Última Palavra"), {});
    loadPreset (start >= 0 ? start : 0);
    history.clear();
}

bool ResenhaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void ResenhaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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
    gateOffPpq.fill (-1.0);
    wasRunning = false;
    freePpq = 0.0;
    numHeld = 0;
    transpose = 0;
    playStep = -1;
    duckGain = 1.0f;
}

void ResenhaProcessor::readSettings()
{
    for (int v = 0; v < numVoices; ++v)
    {
        const auto& p = pv[(size_t) v];
        auto& t = s.t[(size_t) v];
        t.wave    = choice (p[0]);
        t.ratio   = ratios[juce::jlimit (0, (int) std::size (ratios) - 1, choice (p[1]))];
        t.fm      = p[2]->load() * 0.01f;
        t.fb      = p[3]->load() * 0.01f;
        t.fmDecMs = p[4]->load();
        t.cut     = p[5]->load();
        t.res     = p[6]->load() * 0.01f;
        t.decMs   = p[7]->load();
        t.pan     = p[8]->load();
        t.glideMs = p[9]->load();
        t.pEnv    = p[10]->load();
        t.fEnv    = p[11]->load() * 0.01f;
        t.bend    = p[12]->load();
    }
    s.drvType = choice (pDrvType);
    s.drive   = pDrive->load() * 0.01f;
    s.tone    = pTone->load() * 0.01f;
    s.dlOn    = pDlOn->load() > 0.5f;
    s.dlDiv   = choice (pDlDiv);
    s.dlFb    = pDlFb->load() * 0.01f;
    s.dlMix   = pDlMix->load() * 0.01f;
    s.rvOn    = pRvOn->load() > 0.5f;
    s.rvSize  = pRvSize->load() * 0.01f;
    s.rvMix   = pRvMix->load() * 0.01f;
}

PhraseParams ResenhaProcessor::phraseParams() const
{
    PhraseParams p;
    p.steps    = phraseSteps (choice (pBars), choice (pRate));
    p.scale    = choice (pScale);
    p.root     = choice (pRoot);
    p.voices   = choice (pVoices) + 1;
    p.every    = everyCounts[juce::jlimit (0, 5, choice (pEvery))];
    p.order    = (Order) juce::jlimit (0, 2, choice (pOrder));
    p.response = (Response) juce::jlimit (0, 3, choice (pResp));
    p.interval = choice (pInterval);
    return p;
}

int ResenhaProcessor::stepsNow() const
{
    return phraseSteps (choice (pBars), choice (pRate));
}

// teclado: a última nota segurada transpõe a frase (Dó 3 = como está escrita).
// COM NOTA: a frase só toca enquanto houver nota segurada; SEMPRE: toca com o play do host (a nota só transpõe).
void ResenhaProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        int w = 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        numHeld = w;
        if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note;
        transpose = note - 60;
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        int w = 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        numHeld = w;
        if (numHeld > 0) transpose = held[(size_t) numHeld - 1] - 60;
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        numHeld = 0;
        transpose = 0;
    }
}

void ResenhaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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
    s.bpm = bpm;

    const auto pp = phraseParams();
    buildPhrase (pattern, pp, phrase);
    const bool   always = choice (pPlay) == 1;
    const int    steps  = phrase.steps;
    const double sb     = rateBeats[juce::jlimit (0, 4, choice (pRate))];
    const double sw     = (double) pSwing->load() * 0.01 * 0.5 * sb;     // atraso dos passos pares
    const double pps    = bpm / 60.0 / currentRate;
    double freeBase     = freePpq;

    int rendered = 0;
    auto flush = [&] (int upTo)
    {
        if (upTo > rendered)
        {
            engine.render (bufL.data() + rendered, bufR.data() + rendered, upTo - rendered, s);
            rendered = upTo;
        }
    };

    auto stopAll = [&]
    {
        engine.allOff();
        lastAbsStep = -1;
        gateOffPpq.fill (-1.0);
        playStep = -1;
    };

    auto it = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.cend() && (*it).samplePosition <= i)
        {
            flush (i);
            const int before = numHeld;
            handleMidi ((*it).getMessage());
            // host parado: a primeira tecla começa a frase do início, no relógio próprio
            if (! hostRun && before == 0 && numHeld > 0)
            {
                freeBase = -pps * (double) i;
                lastAbsStep = -1;
            }
            ++it;
        }

        const bool run = hostRun ? (always || numHeld > 0) : numHeld > 0;
        if (run != wasRunning)
        {
            if (! run) { flush (i); stopAll(); }
            wasRunning = run;
        }
        if (! run) continue;

        // ---- qual passo é agora (com swing nos pares) ----
        const double p    = hostRun ? ppq0 + pps * (double) i : freeBase + pps * (double) i;
        const double pair = std::floor (p / (2.0 * sb));
        const double t    = p - pair * 2.0 * sb;
        const bool   second = t >= sb + sw;
        const long long absStep = (long long) pair * 2 + (second ? 1 : 0);

        if (absStep != lastAbsStep)
        {
            flush (i);
            const int idx = (int) (((absStep % steps) + steps) % steps);
            const int ni  = phrase.atStep[(size_t) idx];
            if (ni >= 0)
            {
                const auto& pn = phrase.notes[(size_t) ni];
                const auto& tb = s.t[(size_t) pn.voice];
                const double lenBeats = sb * ((double) (pn.len - 1) + (double) pn.gate);
                const float  bendSt = pn.last ? tb.bend : 0.0f;    // o mergulho só na nota que fecha a fala
                const int    bendLen = (int) (lenBeats * 60.0 / bpm * currentRate);
                engine.noteOn (pn.voice, pn.note + transpose, pn.accent, tb.glideMs, bendSt, bendLen);
                const double start = pair * 2.0 * sb + (second ? sb + sw : 0.0);
                gateOffPpq[(size_t) pn.voice] = start + lenBeats;
            }
            lastAbsStep = absStep;
            playStep    = idx;
        }

        for (int v = 0; v < numVoices; ++v)
            if (gateOffPpq[(size_t) v] >= 0.0 && p >= gateOffPpq[(size_t) v])
            {
                flush (i);
                engine.noteOff (v);
                gateOffPpq[(size_t) v] = -1.0;
            }
    }
    flush (n);
    for (; it != midi.cend(); ++it)
        handleMidi ((*it).getMessage());
    freePpq = freeBase + pps * (double) n;
    waitingKey.store (hostRun && ! always && numHeld == 0);

    // efeitos e volume; DUCK = o lead abaixa a cada tempo e volta em meio tempo (o "sidechain" do bumbo do psy)
    fx.process (bufL.data(), bufR.data(), n, s);
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    volSm.setTargetValue (juce::Decibels::decibelsToGain (pVol->load()));
    const float duck = pDuck->load() * 0.01f;
    const float duckK = 1.0f - std::exp (-1.0f / (0.0015f * (float) currentRate));
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float target = 1.0f;
        if (duck > 0.0f)
        {
            const double p = hostRun ? ppq0 + pps * (double) i : freePpq - pps * (double) (n - i);
            const float ph = (float) (p - std::floor (p));
            const float shape = ph < 0.012f ? ph / 0.012f : juce::square (juce::jmax (0.0f, 1.0f - (ph - 0.012f) / 0.5f));
            target = 1.0f - duck * 0.92f * shape;
        }
        duckGain += (target - duckGain) * duckK;
        const float g = volSm.getNextValue() * duckGain;
        l[i] = juce::jlimit (-2.0f, 2.0f, bufL[(size_t) i] * g);
        r[i] = juce::jlimit (-2.0f, 2.0f, bufR[(size_t) i] * g);
        peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
    }
    meterOut.store (juce::jmax (meterOut.load(), peak));
}

juce::AudioProcessorEditor* ResenhaProcessor::createEditor()
{
    return new ResenhaEditor (*this);
}

// ---------------- estado ----------------

void ResenhaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", pattern.toString(), nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetBank", presetBank, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ResenhaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            if (tree.hasProperty ("pattern"))
                pattern.fromString (tree.getProperty ("pattern").toString());
            presetName  = tree.getProperty ("presetName", presetName).toString();
            presetBank  = tree.getProperty ("presetBank", juce::String()).toString();
            presetIndex = findPreset (presetName, presetBank);
            uiScale     = juce::jlimit (0.75f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
            history.clear();
            ++patternVersion;
        }
}

// ---------------- presets ----------------
//
// Bancos avulsos: arquivo .rsnbank (XML) que a pessoa baixa e importa pelo menu de presets.
//   <RESENHABANK format="1" name="Factory 2" author="InsideStudioSP">
//     <PRESET name="Papo Novo" category="FULL ON">
//       <PARAM id="afm" value="62"/>                 (valor REAL, igual ao estado salvo pelo plugin)
//       <PHRASE text="0:7 2:5! - 4:9 ~ -1:3"/>       (mesmo texto dos presets de fábrica, ver parsePattern)
//     </PRESET>
//   </RESENHABANK>
// category = nome em categoryNames (ou o índice); sem categoria ou desconhecida = OUTROS.
// O que o preset não traz volta ao padrão; as travas e o modo de tocar (performance) não mudam.

namespace
{
    const juce::String bankExt = ".rsnbank";
    const juce::String bankTag = "RESENHABANK";
    const juce::String factoryBankName = "FACTORY 1";
    const juce::String userBankKey = "USER";

    juce::String parseBank (const juce::File& file, juce::String& name, std::vector<PresetData>& out)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr || ! xml->hasTagName (bankTag))
            return u8 ("O arquivo não é um banco de presets do RESENHA.");
        if (xml->getIntAttribute ("format", 1) > 1)
            return u8 ("Esse banco é de uma versão mais nova do RESENHA. Atualize o plugin.");

        name = xml->getStringAttribute ("name").trim();
        if (name.isEmpty()) name = file.getFileNameWithoutExtension().trim();

        out.clear();
        for (auto* pe : xml->getChildWithTagNameIterator ("PRESET"))
        {
            PresetData p;
            p.name = pe->getStringAttribute ("name").trim();
            if (p.name.isEmpty()) continue;

            const auto cat = pe->getStringAttribute ("category").trim();
            p.category = numCategories;
            for (int c = 0; c < numCategories; ++c)
                if (cat.equalsIgnoreCase (u8 (categoryNames[c]))) p.category = c;
            if (cat.containsOnly ("0123456789") && cat.isNotEmpty())
                p.category = juce::jlimit (0, numCategories, cat.getIntValue());

            for (auto* v : pe->getChildWithTagNameIterator ("PARAM"))
                if (v->hasAttribute ("id") && v->hasAttribute ("value"))
                    p.values.emplace_back (v->getStringAttribute ("id"), (float) v->getDoubleAttribute ("value"));
            if (auto* ph = pe->getChildByName ("PHRASE"))
                p.phrase = ph->getStringAttribute ("text").trim();
            out.push_back (std::move (p));
        }
        if (out.empty())
            return u8 ("O banco não tem nenhum preset.");
        return {};
    }
}

juce::File ResenhaProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/RESENHA");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/RESENHA/Presets");
   #endif
}

juce::File ResenhaProcessor::bankDir() const
{
   #if JUCE_MAC
    return userPresetDir().getChildFile ("Banks");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/RESENHA/Banks");
   #endif
}

void ResenhaProcessor::refreshBanks()
{
    banks.clear();
    auto dir = bankDir();
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*" + bankExt))
        {
            PresetBank b;
            b.file = f;
            if (parseBank (f, b.name, b.presets).isEmpty())
                banks.push_back (std::move (b));
        }
    std::sort (banks.begin(), banks.end(),
               [] (const PresetBank& a, const PresetBank& b) { return a.name.compareNatural (b.name) < 0; });
    rebuildList();
}

void ResenhaProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    rebuildList();
}

void ResenhaProcessor::rebuildList()
{
    entries.clear();
    for (auto& p : factory)
        entries.push_back ({ 0, p.category, p.name, &p, {} });
    for (size_t b = 0; b < banks.size(); ++b)
        for (auto& p : banks[b].presets)
            entries.push_back ({ (int) b + 1, p.category, p.name, &p, {} });
    for (auto& f : userFiles)
        entries.push_back ({ bankUser, numCategories, f.getFileNameWithoutExtension(), nullptr, f });
}

juce::StringArray ResenhaProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& e : entries) names.add (e.name);
    return names;
}

int ResenhaProcessor::getPresetCategory (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].category;
    return numCategories;
}

int ResenhaProcessor::getPresetBank (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].bank;
    return bankUser;
}

juce::StringArray ResenhaProcessor::getBankNames() const
{
    juce::StringArray names (factoryBankName);
    for (auto& b : banks) names.add (b.name);
    return names;
}

juce::String ResenhaProcessor::bankKey (int bank) const
{
    if (bank == bankUser) return userBankKey;
    if (bank >= 1 && bank <= (int) banks.size()) return banks[(size_t) bank - 1].name;
    return {};
}

int ResenhaProcessor::findPreset (const juce::String& name, const juce::String& bank) const
{
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (e.name == name && bankKey (e.bank) == bank) return (int) i;
    }
    return -1;
}

void ResenhaProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (isPerformanceParam (pid)) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void ResenhaProcessor::setParam (const juce::String& pid, float value)
{
    if (auto* rp = apvts.getParameter (pid))
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void ResenhaProcessor::loadPreset (int index)
{
    refreshUserPresets();
    const int total = (int) entries.size();
    if (total == 0) return;
    index = ((index % total) + total) % total;
    const auto& e = entries[(size_t) index];
    pushHistory();

    if (e.preset != nullptr)
    {
        const auto& preset = *e.preset;
        applyValues ([&preset] (const juce::String& pid, float& v)
        {
            for (auto& kv : preset.values)
                if (pid == kv.first) { v = kv.second; return true; }
            return false;
        });
        if (preset.phrase.isNotEmpty())
            parsePattern (preset.phrase, pattern);
    }
    else
    {
        auto xml = juce::parseXML (e.file);
        if (xml == nullptr) return;
        auto tree = juce::ValueTree::fromXml (*xml);
        applyValues ([&tree] (const juce::String& pid, float& v)
        {
            auto child = tree.getChildWithProperty ("id", pid);
            if (! child.isValid() || ! child.hasProperty ("value")) return false;
            v = (float) child.getProperty ("value");
            return true;
        });
        if (tree.hasProperty ("pattern"))
            pattern.fromString (tree.getProperty ("pattern").toString());
    }

    presetIndex = index;
    presetName  = e.name;
    presetBank  = bankKey (e.bank);
    ++patternVersion;
}

juce::String ResenhaProcessor::importBank (const juce::File& file, juce::String& bankName)
{
    std::vector<PresetData> presets;
    const auto err = parseBank (file, bankName, presets);
    if (err.isNotEmpty()) return err;
    if (bankName.equalsIgnoreCase (factoryBankName) || bankName.equalsIgnoreCase (userBankKey))
        return u8 ("Esse nome de banco é reservado do RESENHA.");

    auto dir = bankDir();
    dir.createDirectory();
    for (auto& b : banks)
        if (b.name.equalsIgnoreCase (bankName) && b.file != file)
            b.file.deleteFile();

    auto target = dir.getChildFile (juce::File::createLegalFileName (bankName) + bankExt);
    if (target != file && ! file.copyFileTo (target))
        return u8 ("Não deu para copiar o banco para a pasta do RESENHA.");

    refreshBanks();
    presetIndex = findPreset (presetName, presetBank);
    return {};
}

void ResenhaProcessor::removeBank (int bank)
{
    if (bank < 1 || bank > (int) banks.size()) return;
    auto file = banks[(size_t) bank - 1].file;
    if (! file.moveToTrash()) file.deleteFile();
    refreshBanks();
    presetIndex = findPreset (presetName, presetBank);
}

juce::File ResenhaProcessor::currentUserFile() const
{
    if (presetIndex < 0 || presetIndex >= (int) entries.size()) return {};
    const auto& e = entries[(size_t) presetIndex];
    if (e.bank != bankUser || e.name != presetName) return {};
    return e.file.existsAsFile() ? e.file : juce::File();
}

bool ResenhaProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String ResenhaProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto nm = "Minha resenha " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (nm + ".xml").existsAsFile()) return nm;
    }
    return "Minha resenha";
}

void ResenhaProcessor::writePreset (const juce::File& file)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", pattern.toString(), nullptr);
    if (auto xml = state.createXml())
        xml->writeTo (file);
}

void ResenhaProcessor::saveUserPreset (const juce::String& name)
{
    const auto base = juce::File::createLegalFileName (name.trim());
    if (base.isEmpty()) return;
    auto dir = userPresetDir();
    dir.createDirectory();

    auto file = dir.getChildFile (base + ".xml");
    for (int i = 2; file.existsAsFile() && i < 1000; ++i)
        file = dir.getChildFile (base + " (" + juce::String (i) + ").xml");
    writePreset (file);

    refreshUserPresets();
    presetName  = file.getFileNameWithoutExtension();
    presetBank  = userBankKey;
    presetIndex = findPreset (presetName, presetBank);
}

void ResenhaProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool ResenhaProcessor::renameUserPreset (const juce::String& newName)
{
    const auto file = currentUserFile();
    const auto base = juce::File::createLegalFileName (newName.trim());
    if (! file.existsAsFile() || base.isEmpty()) return false;

    const auto target = file.getSiblingFile (base + ".xml");
    const bool sameFile = target.getFullPathName().equalsIgnoreCase (file.getFullPathName());
    if ((target.existsAsFile() && ! sameFile) || ! file.moveFileTo (target)) return false;

    refreshUserPresets();
    presetName  = base;
    presetIndex = findPreset (presetName, presetBank);
    return true;
}

void ResenhaProcessor::deleteUserPreset()
{
    auto file = currentUserFile();
    if (! file.existsAsFile()) return;
    if (! file.moveToTrash()) file.deleteFile();
    refreshUserPresets();
    presetName  = "Sem nome";
    presetBank  = {};
    presetIndex = -1;
}

// ---------------- gerador ----------------

void ResenhaProcessor::pushHistory()
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

void ResenhaProcessor::undo()
{
    if (history.empty()) return;
    const auto snap = history.back();
    history.pop_back();
    for (const auto& kv : snap.values) setParam (kv.first, kv.second);
    pattern.fromString (snap.pattern);
    ++patternVersion;
}

// sorteio de um timbre com cara de lead de psy: razões inteiras (metálico, "falado"), FM no ataque,
// pitch env e glide de vez em quando, e cada timbre puxado para um lado do estéreo
void ResenhaProcessor::randomizeTimbre (int v)
{
    auto pick = [this] (std::initializer_list<int> l) { return *(l.begin() + rnd.nextInt ((int) l.size())); };
    auto logRange = [this] (float lo, float hi) { return lo * std::pow (hi / lo, rnd.nextFloat()); };
    auto set = [this, v] (const juce::String& name, float value) { setParam (id::v (v, name), value); };

    set (id::wave,  (float) pick ({ 0, 0, 1, 2, 2, 3 }));
    set (id::ratio, (float) pick ({ 1, 3, 3, 5, 7, 8, 9, 10, 11, 12, 0, 2, 4 }));
    set (id::fm,    25.0f + rnd.nextFloat() * 60.0f);
    set (id::fb,    rnd.nextFloat() * rnd.nextFloat() * 70.0f);
    set (id::fmDec, logRange (40.0f, 700.0f));
    set (id::cut,   logRange (1200.0f, 9000.0f));
    set (id::res,   5.0f + rnd.nextFloat() * 55.0f);
    set (id::dec,   logRange (120.0f, 650.0f));
    set (id::pan,   (v % 2 == 0 ? -1.0f : 1.0f) * (10.0f + rnd.nextFloat() * 40.0f));
    set (id::glide, rnd.nextInt (2) == 0 ? 0.0f : logRange (20.0f, 140.0f));
    set (id::pEnv,  rnd.nextInt (5) < 2 ? 0.0f : (float) (pick ({ 12, 7, 5, 3, 24 }) * (rnd.nextInt (4) == 0 ? -1 : 1)));
    set (id::fEnv,  rnd.nextFloat() * 70.0f);
    // mergulho no fim da fala: quase sempre para baixo (o lead "cai"), às vezes sobe
    set (id::bend,  rnd.nextInt (5) < 2 ? 0.0f : (float) (pick ({ -12, -12, -7, -24, -5, 12, 7 })));
}

// GERAR CONVERSA também sorteia o jeito da conversa (a quantidade de vozes é escolha da pessoa e fica)
void ResenhaProcessor::randomizeConversation()
{
    static const int everyIdx[] = { 1, 2, 3, 3, 4, 5 };
    static const int resp[] = { 1, 1, 2, 2, 3, 0 };
    static const int itv[] = { 0, 1, 1, 2, 3 };
    setParam (id::every,    (float) everyIdx[rnd.nextInt (6)]);
    setParam (id::response, (float) resp[rnd.nextInt (6)]);
    setParam (id::interval, (float) itv[rnd.nextInt (5)]);
    setParam (id::order,    (float) rnd.nextInt (3));
    setParam (id::contour,  (float) rnd.nextInt (4));
}

void ResenhaProcessor::generate (int parts, bool respectLocks)
{
    pushHistory();
    auto locked = [this, respectLocks] (const juce::String& pid)
    {
        return respectLocks && apvts.getRawParameterValue (pid)->load() > 0.5f;
    };

    // GERAR CONVERSA sorteia antes o jeito da conversa (o CONTORNO vale já para as notas desta vez)
    if (parts == partAll)
        randomizeConversation();

    // sorteia os 64 passos: aumentar os COMPASSOS depois mostra frase nova, não silêncio
    if ((parts & partNotes) != 0 && ! locked (id::lockNotes))
        generateNotes (pattern, maxSteps, choice (pScale), juce::roundToInt (pOct->load()), rnd,
                       (Contour) juce::jlimit (0, 3, choice (pContour)));
    if ((parts & partRhythm) != 0 && ! locked (id::lockRhythm))
        generateRhythm (pattern, maxSteps, pDens->load() * 0.01f, rnd);
    if ((parts & partGate) != 0 && ! locked (id::lockGate))
        generateGate (pattern, maxSteps, pGate->load() * 0.01f, pGateVar->load() * 0.01f, rnd);
    if ((parts & partTimbres) != 0)
        for (int v = 0; v < numVoices; ++v)
            if (! locked (id::lockVoice (v))) randomizeTimbre (v);
    ++patternVersion;
}

void ResenhaProcessor::randomizeVoice (int voice)
{
    pushHistory();
    randomizeTimbre (juce::jlimit (0, numVoices - 1, voice));
}

// a frase como .mid: um canal por timbre (A = 1 … D = 4), na altura escrita (Dó 3 = 60)
juce::File ResenhaProcessor::writeMidiFile()
{
    Phrase ph;
    buildPhrase (pattern, phraseParams(), ph);
    const int tpq = 960;
    const double stepTicks = rateBeats[juce::jlimit (0, 4, choice (pRate))] * tpq;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jmax (20.0f, shownBpm.load()))), 0.0);
    for (int i = 0; i < ph.count; ++i)
    {
        const auto& pn = ph.notes[(size_t) i];
        const double on  = pn.step * stepTicks;
        const double off = on + juce::jmax (10.0, stepTicks * ((double) (pn.len - 1) + (double) pn.gate));
        const int ch = pn.voice + 1;
        seq.addEvent (juce::MidiMessage::noteOn (ch, pn.note, (juce::uint8) (pn.accent ? 127 : 96)), on);
        seq.addEvent (juce::MidiMessage::noteOff (ch, pn.note), off);
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), ph.steps * stepTicks);
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);
    mf.addTrack (seq);

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RESENHA");
    dir.createDirectory();
    auto name = juce::File::createLegalFileName (presetName.trim());
    auto file = dir.getChildFile ("RESENHA - " + (name.isEmpty() ? juce::String ("frase") : name) + ".mid");
    file.deleteFile();
    juce::FileOutputStream os (file);
    if (os.openedOk()) mf.writeTo (os);
    return file;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResenhaProcessor();
}
