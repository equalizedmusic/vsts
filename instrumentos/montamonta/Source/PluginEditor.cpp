#include "PluginEditor.h"
#include "Draw.h"

using namespace mm;
using namespace md;

namespace
{
constexpr int sizeMenuBase = 10000;
constexpr int menuUnlockAll = 20001, menuRestart = 20002, menuLicenca = 20007;

juce::Image holeTile()
{
    juce::Image img (juce::Image::ARGB, 48, 48, true);
    juce::Graphics g (img);
    g.setColour (juce::Colour (0xff1b1d25));
    g.fillEllipse (19.0f, 19.0f, 10.0f, 10.0f);
    g.setColour (juce::Colour (0xff050608));
    g.fillEllipse (20.0f, 20.5f, 8.0f, 8.0f);
    return img;
}
} // namespace

MontaMontaEditor::MontaMontaEditor (MontaMontaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setWantsKeyboardFocus (false);
    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, mu::baseW, mu::baseH);
    addAndMakeVisible (canvas);

    for (int m = 0; m < numModules; ++m)
    {
        panels[(size_t) m] = std::make_unique<ModulePanel> (*this, proc, m);
        const auto r = slotRect (m);
        panels[(size_t) m]->setBounds (RF (r.getX(), r.getY() - faceY, slotW + 30.0f, slotH + faceY).toNearestInt());
        canvas.addChildComponent (*panels[(size_t) m]);
    }
    cablesLayer.setBounds (rackArea.toNearestInt());
    canvas.addChildComponent (cablesLayer);

    tray.setBounds (trayArea.toNearestInt());
    canvas.addAndMakeVisible (tray);
    info.setBounds (cardArea.toNearestInt());
    canvas.addAndMakeVisible (info);
    scope.setBounds (808, 628, 150, 88);
    canvas.addAndMakeVisible (scope);
    keys.setBounds (972, 628, 200, 88);
    canvas.addAndMakeVisible (keys);
    dots.setBounds (577, 28, 134, 22);
    canvas.addAndMakeVisible (dots);

    menuBtn.setBounds (306, 27, 54, 24);
    menuBtn.onClick = [this] { showMenu(); };
    modeFit.setBounds (754, 36, 86, 22);
    modeFit.onClick = [this] { setMode (false); };
    modeCables.setBounds (846, 36, 86, 22);
    modeCables.onClick = [this] { setMode (true); };
    langPt.setBounds (966, 36, 38, 22);
    langPt.onClick = [this] { proc.setLanguage (0); syncPanels(); };
    langEn.setBounds (1008, 36, 38, 22);
    langEn.onClick = [this] { proc.setLanguage (1); syncPanels(); };
    for (auto* b : { &menuBtn, &modeFit, &modeCables, &langPt, &langEn })
        canvas.addAndMakeVisible (*b);

    nextBtn.setBounds (633, 540, 200, 26);
    nextBtn.play = true;
    nextBtn.onClick = [this] { setLesson (juce::jmin (numLessons, proc.patch.lesson + 1)); };
    canvas.addChildComponent (nextBtn);

    wireDefault.setBounds (28, 430, 186, 30);
    wireDefault.onClick = [this]
    {
        proc.patch.wires = { { vco1, inMix1 }, { vco2, inMix2 }, { mixer, inVcf }, { envMod, inVcfCv }, { vcf, inAmp }, { envAmp, inOut } };
        patchEdited();
    };
    wireClear.setBounds (28, 468, 186, 30);
    wireClear.onClick = [this] { proc.patch.wires.clear(); patchEdited(); };
    canvas.addChildComponent (wireDefault);
    canvas.addChildComponent (wireClear);

    dragLayer.setBounds (0, 0, mu::baseW, mu::baseH);
    canvas.addAndMakeVisible (dragLayer);

    lastPatchVersion = proc.patchVersion.load();
    noteMark = proc.notesPlayed.load();
    const int need = nextNeeded();
    select (need >= 0 ? need : vco1);
    syncPanels();
    updateReach();

    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) mu::baseW * mu::minScale), juce::roundToInt ((float) mu::baseH * mu::minScale),
                     juce::roundToInt ((float) mu::baseW * mu::maxScale), juce::roundToInt ((float) mu::baseH * mu::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) mu::baseW / (double) mu::baseH);
    setScale (openScale);
    startTimerHz (30);
    ativacao.anexar (*this);
}

MontaMontaEditor::~MontaMontaEditor()
{
    stopTimer();
    proc.keyboard.allNotesOff (1);
}

juce::Rectangle<float> MontaMontaEditor::slotRect (int m)
{
    // peças encostadas (sem folga): o pino de uma entra no recorte da vizinha
    static const float xs[] = { 285.0f, 397.0f, 509.0f, 621.0f, 733.0f };
    constexpr float y1 = 122.0f, y2 = 348.0f;
    switch (m)
    {
        case vco1:   return { xs[0], y1, slotW, slotH };
        case mixer:  return { xs[1], y1, slotW, slotH };
        case vcf:    return { xs[2], y1, slotW, slotH };
        case envAmp: return { xs[3], y1, slotW, slotH };
        case output: return { xs[4], y1, slotW, slotH };
        case vco2:   return { xs[1], y2, slotW, slotH };
        default:     return { xs[2], y2, slotW, slotH };
    }
}

