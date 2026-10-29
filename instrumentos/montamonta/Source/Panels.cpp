#include "PluginEditor.h"
#include "Draw.h"

using namespace mm;
using namespace md;

// =====================================================================
// PEÇA NO RACK
// =====================================================================
ModulePanel::ModulePanel (MontaMontaEditor& ed, MontaMontaProcessor& p, int m) : module (m), editor (ed), proc (p)
{
    auto& st = proc.apvts;
    const auto col = mu::moduleColour (m);
    const float oy = faceY;
    auto add = [this] (juce::Component* c, RF b)
    {
        controls.emplace_back (c);
        addAndMakeVisible (c);
        c->setBounds (b.toNearestInt());
        c->addMouseListener (this, true);   // tocar num controle também mostra o cartão da peça
    };
    auto knob = [&] (const juce::String& pid, const char* label, float r, bool scale, RF b)
    {
        add (new mu::Knob (st, pid, label, col, r, scale), b);
    };
    auto repaintOn = [this, &st] (const juce::String& pid, juce::Rectangle<int> area)
    {
        watch.push_back (std::make_unique<juce::ParameterAttachment> (*st.getParameter (pid), [this, area] (float) { repaint (area); }));
    };

    switch (m)
    {
        case vco1:
        case vco2:
        {
            const bool one = m == vco1;
            knob (one ? id::vco1Range : id::vco2Range, "RANGE", 14.0f, false, { 26.0f, oy + 63.0f, 60.0f, 50.0f });
            add (new mu::WaveSelector (st, one ? id::vco1Wave : id::vco2Wave, col), { 10.0f, oy + 116.0f, 92.0f, 14.0f });
            knob (one ? id::vco1Tune : id::vco2Tune, "TUNE", 11.0f, true, { 26.0f, oy + 134.0f, 60.0f, 50.0f });
            break;
        }
        case mixer:
        {
            const juce::String* lv[] = { &id::mixOsc1, &id::mixOsc2, &id::mixNoise };
            const juce::String* sw[] = { &id::mixOsc1On, &id::mixOsc2On, &id::mixNoiseOn };
            const char* names[] = { "OSC 1", "OSC 2", "NOISE" };
            for (int r = 0; r < 3; ++r)
            {
                const float rowY = oy + 72.0f + (float) r * 43.0f;
                knob (*lv[r], names[r], 12.0f, false, { 8.0f, rowY - 15.0f, 56.0f, 44.0f });
                add (new mu::Rocker (st, *sw[r]), { 68.0f, rowY - 11.0f, 14.0f, 22.0f });
                const juce::Rectangle<int> ledArea (84, juce::roundToInt (rowY) - 12, 24, 24);
                repaintOn (*lv[r], ledArea);
                repaintOn (*sw[r], ledArea);
            }
            break;
        }
        case vcf:
            knob (id::vcfCutoff, "CUTOFF", 15.0f, true, { 21.0f, oy + 92.0f, 70.0f, 56.0f });
            knob (id::vcfEmph, "EMPHASIS", 9.0f, true, { 2.0f, oy + 150.0f, 54.0f, 42.0f });
            knob (id::vcfContour, "CONTOUR", 9.0f, true, { 56.0f, oy + 150.0f, 54.0f, 42.0f });
            repaintOn (id::vcfCutoff, { 8, (int) oy + 52, 96, 40 });
            repaintOn (id::vcfEmph, { 8, (int) oy + 52, 96, 40 });
            break;
        case envAmp:
        case envMod:
        {
            const bool amp = m == envAmp;
            const juce::String* ids[] = { amp ? &id::ampA : &id::modA, amp ? &id::ampD : &id::modD,
                                          amp ? &id::ampS : &id::modS, amp ? &id::ampR : &id::modR };
            const char* names[] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
            for (int i = 0; i < 4; ++i)
            {
                knob (*ids[i], names[i], 11.0f, true, { 2.0f + (float) (i % 2) * 54.0f, oy + 90.0f + (float) (i / 2) * 52.0f, 54.0f, 52.0f });
                repaintOn (*ids[i], { 8, (int) oy + 52, 96, 38 });
            }
            break;
        }
        case output:
            knob (id::outVol, "VOLUME", 13.0f, true, { 26.0f, oy + 138.0f, 60.0f, 54.0f });
            break;
        default: break;
    }
}

bool ModulePanel::hitTest (int x, int y)
{
    return mu::piecePath ({ 0.0f, faceY, slotW, slotH }, mu::moduleShape (module)).contains ((float) x, (float) y);
}

void ModulePanel::setLevel (float l)
{
    if (std::abs (l - level) < 0.01f) return;
    level = l;
    repaint (18, (int) faceY + 54, 92, 84);
}

void ModulePanel::drawFace (juce::Graphics& g, RF r, int m, int lang, bool withSub)
{
    const auto col = mu::moduleColour (m);
    const auto path = mu::piecePath (r, mu::moduleShape (m));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d47), 0.0f, r.getY(), juce::Colour (0xff1f2129), 0.0f, r.getBottom(), false));
    g.fillPath (path);
    g.setColour (juce::Colours::black);
    g.strokePath (path, juce::PathStrokeType (1.2f));
    g.setColour (col.withAlpha (0.9f));
    g.strokePath (path, juce::PathStrokeType (2.0f));

    const RF bar (r.getX() + 6.0f, r.getY() + 6.0f, r.getWidth() - 12.0f, 30.0f);
    g.setColour (col);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillRoundedRectangle (bar.withHeight (12.0f), 3.0f);
    mu::fitText (g, pieceName (m, lang), bar.reduced (4.0f, 0.0f), 15.0f, juce::Colour (0xff111318), juce::Justification::centred);
    if (withSub)
        mu::fitText (g, pieceText (m).sub.get (lang), { r.getX() + 4.0f, r.getY() + 39.0f, r.getWidth() - 8.0f, 11.0f }, 8.0f,
                     juce::Colour (0xff8d93a8), juce::Justification::centred);
}

