#include "PluginEditor.h"
#include <complex>

using namespace vacina;
using vac::RF;

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
    vac::glassUnder (g, r, 3.0f);
    const auto in = r.reduced (12.0f, 7.0f);
    vac::oledText (g, l1, in.withHeight (13.0f), 11.5f, juce::Justification::centredLeft, vac::inkDim);
    vac::oledText (g, l1r, in.withHeight (13.0f), 11.5f, juce::Justification::centredRight, vac::inkDim);
    g.setColour (vac::inkDim.withAlpha (0.35f));
    g.fillRect (in.getX(), in.getY() + 15.0f, in.getWidth(), 0.7f);
    vac::oledText (g, l2, in.withTrimmedTop (17.0f).withHeight (30.0f), 23.0f, juce::Justification::centredLeft, vac::ink, false);
    vac::oledText (g, l3, in.withTrimmedTop (48.0f).withHeight (15.0f), 12.0f, juce::Justification::centredLeft, vac::redInk);
    vac::glassOver (g, r, 3.0f);
}

// =====================================================================
// EDITOR
// =====================================================================
vac::VKnob* VacinaEditor::knob (int cx, int y, int size, const juce::String& pid, const juce::String& label, juce::Colour pointer)
{
    // largo para o nome não cortar (SUPER SPREAD); o VKnob não pega clique fora do knob, então pode sobrepor o vizinho
    const int w = juce::jmax (size + 14, 92);
    return add<vac::VKnob> ({ cx - w / 2, y, w, size + 10 + (label.isNotEmpty() ? 12 : 0) }, proc.apvts, pid, label, size, pointer);
}

vac::Key* VacinaEditor::key (int cx, int y, const juce::String& label, bool led)
{
    const int w = 64 + (led ? 20 : 0);
    return add<vac::Key> ({ cx - 32, y, w, label.isNotEmpty() ? 32 : 20 }, label, led);
}

vac::LcdBox* VacinaEditor::box (juce::Rectangle<int> r, const juce::String& pid, const juce::String& caption)
{
    if (caption.isNotEmpty())
        captions.push_back ({ RF ((float) r.getX(), (float) r.getBottom(), (float) r.getWidth(), 10.0f), caption });
    return add<vac::LcdBox> (r, proc.apvts, pid);
}

void VacinaEditor::attach (juce::Button& b, const juce::String& paramId)
{
    b.setClickingTogglesState (true);
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramId, b));
}

vac::Scope* VacinaEditor::scope (juce::Rectangle<int> r)
{
    auto* s = add<vac::Scope> (r);
    s->setInterceptsMouseClicks (false, false);
    scopes.push_back (s);
    return s;
}

