#include "PluginEditor.h"

using namespace torno;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuLicenca = 20005;

// rack de 19": a frente tem o layout de 1000 × 580 e as orelhas ficam dos lados
constexpr int ear = 32, rackW = ui::baseW + 2 * ear;
const juce::Colour tinta (0xff2f4a3a);   // cor da frente do TORNO

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

Curve curveOf (const Settings& s)
{
    return makeCurve (s.threshold, s.ratio, s.knee, s.squeeze);
}

// dB com o sinal de menos tipográfico
juce::String dbSigned (float v)
{
    return dbText (v).replace ("-", u8 ("−"));
}

// redução (dB ≥ 0) mostrada como negativa
juce::String grText (float gr)
{
    return gr > 0.05f ? u8 ("−") + juce::String (gr, 1).replaceCharacter ('.', ',') + " dB" : juce::String ("0,0 dB");
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
// HISTÓRICO
// =====================================================================
void HistoryView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    const auto  c = curveOf (proc.readSettings());

    painel::secao (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, u8 ("HISTÓRICO "), u8 ("· INPUT E GAIN REDUCTION, ÚLTIMOS 4 s"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::amber, "THRESHOLD", true);
        legendItem (g, x, theme::coral, "GR", false);
        legendItem (g, x, theme::cream, "INPUT", false);
    }

    const float x0 = 12.0f, x1 = W - 46.0f, top = 26.0f, bot = H - 10.0f;
    const float lvlLo = -60.0f;
    auto yLvl = [top, bot, lvlLo] (float db)
    {
        return bot - (juce::jlimit (lvlLo, 0.0f, db) - lvlLo) / (0.0f - lvlLo) * (bot - top);
    };

    const juce::Rectangle<float> screenR (x0, top, x1 - x0, bot - top);
    real::screen (g, screenR, 3.0f);

    // grade do nível (dBFS), com a escala na direita
    g.setFont (theme::mono (9.0f));
    for (int k = 0; k <= 4; ++k)
    {
        const float db = -12.0f * (float) k;
        const float y  = yLvl (db);
        if (k > 0)
        {
            g.setColour (theme::segOff.withAlpha (0.6f));
            g.fillRect (x0, y, x1 - x0, 1.0f);
        }
        g.setColour (theme::dim);
        g.drawText (k == 0 ? juce::String ("0") : u8 ("−") + juce::String (12 * k),
                    juce::Rectangle<float> (x1 + 4.0f, y - 6.0f, W - x1 - 10.0f, 12.0f), juce::Justification::centredLeft);
    }

    // colunas: mais antiga à esquerda, agora à direita
    const int   wpos = proc.engine.colWrite.load (std::memory_order_acquire);
    const float cw   = (x1 - x0) / (float) historyCols;

    // escala da redução antes de desenhar (degraus fixos, para os números não ficarem pulando);
    // calculada depois, a primeira pintura usava a escala velha e a redução enchia a altura toda
    float maxGr = 0.0f;
    for (int k = 0; k < historyCols; ++k)
        maxGr = juce::jmax (maxGr, proc.engine.colGr[(size_t) k].load (std::memory_order_relaxed));
    grRange = maxGr < 5.0f ? 6.0f : maxGr < 10.0f ? 12.0f : maxGr < 20.0f ? 24.0f : 48.0f;

    for (int k = 0; k < historyCols; ++k)
    {
        const auto  idx = (size_t) ((wpos + k) % historyCols);
        const float lvl = proc.engine.colIn[idx].load (std::memory_order_relaxed);
        const float gr  = proc.engine.colGr[idx].load (std::memory_order_relaxed);
        const float x = x0 + (float) k * cw;

        const float db = juce::Decibels::gainToDecibels (lvl, -100.0f);
        if (db > lvlLo)
        {
            const float yy = yLvl (db);
            g.setColour (theme::cream.withAlpha (0.55f));
            g.fillRect (x, yy, cw + 0.4f, bot - yy);
        }
        if (gr > 0.02f)
        {
            const float h = juce::jmin (1.0f, gr / grRange) * (bot - top);
            g.setColour (theme::coral.withAlpha (0.85f));
            g.fillRect (x, top, cw + 0.4f, h);
        }
    }

    g.setColour (theme::coral);
    g.setFont (theme::mono (9.0f));
    g.drawText (u8 ("GR 0 a −") + juce::String (juce::roundToInt (grRange)) + " dB",
                juce::Rectangle<float> (x0 + 6.0f, top + 3.0f, 200.0f, 11.0f), juce::Justification::centredLeft);

    // limiar (já com o APERTO)
    if (c.threshold > lvlLo)
    {
        const float yT = yLvl (c.threshold);
        g.setColour (theme::amber);
        g.fillRect (x0, yT - 0.5f, x1 - x0, 1.2f);
        g.setFont (theme::mono (9.5f));
        g.drawText ("THRESHOLD " + dbSigned (c.threshold),
                    juce::Rectangle<float> (x1 - 130.0f, yT - 13.0f, 124.0f, 11.0f), juce::Justification::centredRight);
    }

    real::glass (g, screenR, 3.0f);
}

