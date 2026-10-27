#include "PluginEditor.h"

using namespace barragem;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuLicenca = 20005;

// rack de 19": a frente tem o layout de 1000 × 580 e as orelhas ficam dos lados
constexpr int ear = 32, rackW = ui::baseW + 2 * ear;
const juce::Colour tinta (0xff4a4f3a);   // cor da frente do BARRAGEM

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

float param (BarragemProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

// sinal de menos tipográfico
juce::String neg (const juce::String& s)
{
    return s.replace ("-", u8 ("−"));
}

juce::String dbfsText (float gain)
{
    if (gain < 1.0e-5f) return u8 ("−∞ dBFS");
    const float db = juce::Decibels::gainToDecibels (gain);
    return neg ((db > 0.05f ? "+" : "") + juce::String (db, 1).replaceCharacter ('.', ',')) + " dBFS";
}

juce::String lufsText (float v)
{
    if (v <= LoudnessMeter::silence + 1.0f) return u8 ("— LUFS");
    return neg (juce::String (v, 1).replaceCharacter ('.', ',')) + " LUFS";
}

juce::String grText (float gr)
{
    return gr > 0.05f ? u8 ("−") + juce::String (gr, 1).replaceCharacter ('.', ',') + " dB" : juce::String ("0,0 dB");
}

juce::String timeText (float secs)
{
    const int t = juce::jmax (0, (int) secs);
    return juce::String (t / 60).paddedLeft ('0', 2) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
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

// rótulo à esquerda e valor à direita, numa linha de 12 px
void labelValue (juce::Graphics& g, float w, float y, const juce::String& label, const juce::String& value, juce::Colour c)
{
    g.setColour (theme::dim);
    g.setFont (theme::sans (9.0f, true, 0.2f));
    g.drawText (label, juce::Rectangle<float> (0.0f, y, w, 12.0f), juce::Justification::centredLeft);
    g.setColour (c);
    g.setFont (theme::mono (11.0f));
    g.drawText (value, juce::Rectangle<float> (0.0f, y, w, 12.0f), juce::Justification::centredRight);
}

void thinBar (juce::Graphics& g, float w, float frac, float y, juce::Colour c)
{
    g.setColour (theme::segOff);
    g.fillRoundedRectangle (0.0f, y, w, 4.0f, 2.0f);
    g.setColour (c);
    g.fillRoundedRectangle (0.0f, y, w * juce::jlimit (0.0f, 1.0f, frac), 4.0f, 2.0f);
}
} // namespace

// =====================================================================
// FORMA DE ONDA
// =====================================================================
void WaveStrip::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    const float ceilDb = param (proc, id::ceiling);
    const float ceilG  = juce::Decibels::decibelsToGain (ceilDb);

    painel::secao (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, "WAVEFORM ", u8 ("· O QUE SAI E O QUE A BARRAGEM SEGURA, AO VIVO"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::amber, "CEILING", true);
        legendItem (g, x, theme::ice, "GAIN REDUCTION", true);
        legendItem (g, x, theme::coral, "SEGURADO", false);
        legendItem (g, x, theme::cream, "OUTPUT", false);
    }

    const float x0 = 12.0f, x1 = W - 12.0f, top = 26.0f, bot = H - 10.0f;
    const float mid = (top + bot) * 0.5f, half = (bot - top) * 0.5f - 2.0f;
    const juce::Rectangle<float> scr (x0, top, x1 - x0, bot - top);
    real::screen (g, scr, 3.0f);

    // mais antigo à esquerda, agora à direita
    const int   wpos = proc.engine.colWrite.load (std::memory_order_acquire);
    const float cw   = (x1 - x0) / (float) historyCols;
    const float grTop = top + 2.0f, grSpan = (bot - top) * 0.5f;   // a redução pendura do alto, até o meio
    float maxIn = 0.0f, maxGr = 0.0f;
    juce::Path grLine, grFill;
    grFill.startNewSubPath (x0, grTop);
    for (int k = 0; k < historyCols; ++k)
    {
        const auto  idx = (size_t) ((wpos + k) % historyCols);
        const float a  = proc.engine.colIn[idx].load (std::memory_order_relaxed);
        const float b  = proc.engine.colOut[idx].load (std::memory_order_relaxed);
        const float gr = proc.engine.colGr[idx].load (std::memory_order_relaxed);
        maxIn = juce::jmax (maxIn, a);
        maxGr = juce::jmax (maxGr, gr);
        const float ha = juce::jmin (1.0f, a / range) * half, hb = juce::jmin (1.0f, b / range) * half;
        const float x  = x0 + (float) k * cw;
        if (ha > hb + 0.5f)
        {
            g.setColour (theme::coral.withAlpha (0.9f));
            g.fillRect (x, mid - ha, cw + 0.4f, ha - hb);
            g.fillRect (x, mid + hb, cw + 0.4f, ha - hb);
        }
        g.setColour (theme::cream.withAlpha (0.78f));
        g.fillRect (x, mid - hb, cw + 0.4f, juce::jmax (1.0f, hb * 2.0f));

        const float gx = x + cw * 0.5f;
        const float gy = grTop + juce::jmin (1.0f, gr / grRange) * grSpan;
        if (k == 0) grLine.startNewSubPath (gx, gy); else grLine.lineTo (gx, gy);
        grFill.lineTo (gx, gy);
    }

    // tetos
    const float yPos = mid - juce::jmin (1.0f, ceilG / range) * half;
    const float yNeg = mid + juce::jmin (1.0f, ceilG / range) * half;
    g.setColour (theme::amber);
    g.fillRect (x0, yPos - 0.5f, x1 - x0, 1.2f);
    g.fillRect (x0, yNeg - 0.5f, x1 - x0, 1.2f);
    g.setFont (theme::mono (9.5f));
    g.drawText ("CEILING " + neg (tpText (ceilDb)),
                juce::Rectangle<float> (x0 + 6.0f, yPos - 13.0f, 140.0f, 11.0f), juce::Justification::centredLeft);

    // redução de ganho por cima (azul), com a régua em dB à direita
    grFill.lineTo (x1, grTop);
    grFill.closeSubPath();
    g.setColour (theme::ice.withAlpha (0.12f));
    g.fillPath (grFill);
    g.setColour (theme::ice);
    g.strokePath (grLine, juce::PathStrokeType (1.5f));
    g.setFont (theme::mono (8.5f));
    for (int step = 1; step <= 2; ++step)
    {
        const float db = grRange * 0.5f * (float) step;
        const float y  = grTop + grSpan * 0.5f * (float) step;
        g.setColour (theme::ice.withAlpha (0.25f));
        g.fillRect (x1 - 40.0f, y, 36.0f, 1.0f);
        g.setColour (theme::ice.withAlpha (0.7f));
        g.drawText (u8 ("−") + juce::String (juce::roundToInt (db)) + " dB",
                    juce::Rectangle<float> (x1 - 44.0f, y - 11.0f, 40.0f, 10.0f), juce::Justification::centredRight);
    }

    // escalas acompanham devagar (sem pular a cada frame)
    const float target = juce::jlimit (ceilG * 1.25f, ceilG * 8.0f, maxIn * 1.1f);
    range += (target - range) * 0.08f;
    if (maxGr > 11.0f)     grRange = 24.0f;
    else if (maxGr < 9.0f) grRange = 12.0f;

    real::glass (g, scr, 3.0f);
}

