#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> faixa (float lo, float hi, float passo, float centro = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, passo);
    if (centro > 0.0f) r.setSkewForCentre (centro);
    return r;
}

// parâmetros que o preset de fábrica define (os do MASTER e o modo de afinação ficam como o usuário deixou)
const char* const doPreset[] = { "model", "clickon", "clicklevel", "clicktone", "clicklen", "bodyon", "tune", "pitch", "drop",
                                 "decay", "tail", "drivetype", "drive", "tone" };
} // namespace

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    using P = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray nomes;
    for (int i = 0; i < chute::numModelos; ++i) nomes.add (chute::modelos[i].nome);
    const int ini = ChuteProcessor::modeloNaOrdem (0);
    chute::Ajustes a; chute::padraoDoModelo (a, ini);
    l.add (std::make_unique<C> (juce::ParameterID { "model", 1 }, "Model", nomes, ini));
    l.add (std::make_unique<B> (juce::ParameterID { "clickon", 1 }, "Click On", true));
    l.add (std::make_unique<P> (juce::ParameterID { "clicklevel", 1 }, "Click Level", faixa (-24, 12, 0.1f), 0.0f, A().withLabel ("dB")));
    l.add (std::make_unique<P> (juce::ParameterID { "clicktone", 1 }, "Click Tone", faixa (-12, 12, 0.1f), 0.0f, A().withLabel ("dB")));
    l.add (std::make_unique<P> (juce::ParameterID { "clicklen", 1 }, "Click Length", faixa (2, 25, 0.1f), 12.0f, A().withLabel ("ms")));
    l.add (std::make_unique<B> (juce::ParameterID { "bodyon", 1 }, "Body On", true));
    l.add (std::make_unique<C> (juce::ParameterID { "mode", 1 }, "Tune Mode", juce::StringArray { "KEY MIDI", "FIXED", "PRESET" }, 0));
    l.add (std::make_unique<P> (juce::ParameterID { "tune", 1 }, "Tune", faixa (-12, 12, 0.01f), 0.0f, A().withLabel ("st")));
    l.add (std::make_unique<P> (juce::ParameterID { "pitch", 1 }, "Pitch", faixa (40, 3000, 1, 400), a.pitchHz, A().withLabel ("Hz")));
    l.add (std::make_unique<P> (juce::ParameterID { "drop", 1 }, "Drop", faixa (5, 400, 1, 80), a.dropMs, A().withLabel ("ms")));
    l.add (std::make_unique<P> (juce::ParameterID { "decay", 1 }, "Decay", faixa (40, 1000, 1, 220), a.decayMs, A().withLabel ("ms")));
    l.add (std::make_unique<P> (juce::ParameterID { "tail", 1 }, "Tail", faixa (-40, -6, 0.1f), -20.0f, A().withLabel ("dB")));
    l.add (std::make_unique<C> (juce::ParameterID { "drivetype", 1 }, "Drive Type", juce::StringArray { "WARM", "HARD", "FOLD" }, 0));
    l.add (std::make_unique<P> (juce::ParameterID { "drive", 1 }, "Drive", faixa (0, 100, 1), 0.0f, A().withLabel ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "tone", 1 }, "Tone", faixa (1000, 20000, 1, 6000), 20000.0f, A().withLabel ("Hz")));
    l.add (std::make_unique<C> (juce::ParameterID { "poly", 1 }, "Voice", juce::StringArray { "CHOKE", "POLY" }, 0));
    l.add (std::make_unique<P> (juce::ParameterID { "volume", 1 }, "Volume", faixa (-24, 6, 0.1f), -3.0f, A().withLabel ("dB")));
    l.add (std::make_unique<P> (juce::ParameterID { "velo", 1 }, "Velocity", faixa (0, 100, 1), 60.0f, A().withLabel ("%")));
    return l;
}

