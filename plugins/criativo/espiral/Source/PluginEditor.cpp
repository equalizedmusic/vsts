#include "PluginEditor.h"
#include <complex>

using namespace espiral;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004;

// pedal: chapa esmaltada de 1000 × 580 (o layout) na moldura de madeira (laterais de 24, travessas de 20)
constexpr int lado = 24, topo = 20, pedalW = ui::baseW + 2 * lado, pedalH = ui::baseH + 2 * topo;
const juce::Colour esmalte (0xff1f5a6b);      // cor da chapa do ESPIRAL
const juce::Colour serigrafia (0xffede3cc);   // tinta creme impressa na chapa

// função, e não array global: cores copiadas do tema na carga do plugin podem ficar zeradas
// (ordem de inicialização entre arquivos)
juce::Colour voiceColour (int v)
{
    switch (v)
    {
        case 0:  return theme::amber;
        case 1:  return theme::coral;
        case 2:  return theme::ice;
        default: return theme::violet;
    }
}

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
    painel::chapaPreta (g, r, false);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void drawBadge (juce::Graphics& g, juce::Rectangle<float> badge, juce::Colour c, const juce::String& text)
{
    g.setColour (c.withAlpha (0.13f));
    g.fillRoundedRectangle (badge, 3.0f);
    g.setColour (c.withAlpha (0.4f));
    g.drawRoundedRectangle (badge, 3.0f, 1.0f);
    g.setColour (c);
    g.setFont (theme::sans (9.0f, true, 0.2f));
    g.drawText (text, badge, juce::Justification::centred);
}

juce::String paramText (EspiralProcessor& p, const char* pid)
{
    return p.apvts.getParameter (pid)->getCurrentValueAsText().toUpperCase();
}
} // namespace

