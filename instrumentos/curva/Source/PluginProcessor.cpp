#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout parametros()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "CURVA ON", true));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "listen", 1 }, "LISTEN LEVEL",
               juce::NormalisableRange<float> ((float) curva::listenMin, (float) curva::listenMax, 0.5f), 60.0f,
               juce::AudioParameterFloatAttributes().withLabel ("phon")));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "target", 1 }, "TARGET",
               juce::NormalisableRange<float> ((float) curva::targetMin, (float) curva::targetMax, 0.5f), 83.0f,
               juce::AudioParameterFloatAttributes().withLabel ("phon")));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "amount", 1 }, "AMOUNT",
               juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
               juce::AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "OUTPUT",
               juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
               juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "autogain", 1 }, "AUTO GAIN", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "safe", 1 }, "SAFE LIMIT", true));
    return l;
}

CurvaProcessor::CurvaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CURVA", parametros())
{
    pOn = apvts.getRawParameterValue ("on");
    pListen = apvts.getRawParameterValue ("listen");
    pTarget = apvts.getRawParameterValue ("target");
    pAmount = apvts.getRawParameterValue ("amount");
    pOutput = apvts.getRawParameterValue ("output");
    pAuto = apvts.getRawParameterValue ("autogain");
    pSafe = apvts.getRawParameterValue ("safe");
    anelIn.assign ((size_t) anel, 0.0f);
    anelOut.assign ((size_t) anel, 0.0f);
    stL.assign ((size_t) anelSt, 0.0f);
    stR.assign ((size_t) anelSt, 0.0f);
    for (int i = 0; i < 4; ++i) { pico[i] = 0.0f; rms[i] = 0.0f; }
    startTimerHz (15);
}

bool CurvaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

curva::Ajuste CurvaProcessor::ajuste() const
{
    curva::Ajuste a;
    a.listen = pListen->load();
    a.target = pTarget->load();
    a.amount = pAmount->load() / 100.0;
    return a;
}

void CurvaProcessor::refazFiltro (bool agora)
{
    const juce::ScopedLock t (trava);
    const auto a = ajuste();
    if (! agora && a == feito && fs == fsFeito) return;
    const int taps = curva::tapsPara (fs);
    const auto h = curva::faseMinima (fs, taps, [&a] (double hz) { return curva::compensacao (hz, a); });
    juce::AudioBuffer<float> ir (1, taps);
    std::copy (h.begin(), h.end(), ir.getWritePointer (0));
    conv.loadImpulseResponse (std::move (ir), fs, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no,
                              juce::dsp::Convolution::Normalise::no);
    autoDb = (float) curva::autoGanhoDb (a);
    feito = a; fsFeito = fs;
}

void CurvaProcessor::timerCallback() { refazFiltro (false); }

void CurvaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock t (trava);   // o timer não refaz o filtro no meio do prepare (a trava é reentrante)
    fs = sampleRate;
    const int bloco = juce::jmax (64, samplesPerBlock);
    refazFiltro (true);   // antes do prepare: o filtro já vale no primeiro bloco
    conv.prepare ({ fs, (juce::uint32) bloco, 2 });
    conv.reset();
    seco.setSize (2, bloco);
    mistura.reset (fs, 0.03);
    mistura.setCurrentAndTargetValue (ligado() ? 1.0f : 0.0f);
    ganho.reset (fs, 0.05);
    ganho.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load() + (pAuto->load() > 0.5f ? autoDb.load() : 0.0f)));
    loud.prepara (fs);
    tp.zera();
    safe.prepara (fs);
    coefRms = std::exp (-1.0 / (0.3 * fs));
    coefCor = std::exp (-1.0 / (0.3 * fs));
    msL = msR = msLo = msRo = cLR = cLL = cRR = 0;
    truePeakMax = 0.0f;
    setLatencySamples (0);
}

void CurvaProcessor::zeraMedidores()
{
    loud.pedeZerar();
    truePeakMax = 0.0f;
}

void CurvaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    const int total = buffer.getNumSamples();
    if (nch == 0 || total == 0) return;
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, total);
    if (! licenca->liberada()) return;   // sem ativação: o som passa direto

    const bool temSafe = pSafe->load() > 0.5f;
    mistura.setTargetValue (ligado() ? 1.0f : 0.0f);
    ganho.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load() + (pAuto->load() > 0.5f ? autoDb.load() : 0.0f)));

    // em pedaços do tamanho preparado (host que manda bloco maior que o anunciado)
    const int passo = seco.getNumSamples();
    float picoBloco[4] = { 0, 0, 0, 0 }, reducao = 0.0f, tpBloco = 0.0f;
    int w = escrita.load(), ws = escritaSt.load();
    for (int ini = 0; ini < total; ini += passo)
    {
        const int n = juce::jmin (passo, total - ini);
        float* ch[2] = { buffer.getWritePointer (0, ini), nch > 1 ? buffer.getWritePointer (1, ini) : nullptr };
        const float* chc[2] = { ch[0], ch[1] };

        // ── medidores da MIX (antes da CURVA) ──
        loud.processa (chc, nch, n);
        tpBloco = juce::jmax (tpBloco, tp.processa (chc, nch, n));
        for (int i = 0; i < n; ++i)
        {
            const float l = ch[0][i], r = nch > 1 ? ch[1][i] : l;
            picoBloco[0] = juce::jmax (picoBloco[0], std::abs (l));
            picoBloco[1] = juce::jmax (picoBloco[1], std::abs (r));
            msL = l * l + (msL - l * l) * coefRms;
            msR = r * r + (msR - r * r) * coefRms;
            cLR = l * r + (cLR - l * r) * coefCor;
            cLL = l * l + (cLL - l * l) * coefCor;
            cRR = r * r + (cRR - r * r) * coefCor;
            anelIn[(size_t) ((w + i) & anelMask)] = 0.5f * (l + r);
            if ((i & 1) == 0) { stL[(size_t) (ws % anelSt)] = l; stR[(size_t) (ws % anelSt)] = r; ws = (ws + 1) % anelSt; }
        }

        // ── a CURVA ──
        for (int c = 0; c < nch; ++c) seco.copyFrom (c, 0, ch[c], n);
        {
            juce::dsp::AudioBlock<float> bloco (ch, (size_t) nch, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (bloco);
            conv.process (ctx);
        }
        for (int i = 0; i < n; ++i)
        {
            const float m = mistura.getNextValue(), g = ganho.getNextValue();
            for (int c = 0; c < nch; ++c)
                ch[c][i] = seco.getSample (c, i) * (1.0f - m) + ch[c][i] * g * m;
        }
        if (temSafe && (ligado() || mistura.isSmoothing()))
        {
            safe.processa (ch, nch, n);
            reducao = juce::jmin (reducao, safe.reducaoDb());
        }

        // ── medidores do MONITOR (depois da CURVA) ──
        for (int i = 0; i < n; ++i)
        {
            const float l = ch[0][i], r = nch > 1 ? ch[1][i] : l;
            picoBloco[2] = juce::jmax (picoBloco[2], std::abs (l));
            picoBloco[3] = juce::jmax (picoBloco[3], std::abs (r));
            msLo = l * l + (msLo - l * l) * coefRms;
            msRo = r * r + (msRo - r * r) * coefRms;
            anelOut[(size_t) ((w + i) & anelMask)] = 0.5f * (l + r);
        }
        w = (w + n) & anelMask;
    }
    escrita = w;
    escritaSt = ws;
    for (int i = 0; i < 4; ++i) pico[i] = juce::jmax (pico[i].load(), picoBloco[i]);   // a tela lê e zera
    rms[0] = (float) std::sqrt (msL); rms[1] = (float) std::sqrt (msR);
    rms[2] = (float) std::sqrt (msLo); rms[3] = (float) std::sqrt (msRo);
    truePeak = juce::jmax (truePeak.load(), tpBloco);
    if (tpBloco > truePeakMax.load()) truePeakMax = tpBloco;
    if (cLL > 1e-9 && cRR > 1e-9) correlacao = (float) juce::jlimit (-1.0, 1.0, cLR / std::sqrt (cLL * cRR));
    reducaoSafe = juce::jmin (reducaoSafe.load(), reducao);
}

juce::AudioProcessorEditor* CurvaProcessor::createEditor() { return new CurvaEditor (*this); }

void CurvaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiw", uiW.load(), nullptr);
    state.setProperty ("suave", suave.load(), nullptr);
    state.setProperty ("tilt", tilt.load(), nullptr);
    state.setProperty ("verin", verIn.load() ? 1 : 0, nullptr);
    state.setProperty ("verout", verOut.load() ? 1 : 0, nullptr);
    state.setProperty ("hold", verHold.load() ? 1 : 0, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void CurvaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiW = (int) vt.getProperty ("uiw", 0);
            suave = juce::jlimit (0, 3, (int) vt.getProperty ("suave", 2));
            tilt = juce::jlimit (0, 2, (int) vt.getProperty ("tilt", 2));
            verIn = (int) vt.getProperty ("verin", 1) != 0;
            verOut = (int) vt.getProperty ("verout", 1) != 0;
            verHold = (int) vt.getProperty ("hold", 1) != 0;
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CurvaProcessor(); }