// =====================================================================
// CURVA
// =====================================================================
void CurveView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth();
    const auto  s = proc.readSettings();
    const auto  c = curveOf (s);
    const float lo = -60.0f, hi = 0.0f, pad = 12.0f;
    auto px = [S, lo, hi, pad] (float v) { return pad + (juce::jlimit (lo, hi, v) - lo) / (hi - lo) * (S - 2.0f * pad); };
    auto py = [S, lo, hi, pad] (float v) { return S - pad - (juce::jlimit (lo, hi, v) - lo) / (hi - lo) * (S - 2.0f * pad); };

    // a tela ocupa o componente todo: o gráfico (S × S) e o rodapé com os números embaixo dele
    const auto screenR = getLocalBounds().toFloat().reduced (1.5f);
    real::screen (g, screenR, 3.0f);

    // grade a cada 12 dB
    g.setColour (theme::segOff.withAlpha (0.6f));
    for (int k = 1; k < 5; ++k)
    {
        const float db = -12.0f * (float) k;
        g.fillRect (px (db), pad, 1.0f, S - 2.0f * pad);
        g.fillRect (pad, py (db), S - 2.0f * pad, 1.0f);
    }

    // joelho (faixa violeta) e limiar (âmbar)
    if (c.knee > 0.01f)
    {
        const float k0 = px (c.threshold - c.knee * 0.5f), k1 = px (c.threshold + c.knee * 0.5f);
        if (k1 - k0 > 0.5f)
        {
            g.setColour (theme::violet.withAlpha (0.16f));
            g.fillRect (k0, pad, k1 - k0, S - 2.0f * pad);
        }
    }
    if (c.threshold >= lo)
    {
        g.setColour (theme::amber.withAlpha (0.8f));
        g.fillRect (px (c.threshold), pad, 1.0f, S - 2.0f * pad);
    }

    // diagonal (sem compressão)
    {
        juce::Path diag;
        diag.startNewSubPath (px (lo), py (lo));
        diag.lineTo (px (hi), py (hi));
        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, diag, dashes, 2);
        g.setColour (theme::dim.withAlpha (0.5f));
        g.fillPath (dashed);
    }

    // curva; com compensação, a saída final aparece tracejada em verde
    const int pts = 200;
    const float mk = totalMakeupDb (s, c);
    juce::Path curvePath, makeupPath;
    for (int k = 0; k <= pts; ++k)
    {
        const float x = lo + (hi - lo) * (float) k / (float) pts;
        const float y = curveOut (x, c);
        if (k == 0)
        {
            curvePath.startNewSubPath (px (x), py (y));
            makeupPath.startNewSubPath (px (x), py (y + mk));
        }
        else
        {
            curvePath.lineTo (px (x), py (y));
            makeupPath.lineTo (px (x), py (y + mk));
        }
    }
    if (std::abs (mk) > 0.05f)
    {
        juce::Path dashed;
        const float dashes[] = { 5.0f, 3.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, makeupPath, dashes, 2);
        g.setColour (theme::leaf.withAlpha (0.85f));
        g.fillPath (dashed);
    }
    g.setColour (theme::cream);
    g.strokePath (curvePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ponto de agora: o traço coral é a redução aplicada
    if (liveDet > lo)
    {
        const float xin  = juce::jmin (liveDet, hi);
        const float yout = xin - liveGr;
        if (liveGr > 0.05f)
        {
            g.setColour (theme::coral);
            g.fillRect (px (xin) - 1.0f, py (xin), 2.0f, py (yout) - py (xin));
        }
        g.setColour (theme::coral.withAlpha (0.3f));
        g.fillEllipse (px (xin) - 8.0f, py (yout) - 8.0f, 16.0f, 16.0f);
        g.setColour (theme::coral);
        g.fillEllipse (px (xin) - 4.0f, py (yout) - 4.0f, 8.0f, 8.0f);
    }

    // rodapé
    auto line = [] (const juce::String& a, const juce::String& b, juce::Colour cb)
    {
        juce::AttributedString t;
        t.append (a, theme::mono (10.5f), theme::dim);
        t.append (b, theme::mono (10.5f), cb);
        return t;
    };
    auto l1 = line ("THRESHOLD ", dbSigned (c.threshold), theme::amber);
    auto r1 = line ("RATIO ", ratioText (c.ratio), theme::cream);
    auto l2 = line ("KNEE ", juce::String (c.knee, 1).replaceCharacter ('.', ',') + " dB", theme::violet);
    auto r2 = line ("SC ", hzText (s.hpfHz), theme::ice);
    l1.setJustification (juce::Justification::centredLeft);
    r1.setJustification (juce::Justification::centredRight);
    l2.setJustification (juce::Justification::centredLeft);
    r2.setJustification (juce::Justification::centredRight);
    // rodapé recuado 8 px das bordas, para não encostar no rebaixo da tela
    l1.draw (g, { 8.0f, S + 8.0f, S - 16.0f, 14.0f });
    r1.draw (g, { 8.0f, S + 8.0f, S - 16.0f, 14.0f });
    l2.draw (g, { 8.0f, S + 26.0f, S - 16.0f, 14.0f });
    r2.draw (g, { 8.0f, S + 26.0f, S - 16.0f, 14.0f });

    real::glass (g, screenR, 3.0f);
}

