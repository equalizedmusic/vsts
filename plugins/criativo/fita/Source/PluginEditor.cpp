#include "PluginEditor.h"

using namespace fita;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004;

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
    real::card (g, r);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

juce::String paramText (FitaProcessor& p, const char* pid)
{
    return p.apvts.getParameter (pid)->getCurrentValueAsText().toUpperCase();
}

int choiceIndex (FitaProcessor& p, const char* pid)
{
    return juce::roundToInt (p.apvts.getRawParameterValue (pid)->load());
}

juce::String machineText (FitaProcessor& p)
{
    const int m = choiceIndex (p, id::model), sp = choiceIndex (p, id::speed);
    return u8 ("· ") + paramText (p, id::model) + u8 (" · ") + ipsText (tapeIps (m, sp)) + " IPS";
}

// ---------------- VU de ponteiro ----------------
// escala clássica: posição proporcional à tensão; 0 VU fica a ~70% do arco
float vuPos (float db)
{
    return (std::pow (10.0f, db / 20.0f) - 0.1f) / (std::pow (10.0f, 3.0f / 20.0f) - 0.1f);
}

void drawVu (juce::Graphics& g, juce::Rectangle<float> r, float needleDb, const juce::String& label)
{
    real::screen (g, r, 4.0f);
    const auto face = r.reduced (5.0f);
    g.setColour (theme::cream.darker (0.12f));
    g.fillRoundedRectangle (face, 3.0f);

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (face.toNearestInt());

    const float cx = face.getCentreX(), pivotY = face.getBottom() + face.getHeight() * 0.35f;
    const float R = face.getHeight() * 1.05f;
    const float span = juce::degreesToRadians (42.0f);
    auto angleOf = [span] (float db) { return -span + 2.0f * span * vuPos (db); };
    auto pt = [cx, pivotY] (float rad, float a) { return juce::Point<float> (cx + rad * std::sin (a), pivotY - rad * std::cos (a)); };

    juce::Path arc;
    arc.addCentredArc (cx, pivotY, R, R, 0.0f, angleOf (-20.0f), angleOf (0.0f), true);
    g.setColour (theme::dark.withAlpha (0.8f));
    g.strokePath (arc, juce::PathStrokeType (1.2f));
    juce::Path red;
    red.addCentredArc (cx, pivotY, R, R, 0.0f, angleOf (0.0f), angleOf (3.0f), true);
    g.setColour (theme::coral);
    g.strokePath (red, juce::PathStrokeType (3.0f));

    g.setFont (theme::mono (8.5f));
    for (float db : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = angleOf (db);
        g.setColour (db > 0.0f ? theme::coral : theme::dark);
        g.drawLine (juce::Line<float> (pt (R, a), pt (R + 6.0f, a)), 1.2f);
        if (db == -20.0f || db == -10.0f || db == -5.0f || db == -3.0f || db == 0.0f || db == 3.0f)
        {
            const auto p = pt (R - 10.0f, a);
            g.drawText (db > 0.0f ? "+" + juce::String ((int) db) : juce::String ((int) db),
                        juce::Rectangle<float> (24.0f, 10.0f).withCentre (p), juce::Justification::centred);
        }
    }

    g.setColour (theme::dark);
    g.setFont (theme::sans (13.0f, true, 0.1f));
    g.drawText ("VU", juce::Rectangle<float> (cx - 30.0f, face.getY() + face.getHeight() * 0.48f, 60.0f, 16.0f),
                juce::Justification::centred);
    g.setFont (theme::mono (8.0f));
    g.drawText ("dBFS", juce::Rectangle<float> (cx - 30.0f, face.getY() + face.getHeight() * 0.48f + 15.0f, 60.0f, 10.0f),
                juce::Justification::centred);
    g.setFont (theme::sans (9.0f, true, 0.1f));
    g.drawText (label, juce::Rectangle<float> (face.getX() + 6.0f, face.getBottom() - 15.0f, 20.0f, 12.0f),
                juce::Justification::centredLeft);

    const float a = angleOf (juce::jlimit (-23.0f, 4.0f, needleDb));
    g.setColour (theme::dark);
    g.drawLine (juce::Line<float> (pt (R * 0.35f, a), pt (R + 8.0f, a)), 1.4f);

    // vidro sobre o mostrador (a moldura escura em volta é o real::screen acima)
    real::glass (g, face, 3.0f);
}

