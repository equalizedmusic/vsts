#include "PluginEditor.h"

using namespace repeteco;

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

// eixo de frequência em log, 20 Hz–20 kHz
float freqX (float hz, float x0, float x1)
{
    return x0 + (std::log10 (juce::jmax (20.0f, hz)) - std::log10 (20.0f)) / 3.0f * (x1 - x0);
}

float param (RepetecoProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

// os ajustes como o motor vê (para os textos de tempo)
Settings uiSettings (RepetecoProcessor& p)
{
    Settings s;
    s.bpm       = p.shownBpm.load();
    s.time      = param (p, id::time) * 0.01f;
    s.longRange = juce::roundToInt (param (p, id::range)) == 1;
    s.free      = juce::roundToInt (param (p, id::sync)) == 1;
    return s;
}

// pico do que entrou nos baldes 'age' segundos atrás
float histAt (RepetecoProcessor& p, double age)
{
    const int w    = p.engine.histWrite.load (std::memory_order_acquire);
    const int back = juce::jlimit (0, histLen - 1, (int) std::round (age / juce::jmax (1.0e-4f, p.engine.histStepSec.load())));
    return p.engine.hist[(size_t) ((w - 1 - back + 2 * histLen) % histLen)].load (std::memory_order_relaxed);
}

// escala comum dos desenhos: o maior pico dos últimos segundos
float histNorm (RepetecoProcessor& p)
{
    float m = 0.0f;
    for (auto& h : p.engine.hist) m = juce::jmax (m, h.load (std::memory_order_relaxed));
    return juce::jmax (0.02f, m);
}

// mais velho = mais escuro: âmbar → coral → marrom
juce::Colour repeatColour (float age01)
{
    const juce::Colour brown (0xff8a4a3a);
    if (age01 < 0.5f) return theme::amber.interpolatedWith (theme::coral, age01 * 2.0f);
    return theme::coral.interpolatedWith (brown, (age01 - 0.5f) * 2.0f);
}
} // namespace

// =====================================================================
// REPETIÇÕES
// =====================================================================
void RepeatsStrip::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    const auto  s = uiSettings (proc);
    const bool  inf = param (proc, id::infinite) > 0.5f;
    const float fb  = inf ? 1.0f : proc.engine.fbNow.load();
    const float dk  = param (proc, id::dark) * 0.01f;
    const double delay = proc.engine.delayNow.load();

    real::card (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, u8 ("REPETIÇÕES "), u8 ("· ") + timeName (s) + u8 (" · ")
                                                    + (inf ? juce::String ("INFINITO") : juce::String (juce::roundToInt (param (proc, id::feedback))) + "%"));
    {
        juce::AttributedString a;
        a.append (juce::String (juce::roundToInt (delay * 1000.0)) + " ms", theme::mono (10.0f), theme::cream);
        a.append (u8 (" · ESCURECE A CADA VOLTA"), theme::mono (10.0f), theme::dim);
        a.setJustification (juce::Justification::centredRight);
        a.draw (g, { W - 400.0f, 6.0f, 388.0f, 14.0f });
    }

    const float x0 = 12.0f, x1 = W - 12.0f, y0 = 26.0f, y1 = H - 20.0f;
    const juce::Rectangle<float> scr (x0, y0, x1 - x0, y1 - y0);
    real::screen (g, scr, 3.0f);

    const int   bars = 14;
    const float step = (x1 - x0 - 20.0f) / (float) bars;
    const float norm = histNorm (proc);
    auto yOf = [y0, y1] (float v) { return y1 - juce::jlimit (0.0f, 1.0f, v) * (y1 - y0 - 8.0f); };

    // envelope teórico: cada volta multiplica pela realimentação
    {
        juce::Path env;
        for (int i = 0; i < bars; ++i)
        {
            const float x = x0 + 16.0f + (float) i * step + step * 0.3f;
            const float y = yOf (juce::jmin (1.0f, std::pow (fb, (float) i)) * 0.9f);
            if (i == 0) env.startNewSubPath (x, y); else env.lineTo (x, y);
        }
        g.setColour (theme::coral.withAlpha (0.5f));
        g.strokePath (env, juce::PathStrokeType (1.0f));
    }

    g.setFont (theme::mono (9.0f));
    for (int i = 0; i < bars; ++i)
    {
        // o que entrou i voltas atrás, já multiplicado i vezes pela realimentação
        const float a    = juce::jmin (1.0f, std::pow (fb, (float) i));
        const float live = juce::jmin (1.0f, histAt (proc, delay * i) / norm);
        const float h    = a * (0.08f + 0.92f * live) * 0.9f;
        const float x    = x0 + 16.0f + (float) i * step;
        const float age  = juce::jlimit (0.0f, 1.0f, (float) i / (float) (bars - 1) * (0.5f + dk));
        const float blur = 2.0f + (float) i * 1.6f * (0.3f + dk);
        const auto  col  = i == 0 ? theme::cream : repeatColour (age);
        const float bw   = step * 0.6f;

        juce::ColourGradient grad (col.withAlpha (0.0f), x - blur, 0.0f, col.withAlpha (0.0f), x + bw + blur, 0.0f, false);
        grad.addColour (blur / (bw + 2.0f * blur), col);
        grad.addColour (1.0f - blur / (bw + 2.0f * blur), col);
        g.setGradientFill (grad);
        g.setOpacity (0.3f + 0.7f * a);
        g.fillRect (x - blur, yOf (h), bw + 2.0f * blur, y1 - yOf (h));
        g.setOpacity (1.0f);

        if (i > 0)
        {
            g.setColour (theme::dim);
            g.drawText (juce::String (i), juce::Rectangle<float> (x, y1 + 3.0f, bw, 12.0f), juce::Justification::centred);
        }
    }

    real::glass (g, scr, 3.0f);
}

