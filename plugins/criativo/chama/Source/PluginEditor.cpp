#include "PluginEditor.h"

using namespace chama;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuLicenca = 20005;

// pedal: chapa esmaltada na moldura de madeira (laterais de 24, travessas de 20)
constexpr float lado = 24.0f, topo = 20.0f;
const juce::Colour esmalte (0xff7c2216);              // vermelho-tijolo do CHAMA
const juce::Point<float> knobBase (250.0f, 372.0f);   // centro da base do knob grandão

// posição das 11 lâmpadas em volta da base do knob, em pixels do render do FOLE (Mapa.h da SÉRIE KNOB); aqui a 50%
constexpr float lampPos[11][2] = { { -168.86f, 162.15f }, { -225.96f, 68.93f }, { -233.29f, -38.87f }, { -190.05f, -137.65f },
                                   { -106.24f, -206.45f }, { 0.0f, -231.05f }, { 106.24f, -206.45f }, { 190.05f, -137.65f },
                                   { 233.29f, -38.87f }, { 225.96f, 68.93f }, { 168.86f, 162.15f } };
constexpr float lampEscala = 0.5f;

void drawTitle (juce::Graphics& g, float x, float y, const juce::String& prefix, const juce::String& accent)
{
    juce::AttributedString s;
    const auto f = theme::sans (9.5f, true, 0.25f);
    s.append (prefix, f, theme::dim);
    s.append (accent, f, theme::amber);
    s.setJustification (juce::Justification::centredLeft);
    s.draw (g, { x, y, 600.0f, 13.0f });
}

// contador do compasso embaixo do FOGUINHO em LEDs de hardware: mesma posição e cores do drawBeatDots
// (o 1 em coral, os outros em âmbar); o tempo atual acende e esmorece até o próximo
void drawBeatLeds (juce::Graphics& g, float cx, float y, const chama::DanceState& d)
{
    const bool on = d.dancing && ! d.drop;
    const int beat = juce::jlimit (0, 3, (int) std::floor (d.beatPos));
    const float b = d.beatPos - std::floor (d.beatPos);
    for (int k = 0; k < 4; ++k)
    {
        const float x = cx + ((float) k - 1.5f) * 16.0f;
        const bool lit = on && k == beat;
        const auto c = k == 0 ? theme::coral : theme::amber;
        hw::led (g, { x, y }, 3.8f, c, lit ? 1.0f - 0.45f * b : 0.0f);
    }
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
    // janela embutida na chapa: o fogo aparece por trás do vidro
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    real::screen (g, r, 4.0f);
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 4.0f);
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

    const float chipW = (r.getWidth() - 24.0f - 5.0f * 10.0f) / 6.0f;
    for (int k = 0; k < 6; ++k)
    {
        const juce::Rectangle<float> chip (12.0f + (float) k * (chipW + 10.0f), 24.0f, chipW, 38.0f);
        real::screen (g, chip, 3.0f);
        g.setColour (tint[k]);
        g.fillRect (chip.getX(), chip.getY() + 6.0f, 2.0f, chip.getHeight() - 12.0f);
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.18f));
        g.drawText (names[k], chip.reduced (10.0f, 4.0f).removeFromTop (12.0f), juce::Justification::centredLeft);
        g.setColour (theme::cream);
        g.setFont (theme::mono (13.0f));
        g.drawText (vals[k], chip.reduced (10.0f, 4.0f).removeFromBottom (16.0f), juce::Justification::centredLeft);
        real::glass (g, chip, 3.0f);
    }
    real::glass (g, r, 4.0f);
}

// =====================================================================
// LÂMPADAS em volta do knob: a tensão de agora, de 0 a 10, respirando no tempo
// =====================================================================
void LampRing::paint (juce::Graphics& g)
{
    const float v = drop ? 0.0f : heat * 10.0f;
    const juce::Colour creme (0xffede3cc);
    g.setFont (theme::sans (10.5f, true, 0.05f));
    for (int i = 0; i < 11; ++i)
    {
        const juce::Point<float> d (lampPos[i][0] * lampEscala, lampPos[i][1] * lampEscala);
        const auto c = centre + d;
        const float b = juce::jlimit (0.0f, 1.0f, v - (float) i + 1.0f);
        const auto cor = theme::amber.interpolatedWith (theme::coral, (float) i / 10.0f);
        hw::lamp (g, c, 6.2f, cor, b * (0.78f + 0.22f * pulse));

        const auto n = centre + d * 1.2f;   // número serigrafado do lado de fora
        g.setColour (real::black (0.35f));
        g.drawText (juce::String (i), juce::Rectangle<float> (n.x - 11.0f, n.y - 6.0f, 22.0f, 13.0f), juce::Justification::centred, false);
        g.setColour (creme);
        g.drawText (juce::String (i), juce::Rectangle<float> (n.x - 11.0f, n.y - 7.0f, 22.0f, 13.0f), juce::Justification::centred, false);
    }
}

