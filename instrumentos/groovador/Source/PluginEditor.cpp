#include "PluginEditor.h"

using namespace grv;
using gv::RF;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuNoUser = 20005,
              menuLicenca = 20007;

float param (GroovadorProcessor& p, const juce::String& pid) { return p.apvts.getRawParameterValue (pid)->load(); }

// o som A de um instrumento, lido direto dos parâmetros (para o desenho no LCD)
Sound soundFromParams (GroovadorProcessor& p, int k)
{
    auto v = [&p, k] (const juce::String& name) { return param (p, id::i (k, name)); };
    Sound s;
    s.model = juce::jlimit (0, 4, juce::roundToInt (v (id::model)));
    s.inst  = k;
    s.hz    = baseHz[k] * std::exp2 (v (id::tune) / 12.0f);
    s.decMs = v (id::dec);
    s.tone  = v (id::tone);
    s.snap  = v (id::snap) * 0.01f;
    s.body  = v (id::body) * 0.01f;
    s.noise = v (id::noise) * 0.01f;
    s.pEnv  = v (id::pEnv);
    s.drive = v (id::drive) * 0.01f;
    s.gain  = 1.0f;
    s.highPass = k == hatC || k == hatO || k == shaker;
    return s;
}
} // namespace

// =====================================================================
// FILEIRA DO INSTRUMENTO
// =====================================================================
LaneRow::LaneRow (GroovadorProcessor& p, int instIndex) : proc (p), k (instIndex)
{
    for (auto* b : { &mute, &solo, &dice }) addAndMakeVisible (*b);
    mute.setBounds (182, 4, 19, 19);
    solo.setBounds (204, 4, 19, 19);
    dice.setBounds (226, 4, 19, 19);
    muteAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id::mute (k), mute);
    soloAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id::solo (k), solo);
    dice.onClick = [this] { proc.randomizeInstrument (k); };
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LaneRow::tick()
{
    const float lv = proc.engine.meter[(size_t) k].load();
    const float vr = param (proc, id::who (k)) > 0.5f ? proc.varNow[(size_t) k].load() : -9.0f;
    const bool sel = proc.uiSel.load() == k;
    const int model = juce::roundToInt (param (proc, id::i (k, id::model)));
    if (std::abs (lv - shownLevel) > 0.03f || std::abs (vr - shownVar) > 0.02f || sel != shownSel || model != shownModel)
    {
        shownLevel = lv; shownVar = vr; shownSel = sel; shownModel = model;
        repaint();
    }
}

void LaneRow::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f).withTrimmedBottom (1.0f);
    const bool sel = proc.uiSel.load() == k;
    if (sel)
    {
        g.setColour (gv::led.withAlpha (0.3f));
        g.fillRoundedRectangle (r.expanded (2.5f), 5.0f);
    }
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27272a), 0.0f, r.getY(), juce::Colour (0xff18181a), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (gv::instColour (k));
    g.fillRoundedRectangle (r.withWidth (8.0f), 3.0f);
    g.fillRect (r.getX() + 4.0f, r.getY(), 4.0f, r.getHeight());
    if (sel)
    {
        g.setColour (gv::led);
        g.drawRoundedRectangle (r.expanded (1.0f), 4.0f, 2.0f);
    }

    g.setFont (gv::sans (11.0f, true, 0.1f));
    g.setColour (gv::silk);
    g.drawText (u8 (instNames[k]), RF (r.getX() + 14.0f, r.getY() + 1.0f, 100.0f, 13.0f), juce::Justification::centredLeft, false);
    g.setFont (gv::sans (8.0f, true, 0.06f));
    g.setColour (gv::dim);
    g.drawText (u8 (modelLong[juce::jlimit (0, 4, juce::roundToInt (param (proc, id::i (k, id::model))))]),RF (r.getX() + 14.0f, r.getY() + 13.0f, 80.0f, 11.0f),
                juce::Justification::centredLeft, false);

    // A/B: em qual timbre o instrumento está agora (só quem varia)
    if (param (proc, id::who (k)) > 0.5f)
    {
        const float v = proc.varNow[(size_t) k].load();
        const bool b = std::abs (v) > 0.05f;
        const RF tag (r.getX() + 86.0f, r.getY() + 6.0f, 16.0f, 13.0f);
        g.setColour (b ? gv::led.withAlpha (0.25f) : juce::Colour (0xff2a2a2d));
        g.fillRoundedRectangle (tag, 2.0f);
        g.setFont (gv::mono (10.0f));
        g.setColour (b ? gv::led : gv::dim);
        g.drawText (b ? "B" : "A", tag, juce::Justification::centred, false);
    }

    // LEDs de nível
    const float lv = juce::jlimit (0.0f, 1.0f, std::sqrt (proc.engine.meter[(size_t) k].load()));
    for (int i = 0; i < 10; ++i)
    {
        const bool on = lv > 0.02f && (float) i < lv * 10.0f;
        const RF l (r.getX() + 108.0f + (float) i * 6.0f, r.getCentreY() - 3.0f, 4.0f, 6.0f);
        if (on)
        {
            g.setColour (gv::led.withAlpha (0.3f));
            g.fillRoundedRectangle (l.expanded (1.5f), 2.0f);
        }
        g.setColour (on ? gv::led : juce::Colour (0xff2b1513));
        g.fillRoundedRectangle (l, 1.0f);
    }
}

void LaneRow::mouseUp (const juce::MouseEvent& e)
{
    if (e.position.x < 178.0f && getLocalBounds().contains (e.getPosition()))
    {
        proc.uiSel = k;
        proc.audition = k;
    }
}

