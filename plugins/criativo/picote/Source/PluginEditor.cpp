#include "PluginEditor.h"

using namespace picote;

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
    real::led (g, led.getCentre(), 3.5f, theme::amber, syn ? 1.0f : 0.0f);
}

float param (PicoteProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

// área dos passos dentro da grade
juce::Rectangle<float> stepsArea (juce::Rectangle<float> r)
{
    return { 12.0f, 28.0f, r.getWidth() - 24.0f, r.getHeight() - 38.0f };
}

juce::Colour laneColour (int lane)
{
    return lane == lanePan ? theme::ice : lane == laneFlt ? theme::coral : theme::amber;
}
} // namespace

// =====================================================================
// PASSOS: as 16 colunas; clique e arraste para desenhar a faixa escolhida
// =====================================================================
juce::Rectangle<float> StepGrid::tabRect (int i) const
{
    return { 92.0f + (float) i * 74.0f, 4.0f, 70.0f, 18.0f };
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth();
    const int   len  = juce::roundToInt (param (proc, id::length));
    const int   now  = proc.engine.stepNow.load();
    const bool  held = proc.engine.holding.load();
    const auto  col  = laneColour (lane);

    real::card (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, "STEPS ", u8 ("·"));

    // abas: VOLUME · PAN · FILTRO (seletor embutido; a aba ativa acende na cor da faixa)
    for (int i = 0; i < numLanes; ++i)
    {
        const auto tr = tabRect (i);
        const bool on = i == lane;
        real::screen (g, tr, 3.0f);
        if (on)
        {
            g.setColour (laneColour (i).withAlpha (0.15f));
            g.fillRoundedRectangle (tr, 3.0f);
            g.setColour (laneColour (i));
            g.drawRoundedRectangle (tr.reduced (0.5f), 3.0f, 1.0f);
        }
        g.setColour (on ? theme::cream : theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.15f));
        g.drawText (laneNames[i], tr, juce::Justification::centred);
        real::glass (g, tr, 3.0f);
    }
    {
        juce::AttributedString a;
        a.append (divNames[juce::jlimit (0, numDivs - 1, juce::roundToInt (param (proc, id::div)))], theme::mono (10.0f), theme::cream);
        a.append (u8 (" · ") + juce::String (len) + " STEPS", theme::mono (10.0f), theme::dim);
        if (held) a.append (u8 (" · PRESO NO ") + juce::String (now + 1), theme::mono (10.0f), theme::coral);
        a.setJustification (juce::Justification::centredRight);
        a.draw (g, { W - 400.0f, 6.0f, 388.0f, 14.0f });
    }

    const auto  area = stepsArea (r);
    const float cw = area.getWidth() / (float) numSteps;
    float norm = 0.02f;
    for (auto& p : proc.engine.stepPeak) norm = juce::jmax (norm, p.load (std::memory_order_relaxed));

    for (int i = 0; i < numSteps; ++i)
    {
        const auto  cell = juce::Rectangle<float> (area.getX() + (float) i * cw + 2.0f, area.getY(), cw - 4.0f, area.getHeight());
        const bool  active = i < len;
        const float alpha  = active ? 1.0f : 0.3f;
        const float v = param (proc, laneId (lane, i));

        g.setColour ((i % 4 == 0 ? theme::panel2.brighter (0.06f) : theme::panel2).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (cell, 2.0f);

        // o que passou por esse passo na última volta (fundo)
        const float live = active ? proc.engine.stepPeak[(size_t) i].load (std::memory_order_relaxed) / norm : 0.0f;
        g.setColour (theme::cream.withAlpha (0.07f));
        g.fillRect (cell.withTop (cell.getBottom() - cell.getHeight() * juce::jlimit (0.0f, 1.0f, live)));

        if (lane == lanePan)
        {
            // pan: para cima = direita, para baixo = esquerda
            const float mid = cell.getCentreY(), h = v / 100.0f * cell.getHeight() * 0.5f;
            g.setColour (theme::line.withMultipliedAlpha (alpha));
            g.fillRect (cell.getX(), mid, cell.getWidth(), 1.0f);
            g.setColour (col.withMultipliedAlpha (alpha * 0.85f));
            g.fillRect (cell.getX() + 3.0f, juce::jmin (mid, mid - h), cell.getWidth() - 6.0f, juce::jmax (2.0f, std::abs (h)));
        }
        else
        {
            const float h = v / 100.0f * cell.getHeight();
            g.setColour (col.withMultipliedAlpha (alpha * (0.35f + 0.6f * v / 100.0f)));
            g.fillRect (cell.getX() + 3.0f, cell.getBottom() - h, cell.getWidth() - 6.0f, h);
        }

        if (active && i == now)
        {
            g.setColour (held ? theme::coral : theme::cream);
            g.drawRoundedRectangle (cell.reduced (0.5f), 2.0f, 1.5f);
        }
        // LED do passo (estilo sequenciador): acende no passo que está tocando
        if (active)
            real::led (g, { cell.getRight() - 7.0f, cell.getY() + 8.0f }, 2.2f,
                       held ? theme::coral : theme::cream, i == now ? 1.0f : 0.0f);
        g.setColour (active ? theme::dim : theme::off);
        g.setFont (theme::mono (8.5f));
        g.drawText (juce::String (i + 1), cell.withHeight (12.0f).translated (0.0f, 2.0f), juce::Justification::centred);
    }

    if (lane == lanePan)
    {
        g.setColour (theme::dim);
        g.setFont (theme::mono (8.5f));
        g.drawText ("D", juce::Rectangle<float> (2.0f, area.getY(), 10.0f, 12.0f), juce::Justification::centred);
        g.drawText ("E", juce::Rectangle<float> (2.0f, area.getBottom() - 12.0f, 10.0f, 12.0f), juce::Justification::centred);
    }
}

