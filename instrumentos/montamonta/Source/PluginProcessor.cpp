#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mm;

namespace
{
// notas do exemplo sonoro de cada peça (segundos, nota MIDI)
struct DemoNote { double start, end; int note; };

const std::vector<DemoNote>& demoScript (int module)
{
    static const std::vector<DemoNote> held48 { { 0.0, 3.0, 48 } };
    static const std::vector<DemoNote> vcfNotes { { 0.0, 3.0, 45 } };
    static const std::vector<DemoNote> ampNotes { { 0.0, 0.15, 48 }, { 0.4, 0.55, 52 }, { 0.8, 0.95, 55 }, { 1.3, 2.4, 48 } };
    static const std::vector<DemoNote> modNotes { { 0.0, 0.3, 36 }, { 0.5, 0.8, 36 }, { 1.0, 1.3, 48 }, { 1.5, 1.8, 36 }, { 2.0, 2.3, 43 } };
    switch (module)
    {
        case vcf:    return vcfNotes;
        case envAmp: return ampNotes;
        case envMod: return modNotes;
        default:     return held48;
    }
}
constexpr double demoLength = 3.3;
} // namespace

MontaMontaProcessor::MontaMontaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MONTAMONTA", createLayout())
{
    auto raw = [this] (const juce::String& pid) { return apvts.getRawParameterValue (pid); };
    pv1Range = raw (id::vco1Range); pv1Wave = raw (id::vco1Wave); pv1Tune = raw (id::vco1Tune);
    pv2Range = raw (id::vco2Range); pv2Wave = raw (id::vco2Wave); pv2Tune = raw (id::vco2Tune);
    pMix1 = raw (id::mixOsc1);      pMix2 = raw (id::mixOsc2);    pMixN = raw (id::mixNoise);
    pMix1On = raw (id::mixOsc1On);  pMix2On = raw (id::mixOsc2On); pMixNOn = raw (id::mixNoiseOn);
    pCut = raw (id::vcfCutoff);     pEmph = raw (id::vcfEmph);    pCont = raw (id::vcfContour);
    pAA = raw (id::ampA); pAD = raw (id::ampD); pAS = raw (id::ampS); pAR = raw (id::ampR);
    pMA = raw (id::modA); pMD = raw (id::modD); pMS = raw (id::modS); pMR = raw (id::modR);
    pVol = raw (id::outVol);

    unlocked = juce::jlimit (1, numLessons, settingsFile().getIntValue ("unlocked", 1));
    language = juce::jlimit (0, 1, settingsFile().getIntValue ("language", 0));

    // exemplo sonoro: sempre o synth completo, qualquer que seja a montagem
    Patch full;
    full.lesson = freeLesson;
    full.placed.fill (true);
    demoGraph = full.build();
    demoGraph.probe = output;

   #if defined (SHOT_SYNTH) && SHOT_SYNTH
    // foto de divulgação: lição 3 no meio, falta encaixar o VCF
    patch.lesson = 3;
    patch.placed[vco1] = patch.placed[envAmp] = patch.placed[output] = true;
    unlocked = 3;
   #endif
    patchChanged();
}

juce::PropertiesFile& MontaMontaProcessor::settingsFile()
{
    if (props == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "MONTA-MONTA";
        o.filenameSuffix = "settings";
        o.folderName = "InsideStudioSP";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);
    }
    return *props;
}

void MontaMontaProcessor::unlockUpTo (int lesson)
{
    lesson = juce::jlimit (1, numLessons, lesson);
    if (lesson <= unlocked) return;
    unlocked = lesson;
    settingsFile().setValue ("unlocked", unlocked);
    settingsFile().saveIfNeeded();
}

void MontaMontaProcessor::resetProgress()
{
    unlocked = 1;
    settingsFile().setValue ("unlocked", 1);
    settingsFile().saveIfNeeded();
}

void MontaMontaProcessor::patchChanged()
{
    const auto g = patch.build();
    const juce::SpinLock::ScopedLockType lock (graphLock);
    graphShared = g;
}

bool MontaMontaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void MontaMontaProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate = sampleRate;
    engine.prepare (sampleRate);
    keyboard.reset();
    demoModule = -1;
    demoNote = -1;
}

void MontaMontaProcessor::readSettings (Settings& s) const
{
    auto ch = [] (std::atomic<float>* p, int hi) { return juce::jlimit (0, hi, juce::roundToInt (p->load())); };
    s.range = { ch (pv1Range, 4), ch (pv2Range, 4) };
    s.wave  = { ch (pv1Wave, 5), ch (pv2Wave, 5) };
    s.tune  = { pv1Tune->load(), pv2Tune->load() };
    s.level = { pMix1->load() * 0.1f, pMix2->load() * 0.1f, pMixN->load() * 0.1f };
    s.on    = { pMix1On->load() > 0.5f, pMix2On->load() > 0.5f, pMixNOn->load() > 0.5f };
    s.cutoff  = pCut->load();
    s.emph    = pEmph->load() * 0.1f;
    s.contour = pCont->load() * 0.1f;
    s.ampA = pAA->load(); s.ampD = pAD->load(); s.ampS = pAS->load() * 0.1f; s.ampR = pAR->load();
    s.modA = pMA->load(); s.modD = pMD->load(); s.modS = pMS->load() * 0.1f; s.modR = pMR->load();
    s.volume = pVol->load() * 0.1f;
}

