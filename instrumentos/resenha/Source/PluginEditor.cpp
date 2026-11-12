#include "PluginEditor.h"

using namespace resenha;
using rs::RF;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuNoUser = 20005,
              menuImport = 20006, menuLicenca = 20007, menuRemoveBank = 30000;

const char* const roleNames[numVoices] = { "PERGUNTA", "RESPOSTA", "RÉPLICA", "TRÉPLICA" };

// logo: dois balões de conversa (ciano e magenta) com uma onda dentro
void drawLogo (juce::Graphics& g, RF r)
{
    auto bubble = [&g] (RF b, juce::Colour c, bool tailLeft)
    {
        juce::Path p;
        p.addRoundedRectangle (b, 6.0f);
        const float tx = tailLeft ? b.getX() + 8.0f : b.getRight() - 8.0f;
        p.addTriangle (tx - 4.0f, b.getBottom() - 1.0f, tx + 4.0f, b.getBottom() - 1.0f, tx + (tailLeft ? -5.0f : 5.0f), b.getBottom() + 7.0f);
        g.setColour (c.withAlpha (0.2f));
        g.strokePath (p, juce::PathStrokeType (5.0f));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (2.0f));
    };
    bubble ({ r.getX(), r.getY(), r.getWidth() * 0.72f, r.getHeight() * 0.55f }, rs::cyan, true);
    const RF b2 (r.getX() + r.getWidth() * 0.28f, r.getY() + r.getHeight() * 0.36f, r.getWidth() * 0.72f, r.getHeight() * 0.5f);
    g.setColour (rs::bg0);
    g.fillRoundedRectangle (b2, 6.0f);
    bubble (b2, rs::magenta, false);
    juce::Path w;
    for (int i = 0; i <= 24; ++i)
    {
        const float x = b2.getX() + 6.0f + (b2.getWidth() - 12.0f) * (float) i / 24.0f;
        const float y = b2.getCentreY() - std::sin ((float) i / 24.0f * 12.6f) * b2.getHeight() * 0.22f;
        if (i == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
    }
    g.setColour (rs::orange);
    g.strokePath (w, juce::PathStrokeType (1.6f));
}
} // namespace

// =====================================================================
// CARTÃO DO TIMBRE
// =====================================================================
VoiceCard::VoiceCard (ResenhaProcessor& p, int voiceIndex) : proc (p), v (voiceIndex), colour (rs::voiceColour (voiceIndex))
{
    auto& st = proc.apvts;
    const int w = 182;

    addAndMakeVisible (lock);
    lock.setBounds (w - 58, 7, 24, 24);
    lockAtt = std::make_unique<APVTS::ButtonAttachment> (st, id::lockVoice (v), lock);
    addAndMakeVisible (dice);
    dice.setBounds (w - 32, 7, 25, 24);
    dice.onClick = [this] { proc.randomizeVoice (v); };

    wave = std::make_unique<rs::ChoiceBox> (st, id::v (v, id::wave), colour);
    addAndMakeVisible (*wave);
    wave->setBounds (8, 90, w - 74, 16);
    engine = std::make_unique<rs::ChoiceBox> (st, id::v (v, id::eng), colour);
    addAndMakeVisible (*engine);
    engine->setBounds (w - 62, 90, 54, 16);

    struct K { const juce::String* pid; const char* label; bool bipolar; };
    const K rows[4][4] = {
        { { &id::ratio, "RATIO", false }, { &id::fm, "FM", false }, { &id::fb, "FEEDBACK", false }, { &id::fmDec, "FM DEC", false } },
        { { &id::filt, "FILTER", false }, { &id::cut, "CUTOFF", false }, { &id::res, "RESO", false }, { &id::fEnv, "F.ENV", false } },
        { { &id::uni, "UNISON", false }, { &id::move, "MOVE", false }, { &id::mRate, "RATE", false }, { &id::dec, "DECAY", false } },
        { { &id::pEnv, "P.ENV", true }, { &id::glide, "GLIDE", false }, { &id::bend, "BEND", true }, { &id::pan, "PAN", true } },
    };
    const float cw = (float) (w - 6) / 4.0f;
    for (int r = 0; r < 4; ++r)
    {
        const float x0 = 3.0f;
        for (int c = 0; c < 4; ++c)
        {
            const auto& k = rows[r][c];
            auto knob = std::make_unique<rs::NeonKnob> (st, id::v (v, *k.pid), k.label, colour, 25.0f, k.bipolar);
            addAndMakeVisible (*knob);
            knob->setBounds (juce::roundToInt (x0 + cw * (float) c), 109 + r * 47, juce::roundToInt (cw), 47);
            knobs.push_back (std::move (knob));
        }
    }

    for (const auto* pid : { &id::wave, &id::ratio, &id::fm, &id::fb, &id::uni, &id::eng })
        if (auto* prm = st.getParameter (id::v (v, *pid)))
            watch.push_back (std::make_unique<juce::ParameterAttachment> (*prm, [this] (float) { applyEngine(); repaint (8, 60, getWidth() - 16, 28); }));
    applyEngine();

    // HUB: INTERNO | PLUGIN
    for (auto* b : { &srcInt, &srcPlug })
    {
        addAndMakeVisible (*b);
        b->getProperties().set ("accent", (juce::int64) colour.getARGB());
    }
    srcInt.setBounds (8, 40, (w - 16) / 2, 18);
    srcPlug.setBounds (8 + (w - 16) / 2, 40, (w - 16) / 2, 18);
    srcInt.onClick  = [this] { proc.setSlotUsesPlugin (v, false); refresh(); };
    srcPlug.onClick = [this]
    {
        proc.setSlotUsesPlugin (v, true);
        refresh();
        if (proc.slot (v).get() == nullptr && proc.slot (v).missing.isEmpty() && onChoosePlugin) onChoosePlugin (v);
    };
    panel = std::make_unique<PluginPanel> (proc, v, colour);
    panel->onChoose = [this] { if (onChoosePlugin) onChoosePlugin (v); };
    addChildComponent (*panel);
    panel->setBounds (0, 62, w, 240);
    shownPlugin = true;   // força o primeiro showSource
    showSource (false);
}

bool VoiceCard::isVa() const
{
    return juce::roundToInt (proc.apvts.getRawParameterValue (id::v (v, id::eng))->load()) == 1;
}

void VoiceCard::applyEngine()
{
    // knobs na ordem de rows: 0 RATIO, 1 FM, 2 FEEDBACK, 3 FM DEC ... 8 UNISON
    const bool va = isVa();
    auto raw = [this] (const juce::String& name) { return proc.apvts.getRawParameterValue (id::v (v, name)); };
    knobs[0]->setLabel (va ? "OSC 2" : "RATIO");
    knobs[1]->setLabel (va ? "OSC 2 LVL" : "FM");
    knobs[2]->setLabel (va ? "DETUNE" : "FEEDBACK");
    knobs[3]->setLabel (va ? "ENV DEC" : "FM DEC");
    knobs[8]->setLabel (va ? "VOICES" : "UNISON");
    if (va)
    {
        auto* r = raw (id::ratio);
        knobs[0]->setValueText ([r]
        {
            const int st = vaSemis (ratios[juce::jlimit (0, (int) std::size (ratios) - 1, juce::roundToInt (r->load()))]);
            return (st > 0 ? "+" : "") + juce::String (st) + " st";
        });
        auto* u = raw (id::uni);
        knobs[8]->setValueText ([u] { return juce::String (vaVoices (u->load() * 0.01f)); });
    }
    else
    {
        knobs[0]->setValueText ({});
        knobs[8]->setValueText ({});
    }
}

void VoiceCard::setTalk (float level)
{
    const float t = juce::jlimit (0.0f, 1.0f, level * 1.6f);
    if (std::abs (t - talk) < 0.04f) return;
    talk = t;
    repaint();
}

void VoiceCard::setSilent (bool s)
{
    if (s == silent) return;
    silent = s;
    for (auto& k : knobs) k->setDim (s);
    wave->setColour (s ? rs::faint : colour);
    engine->setColour (s ? rs::faint : colour);
    repaint();
}