void ModulePanel::paint (juce::Graphics& g)
{
    const RF f (0.0f, faceY, slotW, slotH);
    drawFace (g, f, module, lang);
    const auto col = mu::moduleColour (module);

    switch (module)
    {
        case vco1:
        case vco2:
        {
            // escala do RANGE em pés de órgão
            const juce::Point<float> c (56.0f, faceY + 80.0f);
            const char* feet[] = { "32", "16", "8", "4", "2" };
            for (int i = 0; i < 5; ++i)
            {
                const float a = juce::degreesToRadians (-135.0f + 67.5f * (float) i);
                const float x = c.x + std::sin (a) * 25.0f, y = c.y - std::cos (a) * 23.0f;
                mu::text (g, feet[i], { x - 9.0f, y - 5.0f, 18.0f, 10.0f }, 7.5f, juce::Colour (0xff9aa0b4), juce::Justification::centred, false);
            }
            break;
        }
        case mixer:
        {
            const juce::String* lv[] = { &id::mixOsc1, &id::mixOsc2, &id::mixNoise };
            const juce::String* sw[] = { &id::mixOsc1On, &id::mixOsc2On, &id::mixNoiseOn };
            for (int r = 0; r < 3; ++r)
                mu::led (g, 96.0f, faceY + 72.0f + (float) r * 43.0f, col, param (proc, *sw[r]) > 0.5f && param (proc, *lv[r]) > 0.05f);
            break;
        }
        case vcf:
        {
            const RF s (12.0f, faceY + 56.0f, 88.0f, 32.0f);
            g.setColour (mu::dark);
            g.fillRoundedRectangle (s, 3.0f);
            g.setColour (mu::line2);
            g.drawRoundedRectangle (s, 3.0f, 1.0f);
            drawResponse (g, s.reduced (2.0f), param (proc, id::vcfCutoff), param (proc, id::vcfEmph) * 0.1f, col, false);
            break;
        }
        case envAmp:
        case envMod:
        {
            const bool amp = module == envAmp;
            const RF s (12.0f, faceY + 56.0f, 88.0f, 30.0f);
            g.setColour (mu::dark);
            g.fillRoundedRectangle (s, 3.0f);
            g.setColour (mu::line2);
            g.drawRoundedRectangle (s, 3.0f, 1.0f);
            drawAdsr (g, s.reduced (2.0f), param (proc, amp ? id::ampA : id::modA), param (proc, amp ? id::ampD : id::modD),
                      param (proc, amp ? id::ampS : id::modS) * 0.1f, param (proc, amp ? id::ampR : id::modR), col, false);
            break;
        }
        case output:
        {
            // alto-falante: o cone acende com o som que sai
            const juce::Point<float> c (56.0f, faceY + 100.0f);
            const float pulse = juce::jlimit (0.0f, 1.0f, level * 1.5f);
            g.setColour (juce::Colour (0xff0b0c10));
            g.fillEllipse (c.x - 32.0f, c.y - 32.0f, 64.0f, 64.0f);
            g.setColour (juce::Colour (0xff3a3e4c));
            g.drawEllipse (c.x - 32.0f, c.y - 32.0f, 64.0f, 64.0f, 2.0f);
            for (float r = 6.0f; r < 30.0f; r += 6.0f)
            {
                const float rr = r + pulse * 1.5f;
                g.setColour (juce::Colour (0xff24262e).interpolatedWith (col, pulse * 0.6f * (1.0f - r / 30.0f)));
                g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 1.0f);
            }
            g.setColour (juce::Colour (0xff1a1b22));
            g.fillEllipse (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f);
            g.setColour (juce::Colour (0xff3a3e4c));
            g.drawEllipse (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f, 1.0f);
            mu::led (g, 98.0f, faceY + 60.0f, col, level > 0.001f);
            break;
        }
        default: break;
    }

    if (! cableMode)
    {
        mu::screw (g, 12.0f, f.getBottom() - 11.0f);
        mu::screw (g, f.getRight() - 12.0f, f.getBottom() - 11.0f);
        return;
    }

    // tomadas do modo CABOS: entradas à esquerda, saída à direita
    auto jack = [&g] (float x, float y, const char* label, juce::Colour c)
    {
        g.setColour (juce::Colour (0xff050608));
        g.fillEllipse (x - 6.5f, y - 6.5f, 13.0f, 13.0f);
        g.setColour (juce::Colour (0xffb8bcc8));
        g.drawEllipse (x - 6.5f, y - 6.5f, 13.0f, 13.0f, 2.0f);
        g.setColour (juce::Colours::black);
        g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
        mu::text (g, label, { x - 18.0f, y + 7.0f, 36.0f, 9.0f }, 7.0f, c, juce::Justification::centred);
    };
    const float jy = f.getBottom() - 22.0f;
    const auto slot = MontaMontaEditor::slotRect (module).getPosition();
    for (int in = 0; in < numInputs; ++in)
        if (inputModule (in) == module)
        {
            const float x = MontaMontaEditor::jackPos (module, in).x - slot.x;
            const char* lbl = in == inMix1 ? "IN 1" : in == inMix2 ? "IN 2" : in == inVcfCv ? "CUT" : "IN";
            jack (x, jy, lbl, inputIsCv (in) ? mu::moduleColour (envMod) : mu::ink2);
        }
    if (hasOutput (module))
        jack (MontaMontaEditor::jackPos (module, -1).x - slot.x, jy, outputIsCv (module) ? "MOD" : "OUT", outputIsCv (module) ? col : mu::ink2);
}