// =====================================================================
// GRADE DO GROOVE
// =====================================================================
bool GrooveView::cellAt (juce::Point<float> pos, int& inst, int& step) const
{
    const auto a = gridArea();
    if (! a.contains (pos)) return false;
    const int steps = proc.stepsNow();
    inst = juce::jlimit (0, numInst - 1, (int) ((pos.y - a.getY()) / (a.getHeight() / (float) numInst)));
    step = juce::jlimit (0, steps - 1, (int) ((pos.x - a.getX()) / (a.getWidth() / (float) steps)));
    return true;
}

void GrooveView::mouseDown (const juce::MouseEvent& e)
{
    dragInst = -1;
    int k, i;
    if (! cellAt (e.position, k, i)) return;
    Step s = proc.pattern.get (k, i);

    if (e.mods.isPopupMenu() || e.mods.isCtrlDown())
    {
        // forte → ghost → roll ×2 → roll ×3 → talvez (50%) → forte
        if (! s.on)                     { s.on = true; s.vel = 2; s.roll = 0; s.prob = 0; }
        else if (s.prob > 0)            { s.prob = 0; s.vel = 5; s.roll = 0; }
        else if (s.roll == 2)           { s.roll = 0; s.prob = 2; s.vel = 5; }
        else if (s.roll == 1)           { s.roll = 2; }
        else if (s.ghost())             { s.vel = 5; s.roll = 1; }
        else                            { s.vel = 2; }
    }
    else if (s.on)
        s.on = false;
    else
    {
        s = Step();
        s.on = true;
        s.vel = 5;
    }
    proc.editStep (k, i, s);
    if (s.on && ! e.mods.isPopupMenu())
    {
        dragInst = k;
        dragStep = i;
        dragVel = s.vel;
    }
    repaint();
}

// arrastar para cima/baixo numa batida muda a força
void GrooveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragInst < 0) return;
    Step s = proc.pattern.get (dragInst, dragStep);
    if (! s.on) return;
    s.vel = juce::jlimit (1, 7, dragVel - e.getDistanceFromDragStartY() / 8);
    proc.pattern.set (dragInst, dragStep, s);
    ++proc.patternVersion;
    repaint();
}

