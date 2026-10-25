#include "PluginEditor.h"

using namespace limao;
using lim::RF;

namespace
{
// placa de alumínio anodizado preto (faixa do nome e painel do sequenciador), aparafusada no corpo prateado
void blackPlate (juce::Graphics& g, RF r, float corner)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (real::black (0.14f));
        g.fillRoundedRectangle (r.expanded ((float) i).translated (0.0f, (float) i), corner + (float) i);
    }
    g.setGradientFill (real::vertical (juce::Colour (0xff2a2c30), r.getY(), juce::Colour (0xff121315), r.getBottom()));
    g.fillRoundedRectangle (r, corner);
    juce::Path face;
    face.addRoundedRectangle (r, corner);
    real::fillTexture (g, face, 0.6f);
    auto bevel = real::vertical (real::white (0.22f), r.getY(), real::black (0.6f), r.getBottom());
    bevel.addColour (0.08, real::white (0.04f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

// faixa verde-limão do nome (alumínio anodizado verde)
void limePlate (juce::Graphics& g, RF r, float corner)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (real::black (0.12f));
        g.fillRoundedRectangle (r.expanded ((float) i).translated (0.0f, (float) i), corner + (float) i);
    }
    auto body = real::vertical (juce::Colour (0xff9fe446), r.getY(), juce::Colour (0xff5fa519), r.getBottom());
    body.addColour (0.45, juce::Colour (0xff86d12f));
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);
    juce::Path face;
    face.addRoundedRectangle (r, corner);
    real::fillTexture (g, face, 0.5f);
    g.setGradientFill (real::vertical (real::white (0.25f), r.getY(), real::white (0.0f), r.getY() + r.getHeight() * 0.45f));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f), corner);
    auto bevel = real::vertical (real::white (0.55f), r.getY(), real::black (0.45f), r.getBottom());
    bevel.addColour (0.08, real::white (0.1f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

// logo: fatia de limão (casca verde, parte branca e os gomos)
void drawLogo (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setColour (real::black (0.5f));
    g.fillEllipse (real::circle (c.translated (0.0f, 1.5f), r + 1.0f));
    g.setGradientFill (real::radial (c.translated (-r * 0.3f, -r * 0.3f), r * 1.4f, juce::Colour (0xff8fd63a), juce::Colour (0xff2f6e0c)));
    g.fillEllipse (real::circle (c, r));
    g.setColour (juce::Colour (0xfff7f4d8));
    g.fillEllipse (real::circle (c, r * 0.84f));
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < 8; ++i)
    {
        const float a0 = (float) i * pi / 4.0f + 0.07f, a1 = (float) (i + 1) * pi / 4.0f - 0.07f;
        juce::Path seg;
        seg.addPieSegment (real::circle (c, r * 0.76f), a0, a1, 0.12f);
        g.setGradientFill (real::radial (c, r * 0.76f, juce::Colour (0xffe6fbb4), juce::Colour (0xffa6dc4c)));
        g.fillPath (seg);
    }
    g.setColour (real::white (0.35f));
    g.fillEllipse (real::circle (c.translated (-r * 0.35f, -r * 0.4f), r * 0.2f));
}

juce::String hzText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
}

constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuNoUser = 20005,
              menuImport = 20006, menuRemoveBank = 30000;   // menuRemoveBank + n = remove o banco importado n
} // namespace

// =====================================================================
// PASSO
// =====================================================================
StepCell::StepCell (LimaoProcessor& p, int stepIndex) : proc (p), index (stepIndex) {}

StepCell::Zone StepCell::zoneAt (juce::Point<float> pt) const
{
    if (noteRect().expanded (2.0f).contains (pt))  return noteZone;
    if (gateRect().contains (pt))                  return gateZone;
    if (accRect().contains (pt))                   return accZone;
    if (slideRect().contains (pt))                 return slideZone;
    return none;
}

