#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vacina;

VacinaProcessor::VacinaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VACINA", createLayout())
{
    for (auto* prm : getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            raw[rp->getParameterID()] = apvts.getRawParameterValue (rp->getParameterID());
        prm->addListener (this);
    }

    factory = factoryPresetData();
    refreshBanks();
    refreshUserPresets();

    // abre num supersaw (o Init é uma serra crua)
    const int start = findPreset ("Dose Dupla", {});
    loadPreset (start >= 0 ? start : 0);
}

VacinaProcessor::~VacinaProcessor()
{
    for (auto* prm : getParameters())
        prm->removeListener (this);
}

void VacinaProcessor::parameterValueChanged (int parameterIndex, float)
{
    if (loading) return;
    lastTouched.store (parameterIndex);
    ++touchCount;
}

bool VacinaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void VacinaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int n = juce::jmax (1, samplesPerBlock);
    synth.prepare (sampleRate);
    fx.prepare (sampleRate, n);
    readSettings();
    volSm.reset (sampleRate, 0.05);
    volSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (get (id::volume)));
    arp.clear();
    arpNow.clear();
    arpLastStep = -1;
    arpGateOff = -1.0;
    arpPpq = 0.0;
    dcL = dcR = dcYL = dcYR = 0.0f;
}

void VacinaProcessor::readSettings()
{
    s.keyMode   = geti (id::keyMode);
    s.portaMs   = get (id::porta);
    s.bendRange = get (id::bendRange);
    s.drift     = get (id::drift) * 0.01f;
    s.unison    = geti (id::unison);
    s.uniDetune = get (id::uniDetune) * 0.01f;
    s.uniSpread = get (id::uniSpread) * 0.01f;

    s.model1  = geti (id::osc1Model);
    s.table1  = geti (id::osc1Table);
    s.shape1  = get (id::osc1Shape) * 0.01f;
    s.pw1     = get (id::osc1Pw) * 0.01f;
    s.semi1   = get (id::osc1Semi);
    s.spread1 = get (id::osc1Spread) * 0.01f;
    s.vol1    = get (id::osc1Vol) * 0.01f;
    s.model2  = geti (id::osc2Model);
    s.table2  = geti (id::osc2Table);
    s.shape2  = get (id::osc2Shape) * 0.01f;
    s.pw2     = get (id::osc2Pw) * 0.01f;
    s.semi2   = get (id::osc2Semi);
    s.detune2 = get (id::osc2Detune);
    s.sync2   = get (id::osc2Sync) > 0.5f;
    s.vol2    = get (id::osc2Vol) * 0.01f;
    s.fm      = get (id::fmAmt) * 0.01f;
    s.shape3  = get (id::osc3Shape) * 0.01f;
    s.semi3   = get (id::osc3Semi);
    s.detune3 = get (id::osc3Detune);
    s.vol3    = get (id::osc3Vol) * 0.01f;
    s.sub     = get (id::subVol) * 0.01f;
    s.oscVol  = get (id::oscVol) * 0.01f;
    s.noise   = get (id::noiseVol) * 0.01f;
    s.noiseColor = get (id::noiseColor) * 0.01f;
    s.ring    = get (id::ringVol) * 0.01f;

    s.cutoff  = get (id::cutoff);
    s.cutoff2 = get (id::cutoff2);
    s.filtBal = get (id::filtBal) * 0.01f;
    s.routing = geti (id::routing);
    s.f1Mode  = geti (id::f1Mode);
    s.f2Mode  = geti (id::f2Mode);
    s.reso1   = get (id::reso1) * 0.01f;
    s.reso2   = get (id::reso2) * 0.01f;
    s.env1    = get (id::env1) * 0.01f;
    s.env2    = get (id::env2) * 0.01f;
    s.key1    = get (id::key1) * 0.01f;
    s.key2    = get (id::key2) * 0.01f;
    s.satType = geti (id::satType);
    s.satAmt  = get (id::satAmt) * 0.01f;

    s.fA = get (id::fA); s.fD = get (id::fD); s.fS = get (id::fS) * 0.01f; s.fR = get (id::fR); s.fVel = get (id::fVel) * 0.01f;
    s.aA = get (id::aA); s.aD = get (id::aD); s.aS = get (id::aS) * 0.01f; s.aR = get (id::aR); s.aVel = get (id::aVel) * 0.01f;
    s.fSlope = get (id::fSlope) * 0.01f;
    s.aSlope = get (id::aSlope) * 0.01f;

    s.l1Rate  = get (id::lfo1Rate);  s.l1Shape = geti (id::lfo1Shape); s.l1Sync = geti (id::lfo1Sync);
    s.l1Pitch = get (id::lfo1Pitch) * 0.01f; s.l1Cut = get (id::lfo1Cut) * 0.01f;
    s.l1Pw    = get (id::lfo1Pw) * 0.01f;    s.l1Amp = get (id::lfo1Amp) * 0.01f;
    s.l2Rate  = get (id::lfo2Rate);  s.l2Shape = geti (id::lfo2Shape); s.l2Sync = geti (id::lfo2Sync);
    s.l2Shp   = get (id::lfo2Shp) * 0.01f; s.l2Fm = get (id::lfo2Fm) * 0.01f;
    s.l2Cut   = get (id::lfo2Cut) * 0.01f; s.l2Pan = get (id::lfo2Pan) * 0.01f;
    s.l3Rate  = get (id::lfo3Rate);  s.l3Shape = geti (id::lfo3Shape); s.l3Sync = geti (id::lfo3Sync);
    s.l3Dst   = geti (id::lfo3Dst);  s.l3Amt   = get (id::lfo3Amt) * 0.01f;

    for (int k = 0; k < numSlots; ++k)
    {
        s.mSrc[(size_t) k] = geti (id::mSrc (k));
        s.mDst[(size_t) k] = geti (id::mDst (k));
        s.mAmt[(size_t) k] = get (id::mAmt (k)) * 0.01f;
    }

    fxs.dlySend  = get (id::dlySend) * 0.01f;
    fxs.dlyTime  = geti (id::dlyTime);
    fxs.dlyFb    = get (id::dlyFb) * 0.01f;
    fxs.rvbSend  = get (id::rvbSend) * 0.01f;
    fxs.rvbSize  = get (id::rvbSize) * 0.01f;
    fxs.rvbDamp  = get (id::rvbDamp) * 0.01f;
    fxs.eqLow    = get (id::eqLow);
    fxs.eqMid    = get (id::eqMid);
    fxs.eqHigh   = get (id::eqHigh);
    fxs.distType = geti (id::distType);
    fxs.distAmt  = get (id::distAmt) * 0.01f;
    fxs.boostAmt = get (id::boostAmt) * 0.01f;
    fxs.boostTune = get (id::boostTune);
    fxs.choMix   = get (id::choMix) * 0.01f;
    fxs.choInt   = get (id::choInt) * 0.01f;
    fxs.phaMix   = get (id::phaMix) * 0.01f;
    fxs.phaInt   = get (id::phaInt) * 0.01f;
}

