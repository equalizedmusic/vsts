#include "PluginEditor.h"

using namespace prisma;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuLicenca = 20005;

// rack de 19": a frente tem o layout de 1000 × 580 e as orelhas ficam dos lados
constexpr int ear = 32, rackW = ui::baseW + 2 * ear;
const juce::Colour tinta (0xff43395a);   // cor da frente do PRISMA

constexpr float minHz = 20.0f, maxHz = 20000.0f;
constexpr float dbTop = 0.0f, dbBottom = -84.0f;   // escala do espectro
constexpr float grFull = 24.0f;                     // redução que enche o medidor (dB)

void drawTitle (juce::Graphics& g, float x, float y, const juce::String& prefix, const juce::String& accent,
                juce::Colour accentColour = theme::amber)
{
    juce::AttributedString s;
    const auto f = theme::sans (9.5f, true, 0.25f);
    s.append (prefix, f, theme::dim);
    s.append (accent, f, accentColour);
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
    painel::secao (g, r);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// nome, duas linhas de descrição e a marca, a partir do x = 56 (o logo fica antes)
float drawBrand (juce::Graphics& g, const juce::String& name, const juce::String& tag1, const juce::String& tag2)
{
    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 260.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText (tag1, juce::Rectangle<float> (tagX, 22.0f, 200.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (tag2, juce::Rectangle<float> (tagX, 35.0f, 200.0f, 12.0f), juce::Justification::centredLeft);

    const float brandX = tagX + juce::jmax (textWidth (tagFont, tag1), textWidth (tagFont, tag2)) + 20.0f;
    g.setColour (theme::line);
    g.fillRect (brandX - 10.0f, 24.0f, 1.0f, 22.0f);
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText ("BY", juce::Rectangle<float> (brandX, 22.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
    const auto brandFont = theme::sans (10.5f, true, 0.06f);
    g.setColour (theme::amber);
    g.setFont (brandFont);
    g.drawText ("InsideStudioSP", juce::Rectangle<float> (brandX, 34.0f, 140.0f, 13.0f), juce::Justification::centredLeft);
    return brandX + textWidth (brandFont, "InsideStudioSP");   // onde a marca termina
}

float param (PrismaProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

// cor de cada banda (é a cor dos knobs de dinâmica dela, da faixa no espectro e da redução)
juce::Colour bandColour (int b)
{
    switch (b)
    {
        case 0:  return theme::amber;
        case 1:  return theme::leaf;
        case 2:  return theme::ice;
        default: return theme::violet;
    }
}

juce::String reductionText (float db)
{
    return db > 0.05f ? u8 ("−") + juce::String (db, 1).replaceCharacter ('.', ',') + " dB" : juce::String ("0,0 dB");
}

// a banda está soando agora? (algum SOLO ligado: só as solo; senão, as que não estão no MUDO)
bool bandAudible (PrismaProcessor& p, int b)
{
    bool anySolo = false;
    for (int k = 0; k < numBands; ++k)
        anySolo = anySolo || param (p, id::solo[k]) > 0.5f;
    return anySolo ? param (p, id::solo[b]) > 0.5f : param (p, id::mute[b]) < 0.5f;
}

// limites de cada banda em Hz (com os cortes atuais)
void bandEdges (PrismaProcessor& p, int b, float& lo, float& hi)
{
    lo = b == 0 ? minHz : param (p, id::xover[b - 1]);
    hi = b == numBands - 1 ? maxHz : param (p, id::xover[b]);
}

void legendItem (juce::Graphics& g, float& x, juce::Colour c, const juce::String& text)
{
    const auto f = theme::sans (8.0f, true, 0.18f);
    const float tw = textWidth (f, text);
    x -= tw;
    g.setColour (theme::dim);
    g.setFont (f);
    g.drawText (text, juce::Rectangle<float> (x, 7.0f, tw + 2.0f, 12.0f), juce::Justification::centredLeft);
    x -= 12.0f;
    g.setColour (c);
    g.fillRoundedRectangle (x, 9.0f, 8.0f, 8.0f, 2.0f);
    x -= 12.0f;
}
} // namespace

// =====================================================================
// ESPECTRO
// =====================================================================
SpectrumView::SpectrumView (PrismaProcessor& p) : proc (p)
{
    spec.fill (-120.0f);
}

juce::Rectangle<float> SpectrumView::plotArea() const
{
    return { 12.0f, 24.0f, (float) getWidth() - 24.0f, (float) getHeight() - 40.0f };
}

float SpectrumView::xOfHz (float hz) const
{
    const auto a = plotArea();
    const float t = std::log (juce::jlimit (minHz, maxHz, hz) / minHz) / std::log (maxHz / minHz);
    return a.getX() + t * a.getWidth();
}

float SpectrumView::hzOfX (float x) const
{
    const auto a = plotArea();
    const float t = juce::jlimit (0.0f, 1.0f, (x - a.getX()) / a.getWidth());
    return minHz * std::pow (maxHz / minHz, t);
}

int SpectrumView::lineAt (float x) const
{
    int best = -1;
    float bestD = 7.0f;
    for (int k = 0; k < 3; ++k)
    {
        const float d = std::abs (x - xOfHz (proc.apvts.getRawParameterValue (id::xover[k])->load()));
        if (d < bestD) { bestD = d; best = k; }
    }
    return best;
}

void SpectrumView::setCrossover (int k, float hz)
{
    // cada corte fica entre os vizinhos (com folga), para as bandas não se cruzarem
    const float lo = k > 0 ? param (proc, id::xover[k - 1]) * 1.2f : minHz;
    const float hi = k < 2 ? param (proc, id::xover[k + 1]) / 1.2f : maxHz;
    hz = juce::jmin (hi, juce::jmax (lo, hz));
    if (auto* rp = proc.apvts.getParameter (id::xover[k]))
        rp->setValueNotifyingHost (rp->convertTo0to1 (hz));
}

void SpectrumView::tick()
{
    auto& eng = proc.engine;
    const int w = eng.scopeWrite.load (std::memory_order_acquire);
    for (int i = 0; i < fftSize; ++i)
    {
        const int idx = (w - fftSize + i + scopeSize) % scopeSize;
        fftData[(size_t) i] = eng.scope[(size_t) idx].load (std::memory_order_relaxed);
    }
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // pico de um seno = 0 dB; sobe rápido e cai devagar
    for (size_t k = 0; k < spec.size(); ++k)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[k] * 2.0f / (float) fftSize, -120.0f);
        float& s = spec[k];
        s += (db - s) * (db > s ? 0.6f : 0.12f);
    }
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth();
    const auto  a = plotArea();
    const float fs = (float) juce::jmax (1.0, proc.rate.load());

    painel::secao (g, r.reduced (1.0f));
    drawTitle (g, 12.0f, 7.0f, "SPECTRUM ", u8 ("· ARRASTE AS LINHAS PARA MUDAR OS CROSSOVERS"));
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::coral, "DRIVE");
        legendItem (g, x, theme::cream, "GR (COR DA BANDA)");
    }

    real::screen (g, a, 3.0f);

    // grade de frequência
    g.setFont (theme::mono (9.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xOfHz (f);
        g.setColour (theme::line.withAlpha (0.6f));
        g.fillRect (x, a.getY(), 1.0f, a.getHeight());
        g.setColour (theme::dim);
        const juce::String t = f >= 1000.0f ? juce::String (juce::roundToInt (f / 1000.0f)) + "k" : juce::String (juce::roundToInt (f));
        g.drawText (t, juce::Rectangle<float> (x - 20.0f, a.getBottom() + 2.0f, 40.0f, 12.0f), juce::Justification::centred);
    }

    // zonas das bandas
    juce::Rectangle<float> zone[numBands];
    for (int b = 0; b < numBands; ++b)
    {
        float lo = 0.0f, hi = 0.0f;
        bandEdges (proc, b, lo, hi);
        const float xa = xOfHz (lo), xb = xOfHz (hi);
        zone[b] = juce::Rectangle<float> (xa, a.getY(), juce::jmax (1.0f, xb - xa), a.getHeight());
        g.setColour (bandColour (b).withAlpha (0.05f));
        g.fillRect (zone[b]);
    }

    // espectro da saída (inclinação de 3 dB/oitava em torno de 1 kHz, para ler melhor os agudos)
    juce::Path line;
    {
        const int   half = fftSize / 2;
        const float binHz = fs / (float) fftSize;
        const float step = 2.0f;
        bool started = false;
        for (float x = a.getX(); x <= a.getRight() + 0.5f; x += step)
        {
            const float f0 = hzOfX (x), f1 = hzOfX (x + step);
            const float bf0 = f0 / binHz, bf1 = f1 / binHz;
            const int   b0 = juce::jlimit (1, half - 2, (int) bf0);
            const int   b1 = juce::jlimit (1, half - 2, (int) bf1);
            float db;
            if (b1 > b0)
            {
                db = spec[(size_t) b0];
                for (int k = b0 + 1; k <= b1; ++k) db = juce::jmax (db, spec[(size_t) k]);
            }
            else
            {
                const float frac = juce::jlimit (0.0f, 1.0f, bf0 - (float) b0);
                db = spec[(size_t) b0] + (spec[(size_t) b0 + 1] - spec[(size_t) b0]) * frac;
            }
            db += 3.0f * std::log2 (juce::jmax (minHz, f0) / 1000.0f);
            const float y = juce::jmap (juce::jlimit (dbBottom, dbTop, db), dbBottom, dbTop, a.getBottom(), a.getY());
            if (! started) { line.startNewSubPath (x, y); started = true; }
            else           line.lineTo (x, y);
        }
    }
    juce::Path fill (line);
    fill.lineTo (a.getRight(), a.getBottom());
    fill.lineTo (a.getX(), a.getBottom());
    fill.closeSubPath();

    for (int b = 0; b < numBands; ++b)
    {
        const bool audible = bandAudible (proc, b);
        const auto col = bandColour (b);
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (zone[b].toNearestInt());
        g.setColour (col.withAlpha (audible ? 0.22f : 0.06f));
        g.fillPath (fill);
        g.setColour (col.withAlpha (audible ? 0.9f : 0.3f));
        g.strokePath (line, juce::PathStrokeType (1.4f));
    }

    // por banda: nome, redução (pendurada no topo) e drive (no pé)
    const float macro01 = param (proc, id::macro) * 0.01f;
    for (int b = 0; b < numBands; ++b)
    {
        const auto  z = zone[b];
        const auto  col = bandColour (b);
        const bool  audible = bandAudible (proc, b);
        const float red = gr[(size_t) b];

        const float h = juce::jmin (1.0f, red / grFull) * a.getHeight() * 0.6f;
        if (h > 0.5f)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillRect (z.getX() + 1.0f, z.getY(), z.getWidth() - 2.0f, h);
            g.setColour (col);
            g.fillRect (z.getX() + 1.0f, z.getY() + h - 1.5f, z.getWidth() - 2.0f, 1.5f);
        }

        if (z.getWidth() > 40.0f)
        {
            g.setColour (audible ? col : theme::dim);
            g.setFont (theme::sans (8.5f, true, 0.15f));
            g.drawText (u8 (bandNames[b]), z.withHeight (12.0f).translated (0.0f, 4.0f), juce::Justification::centred, true);
            g.setFont (theme::mono (10.0f));
            g.setColour (red > 0.05f ? theme::cream : theme::dim);
            g.drawText (reductionText (red), z.withHeight (12.0f).translated (0.0f, 17.0f), juce::Justification::centred, true);
        }

        const float drv = macroDrive (param (proc, id::drive[b]), macro01);
        if (drv > 0.05f)
        {
            const float frac = juce::jmin (1.0f, drv / 36.0f);
            g.setColour (theme::coral.withAlpha (audible ? 0.9f : 0.35f));
            g.fillRect (z.getX() + 2.0f, z.getBottom() - 4.0f, (z.getWidth() - 4.0f) * frac, 3.0f);
            if (z.getWidth() > 40.0f)
            {
                g.setFont (theme::mono (9.0f));
                g.drawText ("DRIVE " + dbText (drv), z.withTop (z.getBottom() - 18.0f).withHeight (12.0f),
                            juce::Justification::centred, true);
            }
        }

        if (! audible)
        {
            g.setColour (theme::bg.withAlpha (0.45f));
            g.fillRect (z);
        }
    }

    // cortes: linhas arrastáveis com a frequência
    for (int k = 0; k < 3; ++k)
    {
        const float hz = param (proc, id::xover[k]);
        const float x  = xOfHz (hz);
        const bool  hot = k == hover || k == dragging;
        g.setColour (theme::cream.withAlpha (hot ? 1.0f : 0.6f));
        g.fillRect (x - (hot ? 1.0f : 0.5f), a.getY(), hot ? 2.0f : 1.0f, a.getHeight());

        const juce::Rectangle<float> tag (x - 27.0f, a.getCentreY() - 8.0f, 54.0f, 16.0f);
        g.setColour (hot ? theme::cream : theme::panel2);
        g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (hot ? theme::cream : theme::line);
        g.drawRoundedRectangle (tag.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (hot ? theme::dark : theme::cream);
        g.setFont (theme::mono (9.5f));
        g.drawText (hzText (hz), tag, juce::Justification::centred);
    }

    real::glass (g, a, 3.0f);
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const int h = plotArea().expanded (0.0f, 4.0f).contains (e.position) ? lineAt (e.position.x) : -1;
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    if (hover >= 0 && dragging < 0)
    {
        hover = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    if (! plotArea().expanded (0.0f, 4.0f).contains (e.position)) return;
    dragging = lineAt (e.position.x);
    if (dragging < 0) return;
    if (auto* rp = proc.apvts.getParameter (id::xover[dragging]))
        rp->beginChangeGesture();
    repaint();
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    setCrossover (dragging, hzOfX (e.position.x));
    repaint();
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0) return;
    if (auto* rp = proc.apvts.getParameter (id::xover[dragging]))
        rp->endChangeGesture();
    dragging = -1;
    repaint();
}

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    // duplo clique numa linha: volta o corte ao padrão
    const int k = lineAt (e.position.x);
    if (k < 0) return;
    if (auto* rp = proc.apvts.getParameter (id::xover[k]))
    {
        rp->beginChangeGesture();
        setCrossover (k, xoverDefaults[k]);
        rp->endChangeGesture();
    }
    repaint();
}

// =====================================================================
// FAIXA DA BANDA + REDUÇÃO
// =====================================================================
void BandStatus::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    const auto  col = bandColour (band);

    float lo = 0.0f, hi = 0.0f;
    bandEdges (proc, band, lo, hi);
    g.setColour (theme::dim);
    g.setFont (theme::mono (10.0f));
    g.drawText (hzText (lo) + u8 (" – ") + hzText (hi), juce::Rectangle<float> (0.0f, 0.0f, W, 12.0f), juce::Justification::centredLeft);

    g.setFont (theme::sans (8.5f, true, 0.18f));
    g.drawText ("GAIN REDUCTION", juce::Rectangle<float> (0.0f, 16.0f, W, 12.0f), juce::Justification::centredLeft);
    g.setColour (gr > 0.05f ? theme::cream : theme::dim);
    g.setFont (theme::mono (11.0f));
    g.drawText (reductionText (gr), juce::Rectangle<float> (0.0f, 16.0f, W, 12.0f), juce::Justification::centredRight);

    // barra: 0 a −24 dB; marcas a cada 6 dB
    const juce::Rectangle<float> bar (0.0f, 31.0f, W, 6.0f);
    g.setColour (theme::segOff);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (col);
    g.fillRoundedRectangle (bar.withWidth (W * juce::jlimit (0.0f, 1.0f, gr / grFull)), 2.0f);
    g.setColour (theme::panel);
    for (int k = 1; k < 4; ++k)
        g.fillRect (W * (float) k / 4.0f, bar.getY(), 1.0f, bar.getHeight());
}

