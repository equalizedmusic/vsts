#include "PluginEditor.h"

using namespace soco;

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
    real::card (g, r);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

float param (SocoProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

// dB com sinal: "+3,2 dB", "−4,0 dB", "0,0 dB"
juce::String signedDb (float v)
{
    if (v > 0.05f)  return "+" + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
    if (v < -0.05f) return u8 ("−") + juce::String (-v, 1).replaceCharacter ('.', ',') + " dB";
    return "0,0 dB";
}

juce::String dbfs (float gain)
{
    if (gain < 1.0e-5f) return u8 ("−∞ dBFS");
    const float db = juce::Decibels::gainToDecibels (gain);
    const auto  t  = juce::String (std::abs (db), 1).replaceCharacter ('.', ',') + " dBFS";
    return db < -0.05f ? u8 ("−") + t : db > 0.05f ? "+" + t : t;
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

// a cor da parte que manda no ganho: ATAQUE coral, SUSTAIN azul
juce::Colour partColour (float atkDb, float susDb)
{
    return std::abs (atkDb) >= std::abs (susDb) ? theme::coral : theme::ice;
}
} // namespace

// =====================================================================
// FORMA DE ONDA
// =====================================================================
void WaveStrip::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();

    real::card (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, "WAVEFORM ", u8 ("· ANTES E DEPOIS DO SOCO, AO VIVO"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::ice, "SUSTAIN", false);
        legendItem (g, x, theme::coral, "ATTACK", false);
        legendItem (g, x, theme::cream, "DEPOIS", false);
        legendItem (g, x, theme::dim, "ANTES", true);
    }

    const float x0 = 12.0f, x1 = W - 12.0f;
    const float top = 26.0f, bot = H - 44.0f;           // forma de onda
    const float lTop = H - 40.0f, lBot = H - 8.0f;      // faixa do ganho aplicado
    const float mid = (top + bot) * 0.5f, half = (bot - top) * 0.5f - 2.0f;
    const float lMid = (lTop + lBot) * 0.5f, lHalf = (lBot - lTop) * 0.5f - 2.0f;
    const juce::Rectangle<float> waveR (x0, top, x1 - x0, bot - top), laneR (x0, lTop, x1 - x0, lBot - lTop);
    real::screen (g, waveR, 3.0f);
    real::screen (g, laneR, 3.0f);

    // mais antigo à esquerda, agora à direita
    const int   wpos = proc.engine.colWrite.load (std::memory_order_acquire);
    const float cw   = (x1 - x0) / (float) historyCols;
    float maxV = 0.0f;
    juce::Path ghostTop, ghostBot, atkPath, susPath;
    for (int k = 0; k < historyCols; ++k)
    {
        const auto  idx = (size_t) ((wpos + k) % historyCols);
        const float b  = proc.engine.colBefore[idx].load (std::memory_order_relaxed);
        const float a  = proc.engine.colAfter[idx].load (std::memory_order_relaxed);
        const float ga = proc.engine.colAtk[idx].load (std::memory_order_relaxed);
        const float gs = proc.engine.colSus[idx].load (std::memory_order_relaxed);
        maxV = juce::jmax (maxV, a, b);

        const float hb = juce::jmin (1.0f, b / range) * half, ha = juce::jmin (1.0f, a / range) * half;
        const float hc = juce::jmin (ha, hb);
        const float x  = x0 + (float) k * cw;
        const auto  tint = partColour (ga, gs);

        // ANTES: fantasma; DEPOIS: creme; o que o SOCO pôs a mais (ou tirou) na cor da parte
        g.setColour (theme::dim.withAlpha (0.2f));
        g.fillRect (x, mid - hb, cw + 0.4f, juce::jmax (1.0f, hb * 2.0f));
        g.setColour (theme::cream.withAlpha (0.75f));
        g.fillRect (x, mid - hc, cw + 0.4f, juce::jmax (1.0f, hc * 2.0f));
        if (ha > hb + 0.5f)
        {
            g.setColour (tint.withAlpha (0.9f));
            g.fillRect (x, mid - ha, cw + 0.4f, ha - hb);
            g.fillRect (x, mid + hb, cw + 0.4f, ha - hb);
        }
        else if (hb > ha + 0.5f)
        {
            g.setColour (tint.withAlpha (0.3f));
            g.fillRect (x, mid - hb, cw + 0.4f, hb - ha);
            g.fillRect (x, mid + ha, cw + 0.4f, hb - ha);
        }

        // traço do ANTES e o ganho de cada parte (±15 dB ocupa a faixa)
        const float xc = x + cw * 0.5f;
        const float ya = lMid - juce::jlimit (-1.0f, 1.0f, ga / maxAttackDb) * lHalf;
        const float ys = lMid - juce::jlimit (-1.0f, 1.0f, gs / maxAttackDb) * lHalf;
        if (k == 0)
        {
            ghostTop.startNewSubPath (xc, mid - hb);
            ghostBot.startNewSubPath (xc, mid + hb);
            atkPath.startNewSubPath (xc, ya);
            susPath.startNewSubPath (xc, ys);
        }
        else
        {
            ghostTop.lineTo (xc, mid - hb);
            ghostBot.lineTo (xc, mid + hb);
            atkPath.lineTo (xc, ya);
            susPath.lineTo (xc, ys);
        }
    }
    g.setColour (theme::dim.withAlpha (0.85f));
    g.strokePath (ghostTop, juce::PathStrokeType (1.0f));
    g.strokePath (ghostBot, juce::PathStrokeType (1.0f));

    g.setColour (theme::segOff);
    g.fillRect (x0 + 4.0f, lMid, x1 - x0 - 8.0f, 1.0f);
    g.setColour (theme::ice);
    g.strokePath (susPath, juce::PathStrokeType (1.4f));
    g.setColour (theme::coral);
    g.strokePath (atkPath, juce::PathStrokeType (1.4f));
    g.setColour (theme::dim);
    g.setFont (theme::mono (8.5f));
    g.drawText ("GAIN APLICADO", juce::Rectangle<float> (x0 + 6.0f, lTop + 2.0f, 120.0f, 10.0f), juce::Justification::centredLeft);
    g.drawText (u8 ("±15 dB"), juce::Rectangle<float> (x1 - 66.0f, lTop + 2.0f, 60.0f, 10.0f), juce::Justification::centredRight);

    real::glass (g, waveR, 3.0f);
    real::glass (g, laneR, 3.0f);

    // escala acompanha os picos devagar (sem pular a cada frame)
    const float target = juce::jlimit (0.02f, 4.0f, maxV * 1.1f);
    range += (target - range) * 0.08f;
}