void ModulePanel::mouseDown (const juce::MouseEvent&)
{
    editor.select (module);
    grabbing = false;
}

void ModulePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (e.eventComponent != this || cableMode) return;
    // arrastar pela faixa do nome tira a peça do rack
    if (! grabbing && e.getMouseDownY() < (int) faceY + 40 && e.getDistanceFromDragStart() > 5)
    {
        grabbing = true;
        editor.beginDrag (module, true, e, e.mouseDownPosition - juce::Point<float> (0.0f, faceY));
    }
    if (grabbing) editor.dragMove (e);
}

void ModulePanel::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent != this) return;
    if (grabbing) editor.endDrag (e);
    grabbing = false;
}

void ModulePanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.eventComponent == this && ! cableMode && e.y < (int) faceY + 40)
        editor.removeFromRack (module);
}

// =====================================================================
// BANDEJA
// =====================================================================
int TrayView::itemAt (juce::Point<float> p) const
{
    for (int i = 0; i < (int) order.size(); ++i)
        if (itemRect (i).expanded (4.0f).contains (p)) return i;
    return -1;
}

void TrayView::paint (juce::Graphics& g)
{
    const int lang = editor.lang();
    const auto& patch = editor.proc.patch;
    const RF b = getLocalBounds().toFloat();
    g.setColour (mu::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (mu::line);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    if (patch.cables)
    {
        mu::text (g, lang == 0 ? "MODO CABOS" : "CABLES MODE", { 14.0f, 12.0f, 186.0f, 16.0f }, 11.0f, mu::ink);
        mu::text (g, lang == 0 ? "Cables mode" : "Modo cabos", { 14.0f, 28.0f, 186.0f, 12.0f }, 9.0f, mu::dim, juce::Justification::centredLeft, false, true);
        const float h = mu::para (&g, txtCablesHelp().get (lang), { 14.0f, 50.0f, 186.0f, 200.0f }, 10.5f, mu::ink2);
        mu::para (&g, txtCablesHelp().other (lang), { 14.0f, 58.0f + h, 186.0f, 160.0f }, 8.8f, mu::dim, false, true);
        return;
    }

    mu::text (g, lang == 0 ? u8 ("BANDEJA DE PEÇAS") : juce::String ("PARTS TRAY"), { 14.0f, 12.0f, 186.0f, 16.0f }, 11.0f, mu::ink);
    mu::fitText (g, lang == 0 ? u8 ("Parts tray · arraste para o rack") : u8 ("Bandeja · drag into the rack"), { 14.0f, 28.0f, 190.0f, 12.0f },
                 9.0f, mu::dim, juce::Justification::centredLeft, false, true);

    for (int i = 0; i < (int) order.size(); ++i)
    {
        const int m = order[(size_t) i];
        const auto r = itemRect (i);
        const auto col = mu::moduleColour (m);
        const bool locked = ! inLesson (patch.lesson, m);
        const bool placed = ! locked && patch.placed[(size_t) m];
        const bool inHand = editor.draggedModule() == m;
        const auto shape = mu::piecePath (r, { false, true, false, false }, 7.0f);
        if (inHand)
        {
            dashed (g, shape, col.withAlpha (0.8f), 1.5f, 4.0f, 4.0f);
            mu::text (g, lang == 0 ? u8 ("(na mão)") : juce::String ("(in hand)"), r, 9.0f, col, juce::Justification::centred, false, true);
            continue;
        }
        const float alpha = placed ? 0.45f : 1.0f;
        if (locked)
            g.setColour (juce::Colour (0xff1b1d25));
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d47), 0.0f, r.getY(), juce::Colour (0xff1f2129), 0.0f, r.getBottom(), false));
        g.setOpacity (alpha);
        g.fillPath (shape);
        g.setOpacity (1.0f);
        g.setColour (locked ? juce::Colour (0xff30333f) : col.withAlpha (alpha));
        g.strokePath (shape, juce::PathStrokeType (1.5f));
        g.setColour (locked ? juce::Colour (0xff2c2f3a) : col.withAlpha (alpha));
        g.fillRoundedRectangle (r.getX() + 6.0f, r.getY() + 6.0f, 10.0f, r.getHeight() - 12.0f, 2.0f);
        mu::text (g, pieceName (m, lang), { r.getX() + 24.0f, r.getY() + 7.0f, 104.0f, 16.0f }, 12.0f,
                  locked ? juce::Colour (0xff555a6c) : mu::ink.withAlpha (placed ? 0.5f : 1.0f));
        const RF st (r.getX() + 24.0f, r.getY() + 25.0f, 106.0f, 13.0f);
        if (placed)
        {
            mu::checkIcon (g, { st.getX(), st.getY() + 3.0f, 8.0f, 7.0f }, mu::ok, 1.6f);
            mu::text (g, lang == 0 ? "no rack" : "in the rack", st.withTrimmedLeft (12.0f), 9.0f, mu::ok);
        }
        else if (locked)
        {
            mu::lockIcon (g, { st.getX(), st.getY() + 1.5f, 7.0f, 9.5f }, juce::Colour (0xff5d6276));
            mu::text (g, (lang == 0 ? u8 ("lição ") : juce::String ("lesson ")) + juce::String (lessonOf (m)), st.withTrimmedLeft (12.0f),
                      9.0f, juce::Colour (0xff5d6276));
        }
        else
            mu::fitText (g, lang == 0 ? "arraste para o rack" : "drag into the rack", st, 8.5f, col.withAlpha (0.85f), juce::Justification::centredLeft, false);
    }
    mu::fitText (g, lang == 0 ? u8 ("Peças novas liberam a cada lição") : juce::String ("New parts unlock with each lesson"),
                 { 10.0f, b.getBottom() - 20.0f, b.getWidth() - 20.0f, 12.0f }, 8.5f, juce::Colour (0xff5d6276), juce::Justification::centred, false);
}

