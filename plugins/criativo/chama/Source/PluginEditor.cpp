#include "PluginEditor.h"

using namespace chama;

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

} // namespace

// =====================================================================
// TENSÃO (faixa de cima)
// =====================================================================
FireParams HeatStrip::fireParams() const
{
    FireParams fp;
    fp.heat = st.drop ? 0.0f : st.heat;
    fp.level = st.level;
    fp.noise = st.noise;
    fp.space = st.space;
    fp.pulse = st.pulse;
    fp.t = st.t;
    fp.centred = false;
    return fp;
}

void HeatStrip::set (const FlameState& s)
{
    st = s;
    const auto area = getLocalBounds().toFloat().withTrimmedTop (60.0f);
    if (! area.isEmpty())
        fire.render (juce::jmax (8, (int) (area.getWidth() / 4.0f)), juce::jmax (4, (int) (area.getHeight() / 4.0f)),
                     area.getWidth() / area.getHeight(), fireParams());
    repaint();
}

void HeatStrip::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r.reduced (1.0f), 4.0f);
        g.reduceClipRegion (clip);
        const auto area = r.withTrimmedTop (60.0f);
        const auto fp = fireParams();
        fire.draw (g, area, fp);
        drawSparks (g, area, fp, 50);
    }

    const float h = st.drop ? 0.0f : st.heat;
    drawTitle (g, 12.0f, 7.0f, u8 ("TENSÃO "), u8 ("· ") + juce::String (juce::roundToInt (h * 100.0f)) + "%"
                                                 + (st.drop ? u8 (" · DROP") : juce::String()));

    // o que a CHAMA está fazendo agora
    const float gd  = gateDepth (h);
    const float nzg = noiseGain (h, st.noise);
    const bool  waitPlay = proc.waiting.load();
    const juce::String vals[] = {
        hzText (cutoffHz (h)),
        juce::String (resonanceQ (h), 1).replaceCharacter ('.', ','),
        juce::String (juce::roundToInt (widthAmount (h) * 100.0f)) + "%",
        nzg > 1.0e-4f ? juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (nzg))).replace ("-", u8 ("−")) + " dB" : u8 ("—"),
        juce::String (juce::roundToInt (washAmount (h, st.space) / 0.8f * 100.0f)) + "%",
        gd > 0.005f ? (waitPlay ? juce::String ("ESPERA PLAY") : juce::String (gateText (h)) + u8 (" · ") + juce::String (juce::roundToInt (gd * 100.0f)) + "%")
                    : u8 ("—"),
    };
    const juce::String names[] = { "LOW CUT", "RESONANCE", "WIDTH", "NOISE", u8 ("ESPAÇO"), "GATE SYNC" };
    const juce::Colour tint[] = { theme::amber, theme::amber, theme::cream, theme::ice, theme::violet, theme::coral };

    for (int k = 0; k < 6; ++k)
    {
        const juce::Rectangle<float> chip (12.0f + (float) k * 160.0f, 24.0f, 150.0f, 38.0f);
        g.setColour (theme::dark.withAlpha (0.78f));
        g.fillRoundedRectangle (chip, 3.0f);
        g.setColour (tint[k]);
        g.fillRect (chip.getX(), chip.getY() + 6.0f, 2.0f, chip.getHeight() - 12.0f);
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.18f));
        g.drawText (names[k], chip.reduced (10.0f, 4.0f).removeFromTop (12.0f), juce::Justification::centredLeft);
        g.setColour (theme::cream);
        g.setFont (theme::mono (13.0f));
        g.drawText (vals[k], chip.reduced (10.0f, 4.0f).removeFromBottom (16.0f), juce::Justification::centredLeft);
    }

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// =====================================================================
// PALCO DO KNOB
// =====================================================================
FireParams FlameStage::fireParams() const
{
    FireParams fp;
    fp.heat = st.drop ? 0.0f : st.heat;
    fp.level = st.level;
    fp.noise = st.noise;
    fp.space = st.space;
    fp.pulse = st.pulse;
    fp.t = st.t;
    fp.centred = true;
    return fp;
}