juce::Point<float> MontaMontaEditor::jackPos (int m, int in)
{
    const auto r = slotRect (m);
    const float y = r.getBottom() - 22.0f;
    if (in < 0) return { r.getRight() - 18.0f, y };
    switch (in)
    {
        case inMix2:  return { r.getX() + 46.0f, y };
        case inVcfCv: return { r.getX() + 56.0f, y };
        default:      return { r.getX() + 18.0f, y };
    }
}

void MontaMontaEditor::select (int m)
{
    info.setModule (m);
    keys.colour = mu::moduleColour (m);
}

void MontaMontaEditor::showMessage (const juce::String& pt, const juce::String& en, juce::Colour c)
{
    msgPt = pt;
    msgEn = en;
    msgColour = c;
    msgUntil = juce::Time::getMillisecondCounterHiRes() + 6000.0;
    nextBtn.setVisible (false);
    canvas.repaint (tipArea.expanded (2.0f).toNearestInt());
}

int MontaMontaEditor::nextNeeded() const
{
    const auto& p = proc.patch;
    if (p.cables) return -1;
    for (int m : { (int) vco1, (int) mixer, (int) vcf, (int) envAmp, (int) output, (int) envMod, (int) vco2 })
        if (inLesson (p.lesson, m) && ! p.placed[(size_t) m]) return m;
    return -1;
}

// ---------------- arrastar peças ----------------
void MontaMontaEditor::beginDrag (int m, bool fromRack, const juce::MouseEvent& e, juce::Point<float> grabOffset)
{
    drag.module = m;
    drag.fromRack = fromRack;
    dragLayer.module = m;
    dragLayer.lang = lang();
    dragLayer.grab = grabOffset;
    dragLayer.pos = e.getEventRelativeTo (&canvas).position;
    select (m);
    syncPanels();
}

void MontaMontaEditor::dragMove (const juce::MouseEvent& e)
{
    if (drag.module < 0) return;
    auto area = [this] { return RF (dragLayer.pos.x - dragLayer.grab.x, dragLayer.pos.y - dragLayer.grab.y, slotW, slotH).expanded (40.0f); };
    const auto old = area();
    dragLayer.pos = e.getEventRelativeTo (&canvas).position;
    dragLayer.repaint (old.getUnion (area()).toNearestInt());
}

void MontaMontaEditor::endDrag (const juce::MouseEvent& e)
{
    const int m = drag.module;
    if (m < 0) return;
    dragLayer.pos = e.getEventRelativeTo (&canvas).position;
    const auto centre = RF (dragLayer.pos.x - dragLayer.grab.x, dragLayer.pos.y - dragLayer.grab.y, slotW, slotH).getCentre();
    auto& patch = proc.patch;

    int target = -1;
    for (int s = 0; s < numModules; ++s)
        if (slotRect (s).expanded (14.0f).contains (centre)) { target = s; break; }

    if (target == m)
        patch.placed[(size_t) m] = true;
    else if (target >= 0)
    {
        const auto& t = pieceText (m);
        showMessage ((m == output ? "A " : "O ") + pieceName (m, 0) + u8 (" não encaixa aqui: o lugar ") + (m == output ? "dela" : "dele")
                         + u8 (" é ") + juce::String::fromUTF8 (t.where.pt) + ".",
                     "The " + pieceName (m, 1) + " does not fit here: its place is " + t.where.en + ".", mu::warn);
        if (drag.fromRack) patch.placed[(size_t) m] = true;   // volta para o lugar
    }
    else if (drag.fromRack)
        patch.placed[(size_t) m] = false;                     // largou fora do rack: volta para a bandeja

    drag.module = -1;
    dragLayer.module = -1;
    dragLayer.repaint();
    patchEdited();
}

void MontaMontaEditor::removeFromRack (int m)
{
    proc.patch.placed[(size_t) m] = false;
    patchEdited();
}

void MontaMontaEditor::patchEdited()
{
    proc.patchChanged();
    syncPanels();
    updateReach();
}

// ---------------- lições e modo ----------------
void MontaMontaEditor::setLesson (int l)
{
    l = juce::jlimit (1, juce::jmin (numLessons, proc.unlockedLesson()), l);
    auto& p = proc.patch;
    p.cables = false;
    p.lesson = l;
    for (int m = 0; m < numModules; ++m)
        p.placed[(size_t) m] = l == freeLesson || (p.placed[(size_t) m] && inLesson (l, m));
    noteMark = proc.notesPlayed.load();
    pathComplete = false;
    lessonDone = false;
    msgUntil = 0.0;
    const int need = nextNeeded();
    if (need >= 0) select (need);
    patchEdited();
}

void MontaMontaEditor::setMode (bool cables)
{
    if (cables && ! proc.cablesUnlocked())
    {
        showMessage (u8 ("Os CABOS abrem quando você terminar a lição 5: primeiro a gente aprende a montar pelos encaixes."),
                     "CABLES open when you finish lesson 5: first you learn to build with the puzzle fit.", mu::dim);
        return;
    }
    proc.patch.cables = cables;
    noteMark = proc.notesPlayed.load();
    pathComplete = false;
    msgUntil = 0.0;
    patchEdited();
}