void TrayView::mouseMove (const juce::MouseEvent& e)
{
    const int i = itemAt (e.position);
    const auto& patch = editor.proc.patch;
    const bool grab = i >= 0 && ! patch.cables && inLesson (patch.lesson, order[(size_t) i]) && ! patch.placed[(size_t) order[(size_t) i]];
    setMouseCursor (grab ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void TrayView::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;
    const auto& patch = editor.proc.patch;
    const int i = itemAt (e.position);
    if (i < 0 || patch.cables) return;
    const int m = order[(size_t) i];
    editor.select (m);
    if (! inLesson (patch.lesson, m))
    {
        const auto n = juce::String (lessonOf (m));
        editor.showMessage ("O " + pieceName (m, 0) + u8 (" chega na lição ") + n + u8 (". Por enquanto, leia no cartão o que ele faz."),
                            "The " + pieceName (m, 1) + " arrives in lesson " + n + ". For now, read on the card what it does.", mu::dim);
        return;
    }
    if (! patch.placed[(size_t) m]) dragging = m;
}

void TrayView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    if (! editor.draggingPiece() && e.getDistanceFromDragStart() > 4)
        editor.beginDrag (dragging, false, e, { slotW * 0.5f, 22.0f });
    if (editor.draggingPiece()) editor.dragMove (e);
}

void TrayView::mouseUp (const juce::MouseEvent& e)
{
    if (dragging >= 0 && editor.draggingPiece()) editor.endDrag (e);
    dragging = -1;
}

// =====================================================================
// FIOS
// =====================================================================
int CableLayer::outputAt (juce::Point<float> p) const
{
    for (int m = 0; m < numModules; ++m)
        if (hasOutput (m) && (MontaMontaEditor::jackPos (m, -1) - getPosition().toFloat()).getDistanceFrom (p) < 11.0f) return m;
    return -1;
}

int CableLayer::inputAt (juce::Point<float> p) const
{
    for (int in = 0; in < numInputs; ++in)
        if ((MontaMontaEditor::jackPos (inputModule (in), in) - getPosition().toFloat()).getDistanceFrom (p) < 11.0f) return in;
    return -1;
}

bool CableLayer::hitTest (int x, int y)
{
    const juce::Point<float> p ((float) x, (float) y);
    return dragFrom >= 0 || outputAt (p) >= 0 || inputAt (p) >= 0;
}

void CableLayer::paint (juce::Graphics& g)
{
    const auto o = getPosition().toFloat();
    auto cable = [&g] (juce::Point<float> a, juce::Point<float> b, juce::Colour c)
    {
        const float sag = 30.0f + a.getDistanceFrom (b) * 0.22f;
        juce::Path p;
        p.startNewSubPath (a);
        p.cubicTo (a.x, a.y + sag, b.x, b.y + sag, b.x, b.y);
        const juce::PathStrokeType thick (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (1.5f, 3.0f));
        g.setColour (c.darker (0.35f));
        g.strokePath (p, thick);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (-0.6f, -0.8f));
        for (auto e : { a, b })
        {
            g.setColour (c.darker (0.5f));
            g.fillEllipse (e.x - 5.5f, e.y - 5.5f, 11.0f, 11.0f);
            g.setColour (c);
            g.fillEllipse (e.x - 3.5f, e.y - 3.5f, 7.0f, 7.0f);
        }
    };

    for (const auto& w : proc.patch.wires)
        cable (MontaMontaEditor::jackPos (w.from, -1) - o, MontaMontaEditor::jackPos (inputModule (w.to), w.to) - o, mu::moduleColour (w.from));

    if (dragFrom >= 0)
    {
        // as entradas que aceitam este fio acendem
        for (int in = 0; in < numInputs; ++in)
            if (inputIsCv (in) == outputIsCv (dragFrom) && inputModule (in) != dragFrom)
            {
                const auto p = MontaMontaEditor::jackPos (inputModule (in), in) - o;
                g.setColour (mu::ok.withAlpha (0.8f));
                g.drawEllipse (p.x - 10.0f, p.y - 10.0f, 20.0f, 20.0f, 2.0f);
            }
        cable (MontaMontaEditor::jackPos (dragFrom, -1) - o, dragPos, mu::moduleColour (dragFrom));
    }
}

void CableLayer::mouseDown (const juce::MouseEvent& e)
{
    dragFrom = -1;
    if (const int m = outputAt (e.position); m >= 0)
    {
        dragFrom = m;
        editor.select (m);
    }
    else if (const int in = inputAt (e.position); in >= 0)
    {
        editor.select (inputModule (in));
        const int w = proc.patch.wireAt (in);
        if (w >= 0)
        {
            // puxar o fio pela ponta da entrada: solta dali e fica na mão
            dragFrom = proc.patch.wires[(size_t) w].from;
            proc.patch.wires.erase (proc.patch.wires.begin() + w);
            editor.patchEdited();
        }
    }
    dragPos = e.position;
    repaint();
}

void CableLayer::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0) return;
    dragPos = e.position;
    repaint();
}

