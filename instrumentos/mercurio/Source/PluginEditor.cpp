#include "PluginEditor.h"
#include <complex>

using namespace mercurio;
using mrc::RF;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuNoUser = 20005,
              menuImport = 20006, menuRemoveBank = 30000;   // menuRemoveBank + n = remove o banco importado n

// posições fixas desenhadas no painel (o timer repinta só elas)
const juce::Point<float> midiLedPos { 66.0f, 88.0f };
const juce::Point<float> syncLedPos { 118.0f, 88.0f };
const juce::Rectangle<int> statusArea { 56, 78, 190, 22 };
const juce::Rectangle<int> meterArea { 1352, 20, 40, 88 };
const juce::Point<float> lfoLedPos[3] = { { 288.0f, 621.0f }, { 548.0f, 621.0f }, { 748.0f, 621.0f } };

// ---------------- o que os visores desenham ----------------
float lfoShape (int shape, float t)
{
    static const float sh[8] = { 0.6f, -0.3f, 0.9f, -0.7f, 0.2f, -0.9f, 0.5f, 0.0f };
    t -= std::floor (t);
    switch (shape)
    {
        case 0:  return std::sin (juce::MathConstants<float>::twoPi * t);
        case 1:  return 1.0f - 4.0f * std::abs (t - 0.5f);
        case 2:  return 1.0f - 2.0f * t;
        case 3:  return t < 0.5f ? 1.0f : -1.0f;
        case 4:  return sh[(int) (t * 8.0f) & 7];
        default:
        {
            const float p = t * 8.0f;
            const int i = (int) p;
            const float f = p - (float) i, a = sh[(i + 7) & 7], b = sh[i & 7];
            return a + (b - a) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * f));
        }
    }
}

// módulo da resposta de um filtro (modos na numeração do FILTRO 1: 0 LP24 … 6 VOWEL) na frequência f
float filterMag (int mode, float f, float fc, float reso)
{
    using C = std::complex<float>;
    const float w = f / juce::jmax (1.0f, fc);
    const C s (0.0f, w);
    const float K = 2.0f * (1.0f - 0.97f * reso);
    const C den = s * s + K * s + 1.0f;
    switch (mode)
    {
        case 0: { const float k = 3.9f * reso; return std::abs ((1.0f + k * 0.45f) / (std::pow (1.0f + s, 4.0f) + k)); }
        case 1:  return std::abs (1.0f / den);
        case 2:  return std::abs (s * s / den);
        case 3:  return std::abs (K * s / den);
        case 4:  return std::abs ((s * s + 1.0f) / den);
        case 5:
        {
            const float fb = 0.5f + 0.49f * reso;
            const C z = std::polar (1.0f, -juce::MathConstants<float>::twoPi * w);
            return std::abs (1.0f / (1.0f - fb * 0.9f * z)) * (1.0f - 0.45f * fb);
        }
        default:
        {
            static constexpr float F[5][3] = { { 300.0f, 870.0f, 2240.0f }, { 570.0f, 840.0f, 2410.0f }, { 730.0f, 1090.0f, 2440.0f },
                                               { 530.0f, 1840.0f, 2480.0f }, { 270.0f, 2290.0f, 3010.0f } };
            const float pos = juce::jlimit (0.0f, 1.0f, (std::log2 (juce::jmax (20.0f, fc)) - 7.6f) / 4.6f) * 4.0f;
            const int i = juce::jmin (3, (int) pos);
            const float fr = pos - (float) i, q = 0.9f - 0.8f * reso;
            float m = 0.0f;
            const float gains[3] = { 1.0f, 0.7f, 0.45f };
            for (int b = 0; b < 3; ++b)
            {
                const float fb = F[i][b] + (F[i + 1][b] - F[i][b]) * fr;
                const float kb = 2.0f * q * (b == 0 ? 0.55f : 0.35f);
                const C sb (0.0f, f / fb);
                m += gains[b] * std::abs (kb * sb / (sb * sb + kb * sb + 1.0f));
            }
            return m * 1.6f;
        }
    }
}
} // namespace

// =====================================================================
// VISOR DO PRESET
// =====================================================================
void LcdDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (5.0f);
    mrc::glassUnder (g, r, 3.0f);
    const auto in = r.reduced (12.0f, 7.0f);
    mrc::oledText (g, l1, in.withHeight (13.0f), 11.5f, juce::Justification::centredLeft, mrc::inkDim);
    mrc::oledText (g, l1r, in.withHeight (13.0f), 11.5f, juce::Justification::centredRight, mrc::inkDim);
    g.setColour (mrc::inkDim.withAlpha (0.35f));
    g.fillRect (in.getX(), in.getY() + 15.0f, in.getWidth(), 0.7f);
    mrc::oledText (g, l2, in.withTrimmedTop (17.0f).withHeight (30.0f), 23.0f, juce::Justification::centredLeft, mrc::ink, false);
    mrc::oledText (g, l3, in.withTrimmedTop (48.0f).withHeight (15.0f), 12.0f, juce::Justification::centredLeft, mrc::hotGlass);
    mrc::glassOver (g, r, 3.0f);
}

// =====================================================================
// EDITOR
// =====================================================================
mrc::VKnob* MercurioEditor::knob (int cx, int y, int size, const juce::String& pid, const juce::String& label, juce::Colour pointer)
{
    // largo para o nome não cortar (SUPER SPREAD); o VKnob não pega clique fora do knob, então pode sobrepor o vizinho
    const int w = juce::jmax (size + 14, 92);
    return add<mrc::VKnob> ({ cx - w / 2, y, w, size + 10 + (label.isNotEmpty() ? 12 : 0) }, proc.apvts, pid, label, size, pointer);
}

mrc::Key* MercurioEditor::key (int cx, int y, const juce::String& label, bool led)
{
    const int w = 64 + (led ? 20 : 0);
    return add<mrc::Key> ({ cx - 32, y, w, label.isNotEmpty() ? 32 : 20 }, label, led);
}

mrc::LcdBox* MercurioEditor::box (juce::Rectangle<int> r, const juce::String& pid, const juce::String& caption)
{
    if (caption.isNotEmpty())
        captions.push_back ({ RF ((float) r.getX(), (float) r.getBottom(), (float) r.getWidth(), 10.0f), caption });
    return add<mrc::LcdBox> (r, proc.apvts, pid);
}

void MercurioEditor::attach (juce::Button& b, const juce::String& paramId)
{
    b.setClickingTogglesState (true);
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramId, b));
}

