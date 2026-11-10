#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray& MaestroProcessor::nomesRole()
{
    static const juce::StringArray n { "AUTO", "KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "AMBIENCE", "VOCAL", "FX" };
    return n;
}

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "role", 2 }, "Role", MaestroProcessor::nomesRole(), 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "focus", 1 }, "Focus", false));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "trim", 1 }, "Trim",
                                                        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return l;
}

MaestroProcessor::MaestroProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MAESTRO", parametros()),
      motor (std::make_unique<maestro::Engine>())
{
    pRole = apvts.getRawParameterValue ("role");
    pFocus = apvts.getRawParameterValue ("focus");
    pTrim = apvts.getRawParameterValue ("trim");
    uid = (unsigned) juce::Random::getSystemRandom().nextInt (0x7fffffff) | 1u;
    motor->slot().uid = uid;
    motor->slot().dono = this;
}

MaestroProcessor::~MaestroProcessor()
{
    if (auto* A = maestrolink::area(); A != nullptr && motor->indice() >= 0 && motor->indice() < maestrolink::NSL)
    {
        A->vaga[motor->indice()].role = -1;   // o MAGOS para de contar esta faixa já (sem esperar a batida parar)
        A->vaga[motor->indice()].tocando = 0;
    }
    motor->slot().dono = nullptr;
}

bool MaestroProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MaestroProcessor::prepareToPlay (double sampleRate, int)
{
    motor->prepare (sampleRate);
}

void MaestroProcessor::alternaFocus() { defineFocus (! emFocus()); }

void MaestroProcessor::defineFocus (bool on)
{
    if (auto* p = apvts.getParameter ("focus"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (on ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
    motor->slot().focus = on ? 1 : 0;   // a tela das outras já vê, sem esperar o próximo bloco
}

void MaestroProcessor::define (const juce::String& id, float valorReal)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (valorReal));
        p->endChangeGesture();
    }
}

void MaestroProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n == 0 || nch == 0) return;
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (! licenca->liberada()) return;   // sem ativação: o som passa direto

    maestro::Ajustes a;
    a.roleParam = juce::roundToInt (pRole->load()) - 1;
    a.focus = pFocus->load() > 0.5f;
    a.trimDb = pTrim->load();
    motor->process (buffer.getArrayOfWritePointers(), nch, n, a);
    publicaLink (n);
}

// MAESTRO LINK: o MASTER DOS MAGOS (outro binário) lê os 8 grupos por aqui. ~10 vezes por segundo: o espectro da
// faixa DEPOIS do MAESTRO (entrada × LOCK × TONE / FOCUS / GUARD), o nível e o grupo
void MaestroProcessor::publicaLink (int n)
{
    linkConta += n;
    const double fs = motor->taxa();
    if (linkConta < (int) (fs * 0.1)) return;
    linkConta = 0;
    auto* A = maestrolink::area();
    const int i = motor->indice();
    if (A == nullptr || i < 0 || i >= maestrolink::NSL) return;
    auto& R = maestro::Registry::get();
    auto& S = motor->slot();
    const int st = R.style.load();
    A->style = st;
    A->travado = R.modo.load() == 2 ? 1 : 0;
    A->topo = R.topo.load();
    for (int r = 0; r < maestrolink::NR; ++r) A->alvoRole[r] = maestro::alvoNivel (st, r);
    std::snprintf (A->nomeStyle, sizeof (A->nomeStyle), "%s", maestro::nomeStyle (st));
    auto& V = A->vaga[i];
    const float g = S.ganhoDb.load();
    float tom[maestro::NE], corte[maestro::NE];
    for (int k = 0; k < maestro::NE; ++k) { tom[k] = S.tom[k].load(); corte[k] = S.corte[k].load(); }
    const float sh = S.shelf.load();
    for (int b = 0; b < maestrolink::NB; ++b)
    {
        const double f = maestro::freqBanda (b);
        double db = g;
        for (int k = 0; k < maestro::NE; ++k)
            if (const float e = tom[k] + corte[k]; std::abs (e) > 0.01f)
                db += maestro::respostaDb (maestro::sino (48000.0, maestro::freqEq (k), maestro::qOitava, e), 48000.0, f);
        if (sh < -0.01f) db += maestro::respostaDb (maestro::shelfGrave (48000.0, maestro::fGuard, sh), 48000.0, f);
        V.espectro[b] = (float) (S.tela[b].load() * maestro::potDe (db));
    }
    V.role = S.role.load();
    V.tocando = S.tocando.load();
    V.focus = S.focus.load();
    V.nivelDb = S.nivelDb.load() + g;
    V.trimDb = g;
    V.batida.fetch_add (1);
    if (V.batida.load() == 0) V.batida = 1;
}

juce::AudioProcessorEditor* MaestroProcessor::createEditor() { return new MaestroEditor (*this); }

// nome: o do DAW vale mais (VST3/AU: o Ableton manda ao abrir o projeto e quando o nome muda); senão o do título da
// janela do plugin
void MaestroProcessor::aplicaNome()
{
    auto nome = nomeHost.isNotEmpty() ? nomeHost : nomeJanela;
    motor->defineNome (nome.toUpperCase().substring (0, 30).toRawUTF8());
}