void StepGrid::setFromMouse (juce::Point<float> p)
{
    const auto area = stepsArea (getLocalBounds().toFloat());
    const int  step = (int) std::floor ((p.x - area.getX()) / (area.getWidth() / (float) numSteps));
    if (step < 0 || step >= numSteps) return;

    const float v01 = juce::jlimit (0.0f, 1.0f, (area.getBottom() - p.y) / area.getHeight());
    float v = lane == lanePan ? (v01 * 2.0f - 1.0f) * 100.0f : v01 * 100.0f;
    if (lane == lanePan && std::abs (v) < 8.0f) v = 0.0f;   // perto do meio cai no centro
    if (auto* rp = proc.apvts.getParameter (laneId (lane, step)))
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        rp->endChangeGesture();
    }
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numLanes; ++i)
        if (tabRect (i).contains (e.position))
        {
            lane = i;
            repaint();
            return;
        }
    if (stepsArea (getLocalBounds().toFloat()).expanded (0.0f, 6.0f).contains (e.position))
        setFromMouse (e.position);
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mouseDownPosition.y >= 24.0f)
        setFromMouse (e.position);
}

void StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    // duplo clique: volume liga/desliga; pan volta ao centro; filtro abre
    const auto area = stepsArea (getLocalBounds().toFloat());
    const int  step = (int) std::floor ((e.position.x - area.getX()) / (area.getWidth() / (float) numSteps));
    if (step < 0 || step >= numSteps || ! area.contains (e.position)) return;
    if (auto* rp = proc.apvts.getParameter (laneId (lane, step)))
    {
        const float cur = rp->convertFrom0to1 (rp->getValue());
        const float v = lane == laneVol ? (cur > 0.5f ? 0.0f : 100.0f) : lane == lanePan ? 0.0f : 100.0f;
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        rp->endChangeGesture();
    }
    repaint();
}

