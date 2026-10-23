#include "PluginEditor.h"
#include <complex>

using namespace console;

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
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

float param (ConsoleProcessor& p, const char* pid)
{
    return p.apvts.getRawParameterValue (pid)->load();
}

int modelIndex (ConsoleProcessor& p)
{
    return juce::jlimit (0, numModels - 1, juce::roundToInt (param (p, id::model)));
}

// sinal de menos tipográfico nos textos da tela
juce::String minus (const juce::String& s)
{
    return s.replace ("-", u8 ("−"));
}

juce::String dbfsText (float gain)
{
    if (gain < 1.0e-5f) return u8 ("−∞ dBFS");
    return minus (juce::String (juce::Decibels::gainToDecibels (gain), 1).replaceCharacter ('.', ',')) + " dBFS";
}

void legendItem (juce::Graphics& g, float& x, juce::Colour c, const juce::String& text, bool line)
{
    const auto f = theme::sans (8.0f, true, 0.18f);
    const float tw = textWidth (f, text);
    x -= tw;
    g.setColour (theme::dim);
    g.setFont (f);
    g.drawText (text, juce::Rectangle<float> (x, 7.0f, tw + 2.0f, 12.0f), juce::Justification::centredLeft);
    x -= 12.0f;
    g.setColour (c);
    if (line) g.fillRect (x - 2.0f, 12.0f, 12.0f, 2.0f);
    else      g.fillRoundedRectangle (x, 9.0f, 8.0f, 8.0f, 2.0f);
    x -= 12.0f;
}

// resposta de 1 polo (aproximação analógica) para o desenho do EQ
std::complex<float> lowPass (float f, float fc)
{
    return 1.0f / std::complex<float> (1.0f, f / fc);
}
} // namespace