void StepCell::paint (juce::Graphics& g)
{
    const auto st = proc.pattern.get (index);
    const float w = (float) getWidth();

    // número do passo (o primeiro de cada tempo mais claro) e o LED de "tocando agora"
    g.setFont (theme::mono (9.5f));
    g.setColour (index % 4 == 0 ? theme::cream : theme::dim);
    g.drawText (juce::String (index + 1).paddedLeft ('0', 2), RF (4.0f, 1.0f, 26.0f, 13.0f), juce::Justification::centredLeft, false);
    hw::led (g, { w - 11.0f, 8.0f }, 3.4f, lim::lemon, playing ? 1.0f : 0.0f);

    // visor da nota
    const auto nr = noteRect();
    real::screen (g, nr, 2.0f);
    if (playing)
    {
        g.setGradientFill (real::radial (nr.getCentre(), nr.getWidth() * 0.6f, lim::lemon.withAlpha (0.22f), lim::lemon.withAlpha (0.0f)));
        g.fillRect (nr);
    }
    if (st.mode == Mode::note)
    {
        g.setColour (st.accent ? juce::Colour (0xffffd27a) : lim::lemon);
        g.setFont (theme::mono (15.0f));
        g.drawText (noteName (scaledPitch (st.pitch, proc.scaleNow(), proc.rootNow())), nr, juce::Justification::centred, false);
    }
    else if (st.mode == Mode::tie)
    {
        // ligadura: arco segurando a nota anterior
        juce::Path arc;
        arc.startNewSubPath (nr.getCentreX() - 14.0f, nr.getCentreY() + 4.0f);
        arc.quadraticTo (nr.getCentreX(), nr.getCentreY() - 8.0f, nr.getCentreX() + 14.0f, nr.getCentreY() + 4.0f);
        g.setColour (theme::ice);
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (theme::dim.withAlpha (0.6f));
        g.fillRect (nr.getCentreX() - 8.0f, nr.getCentreY() - 0.5f, 16.0f, 1.5f);
    }
    real::glass (g, nr, 2.0f);

    // tecla do modo: acende verde-limão no passo que está tocando (a luz corre pelas teclas, como numa bateria eletrônica)
    {
        const auto cap = hw::keycap (g, gateRect(), 3.0f, pressed == gateZone, lim::lemon,
                                     playing && st.mode != Mode::rest ? 1.0f : 0.0f);
        const juce::String txt = st.mode == Mode::note ? "NOTA" : st.mode == Mode::tie ? "LIGA" : "PAUSA";
        const auto col = st.mode == Mode::note ? theme::cream : st.mode == Mode::tie ? theme::ice : theme::dim;
        g.setFont (theme::sans (8.5f, true, 0.1f));
        real::engraved (g, txt, cap, col, juce::Justification::centred);
    }

    // ACC e SLD: tecla com o LED dentro
    auto smallKey = [&g] (RF r, bool down, bool on, juce::Colour c, const juce::String& txt)
    {
        const auto cap = hw::keycap (g, r, 3.0f, down);
        hw::led (g, { cap.getX() + 6.0f, cap.getCentreY() }, 2.2f, c, on ? 1.0f : 0.0f);
        g.setFont (theme::sans (7.0f, true));
        real::engraved (g, txt, cap.withTrimmedLeft (10.0f), on ? theme::cream : theme::dim, juce::Justification::centred);
    };
    smallKey (accRect(), pressed == accZone, st.accent, theme::coral, "ACC");
    smallKey (slideRect(), pressed == slideZone, st.slide, theme::ice, "SLD");

    // fora do comprimento: apagado
    if (! active)
    {
        g.setColour (real::black (0.6f));
        g.fillRect (getLocalBounds());
    }
}

void StepCell::mouseDown (const juce::MouseEvent& e)
{
    pressed = zoneAt (e.position);
    auto st = proc.pattern.get (index);
    switch (pressed)
    {
        case noteZone:  dragStartPitch = st.pitch; break;
        case gateZone:  st.mode = st.mode == Mode::note ? Mode::rest : st.mode == Mode::rest ? Mode::tie : Mode::note; break;
        case accZone:   st.accent = ! st.accent; break;
        case slideZone: st.slide = ! st.slide; break;
        case none:      break;
    }
    if (pressed != noteZone && pressed != none)
        proc.pattern.set (index, st);
    repaint();
}

void StepCell::mouseDrag (const juce::MouseEvent& e)
{
    if (pressed != noteZone) return;
    auto st = proc.pattern.get (index);
    // com SCALE, cada 6 px é um grau da escala (sem escala, um semitom)
    const int steps = -e.getDistanceFromDragStartY() / 6, sc = proc.scaleNow(), rt = proc.rootNow();
    int pitch = dragStartPitch + steps;
    if (scales::active (sc))
    {
        int m = 36 + scaledPitch (dragStartPitch, sc, rt);
        for (int k = 0; k < std::abs (steps); ++k) m = scales::step (m, steps, sc, rt);
        pitch = m - 36;
    }
    pitch = juce::jlimit (-12, 24, pitch);
    if (pitch == st.pitch && st.mode == Mode::note) return;
    if (std::abs (e.getDistanceFromDragStartY()) < 3) return;
    st.pitch = pitch;
    st.mode  = Mode::note;
    proc.pattern.set (index, st);
    repaint();
}

void StepCell::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (zoneAt (e.position) != noteZone) return;
    auto st = proc.pattern.get (index);
    st.mode = st.mode == Mode::note ? Mode::rest : Mode::note;
    proc.pattern.set (index, st);
    repaint();
}