// ---------------- máquina ----------------
// ponto onde a fita sai do carretel (c, r) em direção a p; side escolhe o lado de fora
juce::Point<float> tangent (juce::Point<float> c, float r, juce::Point<float> p, float side)
{
    const auto  d    = p - c;
    const float dist = juce::jmax (r + 1.0f, d.getDistanceFromOrigin());
    const float a    = std::atan2 (d.y, d.x) + side * std::acos (r / dist);
    return { c.x + r * std::cos (a), c.y + r * std::sin (a) };
}

void drawPack (juce::Graphics& g, juce::Point<float> c, float pack, float hub)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e1f12), c.x, c.y, juce::Colour (0xff6a4a2c), c.x + pack, c.y, true));
    g.fillEllipse (c.x - pack, c.y - pack, pack * 2.0f, pack * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float rr = hub + 4.0f; rr < pack; rr += 5.0f)
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.6f);
}

void drawReel (juce::Graphics& g, juce::Point<float> c, float R, float pack, float hub, float ang)
{
    const float twoPi = juce::MathConstants<float>::twoPi;
    drawPack (g, c, pack, hub);

    // flange fumê com 3 janelas vazadas: dá para ver a fita por dentro
    juce::Path fl;
    fl.addEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    const float rin = hub + 6.0f, rout = R - 6.0f;
    for (int k = 0; k < 3; ++k)
    {
        const float a = ang + (float) k * twoPi / 3.0f;
        fl.addPieSegment (c.x - rout, c.y - rout, rout * 2.0f, rout * 2.0f, a - 0.6f, a + 0.6f, rin / rout);
    }
    fl.setUsingNonZeroWinding (false);
    g.setColour (theme::dim.withAlpha (0.55f));
    g.fillPath (fl);
    g.setColour (theme::cream.withAlpha (0.25f));
    g.drawEllipse (c.x - R + 0.5f, c.y - R + 0.5f, R * 2.0f - 1.0f, R * 2.0f - 1.0f, 1.0f);

    // cubo com 3 raios
    g.setColour (theme::panel2);
    g.fillEllipse (c.x - hub, c.y - hub, hub * 2.0f, hub * 2.0f);
    g.setColour (theme::cream.withAlpha (0.8f));
    for (int k = 0; k < 3; ++k)
    {
        const float a = ang + ((float) k + 0.5f) * twoPi / 3.0f;
        g.drawLine (c.x + 4.0f * std::sin (a), c.y - 4.0f * std::cos (a),
                    c.x + (hub - 2.0f) * std::sin (a), c.y - (hub - 2.0f) * std::cos (a), 2.0f);
    }
    g.setColour (theme::amber);
    g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);

    // marca na borda: deixa o giro (e o wow) visível
    const float mx = c.x + (R - 3.5f) * std::sin (ang), my = c.y - (R - 3.5f) * std::cos (ang);
    g.setColour (theme::amber.withAlpha (0.9f));
    g.fillEllipse (mx - 2.0f, my - 2.0f, 4.0f, 4.0f);
}

void drawCassetteHub (juce::Graphics& g, juce::Point<float> c, float ang)
{
    g.setColour (theme::txt.withAlpha (0.85f));
    g.fillEllipse (c.x - 11.0f, c.y - 11.0f, 22.0f, 22.0f);
    g.setColour (theme::dark);
    g.fillEllipse (c.x - 7.5f, c.y - 7.5f, 15.0f, 15.0f);
    g.setColour (theme::txt.withAlpha (0.85f));
    for (int k = 0; k < 6; ++k)
    {
        const float a = ang + (float) k * juce::MathConstants<float>::pi / 3.0f;
        g.drawLine (c.x + 4.5f * std::sin (a), c.y - 4.5f * std::cos (a),
                    c.x + 7.5f * std::sin (a), c.y - 7.5f * std::cos (a), 2.2f);
    }
}