// VA: um ciclo do OSC 1 somado ao OSC 2 (no intervalo e no nível dele), e quantas vozes de uníssono
void VoiceCard::drawVa (juce::Graphics& g, RF r)
{
    auto val = [this] (const juce::String& name) { return proc.apvts.getRawParameterValue (id::v (v, name))->load(); };
    const int waveIdx = juce::roundToInt (val (id::wave));
    const float ratio = ratios[juce::jlimit (0, (int) std::size (ratios) - 1, juce::roundToInt (val (id::ratio)))];
    const float mul = std::exp2 ((float) vaSemis (ratio) / 12.0f);
    const float lvl2 = val (id::fm) * 0.01f;
    const int voicesN = vaVoices (val (id::uni) * 0.01f);
    auto shape = [waveIdx] (float ph)
    {
        ph -= std::floor (ph);
        switch (waveIdx)
        {
            case 0:  return std::sin (juce::MathConstants<float>::twoPi * ph);
            case 1:  return 4.0f * std::abs (ph - 0.5f) - 1.0f;
            case 2:  return 2.0f * ph - 1.0f;
            default:
            {
                const float w = waveIdx == 4 ? 0.25f : waveIdx == 5 ? 0.125f : 0.5f;
                return (ph < w ? 1.0f : -1.0f) * 0.85f;
            }
        }
    };
    const float waveH = r.getHeight() - 12.0f;
    const float waveMid = r.getY() + 2.0f + waveH * 0.5f, waveAmp = waveH * 0.44f;
    const int N = 320;
    juce::Path p;
    for (int i = 0; i <= N; ++i)
    {
        const float ph = 2.0f * (float) i / (float) N;
        const float y = (shape (ph) + lvl2 * shape (ph * mul + 0.25f)) / (1.0f + lvl2);
        const float x = r.getX() + 4.0f + (r.getWidth() - 8.0f) * (float) i / (float) N;
        const float yy = waveMid - y * waveAmp;   // a onda fica acima da legenda de baixo
        if (i == 0) p.startNewSubPath (x, yy); else p.lineTo (x, yy);
    }
    const auto c = silent ? rs::faint : colour;
    g.setColour (c.withAlpha (0.2f));
    g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (theme::mono (7.5f));
    g.setColour (rs::dim.withAlpha (0.8f));
    g.drawText (juce::String::fromUTF8 ("OSC 1 + OSC 2 · ") + juce::String (voicesN) + (voicesN == 1 ? " VOZ" : " VOZES"),
                RF (r.getX() + 5.0f, r.getBottom() - 11.0f, r.getWidth() - 10.0f, 10.0f), juce::Justification::centredLeft, false);
}

void VoiceCard::drawWave (juce::Graphics& g, RF r)
{
    if (isVa()) { drawVa (g, r); return; }
    // a forma de onda do timbre no pico do ataque: o modulador (RATIO, FEEDBACK) entortando a portadora (FM)
    auto val = [this] (const juce::String& name) { return proc.apvts.getRawParameterValue (id::v (v, name))->load(); };
    const int waveIdx = juce::roundToInt (val (id::wave));
    const float ratio = ratios[juce::jlimit (0, (int) std::size (ratios) - 1, juce::roundToInt (val (id::ratio)))];
    const float fm = val (id::fm) * 0.01f, fb = val (id::fb) * 0.01f * 1.5f;
    const float index = fm * fm * 7.0f;
    const float twoPi = juce::MathConstants<float>::twoPi;

    const float waveH = r.getHeight() - 12.0f;
    const float waveMid = r.getY() + 2.0f + waveH * 0.5f, waveAmp = waveH * 0.44f;
    const int N = 320;
    const float cycles = 2.0f;
    juce::Path p;
    float m1 = 0.0f, m2 = 0.0f;
    for (int i = 0; i <= N; ++i)
    {
        const float cph = cycles * (float) i / (float) N;
        const float m = std::sin (twoPi * cph * ratio + fb * 0.5f * (m1 + m2));
        m2 = m1;
        m1 = m;
        float phi = cph + index * m / twoPi;
        phi -= std::floor (phi);
        float y;
        switch (waveIdx)
        {
            case 1:  y = 4.0f * std::abs (phi - 0.5f) - 1.0f; break;
            case 2:  y = 2.0f * phi - 1.0f; break;
            case 3:  y = phi < 0.5f ? 0.8f : -0.8f; break;
            default: y = std::sin (twoPi * phi); break;
        }
        const float x = r.getX() + 4.0f + (r.getWidth() - 8.0f) * (float) i / (float) N;
        const float yy = waveMid - y * waveAmp;   // a onda fica acima da legenda de baixo
        if (i == 0) p.startNewSubPath (x, yy); else p.lineTo (x, yy);
    }
    const auto c = silent ? rs::faint : colour;
    g.setColour (c.withAlpha (0.2f));
    g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (theme::mono (7.5f));
    g.setColour (rs::dim.withAlpha (0.8f));
    g.drawText (juce::String::fromUTF8 ("MOD → CARRIER"), RF (r.getX() + 5.0f, r.getBottom() - 11.0f, 100.0f, 10.0f),
                juce::Justification::centredLeft, false);
}

void VoiceCard::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (4.0f);
    const auto c = silent ? rs::faint : colour;
    rs::glow (g, r, 8.0f, colour, silent ? 0.0f : 0.35f + talk * 0.65f);
    rs::box (g, r, 8.0f, c.withAlpha (silent ? 0.5f : 0.45f + talk * 0.55f));
    g.setColour (c.withAlpha (0.9f));
    g.fillRoundedRectangle (r.getX() + 8.0f, r.getY(), r.getWidth() - 16.0f, 2.5f, 1.0f);

    // letra no círculo, nome e o papel na conversa
    const juce::Point<float> cc (24.0f, 20.0f);
    if (! silent)
    {
        g.setColour (colour.withAlpha (0.25f + talk * 0.4f));
        g.fillEllipse (cc.x - 15.0f, cc.y - 15.0f, 30.0f, 30.0f);
    }
    g.setColour (silent ? rs::faint : colour);
    g.fillEllipse (cc.x - 11.0f, cc.y - 11.0f, 22.0f, 22.0f);
    g.setColour (rs::bg0);
    g.setFont (theme::sans (13.0f, true));
    g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + v)), RF (cc.x - 11.0f, cc.y - 11.0f, 22.0f, 22.0f),
                juce::Justification::centred, false);
    rs::label (g, "SLOT " + juce::String::charToString ((juce::juce_wchar) ('A' + v)), { 42.0f, 8.0f, 90.0f, 12.0f },
               juce::Justification::centredLeft, 9.0f, silent ? rs::faint : rs::ink);
    rs::label (g, role, { 42.0f, 21.0f, 90.0f, 12.0f }, juce::Justification::centredLeft, 8.5f, c);

    if (! shownPlugin)
    {
        const RF disp (8.0f, 62.0f, (float) getWidth() - 16.0f, 25.0f);
        rs::screen (g, disp, 4.0f);
        drawWave (g, disp);
    }
}