void StepCell::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (zoneAt (e.position) != noteZone || w.deltaY == 0.0f) return;
    auto st = proc.pattern.get (index);
    const int dir = w.deltaY > 0.0f ? 1 : -1, sc = proc.scaleNow();
    st.pitch = juce::jlimit (-12, 24, scales::active (sc) ? scales::step (36 + st.pitch, dir, sc, proc.rootNow()) - 36 : st.pitch + dir);
    st.mode  = Mode::note;
    proc.pattern.set (index, st);
    repaint();
}

// =====================================================================
// OSCILOSCÓPIO
// =====================================================================
void ScopeView::tick()
{
    const int size = LimaoProcessor::scopeSize;
    const int w = proc.scopeWrite.load();
    for (int i = 0; i < size; ++i)
        ring[(size_t) i] = proc.scope[(w + i) % size];

    // prende a imagem num cruzamento por zero subindo, para a onda ficar parada na tela
    int start = size - window;
    for (int j = size - window; j > size - window - 1000 && j > 1; --j)
        if (ring[(size_t) j - 1] < 0.0f && ring[(size_t) j] >= 0.0f) { start = j; break; }
    shown.assign (ring.begin() + start, ring.begin() + start + window);

    cutoff = proc.voice.cutNow.load();
    accent = proc.voice.accNow.load();
    repaint();
}

void ScopeView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    real::screen (g, r, 4.0f);
    auto in = r.reduced (8.0f, 8.0f);

    // retícula gravada no vidro
    g.setColour (lim::lime.withAlpha (0.07f));
    for (int i = 1; i < 10; ++i) g.fillRect (in.getX() + in.getWidth() * (float) i / 10.0f, in.getY(), 1.0f, in.getHeight());
    for (int i = 1; i < 6; ++i)  g.fillRect (in.getX(), in.getY() + in.getHeight() * (float) i / 6.0f, in.getWidth(), 1.0f);
    g.setColour (lim::lime.withAlpha (0.16f));
    g.fillRect (in.getX(), in.getCentreY(), in.getWidth(), 1.0f);

    // traço de fósforo: halo largo, depois o fio aceso
    if (! shown.empty())
    {
        float peak = 0.05f;
        for (float v : shown) peak = juce::jmax (peak, std::abs (v));
        const float gain = 0.85f / juce::jmax (0.25f, peak);
        juce::Path trace;
        for (size_t i = 0; i < shown.size(); ++i)
        {
            const float x = in.getX() + in.getWidth() * (float) i / (float) (shown.size() - 1);
            const float y = in.getCentreY() - juce::jlimit (-1.0f, 1.0f, shown[i] * gain) * in.getHeight() * 0.45f;
            if (i == 0) trace.startNewSubPath (x, y); else trace.lineTo (x, y);
        }
        g.setColour (lim::lemon.withAlpha (0.10f));
        g.strokePath (trace, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (lim::lemon.withAlpha (0.25f));
        g.strokePath (trace, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (lim::lemon.brighter (0.6f));
        g.strokePath (trace, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setFont (theme::mono (10.0f));
    g.setColour (lim::lime.withAlpha (0.7f));
    g.drawText (juce::String::fromUTF8 ("OSCILOSCÓPIO"), RF (in.getX() + 2.0f, in.getY(), 160.0f, 14.0f), juce::Justification::centredLeft, false);
    g.drawText ("CUTOFF " + hzText (cutoff), RF (in.getRight() - 162.0f, in.getY(), 160.0f, 14.0f), juce::Justification::centredRight, false);

    // barrinha do accent acumulado (o "grito" que sobe com accents seguidos)
    const RF bar (in.getRight() - 112.0f, in.getBottom() - 10.0f, 110.0f, 5.0f);
    g.setColour (lim::lime.withAlpha (0.15f));
    g.fillRect (bar);
    g.setColour (theme::coral.withAlpha (0.9f));
    g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, accent)));
    g.setColour (lim::lime.withAlpha (0.7f));
    g.drawText ("ACCENT", RF (bar.getX() - 70.0f, bar.getY() - 5.0f, 66.0f, 14.0f), juce::Justification::centredRight, false);

    real::glass (g, r, 4.0f);
}