// =====================================================================
// LOUDNESS
// =====================================================================
juce::Rectangle<float> LoudnessView::selectorArea() const
{
    return { 0.0f, (float) getHeight() - 40.0f, (float) getWidth(), 40.0f };
}

void LoudnessView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    const auto& meter = proc.engine.loud;
    const float mom   = meter.momentary.load();
    const float st    = meter.shortTerm.load();
    const float integ = meter.integrated.load();
    const float secs  = meter.seconds.load();
    const int   ti    = proc.getTargetIndex();
    const float tgt   = targets[ti].lufs;
    const float none  = LoudnessMeter::silence + 1.0f;
    const bool  hasInt = integ > none;

    // ---- integrado e a distância até o alvo ----
    g.setColour (theme::dim);
    g.setFont (theme::sans (9.0f, true, 0.2f));
    g.drawText ("INTEGRATED", juce::Rectangle<float> (0.0f, 0.0f, W, 12.0f), juce::Justification::centredLeft);
    g.setColour (hasInt ? theme::cream : theme::dim);
    g.setFont (theme::mono (30.0f));
    g.drawText (lufsText (integ), juce::Rectangle<float> (0.0f, 14.0f, W, 36.0f), juce::Justification::centredLeft);
    {
        juce::String txt (u8 ("ESPERANDO SOM PARA MEDIR…"));
        juce::Colour c = theme::dim;
        if (hasInt)
        {
            const float d = integ - tgt;
            const auto  amount = juce::String (std::abs (d), 1).replaceCharacter ('.', ',');
            if (std::abs (d) < 0.5f) { txt = "ON TARGET";                                 c = theme::leaf; }
            else if (d > 0.0f)       { txt = "+" + amount + u8 (" LU ACIMA DO TARGET");   c = theme::amber; }
            else                     { txt = u8 ("−") + amount + u8 (" LU ABAIXO DO TARGET"); c = theme::cream; }
        }
        g.setColour (c);
        g.setFont (theme::sans (9.5f, true, 0.12f));
        g.drawText (txt, juce::Rectangle<float> (0.0f, 52.0f, W, 14.0f), juce::Justification::centredLeft);
    }

    // ---- barra: faixa = curto, traço = momentâneo, triângulo = integrado, verde = alvo ----
    const float lo = -32.0f, hi = -4.0f, barY = 84.0f, barH = 18.0f;
    auto xOf = [W, lo, hi] (float v) { return juce::jlimit (0.0f, W, (v - lo) / (hi - lo) * W); };

    const juce::Rectangle<float> barR (0.0f, barY, W, barH);
    real::screen (g, barR, 3.0f);
    if (st > none)
    {
        g.setColour (theme::cream.withAlpha (0.35f));
        g.fillRoundedRectangle (0.0f, barY, xOf (st), barH, 3.0f);
    }
    if (mom > none)
    {
        g.setColour (theme::cream);
        g.fillRect (xOf (mom) - 1.0f, barY, 2.0f, barH);
    }
    {
        const float tx = xOf (tgt);
        g.setColour (theme::leaf);
        g.fillRect (tx - 1.0f, barY - 4.0f, 2.0f, barH + 8.0f);
    }
    if (hasInt)
    {
        const float ix = xOf (integ);
        juce::Path tri;
        tri.addTriangle (ix - 4.5f, barY - 7.0f, ix + 4.5f, barY - 7.0f, ix, barY - 1.0f);
        g.setColour (theme::cream);
        g.fillPath (tri);
    }
    real::glass (g, barR, 3.0f);

    g.setFont (theme::mono (8.5f));
    for (int v = -30; v <= -6; v += 6)
    {
        const float x = xOf ((float) v);
        g.setColour (theme::segOff);
        g.fillRect (x, barY + barH, 1.0f, 4.0f);
        g.setColour (theme::dim);
        g.drawText (neg (juce::String (v)), juce::Rectangle<float> (x - 15.0f, barY + barH + 4.0f, 30.0f, 11.0f),
                    juce::Justification::centred);
    }

    // ---- leituras ----
    labelValue (g, W, 132.0f, "MOMENTARY (400 ms)", lufsText (mom), mom > none ? theme::cream : theme::dim);
    labelValue (g, W, 154.0f, "SHORT-TERM (3 s)", lufsText (st), st > none ? theme::cream : theme::dim);
    labelValue (g, W, 176.0f, "TEMPO MEDIDO", timeText (secs), theme::cream);

    g.setColour (theme::dim);
    g.setFont (theme::sans (10.5f));
    g.drawFittedText (u8 ("Medido na saída do limiter, com o K-weighting e os gates (−70 LUFS e −10 LU) da BS.1770."),
                      juce::Rectangle<int> (0, 198, (int) W, 44), juce::Justification::topLeft, 3);

    // ---- seletor do alvo ----
    const auto sel = selectorArea();
    g.setColour (theme::panel2);
    g.fillRoundedRectangle (sel, 3.0f);
    g.setColour (theme::leaf.withAlpha (0.8f));
    g.drawRoundedRectangle (sel.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (theme::dim);
    g.setFont (theme::sans (8.5f, true, 0.2f));
    g.drawText ("TARGET", juce::Rectangle<float> (sel.getX() + 10.0f, sel.getY() + 5.0f, 100.0f, 11.0f), juce::Justification::centredLeft);
    g.setColour (theme::cream);
    g.setFont (theme::mono (12.5f));
    g.drawText (neg (targetText (ti)), juce::Rectangle<float> (sel.getX() + 10.0f, sel.getY() + 18.0f, sel.getWidth() - 36.0f, 16.0f),
                juce::Justification::centredLeft, true);
    {
        const float cx = sel.getRight() - 15.0f, cy = sel.getCentreY();
        juce::Path caret;
        caret.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.0f);
        g.setColour (theme::leaf);
        g.fillPath (caret);
    }
}