void FlameStage::set (const FlameState& s)
{
    st = s;
    const auto r = getLocalBounds().toFloat();
    if (! r.isEmpty())
        fire.render (juce::jmax (8, (int) (r.getWidth() / 3.0f)), juce::jmax (4, (int) (r.getHeight() / 3.0f)),
                     r.getWidth() / r.getHeight(), fireParams());
    repaint();
}

void FlameStage::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 4.0f);
        g.reduceClipRegion (clip);
        g.setColour (theme::dark);
        g.fillRoundedRectangle (r, 4.0f);
        const auto fp = fireParams();
        fire.draw (g, r, fp);
        drawSparks (g, r, fp, 60);

        // o FOGUINHO dança à direita do knob, no tempo do projeto; embaixo, o compasso 1-2-3-4
        const float cx = r.getX() + r.getWidth() * 0.855f, floorY = r.getBottom() - 44.0f;
        drawDancer (g, cx, floorY, st.dance);
        g.setColour (theme::dark.withAlpha (0.7f));
        g.fillRoundedRectangle (juce::Rectangle<float> (cx - 40.0f, floorY + 15.0f, 80.0f, 18.0f), 9.0f);
        drawBeatDots (g, cx, floorY + 24.0f, st.dance);
    }

    // fundo escuro atrás do nome: com o fogo no talo, o texto não some
    g.setColour (theme::dark.withAlpha (0.7f));
    g.fillRoundedRectangle (juce::Rectangle<float> (r.getCentreX() - 46.0f, r.getBottom() - 36.0f, 92.0f, 22.0f), 11.0f);
    g.setColour (st.drop ? theme::ice : theme::amber);
    g.setFont (theme::sans (14.0f, true, 0.3f));
    g.drawText (st.drop ? "DROP" : "CHAMA", juce::Rectangle<float> (0.0f, r.getBottom() - 34.0f, r.getWidth(), 18.0f),
                juce::Justification::centred);
}

// =====================================================================
// EDITOR
// =====================================================================
ChamaEditor::ChamaEditor (ChamaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p),
      // cor do knob = cor do que ele mexe no desenho: chama âmbar, faíscas azuis (NOISE), fumaça violeta (ESPAÇO)
      chamaKnob (p.apvts, id::chama,  "CHAMA",        true,  theme::amber),
      noise     (p.apvts, id::noise,  "NOISE",        false, theme::ice),
      space     (p.apvts, id::space,  u8 ("ESPAÇO"),  false, theme::violet),
      output    (p.apvts, id::output, "GAIN",         false, theme::cream),
      dropKey   ("DROP", theme::ice)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &stage, &chamaKnob, &noise, &space, &output, &dropKey,
             &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    dropAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::drop, dropKey);
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

