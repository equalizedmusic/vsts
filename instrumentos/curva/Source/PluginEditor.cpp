#include "PluginEditor.h"
#include "EditorTema.h"

using namespace tema;

CurvaEditor::CurvaEditor (CurvaProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    janela.resize ((size_t) N);
    for (int i = 0; i < N; ++i) janela[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) N);
    buf.assign ((size_t) N * 2, 0.0f);
    acum.assign ((size_t) N / 2 + 2, 0.0f);
    for (int i = 0; i < NP; ++i) freq[(size_t) i] = (float) (fMin * std::pow (fMax / fMin, i / (double) (NP - 1)));
    specIn.fill (-120); specOut.fill (-120); holdIn.fill (-120); holdOut.fill (-120);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (750, 564, 2000, 1504);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (750, 2000, w0) : baseW;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));
    startTimerHz (30);
}

void CurvaEditor::resized() { proc.uiW = getWidth(); }

// ───────────────────────────── análise ─────────────────────────────

void CurvaEditor::espectro (const std::vector<float>& anel, int fim, std::array<float, NP>& alvo, std::array<float, NP>& pico, bool primeiro)
{
    const double fs = proc.sampleRate();
    float soma = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        buf[(size_t) i] = anel[(size_t) ((fim - N + i) & CurvaProcessor::anelMask)] * janela[(size_t) i];
        soma += janela[(size_t) i];
    }
    std::fill (buf.begin() + N, buf.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (buf.data(), true);
    // potência por bin (seno de amplitude 1 = 0 dB) e soma acumulada para a média por faixa de oitava
    const float k = 2.0f / soma;
    acum[0] = 0.0f;
    for (int b = 0; b <= N / 2; ++b)
    {
        const float m = buf[(size_t) b] * k;
        acum[(size_t) b + 1] = acum[(size_t) b] + m * m;
    }
    auto potencia = [this] (double b)   // potência interpolada num bin fracionário
    {
        const int i = juce::jlimit (0, N / 2 - 1, (int) b);
        const double t = juce::jlimit (0.0, 1.0, b - i);
        const double p0 = acum[(size_t) i + 1] - acum[(size_t) i], p1 = acum[(size_t) i + 2] - acum[(size_t) i + 1];
        return p0 + (p1 - p0) * t;
    };
    static const double larguras[4] = { 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0, 1.0 / 24.0 };
    const double oit = larguras[juce::jlimit (0, 3, proc.suave.load())];
    static const float tilts[3] = { 0.0f, 3.0f, 4.5f };
    const float tilt = tilts[juce::jlimit (0, 2, proc.tilt.load())];
    const double binHz = fs / N;
    for (int i = 0; i < NP; ++i)
    {
        const double f = freq[(size_t) i];
        const double lo = f * std::pow (2.0, -oit / 2) / binHz, hi = f * std::pow (2.0, oit / 2) / binHz;
        double p;
        if (hi - lo < 1.0) p = potencia (f / binHz);
        else
        {
            const int a = juce::jlimit (0, N / 2, (int) std::floor (lo)), b = juce::jlimit (a + 1, N / 2 + 1, (int) std::ceil (hi));
            p = (acum[(size_t) b] - acum[(size_t) a]) / (double) (b - a);
        }
        float db = (float) (10.0 * std::log10 (p + 1e-14)) + tilt * (float) std::log2 (f / 1000.0);
        if (f > fs * 0.5) db = -140.0f;
        float& v = alvo[(size_t) i];
        if (primeiro) v = db;
        else v = db > v ? v + (db - v) * 0.7f : v + (db - v) * 0.18f;   // sobe rápido, desce macio
        float& h = pico[(size_t) i];
        h = juce::jmax (v, h - 0.35f);                                    // PEAK HOLD cai ~10 dB/s
    }
}