void LoudnessView::mouseDown (const juce::MouseEvent& e)
{
    if (selectorArea().contains (e.position))
        showTargetMenu();
}

void LoudnessView::showTargetMenu()
{
    juce::PopupMenu menu;
    const int current = proc.getTargetIndex();
    for (int i = 0; i < numTargets; ++i)
        menu.addItem (i + 1, neg (targetText (i)), true, i == current);

    juce::Component::SafePointer<LoudnessView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->setTarget (result - 1);
                        });
}

void LoudnessView::setTarget (int index)
{
    if (auto* rp = proc.apvts.getParameter (id::target))
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 ((float) clampTarget (index)));
        rp->endChangeGesture();
    }
    repaint();
}

// =====================================================================
// ANÁLISE
// =====================================================================
void StatsView::update (float grDb, float peakIn, float peakOut, float dt)
{
    grNow = juce::jmax (grDb, grNow - 18.0f * dt);   // cai 18 dB/s
    if (grDb >= grMax) { grMax = grDb; holdLeft = 3.0f; }
    else if ((holdLeft -= dt) <= 0.0f) grMax = juce::jmax (grNow, grMax - 12.0f * dt);
    pkIn  = juce::jmax (peakIn,  pkIn  * std::exp (-dt * 3.0f));
    pkOut = juce::jmax (peakOut, pkOut * std::exp (-dt * 3.0f));
}