// =====================================================================
// EDITOR
// =====================================================================
LimaoEditor::LimaoEditor (LimaoProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, lim::baseW, lim::baseH);
    addAndMakeVisible (canvas);

    auto& st = proc.apvts;
    using lim::SilverKnob;

    // ---- cabeçalho: preset ----
    presetName = add<PresetName> ({ 712, 30, 220, 32 });
    presetName->onClick = [this] { showPresetMenu(); };
    auto* prev = add<IconButton> ({ 934, 31, 30, 30 }, IconButton::Icon::prev);
    auto* next = add<IconButton> ({ 966, 31, 30, 30 }, IconButton::Icon::next);
    auto* save = add<IconButton> ({ 998, 31, 30, 30 }, IconButton::Icon::save);
    prev->onClick = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    next->onClick = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    save->onClick = [this] { askNewPreset(); };

    // ---- VOZ ----
    const int ky = 122, kw = 80, kh = 100;
    add<SilverKnob> ({ 40,  ky, kw, kh }, st, id::tune,   "TUNE",      theme::cream);
    add<SilverKnob> ({ 120, ky, kw, kh }, st, id::cutoff, "CUTOFF",    lim::lime);
    add<SilverKnob> ({ 200, ky, kw, kh }, st, id::reso,   "RESONANCE", theme::amber);
    add<SilverKnob> ({ 280, ky, kw, kh }, st, id::envmod, "ENV MOD",   lim::lemon);
    add<SilverKnob> ({ 360, ky, kw, kh }, st, id::decay,  "DECAY",     theme::cream);
    add<SilverKnob> ({ 440, ky, kw, kh }, st, id::accent, "ACCENT",    theme::coral);
    add<SilverKnob> ({ 520, ky, kw, kh }, st, id::slide,  "SLIDE",     theme::ice);
    add<lim::ParamBox> ({ 104, 250, 104, 26 }, st, id::wave, lim::lemon);
    add<SilverKnob> ({ 618, 104, 150, 178 }, st, id::acid, juce::String::fromUTF8 ("ÁCIDO"), lim::lime, true);

    scope = add<ScopeView> ({ 792, 100, 370, 192 }, proc);

    // ---- EFEITOS ----
    const int fy = 336;
    add<lim::ParamBox> ({ 44, 370, 96, 26 }, st, id::drvType, theme::coral);
    add<SilverKnob> ({ 150, fy, kw, kh }, st, id::drive, "DRIVE", theme::coral);

    auto* dl = add<lim::Rocker> ({ 262, 340, 126, 34 }, "DELAY", theme::ice);
    attach (*dl, id::dlOn);
    add<lim::ParamBox> ({ 300, 394, 86, 26 }, st, id::dlDiv, theme::ice);
    add<SilverKnob> ({ 400, fy, kw, kh }, st, id::dlFb,  "FEEDBACK", theme::ice);
    add<SilverKnob> ({ 480, fy, kw, kh }, st, id::dlMix, "MIX",      theme::ice);

    auto* rv = add<lim::Rocker> ({ 600, 340, 126, 34 }, "REVERB", theme::violet);
    attach (*rv, id::rvOn);
    add<SilverKnob> ({ 740, fy, kw, kh }, st, id::rvSize, "SIZE", theme::violet);
    add<SilverKnob> ({ 820, fy, kw, kh }, st, id::rvMix,  "MIX",  theme::violet);

    add<SilverKnob> ({ 960, fy, kw, kh }, st, id::volume, "VOLUME", theme::cream);

    // ---- SEQUENCIADOR ----
    auto* seq = add<lim::Rocker> ({ 56, 478, 132, 34 }, "SEQUENCER", lim::lemon, true);
    attach (*seq, id::seqOn);
    add<lim::ParamBox> ({ 262, 482, 58, 26 }, st, id::seqLen, lim::lemon, std::vector<int> { 4, 8, 12, 16, 24, 32 });
    add<lim::ParamBox> ({ 382, 482, 72, 26 }, st, id::seqRate, lim::lemon);
    add<lim::ParamBox> ({ 522, 482, 64, 26 }, st, id::swing, lim::lemon);

    auto* left  = add<lim::ActionKey> ({ 616, 481, 34, 28 }, "<");
    auto* right = add<lim::ActionKey> ({ 652, 481, 34, 28 }, ">");
    auto* down  = add<lim::ActionKey> ({ 714, 481, 34, 28 }, juce::String::fromUTF8 ("−1"));
    auto* up    = add<lim::ActionKey> ({ 750, 481, 34, 28 }, "+1");
    add<lim::ParamBox> ({ 800, 482, 112, 26 }, st, id::scale, lim::lemon);
    add<lim::ParamBox> ({ 916, 482, 52, 26 }, st, id::root, lim::lemon);
    auto* rnd   = add<lim::ActionKey> ({ 978, 481, 100, 28 }, "ALEATORIZAR");
    auto* clr   = add<lim::ActionKey> ({ 1082, 481, 62, 28 }, "LIMPAR");
    left->onClick  = [this] { proc.shiftPattern (-1); };
    right->onClick = [this] { proc.shiftPattern (1); };
    down->onClick  = [this] { proc.transposePattern (-1); };
    up->onClick    = [this] { proc.transposePattern (1); };
    rnd->onClick   = [this] { proc.randomizePattern(); };
    clr->onClick   = [this] { proc.clearPattern(); };

    for (int i = 0; i < maxSteps; ++i)
    {
        const int row = i / 16, col = i % 16;
        cells[(size_t) i] = add<StepCell> ({ 56 + col * 68, 524 + row * 100, 66, 96 }, proc, i);
    }

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto.
    // o tamanho salvo é lido ANTES do setResizeLimits: ele encolhe a janela ainda vazia para o mínimo
    // e o resized() gravaria 75% por cima do que o usuário escolheu
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) lim::baseW * lim::minScale), juce::roundToInt ((float) lim::baseH * lim::minScale),
                     juce::roundToInt ((float) lim::baseW * lim::maxScale), juce::roundToInt ((float) lim::baseH * lim::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) lim::baseW / (double) lim::baseH);
    setScale (openScale);
    startTimerHz (30);
}