// =====================================================================
// GOLPE: um golpe de teste (estalo + corpo) passando pelo detector de verdade
// =====================================================================
void HitView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), PH = 240.0f;
    const float x0 = 30.0f, x1 = W - 8.0f, y0 = 10.0f, y1 = PH - 20.0f;
    constexpr double simRate = 8000.0;
    constexpr float  onset = 0.004f, span = 0.4f;   // s
    const int total = (int) (simRate * (double) span);

    // ---- simula ----
    if ((int) lvl.size() != total)
    {
        lvl.assign ((size_t) total, 0.0f);
        gAtk.assign ((size_t) total, 0.0f);
        gSus.assign ((size_t) total, 0.0f);
    }
    const float focusMs = param (proc, id::focus);
    Detector det;
    det.prepare (simRate);
    det.setFocus (focusMs);
    const float atkAmt = param (proc, id::attack) * 0.01f, susAmt = param (proc, id::sustain) * 0.01f;
    float atkBig = 0.0f, susBig = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        const float t = (float) i / (float) simRate - onset;
        float e = 0.0f;
        if (t >= 0.0f)
            e = juce::jmin (1.0f, t / 0.001f) * (0.7f * std::exp (-t / 0.012f) + 0.3f * std::exp (-t / 0.25f));
        float a = 0.0f, s = 0.0f;
        det.process (e, atkAmt, susAmt, a, s);
        lvl[(size_t) i]  = e;
        gAtk[(size_t) i] = a;
        gSus[(size_t) i] = s;
        if (std::abs (a) > std::abs (atkBig)) atkBig = a;
        if (std::abs (s) > std::abs (susBig)) susBig = s;
    }

    // tempo em escala de raiz: o começo do golpe fica largo
    auto xOfT = [=] (float tAbs) { return x0 + std::sqrt (juce::jlimit (0.0f, 1.0f, tAbs / span)) * (x1 - x0); };
    auto yOf  = [=] (float db) { return y0 + (12.0f - juce::jlimit (-48.0f, 12.0f, db)) / 60.0f * (y1 - y0); };

    const auto screenR = juce::Rectangle<float> (0.0f, 0.0f, W, PH).reduced (1.5f);
    real::screen (g, screenR, 4.0f);

    // grade de dB
    g.setFont (theme::mono (8.5f));
    for (int db = 0; db >= -36; db -= 12)
    {
        const float y = yOf ((float) db);
        g.setColour (theme::segOff);
        g.fillRect (x0, y, x1 - x0, 1.0f);
        g.setColour (theme::dim);
        g.drawText (db == 0 ? juce::String ("0") : u8 ("−") + juce::String (-db),
                    juce::Rectangle<float> (2.0f, y - 6.0f, x0 - 6.0f, 12.0f), juce::Justification::centredRight);
    }

    // janela do FOCO
    {
        const float fx0 = xOfT (onset), fx1 = xOfT (onset + focusMs * 0.001f);
        g.setColour (theme::cream.withAlpha (0.06f));
        g.fillRect (fx0, y0, fx1 - fx0, y1 - y0);
        g.setColour (theme::cream.withAlpha (0.7f));
        g.fillRect (fx0, y0, juce::jmax (1.0f, fx1 - fx0), 2.0f);
        g.setColour (theme::dim);
        g.drawText ("FOCO", juce::Rectangle<float> (fx1 + 3.0f, y0 - 2.0f, 40.0f, 10.0f), juce::Justification::centredLeft);
    }

    // ANTES (fantasma), o que mudou na cor da parte, DEPOIS (creme)
    juce::Path before, after;
    const int npx = juce::jmax (2, (int) (x1 - x0));
    for (int px = 0; px <= npx; ++px)
    {
        const float u  = (float) px / (float) npx;
        const int   i  = juce::jlimit (0, total - 1, (int) (u * u * span * (float) simRate));
        const float x  = x0 + u * (x1 - x0);
        const float bDb = juce::Decibels::gainToDecibels (lvl[(size_t) i], -60.0f);
        const float aDb = bDb + gAtk[(size_t) i] + gSus[(size_t) i];
        const float yb = yOf (bDb), ya = yOf (aDb);

        g.setColour (theme::dim.withAlpha (0.16f));
        g.fillRect (x, yb, 1.0f, juce::jmax (0.0f, y1 - yb));
        if (std::abs (ya - yb) > 0.5f)
        {
            g.setColour (partColour (gAtk[(size_t) i], gSus[(size_t) i]).withAlpha (ya < yb ? 0.6f : 0.3f));
            g.fillRect (x, juce::jmin (ya, yb), 1.0f, std::abs (ya - yb));
        }
        if (px == 0) { before.startNewSubPath (x, yb); after.startNewSubPath (x, ya); }
        else         { before.lineTo (x, yb);          after.lineTo (x, ya); }
    }
    g.setColour (theme::dim.withAlpha (0.9f));
    g.strokePath (before, juce::PathStrokeType (1.0f));
    g.setColour (theme::cream);
    g.strokePath (after, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // régua de tempo (a partir do golpe)
    g.setFont (theme::mono (8.5f));
    g.setColour (theme::dim);
    for (int ms : { 0, 10, 50, 100, 200, 390 })
    {
        const float x = xOfT (onset + (float) ms * 0.001f);
        g.fillRect (x, y1, 1.0f, 3.0f);
        const juce::String txt = ms == 390 ? juce::String ("ms") : juce::String (ms);
        g.drawText (txt, juce::Rectangle<float> (x - 15.0f, y1 + 4.0f, 30.0f, 11.0f), juce::Justification::centred);
    }

    real::glass (g, screenR, 4.0f);

    // rodapé: o máximo de cada parte nesse golpe
    auto line = [] (const juce::String& a, const juce::String& b, juce::Colour c)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), c);
        return s;
    };
    auto sl = line ("ATTACK ", signedDb (atkBig), theme::coral);
    auto sr = line ("SUSTAIN ", signedDb (susBig), theme::ice);
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, PH + 10.0f, W, 14.0f });
    sr.draw (g, { 0.0f, PH + 10.0f, W, 14.0f });

    g.setColour (theme::dim);
    g.setFont (theme::sans (10.0f));
    g.drawFittedText (u8 ("Golpe de teste com os ajustes de agora (sem o filtro do detector)."),
                      juce::Rectangle<int> (0, (int) PH + 30, (int) W, 26), juce::Justification::topLeft, 2);
}

