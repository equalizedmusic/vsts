#include "PluginEditor.h"
#include <cmath>

using namespace horizonte;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004;

void drawTitle (juce::Graphics& g, float x, float y, const juce::String& prefix, const juce::String& accent)
{
    juce::AttributedString s;
    const auto f = theme::sans (9.5f, true, 0.25f);
    s.append (prefix, f, theme::dim);
    s.append (accent, f, theme::amber);
    s.setJustification (juce::Justification::centredLeft);
    s.draw (g, { x, y, 600.0f, 13.0f });
}

void drawDesc (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title, const juce::String& body)
{
    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.setLineSpacing (3.0f);
    s.append (title + "\n", theme::sans (11.5f, true, 0.08f), theme::cream);
    s.append (body, theme::sans (11.0f), theme::dim);
    s.draw (g, area);
}

void drawCard (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

float param (HorizonteProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

void setBool (HorizonteProcessor& p, const char* pid, bool on)
{
    if (auto* prm = p.apvts.getParameter (pid))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (on ? 1.0f : 0.0f);
        prm->endChangeGesture();
    }
}

// número com sinal (+ / −) e vírgula
juce::String signedText (float v, int decimals)
{
    const float eps = 0.5f * std::pow (10.0f, (float) -decimals);
    const auto  t = juce::String (std::abs (v), decimals).replaceCharacter ('.', ',');
    if (v >= eps)  return "+" + t;
    if (v <= -eps) return u8 ("−") + t;
    return t;
}

juce::Colour corrColour (float c)
{
    return c < 0.0f ? theme::coral : c < 0.3f ? theme::amber : theme::leaf;
}

void legendItem (juce::Graphics& g, float& x, juce::Colour c, const juce::String& text, bool line)
{
    const auto f = theme::sans (8.0f, true, 0.18f);
    const float tw = textWidth (f, text);
    x -= tw;
    g.setColour (theme::dim);
    g.setFont (f);
    g.drawText (text, juce::Rectangle<float> (x, 7.0f, tw + 2.0f, 12.0f), juce::Justification::centredLeft);
    x -= 12.0f;
    g.setColour (c);
    if (line) g.fillRect (x - 2.0f, 12.0f, 12.0f, 2.0f);
    else      g.fillRoundedRectangle (x, 9.0f, 8.0f, 8.0f, 2.0f);
    x -= 12.0f;
}

void dashedLine (juce::Graphics& g, float xa, float ya, float xb, float yb, float thickness)
{
    juce::Path line;
    line.startNewSubPath (xa, ya);
    line.lineTo (xb, yb);
    juce::Path dashed;
    const float dashes[] = { 4.0f, 4.0f };
    juce::PathStrokeType (thickness).createDashedStroke (dashed, line, dashes, 2);
    g.fillPath (dashed);
}
} // namespace

// =====================================================================
// VETORSCÓPIO
// =====================================================================
GonioView::GonioView (HorizonteProcessor& p) : proc (p)
{
    for (auto& t : trail)
        t.reserve ((size_t) (maxPoints * 2 + 4));
}

void GonioView::capture()
{
    const auto& eng = proc.engine;
    const int w = eng.scopeWrite.load (std::memory_order_acquire);
    int fresh = (w - lastWrite + scopeSize) & (scopeSize - 1);
    lastWrite = w;
    fresh = juce::jmin (fresh, scopeSize / 2);
    const int step = juce::jmax (1, fresh / maxPoints);

    // um quadro novo no rastro (vazio se o host parou: o desenho esmaece sozinho)
    trailHead = (trailHead + 1) % trailFrames;
    auto& pts = trail[(size_t) trailHead];
    pts.clear();

    float peak = 0.0f;
    for (int k = fresh; k > 0; k -= step)
    {
        const auto  idx = (size_t) ((w - k + scopeSize) & (scopeSize - 1));
        const float l = eng.scopeL[idx].load (std::memory_order_relaxed);
        const float r = eng.scopeR[idx].load (std::memory_order_relaxed);
        const float x = (r - l) * 0.70710678f;   // lado: esquerda/direita
        const float y = (l + r) * 0.70710678f;   // meio: para cima
        pts.push_back ({ x, y });
        peak = juce::jmax (peak, std::abs (x), std::abs (y));
    }

    // escala automática: sobe na hora, desce devagar
    const float target = juce::jlimit (0.02f, 2.0f, peak * 1.15f);
    if (target > range)  range = target;
    else if (fresh > 0)  range += (target - range) * 0.03f;
}