// =====================================================================
// MESA (faixa do alto)
// =====================================================================
void ColorStrip::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();

    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    drawTitle (g, 12.0f, 7.0f, "MESA ", u8 ("· AO VIVO"));
    {
        juce::AttributedString s;
        s.append ("PEAK  IN ", theme::mono (9.5f), theme::dim);
        s.append (dbfsText (peakIn), theme::mono (9.5f), theme::cream);
        s.append ("   OUT ", theme::mono (9.5f), theme::dim);
        s.append (dbfsText (peakOut), theme::mono (9.5f), theme::cream);
        s.setJustification (juce::Justification::centredLeft);
        s.draw (g, { 130.0f, 7.0f, 360.0f, 13.0f });
    }
    {
        float x = W - 10.0f;
        legendItem (g, x, theme::violet, "NOISE", true);
        legendItem (g, x, theme::coral, "COR GERADA", false);
        legendItem (g, x, theme::cream, "SINAL NO CANAL", false);
    }

    const float x0 = 12.0f, x1 = W - 12.0f, top = 26.0f, bot = H - 10.0f;
    const float lo = -96.0f, hi = 24.0f;
    auto yOf = [=] (float db) { return bot - 1.0f - (juce::jlimit (lo, hi, db) - lo) / (hi - lo) * (bot - top - 2.0f); };

    g.setColour (theme::dark);
    g.fillRoundedRectangle (x0, top, x1 - x0, bot - top, 3.0f);

    // mais antigo à esquerda, agora à direita
    const int   wpos = proc.engine.colWrite.load (std::memory_order_acquire);
    const float cw   = (x1 - x0) / (float) historyCols;
    for (int k = 0; k < historyCols; ++k)
    {
        const auto  idx = (size_t) ((wpos + k) % historyCols);
        const float a = proc.engine.colIn[idx].load (std::memory_order_relaxed);
        const float e = proc.engine.colErr[idx].load (std::memory_order_relaxed);
        const float x = x0 + (float) k * cw;
        if (a > 1.0e-5f)
        {
            const float ya = yOf (juce::Decibels::gainToDecibels (a));
            g.setColour (theme::cream.withAlpha (0.55f));
            g.fillRect (x, ya, cw + 0.4f, bot - 1.0f - ya);
        }
        if (e > 1.0e-6f)
        {
            const float ye = yOf (juce::Decibels::gainToDecibels (e));
            g.setColour (theme::coral.withAlpha (0.9f));
            g.fillRect (x, ye, cw + 0.4f, bot - 1.0f - ye);
        }
    }

    // grade em dBFS
    g.setFont (theme::mono (8.5f));
    for (int step = 0; step < 4; ++step)
    {
        const float db = -24.0f * (float) step;
        const float y  = yOf (db);
        g.setColour (theme::line.withAlpha (step == 0 ? 1.0f : 0.6f));
        g.fillRect (x0, y, x1 - x0, 1.0f);
        g.setColour (theme::dim);
        g.drawText (step == 0 ? juce::String ("0 dBFS") : minus (juce::String (-24 * step)),
                    juce::Rectangle<float> (x1 - 60.0f, y - 11.0f, 56.0f, 10.0f), juce::Justification::centredRight);
    }

    // ruído de fundo (violeta)
    const float nz = param (proc, id::noise);
    if (nz > 0.5f)
    {
        const float db = noiseDbfs (nz * 0.01f);
        const float y  = yOf (db);
        g.setColour (theme::violet);
        g.fillRect (x0, y - 0.5f, x1 - x0, 1.2f);
        g.setFont (theme::mono (9.0f));
        g.drawText ("NOISE " + minus (juce::String (juce::roundToInt (db))) + " dBFS",
                    juce::Rectangle<float> (x0 + 6.0f, y - 12.0f, 140.0f, 10.0f), juce::Justification::centredLeft);
    }

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// =====================================================================
// CURVA
// =====================================================================
void CurveView::paint (juce::Graphics& g)
{
    const float S = (float) getWidth();
    const int   mi = modelIndex (proc);
    Shaper sh;
    sh.set (modelSpec (mi));
    const float R = 2.0f;   // eixos: ±R (nível que chega na curva, depois do DRIVE)
    const float half = S * 0.5f - 12.0f;
    auto px = [=] (float v) { return S * 0.5f + v / R * half; };
    auto py = [=] (float v) { return S * 0.5f - juce::jlimit (-R, R, v) / R * half; };

    g.setColour (theme::dark);
    g.fillRoundedRectangle (0.0f, 0.0f, S, S, 4.0f);

    // eixos e a diagonal (canal sem cor)
    g.setColour (theme::segOff);
    g.fillRect (12.0f, S * 0.5f, S - 24.0f, 1.0f);
    g.fillRect (S * 0.5f, 12.0f, 1.0f, S - 24.0f);
    {
        juce::Path diag;
        diag.startNewSubPath (px (-R), py (-R));
        diag.lineTo (px (R), py (R));
        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, diag, dashes, 2);
        g.setColour (theme::dim.withAlpha (0.5f));
        g.fillPath (dashed);
    }

    // curva do modelo
    juce::Path curvePath;
    const int pts = 200;
    for (int k = 0; k <= pts; ++k)
    {
        const float x = -R + 2.0f * R * (float) k / (float) pts;
        const float y = sh (x);
        if (k == 0) curvePath.startNewSubPath (px (x), py (y)); else curvePath.lineTo (px (x), py (y));
    }
    g.setColour (theme::amber);
    g.strokePath (curvePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pico de agora: o traço coral é o quanto a curva se afasta da reta
    if (live > 1.0e-4f)
    {
        const float xin = juce::jmin (live, R);
        const float y   = sh (xin);
        if (std::abs (xin - y) > 1.0e-3f)
        {
            g.setColour (theme::coral);
            const float ya = juce::jmin (py (xin), py (y)), yb = juce::jmax (py (xin), py (y));
            g.fillRect (px (xin) - 1.0f, ya, 2.0f, yb - ya);
        }
        g.setColour (theme::coral.withAlpha (0.3f));
        g.fillEllipse (px (xin) - 8.0f, py (y) - 8.0f, 16.0f, 16.0f);
        g.setColour (theme::coral);
        g.fillEllipse (px (xin) - 4.0f, py (y) - 4.0f, 8.0f, 8.0f);
    }

    // rodapé: modelo, drive e a descrição do modelo
    auto line = [] (const juce::String& a, const juce::String& b, juce::Colour c)
    {
        juce::AttributedString s;
        s.append (a, theme::mono (10.5f), theme::dim);
        s.append (b, theme::mono (10.5f), c);
        return s;
    };
    auto sl = line ("MODELO ", juce::String (modelNames[mi]).toUpperCase(), theme::amber);
    auto sr = line ("DRIVE ", minus (dbText (driveDb (param (proc, id::drive) * 0.01f))), theme::coral);
    sl.setJustification (juce::Justification::centredLeft);
    sr.setJustification (juce::Justification::centredRight);
    sl.draw (g, { 0.0f, S + 8.0f, S, 14.0f });
    sr.draw (g, { 0.0f, S + 8.0f, S, 14.0f });

    g.setColour (theme::dim);
    g.setFont (theme::sans (10.5f));
    g.drawFittedText (u8 (modelDescription (mi)), juce::Rectangle<int> (0, (int) S + 28, (int) S, getHeight() - (int) S - 28),
                      juce::Justification::topLeft, 3);
}

