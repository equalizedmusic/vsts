#include "PluginEditor.h"

namespace
{
// paleta do ALIGN / BAREBASS: escuro arroxeado; LARANJA = ataque (click), VERDE = corpo, ROXO = pitch / marca,
// AZUL = master
const juce::Colour cFundoA { 0xff1b1726 }, cFundoB { 0xff110f18 }, cTopoA { 0xff241f33 }, cTopoB { 0xff17141f },
                   cChapa { 0xff1a1725 }, cBorda { 0xff2e2842 }, cTexto { 0xffdcd7ea }, cApagado { 0xff8b84a3 },
                   cTela { 0xff07060c }, cGrade { 0xff181425 }, cGradeF { 0xff2b2442 },
                   cLaranja { 0xffff8a3d }, cVerde { 0xff3fd68a }, cRoxo { 0xffa77bff },
                   cRoxoClaro { 0xffc9b0ff }, cBranco { 0xfff3efff }, cAzul { 0xff4fa3ff }, cRuim { 0xffff4f8b };

constexpr int menuSobre = 1, menuLicenca = 3;

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}

float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }

juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::String sinal (float v, int casas)
{
    if (std::abs (v) < (casas ? 0.05f : 0.5f)) return casas ? "0.0" : "0";
    return (v > 0 ? "+" : u8 ("−")) + juce::String (std::abs (v), casas);
}

juce::String hz (float f)
{
    if (f >= 1000.0f) return juce::String (f / 1000.0f, 1) + " kHz";
    if (f < 100.0f) return juce::String (f, 1) + " Hz";
    return juce::String (juce::roundToInt (f)) + " Hz";
}

juce::String ms (float v)
{
    if (v < 10.0f && std::abs (v - std::round (v)) > 0.05f) return juce::String (v, 1) + " ms";
    return juce::String (juce::roundToInt (v)) + " ms";
}

void led (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour cor, bool aceso)
{
    if (aceso)
    {
        g.setColour (cor.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (r * 4, r * 4).withCentre (c));
        g.setColour (cor);
    }
    else g.setColour (juce::Colour (0xff2c2838));
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
}

void chapa (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (cChapa);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (cBorda.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.0f);
}

void rotulo (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor = cApagado,
             juce::Justification j = juce::Justification::centredLeft, float h = 10.0f)
{
    g.setColour (cor);
    g.setFont (fonte (h).withExtraKerningFactor (0.2f));
    g.drawText (t, r, j);
}

void legenda (juce::Graphics& g, float& x, float y, juce::Colour cor, const juce::String& t, bool tracejado = false)
{
    g.setColour (cor);
    if (tracejado) { g.fillRect (x, y - 1.0f, 4.0f, 2.0f); g.fillRect (x + 6, y - 1.0f, 4.0f, 2.0f); g.fillRect (x + 12, y - 1.0f, 3.0f, 2.0f); }
    else g.fillRect (x, y - 1.5f, 14.0f, 3.0f);
    const auto f = fonte (10.0f, true).withExtraKerningFactor (0.2f);
    g.setColour (cTexto); g.setFont (f);
    const float w = largura (f, t) + 2;
    g.drawText (t, juce::Rectangle<float> (x + 19, y - 7, w, 14), juce::Justification::centredLeft);
    x += 19 + w + 14;
}

void tela (juce::Graphics& g, juce::Rectangle<float> t)
{
    g.setColour (cTela); g.fillRoundedRectangle (t, 3.0f);
}

void tracejado (juce::Graphics& g, const juce::Path& p, juce::Colour cor, float lw, float traco = 5.0f, float vao = 4.0f)
{
    juce::Path tr; const float dash[] = { traco, vao };
    juce::PathStrokeType (lw).createDashedStroke (tr, p, dash, 2);
    g.setColour (cor); g.fillPath (tr);
}
} // namespace

ChuteEditor::ChuteEditor (ChuteProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (700, 526, 2000, 1504);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (700, 2000, w0) : baseW;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));
    atualizaPrevia();
    startTimerHz (30);
    ativacao.anexar (*this);
}

void ChuteEditor::resized() { proc.uiW = getWidth(); }

int ChuteEditor::notaTocada() const { return proc.engine().leitura.ultimaNota.load(); }

void ChuteEditor::atualizaPrevia()
{
    const auto a = proc.ajustes();
    const int nota = a.modo == chute::modoKey ? notaTocada() : -1;
    juce::String s;
    for (auto* prm : proc.getParameters()) s << juce::String (prm->getValue(), 4) << ",";
    s << nota;
    if (s == assinatura) return;
    assinatura = s;
    fimPrevia = chute::Engine::previa (a, nota, taxaPrevia, pClick.data(), pCorpo.data(), nPrevia);
    lenCurva = chute::curvasCorpo (a, chute::fCauda (a, nota), curvaF.data(), curvaA.data());
    {
        // a frequência medida anda em degraus (um valor por meio ciclo): na tela vai suavizada (média em log, ±3 ms;
        // mais curta no começo, onde a queda é rápida)
        std::vector<float> s ((size_t) lenCurva);
        for (int k = 0; k < lenCurva; ++k)
        {
            const int r = juce::jmin (3, k / 2);
            double acc = 0; int n = 0;
            for (int j = juce::jmax (0, k - r); j <= juce::jmin (lenCurva - 1, k + r); ++j) { acc += std::log (juce::jmax (1.0f, curvaF[(size_t) j])); ++n; }
            s[(size_t) k] = (float) std::exp (acc / juce::jmax (1, n));
        }
        for (int k = 0; k < lenCurva; ++k) curvaF[(size_t) k] = s[(size_t) k];
    }
    picoPrevia = 1.0e-4f;
    for (int i = 0; i < fimPrevia; ++i) picoPrevia = juce::jmax (picoPrevia, std::abs (pClick[(size_t) i] + pCorpo[(size_t) i]));
}