LimaoEditor::~LimaoEditor()
{
    dialog.close();
    stopTimer();
    setLookAndFeel (nullptr);
}

void LimaoEditor::attach (juce::Button& b, const juce::String& paramId)
{
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramId, b));
}

// ---------------- menu de presets (com o tamanho da janela) ----------------
void LimaoEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto names = proc.getPresetNames();
    const auto bankNames = proc.getBankNames();
    const int  cur = proc.getPresetIndex();
    const int  numBanks = bankNames.size();

    // Init no topo; cada banco (FACTORY 1 e os importados, embaixo dele) abre as categorias; os do usuário em USER.
    // A categoria numCategories é OUTROS (preset de banco importado sem categoria).
    std::vector<std::array<juce::PopupMenu, numCategories + 1>> cats ((size_t) numBanks);
    std::vector<std::array<bool, numCategories + 1>> catTicked ((size_t) numBanks);
    juce::PopupMenu user;
    bool userTicked = false;
    for (int i = 0; i < names.size() && i + 1 < sizeMenuBase; ++i)
    {
        const int  c = proc.getPresetCategory (i);
        const int  b = proc.getPresetBank (i);
        const bool ticked = i == cur;
        if (c < 0)
            menu.addItem (i + 1, names[i], true, ticked);
        else if (b == LimaoProcessor::bankUser)
        {
            user.addItem (i + 1, names[i], true, ticked);
            userTicked = userTicked || ticked;
        }
        else if (b >= 0 && b < numBanks)
        {
            const auto k = (size_t) juce::jlimit (0, numCategories, c);
            cats[(size_t) b][k].addItem (i + 1, names[i], true, ticked);
            catTicked[(size_t) b][k] = catTicked[(size_t) b][k] || ticked;
        }
    }
    menu.addSeparator();
    for (int b = 0; b < numBanks; ++b)
    {
        juce::PopupMenu bank;
        bool bankTicked = false;
        for (int c = 0; c <= numCategories; ++c)
        {
            auto& sub = cats[(size_t) b][(size_t) c];
            if (sub.getNumItems() == 0) continue;
            const bool t = catTicked[(size_t) b][(size_t) c];
            bank.addSubMenu (c < numCategories ? u8 (categoryNames[c]) : juce::String ("OUTROS"), sub, true,
                             std::unique_ptr<juce::Drawable>(), t);
            bankTicked = bankTicked || t;
        }
        if (bank.getNumItems() > 0)
            menu.addSubMenu (bankNames[b].toUpperCase(), bank, true, std::unique_ptr<juce::Drawable>(), bankTicked);
    }
    if (user.getNumItems() > 0)
        menu.addSubMenu ("USER", user, true, std::unique_ptr<juce::Drawable>(), userTicked);
    else
        menu.addItem (menuNoUser, "USER (nenhum ainda)", false);

    // presets do usuário dá para gravar por cima, renomear e apagar; os de fábrica e os dos bancos não mudam
    menu.addSeparator();
    menu.addItem (menuSaveAs, u8 ("Salvar como novo preset…"));
    if (proc.isUserPreset())
    {
        const auto name = proc.getPresetName();
        menu.addItem (menuOverwrite, u8 ("Salvar alterações em “") + name + u8 ("”"));
        menu.addItem (menuRename,    u8 ("Renomear “") + name + u8 ("”…"));
        menu.addItem (menuDelete,    u8 ("Apagar “") + name + u8 ("”…"));
    }

    // bancos avulsos (.lmobank)
    menu.addSeparator();
    menu.addItem (menuImport, u8 ("Importar banco de presets…"));
    if (numBanks > 1)
    {
        juce::PopupMenu remove;
        for (int b = 1; b < numBanks; ++b)
            remove.addItem (menuRemoveBank + b, bankNames[b].toUpperCase());
        menu.addSubMenu ("Remover banco", remove);
    }

    juce::PopupMenu sizes;
    const float current = proc.uiScale.load();
    for (int pct : { 75, 100, 125, 150, 175, 200 })
        sizes.addItem (sizeMenuBase + pct, juce::String (pct) + "%", true, std::abs (current * 100.0f - (float) pct) < 1.0f);
    menu.addSeparator();
    menu.addSubMenu ("Tamanho da janela", sizes);
   #ifdef JucePlugin_VersionString   // (a foto do workflow shots compila a tela fora do plugin)
    menu.addSeparator();
    menu.addItem (sizeMenuBase - 1, juce::String::fromUTF8 ("LIMÃO ") + JucePlugin_VersionString, false, false);
   #endif

    // o menu (e cada camada: banco → categoria → preset) abre DENTRO da janela do plugin: como janela solta,
    // a camada de dentro ficava por baixo da janela do plugin (que o host deixa sempre na frente) e não dava clique
    juce::Component::SafePointer<LimaoEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetName).withParentComponent (this), [safe] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->presetMenuChosen (result);
    });
}