// cabeças (apaga, grava, toca), cabrestante e rolo de pressão
void drawHeads (juce::Graphics& g, float tapeY, float capA)
{
    for (float x : { 84.0f, 108.0f, 132.0f })
    {
        juce::Rectangle<float> h (x - 8.0f, tapeY, 16.0f, 20.0f);
        g.setColour (theme::body);
        g.fillRoundedRectangle (h, 3.0f);
        g.setColour (theme::line);
        g.drawRoundedRectangle (h, 3.0f, 1.0f);
        g.setColour (theme::amber.withAlpha (0.7f));
        g.fillRect (x - 0.5f, tapeY + 1.0f, 1.0f, 6.0f);
    }
    const juce::Point<float> cap (166.0f, tapeY + 6.0f);
    g.setColour (theme::cream.darker (0.3f));
    g.fillEllipse (cap.x - 5.0f, cap.y - 5.0f, 10.0f, 10.0f);
    g.setColour (theme::dark);
    g.fillEllipse (cap.x + 3.0f * std::sin (capA) - 1.2f, cap.y - 3.0f * std::cos (capA) - 1.2f, 2.4f, 2.4f);
}

void drawRoller (juce::Graphics& g, float tapeY, float capA)
{
    const juce::Point<float> c (166.0f, tapeY - 8.0f);
    g.setColour (theme::body);
    g.fillEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
    g.setColour (theme::line);
    g.drawEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f, 1.0f);
    const float a = -capA * 5.0f / 7.0f;
    g.setColour (theme::dim);
    g.fillEllipse (c.x + 4.0f * std::sin (a) - 1.5f, c.y - 4.0f * std::cos (a) - 1.5f, 3.0f, 3.0f);
}

void drawGuide (juce::Graphics& g, juce::Point<float> p)
{
    g.setColour (theme::off);
    g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
    g.setColour (theme::cream.withAlpha (0.3f));
    g.drawEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, 1.0f);
}
} // namespace

// =====================================================================
// MÁQUINA (faixa de cima)
// =====================================================================
// escala em dBFS de pico, igual ao medidor do canal no Ableton (−9 no canal = ponteiro no −9);
// sobe junto com o pico e desce devagar, como ponteiro de VU
void MachineStrip::setVu (float peakL, float peakR, float dt)
{
    const float kDown = 1.0f - std::exp (-dt / 0.3f);
    auto follow = [kDown] (float& needle, float peak) { needle = peak > needle ? peak : needle + kDown * (peak - needle); };
    follow (needleL, peakL);
    follow (needleR, peakR);
}

