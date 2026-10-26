#include "PluginEditor.h"

using namespace navalha;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004;

// rack de 19": a frente tem o layout de 1000 × 580 e as orelhas ficam dos lados
constexpr int ear = 32, rackW = ui::baseW + 2 * ear;
const juce::Colour tinta (0xff5a2a33);   // cor da frente do NAVALHA

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
    painel::secao (g, r);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

float param (NavalhaProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

Shape currentShape (NavalhaProcessor& p)
{
    return makeShape (param (p, id::ceiling), param (p, id::knee) * 0.01f, param (p, id::asym) * 0.01f);
}

juce::String dbfs (float gain)
{
    return gain < 1.0e-5f ? juce::String (u8 ("−∞ dB")) : dbText (juce::Decibels::gainToDecibels (gain)).replace ("-", u8 ("−"));
}

void legendItem (juce::Graphics& g, float& x, juce::Colour c, const juce::String& text, bool line)
{
    const auto f = theme::sans (8.0f, true, 0.18f);
    const float tw = textWidth (f, text);
    x -= tw;
    g.setColour (theme::dim);
    g.setFont (f);
    g.drawText (text, juce::Rectangle<float> (x, 7.0f, tw + 2.0f, 12.0f), juce::Justification::centredLeft);
    x -= 12.0f;
    g.setColour (c);
    if (line) g.fillRect (x - 2.0f, 12.0f, 12.0f, 2.0f);
    else      g.fillRoundedRectangle (x, 9.0f, 8.0f, 8.0f, 2.0f);
    x -= 12.0f;
}
} // namespace

// =====================================================================
// FORMA DE ONDA
// =====================================================================
void WaveStrip::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    const auto  shape = currentShape (proc);
    const bool  asymOn = param (proc, id::asym) > 0.5f;

    painel::secao (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, "WAVEFORM ", u8 ("· O QUE PASSA E O QUE A NAVALHA CORTA, AO VIVO"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::amber, "CEILING", true);
        legendItem (g, x, theme::coral, "CORTADO", false);
        legendItem (g, x, theme::cream, "PASSA", false);
    }

    const float x0 = 12.0f, x1 = W - 12.0f, top = 26.0f, bot = H - 10.0f;
    const float mid = (top + bot) * 0.5f, half = (bot - top) * 0.5f - 2.0f;
    const juce::Rectangle<float> screenR (x0, top, x1 - x0, bot - top);
    real::screen (g, screenR, 3.0f);

    // mais antigo à esquerda, agora à direita
    const int   wpos = proc.engine.colWrite.load (std::memory_order_acquire);
    const float cw   = (x1 - x0) / (float) historyCols;
    float maxIn = 0.0f;
    for (int k = 0; k < historyCols; ++k)
    {
        const auto  idx = (size_t) ((wpos + k) % historyCols);
        const float a = proc.engine.colIn[idx].load (std::memory_order_relaxed);
        const float b = proc.engine.colOut[idx].load (std::memory_order_relaxed);
        maxIn = juce::jmax (maxIn, a);
        const float ha = juce::jmin (1.0f, a / range) * half, hb = juce::jmin (1.0f, b / range) * half;
        const float x  = x0 + (float) k * cw;
        if (ha > hb + 0.5f)
        {
            g.setColour (theme::coral.withAlpha (0.9f));
            g.fillRect (x, mid - ha, cw + 0.4f, ha - hb);
            g.fillRect (x, mid + hb, cw + 0.4f, ha - hb);
        }
        g.setColour (theme::cream.withAlpha (0.78f));
        g.fillRect (x, mid - hb, cw + 0.4f, juce::jmax (1.0f, hb * 2.0f));
    }

    // tetos (o negativo em azul quando há assimetria)
    const float yPos = mid - juce::jmin (1.0f, shape.ceiling / range) * half;
    const float yNeg = mid + juce::jmin (1.0f, shape.negCeiling / range) * half;
    g.setColour (theme::amber);
    g.fillRect (x0, yPos - 0.5f, x1 - x0, 1.2f);
    g.setColour (asymOn ? theme::ice : theme::amber);
    g.fillRect (x0, yNeg - 0.5f, x1 - x0, 1.2f);
    g.setColour (theme::amber);
    g.setFont (theme::mono (9.5f));
    g.drawText ("CEILING " + dbText (param (proc, id::ceiling)).replace ("-", u8 ("−")),
                juce::Rectangle<float> (x0 + 6.0f, yPos - 13.0f, 120.0f, 11.0f), juce::Justification::centredLeft);

    real::glass (g, screenR, 3.0f);

    // escala acompanha os picos devagar (sem pular a cada frame)
    const float target = juce::jlimit (shape.ceiling * 1.25f, shape.ceiling * 8.0f, maxIn * 1.1f);
    range += (target - range) * 0.08f;
}