void CableLayer::mouseUp (const juce::MouseEvent& e)
{
    if (dragFrom < 0) return;
    const int from = dragFrom;
    dragFrom = -1;
    const int in = inputAt (e.position);
    if (in >= 0)
    {
        auto& patch = proc.patch;
        if (outputIsCv (from) && ! inputIsCv (in))
            editor.showMessage (u8 ("O ENV MOD não faz som: ele só mexe em outra peça. Ligue ele na entrada CUT do VCF."),
                                "ENV MOD makes no sound: it only moves another part. Plug it into the VCF CUT input.", mu::warn);
        else if (! outputIsCv (from) && inputIsCv (in))
            editor.showMessage (u8 ("A entrada CUT só aceita o ENV MOD: ela mexe no corte, não recebe som."),
                                "The CUT input only takes ENV MOD: it moves the cutoff, it does not take sound.", mu::warn);
        else if (inputModule (in) == from)
            editor.showMessage (u8 ("Uma peça não pode se ligar nela mesma."), "A part cannot be wired into itself.", mu::warn);
        else
        {
            Patch test = patch;
            if (const int old = test.wireAt (in); old >= 0) test.wires.erase (test.wires.begin() + old);
            if (test.makesLoop (from, in))
                editor.showMessage (u8 ("Esse fio faria o som dar a volta e voltar para a mesma peça. Ligue em outra entrada."),
                                    "That wire would send the sound around in a loop. Use another input.", mu::warn);
            else
            {
                patch.wires = test.wires;
                patch.wires.push_back ({ from, in });
            }
        }
    }
    editor.patchEdited();
    repaint();
}

// =====================================================================
// PEÇA NA MÃO
// =====================================================================
void DragLayer::paint (juce::Graphics& g)
{
    if (module < 0) return;
    const RF r (pos.x - grab.x, pos.y - grab.y, slotW, slotH);
    const auto t = juce::AffineTransform::rotation (-0.07f, r.getCentreX(), r.getCentreY());
    {
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (t.translated (10.0f, 16.0f));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillPath (mu::piecePath (r, mu::moduleShape (module)));
    }
    {
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (t);
        ModulePanel::drawFace (g, r, module, lang);
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (juce::Colour (0xff15161c));
            g.fillRoundedRectangle (r.getX() + 14.0f, r.getY() + 66.0f + (float) i * 36.0f, r.getWidth() - 28.0f, 22.0f, 4.0f);
        }
    }
    // mãozinha segurando
    juce::Path hand;
    hand.addRoundedRectangle (0.0f, 0.0f, 22.0f, 26.0f, 8.0f);
    hand.addRoundedRectangle (2.0f, -12.0f, 6.0f, 16.0f, 3.0f);
    hand.addRoundedRectangle (8.0f, -15.0f, 6.0f, 18.0f, 3.0f);
    hand.addRoundedRectangle (14.0f, -12.0f, 6.0f, 16.0f, 3.0f);
    hand.addRoundedRectangle (-7.0f, 6.0f, 10.0f, 6.0f, 3.0f);
    const auto ht = juce::AffineTransform::translation (pos.x - 8.0f, pos.y - 4.0f);
    g.setColour (juce::Colour (0xfff2f2f5));
    g.fillPath (hand, ht);
    g.setColour (juce::Colour (0xff111111));
    g.strokePath (hand, juce::PathStrokeType (1.4f), ht);
}

// =====================================================================
// CARTÃO DA PEÇA
// =====================================================================
InfoCard::InfoCard (MontaMontaEditor& ed, MontaMontaProcessor& p) : editor (ed), proc (p)
{
    listen.play = true;
    listen.fontSize = 8.5f;
    done.fontSize = 8.5f;
    addAndMakeVisible (listen);
    addAndMakeVisible (done);
    listen.onClick = [this] { proc.startDemo (module); };
    done.onClick = [this]
    {
        proc.patch.challengesDone ^= 1u << (unsigned) module;
        repaint();
    };
    for (const auto* pid : { &id::vco1Wave, &id::vco2Wave, &id::mixOsc1, &id::mixOsc2, &id::mixNoise, &id::mixOsc1On, &id::mixOsc2On, &id::mixNoiseOn,
                             &id::vcfCutoff, &id::vcfEmph, &id::ampA, &id::ampD, &id::ampS, &id::ampR, &id::modA, &id::modD, &id::modS, &id::modR })
        watch.push_back (std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (*pid), [this] (float) { refresh(); }));
}

void InfoCard::resized()
{
    listen.setBounds (28, 476, 120, 20);
    done.setBounds (154, 476, getWidth() - 182, 20);
}

void InfoCard::setModule (int m)
{
    if (m < 0 || m >= numModules || m == module) return;
    module = m;
    repaint();
}

