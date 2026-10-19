#include "PluginEditor.h"
#include "WaveBank.h"
#include <cmath>
#include <complex>

using namespace quimera;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuNoUser = 20005;

juce::Rectangle<int> R (int x, int y, int w, int h)
{
    return { x, y, w, h };
}

std::atomic<float>* raw (QuimeraProcessor& p, const juce::String& pid)
{
    static std::atomic<float> missing { 0.0f };
    auto* v = p.apvts.getRawParameterValue (pid);
    jassert (v != nullptr);
    return v != nullptr ? v : &missing;
}

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
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

// nome, duas linhas de descrição e a marca, a partir do x = 56 (o logo fica antes)
float drawBrand (juce::Graphics& g, const juce::String& name, const juce::String& tag1, const juce::String& tag2)
{
    const auto nameFont = theme::sans (27.0f, true, 0.12f);
    const float tagX = 56.0f + qui::textWidth (nameFont, name) + 20.0f;

    g.setColour (theme::cream);
    g.setFont (nameFont);
    g.drawText (name, juce::Rectangle<float> (56.0f, 12.0f, 260.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (tagX - 10.0f, 24.0f, 1.0f, 22.0f);
    const auto tagFont = theme::sans (9.5f, false, 0.2f);
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText (tag1, juce::Rectangle<float> (tagX, 22.0f, 240.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText (tag2, juce::Rectangle<float> (tagX, 35.0f, 240.0f, 12.0f), juce::Justification::centredLeft);

    const float brandX = tagX + juce::jmax (qui::textWidth (tagFont, tag1), qui::textWidth (tagFont, tag2)) + 20.0f;
    g.setColour (theme::line);
    g.fillRect (brandX - 10.0f, 24.0f, 1.0f, 22.0f);
    g.setColour (theme::dim);
    g.setFont (tagFont);
    g.drawText ("BY", juce::Rectangle<float> (brandX, 22.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
    const auto brandFont = theme::sans (10.5f, true, 0.06f);
    g.setColour (theme::amber);
    g.setFont (brandFont);
    g.drawText ("InsideStudioSP", juce::Rectangle<float> (brandX, 34.0f, 140.0f, 13.0f), juce::Justification::centredLeft);
    return brandX + qui::textWidth (brandFont, "InsideStudioSP");   // onde a marca termina
}

// BPM: SYNC seguindo o host; sem régua do host = LIVRE (o synth não espera o play)
// (se encostar na marca, o estado desce para uma segunda linha)
void drawSync (juce::Graphics& g, float bpm, bool syn, float brandEnd, float right)
{
    const auto f = theme::mono (11.5f);
    const juce::String state = syn ? "SYNC" : "LIVRE";
    const juce::Colour stateColour = syn ? theme::cream : theme::dim;

    juce::AttributedString s;
    s.append (juce::String (bpm, 1), f, theme::cream);
    s.append (u8 (" BPM · "), f, theme::dim);
    s.append (state, f, stateColour);
    juce::TextLayout tl;
    tl.createLayout (s, 400.0f);
    float w = tl.getWidth(), ledY = 30.5f;

    if (right - w - 13.0f < brandEnd + 14.0f)
    {
        juce::AttributedString a, b;
        a.append (juce::String (bpm, 1), f, theme::cream);
        a.append (" BPM", f, theme::dim);
        b.append (state, f, stateColour);
        juce::TextLayout ta, tb;
        ta.createLayout (a, 400.0f);
        tb.createLayout (b, 400.0f);
        ta.draw (g, { right - ta.getWidth(), 27.0f - ta.getHeight() * 0.5f, ta.getWidth() + 1.0f, ta.getHeight() });
        tb.draw (g, { right - tb.getWidth(), 41.0f - tb.getHeight() * 0.5f, tb.getWidth() + 1.0f, tb.getHeight() });
        w = ta.getWidth();
        ledY = 23.5f;
    }
    else
        tl.draw (g, { right - w, 34.0f - tl.getHeight() * 0.5f, w + 1.0f, tl.getHeight() });

    const juce::Rectangle<float> led (right - w - 13.0f, ledY, 7.0f, 7.0f);
    if (syn)
    {
        g.setColour (theme::amber.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (syn ? theme::amber : theme::off);
    g.fillEllipse (led);
}

// logo: máscara metade orgânica (folha, à esquerda) e metade circuito (à direita)
void drawLogo (juce::Graphics& g)
{
    const float cx = 31.0f, cy = 34.0f;

    // metade orgânica: folha cheia, com nervuras
    juce::Path leaf;
    leaf.startNewSubPath (cx, cy - 15.0f);
    leaf.cubicTo (cx - 9.0f, cy - 15.5f, cx - 14.5f, cy - 8.0f, cx - 13.5f, cy);
    leaf.cubicTo (cx - 12.5f, cy + 8.0f, cx - 6.5f, cy + 14.0f, cx, cy + 15.0f);
    leaf.closeSubPath();
    g.setGradientFill (juce::ColourGradient (theme::leaf.brighter (0.15f), cx - 12.0f, cy - 12.0f,
                                             theme::leaf.darker (0.7f), cx, cy + 15.0f, false));
    g.fillPath (leaf);
    g.setColour (theme::dark.withAlpha (0.55f));
    for (int k = 0; k < 3; ++k)
    {
        juce::Path vein;
        const float yy = cy - 7.0f + (float) k * 7.0f;
        vein.startNewSubPath (cx - 0.8f, yy + 2.5f);
        vein.quadraticTo (cx - 5.0f, yy + 1.0f, cx - 9.5f - (k == 1 ? 1.5f : 0.0f), yy - 2.5f);
        g.strokePath (vein, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // olho orgânico (amêndoa)
    juce::Path eye;
    eye.addEllipse (cx - 9.0f, cy - 3.6f, 6.0f, 3.4f);
    g.setColour (theme::dark);
    g.fillPath (eye);
    g.setColour (theme::amber);
    g.fillEllipse (cx - 6.9f, cy - 2.9f, 2.0f, 2.0f);

    // metade circuito: contorno facetado + trilhas com nós
    juce::Path shell;
    shell.startNewSubPath (cx, cy - 15.0f);
    shell.lineTo (cx + 8.0f, cy - 14.0f);
    shell.lineTo (cx + 13.5f, cy - 7.0f);
    shell.lineTo (cx + 13.5f, cy + 2.0f);
    shell.lineTo (cx + 8.0f, cy + 11.0f);
    shell.lineTo (cx, cy + 15.0f);
    g.setColour (theme::ice.withAlpha (0.12f));
    g.fillPath (shell);
    g.setColour (theme::ice);
    g.strokePath (shell, juce::PathStrokeType (1.3f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

    juce::Path traces;
    traces.startNewSubPath (cx + 1.5f, cy - 9.0f);
    traces.lineTo (cx + 6.0f, cy - 9.0f);
    traces.lineTo (cx + 9.0f, cy - 6.0f);
    traces.startNewSubPath (cx + 1.5f, cy + 6.0f);
    traces.lineTo (cx + 5.0f, cy + 6.0f);
    traces.lineTo (cx + 8.0f, cy + 9.0f);
    traces.startNewSubPath (cx + 1.5f, cy - 1.5f);
    traces.lineTo (cx + 4.5f, cy - 1.5f);
    g.setColour (theme::ice.withAlpha (0.85f));
    g.strokePath (traces, juce::PathStrokeType (1.0f));
    g.fillEllipse (cx + 8.0f, cy - 7.0f, 2.4f, 2.4f);
    g.fillEllipse (cx + 7.0f, cy + 8.0f, 2.4f, 2.4f);
    // olho de circuito (quadrado aceso)
    g.setColour (theme::amber);
    g.fillRect (cx + 4.5f, cy - 3.6f, 4.2f, 4.2f);

    // costura no meio
    g.setColour (theme::cream);
    g.fillRect (cx - 0.6f, cy - 15.0f, 1.2f, 30.0f);
}

// eixo de frequência em log, 20 Hz–20 kHz
float freqX (float hz, float x0, float x1)
{
    return x0 + (std::log10 (juce::jmax (20.0f, hz)) - std::log10 (20.0f)) / 3.0f * (x1 - x0);
}

// resposta aproximada de cada tipo de filtro (só para desenhar)
float filterMag (int type, float hz, float fc, float res01)
{
    using C = std::complex<float>;
    fc = juce::jmax (10.0f, fc);
    const C s (0.0f, hz / fc);
    const C one (1.0f, 0.0f);

    switch (type)
    {
        case 0:   // Ladder 24
        {
            const float k = res01 * 3.6f;
            const C p = one + s;
            return (1.0f + 0.5f * k) / std::abs (p * p * p * p + k);
        }
        case 1:   // Ladder 12
        {
            const float k = res01 * 1.8f;
            const C p = one + s;
            return (1.0f + 0.5f * k) / std::abs (p * p + k);
        }
        case 2: case 3: case 4: case 5:   // PB, PA, PF, Rejeita (SVF)
        {
            const float q = 0.6f + res01 * res01 * 14.0f;
            const C den = s * s + s / q + one;
            const C num = type == 2 ? one : type == 3 ? s * s : type == 4 ? s / q : s * s + one;
            return std::abs (num / den);
        }
        case 6:   // Pente: picos nos múltiplos do corte
        {
            const float gfb = 0.25f + 0.7f * res01;
            const float w = juce::MathConstants<float>::twoPi * hz / fc;
            const C e = std::polar (1.0f, -w);
            return 2.0f * (1.0f - gfb) / std::abs (one - gfb * e);
        }
        case 7:   // Formante: três picos de vogal
        {
            const float q = 3.0f + res01 * 9.0f;
            auto bp = [hz, q, one] (float f)
            {
                const C ss (0.0f, hz / f);
                const C den = ss * ss + ss / q + one;
                return std::abs ((ss / q) / den);
            };
            return bp (fc) + 0.6f * bp (fc * 1.8f) + 0.35f * bp (fc * 2.9f);
        }
        default:
            return 1.0f;
    }
}

// curva do envelope: − = rápida no começo (exponencial), + = devagar no começo
float envShape (float u, float curve)
{
    u = juce::jlimit (0.0f, 1.0f, u);
    const float p = 1.0f + std::abs (curve) * 0.04f;
    if (curve < -0.5f) return 1.0f - std::pow (1.0f - u, p);
    if (curve > 0.5f)  return std::pow (u, p);
    return u;
}

float envInverse (float v, float curve)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    const float p = 1.0f + std::abs (curve) * 0.04f;
    if (curve < -0.5f) return 1.0f - std::pow (1.0f - v, 1.0f / p);
    if (curve > 0.5f)  return std::pow (v, 1.0f / p);
    return v;
}

// forma do LFO numa volta (ph 0..1), −1..1
float lfoShapeValue (int shape, float ph)
{
    static const float steps[8] = { 0.6f, -0.3f, 0.9f, -0.8f, 0.2f, -0.55f, 0.75f, -0.1f };
    ph = ph - std::floor (ph);
    switch (shape)
    {
        case 0:  return std::sin (juce::MathConstants<float>::twoPi * ph);
        case 1:  return ph < 0.25f ? 4.0f * ph : ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f;
        case 2:  return 1.0f - 2.0f * ph;
        case 3:  return 2.0f * ph - 1.0f;
        case 4:  return ph < 0.5f ? 1.0f : -1.0f;
        case 5:  return steps[juce::jlimit (0, 7, (int) (ph * 8.0f))];
        case 6:
        {
            const float x = ph * 8.0f;
            const int   i = juce::jlimit (0, 7, (int) x);
            const float t = x - (float) i;
            const float a = steps[i], b = steps[(i + 1) % 8];
            return a + (b - a) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t));
        }
        default: return 0.0f;
    }
}

// cor de cada fonte da matriz (a mesma do bloco que a gera)
juce::Colour srcColour (int s)
{
    if (s >= srcEnv1 && s <= srcEnv3)     return theme::cream;
    if (s == srcLfo1 || s == srcLfo2)     return theme::leaf;
    if (s == srcRandom)                   return theme::violet;
    if (s >= srcMacro1 && s <= srcMacro4) return qui::jade;
    if (s == srcNone)                     return theme::dim;
    return qui::steel;   // velocidade, nota, mod wheel, pressão, slide, bend
}

juce::String srcGroup (int i)
{
    if (i <= srcNone)    return {};
    if (i <= srcEnv3)    return "ENVELOPES";
    if (i <= srcLfo2)    return "LFOS";
    if (i <= srcRandom)  return "TECLADO E CONTROLADOR";
    return "MACROS";
}

juce::String dstGroup (int i)
{
    if (i <= dstNone)          return {};
    if (i < dstO2Pitch)        return "OSC 1";
    if (i < dstO3Pitch)        return "OSC 2";
    if (i < dstPitch)          return "OSC 3";
    if (i < dstFACut)          return "FONTES";
    if (i < dstAmp)            return "FILTROS";
    return u8 ("VOZ E LFO");
}
} // namespace

// =====================================================================
// WAVETABLE 3D
// =====================================================================
WaveView::WaveView (QuimeraProcessor& p, int oscIndex) : proc (p), osc (oscIndex)
{
    onP  = raw (p, oscId (osc, "on"));
    tabP = raw (p, oscId (osc, "tab"));
    posP = raw (p, oscId (osc, "pos"));
    setInterceptsMouseClicks (false, false);
}

float WaveView::livePos() const
{
    if (proc.engine.tel.activeVoices.load() > 0)
        return juce::jlimit (0.0f, 1.0f, proc.engine.tel.pos[(size_t) osc].load());
    return juce::jlimit (0.0f, 1.0f, posP->load() * 0.01f);
}

void WaveView::tick()
{
    const float pos   = livePos();
    const int   table = juce::roundToInt (tabP->load());
    const bool  on    = onP->load() > 0.5f;
    if (std::abs (pos - shownPos) > 0.001f || table != shownTable || on != shownOn)
    {
        shownPos = pos;
        shownTable = table;
        shownOn = on;
        repaint();
    }
}

// quadro 0 na frente (embaixo, à esquerda), o último no fundo (em cima, à direita)
juce::Point<float> WaveView::pointAt (float frame, float u, float sample) const
{
    const float W = (float) getWidth(), H = (float) getHeight();
    const float waveW = W * 0.68f, depthX = W - 16.0f - waveW, depthY = H * 0.40f, amp = H * 0.19f;
    const float d  = frame / (float) (WaveBank::frames - 1);
    const float x0 = 8.0f + d * depthX;
    const float y0 = H - 9.0f - amp - d * depthY;
    return { x0 + u * waveW, y0 - sample * amp };
}

void WaveView::paint (juce::Graphics& g)
{
    const auto  r     = getLocalBounds().toFloat();
    const auto  col   = qui::oscColour (osc);
    const bool  on    = onP->load() > 0.5f;
    const int   table = juce::jlimit (0, numTables - 1, juce::roundToInt (tabP->load()));
    const float pos   = livePos();
    const float k     = on ? 1.0f : 0.45f;
    const auto& bank  = WaveBank::instance();
    constexpr int frames = WaveBank::frames, len = WaveBank::size;

    g.setColour (theme::dark);
    g.fillRoundedRectangle (r, 3.0f);

    // piso em perspectiva
    g.setColour (theme::line);
    for (float u : { 0.0f, 0.5f, 1.0f })
    {
        const auto a = pointAt (0.0f, u, 0.0f), b = pointAt ((float) (frames - 1), u, 0.0f);
        g.drawLine (a.x, a.y, b.x, b.y, 1.0f);
    }

    // os quadros empilhados (o desenho só muda quando troca a tabela ou o tamanho)
    if (cachedTable != table)
    {
        stack.clear();
        constexpr int pts = 64;
        for (int f = 0; f < frames; ++f)
        {
            const float* w = bank.frame (table, f);
            for (int i = 0; i <= pts; ++i)
            {
                const float u = (float) i / (float) pts;
                const int   s = juce::jmin (len - 1, (int) (u * (float) len));
                const auto  pt = pointAt ((float) f, u, w[s]);
                if (i == 0) stack.startNewSubPath (pt);
                else        stack.lineTo (pt);
            }
        }
        cachedTable = table;
    }
    g.setColour (col.withAlpha (0.17f * k));
    g.strokePath (stack, juce::PathStrokeType (1.0f));

    // o quadro de agora (entre dois quadros, interpolado)
    const float fpos = pos * (float) (frames - 1);
    const int   f0 = juce::jlimit (0, frames - 1, (int) fpos);
    const int   f1 = juce::jmin (frames - 1, f0 + 1);
    const float t  = fpos - (float) f0;
    const float* wa = bank.frame (table, f0);
    const float* wb = bank.frame (table, f1);

    juce::Path line, area;
    constexpr int pts = 160;
    area.startNewSubPath (pointAt (fpos, 0.0f, 0.0f));
    for (int i = 0; i <= pts; ++i)
    {
        const float u = (float) i / (float) pts;
        const int   s = juce::jmin (len - 1, (int) (u * (float) len));
        const float v = wa[s] + (wb[s] - wa[s]) * t;
        const auto  pt = pointAt (fpos, u, v);
        if (i == 0) line.startNewSubPath (pt);
        else        line.lineTo (pt);
        area.lineTo (pt);
    }
    area.lineTo (pointAt (fpos, 1.0f, 0.0f));
    area.closeSubPath();

    const auto top = pointAt (fpos, 0.0f, 1.0f), bottom = pointAt (fpos, 0.0f, -1.0f);
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.38f * k), 0.0f, top.y, col.withAlpha (0.04f * k), 0.0f, bottom.y, false));
    g.fillPath (area);
    const auto b0 = pointAt (fpos, 0.0f, 0.0f), b1 = pointAt (fpos, 1.0f, 0.0f);
    g.setColour (col.withAlpha (0.35f * k));
    g.drawLine (b0.x, b0.y, b1.x, b1.y, 1.0f);
    g.setColour (col.withAlpha (0.22f * k));
    g.strokePath (line, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (on ? col.brighter (0.25f) : col.withAlpha (0.5f));
    g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (theme::mono (9.0f));
    g.setColour (theme::dim);
    g.drawText ("QUADRO " + juce::String (juce::roundToInt (fpos) + 1).paddedLeft ('0', 2) + "/" + juce::String (frames),
                r.reduced (6.0f, 3.0f), juce::Justification::topLeft, false);
    if (! on)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (9.0f, true, 0.25f));
        g.drawText ("DESLIGADO", r.reduced (6.0f, 3.0f), juce::Justification::topRight, false);
    }

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

// =====================================================================
// FILTRO
// =====================================================================
FilterView::FilterView (QuimeraProcessor& p, int filterIndex) : proc (p), flt (filterIndex)
{
    onP   = raw (p, filterId (flt, "on"));
    typeP = raw (p, filterId (flt, "type"));
    cutP  = raw (p, filterId (flt, "cut"));
    resP  = raw (p, filterId (flt, "res"));
    mixP  = raw (p, filterId (flt, "mix"));
    setInterceptsMouseClicks (false, false);
}

float FilterView::liveCut() const
{
    if (proc.engine.tel.activeVoices.load() > 0)
    {
        const float c = proc.engine.tel.cutoff[(size_t) flt].load();
        if (c > 1.0f) return juce::jlimit (20.0f, 20000.0f, c);
    }
    return cutP->load();
}

void FilterView::tick()
{
    const std::array<float, 6> now { onP->load(), typeP->load(), cutP->load(), resP->load(), mixP->load(), liveCut() };
    bool changed = false;
    for (size_t i = 0; i < now.size(); ++i)
    {
        const bool  hz  = i == 2 || i == 5;
        const float tol = hz ? juce::jmax (0.5f, now[i] * 0.003f) : 0.01f;
        changed = changed || std::abs (now[i] - shown[i]) > tol;
    }
    if (changed)
    {
        shown = now;
        repaint();
    }
}

void FilterView::paint (juce::Graphics& g)
{
    const auto  r     = getLocalBounds().toFloat();
    const bool  on    = onP->load() > 0.5f;
    const int   type  = juce::jlimit (0, numFilterTypes - 1, juce::roundToInt (typeP->load()));
    const float res01 = juce::jlimit (0.0f, 1.0f, resP->load() * 0.01f);
    const float mix   = juce::jlimit (0.0f, 1.0f, mixP->load() * 0.01f);
    const float base  = juce::jlimit (20.0f, 20000.0f, cutP->load());
    const float now   = liveCut();
    const float k     = on ? 1.0f : 0.4f;

    g.setColour (theme::dark);
    g.fillRoundedRectangle (r, 3.0f);

    const float x0 = 4.0f, x1 = r.getWidth() - 4.0f, y0 = 16.0f, y1 = r.getHeight() - 4.0f;
    g.setColour (theme::line);
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.fillRect (freqX (f, x0, x1), y0, 1.0f, y1 - y0);

    auto yOf = [y0, y1] (float mag)
    {
        const float db = juce::jlimit (-30.0f, 18.0f, juce::Decibels::gainToDecibels (mag, -60.0f));
        return juce::jmap (db, 18.0f, -30.0f, y0, y1);
    };
    auto magAt = [type, res01, mix] (float hz, float fc)
    {
        return (1.0f - mix) + mix * filterMag (type, hz, fc, res01);
    };
    g.setColour (theme::line.brighter (0.2f));
    g.fillRect (x0, yOf (1.0f), x1 - x0, 1.0f);

    auto curve = [&] (float fc)
    {
        juce::Path p;
        bool first = true;
        for (float x = x0; x <= x1 + 0.01f; x += 1.5f)
        {
            const float hz = 20.0f * std::pow (1000.0f, (x - x0) / (x1 - x0));
            const float y  = yOf (magAt (hz, fc));
            if (first) p.startNewSubPath (x, y);
            else       p.lineTo (x, y);
            first = false;
        }
        return p;
    };

    // onde o knob deixou (fino) e onde a modulação levou (cheio)
    if (std::abs (std::log2 (now / base)) > 0.02f)
    {
        g.setColour (theme::dim.withAlpha (0.55f * k));
        g.strokePath (curve (base), juce::PathStrokeType (1.0f));
        const float bx = freqX (base, x0, x1), nx = freqX (now, x0, x1);
        g.setColour (theme::leaf.withAlpha (0.8f * k));
        g.fillRect (juce::jmin (bx, nx), y1 - 2.0f, std::abs (nx - bx), 2.0f);
    }

    auto live = curve (now);
    auto fill = live;
    fill.lineTo (x1, y1);
    fill.lineTo (x0, y1);
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (theme::coral.withAlpha (0.3f * k), 0.0f, y0,
                                             theme::coral.withAlpha (0.0f), 0.0f, y1, false));
    g.fillPath (fill);
    g.setColour (on ? theme::coral : theme::dim);
    g.strokePath (live, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float px = freqX (now, x0, x1), py = yOf (magAt (now, now));
    g.setColour (theme::cream.withAlpha (0.3f * k));
    g.fillEllipse (px - 5.5f, py - 5.5f, 11.0f, 11.0f);
    g.setColour (on ? theme::cream : theme::dim);
    g.fillEllipse (px - 3.0f, py - 3.0f, 6.0f, 6.0f);

    g.setFont (theme::mono (9.0f));
    g.setColour (on ? theme::cream : theme::dim);
    g.drawText (hzText (now), juce::Rectangle<float> (6.0f, 2.0f, r.getWidth() - 12.0f, 12.0f), juce::Justification::centredLeft, false);
    if (! on)
    {
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.25f));
        g.drawText ("DESLIGADO", juce::Rectangle<float> (6.0f, 2.0f, r.getWidth() - 12.0f, 12.0f), juce::Justification::centredRight, false);
    }

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

// =====================================================================
// ENVELOPE
// =====================================================================
EnvView::EnvView (QuimeraProcessor& p, int envIndex) : proc (p), env (envIndex)
{
    aP = raw (p, envId (env, "a"));
    dP = raw (p, envId (env, "d"));
    sP = raw (p, envId (env, "s"));
    rP = raw (p, envId (env, "r"));
    cP = raw (p, envId (env, "c"));
    setInterceptsMouseClicks (false, false);
}

void EnvView::tick()
{
    const float v  = juce::jlimit (0.0f, 1.0f, proc.engine.tel.env[(size_t) env].load());
    const float s  = juce::jlimit (0.0f, 1.0f, sP->load() * 0.01f);
    const float dv = v - lastLevel;

    // em que trecho a voz está (a telemetria só dá o nível; o trecho sai do movimento)
    if (v < 0.0005f && dv <= 0.0f)                       stage = 0;
    else if (dv > 0.0005f && v < 0.999f)                 stage = 1;
    else if (stage == 1 && (dv <= 0.0f || v >= 0.999f))  stage = 2;
    else if (stage == 0)                                 stage = std::abs (v - s) < 0.01f ? 3 : 2;   // abriu a tela no meio da nota
    if (stage == 2 && std::abs (v - s) < 0.01f)          stage = 3;
    if ((stage == 2 || stage == 3) && v < s - 0.015f)    stage = 4;
    lastLevel = v;
    level = v;

    const std::array<float, 5> now { aP->load(), dP->load(), sP->load(), rP->load(), cP->load() };
    if (now != shown || stage != shownStage || std::abs (level - shownLevel) > 0.002f)
    {
        shown = now;
        shownStage = stage;
        shownLevel = level;
        repaint();
    }
}

void EnvView::paint (juce::Graphics& g)
{
    const auto  r  = getLocalBounds().toFloat();
    const float a  = aP->load(), d = dP->load(), rl = rP->load(), c = cP->load();
    const float s  = juce::jlimit (0.0f, 1.0f, sP->load() * 0.01f);
    const auto  col = theme::cream;

    g.setColour (theme::dark);
    g.fillRoundedRectangle (r, 3.0f);

    // largura de cada trecho em escala log do tempo (o sustain tem largura fixa)
    const float wa = std::log1p (a / 10.0f) + 0.3f, wd = std::log1p (d / 10.0f) + 0.3f;
    const float ws = 1.8f, wr = std::log1p (rl / 10.0f) + 0.3f;
    const float total = wa + wd + ws + wr;
    const float x0 = 5.0f, x1 = r.getWidth() - 5.0f, y0 = 5.0f, y1 = r.getHeight() - 4.0f, W = x1 - x0;
    const float xa = x0 + W * wa / total, xd = xa + W * wd / total, xs = xd + W * ws / total;

    auto yOf = [y0, y1] (float lv) { return y1 - lv * (y1 - y0); };
    auto levelAt = [&] (float x)
    {
        if (x < xa) return envShape ((x - x0) / juce::jmax (0.001f, xa - x0), c);
        if (x < xd) return 1.0f - (1.0f - s) * envShape ((x - xa) / juce::jmax (0.001f, xd - xa), c);
        if (x < xs) return s;
        return s * (1.0f - envShape ((x - xs) / juce::jmax (0.001f, x1 - xs), c));
    };

    g.setColour (theme::line);
    for (float xx : { xa, xd, xs })
        g.fillRect (xx, y0, 1.0f, y1 - y0);

    juce::Path line;
    bool first = true;
    for (float x = x0; x <= x1 + 0.01f; x += 1.0f)
    {
        const float y = yOf (levelAt (juce::jmin (x, x1)));
        if (first) line.startNewSubPath (x, y);
        else       line.lineTo (x, y);
        first = false;
    }
    juce::Path fill (line);
    fill.lineTo (x1, y1);
    fill.lineTo (x0, y1);
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.25f), 0.0f, y0, col.withAlpha (0.02f), 0.0f, y1, false));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (line, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ponto de onde a voz mais recente está
    if (stage > 0)
    {
        float x = xs;
        switch (stage)
        {
            case 1:  x = x0 + (xa - x0) * envInverse (level, c); break;
            case 2:  x = xa + (xd - xa) * (1.0f - s > 0.001f ? envInverse ((1.0f - level) / (1.0f - s), c) : 0.0f); break;
            case 3:  x = (xd + xs) * 0.5f; break;
            default: x = xs + (x1 - xs) * (s > 0.001f ? envInverse (1.0f - level / s, c) : 1.0f - level); break;
        }
        const float y = yOf (level);
        g.setColour (theme::amber.withAlpha (0.3f));
        g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
        g.setColour (theme::amber);
        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    }

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

// =====================================================================
// LFO
// =====================================================================
LfoView::LfoView (QuimeraProcessor& p, int lfoIndex) : proc (p), lfo (lfoIndex)
{
    shapeP = raw (p, lfoId (lfo, "shape"));
    syncP  = raw (p, lfoId (lfo, "sync"));
    rateP  = raw (p, lfoId (lfo, "rate"));
    divP   = raw (p, lfoId (lfo, "div"));
    modeP  = raw (p, lfoId (lfo, "mode"));
    setInterceptsMouseClicks (false, false);
}

juce::String LfoView::infoText() const
{
    const bool sync = syncP->load() > 0.5f;
    const int  mode = juce::jlimit (0, 2, juce::roundToInt (modeP->load()));
    const juce::String rate = sync ? juce::String (divNames[juce::jlimit (0, numDivs - 1, juce::roundToInt (divP->load()))])
                                   : num1 (rateP->load()) + " Hz";
    return rate + u8 (" · ") + u8 (lfoModeNames[mode]).toUpperCase();
}

void LfoView::tick()
{
    const float v     = juce::jlimit (-1.0f, 1.0f, proc.engine.tel.lfo[(size_t) lfo].load());
    const int   shape = juce::jlimit (0, numLfoShapes - 1, juce::roundToInt (shapeP->load()));
    const auto  info  = infoText();
    if (std::abs (v - value) < 0.0005f && shape == shownShape && info == shownInfo)
        return;

    // fase estimada: o ponto da forma com esse valor, preferindo seguir em frente
    constexpr int n = 96;
    int   best = phaseIdx;
    float bestScore = 1.0e9f;
    for (int i = 0; i < n; ++i)
    {
        const float sv  = lfoShapeValue (shape, (float) i / (float) n);
        const int   fwd = (i - phaseIdx + n) % n;
        const float score = std::abs (sv - v) + 0.25f * (float) fwd / (float) n;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    phaseIdx = best;
    value = v;
    shownShape = shape;
    shownInfo = info;
    repaint();
}

void LfoView::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const int   shape = juce::jlimit (0, numLfoShapes - 1, juce::roundToInt (shapeP->load()));
    const auto  col = theme::leaf;
    const float x0 = 5.0f, x1 = r.getWidth() - 5.0f, cy = r.getCentreY(), amp = r.getHeight() * 0.38f;

    g.setColour (theme::dark);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (theme::line);
    g.fillRect (x0, cy, x1 - x0, 1.0f);

    juce::Path line;
    const int steps = juce::jmax (8, (int) (x1 - x0));
    for (int i = 0; i <= steps; ++i)
    {
        const float ph = (float) i / (float) steps;
        const float x  = x0 + ph * (x1 - x0);
        const float y  = cy - lfoShapeValue (shape, juce::jmin (ph, 0.9999f)) * amp;
        if (i == 0) line.startNewSubPath (x, y);
        else        line.lineTo (x, y);
    }
    juce::Path fill (line);
    fill.lineTo (x1, cy);
    fill.lineTo (x0, cy);
    fill.closeSubPath();
    g.setColour (col.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (line, juce::PathStrokeType (1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    const float px = x0 + (float) phaseIdx / 96.0f * (x1 - x0);
    const float py = cy - value * amp;
    g.setColour (theme::amber.withAlpha (0.3f));
    g.fillEllipse (px - 6.0f, py - 6.0f, 12.0f, 12.0f);
    g.setColour (theme::amber);
    g.fillEllipse (px - 3.0f, py - 3.0f, 6.0f, 6.0f);

    g.setColour (theme::dim);
    g.setFont (theme::mono (8.5f));
    g.drawText (shownInfo.isNotEmpty() ? shownInfo : infoText(), r.reduced (5.0f, 2.0f), juce::Justification::topRight, false);

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

// =====================================================================
// OSCILOSCÓPIO
// =====================================================================
ScopeView::ScopeView (QuimeraProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
}

void ScopeView::tick()
{
    constexpr int n = QuimeraProcessor::scopeSize;
    const int w = ((proc.scopeWrite.load() % n) + n) % n;   // a mais antiga fica em scopeWrite
    for (int i = 0; i < n; ++i)
        ring[(size_t) i] = proc.scope[(w + i) % n];

    // gatilho: subida pelo zero na metade mais antiga, para a onda ficar parada
    start = window;
    for (int t = window; t > 0; --t)
        if (ring[(size_t) (t - 1)] <= 0.0f && ring[(size_t) t] > 0.0f)
        {
            start = t;
            break;
        }

    peak = 0.0f;
    for (float v : ring)
        peak = juce::jmax (peak, std::abs (v));
    repaint();
}

void ScopeView::paint (juce::Graphics& g)
{
    const auto  r  = getLocalBounds().toFloat();
    const float cy = r.getCentreY(), amp = r.getHeight() * 0.44f;
    const float x0 = 6.0f, x1 = r.getWidth() - 6.0f;

    g.setColour (theme::dark);
    g.fillRoundedRectangle (r, 3.0f);

    g.setColour (theme::line.withAlpha (0.6f));
    for (int k = 1; k < 16; ++k)
        g.fillRect (x0 + (x1 - x0) * (float) k / 16.0f, 4.0f, 1.0f, r.getHeight() - 8.0f);
    for (float lv : { -0.5f, 0.5f })
        g.fillRect (x0, cy - lv * amp, x1 - x0, 1.0f);
    g.setColour (theme::line.brighter (0.25f));
    g.fillRect (x0, cy, x1 - x0, 1.0f);

    juce::Path p;
    for (int i = 0; i < window; ++i)
    {
        const float x = x0 + (x1 - x0) * (float) i / (float) (window - 1);
        const float y = cy - juce::jlimit (-1.1f, 1.1f, ring[(size_t) (start + i)]) * amp;
        if (i == 0) p.startNewSubPath (x, y);
        else        p.lineTo (x, y);
    }
    g.setColour (theme::amber.withAlpha (0.18f));
    g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (theme::cream);
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::AttributedString s;
    s.append ("PICO ", theme::mono (10.0f), theme::dim);
    s.append (peak > 1.0e-5f ? dbText (juce::Decibels::gainToDecibels (peak)) + "FS" : u8 ("—"),
              theme::mono (10.0f), peak > 0.89f ? theme::coral : theme::cream);
    s.setJustification (juce::Justification::topRight);
    s.draw (g, r.reduced (10.0f, 6.0f));

    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

// =====================================================================
// VOZES
// =====================================================================
VoiceMeter::VoiceMeter (QuimeraProcessor& p) : proc (p)
{
    voicesP = raw (p, id::voices);
    setInterceptsMouseClicks (false, false);
}

void VoiceMeter::tick()
{
    const auto& tel = proc.engine.tel;
    juce::uint32 mask = 0;
    for (size_t i = 0; i < tel.voice.size(); ++i)
        if (tel.voice[i].note.load() >= 0)
            mask |= (juce::uint32) 1 << i;
    const int count = tel.activeVoices.load();
    const int limit = juce::roundToInt (voicesP->load());
    if (mask != shownMask || count != shownCount || limit != shownLimit || mask != 0)   // com voz soando, o brilho anda
    {
        shownMask = mask;
        shownCount = count;
        shownLimit = limit;
        repaint();
    }
}

void VoiceMeter::paint (juce::Graphics& g)
{
    const auto& tel = proc.engine.tel;
    const int count = juce::jmax (0, shownCount), limit = juce::jlimit (1, maxVoices, shownLimit);

    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);

    g.setColour (theme::dim);
    g.setFont (theme::sans (8.0f, true, 0.2f));
    g.drawText ("VOZES", juce::Rectangle<float> (9.0f, 3.0f, 60.0f, 11.0f), juce::Justification::centredLeft, false);
    juce::AttributedString s;
    s.append (juce::String (count).paddedLeft ('0', 2), theme::mono (13.0f), count > 0 ? theme::cream : theme::dim);
    s.append ("/" + juce::String (limit).paddedLeft ('0', 2), theme::mono (10.0f), theme::dim);
    s.setJustification (juce::Justification::centredLeft);
    s.draw (g, { 9.0f, 14.0f, 64.0f, 16.0f });

    // 16 vozes: acesas com o volume de cada uma; além do limite ficam mais apagadas
    const float dx = 9.0f, x0 = (float) getWidth() - 8.0f * dx - 6.0f;
    for (int i = 0; i < maxVoices; ++i)
    {
        const float x = x0 + (float) (i % 8) * dx, y = i < 8 ? 9.0f : 19.0f;
        const juce::Rectangle<float> dot (x, y, 5.0f, 5.0f);
        const bool on = ((shownMask >> i) & 1u) != 0;
        if (on)
        {
            const float lv = juce::jlimit (0.0f, 1.0f, tel.voice[(size_t) i].level.load());
            g.setColour (theme::amber.withAlpha (0.25f * lv));
            g.fillEllipse (dot.expanded (2.0f));
            g.setColour (theme::amber.withAlpha (0.4f + 0.6f * lv));
        }
        else
            g.setColour (i < limit ? theme::segOff : theme::ledOff);
        g.fillEllipse (dot);
    }
}

// =====================================================================
// LINHA DA MATRIZ
// =====================================================================
MatrixRow::MatrixRow (QuimeraProcessor& p, int slotIndex)
    : proc (p), slot (slotIndex),
      src (p.apvts, slotId (slotIndex, "src"), qui::steel),
      dst (p.apvts, slotId (slotIndex, "dst"), theme::coral),
      amt (p.apvts, slotId (slotIndex, "amt"))
{
    srcP = raw (p, slotId (slot, "src"));
    dstP = raw (p, slotId (slot, "dst"));
    amtP = raw (p, slotId (slot, "amt"));
    src.groupFor = [] (int i) { return srcGroup (i); };
    dst.groupFor = [] (int i) { return dstGroup (i); };
    addAndMakeVisible (src);
    addAndMakeVisible (dst);
    addAndMakeVisible (amt);
}

void MatrixRow::resized()
{
    src.setBounds (40, 4, 170, 19);
    dst.setBounds (232, 4, 230, 19);
    amt.setBounds (474, 4, juce::jmax (40, getWidth() - 476), 19);
}

void MatrixRow::tick()
{
    const int   s = juce::roundToInt (srcP->load()), d = juce::roundToInt (dstP->load());
    const bool  used = s != srcNone && d != dstNone;
    const float v = used ? proc.engine.tel.slotValue[(size_t) slot].load() : 0.0f;
    const float a = juce::jlimit (-1.0f, 1.0f, v * amtP->load() * 0.01f);
    amt.setLive (a, srcColour (s), used);
    if (used != shownUsed || s != shownSrc || std::abs (a - shownLive) > 0.01f)
    {
        shownUsed = used;
        shownSrc = s;
        shownLive = a;
        repaint();
    }
}

void MatrixRow::paint (juce::Graphics& g)
{
    const auto  r   = getLocalBounds().toFloat();
    const auto  col = srcColour (shownSrc);
    const float a   = shownUsed ? juce::jlimit (0.0f, 1.0f, std::abs (shownLive)) : 0.0f;

    if (shownUsed)
    {
        g.setColour (theme::panel2.withAlpha (0.7f));
        g.fillRoundedRectangle (r, 3.0f);
    }
    g.setColour (shownUsed ? theme::txt : theme::dim);
    g.setFont (theme::mono (10.0f));
    g.drawText (juce::String (slot + 1).paddedLeft ('0', 2), juce::Rectangle<float> (5.0f, 0.0f, 22.0f, r.getHeight()),
                juce::Justification::centredLeft, false);

    // LED: acende com o tanto que a fonte está mexendo agora
    const juce::Rectangle<float> led (27.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);
    if (a > 0.02f)
    {
        g.setColour (col.withAlpha (0.35f * a));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (shownUsed ? col.withAlpha (0.35f + 0.65f * a) : theme::off);
    g.fillEllipse (led);

    for (float ax : { 216.0f, 465.0f })
    {
        juce::Path arrow;
        const float ay = r.getCentreY();
        arrow.addTriangle (ax, ay - 4.0f, ax + 6.0f, ay, ax, ay + 4.0f);
        g.setColour (shownUsed ? col : theme::off);
        g.fillPath (arrow);
    }
}

// =====================================================================
// EDITOR
// =====================================================================
QuimeraEditor::QuimeraEditor (QuimeraProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, qui::baseW, qui::baseH);
    addAndMakeVisible (canvas);

    lastLfoSync.fill (-1);
    buildHeader();
    buildSynth();
    buildMatrix();
    buildFx();
    buildBottom();
    setTab (0);

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto
    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) qui::baseW * qui::minScale), juce::roundToInt ((float) qui::baseH * qui::minScale),
                     juce::roundToInt ((float) qui::baseW * qui::maxScale), juce::roundToInt ((float) qui::baseH * qui::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) qui::baseW / (double) qui::baseH);
    setScale (proc.uiScale.load());
    startTimerHz (30);
}

QuimeraEditor::~QuimeraEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------- montagem ----------------
void QuimeraEditor::attachButton (juce::Button& b, const juce::String& paramId)
{
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramId, b));
}

void QuimeraEditor::addCard (int tab, int x, int y, int w, int h)
{
    Card c;
    c.r = juce::Rectangle<int> (x, y, w, h).toFloat();
    c.tab = tab;
    cards.push_back (c);
}

void QuimeraEditor::addTitle (int tab, float x, float y, const juce::String& prefix, const juce::String& accent, juce::Colour c)
{
    Title t;
    t.x = x;
    t.y = y;
    t.prefix = prefix;
    t.accent = accent;
    t.colour = c;
    t.tab = tab;
    titles.push_back (t);
}

void QuimeraEditor::addCaption (int tab, float x, float y, float w, float h, const juce::String& text,
                                juce::Justification just, float size, bool bold)
{
    captions.push_back (Caption { juce::Rectangle<float> (x, y, w, h), text, just, size, bold, tab });
}

void QuimeraEditor::addDesc (int tab, float x, float y, float w, float h, const juce::String& title, const juce::String& body)
{
    descs.push_back (Desc { juce::Rectangle<float> (x, y, w, h), title, body, tab });
}

void QuimeraEditor::buildHeader()
{
    auto* prev = add<IconButton> (-1, R (712, 18, 30, 32), IconButton::Icon::prev);
    presetName = add<PresetName> (-1, R (742, 18, 224, 32));
    auto* next = add<IconButton> (-1, R (966, 18, 30, 32), IconButton::Icon::next);
    auto* save = add<IconButton> (-1, R (996, 18, 30, 32), IconButton::Icon::save);
    voiceMeter = add<VoiceMeter> (-1, R (1036, 18, 152, 32), proc);

    prev->onClick       = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    next->onClick       = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    save->onClick       = [this] { askNewPreset(); };
    presetName->onClick = [this] { showPresetMenu(); };

    juce::StringArray names;
    names.add (u8 ("SÍNTESE"));
    names.add ("MATRIZ");
    names.add ("EFEITOS");
    tabs = add<TabBar> (-1, R (12, 64, 330, 22), names);
    tabs->onChange = [this] (int t) { setTab (t); };
}

void QuimeraEditor::buildSynth()
{
    // ---- osciladores ----
    for (int o = 0; o < numOsc; ++o)
    {
        const int  x = 12 + o * 290, y = 92;
        const auto col = qui::oscColour (o);
        addCard (0, x, y, 282, 258);
        addTitle (0, (float) x + 30.0f, (float) y + 9.5f, "0" + juce::String (o + 1) + u8 (" · "), "OSC", col);
        attachButton (*add<LedToggle> (0, R (x + 9, y + 8, 16, 16), col), oscId (o, "on"));

        auto* table = add<ChoiceBox> (0, R (x + 98, y + 6, 174, 20), proc.apvts, oscId (o, "tab"), col);
        table->labelFor = [] (int i) { return tableLabel (i); };
        table->groupFor = [] (int i) { return tableGroup (i); };
        table->groupInBox = true;

        waves[(size_t) o] = add<WaveView> (0, R (x + 10, y + 30, 262, 82), proc, o);
        add<ChoiceBox> (0, R (x + 10, y + 116, 128, 18), proc.apvts, oscId (o, "warp"), col, juce::String ("WARP"));
        add<ChoiceBox> (0, R (x + 144, y + 116, 128, 18), proc.apvts, oscId (o, "rt"), col, u8 ("SAÍDA"));

        const char* f1[] = { "pos", "wamt", "oct", "semi", "fine" };
        const juce::String l1[] = { u8 ("POSIÇÃO"), "WARP", "OITAVA", "SEMI", "FINO" };
        const char* f2[] = { "uni", "det", "wid", "lvl", "pan" };
        const juce::String l2[] = { u8 ("UNÍSSONO"), "DETUNE", "ABERTURA", "VOLUME", "PAN" };
        for (int k = 0; k < 5; ++k)
        {
            add<MiniKnob> (0, R (x + 11 + k * 52, y + 138, 52, 57), proc.apvts, oscId (o, f1[k]), l1[k], col, 34);
            add<MiniKnob> (0, R (x + 11 + k * 52, y + 196, 52, 57), proc.apvts, oscId (o, f2[k]), l2[k], col, 34);
        }
    }

    // ---- sub e ruído ----
    {
        const int x = 882, y = 92, x1 = x + 153;
        addCard (0, x, y, 306, 125);

        addTitle (0, (float) x + 28.0f, (float) y + 9.5f, u8 ("04 · "), "SUB", qui::sand);
        attachButton (*add<LedToggle> (0, R (x + 8, y + 8, 16, 16), qui::sand), id::subOn);
        add<MiniKnob> (0, R (x + 6, y + 30, 52, 57), proc.apvts, id::subLvl, "VOLUME", qui::sand, 34);
        add<ChoiceBox> (0, R (x + 62, y + 33, 84, 18), proc.apvts, id::subWave, qui::sand);
        auto* oct = add<ChoiceBox> (0, R (x + 62, y + 57, 84, 18), proc.apvts, id::subOct, qui::sand);
        oct->labelFor = [] (int i) { return u8 (i == 0 ? "−1 OIT." : "−2 OIT."); };
        add<ChoiceBox> (0, R (x + 8, y + 96, 138, 18), proc.apvts, id::subRt, qui::sand, u8 ("SAÍDA"));

        addTitle (0, (float) x1 + 28.0f, (float) y + 9.5f, u8 ("05 · "), u8 ("RUÍDO"), qui::steel);
        attachButton (*add<LedToggle> (0, R (x1 + 8, y + 8, 16, 16), qui::steel), id::noiseOn);
        add<MiniKnob> (0, R (x1 + 6, y + 30, 52, 57), proc.apvts, id::noiseColor, "COR", qui::steel, 34);
        add<MiniKnob> (0, R (x1 + 60, y + 30, 52, 57), proc.apvts, id::noiseLvl, "VOLUME", qui::steel, 34);
        add<ChoiceBox> (0, R (x1 + 8, y + 96, 138, 18), proc.apvts, id::noiseRt, qui::steel, u8 ("SAÍDA"));
    }

    // ---- voz ----
    {
        const int x = 882, y = 225;
        addCard (0, x, y, 306, 125);
        addTitle (0, (float) x + 10.0f, (float) y + 9.5f, u8 ("06 · "), "VOZ", qui::steel);
        add<ChoiceBox> (0, R (x + 120, y + 6, 176, 18), proc.apvts, id::polyMode, qui::steel, juce::String ("MODO"));
        const char* ids[] = { id::voices, id::glide, id::bend, id::drift, id::velAmp };
        const juce::String labs[] = { "VOZES", "GLIDE", "BEND", "DERIVA", "VELOC." };
        for (int k = 0; k < 5; ++k)
            add<MiniKnob> (0, R (x + 13 + k * 56, y + 34, 56, 57), proc.apvts, ids[k], labs[k], qui::steel, 34);
        addCaption (0, (float) x + 10.0f, (float) y + 94.0f, 286.0f, 26.0f,
                    u8 ("DERIVA: a afinação respira como num analógico. VELOC.: quanto a força da tecla mexe no volume."),
                    juce::Justification::centredLeft, 8.5f, false);
    }

    // ---- filtros ----
    for (int f = 0; f < numFilters; ++f)
    {
        const int x = 12 + f * 482, y = 358;
        addCard (0, x, y, 474, 121);
        addTitle (0, (float) x + 30.0f, (float) y + 9.5f, u8 (f == 0 ? "07 · " : "08 · "), f == 0 ? "FILTRO A" : "FILTRO B", theme::coral);
        attachButton (*add<LedToggle> (0, R (x + 9, y + 8, 16, 16), theme::coral), filterId (f, "on"));
        add<ChoiceBox> (0, R (x + 146, y + 6, 140, 18), proc.apvts, filterId (f, "type"), theme::coral, juce::String ("TIPO"));
        filters[(size_t) f] = add<FilterView> (0, R (x + 10, y + 30, 150, 84), proc, f);
        add<MiniKnob> (0, R (x + 164, y + 32, 64, 69), proc.apvts, filterId (f, "cut"), "CORTE", theme::coral, 46);

        const char* fl[] = { "res", "drv", "key", "env", "mix" };
        const juce::String lb[] = { "RESSON.", "DRIVE", "KEY", "ENV 2", "MIX" };
        for (int k = 0; k < 5; ++k)
            add<MiniKnob> (0, R (x + 232 + k * 47, y + 38, 47, 57), proc.apvts, filterId (f, fl[k]), lb[k], theme::coral, 34);

        if (f == 0)
            attachButton (*add<SwitchButton> (0, R (x + 300, y + 6, 164, 18), juce::String ("PARALELO"), u8 ("SÉRIE A → B"), theme::coral),
                          id::filterSerial);
    }

    // ---- macros ----
    {
        const int x = 976, y = 358;
        addCard (0, x, y, 212, 121);
        addTitle (0, (float) x + 10.0f, (float) y + 9.5f, u8 ("09 · "), "MACROS", qui::jade);
        for (int m = 0; m < numMacros; ++m)
            add<MiniKnob> (0, R (x + 6 + m * 50, y + 28, 50, 59), proc.apvts, macroId (m), "MACRO " + juce::String (m + 1), qui::jade, 36);
        addCaption (0, (float) x + 10.0f, (float) y + 92.0f, 192.0f, 22.0f, u8 ("Ligue cada macro aos destinos na aba MATRIZ."),
                    juce::Justification::centredLeft, 8.5f, false);
    }

    // ---- envelopes ----
    const juce::String envNames[] = { "VOLUME", "FILTRO", "MOD" };
    for (int e = 0; e < numEnvs; ++e)
    {
        const int x = 12 + e * 252, y = 487;
        addCard (0, x, y, 244, 121);
        addTitle (0, (float) x + 10.0f, (float) y + 6.0f, "ENV " + juce::String (e + 1) + u8 (" · "), envNames[e], theme::cream);
        envs[(size_t) e] = add<EnvView> (0, R (x + 8, y + 22, 228, 38), proc, e);
        const char* fl[] = { "a", "d", "s", "r", "c" };
        const juce::String lb[] = { "ATAQUE", "DECAIM.", "SUSTAIN", "SOLTURA", "CURVA" };
        for (int k = 0; k < 5; ++k)
            add<MiniKnob> (0, R (x + 7 + k * 46, y + 62, 46, 57), proc.apvts, envId (e, fl[k]), lb[k], theme::cream, 34);
    }

    // ---- LFOs ----
    for (int l = 0; l < numLfos; ++l)
    {
        const int x = 768 + l * 214, y = 487;
        addCard (0, x, y, 206, 121);
        addTitle (0, (float) x + 10.0f, (float) y + 6.0f, "", "LFO " + juce::String (l + 1), theme::leaf);
        attachButton (*add<LedToggle> (0, R (x + 146, y + 3, 54, 16), theme::leaf, juce::String ("SYNC")), lfoId (l, "sync"));
        lfos[(size_t) l] = add<LfoView> (0, R (x + 8, y + 22, 190, 38), proc, l);
        lfoRate[(size_t) l] = add<MiniKnob> (0, R (x + 6, y + 62, 50, 57), proc.apvts, lfoId (l, "rate"), "VELOC.", theme::leaf, 34);
        lfoDiv[(size_t) l]  = add<MiniKnob> (0, R (x + 6, y + 62, 50, 57), proc.apvts, lfoId (l, "div"), u8 ("DIVISÃO"), theme::leaf, 34);
        lfoSyncP[(size_t) l] = raw (proc, lfoId (l, "sync"));
        add<MiniKnob> (0, R (x + 54, y + 62, 50, 57), proc.apvts, lfoId (l, "rise"), "SUBIDA", theme::leaf, 34);

        auto* shape = add<ChoiceBox> (0, R (x + 108, y + 68, 90, 18), proc.apvts, lfoId (l, "shape"), theme::leaf);
        shape->labelFor = [] (int i)
        {
            static const char* const n[] = { "Seno", "Triângulo", "Serra Desce", "Serra Sobe", "Quadrada", "S&H", "Aleat. Suave" };
            return u8 (n[juce::jlimit (0, 6, i)]);
        };
        add<ChoiceBox> (0, R (x + 108, y + 92, 90, 18), proc.apvts, lfoId (l, "mode"), theme::leaf);
    }
}

void QuimeraEditor::buildMatrix()
{
    addCard (1, 12, 92, 790, 516);
    addTitle (1, 22.0f, 101.5f, u8 ("MATRIZ · "), u8 ("16 LIGAÇÕES"), theme::amber);
    addCaption (1, 60.0f, 116.0f, 160.0f, 12.0f, "FONTE");
    addCaption (1, 252.0f, 116.0f, 200.0f, 12.0f, "DESTINO");
    addCaption (1, 494.0f, 116.0f, 290.0f, 12.0f, u8 ("QUANTIDADE  (−100% A +100%)"));
    for (int s = 0; s < numSlots; ++s)
        rows[(size_t) s] = add<MatrixRow> (1, R (20, 132 + s * 29, 774, 27), proc, s);

    // macros grandes
    addCard (1, 810, 92, 378, 168);
    addTitle (1, 820.0f, 101.5f, "", "MACROS", qui::jade);
    for (int m = 0; m < numMacros; ++m)
    {
        add<Knob> (1, R (822 + m * 90, 118, 82, 82), proc.apvts, macroId (m), "MACRO " + juce::String (m + 1), true, qui::jade);
        addCaption (1, (float) (822 + m * 90), 202.0f, 82.0f, 12.0f, "MACRO " + juce::String (m + 1), juce::Justification::centred);
    }
    addCaption (1, 820.0f, 222.0f, 358.0f, 30.0f,
                u8 ("Um knob, vários destinos: escolha Macro 1–4 como fonte nas linhas ao lado."),
                juce::Justification::centredLeft, 9.5f, false);

    // MPE e bend
    addCard (1, 810, 268, 378, 340);
    addTitle (1, 820.0f, 277.5f, "", "MPE E BEND", qui::steel);
    attachButton (*add<KeyButton> (1, R (822, 304, 80, 44), juce::String ("MPE"), qui::steel), id::mpe);
    add<Knob> (1, R (914, 296, 84, 90), proc.apvts, juce::String (id::mpeBend), juce::String ("MPE BEND"), false, qui::steel);
    add<Knob> (1, R (1004, 296, 84, 90), proc.apvts, juce::String (id::bend), juce::String ("BEND"), false, qui::steel);
    addDesc (1, 822.0f, 402.0f, 354.0f, 200.0f, "COMO FUNCIONA",
             u8 ("Pressão e Slide vêm do controlador MPE (sem MPE, a pressão é o aftertouch). Com o MPE ligado, cada nota "
                 "tem o próprio bend, pressão e slide: escolha Pressão ou Slide como fonte numa linha da matriz. "
                 "MPE BEND é a faixa do bend por nota (o padrão MPE é ±48 st); BEND é a faixa da alavanca comum. "
                 "MPE e SEGURAR são de performance: trocar de preset não mexe neles."));
}

void QuimeraEditor::buildFx()
{
    addCard (2, 12, 92, 1176, 156);
    addTitle (2, 22.0f, 101.5f, u8 ("SAÍDA · "), u8 ("OSCILOSCÓPIO"), theme::amber);
    addCaption (2, 600.0f, 100.0f, 578.0f, 14.0f, u8 ("DEPOIS DE TODOS OS EFEITOS E DO VOLUME"), juce::Justification::centredRight);
    scope = add<ScopeView> (2, R (22, 118, 1156, 122), proc);

    // cada bloco: LED, número na ordem da corrente, nome, tipo (se tiver) e knobs na cor do bloco
    auto block = [this] (int index, const juce::String& name, juce::Colour col, const char* onId, const juce::String& desc)
    {
        const int x = 12 + (index % 4) * 296, y = index < 4 ? 256 : 436;
        addCard (2, x, y, 288, 172);
        addTitle (2, (float) x + 30.0f, (float) y + 9.5f, "0" + juce::String (index + 1) + u8 (" · "), name, col);
        attachButton (*add<LedToggle> (2, R (x + 9, y + 8, 16, 16), col), onId);
        if (desc.isNotEmpty())
            addCaption (2, (float) x + 10.0f, (float) y + 30.0f, 268.0f, 16.0f, desc, juce::Justification::centredLeft, 9.0f, false);
        return juce::Point<int> (x, y);
    };
    auto knob = [this] (juce::Point<int> at, int k, const char* pid, const juce::String& label, juce::Colour col)
    {
        add<Knob> (2, R (at.x + 8 + k * 68, at.y + 56, 68, 90), proc.apvts, juce::String (pid), label, false, col);
    };
    auto type = [this] (juce::Point<int> at, const char* pid, juce::Colour col)
    {
        add<ChoiceBox> (2, R (at.x + 160, at.y + 6, 118, 18), proc.apvts, juce::String (pid), col);
    };

    {
        const auto at = block (0, "HIPER", theme::violet, id::hypOn, u8 ("Cópias desafinadas em volta do som (super serra)."));
        knob (at, 0, id::hypRate, "VELOC.", theme::violet);
        knob (at, 1, id::hypDetune, "DETUNE", theme::violet);
        knob (at, 2, id::hypMix, "MIX", theme::violet);
    }
    {
        const auto at = block (1, u8 ("DISTORÇÃO"), theme::coral, id::dstOn, u8 ("Válvula, duro, dobra, bitcrush… TOM abre ou fecha."));
        type (at, id::dstType, theme::coral);
        knob (at, 0, id::dstDrive, "DRIVE", theme::coral);
        knob (at, 1, id::dstTone, "TOM", theme::coral);
        knob (at, 2, id::dstMix, "MIX", theme::coral);
    }
    {
        const auto at = block (2, "PHASER", theme::leaf, id::phsOn, u8 ("Vales que varrem o espectro, com realimentação."));
        knob (at, 0, id::phsRate, "VELOC.", theme::leaf);
        knob (at, 1, id::phsDepth, "PROFUND.", theme::leaf);
        knob (at, 2, id::phsFb, "REALIM.", theme::leaf);
        knob (at, 3, id::phsMix, "MIX", theme::leaf);
    }
    {
        const auto at = block (3, "CHORUS", theme::ice, id::choOn, u8 ("Engrossa e abre o estéreo."));
        knob (at, 0, id::choRate, "VELOC.", theme::ice);
        knob (at, 1, id::choDepth, "PROFUND.", theme::ice);
        knob (at, 2, id::choMix, "MIX", theme::ice);
    }
    {
        const auto at = block (4, "DELAY", theme::amber, id::dlyOn, {});
        type (at, id::dlyDiv, theme::amber);
        attachButton (*add<LedToggle> (2, R (at.x + 8, at.y + 29, 100, 16), theme::amber, juce::String ("PING-PONG")), id::dlyPing);
        delayTime = add<Readout> (2, R (at.x + 120, at.y + 29, 158, 16), theme::cream, juce::Justification (juce::Justification::centredRight));
        knob (at, 0, id::dlyFb, "REALIM.", theme::amber);
        knob (at, 1, id::dlyTone, "TOM", theme::amber);
        knob (at, 2, id::dlyMix, "MIX", theme::amber);
    }
    {
        const auto at = block (5, "REVERB", qui::jade, id::revOn, u8 ("Sala, placa ou hall; ABAFAR tira o brilho da cauda."));
        type (at, id::revType, qui::jade);
        knob (at, 0, id::revSize, "TAMANHO", qui::jade);
        knob (at, 1, id::revDecay, "DECAY", qui::jade);
        knob (at, 2, id::revDamp, "ABAFAR", qui::jade);
        knob (at, 3, id::revMix, "MIX", qui::jade);
    }
    {
        const auto at = block (6, "EQ", theme::cream, id::eqOn, u8 ("Três bandas no fim da corrente."));
        knob (at, 0, id::eqLow, "GRAVES", theme::cream);
        knob (at, 1, id::eqMid, u8 ("MÉDIOS"), theme::cream);
        knob (at, 2, id::eqMidF, "FREQ.", theme::cream);
        knob (at, 3, id::eqHigh, "AGUDOS", theme::cream);
    }
    {
        const auto at = block (7, "COMPRESSOR", qui::steel, id::cmpOn, u8 ("Multibanda: 3 bandas que sobem e descem (estilo OTT)."));
        type (at, id::cmpMode, qui::steel);
        knob (at, 0, id::cmpAmt, "QUANT.", qui::steel);
        knob (at, 1, id::cmpMix, "MIX", qui::steel);
        compGr = add<GrMeter> (2, R (at.x + 152, at.y + 66, 126, 54), qui::steel);
    }
}

void QuimeraEditor::buildBottom()
{
    addCard (-1, 12, 616, 1176, 112);
    attachButton (*add<KeyButton> (-1, R (22, 624, 78, 44), juce::String ("MPE"), qui::steel), id::mpe);
    attachButton (*add<KeyButton> (-1, R (22, 674, 78, 44), juce::String ("SEGURAR"), theme::amber), id::hold);

    keys = add<StyledKeyboard> (-1, R (108, 622, 940, 100), proc.keyboard, proc.engine.tel);
    keys->setKeyWidth (940.0f / 43.0f);   // C1..C7 = 43 teclas brancas
    keys->setLowestVisibleKey (24);

    add<Knob> (-1, R (1054, 624, 80, 90), proc.apvts, juce::String (id::volume), juce::String ("VOLUME"), false, theme::cream);
    meterOut = add<LedMeter> (-1, R (1146, 616, 34, 112), juce::String ("OUT"));
}

// ---------------- abas ----------------
void QuimeraEditor::setTab (int tab)
{
    currentTab = juce::jlimit (0, 2, tab);
    for (size_t k = 0; k < tabItems.size(); ++k)
        for (auto* c : tabItems[k])
            c->setVisible ((int) k == currentTab);
    updateLfoVisibility (true);
    canvas.repaint();
}

// VELOC. (Hz) ou DIVISÃO (grid do host), conforme o SYNC do LFO
void QuimeraEditor::updateLfoVisibility (bool force)
{
    for (size_t l = 0; l < lfoRate.size(); ++l)
    {
        if (lfoRate[l] == nullptr || lfoDiv[l] == nullptr || lfoSyncP[l] == nullptr) continue;
        const int sync = lfoSyncP[l]->load() > 0.5f ? 1 : 0;
        if (! force && sync == lastLfoSync[l]) continue;
        lastLfoSync[l] = sync;
        lfoRate[l]->setVisible (currentTab == 0 && sync == 0);
        lfoDiv[l]->setVisible (currentTab == 0 && sync == 1);
    }
}

// ---------------- presets ----------------
void QuimeraEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto names = proc.getPresetNames();
    const int  cur = proc.getPresetIndex();

    // Init no topo; os de fábrica em submenus por categoria; os do usuário em MEUS PRESETS
    std::array<juce::PopupMenu, numCategories> cats;
    std::array<bool, numCategories> catTicked {};
    juce::PopupMenu user;
    bool userTicked = false;
    for (int i = 0; i < names.size(); ++i)
    {
        const int  c = proc.getPresetCategory (i);
        const bool ticked = i == cur;
        if (c < 0)
            menu.addItem (i + 1, names[i], true, ticked);
        else if (c < numCategories)
        {
            cats[(size_t) c].addItem (i + 1, names[i], true, ticked);
            catTicked[(size_t) c] = catTicked[(size_t) c] || ticked;
        }
        else
        {
            user.addItem (i + 1, names[i], true, ticked);
            userTicked = userTicked || ticked;
        }
    }
    menu.addSeparator();
    for (int c = 0; c < numCategories; ++c)
        if (cats[(size_t) c].getNumItems() > 0)
            menu.addSubMenu (u8 (categoryNames[c]), cats[(size_t) c], true, std::unique_ptr<juce::Drawable>(), catTicked[(size_t) c]);
    menu.addSeparator();
    if (user.getNumItems() > 0)
        menu.addSubMenu ("MEUS PRESETS", user, true, std::unique_ptr<juce::Drawable>(), userTicked);
    else
        menu.addItem (menuNoUser, "MEUS PRESETS (nenhum ainda)", false);

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

    juce::Component::SafePointer<QuimeraEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetName),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->presetMenuChosen (result);
                        });
}

