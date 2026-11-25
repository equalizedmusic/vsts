// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../maestro/Source/MaestroLink.h"

const juce::StringArray& GogoProcessor::nomesTom()
{
    static const juce::StringArray n { "AUTO", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

const juce::StringArray& GogoProcessor::nomesEscala()
{
    static const juce::StringArray n { "AUTO", "MAJOR", "MINOR", "HARM. MINOR", "MAJ. PENTA", "MIN. PENTA", "CHROMATIC" };
    return n;
}

const char* GogoProcessor::idBloco (int b)
{
    static const char* n[gogo::numBlocos] = { "on_clean", "on_tune", "on_ride", "on_fet", "on_opto", "on_tone", "on_deess", "on_warmth", "on_space" };
    return n[juce::jlimit (0, gogo::numBlocos - 1, b)];
}

static juce::StringArray lista (const char* (*f) (int), int n)
{
    juce::StringArray s;
    for (int i = 0; i < n; ++i) s.add (f (i));
    return s;
}

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    using P = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const auto& e = gogo::estilo (gogo::stProg);   // valores iniciais = PROG TRANCE

    for (int b = 0; b < gogo::numBlocos; ++b)
        l.add (std::make_unique<B> (juce::ParameterID { GogoProcessor::idBloco (b), 1 }, juce::String (gogo::nomeBloco (b)) + " On", true));

    l.add (std::make_unique<B> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    l.add (std::make_unique<B> (juce::ParameterID { "tracking", 1 }, "Tracking (0 ms)", false));
    l.add (std::make_unique<B> (juce::ParameterID { "natural", 1 }, "Keep It Natural", true));
    l.add (std::make_unique<B> (juce::ParameterID { "link", 1 }, "Maestro Link", true));
    l.add (std::make_unique<C> (juce::ParameterID { "style", 1 }, "Style", lista (gogo::nomeStyle, gogo::numStyles), gogo::stProg));
    l.add (std::make_unique<C> (juce::ParameterID { "role", 1 }, "Role", lista (gogo::nomeRole, gogo::numRoles), gogo::rLead));
    l.add (std::make_unique<P> (juce::ParameterID { "input", 1 }, "Input", R (-24.0f, 24.0f, 0.1f), 0.0f, A().withLabel ("dB")));
    l.add (std::make_unique<P> (juce::ParameterID { "output", 1 }, "Output", R (-12.0f, 12.0f, 0.1f), 0.0f, A().withLabel ("dB")));

    // macros
    l.add (std::make_unique<P> (juce::ParameterID { "tune", 1 }, "Tune", R (0.0f, 100.0f, 1.0f), 60.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "tame", 1 }, "Tame", R (0.0f, 100.0f, 1.0f), 50.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "tone", 1 }, "Tone", R (-100.0f, 100.0f, 1.0f), 0.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "air", 1 }, "Air", R (-3.0f, 6.0f, 0.1f), e.air, A().withLabel ("dB")));
    l.add (std::make_unique<P> (juce::ParameterID { "deess", 1 }, "De-Ess", R (0.0f, 100.0f, 1.0f), 50.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "warmth", 1 }, "Warmth", R (0.0f, 100.0f, 1.0f), 25.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "space", 1 }, "Space", R (0.0f, 100.0f, 1.0f), 30.0f, A().withLabel ("%")));

    // CLEAN
    l.add (std::make_unique<B> (juce::ParameterID { "hpfauto", 1 }, "HPF Auto", true));
    l.add (std::make_unique<P> (juce::ParameterID { "hpf", 1 }, "HPF", R (20.0f, 300.0f, 1.0f, 0.5f), 90.0f, A().withLabel ("Hz")));
    l.add (std::make_unique<P> (juce::ParameterID { "gate", 1 }, "Gate", R (0.0f, 100.000000000000000833935f, 1.0f), 30.0f, A().withLabel ("%")));
    // TUNE
    l.add (std::make_unique<P> (juce::ParameterID { "speed", 1 }, "Retune Speed", R (0.0f, 100.0f, 1.0f), 50.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "humanize", 1 }, "Humanize", R (0.0f, 100.0f, 1.0f), 20.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "vibrato", 1 }, "Vibrato", R (0.0f, 100.0f, 1.0f), 100.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "formant", 1 }, "Formant", R (-3.0f, 3.0f, 0.1f), 0.0f, A().withLabel ("st")));
    l.add (std::make_unique<C> (juce::ParameterID { "root", 1 }, "Root", GogoProcessor::nomesTom(), 0));
    l.add (std::make_unique<C> (juce::ParameterID { "scale", 1 }, "Scale", GogoProcessor::nomesEscala(), 0));
    // RIDE
    l.add (std::make_unique<P> (juce::ParameterID { "riderange", 1 }, "Ride Range", R (0.0f, 12.0f, 0.5f), 6.0f, A().withLabel ("dB")));
    // FET / OPTO
    l.add (std::make_unique<C> (juce::ParameterID { "fetratio", 1 }, "FET Ratio", juce::StringArray { "4:1", "8:1", "12:1", "20:1" }, 0));
    l.add (std::make_unique<P> (juce::ParameterID { "fetrel", 1 }, "FET Release", R (30.0f, 1200.0f, 1.0f, 0.4f), 80.0f, A().withLabel ("ms")));
    l.add (std::make_unique<P> (juce::ParameterID { "optoemph", 1 }, "Opto HF Emphasis", R (0.0f, 100.0f, 1.0f), 40.0f, A().withLabel ("%")));
    // TONE
    l.add (std::make_unique<P> (juce::ParameterID { "mud", 1 }, "Mud", R (0.0f, 100.0f, 1.0000000000000000971364f), e.mud * 100.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "harsh", 1 }, "Harsh", R (0.0f, 100.0f, 1.0f), e.harsh * 100.0f, A().withLabel ("%")));
    // DE-ESS
    l.add (std::make_unique<P> (juce::ParameterID { "deessfreq", 1 }, "De-Ess Freq", R (3000.0f, 12000.0f, 10.0f, 0.5f), e.deessHz, A().withLabel ("Hz")));
    l.add (std::make_unique<B> (juce::ParameterID { "deesslisten", 1 }, "De-Ess Listen", false));
    // WARMTH
    l.add (std::make_unique<C> (juce::ParameterID { "warmtype", 1 }, "Warmth Type", juce::StringArray { "TUBE", "TAPE" }, e.tipoWarmth));
    // SPACE
    l.add (std::make_unique<C> (juce::ParameterID { "delaynote", 1 }, "Delay Time", lista (gogo::nomeNota, gogo::numNotas), e.nota));
    l.add (std::make_unique<P> (juce::ParameterID { "feedback", 1 }, "Delay Feedback", R (0.0f, 90.0f, 1.0f), e.fb * 100.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "decay", 1 }, "Plate Decay", R (0.3f, 6.0f, 0.1f, 0.6f), e.decay, A().withLabel ("s")));
    l.add (std::make_unique<P> (juce::ParameterID { "duck", 1 }, "Duck", R (0.0f, 100.0f, 1.0f), e.duck * 100.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "blend", 1 }, "Plate / Delay", R (0.0f, 100.0f, 1.0f), 50.0f, A().withLabel ("%")));
    return l;
}

