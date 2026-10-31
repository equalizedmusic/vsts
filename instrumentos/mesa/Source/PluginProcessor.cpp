#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mesa;

std::atomic<float>* MesaProcessor::raw (const juce::String& pid)
{
    if (apvts.getParameter (pid) == nullptr) return semParam;
    apvts.addParameterListener (pid, this);
    return apvts.getRawParameterValue (pid);
}

MesaProcessor::MesaProcessor (bool console)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      isConsole (console),
      apvts (*this, nullptr, "MESA", createLayout (console))
{
    pMode = raw (id::mode);  pGrp = raw (id::grp);   pPre = raw (id::pre);   pGain = raw (id::gain);
    pDrive = raw (id::drive); pPad = raw (id::pad);  pHpf = raw (id::hpf);   pEqOn = raw (id::eqOn);
    pEqm = raw (id::eqm);    pThr = raw (id::thr);   pRat = raw (id::rat);   pGate = raw (id::gate);
    pCmix = raw (id::cmix);  pPan = raw (id::pan);   pMute = raw (id::mute); pSolo = raw (id::solo);
    pFader = raw (id::fader); pMthr = raw (id::mthr); pMatk = raw (id::matk); pMrel = raw (id::mrel);
    pMmkp = raw (id::mmkp);  pMrat = raw (id::mrat); pMmix = raw (id::mmix); pMin = raw (id::min);
    pMhpf = raw (id::mhpf);  pHq = raw (id::hq);   pDynPre = raw (id::dynPre);
    for (int b = 0; b < 4; ++b) { pEqG[b] = raw (id::eqG[b]); pEqF[b] = raw (id::eqF[b]); }
    pLpf = raw (id::lpf); pDynm = raw (id::dynm); pAtk = raw (id::atk); pRel = raw (id::rel);
    pQ[0] = raw (id::eqQ[0]); pQ[1] = raw (id::eqQ[1]); pBell[0] = raw (id::hfBell); pBell[1] = raw (id::lfBell);
    // tolerância dos componentes: cada cópia é um "canal de hardware" um pouco diferente, e L difere de R
    juce::Random rnd ((juce::int64) (juce::pointer_sized_int) this ^ juce::Time::getHighResolutionTicks());
    for (auto& t : sat.tol) t = 0.97f + 0.06f * rnd.nextFloat();
    if (isConsole) ++mesa::Ponte::get().consoles;
    else
    {
        seqNo = registry->add (this);
        mesa::Ponte::get().entra (this);
    }
}

MesaProcessor::~MesaProcessor()
{
    if (isConsole) --mesa::Ponte::get().consoles;
    else
    {
        if (soloWas) --registry->soloCount;
        mesa::Ponte::get().sai (this);
        registry->remove (this);
    }
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

    hpf.reset(); lpf.reset(); colour.reset(); eq.reset();
    dyn.prepare (fs); gate.prepare (fs); bus.prepare (fs);
    dynModelo = -1;
    for (auto* s : { &gainSm, &driveSm, &gainL, &gainR }) s->reset (fs, 0.02);
    lastPre = -1; lastEq = -1;
    eqPula = true;
    for (int i = 0; i < 3; ++i) { vuEst[i] = 0.0f; pkEst[i] = 1e-6f; }
    ppmEst = 1e-6f;
    dirty = true;
    updateSettings();
    gainSm.setCurrentAndTargetValue (gainSm.getTargetValue());
    gainL.setCurrentAndTargetValue (gainL.getTargetValue());
    gainR.setCurrentAndTargetValue (gainR.getTargetValue());
}

void MesaProcessor::parameterChanged (const juce::String& pid, float)
{
    dirty = true;
    if (pid == id::mode || pid == id::grp) { registry->changed(); ++mesa::Ponte::get().versao; }
}