void ChuteEditor::timerCallback()
{
    atualizaPrevia();
    auto& L = proc.engine().leitura;
    const float pk = L.pico.load();
    picoTela = juce::jmax (pk, picoTela * 0.93f);
    const uint32_t b = L.batidas.load();
    if (b != batidasVistas) { batidasVistas = b; flash = 1.0f; }
    else flash *= 0.85f;
    repaint();
}

juce::RangedAudioParameter* ChuteEditor::param (const juce::String& id) const
{
    return dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
}

float ChuteEditor::real (const juce::String& id) const
{
    if (auto* p = param (id)) return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

juce::String ChuteEditor::texto (const juce::String& id) const
{
    const float v = real (id);
    if (id == "clicklevel" || id == "volume" || id == "tail") return sinal (v, 1) + " dB";
    if (id == "clicktone") return sinal (v, 1);
    if (id == "clicklen" || id == "drop" || id == "decay") return ms (v);
    if (id == "pitch") return hz (v);
    if (id == "tone") return v >= 19500.0f ? juce::String ("OPEN") : hz (v);
    if (id == "tune")
    {
        const auto a = proc.ajustes();
        return ChuteProcessor::nomeNota (chute::ftom (chute::fCauda (a, a.modo == chute::modoKey ? notaTocada() : -1)), true);
    }
    return juce::String (juce::roundToInt (v)) + " %";
}

void ChuteEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    g.setGradientFill (juce::ColourGradient (cFundoA, 0, 0, cFundoB, 0, (float) baseH, false));
    g.fillRect (juce::Rectangle<float> (0, 0, (float) baseW, (float) baseH));
    knobs.clear();
    botoes.clear();

    topo (g);
    painelKick (g, { 16, 84, 584, 360 });
    painelPitch (g, { 610, 84, 276, 196 });
    painelAmp (g, { 610, 290, 276, 154 });
    painelSaida (g, { 896, 84, 88, 360 });
    painelClick (g, { 16, 454, 276, 236 });
    painelBody (g, { 302, 454, 350, 236 });
    painelDrive (g, { 662, 454, 158, 236 });
    painelMaster (g, { 830, 454, 154, 236 });
    rodape (g);
}

void ChuteEditor::topo (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 0, (float) baseW, 70);
    g.setGradientFill (juce::ColourGradient (cTopoA, 0, 0, cTopoB, 0, 70, false));
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 70, (float) baseW, 70, 1.0f);

    // marca: o "chute" (pico do ataque e a onda caindo), laranja
    {
        juce::Path p;
        p.startNewSubPath (20, 38);
        p.cubicTo (24, 38, 25, 18, 29, 18);
        p.cubicTo (33, 18, 33, 50, 38, 50);
        p.cubicTo (42, 50, 42, 32, 46, 32);
        p.lineTo (52, 32);
        g.setColour (cLaranja.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cLaranja); g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const auto fT = fonte (34.0f, true).withExtraKerningFactor (0.3f);
    const juce::String nome ("CHUTE");
    const float wT = largura (fT, nome);
    g.setFont (fT);
    float x = 66;
    for (int i = 0; i < nome.length(); ++i)
    {
        const juce::String c = nome.substring (i, i + 1);
        g.setColour (cRoxo.interpolatedWith (cLaranja, i / 4.0f));
        g.drawText (c, juce::Rectangle<float> (x, 12, 40, 42), juce::Justification::centredLeft);
        x += largura (fT, c);
    }
    g.setColour (cLaranja); g.fillRect (66.0f, 56.0f, wT - 8, 2.5f);
    rotulo (g, { 66 + wT + 12, 28, 160, 14 }, "KICK DESIGNER", cApagado, juce::Justification::centredLeft, 11.0f);

    // da direita para a esquerda: SAVE AS, ◀ ▶, PRESET, KEY, ≡
    const auto a = proc.ajustes();
    auto chip = [&] (juce::Rectangle<float> c, juce::Colour borda)
    {
        g.setColour (juce::Colour (0xff100e16)); g.fillRoundedRectangle (c, 4.0f);
        g.setColour (borda); g.drawRoundedRectangle (c, 4.0f, borda == cBorda ? 1.0f : 1.5f);
    };
    rSalvar = { 892, 20, 90, 32 };
    chip (rSalvar, cLaranja);
    rotulo (g, rSalvar, "SAVE AS", cLaranja, juce::Justification::centred, 12.0f);

    const juce::Rectangle<float> setas (834, 20, 50, 32);
    chip (setas, cBorda);
    rAnt = setas.withWidth (25); rProx = setas.withTrimmedLeft (25);
    for (int i = 0; i < 2; ++i)
    {
        const auto c = (i == 0 ? rAnt : rProx).getCentre();
        juce::Path t;
        if (i == 0) t.addTriangle (c.x + 4, c.y - 6, c.x + 4, c.y + 6, c.x - 5, c.y);
        else t.addTriangle (c.x - 4, c.y - 6, c.x - 4, c.y + 6, c.x + 5, c.y);
        g.setColour (cTexto); g.fillPath (t);
    }

    rPreset = { 588, 20, 238, 32 };
    chip (rPreset, cBorda);
    {
        const bool mexido = proc.alterado();
        const auto fB = fonte (12.0f).withExtraKerningFactor (0.14f), fN = fonte (12.0f, true).withExtraKerningFactor (0.14f);
        float px = rPreset.getX() + 12;
        auto parte = [&] (const juce::String& t, const juce::Font& f, juce::Colour cor)
        {
            g.setFont (f); g.setColour (cor);
            const float w = largura (f, t);
            g.drawText (t, juce::Rectangle<float> (px, rPreset.getY(), w + 2, rPreset.getHeight()), juce::Justification::centredLeft);
            px += w + 2;
        };
        parte ("PRESET  ", fB, cApagado);
        parte (proc.preset.banco + " / ", fB, cTexto);
        juce::String nm = proc.preset.nome;
        while (nm.length() > 4 && px + largura (fN, nm) > rPreset.getRight() - 40) nm = nm.dropLastCharacters (1);
        parte (nm, fN, cBranco);
        if (mexido) parte (" *", fN, cLaranja);
        juce::Path t;
        const float cx = rPreset.getRight() - 16, cy = rPreset.getCentreY();
        t.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (cTexto); g.fillPath (t);
    }

    {
        const juce::String modo = a.modo == chute::modoKey ? "KEY" : (a.modo == chute::modoFixed ? "FIXED" : "PRESET");
        const juce::String nota = ChuteProcessor::nomeNota (chute::ftom (chute::fCauda (a, a.modo == chute::modoKey ? notaTocada() : -1)));
        const juce::String t = modo + u8 (" · ") + nota + (a.modo == chute::modoKey ? u8 (" · MIDI") : juce::String());
        const auto f = fonte (12.5f, true).withExtraKerningFactor (0.16f);
        const float w = largura (f, t) + 44;
        const juce::Rectangle<float> c (rPreset.getX() - 10 - w, 20, w, 32);
        chip (c, cBorda);
        led (g, { c.getX() + 16, c.getCentreY() }, 4.5f, cVerde, true);
        g.setColour (cBranco); g.setFont (f);
        g.drawText (t, c.withTrimmedLeft (30), juce::Justification::centredLeft);
        rMenu = { c.getX() - 40, 20, 32, 32 };
        chip (rMenu, cBorda);
        rotulo (g, rMenu, u8 ("≡"), cTexto, juce::Justification::centred, 15.0f);
    }
}