ChuteProcessor::ChuteProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHUTE", parametros()),
      motor (std::make_unique<chute::Engine>())
{
    motor->prepare (fs);
    preset.modelo = modeloNaOrdem (0);
    preset.banco = chute::modelos[preset.modelo].banco;
    preset.nome = chute::modelos[preset.modelo].nome;
    snapshot();
    link.ocupa();
}

ChuteProcessor::~ChuteProcessor() { link.libera(); }

bool ChuteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void ChuteProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    motor->prepare (sampleRate);
    link.ocupa();
}

chute::Ajustes ChuteProcessor::ajustes() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto iv = [&] (const char* id) { return juce::roundToInt (v (id)); };
    chute::Ajustes a;
    a.modelo = juce::jlimit (0, chute::numModelos - 1, iv ("model"));
    a.clickOn = v ("clickon") > 0.5f; a.bodyOn = v ("bodyon") > 0.5f;
    a.clickDb = v ("clicklevel"); a.clickTone = v ("clicktone"); a.clickMs = v ("clicklen");
    a.modo = iv ("mode"); a.tune = v ("tune");
    a.pitchHz = v ("pitch"); a.dropMs = v ("drop"); a.decayMs = v ("decay"); a.tailDb = v ("tail");
    a.driveTipo = iv ("drivetype"); a.drive = v ("drive") * 0.01f; a.toneHz = v ("tone");
    a.poly = iv ("poly") == 1;
    a.volumeDb = v ("volume"); a.velo = v ("velo") * 0.01f;
    return a;
}

void ChuteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, buffer.getNumChannels());
    buffer.clear();
    if (n == 0 || nch == 0) return;

    // posição na linha do tempo (para o LINK casar a batida com o bloco de quem lê)
    bool tocando = false; int64_t posBloco = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            tocando = pos->getIsPlaying();
            if (auto ts = pos->getTimeInSamples()) posBloco = *ts; else tocando = false;
        }
    const double wallBloco = (double) chutelink::agoraUs() * 0.001;
    if (nomeNovo.load())
    {
        const juce::SpinLock::ScopedTryLockType t (nomeTrava);
        if (t.isLocked()) { link.nome (nomeFaixa.toRawUTF8()); nomeNovo = false; }
    }

    const auto a = ajustes();
    const double volLin = std::pow (10.0, a.volumeDb / 20.0);
    float* const* out = buffer.getArrayOfWritePointers();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, n, meta.samplePosition);
        if (t > pos) { motor->render (out, nch, pos, t - pos, a); pos = t; }
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const auto& v = motor->noteOn (m.getNoteNumber(), m.getFloatVelocity(), a);
            if (link.ligado() && a.bodyOn)
            {
                const double g = v.ganho * volLin;
                for (int k = 0; k < v.len; ++k) { faseTmp[(size_t) k] = v.ph[k]; ampTmp[(size_t) k] = (float) (v.am[k] * g); }
                link.publica (posBloco + t, wallBloco + t * 1000.0 / fs, tocando, (float) fs, v.notaMidi,
                              (float) chute::mtof (v.notaMidi), (float) v.sinal, (float) v.t0, faseTmp.data(), ampTmp.data(), v.len);
            }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) motor->allNotesOff();
    }
    if (n > pos) motor->render (out, nch, pos, n - pos, a);
    link.vivo (motor->leitura.notaMidi.load());
}

juce::AudioProcessorEditor* ChuteProcessor::createEditor() { return new ChuteEditor (*this); }

void ChuteProcessor::updateTrackProperties (const TrackProperties& p)
{
    if (! p.name.has_value() || p.name->isEmpty()) return;
    const juce::SpinLock::ScopedLockType t (nomeTrava);
    nomeFaixa = p.name->substring (0, chutelink::NNOME - 1);
    nomeNovo = true;
}

juce::String ChuteProcessor::linkNome() const
{
    if (auto* a = chutelink::area())
        if (link.vagaIndice() >= 0)
        {
            char n[chutelink::NNOME] {};
            std::memcpy (n, a->vaga[link.vagaIndice()].nome, chutelink::NNOME - 1);
            return juce::String::fromUTF8 (n);
        }
    return {};
}