void VacinaProcessor::arpNotesOff()
{
    for (auto& nv : arpNow) synth.noteOff (nv.first, s);
    arpNow.clear();
    arpGateOff = -1.0;
}

// teclado: com o ARP ligado as notas vão para o arpejador; sem ele, direto para as vozes
void VacinaProcessor::handleMidi (const juce::MidiMessage& m, bool arpOn)
{
    const bool hold = get (id::arpHold) > 0.5f;
    if (m.isNoteOn())
    {
        ++midiActivity;
        if (arpOn)
        {
            if (! arp.hasNotes()) { arpPpq = 0.0; arpLastStep = -1; arp.restart(); }   // o primeiro passo sai na hora
            arp.noteOn (m.getNoteNumber(), m.getVelocity(), hold);
        }
        else
            synth.noteOn (m.getNoteNumber(), m.getFloatVelocity(), s);
    }
    else if (m.isNoteOff())
    {
        if (arpOn) arp.noteOff (m.getNoteNumber(), hold);
        else       synth.noteOff (m.getNoteNumber(), s);
    }
    else if (m.isPitchWheel())
        synth.setBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isController() && m.getControllerNumber() == 1)
        synth.setWheel ((float) m.getControllerValue() / 127.0f);
    else if (m.isController() && m.getControllerNumber() == 64)
        synth.setSustain (m.getControllerValue() >= 64);
    else if (m.isChannelPressure())
        synth.setAfter ((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        synth.setAfter ((float) m.getAfterTouchValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        arp.clear();
        arpNotesOff();
        synth.allNotesOff();
    }
}

void VacinaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0 || buffer.getNumChannels() < 2) return;

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
    const bool running = playing && havePpq;
    synced.store (running);

    readSettings();
    fxs.bpm = bpm;
    const double pps = bpm / 60.0 / currentRate;

    // ---- arpejador: liga/desliga e HOLD ----
    const bool arpOn = get (id::arpOn) > 0.5f;
    const bool hold  = get (id::arpHold) > 0.5f;
    if (arpOn != arpWasOn)
    {
        arpNotesOff();
        arp.clear();
        if (! arpOn) synth.allNotesOff();
        arpWasOn = arpOn;
    }
    if (arpHoldWas && ! hold) arp.releaseHold();
    arpHoldWas = hold;

    const double rb   = arpRateBeats[juce::jlimit (0, 5, geti (id::arpRate))];
    const double gate = juce::jlimit (0.05, 1.0, (double) get (id::arpGate) * 0.01);
    const int    mode = geti (id::arpMode), octaves = juce::jlimit (1, 4, geti (id::arpOct));

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    int rendered = 0;
    auto flush = [&] (int upTo)
    {
        if (upTo > rendered)
        {
            synth.render (l + rendered, r + rendered, upTo - rendered, s, bpm, running, ppq0 + pps * rendered, pps);
            rendered = upTo;
        }
    };

    auto it = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.cend() && (*it).samplePosition <= i)
        {
            flush (i);
            handleMidi ((*it).getMessage(), arpOn);
            ++it;
        }
        if (! arpOn) continue;

        // ---- passo do arpejo: no tempo do host tocando; parado, num relógio próprio no BPM do projeto ----
        const double p = running ? ppq0 + pps * (double) i : arpPpq;
        if (! running) arpPpq += pps;
        if (! arp.hasNotes())
        {
            if (! arpNow.empty()) { flush (i); arpNotesOff(); }
            arpLastStep = -1;
            continue;
        }
        const long long step = (long long) std::floor (p / rb + 1.0e-9);
        if (step != arpLastStep)
        {
            flush (i);
            arpNotesOff();
            arp.next (mode, octaves, arpRnd, arpNext);
            for (auto& nv : arpNext) synth.noteOn (nv.first, (float) nv.second / 127.0f, s);
            arpNow = arpNext;
            arpGateOff = ((double) step + gate) * rb;
            arpLastStep = step;
        }
        if (arpGateOff >= 0.0 && p >= arpGateOff && gate < 0.999)
        {
            flush (i);
            arpNotesOff();
        }
    }
    flush (n);
    for (; it != midi.cend(); ++it)
        handleMidi ((*it).getMessage(), arpOn);

    // efeitos, DC e volume
    fx.process (l, r, n, fxs);
    volSm.setTargetValue (juce::Decibels::decibelsToGain (get (id::volume)));
    const float R = 1.0f - juce::MathConstants<float>::twoPi * 8.0f / (float) currentRate;
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = volSm.getNextValue();
        const float yl = l[i] - dcL + R * dcYL;  dcL = l[i];  dcYL = yl;
        const float yr = r[i] - dcR + R * dcYR;  dcR = r[i];  dcYR = yr;
        l[i] = juce::jlimit (-2.0f, 2.0f, yl * g);
        r[i] = juce::jlimit (-2.0f, 2.0f, yr * g);
        pl = juce::jmax (pl, std::abs (l[i]));
        pr = juce::jmax (pr, std::abs (r[i]));
    }
    meterL.store (juce::jmax (meterL.load(), pl));
    meterR.store (juce::jmax (meterR.load(), pr));
}