void GrooveView::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff333333));
    g.fillRoundedRectangle (all, 4.0f);
    g.setColour (juce::Colour (0xff0d0d0e));
    g.fillRoundedRectangle (all.reduced (1.0f), 4.0f);

    const int steps = proc.stepsNow();
    const int spb   = proc.stepsPerBarNow();
    const int rateIdx = juce::roundToInt (param (proc, id::rate));
    const int perBeat = juce::jmax (1, (int) std::round (1.0 / rateBeats[juce::jlimit (0, 2, rateIdx)]));
    const auto a = gridArea();
    const float sw = a.getWidth() / (float) steps, rh = a.getHeight() / (float) numInst;
    const int ps = proc.playStep.load();
    const bool fine = sw >= 9.0f;

    // régua: LED do passo que toca e a numeração
    g.setFont (gv::sans (8.0f));
    for (int s = 0; s < steps; ++s)
    {
        const float cx = a.getX() + sw * ((float) s + 0.5f);
        gv::ledDot (g, cx, 8.0f, juce::jmin (2.6f, sw * 0.3f), s == ps);
        const int inBar = s % spb;
        const bool beatStart = inBar % perBeat == 0;
        if (sw >= 13.0f || beatStart)
        {
            const juce::String num = sw >= 13.0f ? juce::String (inBar + 1) : juce::String (inBar / perBeat + 1);
            g.setColour (beatStart ? gv::silk : juce::Colour (0xff77746c));
            g.drawText (num, RF (cx - 10.0f, 14.0f, 20.0f, 10.0f), juce::Justification::centred, false);
        }
    }

    // teclas
    for (int k = 0; k < numInst; ++k)
    {
        const bool muted = param (proc, id::mute (k)) > 0.5f;
        for (int s = 0; s < steps; ++s)
        {
            const Step st = proc.pattern.get (k, s);
            const RF key (a.getX() + sw * (float) s + (fine ? 1.5f : 0.5f), a.getY() + rh * (float) k + 2.0f,
                          sw - (fine ? 3.0f : 1.0f), rh - 4.0f);
            const auto kc = gv::keyColour ((s % spb) / perBeat);
            g.setColour (kc.withAlpha (st.on ? (st.ghost() ? 0.65f : 1.0f) : 0.22f));
            g.fillRoundedRectangle (key, 2.0f);
            if (fine)
            {
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.fillRect (key.getX() + 1.0f, key.getY() + 1.0f, key.getWidth() - 2.0f, 1.2f);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillRect (key.getX() + 0.5f, key.getBottom() - 2.0f, key.getWidth() - 1.0f, 1.5f);
            }
            const float lx = key.getCentreX(), ly = key.getY() + 6.0f;
            const float lr = juce::jmin (2.8f, sw * 0.28f);
            if (! st.on)
            {
                g.setColour (juce::Colour (0xff2a0d0a));
                g.fillEllipse (lx - lr * 0.85f, ly - lr * 0.85f, lr * 1.7f, lr * 1.7f);
                continue;
            }
            if (st.roll > 0 && fine)
            {
                const int nDots = st.roll + 1;
                for (int d = 0; d < nDots; ++d)
                    gv::ledDot (g, lx + ((float) d - (float) (nDots - 1) * 0.5f) * 4.2f, ly, 1.6f, true);
            }
            else
                gv::ledDot (g, lx, ly, lr, true, st.ghost() ? 0.45f : 0.4f + st.gain() * 0.6f);
            if (st.prob > 0)
            {
                juce::Path p;
                p.addRoundedRectangle (key.reduced (1.5f), 2.0f);
                juce::Path dashed;
                const float dash[] = { 2.0f, 2.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dash, 2);
                g.setColour (juce::Colour (0xff111111));
                g.fillPath (dashed);
            }
            if (fine)
            {
                const RF bar (key.getX() + 2.0f, key.getBottom() - 7.0f, key.getWidth() - 4.0f, 2.5f);
                g.setColour (juce::Colours::black.withAlpha (0.3f));
                g.fillRect (bar);
                g.setColour (juce::Colour (0xff1a1a1a));
                g.fillRect (bar.withWidth (bar.getWidth() * st.gain()));
            }
        }
        if (muted)
        {
            g.setColour (juce::Colour (0xff0d0d0e).withAlpha (0.6f));
            g.fillRect (a.getX(), a.getY() + rh * (float) k, a.getWidth(), rh);
        }
    }

    // divisória dos compassos
    g.setColour (gv::silk);
    for (int s = spb; s < steps; s += spb)
        g.fillRect (a.getX() + sw * (float) s - 0.75f, a.getY(), 1.5f, a.getHeight());

    // coluna do passo que toca
    if (ps >= 0 && ps < steps)
    {
        const RF col (a.getX() + sw * (float) ps, a.getY(), sw, a.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRect (col);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawRect (col.reduced (0.5f), 1.0f);
    }

    // faixa de timbre: em que trecho cada um está em A ou B
    const int everyIdx = juce::roundToInt (param (proc, id::every));
    const int segLen = juce::jmax (1, (int) std::lround (everyBars[juce::jlimit (0, 4, everyIdx)] * spb));
    const auto mode = (VMode) juce::jlimit (0, 2, juce::roundToInt (param (proc, id::vmode)));
    const float amount = param (proc, id::vamount) * 0.01f;
    bool anyWho = false;
    for (int k = 0; k < numInst; ++k) anyWho = anyWho || param (proc, id::who (k)) > 0.5f;
    const float ty = a.getBottom() + 4.0f;
    for (int start = 0; start < steps; start += segLen)
    {
        const int len = juce::jmin (segLen, steps - start);
        const long long seg = start / segLen;
        const RF blk (a.getX() + sw * (float) start + 1.0f, ty, sw * (float) len - 2.0f, 14.0f);

        juce::String txt;
        float heat = 0.0f;
        if (! anyWho)
            txt = u8 ("NINGUÉM VARIA");
        else if (mode == VMode::drift)
        {
            float v = 0.0f;
            for (int k = 0; k < numInst; ++k)
                if (param (proc, id::who (k)) > 0.5f) { v = proc.varFactorFor (k, seg); break; }
            heat = std::abs (v) * amount;
            txt = "B " + juce::String (juce::roundToInt (v * amount * 100.0f)) + "%";
        }
        else
        {
            juce::StringArray inB;
            for (int k = 0; k < numInst; ++k)
                if (param (proc, id::who (k)) > 0.5f && proc.varFactorFor (k, seg) > 0.5f) inB.add (u8 (instShort[k]));
            heat = inB.isEmpty() ? 0.0f : 1.0f;
            txt = inB.isEmpty() ? juce::String ("TIMBRE A") : "TIMBRE B  " + inB.joinIntoString (" + ");
        }
        if (segLen >= spb) txt = "COMP. " + juce::String (start / spb + 1) + u8 (" · ") + txt;

        g.setColour (anyWho ? gv::silk.interpolatedWith (gv::red, juce::jlimit (0.0f, 1.0f, heat)) : juce::Colour (0xff3a3a3d));
        g.fillRoundedRectangle (blk, 2.0f);
        g.setColour (heat > 0.5f ? juce::Colours::white : juce::Colour (0xff111111));
        g.setFont (gv::sans (9.0f, true, 0.03f));
        g.drawFittedText (txt, blk.reduced (3.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }

    // legenda
    const int bars = barCounts[juce::jlimit (0, 2, juce::roundToInt (param (proc, id::bars)))];
    const int fe = fillEvery[juce::jlimit (0, 2, juce::roundToInt (param (proc, id::fill)))];
    juce::String info = juce::String (bars) + (bars == 1 ? " COMPASSO" : " COMPASSOS") + u8 (" · ")
                      + meterNames[juce::jlimit (0, 1, juce::roundToInt (param (proc, id::meter)))] + u8 (" · ") + rateNames[juce::jlimit (0, 2, rateIdx)];
    if (fe > 0) info << u8 (" · FILL A CADA ") << fe;
    g.setFont (gv::sans (10.0f, false));
    g.setColour (gv::silk);
    g.drawText (info, RF (10.0f, 262.0f, 230.0f, 14.0f), juce::Justification::centredLeft, false);
    if (proc.fillNow.load() && ps >= 0)
    {
        g.setFont (gv::sans (10.0f, true, 0.2f));
        g.setColour (gv::led);
        g.drawText ("FILL", RF (all.getRight() - 50.0f, 262.0f, 40.0f, 14.0f), juce::Justification::centredRight, false);
    }
    g.setFont (gv::sans (9.0f, false));
    g.setColour (gv::dim);
    g.drawFittedText (u8 ("clique liga/desliga · arraste ↕ = força · botão direito: ghost, roll, talvez"),
                      juce::Rectangle<int> (236, 262, (int) all.getWidth() - 296, 14), juce::Justification::centredLeft, 1, 0.7f);
}

// =====================================================================
// EDITOR DE SOM
// =====================================================================
InstPanel::InstPanel (GroovadorProcessor& p, int k)
{
    auto& st = p.apvts;
    model = std::make_unique<gv::LedSegmented> (st, id::i (k, id::model));
    model->fontSize = 9.0f;
    addAndMakeVisible (*model);
    model->setBounds (12, 112, 236, 38);

    struct K { const juce::String* pid; const char* label; juce::Colour c; };
    const K list[10] = { { &id::tune, "TUNE", gv::red }, { &id::dec, "DECAY", gv::red }, { &id::tone, "TONE", gv::orange },
                         { &id::snap, "SNAP", gv::orange }, { &id::body, "BODY", gv::yellow }, { &id::noise, "NOISE", gv::yellow },
                         { &id::pEnv, "PITCH ENV", gv::cream }, { &id::drive, "DRIVE", gv::cream }, { &id::pan, "PAN", gv::grey },
                         { &id::level, "LEVEL", gv::grey } };
    for (int q = 0; q < 10; ++q)
    {
        auto kn = std::make_unique<gv::GKnob> (st, id::i (k, *list[q].pid), list[q].label, list[q].c, 26.0f);
        addAndMakeVisible (*kn);
        kn->setBounds (6 + (q % 5) * 60, 154 + (q / 5) * 62, 60, 62);
        knobs.push_back (std::move (kn));
    }
}

SoundPanel::SoundPanel (GroovadorProcessor& p) : proc (p)
{
    for (int k = 0; k < numInst; ++k)
    {
        panels[(size_t) k] = std::make_unique<InstPanel> (proc, k);
        addChildComponent (*panels[(size_t) k]);
        panels[(size_t) k]->setBounds (0, 0, 312, 278);
        panels[(size_t) k]->setInterceptsMouseClicks (false, true);
    }
    addAndMakeVisible (dice);
    dice.setBounds (256, 118, 46, 36);
    dice.onClick = [this] { proc.randomizeInstrument (proc.uiSel.load()); };
    tick();
}

void SoundPanel::tick()
{
    const int s = juce::jlimit (0, numInst - 1, proc.uiSel.load());
    if (s != sel)
    {
        sel = s;
        for (int k = 0; k < numInst; ++k) panels[(size_t) k]->setVisible (k == sel);
        waveKey.clear();
    }
    juce::String key;
    for (auto& n : id::instParams) key << param (proc, id::i (sel, n)) << ",";
    key << param (proc, id::who (sel)) << param (proc, id::vamount);
    if (key != waveKey)
    {
        waveKey = key;
        renderWave();
        repaint();
    }
}

// a forma da batida (som A, força máxima), como no visor de um sampler: mínimo e máximo por coluna
void SoundPanel::renderWave()
{
    const auto s = soundFromParams (proc, sel);
    waveMs = juce::jlimit (60.0f, 1200.0f, s.decMs * 1.3f + 30.0f);
    const double fs = 16000.0;
    const int n = (int) (waveMs * 0.001f * fs);
    DrumVoice v;
    v.prepare (fs);
    v.trigger (s, 1.0f);
    const int cols = 284;
    wave.assign ((size_t) cols * 2, 0.0f);
    float peak = 1.0e-4f;
    for (int i = 0; i < n; ++i)
    {
        float l = 0.0f, r = 0.0f;
        v.render (l, r);
        const float x = (l + r) * 0.5f;
        const auto c = (size_t) juce::jlimit (0, cols - 1, i * cols / n);
        wave[c * 2] = juce::jmin (wave[c * 2], x);
        wave[c * 2 + 1] = juce::jmax (wave[c * 2 + 1], x);
        peak = juce::jmax (peak, std::abs (x));
    }
    for (auto& w : wave) w /= peak;
}

void SoundPanel::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (all.translated (0.0f, 2.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232326), 0.0f, 0.0f, juce::Colour (0xff18181a), 0.0f, all.getBottom(), false));
    g.fillRoundedRectangle (all, 4.0f);
    g.setColour (juce::Colour (0xff333333));
    g.drawRoundedRectangle (all.reduced (0.5f), 4.0f, 1.0f);

    // LCD verde
    const RF lcd (10.0f, 10.0f, 292.0f, 92.0f);
    g.setColour (juce::Colour (0xff444444));
    g.fillRoundedRectangle (lcd.expanded (4.0f), 5.0f);
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillRoundedRectangle (lcd.expanded (3.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (gv::lcd0, 0.0f, lcd.getY(), gv::lcd1, 0.0f, lcd.getBottom(), false));
    g.fillRoundedRectangle (lcd, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.05f));
    for (float y = lcd.getY(); y < lcd.getBottom(); y += 2.0f) g.fillRect (lcd.getX(), y, lcd.getWidth(), 1.0f);
    for (float x = lcd.getX(); x < lcd.getRight(); x += 2.0f) g.fillRect (x, lcd.getY(), 1.0f, lcd.getHeight());

    const int model = juce::jlimit (0, 4, juce::roundToInt (param (proc, id::i (sel, id::model))));
    g.setFont (gv::mono (11.0f));
    g.setColour (gv::lcdInk);
    g.drawText (juce::String (sel + 1).paddedLeft ('0', 2) + " " + u8 (instNames[sel]), RF (lcd.getX() + 8.0f, lcd.getY() + 4.0f, 150.0f, 14.0f),
                juce::Justification::centredLeft, false);
    g.drawText (u8 ("◂ ") + u8 (modelLong[model]) + u8 (" ▸"), RF (lcd.getRight() - 140.0f, lcd.getY() + 4.0f, 132.0f, 14.0f),
                juce::Justification::centredRight, false);

    // a batida
    const RF wv (lcd.getX() + 6.0f, lcd.getY() + 22.0f, 280.0f, 50.0f);
    g.setColour (gv::lcdInk.withAlpha (0.3f));
    for (float x = wv.getX(); x < wv.getRight(); x += 6.0f) g.fillRect (x, wv.getCentreY(), 3.0f, 1.0f);
    g.setColour (gv::lcdInk);
    const int cols = (int) wave.size() / 2;
    for (int c = 0; c < cols; ++c)
    {
        const float x = wv.getX() + wv.getWidth() * (float) c / (float) cols;
        const float y0 = wv.getCentreY() - wave[(size_t) c * 2 + 1] * wv.getHeight() * 0.48f;
        const float y1 = wv.getCentreY() - wave[(size_t) c * 2] * wv.getHeight() * 0.48f;
        g.fillRect (x, y0, 1.0f, juce::jmax (1.0f, y1 - y0));
    }

    g.setFont (gv::mono (9.5f));
    g.drawText (u8 (modelHint[model]), RF (lcd.getX() + 8.0f, lcd.getBottom() - 17.0f, 160.0f, 13.0f), juce::Justification::centredLeft, false);
    juce::String right = juce::String (juce::roundToInt (waveMs)) + " ms";
    if (param (proc, id::who (sel)) > 0.5f)
        right = "VARIA " + juce::String (juce::roundToInt (param (proc, id::vamount))) + u8 ("% · ") + right;
    g.drawText (right, RF (lcd.getRight() - 150.0f, lcd.getBottom() - 17.0f, 142.0f, 13.0f), juce::Justification::centredRight, false);
}