void QuimeraEditor::askNewPreset()
{
    juce::Component::SafePointer<QuimeraEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

void QuimeraEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<QuimeraEditor> safe (this);
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
        case menuNoUser:
            break;
        default:
            if (result > 0 && result < sizeMenuBase)
                proc.loadPreset (result - 1);
            break;
    }
}

// ---------------- timer (30 Hz): só a aba aberta se mexe ----------------
void QuimeraEditor::timerCallback()
{
    if (currentTab == 0)
    {
        for (auto* w : waves)   if (w != nullptr) w->tick();
        for (auto* f : filters) if (f != nullptr) f->tick();
        for (auto* e : envs)    if (e != nullptr) e->tick();
        for (auto* l : lfos)    if (l != nullptr) l->tick();
        updateLfoVisibility (false);
    }
    else if (currentTab == 1)
    {
        for (auto* r : rows) if (r != nullptr) r->tick();
    }
    else
    {
        if (scope != nullptr) scope->tick();
        if (delayTime != nullptr)
        {
            const float ms = proc.fx.delayTimeMs.load();
            delayTime->set (ms > 0.5f ? "TEMPO " + msText (ms) : juce::String ("TEMPO --"));
        }
        if (compGr != nullptr) compGr->setReduction (proc.fx.compGainReductionDb.load());
    }

    if (voiceMeter != nullptr) voiceMeter->tick();
    if (keys != nullptr) keys->tick();

    levelOut = juce::jmax (proc.meterOut.exchange (0.0f), levelOut * 0.85f);
    if (meterOut != nullptr) meterOut->setLevel (levelOut);

    if (presetName != nullptr)
        presetName->set (proc.getPresetName(), proc.getPresetIndex(), proc.getPresetNames().size());

    const float bpm = proc.shownBpm.load();
    const bool  syn = proc.synced.load();
    if (std::abs (bpm - lastBpm) > 0.05f || syn != lastSync)
    {
        lastBpm = bpm;
        lastSync = syn;
        canvas.repaint (480, 12, 228, 44);
    }
}

