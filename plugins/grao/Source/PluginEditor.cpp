#include "PluginEditor.h"

using namespace grao;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"

void drawTitle (juce::Graphics& g, float x, float y, const juce::String& prefix, const juce::String& accent)
{
    juce::AttributedString s;
    const auto f = theme::sans (9.5f, true, 0.25f);
    s.append (prefix, f, theme::dim);
    s.append (accent, f, theme::amber);
    s.setJustification (juce::Justification::centredLeft);
    s.draw (g, { x, y, 260.0f, 13.0f });
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
} // namespace

// =====================================================================
// FITA
// =====================================================================
void TapeView::paint (juce::Graphics& g)
{
    const auto  r      = getLocalBounds().toFloat();
    const float W      = r.getWidth(), H = r.getHeight(), mid = H * 0.5f + 8.0f;
    const bool  frozen = proc.apvts.getRawParameterValue (id::freeze)->load() > 0.5f;

    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);

    // régua de tempo
    g.setFont (theme::mono (9.5f));
    for (int s = 0; s <= 4; ++s)
    {
        const float x = W - (float) s / 4.0f * W;
        g.setColour (theme::body);
        g.fillRect (x, 26.0f, 1.0f, H - 26.0f);
        if (s > 0)
        {
            g.setColour (juce::Colour (0xff5c5548));
            g.drawText ("-" + juce::String (s) + "s", juce::Rectangle<float> (x + 4.0f, H - 17.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
        }
    }

    // forma de onda gravada (da mais antiga, à esquerda, para a mais nova)
    auto& e = proc.engine;
    const int   cols  = GrainEngine::peakCols;
    const int   start = e.peakWrite.load (std::memory_order_acquire);
    const float colW  = W / (float) cols, maxH = (H - 50.0f) * 0.5f;
    g.setColour (frozen ? theme::ice.withAlpha (0.27f) : theme::off);
    for (int i = 0; i < cols; ++i)
    {
        const float v = e.peaks[(size_t) ((start + i) % cols)].load (std::memory_order_relaxed);
        const float h = juce::jmax (0.5f, std::sqrt (juce::jmin (1.0f, v)) * maxH);
        g.fillRect ((float) i * colW, mid - h, juce::jmax (1.0f, colW - 0.7f), h * 2.0f);
    }

    // grãos: posição na fita, altura = afinação, cauda = espaço/feedback/decaimento
    const float sp    = proc.apvts.getRawParameterValue (id::space)->load() * 0.01f;
    const float fb    = proc.apvts.getRawParameterValue (id::feedback)->load() * 0.01f;
    const float dec   = (proc.apvts.getRawParameterValue (id::decay)->load() - 0.2f) / 9.8f;
    for (auto& v : e.views)
    {
        if (! v.active.load (std::memory_order_acquire)) continue;
        const float env = v.env.load (std::memory_order_relaxed);
        const float x   = W * (1.0f - v.lag.load (std::memory_order_relaxed));
        const float w   = juce::jmax (2.0f, v.span.load (std::memory_order_relaxed) * W);
        const float y   = mid - juce::jlimit (-1.0f, 1.0f, v.pitch.load (std::memory_order_relaxed) / 24.0f) * (H - 60.0f) * 0.5f;
        const float x0  = x - w * 0.5f;

        const float tail = w * (sp * 2.0f + fb * 3.0f) * (1.0f + dec);
        if (tail > 1.0f)
        {
            g.setGradientFill (juce::ColourGradient (theme::coral.withAlpha (env * 0.35f), x0 + w, 0.0f,
                                                     theme::coral.withAlpha (0.0f), x0 + w + tail, 0.0f, false));
            g.fillRect (x0 + w, y - 3.0f, tail, 6.0f);
        }
        g.setColour (theme::amber.withAlpha (env * 0.85f));
        g.fillRect (x0, y - 5.0f, w, 10.0f);

        if (v.reverse.load (std::memory_order_relaxed) && w > 8.0f)
        {
            juce::Path p;
            p.addTriangle (x0 + 3.0f, y, x0 + 9.0f, y - 3.5f, x0 + 9.0f, y + 3.5f);
            g.setColour (theme::bg.withAlpha (env));
            g.fillPath (p);
        }
    }

    // cabeçote de gravação
    if (! frozen)
    {
        g.setColour (theme::coral);
        g.fillRect (W - 2.0f, 22.0f, 2.0f, H - 22.0f);
        g.setFont (theme::sans (8.5f, true));
        g.drawText ("REC", juce::Rectangle<float> (W - 34.0f, 25.0f, 28.0f, 10.0f), juce::Justification::centredRight);
    }
    else
    {
        auto badge = juce::Rectangle<float> (W - 96.0f, 7.0f, 86.0f, 17.0f);
        g.setColour (theme::ice.withAlpha (0.13f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (theme::ice.withAlpha (0.33f));
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setColour (juce::Colour (0xff8fd0ff));
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText ("CONGELADA", badge, juce::Justification::centred);
    }

    drawTitle (g, 12.0f, 7.0f, "FITA ", u8 ("· 4 s"));

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// =====================================================================
// CAMPO XY
// =====================================================================
XYPad::XYPad (APVTS& state)
    : space (state.getParameter (id::space)), grain (state.getParameter (id::grain))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void XYPad::tick()
{
    t += 1.0f;
    const auto f = field();
    trail.push_back ({ space->getValue() * f.getWidth(), (1.0f - grain->getValue()) * f.getHeight() });
    if (trail.size() > 40) trail.erase (trail.begin());
    repaint();
}

void XYPad::paint (juce::Graphics& g)
{
    const auto  f  = field();
    const float PS = f.getWidth();
    const float px = space->getValue() * PS, py = (1.0f - grain->getValue()) * PS;

    g.setColour (theme::dark);
    g.fillRoundedRectangle (f, 3.0f);

    // campo de pontos: acende perto do cursor
    const int N = 16;
    const float st = PS / (float) N;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
        {
            const float x = st * ((float) i + 0.5f), y = st * ((float) j + 0.5f);
            const float d = std::hypot (x - px, y - py) / PS;
            const float w = juce::jmax (0.0f, 1.0f - d * 2.6f) * (0.75f + 0.25f * std::sin (t / 14.0f - d * 18.0f));
            if (w > 0.02f)
                g.setColour (juce::Colour::fromFloatRGBA (1.0f, (107.0f + 72.0f * (1.0f - w)) / 255.0f,
                                                         74.0f * (1.0f - w) / 255.0f, 0.15f + w * 0.85f));
            else
                g.setColour (theme::ledOff);
            const float rad = 1.2f + w * 2.6f;
            g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);
        }

    // mira
    const float dash[] = { 3.0f, 4.0f };
    g.setColour (theme::cream.withAlpha (0.2f));
    g.drawDashedLine ({ px, 0.0f, px, PS }, dash, 2, 1.0f);
    g.drawDashedLine ({ 0.0f, py, PS, py }, dash, 2, 1.0f);

    // rastro
    if (trail.size() > 1)
    {
        juce::Path p;
        p.startNewSubPath (trail.front());
        for (size_t i = 1; i < trail.size(); ++i) p.lineTo (trail[i]);
        g.setColour (theme::amber.withAlpha (0.33f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    // cursor em losango
    {
        juce::Path outer, inner;
        outer.addRectangle (-8.0f, -8.0f, 16.0f, 16.0f);
        inner.addRectangle (-3.5f, -3.5f, 7.0f, 7.0f);
        const auto tr = juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.25f).translated (px, py);
        outer.applyTransform (tr);
        inner.applyTransform (tr);
        g.setColour (theme::cream);
        g.strokePath (outer, juce::PathStrokeType (1.5f));
        g.setColour (theme::amber);
        g.fillPath (inner);
    }

    // eixos
    g.setColour (theme::dim);
    g.setFont (theme::sans (8.5f, true, 0.1f));
    g.drawText (u8 ("ESPAÇO →"), juce::Rectangle<float> (PS - 70.0f, PS - 16.0f, 64.0f, 12.0f), juce::Justification::centredRight);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi).translated (6.0f, 70.0f));
        g.drawText (u8 ("GRÃO →"), juce::Rectangle<float> (0.0f, 0.0f, 64.0f, 12.0f), juce::Justification::centredLeft);
    }

    g.setColour (theme::line);
    g.drawRoundedRectangle (f.reduced (0.5f), 3.0f, 1.0f);

    // valores
    auto line = [] (const juce::String& a, int v)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (juce::String (v), theme::mono (10.5f), theme::cream);
        return s;
    };
    auto sx = line (u8 ("X ESPAÇO "), juce::roundToInt (space->getValue() * 100.0f));
    auto sy = line (u8 ("Y GRÃO "),   juce::roundToInt (grain->getValue() * 100.0f));
    sx.setJustification (juce::Justification::centredLeft);
    sy.setJustification (juce::Justification::centredRight);
    sx.draw (g, { 0.0f, PS + 8.0f, PS, 14.0f });
    sy.draw (g, { 0.0f, PS + 8.0f, PS, 14.0f });
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    space->beginChangeGesture();
    grain->beginChangeGesture();
    setFrom (e);
}

void XYPad::mouseDrag (const juce::MouseEvent& e) { setFrom (e); }

void XYPad::mouseUp (const juce::MouseEvent&)
{
    space->endChangeGesture();
    grain->endChangeGesture();
}

void XYPad::setFrom (const juce::MouseEvent& e)
{
    const auto f = field();
    const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - f.getX()) / f.getWidth());
    const float y = 1.0f - juce::jlimit (0.0f, 1.0f, (e.position.y - f.getY()) / f.getHeight());
    space->setValueNotifyingHost (x);
    grain->setValueNotifyingHost (y);
    repaint();
}