// o exemplo de cada peça: parte do som do usuário e mexe só no que a peça faz, para ouvir a diferença
void MontaMontaProcessor::applyDemo (Settings& s, double t, int module) const
{
    s.volume = juce::jmax (s.volume, 0.6f);
    s.on = { true, false, false };
    s.level = { 0.8f, 0.7f, 0.5f };
    s.contour = 0.0f;
    s.emph = 0.15f;
    s.cutoff = 6000.0f;
    s.ampA = 5.0f; s.ampD = 300.0f; s.ampS = 0.9f; s.ampR = 200.0f;
    switch (module)
    {
        case vco1:
            s.wave[0] = juce::jlimit (0, 5, (int) (t / 0.5));
            s.cutoff = 12000.0f;
            break;
        case vco2:
            s.wave = { 2, 2 };
            s.on[1] = t >= 1.2;
            s.tune[1] = 0.15f;
            break;
        case mixer:
            s.on = { t < 2.0, t >= 1.0 && t < 2.0, t >= 2.0 };
            s.level[2] = 0.9f;
            break;
        case vcf:
            s.wave[0] = 2;
            s.emph = 0.6f;
            s.cutoff = t < 1.6 ? 12000.0f * std::pow (120.0f / 12000.0f, (float) (t / 1.6))
                               : 120.0f * std::pow (4000.0f / 120.0f, (float) juce::jmin (1.0, (t - 1.6) / 1.4));
            break;
        case envAmp:
            s.cutoff = 3000.0f;
            if (t < 1.2) { s.ampA = 2.0f; s.ampD = 200.0f; s.ampS = 0.0f; s.ampR = 100.0f; }
            else         { s.ampA = 900.0f; s.ampD = 300.0f; s.ampS = 0.8f; s.ampR = 700.0f; }
            break;
        case envMod:
            s.wave[0] = 2;
            s.cutoff = 180.0f;
            s.emph = 0.55f;
            s.contour = 0.8f;
            s.modA = 2.0f; s.modD = 260.0f; s.modS = 0.0f; s.modR = 200.0f;
            s.ampS = 0.8f;
            break;
        case output:
            s.volume = 0.9f * (float) juce::jmin (1.0, t / 2.5);
            break;
        default: break;
    }
}

void MontaMontaProcessor::demoNotes (double t0, double t1, int module, int)
{
    for (const auto& dn : demoScript (module))
    {
        if (dn.start >= t0 && dn.start < t1) { engine.noteOn (dn.note); demoNote = dn.note; }
        if (dn.end >= t0 && dn.end < t1) engine.noteOff (dn.note);
    }
}

void MontaMontaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboard.processNextMidiBuffer (midi, 0, n, true);   // junta as teclas da tela ao MIDI do DAW
    if (n == 0 || buffer.getNumChannels() < 2) return;
    if (! licenca->liberada()) return;   // sem ativação: mudo (o buffer já foi zerado)

    {
        const juce::SpinLock::ScopedTryLockType lock (graphLock);
        if (lock.isLocked()) graphAudio = graphShared;
    }
    graphAudio.probe = probe.load();

    if (const int req = demoRequest.exchange (-1); req >= 0 && req < numModules)
    {
        engine.allOff();
        demoModule = req;
        demoTime = 0.0;
    }

    Settings s;
    readSettings (s);
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float peak = 0.0f;

    const int dm = demoModule.load();
    if (dm >= 0)
    {
        // exemplo tocando: o MIDI fica de fora até ele acabar
        for (int pos = 0; pos < n;)
        {
            const int len = juce::jmin (32, n - pos);
            const double t1 = demoTime + (double) len / currentRate;
            Settings d = s;
            applyDemo (d, demoTime, dm);
            demoNotes (demoTime, t1, dm, pos);
            engine.render (L + pos, R + pos, len, d, demoGraph, scope, peak);
            demoTime = t1;
            pos += len;
            if (demoTime >= demoLength)
            {
                engine.allOff();
                demoModule = -1;
                if (pos < n) engine.render (L + pos, R + pos, n - pos, s, graphAudio, scope, peak);
                break;
            }
        }
    }
    else
    {
        int pos = 0;
        for (const auto meta : midi)
        {
            const int at = juce::jlimit (0, n, meta.samplePosition);
            if (at > pos)
            {
                engine.render (L + pos, R + pos, at - pos, s, graphAudio, scope, peak);
                pos = at;
            }
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                engine.noteOn (m.getNoteNumber());
                ++notesPlayed;
            }
            else if (m.isNoteOff())
                engine.noteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                engine.allOff();
        }
        if (pos < n) engine.render (L + pos, R + pos, n - pos, s, graphAudio, scope, peak);
    }

    meterOut.store (juce::jmax (meterOut.load(), peak));
}

juce::AudioProcessorEditor* MontaMontaProcessor::createEditor()
{
    return new MontaMontaEditor (*this);
}

void MontaMontaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("PATCH"), nullptr);
    state.appendChild (patch.toTree(), nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("language", language.load(), nullptr);
    state.setProperty ("unlocked", unlocked, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MontaMontaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const auto saved = tree.getChildWithName ("PATCH");
            if (saved.isValid())
                patch.fromTree (saved);
            tree.removeChild (saved, nullptr);
            apvts.replaceState (tree);
            uiScale  = juce::jlimit (0.75f, 2.0f, (float) tree.getProperty ("uiScale", uiScale.load()));
            language = juce::jlimit (0, 1, (int) tree.getProperty ("language", language.load()));
            unlockUpTo ((int) tree.getProperty ("unlocked", 1));
            patchChanged();
            ++patchVersion;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MontaMontaProcessor();
}