mrc::Scope* MercurioEditor::scope (juce::Rectangle<int> r)
{
    auto* s = add<mrc::Scope> (r);
    s->setInterceptsMouseClicks (false, false);
    scopes.push_back (s);
    return s;
}

MercurioEditor::MercurioEditor (MercurioProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, mrc::baseW, mrc::baseH);
    addAndMakeVisible (canvas);

    auto& st = proc.apvts;
    const auto red = mrc::hot;

    // ================= CABEÇALHO: presets, VOICE, MASTER =================
    lcd = add<LcdDisplay> ({ 336, 20, 470, 84 });
    lcd->onClick = [this] { showPresetMenu(); };
    key (296, 26, "BROWSE")->onClick = [this] { showPresetMenu(); };
    key (296, 64, "STORE")->onClick = [this] { askNewPreset(); };
    key (840, 26, "PREV")->onClick = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    key (840, 64, "NEXT")->onClick = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };

    add<mrc::LedList> ({ 888, 40, 72, 45 }, keyModeNames, true, 15.0f)->bind (st, id::keyMode);
    knob (990, 38, 30, id::porta, "GLIDE");
    knob (1042, 38, 30, id::bendRange, "BEND");
    knob (1094, 38, 30, id::unison, "UNISON");
    knob (1146, 38, 30, id::uniDetune, "DETUNE");
    knob (1198, 38, 30, id::uniSpread, "SPREAD");
    knob (1250, 38, 30, id::drift, "DRIFT");
    knob (1314, 28, 46, id::volume, "MASTER");

    // ================= FM: algoritmo, realimentação e o que vem depois dos operadores =================
    algoScope = scope ({ 58, 130, 284, 116 });
    box ({ 60, 250, 280, 28 }, id::algo);
    knob (80, 286, 32, id::feedback, "FEEDBACK", red);
    knob (140, 286, 32, id::fold, "FOLD", red);
    knob (200, 286, 32, id::ring, "RING MOD");
    knob (260, 286, 32, id::noiseVol, "NOISE");
    knob (320, 286, 32, id::oscVol, "DRIVE");

    // ================= OPERADORES =================
    for (int k = 0; k < numOps; ++k)
    {
        const int x0 = opX (k);
        opScopes[(size_t) k] = scope ({ x0 + 6, 130, 152, 88 });
        box ({ x0 + 160, 132, 86, 28 }, id::op (k, "wave"), "WAVE");
        knob (x0 + 203, 172, 30, id::op (k, "lvl"), "LEVEL", mrc::opColour[k]);
        knob (x0 + 34, 224, 30, id::op (k, "coarse"), "RATIO");
        knob (x0 + 94, 224, 30, id::op (k, "fine"), "FINE");
        knob (x0 + 154, 224, 30, id::op (k, "det"), "DETUNE");
        knob (x0 + 214, 224, 30, id::op (k, "vel"), "VELOCITY");
        knob (x0 + 34, 286, 30, id::op (k, "a"), "ATTACK");
        knob (x0 + 94, 286, 30, id::op (k, "d"), "DECAY");
        knob (x0 + 154, 286, 30, id::op (k, "s"), "SUSTAIN");
        knob (x0 + 214, 286, 30, id::op (k, "r"), "RELEASE");
    }

    // ================= FILTROS =================
    filterScope = scope ({ 60, 384, 238, 104 });
    add<mrc::LedList> ({ 68, 498, 96, 30 }, routingNames, true, 15.0f)->bind (st, id::routing);
    knob (196, 494, 34, id::filtBal, "BALANCE");
    box ({ 64, 550, 120, 28 }, id::satType, "SATURATION");
    knob (240, 544, 34, id::satAmt, "SAT DRIVE", red);

    add<mrc::LedList> ({ 312, 398, 74, 105 }, f1ModeNames, true, 15.0f)->bind (st, id::f1Mode);
    knob (456, 398, 66, id::cutoff, "CUTOFF", red);
    knob (340, 512, 36, id::reso1, "RESONANCE");
    knob (412, 512, 36, id::env1, "ENV AMOUNT");
    knob (484, 512, 36, id::key1, "KEY TRACK");

    add<mrc::LedList> ({ 538, 398, 74, 90 }, f2ModeNames, true, 15.0f)->bind (st, id::f2Mode);
    knob (674, 404, 48, id::cutoff2, "CUTOFF 2");
    knob (566, 512, 36, id::reso2, "RESONANCE");
    knob (638, 512, 36, id::env2, "ENV AMOUNT");
    knob (710, 512, 36, id::key2, "KEY TRACK");

    // ================= ENVELOPES =================
    const char* const envNames[6] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "SLOPE", "VELOCITY" };
    const juce::String fIds[6] = { id::fA, id::fD, id::fS, id::fR, id::fSlope, id::fVel };
    const juce::String aIds[6] = { id::aA, id::aD, id::aS, id::aR, id::aSlope, id::aVel };
    envScopes[0] = scope ({ 770, 392, 160, 86 });
    envScopes[1] = scope ({ 770, 502, 160, 86 });
    for (int i = 0; i < 6; ++i)
    {
        knob (968 + i * 74, 396, 38, fIds[i], envNames[i]);
        knob (968 + i * 74, 506, 38, aIds[i], envNames[i]);
    }

    // ================= LFOs =================
    const juce::String rateIds[3]  = { id::lfo1Rate, id::lfo2Rate, id::lfo3Rate };
    const juce::String shapeIds[3] = { id::lfo1Shape, id::lfo2Shape, id::lfo3Shape };
    const juce::String syncIds[3]  = { id::lfo1Sync, id::lfo2Sync, id::lfo3Sync };
    const int lx[3] = { 50, 310, 570 };
    for (int k = 0; k < 3; ++k)
    {
        const int x = lx[k];
        const bool small = k == 2;
        lfoScopes[(size_t) k] = scope ({ x + 10, 634, small ? 104 : 132, 72 });
        knob (x + (small ? 156 : 196), 634, 40, rateIds[k], "RATE");
        box ({ x + 10, 710, small ? 84 : 112, 26 }, shapeIds[k], "SHAPE");
        box ({ x + (small ? 98 : 128), 710, small ? 84 : 112, 26 }, syncIds[k], "CLOCK");
    }
    knob (82, 768, 30, id::lfo1Pitch, "PITCH");
    knob (140, 768, 30, id::lfo1Cut, "CUTOFF");
    knob (198, 768, 30, id::lfo1Ratio, "RATIO");
    knob (256, 768, 30, id::lfo1Amp, "AMP");
    knob (342, 768, 30, id::lfo2Index, "FM INDEX");
    knob (400, 768, 30, id::lfo2Fb, "FEEDBACK");
    knob (458, 768, 30, id::lfo2Cut, "CUTOFF");
    knob (516, 768, 30, id::lfo2Pan, "PAN");
    box ({ 580, 772, 116, 26 }, id::lfo3Dst, "DESTINATION");
    knob (724, 766, 30, id::lfo3Amt, "AMOUNT", red);

    // ================= MATRIZ =================
    for (int k = 0; k < numSlots; ++k)
    {
        const int y = 646 + k * 31;
        box ({ 796, y, 140, 29 }, id::mSrc (k));
        box ({ 940, y, 150, 29 }, id::mDst (k));
        knob (1110, y - 2, 22, id::mAmt (k), {}, red);
    }

    // ================= ARP =================
    attach (*key (1172, 640, "ARP ON", true), id::arpOn);
    attach (*key (1172, 686, "HOLD", true), id::arpHold);
    add<mrc::LedList> ({ 1226, 634, 88, 90 }, arpModeNames, true, 15.0f)->bind (st, id::arpMode);
    box ({ 1318, 640, 64, 28 }, id::arpOct, "OCTAVES");
    knob (1196, 752, 38, id::arpRate, "RATE");
    knob (1270, 752, 38, id::arpGate, "GATE");

    // ================= EFEITOS =================
    box ({ 64, 884, 80, 28 }, id::distType, "TYPE");
    knob (180, 878, 34, id::distAmt, "AMOUNT", red);
    knob (250, 878, 34, id::boostAmt, "AMOUNT", red);
    knob (308, 878, 34, id::boostTune, "TUNE");
    knob (378, 878, 34, id::choMix, "MIX");
    knob (436, 878, 34, id::choInt, "DEPTH");
    knob (506, 878, 34, id::phaMix, "MIX");
    knob (564, 878, 34, id::phaInt, "DEPTH");
    knob (634, 878, 34, id::dlySend, "SEND", red);
    knob (692, 878, 34, id::dlyTime, "TIME");
    knob (750, 878, 34, id::dlyFb, "FEEDBACK");
    knob (820, 878, 34, id::rvbSend, "SEND", red);
    knob (878, 878, 34, id::rvbSize, "SIZE");
    knob (936, 878, 34, id::rvbDamp, "DAMPING");
    knob (1006, 878, 34, id::eqLow, "LOW");
    knob (1064, 878, 34, id::eqMid, "MID");
    knob (1122, 878, 34, id::eqHigh, "HIGH");

    buildScopes();

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto.
    // o tamanho salvo é lido ANTES do setResizeLimits: ele encolhe a janela ainda vazia para o mínimo
    // e o resized() gravaria o mínimo por cima do que o usuário escolheu
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) mrc::baseW * mrc::minScale), juce::roundToInt ((float) mrc::baseH * mrc::minScale),
                     juce::roundToInt ((float) mrc::baseW * mrc::maxScale), juce::roundToInt ((float) mrc::baseH * mrc::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) mrc::baseW / (double) mrc::baseH);
    setScale (openScale);
    startTimerHz (30);
}

