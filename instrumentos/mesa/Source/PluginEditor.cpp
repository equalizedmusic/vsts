#include "PluginEditor.h"
#include "Atlas.h"
#include "PluginImg.h"

using namespace mesa;

namespace
{
constexpr float basePpcm = 20.0f;   // 100% = 20 px por cm (as peças estão a 40 px/cm: nítidas até 200%)
constexpr int menuLicenca = 20007, menuRenomear = 20008, menuSobre = 20009;

const juce::Colour cBase (0xff0b0b0c), cBridge (0xff18191c), cLegend (0xffece8dc), cAmber (0xfff2c14e),
                   cInk (0xff16182a), cLensText (0xff0c0c0d), cLensOff (0xff3a3a3a);

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

juce::String ledColour (int i, int n) { const float f = (float) i / (float) (n - 1); return f < 0.68f ? "g" : (f < 0.86f ? "y" : "r"); }
} // namespace

// =====================================================================================================================
MesaEditor::MesaEditor (MesaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    chapasImg = juce::ImageCache::getFromMemory (PluginImg::chapas_jpg, PluginImg::chapas_jpgSize);
    pecasImg  = juce::ImageCache::getFromMemory (PluginImg::pecas_png, PluginImg::pecas_pngSize);
    for (auto& a : atlas::chapas) chapaRect[a.nome] = { a.x, a.y, a.w, a.h };
    for (auto& a : atlas::pecas)  pecaRect[a.nome]  = { a.x, a.y, a.w, a.h };

    addChildComponent (scrollBar);
    scrollBar.addListener (this);
    scrollBar.setAutoHide (false);
    scrollBar.setColour (juce::ScrollBar::thumbColourId, juce::Colour (0x66ece8dc));

    proc.registry->listeners.add (this);
    rebuild();

    const float W = lay::TOTAL_W * basePpcm, H = lay::TOTAL_H * basePpcm;
    const float saved = proc.uiScale.load();
    const float openScale = saved > 0.0f ? juce::jlimit (0.45f, 1.6f, saved) : 0.7f;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) W / (double) H);
    setResizeLimits ((int) (W * 0.45f), (int) (H * 0.45f), (int) (W * 1.6f), (int) (H * 1.6f));
    setSize ((int) (W * openScale), (int) (H * openScale));
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

void MesaEditor::scrollBarMoved (juce::ScrollBar*, double start)
{
    const int s = juce::roundToInt (start);
    if (s != scroll) { scroll = s; openSettle = juce::jmax (openSettle, 1); rebuild(); }
}

void MesaEditor::buildCtls()
{
    ctls.clear();
    for (auto& s : strips)
        if (s.p != nullptr) addStripCtls (s);
    addMasterCtls();
}

