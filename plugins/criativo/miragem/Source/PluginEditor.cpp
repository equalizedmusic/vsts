#include "PluginEditor.h"

using namespace miragem;

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

float param (MiragemProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

// pico da cauda 'age' segundos atrás
float histAt (MiragemProcessor& p, double age)
{
    const int w    = p.engine.histWrite.load (std::memory_order_acquire);
    const int back = juce::jlimit (0, histLen - 1, (int) std::round (age / 0.01));
    return p.engine.hist[(size_t) ((w - 1 - back + 2 * histLen) % histLen)].load (std::memory_order_relaxed);
}

// escala comum dos desenhos: o maior pico dos últimos segundos
float histNorm (MiragemProcessor& p)
{
    float m = 0.0f;
    for (auto& h : p.engine.hist) m = juce::jmax (m, h.load (std::memory_order_relaxed));
    return juce::jmax (0.02f, m);
}
} // namespace

// =====================================================================
// MIRAGEM: a cauda vira uma serra no horizonte, e o reflexo treme como calor no asfalto
// =====================================================================
void MirageView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    const bool  frz = param (proc, id::freeze) > 0.5f;
    const bool  rev = param (proc, id::reverse) > 0.5f;
    const float shm = param (proc, id::shimmer) * 0.01f;
    const float mot = param (proc, id::motion) * 0.01f;
    const auto  hill = frz ? theme::ice : theme::amber;

    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    drawTitle (g, 12.0f, 7.0f, "MIRAGEM ", u8 ("· CAUDA ") + secText (param (proc, id::decay))
                                            + (frz ? juce::String (u8 (" · CONGELADA")) : juce::String())
                                            + (rev ? juce::String (u8 (" · REVERSO")) : juce::String()));
    {
        juce::AttributedString a;
        a.append ("SHIMMER ", theme::mono (10.0f), theme::dim);
        a.append (juce::String (juce::roundToInt (shm * 100.0f)) + "% ", theme::mono (10.0f), theme::cream);
        a.append (u8 ("· ") + u8 (intervalNames[juce::jlimit (0, numIntervals - 1, juce::roundToInt (param (proc, id::interval)))]).toUpperCase(),
                  theme::mono (10.0f), theme::dim);
        a.setJustification (juce::Justification::centredRight);
        a.draw (g, { W - 400.0f, 6.0f, 388.0f, 14.0f });
    }

    const float x0 = 12.0f, x1 = W - 12.0f, y0 = 26.0f, y1 = H - 10.0f;
    const float horizon = y0 + (y1 - y0) * 0.64f;
    const float sceneW = x1 - x0, skyH = horizon - y0;

    // céu: escuro em cima, brilho no horizonte (o shimmer acende o céu)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<float> (x0, y0, sceneW, y1 - y0).toNearestInt());
        g.setGradientFill (juce::ColourGradient (theme::dark, 0.0f, y0, hill.withAlpha (0.10f + 0.15f * shm), 0.0f, horizon, false));
        g.fillRect (x0, y0, sceneW, skyH);
        g.setColour (theme::dark);
        g.fillRect (x0, horizon, sceneW, y1 - horizon);

        // sol: o halo violeta cresce com o shimmer
        const float sx = x0 + sceneW * 0.78f, sy = horizon - skyH * 0.45f, sr = 14.0f;
        g.setGradientFill (juce::ColourGradient (theme::violet.withAlpha (0.35f * shm), sx, sy,
                                                 theme::violet.withAlpha (0.0f), sx + sr * (2.0f + 4.0f * shm), sy, true));
        g.fillEllipse (sx - sr * 6.0f, sy - sr * 6.0f, sr * 12.0f, sr * 12.0f);
        g.setColour (hill.withAlpha (0.85f));
        g.fillEllipse (sx - sr, sy - sr, sr * 2.0f, sr * 2.0f);

        // serra: cada coluna é o nível da cauda num instante (mais novo à direita; no REVERSO, à esquerda)
        const float norm = histNorm (proc);
        const double span = 5.0;
        juce::Path ridge;
        ridge.startNewSubPath (x0, horizon);
        for (float x = x0; x <= x1 + 0.5f; x += 3.0f)
        {
            const float u   = (x - x0) / sceneW;
            const double age = (rev ? u : 1.0f - u) * span;
            const float v   = juce::jlimit (0.0f, 1.0f, histAt (proc, age) / norm);
            ridge.lineTo (x, horizon - (0.04f + 0.9f * std::sqrt (v)) * (skyH - 8.0f));
        }
        ridge.lineTo (x1, horizon);
        ridge.closeSubPath();
        g.setGradientFill (juce::ColourGradient (hill.withAlpha (0.85f), 0.0f, y0 + 10.0f, theme::coral.withAlpha (0.45f), 0.0f, horizon, false));
        g.fillPath (ridge);
        g.setColour (hill);
        g.strokePath (ridge, juce::PathStrokeType (1.2f));

        // reflexo: o desenho de cima, de ponta-cabeça, em fatias que balançam (MOVIMENTO = mais calor)
        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        juce::Image above (juce::Image::ARGB, juce::roundToInt (sceneW * scale), juce::roundToInt (skyH * scale), true);
        {
            juce::Graphics ig (above);
            ig.addTransform (juce::AffineTransform::scale (scale).translated (-x0 * scale, -y0 * scale));
            ig.setColour (hill.withAlpha (0.9f));
            ig.fillPath (ridge);
            ig.setColour (hill.withAlpha (0.85f));
            ig.fillEllipse (sx - sr, sy - sr, sr * 2.0f, sr * 2.0f);
        }
        const float t   = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const float amp = frz ? 0.6f : 1.5f + 9.0f * mot;
        const float refH = y1 - horizon;
        for (float yy = 0.0f; yy < refH; yy += 2.0f)
        {
            const float depth01 = yy / refH;
            const float dx  = std::sin (t * (2.0f + 3.0f * mot) + yy * 0.45f) * amp * (0.3f + depth01);
            const float srcY = (skyH - 1.0f - yy * 1.6f);   // o reflexo é achatado
            if (srcY < 0.0f) break;
            g.setOpacity (0.55f * (1.0f - depth01));
            g.drawImage (above, juce::roundToInt (x0 + dx), juce::roundToInt (horizon + yy), juce::roundToInt (sceneW), 2,
                         0, juce::roundToInt (srcY * scale), above.getWidth(), juce::jmax (1, juce::roundToInt (2.0 * scale)));
        }
        g.setOpacity (1.0f);

        // ondas de calor sobre a serra
        g.setColour (theme::ice.withAlpha (0.10f + 0.25f * mot));
        for (int k = 0; k < 3; ++k)
        {
            juce::Path wave;
            const float yb = horizon - 8.0f - (float) k * 9.0f;
            for (float x = x0; x <= x1; x += 6.0f)
            {
                const float yv = yb + std::sin (t * 1.7f + x * 0.03f + (float) k) * (1.0f + 2.5f * mot);
                if (x == x0) wave.startNewSubPath (x, yv); else wave.lineTo (x, yv);
            }
            g.strokePath (wave, juce::PathStrokeType (1.0f));
        }

        g.setColour (hill.withAlpha (0.6f));
        g.fillRect (x0, horizon, sceneW, 1.0f);
    }

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// =====================================================================
// CAUDA: como o som cai (dB × tempo), o pré-delay e a faixa de tom
// =====================================================================
void DecayView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth();
    const bool  frz = param (proc, id::freeze) > 0.5f;
    const float dec = param (proc, id::decay), pre = param (proc, id::predelay) * 0.001f;
    const float shm = param (proc, id::shimmer) * 0.01f;
    const float span = juce::jmax (1.0f, pre + juce::jmin (dec * 1.15f, 30.0f));

    g.setColour (theme::dark);
    g.fillRoundedRectangle (0.0f, 0.0f, S, S, 4.0f);

    const float gx0 = 30.0f, gx1 = S - 10.0f, gy0 = 14.0f, gy1 = S - 54.0f;
    auto xOf = [=] (float sec) { return gx0 + juce::jlimit (0.0f, 1.0f, sec / span) * (gx1 - gx0); };
    auto yOf = [=] (float db)  { return gy0 + juce::jlimit (0.0f, 1.0f, -db / 60.0f) * (gy1 - gy0); };

    g.setFont (theme::mono (9.0f));
    for (int db = 0; db >= -60; db -= 20)
    {
        g.setColour (theme::segOff);
        g.fillRect (gx0, yOf ((float) db), gx1 - gx0, 1.0f);
        g.setColour (theme::dim);
        g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, yOf ((float) db) - 6.0f, 26.0f, 12.0f), juce::Justification::centredRight);
    }

    // pré-delay: o silêncio antes da cauda
    if (pre > 0.0f)
    {
        g.setColour (theme::amber.withAlpha (0.12f));
        g.fillRect (gx0, gy0, xOf (pre) - gx0, gy1 - gy0);
    }

    // shimmer: uma segunda cauda, violeta, que segura mais em cima
    if (shm > 0.01f && ! frz)
    {
        juce::Path sp;
        for (int k = 0; k <= 60; ++k)
        {
            const float tt = pre + span * (float) k / 60.0f;
            const float db = -60.0f * (tt - pre) / (dec * (1.0f + 1.5f * shm)) - 18.0f * (1.0f - shm);
            if (k == 0) sp.startNewSubPath (xOf (tt), yOf (db)); else sp.lineTo (xOf (tt), yOf (db));
        }
        g.setColour (theme::violet.withAlpha (0.3f + 0.6f * shm));
        g.strokePath (sp, juce::PathStrokeType (1.4f));
    }

    // cauda principal (congelada = reta, em gelo)
    {
        juce::Path cp, fill;
        cp.startNewSubPath (xOf (pre), yOf (0.0f));
        cp.lineTo (frz ? juce::Point<float> (gx1, yOf (0.0f)) : juce::Point<float> (xOf (pre + dec), yOf (-60.0f)));
        fill = cp;
        fill.lineTo (frz ? gx1 : xOf (pre + dec), yOf (-60.0f));
        fill.lineTo (xOf (pre), yOf (-60.0f));
        fill.closeSubPath();
        const auto col = frz ? theme::ice : theme::amber;
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.3f), 0.0f, gy0, col.withAlpha (0.0f), 0.0f, gy1, false));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (cp, juce::PathStrokeType (2.0f));
    }

    // nível da cauda agora
    {
        const float lvl = juce::Decibels::gainToDecibels (levelShown, -60.0f);
        g.setColour (theme::cream);
        g.fillRect (gx1 - 6.0f, yOf (lvl) - 1.0f, 8.0f, 2.0f);
    }

    // tom da cauda: o que sobra entre o CORTE GRAVE e o ESCURO (20 Hz–20 kHz)
    {
        const float by = S - 42.0f;
        g.setColour (theme::segOff);
        g.fillRoundedRectangle (gx0, by, gx1 - gx0, 8.0f, 2.0f);
        const float lo = freqX (param (proc, id::lowCut), gx0, gx1);
        const float hi = freqX (frz ? 20000.0f : dampHz (param (proc, id::dark) * 0.01f), gx0, gx1);
        g.setColour (theme::coral);
        g.fillRoundedRectangle (lo, by, juce::jmax (2.0f, hi - lo), 8.0f, 2.0f);
        g.setColour (theme::dim);
        g.setFont (theme::mono (9.0f));
        g.drawText ("20", juce::Rectangle<float> (gx0, by + 10.0f, 40.0f, 11.0f), juce::Justification::centredLeft);
        g.drawText ("20k", juce::Rectangle<float> (gx1 - 40.0f, by + 10.0f, 40.0f, 11.0f), juce::Justification::centredRight);
        g.drawText ("TOM", juce::Rectangle<float> (gx0, by + 10.0f, gx1 - gx0, 11.0f), juce::Justification::centred);
    }

    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    auto sl = line ("RT60 ", frz ? juce::String ("INFINITO") : secText (dec));
    auto sr = line ("DUCK ", duckShown < -0.1f ? juce::String (duckShown, 1).replaceCharacter ('.', ',') + " dB" : juce::String ("0 dB"));
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
    sr.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
}

