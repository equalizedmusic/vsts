#include "PluginEditor.h"

using namespace caleido;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuLicenca = 20005;

// pedal: chapa esmaltada de 1000 × 580 (o layout) na moldura de madeira (laterais de 24, travessas de 20)
constexpr int lado = 24, topo = 20, pedalW = ui::baseW + 2 * lado, pedalH = ui::baseH + 2 * topo;
const juce::Colour esmalte (0xff6a2a6e);      // cor da chapa do CALEIDO
const juce::Colour serigrafia (0xffede3cc);   // tinta creme impressa na chapa

// função, e não array global: um array copiado das cores do tema na carga do plugin podia ficar zerado
// (ordem de inicialização entre arquivos) e a mandala saía sem cor
juce::Colour laneColour (int lane)
{
    switch (lane)
    {
        case laneRepeat:  return theme::amber;
        case laneReverse: return theme::coral;
        case laneTape:    return juce::Colour (0xff7fe0a8);   // verde: creme se confundia com os knobs comuns
        case laneFilter:  return theme::ice;
        default:          return theme::violet;
    }
}
const char* const laneName[numLanes] = { "REPETE", "REVERSO", "FITA", "FILTER", "CRUSH" };

void drawTitle (juce::Graphics& g, float x, float y, const juce::String& prefix, const juce::String& accent)
{
    juce::AttributedString s;
    const auto f = theme::sans (9.5f, true, 0.25f);
    s.append (prefix, f, theme::dim);
    s.append (accent, f, theme::amber);
    s.setJustification (juce::Justification::centredLeft);
    s.draw (g, { x, y, 400.0f, 13.0f });
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

bool laneOn (CaleidoProcessor& p, int lane, int step)
{
    return ((p.engine.lanes[(size_t) lane].load (std::memory_order_relaxed) >> step) & 1) != 0;
}
} // namespace

