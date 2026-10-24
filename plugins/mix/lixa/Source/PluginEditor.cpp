#include "PluginEditor.h"

using namespace lixa;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004;

constexpr float specFloor = -96.0f;   // dB no pé do espectro
constexpr float minFreq = 20.0f, maxFreq = 20000.0f;

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

float param (LixaProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

juce::String dbfs (float gain)
{
    return gain < 1.0e-5f ? juce::String (u8 ("−∞ dB")) : dbText (juce::Decibels::gainToDecibels (gain)).replace ("-", u8 ("−"));
}

juce::String cutText (float cut)
{
    return cut > 0.05f ? u8 ("−") + juce::String (cut, 1).replaceCharacter ('.', ',') + " dB" : juce::String ("0,0 dB");
}

juce::String freqLabel (float f)
{
    return f >= 1000.0f ? juce::String (juce::roundToInt (f / 1000.0f)) + "k" : juce::String (juce::roundToInt (f));
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

// maior valor dos bins entre duas frequências (pelo menos um bin)
float peakBetween (const std::vector<float>& db, float f0, float f1, double sampleRate)
{
    const float perBin = (float) (sampleRate / (double) fftSize);
    const int   last   = (int) db.size() - 1;
    int b0 = juce::jlimit (1, last, (int) std::floor (f0 / perBin));
    int b1 = juce::jlimit (1, last, (int) std::ceil (f1 / perBin));
    if (b1 < b0) std::swap (b0, b1);
    float v = specFloor;
    for (int b = b0; b <= b1; ++b)
        v = juce::jmax (v, db[(size_t) b]);
    return v;
}
} // namespace

// =====================================================================
// ESPECTRO
// =====================================================================
SpectrumView::SpectrumView (LixaProcessor& p) : proc (p)
{
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
    fftData.assign ((size_t) (fftSize * 2), 0.0f);
    inDb.assign ((size_t) (fftSize / 2 + 1), specFloor);
    outDb.assign ((size_t) (fftSize / 2 + 1), specFloor);
}

void SpectrumView::analyse (const std::array<std::atomic<float>, specRing>& ring, int wpos, std::vector<float>& dbOut)
{
    for (int i = 0; i < fftSize; ++i)
    {
        const int idx = (wpos - fftSize + i) & (specRing - 1);
        fftData[(size_t) i] = ring[(size_t) idx].load (std::memory_order_relaxed) * window[(size_t) i];
    }
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // seno de 0 dBFS com janela Hann dá N/4 no bin; sobe rápido, desce devagar
    const float norm = 4.0f / (float) fftSize;
    for (size_t b = 0; b < dbOut.size(); ++b)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[b] * norm, specFloor);
        dbOut[b] = db > dbOut[b] ? db : dbOut[b] + (db - dbOut[b]) * 0.25f;
    }
}

void SpectrumView::refresh()
{
    const int wpos = proc.engine.specWrite.load (std::memory_order_acquire);
    analyse (proc.engine.specIn, wpos, inDb);
    analyse (proc.engine.specOut, wpos, outDb);
    repaint();
}

juce::Rectangle<float> SpectrumView::plotArea() const
{
    return { 12.0f, 26.0f, (float) getWidth() - 24.0f, (float) getHeight() - 26.0f - 22.0f };
}

float SpectrumView::xForFreq (float f) const
{
    const auto pa = plotArea();
    const float t = std::log (juce::jlimit (minFreq, maxFreq, f) / minFreq) / std::log (maxFreq / minFreq);
    return pa.getX() + t * pa.getWidth();
}

float SpectrumView::freqForX (float x) const
{
    const auto pa = plotArea();
    const float t = juce::jlimit (0.0f, 1.0f, (x - pa.getX()) / pa.getWidth());
    return minFreq * std::pow (maxFreq / minFreq, t);
}

float SpectrumView::yForDb (float db) const
{
    const auto pa = plotArea();
    const float t = juce::jlimit (0.0f, 1.0f, db / specFloor);   // 0 dB em cima, specFloor embaixo
    return pa.getY() + t * pa.getHeight();
}