// =====================================================================
// HARMÔNICOS
// =====================================================================
void HarmonicsView::update (float drivenPeak, float dt)
{
    level = juce::jmax (drivenPeak, level * std::exp (-dt * 2.0f));

    // um período de seno no nível de agora passa pela curva do modelo; a DFT diz o que nasceu
    Shaper sh;
    sh.set (modelSpec (modelIndex (proc)));
    constexpr int N = 64;
    const float twoPi = juce::MathConstants<float>::twoPi;
    float y[N];
    for (int i = 0; i < N; ++i)
        y[i] = sh (level * std::sin (twoPi * (float) i / (float) N));
    for (int k = 1; k <= 5; ++k)
    {
        float re = 0.0f, im = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float ang = twoPi * (float) (k * i) / (float) N;
            re += y[i] * std::cos (ang);
            im -= y[i] * std::sin (ang);
        }
        harm[k] = 2.0f * std::sqrt (re * re + im * im) / (float) N;
    }
}

void HarmonicsView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    auto label = [&g] (const juce::String& t, float y)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.2f));
        g.drawText (t, juce::Rectangle<float> (0.0f, y, 200.0f, 12.0f), juce::Justification::centredLeft);
    };
    auto value = [&g, W] (const juce::String& t, float y, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (theme::mono (11.0f));
        g.drawText (t, juce::Rectangle<float> (0.0f, y, W, 12.0f), juce::Justification::centredRight);
    };

    const bool  alive = level > 1.0e-4f && harm[1] > 1.0e-6f;
    const float floorDb = -96.0f;

    // ---- barras do 2º ao 5º (dB em relação à fundamental) ----
    label (u8 ("EM RELAÇÃO À FUNDAMENTAL"), 0.0f);
    const float barTop = 18.0f, barH = 100.0f, gap = 10.0f;
    const float barW = (W - 3.0f * gap) / 4.0f;
    for (int k = 2; k <= 5; ++k)
    {
        const float rel  = alive ? juce::Decibels::gainToDecibels (harm[k] / harm[1], -120.0f) : -120.0f;
        const float frac = juce::jlimit (0.0f, 1.0f, (rel - floorDb) / -floorDb);
        const float x    = (float) (k - 2) * (barW + gap);
        const bool  even = (k % 2) == 0;

        g.setColour (theme::dark);
        g.fillRoundedRectangle (x, barTop, barW, barH, 3.0f);
        if (frac > 0.0f)
        {
            g.setColour (even ? theme::coral : theme::coral.withAlpha (0.55f));
            g.fillRoundedRectangle (x, barTop + barH * (1.0f - frac), barW, barH * frac, 3.0f);
        }

        g.setColour (rel > floorDb ? theme::cream : theme::dim);
        g.setFont (theme::mono (10.5f));
        g.drawText (rel > floorDb ? minus (juce::String (juce::roundToInt (rel))) + " dB" : u8 ("—"),
                    juce::Rectangle<float> (x, barTop + barH + 4.0f, barW, 12.0f), juce::Justification::centred);
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.15f));
        g.drawText (juce::String (k) + u8 ("º ") + (even ? juce::String ("PAR") : u8 ("ÍMPAR")),
                    juce::Rectangle<float> (x, barTop + barH + 18.0f, barW, 11.0f), juce::Justification::centred);
    }

    // ---- THD e equilíbrio pares × ímpares ----
    const float h2 = harm[2] * harm[2], h3 = harm[3] * harm[3], h4 = harm[4] * harm[4], h5 = harm[5] * harm[5];
    const float thd = alive ? std::sqrt (h2 + h3 + h4 + h5) / harm[1] : 0.0f;
    const float evenShare = alive && (h2 + h3 + h4 + h5) > 1.0e-14f ? (h2 + h4) / (h2 + h3 + h4 + h5) : 0.5f;

    label ("DISTORTION (THD)", 156.0f);
    value (juce::String (thd * 100.0f, thd < 0.1f ? 2 : 1).replaceCharacter ('.', ',') + "%", 156.0f, theme::cream);

    label (u8 ("PARES · ÍMPARES"), 174.0f);
    value (juce::String (juce::roundToInt (evenShare * 100.0f)) + u8 ("% · ") + juce::String (100 - juce::roundToInt (evenShare * 100.0f)) + "%",
           174.0f, theme::cream);
    g.setColour (theme::coral.withAlpha (0.55f));
    g.fillRoundedRectangle (0.0f, 190.0f, W, 4.0f, 2.0f);
    g.setColour (theme::coral);
    g.fillRoundedRectangle (0.0f, 190.0f, W * juce::jlimit (0.0f, 1.0f, evenShare), 4.0f, 2.0f);

    // ---- resposta: EQ do modelo + TOM (verde) e o vazamento L↔R (azul) ----
    const auto& spec = modelSpec (modelIndex (proc));
    const float toneDb = param (proc, id::tone);
    const float xtG    = crosstalkGain (param (proc, id::crosstalk) * 0.01f);
    const float fsHalf = (float) (0.5 * proc.rate.load());

    label ("RESPOSTA DO CANAL", 206.0f);
    value ("TONE " + minus (dbText (toneDb)), 206.0f, theme::leaf);

    const juce::Rectangle<float> plot (0.0f, 222.0f, W, 64.0f);
    const float dbTop = 6.0f, dbBot = -30.0f;
    auto yOf = [plot, dbTop, dbBot] (float db)
    {
        return plot.getY() + (dbTop - juce::jlimit (dbBot, dbTop, db)) / (dbTop - dbBot) * plot.getHeight();
    };
    g.setColour (theme::dark);
    g.fillRoundedRectangle (plot, 3.0f);
    g.setColour (theme::line);
    g.fillRect (plot.getX(), yOf (0.0f), plot.getWidth(), 1.0f);
    g.setColour (theme::line.withAlpha (0.5f));
    g.fillRect (plot.getX(), yOf (-12.0f), plot.getWidth(), 1.0f);
    g.fillRect (plot.getX(), yOf (-24.0f), plot.getWidth(), 1.0f);

    const float tilt = toneDb + spec.tiltDb;
    const float gH = std::pow (10.0f, tilt / 40.0f), gL = 1.0f / gH;
    const float lowG = juce::Decibels::decibelsToGain (spec.lowDb);
    const bool  topOn = spec.topHz < 0.9f * fsHalf;
    juce::Path eq, leak;
    const int pts = 120;
    for (int k = 0; k <= pts; ++k)
    {
        const float t = (float) k / (float) pts;
        const float f = 20.0f * std::pow (1000.0f, t);
        const auto  lpTilt = lowPass (f, tiltPivotHz);
        std::complex<float> h = (1.0f + (lowG - 1.0f) * lowPass (f, spec.lowHz)) * (gL * lpTilt + gH * (1.0f - lpTilt));
        if (topOn) h *= lowPass (f, spec.topHz);
        const float x  = plot.getX() + t * plot.getWidth();
        const float ye = yOf (juce::Decibels::gainToDecibels (std::abs (h), -100.0f));
        if (k == 0) eq.startNewSubPath (x, ye); else eq.lineTo (x, ye);

        if (xtG > 0.0f)
        {
            const float lk = xtG * std::abs ((1.0f - lowPass (f, xtLowHz)) * lowPass (f, xtHighHz));
            const float yl = yOf (juce::Decibels::gainToDecibels (lk, -100.0f));
            if (k == 0) leak.startNewSubPath (x, yl); else leak.lineTo (x, yl);
        }
    }
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.toNearestInt());
        if (xtG > 0.0f)
        {
            g.setColour (theme::ice);
            g.strokePath (leak, juce::PathStrokeType (1.5f));
        }
        g.setColour (theme::leaf);
        g.strokePath (eq, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ---- vazamento e ruído ----
    g.setFont (theme::mono (10.0f));
    g.setColour (theme::ice);
    g.drawText (xtG > 0.0f ? u8 ("L↔R ") + minus (juce::String (juce::Decibels::gainToDecibels (xtG), 1).replaceCharacter ('.', ',')) + " dB"
                           : u8 ("L↔R desligado"),
                juce::Rectangle<float> (0.0f, 294.0f, W, 12.0f), juce::Justification::centredLeft);
    g.setColour (theme::violet);
    g.drawText ("NOISE " + minus (noiseText (param (proc, id::noise))),
                juce::Rectangle<float> (0.0f, 294.0f, W, 12.0f), juce::Justification::centredRight);
}