// =====================================================================
// EDITOR
// =====================================================================
PrismaEditor::PrismaEditor (PrismaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p),
      // cor do knob = cor do que ele mexe no desenho
      macro  (p.apvts, id::macro,  "MACRO", true,  theme::coral),
      output (p.apvts, id::output, "GAIN",  false, theme::cream),
      mix    (p.apvts, id::mix)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, rackW, ui::baseH);
    addAndMakeVisible (canvas);

    for (int b = 0; b < numBands; ++b)
    {
        const auto col = bandColour (b);
        status[b]  = std::make_unique<BandStatus> (p, b);
        thrK[b]    = std::make_unique<Knob> (p.apvts, id::thr[b],   "THRESH",       false, col);
        ratioK[b]  = std::make_unique<Knob> (p.apvts, id::ratio[b], "RATIO",        false, col);
        attK[b]    = std::make_unique<Knob> (p.apvts, id::att[b],   "ATTACK",       false, col);
        relK[b]    = std::make_unique<Knob> (p.apvts, id::rel[b],   "RELEASE",      false, col);
        driveK[b]  = std::make_unique<Knob> (p.apvts, id::drive[b], "DRIVE",        false, theme::coral);
        gainK[b]   = std::make_unique<Knob> (p.apvts, id::gain[b],  "GAIN",         false, theme::cream);
        soloKey[b] = std::make_unique<KeyButton> ("SOLO", col);
        muteKey[b] = std::make_unique<KeyButton> ("MUTE", theme::dim);

        for (auto* c : std::initializer_list<juce::Component*> {
                 status[b].get(), thrK[b].get(), ratioK[b].get(), attK[b].get(), relK[b].get(),
                 driveK[b].get(), gainK[b].get(), soloKey[b].get(), muteKey[b].get() })
            canvas.addAndMakeVisible (c);

        buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::solo[b], *soloKey[b]));
        buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::mute[b], *muteKey[b]));
    }

    for (auto* c : std::initializer_list<juce::Component*> {
             &spectrum, &macro, &output, &mix, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName, &meterIn, &meterOut })
        canvas.addAndMakeVisible (c);

    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass));

    prevBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    nextBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    saveBtn.onClick    = [this] { askNewPreset(); };
    presetName.onClick = [this] { showPresetMenu(); };

    layoutCanvas();

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    // o tamanho salvo é lido ANTES do setResizeLimits: ele encolhe a janela ainda vazia para o mínimo
    // e o resized() gravaria 75% por cima do que o usuário escolheu
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (rackW * ui::minScale), juce::roundToInt (ui::baseH * ui::minScale),
                     juce::roundToInt (rackW * ui::maxScale), juce::roundToInt (ui::baseH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) rackW / (double) ui::baseH);
    setScale (openScale);
    startTimerHz (30);
    ativacao.anexar (*this);
}