void MachineStrip::paint (juce::Graphics& g)
{
    const auto  r  = getLocalBounds().toFloat();
    const float W  = r.getWidth(), H = r.getHeight();
    const int   m  = choiceIndex (proc, id::model), sp = choiceIndex (proc, id::speed);
    const float fs = proc.engine.rate.load();

    real::card (g, r.reduced (1.0f));

    drawTitle (g, 12.0f, 7.0f, u8 ("MÁQUINA "), machineText (proc));

    // ---- VUs ----
    drawVu (g, { 12.0f, 26.0f, 150.0f, 102.0f },  juce::Decibels::gainToDecibels (needleL, -40.0f), "L");
    drawVu (g, { 170.0f, 26.0f, 150.0f, 102.0f }, juce::Decibels::gainToDecibels (needleR, -40.0f), "R");

    g.setColour (theme::line);
    g.fillRect (330.0f, 26.0f, 1.0f, H - 38.0f);
    g.fillRect (800.0f, 26.0f, 1.0f, H - 38.0f);

    // ---- curva da fita: head bump + perda de agudo + desgaste ----
    {
        const float px0 = 344.0f, px1 = 788.0f, top = 30.0f, bot = H - 18.0f;
        const float dbTop = 9.0f, dbBot = -18.0f;
        auto fx = [=] (float hz) { return px0 + std::log (hz / 20.0f) / std::log (1000.0f) * (px1 - px0); };
        auto dy = [=] (float db) { return top + (dbTop - juce::jlimit (dbBot, dbTop, db)) / (dbTop - dbBot) * (bot - top); };

        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const bool major = hz == 100.0f || hz == 1000.0f || hz == 10000.0f;
            g.setColour (major ? theme::segOff : juce::Colour (0xff25211b));
            g.fillRect (fx (hz), top, 1.0f, bot - top);
            if (major)
            {
                g.setColour (theme::dim);
                g.setFont (theme::mono (9.5f));
                g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                            juce::Rectangle<float> (fx (hz) + 4.0f, bot + 3.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
            }
        }
        g.setColour (theme::segOff);
        g.fillRect (px0, dy (0.0f), px1 - px0, 1.0f);

        const float bump = proc.apvts.getRawParameterValue (id::bump)->load() * 0.01f;
        const float wear = proc.apvts.getRawParameterValue (id::wear)->load() * 0.01f;
        const auto  eq   = designPlayback (fs, m, sp, bump, wear);

        juce::Path curve;
        const int pts = 240;
        for (int k = 0; k < pts; ++k)
        {
            const float f  = juce::jmin (20.0f * std::pow (1000.0f, (float) k / (float) (pts - 1)), fs * 0.49f);
            const float om = juce::MathConstants<float>::twoPi * f / fs;
            const float mag = eq.bump.magnitude (om) * eq.dip.magnitude (om) * eq.hf.magnitude (om) * eq.wear.magnitude (om);
            const float x = fx (f), y = dy (juce::Decibels::gainToDecibels (mag, -60.0f));
            if (k == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (px1, bot);
        fill.lineTo (px0, bot);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (0.28f), 0.0f, top,
                                                 theme::coral.withAlpha (0.03f), 0.0f, bot, false));
        g.fillPath (fill);
        g.setColour (theme::cream);
        g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto& vc = voicing (m);
        const float hfHz = juce::jmin (vc.hfHz[juce::jlimit (0, 2, sp)], 20000.0f * std::pow (0.2f, wear));
        g.setColour (theme::dim);
        g.setFont (theme::mono (10.0f));
        g.drawText ("HEAD BUMP " + paramText (proc, id::bump) + " @ " + hzText (vc.bumpHz[juce::jlimit (0, 2, sp)])
                        + u8 (" · HIGH CUT ") + hzText (hfHz),
                    juce::Rectangle<float> (px1 - 420.0f, 6.0f, 420.0f, 14.0f), juce::Justification::centredRight);
    }

    // ---- transporte ----
    {
        const float tx = 812.0f, tw = W - 12.0f - tx;
        const int   st = proc.engine.state.load();
        const float v  = juce::jlimit (0.0f, 1.0f, proc.engine.speedNow.load());
        const auto  c  = st == tBraking || st == tStopped ? theme::coral : st == tStarting ? theme::ice : theme::amber;
        static const char* names[] = { "RODANDO", "FREANDO", "PARADA", "PARTINDO" };

        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText ("TAPE SPEED", juce::Rectangle<float> (tx, 26.0f, tw, 12.0f), juce::Justification::centredLeft);

        g.setColour (st == tRunning ? theme::cream : c);
        g.setFont (theme::mono (26.0f));
        g.drawText (juce::String (juce::roundToInt (v * 100.0f)) + "%", juce::Rectangle<float> (tx, 40.0f, tw, 30.0f),
                    juce::Justification::centredLeft);

        g.setColour (theme::segOff);
        g.fillRoundedRectangle (tx, 76.0f, tw, 5.0f, 2.0f);
        g.setColour (c);
        g.fillRoundedRectangle (tx, 76.0f, tw * v, 5.0f, 2.0f);

        const juce::Rectangle<float> led (tx, 92.5f, 7.0f, 7.0f);
        real::led (g, led.getCentre(), 3.5f, c, st != tRunning ? 1.0f : 0.0f);
        g.setColour (st != tRunning ? theme::cream : theme::dim);
        g.setFont (theme::sans (10.0f, true, 0.15f));
        g.drawText (names[juce::jlimit (0, 3, st)], juce::Rectangle<float> (tx + 14.0f, 89.0f, tw - 14.0f, 14.0f),
                    juce::Justification::centredLeft);

        g.setColour (theme::dim);
        g.setFont (theme::mono (9.5f));
        g.drawText ("FREIO   " + paramText (proc, id::brake), juce::Rectangle<float> (tx, 108.0f, tw, 12.0f),
                    juce::Justification::centredLeft);
        g.drawText ("PARTIDA " + paramText (proc, id::start), juce::Rectangle<float> (tx, 121.0f, tw, 12.0f),
                    juce::Justification::centredLeft);
    }
}