void InfoCard::drawGraph (juce::Graphics& g, RF r)
{
    const auto col = mu::moduleColour (module);
    g.setColour (mu::dark);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (mu::line2);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    for (int i = 1; i < 6; ++i)
    {
        g.setColour (juce::Colour (0xff16181f));
        const float x = r.getX() + r.getWidth() * (float) i / 6.0f;
        g.drawLine (x, r.getY() + 4.0f, x, r.getBottom() - 4.0f);
    }
    const int lang = editor.lang();
    switch (module)
    {
        case vco1:
        case vco2:
        {
            const int w = juce::jlimit (0, 5, juce::roundToInt (param (proc, module == vco1 ? id::vco1Wave : id::vco2Wave)));
            drawWave (g, r.reduced (8.0f, 12.0f), w, col, 2.5f);
            mu::text (g, waveNames[w], { r.getX() + 8.0f, r.getY() + 4.0f, 120.0f, 10.0f }, 7.5f, col);
            break;
        }
        case mixer:
        {
            const juce::String* lv[] = { &id::mixOsc1, &id::mixOsc2, &id::mixNoise };
            const juce::String* sw[] = { &id::mixOsc1On, &id::mixOsc2On, &id::mixNoiseOn };
            const char* n[] = { "OSC 1", "OSC 2", "NOISE" };
            for (int i = 0; i < 3; ++i)
            {
                const bool on = param (proc, *sw[i]) > 0.5f;
                const float v = param (proc, *lv[i]) * 0.1f;
                const RF bar (r.getX() + 24.0f + (float) i * 76.0f, r.getY() + 8.0f, 50.0f, r.getHeight() - 26.0f);
                g.setColour (juce::Colour (0xff15161c));
                g.fillRoundedRectangle (bar, 3.0f);
                const auto c = i == 2 ? juce::Colours::white.withAlpha (0.8f) : mu::moduleColour (i == 0 ? vco1 : vco2);
                g.setColour (on ? c : mu::faint.withAlpha (0.6f));
                g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * v), 3.0f);
                mu::text (g, n[i], { bar.getX() - 6.0f, r.getBottom() - 15.0f, 62.0f, 10.0f }, 7.5f, on ? mu::ink2 : mu::faint, juce::Justification::centred);
            }
            break;
        }
        case vcf:
            drawResponse (g, r.reduced (6.0f, 4.0f), param (proc, id::vcfCutoff), param (proc, id::vcfEmph) * 0.1f, col, true);
            mu::text (g, lang == 0 ? "graves" : "lows", { r.getX() + 8.0f, r.getBottom() - 15.0f, 60.0f, 10.0f }, 7.5f, juce::Colour (0xff6a6f84),
                      juce::Justification::centredLeft, false);
            mu::text (g, lang == 0 ? "agudos" : "highs", { r.getRight() - 68.0f, r.getBottom() - 15.0f, 60.0f, 10.0f }, 7.5f, juce::Colour (0xff6a6f84),
                      juce::Justification::centredRight, false);
            break;
        case envAmp:
        case envMod:
        {
            const bool amp = module == envAmp;
            drawAdsr (g, r.reduced (6.0f, 4.0f), param (proc, amp ? id::ampA : id::modA), param (proc, amp ? id::ampD : id::modD),
                      param (proc, amp ? id::ampS : id::modS) * 0.1f, param (proc, amp ? id::ampR : id::modR), col, true);
            mu::text (g, amp ? (lang == 0 ? "volume" : "loudness") : (lang == 0 ? "corte do filtro" : "filter cutoff"),
                      { r.getRight() - 128.0f, r.getY() + 4.0f, 120.0f, 10.0f }, 7.5f, col, juce::Justification::centredRight, false);
            break;
        }
        case output:
        {
            const float lv = editor.shownOutLevel();
            const float db = juce::Decibels::gainToDecibels (lv, -60.0f);
            const RF bar (r.getX() + 14.0f, r.getCentreY() - 10.0f, r.getWidth() - 28.0f, 16.0f);
            g.setColour (juce::Colour (0xff15161c));
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (col);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f)), 3.0f);
            mu::text (g, lv > 0.0005f ? juce::String (db, 1) + " dBFS" : (lang == 0 ? juce::String ("sem som") : juce::String ("no sound")),
                      { bar.getX(), bar.getBottom() + 4.0f, bar.getWidth(), 12.0f }, 8.5f, lv > 0.0005f ? mu::ink2 : mu::warn, juce::Justification::centred);
            break;
        }
        default: break;
    }
}

