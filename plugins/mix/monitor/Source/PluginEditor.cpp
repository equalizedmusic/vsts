#include "PluginEditor.h"

using namespace medidor;

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

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
}

void drawPanelEdge (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

float param (MonitorProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

// LUFS com uma casa ("—" quando ainda não há leitura acima do portão de −70)
juce::String lufsText (float v)
{
    return v <= -70.0f ? juce::String (u8 ("—")) : num (v, 1);
}

// ganho linear em dB com uma casa
juce::String gainText (float gain)
{
    return gain < 1.0e-6f ? juce::String (u8 ("−∞")) : num (juce::Decibels::gainToDecibels (gain), 1);
}

juce::String clockText (float seconds)
{
    const int t = juce::jmax (0, (int) seconds);
    const int h = t / 3600, m = (t / 60) % 60, s = t % 60;
    return (h > 0 ? juce::String (h) + ":" : juce::String())
         + juce::String (m).paddedLeft ('0', 2) + ":" + juce::String (s).paddedLeft ('0', 2);
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

juce::Path dashedLine (float xa, float xb, float y)
{
    juce::Path line, dashed;
    line.startNewSubPath (xa, y);
    line.lineTo (xb, y);
    const float dashes[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, line, dashes, 2);
    return dashed;
}
} // namespace

// =====================================================================
// LOUDNESS
// =====================================================================
void LoudnessView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth();
    const float target = param (proc, id::target), tol = param (proc, id::tol);
    const float tpLim  = param (proc, id::tpMax),  lraLim = param (proc, id::lraMax);

    drawPanel (g, r);
    drawTitle (g, 12.0f, 7.0f, "LOUDNESS ", u8 ("· BS.1770-4 / EBU R128"));

    // ---- integrado (a leitura principal) ----
    g.setColour (theme::dim);
    g.setFont (theme::sans (9.0f, true, 0.18f));
    g.drawText ("INTEGRADO", juce::Rectangle<float> (12.0f, 28.0f, 140.0f, 12.0f), juce::Justification::centredLeft);

    const bool hasI = rd.integrated > -70.0f;
    {
        juce::AttributedString s;
        s.append (lufsText (rd.integrated), theme::mono (34.0f), hasI ? theme::leaf : theme::leaf.withAlpha (0.4f));
        s.append (" LUFS", theme::mono (12.0f), theme::dim);
        s.setJustification (juce::Justification::bottomLeft);
        s.draw (g, { 12.0f, 38.0f, 220.0f, 40.0f });
    }

    // situação do integrado em relação ao alvo
    juce::String chip;
    juce::Colour chipColour = theme::dim;
    if (! hasI)
        chip = u8 ("MEDINDO…");
    else
    {
        const float d = rd.integrated - target;
        if (std::abs (d) <= tol)  { chip = "NO ALVO"; chipColour = theme::leaf; }
        else if (d > 0.0f)        { chip = "+" + num (d, 1) + " LU ACIMA"; chipColour = theme::coral; }
        else                      { chip = num (d, 1) + " LU ABAIXO"; chipColour = theme::cream; }
    }
    {
        const auto  cf = theme::sans (9.0f, true, 0.12f);
        const float cw = textWidth (cf, chip) + 16.0f;
        const juce::Rectangle<float> cr (W - 12.0f - cw, 52.0f, cw, 20.0f);
        g.setColour (chipColour.withAlpha (0.14f));
        g.fillRoundedRectangle (cr, 3.0f);
        g.setColour (chipColour);
        g.drawRoundedRectangle (cr.reduced (0.5f), 3.0f, 1.0f);
        g.setFont (cf);
        g.drawText (chip, cr, juce::Justification::centred);
    }

    // ---- alvo e teto do preset ----
    {
        const auto f = theme::mono (10.5f);
        juce::AttributedString s;
        s.append ("ALVO ", f, theme::dim);
        s.append (num (target, 0) + " LUFS " + u8 ("±") + num (tol, 1), f, theme::violet);
        s.append (u8 ("  ·  TETO "), f, theme::dim);
        s.append (num (tpLim, 1) + " dBTP", f, theme::coral);
        s.setJustification (juce::Justification::centredLeft);
        s.draw (g, { 12.0f, 82.0f, W - 24.0f, 14.0f });
    }
    g.setColour (theme::line);
    g.fillRect (12.0f, 102.0f, W - 24.0f, 1.0f);

    // ---- demais leituras (cada uma na cor do seu desenho) ----
    auto row = [&g, W] (int i, const juce::String& label, const juce::String& value, juce::Colour c, const juce::String& tag)
    {
        const float y = 110.0f + (float) i * 20.0f;
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.18f));
        g.drawText (label, juce::Rectangle<float> (12.0f, y, 150.0f, 14.0f), juce::Justification::centredLeft);
        if (tag.isNotEmpty())
        {
            g.setColour (theme::coral);
            g.setFont (theme::sans (8.0f, true, 0.15f));
            g.drawText (tag, juce::Rectangle<float> (112.0f, y, W - 224.0f, 14.0f), juce::Justification::centredLeft);
        }
        g.setColour (c);
        g.setFont (theme::mono (12.0f));
        g.drawText (value, juce::Rectangle<float> (W - 112.0f, y, 100.0f, 14.0f), juce::Justification::centredRight);
    };

    const float tpDb = juce::Decibels::gainToDecibels (rd.truePeakMax, -120.0f);
    row (0, u8 ("MOMENTÂNEO · 400 ms"), lufsText (rd.momentary) + " LUFS", theme::cream, {});
    row (1, u8 ("CURTO · 3 s"),          lufsText (rd.shortTerm) + " LUFS", theme::amber, {});
    row (2, u8 ("CURTO MÁXIMO"),         lufsText (rd.shortMax) + " LUFS",  theme::amber, {});
    row (3, "LRA", rd.lraLo > -70.0f ? num (rd.lra, 1) + " LU" : juce::String (u8 ("— LU")), theme::ice,
         rd.lraLo > -70.0f && rd.lra > lraLim ? juce::String (u8 ("ACIMA DO MÁX")) : juce::String());
    row (4, u8 ("TRUE PEAK MÁX"), gainText (rd.truePeakMax) + " dBTP", theme::coral,
         rd.truePeakMax > 1.0e-6f && tpDb > tpLim ? juce::String ("ACIMA DO TETO") : juce::String());
    row (5, "TEMPO MEDIDO", clockText (rd.seconds), theme::txt, {});

    drawPanelEdge (g, r);
}