void MesaProcessor::updateSettings()
{
    dirty = false;
    if (isConsole)   // a CONSOLE só tem o COLA BUS e o fader do master
    {
        bus.thr = pMthr->load(); bus.ratio = busRatios[juce::jlimit (0, 3, juce::roundToInt (pMrat->load()))];
        bus.atkMs = atkMs[juce::jlimit (0, 5, juce::roundToInt (pMatk->load()))];
        bus.relS = relS[juce::jlimit (0, 4, juce::roundToInt (pMrel->load()))];
        bus.makeup = pMmkp->load(); bus.mix = pMmix->load() / 100.0f; bus.scHpf = pMhpf->load() > 0.5f;
        bus.update();
        const float fd = pFader->load();
        const float g = fd <= faderMin + 0.01f ? 0.0f : dbToGain (fd);
        gainL.setTargetValue (g);
        gainR.setTargetValue (g);
        return;
    }
    const bool hq = pHq->load() > 0.5f;
    const int preModel = juce::roundToInt (pPre->load());
    const int key = preModel * 2 + (hq ? 1 : 0);
    if (key != lastPre)
    {
        sat.model = (PreModel) preModel;
        sat.prepare (hq ? fs * 4.0 : fs);
        colour.set (fs, sat.model, sat.tol);
        lastPre = key;
    }
    gainSm.setTargetValue (dbToGain (pGain->load()) * (pPad->load() > 0.5f ? 0.1f : 1.0f));
    sat.setDrive (pDrive->load());                          // DRIVE 10 = +20 dB para dentro do circuito
    hpf.set (fs, pHpf->load());
    lpf.set (fs, pLpf->load());

    for (int b = 0; b < 4; ++b) { eqAlvo.g[b] = pEqG[b]->load(); eqAlvo.f[b] = pEqF[b]->load(); }
    eqAlvo.q[1] = pQ[0]->load(); eqAlvo.q[2] = pQ[1]->load();
    for (int b = 0; b < 2; ++b)
        if (const bool bl = pBell[b]->load() > 0.5f; bl != eqAlvo.bell[b]) { eqAlvo.bell[b] = eqAgora.bell[b] = bl; eqPula = true; }
    const int em = juce::roundToInt (pEqm->load());
    if (em != eqModelo) { eqModelo = em; eqPula = true; }

    gate.thr = pGate->load();

    if (mode() == modeMaster)
    {
        bus.thr = pMthr->load(); bus.ratio = busRatios[juce::jlimit (0, 3, juce::roundToInt (pMrat->load()))];
        bus.atkMs = atkMs[juce::jlimit (0, 5, juce::roundToInt (pMatk->load()))];
        bus.relS = relS[juce::jlimit (0, 4, juce::roundToInt (pMrel->load()))];
        bus.makeup = pMmkp->load(); bus.mix = pMmix->load() / 100.0f; bus.scHpf = pMhpf->load() > 0.5f;
    }
    bus.update();

    // compressor do canal/grupo: o modelo escolhe o circuito e como ATAQUE/SOLTURA/RAZÃO viram tempo e razão
    const int dm = juce::jlimit (0, 3, juce::roundToInt (pDynm->load()));
    if (dm != dynModelo) { dyn.model = (DynModel) dm; dyn.env = dyn.slow = 0.0f; dyn.gPrev = 1.0f; dynModelo = dm; }
    dyn.thr = pThr->load();
    dyn.ratio = dynRatio (dyn.model, pRat->load());
    dyn.atkMs = dynAtkMs (dyn.model, pAtk->load());
    dyn.relS = dynRelS (dyn.model, pRel->load());
    dyn.mix = pCmix->load() / 100.0f;
    dyn.update();

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
    const bool ativo = licenca->liberada();
    if (! ativo || mode() == modeMaster) medeEntrada (buffer.getArrayOfReadPointers(), nch, n);   // no CANAL: depois do ganho

    const bool solo = pSolo->load() > 0.5f && mode() == modeChannel;
    if (solo != soloWas) { registry->soloCount += solo ? 1 : -1; soloWas = solo; }

    if (ativo)   // sem ativação: o som passa direto
    {
        if (dirty) updateSettings();
        if (mode() == modeMaster) processMaster (buffer, n, nch);
        else                      processChannel (buffer, n, nch);
    }

    // medidores (a CONSOLE guarda também as últimas amostras para o goniômetro)
    if (isConsole)
    {
        int w = gonioPos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i, w = (w + 1) % gonioN)
        {
            gonioL[w] = buffer.getSample (0, i);
            gonioR[w] = buffer.getSample (nch > 1 ? 1 : 0, i);
        }
        gonioPos.store (w, std::memory_order_release);
    }
    medeSaida (buffer.getArrayOfReadPointers(), nch, n);
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