SpectrumView::Grab SpectrumView::grabAt (float x) const
{
    const float xl = xForFreq (param (proc, id::lo)), xh = xForFreq (param (proc, id::hi));
    if (std::abs (x - xl) <= 7.0f) return Grab::lo;
    if (std::abs (x - xh) <= 7.0f) return Grab::hi;
    if (x > xl && x < xh) return Grab::band;
    return Grab::none;
}

void SpectrumView::setFreq (const char* pid, float hz)
{
    if (auto* rp = proc.apvts.getParameter (pid))
        rp->setValueNotifyingHost (rp->convertTo0to1 (hz));
}

void SpectrumView::gesture (bool begin)
{
    for (auto* pid : { id::lo, id::hi })
        if (auto* rp = proc.apvts.getParameter (pid))
        {
            if (begin) rp->beginChangeGesture();
            else       rp->endChangeGesture();
        }
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const auto g = plotArea().contains (e.position) ? grabAt (e.position.x) : Grab::none;
    if (g == hover) return;
    hover = g;
    setMouseCursor (g == Grab::lo || g == Grab::hi ? juce::MouseCursor::LeftRightResizeCursor
                    : g == Grab::band              ? juce::MouseCursor::DraggingHandCursor
                                                   : juce::MouseCursor::NormalCursor);
    repaint();
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    if (grab != Grab::none) return;
    hover = Grab::none;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    grab = plotArea().contains (e.position) ? grabAt (e.position.x) : Grab::none;
    if (grab == Grab::none) return;
    grabLo = param (proc, id::lo);
    grabHi = param (proc, id::hi);
    grabF  = freqForX (e.position.x);
    gesture (true);
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (grab == Grab::none) return;
    const float f = freqForX (e.position.x);
    if (grab == Grab::lo)
    {
        setFreq (id::lo, juce::jlimit (minLo, juce::jmax (minLo, juce::jmin (maxLo, grabHi / minRatio)), f));
    }
    else if (grab == Grab::hi)
    {
        setFreq (id::hi, juce::jlimit (juce::jmin (maxHi, juce::jmax (minHi, grabLo * minRatio)), maxHi, f));
    }
    else
    {
        // a faixa inteira anda junto (mesma largura em oitavas), sem passar dos limites
        const float lowest  = juce::jmax (minLo / grabLo, minHi / grabHi);
        const float highest = juce::jmin (maxLo / grabLo, maxHi / grabHi);
        const float ratio   = juce::jlimit (juce::jmin (lowest, highest), juce::jmax (lowest, highest), f / grabF);
        setFreq (id::lo, grabLo * ratio);
        setFreq (id::hi, grabHi * ratio);
    }
    repaint();
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    if (grab != Grab::none) gesture (false);
    grab = Grab::none;
}

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    // duplo clique na faixa: volta para 4–10 kHz
    if (! plotArea().contains (e.position) || grabAt (e.position.x) == Grab::none) return;
    gesture (true);
    for (auto* pid : { id::lo, id::hi })
        if (auto* rp = proc.apvts.getParameter (pid))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    gesture (false);
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto  r  = getLocalBounds().toFloat();
    const float W  = r.getWidth();
    const auto  pa = plotArea();
    const double fs = juce::jmax (8000.0, proc.rate.load());

    real::card (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, "SPECTRUM ", u8 ("· ARRASTE AS BORDAS DA FAIXA DE DETECÇÃO"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::amber, "FAIXA", false);
        legendItem (g, x, theme::coral, "TIRADO", false);
        legendItem (g, x, theme::cream, "OUTPUT", true);
        legendItem (g, x, theme::dim, "INPUT", false);
    }

    real::screen (g, pa, 3.0f);

    // grade
    g.setFont (theme::mono (9.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (theme::segOff.withAlpha (0.6f));
        g.fillRect (x, pa.getY(), 1.0f, pa.getHeight());
        g.setColour (theme::dim);
        g.drawText (freqLabel (f), juce::Rectangle<float> (x - 20.0f, pa.getBottom() + 4.0f, 40.0f, 12.0f), juce::Justification::centred);
    }
    for (float db : { -24.0f, -48.0f, -72.0f })
    {
        const float y = yForDb (db);
        g.setColour (theme::segOff.withAlpha (0.6f));
        g.fillRect (pa.getX(), y, pa.getWidth(), 1.0f);
        g.setColour (theme::dim);
        g.drawText (juce::String (juce::roundToInt (db)).replace ("-", u8 ("−")),
                    juce::Rectangle<float> (pa.getX() + 4.0f, y - 12.0f, 40.0f, 11.0f), juce::Justification::centredLeft);
    }

    // faixa de detecção
    const float lo = param (proc, id::lo), hi = param (proc, id::hi);
    const float xl = xForFreq (lo), xh = xForFreq (hi);
    const bool  bandHot = hover == Grab::band || grab == Grab::band;
    g.setColour (theme::amber.withAlpha (bandHot ? 0.14f : 0.08f));
    g.fillRect (xl, pa.getY(), xh - xl, pa.getHeight());

    // espectro: entrada (cinza), o que foi tirado (coral) e a saída (creme)
    std::vector<float> xs, yIn, yOut;
    const float step = 2.0f;
    for (float x = pa.getX(); x <= pa.getRight(); x += step)
    {
        const float f0 = freqForX (x), f1 = freqForX (x + step);
        xs.push_back (x);
        yIn.push_back (yForDb (peakBetween (inDb, f0, f1, fs)));
        yOut.push_back (yForDb (peakBetween (outDb, f0, f1, fs)));
    }
    if (xs.size() > 1)
    {
        juce::Path inFill, cutFill, outLine;
        inFill.startNewSubPath (xs.front(), pa.getBottom());
        for (size_t k = 0; k < xs.size(); ++k)
        {
            inFill.lineTo (xs[k], yIn[k]);
            if (k == 0) outLine.startNewSubPath (xs[k], yOut[k]); else outLine.lineTo (xs[k], yOut[k]);
            if (k == 0) cutFill.startNewSubPath (xs[k], yIn[k]); else cutFill.lineTo (xs[k], yIn[k]);
        }
        inFill.lineTo (xs.back(), pa.getBottom());
        inFill.closeSubPath();
        for (size_t k = xs.size(); k-- > 0;)
            cutFill.lineTo (xs[k], juce::jmax (yOut[k], yIn[k]));   // a saída nunca acima da entrada
        cutFill.closeSubPath();

        g.setColour (theme::dim.withAlpha (0.22f));
        g.fillPath (inFill);
        g.setColour (theme::coral.withAlpha (0.85f));
        g.fillPath (cutFill);
        g.setColour (theme::cream.withAlpha (0.9f));
        g.strokePath (outLine, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // bordas e alças da faixa
    auto edge = [&] (float x, bool hot)
    {
        g.setColour (theme::amber.withAlpha (hot ? 1.0f : 0.75f));
        g.fillRect (x - (hot ? 1.0f : 0.5f), pa.getY(), hot ? 2.0f : 1.2f, pa.getHeight());
        const juce::Rectangle<float> handle (x - 4.0f, pa.getCentreY() - 15.0f, 8.0f, 30.0f);
        g.setColour (hot ? theme::amber : theme::panel2);
        g.fillRoundedRectangle (handle, 3.0f);
        g.setColour (theme::amber);
        g.drawRoundedRectangle (handle, 3.0f, 1.2f);
    };
    edge (xl, hover == Grab::lo || grab == Grab::lo || bandHot);
    edge (xh, hover == Grab::hi || grab == Grab::hi || bandHot);

    g.setFont (theme::mono (10.0f));
    g.setColour (theme::amber);
    g.drawText (hzText (lo), juce::Rectangle<float> (xl - 76.0f, pa.getY() + 4.0f, 70.0f, 12.0f), juce::Justification::centredRight);
    g.drawText (hzText (hi), juce::Rectangle<float> (xh + 6.0f, pa.getY() + 4.0f, 70.0f, 12.0f), juce::Justification::centredLeft);

    real::glass (g, pa, 3.0f);
}

// =====================================================================
// REDUÇÃO
// =====================================================================
void ReductionView::update (float grDb, float levelDb, float dt)
{
    grNow = juce::jmax (grDb, grNow - 24.0f * dt);   // cai 24 dB/s
    if (grDb >= grMax) { grMax = grDb; holdLeft = 3.0f; }
    else if ((holdLeft -= dt) <= 0.0f) grMax = juce::jmax (grNow, grMax - 12.0f * dt);
    lvlNow = juce::jmax (levelDb, lvlNow - 30.0f * dt);
}

void ReductionView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    const float thr   = param (proc, id::thresh);
    const float range = param (proc, id::range);

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

    label ("GAIN REDUCTION", 0.0f);
    g.setColour (grNow > 0.05f ? theme::coral : theme::dim);
    g.setFont (theme::mono (30.0f));
    g.drawText (cutText (grNow), juce::Rectangle<float> (0.0f, 14.0f, W, 36.0f), juce::Justification::centredLeft);

    label ("MAX (3 s)", 56.0f);
    value (cutText (grMax), 56.0f, grMax > 0.05f ? theme::coral : theme::dim);

    // nível da faixa contra o THRESHOLD (marca azul)
    label (u8 ("NÍVEL NA FAIXA"), 76.0f);
    value (lvlNow > -119.0f ? dbText (lvlNow).replace ("-", u8 ("−")) : juce::String (u8 ("−∞ dB")), 76.0f,
           lvlNow > thr ? theme::cream : theme::dim);
    {
        const float y = 92.0f;
        const float frac = juce::jlimit (0.0f, 1.0f, juce::jmap (lvlNow, -60.0f, 0.0f, 0.0f, 1.0f));
        const float tx   = W * juce::jlimit (0.0f, 1.0f, juce::jmap (thr, -60.0f, 0.0f, 0.0f, 1.0f));
        g.setColour (theme::segOff);
        g.fillRoundedRectangle (0.0f, y, W, 4.0f, 2.0f);
        g.setColour (theme::cream.withAlpha (0.7f));
        g.fillRoundedRectangle (0.0f, y, W * frac, 4.0f, 2.0f);
        g.setColour (theme::ice);
        g.fillRect (tx - 1.0f, y - 3.0f, 2.0f, 10.0f);
    }

    // histórico: a redução desce do topo (coral); a linha tracejada é o RANGE
    const juce::Rectangle<float> area (0.0f, 112.0f, W, H - 112.0f - 16.0f);
    const auto screenR = area.reduced (1.0f, 0.0f);   // 1 px para dentro nas laterais: o rebaixo cabe no componente
    real::screen (g, screenR, 3.0f);
    const float scaleDb = juce::jmax (6.0f, range * 1.15f);
    auto yFor = [&area, scaleDb] (float db) { return area.getY() + juce::jlimit (0.0f, 1.0f, db / scaleDb) * area.getHeight(); };

    g.setFont (theme::mono (9.0f));
    for (float db : { 3.0f, 6.0f, 12.0f, 18.0f })
    {
        if (db >= scaleDb) continue;
        const float y = yFor (db);
        g.setColour (theme::segOff.withAlpha (0.7f));
        g.fillRect (area.getX(), y, area.getWidth(), 1.0f);
        g.setColour (theme::dim);
        g.drawText (u8 ("−") + juce::String (juce::roundToInt (db)), juce::Rectangle<float> (area.getRight() - 30.0f, y - 11.0f, 26.0f, 10.0f),
                    juce::Justification::centredRight);
    }

    const int   wpos = proc.engine.colWrite.load (std::memory_order_acquire);
    const float cw   = area.getWidth() / (float) historyCols;
    g.setColour (theme::coral.withAlpha (0.85f));
    for (int k = 0; k < historyCols; ++k)
    {
        const auto  idx = (size_t) ((wpos + k) % historyCols);
        const float gr  = proc.engine.colGr[idx].load (std::memory_order_relaxed);
        if (gr <= 0.02f) continue;
        g.fillRect (area.getX() + (float) k * cw, area.getY(), cw + 0.4f, yFor (gr) - area.getY());
    }

    {
        juce::Path maxLine, dashed;
        const float y = yFor (range);
        maxLine.startNewSubPath (area.getX(), y);
        maxLine.lineTo (area.getRight(), y);
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, maxLine, dashes, 2);
        g.setColour (theme::coral.withAlpha (0.5f));
        g.fillPath (dashed);
    }

    real::glass (g, screenR, 3.0f);

    g.setColour (theme::dim);
    g.setFont (theme::sans (9.0f, false, 0.1f));
    g.drawText (u8 ("ÚLTIMOS ~4 s"), juce::Rectangle<float> (0.0f, H - 13.0f, W, 12.0f), juce::Justification::centredLeft);
    g.drawText ("MAX " + cutText (range), juce::Rectangle<float> (0.0f, H - 13.0f, W, 12.0f), juce::Justification::centredRight);
}