// =====================================================================
// PADRÃO
// =====================================================================
void PatternStrip::paint (juce::Graphics& g)
{
    const auto  r    = getLocalBounds().toFloat();
    const float W    = r.getWidth(), H = r.getHeight();
    const float top  = 30.0f, bot = H - 20.0f, mid = (top + bot) * 0.5f;
    const float sw   = W / (float) numSteps, cw = sw / (float) colsPerStep;
    const int   step = proc.engine.step.load (std::memory_order_relaxed);
    const float frac = proc.engine.stepFrac.load (std::memory_order_relaxed);

    painel::chapaPreta (g, r.reduced (1.0f), false);

    for (int s = 0; s < numSteps; ++s)
    {
        const float x0 = (float) s * sw;
        juce::Array<juce::Colour> cols;
        for (int l = 0; l < numLanes; ++l)
            if (laneOn (proc, l, s)) cols.add (laneColour (l));

        // fundo do passo com a cor dos efeitos ativos
        for (auto& c : cols)
        {
            g.setColour (c.withAlpha (0.13f));
            g.fillRect (x0 + 1.0f, top - 6.0f, sw - 2.0f, bot - top + 12.0f);
        }
        if (s == step)
        {
            g.setColour (theme::cream.withAlpha (0.05f));
            g.fillRect (x0, 22.0f, sw, H - 22.0f);
        }

        // forma de onda do que saiu do efeito neste passo
        g.setColour (cols.isEmpty() ? theme::off : cols.getFirst().withAlpha (0.85f));
        for (int j = 0; j < colsPerStep; ++j)
        {
            const float v = proc.engine.peaks[(size_t) (s * colsPerStep + j)].load (std::memory_order_relaxed);
            const float h = juce::jmax (0.5f, std::sqrt (juce::jmin (1.0f, v)) * (bot - top) * 0.5f);
            g.fillRect (x0 + (float) j * cw, mid - h, juce::jmax (1.0f, cw - 0.6f), h * 2.0f);
        }

        // marcas dos efeitos embaixo
        for (int i = 0; i < cols.size(); ++i)
        {
            g.setColour (cols[i]);
            g.fillRect (x0 + 4.0f + (float) i * 7.0f, bot + 6.0f, 5.0f, 3.0f);
        }

        // divisória e número do tempo
        g.setColour (s % 4 == 0 ? theme::segOff : juce::Colour (0xff25211b));
        g.fillRect (x0, 22.0f, 1.0f, H - 22.0f);
        if (s % 4 == 0)
        {
            g.setColour (theme::dim);
            g.setFont (theme::mono (9.5f));
            g.drawText (juce::String (s / 4 + 1), juce::Rectangle<float> (x0 + 4.0f, 24.0f, 20.0f, 11.0f), juce::Justification::centredLeft);
        }
    }

    // cabeçote
    g.setColour (theme::coral);
    g.fillRect (((float) step + frac) * sw - 1.0f, 22.0f, 2.0f, H - 22.0f);

    // título (o compasso inteiro depende da DIVISÃO)
    const int div = juce::roundToInt (proc.apvts.getRawParameterValue (id::division)->load());
    const juce::String span = div == 0 ? "2 COMPASSOS" : div == 1 ? "1 COMPASSO" : "MEIO COMPASSO";
    const juce::String note = div == 0 ? "1/8" : div == 1 ? "1/16" : "1/32";
    drawTitle (g, 12.0f, 7.0f, u8 ("PADRÃO "), u8 ("· ") + span + u8 (" · ") + note);

    // legenda, da direita para a esquerda
    {
        const auto f = theme::sans (8.0f, true, 0.18f);
        g.setFont (f);
        float x = W - 10.0f;
        for (int l = numLanes - 1; l >= 0; --l)
        {
            const float tw = textWidth (f, laneName[l]);
            x -= tw;
            g.setColour (theme::dim);
            g.drawText (laneName[l], juce::Rectangle<float> (x, 7.0f, tw + 2.0f, 12.0f), juce::Justification::centredLeft);
            x -= 12.0f;
            g.setColour (laneColour (l));
            g.fillRoundedRectangle (x, 9.0f, 8.0f, 8.0f, 2.0f);
            x -= 10.0f;
        }
    }

    // selo SEGURANDO
    if (proc.engine.holding.load (std::memory_order_relaxed))
    {
        auto badge = juce::Rectangle<float> (98.0f, 17.0f).withCentre ({ W * 0.5f, 15.5f });
        g.setColour (theme::violet.withAlpha (0.13f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (theme::violet.withAlpha (0.33f));
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        const auto c = juce::Colour (0xffd7bcff);
        g.setColour (c);
        g.drawEllipse (badge.getX() + 8.0f, badge.getCentreY() - 3.5f, 7.0f, 7.0f, 1.0f);
        g.fillEllipse (badge.getX() + 10.0f, badge.getCentreY() - 1.5f, 3.0f, 3.0f);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText ("SEGURANDO", badge.withTrimmedLeft (16.0f), juce::Justification::centred);
    }
}

// =====================================================================
// MANDALA
// =====================================================================
namespace
{
constexpr float ringR0 = 34.0f, ringW = 13.0f, ringGap = 3.0f;
constexpr float segA   = juce::MathConstants<float>::twoPi / (float) numSteps;
}

void Mandala::paint (juce::Graphics& g)
{
    const float MS = (float) getWidth(), MC = MS * 0.5f;
    const int   step = proc.engine.step.load (std::memory_order_relaxed);
    const float frac = proc.engine.stepFrac.load (std::memory_order_relaxed);

    // tela redonda: canto = metade do lado vira círculo
    const juce::Rectangle<float> scr (1.5f, 1.5f, MS - 3.0f, MS - 3.0f);
    const float scrCorner = scr.getWidth() * 0.5f;
    real::screen (g, scr, scrCorner);

    // anéis: REPETE por fora, CRUSH por dentro
    for (int l = 0; l < numLanes; ++l)
    {
        const float ri = ringR0 + (float) (numLanes - 1 - l) * (ringW + ringGap), ro = ri + ringW;
        for (int s = 0; s < numSteps; ++s)
        {
            const float a0 = (float) s * segA + 0.035f, a1 = a0 + segA - 0.07f;
            juce::Path seg;
            seg.addCentredArc (MC, MC, ro, ro, 0.0f, a0, a1, true);
            seg.addCentredArc (MC, MC, ri, ri, 0.0f, a1, a0, false);
            seg.closeSubPath();

            const bool on = laneOn (proc, l, s), cur = s == step;
            if (on)
            {
                if (cur)
                {
                    g.setColour (laneColour (l).withAlpha (0.35f));
                    g.strokePath (seg, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                g.setColour (laneColour (l).withAlpha (cur ? 1.0f : 0.55f));
            }
            else
                g.setColour (cur ? theme::segOff : juce::Colour (0xff25211b));
            g.fillPath (seg);
        }
    }

    // ponteiro
    {
        const float pa = ((float) step + frac) * segA;
        const float r1 = ringR0 - 4.0f, r2 = ringR0 + (float) numLanes * (ringW + ringGap) + 2.0f;
        g.setColour (theme::cream.withAlpha (0.67f));
        g.drawLine (MC + r1 * std::sin (pa), MC - r1 * std::cos (pa), MC + r2 * std::sin (pa), MC - r2 * std::cos (pa), 1.2f);
    }

    // miolo caleidoscópico: o que está tocando agora, espelhado em 8
    {
        juce::Graphics::ScopedSaveState save (g);
        const float rc = ringR0 - 6.0f;
        juce::Path clip;
        clip.addEllipse (MC - rc, MC - rc, rc * 2.0f, rc * 2.0f);
        g.reduceClipRegion (clip);
        g.setColour (theme::bg);
        g.fillPath (clip);

        const float trip = proc.apvts.getRawParameterValue (id::trip)->load() * 0.01f;
        int i = 0;
        for (int l = 0; l < numLanes; ++l)
        {
            if (! laneOn (proc, l, step)) continue;
            const float d = 9.0f + (float) i * 6.0f + std::sin (t / 12.0f + (float) i) * 4.0f, w = 4.0f + trip * 6.0f;
            juce::Path tri;
            tri.addTriangle (d, 0.0f, d + w, w * 0.5f, d + w * 0.4f, w * 1.2f);
            g.setColour (laneColour (l).withAlpha (0.75f));
            for (int k = 0; k < 8; ++k)
            {
                auto tr = juce::AffineTransform::scale (1.0f, k % 2 ? -1.0f : 1.0f)
                              .rotated ((float) k * juce::MathConstants<float>::pi * 0.25f + t / 90.0f)
                              .translated (MC, MC);
                g.fillPath (tri, tr);
            }
            ++i;
        }
    }
    g.setColour (theme::cream);
    g.fillEllipse (MC - 2.5f, MC - 2.5f, 5.0f, 5.0f);
    real::glass (g, scr, scrCorner);

    // passo e semente
    auto line = [] (const juce::String& a, const juce::String& b, const juce::String& c)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        s.append (c, theme::mono (10.5f), theme::dim);
        return s;
    };
    auto sl = line ("STEP ", juce::String (step + 1).paddedLeft ('0', 2), "/16");
    auto sr = line ("SEMENTE ", juce::String ((int) (proc.engine.seed.load() % 10000u)).paddedLeft ('0', 4), {});
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, MS + 8.0f, MS, 14.0f });
    sr.draw (g, { 0.0f, MS + 8.0f, MS, 14.0f });
}

void Mandala::mouseDown (const juce::MouseEvent& e)
{
    const float MC = (float) getWidth() * 0.5f;
    const float dx = e.position.x - MC, dy = e.position.y - MC;
    const int   k  = (int) std::floor ((std::hypot (dx, dy) - ringR0) / (ringW + ringGap));
    if (k < 0 || k >= numLanes) return;

    float a = std::atan2 (dx, -dy);
    if (a < 0.0f) a += juce::MathConstants<float>::twoPi;
    const int s = juce::jlimit (0, numSteps - 1, (int) (a / segA));
    proc.engine.toggle (numLanes - 1 - k, s);
    repaint();
}

// =====================================================================
// GERAR
// =====================================================================
void ChipButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r = real::keycap (g, getLocalBounds().toFloat().reduced (1.5f), 3.0f, down);

    const auto c = over || down ? theme::amber : theme::dim;
    g.setColour (c);
    const juce::Point<float> o (r.getX() + 11.0f, r.getCentreY());
    juce::Path arc;
    arc.addCentredArc (o.x, o.y, 3.6f, 3.6f, 0.0f, 0.6f, 5.6f, true);
    g.strokePath (arc, juce::PathStrokeType (1.1f));
    juce::Path head;
    head.addTriangle (o.x + 1.0f, o.y - 6.0f, o.x + 4.2f, o.y - 3.4f, o.x + 0.4f, o.y - 1.8f);
    g.fillPath (head);

    g.setFont (theme::sans (8.0f, true, 0.18f));
    g.drawText ("GERAR", r.withTrimmedLeft (18.0f), juce::Justification::centred);
}