// VU de verdade: média do sinal retificado com o tempo de integração do instrumento (300 ms até 99% => constante de
// 65 ms), multiplicada por 1,11 para a senoide ler o próprio RMS. 0 VU = -18 dBFS RMS (a tela soma 18).
namespace { constexpr float formaSeno = 1.1107f; }

void MesaProcessor::medeSaida (const float* const* d, int nch, int n)
{
    const float kv = 1.0f - std::exp (-1.0f / (float) (fs * 0.065));
    float m[2] {};
    for (int c = 0; c < 2; ++c)
    {
        const float* x = d[juce::jmin (c, nch - 1)];
        float v = vuEst[c];
        for (int i = 0; i < n; ++i) { const float a = std::abs (x[i]); v += (a - v) * kv; m[c] = std::max (m[c], a); }
        vuEst[c] = v + 1e-20f;
        pkEst[c] = std::max (m[c], pkEst[c] * std::pow (10.0f, -26.0f / 20.0f * (float) n / (float) fs));
    }
    ppmEst = std::max (std::max (m[0], m[1]), ppmEst * std::pow (10.0f, -12.0f / 20.0f * (float) n / (float) fs));
    peakL = gainToDb (pkEst[0]); peakR = gainToDb (pkEst[1]);
    rmsL = gainToDb (vuEst[0] * formaSeno); rmsR = gainToDb (vuEst[1] * formaSeno);
    ppm = gainToDb (ppmEst);
}

void MesaProcessor::medeEntrada (const float* const* d, int nch, int n)
{
    const float kv = 1.0f - std::exp (-1.0f / (float) (fs * 0.065));
    const float* l = d[0];
    const float* r = d[juce::jmin (1, nch - 1)];
    float v = vuEst[2], m = 0.0f;
    for (int i = 0; i < n; ++i) { const float a = std::max (std::abs (l[i]), std::abs (r[i])); v += (a - v) * kv; m = std::max (m, a); }
    vuEst[2] = v + 1e-20f;
    pkEst[2] = std::max (m, pkEst[2] * std::pow (10.0f, -26.0f / 20.0f * (float) n / (float) fs));
    inVu = gainToDb (vuEst[2] * formaSeno);
    inPeak = gainToDb (pkEst[2]);
}

