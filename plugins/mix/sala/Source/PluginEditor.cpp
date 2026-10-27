#include "PluginEditor.h"
#include <cstdint>

using namespace sala;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuLicenca = 20005;

// rack de 19": a frente tem o layout de 1000 × 580 e as orelhas ficam dos lados
constexpr int ear = 32, rackW = ui::baseW + 2 * ear;
const juce::Colour tinta (0xff2c3a5a);   // cor da frente do SALA

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
    painel::secao (g, r);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// nome, duas linhas de descrição e a marca, a partir do x = 56 (o logo fica antes)
float drawBrand (juce::Graphics& g, const juce::String& name, const juce::String& tag1, const juce::String& tag2)
{
    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 260.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText (tag1, juce::Rectangle<float> (tagX, 22.0f, 200.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (tag2, juce::Rectangle<float> (tagX, 35.0f, 200.0f, 12.0f), juce::Justification::centredLeft);

    const float brandX = tagX + juce::jmax (textWidth (tagFont, tag1), textWidth (tagFont, tag2)) + 20.0f;
    g.setColour (theme::line);
    g.fillRect (brandX - 10.0f, 24.0f, 1.0f, 22.0f);
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText ("BY", juce::Rectangle<float> (brandX, 22.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
    const auto brandFont = theme::sans (10.5f, true, 0.06f);
    g.setColour (theme::amber);
    g.setFont (brandFont);
    g.drawText ("InsideStudioSP", juce::Rectangle<float> (brandX, 34.0f, 140.0f, 13.0f), juce::Justification::centredLeft);
    return brandX + textWidth (brandFont, "InsideStudioSP");   // onde a marca termina
}

// cabeçalho: o pré-delay em uso; no tempo, a divisão e o BPM (LED aceso = andamento vindo do host)
void drawPreStatus (juce::Graphics& g, float preMs, int sync, float bpm, bool host)
{
    const auto f = theme::mono (11.5f);
    const float right = 604.0f;

    juce::AttributedString a, b;
    a.append ("PRE-DELAY ", f, theme::dim);
    a.append (msText (preMs), f, theme::cream);
    if (sync > 0)
    {
        b.append (juce::String (syncNames[juce::jlimit (0, numSync - 1, sync)]), f, theme::cream);
        b.append (u8 (" · "), f, theme::dim);
        b.append (juce::String (bpm, 1).replaceCharacter ('.', ','), f, theme::cream);
        b.append (host ? juce::String (" BPM") : u8 (" BPM (PADRÃO)"), f, theme::dim);
    }
    else
        b.append ("LIVRE", f, theme::dim);

    juce::TextLayout ta, tb;
    ta.createLayout (a, 400.0f);
    tb.createLayout (b, 400.0f);
    ta.draw (g, { right - ta.getWidth(), 27.0f - ta.getHeight() * 0.5f, ta.getWidth() + 1.0f, ta.getHeight() });
    tb.draw (g, { right - tb.getWidth(), 41.0f - tb.getHeight() * 0.5f, tb.getWidth() + 1.0f, tb.getHeight() });

    const bool lit = sync > 0 && host;
    const juce::Rectangle<float> led (right - juce::jmax (ta.getWidth(), tb.getWidth()) - 13.0f, 30.5f, 7.0f, 7.0f);
    real::led (g, led.getCentre(), 3.5f, theme::amber, lit ? 1.0f : 0.0f);
}

float param (SalaProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
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

juce::String minusDb (float db)
{
    return db < -0.05f ? u8 ("−") + juce::String (-db, 1).replaceCharacter ('.', ',') + " dB" : juce::String ("0,0 dB");
}

// envelope desenhado da cauda (dB) a 't' segundos do golpe
float tailDb (float t, float onset, float rise, float dec)
{
    const float a = t - onset;
    float db = -10.0f - 60.0f * a / juce::jmax (0.1f, dec);
    if (a < rise) db -= 18.0f * (1.0f - a / rise);   // a difusão faz a cauda crescer mais macia
    return db;
}
} // namespace

// =====================================================================
// RESPOSTA: o que acontece depois de um golpe (tempo em escala de raiz, para caber o começo e o fim)
// =====================================================================
void ResponseView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    const float pre = proc.preMsNow.load() * 0.001f;
    const float dec = param (proc, id::decay);
    const float sz  = param (proc, id::size) * 0.01f;
    const float dif = param (proc, id::diffusion) * 0.01f;
    const float ear = param (proc, id::early) * 0.01f;
    const float erF = erFactor (sz);
    const float span = juce::jmax (0.3f, pre + dec * 1.08f);

    painel::secao (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, "RESPOSTA ", u8 ("· RT60 ") + secText (dec) + u8 (" · PRE-DELAY ") + msText (pre * 1000.0f));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::cream, "AGORA", true);
        legendItem (g, x, theme::amber, "TAIL", false);
        legendItem (g, x, theme::violet, "EARLY REFLECTIONS", false);
        legendItem (g, x, theme::ice, "PRE-DELAY", false);
        legendItem (g, x, theme::cream, "DRY", false);
    }

    const float x0 = 12.0f, x1 = W - 12.0f, top = 26.0f, bot = H - 10.0f;
    const juce::Rectangle<float> screenR (x0, top, x1 - x0, bot - top);
    real::screen (g, screenR, 3.0f);

    const float gx0 = x0 + 34.0f, gx1 = x1 - 10.0f, gy0 = top + 8.0f, gy1 = bot - 16.0f;
    auto xOf = [=] (float sec) { return gx0 + std::sqrt (juce::jlimit (0.0f, 1.0f, sec / span)) * (gx1 - gx0); };
    auto yOf = [=] (float db)  { return gy0 + juce::jlimit (0.0f, 1.0f, -db / 60.0f) * (gy1 - gy0); };

    // grade: dB e tempo
    g.setFont (theme::mono (9.0f));
    for (int db = 0; db >= -60; db -= 20)
    {
        g.setColour (theme::segOff);
        g.fillRect (gx0, yOf ((float) db), gx1 - gx0, 1.0f);
        g.setColour (theme::dim);
        g.drawText (juce::String (db), juce::Rectangle<float> (x0, yOf ((float) db) - 6.0f, 28.0f, 12.0f), juce::Justification::centredRight);
    }
    {
        const float ticks[] = { 0.01f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
        float lastX = -100.0f;
        for (float t : ticks)
        {
            if (t > span * 0.97f) break;
            const float x = xOf (t);
            g.setColour (theme::segOff);
            g.fillRect (x, gy0, 1.0f, gy1 - gy0);
            if (x - lastX < 40.0f) continue;
            lastX = x;
            g.setColour (theme::dim);
            const juce::String txt = t < 1.0f ? juce::String (juce::roundToInt (t * 1000.0f)) + " ms" : secText (t);
            g.drawText (txt, juce::Rectangle<float> (x - 30.0f, gy1 + 3.0f, 60.0f, 11.0f), juce::Justification::centred);
        }
    }

    // pré-delay: o silêncio antes da sala responder
    if (pre > 0.0005f)
    {
        const float px = xOf (pre);
        g.setColour (theme::ice.withAlpha (0.12f));
        g.fillRect (gx0, gy0, px - gx0, gy1 - gy0);
        g.setColour (theme::ice.withAlpha (0.8f));
        g.fillRect (px - 0.5f, gy0, 1.0f, gy1 - gy0);
        g.setColour (theme::ice);
        g.setFont (theme::mono (9.0f));
        g.drawText (msText (pre * 1000.0f), juce::Rectangle<float> (px + 4.0f, gy0, 60.0f, 11.0f), juce::Justification::centredLeft);
    }

    // cauda: textura (grão some com a DIFUSÃO) e a curva de decaimento
    const float onset = pre + lineMs[0] * 0.001f * sizeFactor (sz) * 0.5f;
    const float rise  = 0.008f + 0.03f * dif;
    {
        const int cols = 300;
        const float cw = (gx1 - gx0) / (float) cols;
        const float grain = 0.25f + 0.6f * dif;   // menor = mais irregular
        std::uint32_t seed = 0x2545F491u;
        g.setColour (theme::amber.withAlpha (0.28f));
        for (int c = 0; c < cols; ++c)
        {
            seed = seed * 1664525u + 1013904223u;
            const float rnd = (float) (seed >> 8) * (1.0f / 16777216.0f);
            const float u = ((float) c + 0.5f) / (float) cols;
            const float t = span * u * u;
            if (t < onset) continue;
            const float db = tailDb (t, onset, rise, dec) + 20.0f * std::log10 (grain + (1.0f - grain) * rnd);
            if (db < -60.0f) continue;
            const float y = yOf (db);
            g.fillRect (gx0 + (float) c * cw, y, juce::jmax (1.0f, cw - 0.6f), gy1 - y);
        }

        juce::Path curve, fill;
        const int pts = 160;
        bool started = false;
        float lastXc = gx0;
        for (int k = 0; k <= pts; ++k)
        {
            const float t = onset + (span - onset) * (float) k / (float) pts;
            const float db = juce::jmax (-60.0f, tailDb (t, onset, rise, dec));
            const float x = xOf (t), y = yOf (db);
            if (! started) { curve.startNewSubPath (x, y); started = true; }
            else curve.lineTo (x, y);
            lastXc = x;
            if (db <= -60.0f) break;
        }
        fill = curve;
        fill.lineTo (lastXc, gy1);
        fill.lineTo (xOf (onset), gy1);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (0.22f), 0.0f, gy0, theme::amber.withAlpha (0.0f), 0.0f, gy1, false));
        g.fillPath (fill);
        g.setColour (theme::amber);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // reflexões iniciais: um traço por eco (mais largo e mais suave com DIFUSÃO); direita mais fraca
    {
        const float bw = 1.5f + 5.0f * dif;
        for (int side = 0; side < 2; ++side)
            for (int k = 0; k < numTaps; ++k)
            {
                const float w = tapWeight (k, ear);
                if (w <= 0.001f) continue;
                const float t  = pre + (side == 0 ? tapMsL[k] : tapMsR[k]) * 0.001f * erF;
                const float db = 20.0f * std::log10 (std::abs (tapGain[k])) - 3.0f;
                const float x = xOf (t), y = yOf (db);
                g.setColour (theme::violet.withAlpha (w * (0.95f - 0.5f * dif) * (side == 0 ? 1.0f : 0.5f)));
                g.fillRect (x - bw * 0.5f, y, bw, gy1 - y);
            }
    }

    // seco: o golpe
    g.setColour (theme::cream);
    g.fillRect (gx0 - 1.0f, yOf (0.0f), 2.0f, gy1 - yOf (0.0f));

    // cauda de agora (pico do molhado)
    {
        const float lvl = juce::Decibels::gainToDecibels (levelShown, -60.0f);
        if (lvl > -59.0f)
        {
            const float y = yOf (lvl);
            g.setColour (theme::cream.withAlpha (0.85f));
            g.fillRect (gx0, y - 0.6f, gx1 - gx0, 1.2f);
            g.setFont (theme::mono (9.0f));
            g.drawText ("AGORA " + minusDb (lvl), juce::Rectangle<float> (gx1 - 120.0f, y - 12.0f, 118.0f, 11.0f), juce::Justification::centredRight);
        }
    }

    real::glass (g, screenR, 3.0f);
}

// =====================================================================
// EQ DA CAUDA: passa-altas × passa-baixas (coral), o amortecimento (folha) e a largura
// =====================================================================
void TailEqView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth();
    const float lo  = param (proc, id::lowCut), hi = param (proc, id::highCut);
    const float dmp = param (proc, id::damp) * 0.01f;
    const float dec = param (proc, id::decay);
    const float sz  = param (proc, id::size) * 0.01f;
    const float wid = param (proc, id::width) * 0.01f;
    const float energy = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (levelShown, -60.0f) + 60.0f) / 60.0f);

    const auto screenR = juce::Rectangle<float> (0.0f, 0.0f, S, S).reduced (1.5f);
    real::screen (g, screenR, 4.0f);

    const float gx0 = 30.0f, gx1 = S - 16.0f, gy0 = 14.0f, gy1 = S - 78.0f;
    auto xOf = [=] (float hz) { return gx0 + (std::log10 (juce::jmax (20.0f, hz)) - std::log10 (20.0f)) / 3.0f * (gx1 - gx0); };
    auto hzOf = [=] (float x) { return 20.0f * std::pow (10.0f, 3.0f * (x - gx0) / (gx1 - gx0)); };
    auto yOf = [=] (float db) { return gy0 + juce::jlimit (0.0f, 1.0f, (6.0f - db) / 42.0f) * (gy1 - gy0); };

    // grade
    g.setFont (theme::mono (9.0f));
    for (int db = 0; db >= -36; db -= 12)
    {
        g.setColour (theme::segOff);
        g.fillRect (gx0, yOf ((float) db), gx1 - gx0, 1.0f);
        g.setColour (theme::dim);
        g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, yOf ((float) db) - 6.0f, 26.0f, 12.0f), juce::Justification::centredRight);
    }
    {
        const float marks[] = { 100.0f, 1000.0f, 10000.0f };
        const char* const names[] = { "100", "1k", "10k" };
        for (int k = 0; k < 3; ++k)
        {
            const float x = xOf (marks[k]);
            g.setColour (theme::segOff);
            g.fillRect (x, gy0, 1.0f, gy1 - gy0);
            g.setColour (theme::dim);
            g.drawText (names[k], juce::Rectangle<float> (x - 20.0f, gy1 + 3.0f, 40.0f, 11.0f), juce::Justification::centred);
        }
    }

    // curva do EQ: passa-altas e passa-baixas de 2 polos (−6 dB no corte); o brilho segue a cauda de agora
    {
        juce::Path curve;
        const int pts = 120;
        for (int k = 0; k <= pts; ++k)
        {
            const float x  = gx0 + (gx1 - gx0) * (float) k / (float) pts;
            const float hz = hzOf (x);
            const float q = hz / lo, p = hz / hi;
            const float a = (q * q / (1.0f + q * q)) / (1.0f + p * p);
            const float y = yOf (20.0f * std::log10 (juce::jmax (1.0e-6f, a)));
            if (k == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (gx1, gy1);
        fill.lineTo (gx0, gy1);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (theme::coral.withAlpha (0.10f + 0.45f * energy), 0.0f, gy0,
                                                 theme::coral.withAlpha (0.0f), 0.0f, gy1, false));
        g.fillPath (fill);
        g.setColour (theme::coral);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (float fc : { lo, hi })
        {
            const float cx = xOf (fc), cy = yOf (-6.0f);
            g.setColour (theme::coral.withAlpha (0.3f));
            g.fillEllipse (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
            g.setColour (theme::coral);
            g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        }
    }

    // amortecimento: quanto do decay sobra em cada frequência (100% no alto da grade)
    {
        const float lenSec = 0.001f * 0.125f * (lineMs[0] + lineMs[1] + lineMs[2] + lineMs[3]
                                               + lineMs[4] + lineMs[5] + lineMs[6] + lineMs[7]) * sizeFactor (sz);
        const float gDb = -60.0f * lenSec / juce::jmax (0.1f, dec);
        const float fd  = dampHz (dmp);
        juce::Path rt;
        const int pts = 80;
        for (int k = 0; k <= pts; ++k)
        {
            const float x  = gx0 + (gx1 - gx0) * (float) k / (float) pts;
            const float hz = hzOf (x);
            const float lpDb = -10.0f * std::log10 (1.0f + (hz / fd) * (hz / fd));
            const float ratio = gDb / (gDb + lpDb);
            const float y = gy0 + (1.0f - juce::jlimit (0.0f, 1.0f, ratio)) * (gy1 - gy0);
            if (k == 0) rt.startNewSubPath (x, y); else rt.lineTo (x, y);
        }
        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, rt, dashes, 2);
        g.setColour (theme::leaf.withAlpha (0.85f));
        g.fillPath (dashed);
        g.setFont (theme::mono (8.5f));
        g.drawText ("DECAY", juce::Rectangle<float> (gx1 - 60.0f, gy0 + 2.0f, 58.0f, 10.0f), juce::Justification::centredRight);
    }

    // energia da cauda agora
    {
        const float bx = S - 10.0f;
        g.setColour (theme::segOff);
        g.fillRoundedRectangle (bx, gy0, 4.0f, gy1 - gy0, 2.0f);
        const float h = (gy1 - gy0) * energy;
        g.setColour (theme::cream);
        g.fillRoundedRectangle (bx, gy1 - h, 4.0f, h, 2.0f);
    }

    // LARGURA: do mono (meio) ao 150%
    {
        const float by = S - 46.0f, cx = (gx0 + gx1) * 0.5f;
        g.setColour (theme::segOff);
        g.fillRoundedRectangle (gx0, by, gx1 - gx0, 6.0f, 2.0f);
        const float half = juce::jmax (1.0f, (gx1 - gx0) * 0.5f * wid / 1.5f);
        g.setColour (theme::cream);
        g.fillRoundedRectangle (cx - half, by, half * 2.0f, 6.0f, 2.0f);
        g.setColour (theme::dim);
        g.setFont (theme::mono (9.0f));
        g.drawText ("MONO", juce::Rectangle<float> (gx0, by + 9.0f, 50.0f, 11.0f), juce::Justification::centredLeft);
        g.drawText ("150%", juce::Rectangle<float> (gx1 - 50.0f, by + 9.0f, 50.0f, 11.0f), juce::Justification::centredRight);
        g.drawText ("WIDTH", juce::Rectangle<float> (gx0, by + 9.0f, gx1 - gx0, 11.0f), juce::Justification::centred);
    }

    real::glass (g, screenR, 4.0f);

    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    auto sl = line ("RT60 ", secText (dec));
    auto sr = line ("DUCK ", minusDb (duckShown));
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
    sr.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
}