MercurioEditor::~MercurioEditor()
{
    dialog.close();
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------- o que cada visor desenha ----------------
void MercurioEditor::buildScopes()
{
    // ---- algoritmo: quem modula quem; os portadores (que soam) acesos, a realimentação no OP 4 ----
    algoScope->hash = [this] { return pv (id::algo) + pv (id::feedback) * 0.001f; };
    algoScope->caption = [] { return juce::String ("ALGORITHM"); };
    algoScope->draw = [this] (juce::Graphics& g, RF r) { drawAlgorithm (g, r, juce::roundToInt (pv (id::algo)), pv (id::feedback) * 0.01f); };

    // ---- operadores: um ciclo da onda, com a razão e o papel (portador / modulador) ----
    for (int k = 0; k < numOps; ++k)
    {
        const juce::String wid = id::op (k, "wave"), cid = id::op (k, "coarse"), fid = id::op (k, "fine"), lid = id::op (k, "lvl");
        auto* s = opScopes[(size_t) k];
        auto carrier = [this, k] { return ((algos[juce::jlimit (0, numAlgos - 1, juce::roundToInt (pv (id::algo)))].carriers >> k) & 1) != 0; };
        s->hash = [this, wid, cid, fid, lid, carrier]
        {
            return pv (wid) * 1000.0f + pv (cid) * 7.0f + pv (fid) * 0.013f + pv (lid) * 0.00071f + (carrier() ? 0.5f : 0.0f);
        };
        s->caption = [this, k, cid, fid, carrier]
        {
            return "OP" + juce::String (k + 1) + "  x" + juce::String (opRatio (pv (cid), pv (fid)), 2) + (carrier() ? "  OUT" : "  MOD");
        };
        s->draw = [this, k, wid, lid, carrier] (juce::Graphics& g, RF r)
        {
            mrc::gridLines (g, r, 4, 2);
            const int w = juce::roundToInt (pv (wid));
            const float lv = juce::jmax (0.15f, pv (lid) * 0.01f);
            juce::Path p;
            const int n = 200;
            for (int i = 0; i <= n; ++i)
            {
                const float t = (float) i / (float) n;
                const float y = r.getCentreY() + 5.0f - OpWaves::shape (w, t * 2.0f) * lv * (r.getHeight() - 14.0f) * 0.5f;
                const float x = r.getX() + t * r.getWidth();
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            mrc::glowPath (g, p, carrier() ? mrc::opColour[k] : mrc::opColour[k].withAlpha (0.55f), 1.5f);
        };
    }

    // ---- filtros: a curva dos dois juntos, como estão roteados ----
    filterScope->hash = [this]
    {
        return std::log2 (pv (id::cutoff)) * 1.0f + pv (id::reso1) * 0.011f + pv (id::reso2) * 0.0137f + pv (id::f1Mode) * 31.0f
             + pv (id::f2Mode) * 257.0f + pv (id::cutoff2) * 0.71f + pv (id::filtBal) * 0.0093f + pv (id::routing) * 3001.0f;
    };
    filterScope->caption = [this]
    {
        return "FILTERS  " + f1ModeNames[juce::roundToInt (pv (id::f1Mode))] + " > " + f2ModeNames[juce::roundToInt (pv (id::f2Mode))];
    };
    filterScope->draw = [this] (juce::Graphics& g, RF r)
    {
        mrc::gridLines (g, r, 6, 4);
        const float fc1 = pv (id::cutoff), fc2 = fc1 * std::exp2 (pv (id::cutoff2) / 12.0f);
        const int m1 = juce::roundToInt (pv (id::f1Mode)), m2 = juce::roundToInt (pv (id::f2Mode)) + 1;
        const float r1 = pv (id::reso1) * 0.01f, r2 = pv (id::reso2) * 0.01f, bal = pv (id::filtBal) * 0.01f;
        const bool par = pv (id::routing) > 0.5f;
        juce::Path p;
        const int n = 220;
        for (int i = 0; i <= n; ++i)
        {
            const float t = (float) i / (float) n;
            const float f = 20.0f * std::pow (1000.0f, t);
            const float h1 = filterMag (m1, f, fc1, r1), h2 = filterMag (m2, f, fc2, r2);
            const float h = par ? (1.0f - bal) * h1 + bal * h2 : h1 * ((1.0f - bal) + bal * h2);
            const float db = juce::jlimit (-36.0f, 18.0f, juce::Decibels::gainToDecibels (h, -60.0f));
            const float y = r.getY() + (18.0f - db) / 54.0f * r.getHeight();
            const float x = r.getX() + t * r.getWidth();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        juce::Path fill (p);
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setGradientFill (real::vertical (mrc::ink.withAlpha (0.16f), r.getY(), mrc::ink.withAlpha (0.0f), r.getBottom()));
        g.fillPath (fill);
        mrc::glowPath (g, p, mrc::ink, 1.5f);
        const float xc = r.getX() + std::log10 (juce::jlimit (20.0f, 20000.0f, fc1) / 20.0f) / 3.0f * r.getWidth();
        g.setColour (mrc::hot.withAlpha (0.8f));
        g.fillRect (xc - 0.5f, r.getY(), 1.0f, r.getHeight());
        const float hz = fc1;
        mrc::oledText (g, hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz",
                       r.withTrimmedTop (r.getHeight() - 14.0f), 10.5f, juce::Justification::bottomRight, mrc::hotGlass);
    };

    // ---- envelopes: ADSR com a inclinação do sustain ----
    for (int e = 0; e < 2; ++e)
    {
        const juce::String a = e == 0 ? id::fA : id::aA, d = e == 0 ? id::fD : id::aD, su = e == 0 ? id::fS : id::aS,
                           rl = e == 0 ? id::fR : id::aR, sl = e == 0 ? id::fSlope : id::aSlope;
        auto* s = envScopes[(size_t) e];
        s->hash = [this, a, d, su, rl, sl] { return pv (a) * 1.3f + pv (d) * 0.71f + pv (su) * 17.0f + pv (rl) * 0.37f + pv (sl) * 101.0f; };
        s->caption = [e] { return juce::String (e == 0 ? "FILTER ENV" : "AMP ENV"); };
        s->draw = [this, a, d, su, rl, sl] (juce::Graphics& g, RF r)
        {
            mrc::gridLines (g, r, 4, 2);
            const float A = pv (a), D = pv (d), S = pv (su) * 0.01f, Rl = pv (rl), slope = pv (sl) * 0.01f;
            auto len = [] (float ms) { return std::sqrt (ms + 4.0f); };
            const float hold = 40.0f;
            const float sum = len (A) + len (D) + hold + len (Rl);
            const float k = r.getWidth() / sum;
            const float x0 = r.getX(), x1 = x0 + len (A) * k, x2 = x1 + len (D) * k, x3 = x2 + hold * k, x4 = r.getRight();
            auto Y = [&] (float v) { return r.getBottom() - v * r.getHeight() * 0.92f; };
            juce::Path p;
            p.startNewSubPath (x0, Y (0.0f));
            p.lineTo (x1, Y (1.0f));
            for (int i = 1; i <= 24; ++i)
            {
                const float t = (float) i / 24.0f;
                p.lineTo (x1 + (x2 - x1) * t, Y (S + (1.0f - S) * std::exp (-t * 4.6f)));
            }
            float end = S;
            for (int i = 1; i <= 16; ++i)
            {
                const float t = (float) i / 16.0f;
                if (std::abs (slope) > 0.005f)
                {
                    const float tgt = slope > 0.0f ? 1.0f : 0.0f;
                    end = S + (tgt - S) * (1.0f - std::exp (-t * 3.0f * std::abs (slope)));
                }
                p.lineTo (x2 + (x3 - x2) * t, Y (end));
            }
            for (int i = 1; i <= 24; ++i)
            {
                const float t = (float) i / 24.0f;
                p.lineTo (x3 + (x4 - x3) * t, Y (end * std::exp (-t * 4.6f)));
            }
            juce::Path fill (p);
            fill.lineTo (x4, r.getBottom());
            fill.closeSubPath();
            g.setGradientFill (real::vertical (mrc::ink.withAlpha (0.14f), r.getY(), mrc::ink.withAlpha (0.0f), r.getBottom()));
            g.fillPath (fill);
            mrc::glowPath (g, p, mrc::ink, 1.4f);
        };
    }

    // ---- LFOs: dois ciclos da forma ----
    const juce::String shapeIds[3] = { id::lfo1Shape, id::lfo2Shape, id::lfo3Shape };
    for (int l = 0; l < 3; ++l)
    {
        const juce::String sid = shapeIds[l];
        auto* s = lfoScopes[(size_t) l];
        s->hash = [this, sid] { return pv (sid); };
        s->caption = [l] { return "LFO " + juce::String (l + 1); };
        s->draw = [this, sid] (juce::Graphics& g, RF r)
        {
            mrc::gridLines (g, r, 4, 2);
            const int shape = juce::roundToInt (pv (sid));
            juce::Path p;
            const int n = 200;
            for (int i = 0; i <= n; ++i)
            {
                const float t = (float) i / (float) n;
                const float y = r.getCentreY() - lfoShape (shape, t * 2.0f) * r.getHeight() * 0.38f;
                const float x = r.getX() + t * r.getWidth();
                if (i == 0) p.startNewSubPath (x, y);
                else if (shape >= 3 && shape != 5) { p.lineTo (x, p.getCurrentPosition().y); p.lineTo (x, y); }
                else p.lineTo (x, y);
            }
            mrc::glowPath (g, p, mrc::ink, 1.4f);
        };
    }
}

// ---------------- desenho do algoritmo ----------------
// posição de cada operador (coluna 0..3, linha 0..3 de cima para baixo) em cada algoritmo; os portadores ficam embaixo
void MercurioEditor::drawAlgorithm (juce::Graphics& g, RF r, int algo, float feedback)
{
    static constexpr float pos[numAlgos][numOps][2] = {
        { { 1.5f, 3 }, { 1.5f, 2 }, { 1.5f, 1 }, { 1.5f, 0 } },   // 1  4 > 3 > 2 > 1
        { { 1.5f, 2 }, { 1.5f, 1 }, { 1.0f, 0 }, { 2.0f, 0 } },   // 2  (3 + 4) > 2 > 1
        { { 1.5f, 2 }, { 1.0f, 1 }, { 1.0f, 0 }, { 2.0f, 1 } },   // 3  3 > 2 > 1, 4 > 1
        { { 1.5f, 2 }, { 1.0f, 1 }, { 2.0f, 1 }, { 2.0f, 0 } },   // 4  4 > 3 > 1, 2 > 1
        { { 1.0f, 2 }, { 1.0f, 1 }, { 2.0f, 2 }, { 2.0f, 1 } },   // 5  2 > 1, 4 > 3
        { { 0.5f, 2 }, { 1.5f, 2 }, { 2.5f, 2 }, { 1.5f, 1 } },   // 6  4 > 1, 2, 3
        { { 0.5f, 2 }, { 1.5f, 2 }, { 2.5f, 2 }, { 2.5f, 1 } },   // 7  4 > 3; 1, 2, 3
        { { 0.0f, 2 }, { 1.0f, 2 }, { 2.0f, 2 }, { 3.0f, 2 } },   // 8  os quatro soltos
    };
    algo = juce::jlimit (0, numAlgos - 1, algo);
    const auto& al = algos[algo];
    const float bw = 30.0f, bh = 17.0f;
    const float top = r.getY() + 14.0f, bus = r.getBottom() - 4.0f;
    const float rowStep = (bus - 8.0f - bh - top) / 3.0f;
    float maxRow = 0.0f;
    for (int k = 0; k < numOps; ++k) maxRow = juce::jmax (maxRow, pos[algo][k][1]);
    const float shift = (3.0f - maxRow) * rowStep;                        // os portadores sempre na linha de baixo
    auto box = [&] (int k)
    {
        const float cx = r.getX() + (pos[algo][k][0] + 0.5f) / 4.0f * r.getWidth();
        return RF (cx - bw * 0.5f, top + shift + pos[algo][k][1] * rowStep, bw, bh);
    };
    // ligações: modulador (embaixo dele) → topo do modulado
    for (int k = 0; k < numOps; ++k)
        for (int j = 0; j < numOps; ++j)
            if ((al.mods[k] >> j) & 1)
            {
                const auto a = box (j), b = box (k);
                juce::Path p;
                p.startNewSubPath (a.getCentreX(), a.getBottom());
                p.lineTo (a.getCentreX(), (a.getBottom() + b.getY()) * 0.5f);
                p.lineTo (b.getCentreX(), (a.getBottom() + b.getY()) * 0.5f);
                p.lineTo (b.getCentreX(), b.getY());
                mrc::glowPath (g, p, mrc::ink.withAlpha (0.75f), 1.2f);
            }
    // saída: cada portador desce até a barra
    float busL = r.getRight(), busR = r.getX();
    for (int k = 0; k < numOps; ++k)
        if ((al.carriers >> k) & 1)
        {
            const auto b = box (k);
            juce::Path p;
            p.startNewSubPath (b.getCentreX(), b.getBottom());
            p.lineTo (b.getCentreX(), bus);
            mrc::glowPath (g, p, mrc::ink.withAlpha (0.75f), 1.2f);
            busL = juce::jmin (busL, b.getCentreX());
            busR = juce::jmax (busR, b.getCentreX());
        }
    {
        juce::Path p;
        p.startNewSubPath (busL - 6.0f, bus);
        p.lineTo (busR + 6.0f, bus);
        mrc::glowPath (g, p, mrc::ink, 1.6f);
    }
    // realimentação do OP 4: laço pela direita (mais forte, mais aceso)
    {
        const auto b = box (3);
        juce::Path p;
        p.startNewSubPath (b.getRight(), b.getCentreY());
        p.lineTo (b.getRight() + 8.0f, b.getCentreY());
        p.lineTo (b.getRight() + 8.0f, b.getY() - 5.0f);
        p.lineTo (b.getCentreX(), b.getY() - 5.0f);
        p.lineTo (b.getCentreX(), b.getY());
        mrc::glowPath (g, p, (feedback > 0.005f ? mrc::opColour[3] : mrc::inkDim).withAlpha (0.4f + 0.6f * juce::jmin (1.0f, feedback * 2.0f)), 1.2f);
    }
    // caixas: portador cheio, modulador só o contorno
    for (int k = 0; k < numOps; ++k)
    {
        const auto b = box (k);
        const bool car = ((al.carriers >> k) & 1) != 0;
        const auto oc = mrc::opColour[k];
        g.setColour (oc.withAlpha (car ? 0.85f : 0.0f));
        g.fillRoundedRectangle (b, 2.5f);
        g.setColour (oc.withAlpha (0.95f));
        g.drawRoundedRectangle (b, 2.5f, 1.2f);
        mrc::oledText (g, juce::String (k + 1), b, 12.0f, juce::Justification::centred, car ? juce::Colour (0xff0b0d12) : oc);
    }
    mrc::oledText (g, algoNames[algo].fromLastOccurrenceOf (" ", false, false), r.withHeight (13.0f), 10.0f,
                   juce::Justification::topRight, mrc::hotGlass);
}

// ---------------- menu de presets (com o tamanho da janela) ----------------
void MercurioEditor::showPresetMenu()
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
        else if (b == MercurioProcessor::bankUser)
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

    // bancos avulsos (.mrcbank)
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
    for (int pct : { 60, 75, 90, 100, 125, 150, 180 })
        sizes.addItem (sizeMenuBase + pct, juce::String (pct) + "%", true, std::abs (current * 100.0f - (float) pct) < 1.0f);
    menu.addSeparator();
    menu.addSubMenu ("Tamanho da janela", sizes);
   #ifdef JucePlugin_VersionString   // (a foto do workflow shots compila a tela fora do plugin)
    menu.addSeparator();
    menu.addItem (sizeMenuBase - 1, u8 ("MERCÚRIO ") + JucePlugin_VersionString, false, false);
   #endif

    // o menu (e cada camada: banco → categoria → preset) abre DENTRO da janela do plugin: como janela solta,
    // a camada de dentro ficava por baixo da janela do plugin (que o host deixa sempre na frente) e não dava clique
    juce::Component::SafePointer<MercurioEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lcd).withParentComponent (this), [safe] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->presetMenuChosen (result);
    });
}