void MesaEditor::addStripCtls (const Strip& s)
{
    auto* p = s.p;
    auto add = [&] (Kind k, const juce::String& pid, float x, float yLayout, float d, const juce::String& spr = {}, int v = 0, const juce::String& txt = {})
    {
        Ctl c; c.kind = k; c.p = p; c.pid = pid; c.x = s.cx + x; c.y = sy (yLayout); c.d = d; c.sprite = spr; c.value = v; c.text = txt;
        ctls.push_back (c);
    };
    const int pre = juce::roundToInt (p->value (id::pre));
    const juce::String fam = pre == 1 ? "api" : (pre == 2 ? "mar" : "ssl");
    auto preK = [&] (const char* ssl, const char* mar) { return fam == "api" ? juce::String ("k_api") : (fam == "mar" ? juce::String (mar) : juce::String (ssl)); };
    add (Kind::preModel, id::pre, 0.0f, lay::PRE_NAME_Y, 2.9f);
    add (Kind::knob, id::gain,  lay::PRE_GAIN[0],  lay::PRE_GAIN[1],  lay::PRE_GAIN[2],  preK ("k_ssl_gry", "k_mar_gain"));
    add (Kind::knob, id::drive, lay::PRE_DRIVE[0], lay::PRE_DRIVE[1], lay::PRE_DRIVE[2], preK ("k_ssl_org", "k_mar_org"));
    add (Kind::knob, id::hpf,   lay::PRE_HPF[0],   lay::PRE_HPF[1],   lay::PRE_HPF[2],   preK ("k_ssl_blk", "k_mar"));
    add (Kind::toggle, id::pad, lay::PRE_PAD[0], lay::PRE_PAD[1], lay::PRE_PAD[2], "amb");

    const bool brit = juce::roundToInt (p->value (id::eqm)) == 1;
    const char* caps[4] = { "red", "grn", "blu", "brn" };
    add (Kind::eqModel, id::eqm, 0.0f, lay::EQ_NAME_Y, 2.9f);
    for (int b = 0; b < 4; ++b)
    {
        const juce::String spr = juce::String (brit ? "k_mar_" : "k_ssl_") + caps[b];
        add (Kind::knob, id::eqG[b], lay::EQ_GAIN_X, lay::EQ_ROW_Y[b], lay::EQ_GAIN_D, spr);
        add (Kind::knob, id::eqF[b], lay::EQ_FREQ_X, lay::EQ_ROW_Y[b], lay::EQ_FREQ_D, spr);
    }
    add (Kind::toggle, id::eqOn, lay::EQ_BTN[0], lay::EQ_BTN[1], lay::EQ_BTN[2], "grn", 0, "EQ");
    add (Kind::toggle, id::dynPre, lay::DYNPRE_BTN[0], lay::DYNPRE_BTN[1], lay::DYNPRE_BTN[2], "amb", 0, juce::String::fromUTF8 ("PRÉ"));

    add (Kind::knob, id::thr, lay::DYN_X[0], lay::DYN_KNOBS_Y, lay::DYN_D, "k_ssl_gry");
    add (Kind::knob, id::rat, lay::DYN_X[1], lay::DYN_KNOBS_Y, lay::DYN_D, "k_ssl_gry");
    add (Kind::knob, s.group ? id::cmix : id::gate, lay::DYN_X[2], lay::DYN_KNOBS_Y, lay::DYN_D, "k_ssl_grn");

    for (int k = 0; k < 3; ++k)
        add (Kind::radio, id::grp, lay::G_BTNS[k][0], lay::G_BTNS[k][1], lay::G_BTNS[k][2], "grn", k + 1, "G" + juce::String (k + 1));
    add (Kind::knob, id::pan, lay::PAN[0], lay::PAN[1], lay::PAN[2], "k_ssl_wht");
    add (Kind::toggle, id::mute, lay::MUTE[0], lay::MUTE[1], lay::MUTE[2], "red", 0, "M");
    if (! s.group) add (Kind::toggle, id::solo, lay::SOLO[0], lay::SOLO[1], lay::SOLO[2], "yel", 0, "S");
    add (Kind::fader, id::fader, lay::FAD_X, lay::FAD_YT, lay::FAD_YT - lay::FAD_YB, "fcap");
    add (Kind::tape, {}, lay::TAPE[0], lay::TAPE[1], lay::TAPE[2]);
}

