#include "PluginEditor.h"
#include "Atlas.h"
#include "PluginImg.h"

using namespace mesa;

namespace
{
constexpr int menuLicenca = 20007, menuRenomear = 20008, menuSobre = 20009;
constexpr float closeCm = lay::WOOD_W + lay::EDGE + 6.0f * lay::SW;   // PERTO: uns 6 canais na largura da janela
constexpr float maxZoom = 140.0f;                                     // px por cm (as peças estão a 56–64 px/cm)

const juce::Colour cBase (0xff0b0b0c), cLegend (0xffece8dc), cAmber (0xfff2c14e), cInk (0xff16182a);

juce::Font uiFont (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}
juce::Font handFont (float h)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Marker Felt", h, juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions ("Segoe Print", h, juce::Font::plain));
   #endif
}

// posição do fader (0 = topo, 1 = fundo) <-> dB, pela escala serigrafada na chapa
const float faderDbs[9] = { 10, 5, 0, -5, -10, -20, -30, -50, faderMin };
float faderFrac (float db)
{
    for (int i = 0; i < 8; ++i)
        if (db >= faderDbs[i + 1])
            return juce::jmap (db, faderDbs[i], faderDbs[i + 1], lay::FAD_MARKS[i], lay::FAD_MARKS[i + 1]);
    return 1.0f;
}
float faderDb (float frac)
{
    frac = juce::jlimit (0.0f, 1.0f, frac);
    for (int i = 0; i < 8; ++i)
        if (frac <= lay::FAD_MARKS[i + 1])
            return juce::jmap (frac, lay::FAD_MARKS[i], lay::FAD_MARKS[i + 1], faderDbs[i], faderDbs[i + 1]);
    return faderMin;
}

// VU: dB da escala -> ângulo da agulha (graus, 0 = em pé), a mesma tabela do render
const float vuDb[11]  = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
const float vuDeg[11] = { -47, -30, -21, -13, -3, 3, 9.5f, 17, 25, 33, 45 };
float vuAngle (float v)
{
    if (v <= vuDb[0]) return vuDeg[0] - juce::jmin (6.0f, (vuDb[0] - v) * 0.6f);   // encosta no pino
    for (int i = 0; i < 10; ++i)
        if (v <= vuDb[i + 1]) return juce::jmap (v, vuDb[i], vuDb[i + 1], vuDeg[i], vuDeg[i + 1]);
    return juce::jmin (50.0f, vuDeg[10] + (v - 3.0f) * 2.0f);
}

const float chThr[lay::METER_N] = { -60, -50, -42, -36, -30, -26, -22, -18, -15, -12, -10, -8, -6, -5, -4, -3, -2, -1, -0.5f, 0 };
const float mThr[lay::MMETER_N] = { -60, -54, -48, -42, -36, -32, -28, -24, -21, -18, -15, -12, -10, -8, -6, -5, -4, -3, -2, -1.5f, -1, -0.5f, -0.2f, 0 };
const float grThr[5] = { 1, 3, 6, 10, 15 };

const char* ledColour (int i, int n) { const float f = (float) i / (float) (n - 1); return f < 0.68f ? "g" : (f < 0.86f ? "y" : "r"); }