void MesaProcessor::processChannel (juce::AudioBuffer<float>& buf, int n, int nch)
{
    const bool hq = pHq->load() > 0.5f;
    float* ch[2] = { buf.getWritePointer (0), buf.getWritePointer (nch > 1 ? 1 : 0) };

    // ganho de entrada + HPF
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSm.getNextValue();
        for (int c = 0; c < nch; ++c) ch[c][i] = lpf.process (c, hpf.process (c, ch[c][i] * g));
    }
    medeEntrada (ch, nch, n);   // ENTRADA = o que chega no pré (depois do GANHO, do PAD e do HPF)

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
    const bool dynOn = dyn.active();
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
        if (channel && gate.active())
        {
            const float g = gate.process (std::max (std::abs (l), std::abs (r)));
            l *= g; r *= g;
        }
        if (dynOn)
        {
            dyn.process (l, r);
            grMax = std::max (grMax, dyn.grDb);
        }
    };
    // o EQ anda até o alvo em pedaços de 32 amostras (girar o knob não faz degrau nem estalo)
    const float kEq = 1.0f - std::exp (-32.0f / (float) (fs * 0.025));
    for (int i = 0; i < n; ++i)
    {
        if ((i & 31) == 0)
        {
            bool mexeu = eqPula;
            for (int b = 0; b < 4; ++b)
            {
                const float g0 = eqAgora.g[b], f0 = eqAgora.f[b];
                const float q0 = eqAgora.q[b];
                if (eqPula) { eqAgora.g[b] = eqAlvo.g[b]; eqAgora.f[b] = eqAlvo.f[b]; eqAgora.q[b] = eqAlvo.q[b]; continue; }
                eqAgora.g[b] += (eqAlvo.g[b] - eqAgora.g[b]) * kEq;
                eqAgora.f[b] *= std::pow (eqAlvo.f[b] / eqAgora.f[b], kEq);   // frequência e Q andam em oitavas
                eqAgora.q[b] *= std::pow (eqAlvo.q[b] / eqAgora.q[b], kEq);
                if (std::abs (eqAgora.g[b] - eqAlvo.g[b]) < 0.005f) eqAgora.g[b] = eqAlvo.g[b];
                if (std::abs (eqAgora.f[b] / eqAlvo.f[b] - 1.0f) < 0.0005f) eqAgora.f[b] = eqAlvo.f[b];
                if (std::abs (eqAgora.q[b] / eqAlvo.q[b] - 1.0f) < 0.0005f) eqAgora.q[b] = eqAlvo.q[b];
                mexeu = mexeu || eqAgora.g[b] != g0 || eqAgora.f[b] != f0 || eqAgora.q[b] != q0;
            }
            if (mexeu) eq.set (fs, (EqModel) eqModelo, eqAgora);
            eqPula = false;
        }
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
    // "3-Audio" / "12 Baixo": o número vira a posição (LED); na fita vai só o nome
    int k = 0;
    const auto t = nomeFaixa().trimStart();
    while (k < t.length() && k < 4 && juce::CharacterFunctions::isDigit (t[k])) ++k;
    if (k > 0 && k < t.length() && (t[k] == '-' || t[k] == ' ' || t[k] == '.' || t[k] == '_'))
        if (const auto resto = t.substring (k + 1).trim(); resto.isNotEmpty()) return resto;
    return t;
}

// Ableton: o título da janela do plugin é "<faixa>/<plugin>" ("1-Audio/MESA CANAL"). Só vale se um dos lados é o
// nosso plugin — em outro DAW o título é outra coisa e fica de fora.
void MesaProcessor::nomeDaJanela (const juce::String& titulo)
{
    const int b = titulo.lastIndexOfChar ('/');
    if (b <= 0) return;
    juce::String faixa;
    const auto esq = titulo.substring (0, b).trim(), dir = titulo.substring (b + 1).trim();
    if (dir.containsIgnoreCase ("MESA")) faixa = esq;
    else if (esq.containsIgnoreCase ("MESA")) faixa = dir;
    faixa = faixa.substring (0, 40);
    if (faixa.isEmpty()) return;
    {
        const juce::ScopedLock sl (nameLock);
        if (faixa == janelaName) return;
        janelaName = faixa;
    }
    registry->changed();
    ++mesa::Ponte::get().versao;
}
juce::String MesaProcessor::userName() const { const juce::ScopedLock sl (nameLock); return nameOverride; }
void MesaProcessor::renomeia (const char* utf8)
{
    { const juce::ScopedLock sl (nameLock); nameOverride = juce::String::fromUTF8 (utf8 != nullptr ? utf8 : "").trim().substring (0, 24); }
    registry->changed();
    ++mesa::Ponte::get().versao;
}

void MesaProcessor::updateTrackProperties (const TrackProperties& p)
{
    {
        const juce::ScopedLock sl (nameLock);
        hostName = p.name.value_or (juce::String());
        if (p.colourARGB) hostColour = juce::Colour (*p.colourARGB).withAlpha (1.0f);
    }
    registry->changed();
    ++mesa::Ponte::get().versao;
}

void MesaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("nome", userName(), nullptr);
    { const juce::ScopedLock sl (nameLock); state.setProperty ("janela", janelaName, nullptr); }
    state.setProperty ("uirec", uiRec.load(), nullptr);
    state.setProperty ("pos", posManual.load(), nullptr);
    state.setProperty ("uizoom", (double) uiZoom.load(), nullptr);
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
                janelaName = vt.getProperty ("janela").toString();   // o nome da faixa fica guardado: a console mostra sem abrir a janela
            }
            uiRec = juce::jlimit (0, 31, (int) vt.getProperty ("uirec", 0));
            if ((int) vt.getProperty ("uiw", 0) > 0) { uiW = (int) vt.getProperty ("uiw"); uiH = (int) vt.getProperty ("uih"); }
            uiGeral = (bool) vt.getProperty ("uigeral", false);
            posManual = juce::jlimit (0, 999, (int) vt.getProperty ("pos", 0));
            uiZoom = juce::jlimit (0.5f, 2.0f, (float) (double) vt.getProperty ("uizoom", 1.0));
            for (auto k : { "ui", "uiw", "uih", "uigeral", "pos", "uizoom", "uirec", "janela" }) vt.removeProperty (k, nullptr);
            // sessão de antes da 0.5 (sem modelo de compressor): o GRUPO usava o COLA 4K com ataque de 10 ms
            const bool antigo = ! isConsole && ! vt.getChildWithProperty ("id", id::dynm).isValid();
            apvts.replaceState (vt);
            if (antigo && juce::roundToInt (pMode->load()) == modeGroup)
            {
                if (auto* p = apvts.getParameter (id::dynm)) p->setValueNotifyingHost (p->convertTo0to1 (1.0f));
                if (auto* p = apvts.getParameter (id::atk))  p->setValueNotifyingHost (p->convertTo0to1 (8.0f));
            }
            dirty = true;
            registry->changed();
            ++mesa::Ponte::get().versao;
        }
}

// ---- mesa::Canal ----
float MesaProcessor::valor (const char* pid) const
{
    if (auto* p = apvts.getParameter (pid)) return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}