void VoiceCard::paintOverChildren (juce::Graphics& g)
{
    if (silent)
    {
        g.setColour (rs::bg0.withAlpha (0.35f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 8.0f);
    }
}

void VoiceCard::showSource (bool plugin)
{
    if (plugin == shownPlugin) return;
    shownPlugin = plugin;
    wave->setVisible (! plugin);
    engine->setVisible (! plugin);
    for (auto& k : knobs) k->setVisible (! plugin);
    panel->setVisible (plugin);
    srcInt.lit = ! plugin;   srcInt.filled = ! plugin;
    srcPlug.lit = plugin;    srcPlug.filled = plugin;
    srcInt.repaint();
    srcPlug.repaint();
    repaint();
}

void VoiceCard::refresh()
{
    showSource (proc.slotUsesPlugin (v));
    if (shownPlugin) panel->refresh();
}

// =====================================================================
// HUB: botões, knobs ligados ao plugin, cartão do plugin
// =====================================================================
void PillButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (down) r = r.translated (0.0f, 0.6f);
    const auto accent = getProperties().contains ("accent") ? juce::Colour ((juce::uint32) (juce::int64) getProperties()["accent"]) : colour;
    if (filled)
    {
        g.setColour (accent);
        g.fillRoundedRectangle (r, 3.0f);
    }
    else
    {
        rs::screen (g, r, 3.0f);
        if (lit || over)
        {
            g.setColour (accent.withAlpha (lit ? 0.9f : 0.5f));
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        }
    }
    const auto textCol = filled ? rs::bg0 : ! isEnabled() ? rs::faint : lit ? accent : over ? rs::ink : rs::dim;
    g.setColour (textCol);
    g.setFont (theme::sans (juce::jmin (10.0f, r.getHeight() * 0.55f), true, 0.14f));
    g.drawFittedText (getButtonText(), r.reduced (3.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
}

MacroKnob::MacroKnob (ResenhaProcessor& p, int voiceIndex, int knobIndex, juce::Colour c)
    : proc (p), v (voiceIndex), k (knobIndex), colour (c),
      knob (p.apvts, id::v (voiceIndex, id::hMacro (knobIndex)), "LIGAR", c, 25.0f)
{
    addChildComponent (knob);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MacroKnob::refresh()
{
    auto& sl = proc.slot (v);
    const bool learnNow = sl.learnMacro.load() == k;
    int idx = sl.macroIdx[(size_t) k].load();
    auto* inst = sl.get();
    if (inst == nullptr || idx >= inst->getParameters().size()) idx = -1;
    if (idx != shownIdx || learnNow != learning || (const void*) inst != shownInst)
    {
        shownIdx = idx;
        shownInst = inst;
        learning = learnNow;
        knob.setVisible (idx >= 0 && ! learning);
        if (idx >= 0 && inst != nullptr)
        {
            auto* prm = inst->getParameters()[idx];
            knob.setLabel (prm->getName (12).toUpperCase());
            knob.setValueText ([prm] { return prm->getCurrentValueAsText().substring (0, 10); });
        }
        repaint();
    }
    else if (knob.isVisible())
        knob.repaint();
}

void MacroKnob::paint (juce::Graphics& g)
{
    if (knob.isVisible()) return;
    // livre: círculo tracejado com "+" e LIGAR embaixo (aprendendo: pisca "MEXA NO PLUGIN")
    const float d = 25.0f;
    const RF c ((float) getWidth() * 0.5f - d * 0.5f, 0.0f, d, d);
    const bool blink = learning && (juce::Time::getMillisecondCounter() / 350) % 2 == 0;
    const auto col = learning ? (blink ? rs::amber : rs::amber.withAlpha (0.4f)) : colour.withAlpha (0.8f);
    juce::Path circle;
    circle.addEllipse (c.reduced (2.0f));
    juce::Path dashed;
    const float dash[] = { 3.0f, 3.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, circle, dash, 2);
    g.setColour (col);
    g.fillPath (dashed);
    g.fillRect (c.getCentreX() - 4.0f, c.getCentreY() - 0.6f, 8.0f, 1.2f);
    g.fillRect (c.getCentreX() - 0.6f, c.getCentreY() - 4.0f, 1.2f, 8.0f);
    g.setFont (theme::sans (7.5f, true, 0.1f));
    g.drawFittedText (learning ? juce::String ("MEXA NO PLUGIN") : juce::String ("LIGAR"),
                      juce::Rectangle<int> (0, (int) d + 1, getWidth(), 11), juce::Justification::centred, 1, 0.6f);
    if (learning) repaint();   // pisca enquanto espera
}

void MacroKnob::mouseDown (const juce::MouseEvent& e)
{
    if (knob.isVisible() && e.y < 26 && ! e.mods.isPopupMenu()) return;   // no knob: o próprio knob trata
    showMenu();
}

void MacroKnob::showMenu()
{
    auto& sl = proc.slot (v);
    auto* inst = sl.get();
    if (inst == nullptr) return;
    juce::PopupMenu menu;
    menu.addSectionHeader ("KNOB " + juce::String (k + 1) + " DO SLOT " + juce::String::charToString ((juce::juce_wchar) ('A' + v)));
    menu.addItem (1, rs::u8 ("Aprender: mexa um controle na janela do plugin"));
    juce::PopupMenu all;
    const auto& prms = inst->getParameters();
    const int chunk = 40;
    for (int start = 0; start < prms.size(); start += chunk)
    {
        juce::PopupMenu sub;
        const int end = juce::jmin (prms.size(), start + chunk);
        for (int i = start; i < end; ++i)
            sub.addItem (1000 + i, prms[i]->getName (40), true, sl.macroIdx[(size_t) k].load() == i);
        all.addSubMenu (juce::String (start + 1) + rs::u8 ("–") + juce::String (end), sub);
    }
    menu.addSubMenu ("Escolher da lista", all, prms.size() > 0);
    if (sl.learnMacro.load() == k) menu.addItem (3, "Parar de aprender");
    if (sl.macroIdx[(size_t) k].load() >= 0) menu.addItem (2, "Desligar este knob");

    juce::Component::SafePointer<MacroKnob> safe (this);
    auto opts = juce::PopupMenu::Options().withTargetComponent (this);
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>()) opts = opts.withParentComponent (editor);
    menu.showMenuAsync (opts, [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& pr = safe->proc;
        if (r == 1) { pr.startLearn (safe->v, safe->k); pr.openSlotWindow (safe->v); }
        else if (r == 2) pr.linkMacro (safe->v, safe->k, -1);
        else if (r == 3) pr.cancelLearn (safe->v);
        else if (r >= 1000) pr.linkMacro (safe->v, safe->k, r - 1000);
        safe->refresh();
    });
}

PluginPanel::PluginPanel (ResenhaProcessor& p, int voiceIndex, juce::Colour c) : proc (p), v (voiceIndex), colour (c)
{
    const int w = 182;
    for (auto* b : { &openBtn, &swapBtn, &bypBtn })
    {
        addAndMakeVisible (*b);
        b->getProperties().set ("accent", (juce::int64) (b == &bypBtn ? rs::amber : colour).getARGB());
    }
    openBtn.setBounds (8, 56, 56, 20);
    swapBtn.setBounds (68, 56, 64, 20);
    bypBtn.setBounds (136, 56, 38, 20);
    openBtn.onClick = [this] { proc.openSlotWindow (v); };
    swapBtn.onClick = [this] { if (onChoose) onChoose(); };
    bypBtn.onClick  = [this] { auto& b = proc.slot (v).bypass; b = ! b.load(); refresh(); };

    const float cw = (float) (w - 6) / 4.0f;
    for (int k = 0; k < 4; ++k)
    {
        auto m = std::make_unique<MacroKnob> (proc, v, k, colour);
        addAndMakeVisible (*m);
        m->setBounds (juce::roundToInt (3.0f + cw * (float) k), 96, juce::roundToInt (cw), 47);
        macros.push_back (std::move (m));
    }
    struct K { const juce::String* pid; const char* label; bool bipolar; };
    const K row[4] = { { &id::hOct, "OITAVA", true }, { &id::hVel, "VELOC.", false }, { &id::hBend, "BEND", true }, { &id::hLvl, "NIVEL", false } };
    for (int k = 0; k < 4; ++k)
    {
        auto kn = std::make_unique<rs::NeonKnob> (proc.apvts, id::v (v, *row[k].pid), k == 3 ? rs::u8 ("NÍVEL") : juce::String (row[k].label),
                                                  colour, 25.0f, row[k].bipolar);
        addAndMakeVisible (*kn);
        kn->setBounds (juce::roundToInt (3.0f + cw * (float) k), 168, juce::roundToInt (cw), 47);
        knobs.push_back (std::move (kn));
    }
}

void PluginPanel::refresh()
{
    auto& sl = proc.slot (v);
    auto* inst = sl.get();
    const juce::String name = inst != nullptr ? sl.desc.name : sl.missing.isNotEmpty() ? sl.desc.name : juce::String();
    const juce::String maker = inst != nullptr ? sl.desc.manufacturerName + rs::u8 (" · ") + sl.desc.pluginFormatName
                             : sl.missing.isNotEmpty() ? rs::u8 ("NÃO ABRIU NESTA MÁQUINA") : juce::String();
    juce::String preset = inst != nullptr ? proc.slotPresetName (v) : juce::String();
    if (inst != nullptr && preset.isEmpty()) preset = "(preset do plugin)";
    const float cpu = inst != nullptr ? juce::jlimit (0.0f, 1.0f, sl.cpu.load()) : 0.0f;
    if (name != shownName || maker != shownMaker || preset != shownPreset || std::abs (cpu - shownCpu) > 0.04f)
    {
        shownName = name; shownMaker = maker; shownPreset = preset; shownCpu = cpu;
        repaint (0, 0, getWidth(), 54);
    }
    openBtn.setEnabled (inst != nullptr);
    swapBtn.setButtonText (inst != nullptr || sl.missing.isNotEmpty() ? "TROCAR" : "ESCOLHER");
    if (bypBtn.lit != sl.bypass.load()) { bypBtn.lit = sl.bypass.load(); bypBtn.repaint(); }
    for (auto& m : macros) m->refresh();
}

void PluginPanel::paint (juce::Graphics& g)
{
    const RF info (8.0f, 0.0f, (float) getWidth() - 16.0f, 50.0f);
    rs::screen (g, info, 4.0f);
    if (shownName.isEmpty())
    {
        rs::label (g, "NENHUM PLUGIN", info.withHeight (30.0f).translated (0.0f, 4.0f), juce::Justification::centred, 10.0f, colour);
        g.setFont (theme::mono (8.0f));
        g.setColour (rs::dim);
        g.drawText ("clique em ESCOLHER", info.withTrimmedTop (28.0f).withHeight (14.0f), juce::Justification::centred, false);
    }
    else
    {
        g.setFont (theme::sans (13.0f, true, 0.02f));
        g.setColour (proc.slot (v).missing.isNotEmpty() ? rs::faint : colour);
        g.drawFittedText (shownName, juce::Rectangle<int> (14, 3, (int) info.getWidth() - 50, 18), juce::Justification::centredLeft, 1, 0.7f);
        g.setFont (theme::mono (7.5f));
        g.setColour (proc.slot (v).missing.isNotEmpty() ? rs::orange : rs::dim);
        g.drawFittedText (shownMaker, juce::Rectangle<int> (14, 21, (int) info.getWidth() - 12, 11), juce::Justification::centredLeft, 1, 0.7f);
        g.setFont (theme::mono (8.5f));
        g.setColour (rs::ink);
        g.drawFittedText (juce::String::fromUTF8 ("▸ ") + shownPreset, juce::Rectangle<int> (14, 33, (int) info.getWidth() - 12, 13),
                          juce::Justification::centredLeft, 1, 0.7f);
        // CPU: quanto do tempo do bloco o plugin gasta
        g.setFont (theme::mono (6.5f));
        g.setColour (rs::dim);
        g.drawText ("CPU", RF (info.getRight() - 46.0f, 5.0f, 18.0f, 8.0f), juce::Justification::centredLeft, false);
        for (int i = 0; i < 4; ++i)
        {
            const bool on = shownCpu > 0.02f + (float) i * 0.15f;
            g.setColour (on ? (i == 3 ? rs::orange : colour) : rs::faint.withAlpha (0.6f));
            g.fillRoundedRectangle (info.getRight() - 26.0f + (float) i * 5.0f, 4.0f, 3.5f, 9.0f, 1.0f);
        }
    }
    rs::label (g, "KNOBS LIGADOS AO PLUGIN", { 8.0f, 82.0f, (float) getWidth() - 16.0f, 10.0f }, juce::Justification::centredLeft, 7.0f);
    rs::label (g, "NA CONVERSA", { 8.0f, 154.0f, (float) getWidth() - 16.0f, 10.0f }, juce::Justification::centredLeft, 7.0f);
}

// =====================================================================
// ESCOLHER PLUGIN
// =====================================================================
PluginPicker::PluginPicker (ResenhaProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (true, true);
    const auto a = panelArea;
    search.setTextToShowWhenEmpty (rs::u8 ("buscar pelo nome ou fabricante…"), rs::dim);
    search.setFont (theme::mono (12.0f));
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff05050f));
    search.setColour (juce::TextEditor::outlineColourId, rs::edge);
    search.setColour (juce::TextEditor::focusedOutlineColourId, rs::lime.withAlpha (0.6f));
    search.setColour (juce::TextEditor::textColourId, rs::ink);
    search.setIndents (10, 7);
    search.onTextChange = [this] { rebuild(); };
    search.onReturnKey = [this] { load(); };
    addAndMakeVisible (search);
    search.setBounds (a.getX() + 20, a.getY() + 52, 330, 30);

    instBtn.setBounds (a.getX() + 362, a.getY() + 54, 96, 26);
    allBtn.setBounds (a.getX() + 458, a.getY() + 54, 90, 26);
    favBtn.setBounds (a.getX() + 562, a.getY() + 54, 96, 26);
    azBtn.setBounds (a.getX() + 658, a.getY() + 54, 82, 26);
    for (auto* b : { &instBtn, &favBtn, &azBtn }) addAndMakeVisible (*b);   // TODOS saiu: a busca só guarda instrumentos
    instBtn.onClick = [this] { instrumentsOnly = true;  rebuild(); };
    favBtn.onClick  = [this] { favFirst = true;  rebuild(); };
    azBtn.onClick   = [this] { favFirst = false; rebuild(); };

    listBox.setModel (this);
    listBox.setRowHeight (28);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff05050f));
    listBox.setColour (juce::ListBox::outlineColourId, rs::edge);
    listBox.setOutlineThickness (1);
    addAndMakeVisible (listBox);
    listBox.setBounds (a.getX() + 20, a.getY() + 112, a.getWidth() - 40, 344);

    rescanBtn.setBounds (a.getX() + 20, a.getY() + 470, 210, 34);
    cancelBtn.setBounds (a.getRight() - 220, a.getY() + 470, 96, 34);
    loadBtn.setBounds (a.getRight() - 116, a.getY() + 470, 96, 34);
    for (auto* b : { &rescanBtn, &cancelBtn, &loadBtn }) addAndMakeVisible (*b);
    // durante a busca o botão vira PULAR ESTE PLUGIN: encerra o que está sendo lido (vai para os bloqueados) e segue
    rescanBtn.onClick = [this]
    {
        if (proc.catalog->scanning.load()) proc.catalog->skipCurrent();
        else proc.catalog->startScan (true);
        rebuild();
    };
    cancelBtn.onClick = [this] { close(); };
    loadBtn.onClick   = [this] { load(); };
}

