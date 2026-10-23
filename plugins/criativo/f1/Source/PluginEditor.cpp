#include "PluginEditor.h"
#include <complex>

using namespace f1;

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
    g.setColour (theme::amber);
    g.setFont (theme::sans (10.5f, true, 0.06f));
    const auto brandFont = theme::sans (10.5f, true, 0.06f);
    g.drawText ("InsideStudioSP", juce::Rectangle<float> (brandX, 34.0f, 140.0f, 13.0f), juce::Justification::centredLeft);
    return brandX + textWidth (brandFont, "InsideStudioSP");   // onde a marca termina
}

// BPM: SYNC seguindo o host; host parado = espera o play; sem régua do host = LIVRE
// (nome comprido: se encostar na marca, o estado desce para uma segunda linha)
void drawSync (juce::Graphics& g, float bpm, bool syn, bool wait, float brandEnd)
{
    const auto f = theme::mono (11.5f);
    const juce::String state = syn ? "SYNC" : wait ? "AGUARDANDO PLAY" : "LIVRE";
    const juce::Colour stateColour = syn ? theme::cream : theme::dim;
    const float right = 604.0f;

    juce::AttributedString s;
    s.append (juce::String (bpm, 1), f, theme::cream);
    s.append (juce::String::fromUTF8 (" BPM · "), f, theme::dim);
    s.append (state, f, stateColour);
    juce::TextLayout tl;
    tl.createLayout (s, 400.0f);
    float w = tl.getWidth(), ledY = 30.5f;

    if (right - w - 13.0f < brandEnd + 14.0f)
    {
        juce::AttributedString a, b;
        a.append (juce::String (bpm, 1), f, theme::cream);
        a.append (" BPM", f, theme::dim);
        b.append (state, f, stateColour);
        juce::TextLayout ta, tb;
        ta.createLayout (a, 400.0f);
        tb.createLayout (b, 400.0f);
        ta.draw (g, { right - ta.getWidth(), 27.0f - ta.getHeight() * 0.5f, ta.getWidth() + 1.0f, ta.getHeight() });
        tb.draw (g, { right - tb.getWidth(), 41.0f - tb.getHeight() * 0.5f, tb.getWidth() + 1.0f, tb.getHeight() });
        w = ta.getWidth();
        ledY = 23.5f;
    }
    else
        tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });

    const juce::Rectangle<float> led (right - w - 13.0f, ledY, 7.0f, 7.0f);
    if (syn)
    {
        g.setColour (theme::amber.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (syn ? theme::amber : theme::off);
    g.fillEllipse (led);
}

// eixo de frequência em log, 20 Hz–20 kHz
float freqX (float hz, float x0, float x1)
{
    return x0 + (std::log10 (juce::jmax (20.0f, hz)) - std::log10 (20.0f)) / 3.0f * (x1 - x0);
}

float param (F1Processor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

float baseCutoff (F1Processor& p)
{
    return param (p, id::cutoff) * (juce::roundToInt (param (p, id::range)) == 1 ? 0.25f : 1.0f);
}

// resposta do modelo linear: 1 / ((1 + s)^polos + k), com o ganho de compensação
float ladderMag (float hz, float fc, float k, bool four)
{
    const std::complex<float> s (0.0f, hz / fc);
    const auto one = std::complex<float> (1.0f, 0.0f) + s;
    const auto den = (four ? one * one * one * one : one * one) + k;
    return makeup (k, four) / std::abs (den);
}
} // namespace

// =====================================================================
// RESPOSTA
// =====================================================================
void ResponseStrip::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    const bool  four = juce::roundToInt (param (proc, id::poles)) == 1;
    const float k    = feedbackK (param (proc, id::resonance) * 0.01f, four);
    const float top  = maxHz (proc.engine.rate.load());
    const float base = juce::jlimit (minHz, top, baseCutoff (proc));
    const float now  = juce::jlimit (minHz, top, proc.engine.cutNow.load());
    const float envA = param (proc, id::envAmt) * 0.01f;

    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    drawTitle (g, 12.0f, 7.0f, "RESPOSTA ", four ? u8 ("· 4 POLES · 24 dB/OCT") : u8 ("· 2 POLES · 12 dB/OCT"));
    {
        juce::AttributedString s;
        s.append ("CUTOFF ", theme::mono (10.0f), theme::dim);
        s.append (hzText (base), theme::mono (10.0f), theme::cream);
        s.append (u8 (" → "), theme::mono (10.0f), theme::dim);
        s.append (hzText (now), theme::mono (10.0f), theme::leaf);
        s.setJustification (juce::Justification::centredRight);
        s.draw (g, { W - 400.0f, 6.0f, 388.0f, 14.0f });
    }

    const float x0 = 12.0f, x1 = W - 12.0f, y0 = 26.0f, y1 = H - 20.0f;
    g.setColour (theme::dark);
    g.fillRoundedRectangle (x0, y0, x1 - x0, y1 - y0, 3.0f);

    // grade: frequência e 0 dB
    g.setFont (theme::mono (9.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = freqX (f, x0, x1);
        g.setColour (theme::ledOff);
        g.fillRect (x, y0, 1.0f, y1 - y0);
        if (f == 100.0f || f == 1000.0f || f == 10000.0f)
        {
            g.setColour (theme::dim);
            g.drawText (f >= 1000.0f ? juce::String (juce::roundToInt (f / 1000.0f)) + "k" : juce::String (juce::roundToInt (f)),
                        juce::Rectangle<float> (x - 20.0f, y1 + 3.0f, 40.0f, 12.0f), juce::Justification::centred);
        }
    }
    auto yOf = [y0, y1] (float mag)
    {
        const float db = juce::jlimit (-36.0f, 18.0f, juce::Decibels::gainToDecibels (mag, -60.0f));
        return juce::jmap (db, 18.0f, -36.0f, y0 + 4.0f, y1);
    };
    g.setColour (theme::ledOff.brighter (0.2f));
    g.fillRect (x0, yOf (1.0f), x1 - x0, 1.0f);

    // até onde o envelope leva o corte
    if (std::abs (envA) > 0.01f)
    {
        const float to = juce::jlimit (minHz, top, base * std::exp2 (envA * 4.0f));
        const float xa = freqX (juce::jmin (base, to), x0, x1), xb = freqX (juce::jmax (base, to), x0, x1);
        g.setGradientFill (juce::ColourGradient (theme::leaf.withAlpha (envA > 0.0f ? 0.0f : 0.2f), xa, 0.0f,
                                                 theme::leaf.withAlpha (envA > 0.0f ? 0.2f : 0.0f), xb, 0.0f, false));
        g.fillRect (xa, y0, xb - xa, y1 - y0);
    }

    auto curve = [&] (float fc)
    {
        juce::Path p;
        bool first = true;
        for (float x = x0; x <= x1; x += 2.0f)
        {
            const float hz = 20.0f * std::pow (1000.0f, (x - x0) / (x1 - x0));
            const float y  = yOf (ladderMag (hz, fc, k, four));
            if (first) p.startNewSubPath (x, y); else p.lineTo (x, y);
            first = false;
        }
        return p;
    };

    g.setColour (theme::dim.withAlpha (0.55f));
    g.strokePath (curve (base), juce::PathStrokeType (1.0f));

    auto live = curve (now);
    auto fill = live;
    fill.lineTo (x1, y1);
    fill.lineTo (x0, y1);
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (0.28f), 0.0f, y0, theme::amber.withAlpha (0.0f), 0.0f, y1, false));
    g.fillPath (fill);
    g.setColour (theme::cream);
    g.strokePath (live, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ponto do corte de agora e o quanto o envelope/LFO andou
    const float px = freqX (now, x0, x1), py = yOf (ladderMag (now, now, k, four));
    g.setColour (theme::coral.withAlpha (0.3f));
    g.fillEllipse (px - 7.0f, py - 7.0f, 14.0f, 14.0f);
    g.setColour (theme::coral);
    g.fillEllipse (px - 4.0f, py - 4.0f, 8.0f, 8.0f);
    const float bx = freqX (base, x0, x1);
    g.setColour (theme::leaf);
    g.fillRect (juce::jmin (bx, px), y1 - 3.0f, std::abs (px - bx), 3.0f);

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// =====================================================================
// ESCADA
// =====================================================================
void LadderView::tick (float lv)
{
    t += 1.0f / 30.0f;
    level = juce::jmax (lv, level * 0.88f);
    repaint();
}

void LadderView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth();
    const float res   = param (proc, id::resonance) * 0.01f;
    const float fc    = proc.engine.cutNow.load();
    const float env   = proc.engine.envNow.load();
    const float smoke = proc.engine.smokeNow.load();
    const float lv    = juce::jlimit (0.0f, 1.0f, juce::Decibels::gainToDecibels (level, -60.0f) / 48.0f + 1.0f);

    g.setColour (theme::dark);
    g.fillRoundedRectangle (0.0f, 0.0f, S, S, 4.0f);

    const float L = 72.0f, R = 168.0f;
    const float ys[4] = { 46.0f, 92.0f, 138.0f, 184.0f };

    // trilhos
    g.setColour (theme::off);
    g.fillRect (L - 1.5f, 24.0f, 3.0f, 186.0f);
    g.fillRect (R - 1.5f, 24.0f, 3.0f, 186.0f);

    // degraus: acendem com o som; cada um mais apagado (o filtro vai tirando)
    const float smoothness = juce::jlimit (0.0f, 1.0f, 1.0f - std::log2 (juce::jmax (20.0f, fc) / 20.0f) / 10.0f);
    for (int i = 0; i < 4; ++i)
    {
        const float lit = (1.0f - (float) i * 0.17f) * (0.25f + 0.75f * lv);
        g.setColour (theme::amber.withAlpha (0.18f * lit));
        g.fillRoundedRectangle (L, ys[i] - 7.0f, R - L, 14.0f, 7.0f);
        g.setColour (theme::amber.withAlpha (0.35f + 0.65f * lit));
        g.fillRoundedRectangle (L, ys[i] - 3.0f, R - L, 6.0f, 3.0f);
        for (float xx : { L, R })
        {
            g.setColour (theme::body);
            g.fillEllipse (xx - 6.0f, ys[i] - 6.0f, 12.0f, 12.0f);
            g.setColour (theme::dark);
            g.drawEllipse (xx - 6.0f, ys[i] - 6.0f, 12.0f, 12.0f, 2.0f);
        }
        g.setColour (theme::dim);
        g.setFont (theme::mono (9.0f));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (R + 8.0f, ys[i] - 6.0f, 12.0f, 12.0f), juce::Justification::centredLeft);

        // a onda descendo a escada: quadrada no topo, cada vez mais redonda
        const float m = juce::jlimit (0.0f, 1.0f, smoothness * (float) (i + 1) / 4.0f + (float) i * 0.12f);
        juce::Path w;
        for (int x = 0; x <= 70; ++x)
        {
            const float ph = (float) x / 70.0f * juce::MathConstants<float>::twoPi * 2.0f + t * 3.0f;
            const float sq = std::sin (ph) >= 0.0f ? 1.0f : -1.0f;
            const float v  = sq * (1.0f - m) + std::sin (ph) * m;
            const float yy = ys[i] - 15.0f + v * 6.0f * (0.35f + 0.65f * lv) * (1.0f - (float) i * 0.12f);
            if (x == 0) w.startNewSubPath (85.0f + (float) x, yy); else w.lineTo (85.0f + (float) x, yy);
        }
        g.setColour (theme::cream.withAlpha (0.9f - (float) i * 0.15f));
        g.strokePath (w, juce::PathStrokeType (1.4f));
    }

    // laço da ressonância: a saída volta para a entrada
    {
        juce::Path loop;
        loop.startNewSubPath (R, 210.0f);
        loop.lineTo (R + 34.0f, 210.0f);
        loop.lineTo (R + 34.0f, 24.0f);
        loop.lineTo (L, 24.0f);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (2.4f).createDashedStroke (dashed, loop, dashes, 2);
        g.setColour (theme::coral.withAlpha ((0.08f + 0.22f * res) * (0.7f + 0.3f * std::sin (t * 6.0f))));
        g.strokePath (loop, juce::PathStrokeType (6.0f));
        g.setColour (theme::coral.withAlpha (0.25f + 0.75f * res));
        g.fillPath (dashed);

        juce::GlyphArrangement ga;
        ga.addLineOfText (theme::sans (8.0f, true, 0.15f), "RESONANCE", 0.0f, 0.0f);
        juce::Path txt;
        ga.createPath (txt);
        txt.applyTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi).translated (R + 47.0f, 162.0f));
        g.fillPath (txt);
    }

    // fumaça saindo do topo; com FUMAÇA ligada sobe mais e mais forte
    for (int k = 0; k < 6; ++k)
    {
        const float a = (0.12f + 0.05f * (float) (k % 3)) * (0.6f + 1.4f * smoke);
        const float h = 22.0f + 110.0f * smoke;
        juce::Path p;
        for (float yy = 0.0f; yy <= h; yy += 1.0f)
        {
            const float u = yy / h;
            const float x = 120.0f + ((float) k - 2.5f) * 14.0f * (0.6f + u) + std::sin (yy * 0.3f + (float) k * 1.7f + t * 1.6f) * 7.0f * u;
            const float y = 22.0f + 110.0f * smoke - yy;
            if (yy == 0.0f) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour ((k % 2 ? theme::leaf : theme::cream).withAlpha (juce::jmin (0.6f, a)));
        g.strokePath (p, juce::PathStrokeType (juce::jmax (1.0f, 6.0f - (float) k * 0.8f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (smoke > 0.01f)
    {
        g.setColour (theme::leaf.withAlpha (0.10f * smoke));
        g.fillRoundedRectangle (0.0f, 0.0f, S, S, 4.0f);
    }

    // envelope (à esquerda)
    g.setColour (theme::leaf.withAlpha (0.13f));
    g.fillRect (26.0f, 24.0f, 18.0f, 186.0f);
    g.setColour (theme::leaf);
    g.fillRect (26.0f, 24.0f + 186.0f * (1.0f - env), 18.0f, 186.0f * env);
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (theme::sans (8.0f, true, 0.15f), "ENVELOPE", 0.0f, 0.0f);
        juce::Path txt;
        ga.createPath (txt);
        txt.applyTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi).translated (19.0f, 160.0f));
        g.fillPath (txt);
    }

    // rodapé: quanto o envelope está mexendo e o corte de agora
    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    const int envPct = juce::roundToInt (env * param (proc, id::envAmt));
    auto sl = line ("ENVELOPE ", (envPct > 0 ? juce::String ("+") : envPct < 0 ? u8 ("−") : juce::String()) + juce::String (std::abs (envPct)) + "%");
    auto sr = line ("CUTOFF ", hzText (fc));
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
    sr.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
}

// =====================================================================
// EDITOR
// =====================================================================
F1Editor::F1Editor (F1Processor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), ladder (p),
      // cor do knob = cor do que ele mexe no desenho
      cutMacro  (p.apvts, id::cutoff,    "CUTOFF",           true,  theme::amber),
      resonance (p.apvts, id::resonance, "RESONANCE",        false, theme::coral),
      poles     (p.apvts, id::poles,     "POLES",            false, theme::cream),
      drive     (p.apvts, id::drive,     "DRIVE",            false, theme::cream),
      range     (p.apvts, id::range,     "RANGE",            false, theme::cream),
      envMacro  (p.apvts, id::envAmt,    "ENVELOPE",         true,  theme::leaf),
      smooth    (p.apvts, id::smooth,    "SMOOTH",           false, theme::leaf),
      lfoRate   (p.apvts, id::lfoRate,   "LFO",              false, theme::violet),
      lfoAmt    (p.apvts, id::lfoAmt,    "LFO AMOUNT",       false, theme::violet),
      sens      (p.apvts, id::sens,      "SENSITIVITY",      false, theme::leaf),
      output    (p.apvts, id::output,    "GAIN",         false, theme::cream),
      mix       (p.apvts, id::mix),
      openKey   ("ABRIR", theme::amber),
      smokeKey  (u8 ("FUMAÇA"), theme::leaf)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &ladder, &cutMacro, &resonance, &poles, &drive, &range, &envMacro, &smooth, &lfoRate, &lfoAmt, &sens,
             &output, &mix, &openKey, &smokeKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    openAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::open, openKey);
    smokeAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::smoke, smokeKey);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

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