void StatsView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    auto dbBar = [] (float gain) { return juce::jmap (juce::Decibels::gainToDecibels (gain, -48.0f), -48.0f, 12.0f, 0.0f, 1.0f); };

    g.setColour (theme::dim);
    g.setFont (theme::sans (9.0f, true, 0.2f));
    g.drawText ("SEGURANDO AGORA", juce::Rectangle<float> (0.0f, 0.0f, W, 12.0f), juce::Justification::centredLeft);
    g.setColour (grNow > 0.05f ? theme::ice : theme::dim);
    g.setFont (theme::mono (30.0f));
    g.drawText (grText (grNow), juce::Rectangle<float> (0.0f, 14.0f, W, 36.0f), juce::Justification::centredLeft);
    thinBar (g, W, grNow / 12.0f, 56.0f, theme::ice);

    labelValue (g, W, 78.0f, "MAX (3 s)", grText (grMax), grMax > 0.05f ? theme::ice : theme::dim);
    thinBar (g, W, grMax / 12.0f, 94.0f, theme::ice.withAlpha (0.6f));

    labelValue (g, W, 116.0f, "PEAK DEPOIS DO ENCHER", dbfsText (pkIn), theme::cream);
    thinBar (g, W, dbBar (pkIn), 132.0f, theme::coral.withAlpha (0.7f));

    labelValue (g, W, 154.0f, "OUTPUT PEAK", dbfsText (pkOut), theme::cream);
    thinBar (g, W, dbBar (pkOut), 170.0f, theme::cream.withAlpha (0.7f));

    g.setColour (theme::dim);
    g.setFont (theme::sans (10.5f));
    g.drawFittedText (u8 ("A detecção é em true peak (4×): o CEILING vale também entre as amostras. "
                          "O som que sai é o próprio, só com ganho (não passa pelo filtro do oversampling)."),
                      juce::Rectangle<int> (0, 194, (int) W, 80), juce::Justification::topLeft, 5);
}