void PluginPicker::open (int voiceIndex)
{
    v = voiceIndex;
    if (! proc.catalog->hasScanned() && ! proc.catalog->scanning.load())
        proc.catalog->startScan (false);   // 1ª vez: a busca começa sozinha
    seenVersion = -1;
    search.clear();
    rebuild();
    setVisible (true);
    toFront (true);
    search.grabKeyboardFocus();
    startTimerHz (8);
}

void PluginPicker::close()
{
    stopTimer();
    setVisible (false);
}

void PluginPicker::timerCallback()
{
    const int ver = proc.catalog->version.load();
    if (ver != seenVersion) rebuild();
    const auto label = proc.catalog->scanning.load() ? juce::String ("PULAR ESTE PLUGIN") : juce::String ("BUSCAR PLUGINS DE NOVO");
    if (rescanBtn.getButtonText() != label) rescanBtn.setButtonText (label);
    if (proc.catalog->scanning.load()) repaint (panelArea.getX(), panelArea.getY() + 460, panelArea.getWidth(), 100);
}

void PluginPicker::rebuild()
{
    seenVersion = proc.catalog->version.load();
    const auto q = search.getText().trim();
    rows.clear();
    for (const auto& d : proc.catalog->list())
        if (q.isEmpty() || d.name.containsIgnoreCase (q) || d.manufacturerName.containsIgnoreCase (q))
            rows.push_back ({ d, false, {} });
    auto& cat = *proc.catalog;
    std::stable_sort (rows.begin(), rows.end(), [this, &cat] (const Row& a, const Row& b)
    {
        if (favFirst)
        {
            const bool fa = cat.isFavourite (a.d), fb = cat.isFavourite (b.d);
            if (fa != fb) return fa;
        }
        return a.d.name.compareNatural (b.d.name) < 0;
    });
    for (const auto& f : cat.blocked())
    {
        const auto nm = juce::File::createFileWithoutCheckingPath (f).getFileNameWithoutExtension();
        if (q.isEmpty() || nm.containsIgnoreCase (q))
        {
            Row r;
            r.blocked = true;
            r.file = f;
            r.d.name = nm;
            rows.push_back (r);
        }
    }
    instBtn.lit = instBtn.filled = instrumentsOnly;   allBtn.lit = allBtn.filled = ! instrumentsOnly;
    favBtn.lit = favBtn.filled = favFirst;            azBtn.lit = azBtn.filled = ! favFirst;
    for (auto* b : { &instBtn, &allBtn, &favBtn, &azBtn }) b->repaint();
    listBox.updateContent();
    if (listBox.getSelectedRow() < 0 && ! rows.empty() && ! rows.front().blocked) listBox.selectRow (0);
    listBox.repaint();
    repaint();
}

void PluginPicker::load()
{
    const int r = listBox.getSelectedRow();
    if (r < 0 || r >= (int) rows.size() || rows[(size_t) r].blocked) return;
    const auto d = rows[(size_t) r].d;
    const auto err = proc.loadSlotPlugin (v, d);
    close();
    if (err.isNotEmpty() && onError)
        onError (rs::u8 ("Não deu para carregar"), d.name + ": " + err);
}

void PluginPicker::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const auto& r = rows[(size_t) row];
    const auto col = rs::voiceColour (v);
    if (selected && ! r.blocked)
    {
        g.setColour (col.withAlpha (0.12f));
        g.fillRect (0, 0, w, h);
        g.setColour (col);
        g.fillRect (0, 0, 3, h);
    }
    g.setColour (rs::edge.withAlpha (0.5f));
    g.fillRect (0, h - 1, w, 1);
    const auto txt = r.blocked ? rs::faint : rs::ink;
    if (! r.blocked && proc.catalog->isFavourite (r.d))
    {
        g.setColour (rs::amber);
        g.setFont (theme::sans (13.0f, true));
        g.drawText (juce::String::fromUTF8 ("★"), 8, 0, 18, h, juce::Justification::centred, false);
    }
    else if (! r.blocked)
    {
        g.setColour (rs::faint);
        g.setFont (theme::sans (13.0f, false));
        g.drawText (juce::String::fromUTF8 ("☆"), 8, 0, 18, h, juce::Justification::centred, false);
    }
    g.setFont (theme::sans (12.5f, selected));
    g.setColour (txt);
    g.drawFittedText (r.d.name, 36, 0, 230, h, juce::Justification::centredLeft, 1, 0.7f);
    g.setFont (theme::sans (11.5f, false));
    g.setColour (r.blocked ? rs::faint : rs::ink.withAlpha (0.8f));
    g.drawFittedText (r.blocked ? juce::String() : r.d.manufacturerName, 272, 0, 190, h, juce::Justification::centredLeft, 1, 0.7f);
    g.setFont (theme::mono (9.5f));
    g.setColour (rs::dim);
    g.drawText (r.blocked ? juce::String() : r.d.pluginFormatName, 470, 0, 70, h, juce::Justification::centredLeft, false);
    g.drawText (r.blocked ? juce::String() : (r.d.isInstrument ? juce::String ("Instrumento") : juce::String ("Efeito")), 545, 0, 90, h,
                juce::Justification::centredLeft, false);
    g.setColour (r.blocked ? juce::Colour (0xffff4d5e) : rs::lime);
    juce::String sit = juce::String::fromUTF8 ("✓ ok");
    if (r.blocked)
    {
        const auto why = proc.catalog->reasonFor (r.file);   // travou / fechou / não abriu / pulado
        sit = juce::String::fromUTF8 ("✗ ") + (why.isNotEmpty() ? why : juce::String ("bloqueado"));
    }
    g.drawFittedText (sit, 640, 0, w - 644, h, juce::Justification::centredLeft, 1, 0.7f);
}

void PluginPicker::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) rows.size() || rows[(size_t) row].blocked) return;
    if (e.x < 30)   // a estrela
    {
        proc.catalog->toggleFavourite (rows[(size_t) row].d);
        rebuild();
    }
}

void PluginPicker::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    listBox.selectRow (row);
    load();
}

void PluginPicker::mouseDown (const juce::MouseEvent& e)
{
    // o X do canto e o clique fora não fecham sem querer: só o X fecha
    const juce::Rectangle<int> x (panelArea.getRight() - 40, panelArea.getY() + 12, 28, 28);
    if (x.contains (e.getPosition())) close();
}