// =====================================================================
// EDITOR
// =====================================================================
GraoEditor::GraoEditor (GraoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), tape (p), pad (p.apvts),
      grainMacro (p.apvts, id::grain,    u8 ("GRÃO"),      true,  theme::amber),
      spaceMacro (p.apvts, id::space,    u8 ("ESPAÇO"),    true,  theme::coral),
      size       (p.apvts, id::size,     "TAMANHO",        false, theme::cream),
      density    (p.apvts, id::density,  "DENSIDADE",      false, theme::cream),
      spray      (p.apvts, id::spray,    u8 ("DISPERSÃO"), false, theme::cream),
      pitch      (p.apvts, id::pitch,    u8 ("AFINAÇÃO"),  false, theme::cream),
      spread     (p.apvts, id::spread,   u8 ("ESTÉREO"),   false, theme::cream),
      feedback   (p.apvts, id::feedback, "FEEDBACK",       false, theme::cream),
      decay      (p.apvts, id::decay,    "DECAIMENTO",     false, theme::cream),
      tone       (p.apvts, id::tone,     "TOM",            false, theme::cream),
      output     (p.apvts, id::output,   "GANHO",          false, theme::cream),
      mix        (p.apvts, id::mix),
      reverseKey ("REVERSO", theme::coral),
      freezeKey  ("CONGELAR", theme::ice)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &tape, &pad, &grainMacro, &spaceMacro, &size, &density, &spray, &pitch, &spread, &feedback, &decay, &tone,
             &output, &mix, &reverseKey, &freezeKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    reverseAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::reverse, reverseKey);
    freezeAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::freeze, freezeKey);
    bypassAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

    prevBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    nextBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    saveBtn.onClick    = [this] { proc.saveUserPreset(); };
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