juce::AudioProcessorEditor* VacinaProcessor::createEditor()
{
    return new VacinaEditor (*this);
}

// ---------------- estado ----------------

void VacinaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetBank", presetBank, nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VacinaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const juce::ScopedValueSetter<bool> quiet (loading, true);
            apvts.replaceState (tree);
            presetName  = tree.getProperty ("presetName", presetName).toString();
            presetBank  = tree.getProperty ("presetBank", juce::String()).toString();
            presetIndex = findPreset (presetName, presetBank);
            uiScale     = juce::jlimit (0.6f, 1.8f, (float) tree.getProperty ("uiScale", uiScale.load()));
        }
}

// ---------------- presets ----------------
//
// Bancos avulsos: arquivo .vcnbank (XML) que a pessoa baixa e importa pelo menu de presets.
//   <VACINABANK format="1" name="Factory 2" author="InsideStudioSP">
//     <PRESET name="Hino" category="SUPERSAW">
//       <PARAM id="cutoff" value="2400"/>          (valor REAL, igual ao estado salvo pelo plugin)
//     </PRESET>
//   </VACINABANK>
// category = nome em categoryNames (ou o índice); sem categoria ou desconhecida = OUTROS.
// O que o preset não traz volta ao padrão.

namespace
{
    const juce::String bankExt = ".vcnbank";
    const juce::String bankTag = "VACINABANK";
    const juce::String factoryBankName = "FACTORY 1";
    const juce::String userBankKey = "USER";   // presetBank salvo no projeto para os presets do usuário