void MesaEditor::addMasterCtls()
{
    const float mx = lay::X_M0 + lay::MW * 0.5f;
    auto add = [&] (Kind k, MesaProcessor* p, const juce::String& pid, float x, float yLayout, float d, const juce::String& spr = {}, int v = 0, const juce::String& txt = {})
    {
        Ctl c; c.kind = k; c.p = p; c.pid = pid; c.x = mx + x; c.y = sy (yLayout); c.d = d; c.sprite = spr; c.value = v; c.text = txt;
        ctls.push_back (c);
    };
    for (int k = 0; k < 3; ++k) add (Kind::modeBtn, &proc, id::mode, lay::MODE_BTNS[k][0], lay::MODE_BTNS[k][1], lay::MODE_BTNS[k][2], "amb", k);
    for (int k = 0; k < 4; ++k) add (Kind::optBtn, &proc, {}, lay::OPT_BTNS[k][0], lay::OPT_BTNS[k][1], lay::OPT_BTNS[k][2], k < 2 ? "blu" : "grn", k);
    if (master == nullptr) return;
    const juce::String pids[6] = { id::mthr, id::matk, id::mrel, id::mmkp, id::mrat, id::mmix };
    const char* caps[6] = { "gry", "blk", "blk", "org", "blk", "gry" };
    for (int k = 0; k < 6; ++k)
        add (Kind::knob, master, pids[k], lay::COLA_KNOBS[k][0], lay::COLA_KNOBS[k][1], lay::COLA_D, juce::String ("k_ssl_") + caps[k]);
    add (Kind::toggle, master, id::min, lay::COLA_IN[0], lay::COLA_IN[1], lay::COLA_IN[2], "amb", 0, "IN");
    add (Kind::toggle, master, id::mhpf, lay::COLA_HPF[0], lay::COLA_HPF[1], lay::COLA_HPF[2], "blu", 0, "HPF");
    add (Kind::mfader, master, id::fader, lay::MFAD_X, lay::MFAD_YT, lay::MFAD_YT - lay::MFAD_YB, "mfcap");
    add (Kind::tape, master, {}, lay::MTAPE[0], lay::MTAPE[1], lay::MTAPE[2]);
}

// ------------------------------------------------------------------------------------------------ peças
juce::Image& MesaEditor::sprite (const juce::String& name)
{
    auto it = clipped.find (name);
    if (it != clipped.end()) return it->second;
    const bool chapa = name.startsWith ("#");
    const auto key = (chapa ? name.substring (1) : name).toStdString();
    auto& rects = chapa ? chapaRect : pecaRect;
    auto r = rects.find (key);
    juce::Image img;
    if (r != rects.end()) img = (chapa ? chapasImg : pecasImg).getClippedImage (r->second);
    return clipped[name] = img;
}

void MesaEditor::drawPiece (juce::Graphics& g, bool chapa, const juce::String& name, juce::Rectangle<float> cm)
{
    auto& img = sprite (chapa ? "#" + name : name);
    g.setOpacity (1.0f);   // o drawImage herda a transparência da última cor (o número apagado da vaga vazia apagava a peça seguinte)
    if (img.isValid()) g.drawImage (img, cm, juce::RectanglePlacement::stretchToFit);
}

void MesaEditor::drawLed (juce::Graphics& g, const juce::String& col, bool on, float x, float y, float w, float h)
{
    // o sprite do LED é 0,3 × 0,3 cm numa célula de 0,6 cm
    drawPiece (g, false, "l_" + col + (on ? "_1" : "_0"), { x - w, y - h, w * 2.0f, h * 2.0f });
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
            const float s = 2.4f * c.d / 1.5f;
            drawPiece (g, false, c.sprite + "_" + juce::String (frame).paddedLeft ('0', 2), { c.x - s / 2, c.y - s / 2, s, s });
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
            const float s = 1.2f * c.d / 0.66f;
            drawPiece (g, false, "b_" + c.sprite + (on ? "_1" : "_0"), { c.x - s / 2, c.y - s / 2, s, s });
            if (c.text.isNotEmpty())
            {
                g.setColour (on ? cLensText : cLensOff);
                g.setFont (uiFont (c.d * (c.text.length() > 2 ? 0.3f : 0.38f)));
                g.drawText (c.text, juce::Rectangle<float> (c.x - c.d / 2, c.y - c.d / 2 - 0.05f, c.d, c.d), juce::Justification::centred, false);
            }
            break;
        }
        case Kind::fader: case Kind::mfader:
        {
            const float db = c.p->value (id::fader);
            const float capY = c.y + faderFrac (db) * c.d;
            const bool m = c.kind == Kind::mfader;
            const float w = m ? 2.4f : 2.0f, h = m ? 3.2f : 2.8f;
            drawPiece (g, false, c.sprite, { c.x - w / 2, capY - h / 2, w, h });
            break;
        }
        default: break;
    }
}