// =====================================================================
// DETECTOR: resposta do filtro que o detector ouve
// =====================================================================
void DetectorView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    const float x0 = 8.0f, x1 = W - 8.0f, y0 = 20.0f, y1 = H - 16.0f;
    const int   mode = juce::jlimit (0, numDetModes - 1, juce::roundToInt (param (proc, id::detMode)));
    const float fc   = param (proc, id::detFreq);

    auto xOfF  = [=] (float f) { return x0 + std::log (f / 20.0f) / std::log (1000.0f) * (x1 - x0); };
    auto yOfDb = [=] (float db) { return y0 + juce::jlimit (0.0f, 1.0f, -db / 30.0f) * (y1 - y0); };
    auto respDb = [mode, fc] (float f)
    {
        const float r = f / fc, l = 10.0f * std::log10 (1.0f + r * r);
        if (mode == detLow)  return -2.0f * l;
        if (mode == detHigh) return 2.0f * (20.0f * std::log10 (r) - l);
        return 0.0f;
    };

    const auto screenR = getLocalBounds().toFloat().reduced (1.5f);
    real::screen (g, screenR, 4.0f);

    g.setFont (theme::mono (8.5f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = xOfF (f);
        g.setColour (theme::segOff);
        g.fillRect (x, y0, 1.0f, y1 - y0);
        g.setColour (theme::dim);
        g.drawText (f < 999.0f ? juce::String ("100") : f < 9999.0f ? juce::String ("1k") : juce::String ("10k"),
                    juce::Rectangle<float> (x - 15.0f, y1 + 2.0f, 30.0f, 11.0f), juce::Justification::centred);
    }

    juce::Path curve;
    const int pts = 120;
    for (int k = 0; k <= pts; ++k)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) k / (float) pts);
        const float x = xOfF (f), y = yOfDb (respDb (f));
        if (k == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    juce::Path fill (curve);
    fill.lineTo (x1, y1);
    fill.lineTo (x0, y1);
    fill.closeSubPath();
    g.setColour (theme::violet.withAlpha (mode == detAll ? 0.06f : 0.15f));
    g.fillPath (fill);
    g.setColour (mode == detAll ? theme::violet.withAlpha (0.5f) : theme::violet);
    g.strokePath (curve, juce::PathStrokeType (1.8f));

    if (mode != detAll)
    {
        g.setColour (theme::violet.withAlpha (0.5f));
        g.fillRect (xOfF (juce::jlimit (20.0f, 20000.0f, fc)), y0, 1.0f, y1 - y0);
    }

    juce::AttributedString s;
    const juce::String name = juce::String (detModeNames[mode]).toUpperCase();
    s.append (name, theme::mono (10.0f), theme::violet);
    s.append (mode == detAll ? u8 (" · O SOM INTEIRO") : u8 (" · CUTOFF ") + hzText (fc), theme::mono (10.0f), theme::dim);
    s.setJustification (juce::Justification::centredLeft);
    s.draw (g, { 8.0f, 4.0f, W - 16.0f, 13.0f });

    real::glass (g, screenR, 4.0f);
}