// =====================================================================
// EDITOR
// =====================================================================
ConsoleEditor::ConsoleEditor (ConsoleProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), curve (p), harmonics (p),
      // cor do knob = cor do que ele mexe no desenho
      driveMacro (p.apvts, id::drive,     "DRIVE",       true,  theme::coral),
      model      (p.apvts, id::model,     "MODELO",      false, theme::amber),
      tone       (p.apvts, id::tone,      "TONE",        false, theme::leaf),
      crosstalk  (p.apvts, id::crosstalk, "CROSSTALK",   false, theme::ice),
      noise      (p.apvts, id::noise,     "NOISE",       false, theme::violet),
      output     (p.apvts, id::output,    "GAIN",        false, theme::cream),
      mix        (p.apvts, id::mix),
      compareKey ("COMPARAR", theme::amber),
      autoKey    ("AUTO", theme::cream),
      osKey      (u8 ("OS 4×"), theme::cream)
{
    setLookAndFeel (&lnf);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, ui::baseW, ui::baseH);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> {
             &strip, &curve, &harmonics, &driveMacro, &model, &tone, &crosstalk, &noise, &output, &mix,
             &compareKey, &autoKey, &osKey, &bypass, &prevBtn, &nextBtn, &saveBtn, &presetName,
             &meterIn, &meterOut, &readout })
        canvas.addAndMakeVisible (c);

    compareAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::compare, compareKey);
    autoAtt    = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::autoGain, autoKey);
    osAtt      = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::os4, osKey);
    bypassAtt  = std::make_unique<APVTS::ButtonAttachment> (p.apvts, id::bypass, bypass);

    prevBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    nextBtn.onClick    = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    saveBtn.onClick    = [this] { askNewPreset(); };
    presetName.onClick = [this] { showPresetMenu(); };

    lastOs4 = param (proc, id::os4) > 0.5f;
    layoutCanvas();

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (ui::baseW * ui::minScale), juce::roundToInt (ui::baseH * ui::minScale),
                     juce::roundToInt (ui::baseW * ui::maxScale), juce::roundToInt (ui::baseH * ui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) ui::baseW / (double) ui::baseH);
    setScale (proc.uiScale.load());
    startTimerHz (30);
}