void PluginPicker::resized() {}

void PluginPicker::paint (juce::Graphics& g)
{
    g.fillAll (rs::bg0.withAlpha (0.78f));
    const auto a = panelArea.toFloat();
    const auto col = rs::voiceColour (v);
    rs::glow (g, a, 10.0f, col, 0.6f);
    rs::box (g, a, 10.0f, rs::edge);
    g.setColour (col);
    g.fillRoundedRectangle (a.getX() + 10.0f, a.getY(), a.getWidth() - 20.0f, 2.5f, 1.0f);

    g.setFont (theme::sans (14.0f, true, 0.28f));
    g.setColour (rs::ink);
    const juce::String t1 = juce::String::fromUTF8 ("ESCOLHER PLUGIN · ");
    const float w1 = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t1);
    g.drawText (t1, RF (a.getX() + 20.0f, a.getY() + 16.0f, w1 + 4.0f, 22.0f), juce::Justification::centredLeft, false);
    g.setColour (col);
    g.drawText ("SLOT " + juce::String::charToString ((juce::juce_wchar) ('A' + v)), RF (a.getX() + 24.0f + w1, a.getY() + 16.0f, 120.0f, 22.0f),
                juce::Justification::centredLeft, false);
    // X
    const RF x (a.getRight() - 40.0f, a.getY() + 12.0f, 28.0f, 28.0f);
    rs::screen (g, x, 4.0f);
    g.setColour (rs::ink);
    g.drawLine (x.getX() + 9.0f, x.getY() + 9.0f, x.getRight() - 9.0f, x.getBottom() - 9.0f, 1.6f);
    g.drawLine (x.getRight() - 9.0f, x.getY() + 9.0f, x.getX() + 9.0f, x.getBottom() - 9.0f, 1.6f);

    // cabeçalho da lista
    const float hy = a.getY() + 94.0f;
    rs::label (g, "NOME",       { a.getX() + 56.0f,  hy, 200.0f, 12.0f }, juce::Justification::centredLeft, 8.0f);
    rs::label (g, "FABRICANTE", { a.getX() + 292.0f, hy, 180.0f, 12.0f }, juce::Justification::centredLeft, 8.0f);
    rs::label (g, "FORMATO",    { a.getX() + 490.0f, hy, 70.0f, 12.0f },  juce::Justification::centredLeft, 8.0f);
    rs::label (g, "TIPO",       { a.getX() + 565.0f, hy, 70.0f, 12.0f },  juce::Justification::centredLeft, 8.0f);
    rs::label (g, rs::u8 ("SITUAÇÃO"), { a.getX() + 660.0f, hy, 80.0f, 12.0f }, juce::Justification::centredLeft, 8.0f);

    // situação da busca / explicação
    auto& cat = *proc.catalog;
    const RF info (a.getX() + 240.0f, a.getY() + 466.0f, a.getWidth() - 240.0f - 232.0f, 42.0f);
    g.setFont (theme::sans (9.5f, false, 0.02f));
    if (cat.scanning.load())
    {
        g.setColour (rs::lime);
        const auto nm = cat.scanningName();
        g.drawFittedText ("BUSCANDO PLUGINS... " + juce::String (juce::roundToInt (cat.progress.load() * 100.0f)) + "%"
                              + (nm.isNotEmpty() ? juce::String::fromUTF8 (" · ") + nm : juce::String()),
                          info.toNearestInt(), juce::Justification::centredLeft, 3, 0.8f);
    }
    else if (rows.empty())
    {
        g.setColour (rs::orange);
        g.drawFittedText (rs::u8 ("Nenhum plugin na lista. Clique em BUSCAR PLUGINS DE NOVO (a busca olha as pastas padrão do VST3 e, no Mac, do AU)."),
                          info.toNearestInt(), juce::Justification::centredLeft, 3, 0.8f);
    }
    else
    {
        g.setColour (rs::dim);
        g.drawFittedText (rs::u8 ("Busca nas pastas padrão do VST3 (Windows) e do AU/VST3 (Mac). Só entram instrumentos: os efeitos são descartados sem abrir e não são lidos de novo. A 1ª busca pode levar alguns minutos; "
                                  "depois a lista fica guardada. Clique na estrela para favoritar."),
                          info.toNearestInt(), juce::Justification::centredLeft, 3, 0.8f);
    }
    g.setFont (theme::sans (9.0f, false, 0.02f));
    g.setColour (rs::dim);
    g.drawFittedText (cat.unprotected.load()
                          ? rs::u8 ("ATENÇÃO: o leitor de plugins do RESENHA (ResenhaScan) não rodou nesta máquina; a busca está abrindo os plugins dentro do DAW e um plugin que travar pode prender o DAW.")
                          : rs::u8 ("Cada plugin é lido fora do DAW: se travar (30 s), fechar ou não abrir, vai para os bloqueados e a busca segue. "
                                    "PULAR ESTE PLUGIN encerra o atual na hora. O RESENHA não aparece na própria lista."),
                      juce::Rectangle<int> ((int) a.getX() + 20, (int) a.getY() + 516, (int) a.getWidth() - 40, 30),
                      juce::Justification::centredLeft, 2, 0.8f);
}

