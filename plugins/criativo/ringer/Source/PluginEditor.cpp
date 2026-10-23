#include "PluginEditor.h"

using namespace ringer;

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

float param (RingerProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

// lê a janela mais recente do osciloscópio (do mais antigo para o mais novo)
void readScope (RingerProcessor& p, std::vector<float>& in, std::vector<float>& car, std::vector<float>& out, int count)
{
    in.resize ((size_t) count);
    car.resize ((size_t) count);
    out.resize ((size_t) count);
    const int w = p.engine.scWrite.load (std::memory_order_acquire);
    for (int k = 0; k < count; ++k)
    {
        const auto idx = (size_t) ((w - count + k + scopeLen) % scopeLen);
        in[(size_t) k]  = p.engine.scIn[idx].load (std::memory_order_relaxed);
        car[(size_t) k] = p.engine.scCar[idx].load (std::memory_order_relaxed);
        out[(size_t) k] = p.engine.scOut[idx].load (std::memory_order_relaxed);
    }
}

void legendItem (juce::Graphics& g, float x, float y, juce::Colour c, const juce::String& text)
{
    g.setColour (c);
    g.fillRect (x, y + 5.0f, 12.0f, 2.0f);
    g.setFont (theme::sans (8.0f, true, 0.18f));
    g.drawText (text, juce::Rectangle<float> (x + 16.0f, y, 120.0f, 12.0f), juce::Justification::centredLeft);
}
} // namespace

// =====================================================================
// OSCILOSCÓPIO
// =====================================================================
void ScopeStrip::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();

    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    drawTitle (g, 12.0f, 7.0f, "OSCILLOSCOPE ", u8 ("· INPUT × CARRIER"));
    {
        const float lfoA = param (proc, id::lfoAmt);
        juce::AttributedString s;
        s.append ("CARRIER ", theme::mono (10.0f), theme::dim);
        s.append (hzText (proc.engine.hzNow.load()), theme::mono (10.0f), theme::coral);
        s.append (u8 (" · LFO "), theme::mono (10.0f), theme::dim);
        s.append (divNames[juce::jlimit (0, numDivs - 1, juce::roundToInt (param (proc, id::lfoRate)))]
                      + juce::String (u8 (" ±")) + juce::String (juce::roundToInt (lfoA)) + "%",
                  theme::mono (10.0f), theme::violet);
        s.setJustification (juce::Justification::centredRight);
        s.draw (g, { W - 400.0f, 6.0f, 388.0f, 14.0f });
    }

    const float x0 = 12.0f, x1 = W - 12.0f, y0 = 26.0f, y1 = H - 22.0f;
    const float mid = (y0 + y1) * 0.5f, half = (y1 - y0) * 0.5f - 4.0f;
    g.setColour (theme::dark);
    g.fillRoundedRectangle (x0, y0, x1 - x0, y1 - y0, 3.0f);
    g.setColour (theme::ledOff);
    g.fillRect (x0, mid, x1 - x0, 1.0f);
    for (int k = 1; k < 16; ++k)
        g.fillRect (x0 + (x1 - x0) * (float) k / 16.0f, y0, 1.0f, y1 - y0);

    // ~23 ms da janela mais recente
    std::vector<float> in, car, out;
    const int count = juce::jmin (scopeLen, 512);
    readScope (proc, in, car, out, count);
    float peak = 0.0f;
    for (int k = 0; k < count; ++k) peak = juce::jmax (peak, std::abs (in[(size_t) k]), std::abs (out[(size_t) k]));
    const float target = 1.0f / juce::jmax (0.05f, peak);
    scale += (target - scale) * (target < scale ? 0.5f : 0.08f);

    auto trace = [&] (const std::vector<float>& v, float amp, juce::Colour c, float width)
    {
        juce::Path p;
        for (int k = 0; k < count; ++k)
        {
            const float x = x0 + (x1 - x0) * (float) k / (float) (count - 1);
            const float y = mid - juce::jlimit (-1.0f, 1.0f, v[(size_t) k] * amp) * half;
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width));
    };
    trace (in,  scale,  theme::dim.withAlpha (0.6f),   1.2f);
    trace (car, 0.35f,  theme::coral.withAlpha (0.55f), 1.0f);
    trace (out, scale,  theme::amber,                   1.6f);

    legendItem (g, x0 + 4.0f,   y1 + 5.0f, theme::dim,   "INPUT");
    legendItem (g, x0 + 84.0f,  y1 + 5.0f, theme::coral, "CARRIER");
    legendItem (g, x0 + 176.0f, y1 + 5.0f, theme::amber, "OUTPUT");

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// =====================================================================
// ANEL
// =====================================================================
void RingView::tick()
{
    // o anel gira mais rápido quanto mais aguda a portadora (sem virar borrão)
    spin += 0.02f * std::log2 (1.0f + proc.engine.hzNow.load());
    spin -= std::floor (spin);
    repaint();
}