ConsoleEditor::~ConsoleEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ConsoleEditor::showPresetMenu()
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

    juce::Component::SafePointer<ConsoleEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void ConsoleEditor::askNewPreset()
{
    juce::Component::SafePointer<ConsoleEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void ConsoleEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<ConsoleEditor> safe (this);
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

void ConsoleEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float  dt  = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001) : 1.0f / 30.0f;
    lastTick = now;

    harmonics.update (proc.engine.drivenPeak.exchange (0.0f), dt);
    curve.setLive (harmonics.levelShown());

    const float inNow = proc.meterIn.exchange (0.0f), outNow = proc.meterOut.exchange (0.0f);
    levelIn  = juce::jmax (inNow,  levelIn * 0.85f);
    levelOut = juce::jmax (outNow, levelOut * 0.85f);
    holdIn   = juce::jmax (inNow,  holdIn * std::exp (-dt * 1.5f));    // o número cai mais devagar que os LEDs
    holdOut  = juce::jmax (outNow, holdOut * std::exp (-dt * 1.5f));
    meterIn.setLevel (levelIn);
    meterOut.setLevel (levelOut);
    strip.setPeaks (holdIn, holdOut);

    strip.repaint();
    curve.repaint();
    harmonics.repaint();

    // COMPARAR ligado: quanto o seco foi ajustado; senão, a compensação automática
    if (param (proc, id::compare) > 0.5f)
        readout.set ("A/B", minus (dbText (proc.matchDb.load())), theme::amber);
    else if (param (proc, id::autoGain) > 0.5f)
        readout.set ("AUTO", minus (dbText (proc.engine.compDb.load())), theme::cream);
    else
        readout.set ("AUTO", "desligado", theme::dim);

    const bool os4 = param (proc, id::os4) > 0.5f;
    if (os4 != lastOs4)
    {
        lastOs4 = os4;
        canvas.repaint();   // o cabeçalho mostra 2× ou 4×
    }

    presetName.set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());
}