VacinaEditor::VacinaEditor (VacinaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, vac::baseW, vac::baseH);
    addAndMakeVisible (canvas);

    auto& st = proc.apvts;
    const auto red = vac::red;

    // ================= CABEÇALHO: presets, VOICE, MASTER =================
    lcd = add<LcdDisplay> ({ 336, 20, 470, 84 });
    lcd->onClick = [this] { showPresetMenu(); };
    key (296, 26, "BROWSE")->onClick = [this] { showPresetMenu(); };
    key (296, 64, "STORE")->onClick = [this] { askNewPreset(); };
    key (840, 26, "PREV")->onClick = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    key (840, 64, "NEXT")->onClick = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };

    add<vac::LedList> ({ 888, 40, 72, 45 }, keyModeNames, true, 15.0f)->bind (st, id::keyMode);
    knob (990, 38, 30, id::porta, "GLIDE");
    knob (1042, 38, 30, id::bendRange, "BEND");
    knob (1094, 38, 30, id::unison, "UNISON");
    knob (1146, 38, 30, id::uniDetune, "DETUNE");
    knob (1198, 38, 30, id::uniSpread, "SPREAD");
    knob (1250, 38, 30, id::drift, "ANALOG");
    knob (1314, 28, 46, id::volume, "MASTER");

    // ================= OSCILADORES =================
    oscScopes[0] = scope ({ 60, 136, 170, 94 });
    add<vac::LedList> ({ 242, 140, 84, 45 }, model1Names, true, 15.0f)->bind (st, id::osc1Model);
    wave1Box = box ({ 238, 192, 156, 28 }, id::osc1Table, "WAVETABLE");
    knob (92, 244, 44, id::osc1Shape, "SHAPE", red);
    knob (168, 244, 44, id::osc1Pw, "PULSE W");
    knob (244, 244, 44, id::osc1Semi, "SEMITONE");
    knob (320, 244, 44, id::osc1Spread, "SUPER SPREAD");

    oscScopes[1] = scope ({ 422, 136, 170, 94 });
    add<vac::LedList> ({ 604, 140, 84, 30 }, model2Names, true, 15.0f)->bind (st, id::osc2Model);
    attach (*key (724, 138, "SYNC", true), id::osc2Sync);
    wave2Box = box ({ 600, 192, 204, 28 }, id::osc2Table, "WAVETABLE");
    knob (450, 244, 44, id::osc2Shape, "SHAPE", red);
    knob (524, 244, 44, id::osc2Pw, "PULSE W");
    knob (598, 244, 44, id::osc2Semi, "SEMITONE");
    knob (672, 244, 44, id::osc2Detune, "DETUNE");
    knob (750, 244, 44, id::fmAmt, "FM 1 > 2", red);

    oscScopes[2] = scope ({ 832, 136, 178, 94 });
    knob (862, 244, 40, id::osc3Shape, "SHAPE", red);
    knob (921, 244, 40, id::osc3Semi, "SEMITONE");
    knob (980, 244, 40, id::osc3Detune, "DETUNE");

    knob (1074, 138, 44, id::osc1Vol, "OSC 1");
    knob (1160, 138, 44, id::osc2Vol, "OSC 2");
    knob (1246, 138, 44, id::osc3Vol, "OSC 3");
    knob (1332, 138, 44, id::subVol, "SUB");
    knob (1074, 244, 44, id::noiseVol, "NOISE");
    knob (1160, 244, 44, id::noiseColor, "NOISE COLOR");
    knob (1246, 244, 44, id::ringVol, "RING MOD");
    knob (1332, 244, 44, id::oscVol, "DRIVE", red);

    // ================= FILTROS =================
    filterScope = scope ({ 60, 384, 238, 104 });
    add<vac::LedList> ({ 68, 498, 96, 30 }, routingNames, true, 15.0f)->bind (st, id::routing);
    knob (196, 494, 34, id::filtBal, "BALANCE");
    box ({ 64, 550, 120, 28 }, id::satType, "SATURATION");
    knob (240, 544, 34, id::satAmt, "SAT DRIVE", red);

    add<vac::LedList> ({ 312, 398, 74, 105 }, f1ModeNames, true, 15.0f)->bind (st, id::f1Mode);
    knob (456, 398, 66, id::cutoff, "CUTOFF", red);
    knob (340, 512, 36, id::reso1, "RESONANCE");
    knob (412, 512, 36, id::env1, "ENV AMOUNT");
    knob (484, 512, 36, id::key1, "KEY TRACK");

    add<vac::LedList> ({ 538, 398, 74, 90 }, f2ModeNames, true, 15.0f)->bind (st, id::f2Mode);
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
    knob (198, 768, 30, id::lfo1Pw, "PULSE W");
    knob (256, 768, 30, id::lfo1Amp, "AMP");
    knob (342, 768, 30, id::lfo2Shp, "SHAPE");
    knob (400, 768, 30, id::lfo2Fm, "FM");
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
    add<vac::LedList> ({ 1226, 634, 88, 90 }, arpModeNames, true, 15.0f)->bind (st, id::arpMode);
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
    setResizeLimits (juce::roundToInt ((float) vac::baseW * vac::minScale), juce::roundToInt ((float) vac::baseH * vac::minScale),
                     juce::roundToInt ((float) vac::baseW * vac::maxScale), juce::roundToInt ((float) vac::baseH * vac::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) vac::baseW / (double) vac::baseH);
    setScale (openScale);
    startTimerHz (30);
}