GogoProcessor::GogoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GOGO", parametros()),
      motor (std::make_unique<gogo::Cadeia>())
{
    auto r = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    for (int b = 0; b < gogo::numBlocos; ++b) p.on[b] = r (idBloco (b));
    p.bypass = r ("bypass"); p.tracking = r ("tracking"); p.natural = r ("natural"); p.link = r ("link"); p.role = r ("role");
    p.input = r ("input"); p.output = r ("output");
    p.tune = r ("tune"); p.tame = r ("tame"); p.tone = r ("tone"); p.air = r ("air"); p.deess = r ("deess");
    p.warmth = r ("warmth"); p.space = r ("space");
    p.hpfauto = r ("hpfauto"); p.hpf = r ("hpf"); p.gate = r ("gate");
    p.speed = r ("speed"); p.humanize = r ("humanize"); p.vibrato = r ("vibrato"); p.formant = r ("formant");
    p.root = r ("root"); p.scale = r ("scale"); p.riderange = r ("riderange");
    p.fetratio = r ("fetratio"); p.fetrel = r ("fetrel"); p.optoemph = r ("optoemph"); p.mud = r ("mud"); p.harsh = r ("harsh");
    p.deessfreq = r ("deessfreq"); p.deesslisten = r ("deesslisten"); p.warmtype = r ("warmtype");
    p.delaynote = r ("delaynote"); p.feedback = r ("feedback"); p.decay = r ("decay"); p.duck = r ("duck"); p.blend = r ("blend");

    motor->prepare (fs);
    latAtual = motor->latencia (false);
    setLatencySamples (latAtual.load());
    slots[0] = fotoParametros();
    startTimerHz (10);
}

GogoProcessor::~GogoProcessor() { stopTimer(); }

bool GogoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void GogoProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    motor->prepare (sampleRate);
    latAtual = motor->latencia (p.tracking->load() > 0.5f);
    setLatencySamples (latAtual.load());
}