void CurvaEditor::medidores()
{
    for (int i = 0; i < 4; ++i)
    {
        const float p = proc.pico[i].exchange (0.0f);
        const float db = juce::Decibels::gainToDecibels (p, -100.0f);
        nivel[i] = db > nivel[i] ? db : juce::jmax (db, nivel[i] - 0.8f);   // ~24 dB/s
        if (db >= nivelHold[i]) { nivelHold[i] = db; holdCont[i] = 45; }
        else if (--holdCont[i] < 0) nivelHold[i] = juce::jmax (nivel[i], nivelHold[i] - 0.6f);
        rmsDb[i] = juce::Decibels::gainToDecibels (proc.rms[i].load(), -100.0f);
    }
    const float r = proc.reducaoSafe.exchange (0.0f);
    safeDb = r < safeDb ? r : safeDb + (r - safeDb) * 0.15f;

    // estéreo da mix: largura (S/M) e balanço (L/R)
    double sl = 0, sr = 0, sm = 0, ss = 0, pk = 0;
    for (int i = 0; i < CurvaProcessor::anelSt; ++i)
    {
        const double l = proc.stL[(size_t) i], rr = proc.stR[(size_t) i];
        sl += l * l; sr += rr * rr;
        sm += (l + rr) * (l + rr); ss += (l - rr) * (l - rr);
        pk = juce::jmax (pk, std::abs (l), std::abs (rr));
    }
    larguraSt = sm + ss > 1e-9 ? (float) (ss / (sm + ss)) : larguraSt;
    balanco = sl > 1e-12 && sr > 1e-12 ? (float) (10.0 * std::log10 (sr / sl)) : 0.0f;
    const float alvoEsc = (float) (0.85 / juce::jmax (0.05, pk));
    escalaScope += (alvoEsc - escalaScope) * (alvoEsc < escalaScope ? 0.5f : 0.05f);
}

void CurvaEditor::timerCallback()
{
    const int fim = proc.escrita.load();
    espectro (proc.anelIn, fim, specIn, holdIn, specPrimeiro);
    espectro (proc.anelOut, fim, specOut, holdOut, specPrimeiro);
    specPrimeiro = false;
    medidores();
    repaint();
}

// ───────────────────────────── desenho ─────────────────────────────

void CurvaEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    alvos.clear();
    g.setGradientFill (juce::ColourGradient (cFundoA, 0, 0, cFundoB, 0, (float) baseH, false));
    g.fillRect (juce::Rectangle<float> (0, 0, (float) baseW, (float) baseH));

    topo (g);
    painelEspectro (g, { 16, 80, 704, 404 });
    painelNiveis (g, { 734, 80, 250, 262 });
    painelLoudness (g, { 734, 350, 250, 134 });
    painelControles (g, { 16, 494, 456, 242 });
    painelOuvido (g, { 482, 494, 238, 242 });
    painelStereo (g, { 734, 494, 250, 242 });
}