// =====================================================================
// ARRASTAR MIDI
// =====================================================================
void MidiDragButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true;
    const auto file = proc.writeMidiFile();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (file.getFullPathName()), false, this);
}

// =====================================================================
// EDITOR
// =====================================================================
GroovadorEditor::GroovadorEditor (GroovadorProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, gv::baseW, gv::baseH);
    addAndMakeVisible (canvas);

    auto& st = proc.apvts;
    using gv::GKnob;
    using gv::RubberButton;

    // ---- cabeçalho: preset ----
    presetName = add<gv::PresetDisplay> ({ 788, 26, 154, 42 });
    presetName->onClick = [this] { showPresetMenu(); };
    auto* prev = add<RubberButton> ({ 948, 28, 34, 40 }, juce::String(), RubberButton::Icon::prev);
    auto* next = add<RubberButton> ({ 986, 28, 34, 40 }, juce::String(), RubberButton::Icon::next);
    auto* save = add<RubberButton> ({ 1024, 28, 34, 40 }, juce::String(), RubberButton::Icon::plus);
    prev->onClick = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    next->onClick = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    save->onClick = [this] { askNewPreset(); };

    // ---- instrumentos, grade e som ----
    for (int k = 0; k < numInst; ++k)
        lanes[(size_t) k] = add<LaneRow> ({ 22, 114 + k * 30, 250, 28 }, proc, k);
    groove = add<GrooveView> ({ 284, 112, 572, 278 }, proc);
    sound  = add<SoundPanel> ({ 868, 112, 312, 278 }, proc);

    // ---- gerador de groove ----
    add<gv::RedChoice> ({ 22, 442, 100, 30 }, st, id::style);
    add<gv::RedChoice> ({ 128, 442, 56, 30 }, st, id::meter);
    add<gv::RedChoice> ({ 190, 442, 56, 30 }, st, id::bars);
    add<gv::RedChoice> ({ 252, 442, 56, 30 }, st, id::rate);
    add<GKnob> ({ 314, 418, 56, 64 }, st, id::density, "DENSIDADE", gv::red);
    add<GKnob> ({ 370, 418, 56, 64 }, st, id::swing,   "SWING",     gv::orange);
    add<GKnob> ({ 426, 418, 56, 64 }, st, id::ghost,   "GHOST",     gv::yellow);
    add<GKnob> ({ 482, 418, 56, 64 }, st, id::roll,    "ROLL",      gv::yellow);
    add<GKnob> ({ 538, 418, 56, 64 }, st, id::human,   "HUMANO",    gv::cream);

    // ---- variação de timbre ----
    add<gv::LedSegmented> ({ 600, 440, 190, 38 }, st, id::every);
    add<gv::LedSegmented> ({ 802, 440, 250, 38 }, st, id::vmode);
    add<GKnob> ({ 1076, 414, 90, 64 }, st, id::vamount, "QUANTO", gv::orange, 24.0f);
    for (int k = 0; k < numInst; ++k)
    {
        auto* t = add<gv::LedToggle> ({ 600 + k * 57, 490, 53, 38 }, u8 (instShort[k]));
        attach (*t, id::who (k));
    }
    add<gv::LedSegmented> ({ 1066, 490, 114, 38 }, st, id::fill);

    // ---- sorteios ----
    struct Part { const char* text; int part; const juce::String* lockId; int x, w; };
    const Part parts[] = { { "RITMO", GroovadorProcessor::partRhythm, &id::lockRhythm, 22, 128 },
                           { "TIMBRES", GroovadorProcessor::partTimbres, &id::lockTimbre, 156, 128 },
                           { "FILLS", GroovadorProcessor::partFills, &id::lockFills, 290, 128 },
                           { "VELOCITY", GroovadorProcessor::partVelocity, &id::lockVel, 424, 136 } };
    for (const auto& pt : parts)
    {
        auto* b = add<RubberButton> ({ pt.x, 544, pt.w, 44 }, juce::String (pt.text), RubberButton::Icon::dice);
        b->rightPad = 24;
        const int part = pt.part;
        b->onClick = [this, part] { proc.generate (part, false); };
        auto* lk = add<gv::LockButton> ({ pt.x + pt.w - 32, 552, 24, 24 });
        attach (*lk, *pt.lockId);
    }
    auto* gen = add<RubberButton> ({ 570, 538, 222, 54 }, "GERAR GROOVE", RubberButton::Icon::play);
    gen->hero = true;
    gen->onClick = [this] { proc.generate (GroovadorProcessor::partAll, true); };
    undoBtn = add<RubberButton> ({ 802, 544, 84, 44 }, juce::String::fromUTF8 ("DESF."), RubberButton::Icon::undo);
    undoBtn->onClick = [this] { proc.undo(); };
    add<MidiDragButton> ({ 894, 544, 132, 44 }, proc);
    add<gv::LedSegmented> ({ 1036, 542, 144, 40 }, st, id::play);

    // ---- efeitos ----
    add<gv::RedChoice> ({ 22, 652, 80, 30 }, st, id::drvType);
    add<GKnob> ({ 112, 630, 62, 70 }, st, id::fxDrive, "DRIVE", gv::red, 34.0f);
    add<GKnob> ({ 178, 630, 62, 70 }, st, id::fxTone,  "TONE",  gv::red, 34.0f);

    auto* dl = add<gv::Rocker> ({ 318, 646, 50, 36 });
    attach (*dl, id::dlOn);
    add<GKnob> ({ 376, 630, 62, 70 }, st, id::dlDiv, "TIME",     gv::orange, 34.0f);
    add<GKnob> ({ 440, 630, 62, 70 }, st, id::dlFb,  "FEEDBACK", gv::orange, 34.0f);
    add<GKnob> ({ 504, 630, 62, 70 }, st, id::dlMix, "MIX",      gv::orange, 34.0f);

    auto* rv = add<gv::Rocker> ({ 672, 646, 50, 36 });
    attach (*rv, id::rvOn);
    add<GKnob> ({ 730, 630, 62, 70 }, st, id::rvSize, "SIZE", gv::yellow, 34.0f);
    add<GKnob> ({ 794, 630, 62, 70 }, st, id::rvMix,  "MIX",  gv::yellow, 34.0f);

    add<GKnob> ({ 1004, 630, 70, 70 }, st, id::volume, "VOLUME", gv::cream, 34.0f);

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) gv::baseW * gv::minScale), juce::roundToInt ((float) gv::baseH * gv::minScale),
                     juce::roundToInt ((float) gv::baseW * gv::maxScale), juce::roundToInt ((float) gv::baseH * gv::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) gv::baseW / (double) gv::baseH);
    setScale (openScale);
    startTimerHz (30);
    ativacao.anexar (*this);
}