// =====================================================================
// CURVA
// =====================================================================
void CurveView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth();
    const auto  shape = currentShape (proc);
    const bool  asymOn = param (proc, id::asym) > 0.5f;
    const float R = shape.ceiling * 1.8f;   // eixos: ±R (entrada e saída)
    const float half = S * 0.5f - 12.0f;
    auto px = [=] (float v) { return S * 0.5f + v / R * half; };
    auto py = [=] (float v) { return S * 0.5f - v / R * half; };

    // tela quadrada no topo do componente (o rodapé fica embaixo); 1,5 px para dentro: o rebaixo cabe no componente
    const juce::Rectangle<float> screenR (1.5f, 1.5f, S - 3.0f, S - 3.0f);
    real::screen (g, screenR, 4.0f);

    // eixos e a diagonal (sem corte)
    g.setColour (theme::segOff);
    g.fillRect (12.0f, S * 0.5f, S - 24.0f, 1.0f);
    g.fillRect (S * 0.5f, 12.0f, 1.0f, S - 24.0f);
    {
        juce::Path diag;
        diag.startNewSubPath (px (-R), py (-R));
        diag.lineTo (px (R), py (R));
        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, diag, dashes, 2);
        g.setColour (theme::dim.withAlpha (0.5f));
        g.fillPath (dashed);
    }

    // tetos
    g.setColour (theme::amber.withAlpha (0.8f));
    g.fillRect (12.0f, py (shape.ceiling), S - 24.0f, 1.0f);
    g.setColour ((asymOn ? theme::ice : theme::amber).withAlpha (0.8f));
    g.fillRect (12.0f, py (-shape.negCeiling), S - 24.0f, 1.0f);

    // curva
    juce::Path curvePath;
    const int pts = 200;
    for (int k = 0; k <= pts; ++k)
    {
        const float x = -R + 2.0f * R * (float) k / (float) pts;
        const float y = clipSample (x, shape);
        if (k == 0) curvePath.startNewSubPath (px (x), py (y)); else curvePath.lineTo (px (x), py (y));
    }
    g.setColour (theme::cream);
    g.strokePath (curvePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pico de agora: o trecho coral é o que está sendo cortado
    if (live > 1.0e-4f)
    {
        const float xin = juce::jmin (live, R);
        const float y   = clipSample (xin, shape);
        if (xin - y > 1.0e-4f)
        {
            g.setColour (theme::coral);
            g.fillRect (px (xin) - 1.0f, py (xin), 2.0f, py (y) - py (xin));
        }
        g.setColour (theme::coral.withAlpha (0.3f));
        g.fillEllipse (px (xin) - 8.0f, py (y) - 8.0f, 16.0f, 16.0f);
        g.setColour (theme::coral);
        g.fillEllipse (px (xin) - 4.0f, py (y) - 4.0f, 8.0f, 8.0f);
    }

    real::glass (g, screenR, 4.0f);

    // rodapé
    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    auto sl = line ("CEILING ", dbText (param (proc, id::ceiling)).replace ("-", u8 ("−")));
    auto sr = line ("CORTE ", cut > 0.05f ? u8 ("−") + juce::String (cut, 1).replaceCharacter ('.', ',') + " dB" : juce::String ("0,0 dB"));
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
    sr.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
}

