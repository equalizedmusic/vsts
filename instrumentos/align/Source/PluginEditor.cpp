#include "PluginEditor.h"

namespace
{
// paleta: escuro arroxeado; laranja = kick, azul = baixo, verde = corretor/em fase, roxo = marca e memória
const juce::Colour cFundoA { 0xff1b1726 }, cFundoB { 0xff110f18 }, cTopoA { 0xff241f33 }, cTopoB { 0xff17141f },
                   cChapa { 0xff1a1725 }, cBorda { 0xff2e2842 }, cTexto { 0xffdcd7ea }, cApagado { 0xff8b84a3 },
                   cTela { 0xff07060c }, cGrade { 0xff181425 }, cGradeF { 0xff2b2442 },
                   cKick { 0xffff8a3d }, cBaixo { 0xff45a6ff }, cVerde { 0xff3fd68a }, cRoxo { 0xffa77bff },
                   cRoxoClaro { 0xffc9b0ff }, cSoma { 0xfff3efff }, cRuim { 0xffff4f8b };

constexpr int menuLicenca = 1, menuSobre = 2;

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}

float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }

juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::String db (float v)
{
    if (std::abs (v) < 0.05f) return u8 ("±0.0 dB");
    return (v > 0 ? "+" : u8 ("−")) + juce::String (std::abs (v), 1) + " dB";
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
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (cBorda.withAlpha (0.5f));
    g.drawLine (r.getX() + 4, r.getY() + 1.5f, r.getRight() - 4, r.getY() + 1.5f, 1.0f);
}

void rotulo (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor = cApagado)
{
    g.setColour (cor);
    g.setFont (fonte (10.0f).withExtraKerningFactor (0.18f));
    g.drawText (t, r, juce::Justification::centredLeft);
}

void legenda (juce::Graphics& g, float& x, float y, juce::Colour cor, const juce::String& t, bool tracejado = false)
{
    if (tracejado) { for (int i = 0; i < 3; ++i) { g.setColour (cor); g.fillRect (x + i * 5.0f, y - 1.0f, 3.0f, 2.0f); } }
    else { g.setColour (cor); g.fillRect (x, y - 1.5f, 14.0f, 3.0f); }
    g.setColour (cTexto);
    g.setFont (fonte (10.0f).withExtraKerningFactor (0.12f));
    const float w = largura (fonte (10.0f).withExtraKerningFactor (0.12f), t) + 2;
    g.drawText (t, juce::Rectangle<float> (x + 18, y - 7, w, 14), juce::Justification::centredLeft);
    x += 18 + w + 12;
}

void grade (juce::Graphics& g, juce::Rectangle<float> r, int div, bool batidas)
{
    g.setColour (cTela);
    g.fillRoundedRectangle (r, 3.0f);
    if (batidas)   // coluna do kick (1ª semicolcheia de cada tempo) levemente laranja
        for (int i = 0; i < div; i += 4)
        {
            g.setColour (cKick.withAlpha (0.055f));
            g.fillRect (r.getX() + i * r.getWidth() / div, r.getY(), r.getWidth() / div, r.getHeight());
        }
    for (int i = 1; i < div; ++i)
    {
        g.setColour (batidas && i % 4 == 0 ? cGradeF : cGrade);
        const float x = r.getX() + i * r.getWidth() / div;
        g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
    }
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (i == 2 ? cGradeF : cGrade);
        const float y = r.getY() + i * r.getHeight() / 4;
        g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
    }
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