void ChuteEditor::painelKick (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    const auto a = proc.ajustes();
    const float yt = r.getY() + 18;
    rotulo (g, { r.getX() + 14, yt - 7, 100, 14 }, u8 ("KICK · NOW"));
    float lx = r.getX() + 116;
    legenda (g, lx, yt, cLaranja, "CLICK");
    legenda (g, lx, yt, cVerde, "BODY");
    legenda (g, lx, yt, cRoxo, "PITCH", true);
    const double fT = chute::fCauda (a, a.modo == chute::modoKey ? notaTocada() : -1);
    rotulo (g, { r.getRight() - 214, yt - 7, 200, 14 },
            ChuteProcessor::nomeNota (chute::ftom (fT)) + u8 (" · ") + juce::String (fT, 1) + u8 (" Hz · ") + ms (a.decayMs),
            cTexto, juce::Justification::centredRight);

    const juce::Rectangle<float> t (r.getX() + 10, r.getY() + 34, r.getWidth() - 20, r.getHeight() - 44);
    tela (g, t);
    const float W = juce::jlimit (150.0f, 900.0f, a.decayMs * 1.25f + 40.0f);   // ms na tela
    auto xDe = [&] (double tms) { return t.getX() + (float) (tms / W) * t.getWidth(); };
    const float cy = t.getCentreY() - 6, amp = t.getHeight() * 0.40f;
    // grade: a cada 50 ms (100 se a janela for longa), com os rótulos embaixo
    const float passo = W > 450 ? 100.0f : 50.0f;
    for (float m = 0; m < W; m += passo)
    {
        if (m > 0) { g.setColour (cGrade); g.drawLine (xDe (m), t.getY(), xDe (m), t.getBottom(), 1.0f); }
        g.setColour (cApagado.withAlpha (0.8f)); g.setFont (fonte (9.5f));
        g.drawText (juce::String ((int) m) + " ms", juce::Rectangle<float> (xDe (m) + 5, t.getBottom() - 16, 60, 12), juce::Justification::centredLeft);
    }
    g.setColour (cGradeF); g.drawLine (t.getX(), cy, t.getRight(), cy, 1.0f);
    // faixa do ataque
    const float fimClick = juce::jlimit (2.0f, 25.0f, a.clickMs);
    if (a.clickOn)
    {
        g.setColour (cLaranja.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (t.getX(), t.getY(), xDe (fimClick + 2.5f) - t.getX(), t.getHeight()));
    }

    g.saveState();
    g.reduceClipRegion (t.toNearestInt());
    // curva de pitch (escala log 30 Hz .. 1 kHz)
    if (lenCurva > 1)
    {
        juce::Path pp; bool ini = false;
        const float lo = std::log (30.0f), hi = std::log (1000.0f);
        for (int k = 0; k < juce::jmin (lenCurva, (int) W); ++k)
        {
            const float y = t.getBottom() - 22 - (std::log (juce::jlimit (30.0f, 1000.0f, curvaF[(size_t) k])) - lo) / (hi - lo) * (t.getHeight() - 30);
            if (! ini) { pp.startNewSubPath (xDe (k), y); ini = true; } else pp.lineTo (xDe (k), y);
        }
        tracejado (g, pp, cRoxo.withAlpha (0.9f), 1.6f);
    }
    // a batida: ataque (laranja) e corpo (verde), 4 pontos por pixel
    const int npx = (int) (t.getWidth() * 3);
    juce::Path pc, pb;
    bool temClick = false, temCorpo = false;   // (Path::isEmpty() segue true só com o ponto inicial: flag própria)
    auto corpoAte = [&] (float x, float y) { if (! temCorpo) { pb.startNewSubPath (x, y); temCorpo = true; } else pb.lineTo (x, y); };
    for (int j = 0; j <= npx; ++j)
    {
        const double tms = W * j / npx;
        const int i = juce::jmin (nPrevia - 1, (int) (tms * 0.001 * taxaPrevia));
        const float x = t.getX() + t.getWidth() * (float) j / npx;
        const float s = (pClick[(size_t) i] + pCorpo[(size_t) i]) / picoPrevia;
        const float y = cy - juce::jlimit (-1.1f, 1.1f, s) * amp;
        const bool noClick = a.clickOn && tms < fimClick + 5.0;
        if (noClick)
        {
            if (! temClick) { pc.startNewSubPath (x, y); temClick = true; } else pc.lineTo (x, y);
            if (tms >= fimClick) corpoAte (x, y);
        }
        else corpoAte (x, y);
    }
    g.setColour (cVerde.withAlpha (0.16f + 0.2f * flash));
    g.strokePath (pb, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cVerde);
    g.strokePath (pb, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (temClick)
    {
        g.setColour (cLaranja.withAlpha (0.2f)); g.strokePath (pc, juce::PathStrokeType (4.0f));
        g.setColour (cLaranja); g.strokePath (pc, juce::PathStrokeType (1.6f));
    }
    g.restoreState();
    if (a.clickOn) rotulo (g, { t.getX() + 10, t.getY() + 8, 150, 14 }, "CLICK " + ms (fimClick), cLaranja, juce::Justification::centredLeft, 10.5f);
    rotulo (g, { t.getRight() - 220, t.getY() + 8, 210, 14 },
            "PITCH " + hz (a.pitchHz) + u8 (" → ") + ChuteProcessor::nomeNota (chute::ftom (fT)), cRoxoClaro, juce::Justification::centredRight, 10.5f);
    if (! a.bodyOn && ! a.clickOn)
    {
        g.setColour (cApagado); g.setFont (fonte (13.0f));
        g.drawText (u8 ("CLICK e BODY desligados: ligue um dos dois."), t, juce::Justification::centred);
    }
    if (flash > 0.05f) { g.setColour (cVerde.withAlpha (0.5f * flash)); g.drawRoundedRectangle (t, 3.0f, 2.0f); }
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (t, 3.0f, 1.0f);
}

void ChuteEditor::painelPitch (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    const auto a = proc.ajustes();
    rotulo (g, { r.getX() + 14, r.getY() + 11, 120, 14 }, "PITCH ENV");
    rotulo (g, { r.getRight() - 134, r.getY() + 11, 120, 14 }, "DROP " + ms (a.dropMs), cRoxo, juce::Justification::centredRight);
    const juce::Rectangle<float> t (r.getX() + 10, r.getY() + 32, r.getWidth() - 20, r.getHeight() - 42);
    tela (g, t);
    const float lo = std::log (30.0f), hi = std::log (600.0f);
    auto yDe = [&] (float f) { return t.getBottom() - 8 - (std::log (juce::jlimit (30.0f, 600.0f, f)) - lo) / (hi - lo) * (t.getHeight() - 16); };
    const float W = juce::jlimit (80.0f, 600.0f, a.dropMs * 3.5f);
    auto xDe = [&] (float k) { return t.getX() + 2 + k / W * (t.getWidth() - 4); };
    for (float f : { 50.0f, 100.0f, 200.0f, 400.0f })
    {
        g.setColour (cGrade); g.drawLine (t.getX(), yDe (f), t.getRight(), yDe (f), 1.0f);
        g.setColour (cApagado.withAlpha (0.8f)); g.setFont (fonte (9.5f));
        g.drawText (juce::String ((int) f), juce::Rectangle<float> (t.getX() + 5, yDe (f) - 12, 30, 11), juce::Justification::centredLeft);
    }
    const double fT = chute::fCauda (a, a.modo == chute::modoKey ? notaTocada() : -1);
    {
        juce::Path l; l.startNewSubPath (t.getX(), yDe ((float) fT)); l.lineTo (t.getRight(), yDe ((float) fT));
        tracejado (g, l, cVerde.withAlpha (0.85f), 1.2f, 4.0f, 3.0f);
        const juce::String m = a.modo == chute::modoKey ? "KEY " : (a.modo == chute::modoFixed ? "FIXED " : "PRESET ");
        rotulo (g, { t.getRight() - 100, yDe ((float) fT) - 15, 94, 12 }, m + ChuteProcessor::nomeNota (chute::ftom (fT)), cVerde,
                juce::Justification::centredRight, 9.5f);
    }
    if (lenCurva > 1)
    {
        juce::Path p;
        for (int k = 0; k < juce::jmin (lenCurva, (int) W); ++k)
            if (k == 0) p.startNewSubPath (xDe (0), yDe (curvaF[0])); else p.lineTo (xDe ((float) k), yDe (curvaF[(size_t) k]));
        g.saveState(); g.reduceClipRegion (t.toNearestInt());
        g.setColour (cRoxo.withAlpha (0.2f)); g.strokePath (p, juce::PathStrokeType (5.0f));
        g.setColour (cRoxo); g.strokePath (p, juce::PathStrokeType (2.0f));
        // onde termina a queda (DROP)
        g.setColour (cRoxoClaro.withAlpha (0.6f));
        for (float y = t.getY() + 4; y < t.getBottom() - 4; y += 5) g.drawLine (xDe (a.dropMs), y, xDe (a.dropMs), y + 2, 1.0f);
        g.restoreState();
    }
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (t, 3.0f, 1.0f);
}

void ChuteEditor::painelAmp (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    const auto a = proc.ajustes();
    rotulo (g, { r.getX() + 14, r.getY() + 11, 120, 14 }, "AMP ENV");
    rotulo (g, { r.getRight() - 134, r.getY() + 11, 120, 14 }, "DECAY " + ms (a.decayMs), cVerde, juce::Justification::centredRight);
    const juce::Rectangle<float> t (r.getX() + 10, r.getY() + 32, r.getWidth() - 20, r.getHeight() - 42);
    tela (g, t);
    const float W = juce::jlimit (150.0f, 900.0f, a.decayMs * 1.25f + 40.0f);
    auto xDe = [&] (float k) { return t.getX() + 2 + k / W * (t.getWidth() - 4); };
    float amax = 1.0e-6f;
    for (int k = 0; k < lenCurva; ++k) amax = juce::jmax (amax, curvaA[(size_t) k]);
    auto yDe = [&] (float v) { return t.getBottom() - 4 - juce::jlimit (0.0f, 1.0f, v / amax) * (t.getHeight() - 12); };
    if (lenCurva > 1)
    {
        juce::Path p; p.startNewSubPath (xDe (0), yDe (0));
        for (int k = 0; k < juce::jmin (lenCurva, (int) W); ++k) p.lineTo (xDe ((float) k), yDe (curvaA[(size_t) k]));
        juce::Path area (p);
        area.lineTo (xDe ((float) juce::jmin (lenCurva, (int) W) - 1), yDe (0)); area.closeSubPath();
        g.setGradientFill (juce::ColourGradient (cVerde.withAlpha (0.32f), 0, t.getY(), cVerde.withAlpha (0.05f), 0, t.getBottom(), false));
        g.fillPath (area);
        g.setColour (cVerde); g.strokePath (p, juce::PathStrokeType (1.8f));
        // DECAY: onde o corpo chega no TAIL
        g.setColour (cVerde.withAlpha (0.55f));
        for (float y = t.getY() + 4; y < t.getBottom() - 4; y += 5) g.drawLine (xDe (a.decayMs), y, xDe (a.decayMs), y + 2, 1.0f);
    }
    rotulo (g, { t.getRight() - 120, t.getY() + 6, 112, 12 }, "TAIL " + sinal (a.tailDb, 0) + " dB", cApagado, juce::Justification::centredRight, 9.5f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (t, 3.0f, 1.0f);
}

void ChuteEditor::painelSaida (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    rotulo (g, { r.getX() + 12, r.getY() + 11, 80, 14 }, "OUTPUT");
    const float db = 20.0f * std::log10 (juce::jmax (1.0e-5f, picoTela));
    const juce::Colour cor = db > -0.1f ? cRuim : cVerde;
    g.setColour (picoTela > 1.0e-4f ? cor : cApagado);
    g.setFont (fonte (22.0f, true));
    g.drawText (picoTela > 1.0e-4f ? sinal (db, 1) : u8 ("−∞"), juce::Rectangle<float> (r.getX() + 4, r.getY() + 34, r.getWidth() - 8, 28),
                juce::Justification::centred);
    rotulo (g, { r.getX() + 4, r.getY() + 62, r.getWidth() - 8, 12 }, "dB PEAK", cApagado, juce::Justification::centred, 9.5f);
    const float y0 = r.getY() + 92, y1 = r.getBottom() - 34;
    const float niv = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    for (int c = 0; c < 2; ++c)
    {
        const juce::Rectangle<float> b (r.getCentreX() - 18 + c * 22, y0, 14, y1 - y0);
        g.setColour (cTela); g.fillRoundedRectangle (b, 2.0f);
        const auto f = b.withTop (b.getBottom() - b.getHeight() * niv);
        g.setGradientFill (juce::ColourGradient (cVerde, 0, b.getBottom(), db > -3.0f ? cLaranja : cVerde, 0, b.getY(), false));
        g.fillRoundedRectangle (f, 2.0f);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (b, 2.0f, 1.0f);
    }
    rotulo (g, { r.getX() + 4, y1 + 6, r.getWidth() - 8, 12 }, u8 ("L · R"), cApagado, juce::Justification::centred);
}

void ChuteEditor::tituloLiga (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor, const juce::String& idLiga)
{
    const bool ligado = idLiga.isEmpty() || real (idLiga) > 0.5f;
    led (g, { r.getX() + 18, r.getY() + 20 }, 5.0f, cor, ligado);
    const auto f = fonte (14.0f, true).withExtraKerningFactor (0.3f);
    g.setColour (ligado ? cBranco : cApagado); g.setFont (f);
    g.drawText (t, juce::Rectangle<float> (r.getX() + 32, r.getY() + 10, r.getWidth() - 44, 20), juce::Justification::centredLeft);
    if (idLiga.isNotEmpty())
    {
        rotulo (g, { r.getRight() - 64, r.getY() + 13, 50, 14 }, ligado ? "ON" : "OFF", ligado ? cor : cApagado,
                juce::Justification::centredRight, 11.0f);
        botoes.push_back ({ { r.getX() + 6, r.getY() + 6, r.getWidth() - 12, 28 }, idLiga, ligado ? 0.0f : 1.0f });
    }
}

void ChuteEditor::seletor (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& id, const juce::StringArray& nomes,
                           juce::Colour cor)
{
    const int atual = escolha (id);
    g.setColour (cTela); g.fillRoundedRectangle (r, 3.0f);
    const float w = r.getWidth() / nomes.size();
    for (int i = 0; i < nomes.size(); ++i)
    {
        const juce::Rectangle<float> b (r.getX() + i * w, r.getY(), w, r.getHeight());
        botoes.push_back ({ b, id, (float) i });
        const bool sel = i == atual;
        if (sel) { g.setColour (cor); g.fillRoundedRectangle (b.reduced (1.0f), 2.0f); }
        g.setColour (sel ? juce::Colour (0xff120f1a) : cApagado);
        g.setFont (fonte (10.5f, sel).withExtraKerningFactor (0.16f));
        g.drawText (nomes[i], b, juce::Justification::centred);
    }
    g.setColour (cBorda); g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void ChuteEditor::knob (juce::Graphics& g, const juce::String& id, const juce::String& nome, juce::Point<float> c, float raio,
                        juce::Colour cor, bool bipolar)
{
    auto* p = param (id);
    if (p == nullptr) return;
    knobs.push_back ({ id, c, raio });
    const float v = p->getValue();
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float av = a0 + (a1 - a0) * v, am = a0 + (a1 - a0) * (bipolar ? 0.5f : 0.0f);
    const float ra = raio + 6.0f;
    juce::Path tr; tr.addCentredArc (c.x, c.y, ra, ra, 0, a0, a1, true);
    g.setColour (cGradeF); g.strokePath (tr, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path va; va.addCentredArc (c.x, c.y, ra, ra, 0, juce::jmin (am, av), juce::jmax (am, av), true);
    g.setColour (cor); g.strokePath (va, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d283c), c.x, c.y - raio, juce::Colour (0xff0d0b12), c.x, c.y + raio, false));
    g.fillEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c));
    g.setColour (juce::Colours::black); g.drawEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c), 1.2f);
    const juce::Point<float> d (std::sin (av), -std::cos (av));
    g.setColour (cBranco);
    g.drawLine (c.x + d.x * raio * 0.2f, c.y + d.y * raio * 0.2f, c.x + d.x * raio * 0.82f, c.y + d.y * raio * 0.82f, 2.6f);

    const float y = c.y + raio + 13;
    g.setColour (cTexto);
    g.setFont (fonte (10.5f, true).withExtraKerningFactor (0.24f));
    g.drawText (nome, juce::Rectangle<float> (c.x - 50, y, 100, 13), juce::Justification::centred);
    g.setColour (cor);
    g.setFont (fonte (11.5f));
    g.drawText (texto (id), juce::Rectangle<float> (c.x - 50, y + 15, 100, 14), juce::Justification::centred);
}