// =====================================================================
// ANÁLISE
// =====================================================================
void StatsView::update (float cutDb, float share, float peakIn, float peakOut, float dt)
{
    cutNow = juce::jmax (cutDb, cutNow - 18.0f * dt);   // cai 18 dB/s
    if (cutDb >= cutMax) { cutMax = cutDb; holdLeft = 3.0f; }
    else if ((holdLeft -= dt) <= 0.0f) cutMax = juce::jmax (cutNow, cutMax - 12.0f * dt);
    shareSm += (share - shareSm) * juce::jmin (1.0f, dt * 6.0f);
    pkIn  = juce::jmax (peakIn,  pkIn  * std::exp (-dt * 3.0f));
    pkOut = juce::jmax (peakOut, pkOut * std::exp (-dt * 3.0f));
}

void StatsView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    auto label = [&g] (const juce::String& t, float y)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText (t, juce::Rectangle<float> (0.0f, y, 200.0f, 12.0f), juce::Justification::centredLeft);
    };
    auto value = [&g, W] (const juce::String& t, float y, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (theme::mono (11.0f));
        g.drawText (t, juce::Rectangle<float> (0.0f, y, W, 12.0f), juce::Justification::centredRight);
    };
    auto bar = [&g, W] (float frac, float y, juce::Colour c)
    {
        g.setColour (theme::segOff);
        g.fillRoundedRectangle (0.0f, y, W, 4.0f, 2.0f);
        g.setColour (c);
        g.fillRoundedRectangle (0.0f, y, W * juce::jlimit (0.0f, 1.0f, frac), 4.0f, 2.0f);
    };
    auto dbTxt = [] (float cut) { return cut > 0.05f ? u8 ("−") + juce::String (cut, 1).replaceCharacter ('.', ',') + " dB" : juce::String ("0,0 dB"); };

    label ("CORTANDO AGORA", 0.0f);
    g.setColour (cutNow > 0.05f ? theme::coral : theme::dim);
    g.setFont (theme::mono (30.0f));
    g.drawText (dbTxt (cutNow), juce::Rectangle<float> (0.0f, 14.0f, W, 36.0f), juce::Justification::centredLeft);
    bar (cutNow / 12.0f, 56.0f, theme::coral);

    label ("MAX (3 s)", 78.0f);
    value (dbTxt (cutMax), 78.0f, cutMax > 0.05f ? theme::coral : theme::dim);
    bar (cutMax / 12.0f, 94.0f, theme::coral.withAlpha (0.6f));

    label ("AMOSTRAS CORTADAS", 116.0f);
    value (juce::String (juce::roundToInt (shareSm * 100.0f)) + "%", 116.0f, theme::cream);
    bar (shareSm, 132.0f, theme::amber);

    label (u8 ("PEAK PÓS-DRIVE"), 154.0f);
    value (dbfs (pkIn), 154.0f, theme::cream);
    bar (juce::jmap (juce::Decibels::gainToDecibels (pkIn, -48.0f), -48.0f, 12.0f, 0.0f, 1.0f), 170.0f, theme::cream.withAlpha (0.7f));

    label ("OUTPUT PEAK", 192.0f);
    value (dbfs (pkOut), 192.0f, theme::cream);
    bar (juce::jmap (juce::Decibels::gainToDecibels (pkOut, -48.0f), -48.0f, 12.0f, 0.0f, 1.0f), 208.0f, theme::cream.withAlpha (0.7f));

    g.setColour (theme::dim);
    g.setFont (theme::sans (10.5f));
    g.drawFittedText (u8 ("O pico na saída pode passar um pouco do CEILING entre amostras (filtro do oversampling)."),
                      juce::Rectangle<int> (0, 232, (int) W, 40), juce::Justification::topLeft, 3);
}

// =====================================================================
// EDITOR
// =====================================================================
NavalhaEditor::NavalhaEditor (NavalhaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), curve (p),
      // cor do knob = cor do que ele mexe no desenho
      driveMacro (p.apvts, id::drive,   "DRIVE",      true,  theme::coral),
      ceiling    (p.apvts, id::ceiling, "CEILING",      false, theme::amber),
      knee       (p.apvts, id::knee,    "KNEE",         false, theme::cream),
      asym       (p.apvts, id::asym,    "ASYMMETRY",    false, theme::ice),
      link       (p.apvts, id::link,    "LINK L/R",     false, theme::cream),
      output     (p.apvts, id::output,  "GAIN",         false, theme::cream),
      mix        (p.apvts, id::mix),
      deltaKey   (u8 ("SÓ O CORTE"), theme::coral)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, rackW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &curve, &stats, &driveMacro, &ceiling, &knee, &asym, &link, &output, &mix, &deltaKey,
             &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    deltaAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::delta, deltaKey);
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
    setResizeLimits (juce::roundToInt (rackW * ui::minScale), juce::roundToInt (ui::baseH * ui::minScale),
                     juce::roundToInt (rackW * ui::maxScale), juce::roundToInt (ui::baseH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) rackW / (double) ui::baseH);
    setScale (openScale);
    startTimerHz (30);
}