void RingView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth(), C = S * 0.5f;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float lfo = proc.engine.lfoNow.load();
    const float lfoA = param (proc, id::lfoAmt) * 0.01f;

    g.setColour (theme::dark);
    g.fillEllipse (1.0f, 1.0f, S - 2.0f, S - 2.0f);

    // anel de fora: a portadora girando
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) i / 48.0f * twoPi + spin * twoPi;
        const bool lit = (i % 6) < 3;
        juce::Path tickPath;
        tickPath.startNewSubPath (C + std::sin (a) * 104.0f, C - std::cos (a) * 104.0f);
        tickPath.lineTo (C + std::sin (a) * 112.0f, C - std::cos (a) * 112.0f);
        g.setColour (lit ? theme::coral : theme::segOff);
        g.strokePath (tickPath, juce::PathStrokeType (3.0f));
    }

    // arco do LFO: quanto ele está empurrando a portadora agora
    g.setColour (theme::dim.withAlpha (0.35f));
    g.drawEllipse (C - 96.0f, C - 96.0f, 192.0f, 192.0f, 1.0f);
    {
        const float amount = juce::jlimit (-1.0f, 1.0f, lfo * lfoA);
        juce::Path arc;
        arc.addCentredArc (C, C, 96.0f, 96.0f, 0.0f, 0.0f, amount * juce::MathConstants<float>::pi, true);
        g.setColour (theme::violet.withAlpha (0.3f));
        g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (theme::violet);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Lissajous: entrada (x) × saída (y); a portadora deixa o desenho em flor
    std::vector<float> in, car, out;
    const int count = 600;
    readScope (proc, in, car, out, count);
    float peak = 0.0f;
    for (int k = 0; k < count; ++k) peak = juce::jmax (peak, std::abs (in[(size_t) k]), std::abs (out[(size_t) k]));
    const float target = 1.0f / juce::jmax (0.05f, peak);
    scale += (target - scale) * (target < scale ? 0.5f : 0.08f);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addEllipse (C - 88.0f, C - 88.0f, 176.0f, 176.0f);
        g.reduceClipRegion (clip);

        auto figure = [&] (const std::vector<float>& xs, const std::vector<float>& ys, float ampY, juce::Colour c, float width)
        {
            juce::Path p;
            for (int k = 0; k < count; ++k)
            {
                const float x = C + juce::jlimit (-1.2f, 1.2f, xs[(size_t) k] * scale) * 70.0f;
                const float y = C - juce::jlimit (-1.2f, 1.2f, ys[(size_t) k] * ampY) * 70.0f;
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (width));
        };
        figure (in, car, 1.0f,  theme::coral.withAlpha (0.35f), 1.0f);
        figure (in, out, scale, theme::amber.withAlpha (0.9f),  1.3f);
    }
    g.setColour (theme::cream);
    g.fillEllipse (C - 2.5f, C - 2.5f, 5.0f, 5.0f);

    // rodapé
    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    auto sl = line ("CARRIER ", hzText (proc.engine.hzNow.load()));
    auto sr = line ("RANGE ", juce::roundToInt (param (proc, id::range)) == 1 ? "HIGH" : "LOW");
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
    sr.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
}

