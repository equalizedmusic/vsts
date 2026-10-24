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

    factory = factoryPresetData();
    refreshBanks();
    refreshUserPresets();

    // abre no "303 Clássico" (o Init é só uma linha de Dó)
    const int start = findPreset (u8 ("303 Clássico"), {});
    loadPreset (start >= 0 ? start : 0);
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
    state.setProperty ("presetBank", presetBank, nullptr);
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
            presetBank  = tree.getProperty ("presetBank", juce::String()).toString();
            presetIndex = findPreset (presetName, presetBank);
            uiScale     = juce::jlimit (0.75f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
            ++patternVersion;
        }
}

// ---------------- presets ----------------
//
// Bancos avulsos: arquivo .lmobank (XML) que a pessoa baixa e importa pelo menu de presets.
//   <LIMAOBANK format="1" name="Factory 2" author="InsideStudioSP">
//     <PRESET name="Linha Nova" category="ACID HOUSE">
//       <PARAM id="cutoff" value="460"/>          (valor REAL, igual ao estado salvo pelo plugin)
//       <PATTERN notes="0 0 12 - 3 ~ 0 15" acc="x...x..." sld="..x....."/>   (mesmo texto dos presets de fábrica)
//     </PRESET>
//   </LIMAOBANK>
// category = nome em categoryNames (ou o índice); sem categoria ou desconhecida = OUTROS.
// O que o preset não traz volta ao padrão; o liga/desliga do sequenciador é performance e não muda.
// O número de passos vem do padrão (sem PATTERN, o padrão atual fica e o comprimento vem do PARAM seqlen).

namespace
{
    const juce::String bankExt = ".lmobank";
    const juce::String bankTag = "LIMAOBANK";
    const juce::String factoryBankName = "FACTORY 1";
    const juce::String userBankKey = "USER";   // presetBank salvo no projeto para os presets do usuário

    // lê um banco; devolve vazio se deu certo, senão o motivo
    juce::String parseBank (const juce::File& file, juce::String& name, std::vector<PresetData>& out)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr || ! xml->hasTagName (bankTag))
            return u8 ("O arquivo não é um banco de presets do LIMÃO.");
        if (xml->getIntAttribute ("format", 1) > 1)
            return u8 ("Esse banco é de uma versão mais nova do LIMÃO. Atualize o plugin.");

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
            if (auto* pat = pe->getChildByName ("PATTERN"))
            {
                p.notes = pat->getStringAttribute ("notes").trim();
                p.acc   = pat->getStringAttribute ("acc").trim();
                p.sld   = pat->getStringAttribute ("sld").trim();
            }
            out.push_back (std::move (p));
        }
        if (out.empty())
            return u8 ("O banco não tem nenhum preset.");
        return {};
    }
}

juce::File LimaoProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/LIMAO");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/LIMAO/Presets");
   #endif
}

juce::File LimaoProcessor::bankDir() const
{
   #if JUCE_MAC
    return userPresetDir().getChildFile ("Banks");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/LIMAO/Banks");
   #endif
}

void LimaoProcessor::refreshBanks()
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

void LimaoProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    rebuildList();
}

void LimaoProcessor::rebuildList()
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

juce::StringArray LimaoProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& e : entries) names.add (e.name);
    return names;
}

int LimaoProcessor::getPresetCategory (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].category;
    return numCategories;
}

int LimaoProcessor::getPresetBank (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].bank;
    return bankUser;
}

juce::StringArray LimaoProcessor::getBankNames() const
{
    juce::StringArray names (factoryBankName);
    for (auto& b : banks) names.add (b.name);
    return names;
}

// como o banco fica salvo no projeto: vazio = FACTORY 1, USER ou o nome do banco importado
juce::String LimaoProcessor::bankKey (int bank) const
{
    if (bank == bankUser) return userBankKey;
    if (bank >= 1 && bank <= (int) banks.size()) return banks[(size_t) bank - 1].name;
    return {};
}

// o mesmo nome pode existir em bancos diferentes: procura pelo par banco + nome
int LimaoProcessor::findPreset (const juce::String& name, const juce::String& bank) const
{
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (e.name == name && bankKey (e.bank) == bank) return (int) i;
    }
    return -1;
}

void LimaoProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (pid == id::seqOn) continue;      // performance: o preset não liga nem desliga o sequenciador

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (pid, value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

// padrão em texto (fábrica e bancos): o número de passos vira o comprimento do sequenciador
void LimaoProcessor::applyPattern (const juce::String& notes, const juce::String& acc, const juce::String& sld)
{
    if (notes.isEmpty()) return;
    const int steps = parsePattern (notes, acc, sld, pattern);
    if (auto* lenP = apvts.getParameter (id::seqLen))
        lenP->setValueNotifyingHost (lenP->convertTo0to1 ((float) steps));
}

void LimaoProcessor::loadPreset (int index)
{
    refreshUserPresets();
    const int total = (int) entries.size();
    if (total == 0) return;
    index = ((index % total) + total) % total;
    const auto& e = entries[(size_t) index];

    if (e.preset != nullptr)
    {
        const auto& preset = *e.preset;
        applyValues ([&preset] (const juce::String& pid, float& v)
        {
            for (auto& kv : preset.values)
                if (pid == kv.first) { v = kv.second; return true; }
            return false;
        });
        applyPattern (preset.notes, preset.acc, preset.sld);
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

juce::String LimaoProcessor::importBank (const juce::File& file, juce::String& bankName)
{
    std::vector<PresetData> presets;
    const auto err = parseBank (file, bankName, presets);
    if (err.isNotEmpty()) return err;
    if (bankName.equalsIgnoreCase (factoryBankName) || bankName.equalsIgnoreCase (userBankKey))
        return u8 ("Esse nome de banco é reservado do LIMÃO.");

    // um banco com o mesmo nome é substituído (versão nova do mesmo pacote)
    auto dir = bankDir();
    dir.createDirectory();
    for (auto& b : banks)
        if (b.name.equalsIgnoreCase (bankName) && b.file != file)
            b.file.deleteFile();

    auto target = dir.getChildFile (juce::File::createLegalFileName (bankName) + bankExt);
    if (target != file && ! file.copyFileTo (target))
        return u8 ("Não deu para copiar o banco para a pasta do LIMÃO.");

    refreshBanks();
    presetIndex = findPreset (presetName, presetBank);
    return {};
}

void LimaoProcessor::removeBank (int bank)
{
    if (bank < 1 || bank > (int) banks.size()) return;
    auto file = banks[(size_t) bank - 1].file;
    if (! file.moveToTrash()) file.deleteFile();

    // o som continua o mesmo no plugin; se era desse banco, só deixa de estar na lista
    refreshBanks();
    presetIndex = findPreset (presetName, presetBank);
}

juce::File LimaoProcessor::currentUserFile() const
{
    if (presetIndex < 0 || presetIndex >= (int) entries.size()) return {};
    const auto& e = entries[(size_t) presetIndex];
    if (e.bank != bankUser || e.name != presetName) return {};
    return e.file.existsAsFile() ? e.file : juce::File();
}

bool LimaoProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String LimaoProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto nm = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (nm + ".xml").existsAsFile()) return nm;
    }
    return "Meu preset";
}

// o preset do usuário é o estado do plugin (parâmetros) com o padrão do sequenciador junto
void LimaoProcessor::writePreset (const juce::File& file)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", pattern.toString(), nullptr);
    if (auto xml = state.createXml())
        xml->writeTo (file);
}

void LimaoProcessor::saveUserPreset (const juce::String& name)
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
    presetBank  = userBankKey;
    presetIndex = findPreset (presetName, presetBank);
}

void LimaoProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool LimaoProcessor::renameUserPreset (const juce::String& newName)
{
    const auto file = currentUserFile();
    const auto base = juce::File::createLegalFileName (newName.trim());
    if (! file.existsAsFile() || base.isEmpty()) return false;

    const auto target = file.getSiblingFile (base + ".xml");
    const bool sameFile = target.getFullPathName().equalsIgnoreCase (file.getFullPathName());   // só maiúscula/minúscula
    if ((target.existsAsFile() && ! sameFile) || ! file.moveFileTo (target)) return false;

    refreshUserPresets();
    presetName  = base;
    presetIndex = findPreset (presetName, presetBank);
    return true;
}

void LimaoProcessor::deleteUserPreset()
{
    auto file = currentUserFile();
    if (! file.existsAsFile()) return;
    if (! file.moveToTrash()) file.deleteFile();

    // o som continua o mesmo no plugin; só deixa de estar salvo
    refreshUserPresets();
    presetName  = "Sem nome";
    presetBank  = {};
    presetIndex = -1;
}

// ---------------- padrão ----------------

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
