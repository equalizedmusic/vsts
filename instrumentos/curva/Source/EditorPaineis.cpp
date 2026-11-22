// CURVA: painéis da tela (LEVELS, LOUDNESS, controles, EQUAL LOUDNESS, STEREO) e o menu.
#include "PluginEditor.h"
#include "EditorTema.h"

using namespace tema;

void CurvaEditor::painelNiveis (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r, "LEVELS", "peak + RMS");
    const juce::Rectangle<float> area (r.getX() + 40, r.getY() + 52, r.getWidth() - 60, r.getHeight() - 52 - 36);
    auto yDe = [&] (float db) { return juce::jmap (juce::jlimit (mBase, mTopo, db), mTopo, mBase, area.getY(), area.getBottom()); };
    g.setFont (fonte (8.5f));
    for (float db : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f })
    {
        const float y = yDe (db);
        g.setColour (cGrade.brighter (0.2f));
        g.drawLine (area.getX() - 4, y, area.getRight() + 2, y, 1.0f);
        g.setColour (cApagado);
        g.drawText (db == 0.0f ? juce::String ("0") : sinal (db, 0), juce::Rectangle<float> (r.getX() + 4, y - 6, 30, 12), juce::Justification::centredRight);
    }
    const float wBar = 26, gap = 6, gapPar = 32;
    const float xs[4] = { area.getX() + 8, area.getX() + 8 + wBar + gap, area.getX() + 8 + 2 * wBar + gap + gapPar,
                          area.getX() + 8 + 3 * wBar + 2 * gap + gapPar };
    for (int i = 0; i < 4; ++i)
    {
        const juce::Rectangle<float> b (xs[i], area.getY(), wBar, area.getHeight());
        g.setColour (cTela); g.fillRoundedRectangle (b, 2.0f);
        // RMS: barra cheia em degradê verde→âmbar→vermelho
        const float yR = yDe (rmsDb[i]);
        juce::ColourGradient cg (cVerde, 0, yDe (-30), cVermelho, 0, yDe (0), false);
        cg.addColour (0.7, cAmbar);
        g.setGradientFill (cg);
        g.fillRect (b.getX() + 3, yR, b.getWidth() - 6, b.getBottom() - yR);
        // pico: faixa mais clara por cima do RMS e uma linha
        const float yP = yDe (nivel[i]);
        g.setColour (corNivel (nivel[i]).withAlpha (0.35f));
        g.fillRect (b.getX() + 3, yP, b.getWidth() - 6, juce::jmax (0.0f, yR - yP));
        g.setColour (corNivel (nivel[i]));
        g.fillRect (b.getX() + 3, yP - 1, b.getWidth() - 6, 2.0f);
        // hold
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (b.getX() + 1, yDe (nivelHold[i]) - 1, b.getWidth() - 2, 1.6f);
        if (nivelHold[i] > -0.1f) { g.setColour (cVermelho); g.fillRect (b.getX(), area.getY() - 6, b.getWidth(), 4.0f); }
    }
    g.setFont (fonte (9.0f, true).withExtraKerningFactor (0.1f));
    const char* lr[4] = { "L", "R", "L", "R" };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (cApagado);
        g.drawText (lr[i], juce::Rectangle<float> (xs[i], area.getBottom() + 3, wBar, 12), juce::Justification::centred);
    }
    // nomes e valores de pico (hold)
    for (int par = 0; par < 2; ++par)
    {
        const float x = xs[par * 2], w = 2 * wBar + gap;
        g.setColour (par == 0 ? cIn : cOut);
        g.setFont (fonte (9.5f, true).withExtraKerningFactor (0.15f));
        g.drawText (par == 0 ? "IN" : "OUT", juce::Rectangle<float> (x, area.getBottom() + 16, w, 14), juce::Justification::centred);
        const float pk = juce::jmax (nivelHold[par * 2], nivelHold[par * 2 + 1]);
        g.setFont (fonte (11.0f, true));
        g.setColour (pk > -0.1f ? cVermelho : cTexto);
        g.drawText (pk <= -99.0f ? u8 ("−∞") : sinal (pk), juce::Rectangle<float> (x - 6, r.getY() + 28, w + 12, 16), juce::Justification::centred);
    }
    // redução do SAFE
    {
        g.setColour (safeDb < -0.05f ? cAmbar : cApagado.withAlpha (0.6f));
        g.setFont (fonte (8.5f, true).withExtraKerningFactor (0.1f));
        g.drawText ("SAFE", juce::Rectangle<float> (r.getRight() - 40, r.getY() + 7, 30, 14), juce::Justification::centredRight);
        const juce::Rectangle<float> b (r.getRight() - 16, area.getY(), 6, area.getHeight());
        g.setColour (cTela); g.fillRect (b);
        const float h = juce::jlimit (0.0f, 1.0f, -safeDb / 12.0f) * b.getHeight();
        g.setColour (cAmbar); g.fillRect (b.getX(), b.getY(), b.getWidth(), h);
    }
}