void MercurioEditor::askNewPreset()
{
    juce::Component::SafePointer<MercurioEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

// abre o navegador de arquivos; o banco escolhido é copiado para a pasta do MERCÚRIO e entra no menu
void MercurioEditor::importBank()
{
    auto start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    if (start.getChildFile ("Downloads").isDirectory()) start = start.getChildFile ("Downloads");
    chooser = std::make_unique<juce::FileChooser> (u8 ("Importar banco de presets do MERCÚRIO"), start, "*.mrcbank");

    juce::Component::SafePointer<MercurioEditor> safe (this);
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

void MercurioEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<MercurioEditor> safe (this);
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
void MercurioEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) mrc::baseW - openScale) > 0.005f)
        setScale (openScale);

    // visor: banco/categoria, posição, nome e, por 2,5 s, o último parâmetro mexido (nome e valor)
    const int touch = proc.touchCount.load();
    if (touch != lastTouch) { lastTouch = touch; touchTimer = proc.lastTouched.load() >= 0 ? 75 : 0; }
    if (touchTimer > 0) --touchTimer;
    juce::String bottom;
    const auto& params = proc.getParameters();
    const int idx = proc.lastTouched.load();
    if (touchTimer > 0 && idx >= 0 && idx < params.size())
        bottom = params[idx]->getName (40).toUpperCase() + ": " + params[idx]->getCurrentValueAsText();
    else
        bottom = "VOICES " + juce::String (proc.synth.activeVoices.load()).paddedLeft ('0', 2) + "/16";
    const int pi = proc.getPresetIndex();
    const juce::String count = pi >= 0 ? juce::String (pi + 1).paddedLeft ('0', 3) + "/" + juce::String (proc.getNumPresets()) : juce::String();
    lcd->set (proc.getPresetGroup(), count, proc.getPresetName(), bottom);

    for (auto* s : scopes) s->poll();

    // LEDs dos LFOs (piscam no ritmo de cada um)
    const float lv[3] = { proc.synth.lfo1Now.load(), proc.synth.lfo2Now.load(), proc.synth.lfo3Now.load() };
    for (int k = 0; k < 3; ++k)
        if (std::abs (lv[k] - lfoShown[(size_t) k]) > 0.03f)
        {
            lfoShown[(size_t) k] = lv[k];
            canvas.repaint (juce::Rectangle<int> ((int) lfoLedPos[k].x - 12, (int) lfoLedPos[k].y - 12, 24, 24));
        }

    // MIDI e SYNC
    const int midi = proc.midiActivity.load();
    if (midi != lastMidi) { lastMidi = midi; midiGlow = 4; }
    const bool lit = midiGlow > 0;
    if (midiGlow > 0) --midiGlow;
    const bool syn = proc.synced.load();
    const float bpm = proc.shownBpm.load();
    if (lit != midiLit || syn != lastSync || std::abs (bpm - lastBpm) > 0.05f)
    {
        midiLit = lit;
        lastSync = syn;
        lastBpm = bpm;
        canvas.repaint (statusArea);
    }

    levelL = juce::jmax (proc.meterL.exchange (0.0f), levelL * 0.85f);
    levelR = juce::jmax (proc.meterR.exchange (0.0f), levelR * 0.85f);
    if (std::abs (levelL - shownL) > 0.002f || std::abs (levelR - shownR) > 0.002f)
    {
        shownL = levelL;
        shownR = levelR;
        canvas.repaint (meterArea);
    }
}