// =====================================================================
// BALDES
// =====================================================================
void BucketView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth();
    const double delay = proc.engine.delayNow.load();
    const bool  inf = param (proc, id::infinite) > 0.5f;
    const float fb  = inf ? 1.0f : proc.engine.fbNow.load();
    const float norm = histNorm (proc);

    const auto scr = juce::Rectangle<float> (0.0f, 0.0f, S, S).reduced (1.5f);   // o rebaixo desenha 1 px para fora
    real::screen (g, scr, 4.0f);

    // 30 baldes em zigue-zague; o primeiro acabou de receber, o último está saindo
    const int cols = 6, rows = 5;
    const float bw = 26.0f, bh = 24.0f, gx = 36.0f, gy = 40.0f, ox = 22.0f, oy = 22.0f;
    const int total = cols * rows;
    for (int rr = 0; rr < rows; ++rr)
        for (int c = 0; c < cols; ++c)
        {
            const int   k  = rr * cols + c;
            const int   cc = (rr % 2) ? cols - 1 - c : c;
            const float x  = ox + (float) cc * gx, y = oy + (float) rr * gy;
            const float lvl = juce::jlimit (0.0f, 1.0f, histAt (proc, delay * (double) k / (double) (total - 1)) / norm);

            juce::Path cup;
            cup.startNewSubPath (x, y);
            cup.lineTo (x + 3.0f, y + bh);
            cup.lineTo (x + bw - 3.0f, y + bh);
            cup.lineTo (x + bw, y);
            g.setColour (theme::off);
            g.strokePath (cup, juce::PathStrokeType (2.0f));

            const float fillH = juce::jmax (1.5f, bh * lvl * 0.9f);
            g.setColour (repeatColour ((float) k / (float) (total - 1)).withAlpha (0.35f + 0.6f * std::pow (0.97f, (float) k)));
            g.fillRect (x + 4.0f, y + bh - fillH, bw - 8.0f, fillH - 1.0f);

            // ligação para o próximo balde
            if (k < total - 1)
            {
                g.setColour (theme::segOff);
                if (c < cols - 1)
                {
                    const float xa = (rr % 2) ? x : x + bw, xb = (rr % 2) ? x - gx + bw : x + gx;
                    g.fillRect (juce::jmin (xa, xb), y + 6.0f, std::abs (xb - xa), 1.0f);
                }
                else
                    g.fillRect (x + bw * 0.5f, y + bh + 2.0f, 1.0f, gy - bh - 4.0f);
            }
        }

    // realimentação: a saída volta para o primeiro balde
    {
        const float xe = ox + bw * 0.5f, ye = oy + (float) (rows - 1) * gy + bh + 4.0f;
        juce::Path back;
        back.startNewSubPath (xe, ye);
        back.lineTo (xe, 232.0f);
        back.lineTo (8.0f, 232.0f);
        back.lineTo (8.0f, oy + 8.0f);
        back.lineTo (ox - 2.0f, oy + 8.0f);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (dashed, back, dashes, 2);
        if (fb > 1.0f || inf)
        {
            g.setColour (theme::coral.withAlpha (0.25f));
            g.strokePath (back, juce::PathStrokeType (6.0f));
        }
        g.setColour (theme::coral.withAlpha (0.2f + 0.8f * juce::jmin (1.0f, fb)));
        g.fillPath (dashed);
    }

    real::glass (g, scr, 4.0f);

    // rodapé: tempo e o relógio que um BBD de 1024 estágios precisaria
    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    const double clk = (double) bbdStages / (2.0 * juce::jmax (0.001, delay));
    auto sl = line ("TIME ", timeName (uiSettings (proc)));
    auto sr = line (u8 ("RELÓGIO "), clk >= 1000.0 ? juce::String (clk / 1000.0, 1).replaceCharacter ('.', ',') + " kHz"
                                                   : juce::String (juce::roundToInt (clk)) + " Hz");
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
    sr.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
}