    // lê um banco; devolve vazio se deu certo, senão o motivo
    juce::String parseBank (const juce::File& file, juce::String& name, std::vector<PresetData>& out)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr || ! xml->hasTagName (bankTag))
            return u8 ("O arquivo não é um banco de presets do VACINA.");
        if (xml->getIntAttribute ("format", 1) > 1)
            return u8 ("Esse banco é de uma versão mais nova do VACINA. Atualize o plugin.");

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
            out.push_back (std::move (p));
        }
        if (out.empty())
            return u8 ("O banco não tem nenhum preset.");
        return {};
    }
}

juce::File VacinaProcessor::userPresetDir() const
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/InsideStudioSP/VACINA");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/VACINA/Presets");
   #endif
}

juce::File VacinaProcessor::bankDir() const
{
   #if JUCE_MAC
    return userPresetDir().getChildFile ("Banks");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/VACINA/Banks");
   #endif
}

void VacinaProcessor::refreshBanks()
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

void VacinaProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    rebuildList();
}

void VacinaProcessor::rebuildList()
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

juce::StringArray VacinaProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& e : entries) names.add (e.name);
    return names;
}

int VacinaProcessor::getPresetCategory (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].category;
    return numCategories;
}

int VacinaProcessor::getPresetBank (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].bank;
    return bankUser;
}

juce::StringArray VacinaProcessor::getBankNames() const
{
    juce::StringArray names (factoryBankName);
    for (auto& b : banks) names.add (b.name);
    return names;
}

juce::String VacinaProcessor::getPresetGroup() const
{
    if (presetIndex < 0 || presetIndex >= (int) entries.size()) return "EDITADO";
    const auto& e = entries[(size_t) presetIndex];
    if (e.bank == bankUser) return userBankKey;
    const auto cat = e.category < 0 ? juce::String ("INIT")
                   : e.category < numCategories ? u8 (categoryNames[e.category]) : juce::String ("OUTROS");
    return e.bank == 0 ? cat : bankKey (e.bank).toUpperCase() + " / " + cat;
}

// como o banco fica salvo no projeto: vazio = FACTORY 1, USER ou o nome do banco importado
juce::String VacinaProcessor::bankKey (int bank) const
{
    if (bank == bankUser) return userBankKey;
    if (bank >= 1 && bank <= (int) banks.size()) return banks[(size_t) bank - 1].name;
    return {};
}