float MesaProcessor::norm (const char* pid) const
{
    if (auto* p = apvts.getParameter (pid)) return p->getValue();
    return 0.0f;
}
float MesaProcessor::normPadrao (const char* pid) const
{
    if (auto* p = apvts.getParameter (pid)) return p->getDefaultValue();
    return 0.0f;
}
void MesaProcessor::mexe (const char* pid, float v)
{
    if (auto* p = apvts.getParameter (pid)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
}
void MesaProcessor::gesto (const char* pid, bool inicio)
{
    if (auto* p = apvts.getParameter (pid)) { if (inicio) p->beginChangeGesture(); else p->endChangeGesture(); }
}
static int copiaUtf8 (const juce::String& s, char* out, int n)
{
    if (out == nullptr || n <= 0) return 0;
    const auto bytes = (int) s.copyToUTF8 (out, (size_t) n);
    return juce::jmax (0, bytes - 1);
}
// texto que depende do modelo (o DAW mostra o valor cru; a tela mostra o que o circuito faz de verdade)
static juce::String msTexto (float ms)
{
    if (ms < 1.0f) return juce::String (juce::roundToInt (ms * 1000.0f)) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb5s"));
    if (ms < 1000.0f) return juce::String (ms, ms < 10.0f ? 1 : 0) + " ms";
    return juce::String (ms / 1000.0f, 2) + " s";
}
bool MesaProcessor::textoModelo (const juce::String& pid, juce::String& t) const
{
    if (isConsole) return false;
    const auto dm = (DynModel) juce::jlimit (0, 3, juce::roundToInt (pDynm->load()));
    const auto em = (EqModel) juce::jlimit (0, 2, juce::roundToInt (pEqm->load()));
    if (pid == id::atk) { t = msTexto (dynAtkMs (dm, pAtk->load())); return true; }
    if (pid == id::rel)
    {
        const float r = dynRelS (dm, pRel->load());
        t = r < 0 ? juce::String ("AUTO") : msTexto (r * 1000.0f);
        if (dm == DynModel::opto) t = "2 tempos, " + t;
        return true;
    }
    if (pid == id::rat)
    {
        const float r = dynRatio (dm, pRat->load());
        if (r > 50.0f) t = "ALL";
        else if (dm == DynModel::opto) t = (r < 8.0f ? "COMP ~" : "LIMIT ~") + juce::String (1.0f + 0.6f * (r - 1.0f), 1) + ":1";
        else t = juce::String (r, r < 2.0f || r != std::floor (r) ? 1 : 0) + ":1";
        return true;
    }
    for (int b = 0; b < 4; ++b)
    {
        if (pid == id::eqG[b]) { t = dbText (eqSnapG (em, pEqG[b]->load())); return true; }
        if (pid == id::eqF[b]) { t = hzText (eqSnapF (em, b, pEqF[b]->load())); return true; }
    }
    for (int b = 0; b < 2; ++b)
        if (pid == id::eqQ[b])
        {
            const float q = pQ[b]->load();
            t = em == EqModel::brit81 ? (brit81Q (q) > 1.0f ? juce::String ("HI Q") : juce::String ("Q 0.7"))
              : em == EqModel::api550 ? juce::String (juce::CharPointer_UTF8 ("Q proporcional \xc3\x97")) + juce::String (q, 2)
              : "Q " + juce::String (q, 2);
            return true;
        }
    return false;
}
int MesaProcessor::texto (const char* pid, char* out, int n) const
{
    if (juce::String t; textoModelo (pid, t)) return copiaUtf8 (t, out, n);
    if (auto* p = apvts.getParameter (pid)) return copiaUtf8 (p->getCurrentValueAsText(), out, n);
    return copiaUtf8 ({}, out, n);
}
int MesaProcessor::nome (char* out, int n) const { return copiaUtf8 (displayName(), out, n); }
std::uint32_t MesaProcessor::cor() const
{
    const juce::ScopedLock sl (nameLock);
    return hostColour.getARGB();
}
void MesaProcessor::medidores (float* o) const
{
    o[0] = peakL; o[1] = peakR; o[2] = rmsL; o[3] = rmsR; o[4] = grDb; o[5] = inVu; o[6] = inPeak; o[7] = ppm;
}

// Posição na console = o número da faixa no DAW. Ordem de preferência: o que o usuário escolheu na console; o índice
// que o DAW manda (VST3: "channel index"); o número no começo do nome da faixa ("3-Audio", "12 Baixo"); 0 = não se sabe
// (vai para o fim, na ordem em que foi aberto).
int MesaProcessor::posicao() const
{
    if (const int m = posManual.load(); m > 0) return m;
    if (const int h = faixaHost.load(); h > 0) return h;
    const juce::ScopedLock sl (nameLock);
    const auto t = nomeFaixa().trimStart();
    int n = 0, k = 0;
    while (k < t.length() && k < 4 && juce::CharacterFunctions::isDigit (t[k])) n = n * 10 + (t[k++] - '0');
    return k > 0 ? n : 0;
}

void MesaProcessor::posiciona (int n)
{
    posManual = juce::jlimit (0, 999, n);
    registry->changed();
    ++mesa::Ponte::get().versao;
}

// chamado pelo invólucro VST3 do JUCE (remendo no instrumentos/CMakeLists.txt, só nos alvos da MESA) quando o DAW
// informa o índice da faixa. Pode vir fora da thread da interface: só mexe em atômicos.
// A declaração do remendo fica dentro de uma função do namespace juce: o clang a liga a juce::, o MSVC a ::. Existem
// as duas.
void ispIndiceDaFaixa (juce::AudioProcessor* p, int indice)
{
    if (auto* m = dynamic_cast<MesaProcessor*> (p))
    {
        if (m->faixaHost.exchange (juce::jmax (0, indice)) != indice) ++mesa::Ponte::get().versao;
    }
}
namespace juce { void ispIndiceDaFaixa (AudioProcessor* p, int indice) { ::ispIndiceDaFaixa (p, indice); } }

#ifndef MESA_CONSOLE
 #define MESA_CONSOLE 1
#endif

juce::AudioProcessor* mesaCriaCanal() { return new MesaProcessor (false); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MesaProcessor (MESA_CONSOLE != 0);
}