// =====================================================================
// AGORA: ganho de cada parte
// =====================================================================
void GainNowView::update (float atkDb, float susDb, float dt)
{
    const float k = juce::jmin (1.0f, dt * 6.0f);
    atk = std::abs (atkDb) > std::abs (atk) ? atkDb : atk + (atkDb - atk) * k;
    sus = std::abs (susDb) > std::abs (sus) ? susDb : sus + (susDb - sus) * k;
}

void GainNowView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    auto row = [&g, W] (const juce::String& label, float v, float y, juce::Colour c)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText (label, juce::Rectangle<float> (0.0f, y, 200.0f, 12.0f), juce::Justification::centredLeft);
        g.setColour (std::abs (v) > 0.05f ? c : theme::dim);
        g.setFont (theme::mono (11.0f));
        g.drawText (signedDb (v), juce::Rectangle<float> (0.0f, y, W, 12.0f), juce::Justification::centredRight);

        // barra a partir do meio: para a direita sobe, para a esquerda desce
        const float by = y + 17.0f, cx = W * 0.5f;
        const float len = juce::jlimit (-1.0f, 1.0f, v / maxAttackDb) * cx;
        g.setColour (theme::segOff);
        g.fillRoundedRectangle (0.0f, by, W, 4.0f, 2.0f);
        g.setColour (c);
        g.fillRect (juce::jmin (cx, cx + len), by, std::abs (len), 4.0f);
        g.setColour (theme::dim);
        g.fillRect (cx - 0.5f, by - 2.0f, 1.0f, 8.0f);
    };
    row ("ATTACK AGORA", atk, 0.0f, theme::coral);
    row ("SUSTAIN AGORA", sus, 38.0f, theme::ice);
}