void CurvaEditor::painelLoudness (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r, "LOUDNESS", u8 ("mix, antes da CURVA"));
    chip (g, { r.getRight() - 54, r.getY() + 6, 46, 18 }, "RESET", false, cTexto, [this] { proc.zeraMedidores(); }, 8.5f);

    const float I = proc.loud.integrado.load(), M = proc.loud.momentary.load(), S = proc.loud.shortTerm.load();
    const float tpMax = juce::Decibels::gainToDecibels (proc.truePeakMax.load(), -100.0f);
    g.setColour (cTexto);
    g.setFont (fonte (34.0f, true));
    g.drawText (lufs (I), juce::Rectangle<float> (r.getX() + 12, r.getY() + 30, 140, 40), juce::Justification::centredLeft);
    g.setColour (cApagado);
    g.setFont (fonte (9.0f, true).withExtraKerningFactor (0.15f));
    g.drawText ("LUFS  INTEGRATED", juce::Rectangle<float> (r.getX() + 14, r.getY() + 68, 130, 12), juce::Justification::centredLeft);

    auto linha = [&] (float x, float y, const juce::String& n, const juce::String& v, juce::Colour cv)
    {
        g.setColour (cApagado);
        g.setFont (fonte (8.5f, true).withExtraKerningFactor (0.12f));
        g.drawText (n, juce::Rectangle<float> (x, y, 76, 12), juce::Justification::centredLeft);
        g.setColour (cv);
        g.setFont (fonte (13.0f, true));
        g.drawText (v, juce::Rectangle<float> (x, y + 11, 76, 16), juce::Justification::centredLeft);
    };
    linha (r.getX() + 160, r.getY() + 30, "MOMENTARY", lufs (M), cTexto);
    linha (r.getX() + 160, r.getY() + 58, "SHORT-TERM", lufs (S), cTexto);
    const float y3 = r.getY() + 94;
    g.setColour (cBorda);
    g.drawLine (r.getX() + 10, y3 - 4, r.getRight() - 10, y3 - 4, 1.0f);
    const bool semI = I <= -69.9f, semTp = tpMax <= -99.0f;
    linha (r.getX() + 12, y3, "LRA", semI ? u8 ("—") : juce::String (proc.loud.lra.load(), 1) + " LU", cTexto);
    linha (r.getX() + 92, y3, "TRUE PEAK", semTp ? u8 ("—") : sinal (tpMax) + " dB", tpMax > -1.0f ? cVermelho : cTexto);
    linha (r.getX() + 184, y3, "PLR", semI || semTp ? u8 ("—") : juce::String (tpMax - I, 1), cTexto);
}