// =====================================================================
// REDUÇÃO
// =====================================================================
void GrReadout::update (float grDb, float dt)
{
    grNow = juce::jmax (grDb, grNow - 24.0f * dt);   // cai 24 dB/s (só a leitura; o som segue a SOLTURA)
    if (grDb >= grMax) { grMax = grDb; holdLeft = 3.0f; }
    else if ((holdLeft -= dt) <= 0.0f) grMax = juce::jmax (grNow, grMax - 12.0f * dt);
}

void GrReadout::paint (juce::Graphics& g)
{
    // a leitura inteira é uma tela embutida; o conteúdo fica recuado 8 px das bordas
    const auto screenR = getLocalBounds().toFloat().reduced (1.5f);
    real::screen (g, screenR, 3.0f);

    const float X = 8.0f, W = (float) getWidth() - 16.0f;
    auto label = [&] (const juce::String& t, float y)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText (t, juce::Rectangle<float> (X, y, 200.0f, 12.0f), juce::Justification::centredLeft);
    };
    auto value = [&] (const juce::String& t, float y, juce::Colour col)
    {
        g.setColour (col);
        g.setFont (theme::mono (11.0f));
        g.drawText (t, juce::Rectangle<float> (X, y, W, 12.0f), juce::Justification::centredRight);
    };

    label ("GAIN REDUCTION", 4.0f);
    value ("MAX " + grText (grMax), 4.0f, grMax > 0.05f ? theme::coral : theme::dim);
    g.setColour (grNow > 0.05f ? theme::coral : theme::dim);
    g.setFont (theme::mono (26.0f));
    g.drawText (grText (grNow), juce::Rectangle<float> (X, 15.0f, W, 28.0f), juce::Justification::centredLeft);

    g.setColour (theme::segOff);
    g.fillRoundedRectangle (X, 45.0f, W, 4.0f, 2.0f);
    g.setColour (theme::coral.withAlpha (0.5f));
    g.fillRoundedRectangle (X, 45.0f, W * juce::jlimit (0.0f, 1.0f, grMax / 24.0f), 4.0f, 2.0f);
    g.setColour (theme::coral);
    g.fillRoundedRectangle (X, 45.0f, W * juce::jlimit (0.0f, 1.0f, grNow / 24.0f), 4.0f, 2.0f);

    label ("MAKEUP APLICADO", 55.0f);
    value (dbSigned (makeupDb), 55.0f, std::abs (makeupDb) > 0.05f ? theme::leaf : theme::dim);

    real::glass (g, screenR, 3.0f);
}

// =====================================================================
// EDITOR
// =====================================================================
TornoEditor::TornoEditor (TornoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), history (p), curve (p),
      // cor do knob = cor do que ele mexe no desenho
      squeezeMacro (p.apvts, id::squeeze,   "APERTO",            true,  theme::coral),
      threshold    (p.apvts, id::threshold, "THRESHOLD",         false, theme::amber),
      ratio        (p.apvts, id::ratio,     "RATIO",             false, theme::cream),
      knee         (p.apvts, id::knee,      "KNEE",              false, theme::violet),
      scHpf        (p.apvts, id::scHpf,     "SC HPF",            false, theme::ice),
      attack       (p.apvts, id::attack,    "ATTACK",            false, theme::coral),
      release      (p.apvts, id::release,   "RELEASE",           false, theme::coral),
      makeup       (p.apvts, id::makeup,    "MAKEUP",            false, theme::leaf),
      output       (p.apvts, id::output,    "GAIN",              false, theme::cream),
      mix          (p.apvts, id::mix),
      listenKey    ("SC LISTEN", theme::ice),
      autoRelKey   ("AUTO RELEASE", theme::coral),
      autoMakeupKey ("AUTO", theme::leaf)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, rackW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &history, &curve, &readout, &squeezeMacro, &threshold, &ratio, &knee, &scHpf, &attack, &release,
             &makeup, &output, &mix, &listenKey, &autoRelKey, &autoMakeupKey, &bypass,
             &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    listenAtt     = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::listen, listenKey);
    autoRelAtt    = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::autoRel, autoRelKey);
    autoMakeupAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::autoMakeup, autoMakeupKey);
    bypassAtt     = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

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
    ativacao.anexar (*this);
}