// =====================================================================
// PICOS em dBFS
// =====================================================================
void PeakView::update (float peakIn, float peakOut, float dt)
{
    pkIn  = juce::jmax (peakIn,  pkIn  * std::exp (-dt * 3.0f));
    pkOut = juce::jmax (peakOut, pkOut * std::exp (-dt * 3.0f));
    if (peakOut >= maxOut) { maxOut = peakOut; holdLeft = 3.0f; }
    else if ((holdLeft -= dt) <= 0.0f) maxOut = juce::jmax (pkOut, maxOut * std::exp (-dt * 1.5f));
}

void PeakView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    auto row = [&g, W] (const juce::String& label, float v, float y, bool bar)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText (label, juce::Rectangle<float> (0.0f, y, 200.0f, 12.0f), juce::Justification::centredLeft);
        g.setColour (v > 0.891f ? theme::coral : theme::cream);   // acima de −1 dBFS fica coral
        g.setFont (theme::mono (11.0f));
        g.drawText (dbfs (v), juce::Rectangle<float> (0.0f, y, W, 12.0f), juce::Justification::centredRight);
        if (! bar) return;
        const float frac = juce::jmap (juce::Decibels::gainToDecibels (v, -48.0f), -48.0f, 0.0f, 0.0f, 1.0f);
        g.setColour (theme::segOff);
        g.fillRoundedRectangle (0.0f, y + 16.0f, W, 4.0f, 2.0f);
        g.setColour (theme::cream.withAlpha (0.7f));
        g.fillRoundedRectangle (0.0f, y + 16.0f, W * juce::jlimit (0.0f, 1.0f, frac), 4.0f, 2.0f);
    };
    row ("INPUT PEAK", pkIn, 0.0f, true);
    row ("OUTPUT PEAK", pkOut, 28.0f, true);
    row ("MAX OUTPUT (3 s)", maxOut, 58.0f, false);
}

// =====================================================================
// EDITOR
// =====================================================================
SocoEditor::SocoEditor (SocoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), hit (p), detView (p),
      // cor do knob = cor do que ele mexe no desenho
      attackMacro (p.apvts, id::attack,  "ATTACK",   true,  theme::coral),
      sustain     (p.apvts, id::sustain, "SUSTAIN",  false, theme::ice),
      focus       (p.apvts, id::focus,   "FOCO",     false, theme::cream),
      detMode     (p.apvts, id::detMode, "DETECTOR", false, theme::violet),
      detFreq     (p.apvts, id::detFreq, "CUTOFF",   false, theme::violet),
      output      (p.apvts, id::output,  "GAIN",     false, theme::cream),
      mix         (p.apvts, id::mix),
      deltaKey    ("DELTA", theme::leaf),
      clipKey     (u8 ("CLIPPER DE PROTEÇÃO"), theme::amber)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &hit, &detView, &gainNow, &peaks, &attackMacro, &sustain, &focus, &detMode, &detFreq, &output,
             &mix, &deltaKey, &clipKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    deltaAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::delta, deltaKey);
    clipAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::clip, clipKey);
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
    setResizeLimits (juce::roundToInt (ui::baseW * ui::minScale), juce::roundToInt (ui::baseH * ui::minScale),
                     juce::roundToInt (ui::baseW * ui::maxScale), juce::roundToInt (ui::baseH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) ui::baseW / (double) ui::baseH);
    setScale (openScale);
    startTimerHz (30);
}