// ---------------- tamanho ----------------
void QuimeraEditor::setScale (float scale)
{
    scale = juce::jlimit (qui::minScale, qui::maxScale, scale);
    setSize (juce::roundToInt ((float) qui::baseW * scale), juce::roundToInt ((float) qui::baseH * scale));
}

void QuimeraEditor::resized()
{
    const float scale = (float) getWidth() / (float) qui::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}

void QuimeraEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

// ---------------- fundo da tela: cabeçalho, cartões, títulos e textos ----------------
void QuimeraEditor::drawCanvas (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    drawLogo (g);
    const float brandEnd = drawBrand (g, "QUIMERA", "SINTETIZADOR WAVETABLE", u8 ("ORGÂNICO + METÁLICO · MPE"));
    drawSync (g, proc.shownBpm.load(), proc.synced.load(), brandEnd, 700.0f);

    // dica da aba, à direita das abas
    {
        const juce::String hint = currentTab == 0 ? u8 ("ARRASTE NA VERTICAL · DUPLO CLIQUE VOLTA AO PADRÃO · SETAS TROCAM A LISTA")
                                : currentTab == 1 ? u8 ("O BRILHO MOSTRA A MODULAÇÃO AGORA · DUPLO CLIQUE NA QUANTIDADE ZERA")
                                                  : u8 ("ORDEM FIXA: HIPER → DISTORÇÃO → … → COMPRESSOR · O LED LIGA O EFEITO");
        g.setColour (theme::dim);
        g.setFont (theme::sans (8.5f, true, 0.15f));
        g.drawText (hint, juce::Rectangle<float> (352.0f, 64.0f, 836.0f, 22.0f), juce::Justification::centredRight, true);
    }

    auto visible = [this] (int tab) { return tab < 0 || tab == currentTab; };

    for (const auto& c : cards)
        if (visible (c.tab))
            drawCard (g, c.r);

    if (currentTab == 0)
    {
        g.setColour (theme::line);
        g.fillRect (1035.0f, 100.0f, 1.0f, 108.0f);   // entre SUB e RUÍDO
    }

    for (const auto& t : titles)
        if (visible (t.tab))
            drawTitle (g, t.x, t.y, t.prefix, t.accent, t.colour);

    for (const auto& c : captions)
        if (visible (c.tab))
        {
            g.setColour (theme::dim);
            g.setFont (theme::sans (c.size, c.bold, c.bold ? 0.15f : 0.0f));
            g.drawFittedText (c.text, c.r.toNearestInt(), c.just, 2, 0.85f);
        }

    for (const auto& d : descs)
        if (visible (d.tab))
            drawDesc (g, d.r, d.title, d.body);
}