// =====================================================================
// EDITOR
// =====================================================================
MiragemEditor::MiragemEditor (MiragemProcessor& p)
    : AudioProcessorEditor (&p), proc (p), mirage (p), decayView (p),
      // cor do knob = cor do que ele mexe no desenho
      decayMacro  (p.apvts, id::decay,    "CAUDA",        true,  theme::amber),
      size        (p.apvts, id::size,     "TAMANHO",      false, theme::amber),
      predelay    (p.apvts, id::predelay, u8 ("PRÉ-DELAY"), false, theme::amber),
      dark        (p.apvts, id::dark,     "ESCURO",       false, theme::coral),
      lowCut      (p.apvts, id::lowCut,   "CORTE GRAVE",  false, theme::coral),
      shimMacro   (p.apvts, id::shimmer,  "SHIMMER",      true,  theme::violet),
      interval    (p.apvts, id::interval, "TOM",          false, theme::violet),
      motion      (p.apvts, id::motion,   "MOVIMENTO",    false, theme::ice),
      duck        (p.apvts, id::duck,     "DUCK",         false, theme::cream),
      revLen      (p.apvts, id::revLen,   "JANELA REV.",  false, theme::cream),
      output      (p.apvts, id::output,   "GANHO",        false, theme::cream),
      mix         (p.apvts, id::mix),
      freezeKey   ("CONGELAR", theme::ice),
      reverseKey  ("REVERSO", theme::violet)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &mirage, &decayView, &decayMacro, &size, &predelay, &dark, &lowCut, &shimMacro, &interval, &motion, &duck, &revLen,
             &output, &mix, &freezeKey, &reverseKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    freezeAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::freeze, freezeKey);
    reverseAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::reverse, reverseKey);
    bypassAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

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