F1Editor::~F1Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void F1Editor::showPresetMenu()
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

    juce::Component::SafePointer<F1Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void F1Editor::askNewPreset()
{
    juce::Component::SafePointer<F1Editor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void F1Editor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<F1Editor> safe (this);
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

void F1Editor::timerCallback()
{
    strip.repaint();
    ladder.tick (proc.engine.levelNow.exchange (0.0f));

    levelIn  = juce::jmax (proc.meterIn.exchange (0.0f),  levelIn * 0.85f);
    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());

    const float bpm = proc.shownBpm.load();
    const bool  syn = proc.synced.load();
    const bool  wt  = proc.waiting.load();
    if (std::abs (bpm - lastBpm) > 0.05f || syn != lastSync || wt != lastWaiting)
    {
        lastBpm = bpm;
        lastSync = syn;
        lastWaiting = wt;
        canvas.repaint (380, 18, 236, 32);
    }
}

void F1Editor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void F1Editor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void F1Editor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void F1Editor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // ---- logo: folha de sete pontas dentro do anel ----
    {
        const float cx = 31.0f, cy = 34.0f;
        g.setColour (theme::amber.withAlpha (0.5f));
        g.drawEllipse (cx - 14.0f, cy - 14.0f, 28.0f, 28.0f, 1.4f);
        const float angles[7] = { 0.0f, -38.0f, 38.0f, -76.0f, 76.0f, -110.0f, 110.0f };
        const float lens[7]   = { 7.0f, 6.0f, 6.0f, 4.6f, 4.6f, 3.2f, 3.2f };
        for (int i = 0; i < 7; ++i)
        {
            juce::Path leaflet;
            const float ry = lens[i], rx = i == 0 ? 2.1f : 1.7f;
            const float centreY = cy + 2.0f - (i < 5 ? 7.5f : 6.0f);
            leaflet.addEllipse (cx - rx, centreY - ry, rx * 2.0f, ry * 2.0f);
            leaflet.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (angles[i]), cx, cy + 2.0f));
            g.setColour (i == 0 ? theme::leaf.brighter (0.2f) : theme::leaf);
            g.fillPath (leaflet);
        }
        g.setColour (theme::leaf);
        g.fillRect (cx - 0.7f, cy + 2.0f, 1.4f, 8.0f);
    }

    const float brandEnd = drawBrand (g, "F1", "LADDER FILTER 101", u8 ("LOW-PASS · ENVELOPE"));
    drawSync (g, proc.shownBpm.load(), proc.synced.load(), proc.waiting.load(), brandEnd);

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "ESCADA ", u8 ("· 4 DEGRAUS"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "FILTER");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "ENVELOPE");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 110.0f }, "MACRO CUTOFF",
              u8 ("Onde o filtro fecha. Gira devagar e o som abre, como no pedal clássico."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 110.0f }, "MACRO ENVELOPE",
              u8 ("Quanto a força do som abre (+) ou fecha (−) o filtro. É o wah automático."));
}

void F1Editor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    ladder.setBounds (22, 246, 240, 264);

    cutMacro.setBounds  (294, 246, 92, 92);
    resonance.setBounds (294, 376, 119, 92);
    poles.setBounds     (413, 376, 119, 92);
    drive.setBounds     (294, 468, 119, 92);
    range.setBounds     (413, 468, 119, 92);

    envMacro.setBounds (562, 246, 92, 92);
    smooth.setBounds   (562, 376, 119, 92);
    lfoRate.setBounds  (681, 376, 119, 92);
    lfoAmt.setBounds   (562, 468, 119, 92);
    sens.setBounds     (681, 468, 119, 92);

    openKey.setBounds  (830, 246, 71, 40);
    smokeKey.setBounds (907, 246, 71, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);
}