void LimaoEditor::askNewPreset()
{
    juce::Component::SafePointer<LimaoEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

// abre o navegador de arquivos; o banco escolhido é copiado para a pasta do LIMÃO e entra no menu
void LimaoEditor::importBank()
{
    auto start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    if (start.getChildFile ("Downloads").isDirectory()) start = start.getChildFile ("Downloads");
    chooser = std::make_unique<juce::FileChooser> (u8 ("Importar banco de presets do LIMÃO"), start, "*.lmobank");

    juce::Component::SafePointer<LimaoEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (safe == nullptr || ! file.existsAsFile()) return;
                              juce::String name;
                              const auto err = safe->proc.importBank (file, name);
                              if (err.isNotEmpty())
                                  safe->dialog.inform (safe->canvas, u8 ("Não deu para importar"), err);
                              else
                                  safe->dialog.inform (safe->canvas, "Banco importado",
                                                       u8 ("“") + name.toUpperCase() + u8 ("” já está no menu de presets, embaixo do FACTORY 1."));
                          });
}

void LimaoEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<LimaoEditor> safe (this);
    if (result > menuRemoveBank)
    {
        const int  bank = result - menuRemoveBank;
        const auto bankName = proc.getBankNames()[bank].toUpperCase();
        dialog.confirm (canvas, "Remover banco",
                        u8 ("“") + bankName + u8 ("” vai para a lixeira e sai do menu. Os presets do USER não mudam."), "Remover",
                        [safe, bank] { if (safe != nullptr) safe->proc.removeBank (bank); });
        return;
    }

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
        case menuImport:
            importBank();
            break;
        case menuNoUser:
            break;
        default:
            if (result > 0 && result < sizeMenuBase)
                proc.loadPreset (result - 1);
            break;
    }
}

// ---------------- timer (30 Hz) ----------------
void LimaoEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) lim::baseW - openScale) > 0.005f)
        setScale (openScale);

    if (scope != nullptr) scope->tick();

    const int step = proc.playStep.load();
    const int len  = juce::roundToInt (proc.apvts.getRawParameterValue (id::seqLen)->load());
    // trocar SCALE ou ROOT muda o nome da nota nos visores: conta como padrão novo
    const int ver  = proc.patternVersion.load() * 256 + proc.scaleNow() * 12 + proc.rootNow();
    if (step != lastStep || len != lastLen || ver != lastVersion)
    {
        for (int i = 0; i < maxSteps; ++i)
        {
            auto* c = cells[(size_t) i];
            c->setPlaying (i == step);
            c->setActive (i < len);
            if (ver != lastVersion) c->repaint();
        }
        lastStep = step;
        lastLen = len;
        lastVersion = ver;
    }

    level = juce::jmax (proc.meterOut.exchange (0.0f), level * 0.85f);
    if (std::abs (level - shownLevel) > 0.002f)
    {
        shownLevel = level;
        canvas.repaint (1040, 22, 124, 48);
    }

    presetName->set (proc.getPresetName(), proc.getPresetIndex(), proc.getNumPresets());

    const float bpm = proc.shownBpm.load();
    const bool  syn = proc.synced.load();
    if (std::abs (bpm - lastBpm) > 0.05f || syn != lastSync)
    {
        lastBpm = bpm;
        lastSync = syn;
        canvas.repaint (540, 20, 168, 50);
    }
}

// ---------------- tamanho ----------------
void LimaoEditor::setScale (float scale)
{
    scale = juce::jlimit (lim::minScale, lim::maxScale, scale);
    setSize (juce::roundToInt ((float) lim::baseW * scale), juce::roundToInt ((float) lim::baseH * scale));
}

void LimaoEditor::resized()
{
    const float scale = (float) getWidth() / (float) lim::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void LimaoEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0c0d));
}