// =====================================================================
// ROLOS
// =====================================================================
std::pair<float, float> ReelView::packs (int model) const
{
    // a fita passa do rolo esquerdo para o direito; a cada 15 min (cassete: 30) recomeça
    const bool  cas  = model == modelCassette;
    const float hub  = cas ? 11.0f : 14.0f, full = cas ? 30.0f : 50.0f;
    const float f    = std::fmod (proc.engine.counter.load() / (cas ? 1800.0f : 900.0f), 1.0f);
    const float a    = hub * hub, b = full * full;
    return { std::sqrt (b - f * (b - a)), std::sqrt (a + f * (b - a)) };   // área de fita constante
}

void ReelView::tick()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastMs > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastMs) * 0.001) : 0.0f;
    lastMs = now;

    const int   m     = choiceIndex (proc, id::model);
    const float v     = proc.engine.speedNow.load();
    const float wob   = 1.0f + 20.0f * proc.engine.pitchDev.load();     // wow exagerado para aparecer no giro
    const float len   = 13.0f * tapeIps (m, choiceIndex (proc, id::speed)) * v * wob * dt;   // px de fita no quadro
    const auto  pk    = packs (m);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // os dois rolos giram no sentido anti-horário; rolo cheio gira devagar, vazio gira rápido
    angL = std::fmod (angL - len / pk.first, twoPi);
    angR = std::fmod (angR - len / pk.second, twoPi);
    capA = std::fmod (capA + len / 5.0f, twoPi);
    repaint();
}

void ReelView::paint (juce::Graphics& g)
{
    const int   m   = choiceIndex (proc, id::model);
    const float W   = (float) getWidth();
    const auto  pk  = packs (m);
    const bool  cas = m == modelCassette;
    const juce::Colour tapeC (0xff5a3d24);
    const float tapeY = cas ? 166.0f : 196.0f;
    const float tapeW = m == modelWide ? 4.0f : cas ? 1.6f : 2.2f;

    const juce::Rectangle<float> scr (1.5f, 1.5f, W - 3.0f, 229.0f);
    real::screen (g, scr, 4.0f);

    drawHeads (g, tapeY, capA);

    if (cas)
    {
        const juce::Point<float> gl (40.0f, tapeY), gr (W - 40.0f, tapeY);
        g.setColour (tapeC);
        juce::Path tp;
        tp.startNewSubPath (50.0f, 150.0f);
        tp.lineTo (gl);
        tp.lineTo (gr);
        tp.lineTo (W - 50.0f, 150.0f);
        g.strokePath (tp, juce::PathStrokeType (tapeW));

        const juce::Rectangle<float> shell (14.0f, 24.0f, W - 28.0f, 138.0f);
        g.setColour (theme::body);
        g.fillRoundedRectangle (shell, 7.0f);
        g.setColour (theme::line);
        g.drawRoundedRectangle (shell, 7.0f, 1.0f);
        g.setColour (theme::off);
        for (auto p : { shell.getTopLeft() + juce::Point<float> (9.0f, 9.0f), shell.getTopRight() + juce::Point<float> (-9.0f, 9.0f),
                        shell.getBottomLeft() + juce::Point<float> (9.0f, -9.0f), shell.getBottomRight() + juce::Point<float> (-9.0f, -9.0f) })
            g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);

        // etiqueta
        const juce::Rectangle<float> label (shell.getX() + 16.0f, shell.getY() + 10.0f, shell.getWidth() - 32.0f, 30.0f);
        g.setColour (theme::amber.withAlpha (0.85f));
        g.fillRoundedRectangle (label, 3.0f);
        g.setColour (theme::dark);
        g.setFont (theme::sans (10.5f, true, 0.06f));
        g.drawText ("InsideStudioSP", label.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
        g.setFont (theme::mono (9.5f));
        g.drawText (u8 ("C-60 · II"), label.reduced (8.0f, 0.0f), juce::Justification::centredRight);

        // janela com os carretéis
        const juce::Rectangle<float> win (58.0f, 80.0f, W - 116.0f, 46.0f);
        const juce::Point<float> hl (88.0f, 103.0f), hr (W - 88.0f, 103.0f);
        g.setColour (juce::Colour (0xff0b0907));
        g.fillRoundedRectangle (win, 5.0f);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (win.toNearestInt());
            drawPack (g, hl, pk.first, 11.0f);
            drawPack (g, hr, pk.second, 11.0f);
        }
        g.setColour (theme::line);
        g.drawRoundedRectangle (win, 5.0f, 1.0f);
        drawCassetteHub (g, hl, angL);
        drawCassetteHub (g, hr, angR);

        // abertura de baixo, onde a fita encosta nas cabeças
        juce::Path mouth;
        mouth.startNewSubPath (48.0f, shell.getBottom());
        mouth.lineTo (62.0f, shell.getBottom() - 16.0f);
        mouth.lineTo (W - 62.0f, shell.getBottom() - 16.0f);
        mouth.lineTo (W - 48.0f, shell.getBottom());
        mouth.closeSubPath();
        g.setColour (theme::panel2);
        g.fillPath (mouth);

        drawGuide (g, gl);
        drawGuide (g, gr);
    }
    else
    {
        const juce::Point<float> gl (22.0f, tapeY), gr (W - 22.0f, tapeY);
        const juce::Point<float> cl (62.0f, 80.0f), cr (W - 62.0f, 80.0f);

        // fita: carretel → guia → cabeças → guia → carretel
        g.setColour (tapeC);
        juce::Path tp;
        tp.startNewSubPath (tangent (cl, pk.first, gl, 1.0f));
        tp.lineTo (gl);
        tp.lineTo (gr);
        tp.lineTo (tangent (cr, pk.second, gr, -1.0f));
        g.strokePath (tp, juce::PathStrokeType (tapeW));

        drawGuide (g, gl);
        drawGuide (g, gr);
        drawReel (g, cl, 56.0f, pk.first, 14.0f, angL);
        drawReel (g, cr, 56.0f, pk.second, 14.0f, angR);
    }

    drawRoller (g, tapeY, capA);
    real::glass (g, scr, 4.0f);

    // contador e velocidade
    auto line = [] (const juce::String& a, const juce::String& b)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), theme::cream);
        return s;
    };
    auto sl = line ("CONTADOR ", juce::String ((int) proc.engine.counter.load() % 10000).paddedLeft ('0', 4));
    auto sr = line ("SPEED ", ipsText (tapeIps (m, choiceIndex (proc, id::speed))) + " IPS");
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, 242.0f, W, 14.0f });
    sr.draw (g, { 0.0f, 242.0f, W, 14.0f });
}