// =====================================================================
// CONVERSA (piano-roll)
// =====================================================================
void ConversationView::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    rs::box (g, all, 8.0f);

    const auto pp = proc.phraseParams();
    buildPhrase (proc.pattern, pp, phrase);
    auto& st = proc.apvts;
    auto choiceText = [&st] (const juce::String& pid) { return st.getParameter (pid)->getCurrentValueAsText(); };

    // cabeçalho: tamanho da frase e a escala
    const int bars = barCounts[juce::jlimit (0, 2, juce::roundToInt (st.getRawParameterValue (id::bars)->load()))];
    g.setFont (theme::sans (10.0f, true, 0.16f));
    g.setColour (rs::cyan);
    g.drawText (juce::String::fromUTF8 ("FRASE · ") + juce::String (bars) + (bars == 1 ? " COMPASSO" : " COMPASSOS")
                    + juce::String::fromUTF8 (" · ") + choiceText (id::rate),
                RF (14.0f, 8.0f, 240.0f, 16.0f), juce::Justification::centredLeft, false);
    g.setColour (rs::magenta);
    g.drawText (choiceText (id::root) + " " + choiceText (id::scale), RF (all.getRight() - 170.0f, 8.0f, 156.0f, 16.0f),
                juce::Justification::centredRight, false);

    const RF roll (44.0f, 30.0f, all.getWidth() - 56.0f, all.getHeight() - 96.0f);
    rs::screen (g, roll, 3.0f);

    // faixa de notas: o que a frase usa, com folga (no mínimo uma oitava)
    int lo = 127, hi = 0;
    for (int i = 0; i < phrase.count; ++i)
    {
        lo = juce::jmin (lo, phrase.notes[(size_t) i].note);
        hi = juce::jmax (hi, phrase.notes[(size_t) i].note);
    }
    if (phrase.count == 0) { lo = 60; hi = 72; }
    lo -= 1; hi += 1;
    if (hi - lo < 12) { const int mid = (hi + lo) / 2; lo = mid - 6; hi = mid + 6; }
    const int rows = hi - lo + 1;
    const float rh = roll.getHeight() / (float) rows;
    const float cw = roll.getWidth() / (float) phrase.steps;
    const int scale = juce::roundToInt (st.getRawParameterValue (id::scale)->load());
    const int root = juce::roundToInt (st.getRawParameterValue (id::root)->load());

    for (int n = lo; n <= hi; ++n)
    {
        const float y = roll.getBottom() - (float) (n - lo + 1) * rh;
        if (! scales::contains (n, scale, root))
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRect (roll.getX(), y, roll.getWidth(), rh);
        }
        if (((n - root) % 12 + 12) % 12 == 0)
        {
            g.setColour (rs::violet.withAlpha (0.12f));
            g.fillRect (roll.getX(), y, roll.getWidth(), rh);
            g.setFont (theme::mono (8.0f));
            g.setColour (rs::dim);
            g.drawText (noteName (n), RF (6.0f, y + rh * 0.5f - 6.0f, 36.0f, 12.0f), juce::Justification::centredLeft, false);
        }
    }

    // grade: passo, tempo, compasso
    const double sb = rateBeats[juce::jlimit (0, 4, juce::roundToInt (st.getRawParameterValue (id::rate)->load()))];
    for (int i = 0; i <= phrase.steps; ++i)
    {
        const double beat = i * sb;
        const bool bar = std::abs (beat / 4.0 - std::round (beat / 4.0)) < 1e-6;
        const bool onBeat = std::abs (beat - std::round (beat)) < 1e-6;
        g.setColour (bar ? rs::edge.brighter (0.4f) : onBeat ? rs::edge : rs::edge.withAlpha (0.35f));
        g.fillRect (roll.getX() + cw * (float) i, roll.getY(), bar ? 1.4f : 1.0f, roll.getHeight());
    }

    // notas: cor do timbre; a resposta com contorno claro
    for (int i = 0; i < phrase.count; ++i)
    {
        const auto& pn = phrase.notes[(size_t) i];
        const auto c = rs::voiceColour (pn.voice);
        const RF nr (roll.getX() + cw * (float) pn.step + 1.0f, roll.getBottom() - (float) (pn.note - lo + 1) * rh + 1.0f,
                     juce::jmax (3.0f, cw * ((float) (pn.len - 1) + pn.gate) - 2.0f), juce::jmax (2.0f, rh - 2.0f));
        g.setColour (c.withAlpha (0.25f));
        g.fillRoundedRectangle (nr.expanded (2.0f), 3.0f);
        g.setColour (pn.accent ? c.brighter (0.3f) : c.withAlpha (0.85f));
        g.fillRoundedRectangle (nr, 2.0f);
        if (pn.response)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawRoundedRectangle (nr, 2.0f, 0.8f);
        }

        // BEND: a nota que fecha a fala mergulha (ou sobe) — uma cauda curva até onde a altura chega
        const float bend = proc.apvts.getRawParameterValue (id::v (pn.voice, id::bend))->load();
        if (pn.last && std::abs (bend) >= 0.5f)
        {
            const float y0 = nr.getCentreY();
            const float y1 = juce::jlimit (roll.getY() + 2.0f, roll.getBottom() - 2.0f, y0 - bend * rh);
            const float x0 = nr.getX() + juce::jmin (4.0f, nr.getWidth() * 0.4f);
            const float x1 = juce::jmin (roll.getRight() - 1.0f, nr.getRight() + juce::jmax (8.0f, cw * 0.6f));
            juce::Path tail;
            tail.startNewSubPath (x0, y0);
            tail.cubicTo (x0 + (x1 - x0) * 0.55f, y0, x1 - (x1 - x0) * 0.15f, y0 + (y1 - y0) * 0.35f, x1, y1);
            g.setColour (c.withAlpha (0.3f));
            g.strokePath (tail, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (c.brighter (0.4f));
            g.strokePath (tail, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // cabeça de leitura
    const int ps = proc.playStep.load();
    if (ps >= 0 && ps < phrase.steps)
    {
        const float x = roll.getX() + cw * ((float) ps + 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRect (x - 3.0f, roll.getY(), 6.0f, roll.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillRect (x - 0.75f, roll.getY(), 1.5f, roll.getHeight());
    }

    // FALA: quem tem a vez, em blocos (troca de timbre ou de pergunta/resposta = bloco novo)
    const RF lane (roll.getX(), roll.getBottom() + 8.0f, roll.getWidth(), 18.0f);
    g.setFont (theme::sans (8.0f, true, 0.14f));
    g.setColour (rs::dim);
    g.drawText ("FALA", RF (6.0f, lane.getY(), 36.0f, lane.getHeight()), juce::Justification::centredLeft, false);
    const auto respName = responseNames[juce::jlimit (0, 3, (int) pp.response)];
    for (int i = 0; i < phrase.count;)
    {
        const auto& a = phrase.notes[(size_t) i];
        int j = i + 1;
        while (j < phrase.count && phrase.notes[(size_t) j].voice == a.voice && phrase.notes[(size_t) j].response == a.response) ++j;
        const int endStep = j < phrase.count ? phrase.notes[(size_t) j].step : phrase.steps;
        const RF blk (lane.getX() + cw * (float) a.step + 1.0f, lane.getY(), cw * (float) (endStep - a.step) - 2.0f, lane.getHeight());
        const auto c = rs::voiceColour (a.voice);
        g.setColour (c.withAlpha (a.response ? 0.55f : 0.85f));
        g.fillRoundedRectangle (blk, 3.0f);
        juce::String t = juce::String::charToString ((juce::juce_wchar) ('A' + a.voice));
        if (a.response && pp.response != Response::free && blk.getWidth() > 34.0f) t << " " << respName;
        g.setColour (rs::bg0);
        g.setFont (theme::sans (9.0f, true, 0.06f));
        g.drawFittedText (t, blk.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        i = j;
    }

    // a conversa em palavras
    juce::String who;
    for (int k = 0; k < pp.voices; ++k) who << juce::String::charToString ((juce::juce_wchar) ('A' + k));
    juce::String desc;
    if (pp.voices <= 1)
        desc = juce::String::fromUTF8 ("Só o A fala (monólogo) · suba VOZES NA CONVERSA para ter resposta");
    else
    {
        desc << who.substring (0, 1) << juce::String::fromUTF8 (" pergunta ");
        if (pp.every < 0) desc << (pp.every == -2 ? juce::String::fromUTF8 ("meio compasso") : juce::String ("1 compasso"));
        else desc << pp.every << (pp.every == 1 ? " nota" : " notas");
        desc << juce::String::fromUTF8 (" · ");
        if (pp.response == Response::free) desc << juce::String::fromUTF8 ("a resposta toca o que está escrito");
        else desc << juce::String::fromUTF8 ("a resposta faz ") << respName;
        desc << " (" << choiceText (id::interval) << ")" << juce::String::fromUTF8 (" · ordem ") << choiceText (id::order);
    }
    g.setFont (theme::mono (9.5f));
    g.setColour (rs::ink.withAlpha (0.8f));
    g.drawFittedText (desc, juce::Rectangle<int> (14, (int) lane.getBottom() + 6, (int) all.getWidth() - 28, 16),
                      juce::Justification::centredLeft, 1, 0.7f);
}

// =====================================================================
// ARRASTAR MIDI e cadeado dos timbres
// =====================================================================
void MidiDragButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true;
    const auto file = proc.writeMidiFile();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (file.getFullPathName()), false, this);
}

LockAllButton::LockAllButton (ResenhaProcessor& p) : juce::Button ("lockall"), proc (p)
{
    onClick = [this]
    {
        const float to = allLocked() ? 0.0f : 1.0f;
        for (int v = 0; v < numVoices; ++v)
            if (auto* prm = proc.apvts.getParameter (id::lockVoice (v)))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (to);
                prm->endChangeGesture();
            }
    };
}

bool LockAllButton::allLocked() const
{
    for (int v = 0; v < numVoices; ++v)
        if (proc.apvts.getRawParameterValue (id::lockVoice (v))->load() < 0.5f) return false;
    return true;
}

void LockAllButton::paintButton (juce::Graphics& g, bool over, bool)
{
    int n = 0;
    for (int v = 0; v < numVoices; ++v)
        if (proc.apvts.getRawParameterValue (id::lockVoice (v))->load() > 0.5f) ++n;
    const auto col = n == numVoices ? rs::amber : n > 0 ? rs::amber.withAlpha (0.55f) : over ? rs::ink : rs::dim;
    rs::lockIcon (g, getLocalBounds().toFloat().reduced ((float) getHeight() * 0.18f), col, n == numVoices);
}

// =====================================================================
// EDITOR
// =====================================================================
ResenhaEditor::ResenhaEditor (ResenhaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, rs::baseW, rs::baseH);
    addAndMakeVisible (canvas);

    auto& st = proc.apvts;
    using rs::NeonKnob;
    using rs::NeonButton;

    // ---- cabeçalho: preset ----
    presetName = add<rs::NeonPresetName> ({ 700, 24, 250, 38 });
    presetName->onClick = [this] { showPresetMenu(); };
    auto* prev = add<NeonButton> ({ 956, 24, 38, 38 }, juce::String(), NeonButton::Icon::prev);
    auto* next = add<NeonButton> ({ 998, 24, 38, 38 }, juce::String(), NeonButton::Icon::next);
    auto* save = add<NeonButton> ({ 1040, 24, 38, 38 }, juce::String(), NeonButton::Icon::save);
    prev->onClick = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    next->onClick = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    save->onClick = [this] { askNewPreset(); };

    // ---- os 4 timbres ----
    for (int v = 0; v < numVoices; ++v)
    {
        cards[(size_t) v] = add<VoiceCard> ({ 18 + v * 190, 108, 182, 302 }, proc, v);
        cards[(size_t) v]->onChoosePlugin = [this] (int slotIndex) { picker->open (slotIndex); };
    }

    conv = add<ConversationView> ({ 790, 110, 392, 300 }, proc);

    // ---- gerador de frase ----
    add<rs::ChoiceBox> ({ 22, 458, 120, 26 }, st, id::scale, rs::ink);
    add<rs::ChoiceBox> ({ 148, 458, 56, 26 }, st, id::root, rs::ink);
    add<rs::ChoiceBox> ({ 210, 458, 62, 26 }, st, id::bars, rs::ink);
    add<rs::ChoiceBox> ({ 278, 458, 66, 26 }, st, id::rate, rs::ink);
    add<NeonKnob> ({ 350, 440, 50, 60 }, st, id::octaves, "OITAVAS",   rs::cyan,    32.0f);
    add<NeonKnob> ({ 400, 440, 52, 60 }, st, id::density, "DENSIDADE", rs::cyan,    32.0f);
    add<NeonKnob> ({ 452, 440, 50, 60 }, st, id::gate,    "GATE",      rs::magenta, 32.0f);
    add<NeonKnob> ({ 502, 440, 52, 60 }, st, id::gateVar, "VAR. GATE", rs::magenta, 32.0f);
    add<NeonKnob> ({ 554, 440, 50, 60 }, st, id::swing,   "SWING",     rs::violet,  32.0f);
    add<rs::Segmented> ({ 22, 508, 322, 26 }, st, id::contour, rs::lime);

    // ---- troca de timbre ----
    add<rs::Segmented> ({ 618, 458, 140, 26 }, st, id::voices,   rs::magenta);
    add<rs::Segmented> ({ 768, 458, 196, 26 }, st, id::every,    rs::cyan);
    add<rs::Segmented> ({ 974, 458, 208, 26 }, st, id::response, rs::orange);
    add<rs::Segmented> ({ 618, 508, 230, 26 }, st, id::order,    rs::lime);
    add<rs::Segmented> ({ 858, 508, 252, 26 }, st, id::interval, rs::violet);
    add<NeonKnob> ({ 1120, 482, 62, 62 }, st, id::talk, "PAPO", rs::orange, 30.0f);

    // ---- sorteios ----
    struct Part { const char* text; int part; const juce::String* lockId; };
    const Part parts[] = { { "NOTAS", ResenhaProcessor::partNotes, &id::lockNotes },
                           { "RITMO", ResenhaProcessor::partRhythm, &id::lockRhythm },
                           { "GATE", ResenhaProcessor::partGate, &id::lockGate },
                           { "TIMBRES", ResenhaProcessor::partTimbres, nullptr } };
    for (int i = 0; i < 4; ++i)
    {
        const int x = 18 + i * 124;
        auto* b = add<NeonButton> ({ x, 556, 118, 40 }, juce::String (parts[i].text), NeonButton::Icon::dice);
        b->padRight = 26.0f;   // o cadeado fica em x+88..x+112
        const int part = parts[i].part;
        if (parts[i].lockId != nullptr)
        {
            b->onClick = [this, part] { proc.generate (part, false); };
            auto* lk = add<rs::LockButton> ({ x + 88, 564, 24, 24 });
            attach (*lk, *parts[i].lockId);
        }
        else
        {
            lockAll = add<LockAllButton> ({ x + 88, 564, 24, 24 }, proc);
            // TIMBRES respeita os cadeados dos cartões; com os 4 travados, sorteia todos mesmo assim
            b->onClick = [this, part] { proc.generate (part, ! lockAll->allLocked()); };
        }
    }
    auto* gen = add<NeonButton> ({ 518, 552, 230, 48 }, "GERAR CONVERSA", NeonButton::Icon::dice);
    gen->hero = true;
    gen->onClick = [this] { proc.generate (ResenhaProcessor::partAll, true); };
    undoBtn = add<NeonButton> ({ 756, 556, 84, 40 }, juce::String::fromUTF8 ("DESF."), NeonButton::Icon::undo, rs::orange);
    undoBtn->onClick = [this] { proc.undo(); };
    add<MidiDragButton> ({ 846, 556, 134, 40 }, proc);
    add<rs::Segmented> ({ 990, 556, 192, 40 }, st, id::play, rs::cyan);

    // ---- efeitos ----
    add<rs::ChoiceBox> ({ 22, 664, 90, 26 }, st, id::drvType, rs::orange);
    add<NeonKnob> ({ 122, 640, 62, 70 }, st, id::drive, "DRIVE", rs::orange, 40.0f);
    add<NeonKnob> ({ 186, 640, 62, 70 }, st, id::tone,  "TONE",  rs::amber,  40.0f);

    auto* dl = add<rs::NeonSwitch> ({ 300, 652, 40, 40 }, rs::cyan);
    attach (*dl, id::dlOn);
    add<NeonKnob> ({ 352, 640, 62, 70 }, st, id::dlDiv, "TIME",     rs::cyan, 40.0f);
    add<NeonKnob> ({ 416, 640, 62, 70 }, st, id::dlFb,  "FEEDBACK", rs::cyan, 40.0f);
    add<NeonKnob> ({ 480, 640, 62, 70 }, st, id::dlMix, "MIX",      rs::cyan, 40.0f);

    auto* rv = add<rs::NeonSwitch> ({ 620, 652, 40, 40 }, rs::violet);
    attach (*rv, id::rvOn);
    add<NeonKnob> ({ 672, 640, 62, 70 }, st, id::rvSize, "SIZE", rs::violet, 40.0f);
    add<NeonKnob> ({ 736, 640, 62, 70 }, st, id::rvMix,  "MIX",  rs::violet, 40.0f);

    add<NeonKnob> ({ 900, 640, 70, 70 }, st, id::volume, "VOLUME", rs::ink, 40.0f, true);
    add<NeonKnob> ({ 984, 640, 62, 70 }, st, id::duck,   "DUCK",   rs::magenta, 40.0f);

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) rs::baseW * rs::minScale), juce::roundToInt ((float) rs::baseH * rs::minScale),
                     juce::roundToInt ((float) rs::baseW * rs::maxScale), juce::roundToInt ((float) rs::baseH * rs::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) rs::baseW / (double) rs::baseH);
    setScale (openScale);

    // ESCOLHER PLUGIN: por cima de tudo (menos da tela de ativação)
    picker = std::make_unique<PluginPicker> (proc);
    picker->onError = [this] (const juce::String& title, const juce::String& text) { dialog.inform (canvas, title, text); };
    canvas.addChildComponent (*picker);
    picker->setBounds (0, 0, rs::baseW, rs::baseH);

    startTimerHz (30);
    ativacao.anexar (*this);
}