// =====================================================================
// EDITOR
// =====================================================================
RepetecoEditor::RepetecoEditor (RepetecoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), buckets (p),
      // cor do knob = cor do que ele mexe no desenho
      timeMacro   (p.apvts, id::time,     "TIME",              true,  theme::amber),
      range       (p.apvts, id::range,    "RANGE",             false, theme::amber),
      sync        (p.apvts, id::sync,     "SYNC",              false, theme::amber),
      drive       (p.apvts, id::drive,    "DRIVE",             false, theme::cream),
      pingPong    (p.apvts, id::pingPong, "PING-PONG",         false, theme::cream),
      fbMacro     (p.apvts, id::feedback, "FEEDBACK",          true,  theme::coral),
      lfoRate     (p.apvts, id::lfoRate,  "LFO RATE",          false, theme::violet),
      lfoAmt      (p.apvts, id::lfoAmt,   "LFO AMOUNT",        false, theme::violet),
      lfoShape    (p.apvts, id::lfoShape, "SHAPE",             false, theme::violet),
      dark        (p.apvts, id::dark,     "ESCURO",            false, theme::coral),
      output      (p.apvts, id::output,   "GAIN",           false, theme::cream),
      mix         (p.apvts, id::mix),
      infiniteKey ("INFINITO", theme::amber),
      killKey     ("MATAR", theme::coral)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &buckets, &timeMacro, &range, &sync, &drive, &pingPong, &fbMacro, &lfoRate, &lfoAmt, &lfoShape, &dark,
             &output, &mix, &infiniteKey, &killKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    infiniteAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::infinite, infiniteKey);
    killAtt     = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::kill, killKey);
    bypassAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

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

RepetecoEditor::~RepetecoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RepetecoEditor::showPresetMenu()
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

    juce::Component::SafePointer<RepetecoEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void RepetecoEditor::askNewPreset()
{
    juce::Component::SafePointer<RepetecoEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void RepetecoEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<RepetecoEditor> safe (this);
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

void RepetecoEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) ui::baseW - openScale) > 0.005f)
        setScale (openScale);

    strip.repaint();
    buckets.repaint();

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

void RepetecoEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void RepetecoEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void RepetecoEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void RepetecoEditor::drawCanvas (juce::Graphics& g)
{
    real::chassis (g, (float) ui::baseW, (float) ui::baseH);

    // ---- logo: balde com carga ----
    g.setColour (theme::amber.withAlpha (0.5f));
    g.drawEllipse (17.0f, 20.0f, 28.0f, 28.0f, 1.4f);
    {
        juce::Path cup;
        cup.startNewSubPath (24.0f, 27.0f);
        cup.lineTo (38.0f, 27.0f);
        cup.lineTo (36.0f, 40.0f);
        cup.lineTo (26.0f, 40.0f);
        cup.closeSubPath();
        g.setColour (theme::amber);
        g.strokePath (cup, juce::PathStrokeType (2.0f));
        juce::Path charge;
        charge.startNewSubPath (26.5f, 34.0f);
        charge.lineTo (35.5f, 34.0f);
        charge.lineTo (35.0f, 38.5f);
        charge.lineTo (27.0f, 38.5f);
        charge.closeSubPath();
        g.setColour (theme::coral);
        g.fillPath (charge);
    }

    const float brandEnd = drawBrand (g, "REPETECO", "ANALOG DELAY", u8 ("BBD · ECO ESCURO"));
    drawSync (g, proc.shownBpm.load(), proc.synced.load(), proc.waiting.load(), brandEnd);

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "BALDES ", u8 ("· 1024 ESTÁGIOS"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "TIME");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "FEEDBACK");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 110.0f }, "MACRO TIME",
              u8 ("Distância entre as repetições, no grid do host. Mexer com som passando estica o eco."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 110.0f }, "MACRO FEEDBACK",
              u8 ("Quantas vezes o eco volta. Passa de 100% e entra em auto-oscilação."));
}

void RepetecoEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    buckets.setBounds (22, 246, 240, 264);

    timeMacro.setBounds (294, 246, 92, 92);
    range.setBounds     (294, 376, 119, 92);
    sync.setBounds      (413, 376, 119, 92);
    drive.setBounds     (294, 468, 119, 92);
    pingPong.setBounds  (413, 468, 119, 92);

    fbMacro.setBounds  (562, 246, 92, 92);
    lfoRate.setBounds  (562, 376, 119, 92);
    lfoAmt.setBounds   (681, 376, 119, 92);
    lfoShape.setBounds (562, 468, 119, 92);
    dark.setBounds     (681, 468, 119, 92);

    infiniteKey.setBounds (830, 246, 71, 40);
    killKey.setBounds     (907, 246, 71, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);
}