// =====================================================================
// PICOS (dBFS)
// =====================================================================
void PeakReadout::update (float in, float out, float dt)
{
    if (in >= inMax) { inMax = in; inHold = 3.0f; }
    else if ((inHold -= dt) <= 0.0f) inMax = juce::jmax (in, inMax * std::exp (-dt * 3.0f));
    if (out >= outMax) { outMax = out; outHold = 3.0f; }
    else if ((outHold -= dt) <= 0.0f) outMax = juce::jmax (out, outMax * std::exp (-dt * 3.0f));
}

void PeakReadout::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    auto block = [&g, W] (float y, const juce::String& label, float gain)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText (label, juce::Rectangle<float> (0.0f, y, W, 12.0f), juce::Justification::centredLeft);
        g.setColour (gain > 1.0f ? theme::coral : theme::cream);
        g.setFont (theme::mono (14.0f));
        g.drawText (dbfsText (gain), juce::Rectangle<float> (0.0f, y + 14.0f, W, 18.0f), juce::Justification::centredLeft);
    };
    block (0.0f,  "INPUT PEAK", inMax);
    block (44.0f, "OUTPUT PEAK", outMax);
}

// =====================================================================
// EDITOR
// =====================================================================
BarragemEditor::BarragemEditor (BarragemProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), loudness (p),
      // cor do knob = cor do que ele mexe no desenho
      fillMacro (p.apvts, id::drive,   "ENCHER",    true,  theme::coral),
      ceiling   (p.apvts, id::ceiling, "CEILING",   false, theme::amber),
      release   (p.apvts, id::release, "RELEASE",   false, theme::ice),
      look      (p.apvts, id::look,    "LOOKAHEAD", false, theme::ice),
      link      (p.apvts, id::link,    "LINK L/R",  false, theme::cream),
      autoKey   ("AUTO RELEASE", theme::ice),
      deltaKey  ("DELTA", theme::coral),
      resetKey  ("RESET INTEGRATED", theme::leaf)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, rackW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &loudness, &stats, &readout, &fillMacro, &ceiling, &release, &look, &link, &autoKey, &deltaKey,
             &resetKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    autoAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::autoRel, autoKey);
    deltaAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::delta, deltaKey);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

    // ZERAR INTEGRADO é um golpe (não fica ligado): recomeça a medida do integrado
    resetKey.setClickingTogglesState (false);
    resetKey.onClick = [this]
    {
        proc.resetIntegrated();
        resetKey.setToggleState (true, juce::dontSendNotification);
        resetFlash = 8;
    };

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