VacinaEditor::~VacinaEditor()
{
    dialog.close();
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------- o que cada visor desenha ----------------
void VacinaEditor::buildScopes()
{
    // ---- osciladores: um ciclo da forma de onda atual ----
    for (int o = 0; o < 3; ++o)
    {
        auto* s = oscScopes[(size_t) o];
        const juce::String shp = o == 0 ? id::osc1Shape : o == 1 ? id::osc2Shape : id::osc3Shape;
        auto modelOf = [this, o] { return o == 0 ? juce::roundToInt (pv (id::osc1Model)) : o == 1 ? juce::roundToInt (pv (id::osc2Model)) * 2 : 0; };
        auto tableOf = [this, o] { return juce::roundToInt (pv (o == 0 ? id::osc1Table : id::osc2Table)); };
        s->hash = [this, o, shp, modelOf, tableOf]
        {
            const float pw = o == 0 ? pv (id::osc1Pw) : o == 1 ? pv (id::osc2Pw) : 50.0f;
            return pv (shp) + pw * 0.37f + (float) modelOf() * 1000.0f + (float) tableOf() * 10000.0f
                 + (o == 0 ? pv (id::osc1Spread) * 0.013f : 0.0f);
        };
        s->caption = [o, modelOf, tableOf]
        {
            const int m = modelOf();
            const juce::String n = "OSC " + juce::String (o + 1) + "  ";
            if (m == 2) return n + tableNames[tableOf()];
            return n + (m == 1 ? "SUPER" : "CLASSIC");
        };
        s->draw = [this, o, shp, modelOf, tableOf] (juce::Graphics& g, RF r)
        {
            vac::gridLines (g, r, 4, 2);
            const int m = modelOf();
            const float shape = pv (shp) * 0.01f;
            const float pw = (o == 0 ? pv (id::osc1Pw) : o == 1 ? pv (id::osc2Pw) : 50.0f) * 0.01f;
            const int table = tableOf();
            const auto& bank = WaveBank::instance();
            auto sample = [&] (float t, float detune)
            {
                t += detune;
                t -= std::floor (t);
                if (m == 2) return bank.read (table, shape, t, 0);
                if (m == 1) return 2.0f * t - 1.0f;
                return Voice::classic (t, 0.0f, shape, pw);
            };
            const int n = 240;
            auto trace = [&] (float detune)
            {
                juce::Path p;
                for (int i = 0; i <= n; ++i)
                {
                    const float t = (float) i / (float) n;
                    const float y = r.getCentreY() - juce::jlimit (-1.1f, 1.1f, sample (t * 2.0f, detune)) * r.getHeight() * 0.40f;
                    const float x = r.getX() + t * r.getWidth();
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                return p;
            };
            if (m == 1)   // SUPER: as serras vizinhas defasadas, mais fracas
            {
                const float spread = pv (id::osc1Spread) * 0.01f;
                for (float d : { -0.11f, 0.07f, 0.19f })
                    vac::glowPath (g, trace (d * (0.3f + spread)), vac::ink.withAlpha (0.35f), 1.0f);
            }
            vac::glowPath (g, trace (0.0f), vac::ink, 1.5f);
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
        vac::gridLines (g, r, 6, 4);
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
        g.setGradientFill (real::vertical (vac::ink.withAlpha (0.16f), r.getY(), vac::ink.withAlpha (0.0f), r.getBottom()));
        g.fillPath (fill);
        vac::glowPath (g, p, vac::ink, 1.5f);
        const float xc = r.getX() + std::log10 (juce::jlimit (20.0f, 20000.0f, fc1) / 20.0f) / 3.0f * r.getWidth();
        g.setColour (vac::red.withAlpha (0.8f));
        g.fillRect (xc - 0.5f, r.getY(), 1.0f, r.getHeight());
        const float hz = fc1;
        vac::oledText (g, hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz",
                       r.withTrimmedTop (r.getHeight() - 14.0f), 10.5f, juce::Justification::bottomRight, vac::redInk);
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
            vac::gridLines (g, r, 4, 2);
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
            g.setGradientFill (real::vertical (vac::ink.withAlpha (0.14f), r.getY(), vac::ink.withAlpha (0.0f), r.getBottom()));
            g.fillPath (fill);
            vac::glowPath (g, p, vac::ink, 1.4f);
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
            vac::gridLines (g, r, 4, 2);
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
            vac::glowPath (g, p, vac::ink, 1.4f);
        };
    }
}

// ---------------- menu de presets (com o tamanho da janela) ----------------
void VacinaEditor::showPresetMenu()
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
        else if (b == VacinaProcessor::bankUser)
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

    // bancos avulsos (.vcnbank)
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
    menu.addItem (sizeMenuBase - 1, juce::String ("VACINA ") + JucePlugin_VersionString, false, false);
   #endif

    // o menu (e cada camada: banco → categoria → preset) abre DENTRO da janela do plugin: como janela solta,
    // a camada de dentro ficava por baixo da janela do plugin (que o host deixa sempre na frente) e não dava clique
    juce::Component::SafePointer<VacinaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lcd).withParentComponent (this), [safe] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->presetMenuChosen (result);
    });
}