void ConsoleEditor::setScale (float scale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, scale);
    setSize (juce::roundToInt ((float) ui::baseW * scale), juce::roundToInt ((float) ui::baseH * scale));
}

void ConsoleEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void ConsoleEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void ConsoleEditor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // ---- logo: três faders de mesa ----
    {
        const float capY[] = { 31.0f, 37.0f, 27.0f };
        for (int i = 0; i < 3; ++i)
        {
            const float x = 18.0f + (float) i * 9.5f;
            g.setColour (theme::line);
            g.fillRect (x + 2.5f, 24.0f, 1.5f, 22.0f);
            g.setColour (i == 1 ? theme::coral : theme::cream);
            g.fillRoundedRectangle (x, capY[i], 6.5f, 4.5f, 1.0f);
        }
    }

    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const juce::String name ("CONSOLE");
    const float tagX = 56.0f + textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 220.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    const juce::String tag1 ("COR DE CANAL"), tag2 (lastOs4 ? u8 ("4× OVERSAMPLING") : u8 ("2× OVERSAMPLING"));
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText (tag1, juce::Rectangle<float> (tagX, 22.0f, 160.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (tag2, juce::Rectangle<float> (tagX, 35.0f, 160.0f, 12.0f), juce::Justification::centredLeft);

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

    // ---- latência (o host compensa sozinho) ----
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

    // ---- cards ----
    drawCard (g, { 12.0f, 216.0f, 262.0f, 352.0f });
    drawCard (g, { 284.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 552.0f, 216.0f, 258.0f, 352.0f });
    drawCard (g, { 820.0f, 216.0f, 168.0f, 352.0f });

    drawTitle (g, 22.0f,  226.0f, "CURVA ", u8 ("· TRANSFERÊNCIA"));
    drawTitle (g, 294.0f, 226.0f, u8 ("01 · "), "CANAL");
    drawTitle (g, 562.0f, 226.0f, u8 ("02 · "), u8 ("HARMÔNICOS"));
    drawTitle (g, 830.0f, 226.0f, u8 ("03 · "), "OUTPUT");

    drawDesc (g, { 396.0f, 252.0f, 140.0f, 110.0f }, "MACRO DRIVE",
              u8 ("Empurra o sinal contra o canal da mesa. Mais drive, mais harmônicos (as barras coral)."));
}

void ConsoleEditor::layoutCanvas()
{
    prevBtn.setBounds (618, 18, 30, 32);
    presetName.setBounds (648, 18, 230, 32);
    nextBtn.setBounds (878, 18, 30, 32);
    saveBtn.setBounds (908, 18, 30, 32);
    bypass.setBounds (952, 18, 32, 32);

    strip.setBounds (12, 66, 976, 140);
    curve.setBounds (22, 246, 240, 312);

    driveMacro.setBounds (294, 246, 92, 92);
    model.setBounds      (294, 376, 119, 92);
    tone.setBounds       (413, 376, 119, 92);
    crosstalk.setBounds  (294, 468, 119, 92);
    noise.setBounds      (413, 468, 119, 92);

    harmonics.setBounds (566, 248, 230, 312);

    compareKey.setBounds (830, 246, 148, 36);
    autoKey.setBounds    (830, 288, 71, 36);
    osKey.setBounds      (907, 288, 71, 36);
    mix.setBounds        (830, 334, 44, 224);
    output.setBounds     (884, 334, 94, 92);
    readout.setBounds    (884, 428, 94, 14);
    meterIn.setBounds    (904, 446, 24, 112);
    meterOut.setBounds   (932, 446, 24, 112);
}