void ChuteEditor::painelClick (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    tituloLiga (g, r, "CLICK", cLaranja, "clickon");
    // zoom nos primeiros 20 ms (o corpo apagado por baixo)
    const juce::Rectangle<float> t (r.getX() + 12, r.getY() + 40, r.getWidth() - 24, 84);
    tela (g, t);
    rotulo (g, { t.getRight() - 110, t.getY() + 5, 104, 12 }, u8 ("ZOOM 0–20 ms"), cApagado, juce::Justification::centredRight, 9.0f);
    const float cy = t.getCentreY(), amp = t.getHeight() * 0.42f;
    const int n = (int) (0.020 * taxaPrevia);
    juce::Path pc, pb;
    for (int i = 0; i < n; ++i)
    {
        const float x = t.getX() + t.getWidth() * i / (n - 1);
        const float yc = cy - juce::jlimit (-1.1f, 1.1f, pClick[(size_t) i] / picoPrevia) * amp;
        const float yb = cy - juce::jlimit (-1.1f, 1.1f, pCorpo[(size_t) i] / picoPrevia) * amp;
        if (i == 0) { pc.startNewSubPath (x, yc); pb.startNewSubPath (x, yb); } else { pc.lineTo (x, yc); pb.lineTo (x, yb); }
    }
    g.saveState(); g.reduceClipRegion (t.toNearestInt());
    g.setColour (cVerde.withAlpha (0.45f)); g.strokePath (pb, juce::PathStrokeType (1.2f));
    g.setColour (cLaranja.withAlpha (0.2f)); g.strokePath (pc, juce::PathStrokeType (4.0f));
    g.setColour (cLaranja); g.strokePath (pc, juce::PathStrokeType (1.6f));
    {
        const float xf = t.getX() + t.getWidth() * juce::jlimit (0.0f, 1.0f, real ("clicklen") / 20.0f);
        g.setColour (cLaranja.withAlpha (0.6f));
        for (float y = t.getY() + 3; y < t.getBottom() - 3; y += 5) g.drawLine (xf, y, xf, y + 2, 1.0f);
    }
    g.restoreState();
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (t, 3.0f, 1.0f);

    const float y = r.getY() + 160, w = (r.getWidth() - 20) / 3.0f;
    knob (g, "clicklevel", "LEVEL", { r.getX() + 10 + w * 0.5f, y }, 20.0f, cLaranja, true);
    knob (g, "clicktone", "TONE", { r.getX() + 10 + w * 1.5f, y }, 20.0f, cLaranja, true);
    knob (g, "clicklen", "LENGTH", { r.getX() + 10 + w * 2.5f, y }, 20.0f, cLaranja);
    if (real ("clickon") < 0.5f)
    {
        g.setColour (cFundoB.withAlpha (0.6f));
        g.fillRoundedRectangle (r.withTrimmedTop (36).reduced (3, 0).withTrimmedBottom (3), 3.0f);
    }
}