GroovadorEditor::~GroovadorEditor()
{
    dialog.close();
    stopTimer();
    setLookAndFeel (nullptr);
}

void GroovadorEditor::attach (juce::Button& b, const juce::String& paramId)
{
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramId, b));
}

// ---------------- menu de presets (com o tamanho da janela) ----------------
void GroovadorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto names = proc.getPresetNames();
    const int  cur = proc.getPresetIndex();

    std::array<juce::PopupMenu, numCategories> cats;
    std::array<bool, numCategories> catTicked {};
    juce::PopupMenu user;
    bool userTicked = false;
    for (int i = 0; i < names.size() && i + 1 < sizeMenuBase; ++i)
    {
        const int  c = proc.getPresetCategory (i);
        const bool ticked = i == cur;
        if (proc.isUserEntry (i))
        {
            user.addItem (i + 1, names[i], true, ticked);
            userTicked = userTicked || ticked;
        }
        else if (c < 0)
            menu.addItem (i + 1, names[i], true, ticked);
        else
        {
            const auto q = (size_t) juce::jlimit (0, numCategories - 1, c);
            cats[q].addItem (i + 1, names[i], true, ticked);
            catTicked[q] = catTicked[q] || ticked;
        }
    }
    menu.addSeparator();
    juce::PopupMenu bank;
    bool bankTicked = false;
    for (int c = 0; c < numCategories; ++c)
        if (cats[(size_t) c].getNumItems() > 0)
        {
            bank.addSubMenu (u8 (categoryNames[c]), cats[(size_t) c], true, std::unique_ptr<juce::Drawable>(), catTicked[(size_t) c]);
            bankTicked = bankTicked || catTicked[(size_t) c];
        }
    menu.addSubMenu ("FACTORY 1", bank, true, std::unique_ptr<juce::Drawable>(), bankTicked);
    if (user.getNumItems() > 0)
        menu.addSubMenu ("USER", user, true, std::unique_ptr<juce::Drawable>(), userTicked);
    else
        menu.addItem (menuNoUser, "USER (nenhum ainda)", false);

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
   #ifdef JucePlugin_VersionString
    menu.addSeparator();
    menu.addItem (sizeMenuBase - 1, juce::String ("GROOVADOR ") + JucePlugin_VersionString, false, false);
   #endif

    juce::Component::SafePointer<GroovadorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetName).withParentComponent (this), [safe] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->presetMenuChosen (result);
    });
}