void MontaMontaEditor::syncPanels()
{
    const auto& p = proc.patch;
    const int l = lang();
    for (int m = 0; m < numModules; ++m)
    {
        auto& panel = *panels[(size_t) m];
        const bool show = p.present (m) && ! p.isBypass (m) && ! p.isHidden (m);
        panel.setVisible (show);
        panel.setAlpha (drag.module == m && drag.fromRack ? 0.25f : 1.0f);
        panel.setCableMode (p.cables);
        panel.setLang (l);
    }
    cablesLayer.setVisible (p.cables);
    wireDefault.setVisible (p.cables);
    wireClear.setVisible (p.cables);
    wireDefault.setButtonText (l == 0 ? u8 ("LIGAR O PADRÃO") : juce::String ("CLASSIC WIRING"));
    wireClear.setButtonText (l == 0 ? "SOLTAR TODOS" : "UNPLUG ALL");
    nextBtn.setButtonText (l == 0 ? u8 ("PRÓXIMA LIÇÃO") : juce::String ("NEXT LESSON"));
    modeFit.setButtonText (l == 0 ? "ENCAIXE" : "PUZZLE");
    modeCables.setButtonText (l == 0 ? "CABOS" : "CABLES");
    modeFit.lit = ! p.cables;
    modeCables.lit = p.cables;
    modeCables.locked = ! proc.cablesUnlocked();
    langPt.lit = l == 0;
    langEn.lit = l == 1;
    for (auto* b : { &modeFit, &modeCables, &langPt, &langEn, &nextBtn, &wireDefault, &wireClear })
        b->repaint();
    tray.repaint();
    info.repaint();
    dots.repaint();
    cablesLayer.repaint();
    canvas.repaint();
}

// até onde o som chega, o que o osciloscópio mostra e se a lição terminou
void MontaMontaEditor::updateReach()
{
    const auto& p = proc.patch;
    const auto g = p.build();
    const bool noise = param (proc, id::mixNoiseOn) > 0.5f && param (proc, id::mixNoise) > 0.05f;
    reach = computeReach (p, g, noise);

    // osciloscópio: a SAÍDA quando o som sai; senão a peça mais longe da fonte que ainda tem som
    std::array<int, numModules> depth {};
    for (int o = 0; o < g.numOrder; ++o)
    {
        const int m = g.order[(size_t) o];
        for (int in = 0; in < numInputs; ++in)
            if (inputModule (in) == m && ! inputIsCv (in) && g.src[(size_t) in] >= 0)
                depth[(size_t) m] = juce::jmax (depth[(size_t) m], depth[(size_t) g.src[(size_t) in]] + 1);
    }
    int pr = -1;
    if (reach.atOutput)
        pr = output;
    else
        for (int o = 0; o < g.numOrder; ++o)
        {
            const int m = g.order[(size_t) o];
            if (m != envMod && reach.sound[(size_t) m] && (pr < 0 || depth[(size_t) m] >= depth[(size_t) pr])) pr = m;
        }
    probeModule = pr;
    proc.probe = pr;
    scope.live = pr >= 0;
    scope.colour = pr >= 0 ? mu::moduleColour (pr) : mu::dim;
    const int l = lang();
    if (pr < 0)
        scope.title = l == 0 ? "SEM SINAL" : "NO SIGNAL";
    else if (l == 0)
        scope.title = (pr == output ? u8 ("SINAL NA ") : juce::String ("SINAL NO ")) + pieceName (pr, 0);
    else
        scope.title = "SIGNAL AT " + pieceName (pr, 1);

    // lição: tudo encaixado, o som saindo e uma nota tocada depois disso
    if (! p.cables && p.lesson < freeLesson && ! lessonDone)
    {
        bool all = true;
        for (int m = 0; m < numModules; ++m)
            if (inLesson (p.lesson, m) && ! p.placed[(size_t) m]) all = false;
        if (all && reach.atOutput)
        {
            if (! pathComplete) { pathComplete = true; noteMark = proc.notesPlayed.load(); }
            if (proc.notesPlayed.load() > noteMark)
            {
                lessonDone = true;
                proc.unlockUpTo (p.lesson + 1);
                msgUntil = 0.0;
                syncPanels();
            }
        }
        else
            pathComplete = false;
    }
    const bool showNext = lessonDone && ! p.cables && p.lesson < freeLesson && msgUntil <= 0.0;
    if (nextBtn.isVisible() != showNext) nextBtn.setVisible (showNext);

    // a faixa de baixo e a dica só redesenham quando algo muda
    juce::String key;
    for (int m = 0; m < numModules; ++m) key << (reach.sound[(size_t) m] ? "1" : "0") << (p.present (m) ? "p" : "-");
    key << (int) p.cables << p.lesson << (int) lessonDone << l << (int) p.wires.size() << (msgUntil > 0.0 ? "m" : "");
    if (key != lastPathKey)
    {
        lastPathKey = key;
        canvas.repaint (stripArea.toNearestInt());
        canvas.repaint (tipArea.expanded (2.0f).toNearestInt());
    }
}

void MontaMontaEditor::timerCallback()
{
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) mu::baseW - openScale) > 0.005f)
        setScale (openScale);

    if (proc.patchVersion.load() != lastPatchVersion)
    {
        lastPatchVersion = proc.patchVersion.load();
        lessonDone = false;
        pathComplete = false;
        syncPanels();
    }

    if (msgUntil > 0.0 && juce::Time::getMillisecondCounterHiRes() >= msgUntil)
    {
        msgUntil = 0.0;
        canvas.repaint (tipArea.expanded (2.0f).toNearestInt());
    }
    updateReach();

    level = juce::jmax (proc.meterOut.exchange (0.0f), level * 0.85f);
    if (level < 1.0e-5f) level = 0.0f;
    if (std::abs (level - shownLevel) > 0.002f || (level == 0.0f && shownLevel != 0.0f))
    {
        shownLevel = level;
        canvas.repaint (1060, 14, 130, 62);
        panels[output]->setLevel (level);
        if (info.getModule() == output) info.refresh();
    }

    if (proc.demoPlaying() != demoShown)
    {
        demoShown = proc.demoPlaying();
        info.repaint();
    }
    scope.repaint();
    keys.repaint();
}