// a cor (JPEG) e a transparência (PNG cinza) das peças viram uma imagem ARGB (a cor já vem multiplicada pelo alfa)
juce::Image juntaAlfa (const juce::Image& cor, const juce::Image& alfa)
{
    const int w = cor.getWidth(), h = cor.getHeight();
    juce::Image out (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
    if (w <= 0 || alfa.getWidth() != w || alfa.getHeight() != h) return out;
    const juce::Image::BitmapData dc (cor, juce::Image::BitmapData::readOnly), da (alfa, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dout (out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const auto c = dc.getPixelColour (x, y);
            const juce::uint8 a = da.pixelFormat == juce::Image::SingleChannel ? *da.getPixelPointer (x, y) : da.getPixelColour (x, y).getRed();
            juce::PixelARGB px;
            px.setARGB (a, juce::jmin (a, c.getRed()), juce::jmin (a, c.getGreen()), juce::jmin (a, c.getBlue()));
            *reinterpret_cast<juce::PixelARGB*> (dout.getPixelPointer (x, y)) = px;
        }
    return out;
}
} // namespace

// =====================================================================================================================
// imagens: nível 0 = como vieram do Blender; 1 = metade; 2 = um quarto (para a visão GERAL não serrilhar)
struct MesaEditor::Art
{
    juce::Image chapas[3], pecas[3];
    Art()
    {
        chapas[0] = juce::ImageFileFormat::loadFrom (PluginImg::chapas_jpg, PluginImg::chapas_jpgSize);
        pecas[0] = juntaAlfa (juce::ImageFileFormat::loadFrom (PluginImg::pecas_jpg, PluginImg::pecas_jpgSize),
                              juce::ImageFileFormat::loadFrom (PluginImg::pecas_a_png, PluginImg::pecas_a_pngSize));
    }
    const juce::Image& level (bool chapa, int l)
    {
        auto* v = chapa ? chapas : pecas;
        if (l > 0 && ! v[l].isValid())
        {
            const auto& s = level (chapa, l - 1);
            if (s.isValid())
                v[l] = s.rescaled (juce::jmax (1, s.getWidth() / 2), juce::jmax (1, s.getHeight() / 2), juce::Graphics::highResamplingQuality);
        }
        return v[l];
    }
};

// ------------------------------------------------------------------------------------------------ vista
float MesaEditor::sy (float layoutY, float za)
{
    constexpr float top = lay::Y_TOP * lay::TILT_COS + (lay::FRAME_Z - lay::SURF) * lay::TILT_SIN;
    return top - (layoutY * lay::TILT_COS + (za - lay::SURF) * lay::TILT_SIN);
}
float MesaEditor::consoleH() { return sy (lay::Y_BOT - 0.2f, lay::FLOOR_Z); }
float MesaEditor::fitZoom() const { return juce::jmax (1.0f, juce::jmin ((float) getWidth() / lay::TOTAL_W, (float) getHeight() / consoleH())); }
float MesaEditor::closeZoom() const { return juce::jlimit (fitZoom(), maxZoom, (float) getWidth() / closeCm); }

void MesaEditor::setView (float z, float vx, float vy)
{
    zoom = juce::jlimit (juce::jmin (fitZoom(), maxZoom), maxZoom, z);
    const float vw = (float) getWidth() / zoom, vh = (float) getHeight() / zoom;
    // console menor que a janela: centraliza; maior: não deixa passar da borda
    viewX = vw >= lay::TOTAL_W ? (lay::TOTAL_W - vw) / 2 : juce::jlimit (0.0f, lay::TOTAL_W - vw, vx);
    viewY = vh >= consoleH() ? (consoleH() - vh) / 2 : juce::jlimit (0.0f, consoleH() - vh, vy);
    overview = zoom <= fitZoom() * 1.001f;
    updateScrollBars();
    cacheDirty = true;
    repaint();
}

void MesaEditor::zoomAround (float newZoom, juce::Point<float> pos)
{
    const auto c = toCm (pos);
    newZoom = juce::jlimit (fitZoom(), maxZoom, newZoom);
    setView (newZoom, c.x - pos.x / newZoom, c.y - pos.y / newZoom);
}

void MesaEditor::focusOn (float cx)
{
    setView (zoom, cx - (float) getWidth() / zoom * 0.5f, viewY);
}

void MesaEditor::updateScrollBars()
{
    const float vw = (float) getWidth() / zoom, vh = (float) getHeight() / zoom;
    hBar.setRangeLimits (0.0, lay::TOTAL_W);
    hBar.setCurrentRange (viewX, vw, juce::dontSendNotification);
    hBar.setVisible (vw < lay::TOTAL_W - 0.01f);
    vBar.setRangeLimits (0.0, consoleH());
    vBar.setCurrentRange (viewY, vh, juce::dontSendNotification);
    vBar.setVisible (vh < consoleH() - 0.01f);
    // a barra das vagas de canal fica no descanso de braço (anda com a vista)
    const auto r = juce::Rectangle<float> (lay::X_CH0, sy (lay::SEC_FAD[1] - lay::ARM * 0.62f), lay::VIS * lay::SW, lay::ARM * 0.2f)
                       .transformedBy (viewXf()).toNearestInt();
    scrollBar.setBounds (r);
}

juce::Rectangle<float> MesaEditor::btnRect (Btn b) const
{
    const float s = 26.0f, gap = 4.0f, y = 8.0f, wGeral = 64.0f;
    float x = (float) getWidth() - (vBar.isVisible() ? 14.0f : 0.0f) - 8.0f;
    if (b == Btn::overview) return { x - wGeral, y, wGeral, s };
    x -= wGeral + gap;
    if (b == Btn::zoomIn) return { x - s, y, s, s };
    x -= s + gap;
    if (b == Btn::zoomOut) return { x - s, y, s, s };
    return {};
}

MesaEditor::Btn MesaEditor::btnAt (juce::Point<float> p) const
{
    for (auto b : { Btn::zoomOut, Btn::zoomIn, Btn::overview })
        if (btnRect (b).contains (p)) return b;
    return Btn::none;
}

// =====================================================================================================================
MesaEditor::MesaEditor (MesaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    for (auto& a : atlas::chapas) chapaCell[a.nome] = { { a.x, a.y, a.w, a.h }, a.ax, a.ay };
    for (auto& a : atlas::pecas)  pecaCell[a.nome]  = { { a.x, a.y, a.w, a.h }, a.ax, a.ay };

    for (auto* sb : { &scrollBar, &hBar, &vBar })
    {
        addChildComponent (sb);
        sb->addListener (this);
        sb->setAutoHide (false);
        sb->setColour (juce::ScrollBar::thumbColourId, juce::Colour (0x88ece8dc));
        sb->setColour (juce::ScrollBar::trackColourId, juce::Colour (0x40000000));
    }

    proc.registry->listeners.add (this);
    rebuild();

    const int w0 = proc.uiW.load(), h0 = proc.uiH.load();
    setResizable (true, true);
    setResizeLimits (480, 400, 4200, 2600);
    setSize (w0 > 0 ? juce::jlimit (480, 4200, w0) : 1180, h0 > 0 ? juce::jlimit (400, 2600, h0) : 860);
    setView (proc.uiGeral.load() ? 0.0f : closeZoom(), 0.0f, 0.0f);
    for (auto& s : strips)
        if (s.p == &proc) focusOn (s.cx);
    if (proc.mode() == modeMaster) focusOn (lay::X_M0 + lay::MW * 0.5f);
    startTimerHz (30);
    ativacao.anexar (*this);
}

MesaEditor::~MesaEditor()
{
    end();
    proc.registry->listeners.remove (this);
}

// ------------------------------------------------------------------------------------------------ quem está na console
void MesaEditor::rebuild()
{
    auto& reg = *proc.registry;
    lastVersion = reg.version.load();
    channels.clear();
    MesaProcessor* groups[3] = {};
    master = nullptr;
    {
        const juce::ScopedLock sl (reg.lock);
        for (auto* q : reg.list)
        {
            const int m = q->mode();
            if (m == modeChannel) channels.push_back (q);
            else if (m == modeGroup) { auto& g = groups[juce::jlimit (1, 3, q->group()) - 1]; if (g == nullptr) g = q; }
            else if (master == nullptr) master = q;
        }
    }
    std::sort (channels.begin(), channels.end(), [] (auto* a, auto* b) { return a->seq() < b->seq(); });

    const int n = (int) channels.size();
    scroll = juce::jlimit (0, juce::jmax (0, n - lay::VIS), scroll);
    // a própria cópia fora da vista: rola até ela ao abrir
    if (openSettle == 0)
        for (int i = 0; i < n; ++i)
            if (channels[(size_t) i] == &proc && (i < scroll || i >= scroll + lay::VIS)) scroll = juce::jlimit (0, juce::jmax (0, n - lay::VIS), i - lay::VIS / 2);

    strips.clear();
    for (int i = 0; i < lay::VIS; ++i)
    {
        Strip s;
        s.slot = i;
        s.cx = lay::X_CH0 + lay::SW * ((float) i + 0.5f);
        const int idx = scroll + i;
        if (idx < n) s.p = channels[(size_t) idx];
        s.number = juce::String (idx + 1).paddedLeft ('0', 2);
        strips.push_back (s);
    }
    for (int k = 0; k < 3; ++k)
    {
        Strip s;
        s.slot = k; s.group = true; s.p = groups[k];
        s.cx = lay::X_GP0 + lay::SW * ((float) k + 0.5f);
        s.number = "G" + juce::String (k + 1);
        strips.push_back (s);
    }

    scrollBar.setRangeLimits (0.0, (double) juce::jmax (n, lay::VIS));
    scrollBar.setCurrentRange ((double) scroll, (double) lay::VIS, juce::dontSendNotification);
    scrollBar.setVisible (n > lay::VIS);

    for (auto it = meters.begin(); it != meters.end();)
    {
        bool alive = it->first == master;
        for (auto& s : strips) alive = alive || s.p == it->first;
        it = alive ? std::next (it) : meters.erase (it);
    }
    hover = active = -1;
    gestureParam = nullptr;
    buildCtls();
    cacheDirty = true;
    repaint();
}

void MesaEditor::scrollBarMoved (juce::ScrollBar* sb, double start)
{
    if (sb == &hBar) { setView (zoom, (float) start, viewY); return; }
    if (sb == &vBar) { setView (zoom, viewX, (float) start); return; }
    const int s = juce::roundToInt (start);
    if (s != scroll) { scroll = s; openSettle = juce::jmax (openSettle, 1); rebuild(); }
}

void MesaEditor::buildCtls()
{
    ctls.clear();
    for (auto& s : strips)
        if (s.p != nullptr) addStripCtls (s);
    addMasterCtls();
    drawOrder.resize (ctls.size());
    for (size_t i = 0; i < ctls.size(); ++i) drawOrder[i] = (int) i;
    std::stable_sort (drawOrder.begin(), drawOrder.end(), [this] (int a, int b) { return ctls[(size_t) a].y < ctls[(size_t) b].y; });
}

void MesaEditor::addStripCtls (const Strip& s)
{
    auto* p = s.p;
    auto add = [&] (Kind k, const juce::String& pid, float x, float yLayout, float d, const juce::String& spr = {}, int v = 0)
    {
        Ctl c; c.kind = k; c.p = p; c.pid = pid; c.x = s.cx + x; c.ly = yLayout; c.y = sy (yLayout); c.d = d; c.sprite = spr; c.value = v;
        ctls.push_back (c);
    };
    const int pre = juce::roundToInt (p->value (id::pre));
    const juce::String fam = pre == 1 ? "api" : (pre == 2 ? "mar" : "ssl");
    auto preK = [&] (const char* ssl, const char* mar) { return fam == "api" ? juce::String ("k_api") : (fam == "mar" ? juce::String (mar) : juce::String (ssl)); };
    add (Kind::preModel, id::pre, 0.0f, lay::PRE_NAME_Y, 2.9f);
    add (Kind::knob, id::gain,  lay::PRE_GAIN[0],  lay::PRE_GAIN[1],  lay::PRE_GAIN[2],  preK ("k_ssl_gry", "k_mar_gain"));
    add (Kind::knob, id::drive, lay::PRE_DRIVE[0], lay::PRE_DRIVE[1], lay::PRE_DRIVE[2], preK ("k_ssl_org", "k_mar_org"));
    add (Kind::knob, id::hpf,   lay::PRE_HPF[0],   lay::PRE_HPF[1],   lay::PRE_HPF[2],   preK ("k_ssl_blk", "k_mar"));
    add (Kind::toggle, id::pad, lay::PRE_PAD[0], lay::PRE_PAD[1], lay::PRE_PAD[2], "pad");

    const bool brit = juce::roundToInt (p->value (id::eqm)) == 1;
    const char* caps[4] = { "red", "grn", "blu", "brn" };
    add (Kind::eqModel, id::eqm, 0.0f, lay::EQ_NAME_Y, 2.9f);
    for (int b = 0; b < 4; ++b)
    {
        const juce::String spr = juce::String (brit ? "k_mar_" : "k_ssl_") + caps[b];
        add (Kind::knob, id::eqG[b], lay::EQ_GAIN_X, lay::EQ_ROW_Y[b], lay::EQ_GAIN_D, spr);
        add (Kind::knob, id::eqF[b], lay::EQ_FREQ_X, lay::EQ_ROW_Y[b], lay::EQ_FREQ_D, spr);
    }
    add (Kind::toggle, id::eqOn, lay::EQ_BTN[0], lay::EQ_BTN[1], lay::EQ_BTN[2], "eq");
    add (Kind::toggle, id::dynPre, lay::DYNPRE_BTN[0], lay::DYNPRE_BTN[1], lay::DYNPRE_BTN[2], "pre");

    add (Kind::knob, id::thr, lay::DYN_X[0], lay::DYN_KNOBS_Y, lay::DYN_D, "k_ssl_gry");
    add (Kind::knob, id::rat, lay::DYN_X[1], lay::DYN_KNOBS_Y, lay::DYN_D, "k_ssl_gry");
    add (Kind::knob, s.group ? id::cmix : id::gate, lay::DYN_X[2], lay::DYN_KNOBS_Y, lay::DYN_D, "k_ssl_grn");

    for (int k = 0; k < 3; ++k)
        add (Kind::radio, id::grp, lay::G_BTNS[k][0], lay::G_BTNS[k][1], lay::G_BTNS[k][2], "g" + juce::String (k + 1), k + 1);
    add (Kind::knob, id::pan, lay::PAN[0], lay::PAN[1], lay::PAN[2], "k_ssl_wht");
    add (Kind::toggle, id::mute, lay::MUTE[0], lay::MUTE[1], lay::MUTE[2], "m");
    if (! s.group) add (Kind::toggle, id::solo, lay::SOLO[0], lay::SOLO[1], lay::SOLO[2], "s");
    add (Kind::fader, id::fader, lay::FAD_X, lay::FAD_YT, (lay::FAD_YT - lay::FAD_YB) * lay::TILT_COS, "fcap");
    add (Kind::tape, {}, lay::TAPE[0], lay::TAPE[1], lay::TAPE[2]);
}

void MesaEditor::addMasterCtls()
{
    const float mx = lay::X_M0 + lay::MW * 0.5f;
    auto add = [&] (Kind k, MesaProcessor* p, const juce::String& pid, float x, float yLayout, float d, const juce::String& spr = {}, int v = 0)
    {
        Ctl c; c.kind = k; c.p = p; c.pid = pid; c.x = mx + x; c.ly = yLayout; c.y = sy (yLayout); c.d = d; c.sprite = spr; c.value = v;
        ctls.push_back (c);
    };
    for (int k = 0; k < 3; ++k) add (Kind::modeBtn, &proc, id::mode, lay::MODE_BTNS[k][0], lay::MODE_BTNS[k][1], lay::MODE_BTNS[k][2], "mode", k);
    for (int k = 0; k < 4; ++k) add (Kind::optBtn, &proc, {}, lay::OPT_BTNS[k][0], lay::OPT_BTNS[k][1], lay::OPT_BTNS[k][2], k < 2 ? "optb" : "optg", k);
    if (master == nullptr) return;
    const juce::String pids[6] = { id::mthr, id::matk, id::mrel, id::mmkp, id::mrat, id::mmix };
    const char* caps[6] = { "gry", "blk", "blk", "org", "blk", "gry" };
    for (int k = 0; k < 6; ++k)
        add (Kind::knob, master, pids[k], lay::COLA_KNOBS[k][0], lay::COLA_KNOBS[k][1], lay::COLA_D, juce::String ("k_ssl_") + caps[k]);
    add (Kind::toggle, master, id::min, lay::COLA_IN[0], lay::COLA_IN[1], lay::COLA_IN[2], "in");
    add (Kind::toggle, master, id::mhpf, lay::COLA_HPF[0], lay::COLA_HPF[1], lay::COLA_HPF[2], "hpf");
    add (Kind::mfader, master, id::fader, lay::MFAD_X, lay::MFAD_YT, (lay::MFAD_YT - lay::MFAD_YB) * lay::TILT_COS, "mfcap");
    add (Kind::tape, master, {}, lay::MTAPE[0], lay::MTAPE[1], lay::MTAPE[2]);
}

// ------------------------------------------------------------------------------------------------ peças
void MesaEditor::drawCell (juce::Graphics& g, bool chapa, const juce::String& name, float x, float layoutY, float za, float scale, float stretchW)
{
    auto& cells = chapa ? chapaCell : pecaCell;
    const auto it = cells.find (name.toStdString());
    if (it == cells.end()) return;
    const auto& c = it->second;
    const float ppc = chapa ? atlas::ppcChapas : atlas::ppcPecas;
    const float kx = stretchW > 0.0f ? stretchW / ((float) c.r.getWidth() / ppc) : scale;
    const juce::Rectangle<float> dst (x - c.ax / ppc * kx, sy (layoutY, za) - c.ay / ppc * scale,
                                      (float) c.r.getWidth() / ppc * kx, (float) c.r.getHeight() / ppc * scale);
    if (! dst.intersects (drawVis)) return;

    // nível do atlas: o menor que ainda tem ~1,4 px da imagem por px da tela
    int lvl = 0;
    while (lvl < 2 && ppc / (float) (1 << (lvl + 1)) >= drawPx * scale * 1.4f) ++lvl;
    auto& img = clipped[{ (chapa ? "#" : "") + name, lvl }];
    if (! img.isValid())
    {
        const auto& src = art->level (chapa, lvl);
        const int f = 1 << lvl;
        if (src.isValid())
            img = src.getClippedImage ({ c.r.getX() / f, c.r.getY() / f, juce::jmax (1, c.r.getWidth() / f), juce::jmax (1, c.r.getHeight() / f) });
    }
    if (! img.isValid()) return;
    g.setOpacity (1.0f);   // o drawImage herda a transparência da última cor
    g.drawImageTransformed (img, juce::AffineTransform::scale (dst.getWidth() / (float) img.getWidth(), dst.getHeight() / (float) img.getHeight())
                                     .translated (dst.getX(), dst.getY()));
}

float MesaEditor::norm (const Ctl& c) const
{
    if (auto* prm = c.p != nullptr && c.pid.isNotEmpty() ? c.p->param (c.pid) : nullptr) return prm->getValue();
    return 0.0f;
}

void MesaEditor::setNorm (const Ctl& c, float v)
{
    if (c.p == nullptr) return;
    if (auto* prm = c.p->param (c.pid))
    {
        const bool own = gestureParam == prm;
        if (! own) prm->beginChangeGesture();
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
        if (! own) prm->endChangeGesture();
    }
}

void MesaEditor::drawCtl (juce::Graphics& g, const Ctl& c)
{
    switch (c.kind)
    {
        case Kind::knob:
        {
            const int frame = juce::jlimit (0, atlas::framesKnob - 1, juce::roundToInt (norm (c) * (atlas::framesKnob - 1)));
            drawCell (g, false, c.sprite + "_" + juce::String (frame).paddedLeft ('0', 2), c.x, c.ly, lay::SURF, c.d / 1.5f);
            break;
        }
        case Kind::toggle: case Kind::radio: case Kind::modeBtn: case Kind::optBtn:
        {
            bool on = false;
            if (c.kind == Kind::toggle) on = norm (c) > 0.5f;
            else if (c.kind == Kind::radio) on = juce::roundToInt (c.p->value (c.pid)) == c.value;
            else if (c.kind == Kind::modeBtn) on = proc.mode() == c.value;
            else
            {
                const bool hq = proc.value (id::hq) > 0.5f;
                on = c.value == 0 ? proc.registry->meterMode == 0 : c.value == 1 ? proc.registry->meterMode == 1 : c.value == 2 ? ! hq : hq;
            }
            drawCell (g, false, "b_" + c.sprite + (on ? "_1" : "_0"), c.x, c.ly);
            break;
        }
        case Kind::fader: case Kind::mfader:
        {
            const float frac = faderFrac (c.p->value (id::fader));
            const float yt = c.kind == Kind::mfader ? lay::MFAD_YT : lay::FAD_YT, yb = c.kind == Kind::mfader ? lay::MFAD_YB : lay::FAD_YB;
            drawCell (g, false, c.sprite, c.x, yt - frac * (yt - yb));
            break;
        }
        default: break;
    }
}

// ------------------------------------------------------------------------------------------------ fundo (cache)
void MesaEditor::drawStripStatic (juce::Graphics& g, const Strip& s)
{
    drawCell (g, true, "ch_bridge", s.cx, lay::Y_BR_TOP);
    g.setColour (s.p != nullptr ? cLegend : cLegend.withAlpha (0.35f));
    g.setFont (uiFont (0.42f));
    g.drawText (s.number, juce::Rectangle<float> (s.cx - 1.0f, sy (lay::NUM_Y, lay::BR_Z) - 0.3f, 2.0f, 0.6f), juce::Justification::centred, false);

    if (s.p == nullptr)
    {
        drawCell (g, true, "blank", s.cx, lay::SEC_PRE[0]);
        if (s.group)
        {
            g.setColour (cLegend.withAlpha (0.4f));
            g.setFont (uiFont (0.26f));
            g.drawFittedText (juce::String::fromUTF8 ("GRUPO ") + juce::String (s.slot + 1) + juce::String::fromUTF8 ("\nLIVRE"),
                              juce::Rectangle<float> (s.cx - lay::SW / 2, sy (-25.0f), lay::SW, 2.0f).toNearestInt(), juce::Justification::centred, 2);
        }
        return;
    }
    auto* p = s.p;
    const int pre = juce::jlimit (0, preNames.size() - 1, juce::roundToInt (p->value (id::pre)));
    const int eqm = juce::jlimit (0, eqNames.size() - 1, juce::roundToInt (p->value (id::eqm)));
    drawCell (g, true, "pre_" + preNames[pre], s.cx, lay::SEC_PRE[0]);
    drawCell (g, true, "eq_" + eqNames[eqm], s.cx, lay::SEC_EQ[0]);
    drawCell (g, true, s.group ? "dyn_COLA 4K" : "dyn_VCA", s.cx, lay::SEC_DYN[0]);
    drawCell (g, true, "out", s.cx, lay::SEC_OUT[0]);
    drawCell (g, true, "fad", s.cx, lay::SEC_FAD[0]);

    // dB do fader embaixo do medidor
    const float db = p->value (id::fader);
    g.setColour (cLegend);
    g.setFont (uiFont (0.26f));
    g.drawText (db <= faderMin + 0.01f ? juce::String::fromUTF8 ("-∞") : juce::String (db, 1),
                juce::Rectangle<float> (s.cx + lay::FAD_DB[0] - 0.8f, sy (lay::FAD_DB[1]) - 0.2f, 1.6f, 0.4f), juce::Justification::centred, false);

    // fita crepe: bolinha na cor da faixa + nome à mão
    const float tx = s.cx + lay::TAPE[0], ty = sy (lay::TAPE[1]);
    g.setColour (p->trackColour());
    g.fillEllipse (s.cx + lay::DOT[0] - lay::DOT[2] / 2, ty - lay::DOT[2] / 2 * lay::TILT_COS, lay::DOT[2], lay::DOT[2] * lay::TILT_COS);
    juce::String nm = p->displayName();
    if (nm.isEmpty()) nm = s.group ? "GRUPO " + juce::String (s.slot + 1) : "CANAL " + s.number;
    g.setColour (cInk.withAlpha (0.92f));
    g.setFont (handFont (nm.length() < 9 ? 0.46f : 0.38f));
    g.drawFittedText (nm, juce::Rectangle<float> (tx - 1.25f, ty - 0.45f, 2.95f, 0.9f).toNearestInt(), juce::Justification::centred, 1, 0.6f);

    if (p == &proc)   // esta cópia: barra âmbar acesa embaixo do número
    {
        juce::Rectangle<float> bar (s.cx - 1.3f, sy (lay::SEL_Y, lay::BR_Z) - 0.06f, 2.6f, 0.12f);
        g.setColour (cAmber.withAlpha (0.25f));
        g.fillRoundedRectangle (bar.expanded (0.1f), 0.1f);
        g.setColour (juce::Colour (0xffffb347));
        g.fillRoundedRectangle (bar, 0.05f);
    }
}

void MesaEditor::drawMasterStatic (juce::Graphics& g)
{
    const float mx = lay::X_M0 + lay::MW / 2;
    drawCell (g, true, "m_bridge", mx, lay::Y_BR_TOP);
    drawCell (g, true, "m_logo", mx, lay::MSEC_LOGO[0]);
    drawCell (g, true, "m_cola", mx, lay::MSEC_COLA[0]);
    drawCell (g, true, "m_out", mx, lay::MSEC_OUT[0]);

    const float ty = sy (lay::MTAPE[1]);
    g.setColour (cInk.withAlpha (0.92f));
    g.setFont (handFont (0.5f));
    juce::String nm = master != nullptr ? master->displayName() : juce::String();
    g.drawFittedText (nm.isEmpty() ? juce::String ("MIX BUS") : nm,
                      juce::Rectangle<float> (mx + lay::MTAPE[0] - 1.9f, ty - 0.45f, 3.8f, 0.9f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    if (master == nullptr)
    {
        // sem cópia em modo MASTER: o módulo fica apagado com a explicação
        juce::Rectangle<float> r (lay::X_M0 + 0.1f, sy (lay::MSEC_COLA[0]) + 0.1f, lay::MW - 0.2f, sy (lay::MSEC_OUT[1]) - sy (lay::MSEC_COLA[0]) - 0.2f);
        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillRect (r);
        g.setColour (cLegend);
        g.setFont (uiFont (0.55f));
        g.drawText ("SEM MASTER", r.withHeight (2.0f).withY (r.getY() + 6.0f), juce::Justification::centred, false);
        g.setFont (uiFont (0.36f, false));
        g.drawFittedText (juce::String::fromUTF8 ("Ponha uma MESA no canal Master do DAW\ne aperte MASTER na janela dela."),
                          r.withHeight (2.0f).withY (r.getY() + 8.0f).toNearestInt(), juce::Justification::centred, 3);
    }
}

void MesaEditor::drawCache()
{
    const float k = juce::jmax (1.0f, (float) juce::Component::getApproximateScaleFactorForComponent (this));
    const int w = juce::roundToInt ((float) getWidth() * k), h = juce::roundToInt ((float) getHeight() * k);
    if (w <= 0 || h <= 0) return;
    if (cache.getWidth() != w || cache.getHeight() != h) cache = juce::Image (juce::Image::RGB, w, h, false);
    cacheScale = k;
    juce::Graphics g (cache);
    g.fillAll (cBase);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.addTransform (viewXf().scaled (k));
    drawPx = zoom * k;
    drawVis = visibleCm().expanded (0.5f);

    // de trás para a frente: madeira de cima, divisórias, canais, MASTER, peças, descanso de braço, laterais
    drawCell (g, true, "top", lay::TOTAL_W / 2, lay::Y_BR_TOP);
    drawCell (g, true, "div", lay::WOOD_W + lay::EDGE / 2, lay::Y_BR_TOP, lay::SURF, 1.0f, lay::EDGE);
    drawCell (g, true, "div", lay::X_GP0 - 0.5f, lay::Y_BR_TOP);
    drawCell (g, true, "div", lay::X_M0 - 0.5f, lay::Y_BR_TOP);
    drawCell (g, true, "div", lay::X_M0 + lay::MW + lay::EDGE / 2, lay::Y_BR_TOP, lay::SURF, 1.0f, lay::EDGE);
    for (auto& s : strips) drawStripStatic (g, s);
    drawMasterStatic (g);
    for (int i : drawOrder) drawCtl (g, ctls[(size_t) i]);
    drawCell (g, true, "leather", lay::TOTAL_W / 2, lay::SEC_FAD[1]);
    // a lateral de madeira da esquerda faz sombra no 1º canal (a luz principal vem da esquerda)
    {
        const float x0 = lay::WOOD_W, top = sy (lay::Y_BR_TOP, lay::BR_Z), bot = sy (lay::SEC_FAD[1]);
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.45f), x0, 0.0f, juce::Colours::transparentBlack, x0 + 1.1f, 0.0f, false);
        g.setGradientFill (sh);
        g.fillRect (juce::Rectangle<float> (x0, top, 1.1f, bot - top));
    }
    drawCell (g, true, "wood", lay::WOOD_W / 2, lay::Y_TOP);
    drawCell (g, true, "wood", lay::TOTAL_W - lay::WOOD_W / 2, lay::Y_TOP);
    cacheDirty = false;
}

// ------------------------------------------------------------------------------------------------ o que mexe (30 vezes/s)
void MesaEditor::drawNeedle (juce::Graphics& g, float cx, float layoutY, float za, float w, float h, float vu)
{
    // a face do VU fica deitada na ponte: na tela ela é achatada pela inclinação da câmera
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::scale (1.0f, lay::TILT_COS).translated (cx, sy (layoutY, za + lay::VU_FACE_DZ)));
    const juce::Rectangle<float> face (-w / 2, -h / 2, w, h * 0.85f);
    juce::Path clip; clip.addRectangle (face);
    g.reduceClipRegion (clip);
    const float a = juce::degreesToRadians (vuAngle (vu) * lay::VU_ANG);
    const juce::Point<float> pivot (0.0f, h * 0.62f);
    const float L = h * lay::VU_R * 1.12f;
    const juce::Point<float> tip (pivot.x + std::sin (a) * L, pivot.y - std::cos (a) * L);
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.drawLine ({ pivot.translated (0.07f, 0.1f), tip.translated (0.07f, 0.1f) }, h * 0.022f);
    g.setColour (juce::Colour (0xff101010));
    g.drawLine ({ pivot, tip }, h * 0.018f);
    // reflexo do vidro
    juce::ColourGradient gl (juce::Colours::white.withAlpha (0.10f), face.getX(), face.getY(),
                             juce::Colours::white.withAlpha (0.0f), face.getX(), face.getCentreY(), false);
    g.setGradientFill (gl);
    g.fillRect (face.withHeight (face.getHeight() * 0.5f));
}