NavalhaEditor::~NavalhaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NavalhaEditor::showPresetMenu()
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

    juce::Component::SafePointer<NavalhaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void NavalhaEditor::askNewPreset()
{
    juce::Component::SafePointer<NavalhaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void NavalhaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<NavalhaEditor> safe (this);
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

void NavalhaEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) rackW - openScale) > 0.005f)
        setScale (openScale);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    stats.update (proc.engine.cutDb.exchange (0.0f), proc.engine.clipShare.exchange (0.0f),
                  proc.engine.peakIn.exchange (0.0f), proc.engine.peakOut.exchange (0.0f), dt);
    curve.setLive (stats.peakInShown(), stats.cutShown());
    strip.repaint();
    curve.repaint();
    stats.repaint();

    levelIn  = juce::jmax (proc.meterIn.exchange (0.0f),  levelIn * 0.85f);
    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void NavalhaEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) rackW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void NavalhaEditor::resized()
{
    const float scale = (float) getWidth() / (float) rackW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void NavalhaEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void NavalhaEditor::drawCanvas (juce::Graphics& g)
{
    painel::rack (g, (float) rackW, (float) ui::baseH, (float) ear, tinta);
    g.addTransform (juce::AffineTransform::translation ((float) ear, 0.0f));   // daqui em diante, o layout da frente

    // ---- logo: lâmina de barbear ----
    {
        const juce::Rectangle<float> blade (17.0f, 25.0f, 28.0f, 18.0f);
        g.setColour (theme::cream);
        g.fillRoundedRectangle (blade, 2.5f);
        g.setColour (theme::bg);
        g.fillRoundedRectangle (blade.reduced (8.0f, 7.0f), 1.5f);
        g.fillEllipse (blade.getX() + 4.5f, blade.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.fillEllipse (blade.getRight() - 9.5f, blade.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.setColour (theme::coral);
        g.fillRect (blade.getX(), blade.getBottom() - 2.0f, blade.getWidth(), 2.0f);   // o fio
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("NAVALHA");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 220.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 ("CLIPPER"), tag2 (u8 ("4× OVERSAMPLING"));
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

    // ---- latência (o Ableton compensa sozinho) ----
    {
        const double ms = 1000.0 * (double) proc.getLatencySamples() / juce::jmax (1.0, proc.rate.load());
        juce::AttributedString s;
        s.append ("LATENCY ", theme::mono (11.5f), theme::dim);
        s.append (juce::String (ms, 2).replaceCharacter ('.', ',') + " ms", theme::mono (11.5f), theme::cream);
        juce::TextLayout tl;
        tl.createLayout (s, 400.0f);
        const float w = tl.getWidth(), right = 604.0f;
        tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });
    }

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "CURVA ", u8 ("· INPUT × OUTPUT"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "CORTE");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), u8 ("ANÁLISE"));
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 110.0f }, "MACRO DRIVE",
              u8 ("Quanto o som é empurrado contra o CEILING. O que passa dele vira coral na forma de onda."));
}

void NavalhaEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    curve.setBounds (22, 246, 240, 264);

    driveMacro.setBounds (294, 246, 92, 92);
    ceiling.setBounds    (294, 376, 119, 92);
    knee.setBounds       (413, 376, 119, 92);
    asym.setBounds       (294, 468, 119, 92);
    link.setBounds       (413, 468, 119, 92);

    stats.setBounds (566, 248, 230, 300);

    deltaKey.setBounds (830, 246, 148, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (883, 446, 94, 55);
    meterOut.setBounds (883, 503, 94, 55);

    for (auto* c : canvas.getChildren())   // a frente começa depois da orelha esquerda
        c->setTopLeftPosition (c->getX() + ear, c->getY());
}