void GroovadorEditor::askNewPreset()
{
    juce::Component::SafePointer<GroovadorEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void GroovadorEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<GroovadorEditor> safe (this);
    const auto name = proc.getPresetName();
    switch (result)
    {
        case menuSaveAs:    askNewPreset(); break;
        case menuLicenca:   ativacao.mostrarInfo(); break;
        case menuOverwrite: proc.overwriteUserPreset(); break;
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
        case menuNoUser:    break;
        default:
            if (result > 0 && result < sizeMenuBase)
                proc.loadPreset (result - 1);
            break;
    }
}

// ---------------- timer (30 Hz) ----------------
void GroovadorEditor::timerCallback()
{
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) gv::baseW - openScale) > 0.005f)
        setScale (openScale);

    for (auto* l : lanes) l->tick();
    groove->repaint();
    sound->tick();
    undoBtn->setEnabled (proc.canUndo());
    undoBtn->setAlpha (proc.canUndo() ? 1.0f : 0.45f);

    const int sel = proc.uiSel.load();
    if (sel != lastSel)
    {
        lastSel = sel;
        canvas.repaint (860, 90, 330, 22);
    }

    level = juce::jmax (proc.meterOut.exchange (0.0f), level * 0.85f);
    if (std::abs (level - shownLevel) > 0.002f)
    {
        shownLevel = level;
        canvas.repaint (1062, 26, 124, 40);
    }

    presetName->set (proc.getPresetName());
    if (proc.getPresetIndex() != lastPreset)
    {
        lastPreset = proc.getPresetIndex();
        canvas.repaint (726, 24, 60, 46);
    }

    const float bpm = proc.shownBpm.load();
    const bool pads = param (proc, id::play) > 0.5f;
    const int state = pads ? 2 : proc.synced.load() ? 1 : 0;
    if (std::abs (bpm - lastBpm) > 0.05f || state != lastState)
    {
        lastBpm = bpm;
        lastState = state;
        canvas.repaint (580, 10, 145, 62);
        canvas.repaint (600, 90, 260, 22);
    }
}