// ---------------- menu ----------------
void MontaMontaEditor::showMenu()
{
    const int l = lang();
    juce::PopupMenu menu, sizes;
    const float current = proc.uiScale.load();
    for (int pct : { 75, 100, 125, 150, 175, 200 })
        sizes.addItem (sizeMenuBase + pct, juce::String (pct) + "%", true, std::abs (current * 100.0f - (float) pct) < 1.0f);
    menu.addSubMenu (l == 0 ? "Tamanho da janela" : "Window size", sizes);
    menu.addSeparator();
    menu.addItem (menuUnlockAll, l == 0 ? u8 ("Liberar todas as lições e os cabos") : juce::String ("Unlock all lessons and cables"),
                  proc.unlockedLesson() < numLessons);
    menu.addItem (menuRestart, l == 0 ? u8 ("Recomeçar as lições do zero") : juce::String ("Restart the lessons from scratch"));
    menu.addSeparator();
    menu.addItem (menuLicenca, l == 0 ? u8 ("Licença…") : u8 ("License…"));
   #ifdef JucePlugin_VersionString
    menu.addSeparator();
    menu.addItem (sizeMenuBase - 1, juce::String ("MONTA-MONTA ") + JucePlugin_VersionString, false, false);
   #endif

    juce::Component::SafePointer<MontaMontaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuBtn).withParentComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& ed = *safe;
        if (r > sizeMenuBase && r <= sizeMenuBase + 200)
        {
            ed.setScale ((float) (r - sizeMenuBase) / 100.0f);
            ed.proc.uiScale = (float) (r - sizeMenuBase) / 100.0f;
        }
        else if (r == menuUnlockAll)
        {
            ed.proc.unlockUpTo (numLessons);
            ed.syncPanels();
        }
        else if (r == menuRestart)
        {
            ed.proc.resetProgress();
            ed.proc.patch = Patch();
            ed.setLesson (1);
        }
        else if (r == menuLicenca)
            ed.ativacao.mostrarInfo();
    });
}

// ---------------- tamanho ----------------
void MontaMontaEditor::setScale (float scale)
{
    scale = juce::jlimit (mu::minScale, mu::maxScale, scale);
    setSize (juce::roundToInt ((float) mu::baseW * scale), juce::roundToInt ((float) mu::baseH * scale));
}

void MontaMontaEditor::resized()
{
    const float scale = (float) getWidth() / (float) mu::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)
        proc.uiScale = scale;
}

void MontaMontaEditor::paint (juce::Graphics& g)
{
    g.fillAll (mu::bg0);
}

// =====================================================================
// DESENHO DO PAINEL
// =====================================================================
void MontaMontaEditor::drawCanvas (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (mu::bg1, 0.0f, 0.0f, mu::bg0, 0.0f, (float) mu::baseH, false));
    g.fillAll();
    if (g.clipRegionIntersects ({ 0, 0, mu::baseW, 80 })) drawHeader (g);
    if (g.clipRegionIntersects (rackArea.toNearestInt())) drawRack (g);
    if (g.clipRegionIntersects (tipArea.expanded (2.0f).toNearestInt())) drawTipBox (g);
    if (g.clipRegionIntersects (stripArea.toNearestInt())) drawPath (g);
}