// =====================================================================
// EDITOR
// =====================================================================
SalaEditor::SalaEditor (SalaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), response (p), eqView (p),
      // cor do knob = cor do que ele mexe no desenho
      sizeMacro (p.apvts, id::size,      "SIZE",               true,  theme::amber),
      decay     (p.apvts, id::decay,     "DECAY",              false, theme::amber),
      predelay  (p.apvts, id::predelay,  "PRE-DELAY",          false, theme::ice),
      preSync   (p.apvts, id::preSync,   "PRE SYNC",           false, theme::ice),
      diffusion (p.apvts, id::diffusion, "DIFFUSION",          false, theme::violet),
      early     (p.apvts, id::early,     "EARLY REF.",         false, theme::violet),
      lowCut    (p.apvts, id::lowCut,    "LOW CUT",            false, theme::coral),
      highCut   (p.apvts, id::highCut,   "HIGH CUT",           false, theme::coral),
      damp      (p.apvts, id::damp,      "DAMPING",            false, theme::leaf),
      width     (p.apvts, id::width,     "WIDTH",              false, theme::cream),
      duck      (p.apvts, id::duck,      "DUCK",               false, theme::cream),
      output    (p.apvts, id::output,    "GAIN",               false, theme::cream),
      mix       (p.apvts, id::mix),
      soloKey   (u8 ("SÓ A SALA"), theme::amber)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, rackW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &response, &eqView, &sizeMacro, &decay, &predelay, &preSync, &diffusion, &early, &lowCut, &highCut, &damp,
             &width, &duck, &output, &mix, &soloKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    soloAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::solo, soloKey);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

    prevBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    nextBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    saveBtn.onClick    = [this] { askNewPreset(); };
    presetName.onClick = [this] { showPresetMenu(); };

    layoutCanvas();

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    // o tamanho salvo é lido ANTES do setResizeLimits: ele encolhe a janela ainda vazia para o mínimo
    // e o resized() gravaria 75% por cima do que o usuário escolheu
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (rackW * ui::minScale), juce::roundToInt (ui::baseH * ui::minScale),
                     juce::roundToInt (rackW * ui::maxScale), juce::roundToInt (ui::baseH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) rackW / (double) ui::baseH);
    setScale (openScale);
    startTimerHz (30);
    ativacao.anexar (*this);
}