// ------------------------------------------------------------------------------------------------ fundo (cache)
void MesaEditor::drawStripStatic (juce::Graphics& g, const Strip& s)
{
    const float left = s.cx - lay::SW / 2;
    drawPiece (g, true, "ch_bridge", { left, sy (lay::Y_BR_TOP), lay::SW, lay::Y_BR_TOP - lay::Y_BR_BOT });
    g.setColour (s.p != nullptr ? cLegend : cLegend.withAlpha (0.35f));
    g.setFont (uiFont (0.42f));
    g.drawText (s.number, juce::Rectangle<float> (s.cx - 1.0f, sy (lay::NUM_Y) - 0.3f, 2.0f, 0.6f), juce::Justification::centred, false);

    if (s.p == nullptr)
    {
        drawPiece (g, true, "blank", { left, sy (lay::SEC_PRE[0]), lay::SW, lay::SEC_PRE[0] - lay::SEC_FAD[1] });
        if (s.group)
        {
            g.setColour (cLegend.withAlpha (0.4f));
            g.setFont (uiFont (0.26f));
            g.drawFittedText (juce::String::fromUTF8 ("GRUPO ") + juce::String (s.slot + 1) + juce::String::fromUTF8 ("\nLIVRE"),
                              juce::Rectangle<float> (left, sy (-26.0f), lay::SW, 2.0f).toNearestInt(), juce::Justification::centred, 2);
        }
        return;
    }
    auto* p = s.p;
    const int pre = juce::jlimit (0, preNames.size() - 1, juce::roundToInt (p->value (id::pre)));
    const int eqm = juce::jlimit (0, eqNames.size() - 1, juce::roundToInt (p->value (id::eqm)));
    auto sec = [&] (const juce::String& name, const float* y) { drawPiece (g, true, name, { left, sy (y[0]), lay::SW, y[0] - y[1] }); };
    sec ("pre_" + preNames[pre], lay::SEC_PRE);
    sec ("eq_" + eqNames[eqm], lay::SEC_EQ);
    sec (s.group ? "dyn_COLA 4K" : "dyn_VCA", lay::SEC_DYN);
    sec ("out", lay::SEC_OUT);
    sec ("fad", lay::SEC_FAD);

    // dB do fader embaixo do medidor
    const float db = p->value (id::fader);
    g.setColour (cLegend);
    g.setFont (uiFont (0.26f));
    g.drawText (db <= faderMin + 0.01f ? juce::String::fromUTF8 ("-∞") : juce::String (db, 1),
                juce::Rectangle<float> (s.cx + lay::FAD_DB[0] - 0.8f, sy (lay::FAD_DB[1]) - 0.2f, 1.6f, 0.4f), juce::Justification::centred, false);

    // fita crepe: bolinha na cor da faixa + nome à mão
    const float tx = s.cx + lay::TAPE[0], ty = sy (lay::TAPE[1]);
    g.setColour (p->trackColour());
    g.fillRoundedRectangle (s.cx + lay::DOT[0] - lay::DOT[2] / 2, ty - lay::DOT[2] / 2, lay::DOT[2], lay::DOT[2], 0.05f);
    juce::String nm = p->displayName();
    if (nm.isEmpty()) nm = s.group ? "GRUPO " + juce::String (s.slot + 1) : "CANAL " + s.number;
    g.setColour (cInk);
    g.setFont (handFont (nm.length() < 9 ? 0.46f : 0.38f));
    g.drawFittedText (nm, juce::Rectangle<float> (tx - 1.25f, ty - 0.45f, 2.95f, 0.9f).toNearestInt(), juce::Justification::centred, 1, 0.6f);

    if (p == &proc)   // esta cópia: barra âmbar acesa embaixo do número
    {
        juce::Rectangle<float> bar (s.cx - 1.3f, sy (lay::SEL_Y) - 0.06f, 2.6f, 0.12f);
        g.setColour (cAmber.withAlpha (0.25f));
        g.fillRoundedRectangle (bar.expanded (0.1f), 0.1f);
        g.setColour (juce::Colour (0xffffb347));
        g.fillRoundedRectangle (bar, 0.05f);
    }
}