void InfoCard::paint (juce::Graphics& g)
{
    const int lang = editor.lang();
    const auto col = mu::moduleColour (module);
    const auto& t = pieceText (module);
    const RF b = getLocalBounds().toFloat();
    const float w = b.getWidth();

    g.setColour (mu::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (col.withAlpha (0.7f));
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (col);
    g.fillRoundedRectangle (0.0f, 0.0f, w, 58.0f, 8.0f);
    g.fillRect (0.0f, 40.0f, w, 18.0f);

    const auto name = pieceName (module, lang);
    auto nf = mu::font (24.0f);
    float nw = juce::GlyphArrangement::getStringWidth (nf, name);
    if (nw > 110.0f) { nf = mu::font (24.0f * 110.0f / nw); nw = 110.0f; }
    g.setFont (nf);
    g.setColour (juce::Colour (0xff0b0c11));
    g.drawText (name, RF (16.0f, 8.0f, nw + 4.0f, 40.0f), juce::Justification::centredLeft, false);
    const float x2 = 16.0f + nw + 12.0f;
    mu::fitText (g, t.full.get (lang), { x2, 13.0f, w - x2 - 10.0f, 14.0f }, 9.5f, juce::Colour (0xff0b0c11));
    mu::fitText (g, t.full.other (lang), { x2, 30.0f, w - x2 - 10.0f, 13.0f }, 9.0f, col.darker (0.75f), juce::Justification::centredLeft, true, true);

    drawGraph (g, { 16.0f, 68.0f, w - 32.0f, 84.0f });

    // texto: o idioma escolhido grande, o outro menor; a letra diminui até caber
    const juce::String main = t.about.get (lang) + "\n\n" + t.knobs.get (lang);
    const juce::String other = t.about.other (lang);
    const RF tr (16.0f, 162.0f, w - 32.0f, 174.0f);
    float size = 10.5f;
    for (; size > 7.5f; size -= 0.25f)
        if (mu::para (nullptr, main, tr, size, mu::ink2) + 6.0f + mu::para (nullptr, other, tr, size * 0.85f, mu::dim, false, true) <= tr.getHeight())
            break;
    const float h1 = mu::para (&g, main, tr, size, mu::ink2);
    mu::para (&g, other, tr.withTrimmedTop (h1 + 6.0f), size * 0.85f, mu::dim, false, true);

    // onde encaixa: a vizinhança no caminho clássico
    const RF fit (16.0f, 344.0f, w - 32.0f, 28.0f);
    g.setColour (juce::Colour (0xff1b1e28));
    g.fillRoundedRectangle (fit, 5.0f);
    const bool cables = proc.patch.cables;
    mu::text (g, cables ? (lang == 0 ? "CAMINHO:" : "PATH:") : (lang == 0 ? "ENCAIXA:" : "FITS:"), { fit.getX() + 8.0f, fit.getY(), 62.0f, fit.getHeight() },
              8.5f, mu::dim);
    std::vector<int> chain;
    juce::String arrow = u8 ("→");
    switch (module)
    {
        case vco1:   chain = { vco1, mixer }; break;
        case mixer:  chain = { vco1, mixer, vcf }; break;
        case vcf:    chain = { mixer, vcf, envAmp }; break;
        case envAmp: chain = { vcf, envAmp, output }; break;
        case output: chain = { envAmp, output }; break;
        case vco2:   chain = { vco2, mixer }; arrow = u8 ("↑"); break;
        default:     chain = { envMod, vcf }; arrow = u8 ("↑"); break;
    }
    float x = fit.getX() + 66.0f;
    for (size_t i = 0; i < chain.size(); ++i)
    {
        const auto n = pieceName (chain[i], lang);
        const float cw = juce::GlyphArrangement::getStringWidth (mu::font (8.0f), n) + 12.0f;
        const RF chip (x, fit.getY() + 6.0f, cw, 16.0f);
        g.setColour (mu::moduleColour (chain[i]).withAlpha (chain[i] == module ? 1.0f : 0.55f));
        g.fillRoundedRectangle (chip, 3.0f);
        mu::text (g, n, chip, 8.0f, juce::Colour (0xff111111), juce::Justification::centred);
        x += cw;
        if (i + 1 < chain.size())
        {
            mu::text (g, arrow, { x, fit.getY(), 18.0f, fit.getHeight() }, 11.0f, juce::Colour (0xff9aa0b4), juce::Justification::centred);
            x += 18.0f;
        }
    }

    // desafio
    const RF ch (16.0f, 382.0f, w - 32.0f, 120.0f);
    const bool isDone = ((proc.patch.challengesDone >> (unsigned) module) & 1u) != 0;
    g.setColour (juce::Colour (0xff1d1a0c));
    g.fillRoundedRectangle (ch, 6.0f);
    g.setColour (mu::gold.withAlpha (0.6f));
    g.drawRoundedRectangle (ch, 6.0f, 1.0f);
    const juce::Point<float> tc (ch.getX() + 18.0f, ch.getY() + 15.0f);
    g.setColour (mu::gold);
    for (float rr : { 6.0f, 3.5f })
        g.drawEllipse (tc.x - rr, tc.y - rr, rr * 2.0f, rr * 2.0f, 1.3f);
    g.fillEllipse (tc.x - 1.3f, tc.y - 1.3f, 2.6f, 2.6f);
    mu::text (g, lang == 0 ? "DESAFIO / CHALLENGE" : "CHALLENGE / DESAFIO", { ch.getX() + 30.0f, ch.getY() + 8.0f, 200.0f, 14.0f }, 10.0f, mu::gold);
    const RF ct (ch.getX() + 12.0f, ch.getY() + 28.0f, ch.getWidth() - 24.0f, 62.0f);
    float cs = 10.5f;
    for (; cs > 7.5f; cs -= 0.25f)
        if (mu::para (nullptr, t.challenge.get (lang), ct, cs, juce::Colour (0xfff1ead0)) + 2.0f
            + mu::para (nullptr, t.challenge.other (lang), ct, cs * 0.82f, juce::Colour (0xff9b9170), false, true) <= ct.getHeight())
            break;
    const float hc = mu::para (&g, t.challenge.get (lang), ct, cs, juce::Colour (0xfff1ead0));
    mu::para (&g, t.challenge.other (lang), ct.withTrimmedTop (hc + 2.0f), cs * 0.82f, juce::Colour (0xff9b9170), false, true);

    // os botões falam o idioma escolhido
    const juce::String listenText = lang == 0 ? "OUVIR EXEMPLO" : "HEAR EXAMPLE";
    const juce::String doneText = isDone ? (lang == 0 ? "FEITO!" : "DONE!") : (lang == 0 ? "FEITO" : "DONE");
    if (listen.getButtonText() != listenText) listen.setButtonText (listenText);
    if (done.getButtonText() != doneText) done.setButtonText (doneText);
    if (done.lit != isDone) { done.lit = isDone; done.repaint(); }
    const bool playing = proc.demoPlaying();
    if (listen.lit != playing) { listen.lit = playing; listen.repaint(); }
}

// =====================================================================
// OSCILOSCÓPIO E TECLADO
// =====================================================================
void ScopeView::paint (juce::Graphics& g)
{
    const RF b = getLocalBounds().toFloat();
    g.setColour (mu::dark);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (mu::line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (juce::Colour (0xff14161c));
        const float x = b.getWidth() * (float) i / 4.0f;
        g.drawLine (x, 4.0f, x, b.getBottom() - 4.0f);
    }
    const float mid = b.getCentreY() + 5.0f, amp = (b.getHeight() - 24.0f) * 0.5f;
    g.setColour (juce::Colour (0xff1a1c24));
    g.drawLine (4.0f, mid, b.getRight() - 4.0f, mid);
    mu::fitText (g, title, { 8.0f, 4.0f, b.getWidth() - 16.0f, 11.0f }, 7.5f, mu::dim);

    juce::Path p;
    if (live)
    {
        // as últimas amostras, alinhadas numa subida pelo zero para a onda ficar parada
        constexpr int span = 400;
        const int w = proc.scope.write.load (std::memory_order_acquire);
        constexpr int size = mm::ScopeRing::size;
        auto at = [&] (int back) { return proc.scope.data[(size_t) ((w - back + size * 4) & (size - 1))]; };
        int start = span;
        for (int k = span; k < size - 8; ++k)
            if (at (k + 1) < 0.0f && at (k) >= 0.0f) { start = k; break; }
        for (int i = 0; i < span; ++i)
        {
            const float x = 4.0f + (b.getWidth() - 8.0f) * (float) i / (float) (span - 1);
            const float y = mid - juce::jlimit (-1.2f, 1.2f, at (start - i)) * amp;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        mu::glow (g, p, colour, 1.5f);
    }
    else
    {
        p.startNewSubPath (4.0f, mid);
        p.lineTo (b.getRight() - 4.0f, mid);
        g.setColour (mu::faint);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
}

namespace
{
const int whiteSteps[] = { 0, 2, 4, 5, 7, 9, 11 };
}

int KeysView::noteAt (juce::Point<float> p) const
{
    const float kw = (float) getWidth() / 14.0f;
    if (p.y < (float) getHeight() * 0.6f)
        for (int i = 0; i < 13; ++i)
        {
            const int n = i % 7;
            if (n == 2 || n == 6) continue;
            const float bx = (float) (i + 1) * kw - 4.5f;
            if (p.x >= bx && p.x < bx + 9.0f) return 48 + 12 * (i / 7) + whiteSteps[n] + 1;
        }
    const int white = juce::jlimit (0, 13, (int) (p.x / kw));
    return 48 + 12 * (white / 7) + whiteSteps[white % 7];
}

void KeysView::paint (juce::Graphics& g)
{
    const float kw = (float) getWidth() / 14.0f, h = (float) getHeight();
    for (int i = 0; i < 14; ++i)
    {
        const int note = 48 + 12 * (i / 7) + whiteSteps[i % 7];
        const bool on = proc.keyboard.isNoteOnForChannels (0xffff, note);
        g.setColour (on ? colour.interpolatedWith (juce::Colours::white, 0.6f) : juce::Colour (0xffeceef4));
        g.fillRoundedRectangle ((float) i * kw, 0.0f, kw - 1.5f, h, 2.0f);
        g.setColour (juce::Colour (0xff555555));
        g.drawRoundedRectangle ((float) i * kw, 0.0f, kw - 1.5f, h, 2.0f, 0.8f);
        if (on)
        {
            g.setColour (colour);
            g.fillEllipse ((float) i * kw + kw * 0.5f - 4.0f, h - 14.0f, 8.0f, 8.0f);
        }
    }
    for (int i = 0; i < 13; ++i)
    {
        const int n = i % 7;
        if (n == 2 || n == 6) continue;
        const int note = 48 + 12 * (i / 7) + whiteSteps[n] + 1;
        const bool on = proc.keyboard.isNoteOnForChannels (0xffff, note);
        g.setColour (on ? colour : juce::Colour (0xff15161c));
        g.fillRoundedRectangle ((float) (i + 1) * kw - 4.5f, 0.0f, 8.0f, h * 0.6f, 1.5f);
    }
    mu::text (g, "C3", { 2.0f, h - 12.0f, 20.0f, 10.0f }, 7.0f, juce::Colour (0xff8a8f9f), juce::Justification::centredLeft, false);
}

void KeysView::press (int note)
{
    if (note == down) return;
    if (down >= 0) proc.keyboard.noteOff (1, down, 0.0f);
    down = note;
    if (down >= 0) proc.keyboard.noteOn (1, down, 0.8f);
    repaint();
}

void KeysView::mouseDown (const juce::MouseEvent& e) { press (noteAt (e.position)); }
void KeysView::mouseDrag (const juce::MouseEvent& e) { if (getLocalBounds().toFloat().contains (e.position)) press (noteAt (e.position)); }
void KeysView::mouseUp (const juce::MouseEvent&) { press (-1); }

// =====================================================================
// AS LIÇÕES NO CABEÇALHO
// =====================================================================
void LessonDots::paint (juce::Graphics& g)
{
    const auto& patch = editor.proc.patch;
    const int unlocked = editor.proc.unlockedLesson();
    for (int i = 0; i < numLessons; ++i)
    {
        const int l = i + 1;
        const float x = 11.0f + (float) i * 22.0f, y = 11.0f;
        const bool cur = ! patch.cables && patch.lesson == l;
        const bool doneL = l < unlocked;
        const bool open = l <= unlocked;
        if (cur)
        {
            g.setColour (mu::moduleColour (vcf).withAlpha (0.3f));
            g.fillEllipse (x - 10.5f, y - 10.5f, 21.0f, 21.0f);
        }
        g.setColour (cur ? mu::moduleColour (vcf) : doneL ? mu::ok : juce::Colour (0xff22252f));
        g.fillEllipse (x - 7.5f, y - 7.5f, 15.0f, 15.0f);
        g.setColour (cur ? juce::Colour (0xffbff3ff) : juce::Colour (0xff3a3e4c));
        g.drawEllipse (x - 7.5f, y - 7.5f, 15.0f, 15.0f, 1.0f);
        if (doneL && ! cur)
            mu::checkIcon (g, { x - 4.0f, y - 3.0f, 8.0f, 6.5f }, juce::Colour (0xff0b0c11), 1.8f);
        else
            mu::text (g, juce::String (l), { x - 7.5f, y - 7.5f, 15.0f, 15.0f }, 9.0f,
                      cur ? juce::Colour (0xff0b0c11) : open ? mu::ink2 : juce::Colour (0xff6a6f84), juce::Justification::centred);
    }
}

void LessonDots::mouseUp (const juce::MouseEvent& e)
{
    const int i = (int) (e.position.x / 22.0f);
    if (i < 0 || i >= numLessons) return;
    if (i + 1 > editor.proc.unlockedLesson())
    {
        editor.showMessage (u8 ("Essa lição abre quando você terminar a anterior."), "This lesson opens when you finish the previous one.", mu::dim);
        return;
    }
    editor.setLesson (i + 1);
}