// =====================================================================
// FATIA: o envelope de um passo e a roda com o padrão inteiro
// =====================================================================
void SliceView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth();
    const int   len  = juce::roundToInt (param (proc, id::length));
    const int   now  = proc.engine.stepNow.load();
    const bool  held = proc.engine.holding.load();
    const float depth = param (proc, id::depth) * 0.01f;
    const float vol   = param (proc, id::vol[juce::jlimit (0, numSteps - 1, now)]) * 0.01f;

    const auto scr = juce::Rectangle<float> (0.0f, 0.0f, S, S).reduced (1.5f);   // o rebaixo desenha 1 px para fora
    real::screen (g, scr, 4.0f);

    // envelope do passo atual: dois passos lado a lado, chão = 1 − PROFUNDIDADE
    {
        const float ex0 = 12.0f, ex1 = S - 12.0f, ey0 = 14.0f, ey1 = 96.0f;
        const double beats  = divBeats[juce::jlimit (0, numDivs - 1, juce::roundToInt (param (proc, id::div)))];
        const double stepSec = beats * 60.0 / juce::jmax (40.0f, proc.shownBpm.load());
        const float  w = param (proc, id::width) * 0.01f, at = param (proc, id::attack), rl = param (proc, id::release);
        auto yOf = [=] (float gn) { return ey1 - gn * (ey1 - ey0); };

        g.setColour (theme::segOff);
        g.fillRect (ex0, yOf (1.0f - depth), ex1 - ex0, 1.0f);
        g.fillRect ((ex0 + ex1) * 0.5f, ey0, 1.0f, ey1 - ey0);

        juce::Path env, fill;
        const int pts = 120;
        const float e0 = stepEnvelope (stepSec, stepSec, w, at, rl);   // o passo começa de onde o anterior parou
        for (int k = 0; k <= pts; ++k)
        {
            const double u = 2.0 * (double) k / pts;            // dois passos
            const double t = (u - std::floor (u)) * stepSec;
            const float  e = stepEnvelope (u >= 2.0 ? stepSec : t, stepSec, w, at, rl, e0);
            const float  gn = 1.0f - depth + depth * vol * e;
            const float  x = ex0 + (ex1 - ex0) * (float) (u / 2.0);
            if (k == 0) env.startNewSubPath (x, yOf (gn)); else env.lineTo (x, yOf (gn));
        }
        fill = env;
        fill.lineTo (ex1, ey1);
        fill.lineTo (ex0, ey1);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (0.35f), 0.0f, ey0, theme::amber.withAlpha (0.0f), 0.0f, ey1, false));
        g.fillPath (fill);
        g.setColour (theme::amber);
        g.strokePath (env, juce::PathStrokeType (1.6f));

        // onde o passo está agora
        const float fx = ex0 + (ex1 - ex0) * 0.5f * proc.engine.fracNow.load();
        g.setColour (theme::cream);
        g.fillRect (fx, ey0, 1.0f, ey1 - ey0);
    }

    // roda do padrão: cada ponto é um passo, tamanho = volume; aceso = tocando
    {
        const float cx = S * 0.5f, cy = 170.0f, rad = 54.0f;
        g.setColour (held ? theme::coral.withAlpha (0.35f) : theme::line);
        g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.0f);
        for (int i = 0; i < len; ++i)
        {
            const float a  = juce::MathConstants<float>::twoPi * (float) i / (float) len;
            const float px = cx + rad * std::sin (a), py = cy - rad * std::cos (a);
            const float v  = param (proc, id::vol[i]) * 0.01f;
            const float d  = 4.0f + 8.0f * v;
            const bool  on = i == now;
            // cada passo é um LED: o que toca acende cheio; os outros na intensidade do volume
            if (on)
                real::led (g, { px, py }, d * 0.5f, held ? theme::coral : theme::cream, 1.0f);
            else
                real::led (g, { px, py }, d * 0.5f, theme::amber, v > 0.01f ? 0.25f + 0.45f * v : 0.0f);
        }
        g.setColour (held ? theme::coral : theme::cream);
        g.setFont (theme::mono (22.0f));
        g.drawText (juce::String (now + 1), juce::Rectangle<float> (cx - 40.0f, cy - 16.0f, 80.0f, 24.0f), juce::Justification::centred);
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.15f));
        g.drawText (held ? "PRESO" : "STEP", juce::Rectangle<float> (cx - 40.0f, cy + 8.0f, 80.0f, 12.0f), juce::Justification::centred);
    }

    real::glass (g, scr, 4.0f);

    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    const float gn = proc.engine.gainNow.load();
    auto sl = line ("SWING ", juce::String (juce::roundToInt (param (proc, id::swing))) + "%");
    auto sr = line ("GAIN ", gn < 0.001f ? juce::String (u8 ("−∞")) : dbText (juce::Decibels::gainToDecibels (gn)));
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
    sr.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
}

