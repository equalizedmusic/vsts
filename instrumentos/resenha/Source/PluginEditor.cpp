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

    wave = std::make_unique<rs::ChoiceBox> (st, id::v (v, id::wave), colour, " + FM");
    addAndMakeVisible (*wave);
    wave->setBounds (8, 82, w - 16, 20);

    struct K { const juce::String* pid; const char* label; bool bipolar; };
    const K rows[3][4] = {
        { { &id::ratio, "RATIO", false }, { &id::fm, "FM", false }, { &id::fb, "FEEDBACK", false }, { &id::fmDec, "FM DEC", false } },
        { { &id::cut, "CUTOFF", false }, { &id::res, "RESO", false }, { &id::fEnv, "F.ENV", false }, { &id::dec, "DECAY", false } },
        { { &id::pEnv, "P.ENV", true }, { &id::glide, "GLIDE", false }, { &id::pan, "PAN", true }, { nullptr, nullptr, false } },
    };
    const float cw = (float) (w - 6) / 4.0f;
    for (int r = 0; r < 3; ++r)
    {
        const int n = r == 2 ? 3 : 4;
        const float x0 = r == 2 ? ((float) w - cw * 3.0f) * 0.5f : 3.0f;
        for (int c = 0; c < n; ++c)
        {
            const auto& k = rows[r][c];
            auto knob = std::make_unique<rs::NeonKnob> (st, id::v (v, *k.pid), k.label, colour, 30.0f, k.bipolar);
            addAndMakeVisible (*knob);
            knob->setBounds (juce::roundToInt (x0 + cw * (float) c), 108 + r * 63, juce::roundToInt (cw), 56);
            knobs.push_back (std::move (knob));
        }
    }

    for (const auto* pid : { &id::wave, &id::ratio, &id::fm, &id::fb })
        if (auto* prm = st.getParameter (id::v (v, *pid)))
            watch.push_back (std::make_unique<juce::ParameterAttachment> (*prm, [this] (float) { repaint (8, 38, getWidth() - 16, 42); }));
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
    repaint();
}

void VoiceCard::drawWave (juce::Graphics& g, RF r)
{
    // a forma de onda do timbre no pico do ataque: o modulador (RATIO, FEEDBACK) entortando a portadora (FM)
    auto val = [this] (const juce::String& name) { return proc.apvts.getRawParameterValue (id::v (v, name))->load(); };
    const int waveIdx = juce::roundToInt (val (id::wave));
    const float ratio = ratios[juce::jlimit (0, (int) std::size (ratios) - 1, juce::roundToInt (val (id::ratio)))];
    const float fm = val (id::fm) * 0.01f, fb = val (id::fb) * 0.01f * 1.5f;
    const float index = fm * fm * 7.0f;
    const float twoPi = juce::MathConstants<float>::twoPi;

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
        const float yy = r.getCentreY() - y * r.getHeight() * 0.36f;
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
    rs::label (g, "TIMBRE " + juce::String::charToString ((juce::juce_wchar) ('A' + v)), { 42.0f, 8.0f, 90.0f, 12.0f },
               juce::Justification::centredLeft, 9.0f, silent ? rs::faint : rs::ink);
    rs::label (g, role, { 42.0f, 21.0f, 90.0f, 12.0f }, juce::Justification::centredLeft, 8.5f, c);

    const RF disp (8.0f, 38.0f, (float) getWidth() - 16.0f, 40.0f);
    rs::screen (g, disp, 4.0f);
    drawWave (g, disp);
}