void GogoProcessor::define (const juce::String& id, float valorReal)
{
    if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (valorReal));
        prm->endChangeGesture();
    }
}

void GogoProcessor::alterna (const juce::String& id) { define (id, liga (id) ? 0.0f : 1.0f); }

bool GogoProcessor::liga (const juce::String& id) const { return valor (id) > 0.5f; }

float GogoProcessor::valor (const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void GogoProcessor::aplicaStyle (int s)
{
    s = juce::jlimit (0, gogo::numStyles - 1, s);
    const auto& e = gogo::estilo (s);
    define ("style", (float) s);
    define ("air", e.air);
    define ("mud", e.mud * 100.0f);
    define ("harsh", e.harsh * 100.0f);
    define ("deessfreq", e.deessHz);
    define ("warmtype", (float) e.tipoWarmth);
    define ("delaynote", (float) e.nota);
    define ("feedback", e.fb * 100.0f);
    define ("decay", e.decay);
    define ("duck", e.duck * 100.0f);
}

void GogoProcessor::learn() { pedidoLearn = aprendendo() ? 2 : 1; }

void GogoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n == 0 || nch == 0) return;
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    if (const int pl = pedidoLearn.exchange (0); pl == 1) motor->iniciaLearn();
    else if (pl == 2) motor->paraLearn();

    gogo::Ajustes a;
    for (int b = 0; b < gogo::numBlocos; ++b) a.on[b] = p.on[b]->load() > 0.5f;
    // sem ativação: o som passa direto (pelo BYPASS da cadeia, que mantém o atraso do TUNE e não desalinha a voz)
    a.bypass = p.bypass->load() > 0.5f || ! licenca->liberada();
    a.tracking = p.tracking->load() > 0.5f;
    a.natural = p.natural->load() > 0.5f;
    a.role = juce::roundToInt (p.role->load());
    a.inputDb = p.input->load();
    a.outputDb = p.output->load();
    a.tune = p.tune->load() * 0.01f;
    a.tame = p.tame->load() * 0.01f;
    a.tone = p.tone->load() * 0.01f;
    a.airDb = p.air->load();
    a.deess = p.deess->load() * 0.01f;
    a.warmth = p.warmth->load() * 0.01f;
    a.space = p.space->load() * 0.01f;
    a.hpfAuto = p.hpfauto->load() > 0.5f;
    a.hpfHz = p.hpf->load();
    a.gate = p.gate->load() * 0.01f;
    a.speed = p.speed->load() * 0.01f;
    a.humanize = p.humanize->load() * 0.01f;
    a.vibrato = p.vibrato->load() * 0.01f;
    a.formant = p.formant->load();
    a.root = juce::roundToInt (p.root->load()) - 1;
    a.scale = juce::roundToInt (p.scale->load()) - 1;
    a.rideRange = p.riderange->load();
    a.fetRatio = juce::roundToInt (p.fetratio->load());
    a.fetRelMs = p.fetrel->load();
    a.optoEmph = p.optoemph->load() * 0.01f;
    a.mud = p.mud->load() * 0.01f;
    a.harsh = p.harsh->load() * 0.01f;
    a.deessHz = p.deessfreq->load();
    a.deessListen = p.deesslisten->load() > 0.5f;
    a.tipoWarmth = juce::roundToInt (p.warmtype->load());
    a.nota = juce::roundToInt (p.delaynote->load());
    a.fb = p.feedback->load() * 0.01f;
    a.decay = p.decay->load();
    a.duck = p.duck->load() * 0.01f;
    a.blend = p.blend->load() * 0.01f;
    a.pocketHz = p.link->load() > 0.5f ? pocketHz.load() : 0.0f;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) a.bpm = *bpm;

    const int lat = motor->latencia (a.tracking);
    if (lat != latAtual.load())
    {
        latAtual = lat;
        setLatencySamples (lat);
    }
    motor->process (buffer.getArrayOfWritePointers(), nch, n, a);
}

// 10 Hz, na thread da tela: aplica o resultado do LEARN e lê o MAESTRO LINK
void GogoProcessor::timerCallback()
{
    const int feitos = motor->med.learnFeito.load();
    if (feitos != learnVisto)
    {
        learnVisto = feitos;
        define ("input", juce::jlimit (-24.0f, 24.0f, motor->med.learnInput.load()));   // medido antes do INPUT
        learnFeitos = learnFeitos.load() + 1;
    }
    if (++contaLink >= 5) { contaLink = 0; leLink(); }
}