GraoEditor::~GraoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GraoEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto names = proc.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i], true, i == proc.getPresetIndex());

    juce::PopupMenu sizes;
    const float current = proc.uiScale.load();
    for (int pct : { 75, 100, 125, 150, 175, 200 })
        sizes.addItem (sizeMenuBase + pct, juce::String (pct) + "%", true, std::abs (current * 100.0f - (float) pct) < 1.0f);
    menu.addSeparator();
    menu.addSubMenu ("Tamanho da janela", sizes);

    juce::Component::SafePointer<GraoEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            if (result > sizeMenuBase)
                                safe->setScale ((float) (result - sizeMenuBase) / 100.0f);
                            else
                                safe->proc.loadPreset (result - 1);
                        });
}

void GraoEditor::timerCallback()
{
    tape.repaint();
    pad.tick();

    levelIn  = juce::jmax (proc.meterIn.exchange (0.0f),  levelIn * 0.85f);
    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void GraoEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void GraoEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void GraoEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void GraoEditor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // ---- logo ----
    g.setColour (theme::amber);
    g.drawEllipse (18.0f, 21.0f, 26.0f, 26.0f, 2.0f);
    auto dot = [&g] (float x, float y, float r, juce::Colour c)
    {
        g.setColour (c);
        g.fillEllipse (16.0f + x - r, 19.0f + y - r, r * 2.0f, r * 2.0f);
    };
    dot (10.0f, 12.0f, 2.4f, theme::amber);
    dot (18.0f, 9.0f, 1.6f, theme::cream);
    dot (19.0f, 18.0f, 2.8f, theme::coral);
    dot (11.0f, 20.0f, 1.3f, theme::cream);

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const auto name = u8 ("GRÃO");
    juce::GlyphArrangement ga;
    ga.addLineOfText (nameFont, name, 0.0f, 0.0f);
    const float tagX = 56.0f + ga.getBoundingBox (0, -1, true).getWidth() + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 160.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    g.setColour (theme::dim);
    g.setFont (theme::sans (9.5f, false, 0.2f));
    g.drawText ("GRANULAR",   juce::Rectangle<float> (tagX, 22.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText ("TEXTURE FX", juce::Rectangle<float> (tagX, 35.0f, 140.0f, 12.0f), juce::Justification::centredLeft);

    // ---- marca ----
    {
        auto width = [] (const juce::Font& f, const juce::String& s)
        {
            juce::GlyphArrangement a;
            a.addLineOfText (f, s, 0.0f, 0.0f);
            return a.getBoundingBox (0, -1, true).getWidth();
        };
        const auto tagFont = theme::sans (9.5f, false, 0.2f);
        const float brandX = tagX + juce::jmax (width (tagFont, "GRANULAR"), width (tagFont, "TEXTURE FX")) + 20.0f;
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
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "CAMPO ", "XY");
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), u8 ("GRÃO"));
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), u8 ("ESPAÇO"));
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), u8 ("SAÍDA"));

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 90.0f }, u8 ("MACRO GRÃO"),
              u8 ("Quanto o som é picado: tamanho, densidade e variação de afinação."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 90.0f }, u8 ("MACRO ESPAÇO"),
              u8 ("Abertura estéreo, repetição dos grãos e cauda do ambiente."));
}

void GraoEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    tape.setBounds (12, 66, 976, 140);
    pad.setBounds (22, 246, 240, 264);

    grainMacro.setBounds (294, 246, 92, 92);
    size.setBounds    (294, 376, 119, 92);
    density.setBounds (413, 376, 119, 92);
    spray.setBounds   (294, 468, 119, 92);
    pitch.setBounds   (413, 468, 119, 92);

    spaceMacro.setBounds (562, 246, 92, 92);
    spread.setBounds   (562, 376, 119, 92);
    feedback.setBounds (681, 376, 119, 92);
    decay.setBounds    (562, 468, 119, 92);
    tone.setBounds     (681, 468, 119, 92);

    reverseKey.setBounds (830, 246, 71, 40);
    freezeKey.setBounds  (907, 246, 71, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);
}