// =====================================================================
// HISTÓRICO
// =====================================================================
HistoryView::HistoryView (MonitorProcessor& p) : proc (p)
{
    mom.fill (silence);
    sht.fill (silence);
}

void HistoryView::update()
{
    auto& e = proc.engine;
    const int w = e.histWrite.load (std::memory_order_acquire);
    for (int k = 0; k < historyLen; ++k)
    {
        const auto idx = (size_t) ((w + k) % historyLen);   // mais antigo à esquerda
        mom[(size_t) k] = e.histM[idx].load (std::memory_order_relaxed);
        sht[(size_t) k] = e.histS[idx].load (std::memory_order_relaxed);
    }
}

void HistoryView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    const float target = param (proc, id::target), tol = param (proc, id::tol);

    drawPanel (g, r);
    drawTitle (g, 12.0f, 7.0f, u8 ("HISTÓRICO "), u8 ("· LUFS, 60 s"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::cream.withAlpha (0.5f), "MOM.", true);
        legendItem (g, x, theme::amber, "CURTO", true);
        legendItem (g, x, theme::leaf, "INTEGR.", true);
        legendItem (g, x, theme::violet, "ALVO", true);
    }

    const float x0 = 36.0f, x1 = W - 26.0f, top = 26.0f, bot = H - 20.0f;
    const float hi = target + 9.0f, lo = target - 27.0f;
    auto yOf = [=] (float l) { return bot - (juce::jlimit (lo, hi, l) - lo) / (hi - lo) * (bot - top); };

    g.setColour (theme::dark);
    g.fillRoundedRectangle (x0, top, x1 - x0, bot - top, 3.0f);

    // acima da faixa: coral bem leve; a faixa aceita em volta do alvo: violeta
    const float yTolHi = yOf (target + tol), yTolLo = yOf (target - tol);
    g.setColour (theme::coral.withAlpha (0.06f));
    g.fillRect (x0, top, x1 - x0, yTolHi - top);
    g.setColour (theme::violet.withAlpha (0.14f));
    g.fillRect (x0, yTolHi, x1 - x0, yTolLo - yTolHi);

    // grade a cada 6 LU (escala em LUFS)
    g.setFont (theme::mono (8.5f));
    for (int k = -24; k <= 6; k += 6)
    {
        const float v = target + (float) k, y = yOf (v);
        g.setColour (theme::segOff.withAlpha (0.6f));
        g.fillRect (x0, y, x1 - x0, 1.0f);
        g.setColour (theme::dim);
        g.drawText (num (v, 0), juce::Rectangle<float> (0.0f, y - 6.0f, x0 - 4.0f, 12.0f), juce::Justification::centredRight);
    }
    g.setColour (theme::dim);
    g.drawText (u8 ("−60 s"), juce::Rectangle<float> (x0, bot + 4.0f, 60.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (u8 ("−30 s"), juce::Rectangle<float> ((x0 + x1) * 0.5f - 30.0f, bot + 4.0f, 60.0f, 12.0f), juce::Justification::centred);
    g.drawText ("AGORA", juce::Rectangle<float> (x1 - 60.0f, bot + 4.0f, 60.0f, 12.0f), juce::Justification::centredRight);

    // alvo
    g.setColour (theme::violet);
    g.fillRect (x0, yOf (target) - 0.6f, x1 - x0, 1.2f);

    // integrado (tracejado)
    if (rd.integrated > -70.0f)
    {
        g.setColour (theme::leaf);
        g.fillPath (dashedLine (x0, x1, yOf (rd.integrated)));
    }

    // momentâneo (leve) e curto
    auto curve = [&] (const std::array<float, historyLen>& data)
    {
        juce::Path p;
        bool open = false;
        for (int k = 0; k < historyLen; ++k)
        {
            const float v = data[(size_t) k];
            if (v <= -70.0f) { open = false; continue; }
            const float x = x0 + (x1 - x0) * (float) k / (float) (historyLen - 1);
            if (open) p.lineTo (x, yOf (v));
            else      { p.startNewSubPath (x, yOf (v)); open = true; }
        }
        return p;
    };
    g.setColour (theme::cream.withAlpha (0.5f));
    g.strokePath (curve (mom), juce::PathStrokeType (1.0f));
    g.setColour (theme::amber);
    g.strokePath (curve (sht), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // LRA: colchete azul do percentil 10 ao 95
    if (rd.lraLo > -70.0f && rd.lraHi > -70.0f)
    {
        const float ya = yOf (rd.lraHi), yb = yOf (rd.lraLo), xb = x1 + 9.0f;
        g.setColour (theme::ice);
        g.fillRect (xb, ya, 3.0f, juce::jmax (1.0f, yb - ya));
        g.fillRect (xb - 3.0f, ya, 8.0f, 1.0f);
        g.fillRect (xb - 3.0f, yb - 1.0f, 8.0f, 1.0f);
        g.setFont (theme::sans (7.5f, true, 0.1f));
        g.drawText ("LRA", juce::Rectangle<float> (x1 + 1.0f, bot + 4.0f, 24.0f, 12.0f), juce::Justification::centred);
    }

    drawPanelEdge (g, r);
}

// =====================================================================
// ESPECTRO
// =====================================================================
void SpectrumView::update (float dt)
{
    analyzer.update (proc.engine.fftRing, proc.engine.fftWrite.load (std::memory_order_acquire), proc.rate.load(), dt);
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();

    drawPanel (g, r);
    drawTitle (g, 12.0f, 7.0f, "ESPECTRO ", u8 ("· dBFS"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::cream, u8 ("MÉDIA"), true);
        legendItem (g, x, theme::amber, "AO VIVO", false);
    }

    const float x0 = 30.0f, x1 = W - 10.0f, top = 26.0f, bot = H - 20.0f;
    const float dbTop = 0.0f, dbBot = -90.0f;
    const int   nCols = SpectrumAnalyzer::cols;
    auto yOf    = [=] (float db) { return top + (dbTop - juce::jlimit (dbBot, dbTop, db)) / (dbTop - dbBot) * (bot - top); };
    auto xOfCol = [=] (int c) { return x0 + (x1 - x0) * ((float) c + 0.5f) / (float) nCols; };
    auto xOfHz  = [=] (float f) { return x0 + (x1 - x0) * std::log (f / 20.0f) / std::log (1000.0f); };

    g.setColour (theme::dark);
    g.fillRoundedRectangle (x0, top, x1 - x0, bot - top, 3.0f);

    g.setFont (theme::mono (8.5f));
    for (int d = 0; d >= -90; d -= 18)
    {
        const float y = yOf ((float) d);
        g.setColour (theme::segOff.withAlpha (0.6f));
        g.fillRect (x0, y, x1 - x0, 1.0f);
        g.setColour (theme::dim);
        g.drawText (num ((float) d, 0), juce::Rectangle<float> (0.0f, y - 6.0f, x0 - 4.0f, 12.0f), juce::Justification::centredRight);
    }
    const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const char* names[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 8; ++i)
    {
        const float x = xOfHz (freqs[i]);
        g.setColour (theme::segOff.withAlpha (0.6f));
        g.fillRect (x, top, 1.0f, bot - top);
        g.setColour (theme::dim);
        g.drawText (names[i], juce::Rectangle<float> (x - 20.0f, bot + 4.0f, 40.0f, 12.0f), juce::Justification::centred);
    }
    g.drawText ("Hz", juce::Rectangle<float> (x1 - 30.0f, bot + 4.0f, 30.0f, 12.0f), juce::Justification::centredRight);

    const auto& liveDb = analyzer.liveDb();
    const auto& avgDb  = analyzer.avgDb();
    juce::Path fill, edge, mean;
    fill.startNewSubPath (x0, bot);
    for (int c = 0; c < nCols; ++c)
    {
        const float x = xOfCol (c), yl = yOf (liveDb[(size_t) c]), ya = yOf (avgDb[(size_t) c]);
        fill.lineTo (x, yl);
        if (c == 0) { edge.startNewSubPath (x, yl); mean.startNewSubPath (x, ya); }
        else        { edge.lineTo (x, yl);          mean.lineTo (x, ya); }
    }
    fill.lineTo (x1, bot);
    fill.closeSubPath();

    g.setColour (theme::amber.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (theme::amber.withAlpha (0.85f));
    g.strokePath (edge, juce::PathStrokeType (1.0f));
    g.setColour (theme::cream);
    g.strokePath (mean, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    drawPanelEdge (g, r);
}

// =====================================================================
// ESTÉREO
// =====================================================================
void StereoView::update (float corr, float dt)
{
    auto& e = proc.engine;
    const int w = e.gonWrite.load (std::memory_order_acquire);
    float most = 0.0f;
    for (int k = 0; k < points; ++k)
    {
        const auto  idx = (size_t) ((w - points + k + gonioLen) & (gonioLen - 1));
        const float l = e.gonL[idx].load (std::memory_order_relaxed);
        const float r = e.gonR[idx].load (std::memory_order_relaxed);
        // vertical = meio (L+R), horizontal = lado (R−L): só L sobe para a esquerda, só R para a direita
        px[(size_t) k] = (r - l) * 0.7071f;
        py[(size_t) k] = (l + r) * 0.7071f;
        most = juce::jmax (most, std::abs (px[(size_t) k]), std::abs (py[(size_t) k]));
    }
    // escala automática: encolhe na hora, cresce devagar (até +24 dB)
    const float want = 0.9f / juce::jmax (0.06f, most);
    zoom = want < zoom ? want : zoom + (want - zoom) * juce::jmin (1.0f, dt * 1.5f);
    corrSm += (corr - corrSm) * juce::jmin (1.0f, dt * 8.0f);
}

void StereoView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth();

    drawPanel (g, r);
    drawTitle (g, 12.0f, 7.0f, u8 ("ESTÉREO "), u8 ("· FASE"));

    const float S = W - 24.0f;
    const juce::Rectangle<float> box (12.0f, 26.0f, S, S);
    g.setColour (theme::dark);
    g.fillRoundedRectangle (box, 3.0f);
    const auto  c = box.getCentre();
    const float half = S * 0.5f - 8.0f;

    // eixos: vertical = mono (M); diagonais = L e R
    g.setColour (theme::segOff);
    g.drawLine (c.x, c.y - half, c.x, c.y + half);
    g.drawLine (c.x - half * 0.7071f, c.y - half * 0.7071f, c.x + half * 0.7071f, c.y + half * 0.7071f);
    g.drawLine (c.x + half * 0.7071f, c.y - half * 0.7071f, c.x - half * 0.7071f, c.y + half * 0.7071f);
    g.setColour (theme::dim);
    g.setFont (theme::sans (8.0f, true, 0.1f));
    g.drawText ("M", juce::Rectangle<float> (c.x - 6.0f, box.getY() + 2.0f, 12.0f, 10.0f), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (box.getX() + 4.0f, box.getY() + 4.0f, 12.0f, 10.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (box.getRight() - 16.0f, box.getY() + 4.0f, 12.0f, 10.0f), juce::Justification::centred);

    g.setColour (theme::cream.withAlpha (0.45f));
    for (int k = 0; k < points; ++k)
    {
        const float x = c.x + juce::jlimit (-1.0f, 1.0f, px[(size_t) k] * zoom) * half;
        const float y = c.y - juce::jlimit (-1.0f, 1.0f, py[(size_t) k] * zoom) * half;
        g.fillRect (x - 0.75f, y - 0.75f, 1.5f, 1.5f);
    }

    // correlação: +1 = mono, 0 = largo, negativo = fase invertida (some no mono)
    const float yC = box.getBottom() + 10.0f;
    const juce::Colour cc = corrSm < 0.0f ? theme::coral : theme::leaf;
    g.setColour (theme::dim);
    g.setFont (theme::sans (9.0f, true, 0.18f));
    g.drawText (u8 ("CORRELAÇÃO"), juce::Rectangle<float> (12.0f, yC, 120.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour (cc);
    g.setFont (theme::mono (11.0f));
    g.drawText ((corrSm > 0.005f ? "+" : "") + num (corrSm, 2), juce::Rectangle<float> (12.0f, yC, S, 12.0f),
                juce::Justification::centredRight);

    const float by = yC + 18.0f;
    g.setColour (theme::segOff);
    g.fillRoundedRectangle (12.0f, by, S, 5.0f, 2.0f);
    const float mid = 12.0f + S * 0.5f;
    const float pos = 12.0f + S * (juce::jlimit (-1.0f, 1.0f, corrSm) + 1.0f) * 0.5f;
    g.setColour (cc);
    g.fillRect (juce::jmin (mid, pos), by, std::abs (pos - mid), 5.0f);
    g.fillRect (pos - 1.0f, by - 3.0f, 2.0f, 11.0f);
    g.setColour (theme::dim);
    g.fillRect (mid - 0.5f, by - 2.0f, 1.0f, 9.0f);
    g.setFont (theme::mono (8.5f));
    g.drawText (u8 ("−1"), juce::Rectangle<float> (12.0f, by + 9.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
    g.drawText ("0", juce::Rectangle<float> (mid - 15.0f, by + 9.0f, 30.0f, 11.0f), juce::Justification::centred);
    g.drawText ("+1", juce::Rectangle<float> (12.0f + S - 30.0f, by + 9.0f, 30.0f, 11.0f), juce::Justification::centredRight);

    drawPanelEdge (g, r);
}

// =====================================================================
// NÍVEL
// =====================================================================
LevelView::LevelView (MonitorProcessor& p) : proc (p)
{
    level.fill (-120.0f);
    hold.fill (-120.0f);
    holdLeft.fill (0.0f);
}

void LevelView::update (float peak, float truePeak, float rmsGain, float dt)
{
    const std::array<float, 3> in { peak, truePeak, rmsGain };
    for (size_t i = 0; i < 3; ++i)
    {
        const float db = juce::Decibels::gainToDecibels (in[i], -120.0f);
        level[i] = i == 2 ? db : juce::jmax (db, level[i] - 24.0f * dt);   // picos caem 24 dB/s; o RMS já é janela
        if (level[i] >= hold[i]) { hold[i] = level[i]; holdLeft[i] = 2.0f; }
        else if ((holdLeft[i] -= dt) <= 0.0f) hold[i] = juce::jmax (level[i], hold[i] - 12.0f * dt);
    }
}

void LevelView::clearHolds()
{
    hold = level;
    holdLeft.fill (0.0f);
}

void LevelView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();

    drawPanel (g, r);
    drawTitle (g, 12.0f, 7.0f, u8 ("NÍVEL "), u8 ("· dB"));

    const float top = 30.0f, bot = H - 52.0f, lo = -60.0f, hi = 3.0f;
    auto yOf = [=] (float db) { return bot - (juce::jlimit (lo, hi, db) - lo) / (hi - lo) * (bot - top); };

    g.setFont (theme::mono (8.0f));
    for (int d : { 0, -6, -12, -24, -36, -48, -60 })
    {
        const float y = yOf ((float) d);
        g.setColour (theme::segOff.withAlpha (0.6f));
        g.fillRect (22.0f, y, W - 30.0f, 1.0f);
        g.setColour (theme::dim);
        g.drawText (juce::String (-d), juce::Rectangle<float> (0.0f, y - 6.0f, 19.0f, 12.0f), juce::Justification::centredRight);
    }

    const juce::Colour colours[3] = { theme::cream, theme::coral, theme::ice };
    const char* names[3] = { "PICO", "TP", "RMS" };
    const char* units[3] = { "dBFS", "dBTP", "dBFS" };
    const float colW  = (W - 26.0f) / 3.0f;
    const float tpLim = param (proc, id::tpMax);

    for (int i = 0; i < 3; ++i)
    {
        const auto  k  = (size_t) i;
        const float cx = 24.0f + colW * ((float) i + 0.5f);
        const juce::Rectangle<float> bar (cx - 8.0f, top, 16.0f, bot - top);
        g.setColour (theme::dark);
        g.fillRect (bar);
        g.setColour (colours[i].withAlpha (0.85f));
        g.fillRect (bar.withTop (yOf (level[k])));
        if (hold[k] > lo)
        {
            g.setColour (colours[i]);
            g.fillRect (bar.getX(), yOf (hold[k]) - 1.0f, bar.getWidth(), 2.0f);
        }
        if (i == 1)   // TETO de true peak do alvo
        {
            g.setColour (theme::coral.withAlpha (0.7f));
            g.fillRect (bar.getX() - 3.0f, yOf (tpLim) - 0.5f, bar.getWidth() + 6.0f, 1.0f);
        }

        // número = marca segurada (o máximo dos últimos 2 s)
        const juce::Rectangle<float> col (cx - colW * 0.5f, bot + 6.0f, colW, 12.0f);
        g.setColour (colours[i]);
        g.setFont (theme::mono (9.0f));
        g.drawText (hold[k] > -119.0f ? num (hold[k], 1) : juce::String (u8 ("−∞")), col, juce::Justification::centred);
        g.setColour (theme::txt);
        g.setFont (theme::sans (8.5f, true, 0.12f));
        g.drawText (names[i], col.translated (0.0f, 14.0f), juce::Justification::centred);
        g.setColour (theme::dim);
        g.setFont (theme::sans (7.5f, false, 0.1f));
        g.drawText (units[i], col.translated (0.0f, 26.0f), juce::Justification::centred);
    }

    drawPanelEdge (g, r);
}

// =====================================================================
// OUVIR NO ALVO: quanto está baixando
// =====================================================================
void ListenInfo::set (float db, bool on, bool waiting)
{
    if (std::abs (db - gainDb) < 0.05f && on == active && waiting == pending) return;
    gainDb = db;
    active = on;
    pending = waiting;
    repaint();
}

void ListenInfo::paint (juce::Graphics& g)
{
    const auto f = theme::mono (10.5f);
    juce::AttributedString s;
    s.append ("NO ALVO ", f, theme::dim);
    if (! active)
        s.append ("DESLIGADO", f, theme::dim);
    else if (pending)
        s.append ("AGUARDANDO", f, theme::violet);
    else
        s.append (gainDb < -0.05f ? num (gainDb, 1) + " dB" : juce::String (u8 ("SEM MUDANÇA")), f, theme::violet);
    s.setJustification (juce::Justification::centredLeft);
    s.draw (g, getLocalBounds().toFloat());
}

// =====================================================================
// EDITOR
// =====================================================================
MonitorEditor::MonitorEditor (MonitorProcessor& p)
    : AudioProcessorEditor (&p), proc (p), loud (p), history (p), spectrum (p), stereo (p), level (p),
      // cor do knob = cor do que ele mexe no desenho
      targetMacro (p.apvts, id::target, "ALVO",          true,  theme::violet),
      tpKnob      (p.apvts, id::tpMax,  "TETO TP",       false, theme::coral),
      tolKnob     (p.apvts, id::tol,    "FAIXA",         false, theme::violet),
      lraKnob     (p.apvts, id::lraMax, u8 ("LRA MÁX"),  false, theme::ice),
      monoKey     ("OUVIR EM MONO", theme::cream),
      matchKey    ("OUVIR NO ALVO", theme::violet),
      resetKey    ("ZERAR", theme::amber),
      freezeKey   ("CONGELAR", theme::ice)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &loud, &history, &spectrum, &stereo, &level, &listenInfo, &targetMacro, &tpKnob, &tolKnob, &lraKnob,
             &monoKey, &matchKey, &resetKey, &freezeKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName })
        canvas.addAndMakeVisible (c);

    monoAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::mono, monoKey);
    matchAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::match, matchKey);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

    // ZERAR é um toque (acende um instante); CONGELAR é só da tela: não é parâmetro nem vai no preset
    resetKey.setClickingTogglesState (false);
    resetKey.onClick = [this]
    {
        proc.engine.requestReset();
        level.clearHolds();
        resetKey.setToggleState (true, juce::dontSendNotification);
        resetFlash = 6;
    };

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

MonitorEditor::~MonitorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MonitorEditor::showPresetMenu()
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

    juce::Component::SafePointer<MonitorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void MonitorEditor::askNewPreset()
{
    juce::Component::SafePointer<MonitorEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void MonitorEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<MonitorEditor> safe (this);
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
                            u8 ("“") + name + u8 ("” vai para a lixeira. O alvo atual continua no plugin."), "Apagar",
                            [safe] { if (safe != nullptr) safe->proc.deleteUserPreset(); });
            break;
        default:
            proc.loadPreset (result - 1);
            break;
    }
}

void MonitorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    auto& e = proc.engine;
    const float pk = e.peakNow.exchange (0.0f), tp = e.truePeakNow.exchange (0.0f);

    // CONGELAR: a medição continua no áudio; só a tela para de copiar
    if (! freezeKey.getToggleState())
    {
        Readings rd;
        rd.momentary   = e.momentary.load();
        rd.shortTerm   = e.shortTerm.load();
        rd.integrated  = e.integrated.load();
        rd.shortMax    = e.shortMax.load();
        rd.lra         = e.lra.load();
        rd.lraLo       = e.lraLo.load();
        rd.lraHi       = e.lraHi.load();
        rd.truePeakMax = e.truePeakMax.load();
        rd.seconds     = e.seconds.load();

        loud.setReadings (rd);
        history.setReadings (rd);
        history.update();
        spectrum.update (dt);
        stereo.update (e.corr.load(), dt);
        level.update (pk, tp, e.rms.load(), dt);
    }
    loud.repaint();
    history.repaint();
    spectrum.repaint();
    stereo.repaint();
    level.repaint();

    listenInfo.set (proc.listenDb.load(), param (proc, id::match) > 0.5f && param (proc, id::bypass) < 0.5f,
                e.integrated.load() <= -70.0f);

    if (resetFlash > 0 && --resetFlash == 0)
        resetKey.setToggleState (false, juce::dontSendNotification);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void MonitorEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void MonitorEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void MonitorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void MonitorEditor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // ---- logo: telinha de medidor com quatro barras ----
    {
        const juce::Rectangle<float> box (17.0f, 24.0f, 28.0f, 20.0f);
        g.setColour (theme::cream);
        g.drawRoundedRectangle (box, 2.5f, 1.6f);
        const float heights[] = { 5.0f, 9.0f, 12.0f, 8.0f };
        const juce::Colour colours[] = { theme::cream, theme::cream, theme::amber, theme::coral };
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (colours[i]);
            g.fillRect (box.getX() + 5.0f + (float) i * 5.0f, box.getBottom() - 4.0f - heights[i], 3.0f, heights[i]);
        }
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("MONITOR");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 220.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 ("MEDIDOR"), tag2 (u8 ("LUFS · TRUE PEAK"));
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText (tag1, juce::Rectangle<float> (tagX, 22.0f, 160.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (tag2, juce::Rectangle<float> (tagX, 35.0f, 160.0f, 12.0f), juce::Justification::centredLeft);

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

    // ---- latência: nenhuma (o true peak é só análise) ----
    {
        juce::AttributedString s;
        s.append (u8 ("LATÊNCIA "), theme::mono (11.5f), theme::dim);
        s.append ("ZERO", theme::mono (11.5f), theme::cream);
        juce::TextLayout tl;
        tl.createLayout (s, 400.0f);
        const float w = tl.getWidth(), right = 604.0f;
        tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });
    }

    // ---- coluna de controles ----
    drawCard (g, { 782.0f, 66.0f, 206.0f, 502.0f });
    drawTitle (g, 792.0f, 76.0f,  u8 ("01 · "), "ALVO");
    drawTitle (g, 792.0f, 300.0f, u8 ("02 · "), "CHECAGEM");
    g.setColour (theme::line);
    g.fillRect (792.0f, 292.0f, 186.0f, 1.0f);

    drawDesc (g, { 890.0f, 98.0f, 92.0f, 92.0f }, "ALVO",
              u8 ("LUFS integrado que a plataforma pede."));
    drawDesc (g, { 792.0f, 436.0f, 188.0f, 126.0f }, "CHECAGEM",
              u8 ("MONO e NO ALVO só mudam o que você ouve; a medição é sempre da entrada. "
                  "NO ALVO baixa o volume até o ALVO, como as plataformas fazem. "
                  "ZERAR recomeça integrado, LRA e máximos; CONGELAR para a tela."));
}

void MonitorEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    loud.setBounds     (12, 66, 320, 234);
    history.setBounds  (342, 66, 430, 234);
    spectrum.setBounds (12, 310, 440, 258);
    stereo.setBounds   (462, 310, 180, 258);
    level.setBounds    (652, 310, 120, 258);

    targetMacro.setBounds (792, 96, 92, 92);
    tpKnob.setBounds  (786, 196, 66, 92);
    tolKnob.setBounds (852, 196, 66, 92);
    lraKnob.setBounds (918, 196, 66, 92);

    monoKey.setBounds   (792, 318, 92, 40);
    matchKey.setBounds  (890, 318, 92, 40);
    resetKey.setBounds  (792, 366, 92, 40);
    freezeKey.setBounds (890, 366, 92, 40);
    listenInfo.setBounds (792, 414, 190, 16);
}