// ---------------- o aparelho ----------------
void LimaoEditor::drawCanvas (juce::Graphics& g)
{
    const float W = (float) lim::baseW, H = (float) lim::baseH;
    g.fillAll (juce::Colour (0xff0c0c0d));

    // corpo prateado entre as duas laterais pretas
    lim::silver (g, { 22.0f, 0.0f, W - 44.0f, H }, 2.0f);
    hw::cheek (g, { 0.0f, 0.0f, 24.0f, H }, true);
    hw::cheek (g, { W - 24.0f, 0.0f, 24.0f, H }, false);

    const juce::Point<float> screws[] = { { 34.0f, 96.0f }, { W - 34.0f, 96.0f }, { 34.0f, 434.0f }, { W - 34.0f, 434.0f },
                                          { 34.0f, H - 8.0f }, { W - 34.0f, H - 8.0f } };
    for (const auto& sc : screws)
        real::screw (g, sc, 3.6f, sc.x * 0.03f + sc.y * 0.011f);

    drawHeader (g);

    // ---- VOZ ----
    lim::sectionTitle (g, 44.0f, 98.0f, 556.0f, "VOZ");
    lim::inkText (g, "WAVE", { 44.0f, 250.0f, 56.0f, 26.0f }, juce::Justification::centredLeft, 9.5f);
    lim::inkText (g, juce::String::fromUTF8 ("ACCENT soma volume e corte  ·  SLIDE desliza até a próxima nota"),
                  { 222.0f, 250.0f, 388.0f, 26.0f }, juce::Justification::centredLeft, 8.0f, false, lim::inkDim);
    lim::sectionTitle (g, 622.0f, 98.0f, 142.0f, "MACRO");

    // ---- EFEITOS ----
    lim::sectionTitle (g, 44.0f, 310.0f, 196.0f, "DRIVE");
    lim::inkText (g, "TIPO", { 44.0f, 352.0f, 96.0f, 14.0f }, juce::Justification::centred, 8.5f);
    lim::sectionTitle (g, 262.0f, 310.0f, 298.0f, "DELAY");
    lim::inkText (g, "TIME", { 262.0f, 394.0f, 36.0f, 26.0f }, juce::Justification::centredLeft, 8.5f);
    lim::sectionTitle (g, 600.0f, 310.0f, 300.0f, "REVERB");
    lim::sectionTitle (g, 940.0f, 310.0f, 222.0f, juce::String::fromUTF8 ("SAÍDA"));

    // ---- SEQUENCIADOR ----
    const RF plate (40.0f, 446.0f, W - 80.0f, 282.0f);
    blackPlate (g, plate, 4.0f);
    real::screw (g, { plate.getX() + 8.0f, plate.getY() + 8.0f }, 3.0f, 0.7f);
    real::screw (g, { plate.getRight() - 8.0f, plate.getY() + 8.0f }, 3.0f, 1.9f);
    g.setFont (theme::sans (10.5f, true, 0.16f));
    real::engraved (g, "SEQUENCIADOR", { 60.0f, 452.0f, 200.0f, 18.0f }, theme::cream, juce::Justification::centredLeft);
    g.setColour (lim::lemon.withAlpha (0.8f));
    g.fillRect (60.0f, 471.0f, 22.0f, 2.0f);
    g.setFont (theme::sans (8.5f, true, 0.14f));
    real::engraved (g, "PASSOS", { 204.0f, 482.0f, 54.0f, 26.0f }, theme::dim, juce::Justification::centredRight);
    real::engraved (g, "RATE",   { 330.0f, 482.0f, 48.0f, 26.0f }, theme::dim, juce::Justification::centredRight);
    real::engraved (g, "SWING",  { 464.0f, 482.0f, 54.0f, 26.0f }, theme::dim, juce::Justification::centredRight);
    real::engraved (g, "GIRAR",  { 616.0f, 509.0f, 70.0f, 12.0f }, theme::dim, juce::Justification::centred);
    real::engraved (g, "TRANSP", { 714.0f, 509.0f, 70.0f, 12.0f }, theme::dim, juce::Justification::centred);
    real::engraved (g, "SCALE",  { 800.0f, 509.0f, 112.0f, 12.0f }, theme::dim, juce::Justification::centred);
    real::engraved (g, "ROOT",   { 916.0f, 509.0f, 52.0f, 12.0f }, theme::dim, juce::Justification::centred);
    g.setFont (theme::sans (8.0f, false, 0.08f));
    real::engraved (g, juce::String::fromUTF8 ("ARRASTE NO VISOR = NOTA · DUPLO CLIQUE = PAUSA · TECLADO TRANSPÕE (DÓ 3 = 0) · "
                                               "SCALE + ROOT ENCAIXAM AS NOTAS NA ESCALA"),
                    { 270.0f, 452.0f, 874.0f, 18.0f }, theme::dim, juce::Justification::centredRight);

    // marcas de tempo (cada 4 passos) embaixo das duas fileiras
    for (int row = 0; row < 2; ++row)
        for (int beat = 0; beat < 4; ++beat)
        {
            const float x = 56.0f + (float) (beat * 4) * 68.0f, y = 621.0f + (float) row * 100.0f;
            g.setColour (beat % 2 == 0 ? lim::lemon.withAlpha (0.55f) : theme::dim.withAlpha (0.5f));
            g.fillRect (x + 2.0f, y, 4.0f * 68.0f - 6.0f, 1.5f);
        }
}