void MontaMontaEditor::drawHeader (juce::Graphics& g)
{
    const int l = lang();
    const auto& p = proc.patch;
    g.setColour (mu::head);
    g.fillRect (0.0f, 0.0f, (float) mu::baseW, 78.0f);
    g.setColour (mu::line);
    g.fillRect (0.0f, 77.0f, (float) mu::baseW, 2.0f);

    // logo: 4 peças encaixadas
    const juce::Colour lc[] = { mu::moduleColour (vco1), mu::moduleColour (vcf), mu::moduleColour (envAmp), mu::moduleColour (output) };
    const float lx[] = { 18.0f, 42.0f, 18.0f, 42.0f }, ly[] = { 14.0f, 14.0f, 38.0f, 38.0f };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (lc[i]);
        g.fillRoundedRectangle (lx[i], ly[i], 22.0f, 22.0f, 4.0f);
    }
    g.setColour (lc[0]); g.fillEllipse (35.0f, 20.0f, 10.0f, 10.0f); g.fillEllipse (24.0f, 31.5f, 10.0f, 10.0f);
    g.setColour (lc[1]); g.fillEllipse (48.0f, 31.5f, 10.0f, 10.0f);
    g.setColour (lc[2]); g.fillEllipse (35.0f, 44.0f, 10.0f, 10.0f);

    const float w1 = juce::GlyphArrangement::getStringWidth (mu::font (25.0f), "MONTA");
    mu::text (g, "MONTA", { 78.0f, 20.0f, w1 + 4.0f, 30.0f }, 25.0f, mu::moduleColour (vco1));
    g.setColour (mu::ink);
    g.fillRoundedRectangle (82.0f + w1, 33.0f, 11.0f, 5.0f, 1.0f);
    mu::text (g, "MONTA", { 98.0f + w1, 20.0f, w1 + 4.0f, 30.0f }, 25.0f, mu::moduleColour (vcf));
    mu::fitText (g, l == 0 ? u8 ("INSIDE STUDIO SP · SYNTH DIDÁTICO") : u8 ("INSIDE STUDIO SP · LEARNING SYNTH"), { 80.0f, 56.0f, 220.0f, 12.0f },
                 8.5f, mu::dim);

    // lição
    const RF lb (372.0f, 12.0f, 356.0f, 54.0f);
    g.setColour (mu::card);
    g.fillRoundedRectangle (lb, 6.0f);
    g.setColour (mu::line2);
    g.drawRoundedRectangle (lb, 6.0f, 1.0f);
    juce::String t1, t2;
    if (p.cables)
    {
        t1 = l == 0 ? u8 ("NÍVEL 2 · CABOS") : u8 ("LEVEL 2 · CABLES");
        t2 = l == 0 ? u8 ("Level 2 · cables") : u8 ("Nível 2 · cabos");
    }
    else
    {
        const auto& lt = lessonText (p.lesson);
        t1 = (l == 0 ? u8 ("LIÇÃO ") : juce::String ("LESSON ")) + juce::String (p.lesson) + u8 (" · ") + lt.title.get (l).toUpperCase();
        t2 = (l == 0 ? juce::String ("Lesson ") : u8 ("Lição ")) + juce::String (p.lesson) + u8 (" · ") + lt.title.other (l);
    }
    mu::fitText (g, t1, { 386.0f, 20.0f, 186.0f, 18.0f }, 14.5f, juce::Colours::white);
    mu::fitText (g, t2, { 386.0f, 40.0f, 186.0f, 14.0f }, 10.5f, juce::Colour (0xff8d93a8), juce::Justification::centredLeft, false, true);

    // modo e idioma
    for (auto r : { RF (744.0f, 14.0f, 196.0f, 50.0f), RF (956.0f, 14.0f, 96.0f, 50.0f) })
    {
        g.setColour (mu::card);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (mu::line2);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
    mu::text (g, l == 0 ? "MODO / MODE" : "MODE / MODO", { 756.0f, 18.0f, 170.0f, 12.0f }, 8.0f, mu::dim);
    mu::text (g, l == 0 ? "IDIOMA" : "LANGUAGE", { 966.0f, 18.0f, 84.0f, 12.0f }, 8.0f, mu::dim);

    // medidor de saída (pico em dBFS)
    mu::text (g, l == 0 ? u8 ("SAÍDA / OUT") : u8 ("OUT / SAÍDA"), { 1068.0f, 18.0f, 118.0f, 12.0f }, 8.0f, mu::dim);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const int lit = juce::roundToInt (juce::jmap (db, -48.0f, 0.0f, 0.0f, 12.0f));
    for (int i = 0; i < 12; ++i)
    {
        const auto c = i < 8 ? mu::ok : (i < 10 ? mu::gold : mu::warn);
        g.setColour (i < lit ? c : juce::Colour (0xff1d2029));
        g.fillRoundedRectangle (1068.0f + (float) i * 9.5f, 36.0f, 7.0f, 18.0f, 1.5f);
    }
    if (level <= 0.0f)
        mu::text (g, l == 0 ? "sem som ainda" : "no sound yet", { 1062.0f, 58.0f, 128.0f, 12.0f }, 8.5f, mu::warn, juce::Justification::centred, false);
}