// ---------------- tamanho ----------------
void MercurioEditor::setScale (float scale)
{
    scale = juce::jlimit (mrc::minScale, mrc::maxScale, scale);
    setSize (juce::roundToInt ((float) mrc::baseW * scale), juce::roundToInt ((float) mrc::baseH * scale));
}

void MercurioEditor::resized()
{
    const float scale = (float) getWidth() / (float) mrc::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void MercurioEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0e));
}

// ---------------- o aparelho ----------------
// faixa de alumínio anodizado preto (em cima e embaixo), escovada, com o filete iridescente na borda do alumínio
static void anodizedBand (juce::Graphics& g, RF r, bool top)
{
    g.setGradientFill (real::vertical (juce::Colour (0xff202227), r.getY(), juce::Colour (0xff111215), r.getBottom()));
    g.fillRect (r);
    juce::Random rnd (top ? 909 : 313);
    for (int i = 0; i < 320; ++i)
    {
        const float y = r.getY() + rnd.nextFloat() * r.getHeight();
        const float x = r.getX() + rnd.nextFloat() * r.getWidth() * 0.9f - 60.0f;
        g.setColour (real::white (0.012f + rnd.nextFloat() * 0.03f));
        g.fillRect (x, y, 80.0f + rnd.nextFloat() * 420.0f, 0.6f);
    }
    // filete iridescente (titânio aquecido) e a sombra que a faixa faz no alumínio
    const RF edge = top ? RF (r.getX(), r.getBottom() - 2.5f, r.getWidth(), 2.5f) : RF (r.getX(), r.getY(), r.getWidth(), 2.5f);
    g.setGradientFill (mrc::iridescent (edge, 0.9f));
    g.fillRect (edge);
    g.setColour (real::white (0.25f));
    g.fillRect (edge.withHeight (0.7f));
    if (top)
    {
        g.setGradientFill (real::vertical (real::black (0.35f), r.getBottom(), real::black (0.0f), r.getBottom() + 6.0f));
        g.fillRect (r.getX(), r.getBottom(), r.getWidth(), 6.0f);
    }
    else
    {
        g.setGradientFill (real::vertical (real::black (0.0f), r.getY() - 4.0f, real::black (0.25f), r.getY()));
        g.fillRect (r.getX(), r.getY() - 4.0f, r.getWidth(), 4.0f);
    }
}