// =====================================================================
// PICOS (dBFS)
// =====================================================================
void PeakReadout::set (float in, float out)
{
    if (std::abs (in - pkIn) < 1.0e-4f && std::abs (out - pkOut) < 1.0e-4f) return;
    pkIn = in;
    pkOut = out;
    repaint();
}

void PeakReadout::paint (juce::Graphics& g)
{
    const int W = getWidth();
    auto row = [&g, W] (const juce::String& name, float gain, int y)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.15f));
        g.drawText (name, 0, y, W, 12, juce::Justification::centredLeft);
        g.setColour (gain > 0.999f ? theme::coral : theme::cream);
        g.setFont (theme::mono (11.0f));
        g.drawText (dbfs (gain), 0, y + 14, W, 14, juce::Justification::centredLeft);
    };
    row ("PEAK IN", pkIn, 4);
    row ("PEAK OUT", pkOut, 40);
    g.setColour (theme::dim);
    g.setFont (theme::sans (8.5f, false, 0.1f));
    g.drawText ("dBFS", 0, 84, W, 12, juce::Justification::centredLeft);
}

// =====================================================================
// EDITOR
// =====================================================================
LixaEditor::LixaEditor (LixaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p), reduction (p),
      // cor do knob = cor do que ele mexe no desenho
      rangeMacro  (p.apvts, id::range,   "RANGE",          true,  theme::coral),
      loKnob      (p.apvts, id::lo,      "LOW FREQ",        false, theme::amber),
      hiKnob      (p.apvts, id::hi,      "HIGH FREQ",       false, theme::amber),
      threshKnob  (p.apvts, id::thresh,  "THRESHOLD",       false, theme::ice),
      linkKnob    (p.apvts, id::link,    "LINK L/R",        false, theme::cream),
      attackKnob  (p.apvts, id::attack,  "ATTACK",          false, theme::cream),
      releaseKnob (p.apvts, id::release, "RELEASE",         false, theme::cream),
      output      (p.apvts, id::output,  "GAIN",            false, theme::cream),
      wideKey     ("WIDEBAND", theme::amber),
      deltaKey    (u8 ("SÓ O CORTE"), theme::coral)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &spectrum, &reduction, &readout, &rangeMacro, &loKnob, &hiKnob, &threshKnob, &linkKnob, &attackKnob,
             &releaseKnob, &output, &wideKey, &deltaKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName,
             &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    wideAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::wide, wideKey);
    deltaAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::delta, deltaKey);
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