MiragemEditor::~MiragemEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MiragemEditor::showPresetMenu()
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

    juce::Component::SafePointer<MiragemEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void MiragemEditor::askNewPreset()
{
    juce::Component::SafePointer<MiragemEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void MiragemEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<MiragemEditor> safe (this);
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

void MiragemEditor::timerCallback()
{
    decayView.levelShown = juce::jmax (proc.engine.levelNow.exchange (0.0f), decayView.levelShown * 0.9f);
    decayView.duckShown  = juce::jmin (proc.engine.duckNow.exchange (0.0f), decayView.duckShown * 0.85f);
    mirage.repaint();
    decayView.repaint();

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

void MiragemEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void MiragemEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void MiragemEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void MiragemEditor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // ---- logo: sol no horizonte e o reflexo tremendo ----
    g.setColour (theme::amber.withAlpha (0.5f));
    g.drawEllipse (17.0f, 20.0f, 28.0f, 28.0f, 1.4f);
    g.setColour (theme::amber);
    g.fillEllipse (26.0f, 25.0f, 10.0f, 10.0f);
    g.fillRect (21.0f, 36.5f, 20.0f, 1.4f);
    for (int k = 0; k < 2; ++k)
    {
        juce::Path w;
        const float y = 40.0f + (float) k * 3.2f;
        w.startNewSubPath (24.0f, y);
        w.quadraticTo (27.5f, y - 1.6f, 31.0f, y);
        w.quadraticTo (34.5f, y + 1.6f, 38.0f, y);
        g.setColour (theme::violet.withAlpha (0.9f - 0.35f * (float) k));
        g.strokePath (w, juce::PathStrokeType (1.2f));
    }

    const float brandEnd = drawBrand (g, "MIRAGEM", "SHIMMER REVERB", u8 ("CAUDA QUE SE DESFAZ"));
    drawSync (g, proc.shownBpm.load(), proc.synced.load(), proc.waiting.load(), brandEnd);

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "CAUDA ", u8 ("· dB × TEMPO"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), u8 ("ESPAÇO"));
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "SHIMMER");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), u8 ("SAÍDA"));

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 110.0f }, "MACRO CAUDA",
              u8 ("Quanto tempo o som leva para sumir, de uma sala curta a 30 s de nuvem."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 110.0f }, "MACRO SHIMMER",
              u8 ("A cauda sobe de tom e volta para a sala: cada volta fica mais alta e mais brilhante."));
}

void MiragemEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    mirage.setBounds (12, 66, 976, 140);
    decayView.setBounds (22, 246, 240, 264);

    decayMacro.setBounds (294, 246, 92, 92);
    size.setBounds       (294, 376, 119, 92);
    predelay.setBounds   (413, 376, 119, 92);
    dark.setBounds       (294, 468, 119, 92);
    lowCut.setBounds     (413, 468, 119, 92);

    shimMacro.setBounds (562, 246, 92, 92);
    interval.setBounds  (562, 376, 119, 92);
    motion.setBounds    (681, 376, 119, 92);
    duck.setBounds      (562, 468, 119, 92);
    revLen.setBounds    (681, 468, 119, 92);

    freezeKey.setBounds  (830, 246, 71, 40);
    reverseKey.setBounds (907, 246, 71, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);
}