// =====================================================================
// RESPOSTA
// =====================================================================
void ResponseStrip::paint (juce::Graphics& g)
{
    const auto  r   = getLocalBounds().toFloat();
    const float W   = r.getWidth(), H = r.getHeight();
    const float px0 = 12.0f, px1 = W - 12.0f, top = 30.0f, bot = H - 18.0f;
    const float dbTop = 12.0f, dbBot = -30.0f;
    const float fs  = proc.engine.rate.load();

    auto fx = [=] (float hz) { return px0 + std::log (hz / 20.0f) / std::log (1000.0f) * (px1 - px0); };
    auto dy = [=] (float db) { return top + (dbTop - juce::jlimit (dbBot, dbTop, db)) / (dbTop - dbBot) * (bot - top); };

    painel::chapaPreta (g, r.reduced (1.0f), false);

    // grade
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const bool major = hz == 100.0f || hz == 1000.0f || hz == 10000.0f;
        g.setColour (major ? theme::segOff : juce::Colour (0xff25211b));
        g.fillRect (fx (hz), top, 1.0f, bot - top);
        if (major)
        {
            g.setColour (theme::dim);
            g.setFont (theme::mono (9.5f));
            g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (fx (hz) + 4.0f, bot + 3.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
        }
    }
    g.setColour (theme::segOff);
    g.fillRect (px0, dy (0.0f), px1 - px0, 1.0f);

    // vozes: faixa de luz na frequência de cada uma, acesa conforme a janela
    std::array<float, numVoices> hz {}, wv {};
    for (int v = 0; v < numVoices; ++v)
    {
        hz[(size_t) v] = proc.engine.voiceHz[(size_t) v].load (std::memory_order_relaxed);
        wv[(size_t) v] = proc.engine.voiceGain[(size_t) v].load (std::memory_order_relaxed);
        const float lit = wv[(size_t) v] * (float) numVoices * 0.5f;
        const float x = fx (hz[(size_t) v]);
        const auto  c = voiceColour (v);
        juce::ColourGradient band (c.withAlpha (0.0f), x - 22.0f, 0.0f, c.withAlpha (0.0f), x + 22.0f, 0.0f, false);
        band.addColour (0.5, c.withAlpha (0.16f * lit));
        g.setGradientFill (band);
        g.fillRect (x - 22.0f, top, 44.0f, bot - top);
        g.setColour (c.withAlpha (0.25f + 0.6f * lit));
        g.fillRect (x - 1.0f, top, 2.0f, 5.0f);
    }

    // curva: o que sai do plugin (seco + processado), com o mesmo modelo do motor
    {
        const int   modeIdx = juce::roundToInt (proc.apvts.getRawParameterValue (id::mode)->load());
        const int   nst     = stageCount (juce::roundToInt (proc.apvts.getRawParameterValue (id::stages)->load()));
        const float res     = proc.apvts.getRawParameterValue (id::resonance)->load() * 0.01f;
        const float mixV    = proc.apvts.getRawParameterValue (id::mix)->load() * 0.01f;
        const float tone    = proc.apvts.getRawParameterValue (id::tone)->load() * 0.01f;
        const float lpC     = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz (tone) / fs);
        using cf = std::complex<float>;

        juce::Path curve;
        const int pts = 300;
        for (int k = 0; k < pts; ++k)
        {
            const float f  = juce::jmin (20.0f * std::pow (1000.0f, (float) k / (float) (pts - 1)), fs * 0.49f);
            const float om = juce::MathConstants<float>::twoPi * f / fs;
            const cf z1 = std::polar (1.0f, -om);
            cf wet { 0.0f, 0.0f };

            if (modeIdx == modeFlanger)
            {
                cf d { 0.0f, 0.0f };
                for (int v = 0; v < numVoices; ++v)
                    d += wv[(size_t) v] * std::polar (1.0f, -om * fs / (2.0f * hz[(size_t) v]));
                wet = d / (1.0f - res * 0.85f * d);
            }
            else
            {
                for (int v = 0; v < numVoices; ++v)
                {
                    const float t = std::tan (juce::MathConstants<float>::pi * hz[(size_t) v] / fs);
                    const float a = (t - 1.0f) / (t + 1.0f);
                    const cf ap = (a + z1) / (1.0f + a * z1);
                    cf chain { 1.0f, 0.0f };
                    for (int s = 0; s < nst; ++s) chain *= ap;
                    wet += wv[(size_t) v] * chain / (1.0f - res * 0.9f * z1 * chain);
                }
            }
            const cf lp  = lpC / (1.0f - (1.0f - lpC) * z1);
            const cf out = (1.0f - mixV) + mixV * 0.5f * (1.0f + lp * wet);
            const float db = juce::Decibels::gainToDecibels (std::abs (out), -60.0f);
            const float x = fx (f), y = dy (db);
            if (k == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }

        juce::Path fill (curve);
        fill.lineTo (px1, bot);
        fill.lineTo (px0, bot);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (0.28f), 0.0f, top,
                                                 theme::coral.withAlpha (0.03f), 0.0f, bot, false));
        g.fillPath (fill);
        g.setColour (theme::cream);
        g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // faixa da varredura
    {
        const float lo = proc.engine.lowHz.load(), hi = proc.engine.highHz.load();
        g.setColour (theme::amber.withAlpha (0.5f));
        g.fillRect (fx (lo), bot - 2.0f, fx (hi) - fx (lo), 2.0f);
        g.setColour (theme::dim);
        g.setFont (theme::mono (10.0f));
        g.drawText (hzText (lo) + u8 (" – ") + hzText (hi), juce::Rectangle<float> (W - 220.0f, 6.0f, 208.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    // título
    {
        const bool flanger = juce::roundToInt (proc.apvts.getRawParameterValue (id::mode)->load()) == modeFlanger;
        juce::String info = u8 ("· ") + paramText (proc, id::mode);
        if (! flanger) info << " " << paramText (proc, id::stages);
        info << u8 (" · ") << paramText (proc, id::direction) << u8 (" · ") << paramText (proc, id::cycle);
        drawTitle (g, 12.0f, 7.0f, "RESPOSTA ", info);
    }

    // selos de performance
    if (proc.apvts.getRawParameterValue (id::freeze)->load() > 0.5f)
        drawBadge (g, juce::Rectangle<float> (98.0f, 17.0f).withCentre ({ W * 0.5f, 15.5f }), theme::ice, "CONGELADO");
    else if (proc.apvts.getRawParameterValue (id::twice)->load() > 0.5f)
        drawBadge (g, juce::Rectangle<float> (98.0f, 17.0f).withCentre ({ W * 0.5f, 15.5f }), theme::coral, u8 ("DOBRADO · 2×"));
}

// =====================================================================
// ESPIRAL
// =====================================================================
void SpiralView::tick()
{
    t += 1.0f;
    const float ph = proc.engine.phase.load (std::memory_order_relaxed);
    float d = ph - lastPhase;
    d -= std::round (d);
    if (std::abs (d) > 1.0e-5f) motion = d > 0.0f ? 1.0f : -1.0f;
    lastPhase = ph;
    repaint();
}

void SpiralView::paint (juce::Graphics& g)
{
    const float MS = (float) getWidth(), MC = MS * 0.5f;
    const float turns = 2.5f, rIn = 14.0f, rOut = MC - 12.0f;
    const float rot = t * 0.004f;
    const float ph  = proc.engine.phase.load (std::memory_order_relaxed);

    auto pt = [=] (float u)
    {
        const float th = rot + u * turns * juce::MathConstants<float>::twoPi;
        const float rr = rIn + (rOut - rIn) * u;
        return juce::Point<float> (MC + rr * std::sin (th), MC - rr * std::cos (th));
    };

    // tela redonda: canto = metade do lado vira círculo
    const juce::Rectangle<float> scr (1.5f, 1.5f, MS - 3.0f, MS - 3.0f);
    const float scrCorner = scr.getWidth() * 0.5f;
    real::screen (g, scr, scrCorner);

    // trilho: do grave (centro) ao agudo (borda)
    {
        juce::Path track;
        for (int k = 0; k <= 240; ++k)
        {
            const auto p = pt ((float) k / 240.0f);
            if (k == 0) track.startNewSubPath (p); else track.lineTo (p);
        }
        g.setColour (juce::Colour (0xff25211b));
        g.strokePath (track, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (theme::segOff);
        g.strokePath (track, juce::PathStrokeType (1.0f));
    }

    // vozes: cometa com rastro para trás do movimento
    for (int v = 0; v < numVoices; ++v)
    {
        const float p   = frac01 (ph + (float) v / (float) numVoices);
        const float lit = proc.engine.voiceGain[(size_t) v].load (std::memory_order_relaxed) * (float) numVoices * 0.5f;
        const auto  c   = voiceColour (v);

        constexpr int tail = 28;
        for (int j = tail; j > 0; --j)
        {
            const float u0 = p - motion * (float) j * 0.005f, u1 = p - motion * (float) (j - 1) * 0.005f;
            if (u0 < 0.0f || u0 > 1.0f || u1 < 0.0f || u1 > 1.0f) continue;
            const float k = 1.0f - (float) j / (float) tail;
            g.setColour (c.withAlpha ((0.08f + 0.6f * k) * (0.2f + 0.8f * lit)));
            g.drawLine (juce::Line<float> (pt (u0), pt (u1)), 1.0f + 5.0f * k);
        }

        const auto  h  = pt (p);
        const float rd = 2.5f + 4.5f * lit;
        g.setColour (c.withAlpha (0.25f * lit));
        g.fillEllipse (h.x - rd * 2.2f, h.y - rd * 2.2f, rd * 4.4f, rd * 4.4f);
        g.setColour (c.withAlpha (0.3f + 0.7f * lit));
        g.fillEllipse (h.x - rd, h.y - rd, rd * 2.0f, rd * 2.0f);
    }

    g.setColour (theme::cream);
    g.fillEllipse (MC - 2.5f, MC - 2.5f, 5.0f, 5.0f);
    real::glass (g, scr, scrCorner);

    // volta e ciclo
    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    auto sl = line ("VOLTA ", juce::String (((proc.engine.turn.load() % 1000) + 1000) % 1000).paddedLeft ('0', 3));
    auto sr = line ("CICLO ", paramText (proc, id::cycle));
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, MS + 8.0f, MS, 14.0f });
    sr.draw (g, { 0.0f, MS + 8.0f, MS, 14.0f });
}

// =====================================================================
// EDITOR
// =====================================================================
EspiralEditor::EspiralEditor (EspiralProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), spiral (p),
      rangeMacro (p.apvts, id::range,     "RANGE",              true,  theme::amber),
      resMacro   (p.apvts, id::resonance, "RESONANCE",          true,  theme::coral),
      cycle      (p.apvts, id::cycle,     "CICLO",              false, theme::cream),
      direction  (p.apvts, id::direction, u8 ("DIREÇÃO"),       false, theme::cream),
      centre     (p.apvts, id::centre,    "CENTER",             false, theme::cream),
      stereo     (p.apvts, id::stereo,    "STEREO",             false, theme::cream),
      mode       (p.apvts, id::mode,      "MODO",               false, theme::cream),
      stages     (p.apvts, id::stages,    "STAGES",             false, theme::cream),
      drive      (p.apvts, id::drive,     "DRIVE",              false, theme::cream),
      tone       (p.apvts, id::tone,      "TONE",               false, theme::cream),
      output     (p.apvts, id::output,    "GAIN",               false, theme::cream),
      mix        (p.apvts, id::mix),
      freezeKey  ("CONGELAR", theme::ice),
      twiceKey   ("DOBRAR", theme::coral)
{
    setLookAndFeel (&lnf);
    for (auto* k : { &rangeMacro, &resMacro, &cycle, &direction, &centre, &stereo, &mode, &stages, &drive, &tone, &output })
        k->setPedal (true);   // knobs do pedal: os quadros renderizados da SÉRIE KNOB

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, pedalW, pedalH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &spiral, &rangeMacro, &resMacro, &cycle, &direction, &centre, &stereo, &mode, &stages,
             &drive, &tone, &output, &mix, &freezeKey, &twiceKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    freezeAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::freeze, freezeKey);
    twiceAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::twice, twiceKey);
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
    setResizeLimits (juce::roundToInt (pedalW * ui::minScale), juce::roundToInt (pedalH * ui::minScale),
                     juce::roundToInt (pedalW * ui::maxScale), juce::roundToInt (pedalH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) pedalW / (double) pedalH);
    setScale (openScale);
    startTimerHz (30);
}