// traço com 4 pontos por pixel (o "4x" do osciloscópio: a onda sai lisa, sem degrau)
template <typename F>
void traco (juce::Graphics& g, juce::Rectangle<float> r, F valor, double ini, double dur, float amp, juce::Colour cor,
            float lw, bool brilho)
{
    juce::Path p;
    const int n = juce::jmax (2, (int) (r.getWidth() * 4));
    const float meio = r.getCentreY(), h2 = r.getHeight() * 0.5f;
    for (int i = 0; i <= n; ++i)
    {
        const double t = ini + dur * i / n;
        const float x = r.getX() + r.getWidth() * (float) i / (float) n;
        const float y = meio - juce::jlimit (-1.05f, 1.05f, valor (t) * amp) * h2;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    if (brilho)
    {
        g.setColour (cor.withAlpha (0.18f));
        g.strokePath (p, juce::PathStrokeType (lw * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (cor);
    g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

AlignEditor::AlignEditor (AlignProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (700, 434, 2000, 1240);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (700, 2000, w0) : baseW;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));
    startTimerHz (30);
    ativacao.anexar (*this);
}

void AlignEditor::resized()
{
    proc.uiW = getWidth();
}

void AlignEditor::timerCallback() { repaint(); }

float AlignEditor::lido (const std::vector<float>& v, double pos) const
{
    // cúbica (Catmull-Rom) entre as amostras guardadas
    const double f = std::floor (pos);
    const auto i = (std::int64_t) f;
    const float t = (float) (pos - f);
    auto a = [&] (std::int64_t k) { return v[(size_t) (k & align::Scope::mask)]; };
    const float y0 = a (i - 1), y1 = a (i), y2 = a (i + 1), y3 = a (i + 2);
    return y1 + 0.5f * t * (y2 - y0 + t * (2 * y0 - 5 * y1 + 4 * y2 - y3 + t * (3 * (y1 - y2) + y3 - y0)));
}

bool AlignEditor::janela (std::int64_t& ini, double& batida) const
{
    const auto& sc = proc.engine.scope;
    const std::int64_t pos = sc.pos.load();
    const double taxa = sc.taxa.load();
    if (pos < 16) return false;
    const float bpm = proc.bpm.load(), medida = proc.engine.leitura.batidaS.load();
    batida = (bpm > 30.0f ? 60.0 / bpm : (medida > 0.15f ? (double) medida : 0.5)) * taxa;
    const double dur = 2 * batida, pre = 0.004 * taxa;
    const int num = sc.numOn.load();
    for (int j = 0; j < juce::jmin (num, align::Scope::numOnsets); ++j)
    {
        const std::int64_t o = sc.onsets[(num - 1 - j) % align::Scope::numOnsets].load();
        if ((double) o + dur + 2 <= (double) pos && (double) o - pre > (double) (pos - align::Scope::size + 4))
        {
            ini = o - (std::int64_t) pre;
            return true;
        }
    }
    // sem kick: corre livre
    ini = pos - (std::int64_t) (dur + pre) - 2;
    return ini > pos - align::Scope::size + 4;
}

void AlignEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    const juce::Rectangle<float> tudo (0, 0, (float) baseW, (float) baseH);
    g.setGradientFill (juce::ColourGradient (cFundoA, 0, 0, cFundoB, 0, (float) baseH, false));
    g.fillRect (tudo);

    topo (g);

    // meio: osciloscópio + zoom (esquerda) e painéis (direita)
    const juce::Rectangle<float> caixa (16, 68, 704, 446);
    chapa (g, caixa);
    osciloscopio (g, { caixa.getX() + 8, caixa.getY() + 30, caixa.getWidth() - 16, 262 });
    zoom (g, { caixa.getX() + 8, caixa.getY() + 322, caixa.getWidth() - 16, 116 });

    const float xl = 734, wl = 250;
    painelLowEnd (g, { xl, 68, wl, 86 });
    painelCorr (g, { xl, 164, wl, 66 });
    painelCorrecao (g, { xl, 240, wl, 118 });
    painelNotas (g, { xl, 368, wl, 146 });

    base (g);
}

void AlignEditor::topo (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 0, (float) baseW, 54);
    g.setGradientFill (juce::ColourGradient (cTopoA, 0, 0, cTopoB, 0, 54, false));
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 54, (float) baseW, 54, 1.0f);

    // marca: duas ondas (kick laranja, baixo azul) que se encontram, e o nome com o A roxo
    {
        juce::Path pk, pb;
        for (int i = 0; i <= 40; ++i)
        {
            const float x = 20 + i * 0.8f, t = i / 40.0f;
            const float yk = 27 - std::sin (t * 6.2832f * 1.5f) * 9 * (1 - t * 0.4f);
            const float yb = 27 - std::sin (t * 6.2832f * 1.5f + 2.6f * (1 - t)) * 7;
            if (i == 0) { pk.startNewSubPath (x, yk); pb.startNewSubPath (x, yb); }
            else { pk.lineTo (x, yk); pb.lineTo (x, yb); }
        }
        g.setColour (cBaixo); g.strokePath (pb, juce::PathStrokeType (2.0f));
        g.setColour (cKick);  g.strokePath (pk, juce::PathStrokeType (2.0f));
    }
    g.setFont (fonte (27.0f, true).withExtraKerningFactor (0.22f));
    g.setColour (cRoxo);
    g.drawText ("A", juce::Rectangle<float> (62, 10, 24, 34), juce::Justification::centredLeft);
    g.setColour (cSoma);
    g.drawText ("LIGN", juce::Rectangle<float> (83, 10, 90, 34), juce::Justification::centredLeft);
    g.setGradientFill (juce::ColourGradient (cKick, 62, 0, cBaixo, 168, 0, false));
    g.fillRect (62.0f, 43.0f, 106.0f, 2.0f);
    rotulo (g, { 186, 22, 160, 14 }, "KICK / BASS PHASE");

    // chips da direita
    auto& L = proc.engine.leitura;
    const bool ligado = proc.alignLigado();
    const int estado = L.estado.load();
    float x = (float) baseW - 16;
    auto chip = [&] (const juce::String& t, juce::Colour corLed, bool aceso, bool comLed)
    {
        const auto f = fonte (11.0f).withExtraKerningFactor (0.14f);
        const float w = largura (f, t) + (comLed ? 34.0f : 22.0f);
        x -= w;
        const juce::Rectangle<float> c (x, 15, w, 24);
        g.setColour (juce::Colour (0xff100e16)); g.fillRoundedRectangle (c, 3.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (c, 3.0f, 1.0f);
        if (comLed) led (g, { c.getX() + 13, c.getCentreY() }, 4.0f, corLed, aceso);
        g.setColour (cTexto); g.setFont (f);
        g.drawText (t, c.withTrimmedLeft (comLed ? 23.0f : 11.0f), juce::Justification::centredLeft);
        x -= 8;
        return c;
    };
    rMenu = chip (u8 ("≡"), cRoxo, false, false);
    juce::String st; juce::Colour cs = cApagado; bool aceso = true;
    if (estado == 0)       { st = "NO SIDECHAIN"; cs = cRuim; }
    else if (! ligado)     { st = "BYPASS"; aceso = false; }
    else if (estado == 1)  { st = "LEARNING"; cs = cRoxo; }
    else                   { st = "TRACKING"; cs = cVerde; }
    chip (st, cs, aceso, true);
    chip ("IN " + u8 ("·") + " BASS", cBaixo, L.temBaixo.load(), true);
    chip ("SC " + u8 ("·") + " KICK", cKick, L.temKick.load(), true);
    const float bpm = proc.bpm.load(), bs = L.batidaS.load();
    chip (bpm > 30.0f ? juce::String (bpm, bpm == std::floor (bpm) ? 0 : 1) + " BPM"
                      : (bs > 0.15f ? juce::String (60.0f / bs, 0) + " BPM" : u8 ("— BPM")), cRoxo, false, false);
}

void AlignEditor::osciloscopio (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& sc = proc.engine.scope;
    auto& L = proc.engine.leitura;
    const bool grave = proc.verGrave.load();
    const float bpm = proc.bpm.load();

    // linha de cima: rótulo, legendas e FULL / LOW
    const float yt = r.getY() - 15;
    rotulo (g, { r.getX(), yt - 7, 250, 14 },
            u8 ("OSCILLOSCOPE · 2 BEATS") + (bpm > 30.0f ? u8 (" · ") + juce::String (bpm, 0) + " BPM" : juce::String()));
    float lx = r.getX() + 252;
    legenda (g, lx, yt, cKick, "KICK");
    legenda (g, lx, yt, cBaixo, "BASS");
    {
        const auto f = fonte (10.0f).withExtraKerningFactor (0.14f);
        const juce::String tLow = "LOW < " + juce::String (juce::roundToInt (L.xoverHz.load())) + " Hz";
        const float wLow = largura (f, tLow) + 18, wFull = largura (f, "FULL") + 18;
        rLow = { r.getRight() - wLow, yt - 9, wLow, 18 };
        rFull = { rLow.getX() - wFull, yt - 9, wFull, 18 };
        for (int i = 0; i < 2; ++i)
        {
            const auto t = i == 0 ? rFull : rLow;
            const bool sel = (i == 1) == grave;
            g.setColour (sel ? cRoxo : juce::Colour (0xff141119)); g.fillRect (t);
            g.setColour (sel ? cRoxo : cBorda); g.drawRect (t, 1.0f);
            g.setColour (sel ? juce::Colour (0xff120f1a) : cApagado); g.setFont (f);
            g.drawText (i == 0 ? "FULL" : tLow, t, juce::Justification::centred);
        }
    }

    grade (g, r, 8, true);
    std::int64_t ini = 0; double batida = 0;
    if (! janela (ini, batida)) return;
    const double dur = 2 * batida + 0.004 * sc.taxa.load();
    const auto& vk = grave ? sc.kL : sc.kF;
    const auto& vb = grave ? sc.bL : sc.bF;

    // escala automática: o maior pico ocupa ~85% da meia altura
    float pico = 1.0e-4f;
    for (int i = 0; i < 600; ++i)
    {
        const double t = (double) ini + dur * i / 600.0;
        pico = juce::jmax (pico, std::abs (lido (vk, t)), std::abs (lido (vb, t)));
    }
    const float alvo = 0.85f / pico;
    escalaScope = alvo < escalaScope ? alvo : escalaScope + (alvo - escalaScope) * 0.15f;

    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    traco (g, r, [&] (double t) { return lido (vb, t); }, (double) ini, dur, escalaScope, cBaixo, 1.6f, true);
    traco (g, r, [&] (double t) { return lido (vk, t); }, (double) ini, dur, escalaScope, cKick, 1.6f, true);
    g.restoreState();

    // rótulos das semicolcheias: K b B B
    const double fs = sc.taxa.load();
    const float colW = r.getWidth() / 8.0f;
    for (int i = 0; i < 8; ++i)
    {
        const float x = r.getX() + i * colW;
        const bool k = i % 4 == 0;
        g.setFont (fonte (13.0f, true));
        g.setColour (k ? cKick : cBaixo);
        g.drawText (k ? "K" : (i % 4 == 1 ? "b" : "B"), juce::Rectangle<float> (x + 6, r.getY() + 4, 16, 16), juce::Justification::centredLeft);
        g.setFont (fonte (10.0f));
        g.setColour (cGradeF.brighter (0.5f));
        g.drawText (juce::String (juce::roundToInt (i * batida / 4.0 / fs * 1000.0)) + " ms",
                    juce::Rectangle<float> (x + 4, r.getBottom() - 16, 60, 14), juce::Justification::centredLeft);
    }

    // onde a cauda do kick encontra o b
    const float low = L.lowEndDb.load();
    if (L.estado.load() > 0)
    {
        const bool ruim = ! proc.alignLigado() && low < -1.0f;
        const bool bom = proc.alignLigado() && std::abs (low) <= 1.5f;
        if (ruim || bom)
            for (int k : { 0, 4 })
            {
                const float x1 = r.getX() + (k + 1) * colW - 10, x2 = r.getX() + (k + 1) * colW + 46;
                const juce::Rectangle<float> m (x1, r.getY() + 26, x2 - x1, r.getHeight() - 52);
                const auto cor = ruim ? cRuim : cVerde;
                juce::Path p; p.addRectangle (m);
                juce::Path d; const float tr[] = { 4.0f, 4.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (d, p, tr, 2);
                g.setColour (cor.withAlpha (0.85f)); g.fillPath (d);
                g.setFont (fonte (11.0f));
                g.drawText (ruim ? "OUT OF PHASE" : "IN PHASE", juce::Rectangle<float> (x2 + 4, r.getY() + 26, 110, 14), juce::Justification::centredLeft);
            }
    }
}

void AlignEditor::zoom (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& sc = proc.engine.scope;
    const bool ligado = proc.alignLigado();
    const float yt = r.getY() - 15;
    rotulo (g, { r.getX(), yt - 7, 170, 14 }, u8 ("ZOOM · KICK → b · SUM"));
    float lx = r.getX() + 172;
    legenda (g, lx, yt, cKick, "KICK");
    legenda (g, lx, yt, cBaixo, "BASS");
    legenda (g, lx, yt, cSoma, "SUM");
    legenda (g, lx, yt, cRoxoClaro, "TARGET", true);

    grade (g, r, 10, false);
    std::int64_t ini = 0; double batida = 0;
    if (! janela (ini, batida)) return;
    const double fs = sc.taxa.load(), s16 = batida / 4.0, pre = 0.004 * fs;
    const double za = (double) ini + pre + 0.6 * s16, zd = 0.95 * s16;
    rotulo (g, { r.getRight() - 120, yt - 7, 120, 14 },
            juce::String (juce::roundToInt (0.6 * s16 / fs * 1000)) + u8 (" — ") + juce::String (juce::roundToInt (1.55 * s16 / fs * 1000)) + " ms");

    // escala pelo maior entre kick, baixo e soma
    float pico = 1.0e-4f;
    for (int i = 0; i < 300; ++i)
    {
        const double t = za + zd * i / 300.0;
        const float k = lido (sc.kL, t), b = lido (sc.bL, t);
        pico = juce::jmax (pico, std::abs (k + b), std::abs (lido (sc.kL, t) + lido (sc.sem, t)), lido (sc.alvo, t));
    }
    const float alvo = 0.9f / pico;
    escalaZoom = alvo < escalaZoom ? alvo : escalaZoom + (alvo - escalaZoom) * 0.15f;

    // a entrada do b
    const float xb = r.getX() + (float) ((0.4 * s16) / zd) * r.getWidth();
    g.setColour (cBaixo.withAlpha (0.4f)); g.drawLine (xb, r.getY(), xb, r.getBottom(), 1.0f);
    g.setColour (cBaixo); g.setFont (fonte (10.0f));
    g.drawText ("b entra", juce::Rectangle<float> (xb + 4, r.getY() + 3, 60, 12), juce::Justification::centredLeft);

    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    traco (g, r, [&] (double t) { return lido (sc.kL, t); }, za, zd, escalaZoom, cKick.withAlpha (0.55f), 1.2f, false);
    traco (g, r, [&] (double t) { return lido (sc.bL, t); }, za, zd, escalaZoom, cBaixo.withAlpha (0.55f), 1.2f, false);
    if (ligado)   // sem o LEVEL GUARD a soma passaria do alvo
        traco (g, r, [&] (double t) { return lido (sc.kL, t) + lido (sc.sem, t); }, za, zd, escalaZoom, cRuim.withAlpha (0.45f), 1.2f, false);
    // alvo = soma de potência (nem cancelamento nem soma), tracejado roxo
    for (float s : { 1.0f, -1.0f })
    {
        juce::Path p;
        const int n = (int) r.getWidth();
        for (int i = 0; i <= n; ++i)
        {
            const float y = r.getCentreY() - s * juce::jlimit (0.0f, 1.05f, lido (sc.alvo, za + zd * i / n) * escalaZoom) * r.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (r.getX() + i, y); else p.lineTo (r.getX() + i, y);
        }
        juce::Path d; const float tr[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (d, p, tr, 2);
        g.setColour (cRoxoClaro.withAlpha (0.8f)); g.fillPath (d);
    }
    traco (g, r, [&] (double t) { return lido (sc.kL, t) + lido (sc.bL, t); }, za, zd, escalaZoom, cSoma, 2.0f, true);
    g.restoreState();
    if (ligado)
    {
        g.setColour (cRuim); g.setFont (fonte (10.0f));
        g.drawText ("sem LEVEL GUARD", juce::Rectangle<float> (r.getRight() - 110, r.getY() + 3, 104, 12), juce::Justification::centredRight);
    }
}

void AlignEditor::painelLowEnd (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& L = proc.engine.leitura;
    chapa (g, r);
    rotulo (g, { r.getX() + 12, r.getY() + 8, 200, 14 }, "LOW END");
    const bool ligado = proc.alignLigado();
    const float v = L.lowEndDb.load();
    juce::String sub; juce::Colour cor = cTexto;
    if (L.estado.load() == 0) { sub = "WAITING FOR THE KICK (SIDECHAIN)"; cor = cApagado; }
    else if (ligado)
    {
        cor = std::abs (v) <= 1.5f ? cVerde : (v < 0 ? cRuim : cKick);
        char nn[8];
        sub = u8 ("NO CANCEL · NO BUILD-UP") + (L.nota.load() >= 0 ? u8 (" · ON ") + align::nomeNota (L.nota.load(), nn) : juce::String());
    }
    else if (v < -1.0f) { cor = cRuim; sub = "LOST TO PHASE CANCELLATION"; }
    else if (v > 1.0f)  { cor = cKick; sub = "BUILD-UP (KICK + BASS SUMMING)"; }
    else                { cor = cVerde; sub = "IN PHASE"; }
    g.setColour (cor);
    g.setFont (fonte (30.0f, true));
    g.drawText (L.estado.load() == 0 ? u8 ("—") : db (v), juce::Rectangle<float> (r.getX() + 12, r.getY() + 22, r.getWidth() - 24, 36), juce::Justification::centredLeft);
    rotulo (g, { r.getX() + 12, r.getY() + 62, r.getWidth() - 20, 14 }, sub);
}

void AlignEditor::painelCorr (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    rotulo (g, { r.getX() + 12, r.getY() + 8, 200, 14 }, u8 ("CORRELATION · LOW"));
    const float v = juce::jlimit (-1.0f, 1.0f, proc.engine.leitura.corr.load());
    const juce::Rectangle<float> b (r.getX() + 12, r.getY() + 26, r.getWidth() - 24, 20);
    g.setColour (cTela); g.fillRect (b);
    juce::ColourGradient gr (cRuim, b.getX(), 0, cVerde, b.getRight(), 0, false);
    gr.addColour (0.5, cRoxo);
    g.setGradientFill (gr); g.setOpacity (0.22f); g.fillRect (b); g.setOpacity (1.0f);
    const float xm = b.getCentreX(), xv = xm + v * b.getWidth() * 0.5f;
    g.setColour (v > 0.3f ? cVerde : (v > -0.1f ? cRoxo : cRuim));
    g.fillRect (juce::Rectangle<float> (juce::jmin (xm, xv), b.getY() + 4, std::abs (xv - xm), b.getHeight() - 8));
    g.setColour (juce::Colour (0xff4a4360)); g.fillRect (xm, b.getY(), 1.0f, b.getHeight());
    g.setColour (cSoma); g.fillRect (xv - 1.5f, b.getY(), 3.0f, b.getHeight());
    g.setFont (fonte (10.0f)); g.setColour (cApagado);
    g.drawText (u8 ("−1"), juce::Rectangle<float> (b.getX(), b.getBottom() + 2, 30, 12), juce::Justification::centredLeft);
    g.drawText ("+1", juce::Rectangle<float> (b.getRight() - 30, b.getBottom() + 2, 30, 12), juce::Justification::centredRight);
    g.setColour (cTexto); g.setFont (fonte (11.0f, true));
    g.drawText ((v > 0 ? "+" : (v < 0 ? u8 ("−") : juce::String())) + juce::String (std::abs (v), 2),
                juce::Rectangle<float> (xm - 30, b.getBottom() + 2, 60, 12), juce::Justification::centred);
}

void AlignEditor::painelCorrecao (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& L = proc.engine.leitura;
    chapa (g, r);
    rotulo (g, { r.getX() + 12, r.getY() + 8, 200, 14 }, "CORRECTION (AUTO)");
    const bool ligado = proc.alignLigado(), temAlgo = L.estado.load() == 2;
    const juce::String linhas[4][2] = {
        { "POLARITY", temAlgo ? (L.invertida.load() ? "INVERT" : "NORMAL") : u8 ("—") },
        { "ROTATION", temAlgo ? (std::abs (L.rotDeg.load()) < 0.5f ? u8 ("0°") : u8 ("−") + juce::String (juce::roundToInt (-L.rotDeg.load())) + u8 ("°")) : u8 ("—") },
        { "X-OVER", juce::String (juce::roundToInt (L.xoverHz.load())) + " Hz" },
        { "LOW DUCK", ligado ? db (L.duckDb.load()) : u8 ("—") } };
    for (int i = 0; i < 4; ++i)
    {
        const float y = r.getY() + 28 + i * 21.0f;
        rotulo (g, { r.getX() + 12, y, 110, 16 }, linhas[i][0]);
        g.setColour (ligado ? cTexto : cApagado);
        g.setFont (fonte (12.5f, ligado));
        g.drawText (linhas[i][1], juce::Rectangle<float> (r.getX() + 110, y, r.getWidth() - 122, 16), juce::Justification::centredRight);
    }
}

void AlignEditor::painelNotas (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& L = proc.engine.leitura;
    chapa (g, r);
    rotulo (g, { r.getX() + 12, r.getY() + 8, 200, 14 }, "NOTE MEMORY");
    const int atual = L.nota.load();
    juce::Array<int> notas;
    for (int n = 0; n < 128; ++n)
        if ((L.notas[n >> 5].load() >> (n & 31)) & 1u) notas.add (n);
    if (atual >= 0 && ! notas.contains (atual)) notas.add (atual);
    notas.sort();
    while (notas.size() > 18) notas.remove (notas.indexOf (atual) == 0 ? notas.size() - 1 : 0);
    if (notas.isEmpty())
    {
        g.setColour (cApagado); g.setFont (fonte (11.0f));
        g.drawFittedText (u8 ("Cada nota do baixo ganha a sua correção\nlogo nas primeiras batidas."),
                          juce::Rectangle<int> ((int) r.getX() + 12, (int) r.getY() + 30, (int) r.getWidth() - 24, 40), juce::Justification::topLeft, 3);
        return;
    }
    const float cw = 34, ch = 20, gap = 4.5f;
    for (int i = 0; i < notas.size(); ++i)
    {
        const int n = notas[i];
        const bool aprendida = ((L.notas[n >> 5].load() >> (n & 31)) & 1u) != 0;
        const juce::Rectangle<float> c (r.getX() + 12 + (i % 6) * (cw + gap), r.getY() + 30 + (i / 6) * (ch + 8), cw, ch);
        g.setColour (aprendida ? cRoxo : juce::Colour (0xff141119)); g.fillRoundedRectangle (c, 2.5f);
        g.setColour (aprendida ? cRoxo : cBorda); g.drawRoundedRectangle (c, 2.5f, 1.0f);
        if (n == atual) { g.setColour (cSoma); g.drawRoundedRectangle (c.expanded (2.0f), 3.5f, 1.6f); }
        char nn[8];
        g.setColour (aprendida ? juce::Colour (0xff120f1a) : cApagado);
        g.setFont (fonte (10.5f, aprendida));
        g.drawText (align::nomeNota (n, nn), c, juce::Justification::centred);
    }
}

void AlignEditor::base (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 528, (float) baseW, 92);
    g.setGradientFill (juce::ColourGradient (cTopoB, 0, r.getY(), juce::Colour (0xff0f0d15), 0, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::black); g.drawLine (0, r.getY(), (float) baseW, r.getY(), 1.0f);

    const bool ligado = proc.alignLigado();
    rBotao = { 20, r.getY() + 17, 190, 58 };
    if (ligado)
    {
        g.setColour (cVerde.withAlpha (0.18f)); g.fillRoundedRectangle (rBotao.expanded (6), 9.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c7a52), 0, rBotao.getY(), juce::Colour (0xff1b4d34), 0, rBotao.getBottom(), false));
    }
    else g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35304a), 0, rBotao.getY(), juce::Colour (0xff25212f), 0, rBotao.getBottom(), false));
    g.fillRoundedRectangle (rBotao, 5.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (rBotao, 5.0f, 1.0f);
    g.setColour ((ligado ? cVerde : cRoxo).withAlpha (0.35f));
    g.drawLine (rBotao.getX() + 5, rBotao.getY() + 1.5f, rBotao.getRight() - 5, rBotao.getY() + 1.5f, 1.0f);
    led (g, { rBotao.getX() + 34, rBotao.getCentreY() }, 6.0f, cVerde, ligado);
    g.setColour (ligado ? juce::Colour (0xffeafff2) : cApagado);
    g.setFont (fonte (22.0f, true).withExtraKerningFactor (0.28f));
    g.drawText ("ALIGN", rBotao.withTrimmedLeft (50), juce::Justification::centredLeft);

    // ajuda
    auto& L = proc.engine.leitura;
    juce::String t1, t2; juce::Colour c1 = cTexto;
    if (L.estado.load() == 0)
    {
        t1 = u8 ("Sem kick no sidechain."); c1 = cRuim;
        t2 = u8 ("No Ableton: abra o Sidechain do ALIGN e escolha a faixa do kick (Post FX).");
    }
    else if (ligado)
    {
        t1 = u8 ("Corretor ligado: alinha a fase e segura o nível."); c1 = cVerde;
        t2 = u8 ("Onde kick e baixo se cruzam, o grave do baixo abaixa só o que a soma passaria. Notas em roxo já têm correção guardada.");
    }
    else if (L.lowEndDb.load() < -1.0f)
    {
        t1 = u8 ("Kick e baixo cancelando no grave."); c1 = cRuim;
        t2 = u8 ("Aperte ALIGN para ligar o corretor automático. Desligado, o som passa sem nenhuma alteração (serve de A/B).");
    }
    else
    {
        t1 = u8 ("Desligado: o som passa sem nenhuma alteração.");
        t2 = u8 ("O ALIGN já está ouvindo e guardando a correção de cada nota. Aperte ALIGN para ligar.");
    }
    g.setColour (c1); g.setFont (fonte (12.5f, true));
    g.drawText (t1, juce::Rectangle<float> (232, r.getY() + 18, 560, 18), juce::Justification::centredLeft);
    g.setColour (cApagado); g.setFont (fonte (12.0f));
    g.drawFittedText (t2, juce::Rectangle<int> (232, (int) r.getY() + 38, 560, 36), juce::Justification::topLeft, 2);

    g.setFont (fonte (10.0f).withExtraKerningFactor (0.15f));
    g.setColour (cApagado);
    g.drawText ("LATENCY 0 SAMPLES", juce::Rectangle<float> (800, r.getY() + 28, 184, 14), juce::Justification::centredRight);
    g.drawText ("SCOPE 4x OVERSAMPLED", juce::Rectangle<float> (800, r.getY() + 46, 184, 14), juce::Justification::centredRight);
}

void AlignEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (rBotao.contains (p)) { proc.alterna(); repaint(); return; }
    if (rFull.contains (p)) { proc.verGrave = false; repaint(); return; }
    if (rLow.contains (p)) { proc.verGrave = true; repaint(); return; }
    if (rMenu.contains (p) || e.mods.isPopupMenu()) menu();
}

void AlignEditor::menu()
{
    juce::PopupMenu m;
    m.addItem (menuSobre, "Como funciona");
    m.addItem (menuLicenca, u8 ("Licença…"));
    m.addSeparator();
    m.addItem (-1, "ALIGN " + juce::String (JucePlugin_VersionString), false, false);
    juce::Component::SafePointer<AlignEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == menuLicenca) safe->ativacao.mostrarInfo();
                         if (r == menuSobre)
                             juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "ALIGN",
                                 u8 ("Ponha o ALIGN no canal do BAIXO e mande o KICK pelo sidechain.\n\n"
                                     "Ligado, ele acompanha cada batida: escolhe sozinho a polaridade e a rotação de fase do "
                                     "grave do baixo que faz kick e baixo somarem sem cancelar, e guarda a correção por nota. "
                                     "O LEVEL GUARD abaixa o grave do baixo só onde os dois se cruzam, para a soma não passar "
                                     "da relação original (nem buraco, nem embolar).\n\n"
                                     "Latência zero. Desligado, o som passa sem nenhuma alteração."));
                     });
}