void MontaMontaEditor::drawRack (juce::Graphics& g)
{
    const int l = lang();
    const auto& p = proc.patch;
    g.setColour (juce::Colour (0xff0a0b0f));
    g.fillRoundedRectangle (rackArea, 8.0f);
    g.setColour (mu::line);
    g.drawRoundedRectangle (rackArea.reduced (0.5f), 8.0f, 1.0f);

    static const juce::Image holes = holeTile();
    g.setFillType (juce::FillType (holes, juce::AffineTransform::scale (0.25f).translated (rackArea.getX() + 10.0f, rackArea.getY() + 30.0f)));
    g.fillRect (rackArea.getX() + 10.0f, rackArea.getY() + 30.0f, rackArea.getWidth() - 20.0f, rackArea.getHeight() - 40.0f);

    mu::text (g, "RACK", { rackArea.getX() + 14.0f, rackArea.getY() + 2.0f, 50.0f, 16.0f }, 11.0f, mu::ink);
    juce::String hint;
    if (p.cables)
        hint = l == 0 ? u8 ("ligue as saídas (OUT) nas entradas (IN) · patch outputs into inputs") : u8 ("patch outputs (OUT) into inputs (IN) · ligue as saídas nas entradas");
    else
        hint = l == 0 ? u8 ("monte o caminho do som da esquerda para a direita · build the signal path left to right")
                      : u8 ("build the signal path left to right · monte o caminho do som da esquerda para a direita");
    mu::fitText (g, hint, { rackArea.getX() + 58.0f, rackArea.getY() + 2.0f, rackArea.getWidth() - 70.0f, 16.0f }, 9.0f, mu::dim,
                 juce::Justification::centredLeft, false, true);

    // trilhos
    for (float y : { 113.0f, 581.0f })
    {
        const RF rail (rackArea.getX() + 8.0f, y, rackArea.getWidth() - 16.0f, 7.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9ccd6), 0.0f, rail.getY(), juce::Colour (0xff6d717d), 0.0f, rail.getBottom(), false));
        g.fillRect (rail);
        g.setColour (juce::Colour (0xff2a2c34));
        for (int i = 0; i < 22; ++i)
            g.fillEllipse (rail.getX() + 14.0f + (float) i * 29.0f - 3.5f, rail.getCentreY() - 2.0f, 7.0f, 4.0f);
    }

    // encaixes vazios, pontes e trancados (peça montada se desenha sozinha)
    for (int m = 0; m < numModules; ++m)
    {
        if (panels[(size_t) m]->isVisible()) continue;
        const auto r = slotRect (m);
        const auto shape = mu::piecePath (r, mu::moduleShape (m));
        const auto col = mu::moduleColour (m);
        if (p.isBypass (m))
        {
            g.setColour (juce::Colour (0xff1a1c22));
            g.fillPath (shape);
            g.setColour (juce::Colour (0xff3a3e4c));
            g.strokePath (shape, juce::PathStrokeType (1.5f));
            const float cy = r.getCentreY();
            g.setColour (juce::Colour (0xff8d93a8));
            g.drawLine (r.getX() + 8.0f, cy, r.getRight() - 12.0f, cy, 2.0f);
            juce::Path ar;
            ar.addTriangle (r.getRight() - 14.0f, cy - 5.0f, r.getRight() - 14.0f, cy + 5.0f, r.getRight() - 6.0f, cy);
            g.fillPath (ar);
            mu::text (g, l == 0 ? "PONTE" : "BRIDGE", { r.getX(), cy - 34.0f, r.getWidth(), 16.0f }, 12.0f, juce::Colour (0xffa9aec0), juce::Justification::centred);
            mu::fitText (g, l == 0 ? "o som passa direto" : "sound passes through", { r.getX() + 6.0f, cy + 10.0f, r.getWidth() - 12.0f, 12.0f }, 8.5f,
                         mu::dim, juce::Justification::centred, false, true);
            mu::fitText (g, (l == 0 ? juce::String ("vira ") : juce::String ("becomes ")) + pieceName (m, l)
                             + (l == 0 ? u8 (" na lição ") : juce::String (" in lesson ")) + juce::String (lessonOf (m)),
                         { r.getX() + 6.0f, cy + 24.0f, r.getWidth() - 12.0f, 12.0f }, 8.0f, juce::Colour (0xff5d6276), juce::Justification::centred, false);
            mu::screw (g, r.getX() + 12.0f, r.getBottom() - 11.0f);
            mu::screw (g, r.getRight() - 12.0f, r.getBottom() - 11.0f);
        }
        else if (p.isHidden (m))
        {
            dashed (g, shape, juce::Colour (0xff3a3e4c), 1.5f, 5.0f, 5.0f);
            const float cy = r.getCentreY();
            mu::lockIcon (g, { r.getCentreX() - 6.0f, cy - 36.0f, 12.0f, 16.0f }, juce::Colour (0xff555a6c));
            mu::text (g, pieceName (m, l), { r.getX(), cy - 12.0f, r.getWidth(), 16.0f }, 11.0f, juce::Colour (0xff555a6c), juce::Justification::centred);
            mu::text (g, (l == 0 ? u8 ("lição ") : juce::String ("lesson ")) + juce::String (lessonOf (m)), { r.getX(), cy + 6.0f, r.getWidth(), 12.0f },
                      9.0f, juce::Colour (0xff4a4e60), juce::Justification::centred, false, true);
        }
        else
        {
            const bool hot = drag.module == m;
            if (hot)
            {
                g.setColour (col.withAlpha (0.08f));
                g.fillPath (shape);
                for (int i = 3; i >= 1; --i)
                    dashed (g, shape, col.withAlpha (0.1f * (float) (4 - i)), 2.0f + (float) i * 3.0f);
            }
            dashed (g, shape, col.withAlpha (hot ? 1.0f : 0.55f), 2.0f);
            const float cy = r.getY() + 64.0f;
            if (hot)
            {
                mu::text (g, l == 0 ? "SOLTE AQUI" : "DROP HERE", { r.getX(), cy - 8.0f, r.getWidth(), 16.0f }, 11.0f, col, juce::Justification::centred);
                mu::text (g, l == 0 ? "drop here" : "solte aqui", { r.getX(), cy + 8.0f, r.getWidth(), 12.0f }, 9.0f, col.withAlpha (0.8f),
                          juce::Justification::centred, false, true);
            }
            else
            {
                mu::text (g, pieceName (m, l), { r.getX(), cy - 8.0f, r.getWidth(), 16.0f }, 12.0f, col.withAlpha (0.55f), juce::Justification::centred);
                mu::text (g, l == 0 ? "encaixe aqui" : "fits here", { r.getX(), cy + 8.0f, r.getWidth(), 12.0f }, 9.0f, col.withAlpha (0.45f),
                          juce::Justification::centred, false, true);
            }
        }
    }
}

