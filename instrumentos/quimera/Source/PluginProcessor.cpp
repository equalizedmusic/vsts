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
    refreshBanks();
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
    state.setProperty ("presetBank", presetBank, nullptr);
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
            presetBank  = tree.getProperty ("presetBank", juce::String()).toString();
            presetIndex = findPreset (presetName, presetBank);
            if (presetIndex < 0 && ! tree.hasProperty ("presetBank"))   // projeto de antes dos bancos
            {
                presetIndex = getPresetNames().indexOf (presetName);
                if (presetIndex >= 0) presetBank = bankKey (entries[(size_t) presetIndex].bank);
            }
            uiScale     = juce::jlimit (0.75f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
        }
    }
}

// ---------------- presets ----------------
//
// Bancos avulsos: arquivo .qmrbank (XML) que a pessoa baixa e importa pelo menu de presets.
//   <QUIMERABANK format="1" name="Factory 2" author="InsideStudioSP">
//     <PRESET name="Grave Novo" category="BASS">
//       <PARAM id="facut" value="220"/>          (valor REAL, igual ao estado salvo pelo plugin)
//     </PRESET>
//   </QUIMERABANK>
// category = nome em categoryNames (ou o índice); sem categoria ou desconhecida = OUTROS.
// O que o preset não traz volta ao padrão; mpe, mpeBend e hold são ignorados (performance).

namespace
{
    const juce::String bankExt = ".qmrbank";
    const juce::String bankTag = "QUIMERABANK";
    const juce::String factoryBankName = "FACTORY 1";
    const juce::String userBankKey = "USER";   // presetBank salvo no projeto para os presets do usuário

    // lê um banco; devolve vazio se deu certo, senão o motivo
    juce::String parseBank (const juce::File& file, juce::String& name, std::vector<quimera::FactoryPreset>& out)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr || ! xml->hasTagName (bankTag))
            return juce::String::fromUTF8 ("O arquivo não é um banco de presets do QUIMERA.");
        if (xml->getIntAttribute ("format", 1) > 1)
            return juce::String::fromUTF8 ("Esse banco é de uma versão mais nova do QUIMERA. Atualize o plugin.");

        name = xml->getStringAttribute ("name").trim();
        if (name.isEmpty()) name = file.getFileNameWithoutExtension().trim();

        out.clear();
        for (auto* pe : xml->getChildWithTagNameIterator ("PRESET"))
        {
            quimera::FactoryPreset p;
            p.name = pe->getStringAttribute ("name").trim();
            if (p.name.isEmpty()) continue;

            const auto cat = pe->getStringAttribute ("category").trim();
            p.category = quimera::numCategories;
            for (int c = 0; c < quimera::numCategories; ++c)
                if (cat.equalsIgnoreCase (quimera::categoryNames[c])) p.category = c;
            if (cat.containsOnly ("0123456789") && cat.isNotEmpty())
                p.category = juce::jlimit (0, quimera::numCategories, cat.getIntValue());

            for (auto* v : pe->getChildWithTagNameIterator ("PARAM"))
                if (v->hasAttribute ("id") && v->hasAttribute ("value"))
                    p.values.emplace_back (v->getStringAttribute ("id"), (float) v->getDoubleAttribute ("value"));
            out.push_back (std::move (p));
        }
        if (out.empty())
            return juce::String::fromUTF8 ("O banco não tem nenhum preset.");
        return {};
    }
}

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

juce::File QuimeraProcessor::bankDir() const
{
   #if JUCE_MAC
    return userPresetDir().getChildFile ("Banks");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP/QUIMERA/Banks");
   #endif
}

void QuimeraProcessor::refreshBanks()
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

void QuimeraProcessor::refreshUserPresets()
{
    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    rebuildList();
}

void QuimeraProcessor::rebuildList()
{
    entries.clear();
    for (auto& p : factory)
        entries.push_back ({ 0, p.category, p.name, &p, {} });
    for (size_t b = 0; b < banks.size(); ++b)
        for (auto& p : banks[b].presets)
            entries.push_back ({ (int) b + 1, p.category, p.name, &p, {} });
    for (auto& f : userFiles)
        entries.push_back ({ bankUser, quimera::numCategories, f.getFileNameWithoutExtension(), nullptr, f });
}

juce::StringArray QuimeraProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& e : entries) names.add (e.name);
    return names;
}

int QuimeraProcessor::getPresetCategory (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].category;
    return numCategories;
}

int QuimeraProcessor::getPresetBank (int index) const
{
    if (index >= 0 && index < (int) entries.size()) return entries[(size_t) index].bank;
    return bankUser;
}

juce::StringArray QuimeraProcessor::getBankNames() const
{
    juce::StringArray names (factoryBankName);
    for (auto& b : banks) names.add (b.name);
    return names;
}

// como o banco fica salvo no projeto: vazio = FACTORY 1, USER ou o nome do banco importado
juce::String QuimeraProcessor::bankKey (int bank) const
{
    if (bank == bankUser) return userBankKey;
    if (bank >= 1 && bank <= (int) banks.size()) return banks[(size_t) bank - 1].name;
    return {};
}

// o mesmo nome pode existir em bancos diferentes: procura pelo par banco + nome
int QuimeraProcessor::findPreset (const juce::String& name, const juce::String& bank) const
{
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (e.name != name) continue;
        if (bankKey (e.bank) == bank) return (int) i;
    }
    return -1;
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
}

juce::String QuimeraProcessor::importBank (const juce::File& file, juce::String& bankName)
{
    std::vector<quimera::FactoryPreset> presets;
    const auto err = parseBank (file, bankName, presets);
    if (err.isNotEmpty()) return err;
    if (bankName.equalsIgnoreCase (factoryBankName) || bankName.equalsIgnoreCase (userBankKey))
        return juce::String::fromUTF8 ("Esse nome de banco é reservado do QUIMERA.");

    // um banco com o mesmo nome é substituído (versão nova do mesmo pacote)
    auto dir = bankDir();
    dir.createDirectory();
    for (auto& b : banks)
        if (b.name.equalsIgnoreCase (bankName) && b.file != file)
            b.file.deleteFile();

    auto target = dir.getChildFile (juce::File::createLegalFileName (bankName) + bankExt);
    if (target != file && ! file.copyFileTo (target))
        return juce::String::fromUTF8 ("Não deu para copiar o banco para a pasta do QUIMERA.");

    refreshBanks();
    presetIndex = findPreset (presetName, presetBank);
    return {};
}

void QuimeraProcessor::removeBank (int bank)
{
    if (bank < 1 || bank > (int) banks.size()) return;
    auto file = banks[(size_t) bank - 1].file;
    if (! file.moveToTrash()) file.deleteFile();

    // o som continua o mesmo no plugin; se era desse banco, só deixa de estar na lista
    refreshBanks();
    presetIndex = findPreset (presetName, presetBank);
}

juce::File QuimeraProcessor::currentUserFile() const
{
    if (presetIndex < 0 || presetIndex >= (int) entries.size()) return {};
    const auto& e = entries[(size_t) presetIndex];
    if (e.bank != bankUser || e.name != presetName) return {};
    return e.file.existsAsFile() ? e.file : juce::File();
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
    presetBank  = userBankKey;
    presetIndex = findPreset (presetName, presetBank);
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
    presetIndex = findPreset (presetName, presetBank);
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
    presetBank  = {};
    presetIndex = -1;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuimeraProcessor();
}