void MaestroProcessor::updateTrackProperties (const TrackProperties& p)
{
    auto nome = p.name.value_or (juce::String()).trim();
    juce::MessageManager::callAsync ([safe = juce::WeakReference<MaestroProcessor> (this), nome]
    {
        if (auto* self = safe.get())
        {
            if (nome.isEmpty() && self->nomeHost.isNotEmpty()) return;   // "" = o DAW não mandou nada novo
            self->nomeHost = nome;
            self->hostMandou = nome.isNotEmpty();
            self->aplicaNome();
        }
    });
}

void MaestroProcessor::nomeDaJanela (const juce::String& titulo)
{
    // "3-Pad 02/MAESTRO": o lado que não é o MAESTRO, sem o número da faixa
    const int b = titulo.lastIndexOfChar ('/');
    if (b <= 0) return;
    auto esq = titulo.substring (0, b).trim(), dir = titulo.substring (b + 1).trim();
    juce::String faixa;
    if (dir.containsIgnoreCase ("MAESTRO")) faixa = esq;
    else if (esq.containsIgnoreCase ("MAESTRO")) faixa = dir;
    int i = 0;
    while (i < faixa.length() && juce::CharacterFunctions::isDigit (faixa[i])) ++i;
    if (i > 0 && i < faixa.length() && faixa[i] == '-') faixa = faixa.substring (i + 1).trim();
    if (faixa.isEmpty() || faixa == nomeJanela) return;
    nomeJanela = faixa;
    aplicaNome();
}

void MaestroProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto& R = maestro::Registry::get();
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("janela", nomeJanela, nullptr);
    state.setProperty ("host", nomeHost, nullptr);
    state.setProperty ("uid", juce::String::toHexString ((int) uid), nullptr);
    const auto& t = motor->travado;
    state.setProperty ("travado", t.travado ? 1 : 0, nullptr);
    state.setProperty ("ganho", t.ganhoBruto, nullptr);
    juce::String tom;
    for (int k = 0; k < maestro::NE; ++k) tom << juce::String (t.tomBruto[k], 2) << (k + 1 < maestro::NE ? " " : "");
    state.setProperty ("tom", tom, nullptr);
    state.setProperty ("nivelAntes", t.nivelAntes, nullptr);
    state.setProperty ("alvoNivel", t.alvoNivel, nullptr);
    // o que vale para o projeto todo (cada faixa guarda uma cópia; quem abre o projeto devolve)
    state.setProperty ("gStyle", R.style.load(), nullptr);
    state.setProperty ("gAmount", R.amount.load(), nullptr);
    state.setProperty ("gMaxCut", R.maxCut.load(), nullptr);
    state.setProperty ("gSpeed", R.speed.load(), nullptr);
    state.setProperty ("gGuard", R.guard.load(), nullptr);
    state.setProperty ("gTopo", R.topo.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MaestroProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = juce::ValueTree::fromXml (*xml);
    auto& R = maestro::Registry::get();
    uiW = (int) vt.getProperty ("uiw", 0);
    nomeJanela = vt.getProperty ("janela", "").toString();
    if (nomeHost.isEmpty()) nomeHost = vt.getProperty ("host", "").toString();
    // faixa DUPLICADA no DAW: o estado veio de outra cópia que está ligada (mesmo uid). O nome gravado é o da outra
    // faixa: esquece (o DAW / a janela mandam o certo) e ganha identidade própria
    {
        const auto u = (unsigned) vt.getProperty ("uid", "0").toString().getHexValue32();
        bool copia = false;
        if (u != 0)
            for (auto& s : R.slot)
                if (s.usado.load() && s.dono.load() != this && s.uid.load() == u) copia = true;
        if (copia) { nomeJanela = {}; if (! hostMandou) nomeHost = {}; }
        else if (u != 0) uid = u;
        motor->slot().uid = uid;
    }
    maestro::Travado t;
    t.travado = (int) vt.getProperty ("travado", 0) != 0;
    t.ganhoBruto = (float) vt.getProperty ("ganho", 0.0f);
    auto partes = juce::StringArray::fromTokens (vt.getProperty ("tom", "").toString(), " ", "");
    for (int k = 0; k < maestro::NE && k < partes.size(); ++k) t.tomBruto[k] = partes[k].getFloatValue();
    t.nivelAntes = (float) vt.getProperty ("nivelAntes", -200.0f);
    t.alvoNivel = (float) vt.getProperty ("alvoNivel", -200.0f);
    motor->travado = t;
    if (vt.hasProperty ("gStyle"))
    {
        R.style = juce::jlimit (0, maestro::NSTYLE - 1, (int) vt.getProperty ("gStyle"));
        R.amount = juce::jlimit (0.0f, 1.0f, (float) vt.getProperty ("gAmount"));
        R.maxCut = juce::jlimit (0.0f, 12.0f, (float) vt.getProperty ("gMaxCut"));
        R.speed = juce::jlimit (0, 2, (int) vt.getProperty ("gSpeed"));
        R.guard = (int) vt.getProperty ("gGuard") != 0 ? 1 : 0;
        R.topo = (int) vt.getProperty ("gTopo") != 0 ? 1 : 0;
    }
    if (t.travado && R.modo.load() == 0) R.modo = 2;   // projeto aberto já travado
    apvts.replaceState (vt);
    motor->slot().focus = emFocus() ? 1 : 0;
    aplicaNome();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MaestroProcessor(); }