void CurvaEditor::knob (juce::Graphics& g, juce::Point<float> c, float raio, const char* id, const juce::String& rotulo,
                        const juce::String& valor, const juce::String& unidade, juce::Colour cor, bool grande)
{
    auto* pr = proc.apvts.getParameter (id);
    const float v = pr != nullptr ? pr->getValue() : 0.0f;
    const float a0 = -2.35f, a1 = 2.35f, av = a0 + (a1 - a0) * v;

    // anel de fundo + arco do valor com brilho
    juce::Path trilho, arco;
    trilho.addCentredArc (c.x, c.y, raio + 7, raio + 7, 0, a0, a1, true);
    arco.addCentredArc (c.x, c.y, raio + 7, raio + 7, 0, a0, juce::jmax (a0 + 0.001f, av), true);
    g.setColour (juce::Colour (0xff0b0f18));
    g.strokePath (trilho, juce::PathStrokeType (grande ? 6.0f : 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cor.withAlpha (0.2f));
    g.strokePath (arco, juce::PathStrokeType (grande ? 12.0f : 8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cor);
    g.strokePath (arco, juce::PathStrokeType (grande ? 4.0f : 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (grande)   // marcas de escala
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / 10.0f;
            const float r1 = raio + 13, r2 = raio + (i % 5 == 0 ? 19.0f : 16.0f);
            g.setColour (cApagado.withAlpha (0.6f));
            g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, c.x + std::sin (a) * r2, c.y - std::cos (a) * r2, 1.0f);
        }
    // corpo
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (juce::Rectangle<float> (raio * 2 + 4, raio * 2 + 4).withCentre (c.translated (0, 2)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c3550), c.x - raio * 0.6f, c.y - raio,
                                             juce::Colour (0xff121725), c.x + raio * 0.5f, c.y + raio, true));
    g.fillEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c));
    g.setColour (juce::Colour (0xff3a4562));
    g.drawEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c), 1.0f);
    g.setColour (juce::Colour (0xff0d111b));
    g.fillEllipse (juce::Rectangle<float> (raio * 1.5f, raio * 1.5f).withCentre (c));
    // ponteiro
    g.setColour (cor);
    g.drawLine (c.x + std::sin (av) * raio * 0.35f, c.y - std::cos (av) * raio * 0.35f,
                c.x + std::sin (av) * raio * 0.72f, c.y - std::cos (av) * raio * 0.72f, grande ? 3.0f : 2.2f);
    if (grande)   // valor no centro
    {
        g.setColour (cTexto);
        g.setFont (fonte (20.0f, true));
        g.drawText (valor, juce::Rectangle<float> (c.x - 40, c.y - 15, 80, 22), juce::Justification::centred);
        g.setColour (cApagado);
        g.setFont (fonte (9.0f));
        g.drawText (unidade, juce::Rectangle<float> (c.x - 40, c.y + 6, 80, 12), juce::Justification::centred);
    }
    else          // valor embaixo
    {
        g.setColour (cTexto);
        g.setFont (fonte (11.5f, true));
        g.drawText (valor + (unidade.isNotEmpty() ? " " + unidade : juce::String()),
                    juce::Rectangle<float> (c.x - 45, c.y + raio + 10, 90, 14), juce::Justification::centred);
    }
    g.setColour (cApagado);
    g.setFont (fonte (grande ? 10.0f : 9.0f, true).withExtraKerningFactor (0.15f));
    g.drawText (rotulo, juce::Rectangle<float> (c.x - 60, c.y - raio - (grande ? 36.0f : 26.0f), 120, 13), juce::Justification::centred);
    alvos.push_back ({ juce::Rectangle<float> (raio * 2 + 18, raio * 2 + 18).withCentre (c), {}, id });
}