// =====================================================================
// PALCO DO KNOB: tela limpa atrás do knob (o fogo fica na faixa de cima) e o FOGUINHO
// =====================================================================
void FlameStage::set (const FlameState& s)
{
    st = s;
    repaint();
}

void FlameStage::paint (juce::Graphics& g)
{
    // o FOGUINHO dança ao lado do knob, no tempo do projeto; embaixo, o compasso 1-2-3-4 em LEDs na chapa
    const auto r = getLocalBounds().toFloat();
    const float cx = r.getCentreX(), floorY = r.getBottom() - 56.0f;
    drawDancer (g, cx, floorY, st.dance);
    g.setColour (real::black (0.55f));
    g.fillRoundedRectangle (juce::Rectangle<float> (cx - 40.0f, floorY + 15.0f, 80.0f, 18.0f), 9.0f);
    drawBeatLeds (g, cx, floorY + 24.0f, st.dance);

    if (st.drop)
    {
        g.setColour (theme::ice);
        g.setFont (theme::sans (12.0f, true, 0.3f));
        g.drawText ("DROP", juce::Rectangle<float> (0.0f, floorY + 36.0f, r.getWidth(), 16.0f), juce::Justification::centred);
    }
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
    // emblema em relevo na tampa do knob grandão: a chama do logo (unidades da tampa, y para cima)
    lnf.emblema = { { { 0.0f, 1.0f }, { 0.3f, 0.5f }, { 0.5f, 0.05f }, { 0.45f, -0.4f }, { 0.2f, -0.7f }, { -0.2f, -0.7f },
                      { -0.45f, -0.4f }, { -0.5f, 0.05f }, { -0.3f, 0.5f }, { 0.0f, 1.0f } },
                    { { 0.0f, 0.25f }, { 0.15f, -0.05f }, { 0.18f, -0.3f }, { 0.0f, -0.45f }, { -0.18f, -0.3f }, { -0.15f, -0.05f }, { 0.0f, 0.25f } } };
    setLookAndFeel (&lnf);
    chamaKnob.setClean (true);   // as lâmpadas em volta já mostram o quanto de CHAMA tem
    for (auto* k : { &chamaKnob, &noise, &space, &output })
        k->setPedal (true);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &ring, &stage, &chamaKnob, &noise, &space, &output, &dropKey,
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
    // o tamanho salvo é lido ANTES do setResizeLimits: ele encolhe a janela ainda vazia para o mínimo
    // e o resized() gravaria 75% por cima do que o usuário escolheu
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (ui::baseW * ui::minScale), juce::roundToInt (ui::baseH * ui::minScale),
                     juce::roundToInt (ui::baseW * ui::maxScale), juce::roundToInt (ui::baseH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) ui::baseW / (double) ui::baseH);
    setScale (openScale);
    startTimerHz (30);
    ativacao.anexar (*this);
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
    menu.addItem (menuLicenca, u8 ("Licença…"));

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
        case menuLicenca:
            ativacao.mostrarInfo();
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
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) ui::baseW - openScale) > 0.005f)
        setScale (openScale);

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
    ring.set (anim.heat, anim.pulse, anim.drop);

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
        canvas.repaint (340, 32, 250, 36);
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
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void ChamaEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void ChamaEditor::drawCanvas (juce::Graphics& g)
{
    const float W = (float) ui::baseW, H = (float) ui::baseH;
    const juce::Colour creme (0xffede3cc);
    painel::pedal (g, W, H, lado, topo, esmalte);

    // ---- logo: chama ----
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::translation (22.0f, 17.0f));
        juce::Path f;
        f.startNewSubPath (31.0f, 47.0f);
        f.quadraticTo (19.0f, 44.0f, 22.0f, 33.0f);
        f.quadraticTo (25.0f, 26.0f, 29.0f, 19.0f);
        f.quadraticTo (31.0f, 28.0f, 35.0f, 30.0f);
        f.quadraticTo (37.0f, 25.0f, 37.0f, 22.0f);
        f.quadraticTo (44.0f, 32.0f, 41.0f, 41.0f);
        f.quadraticTo (38.0f, 47.0f, 31.0f, 47.0f);
        f.closeSubPath();
        g.setGradientFill (juce::ColourGradient (theme::amber, 31.0f, 47.0f, juce::Colour (0xffffd27a), 31.0f, 19.0f, false));
        g.fillPath (f);
        g.setColour (creme);
        g.fillEllipse (28.5f, 37.0f, 5.0f, 7.0f);
    }

    // ---- série e marca, serigrafadas na chapa ----
    const auto serieFont = theme::sans (17.0f, true, 0.02f);
    const juce::String serie (u8 ("série criativo"));
    g.setColour (creme);
    g.setFont (serieFont);
    g.drawText (serie, juce::Rectangle<float> (74.0f, 34.0f, 200.0f, 32.0f), juce::Justification::centredLeft);
    const float brandX = 74.0f + textWidth (serieFont, serie) + 20.0f;
    g.setColour (creme.withAlpha (0.35f));
    g.fillRect (brandX - 10.0f, 39.0f, 1.0f, 22.0f);
    g.setColour (creme.withAlpha (0.7f));
    g.setFont (theme::sans (9.5f, false, 0.2f));
    g.drawText ("BY", juce::Rectangle<float> (brandX, 37.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour (theme::amber.brighter (0.3f));
    g.setFont (theme::sans (10.5f, true, 0.06f));
    g.drawText ("InsideStudioSP", juce::Rectangle<float> (brandX, 49.0f, 140.0f, 13.0f), juce::Justification::centredLeft);

    // ---- BPM (o gate segue o grid): SYNC tocando; host parado = espera o play; sem régua = LIVRE ----
    {
        const bool syn = proc.synced.load();
        juce::AttributedString s;
        s.append (juce::String (proc.shownBpm.load(), 1), theme::mono (11.5f), creme);
        s.append (u8 (" BPM · "), theme::mono (11.5f), creme.withAlpha (0.6f));
        s.append (syn ? "SYNC" : proc.waiting.load() ? "AGUARDANDO PLAY" : "LIVRE", theme::mono (11.5f),
                  syn ? creme : creme.withAlpha (0.6f));
        juce::TextLayout tl;
        tl.createLayout (s, 400.0f);
        const float w = tl.getWidth(), right = 580.0f;
        tl.draw (g, { right - w, 50.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });
        hw::led (g, { right - w - 12.0f, 50.0f }, 3.4f, theme::amber, syn ? 1.0f : 0.0f);
    }

    // ---- nome embaixo do knob, como na SÉRIE KNOB ----
    g.setColour (real::black (0.3f));
    g.setFont (theme::sans (30.0f, true, 0.35f));
    g.drawText ("CHAMA", juce::Rectangle<float> (knobBase.x - 150.0f, 479.0f, 300.0f, 34.0f), juce::Justification::centred);
    g.setColour (creme);
    g.drawText ("CHAMA", juce::Rectangle<float> (knobBase.x - 150.0f, 478.0f, 300.0f, 34.0f), juce::Justification::centred);
    g.setColour (creme.withAlpha (0.75f));
    g.setFont (theme::sans (9.5f, true, 0.3f));
    g.drawText (u8 ("BUILD-UP · UM KNOB · TENSÃO"), juce::Rectangle<float> (knobBase.x - 150.0f, 512.0f, 300.0f, 14.0f), juce::Justification::centred);

    // ---- faixa de controles em chapa preta ----
    painel::chapaPreta (g, { 580.0f, 232.0f, 380.0f, 316.0f });
    drawTitle (g, 598.0f, 244.0f, "AJUSTES ", u8 ("· ATÉ ONDE VAI"));
    g.setColour (theme::dim);
    g.setFont (theme::sans (10.5f));
    g.drawFittedText (u8 ("DROP apaga a chama na hora (20 ms) para a virada; desligue para ela voltar."),
                      juce::Rectangle<int> (598, 304, 340, 30), juce::Justification::topLeft, 2);
}

void ChamaEditor::layoutCanvas()
{
    prevBtn.setBounds (596, 34, 30, 32);
    presetName.setBounds (626, 34, 230, 32);
    nextBtn.setBounds (856, 34, 30, 32);
    saveBtn.setBounds (886, 34, 30, 32);
    bypass.setBounds (924, 34, 32, 32);

    strip.setBounds (40, 80, 920, 136);

    // knob grandão: a base fica em knobBase; a caixa tem folga à direita para a sombra do render
    chamaKnob.setBounds ((int) knobBase.x - 145, (int) knobBase.y - 112, 290, 254);
    ring.setBounds ((int) knobBase.x - 160, (int) knobBase.y - 150, 320, 256);
    ring.setCentre ({ 160.0f, 150.0f });
    stage.setBounds (390, 260, 180, 280);

    dropKey.setBounds (594, 262, 352, 38);
    noise.setBounds (598, 340, 119, 92);
    space.setBounds (728, 340, 119, 92);
    output.setBounds (598, 444, 119, 92);
    meterIn.setBounds  (872, 424, 24, 116);
    meterOut.setBounds (902, 424, 24, 116);
}