void VacinaEditor::askNewPreset()
{
    juce::Component::SafePointer<VacinaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

// abre o navegador de arquivos; o banco escolhido é copiado para a pasta do VACINA e entra no menu
void VacinaEditor::importBank()
{
    auto start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    if (start.getChildFile ("Downloads").isDirectory()) start = start.getChildFile ("Downloads");
    chooser = std::make_unique<juce::FileChooser> ("Importar banco de presets do VACINA", start, "*.vcnbank");

    juce::Component::SafePointer<VacinaEditor> safe (this);
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

void VacinaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<VacinaEditor> safe (this);
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
void VacinaEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) vac::baseW - openScale) > 0.005f)
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
    // o campo WAVETABLE só fica aceso com o oscilador em WAVE
    wave1Box->setDim (juce::roundToInt (pv (id::osc1Model)) != 2);
    wave2Box->setDim (juce::roundToInt (pv (id::osc2Model)) != 1);

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
void VacinaEditor::setScale (float scale)
{
    scale = juce::jlimit (vac::minScale, vac::maxScale, scale);
    setSize (juce::roundToInt ((float) vac::baseW * scale), juce::roundToInt ((float) vac::baseH * scale));
}

void VacinaEditor::resized()
{
    const float scale = (float) getWidth() / (float) vac::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void VacinaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0e));
}