void VoiceCard::paintOverChildren (juce::Graphics& g)
{
    if (silent)
    {
        g.setColour (rs::bg0.withAlpha (0.35f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 8.0f);
    }
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
        desc << who.substring (0, 1) << juce::String::fromUTF8 (" pergunta ") << pp.every
             << (pp.every == 1 ? " nota" : " notas") << juce::String::fromUTF8 (" · ");
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
        cards[(size_t) v] = add<VoiceCard> ({ 18 + v * 190, 108, 182, 302 }, proc, v);

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

    // ---- troca de timbre ----
    add<rs::Segmented> ({ 618, 458, 140, 26 }, st, id::voices,   rs::magenta);
    add<rs::Segmented> ({ 768, 458, 196, 26 }, st, id::every,    rs::cyan);
    add<rs::Segmented> ({ 974, 458, 208, 26 }, st, id::response, rs::orange);
    add<rs::Segmented> ({ 618, 508, 230, 26 }, st, id::order,    rs::lime);
    add<rs::Segmented> ({ 858, 508, 324, 26 }, st, id::interval, rs::violet);

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

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) rs::baseW * rs::minScale), juce::roundToInt ((float) rs::baseH * rs::minScale),
                     juce::roundToInt ((float) rs::baseW * rs::maxScale), juce::roundToInt ((float) rs::baseH * rs::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) rs::baseW / (double) rs::baseH);
    setScale (openScale);
    startTimerHz (30);
    ativacao.anexar (*this);
}

ResenhaEditor::~ResenhaEditor()
{
    dialog.close();
    stopTimer();
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
        c->setTalk (proc.engine.talk[(size_t) v].load());
    }
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

    rs::sectionTitle (g, 22.0f, 88.0f, 750.0f, juce::String::fromUTF8 ("VOZES · TIMBRES QUE CONVERSAM"));
    rs::sectionTitle (g, 790.0f, 88.0f, 392.0f, "CONVERSA");

    // ---- gerador ----
    rs::sectionTitle (g, 22.0f, 418.0f, 582.0f, "GERADOR DE FRASE");
    rs::label (g, "ESCALA",    { 22.0f, 444.0f, 120.0f, 12.0f }, juce::Justification::centred);
    rs::label (g, juce::String::fromUTF8 ("TÔNICA"), { 148.0f, 444.0f, 56.0f, 12.0f }, juce::Justification::centred);
    rs::label (g, "COMPASSOS", { 206.0f, 444.0f, 70.0f, 12.0f }, juce::Justification::centred);
    rs::label (g, "RATE",      { 278.0f, 444.0f, 66.0f, 12.0f }, juce::Justification::centred);

    // ---- troca de timbre ----
    rs::sectionTitle (g, 618.0f, 418.0f, 564.0f, "TROCA DE TIMBRE");
    rs::label (g, "VOZES NA CONVERSA",  { 618.0f, 444.0f, 140.0f, 12.0f });
    rs::label (g, "TROCA A CADA (NOTAS)", { 768.0f, 444.0f, 196.0f, 12.0f });
    rs::label (g, "RESPOSTA",           { 974.0f, 444.0f, 208.0f, 12.0f });
    rs::label (g, "ORDEM",              { 618.0f, 494.0f, 230.0f, 12.0f });
    rs::label (g, "ALTURA DA RESPOSTA", { 858.0f, 494.0f, 324.0f, 12.0f });
    rs::label (g, "TOCAR",              { 990.0f, 541.0f, 192.0f, 12.0f }, juce::Justification::centred);

    g.setFont (theme::sans (9.0f, false, 0.03f));
    g.setColour (rs::dim);
    g.drawText (juce::String::fromUTF8 ("CADEADO = GERAR CONVERSA não mexe naquela parte  ·  COM NOTA: segure uma nota no DAW e a frase toca "
                                        "transposta (Dó 3 = original)  ·  SEMPRE: toca com o play  ·  ARRASTE o ARRASTAR MIDI para o DAW"),
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
    g.setColour (rs::edge.brighter (0.3f));
    g.fillRect (x + 6.0f, 28.0f, 1.0f, 30.0f);
    g.setFont (theme::sans (9.5f, true, 0.3f));
    g.setColour (rs::ink.withAlpha (0.85f));
    g.drawText (juce::String::fromUTF8 ("CALL & RESPONSE · FM"), RF (x + 18.0f, 28.0f, 230.0f, 13.0f), juce::Justification::centredLeft);
    g.drawText ("GERADOR DE FRASES E TIMBRES", RF (x + 18.0f, 45.0f, 230.0f, 13.0f), juce::Justification::centredLeft);
    g.setColour (rs::dim);
    g.setFont (theme::sans (8.0f, true, 0.2f));
    g.drawText ("BY InsideStudioSP", RF (x + 18.0f, 59.0f, 230.0f, 11.0f), juce::Justification::centredLeft);

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