void GonioView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float S = juce::jmin (r.getWidth(), r.getHeight());
    const float cx = r.getCentreX(), cy = r.getCentreY();
    const float rad = S * 0.5f - 18.0f;
    const float d = rad * 0.70710678f;

    g.setColour (theme::dark);
    g.fillRoundedRectangle (r, 4.0f);

    // grade: círculos, eixo M (em pé), eixo S (deitado), diagonais L e R
    g.setColour (theme::segOff);
    g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.0f);
    g.drawEllipse (cx - rad * 0.5f, cy - rad * 0.5f, rad, rad, 1.0f);
    g.drawLine (cx, cy - rad, cx, cy + rad, 1.0f);
    g.drawLine (cx - rad, cy, cx + rad, cy, 1.0f);
    g.drawLine (cx - d, cy - d, cx + d, cy + d, 1.0f);
    g.drawLine (cx + d, cy - d, cx - d, cy + d, 1.0f);

    g.setColour (theme::dim);
    g.setFont (theme::sans (9.0f, true, 0.15f));
    g.drawText ("M", juce::Rectangle<float> (cx - 10.0f, cy - rad - 15.0f, 20.0f, 12.0f), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (cx - d - 16.0f, cy - d - 16.0f, 12.0f, 12.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (cx + d + 4.0f, cy - d - 16.0f, 12.0f, 12.0f), juce::Justification::centred);
    g.drawText ("+S", juce::Rectangle<float> (cx + rad - 4.0f, cy + 3.0f, 20.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (u8 ("−S"), juce::Rectangle<float> (cx - rad - 16.0f, cy + 3.0f, 20.0f, 12.0f), juce::Justification::centredRight);

    // onde fica o centro da imagem depois da ROTAÇÃO e do BALANÇO (verde, como os knobs)
    {
        const float rot = juce::degreesToRadians (param (proc, id::rotation));
        const float bl  = param (proc, id::balance) * 0.01f;
        const float gl  = bl > 0.0f ? 1.0f - bl : 1.0f;
        const float gr  = bl < 0.0f ? 1.0f + bl : 1.0f;
        const float cl  = (std::cos (rot) - std::sin (rot)) * gl;
        const float cr  = (std::sin (rot) + std::cos (rot)) * gr;
        const float ang = std::atan2 (cr - cl, cr + cl);   // 0 = em pé
        if (std::abs (ang) > 0.005f)
        {
            const float tx = cx + rad * std::sin (ang), ty = cy - rad * std::cos (ang);
            g.setColour (theme::leaf.withAlpha (0.8f));
            dashedLine (g, cx, cy, tx, ty, 1.2f);
            g.setColour (theme::leaf);
            g.fillEllipse (tx - 3.5f, ty - 3.5f, 7.0f, 7.0f);
        }
    }

    // rastro: do mais velho (apagado) ao mais novo (aceso)
    const float k = rad / juce::jmax (0.001f, range);
    for (int a = trailFrames - 1; a >= 0; --a)
    {
        const int   idx = (trailHead - a + trailFrames) % trailFrames;
        const float age = (float) a / (float) trailFrames;
        const float dot = a == 0 ? 1.8f : 1.4f;
        g.setColour (theme::cream.withAlpha (0.04f + 0.86f * (1.0f - age) * (1.0f - age)));
        for (const auto& pt : trail[(size_t) idx])
        {
            const float px = cx + juce::jlimit (-rad, rad, pt.x * k);
            const float py = cy - juce::jlimit (-rad, rad, pt.y * k);
            g.fillRect (px - dot * 0.5f, py - dot * 0.5f, dot, dot);
        }
    }

    // escala atual (pico que cabe no círculo)
    g.setColour (theme::dim);
    g.setFont (theme::mono (9.5f));
    const float scaleDb = juce::Decibels::gainToDecibels (range, -60.0f);
    g.drawText ("ESCALA " + signedText (scaleDb, 0) + " dBFS", juce::Rectangle<float> (8.0f, r.getBottom() - 16.0f, 160.0f, 12.0f),
                juce::Justification::centredLeft);

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// =====================================================================
// CORRELAÇÃO
// =====================================================================
void CorrelationView::update (float pL, float pR, float pLR, float dt)
{
    active = pL + pR > 2.0e-8f;   // abaixo de ~−80 dBFS é silêncio
    const float c = active ? juce::jlimit (-1.0f, 1.0f, pLR / std::sqrt (juce::jmax (pL * pR, 1.0e-24f))) : 0.0f;
    corr += (c - corr) * juce::jmin (1.0f, dt * 8.0f);

    if (! active)                { corrMin = corr; holdLeft = 0.0f; }
    else if (corr <= corrMin)    { corrMin = corr; holdLeft = 3.0f; }
    else if ((holdLeft -= dt) <= 0.0f) corrMin = juce::jmin (corr, corrMin + 0.5f * dt);

    const float mid  = 0.25f * (pL + pR + 2.0f * pLR);
    const float side = juce::jmax (0.0f, 0.25f * (pL + pR - 2.0f * pLR));
    const float sm   = juce::jlimit (-60.0f, 30.0f, 10.0f * std::log10 ((side + 1.0e-12f) / (mid + 1.0e-12f)));
    const float bal  = juce::jlimit (-24.0f, 24.0f, 10.0f * std::log10 ((pL + 1.0e-12f) / (pR + 1.0e-12f)));
    const float a    = juce::jmin (1.0f, dt * 6.0f);
    sideMidDb += (sm - sideMidDb) * a;
    balanceDb += (bal - balanceDb) * a;
}

void CorrelationView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    auto label = [&g] (const juce::String& t, float y)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText (t, juce::Rectangle<float> (0.0f, y, 200.0f, 12.0f), juce::Justification::centredLeft);
    };
    auto value = [&g, W] (const juce::String& t, float y, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (theme::mono (11.0f));
        g.drawText (t, juce::Rectangle<float> (0.0f, y, W, 12.0f), juce::Justification::centredRight);
    };

    label (u8 ("CORRELAÇÃO"), 0.0f);
    value (active ? signedText (corr, 2) : juce::String ("SEM SINAL"), 0.0f, active ? corrColour (corr) : theme::dim);

    // barra −1..+1: negativo (some em mono) em coral, positivo em verde
    const float barY = 18.0f, barH = 14.0f;
    g.setColour (theme::dark);
    g.fillRoundedRectangle (0.0f, barY, W, barH, 2.0f);
    g.setColour (theme::coral.withAlpha (0.18f));
    g.fillRect (1.0f, barY + 1.0f, W * 0.5f - 1.0f, barH - 2.0f);
    g.setColour (theme::leaf.withAlpha (0.14f));
    g.fillRect (W * 0.5f, barY + 1.0f, W * 0.5f - 1.0f, barH - 2.0f);
    g.setColour (theme::segOff);
    g.fillRect (W * 0.25f, barY, 1.0f, barH);
    g.fillRect (W * 0.75f, barY, 1.0f, barH);
    g.setColour (theme::dim);
    g.fillRect (W * 0.5f, barY - 2.0f, 1.0f, barH + 4.0f);

    if (active)
    {
        const float xm = W * (corrMin + 1.0f) * 0.5f;
        g.setColour (corrColour (corrMin).withAlpha (0.6f));
        g.fillRect (xm - 0.5f, barY + 2.0f, 1.0f, barH - 4.0f);
    }
    const float xn = juce::jlimit (1.5f, W - 1.5f, W * (corr + 1.0f) * 0.5f);
    g.setColour (active ? corrColour (corr) : theme::off);
    g.fillRoundedRectangle (xn - 1.5f, barY - 3.0f, 3.0f, barH + 6.0f, 1.0f);

    g.setColour (theme::dim);
    g.setFont (theme::mono (9.0f));
    g.drawText (u8 ("−1"), juce::Rectangle<float> (0.0f, barY + barH + 3.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
    g.drawText ("0", juce::Rectangle<float> (W * 0.5f - 15.0f, barY + barH + 3.0f, 30.0f, 11.0f), juce::Justification::centred);
    g.drawText ("+1", juce::Rectangle<float> (W - 30.0f, barY + barH + 3.0f, 30.0f, 11.0f), juce::Justification::centredRight);

    label (u8 ("MÍNIMO (3 s)"), 56.0f);
    value (active ? signedText (corrMin, 2) : juce::String (u8 ("—")), 56.0f, active ? corrColour (corrMin) : theme::dim);

    label ("LADO / MEIO", 76.0f);
    value (! active ? juce::String (u8 ("—"))
                    : sideMidDb <= -59.5f ? juce::String (u8 ("−∞ dB · mono"))
                                          : signedText (sideMidDb, 1) + " dB",
           76.0f, theme::cream);

    label (u8 ("BALANÇO L/R"), 96.0f);
    value (! active ? juce::String (u8 ("—"))
                    : std::abs (balanceDb) < 0.25f ? juce::String ("centro")
                                                   : juce::String (balanceDb > 0.0f ? "E " : "D ") + signedText (std::abs (balanceDb), 1) + " dB",
           96.0f, theme::cream);

    g.setColour (theme::dim);
    g.setFont (theme::sans (10.5f));
    g.drawFittedText (u8 ("Correlação abaixo de zero: parte do som some quando toca em mono. Confira com OUVIR EM MONO."),
                      juce::Rectangle<int> (0, 122, (int) W, 44), juce::Justification::topLeft, 3);
}

// =====================================================================
// LARGURA POR FREQUÊNCIA
// =====================================================================
void WidthCurve::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();

    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    drawTitle (g, 12.0f, 7.0f, u8 ("LARGURA POR FREQUÊNCIA "), u8 ("· LADO (S)"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::coral, "GRAVE MONO", false);
        legendItem (g, x, theme::ice, "AGUDOS", false);
        legendItem (g, x, theme::amber, "GRAVES", false);
        legendItem (g, x, theme::violet, "LARGURA", true);
    }

    float x0 = 46.0f, x1 = W - 14.0f, top = 30.0f, bot = H - 20.0f;
    g.setColour (theme::dark);
    g.fillRoundedRectangle (12.0f, top - 6.0f, W - 24.0f, bot - top + 10.0f, 3.0f);

    // eixo x: 20 Hz a 20 kHz (log); eixo y: 0% a 300% do lado
    auto fx = [=] (float f) { return x0 + (x1 - x0) * std::log (f / 20.0f) / std::log (1000.0f); };
    auto vy = [=] (float v) { return bot - (bot - top) * juce::jlimit (0.0f, 1.0f, v / 3.0f); };

    g.setFont (theme::mono (9.0f));
    for (int k = 0; k <= 3; ++k)
    {
        const float y = vy ((float) k);
        g.setColour (k == 1 ? theme::dim.withAlpha (0.5f) : theme::segOff);
        g.fillRect (x0, y, x1 - x0, 1.0f);
        g.setColour (theme::dim);
        g.drawText (juce::String (k * 100) + "%", juce::Rectangle<float> (12.0f, y - 6.0f, 31.0f, 12.0f), juce::Justification::centredRight);
    }
    const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const char* names[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int k = 0; k < 8; ++k)
    {
        const float x = fx (freqs[k]);
        g.setColour (theme::segOff);
        g.fillRect (x, top, 1.0f, bot - top);
        g.setColour (theme::dim);
        g.drawText (names[k], juce::Rectangle<float> (x - 20.0f, bot + 5.0f, 40.0f, 11.0f), juce::Justification::centred);
    }

    const float wM  = param (proc, id::width) * 0.01f;
    const float wLo = param (proc, id::widthLow) * 0.01f;
    const float wHi = param (proc, id::widthHigh) * 0.01f;
    const float fxo = juce::jlimit (20.0f, 20000.0f, param (proc, id::xover));
    const float fm  = param (proc, id::monoBass);
    const bool  monoOn = fm >= monoOffHz;

    // LR4: graves e agudos ficam em fase, então a largura efetiva soma em magnitude
    auto widthAt = [=] (float f)
    {
        const float a = lr4LowShare (f, fxo);
        const float b = monoOn ? lr4LowShare (f, fm) : 0.0f;
        return wM * (wLo * a + wHi * (1.0f - a)) * (1.0f - b);
    };

    // GRAVE MONO: a faixa onde o lado é zerado
    if (monoOn)
    {
        const float xm = fx (fm);
        g.setColour (theme::coral.withAlpha (0.12f));
        g.fillRect (x0, top, xm - x0, bot - top);
        g.setColour (theme::coral);
        g.fillRect (xm, top, 1.0f, bot - top);
        g.setFont (theme::mono (9.5f));
        g.drawText ("MONO < " + hzText (fm), juce::Rectangle<float> (x0 + 4.0f, top + 15.0f, 120.0f, 12.0f), juce::Justification::centredLeft);
    }

    // curva com o preenchimento dos graves (âmbar) e dos agudos (azul)
    const float xx = fx (fxo);
    const float yx = vy (widthAt (fxo));
    juce::Path lowFill, highFill, curvePath;
    lowFill.startNewSubPath (x0, bot);
    highFill.startNewSubPath (xx, bot);
    highFill.lineTo (xx, yx);
    const int pts = 220;
    for (int k = 0; k <= pts; ++k)
    {
        const float x = x0 + (x1 - x0) * (float) k / (float) pts;
        const float f = 20.0f * std::pow (1000.0f, (x - x0) / (x1 - x0));
        const float y = vy (widthAt (f));
        if (k == 0) curvePath.startNewSubPath (x, y); else curvePath.lineTo (x, y);
        if (x <= xx) lowFill.lineTo (x, y);
        else         highFill.lineTo (x, y);
    }
    lowFill.lineTo (xx, yx);
    lowFill.lineTo (xx, bot);
    lowFill.closeSubPath();
    highFill.lineTo (x1, bot);
    highFill.closeSubPath();

    g.setColour (theme::amber.withAlpha (0.2f));
    g.fillPath (lowFill);
    g.setColour (theme::ice.withAlpha (0.2f));
    g.fillPath (highFill);

    // DIVISÃO
    g.setColour (theme::cream.withAlpha (0.7f));
    dashedLine (g, xx, top, xx, bot, 1.0f);
    g.setColour (theme::cream);
    g.setFont (theme::mono (9.5f));
    const auto xoText = u8 ("DIVISÃO ") + hzText (fxo);
    if (xx < x1 - 120.0f)
        g.drawText (xoText, juce::Rectangle<float> (xx + 5.0f, top + 1.0f, 120.0f, 12.0f), juce::Justification::centredLeft);
    else
        g.drawText (xoText, juce::Rectangle<float> (xx - 125.0f, top + 1.0f, 120.0f, 12.0f), juce::Justification::centredRight);

    g.setColour (theme::violet);
    g.strokePath (curvePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// =====================================================================
// EDITOR
// =====================================================================
HorizonteEditor::HorizonteEditor (HorizonteProcessor& p)
    : AudioProcessorEditor (&p), proc (p), gonio (p), curve (p),
      // cor do knob = cor do que ele mexe no desenho
      widthMacro (p.apvts, id::width,     "LARGURA",          true,  theme::violet),
      widthLow   (p.apvts, id::widthLow,  "GRAVES",           false, theme::amber),
      xover      (p.apvts, id::xover,     u8 ("DIVISÃO"),     false, theme::cream),
      widthHigh  (p.apvts, id::widthHigh, "AGUDOS",           false, theme::ice),
      monoBass   (p.apvts, id::monoBass,  "GRAVE MONO",       false, theme::coral),
      balance    (p.apvts, id::balance,   u8 ("BALANÇO"),     false, theme::leaf),
      rotation   (p.apvts, id::rotation,  u8 ("ROTAÇÃO"),     false, theme::leaf),
      output     (p.apvts, id::output,    "GANHO",            false, theme::cream),
      monoKey    ("OUVIR EM MONO", theme::cream),
      sideKey    (u8 ("OUVIR SÓ O LADO"), theme::violet)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &gonio, &correlation, &curve, &widthMacro, &widthLow, &xover, &widthHigh, &monoBass, &balance, &rotation,
             &output, &monoKey, &sideKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    monoAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::monoListen, monoKey);
    sideAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::sideListen, sideKey);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

    // as duas escutas não andam juntas: ligar uma desliga a outra
    monoKey.onClick = [this] { if (monoKey.getToggleState()) setBool (proc, id::sideListen, false); };
    sideKey.onClick = [this] { if (sideKey.getToggleState()) setBool (proc, id::monoListen, false); };

    prevBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    nextBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    saveBtn.onClick    = [this] { askNewPreset(); };
    presetName.onClick = [this] { showPresetMenu(); };

    layoutCanvas();

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (ui::baseW * ui::minScale), juce::roundToInt (ui::baseH * ui::minScale),
                     juce::roundToInt (ui::baseW * ui::maxScale), juce::roundToInt (ui::baseH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) ui::baseW / (double) ui::baseH);
    setScale (proc.uiScale.load());
    startTimerHz (30);
}