ResenhaEditor::~ResenhaEditor()
{
    dialog.close();
    stopTimer();
    proc.closeSlotWindows();   // as janelas dos plugins fecham junto com a do RESENHA
    setLookAndFeel (nullptr);
}

void ResenhaEditor::attach (juce::Button& b, const juce::String& paramId)
{
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramId, b));
}

// ---------------- menu de presets (com o tamanho da janela) ----------------
void ResenhaEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto names = proc.getPresetNames();
    const auto bankNames = proc.getBankNames();
    const int  cur = proc.getPresetIndex();
    const int  numBanks = bankNames.size();

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
        else if (b == ResenhaProcessor::bankUser)
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

    menu.addSeparator();
    menu.addItem (menuSaveAs, u8 ("Salvar como novo preset…"));
    if (proc.isUserPreset())
    {
        const auto name = proc.getPresetName();
        menu.addItem (menuOverwrite, u8 ("Salvar alterações em “") + name + u8 ("”"));
        menu.addItem (menuRename,    u8 ("Renomear “") + name + u8 ("”…"));
        menu.addItem (menuDelete,    u8 ("Apagar “") + name + u8 ("”…"));
    }

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
    menu.addItem (menuLicenca, u8 ("Licença…"));
   #ifdef JucePlugin_VersionString
    menu.addSeparator();
    menu.addItem (sizeMenuBase - 1, juce::String ("RESENHA ") + JucePlugin_VersionString, false, false);
   #endif

    juce::Component::SafePointer<ResenhaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetName).withParentComponent (this), [safe] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->presetMenuChosen (result);
    });
}

void ResenhaEditor::askNewPreset()
{
    juce::Component::SafePointer<ResenhaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void ResenhaEditor::importBank()
{
    auto start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    if (start.getChildFile ("Downloads").isDirectory()) start = start.getChildFile ("Downloads");
    chooser = std::make_unique<juce::FileChooser> (u8 ("Importar banco de presets do RESENHA"), start, "*.rsnbank");

    juce::Component::SafePointer<ResenhaEditor> safe (this);
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

void ResenhaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<ResenhaEditor> safe (this);
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
        case menuSaveAs:    askNewPreset(); break;
        case menuLicenca:   ativacao.mostrarInfo(); break;
        case menuOverwrite: proc.overwriteUserPreset(); break;
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
        case menuImport:    importBank(); break;
        case menuNoUser:    break;
        default:
            if (result > 0 && result < sizeMenuBase)
                proc.loadPreset (result - 1);
            break;
    }
}

// ---------------- timer (30 Hz) ----------------
void ResenhaEditor::timerCallback()
{
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) rs::baseW - openScale) > 0.005f)
        setScale (openScale);

    const auto pp = proc.phraseParams();
    for (int v = 0; v < numVoices; ++v)
    {
        auto* c = cards[(size_t) v];
        const bool silentNow = v >= pp.voices;
        c->setSilent (silentNow);
        c->setRole (silentNow ? juce::String::fromUTF8 ("EM SILÊNCIO") : juce::String::fromUTF8 (roleNames[v]));
        c->setTalk (juce::jmax (proc.engine.talk[(size_t) v].load(), proc.slot (v).level.load()));
        c->refresh();
    }
    proc.pollLearn();
    conv->repaint();
    lockAll->repaint();
    undoBtn->setEnabled (proc.canUndo());
    undoBtn->setAlpha (proc.canUndo() ? 1.0f : 0.45f);

    level = juce::jmax (proc.meterOut.exchange (0.0f), level * 0.85f);
    if (std::abs (level - shownLevel) > 0.002f)
    {
        shownLevel = level;
        canvas.repaint (1084, 20, 104, 50);
    }

    presetName->set (proc.getPresetName(), proc.getPresetIndex(), proc.getNumPresets());

    const float bpm = proc.shownBpm.load();
    const int state = proc.synced.load() ? (proc.waitingKey.load() ? 2 : 1) : 0;
    if (std::abs (bpm - lastBpm) > 0.05f || state != lastState)
    {
        lastBpm = bpm;
        lastState = state;
        canvas.repaint (520, 18, 176, 52);
    }
}