// ---------------- tamanho ----------------
void GroovadorEditor::setScale (float scale)
{
    scale = juce::jlimit (gv::minScale, gv::maxScale, scale);
    setSize (juce::roundToInt ((float) gv::baseW * scale), juce::roundToInt ((float) gv::baseH * scale));
}

void GroovadorEditor::resized()
{
    const float scale = (float) getWidth() / (float) gv::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)
        proc.uiScale = scale;
}

void GroovadorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

// ---------------- o painel ----------------
void GroovadorEditor::drawCanvas (juce::Graphics& g)
{
    const float W = (float) gv::baseW, H = (float) gv::baseH;

    // chassi preto fosco com linhas horizontais bem fracas
    g.setGradientFill (juce::ColourGradient (gv::body0, 0.0f, 0.0f, gv::body1, 0.0f, H, false));
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (float y = 90.0f; y < H; y += 3.0f) g.fillRect (0.0f, y, W, 1.0f);
    g.setColour (juce::Colour (0xff333333));
    g.drawRoundedRectangle (RF (0.5f, 0.5f, W - 1.0f, H - 1.0f), 6.0f, 1.0f);

    drawHeader (g);

    gv::screw (g, 13.0f, 13.0f);
    gv::screw (g, W - 13.0f, 13.0f);
    gv::screw (g, 13.0f, H - 13.0f);
    gv::screw (g, W - 13.0f, H - 13.0f);

    gv::section (g, 22.0f, 94.0f, 250.0f, "INSTRUMENTOS");
    gv::section (g, 284.0f, 94.0f, 572.0f, "GROOVE");
    gv::section (g, 868.0f, 94.0f, 312.0f, u8 ("SOM · ") + u8 (instNames[juce::jlimit (0, numInst - 1, proc.uiSel.load())]));

    // estado do relógio, na linha do GROOVE
    {
        const bool pads = param (proc, id::play) > 0.5f;
        const bool syn = proc.synced.load();
        juce::String st = pads ? u8 ("MODO PADS · TOQUE PELO MIDI") : syn ? juce::String ("SYNC COM O DAW") : juce::String ("AGUARDANDO PLAY");
        g.setColour (gv::body0);
        g.fillRect (640.0f, 94.0f, 216.0f, 17.0f);
        g.setFont (gv::sans (9.5f, true, 0.16f));
        g.setColour (! pads && ! syn ? gv::led : gv::silk.withAlpha (0.8f));
        g.drawText (st, RF (640.0f, 95.0f, 216.0f, 15.0f), juce::Justification::centredRight, false);
    }

    // ---- gerador ----
    gv::section (g, 22.0f, 404.0f, 560.0f, "GERADOR DE GROOVE");
    gv::caption (g, "ESTILO",   { 22.0f, 428.0f, 100.0f, 12.0f });
    gv::caption (g, "COMPASSO", { 124.0f, 428.0f, 64.0f, 12.0f });
    gv::caption (g, "TAMANHO",  { 186.0f, 428.0f, 64.0f, 12.0f });
    gv::caption (g, "RATE",     { 252.0f, 428.0f, 56.0f, 12.0f });

    // ---- variação ----
    gv::section (g, 600.0f, 404.0f, 580.0f, u8 ("VARIAÇÃO DE TIMBRE"));
    gv::caption (g, "MUDA A CADA (COMPASSOS)", { 600.0f, 426.0f, 200.0f, 12.0f }, juce::Justification::centredLeft);
    gv::caption (g, "MODO",                    { 802.0f, 426.0f, 200.0f, 12.0f }, juce::Justification::centredLeft);
    gv::caption (g, "QUEM VARIA",              { 600.0f, 476.0f, 200.0f, 12.0f }, juce::Justification::centredLeft);
    gv::caption (g, "FILL A CADA",             { 1066.0f, 476.0f, 114.0f, 12.0f }, juce::Justification::centredLeft);
    gv::caption (g, "TOCAR",                   { 1036.0f, 528.0f, 144.0f, 12.0f });

    g.setFont (gv::sans (9.0f, false, 0.02f));
    g.setColour (gv::dim);
    g.drawFittedText (u8 ("CADEADO = o GERAR GROOVE não mexe naquela parte · clique no nome = ouvir · M/S = mudo/solo · "
                          "PADS: C1 rim, D1 snare, E1 clap, F1 hat f., G1 hat ab., A1 shaker, B1 perc 1, C2 perc 2"),
                      juce::Rectangle<int> (22, 594, 1158, 14), juce::Justification::centredLeft, 1, 0.8f);

    // ---- efeitos ----
    gv::section (g, 22.0f, 614.0f, 270.0f, "DRIVE");
    gv::caption (g, "TIPO", { 22.0f, 638.0f, 80.0f, 12.0f });
    gv::section (g, 316.0f, 614.0f, 330.0f, "DELAY PING-PONG");
    gv::section (g, 670.0f, 614.0f, 300.0f, "REVERB");
    gv::section (g, 1004.0f, 614.0f, 176.0f, u8 ("SAÍDA"));
}