void CurvaEditor::painelControles (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    const auto a = proc.ajuste();
    const auto valor = [this] (const char* id) { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.0f; };

    // LISTEN LEVEL (grande) + o nome do volume + atalhos
    const juce::Point<float> cL (r.getX() + 110, r.getY() + 98);
    knob (g, cL, 46, "listen", "LISTEN LEVEL", juce::String (a.listen, 1), "phon", cOut, true);
    g.setColour (cOut.withAlpha (0.9f));
    g.setFont (fonte (9.5f, true).withExtraKerningFactor (0.18f));
    g.drawText (nomeVolume ((float) a.listen), juce::Rectangle<float> (cL.x - 70, cL.y + 62, 140, 13), juce::Justification::centred);
    {
        struct Atalho { float v; const char* n; };
        static const Atalho atalhos[4] = { { 50, "NOITE" }, { 60, "BAIXO" }, { 70, "MÉDIO" }, { 83, "REF" } };
        const float w = 44, x0 = cL.x - (4 * w + 3 * 4) / 2;
        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> b (x0 + (float) i * (w + 4), r.getBottom() - 44, w, 32);
            const bool aceso = std::abs (a.listen - atalhos[i].v) < 0.26;
            g.setColour (aceso ? cOut.withAlpha (0.16f) : juce::Colour (0xff161c2b));
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (aceso ? cOut : cBorda);
            g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (aceso ? cTexto : cApagado);
            g.setFont (fonte (12.0f, true));
            g.drawText (juce::String ((int) atalhos[i].v), b.withHeight (19).translated (0, 2), juce::Justification::centred);
            g.setFont (fonte (7.5f, true).withExtraKerningFactor (0.1f));
            g.drawText (u8 (atalhos[i].n), b.withTrimmedTop (18).withHeight (11), juce::Justification::centred);
            const float alvoV = atalhos[i].v;
            alvos.push_back ({ b, [this, alvoV]
                               {
                                   if (auto* pr = proc.apvts.getParameter ("listen"))
                                   { pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->convertTo0to1 (alvoV)); pr->endChangeGesture(); }
                               }, {} });
        }
    }

    // separador
    g.setColour (cBorda);
    g.drawLine (r.getX() + 222, r.getY() + 16, r.getX() + 222, r.getBottom() - 16, 1.0f);

    // TARGET, AMOUNT, OUTPUT
    const float y = r.getY() + 64, x0 = r.getX() + 262, dx = 76;
    knob (g, { x0, y }, 24, "target", "TARGET", juce::String (a.target, 1), "phon", cAlvo, false);
    knob (g, { x0 + dx, y }, 24, "amount", "AMOUNT", juce::String ((int) std::lround (a.amount * 100)), "%", cCurva, false);
    knob (g, { x0 + 2 * dx, y }, 24, "output", "OUTPUT", sinal (valor ("output")), "dB", cViol, false);

    // AUTO GAIN e SAFE LIMIT
    const bool autoG = valor ("autogain") > 0.5f, seg = valor ("safe") > 0.5f;
    const float yb = r.getY() + 128;
    chip (g, { r.getX() + 238, yb, 100, 26 }, "AUTO GAIN", autoG, cVerde, [this] { alternaParam ("autogain"); }, 9.5f);
    chip (g, { r.getX() + 346, yb, 100, 26 }, "SAFE LIMIT", seg, cAmbar, [this] { alternaParam ("safe"); }, 9.5f);
    g.setFont (fonte (9.0f));
    g.setColour (cApagado);
    g.drawText (autoG ? sinal (proc.autoGanho()) + " dB" : u8 ("desligado"),
                juce::Rectangle<float> (r.getX() + 238, yb + 28, 100, 13), juce::Justification::centred);
    g.drawText (seg ? u8 ("teto −0.3 dBFS") : u8 ("desligado"),
                juce::Rectangle<float> (r.getX() + 346, yb + 28, 100, 13), juce::Justification::centred);

    // dica
    g.setColour (cApagado.withAlpha (0.85f));
    g.setFont (fonte (9.0f));
    g.drawFittedText (u8 ("LISTEN: o volume em que você está ouvindo agora. TARGET: o volume em que a mix tem que "
                          "soar certa (83 = referência de estúdio)."),
                      juce::Rectangle<int> ((int) r.getX() + 236, (int) r.getBottom() - 56, 212, 46), juce::Justification::centredLeft, 4);
}