void ChuteEditor::painelBody (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    tituloLiga (g, r, "BODY", cVerde, "bodyon");
    seletor (g, { r.getX() + 12, r.getY() + 42, 252, 26 }, "mode", { u8 ("KEY · MIDI"), "FIXED", "PRESET" }, cRoxo);
    const float y = r.getY() + 128, w = (r.getWidth() - 16) / 5.0f, x0 = r.getX() + 8;
    knob (g, "tune", "TUNE", { x0 + w * 0.5f, y }, 22.0f, cVerde, true);
    knob (g, "pitch", "PITCH", { x0 + w * 1.5f, y }, 18.0f, cRoxo);
    knob (g, "drop", "DROP", { x0 + w * 2.5f, y }, 22.0f, cRoxo);
    knob (g, "decay", "DECAY", { x0 + w * 3.5f, y }, 22.0f, cVerde);
    knob (g, "tail", "TAIL", { x0 + w * 4.5f, y }, 16.0f, cVerde);
    if (real ("bodyon") < 0.5f)
    {
        g.setColour (cFundoB.withAlpha (0.6f));
        g.fillRoundedRectangle (r.withTrimmedTop (36).reduced (3, 0).withTrimmedBottom (3), 3.0f);
    }
}

void ChuteEditor::painelDrive (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    tituloLiga (g, r, "DRIVE", cRoxo, {});
    seletor (g, { r.getX() + 10, r.getY() + 42, r.getWidth() - 20, 26 }, "drivetype", { "WARM", "HARD", "FOLD" }, cRoxo);
    const float y = r.getY() + 128, w = (r.getWidth() - 12) / 2.0f;
    knob (g, "drive", "DRIVE", { r.getX() + 6 + w * 0.5f, y }, 20.0f, cRoxo);
    knob (g, "tone", "TONE", { r.getX() + 6 + w * 1.5f, y }, 18.0f, cRoxo);
}