// =====================================================================
// EDITOR
// =====================================================================
PicoteEditor::PicoteEditor (PicoteProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p), slice (p),
      // cor do knob = cor do que ele mexe no desenho
      depthMacro (p.apvts, id::depth,    "DEPTH",        true,  theme::amber),
      division   (p.apvts, id::div,      u8 ("DIVISÃO"), false, theme::cream),
      length     (p.apvts, id::length,   "STEPS",        false, theme::cream),
      swing      (p.apvts, id::swing,    "SWING",        false, theme::cream),
      offset     (p.apvts, id::offset,   "DESLOCAR",     false, theme::cream),
      widthMacro (p.apvts, id::width,    "WIDTH",        true,  theme::amber),
      attack     (p.apvts, id::attack,   "ATTACK",       false, theme::amber),
      release    (p.apvts, id::release,  "RELEASE",      false, theme::amber),
      filtType   (p.apvts, id::filtType, "FILTER",       false, theme::coral),
      reso       (p.apvts, id::reso,     "RESONANCE",    false, theme::coral),
      output     (p.apvts, id::output,   "GAIN",         false, theme::cream),
      mix        (p.apvts, id::mix),
      shuffleKey ("EMBARALHAR", theme::violet),
      holdKey    ("PRENDER", theme::coral)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &grid, &slice, &depthMacro, &division, &length, &swing, &offset, &widthMacro, &attack, &release, &filtType, &reso,
             &output, &mix, &shuffleKey, &holdKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    // EMBARALHAR é um golpe (não fica ligado): sorteia a faixa que está na tela
    shuffleKey.setClickingTogglesState (false);
    shuffleKey.onClick = [this]
    {
        proc.shuffleLane (grid.lane);
        shuffleKey.setToggleState (true, juce::dontSendNotification);
        shuffleFlash = 6;
    };
    holdAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::hold, holdKey);
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

PicoteEditor::~PicoteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PicoteEditor::showPresetMenu()
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

    juce::Component::SafePointer<PicoteEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void PicoteEditor::askNewPreset()
{
    juce::Component::SafePointer<PicoteEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void PicoteEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<PicoteEditor> safe (this);
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

void PicoteEditor::timerCallback()
{
    grid.repaint();
    slice.repaint();

    if (shuffleFlash > 0 && --shuffleFlash == 0)
        shuffleKey.setToggleState (false, juce::dontSendNotification);

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
        canvas.repaint (370, 10, 246, 48);   // folga para o brilho do LED de sync
    }
}

void PicoteEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void PicoteEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void PicoteEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void PicoteEditor::drawCanvas (juce::Graphics& g)
{
    real::chassis (g, (float) ui::baseW, (float) ui::baseH);

    // ---- logo: tesoura picando uma onda ----
    g.setColour (theme::amber.withAlpha (0.5f));
    g.drawEllipse (17.0f, 20.0f, 28.0f, 28.0f, 1.4f);
    for (int k = 0; k < 4; ++k)
    {
        const float x = 22.5f + (float) k * 4.6f;
        const float h = k % 2 == 0 ? 12.0f : 5.0f;
        g.setColour (k % 2 == 0 ? theme::amber : theme::coral);
        g.fillRoundedRectangle (x, 34.0f - h * 0.5f, 3.2f, h, 1.0f);
    }

    const float brandEnd = drawBrand (g, "PICOTE", "RHYTHMIC GATE", "16-STEP GRID");
    drawSync (g, proc.shownBpm.load(), proc.synced.load(), proc.waiting.load(), brandEnd);

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "FATIA ", u8 ("· 1 STEP"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "GRADE");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "FORMA");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 110.0f }, "MACRO DEPTH",
              u8 ("Quanto o som fecha entre um golpe e outro. Em 0%, só o PAN e o FILTER trabalham."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 110.0f }, "MACRO WIDTH",
              u8 ("Quanto de cada passo fica aberto: curto é picado seco, 100% só respira."));
}

void PicoteEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    grid.setBounds (12, 66, 976, 140);
    slice.setBounds (22, 246, 240, 264);

    depthMacro.setBounds (294, 246, 92, 92);
    division.setBounds   (294, 376, 119, 92);
    length.setBounds     (413, 376, 119, 92);
    swing.setBounds      (294, 468, 119, 92);
    offset.setBounds     (413, 468, 119, 92);

    widthMacro.setBounds (562, 246, 92, 92);
    attack.setBounds     (562, 376, 119, 92);
    release.setBounds    (681, 376, 119, 92);
    filtType.setBounds   (562, 468, 119, 92);
    reso.setBounds       (681, 468, 119, 92);

    shuffleKey.setBounds (830, 246, 86, 40);
    holdKey.setBounds    (922, 246, 56, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);
}