PrismaEditor::~PrismaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PrismaEditor::showPresetMenu()
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

    juce::Component::SafePointer<PrismaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void PrismaEditor::askNewPreset()
{
    juce::Component::SafePointer<PrismaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void PrismaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<PrismaEditor> safe (this);
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

void PrismaEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) rackW - openScale) > 0.005f)
        setScale (openScale);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    // redução: sobe na hora, desce 30 dB/s
    for (int b = 0; b < numBands; ++b)
    {
        const float v = proc.engine.grDb[(size_t) b].exchange (0.0f);
        grShown[b] = juce::jmax (v, grShown[b] - 30.0f * dt);
        spectrum.setReduction (b, grShown[b]);
        status[b]->setReduction (grShown[b]);
        status[b]->repaint();
    }
    spectrum.tick();
    spectrum.repaint();

    levelIn  = juce::jmax (proc.meterIn.exchange (0.0f),  levelIn * 0.85f);
    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void PrismaEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) rackW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void PrismaEditor::resized()
{
    const float scale = (float) getWidth() / (float) rackW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void PrismaEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void PrismaEditor::drawCanvas (juce::Graphics& g)
{
    painel::rack (g, (float) rackW, (float) ui::baseH, (float) ear, tinta);
    g.addTransform (juce::AffineTransform::translation ((float) ear, 0.0f));   // daqui em diante, o layout da frente

    // ---- logo: um prisma que abre o feixe nas 4 bandas ----
    {
        const juce::Point<float> top (30.0f, 21.0f), left (19.0f, 46.0f), right (41.0f, 46.0f);

        g.setColour (theme::cream);
        g.drawLine (12.0f, 38.0f, 24.5f, 33.5f, 1.6f);
        for (int b = 0; b < numBands; ++b)
        {
            g.setColour (bandColour (b).withAlpha (0.9f));
            g.drawLine (35.5f, 33.5f, 49.0f, 28.0f + (float) b * 4.5f, 1.6f);
        }

        juce::Path tri;
        tri.addTriangle (top, left, right);
        g.setColour (theme::bg);
        g.fillPath (tri);
        g.setColour (theme::cream);
        g.strokePath (tri, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    }

    drawBrand (g, "PRISMA", "MULTIBAND", "DYNAMICS + SATURATION");

    // ---- latência (zero: crossovers IIR) ----
    {
        const double ms = 1000.0 * (double) proc.getLatencySamples() / juce::jmax (1.0, proc.rate.load());
        juce::AttributedString s;
        s.append ("LATENCY ", theme::mono (11.5f), theme::dim);
        s.append (juce::String (ms, 2).replaceCharacter ('.', ',') + " ms", theme::mono (11.5f), theme::cream);
        juce::TextLayout tl;
        tl.createLayout (s, 400.0f);
        const float w = tl.getWidth(), right = 604.0f;
        tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });
    }

    // ---- cards das bandas ----
    for (int b = 0; b < numBands; ++b)
    {
        const float x = 12.0f + (float) b * 202.0f;
        drawCard (g, { x, 230.0f, 194.0f, 338.0f });
        drawTitle (g, x + 10.0f, 240.0f, "0" + juce::String (b + 1) + u8 (" · "), u8 (bandNames[b]), bandColour (b));
    }

    // ---- global ----
    drawCard (g, { 820.0f, 62.0f, 168.0f, 506.0f });
    drawTitle (g, 830.0f, 72.0f, u8 ("05 · "), "GLOBAL");
    drawDesc (g, { 830.0f, 192.0f, 148.0f, 96.0f }, "MACRO",
              u8 ("Mais compressão e mais drive em todas as bandas juntas."));
}

void PrismaEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    spectrum.setBounds (12, 62, 800, 160);

    for (int b = 0; b < numBands; ++b)
    {
        const int x = 12 + b * 202;
        status[b]->setBounds (x + 10, 258, 174, 38);
        soloKey[b]->setBounds (x + 10, 304, 84, 36);
        muteKey[b]->setBounds (x + 100, 304, 84, 36);

        thrK[b]->setBounds   (x + 1,   350, 64, 92);
        ratioK[b]->setBounds (x + 65,  350, 64, 92);
        driveK[b]->setBounds (x + 129, 350, 64, 92);
        attK[b]->setBounds   (x + 1,   452, 64, 92);
        relK[b]->setBounds   (x + 65,  452, 64, 92);
        gainK[b]->setBounds  (x + 129, 452, 64, 92);
    }

    macro.setBounds (858, 92, 92, 92);
    mix.setBounds (830, 296, 44, 262);
    output.setBounds (884, 296, 94, 92);
    meterIn.setBounds  (883, 446, 94, 55);
    meterOut.setBounds (883, 503, 94, 55);

    for (auto* c : canvas.getChildren())   // a frente começa depois da orelha esquerda
        c->setTopLeftPosition (c->getX() + ear, c->getY());
}