void CurvaEditor::topo (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 0, (float) baseW, 68);
    g.setGradientFill (juce::ColourGradient (cTopoA, 0, 0, cTopoB, 0, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (0, r.getBottom(), (float) baseW, r.getBottom(), 1.0f);

    // marca: um contorno de mesma audibilidade estilizado + CURVA
    {
        juce::Path p;
        for (int i = 0; i <= 40; ++i)
        {
            const double hz = 20.0 * std::pow (12500.0 / 20.0, i / 40.0);
            const float x = 20.0f + 34.0f * (float) i / 40.0f;
            const float y = 50.0f - (float) juce::jlimit (-10.0, 60.0, curva::iso226::contorno (hz, 40.0)) * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        juce::ColourGradient cg (cOut, 20, 0, cCurva, 54, 0, false);
        g.setGradientFill (comAlfa (cg, 0.25f));
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (cg);
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (cTexto);
    g.setFont (fonte (30.0f, true).withExtraKerningFactor (0.12f));
    g.drawText ("CURVA", juce::Rectangle<float> (64, 10, 160, 34), juce::Justification::centredLeft);
    g.setColour (cApagado);
    g.setFont (fonte (9.5f).withExtraKerningFactor (0.18f));
    g.drawText (u8 ("EQUAL-LOUDNESS MONITOR  ·  ISO 226"), juce::Rectangle<float> (65, 42, 260, 14), juce::Justification::centredLeft);

    // estado: você ouve a X → soa como Y
    const auto a = proc.ajuste();
    const bool on = proc.ligado();
    {
        const float x0 = 300;
        g.setFont (fonte (10.0f).withExtraKerningFactor (0.15f));
        g.setColour (cApagado);
        g.drawText (u8 ("VOCÊ OUVE A"), juce::Rectangle<float> (x0, 12, 120, 14), juce::Justification::centredLeft);
        g.drawText ("SOA COMO", juce::Rectangle<float> (x0 + 170, 12, 120, 14), juce::Justification::centredLeft);
        g.setFont (fonte (22.0f, true));
        g.setColour (on ? cOut : cApagado);
        g.drawText (juce::String (a.listen, 1), juce::Rectangle<float> (x0, 26, 70, 28), juce::Justification::centredLeft);
        g.setColour (on ? cTexto : cApagado);
        g.drawText (juce::String (a.target, 1), juce::Rectangle<float> (x0 + 170, 26, 70, 28), juce::Justification::centredLeft);
        g.setFont (fonte (10.0f));
        g.setColour (cApagado);
        g.drawText ("phon", juce::Rectangle<float> (x0 + 56, 34, 40, 16), juce::Justification::centredLeft);
        g.drawText ("phon", juce::Rectangle<float> (x0 + 226, 34, 40, 16), juce::Justification::centredLeft);
        juce::Path s;
        s.startNewSubPath (x0 + 100, 40); s.lineTo (x0 + 146, 40);
        g.setColour (on ? cCurva : cApagado.withAlpha (0.5f));
        g.strokePath (s, juce::PathStrokeType (2.0f));
        juce::Path ponta; ponta.addTriangle (x0 + 154, 40, x0 + 144, 35, x0 + 144, 45);
        g.fillPath (ponta);
    }

    // aviso: só para o monitor
    {
        const juce::Rectangle<float> b (640, 14, 218, 40);
        g.setColour (cAmbar.withAlpha (0.10f));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (cAmbar.withAlpha (0.85f));
        g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.2f);
        juce::Path tri; tri.addTriangle (b.getX() + 20, b.getY() + 10, b.getX() + 10, b.getY() + 29, b.getX() + 30, b.getY() + 29);
        g.fillPath (tri);
        g.setColour (cTopoB);
        g.setFont (fonte (11.0f, true));
        g.drawText ("!", juce::Rectangle<float> (b.getX() + 11, b.getY() + 15, 18, 13), juce::Justification::centred);
        g.setColour (cAmbar);
        g.setFont (fonte (10.5f, true).withExtraKerningFactor (0.12f));
        g.drawText (u8 ("SÓ PARA O MONITOR"), juce::Rectangle<float> (b.getX() + 40, b.getY() + 5, 176, 15), juce::Justification::centredLeft);
        g.setFont (fonte (9.5f));
        g.setColour (cAmbar.withAlpha (0.8f));
        g.drawText ("desligue antes de exportar", juce::Rectangle<float> (b.getX() + 40, b.getY() + 20, 176, 14), juce::Justification::centredLeft);
    }

    // ON / OFF
    {
        const juce::Rectangle<float> b (870, 14, 74, 40);
        g.setColour (on ? cOut.withAlpha (0.14f) : juce::Colour (0xff1a2030));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (on ? cOut : cBorda);
        g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.2f);
        const juce::Point<float> c (b.getX() + 20, b.getCentreY());
        juce::Path pw;
        pw.addCentredArc (c.x, c.y, 8, 8, 0, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
        g.setColour (on ? cOut : cApagado);
        g.strokePath (pw, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (c.x, c.y - 10, c.x, c.y - 2, 2.0f);
        g.setFont (fonte (12.0f, true).withExtraKerningFactor (0.1f));
        g.drawText (on ? "ON" : "OFF", juce::Rectangle<float> (b.getX() + 34, b.getY(), 36, b.getHeight()), juce::Justification::centredLeft);
        alvos.push_back ({ b, [this] { alternaParam ("on"); }, {} });
    }

    // menu
    {
        const juce::Rectangle<float> b (952, 14, 32, 40);
        g.setColour (cApagado);
        for (int i = 0; i < 3; ++i) g.fillRoundedRectangle (b.getX() + 8, b.getY() + 13 + (float) i * 6.0f, 16, 2.2f, 1.0f);
        alvos.push_back ({ b, [this] { menu(); }, {} });
    }
}

void CurvaEditor::alternaParam (const char* id)
{
    if (auto* pr = proc.apvts.getParameter (id))
    {
        pr->beginChangeGesture();
        pr->setValueNotifyingHost (pr->getValue() > 0.5f ? 0.0f : 1.0f);
        pr->endChangeGesture();
    }
}

void CurvaEditor::chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool aceso, juce::Colour cor,
                        std::function<void()> acao, float fonteH)
{
    g.setColour (aceso ? cor.withAlpha (0.18f) : juce::Colour (0xff161c2b));
    g.fillRoundedRectangle (r, 3.5f);
    g.setColour (aceso ? cor.withAlpha (0.9f) : cBorda);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.5f, 1.0f);
    g.setColour (aceso ? cor.brighter (0.3f) : cApagado);
    g.setFont (fonte (fonteH, true).withExtraKerningFactor (0.08f));
    g.drawText (t, r, juce::Justification::centred);
    alvos.push_back ({ r, std::move (acao), {} });
}

void CurvaEditor::painelEspectro (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r, "SPECTRUM", proc.ligado() ? u8 ("IN = mix  ·  OUT = monitor  ·  magenta = correção") : u8 ("CURVA desligada: OUT = IN"));

    // botões do cabeçalho (da direita para a esquerda)
    {
        float x = r.getRight() - 10;
        auto bt = [&] (float w) { x -= w; const juce::Rectangle<float> b (x, r.getY() + 6, w, 18); x -= 4; return b; };
        auto rot = [&] (const char* t, float w)
        {
            g.setColour (cApagado); g.setFont (fonte (9.0f).withExtraKerningFactor (0.12f));
            x -= w; g.drawText (t, juce::Rectangle<float> (x, r.getY() + 6, w, 18), juce::Justification::centredRight); x -= 10;
        };
        static const char* tiltN[3] = { "0", "3", "4.5" };
        for (int i = 2; i >= 0; --i)
            chip (g, bt (i == 2 ? 30.0f : 22.0f), tiltN[i], proc.tilt.load() == i, cViol, [this, i] { proc.tilt = i; });
        rot ("TILT", 28);
        static const char* suaveN[4] = { "1/3", "1/6", "1/12", "1/24" };
        for (int i = 3; i >= 0; --i)
            chip (g, bt (i >= 2 ? 32.0f : 28.0f), suaveN[i], proc.suave.load() == i, cViol, [this, i] { proc.suave = i; });
        rot ("SMOOTH", 46);
        x -= 4;
        chip (g, bt (40), "HOLD", proc.verHold.load(), cTexto, [this] { proc.verHold = ! proc.verHold.load(); });
        chip (g, bt (36), "OUT", proc.verOut.load(), cOut, [this] { proc.verOut = ! proc.verOut.load(); });
        chip (g, bt (30), "IN", proc.verIn.load(), cIn, [this] { proc.verIn = ! proc.verIn.load(); });
    }

    const juce::Rectangle<float> p (r.getX() + 38, r.getY() + 32, r.getWidth() - 38 - 40, r.getHeight() - 32 - 24);
    rPlot = p;
    g.setColour (cTela);
    g.fillRoundedRectangle (p.expanded (1), 3.0f);

    // regiões
    struct Regiao { double a, b; const char* n; };
    static const Regiao reg[7] = { { 20, 60, "SUB" }, { 60, 250, "LOW" }, { 250, 500, "LOW-MID" }, { 500, 2000, "MID" },
                                   { 2000, 4000, "HIGH-MID" }, { 4000, 10000, "HIGH" }, { 10000, 20000, "AIR" } };
    for (int i = 0; i < 7; ++i)
    {
        const float xa = xDeFreq (reg[i].a, p), xb = xDeFreq (reg[i].b, p);
        if (i % 2 == 1) { g.setColour (juce::Colours::white.withAlpha (0.014f)); g.fillRect (xa, p.getY(), xb - xa, p.getHeight()); }
        g.setColour (cApagado.withAlpha (0.45f));
        g.setFont (fonte (8.5f).withExtraKerningFactor (0.15f));
        g.drawText (reg[i].n, juce::Rectangle<float> (xa, p.getY() + 3, xb - xa, 12), juce::Justification::centred);
    }

    // grade de frequência
    for (double dec : { 10.0, 100.0, 1000.0, 10000.0 })
        for (int m = 1; m <= 9; ++m)
        {
            const double hz = dec * m;
            if (hz < fMin || hz > fMax) continue;
            g.setColour (m == 1 ? cGradeF : cGrade);
            const float x = xDeFreq (hz, p);
            g.drawLine (x, p.getY(), x, p.getBottom(), 1.0f);
        }
    g.setFont (fonte (9.0f));
    for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xDeFreq (hz, p);
        g.setColour (cApagado);
        g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 20, p.getBottom() + 4, 40, 14), juce::Justification::centred);
    }
    // grade de dB (esquerda) e da correção (direita)
    for (float db = 0.0f; db >= dbBase; db -= 12.0f)
    {
        const float y = yDeDb (db, p);
        g.setColour (db == 0.0f ? cGradeF : cGrade);
        g.drawLine (p.getX(), y, p.getRight(), y, 1.0f);
        g.setColour (cApagado);
        g.drawText (db == 0.0f ? juce::String ("0") : sinal (db, 0), juce::Rectangle<float> (r.getX() + 2, y - 7, 32, 14), juce::Justification::centredRight);
    }
    for (float db = -curvaMax; db <= curvaMax; db += 12.0f)
    {
        const float y = yDeCurva (db, p);
        g.setColour (cCurva.withAlpha (0.55f));
        g.drawText (db == 0.0f ? juce::String ("0") : sinal (db, 0), juce::Rectangle<float> (p.getRight() + 4, y - 7, 32, 14), juce::Justification::centredLeft);
    }
    g.setColour (cApagado.withAlpha (0.8f));
    g.setFont (fonte (8.0f).withExtraKerningFactor (0.1f));
    g.drawText ("dBFS", juce::Rectangle<float> (r.getX() + 2, p.getBottom() + 4, 34, 14), juce::Justification::centredRight);
    g.setColour (cCurva.withAlpha (0.6f));
    g.drawText ("dB EQ", juce::Rectangle<float> (p.getRight() + 2, p.getBottom() + 4, 36, 14), juce::Justification::centredLeft);

    g.saveState();
    g.reduceClipRegion (p.toNearestInt());

    auto caminho = [&] (const std::array<float, NP>& v, bool fechado)
    {
        juce::Path c;
        for (int i = 0; i < NP; ++i)
        {
            const float x = xDeFreq (freq[(size_t) i], p), y = yDeDb (v[(size_t) i], p);
            if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
        }
        if (fechado)
        {
            c.lineTo (p.getRight(), p.getBottom() + 2);
            c.lineTo (p.getX(), p.getBottom() + 2);
            c.closeSubPath();
        }
        return c;
    };

    // IN (mix): linha cinza com um véu
    if (proc.verIn.load())
    {
        g.setColour (cIn.withAlpha (0.07f));
        g.fillPath (caminho (specIn, true));
        g.setColour (cIn.withAlpha (0.85f));
        g.strokePath (caminho (specIn, false), juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));
        if (proc.verHold.load() && ! proc.verOut.load())
        {
            g.setColour (cIn.withAlpha (0.35f));
            g.strokePath (caminho (holdIn, false), juce::PathStrokeType (0.8f));
        }
    }
    // OUT (monitor): preenchimento em degradê + linha ciano→violeta com brilho
    if (proc.verOut.load())
    {
        g.setGradientFill (juce::ColourGradient (cOut.withAlpha (0.38f), 0, p.getY() + p.getHeight() * 0.25f,
                                                 cViol.withAlpha (0.02f), 0, p.getBottom(), false));
        g.fillPath (caminho (specOut, true));
        const auto l = caminho (specOut, false);
        juce::ColourGradient linha (cOut, p.getX(), 0, cViol, p.getRight(), 0, false);
        linha.addColour (0.5, cOut.interpolatedWith (cViol, 0.35f));
        g.setGradientFill (comAlfa (linha, 0.25f));
        g.strokePath (l, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (linha);
        g.strokePath (l, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (proc.verHold.load())
        {
            g.setColour (juce::Colours::white.withAlpha (0.32f));
            g.strokePath (caminho (holdOut, false), juce::PathStrokeType (0.8f));
        }
        // pico do espectro: ponto + Hz e nota
        int im = 0;
        for (int i = 1; i < NP; ++i) if (specOut[(size_t) i] > specOut[(size_t) im]) im = i;
        if (specOut[(size_t) im] > dbBase + 6)
        {
            const float x = xDeFreq (freq[(size_t) im], p), y = yDeDb (specOut[(size_t) im], p);
            g.setColour (cOut.withAlpha (0.3f)); g.fillEllipse (x - 6, y - 6, 12, 12);
            g.setColour (juce::Colours::white); g.fillEllipse (x - 2.5f, y - 2.5f, 5, 5);
            const auto f9 = fonte (9.0f, true);
            g.setFont (f9);
            const auto t = nomeHz (freq[(size_t) im]) + "  " + nota (freq[(size_t) im]);
            const float w = largura (f9, t) + 10;
            const float tx = juce::jlimit (p.getX() + 2, p.getRight() - w - 2, x - w / 2);
            const float ty = juce::jmax (p.getY() + 16, y - 22);
            g.setColour (cTela.withAlpha (0.8f)); g.fillRoundedRectangle (tx, ty, w, 14, 3);
            g.setColour (cTexto); g.drawText (t, juce::Rectangle<float> (tx, ty, w, 14), juce::Justification::centred);
        }
    }

    // curva de correção (eixo da direita)
    {
        const auto a = proc.ajuste();
        const bool on = proc.ligado();
        const float y0 = yDeCurva (0, p);
        juce::Path c, area;
        for (int i = 0; i <= 240; ++i)
        {
            const double hz = fMin * std::pow (fMax / fMin, i / 240.0);
            const float x = xDeFreq (hz, p), y = yDeCurva ((float) curva::compensacao (hz, a), p);
            if (i == 0) { c.startNewSubPath (x, y); area.startNewSubPath (x, y0); area.lineTo (x, y); }
            else { c.lineTo (x, y); area.lineTo (x, y); }
        }
        area.lineTo (p.getRight(), y0);
        area.closeSubPath();
        juce::Path zero, tracejado;
        zero.startNewSubPath (p.getX(), y0); zero.lineTo (p.getRight(), y0);
        const float dash[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (tracejado, zero, dash, 2);
        g.setColour (cCurva.withAlpha (0.25f));
        g.fillPath (tracejado);
        const auto cor = on ? cCurva : cApagado;
        g.setColour (cor.withAlpha (on ? 0.10f : 0.05f));
        g.fillPath (area);
        if (on) glow (g, c, cor, 2.0f);
        g.setColour (cor.withAlpha (on ? 1.0f : 0.5f));
        g.strokePath (c, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // leitura no mouse
    if (p.contains (mouse))
    {
        const double hz = freqDeX (mouse.x, p);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawLine (mouse.x, p.getY(), mouse.x, p.getBottom(), 1.0f);
        const int i = juce::jlimit (0, NP - 1, (int) std::lround ((NP - 1) * std::log (hz / fMin) / std::log (fMax / fMin)));
        const float corr = (float) curva::compensacao (hz, proc.ajuste());
        const juce::String linhas[4] = { nomeHz (hz) + "   " + nota (hz), "OUT   " + sinal (specOut[(size_t) i]) + " dB",
                                         "IN    " + sinal (specIn[(size_t) i]) + " dB", "EQ    " + sinal (corr) + " dB" };
        const float w = 132, h = 64;
        const float bx = mouse.x + 12 + w > p.getRight() ? mouse.x - 12 - w : mouse.x + 12;
        const float by = juce::jlimit (p.getY() + 4, p.getBottom() - h - 4, mouse.y - h / 2);
        g.setColour (cTela.withAlpha (0.92f)); g.fillRoundedRectangle (bx, by, w, h, 4);
        g.setColour (cBorda); g.drawRoundedRectangle (bx, by, w, h, 4, 1);
        const juce::Colour cores[4] = { cTexto, cOut, cIn, cCurva };
        for (int k = 0; k < 4; ++k)
        {
            g.setColour (cores[k]);
            g.setFont (fonte (k == 0 ? 10.0f : 9.5f, k == 0));
            g.drawText (linhas[k], juce::Rectangle<float> (bx + 8, by + 4 + (float) k * 14.0f, w - 12, 14), juce::Justification::centredLeft);
        }
        g.setColour (cOut);
        g.fillEllipse (mouse.x - 3, yDeDb (specOut[(size_t) i], p) - 3, 6, 6);
    }
    g.restoreState();
}

// ───────────────────────────── mouse ─────────────────────────────

void CurvaEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (e.mods.isPopupMenu()) { menu(); return; }
    for (auto it = alvos.rbegin(); it != alvos.rend(); ++it)
        if (it->r.contains (p))
        {
            if (it->param.isNotEmpty())
            {
                if (auto* pr = proc.apvts.getParameter (it->param))
                {
                    arrasto = pr; arrastoY = p.y; arrastoV = pr->getValue();
                    pr->beginChangeGesture();
                }
            }
            else if (it->acao) it->acao();
            repaint();
            return;
        }
}

void CurvaEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (arrasto == nullptr) return;
    const auto p = e.position / escala();
    const float sens = e.mods.isShiftDown() ? 1200.0f : 240.0f;   // SHIFT = ajuste fino
    arrasto->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, arrastoV + (arrastoY - p.y) / sens));
    repaint();
}

void CurvaEditor::mouseUp (const juce::MouseEvent&)
{
    if (arrasto != nullptr) arrasto->endChangeGesture();
    arrasto = nullptr;
}

void CurvaEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (auto& a : alvos)
        if (a.param.isNotEmpty() && a.r.contains (p))
            if (auto* pr = proc.apvts.getParameter (a.param))
            {
                pr->beginChangeGesture();
                pr->setValueNotifyingHost (pr->getDefaultValue());
                pr->endChangeGesture();
            }
}

void CurvaEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / escala();
    for (auto& a : alvos)
        if (a.param.isNotEmpty() && a.r.contains (p))
            if (auto* pr = proc.apvts.getParameter (a.param))
            {
                pr->beginChangeGesture();
                pr->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, pr->getValue() + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f)));
                pr->endChangeGesture();
                repaint();
                return;
            }
}

void CurvaEditor::mouseMove (const juce::MouseEvent& e) { mouse = e.position / escala(); }
void CurvaEditor::mouseExit (const juce::MouseEvent&) { mouse = { -1, -1 }; }