// ---------------- o aparelho ----------------
// lateral de madeira (mogno envernizado) — a mesma de antes
static void woodCheek (juce::Graphics& g, RF r, bool left)
{
    using juce::Colour;
    juce::Path p;
    p.addRoundedRectangle (r, 9.0f);
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (real::black (0.12f));
        g.fillRoundedRectangle (r.expanded ((float) i * 0.6f).translated (left ? -1.0f : 1.0f, (float) i), 9.0f + (float) i);
    }
    juce::ColourGradient cg (Colour (0xff2c0d08), r.getX(), 0.0f, Colour (0xff2c0d08), r.getRight(), 0.0f, false);
    cg.addColour (left ? 0.22 : 0.18, Colour (0xff6d2416));
    cg.addColour (left ? 0.5 : 0.45, Colour (0xff8a3320));
    cg.addColour (left ? 0.8 : 0.78, Colour (0xff5e1e12));
    g.setGradientFill (cg);
    g.fillPath (p);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (p);
        juce::Random rnd (left ? 77 : 131);
        for (int i = 0; i < 90; ++i)
        {
            const float x0 = r.getX() + rnd.nextFloat() * r.getWidth();
            const float amp = 0.4f + rnd.nextFloat() * 2.2f, freq = 0.006f + rnd.nextFloat() * 0.02f, ph = rnd.nextFloat() * 6.28f;
            juce::Path vein;
            for (float y = r.getY() - 4.0f; y <= r.getBottom() + 4.0f; y += 6.0f)
            {
                const float x = x0 + amp * std::sin (y * freq + ph) + 0.7f * std::sin (y * freq * 3.7f + ph * 2.0f);
                if (y <= r.getY() - 4.0f) vein.startNewSubPath (x, y); else vein.lineTo (x, y);
            }
            const bool dark = rnd.nextFloat() < 0.7f;
            g.setColour (dark ? Colour (0xff1a0603).withAlpha (0.10f + rnd.nextFloat() * 0.22f)
                              : Colour (0xffc0603f).withAlpha (0.05f + rnd.nextFloat() * 0.10f));
            g.strokePath (vein, juce::PathStrokeType (0.4f + rnd.nextFloat() * 1.3f));
        }
        for (int i = 0; i < 360; ++i)
        {
            const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
            g.setColour (Colour (0xff140402).withAlpha (0.18f + rnd.nextFloat() * 0.25f));
            g.fillRect (x, y, 0.7f, 1.5f + rnd.nextFloat() * 3.0f);
        }
        const float hx = r.getX() + r.getWidth() * (left ? 0.42f : 0.38f);
        juce::ColourGradient hl (real::white (0.0f), hx - 7.0f, 0.0f, real::white (0.0f), hx + 7.0f, 0.0f, false);
        hl.addColour (0.5, real::white (0.16f));
        g.setGradientFill (hl);
        g.fillRect (r);
        g.setGradientFill (real::vertical (real::white (0.14f), r.getY(), real::white (0.0f), r.getY() + 40.0f));
        g.fillRect (r);
        g.setGradientFill (real::vertical (real::black (0.0f), r.getBottom() - 50.0f, real::black (0.35f), r.getBottom()));
        g.fillRect (r);
    }
    auto bevel = real::vertical (Colour (0xffd88a66).withAlpha (0.5f), r.getY(), real::black (0.7f), r.getBottom());
    bevel.addColour (0.05, real::white (0.06f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (r.reduced (0.6f), 9.0f, 1.2f);
}

// logo: frasco de vacina (tampa, gargalo e o líquido com a gota) + o nome
void VacinaEditor::drawLogo (juce::Graphics& g, float x, float y)
{
    const float s = 1.6f;
    const auto c = vac::silk;
    juce::Path vial;
    vial.addRoundedRectangle (x + 2.0f * s, y + 6.0f * s, 12.0f * s, 16.0f * s, 2.5f * s);
    g.setColour (c);
    g.strokePath (vial, juce::PathStrokeType (1.4f * s));
    g.fillRoundedRectangle (x + 1.0f * s, y, 14.0f * s, 4.5f * s, 1.0f * s);
    g.fillRect (x + 4.0f * s, y + 4.0f * s, 8.0f * s, 2.5f * s);
    g.setColour (vac::red);
    g.fillRoundedRectangle (x + 3.6f * s, y + 13.0f * s, 8.8f * s, 7.4f * s, 1.5f * s);
    juce::Path drop;
    drop.startNewSubPath (x + 8.0f * s, y + 8.0f * s);
    drop.quadraticTo (x + 10.5f * s, y + 11.5f * s, x + 8.0f * s, y + 12.4f * s);
    drop.quadraticTo (x + 5.5f * s, y + 11.5f * s, x + 8.0f * s, y + 8.0f * s);
    g.fillPath (drop);

    g.setFont (theme::sans (30.0f, true, 0.10f));
    g.setColour (c);
    g.drawText ("VACINA", RF (x + 34.0f, y - 2.0f, 180.0f, 32.0f), juce::Justification::centredLeft, false);
    vac::silkText (g, "VIRTUAL ANALOG SYNTHESIZER", { x + 35.0f, y + 29.0f, 190.0f, 10.0f }, juce::Justification::centredLeft,
                   7.5f, true, vac::silkDim, 0.18f);
}

void VacinaEditor::drawCanvas (juce::Graphics& g)
{
    const float W = (float) vac::baseW, H = (float) vac::baseH;
    g.fillAll (juce::Colour (0xff0d0d0e));

    // chapa (imagem renderizada) entre as duas laterais de madeira
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
            g.drawImage (art->panel, panel, juce::RectanglePlacement::stretchToFit);
        }
        else
            g.fillAll (juce::Colour (0xff2a2b2e));
    }
    auto bevel = real::vertical (real::white (0.22f), panel.getY(), real::black (0.7f), panel.getBottom());
    bevel.addColour (0.03, real::white (0.05f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (panel.reduced (0.5f), 4.0f, 1.4f);
    woodCheek (g, { 2.0f, 2.0f, 40.0f, H - 4.0f }, true);
    woodCheek (g, { W - 42.0f, 2.0f, 40.0f, H - 4.0f }, false);

    drawLogo (g, 56.0f, 24.0f);

    // ---- seções ----
    vac::section (g, { 876.0f, 16.0f, 404.0f, 92.0f }, "VOICE");
    vac::section (g, { 50.0f, 116.0f, 352.0f, 238.0f }, "OSC 1");
    vac::section (g, { 412.0f, 116.0f, 400.0f, 238.0f }, "OSC 2");
    vac::section (g, { 822.0f, 116.0f, 196.0f, 238.0f }, "OSC 3");
    vac::section (g, { 1028.0f, 116.0f, 362.0f, 238.0f }, "MIXER");
    vac::section (g, { 50.0f, 364.0f, 700.0f, 240.0f }, "FILTERS");
    vac::section (g, { 760.0f, 364.0f, 630.0f, 240.0f }, "ENVELOPES");
    vac::section (g, { 50.0f, 614.0f, 250.0f, 224.0f }, "LFO 1");
    vac::section (g, { 310.0f, 614.0f, 250.0f, 224.0f }, "LFO 2");
    vac::section (g, { 570.0f, 614.0f, 190.0f, 224.0f }, "LFO 3");
    vac::section (g, { 770.0f, 614.0f, 360.0f, 224.0f }, "MOD MATRIX");
    vac::section (g, { 1140.0f, 614.0f, 250.0f, 224.0f }, "ARP");
    vac::section (g, { 50.0f, 848.0f, 1100.0f, 96.0f }, "EFFECTS");

    vac::subTitle (g, { 310.0f, 382.0f, 216.0f, 12.0f }, "FILTER 1");
    vac::subTitle (g, { 534.0f, 382.0f, 210.0f, 12.0f }, "FILTER 2");
    // (os modos dos filtros ficam sem a legenda MODE: a lista de 7 já encosta nos knobs de baixo)
    vac::silkText (g, "ROUTING", { 68.0f, 530.0f, 84.0f, 11.0f }, juce::Justification::centredLeft, 9.0f, true, vac::silkDim);
    vac::silkText (g, "MODEL", { 242.0f, 126.0f, 74.0f, 11.0f }, juce::Justification::centredLeft, 9.0f, true, vac::silkDim);
    vac::silkText (g, "MODEL", { 604.0f, 126.0f, 74.0f, 11.0f }, juce::Justification::centredLeft, 9.0f, true, vac::silkDim);
    vac::silkText (g, "KEY MODE", { 888.0f, 27.0f, 72.0f, 11.0f }, juce::Justification::centredLeft, 9.0f, true, vac::silkDim);
    vac::silkText (g, "MODE", { 1226.0f, 726.0f, 70.0f, 11.0f }, juce::Justification::centredLeft, 9.0f, true, vac::silkDim);
    for (int k = 0; k < 2; ++k)
    {
        vac::subTitle (g, { 56.0f + (float) k * 260.0f, 750.0f, 236.0f, 12.0f }, "DESTINATIONS", vac::redInk);
    }
    vac::subTitle (g, { 778.0f, 626.0f, 344.0f, 12.0f }, "SOURCE                      DESTINATION                  AMT");
    for (int k = 0; k < numSlots; ++k)
        vac::silkText (g, juce::String (k + 1), { 778.0f, 646.0f + (float) k * 31.0f, 16.0f, 29.0f }, juce::Justification::centred, 10.5f);

    // grupos dos efeitos
    const struct { float x, w; const char* n; } fx[] = { { 58.0f, 152.0f, "DISTORTION" }, { 218.0f, 120.0f, "ANALOG BOOST" },
                                                       { 346.0f, 120.0f, "CHORUS" }, { 474.0f, 120.0f, "PHASER" },
                                                       { 602.0f, 180.0f, "DELAY" }, { 788.0f, 180.0f, "REVERB" },
                                                       { 974.0f, 180.0f, "EQ" } };
    for (auto& f : fx)
        vac::subTitle (g, { f.x, 866.0f, f.w - 8.0f, 12.0f }, f.n);

    // nomes embaixo dos campos de visor
    for (auto& c : captions)
        vac::silkText (g, c.second, c.first.translated (0.0f, 1.0f).withHeight (11.0f), juce::Justification::centredTop, 9.0f, true, vac::silkDim);

    // placa do modelo (com dois parafusos) no canto de baixo
    {
        const RF plate (1162.0f, 852.0f, 226.0f, 90.0f);
        g.setGradientFill (real::vertical (juce::Colour (0xff3a3b3f), plate.getY(), juce::Colour (0xff1b1c1e), plate.getBottom()));
        g.fillRoundedRectangle (plate, 4.0f);
        g.setColour (real::white (0.10f));
        g.drawRoundedRectangle (plate.reduced (0.5f), 4.0f, 0.8f);
        g.setColour (real::black (0.6f));
        g.drawRoundedRectangle (plate.expanded (0.8f).translated (0.0f, 0.8f), 4.5f, 0.8f);
        g.setFont (theme::sans (22.0f, true, 0.32f));
        g.setColour (vac::silk);
        g.drawText ("VACINA", plate.withTrimmedTop (16.0f).withHeight (28.0f), juce::Justification::centred, false);
        g.setColour (vac::red);
        g.fillRect (plate.getCentreX() - 40.0f, plate.getY() + 50.0f, 80.0f, 1.2f);
        vac::silkText (g, u8 ("16 VOICES  ·  3 OSC  ·  24 WAVETABLES"), plate.withTrimmedTop (56.0f).withHeight (12.0f),
                       juce::Justification::centred, 7.5f, true, vac::silkDim, 0.12f);
        if (art->screw.isValid())
            for (float sx : { plate.getX() + 12.0f, plate.getRight() - 12.0f })
                g.drawImage (art->screw, RF (sx - 8.0f, plate.getCentreY() - 8.0f, 16.0f, 16.0f), juce::RectanglePlacement::stretchToFit);
    }

    // LEDs dos LFOs
    const float lv[3] = { proc.synth.lfo1Now.load(), proc.synth.lfo2Now.load(), proc.synth.lfo3Now.load() };
    for (int k = 0; k < 3; ++k)
        real::led (g, lfoLedPos[k], 2.6f, vac::red, 0.5f + 0.5f * lv[k]);

    // estado: MIDI, SYNC e BPM
    real::led (g, midiLedPos, 2.6f, vac::red, midiLit ? 1.0f : 0.0f);
    vac::silkText (g, "MIDI", { midiLedPos.x + 7.0f, midiLedPos.y - 6.0f, 44.0f, 12.0f }, juce::Justification::centredLeft, 9.0f);
    real::led (g, syncLedPos, 2.6f, vac::red, lastSync ? 1.0f : 0.0f);
    vac::silkText (g, "SYNC", { syncLedPos.x + 7.0f, syncLedPos.y - 6.0f, 44.0f, 12.0f }, juce::Justification::centredLeft, 9.0f);
    vac::silkText (g, juce::String (proc.shownBpm.load(), 1) + " BPM", { 168.0f, 82.0f, 78.0f, 12.0f },
                   juce::Justification::centredLeft, 9.0f, true, vac::silkDim);

    drawMeter (g);
}

// medidor de saída em LEDs (pico), dois canais
void VacinaEditor::drawMeter (juce::Graphics& g)
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
            const auto col = f > 0.85f ? vac::red : f > 0.6f ? theme::amber : theme::leaf;
            real::ledBar (g, { (float) meterArea.getX() + 8.0f + (float) c * 14.0f, (float) meterArea.getBottom() - 18.0f - (float) i * 6.2f, 9.0f, 3.6f },
                          col, levels[c] > 0.0005f && f <= v ? 1.0f : 0.0f, 0.4f);
        }
    }
    vac::silkText (g, "L   R", { (float) meterArea.getX(), (float) meterArea.getBottom() - 12.0f, (float) meterArea.getWidth(), 10.0f },
                   juce::Justification::centred, 7.0f);
}