// =====================================================================
// EDITOR
// =====================================================================
RingerEditor::RingerEditor (RingerProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), ring (p),
      // cor do knob = cor do que ele mexe no desenho
      freqMacro (p.apvts, id::freq,     "FREQUENCY",      true,  theme::coral),
      range     (p.apvts, id::range,    "RANGE",          false, theme::coral),
      snap      (p.apvts, id::snap,     "AFINAR",         false, theme::coral),
      drive     (p.apvts, id::drive,    "DRIVE",          false, theme::cream),
      stereo    (p.apvts, id::stereo,   "STEREO",         false, theme::cream),
      lfoMacro  (p.apvts, id::lfoAmt,   "AMOUNT",         true,  theme::violet),
      lfoRate   (p.apvts, id::lfoRate,  "RATE",           false, theme::violet),
      lfoShape  (p.apvts, id::lfoShape, "SHAPE",          false, theme::violet),
      lfoSync   (p.apvts, id::lfoSync,  "SYNC",           false, theme::violet),
      lfoPhase  (p.apvts, id::lfoPhase, "PHASE",          false, theme::violet),
      output    (p.apvts, id::output,   "GAIN",        false, theme::cream),
      mix       (p.apvts, id::mix),
      twiceKey  ("DOBRAR", theme::coral)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &ring, &freqMacro, &range, &snap, &drive, &stereo, &lfoMacro, &lfoRate, &lfoShape, &lfoSync, &lfoPhase,
             &output, &mix, &twiceKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    twiceAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::twice, twiceKey);
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

RingerEditor::~RingerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RingerEditor::showPresetMenu()
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

    juce::Component::SafePointer<RingerEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void RingerEditor::askNewPreset()
{
    juce::Component::SafePointer<RingerEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void RingerEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<RingerEditor> safe (this);
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

void RingerEditor::timerCallback()
{
    strip.repaint();
    ring.tick();

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

void RingerEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void RingerEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void RingerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void RingerEditor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // ---- logo: dois anéis entrelaçados ----
    g.setColour (theme::amber.withAlpha (0.5f));
    g.drawEllipse (17.0f, 20.0f, 28.0f, 28.0f, 1.4f);
    g.setColour (theme::amber);
    g.drawEllipse (20.5f, 27.5f, 13.0f, 13.0f, 2.0f);
    g.setColour (theme::coral);
    g.drawEllipse (28.5f, 27.5f, 13.0f, 13.0f, 2.0f);

    const float brandEnd = drawBrand (g, "RINGER", "RING MODULATOR", u8 ("CARRIER · LFO"));
    drawSync (g, proc.shownBpm.load(), proc.synced.load(), proc.waiting.load(), brandEnd);

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "ANEL ", u8 ("· LISSAJOUS"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "CARRIER");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "LFO");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 110.0f }, "MACRO FREQUENCY",
              u8 ("A nota que multiplica o som. Baixa = tremolo; alta = metálico, sino, robô."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 110.0f }, "MACRO AMOUNT",
              u8 ("Quanto o LFO balança o CARRIER. É daí que vêm sirene, borbulha e laser."));
}

void RingerEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    ring.setBounds (22, 246, 240, 264);

    freqMacro.setBounds (294, 246, 92, 92);
    range.setBounds     (294, 376, 119, 92);
    snap.setBounds      (413, 376, 119, 92);
    drive.setBounds     (294, 468, 119, 92);
    stereo.setBounds    (413, 468, 119, 92);

    lfoMacro.setBounds (562, 246, 92, 92);
    lfoRate.setBounds  (562, 376, 119, 92);
    lfoShape.setBounds (681, 376, 119, 92);
    lfoSync.setBounds  (562, 468, 119, 92);
    lfoPhase.setBounds (681, 468, 119, 92);

    twiceKey.setBounds (830, 246, 148, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);
}