void MontaMontaEditor::drawTipBox (juce::Graphics& g)
{
    const int l = lang();
    const auto& p = proc.patch;
    const RF b = tipArea;
    const bool msg = msgUntil > 0.0;
    const bool done = ! msg && lessonDone && ! p.cables && p.lesson < freeLesson;
    const juce::Colour edge = msg ? msgColour : done ? mu::ok : mu::line2;
    g.setColour (mu::card);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (edge.withAlpha (msg || done ? 0.8f : 1.0f));
    g.drawRoundedRectangle (b, 8.0f, 1.0f);

    juce::String title, a, c;
    juce::Colour tc = mu::gold;
    if (msg)
    {
        title = l == 0 ? u8 ("ATENÇÃO") : juce::String ("HEADS UP");
        a = l == 0 ? msgPt : msgEn;
        c = l == 0 ? msgEn : msgPt;
        tc = msgColour == mu::dim ? mu::ink2 : msgColour;
    }
    else if (p.cables)
    {
        title = l == 0 ? "COMO LIGAR" : "HOW TO PATCH";
        a = txtCablesHelp().get (l);
        c = txtCablesHelp().other (l);
    }
    else if (done)
    {
        title = l == 0 ? u8 ("LIÇÃO CONCLUÍDA") : juce::String ("LESSON COMPLETE");
        a = lessonText (p.lesson).done.get (l);
        c = lessonText (p.lesson).done.other (l);
        tc = mu::ok;
    }
    else
    {
        title = l == 0 ? "DICA" : "TIP";
        a = lessonText (p.lesson).goal.get (l);
        c = lessonText (p.lesson).goal.other (l);
    }

    // ícone: lâmpada (dica), visto (concluída) ou exclamação (aviso)
    const juce::Point<float> ic (b.getX() + 22.0f, b.getY() + 22.0f);
    if (done)
        mu::checkIcon (g, { ic.x - 7.0f, ic.y - 5.0f, 14.0f, 11.0f }, tc, 2.4f);
    else if (msg)
    {
        g.setColour (tc);
        g.drawEllipse (ic.x - 8.0f, ic.y - 8.0f, 16.0f, 16.0f, 1.6f);
        mu::text (g, "!", { ic.x - 8.0f, ic.y - 8.0f, 16.0f, 16.0f }, 11.0f, tc, juce::Justification::centred);
    }
    else
    {
        g.setColour (tc);
        g.fillEllipse (ic.x - 6.0f, ic.y - 9.0f, 12.0f, 12.0f);
        g.fillRect (ic.x - 3.0f, ic.y + 2.0f, 6.0f, 5.0f);
        g.setColour (juce::Colour (0xff8a7a2a));
        g.fillRect (ic.x - 3.0f, ic.y + 5.0f, 6.0f, 1.0f);
    }
    mu::text (g, title, { b.getX() + 38.0f, b.getY() + 14.0f, b.getWidth() - 50.0f, 16.0f }, 11.0f, tc);

    const RF tr (b.getX() + 14.0f, b.getY() + 40.0f, b.getWidth() - 28.0f, done ? 142.0f : 176.0f);
    float size = 10.5f;
    for (; size > 7.5f; size -= 0.25f)
        if (mu::para (nullptr, a, tr, size, mu::ink2) + 6.0f + mu::para (nullptr, c, tr, size * 0.85f, mu::dim, false, true) <= tr.getHeight())
            break;
    const float h = mu::para (&g, a, tr, size, mu::ink2);
    mu::para (&g, c, tr.withTrimmedTop (h + 6.0f), size * 0.85f, mu::dim, false, true);
}

// a corrente do caminho do som: peças na ordem; 0x100 = peça que falta, 0x200 = nada ligado na SAÍDA
std::vector<int> MontaMontaEditor::pathChain() const
{
    const auto& p = proc.patch;
    std::vector<int> chain;
    if (! p.cables)
    {
        for (int m : topRow)
            if (! p.isBypass (m)) chain.push_back (p.placed[(size_t) m] ? m : (m | 0x100));
        return chain;
    }
    const auto g = p.build (true);
    int m = g.src[inOut];
    chain.push_back (output);
    std::array<bool, numModules> seen {};
    while (m >= 0 && ! seen[(size_t) m] && chain.size() < 8)
    {
        seen[(size_t) m] = true;
        chain.insert (chain.begin(), m);
        switch (m)
        {
            case vcf:    m = g.src[inVcf]; break;
            case envAmp: m = g.src[inAmp]; break;
            case mixer:
            {
                const int a = g.src[inMix1], b = g.src[inMix2];
                m = (a >= 0 && reach.sound[(size_t) a]) ? a : (b >= 0 ? b : a);
                break;
            }
            default: m = -1; break;
        }
    }
    if (chain.size() == 1) chain.insert (chain.begin(), vco1 | 0x200);
    return chain;
}