void LimaoEditor::drawHeader (juce::Graphics& g)
{
    const RF plate (40.0f, 12.0f, (float) lim::baseW - 80.0f, 70.0f);
    limePlate (g, plate, 4.0f);
    real::screw (g, { plate.getX() + 9.0f, plate.getCentreY() }, 3.2f, 0.3f);
    real::screw (g, { plate.getRight() - 9.0f, plate.getCentreY() }, 3.2f, 2.4f);

    drawLogo (g, { 82.0f, 47.0f }, 21.0f);

    const juce::Colour darkGreen (0xff1d3a08);
    g.setFont (theme::sans (30.0f, true, 0.14f));
    g.setColour (real::black (0.3f));
    g.drawText (juce::String::fromUTF8 ("LIMÃO"), RF (114.0f, 23.5f, 150.0f, 50.0f), juce::Justification::centredLeft);
    g.setColour (juce::Colours::white);
    g.drawText (juce::String::fromUTF8 ("LIMÃO"), RF (114.0f, 22.0f, 150.0f, 50.0f), juce::Justification::centredLeft);
    g.setColour (darkGreen.withAlpha (0.45f));
    g.fillRect (262.0f, 34.0f, 1.0f, 26.0f);
    g.setFont (theme::sans (9.5f, true, 0.2f));
    g.setColour (darkGreen);
    g.drawText (juce::String::fromUTF8 ("BASSLINE ÁCIDO · MONOFÔNICO"), RF (274.0f, 32.0f, 230.0f, 13.0f), juce::Justification::centredLeft);
    g.drawText ("SEQUENCIADOR DE 32 PASSOS", RF (274.0f, 46.0f, 230.0f, 13.0f), juce::Justification::centredLeft);
    g.setColour (darkGreen.withAlpha (0.45f));
    g.fillRect (470.0f, 34.0f, 1.0f, 26.0f);
    g.setColour (darkGreen);
    g.drawText ("BY", RF (482.0f, 32.0f, 60.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour (juce::Colours::white);
    g.setFont (theme::sans (10.5f, true, 0.06f));
    g.drawText ("InsideStudioSP", RF (482.0f, 45.0f, 110.0f, 13.0f), juce::Justification::centredLeft);

    // BPM e estado do relógio (com o sequenciador ligado, parado = espera o play)
    {
        const bool seqOn = proc.apvts.getRawParameterValue (id::seqOn)->load() > 0.5f;
        const bool syn = proc.synced.load();
        const juce::String state = syn ? "SYNC" : seqOn ? "AGUARDANDO PLAY" : "LIVRE";
        g.setFont (theme::mono (11.0f));
        g.setColour (darkGreen);
        g.drawText (juce::String (proc.shownBpm.load(), 1) + " BPM", RF (590.0f, 30.0f, 112.0f, 14.0f), juce::Justification::centredRight);
        g.setColour (syn ? juce::Colours::white : darkGreen.withAlpha (0.75f));
        g.drawFittedText (state, juce::Rectangle<int> (560, 45, 142, 14), juce::Justification::centredRight, 1, 0.8f);
        hw::led (g, { 600.0f, 38.0f }, 3.4f, lim::lemon, syn ? 1.0f : 0.0f);
    }

    drawMeter (g);
}

// medidor de saída em LEDs (pico), na faixa do nome
void LimaoEditor::drawMeter (juce::Graphics& g)
{
    const int leds = 12;
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float v  = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -42.0f, 0.0f, 0.0f, 1.0f));
    for (int i = 0; i < leds; ++i)
    {
        const float f = (float) i / (float) (leds - 1);
        const auto c = f > 0.85f ? theme::coral : f > 0.6f ? theme::amber : lim::lemon;
        real::ledBar (g, { 1046.0f + (float) i * 9.5f, 34.0f, 6.5f, 12.0f }, c, level > 0.0005f && f <= v ? 1.0f : 0.0f, 0.4f);
    }
    g.setFont (theme::sans (8.0f, true, 0.14f));
    g.setColour (juce::Colour (0xff1d3a08));
    g.drawText ("OUT", RF (1046.0f, 50.0f, 110.0f, 12.0f), juce::Justification::centred);
}