// ---------------- tamanho ----------------
void ResenhaEditor::setScale (float scale)
{
    scale = juce::jlimit (rs::minScale, rs::maxScale, scale);
    setSize (juce::roundToInt ((float) rs::baseW * scale), juce::roundToInt ((float) rs::baseH * scale));
}

void ResenhaEditor::resized()
{
    const float scale = (float) getWidth() / (float) rs::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)
        proc.uiScale = scale;
}

void ResenhaEditor::paint (juce::Graphics& g)
{
    g.fillAll (rs::bg0);
}

// ---------------- o painel ----------------
void ResenhaEditor::drawCanvas (juce::Graphics& g)
{
    const float W = (float) rs::baseW, H = (float) rs::baseH;

    // fundo: azul-noite descendo para violeta, com ondas concêntricas bem fracas (psicodelia sem roubar a cena)
    g.setGradientFill (juce::ColourGradient (rs::bg0, 0.0f, 0.0f, rs::bg1, 0.0f, H, false));
    g.fillAll();
    for (int i = 1; i < 22; ++i)
    {
        const float rr = (float) i * 34.0f;
        g.setColour ((i % 3 == 0 ? rs::magenta : i % 3 == 1 ? rs::violet : rs::cyan).withAlpha (0.035f));
        g.drawEllipse (W * 0.72f - rr, H * 0.78f - rr, rr * 2.0f, rr * 2.0f, 1.2f);
    }
    g.setColour (rs::edge);
    g.drawRoundedRectangle (RF (1.0f, 1.0f, W - 2.0f, H - 2.0f), 10.0f, 1.5f);

    drawHeader (g);

    rs::sectionTitle (g, 22.0f, 88.0f, 750.0f, juce::String::fromUTF8 ("SLOTS · QUEM CONVERSA"));
    rs::sectionTitle (g, 790.0f, 88.0f, 392.0f, "CONVERSA");

    // ---- gerador ----
    rs::sectionTitle (g, 22.0f, 418.0f, 582.0f, "GERADOR DE FRASE");
    rs::label (g, "ESCALA",    { 22.0f, 444.0f, 120.0f, 12.0f }, juce::Justification::centred);
    rs::label (g, juce::String::fromUTF8 ("TÔNICA"), { 148.0f, 444.0f, 56.0f, 12.0f }, juce::Justification::centred);
    rs::label (g, "COMPASSOS", { 206.0f, 444.0f, 70.0f, 12.0f }, juce::Justification::centred);
    rs::label (g, "RATE",      { 278.0f, 444.0f, 66.0f, 12.0f }, juce::Justification::centred);
    rs::label (g, "CONTORNO DAS NOTAS", { 22.0f, 494.0f, 322.0f, 12.0f });

    // ---- troca de timbre ----
    rs::sectionTitle (g, 618.0f, 418.0f, 564.0f, "TROCA DE TIMBRE");
    rs::label (g, "VOZES NA CONVERSA",  { 618.0f, 444.0f, 140.0f, 12.0f });
    rs::label (g, "TROCA A CADA (NOTAS / COMPASSO)", { 768.0f, 444.0f, 196.0f, 12.0f });
    rs::label (g, "RESPOSTA",           { 974.0f, 444.0f, 208.0f, 12.0f });
    rs::label (g, "ORDEM",              { 618.0f, 494.0f, 230.0f, 12.0f });
    rs::label (g, "ALTURA DA RESPOSTA", { 858.0f, 494.0f, 252.0f, 12.0f });
    rs::label (g, "TOCAR",              { 990.0f, 541.0f, 192.0f, 12.0f }, juce::Justification::centred);

    g.setFont (theme::sans (9.0f, false, 0.03f));
    g.setColour (rs::dim);
    g.drawText (juce::String::fromUTF8 ("SLOT PLUGIN: ABRIR = janela do plugin  ·  + LIGAR = clique e mexa um controle na janela do plugin  ·  "
                                        "TIMBRES sorteia um preset do plugin  ·  COM NOTA: segure uma nota no DAW (Dó 3 = original)"),
                RF (22.0f, 602.0f, 1160.0f, 14.0f), juce::Justification::centredLeft, true);

    // ---- efeitos ----
    rs::sectionTitle (g, 22.0f, 622.0f, 236.0f, "DRIVE");
    rs::label (g, "TIPO", { 22.0f, 650.0f, 90.0f, 12.0f }, juce::Justification::centred);
    rs::sectionTitle (g, 300.0f, 622.0f, 280.0f, "DELAY PING-PONG");
    rs::sectionTitle (g, 620.0f, 622.0f, 230.0f, "REVERB");
    rs::sectionTitle (g, 890.0f, 622.0f, 292.0f, juce::String::fromUTF8 ("SAÍDA"));
}

void ResenhaEditor::drawHeader (juce::Graphics& g)
{
    const RF plate (12.0f, 10.0f, (float) rs::baseW - 24.0f, 66.0f);
    rs::box (g, plate, 8.0f, rs::edge, 0.85f);
    g.setGradientFill (rs::brandGradient (20.0f, (float) rs::baseW - 20.0f, 0.0f));
    g.fillRect (20.0f, 78.0f, (float) rs::baseW - 40.0f, 2.0f);

    drawLogo (g, { 28.0f, 20.0f, 52.0f, 46.0f });

    // RESENHA, cada letra num ponto do gradiente da marca
    const juce::String title ("RESENHA");
    g.setFont (theme::sans (34.0f, true, 0.0f));
    float x = 94.0f;
    for (int i = 0; i < title.length(); ++i)
    {
        const auto ch = title.substring (i, i + 1);
        const float cw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), ch);
        const auto col = rs::brand ((float) i / (float) (title.length() - 1));
        g.setColour (col.withAlpha (0.25f));
        g.drawText (ch, RF (x - 1.0f, 19.0f, cw + 4.0f, 50.0f), juce::Justification::centredLeft, false);
        g.setColour (col);
        g.drawText (ch, RF (x, 18.0f, cw + 4.0f, 50.0f), juce::Justification::centredLeft, false);
        x += cw + 5.0f;
    }
    // selo HUB: cada slot pode tocar um plugin seu
    {
        const RF badge (x + 6.0f, 32.0f, 42.0f, 20.0f);
        g.setColour (rs::amber);
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (rs::bg0);
        g.setFont (theme::sans (11.0f, true, 0.2f));
        g.drawText ("HUB", badge, juce::Justification::centred, false);
        x += 54.0f;
    }
    g.setColour (rs::edge.brighter (0.3f));
    g.fillRect (x + 6.0f, 28.0f, 1.0f, 30.0f);
    g.setFont (theme::sans (9.5f, true, 0.3f));
    g.setColour (rs::ink.withAlpha (0.85f));
    g.drawText (juce::String::fromUTF8 ("CALL & RESPONSE · 4 SLOTS"), RF (x + 18.0f, 28.0f, 200.0f, 13.0f), juce::Justification::centredLeft);
    g.drawText ("INTERNO OU SEU PLUGIN", RF (x + 18.0f, 45.0f, 200.0f, 13.0f), juce::Justification::centredLeft);
    g.setColour (rs::dim);
    g.setFont (theme::sans (8.0f, true, 0.2f));
    g.drawText ("BY InsideStudioSP", RF (x + 18.0f, 59.0f, 200.0f, 11.0f), juce::Justification::centredLeft);

    // BPM e o estado do relógio
    {
        const bool syn = proc.synced.load();
        const bool waiting = proc.waitingKey.load();
        const juce::String state = ! syn ? "LIVRE" : waiting ? "SEGURE UMA NOTA" : "SYNC";   // LIVRE: sem play, a nota segurada toca no relógio próprio
        g.setFont (theme::mono (13.0f));
        g.setColour (rs::cyan);
        g.drawText (juce::String (proc.shownBpm.load(), 1) + " BPM", RF (560.0f, 26.0f, 130.0f, 16.0f), juce::Justification::centredRight);
        g.setFont (theme::sans (9.5f, true, 0.14f));
        g.setColour (syn && ! waiting ? rs::lime : rs::amber);
        g.drawFittedText (state, juce::Rectangle<int> (560, 46, 130, 14), juce::Justification::centredRight, 1, 0.8f);
    }

    drawMeter (g);
}

// medidor de saída (pico), barras no gradiente da marca
void ResenhaEditor::drawMeter (juce::Graphics& g)
{
    const int bars = 12;
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float v  = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -42.0f, 0.0f, 0.0f, 1.0f));
    for (int i = 0; i < bars; ++i)
    {
        const float f = (float) i / (float) (bars - 1);
        const bool lit = level > 0.0005f && f <= v;
        const RF b (1092.0f + (float) i * 7.5f, 28.0f, 5.0f, 26.0f);
        const auto c = rs::brand (f);
        g.setColour (lit ? c : rs::faint.withAlpha (0.6f));
        g.fillRoundedRectangle (b, 1.5f);
        if (lit)
        {
            g.setColour (c.withAlpha (0.25f));
            g.fillRoundedRectangle (b.expanded (1.5f), 2.5f);
        }
    }
    g.setFont (theme::sans (7.5f, true, 0.2f));
    g.setColour (rs::dim);
    g.drawText ("OUT", RF (1092.0f, 56.0f, 88.0f, 10.0f), juce::Justification::centred);
}