// logo: gota de mercúrio (metal líquido cromado) + o nome em cromo
void MercurioEditor::drawLogo (juce::Graphics& g, float x, float y)
{
    using juce::Colour;
    const RF drop (x, y + 2.0f, 26.0f, 32.0f);
    juce::Path p;
    p.startNewSubPath (drop.getCentreX(), drop.getY());
    p.cubicTo (drop.getCentreX() + 4.0f, drop.getY() + 9.0f, drop.getRight(), drop.getY() + 14.0f, drop.getRight(), drop.getY() + 20.0f);
    p.cubicTo (drop.getRight(), drop.getBottom() - 2.0f, drop.getX() + 20.0f, drop.getBottom(), drop.getCentreX(), drop.getBottom());
    p.cubicTo (drop.getX() + 6.0f, drop.getBottom(), drop.getX(), drop.getBottom() - 2.0f, drop.getX(), drop.getY() + 20.0f);
    p.cubicTo (drop.getX(), drop.getY() + 14.0f, drop.getCentreX() - 4.0f, drop.getY() + 9.0f, drop.getCentreX(), drop.getY());
    p.closeSubPath();
    // cromo: céu claro em cima, horizonte escuro no meio, chão refletido embaixo
    auto chrome = real::vertical (Colour (0xfff4f6f9), drop.getY() + 6.0f, Colour (0xffc9d0d8), drop.getBottom());
    chrome.addColour (0.45, Colour (0xff7e8590));
    chrome.addColour (0.58, Colour (0xff23262c));
    chrome.addColour (0.80, Colour (0xff6c737d));
    g.setGradientFill (chrome);
    g.fillPath (p);
    g.setGradientFill (mrc::iridescent (drop, 0.28f));
    g.fillPath (p);
    g.setColour (real::white (0.85f));
    g.fillEllipse (drop.getX() + 6.5f, drop.getY() + 15.0f, 5.0f, 7.0f);
    g.setColour (real::black (0.6f));
    g.strokePath (p, juce::PathStrokeType (0.8f));

    const RF name (x + 36.0f, y - 1.0f, 200.0f, 30.0f);
    g.setFont (theme::sans (27.0f, true, 0.08f));
    auto txt = real::vertical (Colour (0xffffffff), name.getY() + 6.0f, Colour (0xffaab2bc), name.getBottom() - 6.0f);
    txt.addColour (0.55, Colour (0xffd5dbe2));
    g.setGradientFill (txt);
    g.drawText (u8 ("MERCÚRIO"), name, juce::Justification::centredLeft, false);
    mrc::silkText (g, "4-OPERATOR FM SYNTHESIZER", { x + 37.0f, y + 29.0f, 200.0f, 10.0f }, juce::Justification::centredLeft,
                   8.0f, true, mrc::silkDim, 0.18f);
}