void MesaEditor::drawDynamic (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    for (auto& s : strips)
    {
        if (s.p == nullptr) continue;
        const auto& m = meters[s.p];
        drawNeedle (g, s.cx + lay::VU_CH[0], lay::VU_CH[1], lay::BR_Z, lay::VU_CH[2], lay::VU_CH[3], m.vu);
        const float pk = juce::jmax (m.peakL, m.peakR);
        if (pk > -50.0f) drawCell (g, false, "l_d_g", s.cx + lay::SIG[0], lay::SIG[1], lay::BR_Z);
        if (pk > -1.0f)  drawCell (g, false, "l_d_r", s.cx + lay::PEAK[0], lay::PEAK[1], lay::BR_Z);

        const float step = (lay::FAD_YT - lay::FAD_YB) / (lay::METER_N - 1);
        for (int c = 0; c < 2; ++c)
        {
            const float lv = c == 0 ? m.peakL : m.peakR;
            for (int i = 0; i < lay::METER_N && lv >= chThr[i]; ++i)
                drawCell (g, false, juce::String ("l_m_") + ledColour (i, lay::METER_N), s.cx + lay::METER_X[c], lay::FAD_YB + (float) i * step);
        }
        for (int i = 0; i < 5 && m.gr >= grThr[i]; ++i)
            drawCell (g, false, i < 4 ? "l_gr_a" : "l_gr_r", s.cx + lay::GR_LEDS[i][0], lay::GR_LEDS[i][1]);
    }

    const float mx = lay::X_M0 + lay::MW / 2;
    const auto& m = masterMeter;
    if (master != nullptr)
    {
        drawNeedle (g, mx + lay::MVU[0][0], lay::MVU[0][1], lay::BR_Z, lay::MVU[0][2], lay::MVU[0][3], m.vuL);
        drawNeedle (g, mx + lay::MVU[1][0], lay::MVU[1][1], lay::BR_Z, lay::MVU[1][2], lay::MVU[1][3], m.vuR);
        drawNeedle (g, mx + lay::COLA_VU[0], lay::COLA_VU[1], lay::SURF, lay::COLA_VU[2], lay::COLA_VU[3], -m.gr);
        const float step = (lay::MFAD_YT - lay::MFAD_YB) / (lay::MMETER_N - 1);
        for (int c = 0; c < 2; ++c)
        {
            const float lv = c == 0 ? m.peakL : m.peakR;
            for (int i = 0; i < lay::MMETER_N && lv >= mThr[i]; ++i)
                drawCell (g, false, juce::String ("l_mm_") + ledColour (i, lay::MMETER_N), mx + lay::MMETER_X[c], lay::MFAD_YB + (float) i * step);
        }
        const float fd = master->value (id::fader);
        const juce::String vals[3] = { fd <= faderMin + 0.01f ? juce::String::fromUTF8 ("-∞") : juce::String (fd, 1) + " dB",
                                       juce::String (juce::jmax (m.peakL, m.peakR), 1), juce::String (-m.gr, 1) };
        g.setColour (cAmber);
        g.setFont (uiFont (0.42f));
        for (int k = 0; k < 3; ++k)
            g.drawText (vals[k], juce::Rectangle<float> (mx + lay::MREAD[k][0] - 1.6f, sy (lay::MREAD[k][1]) - 0.3f, 3.2f, 0.6f), juce::Justification::centred, false);
    }
    else
    {
        drawNeedle (g, mx + lay::MVU[0][0], lay::MVU[0][1], lay::BR_Z, lay::MVU[0][2], lay::MVU[0][3], -25.0f);
        drawNeedle (g, mx + lay::MVU[1][0], lay::MVU[1][1], lay::BR_Z, lay::MVU[1][2], lay::MVU[1][3], -25.0f);
    }
}