// =====================================================================
// EDITOR
// =====================================================================
FitaEditor::FitaEditor (FitaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), reels (p),
      driveMacro (p.apvts, id::drive,   "SATURATION",     true,  theme::amber),
      wowMacro   (p.apvts, id::wow,     "WOW",            true,  theme::coral),
      model      (p.apvts, id::model,   "MODELO",         false, theme::cream),
      speed      (p.apvts, id::speed,   "SPEED",          false, theme::cream),
      bump       (p.apvts, id::bump,    "HEAD BUMP",      false, theme::cream),
      wear       (p.apvts, id::wear,    "WEAR",           false, theme::cream),
      flutter    (p.apvts, id::flutter, "FLUTTER",        false, theme::cream),
      hiss       (p.apvts, id::hiss,    "HISS",           false, theme::cream),
      brake      (p.apvts, id::brake,   "FREIO",          false, theme::cream),
      start      (p.apvts, id::start,   "PARTIDA",        false, theme::cream),
      output     (p.apvts, id::output,  "GAIN",           false, theme::cream),
      mix        (p.apvts, id::mix),
      stopKey    ("TAPE STOP", theme::coral)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &reels, &driveMacro, &wowMacro, &model, &speed, &bump, &wear, &flutter, &hiss,
             &brake, &start, &output, &mix, &stopKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    stopAtt   = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::tapeStop, stopKey);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

    prevBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    nextBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    saveBtn.onClick    = [this] { askNewPreset(); };
    presetName.onClick = [this] { showPresetMenu(); };

    layoutCanvas();

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (ui::baseW * ui::minScale), juce::roundToInt (ui::baseH * ui::minScale),
                     juce::roundToInt (ui::baseW * ui::maxScale), juce::roundToInt (ui::baseH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) ui::baseW / (double) ui::baseH);
    setScale (proc.uiScale.load());
    startTimerHz (30);
}