void CurvaEditor::painelOuvido (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto a = proc.ajuste();
    chapa (g, r, "EQUAL LOUDNESS");
    // as duas curvas ISO 226 (relativas a 1 kHz): quanto mais alto, mais nível o ouvido pede
    const juce::Rectangle<float> p (r.getX() + 12, r.getY() + 30, r.getWidth() - 24, 100);
    g.setColour (cTela); g.fillRoundedRectangle (p, 3.0f);
    auto yDe = [&] (double db) { return juce::jmap ((float) juce::jlimit (-12.0, 66.0, db), 66.0f, -12.0f, p.getY() + 4, p.getBottom() - 4); };
    auto xDe = [&] (double hz) { return p.getX() + p.getWidth() * (float) (std::log (hz / 20.0) / std::log (16000.0 / 20.0)); };
    g.setColour (cGradeF);
    for (double hz : { 100.0, 1000.0, 10000.0 }) g.drawLine (xDe (hz), p.getY(), xDe (hz), p.getBottom(), 1.0f);
    g.drawLine (p.getX(), yDe (0), p.getRight(), yDe (0), 1.0f);
    juce::Path cl, ct;
    for (int i = 0; i <= 120; ++i)
    {
        const double hz = 20.0 * std::pow (16000.0 / 20.0, i / 120.0);
        const float x = xDe (hz), yl = yDe (curva::iso226::contorno (hz, a.listen)), yt = yDe (curva::iso226::contorno (hz, a.target));
        if (i == 0) { cl.startNewSubPath (x, yl); ct.startNewSubPath (x, yt); } else { cl.lineTo (x, yl); ct.lineTo (x, yt); }
    }
    juce::Path entre (cl);
    for (int i = 120; i >= 0; --i)
    {
        const double hz = 20.0 * std::pow (16000.0 / 20.0, i / 120.0);
        entre.lineTo (xDe (hz), yDe (curva::iso226::contorno (hz, a.target)));
    }
    entre.closeSubPath();
    g.setColour (cCurva.withAlpha (0.16f));
    g.fillPath (entre);
    juce::Path tr;
    const float dash[] = { 3.0f, 3.0f };
    juce::PathStrokeType (1.3f).createDashedStroke (tr, ct, dash, 2);
    g.setColour (cAlvo.withAlpha (0.8f));
    g.fillPath (tr);
    glow (g, cl, cOut, 1.6f);
    g.setColour (cOut);
    g.strokePath (cl, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
    g.setFont (fonte (8.5f, true));
    g.setColour (cOut);
    g.drawText (juce::String (juce::roundToInt (a.listen)) + " phon", juce::Rectangle<float> (p.getRight() - 64, p.getY() + 4, 60, 11), juce::Justification::centredRight);
    g.setColour (cAlvo);
    g.drawText (juce::String (juce::roundToInt (a.target)) + " phon", juce::Rectangle<float> (p.getRight() - 64, p.getY() + 15, 60, 11), juce::Justification::centredRight);
    g.setColour (cApagado);
    g.setFont (fonte (8.0f));
    g.drawText ("100", juce::Rectangle<float> (xDe (100) - 12, p.getBottom() - 12, 24, 10), juce::Justification::centred);
    g.drawText ("1k", juce::Rectangle<float> (xDe (1000) - 12, p.getBottom() - 12, 24, 10), juce::Justification::centred);
    g.drawText ("10k", juce::Rectangle<float> (xDe (10000) - 12, p.getBottom() - 12, 24, 10), juce::Justification::centred);

    // o que cada faixa ganha
    g.setColour (cApagado);
    g.setFont (fonte (8.5f, true).withExtraKerningFactor (0.12f));
    g.drawText (u8 ("O QUE A CURVA DEVOLVE"), juce::Rectangle<float> (r.getX() + 12, p.getBottom() + 6, 200, 12), juce::Justification::centredLeft);
    struct Banda { double hz; const char* n; };
    static const Banda bandas[7] = { { 40, "SUB" }, { 100, "LOW" }, { 300, "L-MID" }, { 1000, "MID" },
                                     { 3000, "H-MID" }, { 8000, "HIGH" }, { 14000, "AIR" } };
    const juce::Rectangle<float> bb (r.getX() + 12, p.getBottom() + 22, r.getWidth() - 24, r.getBottom() - p.getBottom() - 30);
    const float w = bb.getWidth() / 7.0f, meio = bb.getY() + bb.getHeight() * 0.62f, esc = (bb.getHeight() * 0.5f) / 20.0f;
    g.setColour (cGradeF);
    g.drawLine (bb.getX(), meio, bb.getRight(), meio, 1.0f);
    for (int i = 0; i < 7; ++i)
    {
        const float v = proc.ligado() ? (float) curva::compensacao (bandas[i].hz, a) : 0.0f;
        const float x = bb.getX() + (float) i * w;
        const float h = juce::jlimit (-bb.getHeight() * 0.3f, bb.getHeight() * 0.48f, v * esc);
        g.setColour (v >= 0 ? cCurva : cViol);
        g.fillRoundedRectangle (juce::Rectangle<float> (x + w * 0.25f, h >= 0 ? meio - h : meio, w * 0.5f, std::abs (h)), 1.5f);
        g.setColour (cTexto);
        g.setFont (fonte (8.5f, true));
        g.drawText (sinal (v, 0), juce::Rectangle<float> (x - 2, (h >= 0 ? meio - h : meio) - 13, w + 4, 12), juce::Justification::centred);
        g.setColour (cApagado);
        g.setFont (fonte (7.5f, true));
        g.drawText (bandas[i].n, juce::Rectangle<float> (x - 2, bb.getBottom() - 10, w + 4, 10), juce::Justification::centred);
    }
}

void CurvaEditor::painelStereo (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r, "STEREO", "mix");
    const juce::Rectangle<float> q (r.getX() + 12, r.getY() + 30, 160, 160);
    g.setColour (cTela); g.fillRoundedRectangle (q, 3.0f);
    const auto c = q.getCentre();
    const float h = q.getWidth() * 0.5f - 4;
    g.setColour (cGradeF);
    g.drawLine (c.x, q.getY() + 4, c.x, q.getBottom() - 4, 1.0f);
    g.drawLine (q.getX() + 4, c.y, q.getRight() - 4, c.y, 1.0f);
    g.setColour (cGrade.brighter (0.3f));
    g.drawLine (c.x - h * 0.7f, c.y - h * 0.7f, c.x + h * 0.7f, c.y + h * 0.7f, 1.0f);
    g.drawLine (c.x + h * 0.7f, c.y - h * 0.7f, c.x - h * 0.7f, c.y + h * 0.7f, 1.0f);
    g.drawEllipse (juce::Rectangle<float> (h * 2, h * 2).withCentre (c), 1.0f);
    g.setColour (cApagado);
    g.setFont (fonte (8.5f, true));
    g.drawText ("L", juce::Rectangle<float> (c.x - h * 0.7f - 12, c.y - h * 0.7f - 12, 12, 12), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (c.x + h * 0.7f, c.y - h * 0.7f - 12, 12, 12), juce::Justification::centred);
    g.drawText ("M", juce::Rectangle<float> (c.x + 2, q.getY() + 2, 12, 12), juce::Justification::centred);
    g.drawText ("S", juce::Rectangle<float> (q.getRight() - 14, c.y - 13, 12, 12), juce::Justification::centred);
    // pontos (as amostras mais novas mais fortes)
    const int fim = proc.escritaSt.load();
    for (int k = 0; k < CurvaProcessor::anelSt; ++k)
    {
        const int i = (fim - 1 - k + 2 * CurvaProcessor::anelSt) % CurvaProcessor::anelSt;
        const float l = proc.stL[(size_t) i], rr = proc.stR[(size_t) i];
        const float x = c.x + (rr - l) * 0.7071f * escalaScope * h, y = c.y - (l + rr) * 0.7071f * escalaScope * h;
        if (! q.contains (x, y)) continue;
        const float idade = 1.0f - (float) k / (float) CurvaProcessor::anelSt;
        g.setColour (cOut.interpolatedWith (cViol, 1.0f - idade).withAlpha (0.10f + 0.45f * idade * idade));
        g.fillRect (x - 0.75f, y - 0.75f, 1.5f, 1.5f);
    }

    // à direita: WIDTH, BALANCE, CORREL.
    auto valorRot = [&] (float y, const juce::String& n, const juce::String& v)
    {
        g.setColour (cApagado);
        g.setFont (fonte (8.5f, true).withExtraKerningFactor (0.12f));
        g.drawText (n, juce::Rectangle<float> (q.getRight() + 10, y, 70, 12), juce::Justification::centredLeft);
        g.setColour (cTexto);
        g.setFont (fonte (14.0f, true));
        g.drawText (v, juce::Rectangle<float> (q.getRight() + 10, y + 12, 70, 18), juce::Justification::centredLeft);
    };
    valorRot (q.getY() + 4, "WIDTH", juce::String ((int) std::lround (larguraSt * 100)) + "%");
    valorRot (q.getY() + 48, "BALANCE", std::abs (balanco) < 0.1f ? juce::String ("C")
                                        : juce::String (balanco > 0 ? "R " : "L ") + juce::String (std::abs (balanco), 1));
    const float cor = proc.correlacao.load();
    valorRot (q.getY() + 92, "CORREL.", (cor >= 0 ? juce::String ("+") : u8 ("−")) + juce::String (std::abs (cor), 2));

    // barra de correlação (-1 … +1)
    const juce::Rectangle<float> b (r.getX() + 12, q.getBottom() + 18, r.getWidth() - 24, 10);
    g.setColour (cTela); g.fillRoundedRectangle (b, 2.0f);
    const float x0 = b.getCentreX(), xv = b.getX() + b.getWidth() * (cor + 1.0f) * 0.5f;
    g.setColour (cor < 0 ? cVermelho : (cor < 0.3f ? cAmbar : cVerde));
    g.fillRect (juce::jmin (x0, xv), b.getY() + 2, std::abs (xv - x0), b.getHeight() - 4);
    g.setColour (cTexto);
    g.fillRect (xv - 1, b.getY() - 2, 2.0f, b.getHeight() + 4);
    g.setColour (cApagado);
    g.setFont (fonte (8.0f));
    g.drawText (u8 ("−1"), juce::Rectangle<float> (b.getX(), b.getBottom() + 2, 20, 10), juce::Justification::centredLeft);
    g.drawText ("0", juce::Rectangle<float> (x0 - 10, b.getBottom() + 2, 20, 10), juce::Justification::centred);
    g.drawText ("+1", juce::Rectangle<float> (b.getRight() - 20, b.getBottom() + 2, 20, 10), juce::Justification::centredRight);
}