void MesaEditor::drawOverlay (juce::Graphics& g)
{
    // vinheta leve (a lente da foto) e os botões da vista
    const auto b = getLocalBounds().toFloat();
    juce::ColourGradient v (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                            juce::Colours::black.withAlpha (0.28f), 0.0f, 0.0f, true);
    v.addColour (0.55, juce::Colours::transparentBlack);
    g.setGradientFill (v);
    g.fillRect (b);

    auto btn = [&] (Btn which, const juce::String& txt, bool lit)
    {
        const auto r = btnRect (which);
        g.setColour (juce::Colour (btnHover == which ? 0xee202024 : 0xcc101012));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (cAmber.withAlpha (lit ? 0.9f : 0.45f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (lit ? cAmber : cLegend.withAlpha (0.85f));
        g.setFont (uiFont (txt.length() > 1 ? 12.0f : 17.0f));
        g.drawText (txt, r, juce::Justification::centred, false);
    };
    btn (Btn::zoomOut, juce::String::fromUTF8 ("−"), false);
    btn (Btn::zoomIn, "+", false);
    btn (Btn::overview, overview ? "PERTO" : "GERAL", overview);
}

void MesaEditor::drawBubble (juce::Graphics& g)
{
    const int i = active >= 0 ? active : hover;
    if (i < 0 || i >= (int) ctls.size()) return;
    const auto txt = describe (ctls[(size_t) i]);
    if (txt.isEmpty()) return;
    const auto& c = ctls[(size_t) i];
    auto f = uiFont (juce::jlimit (12.0f, 18.0f, 0.5f * zoom));
    const float w = juce::GlyphArrangement::getStringWidth (f, txt) + 16.0f, h = f.getHeight() + 8.0f;
    const auto pos = juce::Point<float> (c.x, c.y).transformedBy (viewXf());
    float x = pos.x - w / 2, y = pos.y - c.d * 0.6f * zoom - h - 6.0f;
    if (c.kind == Kind::fader || c.kind == Kind::mfader) y = pos.y - h - 4.0f;
    x = juce::jlimit (2.0f, (float) getWidth() - w - 2.0f, x);
    y = juce::jmax (2.0f, y);
    juce::Rectangle<float> r (x, y, w, h);
    g.setColour (juce::Colour (0xee101012));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (cAmber.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (cAmber);
    g.setFont (f);
    g.drawText (txt, r, juce::Justification::centred, false);
}

void MesaEditor::paint (juce::Graphics& g)
{
    const float k = juce::jmax (1.0f, (float) juce::Component::getApproximateScaleFactorForComponent (this));
    if (cacheDirty || ! cache.isValid() || std::abs (k - cacheScale) > 0.01f
        || cache.getWidth() != juce::roundToInt ((float) getWidth() * k))
        drawCache();
    g.drawImage (cache, getLocalBounds().toFloat());
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (viewXf());
        drawPx = zoom * k;
        drawVis = visibleCm().expanded (0.5f);
        drawDynamic (g);
    }
    drawOverlay (g);
    drawBubble (g);
}

void MesaEditor::resized()
{
    hBar.setBounds (getLocalBounds().removeFromBottom (12).withTrimmedRight (12));
    vBar.setBounds (getLocalBounds().removeFromRight (12).withTrimmedBottom (12));
    if (overview) setView (fitZoom(), 0.0f, 0.0f);
    else          setView (juce::jmax (zoom, fitZoom()), viewX, viewY);
    if (openSettle >= 15) { proc.uiW = getWidth(); proc.uiH = getHeight(); }
}

// ------------------------------------------------------------------------------------------------ medidores e mudanças
void MesaEditor::timerCallback()
{
    if (proc.registry->version.load() != lastVersion) rebuild();
    if (openSettle < 15 && ++openSettle == 15)
    {
        // o DAW pode ter aberto a janela com outro tamanho: volta ao salvo
        const int w0 = proc.uiW.load(), h0 = proc.uiH.load();
        if (w0 > 0 && (w0 != getWidth() || h0 != getHeight())) setSize (w0, h0);
        proc.uiW = getWidth(); proc.uiH = getHeight();
    }

    const float dt = 1.0f / 30.0f;
    const bool ppm = proc.registry->meterMode == 1;
    auto update = [&] (Meter& m, MesaProcessor* p)
    {
        const float pl = p->peakL.load(), pr = p->peakR.load();
        const float tl = (ppm ? pl : p->rmsL.load()) + 18.0f, tr = (ppm ? pr : p->rmsR.load()) + 18.0f;
        auto ball = [&] (float& v, float t)
        {
            if (ppm) v = t > v ? t : v - 12.0f * dt;
            else     v += (t - v) * (1.0f - std::exp (-dt / 0.065f));
            v = juce::jlimit (-30.0f, 6.0f, v);
        };
        ball (m.vuL, tl); ball (m.vuR, tr);
        m.vu = juce::jmax (m.vuL, m.vuR);
        m.peakL = juce::jmax (pl, m.peakL - 26.0f * dt);
        m.peakR = juce::jmax (pr, m.peakR - 26.0f * dt);
        const float gr = p->grDb.load();
        m.gr = gr > m.gr ? gr : juce::jmax (gr, m.gr - 20.0f * dt);
    };
    for (auto& s : strips)
        if (s.p != nullptr) update (meters[s.p], s.p);
    if (master != nullptr) update (masterMeter, master);

    auto snap = snapshot();
    if (snap != lastSnap) { lastSnap = std::move (snap); cacheDirty = true; }
    repaint();
}

std::vector<float> MesaEditor::snapshot() const
{
    std::vector<float> v;
    v.reserve (ctls.size() + 4);
    for (auto& c : ctls)
        v.push_back (c.p != nullptr && c.pid.isNotEmpty() ? c.p->value (c.pid) : 0.0f);
    v.push_back ((float) proc.registry->meterMode.load());
    v.push_back (proc.value (id::hq));
    return v;
}

// ------------------------------------------------------------------------------------------------ mouse
const MesaEditor::Ctl* MesaEditor::hit (juce::Point<float> pos) const
{
    const auto cm = toCm (pos);
    for (int i = (int) ctls.size() - 1; i >= 0; --i)
    {
        const auto& c = ctls[(size_t) i];
        const float dx = cm.x - c.x, dy = cm.y - c.y;
        switch (c.kind)
        {
            case Kind::knob:   // o topo do knob sobe um pouco na tela (câmera inclinada)
            {
                const float dk = dy + c.d * 0.12f;
                if (dx * dx + dk * dk < c.d * c.d * 0.36f) return &c;
                break;
            }
            case Kind::toggle: case Kind::radio: case Kind::modeBtn: case Kind::optBtn:
                if (std::abs (dx) < c.d * 0.6f && std::abs (dy + 0.1f) < c.d * 0.6f) return &c; break;
            case Kind::fader: case Kind::mfader:
                if (std::abs (dx) < 0.75f && dy > -1.6f && dy < c.d + 1.4f) return &c; break;
            case Kind::preModel: case Kind::eqModel:
                if (std::abs (dx) < 1.45f && std::abs (dy) < 0.35f) return &c; break;
            case Kind::tape:
                if (c.p != nullptr && std::abs (dx) < 1.8f && std::abs (dy) < 0.55f) return &c; break;
        }
    }
    return nullptr;
}

juce::String MesaEditor::describe (const Ctl& c) const
{
    auto onOff = [] (bool b) { return b ? juce::String ("LIGADO") : juce::String ("DESLIGADO"); };
    switch (c.kind)
    {
        case Kind::preModel: return juce::String::fromUTF8 ("PRÉ: ") + preNames[juce::roundToInt (c.p->value (id::pre))] + juce::String::fromUTF8 ("  (clique para trocar)");
        case Kind::eqModel:  return "EQ: " + eqNames[juce::roundToInt (c.p->value (id::eqm))] + juce::String::fromUTF8 ("  (clique para trocar)");
        case Kind::tape:     return juce::String::fromUTF8 ("Duplo clique: renomear o canal");
        case Kind::modeBtn:  return juce::String::fromUTF8 ("Esta MESA como ") + modeNames[c.value];
        case Kind::optBtn:
        {
            const char* t[4] = { "Agulhas em VU (média; 0 VU = -18 dBFS)", "Agulhas em PPM (pico)",
                                 "ECO: sem sobreamostragem (menos CPU, todas as MESAs)", "HQ: saturação a 4x (todas as MESAs)" };
            return juce::String::fromUTF8 (t[c.value]);
        }
        default: break;
    }
    auto* prm = c.p != nullptr ? c.p->param (c.pid) : nullptr;
    if (prm == nullptr) return {};
    if (c.pid == id::dynPre) return juce::String::fromUTF8 ("DINÂMICA ANTES DO EQ: ") + onOff (norm (c) > 0.5f);
    if (c.pid == id::eqOn)   return "EQ: " + onOff (norm (c) > 0.5f);
    if (c.pid == id::pad)    return "PAD -20 dB: " + onOff (norm (c) > 0.5f);
    if (c.pid == id::mute)   return "MUTE: " + onOff (norm (c) > 0.5f);
    if (c.pid == id::solo)   return "SOLO: " + onOff (norm (c) > 0.5f);
    if (c.pid == id::min)    return "COLA: " + onOff (norm (c) > 0.5f);
    if (c.pid == id::mhpf)   return "SIDECHAIN HPF 90 Hz: " + onOff (norm (c) > 0.5f);
    if (c.kind == Kind::radio)
        return c.p->mode() == modeGroup ? juce::String::fromUTF8 ("Este grupo é o G") + juce::String (c.value)
                                        : juce::String::fromUTF8 ("Etiqueta do grupo G") + juce::String (c.value);
    return prm->getName (40).toUpperCase() + "  " + prm->getCurrentValueAsText();
}

void MesaEditor::begin (const Ctl& c)
{
    end();
    if (c.p == nullptr || c.pid.isEmpty()) return;
    gestureParam = c.p->param (c.pid);
    if (gestureParam != nullptr) gestureParam->beginChangeGesture();
}

void MesaEditor::end()
{
    if (gestureParam != nullptr) gestureParam->endChangeGesture();
    gestureParam = nullptr;
}

void MesaEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto b = btnAt (e.position);
    if (b != btnHover) { btnHover = b; repaint(); }
    const auto* c = b == Btn::none ? hit (e.position) : nullptr;
    const int h = c != nullptr ? (int) (c - ctls.data()) : -1;
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (b != Btn::none || (c != nullptr && c->kind != Kind::tape) ? juce::MouseCursor::PointingHandCursor
                                                                             : juce::MouseCursor::NormalCursor);
}

void MesaEditor::mouseExit (const juce::MouseEvent&) { hover = -1; btnHover = Btn::none; repaint(); }

void MesaEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMainMenu(); return; }
    if (const auto b = btnAt (e.position); b != Btn::none)
    {
        const juce::Point<float> mid ((float) getWidth() * 0.5f, (float) getHeight() * 0.5f);
        if (b == Btn::zoomIn)  zoomAround (zoom * 1.25f, mid);
        if (b == Btn::zoomOut) zoomAround (zoom / 1.25f, mid);
        if (b == Btn::overview)
        {
            if (overview) zoomAround (closeZoom(), mid);
            else          setView (fitZoom(), 0.0f, 0.0f);
        }
        proc.uiGeral = overview;
        return;
    }
    const auto* c = hit (e.position);
    if (c == nullptr)
    {
        // arrastar a chapa (fora dos controles) passeia pela console
        panning = true;
        panStart = { viewX, viewY };
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }
    active = (int) (c - ctls.data());
    if (c->kind == Kind::knob || c->kind == Kind::fader || c->kind == Kind::mfader)
    {
        begin (*c);
        if (c->kind == Kind::knob) dragStart = norm (*c);
        else dragStart = faderFrac (c->p->value (id::fader));
        dragValue = dragStart;
        return;
    }
    click (*c, e);
}

void MesaEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (panning)
    {
        setView (zoom, panStart.x - (float) e.getDistanceFromDragStartX() / zoom, panStart.y - (float) e.getDistanceFromDragStartY() / zoom);
        return;
    }
    if (active < 0 || active >= (int) ctls.size()) return;
    const auto& c = ctls[(size_t) active];
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float dy = (float) e.getDistanceFromDragStartY();
    if (c.kind == Kind::knob)
    {
        dragValue = juce::jlimit (0.0f, 1.0f, dragStart - dy * fine / 220.0f);
        setNorm (c, dragValue);
    }
    else if (c.kind == Kind::fader || c.kind == Kind::mfader)
    {
        dragValue = juce::jlimit (0.0f, 1.0f, dragStart + dy * fine / (c.d * zoom));
        if (auto* prm = c.p->param (id::fader)) setNorm (c, prm->convertTo0to1 (faderDb (dragValue)));
    }
    else return;
    cacheDirty = true;
    repaint();
}

void MesaEditor::mouseUp (const juce::MouseEvent& e)
{
    if (panning) { panning = false; mouseMove (e); return; }
    end();
    active = -1;
    repaint();
}

void MesaEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (btnAt (e.position) != Btn::none) return;
    const auto* c = hit (e.position);
    if (c == nullptr) return;
    if (c->kind == Kind::tape) { rename (c->p); return; }
    if (c->kind == Kind::knob || c->kind == Kind::fader || c->kind == Kind::mfader)
        if (auto* prm = c->p->param (c->pid))
        {
            end();
            setNorm (*c, prm->getDefaultValue());
            cacheDirty = true;
        }
}

void MesaEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // Ctrl/Cmd + roda: zoom onde está o mouse; roda: rola a console (Shift: para o lado)
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomAround (zoom * std::pow (1.0015f, (w.deltaY != 0 ? w.deltaY : w.deltaX) * 256.0f), e.position);
        return;
    }
    const float step = 520.0f / zoom;   // cm por "dente" da roda
    float dx = -w.deltaX * step, dy = -w.deltaY * step;
    if (e.mods.isShiftDown() && dx == 0.0f) std::swap (dx, dy);
    if ((float) getHeight() / zoom >= consoleH() - 0.01f && dx == 0.0f) std::swap (dx, dy);   // nada a rolar na vertical
    setView (zoom, viewX + dx, viewY + dy);
}

void MesaEditor::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoomAround (zoom * scaleFactor, e.position);
}

void MesaEditor::click (const Ctl& c, const juce::MouseEvent&)
{
    juce::Component::SafePointer<MesaEditor> safe (this);
    switch (c.kind)
    {
        case Kind::toggle:
            setNorm (c, norm (c) > 0.5f ? 0.0f : 1.0f);
            break;
        case Kind::radio:
        {
            const int cur = juce::roundToInt (c.p->value (id::grp));
            const int v = c.p->mode() == modeGroup ? c.value : (cur == c.value ? 0 : c.value);
            setNorm (c, (float) v / 3.0f);
            break;
        }
        case Kind::modeBtn:
        {
            if (c.value == modeGroup && proc.group() == 0)
            {
                // grupo novo: o primeiro G livre
                bool used[4] = {};
                {
                    const juce::ScopedLock sl (proc.registry->lock);
                    for (auto* q : proc.registry->list)
                        if (q != &proc && q->mode() == modeGroup) used[juce::jlimit (1, 3, q->group())] = true;
                }
                int g = 1;
                while (g < 3 && used[g]) ++g;
                if (auto* prm = proc.param (id::grp)) { prm->beginChangeGesture(); prm->setValueNotifyingHost ((float) g / 3.0f); prm->endChangeGesture(); }
            }
            setNorm (c, (float) c.value / 2.0f);
            break;
        }
        case Kind::optBtn:
            if (c.value < 2) proc.registry->meterMode = c.value;
            else
            {
                const float v = c.value == 3 ? 1.0f : 0.0f;
                const juce::ScopedLock sl (proc.registry->lock);
                for (auto* q : proc.registry->list)
                    if (auto* prm = q->param (id::hq)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (v); prm->endChangeGesture(); }
            }
            break;
        case Kind::preModel: case Kind::eqModel:
        {
            const auto& names = c.kind == Kind::preModel ? preNames : eqNames;
            const int cur = juce::roundToInt (c.p->value (c.pid));
            juce::PopupMenu m;
            m.addSectionHeader (c.kind == Kind::preModel ? juce::String::fromUTF8 ("PRÉ") : juce::String ("EQ"));
            for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
            const int seq = c.p->seq();
            const juce::String pid = c.pid;
            const int n = names.size();
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withParentComponent (this)
                                 .withTargetScreenArea (juce::Rectangle<int> (1, 1).withPosition (juce::Desktop::getMousePosition())),
                             [safe, seq, pid, n] (int r)
                             {
                                 if (safe == nullptr || r <= 0) return;
                                 const juce::ScopedLock sl (safe->proc.registry->lock);
                                 for (auto* q : safe->proc.registry->list)
                                     if (q->seq() == seq)
                                         if (auto* prm = q->param (pid))
                                         { prm->beginChangeGesture(); prm->setValueNotifyingHost ((float) (r - 1) / (float) (n - 1)); prm->endChangeGesture(); }
                                 safe->buildCtls();
                                 safe->cacheDirty = true;
                             });
            break;
        }
        default: break;
    }
    // trocar o modelo muda a família dos knobs; o resto só redesenha
    if (c.pid == id::pre || c.pid == id::eqm || c.pid == id::mode || c.pid == id::grp) buildCtls();
    cacheDirty = true;
    repaint();
}