EspiralEditor::~EspiralEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EspiralEditor::showPresetMenu()
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

    juce::Component::SafePointer<EspiralEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void EspiralEditor::askNewPreset()
{
    juce::Component::SafePointer<EspiralEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void EspiralEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<EspiralEditor> safe (this);
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

void EspiralEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) pedalW - openScale) > 0.005f)
        setScale (openScale);

    strip.repaint();
    spiral.tick();

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
        canvas.repaint (380 + lado, 18 + topo, 236, 32);
    }
}

void EspiralEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) pedalW * scale), juce::roundToInt ((float) pedalH * scale));
}

void EspiralEditor::resized()
{
    const float scale = (float) getWidth() / (float) pedalW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void EspiralEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void EspiralEditor::drawCanvas (juce::Graphics& g)
{
    painel::pedal (g, (float) pedalW, (float) pedalH, (float) lado, (float) topo, esmalte);
    g.addTransform (juce::AffineTransform::translation ((float) lado, (float) topo));   // daqui em diante, o layout da chapa

    // ---- logo: espiral saindo do centro ----
    g.setColour (theme::amber);
    g.drawEllipse (18.0f, 21.0f, 26.0f, 26.0f, 2.0f);
    {
        juce::Path sp;
        for (int k = 0; k <= 60; ++k)
        {
            const float u = (float) k / 60.0f, th = u * 2.2f * juce::MathConstants<float>::twoPi, rr = 1.0f + 8.0f * u;
            const juce::Point<float> p (31.0f + rr * std::sin (th), 34.0f - rr * std::cos (th));
            if (k == 0) sp.startNewSubPath (p); else sp.lineTo (p);
        }
        g.setColour (theme::cream);
        g.strokePath (sp, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float th = 2.2f * juce::MathConstants<float>::twoPi;
        g.setColour (theme::coral);
        g.fillEllipse (31.0f + 9.0f * std::sin (th) - 2.0f, 34.0f - 9.0f * std::cos (th) - 2.0f, 4.0f, 4.0f);
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("ESPIRAL");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 200.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (serigrafia.withAlpha (0.35f));
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    g.setColour (serigrafia.withAlpha (0.7f));
    g.setFont (tagFont);
    g.drawText ("INFINITE",                juce::Rectangle<float> (tagX, 22.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (u8 ("PHASER · FLANGER"),   juce::Rectangle<float> (tagX, 35.0f, 140.0f, 12.0f), juce::Justification::centredLeft);

    // ---- marca ----
    {
        const float brandX = tagX + juce::jmax (textWidth (tagFont, "INFINITE"), textWidth (tagFont, u8 ("PHASER · FLANGER"))) + 20.0f;
        g.setColour (serigrafia.withAlpha (0.35f));
        g.fillRect (brandX - 10.0f, 24.0f, 1.0f, 22.0f);
        g.setColour (serigrafia.withAlpha (0.7f));
        g.setFont (tagFont);
        g.drawText ("BY", juce::Rectangle<float> (brandX, 22.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
        g.setColour (theme::amber);
        g.setFont (theme::sans (10.5f, true, 0.06f));
        g.drawText ("InsideStudioSP", juce::Rectangle<float> (brandX, 34.0f, 140.0f, 13.0f), juce::Justification::centredLeft);
    }

    // ---- BPM: SYNC seguindo o host; host parado = espera o play; sem régua do host = LIVRE ----
    {
        const bool syn = proc.synced.load();
        juce::AttributedString s;
        s.append (juce::String (proc.shownBpm.load(), 1), theme::mono (11.5f), theme::cream);
        s.append (u8 (" BPM · "), theme::mono (11.5f), serigrafia.withAlpha (0.7f));
        s.append (syn ? "SYNC" : proc.waiting.load() ? "AGUARDANDO PLAY" : "LIVRE", theme::mono (11.5f),
                  syn ? theme::cream : serigrafia.withAlpha (0.7f));
        juce::TextLayout tl;
        tl.createLayout (s, 400.0f);
        const float w = tl.getWidth(), right = 604.0f;
        tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });

        const juce::Rectangle<float> led (right - w - 13.0f, 30.5f, 7.0f, 7.0f);
        real::led (g, led.getCentre(), 3.5f, theme::amber, syn ? 1.0f : 0.0f);
    }

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "ESPIRAL ", u8 ("· 4 VOICES"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "MOVIMENTO");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "COR");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 90.0f }, "MACRO RANGE",
              u8 ("Quantas oitavas a espiral percorre. Pouco é sutil; tudo é a subida sem fim."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 90.0f }, "MACRO RESONANCE",
              u8 ("FEEDBACK: mais = vales cantando, metálico e ácido."));
}

void EspiralEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    spiral.setBounds (22, 246, 240, 264);

    rangeMacro.setBounds (294, 246, 92, 92);
    cycle.setBounds     (294, 376, 119, 92);
    direction.setBounds (413, 376, 119, 92);
    centre.setBounds    (294, 468, 119, 92);
    stereo.setBounds    (413, 468, 119, 92);

    resMacro.setBounds (562, 246, 92, 92);
    mode.setBounds     (562, 376, 119, 92);
    stages.setBounds   (681, 376, 119, 92);
    drive.setBounds    (562, 468, 119, 92);
    tone.setBounds     (681, 468, 119, 92);

    freezeKey.setBounds (830, 246, 71, 40);
    twiceKey.setBounds  (907, 246, 71, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);

    for (auto* c : canvas.getChildren())   // a chapa começa dentro da moldura
        c->setTopLeftPosition (c->getX() + lado, c->getY() + topo);
}