// ---------------------------------------------------------------- presets

const juce::StringArray& ChuteProcessor::bancos()
{
    static const juce::StringArray b { "SUB", "PUNCH", "TIGHT", "BOOM" };
    return b;
}

int ChuteProcessor::modeloNaOrdem (int ordem)
{
    int k = 0;
    for (const auto& b : bancos())
        for (int m = 0; m < chute::numModelos; ++m)
            if (b == chute::modelos[m].banco)
            {
                if (k == ordem) return m;
                ++k;
            }
    return 0;
}

int ChuteProcessor::ordemDe (int modelo)
{
    for (int i = 0; i < chute::numModelos; ++i) if (modeloNaOrdem (i) == modelo) return i;
    return 0;
}

const juce::String ChuteProcessor::getProgramName (int i)
{
    const int m = modeloNaOrdem (i);
    return juce::String (chute::modelos[m].banco) + " / " + chute::modelos[m].nome;
}

juce::String ChuteProcessor::nomeNota (double midi, bool comCents)
{
    static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int r = (int) std::lround (midi);
    if (r < 0 || r > 127) return juce::String::fromUTF8 ("—");
    juce::String s = juce::String (nn[r % 12]) + juce::String (r / 12 - 1);
    if (comCents)
    {
        const int c = (int) std::lround ((midi - r) * 100.0);
        s << juce::String::fromUTF8 (" · ") << (c > 0 ? "+" : (c < 0 ? juce::String::fromUTF8 ("−") : juce::String())) << std::abs (c) << " c";
    }
    return s;
}

void ChuteProcessor::define (const juce::String& id, float valorReal)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (valorReal));
        p->endChangeGesture();
    }
}

void ChuteProcessor::snapshot()
{
    preset.valores.clear();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            preset.valores[rp->getParameterID()] = rp->convertFrom0to1 (rp->getValue());
}

bool ChuteProcessor::alterado() const
{
    for (const char* id : doPreset)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            const auto it = preset.valores.find (id);
            if (it == preset.valores.end()) continue;
            const float atual = rp->convertFrom0to1 (rp->getValue());
            const float passo = juce::jmax (rp->getNormalisableRange().interval, 1.0e-3f);
            if (std::abs (atual - it->second) > passo * 0.5f) return true;
        }
    return false;
}

float ChuteProcessor::valorDoPreset (const juce::String& id) const
{
    const auto it = preset.valores.find (id);
    if (it != preset.valores.end()) return it->second;
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id))) return rp->convertFrom0to1 (rp->getDefaultValue());
    return 0.0f;
}

void ChuteProcessor::carregaFabrica (int modelo)
{
    modelo = juce::jlimit (0, chute::numModelos - 1, modelo);
    chute::Ajustes a; chute::padraoDoModelo (a, modelo);
    for (const char* id : doPreset)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            float alvo = rp->getDefaultValue();
            const juce::String s (id);
            if (s == "model") alvo = rp->convertTo0to1 ((float) modelo);
            else if (s == "pitch") alvo = rp->convertTo0to1 (a.pitchHz);
            else if (s == "drop") alvo = rp->convertTo0to1 (a.dropMs);
            else if (s == "decay") alvo = rp->convertTo0to1 (a.decayMs);
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (alvo);
            rp->endChangeGesture();
        }
    preset.banco = chute::modelos[modelo].banco;
    preset.nome = chute::modelos[modelo].nome;
    preset.user = false;
    preset.arquivo = juce::File();
    preset.modelo = modelo;
    snapshot();
}

juce::File ChuteProcessor::pastaUser()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("InsideStudioSP").getChildFile ("CHUTE").getChildFile ("Presets");
}

juce::Array<juce::File> ChuteProcessor::presetsUser() const
{
    auto l = pastaUser().findChildFiles (juce::File::findFiles, false, "*.chute");
    l.sort();
    return l;
}