void MesaEditor::rename (MesaProcessor* p)
{
    if (p == nullptr) return;
    const int seq = p->seq();
    auto* w = new juce::AlertWindow ("Nome do canal", juce::String::fromUTF8 ("Escreva o nome que vai na fita (vazio = o nome da faixa do DAW)."),
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("nome", p->userName());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MesaEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, seq] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto nm = w->getTextEditorContents ("nome");
        const juce::ScopedLock sl (safe->proc.registry->lock);
        for (auto* q : safe->proc.registry->list)
            if (q->seq() == seq) { q->setUserName (nm); break; }
    }), true);
}

void MesaEditor::showMainMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("MESA");
    m.addItem (menuRenomear, juce::String::fromUTF8 ("Renomear este canal…"));
    m.addItem (menuSobre, "Como funciona");
    m.addSeparator();
    m.addItem (menuLicenca, juce::String::fromUTF8 ("Licença…"));
    juce::Component::SafePointer<MesaEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withParentComponent (this)
                         .withTargetScreenArea (juce::Rectangle<int> (1, 1).withPosition (juce::Desktop::getMousePosition())),
                     [safe] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == menuLicenca) safe->ativacao.mostrarInfo();
                         if (r == menuRenomear) safe->rename (&safe->proc);
                         if (r == menuSobre)
                             juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MESA", juce::String::fromUTF8 (
                                 "Ponha uma MESA no fim de cada canal do DAW: cada uma vira uma fileira da console, e a janela de "
                                 "qualquer uma mostra todas.\n\n"
                                 "GRUPO: ponha uma MESA no canal de grupo (bus) e aperte GRUPO. Os botões G1/G2/G3 do canal são a "
                                 "etiqueta do grupo; o envio do áudio é feito no roteamento do DAW.\n"
                                 "MASTER: ponha uma MESA no Master e aperte MASTER.\n\n"
                                 "O fader e o pan da MESA mexem no som que sai do plugin: deixe o fader do DAW em 0 dB.\n"
                                 "Botão PRÉ (no EQ): a dinâmica vem antes do EQ.\n"
                                 "Clique no nome do PRÉ ou do EQ para trocar o modelo. Duplo clique: knob volta ao padrão; "
                                 "na fita, renomeia. Shift: ajuste fino.\n\n"
                                 "VISTA: a roda do mouse rola a console (Shift: para o lado); Ctrl/Cmd + roda ou os botões − e + "
                                 "aproximam; GERAL mostra a console toda e PERTO volta para perto. Arrastar a chapa (fora dos "
                                 "controles) também passeia."), "OK", safe.getComponent());
                     });
}

juce::AudioProcessorEditor* MesaProcessor::createEditor()
{
    return new MesaEditor (*this);
}