BarragemEditor::~BarragemEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BarragemEditor::showPresetMenu()
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

    juce::Component::SafePointer<BarragemEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void BarragemEditor::askNewPreset()
{
    juce::Component::SafePointer<BarragemEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void BarragemEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<BarragemEditor> safe (this);
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

void BarragemEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) rackW - openScale) > 0.005f)
        setScale (openScale);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    stats.update (proc.engine.grDb.exchange (0.0f), proc.engine.peakIn.exchange (0.0f), proc.engine.peakOut.exchange (0.0f), dt);

    const float mi = proc.meterIn.exchange (0.0f);
    const float mo = proc.meterOut.exchange (0.0f);
    readout.update (mi, mo, dt);
    levelIn  = juce::jmax (mi, levelIn * 0.85f);
    levelOut = juce::jmax (mo, levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    strip.repaint();
    loudness.repaint();
    stats.repaint();
    readout.repaint();

    if (resetFlash > 0 && --resetFlash == 0)
        resetKey.setToggleState (false, juce::dontSendNotification);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void BarragemEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) rackW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void BarragemEditor::resized()
{
    const float scale = (float) getWidth() / (float) rackW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void BarragemEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void BarragemEditor::drawCanvas (juce::Graphics& g)
{
    painel::rack (g, (float) rackW, (float) ui::baseH, (float) ear, tinta);
    g.addTransform (juce::AffineTransform::translation ((float) ear, 0.0f));   // daqui em diante, o layout da frente

    // ---- logo: água represada (azul) contra o muro (creme); a crista âmbar é o TETO ----
    g.setColour (theme::ice.withAlpha (0.85f));
    for (int k = 0; k < 4; ++k)
        g.fillRoundedRectangle (16.0f, 26.0f + (float) k * 6.0f, 15.0f, 2.2f, 1.0f);
    {
        juce::Path wall;
        wall.startNewSubPath (32.0f, 21.0f);
        wall.lineTo (37.0f, 21.0f);
        wall.lineTo (46.0f, 48.0f);
        wall.lineTo (32.0f, 48.0f);
        wall.closeSubPath();
        g.setColour (theme::cream);
        g.fillPath (wall);
        g.setColour (theme::amber);
        g.fillRect (30.0f, 19.0f, 9.0f, 2.0f);
    }

    const float brandEnd = drawBrand (g, "BARRAGEM", "LIMITER TRUE PEAK", "COM MEDIDOR LUFS");

    // ---- latência (o host compensa sozinho); se encostar na marca, desce para duas linhas ----
    {
        const double ms = 1000.0 * (double) proc.getLatencySamples() / juce::jmax (1.0, proc.rate.load());
        const auto   f  = theme::mono (11.5f);
        const juce::String label ("LATENCY "), value (juce::String (ms, 2).replaceCharacter ('.', ',') + " ms");
        const float right = 604.0f;
        juce::AttributedString s;
        s.append (label, f, theme::dim);
        s.append (value, f, theme::cream);
        juce::TextLayout tl;
        tl.createLayout (s, 400.0f);
        const float w = tl.getWidth();
        if (right - w < brandEnd + 14.0f)
        {
            g.setFont (f);
            g.setColour (theme::dim);
            g.drawText (label.trim(), juce::Rectangle<float> (right - 160.0f, 21.0f, 160.0f, 13.0f), juce::Justification::centredRight);
            g.setColour (theme::cream);
            g.drawText (value, juce::Rectangle<float> (right - 160.0f, 35.0f, 160.0f, 13.0f), juce::Justification::centredRight);
        }
        else
        {
            tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });
        }
    }

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "LOUDNESS ", u8 ("· LUFS"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "LIMITE");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), u8 ("ANÁLISE"));
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 250.0f, 140.0f, 70.0f }, "MACRO ENCHER",
              u8 ("Empurra o som contra o CEILING; o que passaria vira coral."));
}

void BarragemEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    loudness.setBounds (22, 246, 240, 312);

    fillMacro.setBounds (294, 246, 92, 92);
    autoKey.setBounds   (396, 324, 136, 40);
    ceiling.setBounds   (294, 376, 119, 92);
    release.setBounds   (413, 376, 119, 92);
    look.setBounds      (294, 468, 119, 92);
    link.setBounds      (413, 468, 119, 92);

    stats.setBounds (566, 248, 230, 300);

    deltaKey.setBounds (830, 246, 148, 40);
    resetKey.setBounds (830, 292, 148, 40);
    readout.setBounds  (830, 346, 148, 80);
    meterIn.setBounds  (857, 446, 94, 55);
    meterOut.setBounds (857, 503, 94, 55);

    for (auto* c : canvas.getChildren())   // a frente começa depois da orelha esquerda
        c->setTopLeftPosition (c->getX() + ear, c->getY());
}