// o mesmo nome pode existir em bancos diferentes: procura pelo par banco + nome
int VacinaProcessor::findPreset (const juce::String& name, const juce::String& bank) const
{
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (e.name == name && bankKey (e.bank) == bank) return (int) i;
    }
    return -1;
}

void VacinaProcessor::applyValues (const std::function<bool (const juce::String&, float&)>& lookup)
{
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (rp == nullptr) continue;
        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        lookup (rp->getParameterID(), value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void VacinaProcessor::loadPreset (int index)
{
    refreshUserPresets();
    const int total = (int) entries.size();
    if (total == 0) return;
    index = ((index % total) + total) % total;
    const auto& e = entries[(size_t) index];
    const juce::ScopedValueSetter<bool> quiet (loading, true);

    if (e.preset != nullptr)
    {
        const auto& preset = *e.preset;
        applyValues ([&preset] (const juce::String& pid, float& v)
        {
            for (auto& kv : preset.values)
                if (pid == kv.first) { v = kv.second; return true; }
            return false;
        });
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
    }

    presetIndex = index;
    presetName  = e.name;
    presetBank  = bankKey (e.bank);
    lastTouched = -1;
    ++touchCount;
}

juce::String VacinaProcessor::importBank (const juce::File& file, juce::String& bankName)
{
    std::vector<PresetData> presets;
    const auto err = parseBank (file, bankName, presets);
    if (err.isNotEmpty()) return err;
    if (bankName.equalsIgnoreCase (factoryBankName) || bankName.equalsIgnoreCase (userBankKey))
        return u8 ("Esse nome de banco é reservado do VACINA.");

    // um banco com o mesmo nome é substituído (versão nova do mesmo pacote)
    auto dir = bankDir();
    dir.createDirectory();
    for (auto& b : banks)
        if (b.name.equalsIgnoreCase (bankName) && b.file != file)
            b.file.deleteFile();

    auto target = dir.getChildFile (juce::File::createLegalFileName (bankName) + bankExt);
    if (target != file && ! file.copyFileTo (target))
        return u8 ("Não deu para copiar o banco para a pasta do VACINA.");

    refreshBanks();
    presetIndex = findPreset (presetName, presetBank);
    return {};
}

void VacinaProcessor::removeBank (int bank)
{
    if (bank < 1 || bank > (int) banks.size()) return;
    auto file = banks[(size_t) bank - 1].file;
    if (! file.moveToTrash()) file.deleteFile();

    // o som continua o mesmo no plugin; se era desse banco, só deixa de estar na lista
    refreshBanks();
    presetIndex = findPreset (presetName, presetBank);
}

juce::File VacinaProcessor::currentUserFile() const
{
    if (presetIndex < 0 || presetIndex >= (int) entries.size()) return {};
    const auto& e = entries[(size_t) presetIndex];
    if (e.bank != bankUser || e.name != presetName) return {};
    return e.file.existsAsFile() ? e.file : juce::File();
}

bool VacinaProcessor::isUserPreset() const
{
    return currentUserFile().existsAsFile();
}

juce::String VacinaProcessor::suggestPresetName() const
{
    const auto dir = userPresetDir();
    for (int i = 1; i < 1000; ++i)
    {
        const auto nm = "Meu preset " + juce::String (i).paddedLeft ('0', 2);
        if (! dir.getChildFile (nm + ".xml").existsAsFile()) return nm;
    }
    return "Meu preset";
}

void VacinaProcessor::writePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);
}

void VacinaProcessor::saveUserPreset (const juce::String& name)
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

void VacinaProcessor::overwriteUserPreset()
{
    const auto file = currentUserFile();
    if (file.existsAsFile()) writePreset (file);
}

bool VacinaProcessor::renameUserPreset (const juce::String& newName)
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

void VacinaProcessor::deleteUserPreset()
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VacinaProcessor();
}