void MontaMontaEditor::drawPath (juce::Graphics& g)
{
    const int l = lang();
    const auto& p = proc.patch;
    g.setColour (mu::panel);
    g.fillRoundedRectangle (stripArea, 8.0f);
    g.setColour (mu::line);
    g.drawRoundedRectangle (stripArea.reduced (0.5f), 8.0f, 1.0f);
    const auto title = l == 0 ? juce::String ("CAMINHO DO SOM") : juce::String ("SIGNAL PATH");
    const float tw = juce::GlyphArrangement::getStringWidth (mu::font (11.0f), title);
    mu::text (g, title, { 30.0f, 624.0f, tw + 4.0f, 16.0f }, 11.0f, mu::ink);
    mu::text (g, l == 0 ? "signal path" : "caminho do som", { 38.0f + tw, 625.0f, 120.0f, 14.0f }, 9.0f, mu::dim, juce::Justification::centredLeft, false, true);

    const auto chain = pathChain();
    const int n = (int) chain.size();
    const float step = n > 1 ? juce::jmin (104.0f, 470.0f / (float) (n - 1)) : 0.0f;
    const float y = 670.0f;
    for (int i = 0; i < n; ++i)
    {
        const int raw = chain[(size_t) i];
        const int m = raw & 0xff;
        const bool missing = (raw & 0x100) != 0, loose = (raw & 0x200) != 0;
        const float x = 54.0f + (float) i * step;
        const bool sound = ! missing && ! loose && reach.sound[(size_t) m];
        if (i > 0)
        {
            const int prev = chain[(size_t) i - 1];
            const bool prevSound = (prev & 0x300) == 0 && reach.sound[(size_t) (prev & 0xff)];
            g.setColour (prevSound ? mu::moduleColour (prev & 0xff) : (missing || loose ? mu::warn : juce::Colour (0xff333745)));
            g.drawLine (x - step + 20.0f, y, x - 20.0f, y, prevSound ? 3.0f : 2.0f);
        }
        const auto col = mu::moduleColour (m);
        if (missing || loose)
        {
            juce::Path c;
            c.addEllipse (x - 17.0f, y - 17.0f, 34.0f, 34.0f);
            dashed (g, c, mu::warn, 2.0f, 4.0f, 3.0f);
            mu::text (g, "?", { x - 17.0f, y - 17.0f, 34.0f, 34.0f }, 15.0f, mu::warn, juce::Justification::centred);
            mu::fitText (g, loose ? (l == 0 ? "FIO?" : "WIRE?") : pieceName (m, l), { x - 40.0f, y + 21.0f, 80.0f, 12.0f }, 8.5f, mu::warn,
                         juce::Justification::centred);
        }
        else
        {
            if (sound)
            {
                g.setColour (col.withAlpha (0.25f));
                g.fillEllipse (x - 22.0f, y - 22.0f, 44.0f, 44.0f);
            }
            g.setColour (sound ? col : juce::Colour (0xff20232c));
            g.fillEllipse (x - 17.0f, y - 17.0f, 34.0f, 34.0f);
            g.setColour (col);
            g.drawEllipse (x - 17.0f, y - 17.0f, 34.0f, 34.0f, 2.0f);
            mu::fitText (g, pieceName (m, l), { x - 40.0f, y + 21.0f, 80.0f, 12.0f }, 8.5f, sound ? mu::ink : juce::Colour (0xff6a6f84),
                         juce::Justification::centred);
            if (sound) mu::checkIcon (g, { x - 6.0f, y - 4.0f, 12.0f, 9.0f }, juce::Colour (0xff0b0c11), 2.2f);
        }
    }

    g.setColour (mu::line);
    g.fillRect (570.0f, 630.0f, 2.0f, 84.0f);

    // a frase da faixa: o que falta, ou que está tudo certo
    juce::String pt, en;
    juce::Colour c = mu::gold;
    if (reach.atOutput)
    {
        c = mu::ok;
        if (p.cables)
        {
            pt = u8 ("O som está saindo pelos fios! Toque e experimente outras ligações.");
            en = "Sound is flowing through the wires! Play and try other patches.";
        }
        else if (lessonDone && p.lesson < freeLesson)
        {
            pt = u8 ("Lição concluída! Leia a dica no rack e siga para a próxima.");
            en = "Lesson complete! Read the tip in the rack and move on.";
        }
        else
        {
            pt = u8 ("O som está saindo! Toque uma nota no teclado ou no seu DAW.");
            en = "Sound is coming out! Play a note on the keyboard or in your DAW.";
        }
    }
    else if (p.cables)
    {
        if (p.wireAt (inOut) < 0)
        {
            pt = u8 ("Ligue um fio na entrada IN da SAÍDA. Comece pela tomada OUT do VCO.");
            en = "Plug a wire into the OUTPUT IN socket. Start from the VCO OUT socket.";
        }
        else
        {
            pt = u8 ("O som ainda não chega na SAÍDA: siga os fios e veja onde ele para.");
            en = "Sound does not reach the OUTPUT yet: follow the wires and see where it stops.";
        }
    }
    else
    {
        int miss = -1, last = -1;
        for (int i = 0; i < n; ++i)
        {
            const int raw = chain[(size_t) i];
            if ((raw & 0x300) != 0) { if (miss < 0) miss = raw & 0xff; continue; }
            if (miss < 0 && reach.sound[(size_t) raw]) last = raw;
        }
        auto art = [] (int m) { return m == output ? juce::String ("a ") : juce::String ("o "); };
        if (miss >= 0 && last >= 0)
        {
            pt = u8 ("Quase lá! O som chega até ") + art (last) + pieceName (last, 0) + u8 (", mas falta ") + art (miss) + pieceName (miss, 0) + ".";
            en = "Almost there! Sound reaches the " + pieceName (last, 1) + ", but the " + pieceName (miss, 1) + " is missing.";
        }
        else if (miss >= 0)
        {
            pt = "Falta " + art (miss) + pieceName (miss, 0) + u8 (". Arraste da bandeja para o rack.");
            en = "The " + pieceName (miss, 1) + " is missing. Drag it from the tray into the rack.";
        }
        else
        {
            pt = u8 ("Tudo encaixado, mas não sai som: confira as chaves e os volumes do MIXER.");
            en = "Everything fits, but no sound: check the MIXER switches and levels.";
        }
    }
    const RF tr (586.0f, 630.0f, 210.0f, 86.0f);
    const auto a = l == 0 ? pt : en, b = l == 0 ? en : pt;
    float size = 11.0f;
    for (; size > 8.0f; size -= 0.25f)
        if (mu::para (nullptr, a, tr, size, c, true) + 4.0f + mu::para (nullptr, b, tr, size * 0.82f, c.withAlpha (0.6f), false, true) <= tr.getHeight())
            break;
    const float h = mu::para (&g, a, tr, size, c, true);
    mu::para (&g, b, tr.withTrimmedTop (h + 4.0f), size * 0.82f, c.withAlpha (0.6f), false, true);
}