void CurvaEditor::menu()
{
    juce::PopupMenu m;
    m.addItem (1, "Como funciona");
    m.addSeparator();
   #ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    m.addItem (-1, "CURVA " + juce::String (JucePlugin_VersionString), false, false);
   #endif
    juce::Component::SafePointer<CurvaEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r)
                     {
                         if (safe == nullptr || r != 1) return;
                         juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "CURVA",
                             u8 ("O ouvido não escuta grave e agudo do mesmo jeito em todo volume: baixinho, o grave e o "
                                 "agudo somem (curvas de mesma audibilidade, Fletcher-Munson / ISO 226). Por isso a mix "
                                 "feita de madrugada sai com grave demais quando toca alto.\n\n"
                                 "Ponha a CURVA por ÚLTIMO no MASTER. Em LISTEN LEVEL, diga em que volume você está "
                                 "ouvindo agora (NOITE 50, BAIXO 60, MÉDIO 70, REF 83; com um medidor de SPL no celular "
                                 "dá para acertar em dB). Em TARGET, o volume em que a mix tem que soar certa (83 = "
                                 "padrão de estúdio). A CURVA devolve o que o ouvido perde nesse volume, e você escuta "
                                 "o equilíbrio como se estivesse alto.\n\n"
                                 "AMOUNT dosa a correção. AUTO GAIN segura o volume geral igual ao da mix. SAFE LIMIT "
                                 "protege os falantes (teto em −0,3 dBFS). Latência zero.\n\n"
                                 "É SÓ PARA OUVIR: desligue (ON/OFF) ou tire antes de exportar. LOUDNESS, TRUE PEAK e "
                                 "STEREO medem a mix ANTES da CURVA, que é o que vai para o arquivo.\n\n"
                                 "Espectro: IN = mix, OUT = o que vai para o monitor, linha magenta = a correção (eixo "
                                 "da direita). SMOOTH, TILT e HOLD mudam só a tela. Passe o mouse para ler Hz, nota e "
                                 "dB. Knobs: arraste, SHIFT = fino, duplo clique = padrão."));
                     });
}