SalaEditor::~SalaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SalaEditor::showPresetMenu()
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
    menu.addItem (menuLicenca, u8 ("Licença…"));

    juce::Component::SafePointer<SalaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void SalaEditor::askNewPreset()
{
    juce::Component::SafePointer<SalaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void SalaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<SalaEditor> safe (this);
    const auto name = proc.getPresetName();
    switch (result)
    {
        case menuSaveAs:
            askNewPreset();
            break;
        case menuLicenca:
            ativacao.mostrarInfo();
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

void SalaEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) rackW - openScale) > 0.005f)
        setScale (openScale);

    response.levelShown = juce::jmax (proc.engine.levelNow.exchange (0.0f), response.levelShown * 0.9f);
    eqView.levelShown   = response.levelShown;
    eqView.duckShown    = juce::jmin (proc.engine.duckNow.exchange (0.0f), eqView.duckShown * 0.85f);
    response.repaint();
    eqView.repaint();

    levelIn  = juce::jmax (proc.meterIn.exchange (0.0f),  levelIn * 0.85f);
    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());

    const float bpm  = proc.shownBpm.load();
    const float pre  = proc.preMsNow.load();
    const bool  host = proc.hostTempo.load();
    const int   sync = juce::roundToInt (param (proc, id::preSync));
    if (std::abs (bpm - lastBpm) > 0.05f || std::abs (pre - lastPreMs) > 0.5f || host != lastHost || sync != lastSync)
    {
        lastBpm = bpm;
        lastPreMs = pre;
        lastHost = host;
        lastSync = sync;
        canvas.repaint (360 + ear, 14 + 0, 250, 40);
    }
}

void SalaEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) rackW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void SalaEditor::resized()
{
    const float scale = (float) getWidth() / (float) rackW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void SalaEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void SalaEditor::drawCanvas (juce::Graphics& g)
{
    painel::rack (g, (float) rackW, (float) ui::baseH, (float) ear, tinta);
    g.addTransform (juce::AffineTransform::translation ((float) ear, 0.0f));   // daqui em diante, o layout da frente

    // ---- logo: a sala, a fonte no canto e as ondas batendo nas paredes ----
    {
        const juce::Rectangle<float> room (17.0f, 21.0f, 28.0f, 26.0f);
        g.setColour (theme::cream);
        g.drawRoundedRectangle (room, 3.0f, 1.6f);
        const float sx = 23.5f, sy = 41.0f;
        for (int k = 0; k < 3; ++k)
        {
            const float rr = 5.5f + 5.0f * (float) k;
            juce::Path arc;
            arc.addCentredArc (sx, sy, rr, rr, 0.0f, 0.0f, juce::MathConstants<float>::halfPi, true);
            g.setColour ((k == 0 ? theme::violet : theme::amber).withAlpha (0.95f - 0.25f * (float) k));
            g.strokePath (arc, juce::PathStrokeType (1.3f));
        }
        g.setColour (theme::amber);
        g.fillEllipse (sx - 2.5f, sy - 2.5f, 5.0f, 5.0f);
    }

    drawBrand (g, "SALA", "REVERB DE MIX", u8 ("LIMPO E TRANSPARENTE"));
    drawPreStatus (g, proc.preMsNow.load(), juce::roundToInt (param (proc, id::preSync)), proc.shownBpm.load(), proc.hostTempo.load());

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "TAIL EQ ", u8 ("· dB × FREQUÊNCIA"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), u8 ("ESPAÇO"));
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "TAIL");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 685.0f, 472.0f, 117.0f, 90.0f }, "DUCK",
              u8 ("A cauda abaixa enquanto o DRY toca e volta nas pausas."));
}

void SalaEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    response.setBounds (12, 66, 976, 140);
    eqView.setBounds (22, 246, 240, 264);

    sizeMacro.setBounds (294, 246, 92, 92);
    decay.setBounds     (413, 246, 119, 92);
    predelay.setBounds  (294, 376, 119, 92);
    preSync.setBounds   (413, 376, 119, 92);
    diffusion.setBounds (294, 468, 119, 92);
    early.setBounds     (413, 468, 119, 92);

    lowCut.setBounds  (562, 246, 119, 92);
    highCut.setBounds (681, 246, 119, 92);
    damp.setBounds    (562, 376, 119, 92);
    width.setBounds   (681, 376, 119, 92);
    duck.setBounds    (562, 468, 119, 92);

    soloKey.setBounds (830, 246, 148, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (883, 446, 94, 55);
    meterOut.setBounds (883, 503, 94, 55);

    for (auto* c : canvas.getChildren())   // a frente começa depois da orelha esquerda
        c->setTopLeftPosition (c->getX() + ear, c->getY());
}
