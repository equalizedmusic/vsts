#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// STOMPBOX — peças desenhadas em vetor (coordenadas da tela base). Acabamento FOSCO: brilho só de borda,
// nada de verniz (preferência dele desde a MESA).
namespace desenho
{
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
inline juce::Colour cor (uint32_t c) { return juce::Colour (c); }

inline juce::Font fonte (float h, bool negrito = true, const char* nome = nullptr, bool italico = false)
{
    int estilo = (negrito ? juce::Font::bold : juce::Font::plain) | (italico ? juce::Font::italic : 0);
    if (nome != nullptr) return juce::Font (juce::FontOptions (nome, h, estilo));
    return juce::Font (juce::FontOptions (h, estilo));
}

// fonte do nome do pedal (serigrafia)
inline juce::Font fontePedal (int f, float h)
{
    switch (f)
    {
        case 1:  return fonte (h, true, "Arial Black", true);
        case 2:  return fonte (h, true, "Georgia");
        case 3:  return fonte (h, false, "Impact");
        case 4:  return fonte (h, true, "Georgia", true);
        case 5:  return fonte (h, false, "Segoe UI Light");
        default: return fonte (h, true, "Arial Black");
    }
}

inline void texto (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour c, const juce::Font& f,
                   juce::Justification j = juce::Justification::centred)
{
    g.setColour (c); g.setFont (f); g.drawText (t, r, j, false);
}

// sombra suave de um retângulo arredondado (várias camadas, sem DropShadow por quadro)
inline void sombra (juce::Graphics& g, juce::Rectangle<float> r, float raio, float dy, float espalha, float alfa)
{
    for (int i = 6; i >= 1; --i)
    {
        const float e = espalha * (float) i / 6.0f;
        g.setColour (juce::Colours::black.withAlpha (alfa / 6.0f));
        g.fillRoundedRectangle (r.translated (0, dy).expanded (e), raio + e);
    }
}

inline void sombraElipse (juce::Graphics& g, juce::Rectangle<float> r, float dy, float espalha, float alfa)
{
    for (int i = 6; i >= 1; --i)
    {
        const float e = espalha * (float) i / 6.0f;
        g.setColour (juce::Colours::black.withAlpha (alfa / 6.0f));
        g.fillEllipse (r.translated (0, dy).expanded (e));
    }
}

// carcaça de alumínio pintada (pintura fosca, quina mais clara em cima, sombra embaixo)
inline void carcaca (juce::Graphics& g, juce::Rectangle<float> r, float raio, juce::Colour c, bool elipse = false)
{
    if (elipse) sombraElipse (g, r, 14.0f, 10.0f, 0.55f); else sombra (g, r, raio, 14.0f, 10.0f, 0.55f);
    juce::Path p;
    if (elipse) p.addEllipse (r); else p.addRoundedRectangle (r, raio);
    g.setColour (c); g.fillPath (p);
    juce::ColourGradient luz (juce::Colours::white.withAlpha (0.22f), r.getX(), r.getY(),
                              juce::Colours::black.withAlpha (0.20f), r.getRight(), r.getBottom(), false);
    luz.addColour (0.35, juce::Colours::white.withAlpha (0.0f));
    g.setGradientFill (luz); g.fillPath (p);
    // textura de pintura (pó fino, determinístico)
    juce::Random rnd (1234 + (int) r.getX());
    g.saveState(); g.reduceClipRegion (p);
    for (int i = 0; i < (int) (r.getWidth() * r.getHeight() / 90.0f); ++i)
    {
        const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
        g.fillRect (x, y, 1.2f, 1.2f);
    }
    g.restoreState();
    g.setColour (juce::Colours::white.withAlpha (0.30f)); g.strokePath (p, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (0, 0.8f));
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (1.0f));
}

inline void parafuso (juce::Graphics& g, juce::Point<float> c, float ang)
{
    const float r = 4.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e8e8), c.x - 1.5f, c.y - 1.5f, juce::Colour (0xff3a3a3a), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colour (0xff2a2a2a));
    const float dx = std::cos (ang) * (r - 1), dy = std::sin (ang) * (r - 1);
    g.drawLine (c.x - dx, c.y - dy, c.x + dx, c.y + dy, 1.0f);
}

inline void led (juce::Graphics& g, juce::Point<float> c, float r, bool aceso, juce::Colour cor = juce::Colour (0xffff3b2a))
{
    if (aceso)
    {
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (cor.withAlpha (0.10f * (float) (5 - i)));
            g.fillEllipse (c.x - r - i * 3.0f, c.y - r - i * 3.0f, (r + i * 3.0f) * 2, (r + i * 3.0f) * 2);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x - r * 0.3f, c.y - r * 0.4f, cor.darker (0.5f), c.x + r, c.y + r, true));
    }
    else
        g.setGradientFill (juce::ColourGradient (cor.darker (1.6f).withSaturation (0.6f), c.x - r * 0.3f, c.y - r * 0.4f, cor.darker (3.0f), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - r - 1, c.y - r - 1, r * 2 + 2, r * 2 + 2, 1.5f);
}