void ChuteEditor::painelMaster (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    tituloLiga (g, r, "MASTER", cAzul, {});
    // kick é mono: a batida nova sempre corta a anterior (o seletor CHOKE / POLY saiu na 0.1.2)
    rotulo (g, { r.getX() + 10, r.getY() + 42, r.getWidth() - 20, 26 }, "MONO KICK", cApagado, juce::Justification::centred, 10.5f);
    const float y = r.getY() + 128, w = (r.getWidth() - 12) / 2.0f;
    knob (g, "volume", "VOLUME", { r.getX() + 6 + w * 0.5f, y }, 20.0f, cAzul);
    knob (g, "velo", "VELO", { r.getX() + 6 + w * 1.5f, y }, 18.0f, cAzul);
}

void ChuteEditor::rodape (juce::Graphics& g)
{
    const int modo = escolha ("mode");
    juce::String t1;
    if (modo == chute::modoKey) t1 = u8 ("KEY · MIDI: o corpo afina na nota que você toca, e o ataque do preset continua o mesmo.");
    else if (modo == chute::modoFixed) t1 = u8 ("FIXED: o corpo fica na nota do preset, afinada; o TUNE troca a nota (semitons).");
    else t1 = u8 ("PRESET: o corpo fica na afinação original do preset; o TUNE soma semitons.");
    g.setColour (cVerde); g.setFont (fonte (13.0f, true));
    g.drawText (t1, juce::Rectangle<float> (16, 700, 720, 18), juce::Justification::centredLeft);
    g.setColour (cApagado); g.setFont (fonte (11.5f));
    g.drawText (u8 ("Cada preset traz o click, o pitch e o decay dele. Mexeu em qualquer knob, aparece o * no nome; SAVE AS grava "
                    "como preset novo no banco USER."), juce::Rectangle<float> (16, 721, 760, 16), juce::Justification::centredLeft);

    const bool lk = proc.linkLigado();
    led (g, { 806, 709 }, 3.5f, cVerde, lk);
    rotulo (g, { 814, 702, 170, 14 }, lk ? u8 ("LINK · ALIGN / BAREBASS") : u8 ("LINK OFF"), lk ? cTexto : cApagado,
            juce::Justification::centredRight, 9.5f);
    rotulo (g, { 790, 722, 194, 14 }, "LATENCY 0 SAMPLES", cApagado, juce::Justification::centredRight, 9.5f);
}