TornoEditor::~TornoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TornoEditor::showPresetMenu()
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

    juce::Component::SafePointer<TornoEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void TornoEditor::askNewPreset()
{
    juce::Component::SafePointer<TornoEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void TornoEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<TornoEditor> safe (this);
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

void TornoEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) rackW - openScale) > 0.005f)
        setScale (openScale);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    const float gr  = proc.engine.grDb.exchange (0.0f);
    const float det = proc.engine.detDb.exchange (-120.0f);
    readout.update (gr, dt);
    detShown = juce::jmax (det, detShown - 40.0f * dt);   // o ponto desce devagar, sem piscar

    const auto s = proc.readSettings();
    readout.setMakeup (totalMakeupDb (s, curveOf (s)));
    curve.setLive (detShown, readout.grShown());
    history.repaint();
    curve.repaint();
    readout.repaint();

    levelIn  = juce::jmax (proc.meterIn.exchange (0.0f),  levelIn * 0.85f);
    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void TornoEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) rackW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void TornoEditor::resized()
{
    const float scale = (float) getWidth() / (float) rackW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void TornoEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void TornoEditor::drawCanvas (juce::Graphics& g)
{
    painel::rack (g, (float) rackW, (float) ui::baseH, (float) ear, tinta);
    g.addTransform (juce::AffineTransform::translation ((float) ear, 0.0f));   // daqui em diante, o layout da frente

    // ---- logo: as duas castanhas do torno apertando a peça ----
    {
        g.setColour (theme::cream);
        g.fillRoundedRectangle (17.0f, 24.0f, 7.0f, 20.0f, 1.5f);     // castanha esquerda
        g.fillRoundedRectangle (38.0f, 24.0f, 7.0f, 20.0f, 1.5f);     // castanha direita
        g.fillRect (13.0f, 33.0f, 4.0f, 2.0f);                         // fuso
        g.fillRect (45.0f, 33.0f, 4.0f, 2.0f);
        g.setColour (theme::coral);
        g.fillRoundedRectangle (25.5f, 28.0f, 11.0f, 12.0f, 1.5f);    // a peça apertada
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("TORNO");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 220.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 ("COMPRESSOR"), tag2 (u8 ("MIX · PICO"));
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

    // ---- latência (zero: sem lookahead; informada ao host do mesmo jeito) ----
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

    drawTitle (g, 22.0f,  226.0f, "CURVA ", u8 ("· INPUT × OUTPUT (dB)"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), u8 ("COMPRESSÃO"));
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "ENVELOPE E GAIN");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 110.0f }, "MACRO APERTO",
              u8 ("Desce o THRESHOLD e sobe o RATIO juntos. O que ele tira aparece em coral no histórico."));

    // separador entre o tempo e a leitura da redução
    g.setColour (theme::line);
    g.fillRect (562.0f, 380.0f, 238.0f, 1.0f);
    g.fillRect (562.0f, 462.0f, 238.0f, 1.0f);
}

void TornoEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    history.setBounds (12, 66, 976, 140);
    curve.setBounds (22, 246, 240, 290);

    squeezeMacro.setBounds (294, 246, 92, 92);
    threshold.setBounds    (294, 376, 119, 92);
    ratio.setBounds        (413, 376, 119, 92);
    knee.setBounds         (294, 468, 119, 92);
    scHpf.setBounds        (413, 468, 119, 92);

    attack.setBounds     (562, 244, 119, 92);
    release.setBounds    (681, 244, 119, 92);
    autoRelKey.setBounds (692, 340, 98, 32);
    readout.setBounds    (566, 388, 230, 72);
    makeup.setBounds     (562, 468, 119, 92);
    autoMakeupKey.setBounds (692, 490, 98, 32);

    listenKey.setBounds (830, 246, 148, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (883, 446, 94, 55);
    meterOut.setBounds (883, 503, 94, 55);

    for (auto* c : canvas.getChildren())   // a frente começa depois da orelha esquerda
        c->setTopLeftPosition (c->getX() + ear, c->getY());
}