// knob: 0 preto serrilhado, 1 metal, 2 creme, 3 bico (chicken head). valor 0..1 -> -135°..+135°
inline void knob (juce::Graphics& g, juce::Point<float> c, float d, int estilo, float valor)
{
    const float r = d * 0.5f, ang = (-135.0f + 270.0f * juce::jlimit (0.0f, 1.0f, valor)) * juce::MathConstants<float>::pi / 180.0f;
    sombraElipse (g, { c.x - r, c.y - r, d, d }, 4.0f, 3.0f, 0.55f);
    if (estilo == 3)   // bico: base redonda preta + bico apontando
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), c.x - r * 0.4f, c.y - r * 0.5f, juce::Colour (0xff0c0c0c), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f);
        juce::Path bico;
        bico.startNewSubPath (-r * 0.30f, r * 0.55f);
        bico.quadraticTo (-r * 0.36f, -r * 0.5f, 0.0f, -r * 1.12f);
        bico.quadraticTo (r * 0.36f, -r * 0.5f, r * 0.30f, r * 0.55f);
        bico.closeSubPath();
        const auto t = juce::AffineTransform::rotation (ang).translated (c);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillPath (bico, t.translated (0, 2.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b3b), c.x - r, c.y, juce::Colour (0xff0d0d0d), c.x + r, c.y, false));
        g.fillPath (bico, t);
        g.setColour (juce::Colour (0xfff2efe6));
        g.drawLine (juce::Line<float> (c + juce::Point<float> (std::sin (ang), -std::cos (ang)) * (r * 0.35f),
                                       c + juce::Point<float> (std::sin (ang), -std::cos (ang)) * (r * 1.0f)), 2.2f);
        return;
    }
    const juce::Colour base = estilo == 1 ? juce::Colour (0xffa9a9a6) : estilo == 2 ? juce::Colour (0xffd9cfb3) : juce::Colour (0xff1c1c1c);
    const juce::Colour claro = estilo == 1 ? juce::Colour (0xffd8d8d4) : estilo == 2 ? juce::Colour (0xfff3ecda) : juce::Colour (0xff3a3a3a);
    g.setColour (base); g.fillEllipse (c.x - r, c.y - r, d, d);
    if (estilo != 2)   // serrilha da saia
    {
        g.setColour ((estilo == 1 ? juce::Colours::black : juce::Colours::white).withAlpha (estilo == 1 ? 0.18f : 0.07f));
        for (int i = 0; i < 40; ++i)
        {
            const float a = (float) i / 40.0f * juce::MathConstants<float>::twoPi + ang;
            g.drawLine (c.x + std::cos (a) * r * 0.84f, c.y + std::sin (a) * r * 0.84f, c.x + std::cos (a) * r * 0.99f, c.y + std::sin (a) * r * 0.99f, 1.0f);
        }
    }
    const float rt = estilo == 2 ? r * 0.72f : r * 0.80f;   // tampa
    juce::ColourGradient tampa (claro, c.x - rt * 0.5f, c.y - rt * 0.6f, base.darker (estilo == 0 ? 0.3f : 0.25f), c.x + rt, c.y + rt, true);
    g.setGradientFill (tampa); g.fillEllipse (c.x - rt, c.y - rt, rt * 2, rt * 2);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawEllipse (c.x - rt, c.y - rt, rt * 2, rt * 2, 0.8f);
    g.setColour (juce::Colours::white.withAlpha (estilo == 0 ? 0.10f : 0.25f));
    g.drawEllipse (c.x - rt + 0.6f, c.y - rt + 0.6f, rt * 2 - 1.2f, rt * 2 - 1.2f, 0.6f);
    const juce::Colour ponteiro = estilo == 0 ? juce::Colour (0xfff4f4f4) : estilo == 1 ? juce::Colour (0xff1e1e1e) : juce::Colour (0xff3b2a1a);
    g.setColour (ponteiro);
    const juce::Point<float> v (std::sin (ang), -std::cos (ang));
    g.drawLine (juce::Line<float> (c + v * (r * 0.18f), c + v * (r * 0.92f)), juce::jmax (2.0f, d * 0.065f));
}

// footswitch: porca sextavada + botão cromado fosco
inline void footswitch (juce::Graphics& g, juce::Point<float> c, float esc = 1.0f)
{
    const float rn = 30.0f * esc, rb = 21.0f * esc;
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = (float) i / 6.0f * juce::MathConstants<float>::twoPi;
        const juce::Point<float> p (c.x + std::cos (a) * rn, c.y + std::sin (a) * rn);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillPath (hex, juce::AffineTransform::translation (0, 4));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd4d4d0), c.x - rn, c.y - rn, juce::Colour (0xff5a5a58), c.x + rn, c.y + rn, false));
    g.fillPath (hex);
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.strokePath (hex, juce::PathStrokeType (1.0f));
    sombraElipse (g, { c.x - rb, c.y - rb, rb * 2, rb * 2 }, 4.0f * esc, 3.0f * esc, 0.6f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f0), c.x - rb * 0.4f, c.y - rb * 0.5f, juce::Colour (0xff4a4a48), c.x + rb, c.y + rb, true));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);
}

inline void jack (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe0e0dc), r.getX(), r.getY(), juce::Colour (0xff6e6e6a), r.getRight(), r.getY(), false));
    g.fillRoundedRectangle (r, 2.0f);
}

// cabo de áudio entre dois pontos, com barriga
inline void cabo (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, float barriga)
{
    juce::Path p; p.startNewSubPath (a);
    p.cubicTo (a.x + 20, a.y + barriga, b.x - 20, b.y + barriga, b.x, b.y);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (0, 5));
    g.setColour (juce::Colour (0xff0d0d0d)); g.strokePath (p, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (2.0f), juce::AffineTransform::translation (0, -2));
}

inline void plugue (juce::Graphics& g, juce::Point<float> c, int dir)   // dir +1: aponta para a direita
{
    juce::Rectangle<float> r (dir > 0 ? c.x : c.x - 16, c.y - 6, 16, 12);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f0), r.getX(), r.getY(), juce::Colour (0xff3a3a3a), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.0f);
}
} // namespace desenho