FitaEditor::~FitaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FitaEditor::showPresetMenu()
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

    juce::Component::SafePointer<FitaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void FitaEditor::askNewPreset()
{
    juce::Component::SafePointer<FitaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void FitaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<FitaEditor> safe (this);
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

void FitaEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    strip.setVu (proc.vuL.exchange (0.0f), proc.vuR.exchange (0.0f), dt);
    strip.repaint();
    reels.tick();

    levelIn  = juce::jmax (proc.meterIn.exchange (0.0f),  levelIn * 0.85f);
    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());

    const float bpm = proc.shownBpm.load();
    const bool  syn = proc.synced.load();
    if (std::abs (bpm - lastBpm) > 0.05f || syn != lastSync)
    {
        lastBpm = bpm;
        lastSync = syn;
        canvas.repaint (380, 18, 236, 32);
    }

    const int m = choiceIndex (proc, id::model);
    if (m != lastModel)
    {
        lastModel = m;
        canvas.repaint (12, 216, 262, 28);   // título do card da máquina
    }
}

void FitaEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void FitaEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void FitaEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void FitaEditor::drawCanvas (juce::Graphics& g)
{
    real::chassis (g, (float) ui::baseW, (float) ui::baseH);

    // ---- logo: rolo de fita ----
    g.setColour (theme::amber);
    g.drawEllipse (18.0f, 21.0f, 26.0f, 26.0f, 2.0f);
    g.setColour (theme::cream);
    for (int k = 0; k < 3; ++k)
    {
        const float a = (float) k * juce::MathConstants<float>::twoPi / 3.0f;
        g.fillEllipse (31.0f + 6.5f * std::sin (a) - 2.5f, 34.0f - 6.5f * std::cos (a) - 2.5f, 5.0f, 5.0f);
    }
    g.setColour (theme::coral);
    g.fillEllipse (29.5f, 32.5f, 3.0f, 3.0f);

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("FITA");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 200.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 ("TAPE MACHINE"), tag2 (u8 ("SATURATION · WOW · STOP"));
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText (tag1, juce::Rectangle<float> (tagX, 22.0f, 180.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (tag2, juce::Rectangle<float> (tagX, 35.0f, 180.0f, 12.0f), juce::Justification::centredLeft);

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

    // ---- BPM (o FREIO é medido em tempos): SYNC com o host tocando, LIVRE parado ----
    {
        const bool syn = proc.synced.load();
        juce::AttributedString s;
        s.append (juce::String (proc.shownBpm.load(), 1), theme::mono (11.5f), theme::cream);
        s.append (u8 (" BPM · "), theme::mono (11.5f), theme::dim);
        s.append (syn ? "SYNC" : "LIVRE", theme::mono (11.5f), syn ? theme::cream : theme::dim);
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

    drawTitle (g, 22.0f,  226.0f, u8 ("MÁQUINA "), u8 ("· ") + paramText (proc, id::model));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), u8 ("GRAVAÇÃO"));
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), "TRANSPORTE");
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 90.0f }, "MACRO SATURATION",
              u8 ("Quanto a fita é empurrada. Sinal baixo passa igual; os picos arredondam e ganham cor."));
    drawDesc (g, { 664.0f, 252.0f, 140.0f, 90.0f }, "MACRO WOW",
              u8 ("Oscilação lenta da velocidade. Pouco dá vida; muito é fita derretida."));
}

void FitaEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    reels.setBounds (22, 246, 240, 264);

    driveMacro.setBounds (294, 246, 92, 92);
    model.setBounds      (294, 376, 119, 92);
    speed.setBounds      (413, 376, 119, 92);
    bump.setBounds       (294, 468, 119, 92);
    wear.setBounds       (413, 468, 119, 92);

    wowMacro.setBounds (562, 246, 92, 92);
    flutter.setBounds  (562, 376, 119, 92);
    hiss.setBounds     (681, 376, 119, 92);
    brake.setBounds    (562, 468, 119, 92);
    start.setBounds    (681, 468, 119, 92);

    stopKey.setBounds (830, 246, 148, 40);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (904, 446, 24, 112);
    meterOut.setBounds (932, 446, 24, 112);
}