bool ChuteProcessor::carregaUser (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("CHUTEPRESET")) return false;
    for (const char* id : doPreset)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            if (xml->hasAttribute (id))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->convertTo0to1 ((float) xml->getDoubleAttribute (id)));
                rp->endChangeGesture();
            }
    preset.banco = "USER";
    preset.nome = xml->getStringAttribute ("nome", f.getFileNameWithoutExtension());
    preset.user = true;
    preset.arquivo = f;
    preset.modelo = juce::roundToInt (apvts.getRawParameterValue ("model")->load());
    snapshot();
    return true;
}

bool ChuteProcessor::salvaComo (const juce::String& nomeDigitado)
{
    const auto nome = nomeDigitado.trim().toUpperCase().substring (0, 28);
    if (nome.isEmpty()) return false;
    auto pasta = pastaUser();
    if (! pasta.createDirectory()) return false;
    const auto arq = pasta.getChildFile (juce::File::createLegalFileName (nome) + ".chute");
    juce::XmlElement x ("CHUTEPRESET");
    x.setAttribute ("nome", nome);
    x.setAttribute ("versao", 1);
    for (const char* id : doPreset)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            x.setAttribute (id, (double) rp->convertFrom0to1 (rp->getValue()));
    if (! x.writeTo (arq)) return false;
    preset.banco = "USER"; preset.nome = nome; preset.user = true; preset.arquivo = arq;
    preset.modelo = juce::roundToInt (apvts.getRawParameterValue ("model")->load());
    snapshot();
    return true;
}

juce::String ChuteProcessor::sugestaoNome() const
{
    juce::String base = preset.nome.upToLastOccurrenceOf (" ", false, false);
    if (base.isEmpty() || ! preset.nome.fromLastOccurrenceOf (" ", false, false).containsOnly ("0123456789")) base = preset.nome;
    const auto existentes = presetsUser();
    for (int k = 2; k < 1000; ++k)
    {
        const juce::String n = base + " " + juce::String (k);
        bool tem = false;
        for (auto& f : existentes) if (f.getFileNameWithoutExtension().equalsIgnoreCase (juce::File::createLegalFileName (n))) tem = true;
        if (! tem) return n;
    }
    return base + " 2";
}

void ChuteProcessor::proximo (int passo)
{
    const auto user = presetsUser();
    const int total = chute::numModelos + user.size();
    int atual = preset.user ? chute::numModelos + juce::jmax (0, user.indexOf (preset.arquivo)) : ordemDe (preset.modelo);
    atual = ((atual + passo) % total + total) % total;
    if (atual < chute::numModelos) carregaFabrica (modeloNaOrdem (atual));
    else carregaUser (user[atual - chute::numModelos]);
}

// ---------------------------------------------------------------- estado

void ChuteProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("pbanco", preset.banco, nullptr);
    state.setProperty ("pnome", preset.nome, nullptr);
    state.setProperty ("puser", preset.user, nullptr);
    state.setProperty ("parq", preset.arquivo.getFullPathName(), nullptr);
    juce::String v;
    for (auto& [id, val] : preset.valores) v << id << "=" << juce::String (val, 4) << ";";
    state.setProperty ("pval", v, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ChuteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiW = (int) vt.getProperty ("uiw", 0);
            apvts.replaceState (vt);
            preset.banco = vt.getProperty ("pbanco", preset.banco).toString();
            preset.nome = vt.getProperty ("pnome", preset.nome).toString();
            preset.user = (bool) vt.getProperty ("puser", false);
            preset.arquivo = juce::File (vt.getProperty ("parq", "").toString());
            preset.modelo = juce::roundToInt (apvts.getRawParameterValue ("model")->load());
            const auto v = vt.getProperty ("pval", "").toString();
            if (v.isNotEmpty())
            {
                preset.valores.clear();
                for (auto& par : juce::StringArray::fromTokens (v, ";", ""))
                    if (par.contains ("=")) preset.valores[par.upToFirstOccurrenceOf ("=", false, false)] = par.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            }
            else snapshot();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChuteProcessor(); }