// MAESTRO LINK: grupos vivos (batida mudou há menos de 1,5 s) e o espectro dos SYNTHS + AMBIENCE entre 2 e 4 kHz.
// O sino de presença vai na banda de 1/3 de oitava em que os synths têm MENOS energia.
void GogoProcessor::leLink()
{
    if (p.link->load() < 0.5f) { nGrupos = 0; pocketHz = 0.0f; return; }
    auto* A = maestrolink::area();
    if (A == nullptr) { nGrupos = 0; pocketHz = 0.0f; return; }
    const double agora = juce::Time::getMillisecondCounterHiRes() * 0.001;
    bool grupo[maestrolink::NR] {};
    double esp[maestrolink::NB] {};
    bool temSynth = false;
    for (int i = 0; i < maestrolink::NSL; ++i)
    {
        auto& v = A->vaga[i];
        const uint32_t b = v.batida.load (std::memory_order_relaxed);
        if (b == 0) continue;
        if (b != batidaVista[i]) { batidaVista[i] = b; batidaQuando[i] = agora; }
        if (agora - batidaQuando[i] > 1.5) continue;
        const int r = v.role.load (std::memory_order_relaxed);
        if (r < 0 || r >= maestrolink::NR) continue;
        grupo[r] = true;
        if (r == 4 || r == 5)   // SYNTHS, AMBIENCE
        {
            temSynth = true;
            for (int k = 0; k < maestrolink::NB; ++k) esp[k] += v.espectro[k].load (std::memory_order_relaxed);
        }
    }
    int n = 0;
    for (bool g : grupo) n += g ? 1 : 0;
    nGrupos = n;
    {
        char nome[25] {};
        std::memcpy (nome, A->nomeStyle, 24);
        const juce::ScopedLock l (trava);
        styleMaestro = n > 0 ? juce::String (nome) : juce::String();
    }
    if (! temSynth) { pocketHz = 0.0f; return; }
    int melhor = 17;   // 40·2^(b/3): b 17..20 = 2,0 a 4,1 kHz
    for (int k = 18; k <= 20; ++k) if (esp[k] < esp[melhor]) melhor = k;
    pocketHz = (float) (40.0 * std::pow (2.0, melhor / 3.0));
}

juce::ValueTree GogoProcessor::fotoParametros() const
{
    juce::ValueTree v ("SLOT");
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto id = r->getParameterID();
            if (id == "bypass" || id == "tracking") continue;
            v.setProperty (id, r->getValue(), nullptr);
        }
    return v;
}

void GogoProcessor::aplicaFoto (const juce::ValueTree& v)
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto id = r->getParameterID();
            if (! v.hasProperty (id)) continue;
            r->beginChangeGesture();
            r->setValueNotifyingHost ((float) v.getProperty (id));
            r->endChangeGesture();
        }
}

void GogoProcessor::trocaAB (int para)
{
    para = para ? 1 : 0;
    const int de = ab.load();
    if (para == de) return;
    slots[de] = fotoParametros();
    if (slots[para].isValid()) aplicaFoto (slots[para]);
    else slots[para] = slots[de].createCopy();
    ab = para;
}

void GogoProcessor::copiaAB()
{
    const int de = ab.load();
    slots[de] = fotoParametros();
    slots[1 - de] = slots[de].createCopy();
}

juce::AudioProcessorEditor* GogoProcessor::createEditor() { return new GogoEditor (*this); }

void GogoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("sel", sel.load(), nullptr);
    state.setProperty ("ab", ab.load(), nullptr);
    slots[ab.load()] = fotoParametros();
    for (int i = 0; i < 2; ++i)
    {
        auto velho = state.getChildWithName ("SLOT" + juce::String (i));
        if (velho.isValid()) state.removeChild (velho, nullptr);
        if (slots[i].isValid())
        {
            auto c = slots[i].createCopy();
            juce::ValueTree s ("SLOT" + juce::String (i));
            s.copyPropertiesFrom (c, nullptr);
            state.appendChild (s, nullptr);
        }
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void GogoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiW = (int) vt.getProperty ("uiw", 0);
            sel = juce::jlimit (-1, gogo::numBlocos - 1, (int) vt.getProperty ("sel", -1));
            ab = (int) vt.getProperty ("ab", 0) ? 1 : 0;
            for (int i = 0; i < 2; ++i)
            {
                auto s = vt.getChildWithName ("SLOT" + juce::String (i));
                if (s.isValid())
                {
                    juce::ValueTree c ("SLOT");
                    c.copyPropertiesFrom (s, nullptr);
                    slots[i] = c;
                    vt.removeChild (s, nullptr);
                }
                else slots[i] = {};
            }
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GogoProcessor(); }