// =====================================================================
// EDITOR
// =====================================================================
CaleidoEditor::CaleidoEditor (CaleidoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), mandala (p),
      chaosMacro (p.apvts, id::chaos,    "CAOS",             true,  theme::amber),
      tripMacro  (p.apvts, id::trip,     "VIAGEM",           true,  theme::coral),
      division   (p.apvts, id::division, u8 ("DIVISÃO"),     false, theme::cream),
      // os knobs de cada anel usam a cor dele na mandala
      ratchet    (p.apvts, id::ratchet,  u8 ("REPETIÇÃO"),   false, laneColour (laneRepeat)),
      shuffle    (p.apvts, id::shuffle,  "EMBARALHAR",       false, theme::cream),
      reverse    (p.apvts, id::reverse,  "REVERSO",          false, laneColour (laneReverse)),
      tape       (p.apvts, id::tape,     "FITA",             false, laneColour (laneTape)),
      filter     (p.apvts, id::filter,   "FILTER",           false, laneColour (laneFilter)),
      crush      (p.apvts, id::crush,    "CRUSH",            false, laneColour (laneCrush)),
      spin       (p.apvts, id::spin,     "GIRO",             false, theme::cream),
      output     (p.apvts, id::output,   "GAIN",             false, theme::cream),
      mix        (p.apvts, id::mix),
      mutateKey  ("TRANSMUTAR", theme::coral),
      holdKey    ("SEGURAR", theme::violet)
{
    setLookAndFeel (&lnf);
    for (auto* k : { &chaosMacro, &tripMacro, &division, &ratchet, &shuffle, &reverse, &tape, &filter, &crush, &spin, &output })
        k->setPedal (true);   // knobs do pedal: os quadros renderizados da SÉRIE KNOB

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, pedalW, pedalH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &mandala, &generateBtn, &chaosMacro, &tripMacro, &division, &ratchet, &shuffle, &reverse, &tape, &filter,
             &crush, &spin, &output, &mix, &mutateKey, &holdKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    mutateAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::mutate, mutateKey);
    holdAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::hold, holdKey);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

    generateBtn.onClick = [this] { proc.generate(); };
    prevBtn.onClick     = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    nextBtn.onClick     = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    saveBtn.onClick    = [this] { askNewPreset(); };
    presetName.onClick  = [this] { showPresetMenu(); };

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
    ativacao.anexar (*this);
}

