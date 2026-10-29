#pragma once

#include <complex>
#include "PluginProcessor.h"
#include "Lessons.h"
#include "MontaUi.h"

// desenhos e textos usados pela tela e pelas peças
namespace md
{
using mu::RF;
using mu::u8;

inline const RF trayArea (14.0f, 92.0f, 214.0f, 510.0f);
inline const RF rackArea (240.0f, 92.0f, 650.0f, 510.0f);
inline const RF cardArea (904.0f, 92.0f, 282.0f, 510.0f);
inline const RF tipArea (621.0f, 348.0f, 224.0f, 226.0f);
inline const RF stripArea (14.0f, 614.0f, 1172.0f, 114.0f);
inline constexpr float slotW = 112.0f, slotH = 226.0f, faceY = 30.0f;   // o painel tem 30 de folga em cima e à direita (pinos)

inline float param (MontaMontaProcessor& p, const juce::String& pid) { return p.apvts.getRawParameterValue (pid)->load(); }

inline juce::String pieceName (int m, int lang)
{
    if (m == mm::output) return lang == 0 ? u8 ("SAÍDA") : juce::String ("OUTPUT");
    return mm::pieceText (m).name;
}

inline void dashed (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float on = 7.0f, float off = 5.0f)
{
    juce::Path d;
    const float dl[] = { on, off };
    juce::PathStrokeType (width).createDashedStroke (d, p, dl, 2);
    g.setColour (c);
    g.fillPath (d);
}

// forma de onda (algumas voltas), para o cartão do VCO
inline void drawWave (juce::Graphics& g, RF r, int wave, juce::Colour c, float cycles)
{
    juce::Path p;
    const int N = 240;
    for (int i = 0; i <= N; ++i)
    {
        const float ph = std::fmod (cycles * (float) i / (float) N, 1.0f);
        const float tri = 2.0f * std::abs (2.0f * ph - 1.0f) - 1.0f;
        const float saw = 2.0f * ph - 1.0f;
        float y;
        switch (wave)
        {
            case 0:  y = -tri; break;
            case 1:  y = 0.5f * (-tri + saw); break;
            case 2:  y = saw; break;
            case 3:  y = ph < 0.5f ? 1.0f : -1.0f; break;
            case 4:  y = ph < 0.3f ? 1.0f : -1.0f; break;
            default: y = ph < 0.14f ? 1.0f : -1.0f; break;
        }
        const float x = r.getX() + r.getWidth() * (float) i / (float) N;
        const float yy = r.getCentreY() - y * r.getHeight() * 0.4f;
        if (i == 0) p.startNewSubPath (x, yy); else p.lineTo (x, yy);
    }
    mu::glow (g, p, c, 1.6f);
}

// resposta do filtro ladder (protótipo analógico: 1 / ((1+s)^4 + k)), de 20 Hz a 20 kHz
inline void drawResponse (juce::Graphics& g, RF r, float cutoff, float emph, juce::Colour c, bool labels)
{
    const float k = 4.1f * emph;
    const float bottom = r.getBottom() - (labels ? 12.0f : 2.0f);
    juce::Path p;
    const int N = 120;
    for (int i = 0; i <= N; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / N);
        const std::complex<double> s (0.0, f / (double) cutoff);
        const auto h = 1.0 / (std::pow (1.0 + s, 4) + (double) k);
        const float db = (float) juce::jlimit (-48.0, 18.0, 20.0 * std::log10 (std::abs (h) * (1.0 + (double) k * 0.35) + 1.0e-9));
        const float x = r.getX() + r.getWidth() * (float) i / (float) N;
        const float y = juce::jmap (db, 18.0f, -48.0f, r.getY() + 2.0f, bottom);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    juce::Path fill (p);
    fill.lineTo (r.getRight(), bottom);
    fill.lineTo (r.getX(), bottom);
    fill.closeSubPath();
    g.setColour (c.withAlpha (0.12f));
    g.fillPath (fill);
    mu::glow (g, p, c, labels ? 2.0f : 1.4f);
    const float cx = r.getX() + r.getWidth() * (float) (std::log ((double) cutoff / 20.0) / std::log (1000.0));
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawLine (cx, r.getY() + 3.0f, cx, bottom, 1.0f);
    if (labels)
        mu::text (g, "CUTOFF", { cx - 30.0f, r.getBottom() - 11.0f, 60.0f, 10.0f }, 7.5f, juce::Colour (0xffbff3ff), juce::Justification::centred);
}

// envelope (ADSR): o tempo de cada trecho em escala de raiz, para caber de 1 ms a 5 s
inline void drawAdsr (juce::Graphics& g, RF r, float a, float d, float s, float rel, juce::Colour c, bool labels)
{
    auto w = [] (float ms) { return std::sqrt (juce::jmax (1.0f, ms)) / std::sqrt (5000.0f); };
    const float total = w (a) + w (d) + 0.5f + w (rel);
    const float unit = (r.getWidth() - 8.0f) / total;
    const float x0 = r.getX() + 4.0f, base = r.getBottom() - (labels ? 12.0f : 4.0f), top = r.getY() + 4.0f;
    const float xa = x0 + w (a) * unit, xd = xa + w (d) * unit, xs = xd + 0.5f * unit, xr = xs + w (rel) * unit;
    const float ys = base - (base - top) * s;
    juce::Path p;
    p.startNewSubPath (x0, base);
    p.lineTo (xa, top);
    p.quadraticTo (xa + (xd - xa) * 0.25f, ys, xd, ys);
    p.lineTo (xs, ys);
    p.quadraticTo (xs + (xr - xs) * 0.25f, base, xr, base);
    juce::Path fill (p);
    fill.closeSubPath();
    g.setColour (c.withAlpha (0.12f));
    g.fillPath (fill);
    mu::glow (g, p, c, labels ? 2.0f : 1.6f);
    if (labels)
    {
        const char* n[] = { "A", "D", "S", "R" };
        const float xs4[] = { (x0 + xa) * 0.5f, (xa + xd) * 0.5f, (xd + xs) * 0.5f, (xs + xr) * 0.5f };
        for (int i = 0; i < 4; ++i)
            mu::text (g, n[i], { xs4[i] - 10.0f, r.getBottom() - 11.0f, 20.0f, 10.0f }, 8.0f, mu::dim, juce::Justification::centred);
    }
}
} // namespace md