HorizonteEditor::~HorizonteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HorizonteEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto names = proc.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i], true, i == proc.getPresetIndex());

    // presets do usuário dá para gravar por cima, renomear e apagar; os de fábrica não mudam
    menu.addSeparator();
    menu.addItem (menuSaveAs, u8 ("Salvar como novo preset…"));
    if (proc.isUserPreset())
    {
        const auto name = proc.getPresetName();
        menu.addItem (menuOverwrite, u8 ("Salvar alterações em “") + name + u8 ("”"));
        menu.addItem (menuRename,    u8 ("Renomear “") + name + u8 ("”…"));
        menu.addItem (menuDelete,    u8 ("Apagar “") + name + u8 ("”…"));
    }

    juce::PopupMenu sizes;
    const float current = proc.uiScale.load();
    for (int pct : { 75, 100, 125, 150, 175, 200 })
        sizes.addItem (sizeMenuBase + pct, juce::String (pct) + "%", true, std::abs (current * 100.0f - (float) pct) < 1.0f);
    menu.addSeparator();
    menu.addSubMenu ("Tamanho da janela", sizes);

    juce::Component::SafePointer<HorizonteEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void HorizonteEditor::askNewPreset()
{
    juce::Component::SafePointer<HorizonteEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void HorizonteEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<HorizonteEditor> safe (this);
    const auto name = proc.getPresetName();
    switch (result)
    {
        case menuSaveAs:
            askNewPreset();
            break;
        case menuOverwrite:
            proc.overwriteUserPreset();
            break;
        case menuRename:
            dialog.askName (canvas, "Renomear preset", name, [safe] (const juce::String& newName)
            {
                if (safe != nullptr && ! safe->proc.renameUserPreset (newName))
                    safe->dialog.inform (safe->canvas, u8 ("Não deu para renomear"), u8 ("Já existe um preset com esse nome."));
            });
            break;
        case menuDelete:
            dialog.confirm (canvas, "Apagar preset",
                            u8 ("“") + name + u8 ("” vai para a lixeira. O som atual continua no plugin."), "Apagar",
                            [safe] { if (safe != nullptr) safe->proc.deleteUserPreset(); });
            break;
        default:
            proc.loadPreset (result - 1);
            break;
    }
}

void HorizonteEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    gonio.capture();
    correlation.update (proc.engine.powL.load(), proc.engine.powR.load(), proc.engine.powLR.load(), dt);
    gonio.repaint();
    correlation.repaint();
    curve.repaint();

    levelIn  = juce::jmax (proc.meterIn.exchange (0.0f),  levelIn * 0.85f);
    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void HorizonteEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void HorizonteEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void HorizonteEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void HorizonteEditor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // ---- logo: dois círculos (L e R) sobre a linha do horizonte ----
    {
        g.setColour (theme::cream);
        g.drawEllipse (15.0f, 22.0f, 24.0f, 24.0f, 2.0f);
        g.setColour (theme::violet);
        g.drawEllipse (25.0f, 22.0f, 24.0f, 24.0f, 2.0f);
        g.setColour (theme::amber);
        g.fillRect (12.0f, 33.25f, 40.0f, 1.5f);
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("HORIZONTE");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 260.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 (u8 ("IMAGEM ESTÉREO")), tag2 (u8 ("M/S · LATÊNCIA ZERO"));
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText (tag1, juce::Rectangle<float> (tagX, 22.0f, 200.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (tag2, juce::Rectangle<float> (tagX, 35.0f, 200.0f, 12.0f), juce::Justification::centredLeft);

    // ---- marca ----
    {
        const float brandX = tagX + juce::jmax (textWidth (tagFont, tag1), textWidth (tagFont, tag2)) + 20.0f;
        g.setColour (theme::line);
        g.fillRect (brandX - 10.0f, 24.0f, 1.0f, 22.0f);
        g.setColour (theme::dim);
        g.setFont (tagFont);
        g.drawText ("BY", juce::Rectangle<float> (brandX, 22.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
        g.setColour (theme::amber);
        g.setFont (theme::sans (10.5f, true, 0.06f));
        g.drawText ("InsideStudioSP", juce::Rectangle<float> (brandX, 34.0f, 140.0f, 13.0f), juce::Justification::centredLeft);
    }

    // ---- cards ----
    drawCard (g, { 12.0f, 66.0f, 300.0f, 502.0f });
    drawCard (g, { 322.0f, 226.0f, 330.0f, 342.0f });
    drawCard (g, { 662.0f, 226.0f, 170.0f, 342.0f });
    drawCard (g, { 842.0f, 226.0f, 146.0f, 342.0f });

    drawTitle (g, 22.0f,  76.0f,  u8 ("VETORSCÓPIO "), u8 ("· A IMAGEM QUE SAI"));
    drawTitle (g, 332.0f, 236.0f, u8 ("01 · "), "LARGURA");
    drawTitle (g, 672.0f, 236.0f, u8 ("02 · "), u8 ("GRAVE E POSIÇÃO"));
    drawTitle (g, 852.0f, 236.0f, u8 ("03 · "), u8 ("SAÍDA"));

    drawDesc (g, { 438.0f, 258.0f, 204.0f, 92.0f }, "MACRO LARGURA",
              u8 ("Quanto do lado (S) entra: 0% = mono, 100% = como veio, 200% = o dobro."));
    drawDesc (g, { 334.0f, 478.0f, 308.0f, 84.0f }, u8 ("GRAVES · DIVISÃO · AGUDOS"),
              u8 ("Larguras separadas de cada lado da DIVISÃO (Linkwitz-Riley, soma plana). Multiplicam a MACRO."));
}

void HorizonteEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    gonio.setBounds (24, 94, 276, 276);
    correlation.setBounds (24, 384, 276, 176);
    curve.setBounds (322, 66, 666, 150);

    widthMacro.setBounds (334, 256, 92, 92);
    widthLow.setBounds   (332, 372, 103, 92);
    xover.setBounds      (435, 372, 103, 92);
    widthHigh.setBounds  (538, 372, 103, 92);

    monoBass.setBounds (667, 252, 160, 92);
    balance.setBounds  (667, 352, 160, 92);
    rotation.setBounds (667, 452, 160, 92);

    monoKey.setBounds (852, 252, 126, 40);
    sideKey.setBounds (852, 300, 126, 40);
    output.setBounds  (852, 348, 126, 92);
    meterIn.setBounds  (882, 446, 24, 112);
    meterOut.setBounds (910, 446, 24, 112);
}