ChamaEditor::~ChamaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ChamaEditor::showPresetMenu()
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

    juce::Component::SafePointer<ChamaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void ChamaEditor::askNewPreset()
{
    juce::Component::SafePointer<ChamaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void ChamaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<ChamaEditor> safe (this);
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

void ChamaEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    anim.t    += dt;
    anim.heat  = proc.engine.heat.load();
    anim.level = juce::jmax (juce::jmin (1.0f, proc.engine.level.exchange (0.0f)), anim.level * 0.9f);
    anim.noise = proc.apvts.getRawParameterValue (id::noise)->load() * 0.01f;
    anim.space = proc.apvts.getRawParameterValue (id::space)->load() * 0.01f;
    anim.drop  = proc.apvts.getRawParameterValue (id::drop)->load() > 0.5f;

    // ---- compasso 4/4 do projeto: posição do último bloco de áudio, andando com o relógio até o próximo ----
    {
        const bool   wt    = proc.waiting.load();
        const double stamp = proc.uiStampMs.load();
        const double age   = now - stamp;
        double ppq = proc.uiPpq.load();
        if (! wt)
            ppq += juce::jlimit (0.0, 250.0, age) * 0.001 * (double) proc.shownBpm.load() / 60.0;
        double inBar = std::fmod (ppq - (proc.uiHaveBar.load() ? proc.uiBarStart.load() : 0.0), 4.0);
        if (inBar < 0.0) inBar += 4.0;

        auto& d = anim.dance;
        d.beatPos = (float) inBar;
        d.beatAbs = (int) std::floor (ppq);
        d.dancing = ! wt && stamp > 0.0 && age < 500.0;   // sem áudio chegando, ele para
        d.drop    = anim.drop;
        d.t       = anim.t;

        // a euforia segue a CHAMA: sobe rápido, desce devagar
        const float target = anim.drop ? 0.0f : anim.heat;
        euph += (target - euph) * (target > euph ? 0.18f : 0.06f);
        d.euph = euph;

        const float frac = d.beatPos - std::floor (d.beatPos);
        anim.pulse = d.dancing && ! anim.drop ? std::exp (-frac * 5.0f) * ((int) d.beatPos == 0 ? 1.0f : 0.65f) : 0.0f;
    }

    strip.set (anim);
    stage.set (anim);

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

void ChamaEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void ChamaEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void ChamaEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void ChamaEditor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // ---- logo: chama ----
    {
        juce::Path f;
        f.startNewSubPath (31.0f, 47.0f);
        f.quadraticTo (19.0f, 44.0f, 22.0f, 33.0f);
        f.quadraticTo (25.0f, 26.0f, 29.0f, 19.0f);
        f.quadraticTo (31.0f, 28.0f, 35.0f, 30.0f);
        f.quadraticTo (37.0f, 25.0f, 37.0f, 22.0f);
        f.quadraticTo (44.0f, 32.0f, 41.0f, 41.0f);
        f.quadraticTo (38.0f, 47.0f, 31.0f, 47.0f);
        f.closeSubPath();
        g.setGradientFill (juce::ColourGradient (theme::amber, 31.0f, 47.0f, theme::coral, 31.0f, 19.0f, false));
        g.fillPath (f);
        g.setColour (theme::cream);
        g.fillEllipse (28.5f, 37.0f, 5.0f, 7.0f);
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("CHAMA");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 200.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 ("BUILD-UP"), tag2 (u8 ("UM KNOB · TENSÃO"));
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

    // ---- BPM (o gate segue o grid): SYNC tocando; host parado = espera o play; sem régua = LIVRE ----
    {
        const bool syn = proc.synced.load();
        juce::AttributedString s;
        s.append (juce::String (proc.shownBpm.load(), 1), theme::mono (11.5f), theme::cream);
        s.append (u8 (" BPM · "), theme::mono (11.5f), theme::dim);
        s.append (syn ? "SYNC" : proc.waiting.load() ? "AGUARDANDO PLAY" : "LIVRE", theme::mono (11.5f),
                  syn ? theme::cream : theme::dim);
        juce::TextLayout tl;
        tl.createLayout (s, 400.0f);
        const float w = tl.getWidth(), right = 604.0f;
        tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });

        const juce::Rectangle<float> led (right - w - 13.0f, 30.5f, 7.0f, 7.0f);
        if (syn)
        {
            g.setColour (theme::amber.withAlpha (0.3f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (syn ? theme::amber : theme::off);
        g.fillEllipse (led);
    }

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 676.0f, 352.0f });
    drawCard (g, { 698.0f, 216.0f, 290.0f, 352.0f });
    drawTitle (g, 22.0f,  226.0f, "CHAMA ", u8 ("· UM KNOB PARA A SUBIDA INTEIRA"));
    drawTitle (g, 708.0f, 226.0f, "AJUSTES ", u8 ("· ATÉ ONDE VAI"));

    g.setColour (theme::dim);
    g.setFont (theme::sans (10.5f));
    g.drawFittedText (u8 ("DROP apaga a chama na hora (20 ms) para a virada; desligue para ela voltar."),
                      juce::Rectangle<int> (708, 292, 270, 30), juce::Justification::topLeft, 2);
}

void ChamaEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    stage.setBounds (22, 246, 656, 312);
    chamaKnob.setBounds (225, 262, 250, 250);

    dropKey.setBounds (708, 246, 270, 40);
    noise.setBounds (716, 330, 119, 92);
    space.setBounds (845, 330, 119, 92);
    output.setBounds (716, 440, 119, 92);
    meterIn.setBounds  (880, 440, 24, 112);
    meterOut.setBounds (908, 440, 24, 112);
}