void GroovadorEditor::drawHeader (juce::Graphics& g)
{
    const float W = (float) gv::baseW;

    // faixa de alumínio escovado
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd4d3cd), 0.0f, 0.0f, juce::Colour (0xffa9a8a2), 0.0f, 82.0f, false));
    g.fillRect (0.0f, 0.0f, W, 82.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float x = 0.0f; x < W; x += 3.0f) g.fillRect (x, 0.0f, 1.0f, 82.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, 90.0f, juce::Colours::transparentBlack, 0.0f, 98.0f, false));
    g.fillRect (0.0f, 90.0f, W, 8.0f);

    // listra de 4 cores
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (gv::keyColour (i));
        g.fillRect (W * 0.25f * (float) i, 82.0f, W * 0.25f + 1.0f, 8.0f);
    }

    // GROOVA + DOR
    g.setFont (gv::sans (42.0f, true, 0.08f));
    const float gw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "GROOVA");
    g.setColour (juce::Colour (0xff151515));
    g.drawText ("GROOVA", RF (28.0f, 8.0f, gw + 6.0f, 46.0f), juce::Justification::centredLeft, false);
    g.setColour (gv::red);
    g.drawText ("DOR", RF (28.0f + gw, 8.0f, 140.0f, 46.0f), juce::Justification::centredLeft, false);
    g.setFont (gv::sans (10.0f, true, 0.35f));
    g.setColour (juce::Colour (0xff3c3b37));
    g.drawText (u8 ("GR-145 · RHYTHM COMPOSER"), RF (30.0f, 54.0f, 280.0f, 14.0f), juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff2b2a27));
    g.fillRect (330.0f, 20.0f, 2.0f, 36.0f);
    g.setFont (gv::sans (10.0f, true, 0.25f));
    g.drawText (u8 ("PERCUSSÃO SINTETIZADA"), RF (344.0f, 20.0f, 230.0f, 16.0f), juce::Justification::centredLeft, false);
    g.drawText ("GERADOR DE GROOVE E TIMBRES", RF (344.0f, 38.0f, 230.0f, 16.0f), juce::Justification::centredLeft, false);
    g.setFont (gv::sans (8.0f, true, 0.2f));
    g.setColour (juce::Colour (0xff55544f));
    g.drawText ("BY InsideStudioSP", RF (344.0f, 56.0f, 230.0f, 12.0f), juce::Justification::centredLeft, false);

    // TEMPO, SYNC e PADRÃO
    auto lab = [&g] (const juce::String& t, float x)
    {
        g.setFont (gv::sans (8.5f, true, 0.2f));
        g.setColour (juce::Colour (0xff2b2a27));
        g.drawText (t, RF (x, 12.0f, 80.0f, 12.0f), juce::Justification::centredLeft, false);
    };
    lab ("TEMPO", 586.0f);
    const RF bpmR (586.0f, 28.0f, 96.0f, 38.0f);
    gv::displayWell (g, bpmR);
    const float bpm = proc.shownBpm.load();
    gv::sevenSeg (g, bpmR, juce::String (bpm, 1), bpm >= 100.0f ? 14.5f : 18.0f);

    lab ("SYNC", 690.0f);
    const bool pads = param (proc, id::play) > 0.5f;
    g.setColour (juce::Colour (0xff3a3936));
    g.fillEllipse (695.0f, 40.0f, 14.0f, 14.0f);
    gv::ledDot (g, 702.0f, 47.0f, 5.0f, proc.synced.load() && ! pads);

    lab (u8 ("PADRÃO"), 730.0f);
    const RF numR (730.0f, 28.0f, 52.0f, 38.0f);
    gv::displayWell (g, numR);
    const int pi = proc.getPresetIndex();
    gv::sevenSeg (g, numR, pi < 0 ? juce::String ("--") : juce::String ((pi + 1) % 100).paddedLeft ('0', 2), 15.0f);

    // medidor de saída
    const RF m (1066.0f, 30.0f, 110.0f, 30.0f);
    g.setColour (juce::Colour (0xffe8e7e1));
    g.fillRoundedRectangle (m.translated (0.0f, 1.0f), 3.0f);
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillRoundedRectangle (m, 3.0f);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float v = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -42.0f, 0.0f, 0.0f, 1.0f));
    for (int i = 0; i < 10; ++i)
    {
        const float f = (float) i / 9.0f;
        const bool on = level > 0.0005f && f <= v;
        const auto c = i < 6 ? juce::Colour (0xff3dff4a) : i < 8 ? gv::yellow : gv::led;
        const RF b (m.getX() + 7.0f + (float) i * 10.0f, m.getY() + 7.0f, 7.0f, 16.0f);
        if (on)
        {
            g.setColour (c.withAlpha (0.3f));
            g.fillRoundedRectangle (b.expanded (1.5f), 2.0f);
        }
        g.setColour (on ? c : juce::Colour (0xff222222));
        g.fillRoundedRectangle (b, 1.0f);
    }
}