LixaEditor::~LixaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LixaEditor::showPresetMenu()
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

    juce::Component::SafePointer<LixaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void LixaEditor::askNewPreset()
{
    juce::Component::SafePointer<LixaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void LixaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<LixaEditor> safe (this);
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

void LixaEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) ui::baseW - openScale) > 0.005f)
        setScale (openScale);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    spectrum.refresh();
    reduction.update (proc.engine.grDb.exchange (0.0f), proc.engine.bandDb.exchange (-120.0f), dt);
    reduction.repaint();

    const float inNow = proc.meterIn.exchange (0.0f), outNow = proc.meterOut.exchange (0.0f);
    levelIn  = juce::jmax (inNow,  levelIn * 0.85f);
    levelOut = juce::jmax (outNow, levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    // número do pico: segura e cai devagar, para dar tempo de ler
    const float fall = std::exp (-dt * 1.5f);
    holdIn  = juce::jmax (inNow,  holdIn * fall);
    holdOut = juce::jmax (outNow, holdOut * fall);
    readout.set (holdIn, holdOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void LixaEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void LixaEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void LixaEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void LixaEditor::drawCanvas (juce::Graphics& g)
{
    real::chassis (g, (float) ui::baseW, (float) ui::baseH);

    // ---- logo: folha de lixa (grãos) ----
    {
        const juce::Rectangle<float> sheet (17.0f, 23.0f, 28.0f, 22.0f);
        g.setColour (theme::amber);
        g.fillRoundedRectangle (sheet, 2.5f);
        g.setColour (theme::bg);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
            {
                const float gx = sheet.getX() + 4.5f + (float) col * 6.0f + (row % 2 == 1 ? 3.0f : 0.0f);
                const float gy = sheet.getY() + 4.5f + (float) row * 6.0f;
                if (gx < sheet.getRight() - 3.0f)
                    g.fillEllipse (gx - 1.3f, gy - 1.3f, 2.6f, 2.6f);
            }
        g.setColour (theme::coral);
        g.fillRect (sheet.getX(), sheet.getBottom() - 2.0f, sheet.getWidth(), 2.0f);   // a borda que tira
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("LIXA");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 220.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 ("DE-ESSER"), tag2 (u8 ("FAIXA AJUSTÁVEL"));
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

    // ---- latência (zero: filtros IIR, sem olhar à frente) ----
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
    drawCard (g, { 12.0f, 266.0f, 262.0f, 302.0f });
    drawCard (g, { 284.0f, 266.0f, 258.0f, 302.0f });
    drawCard (g, { 552.0f, 266.0f, 258.0f, 302.0f });
    drawCard (g, { 820.0f, 266.0f, 168.0f, 302.0f });

    drawTitle (g, 22.0f,  276.0f, u8 ("01 · "), u8 ("FAIXA DE DETECÇÃO"));
    drawTitle (g, 294.0f, 276.0f, u8 ("02 · "), "GAIN REDUCTION");
    drawTitle (g, 562.0f, 276.0f, u8 ("03 · "), u8 ("ANÁLISE"));
    drawTitle (g, 830.0f, 276.0f, u8 ("04 · "), "OUTPUT");

    drawDesc (g, { 22.0f, 448.0f, 242.0f, 112.0f }, "WIDEBAND",
              u8 ("Desligado: só a faixa abaixa (corpo e grave não mexem). Ligado: o som inteiro abaixa junto "
                  "quando o S passa do THRESHOLD, mais natural em vozes expostas."));
    drawDesc (g, { 396.0f, 296.0f, 140.0f, 92.0f }, "MACRO RANGE",
              u8 ("O máximo que o S pode baixar. O que sai vira coral no espectro."));
}

void LixaEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    spectrum.setBounds (12, 66, 976, 190);

    loKnob.setBounds  (22, 296, 119, 92);
    hiKnob.setBounds  (141, 296, 119, 92);
    wideKey.setBounds (22, 398, 238, 40);

    rangeMacro.setBounds  (294, 294, 92, 92);
    threshKnob.setBounds  (294, 388, 119, 92);
    linkKnob.setBounds    (413, 388, 119, 92);
    attackKnob.setBounds  (294, 474, 119, 92);
    releaseKnob.setBounds (413, 474, 119, 92);

    reduction.setBounds (566, 296, 230, 262);

    deltaKey.setBounds (830, 294, 148, 40);
    output.setBounds   (830, 344, 148, 92);
    meterIn.setBounds  (834, 446, 24, 112);
    meterOut.setBounds (862, 446, 24, 112);
    readout.setBounds  (894, 450, 88, 100);
}