SocoEditor::~SocoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SocoEditor::showPresetMenu()
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

    juce::Component::SafePointer<SocoEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void SocoEditor::askNewPreset()
{
    juce::Component::SafePointer<SocoEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void SocoEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<SocoEditor> safe (this);
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

void SocoEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) ui::baseW - openScale) > 0.005f)
        setScale (openScale);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    gainNow.update (proc.engine.atkNow.exchange (0.0f), proc.engine.susNow.exchange (0.0f), dt);

    const float pin = proc.meterIn.exchange (0.0f), pout = proc.meterOut.exchange (0.0f);
    peaks.update (pin, pout, dt);
    levelIn  = juce::jmax (pin,  levelIn * 0.85f);
    levelOut = juce::jmax (pout, levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    strip.repaint();
    hit.repaint();
    detView.repaint();
    gainNow.repaint();
    peaks.repaint();

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void SocoEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void SocoEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void SocoEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void SocoEditor::drawCanvas (juce::Graphics& g)
{
    real::chassis (g, (float) ui::baseW, (float) ui::baseH);

    // ---- logo: o golpe (miolo coral e o estalo em volta) ----
    {
        const float cx = 31.0f, cy = 34.0f;
        g.setColour (theme::coral);
        g.fillEllipse (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
        g.setColour (theme::cream);
        const juce::PathStrokeType rayStroke (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        for (int k = 0; k < 8; ++k)
        {
            const float a = (float) k * juce::MathConstants<float>::pi * 0.25f;
            const float r0 = 9.0f, r1 = k % 2 == 0 ? 14.0f : 12.0f;
            juce::Path ray;
            ray.startNewSubPath (cx + r0 * std::sin (a), cy - r0 * std::cos (a));
            ray.lineTo (cx + r1 * std::sin (a), cy - r1 * std::cos (a));
            g.strokePath (ray, rayStroke);
        }
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("SOCO");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 220.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 ("TRANSIENT SHAPER"), tag2 (u8 ("ATTACK · SUSTAIN"));
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

    // ---- latência (zero: o detector não olha para a frente) ----
    {
        const double ms = 1000.0 * (double) proc.getLatencySamples() / juce::jmax (1.0, proc.rate.load());
        juce::AttributedString s;
        s.append ("LATENCY ", theme::mono (11.5f), theme::dim);
        s.append (juce::String (ms, 2).replaceCharacter ('.', ',') + " ms", theme::mono (11.5f), theme::cream);
        juce::TextLayout tl;
        tl.createLayout (s, 400.0f);
        const float w = tl.getWidth(), right = 604.0f;
        tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });
    }

    // ---- cards ----
    drawCard (g, { 12.0f, 226.0f, 262.0f, 342.0f });
    drawCard (g, { 284.0f, 226.0f, 258.0f, 342.0f });
    drawCard (g, { 552.0f, 226.0f, 258.0f, 342.0f });
    drawCard (g, { 820.0f, 226.0f, 168.0f, 342.0f });

    drawTitle (g, 22.0f,  236.0f, "GOLPE ", u8 ("· ANTES × DEPOIS"));
    drawTitle (g, 294.0f, 236.0f, u8 ("01 · "), "SOCO");
    drawTitle (g, 562.0f, 236.0f, u8 ("02 · "), "DETECTOR");
    drawTitle (g, 830.0f, 236.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 262.0f, 140.0f, 110.0f }, "MACRO ATTACK",
              u8 ("Mais = o começo do golpe salta (coral). Menos = o golpe amacia. Não depende do volume."));
}

void SocoEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 150);
    hit.setBounds (22, 256, 240, 300);

    attackMacro.setBounds (294, 256, 92, 92);
    sustain.setBounds     (294, 382, 119, 92);
    focus.setBounds       (413, 382, 119, 92);
    gainNow.setBounds     (298, 482, 230, 74);

    detView.setBounds (562, 256, 238, 106);
    detMode.setBounds (562, 368, 119, 92);
    detFreq.setBounds (681, 368, 119, 92);
    peaks.setBounds   (566, 474, 230, 84);

    deltaKey.setBounds (830, 256, 148, 36);
    clipKey.setBounds  (830, 298, 148, 36);
    mix.setBounds (830, 342, 44, 216);
    output.setBounds (884, 342, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);
}