void MercurioEditor::drawCanvas (juce::Graphics& g)
{
    const float W = (float) mrc::baseW, H = (float) mrc::baseH;
    g.fillAll (juce::Colour (0xff0d0d0e));

    // chapa de alumínio escovado (imagem renderizada) entre as duas laterais de metal, com as faixas anodizadas
    const RF panel (30.0f, 6.0f, W - 60.0f, H - 12.0f);
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (real::black (0.14f));
        g.fillRoundedRectangle (panel.expanded ((float) i).translated (0.0f, (float) i * 0.8f), 4.0f + (float) i);
    }
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path face;
        face.addRoundedRectangle (panel, 4.0f);
        g.reduceClipRegion (face);
        if (art->panel.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (1.0f);   // a imagem herda a transparência da última cor: desenhar sempre opaca
            g.drawImage (art->panel, panel, juce::RectanglePlacement::stretchToFit);
        }
        else
            g.fillAll (juce::Colour (0xffa9adb3));
        anodizedBand (g, { panel.getX(), panel.getY(), panel.getWidth(), mrc::topBand - panel.getY() }, true);
        anodizedBand (g, { panel.getX(), mrc::bottomBand, panel.getWidth(), panel.getBottom() - mrc::bottomBand }, false);
    }
    auto bevel = real::vertical (real::white (0.30f), panel.getY(), real::black (0.7f), panel.getBottom());
    bevel.addColour (0.03, real::white (0.06f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (panel.reduced (0.5f), 4.0f, 1.4f);
    hw::cheek (g, { 0.0f, 0.0f, 42.0f, H }, true);     // laterais de nogueira renderizadas (instrumentos/_hardware)
    hw::cheek (g, { W - 42.0f, 0.0f, 42.0f, H }, false);

    drawLogo (g, 56.0f, 22.0f);

    // ---- seções ----
    mrc::section (g, { 876.0f, 16.0f, 404.0f, 88.0f }, "VOICE");
    mrc::section (g, { 50.0f, 116.0f, 300.0f, 238.0f }, "FM");
    for (int k = 0; k < numOps; ++k)
    {
        const RF r ((float) opX (k), 116.0f, 250.0f, 238.0f);
        mrc::section (g, r, "OPERATOR " + juce::String (k + 1));
        // a cor do operador: plaqueta no canto (a mesma do ponteiro do LEVEL, da onda e da caixa no algoritmo)
        const RF tag (r.getRight() - 40.0f, r.getY() + 2.0f, 30.0f, 8.0f);
        g.setColour (real::black (0.45f));
        g.fillRoundedRectangle (tag.expanded (1.0f).translated (0.0f, 0.6f), 2.5f);
        g.setGradientFill (real::vertical (mrc::opColour[k].brighter (0.25f), tag.getY(), mrc::opColour[k].darker (0.25f), tag.getBottom()));
        g.fillRoundedRectangle (tag, 2.0f);
    }
    mrc::section (g, { 50.0f, 364.0f, 700.0f, 240.0f }, "FILTERS");
    mrc::section (g, { 760.0f, 364.0f, 630.0f, 240.0f }, "ENVELOPES");
    mrc::section (g, { 50.0f, 614.0f, 250.0f, 222.0f }, "LFO 1");
    mrc::section (g, { 310.0f, 614.0f, 250.0f, 222.0f }, "LFO 2");
    mrc::section (g, { 570.0f, 614.0f, 190.0f, 222.0f }, "LFO 3");
    mrc::section (g, { 770.0f, 614.0f, 360.0f, 222.0f }, "MOD MATRIX");
    mrc::section (g, { 1140.0f, 614.0f, 250.0f, 222.0f }, "ARP");
    mrc::section (g, { 50.0f, 848.0f, 1100.0f, 96.0f }, "EFFECTS");

    mrc::subTitle (g, { 310.0f, 382.0f, 216.0f, 12.0f }, "FILTER 1");
    mrc::subTitle (g, { 534.0f, 382.0f, 210.0f, 12.0f }, "FILTER 2");
    // (os modos dos filtros ficam sem a legenda MODE: a lista de 7 já encosta nos knobs de baixo)
    mrc::silkText (g, "ROUTING", { 68.0f, 530.0f, 84.0f, 11.0f }, juce::Justification::centredLeft, 9.0f, true, mrc::dimOn (530.0f));
    mrc::silkText (g, "KEY MODE", { 888.0f, 27.0f, 72.0f, 11.0f }, juce::Justification::centredLeft, 9.0f, true, mrc::dimOn (27.0f));
    mrc::silkText (g, "MODE", { 1226.0f, 726.0f, 70.0f, 11.0f }, juce::Justification::centredLeft, 9.0f, true, mrc::dimOn (726.0f));
    for (int k = 0; k < 2; ++k)
        mrc::subTitle (g, { 56.0f + (float) k * 260.0f, 750.0f, 236.0f, 12.0f }, "DESTINATIONS", mrc::hotOn (750.0f));
    mrc::subTitle (g, { 778.0f, 626.0f, 344.0f, 12.0f }, "SOURCE                      DESTINATION                  AMT");
    for (int k = 0; k < numSlots; ++k)
        mrc::silkText (g, juce::String (k + 1), { 778.0f, 646.0f + (float) k * 31.0f, 16.0f, 29.0f }, juce::Justification::centred, 10.5f);

    // grupos dos efeitos
    const struct { float x, w; const char* n; } fx[] = { { 58.0f, 152.0f, "DISTORTION" }, { 218.0f, 120.0f, "ANALOG BOOST" },
                                                       { 346.0f, 120.0f, "CHORUS" }, { 474.0f, 120.0f, "PHASER" },
                                                       { 602.0f, 180.0f, "DELAY" }, { 788.0f, 180.0f, "REVERB" },
                                                       { 974.0f, 180.0f, "EQ" } };
    for (auto& f : fx)
        mrc::subTitle (g, { f.x, 866.0f, f.w - 8.0f, 12.0f }, f.n);

    // nomes embaixo dos campos de visor
    for (auto& c : captions)
        mrc::silkText (g, c.second, c.first.translated (0.0f, 1.0f).withHeight (11.0f), juce::Justification::centredTop, 9.0f, true,
                       mrc::dimOn (c.first.getY()));

    // placa do modelo: alumínio escovado gravado, com dois parafusos Allen, na faixa preta de baixo
    {
        const RF plate (1162.0f, 852.0f, 226.0f, 90.0f);
        g.setColour (real::black (0.6f));
        g.fillRoundedRectangle (plate.expanded (1.2f).translated (0.0f, 1.2f), 4.5f);
        if (art->panel.isValid())
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path pp;
            pp.addRoundedRectangle (plate, 4.0f);
            g.reduceClipRegion (pp);
            g.setOpacity (1.0f);   // a imagem herda a transparência da última cor: desenhar sempre opaca
            g.drawImage (art->panel, plate, juce::RectanglePlacement::fillDestination);
        }
        else
            g.fillRoundedRectangle (plate, 4.0f);
        g.setGradientFill (real::vertical (real::white (0.18f), plate.getY(), real::black (0.18f), plate.getBottom()));
        g.fillRoundedRectangle (plate, 4.0f);
        g.setColour (real::white (0.45f));
        g.drawRoundedRectangle (plate.reduced (0.5f), 4.0f, 0.8f);
        // gravado: o sulco escuro e a luz na borda de baixo
        const auto nameArea = plate.withTrimmedTop (16.0f).withHeight (28.0f);
        g.setFont (theme::sans (22.0f, true, 0.30f));
        g.setColour (real::white (0.5f));
        g.drawText (u8 ("MERCÚRIO"), nameArea.translated (0.0f, 0.9f), juce::Justification::centred, false);
        g.setColour (mrc::print);
        g.drawText (u8 ("MERCÚRIO"), nameArea, juce::Justification::centred, false);
        const RF rule (plate.getCentreX() - 56.0f, plate.getY() + 50.0f, 112.0f, 2.0f);
        g.setGradientFill (mrc::iridescent (rule, 1.0f));
        g.fillRect (rule);
        mrc::silkText (g, u8 ("16 VOICES  ·  4 OPERATORS  ·  8 ALGORITHMS"), plate.withTrimmedTop (57.0f).withHeight (12.0f),
                       juce::Justification::centred, 7.5f, true, mrc::printDim, 0.12f);
        g.setOpacity (1.0f);   // a imagem herda a transparência da última cor: desenhar sempre opaca
        if (art->screw.isValid())
            for (float sx : { plate.getX() + 12.0f, plate.getRight() - 12.0f })
                g.drawImage (art->screw, RF (sx - 8.0f, plate.getCentreY() - 8.0f, 16.0f, 16.0f), juce::RectanglePlacement::stretchToFit);
    }

    // LEDs dos LFOs
    const float lv[3] = { proc.synth.lfo1Now.load(), proc.synth.lfo2Now.load(), proc.synth.lfo3Now.load() };
    for (int k = 0; k < 3; ++k)
        hw::led (g, lfoLedPos[k], 2.6f, mrc::hot, 0.5f + 0.5f * lv[k]);

    // estado: MIDI, SYNC e BPM
    hw::led (g, midiLedPos, 2.6f, mrc::hot, midiLit ? 1.0f : 0.0f);
    mrc::silkText (g, "MIDI", { midiLedPos.x + 7.0f, midiLedPos.y - 6.0f, 44.0f, 12.0f }, juce::Justification::centredLeft, 9.0f);
    hw::led (g, syncLedPos, 2.6f, mrc::hot, lastSync ? 1.0f : 0.0f);
    mrc::silkText (g, "SYNC", { syncLedPos.x + 7.0f, syncLedPos.y - 6.0f, 44.0f, 12.0f }, juce::Justification::centredLeft, 9.0f);
    mrc::silkText (g, juce::String (proc.shownBpm.load(), 1) + " BPM", { 168.0f, 82.0f, 78.0f, 12.0f },
                   juce::Justification::centredLeft, 9.0f, true, mrc::silkDim);

    drawMeter (g);
}

// medidor de saída em LEDs (pico), dois canais
void MercurioEditor::drawMeter (juce::Graphics& g)
{
    const int leds = 11;
    const float levels[2] = { levelL, levelR };
    for (int c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (levels[c], -60.0f);
        const float v  = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -42.0f, 0.0f, 0.0f, 1.0f));
        for (int i = 0; i < leds; ++i)
        {
            const float f = (float) i / (float) (leds - 1);
            const auto col = f > 0.85f ? mrc::hot : f > 0.6f ? theme::amber : theme::leaf;
            real::ledBar (g, { (float) meterArea.getX() + 8.0f + (float) c * 14.0f, (float) meterArea.getBottom() - 18.0f - (float) i * 6.2f, 9.0f, 3.6f },
                          col, levels[c] > 0.0005f && f <= v ? 1.0f : 0.0f, 0.4f);
        }
    }
    mrc::silkText (g, "L   R", { (float) meterArea.getX(), (float) meterArea.getBottom() - 12.0f, (float) meterArea.getWidth(), 10.0f },
                   juce::Justification::centred, 7.0f);
}