void MesaEditor::drawMasterStatic (juce::Graphics& g)
{
    const float left = lay::X_M0;
    drawPiece (g, true, "m_bridge", { left, sy (lay::Y_BR_TOP), lay::MW, lay::Y_BR_TOP - lay::Y_BR_BOT });
    drawPiece (g, true, "m_logo", { left, sy (lay::MSEC_LOGO[0]), lay::MW, lay::MSEC_LOGO[0] - lay::MSEC_LOGO[1] });
    drawPiece (g, true, "m_cola", { left, sy (lay::MSEC_COLA[0]), lay::MW, lay::MSEC_COLA[0] - lay::MSEC_COLA[1] });
    drawPiece (g, true, "m_out", { left, sy (lay::MSEC_OUT[0]), lay::MW, lay::MSEC_OUT[0] - lay::MSEC_OUT[1] });

    const float mx = left + lay::MW / 2;
    const float ty = sy (lay::MTAPE[1]);
    g.setColour (cInk);
    g.setFont (handFont (0.5f));
    juce::String nm = master != nullptr ? master->displayName() : juce::String();
    g.drawFittedText (nm.isEmpty() ? juce::String ("MIX BUS") : nm,
                      juce::Rectangle<float> (mx + lay::MTAPE[0] - 1.9f, ty - 0.45f, 3.8f, 0.9f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    if (master == nullptr)
    {
        // sem cópia em modo MASTER: o módulo fica apagado com a explicação
        juce::Rectangle<float> r (left + 0.1f, sy (lay::MSEC_COLA[0]) + 0.1f, lay::MW - 0.2f, lay::MSEC_COLA[0] - lay::MSEC_OUT[1] - 0.2f);
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
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.addTransform (juce::AffineTransform::scale (ppcm() * k));

    g.fillAll (cBase);
    g.setColour (cBridge);
    g.fillRect (juce::Rectangle<float> (lay::WOOD_W, sy (lay::Y_BR_TOP), lay::TOTAL_W - 2 * lay::WOOD_W, lay::Y_BR_TOP - lay::Y_BR_BOT));
    drawPiece (g, true, "wood", { 0.0f, 0.0f, lay::WOOD_W, lay::TOTAL_H });
    drawPiece (g, true, "wood", { lay::TOTAL_W - lay::WOOD_W, 0.0f, lay::WOOD_W, lay::TOTAL_H });
    drawPiece (g, true, "top", { 0.0f, 0.0f, lay::TOTAL_W, lay::TOP_FRAME });
    drawPiece (g, true, "leather", { lay::WOOD_W, sy (lay::SEC_FAD[1]), lay::TOTAL_W - 2 * lay::WOOD_W, lay::ARM });

    for (auto& s : strips) drawStripStatic (g, s);
    drawMasterStatic (g);
    for (auto& c : ctls) drawCtl (g, c);
    cacheDirty = false;
}

// ------------------------------------------------------------------------------------------------ o que mexe (30 vezes/s)
void MesaEditor::drawNeedle (juce::Graphics& g, float cx, float cy, float w, float h, float vu)
{
    juce::Graphics::ScopedSaveState ss (g);
    const juce::Rectangle<float> face (cx - w / 2, cy - h / 2, w, h * 0.85f);
    juce::Path clip; clip.addRectangle (face);
    g.reduceClipRegion (clip);
    const float a = juce::degreesToRadians (vuAngle (vu));
    const juce::Point<float> pivot (cx, cy + h * 0.62f);
    const float L = h * 0.98f * 1.06f;
    const juce::Point<float> tip (pivot.x + std::sin (a) * L, pivot.y - std::cos (a) * L);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine ({ pivot.translated (0.06f, 0.08f), tip.translated (0.06f, 0.08f) }, h * 0.02f);
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
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    for (auto& s : strips)
    {
        if (s.p == nullptr) continue;
        const auto& m = meters[s.p];
        drawNeedle (g, s.cx + lay::VU_CH[0], sy (lay::VU_CH[1]), lay::VU_CH[2], lay::VU_CH[3], m.vu);
        const float pk = juce::jmax (m.peakL, m.peakR);
        if (pk > -50.0f) drawLed (g, "g", true, s.cx + lay::SIG[0], sy (lay::SIG[1]), 0.11f, 0.11f);
        if (pk > -1.0f)  drawLed (g, "r", true, s.cx + lay::PEAK[0], sy (lay::PEAK[1]), 0.11f, 0.11f);

        const float step = (lay::FAD_YT - lay::FAD_YB) / (lay::METER_N - 1), hh = step * 0.62f;
        for (int c = 0; c < 2; ++c)
        {
            const float lv = c == 0 ? m.peakL : m.peakR;
            for (int i = 0; i < lay::METER_N && lv >= chThr[i]; ++i)
                drawLed (g, ledColour (i, lay::METER_N), true, s.cx + lay::METER_X[c], sy (lay::FAD_YB + (float) i * step), lay::METER_W / 2, hh / 2);
        }
        for (int i = 0; i < 5 && m.gr >= grThr[i]; ++i)
            drawLed (g, i < 4 ? "a" : "r", true, s.cx + lay::GR_LEDS[i][0], sy (lay::GR_LEDS[i][1]), lay::GR_LED_W / 2, lay::GR_LED_H / 2);
    }

    const float mx = lay::X_M0 + lay::MW / 2;
    const auto& m = masterMeter;
    if (master != nullptr)
    {
        drawNeedle (g, mx + lay::MVU[0][0], sy (lay::MVU[0][1]), lay::MVU[0][2], lay::MVU[0][3], m.vuL);
        drawNeedle (g, mx + lay::MVU[1][0], sy (lay::MVU[1][1]), lay::MVU[1][2], lay::MVU[1][3], m.vuR);
        drawNeedle (g, mx + lay::COLA_VU[0], sy (lay::COLA_VU[1]), lay::COLA_VU[2], lay::COLA_VU[3], -m.gr);
        const float step = (lay::MFAD_YT - lay::MFAD_YB) / (lay::MMETER_N - 1), hh = step * 0.62f;
        for (int c = 0; c < 2; ++c)
        {
            const float lv = c == 0 ? m.peakL : m.peakR;
            for (int i = 0; i < lay::MMETER_N && lv >= mThr[i]; ++i)
                drawLed (g, ledColour (i, lay::MMETER_N), true, mx + lay::MMETER_X[c], sy (lay::MFAD_YB + (float) i * step), lay::MMETER_W / 2, hh / 2);
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
        drawNeedle (g, mx + lay::MVU[0][0], sy (lay::MVU[0][1]), lay::MVU[0][2], lay::MVU[0][3], -25.0f);
        drawNeedle (g, mx + lay::MVU[1][0], sy (lay::MVU[1][1]), lay::MVU[1][2], lay::MVU[1][3], -25.0f);
    }
}

void MesaEditor::drawBubble (juce::Graphics& g)
{
    const int i = active >= 0 ? active : hover;
    if (i < 0 || i >= (int) ctls.size()) return;
    const auto txt = describe (ctls[(size_t) i]);
    if (txt.isEmpty()) return;
    const float k = ppcm();
    const auto& c = ctls[(size_t) i];
    auto f = uiFont (juce::jmax (11.0f, 0.5f * k));
    const float w = juce::GlyphArrangement::getStringWidth (f, txt) + 16.0f, h = f.getHeight() + 8.0f;
    float x = c.x * k - w / 2, y = (c.y - c.d * 0.5f) * k - h - 6.0f;
    if (c.kind == Kind::fader || c.kind == Kind::mfader) y = c.y * k - h - 4.0f;
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
        g.addTransform (juce::AffineTransform::scale (ppcm()));
        drawDynamic (g);
    }
    drawBubble (g);
}

void MesaEditor::resized()
{
    const float k = ppcm();
    scrollBar.setBounds (juce::Rectangle<float> (lay::X_CH0, sy (lay::SEC_FAD[1]) + lay::ARM * 0.62f, lay::VIS * lay::SW, lay::ARM * 0.22f)
                             .transformedBy (juce::AffineTransform::scale (k)).toNearestInt());
    if (openSettle >= 15) proc.uiScale = (float) getWidth() / (lay::TOTAL_W * basePpcm);
    cacheDirty = true;
}

// ------------------------------------------------------------------------------------------------ medidores e mudanças
void MesaEditor::timerCallback()
{
    if (proc.registry->version.load() != lastVersion) rebuild();
    if (openSettle < 15 && ++openSettle == 15)
    {
        const float saved = proc.uiScale.load();
        if (saved > 0.0f && std::abs ((float) getWidth() / (lay::TOTAL_W * basePpcm) - saved) > 0.01f)
            setSize (juce::roundToInt (lay::TOTAL_W * basePpcm * saved), juce::roundToInt (lay::TOTAL_H * basePpcm * saved));
        else if (saved <= 0.0f)
            proc.uiScale = (float) getWidth() / (lay::TOTAL_W * basePpcm);
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
    const float k = ppcm();
    const float x = pos.x / k, y = pos.y / k;
    for (int i = (int) ctls.size() - 1; i >= 0; --i)
    {
        const auto& c = ctls[(size_t) i];
        const float dx = x - c.x, dy = y - c.y;
        switch (c.kind)
        {
            case Kind::knob: if (dx * dx + dy * dy < c.d * c.d * 0.36f) return &c; break;
            case Kind::toggle: case Kind::radio: case Kind::modeBtn: case Kind::optBtn:
                if (std::abs (dx) < c.d * 0.6f && std::abs (dy) < c.d * 0.6f) return &c; break;
            case Kind::fader: case Kind::mfader:
                if (std::abs (dx) < 0.75f && dy > -1.4f && dy < c.d + 1.4f) return &c; break;
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
    const auto* c = hit (e.position);
    const int h = c != nullptr ? (int) (c - ctls.data()) : -1;
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (c != nullptr && c->kind != Kind::tape ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void MesaEditor::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void MesaEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMainMenu(); return; }
    const auto* c = hit (e.position);
    if (c == nullptr) return;
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
        dragValue = juce::jlimit (0.0f, 1.0f, dragStart + dy * fine / (c.d * ppcm()));
        if (auto* prm = c.p->param (id::fader)) setNorm (c, prm->convertTo0to1 (faderDb (dragValue)));
    }
    else return;
    cacheDirty = true;
    repaint();
}

void MesaEditor::mouseUp (const juce::MouseEvent&)
{
    end();
    active = -1;
    repaint();
}

void MesaEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
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
    const auto* c = hit (e.position);
    if (c != nullptr && c->kind == Kind::knob)
    {
        setNorm (*c, norm (*c) + (w.deltaY != 0 ? w.deltaY : w.deltaX) * 0.06f);
        cacheDirty = true;
        return;
    }
    if (c != nullptr && (c->kind == Kind::fader || c->kind == Kind::mfader))
    {
        if (auto* prm = c->p->param (id::fader))
            setNorm (*c, prm->convertTo0to1 (faderDb (faderFrac (c->p->value (id::fader)) - w.deltaY * 0.04f)));
        cacheDirty = true;
        return;
    }
    if (scrollBar.isVisible())
    {
        const float d = w.deltaX != 0 ? -w.deltaX : -w.deltaY;
        scrollBar.setCurrentRangeStart (scrollBar.getCurrentRangeStart() + (d > 0 ? 1.0 : -1.0));
    }
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
                                 "na fita, renomeia. Shift: ajuste fino."), "OK", safe.getComponent());
                     });
}

juce::AudioProcessorEditor* MesaProcessor::createEditor()
{
    return new MesaEditor (*this);
}