CaleidoEditor::~CaleidoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CaleidoEditor::showPresetMenu()
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

    juce::Component::SafePointer<CaleidoEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void CaleidoEditor::askNewPreset()
{
    juce::Component::SafePointer<CaleidoEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void CaleidoEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<CaleidoEditor> safe (this);
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

void CaleidoEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) pedalW - openScale) > 0.005f)
        setScale (openScale);

    strip.repaint();
    mandala.tick();

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

void CaleidoEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) pedalW * scale), juce::roundToInt ((float) pedalH * scale));
}

void CaleidoEditor::resized()
{
    const float scale = (float) getWidth() / (float) pedalW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void CaleidoEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void CaleidoEditor::drawCanvas (juce::Graphics& g)
{
    painel::pedal (g, (float) pedalW, (float) pedalH, (float) lado, (float) topo, esmalte);
    g.addTransform (juce::AffineTransform::translation ((float) lado, (float) topo));   // daqui em diante, o layout da chapa

    // ---- logo: três lâminas de caleidoscópio ----
    g.setColour (theme::amber);
    g.drawEllipse (18.0f, 21.0f, 26.0f, 26.0f, 2.0f);
    {
        juce::Path kite;
        kite.startNewSubPath (0.0f, -9.0f);
        kite.lineTo (3.0f, -3.0f);
        kite.lineTo (0.0f, 0.0f);
        kite.lineTo (-3.0f, -3.0f);
        kite.closeSubPath();
        const juce::Colour cs[] = { theme::amber, theme::coral, theme::violet };
        for (int k = 0; k < 3; ++k)
        {
            g.setColour (cs[k]);
            g.fillPath (kite, juce::AffineTransform::rotation ((float) k * juce::MathConstants<float>::twoPi / 3.0f).translated (31.0f, 34.0f));
        }
        g.setColour (theme::cream);
        g.fillEllipse (31.0f - 1.6f, 34.0f - 1.6f, 3.2f, 3.2f);
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("CALEIDO");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 200.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (serigrafia.withAlpha (0.35f));
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    g.setColour (serigrafia.withAlpha (0.7f));
    g.setFont (theme::sans (9.5f, false, 0.2f));
    g.drawText ("GLITCH",     juce::Rectangle<float> (tagX, 22.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText ("PATTERN FX", juce::Rectangle<float> (tagX, 35.0f, 140.0f, 12.0f), juce::Justification::centredLeft);

    // ---- marca ----
    {
        auto width = [] (const juce::Font& f, const juce::String& s)
        {
            juce::GlyphArrangement a;
            a.addLineOfText (f, s, 0.0f, 0.0f);
            return a.getBoundingBox (0, -1, true).getWidth();
        };
        const auto tagFont = theme::sans (9.5f, false, 0.2f);
        const float brandX = tagX + juce::jmax (width (tagFont, "GLITCH"), width (tagFont, "PATTERN FX")) + 20.0f;
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

    drawTitle (g, 22.0f,  226.0f, "MANDALA ", u8 ("· 16"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "CORTE");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "VIAGEM");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 90.0f }, "MACRO CAOS",
              u8 ("Chance de cada passo ser cortado, repetido, invertido ou trocado de lugar."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 90.0f }, "MACRO VIAGEM",
              u8 ("Quanto cada corte é colorido: PITCH de fita, FILTER, CRUSH e giro no STEREO."));
}

void CaleidoEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    mandala.setBounds (22, 246, 240, 264);
    generateBtn.setBounds (200, 222, 62, 19);

    chaosMacro.setBounds (294, 246, 92, 92);
    division.setBounds (294, 376, 119, 92);
    ratchet.setBounds  (413, 376, 119, 92);
    shuffle.setBounds  (294, 468, 119, 92);
    reverse.setBounds  (413, 468, 119, 92);

    tripMacro.setBounds (562, 246, 92, 92);
    tape.setBounds   (562, 376, 119, 92);
    filter.setBounds (681, 376, 119, 92);
    crush.setBounds  (562, 468, 119, 92);
    spin.setBounds   (681, 468, 119, 92);

    mutateKey.setBounds (830, 246, 80, 40);   // TRANSMUTAR é o nome mais comprido
    holdKey.setBounds   (916, 246, 62, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);

    for (auto* c : canvas.getChildren())   // a chapa começa dentro da moldura
        c->setTopLeftPosition (c->getX() + lado, c->getY() + topo);
}