// ---------------------------------------------------------------- mouse

void ChuteEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 8)
        {
            if (auto* prm = param (k.id))
            {
                arrastando = k.id;
                valorIni = prm->getValue();
                yIni = p.y;
                prm->beginChangeGesture();
            }
            return;
        }
    for (auto& b : botoes)
        if (b.r.contains (p)) { proc.define (b.id, b.valor); repaint(); return; }
    if (rPreset.contains (p)) { menuPresets(); return; }
    if (rAnt.contains (p)) { proc.proximo (-1); repaint(); return; }
    if (rProx.contains (p)) { proc.proximo (1); repaint(); return; }
    if (rSalvar.contains (p)) { salvarComo(); return; }
    if (rMenu.contains (p) || e.mods.isPopupMenu()) menu();
}

void ChuteEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (arrastando.isEmpty()) return;
    if (auto* prm = param (arrastando))
    {
        const float dy = yIni - e.position.y / escala();
        const float sens = e.mods.isShiftDown() ? 900.0f : 200.0f;
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, valorIni + dy / sens));
        repaint();
    }
}

void ChuteEditor::mouseUp (const juce::MouseEvent&)
{
    if (arrastando.isNotEmpty())
        if (auto* prm = param (arrastando)) prm->endChangeGesture();
    arrastando.clear();
}

void ChuteEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    // duplo clique: volta ao valor do PRESET (não ao padrão do plugin)
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 8)
        {
            proc.define (k.id, proc.valorDoPreset (k.id));
            repaint();
            return;
        }
}

void ChuteEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 8)
            if (auto* prm = param (k.id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * 0.05f));
                prm->endChangeGesture();
                repaint();
                return;
            }
}

// ---------------------------------------------------------------- menus

void ChuteEditor::menuPresets()
{
    juce::PopupMenu m;
    const auto user = proc.presetsUser();
    static constexpr int baseUser = 100000;   // ids do menu: 1 + modelo (fábrica e bancos), baseUser + k (USER)
    for (const auto& banco : ChuteProcessor::bancos())
    {
        m.addSectionHeader (banco);
        for (int o = 0; o < chute::numModelos(); ++o)
        {
            const int i = ChuteProcessor::modeloNaOrdem (o);
            const auto& M = chute::modelo (i);
            if (banco != juce::String::fromUTF8 (M.banco.c_str())) continue;
            juce::PopupMenu::Item it (juce::String::fromUTF8 (M.nome.c_str()));
            it.itemID = 1 + i;
            it.isTicked = ! proc.preset.user && proc.preset.modelo == i;
            it.shortcutKeyDescription = ChuteProcessor::nomeNota (M.nota);   // a nota em cifra, à direita
            m.addItem (it);
        }
    }
    m.addSectionHeader ("USER");
    if (user.isEmpty()) m.addItem (-1, u8 ("(vazio: use o SAVE AS)"), false, false);
    for (int k = 0; k < user.size(); ++k)
        m.addItem (baseUser + k, user[k].getFileNameWithoutExtension(), true, proc.preset.user && proc.preset.arquivo == user[k]);
    juce::Component::SafePointer<ChuteEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, user] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         if (r >= baseUser) safe->proc.carregaUser (user[r - baseUser]);
                         else safe->proc.carregaFabrica (r - 1);
                         safe->repaint();
                     });
}

void ChuteEditor::salvarComo()
{
    auto* w = new juce::AlertWindow ("SAVE AS", u8 ("Nome do preset novo (banco USER):"), juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("nome", proc.sugestaoNome());
    w->addButton ("SALVAR", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCELAR", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<ChuteEditor> safe (this);
    juce::Component::SafePointer<juce::AlertWindow> sw (w);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, sw] (int r)
    {
        if (r != 1 || safe == nullptr || sw == nullptr) return;
        const auto nome = sw->getTextEditorContents ("nome");
        if (! safe->proc.salvaComo (nome))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "CHUTE",
                                                    u8 ("Não deu para gravar o preset em ") + ChuteProcessor::pastaUser().getFullPathName());
        safe->repaint();
    }), true);
}

void ChuteEditor::menu()
{
    juce::PopupMenu m;
    m.addItem (menuSobre, "Como funciona");
    m.addItem (2, u8 ("Abrir a pasta dos presets USER"));
    m.addItem (menuLicenca, u8 ("Licença…"));
    m.addSeparator();
   #ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    m.addItem (-1, "CHUTE " + juce::String (JucePlugin_VersionString), false, false);
   #endif
    juce::Component::SafePointer<ChuteEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == 2) { auto d = ChuteProcessor::pastaUser(); d.createDirectory(); d.startAsProcess(); }
                         if (r == menuLicenca) safe->ativacao.mostrarInfo();
                         if (r == menuSobre)
                             juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "CHUTE",
                                 u8 ("Kick designer: cada nota MIDI toca uma batida.\n\n"
                                     "CLICK é o ataque do preset (LEVEL, TONE e LENGTH). BODY é o corpo do kick: TUNE e o modo "
                                     "dizem a nota (KEY · MIDI segue a nota que você toca, FIXED fica na nota do preset, PRESET "
                                     "fica na afinação original); PITCH é onde a queda começa, DROP a velocidade da queda, DECAY o "
                                     "comprimento e TAIL o quanto sobra no fim.\n\n"
                                     "DRIVE satura (WARM, HARD ou FOLD); o TONE fecha o agudo só da saturação (com DRIVE em 0 ele "
                                     "não muda nada). Kick é mono: a batida nova corta a anterior.\n\n"
                                     "Presets: o menu mostra a nota de cada um. Mexeu em qualquer knob, aparece o * no nome; "
                                     "SAVE AS grava como preset novo no banco USER. Duplo clique no knob volta ao valor do preset.\n\n"
                                     "LINK: o ALIGN e o BAREBASS do mesmo projeto acham o CHUTE sozinhos, sem sidechain.\n\n"
                                     "Latência zero."));
                     });
}
