#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
inline const juce::Colour bg     { 0xff14120e };
inline const juce::Colour panel  { 0xff1d1a15 };
inline const juce::Colour panel2 { 0xff27231c };
inline const juce::Colour line   { 0xff353027 };
inline const juce::Colour txt    { 0xffece3d2 };
inline const juce::Colour dim    { 0xff8b8273 };
inline const juce::Colour amber  { 0xffffb347 };
inline const juce::Colour coral  { 0xffff6b4a };
inline const juce::Colour cream  { 0xfff4e9d4 };
inline const juce::Colour ice    { 0xff6ec3ff };
inline const juce::Colour violet { 0xffc49bff };
inline const juce::Colour leaf   { 0xff8fd16a };
inline const juce::Colour dark   { 0xff100e0b };
inline const juce::Colour segOff { 0xff3a342b };
inline const juce::Colour body   { 0xff2a261f };
inline const juce::Colour ledOff { 0xff2c2821 };
inline const juce::Colour off    { 0xff4a443a };

inline juce::Font sans (float h, bool bold = false, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}

inline juce::Font mono (float h)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
}
} // namespace theme

// =====================================================================================================
// acabamento de hardware: chassi de metal, placas em relevo, telas embutidas, LEDs com lente, parafusos.
// Luz sempre de cima: borda de cima clara, de baixo escura, sombra caindo para baixo.
// =====================================================================================================
namespace real
{
using RF = juce::Rectangle<float>;
using PF = juce::Point<float>;

inline juce::Colour black (float a) { return juce::Colours::black.withAlpha (a); }
inline juce::Colour white (float a) { return juce::Colours::white.withAlpha (a); }

inline RF circle (PF c, float r) { return { c.x - r, c.y - r, r * 2.0f, r * 2.0f }; }

inline juce::ColourGradient vertical (juce::Colour top, float y0, juce::Colour bottom, float y1)
{
    return juce::ColourGradient (top, 0.0f, y0, bottom, 0.0f, y1, false);
}

inline juce::ColourGradient radial (PF c, float r, juce::Colour inner, juce::Colour outer)
{
    return juce::ColourGradient (inner, c.x, c.y, outer, c.x + r, c.y, true);
}

// grão do metal (ruído fino + escovado horizontal), ladrilho de 512 px gerado uma vez;
// o ScaledCanvas segura uma referência, então ele vive enquanto houver janela aberta
struct Texture
{
    juce::Image tile { juce::Image::ARGB, 512, 512, true };

    Texture()
    {
        juce::Random rnd (1234567);
        float streak[512];
        for (auto& s : streak) s = (rnd.nextFloat() - 0.5f) * 5.0f;

        juce::Image::BitmapData bd (tile, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 512; ++y)
            for (int x = 0; x < 512; ++x)
            {
                const float wave = 0.6f + 0.4f * std::sin (juce::MathConstants<float>::twoPi * 6.0f * (float) x / 512.0f + (float) y);
                const float v    = (rnd.nextFloat() - 0.5f) * 7.0f + streak[y] * wave;     // em níveis de 0–255 sobre o fundo
                bd.setPixelColour (x, y, v > 0.0f ? white (juce::jmin (1.0f, v / 235.0f))
                                                  : black (juce::jmin (1.0f, -v / 20.0f)));
            }
    }
};

inline void fillTexture (juce::Graphics& g, const juce::Path& p, float opacity)
{
    juce::SharedResourcePointer<Texture> t;
    juce::FillType ft (t->tile, juce::AffineTransform::scale (0.5f));
    ft.setOpacity (opacity);
    g.setFillType (ft);
    g.fillPath (p);
}

// parafuso de cabeça redonda com fenda
inline void screw (juce::Graphics& g, PF c, float r, float angle)
{
    auto sh = radial (c.translated (0.0f, 1.0f), r * 1.6f, black (0.6f), black (0.0f));
    sh.addColour (0.6, black (0.35f));
    g.setGradientFill (sh);
    g.fillEllipse (circle (c.translated (0.0f, 1.0f), r * 1.6f));

    g.setGradientFill (vertical (juce::Colour (0xff6d665a), c.y - r, juce::Colour (0xff26221c), c.y + r));
    g.fillEllipse (circle (c, r));
    g.setGradientFill (radial (c.translated (-r * 0.35f, -r * 0.4f), r * 0.9f, white (0.35f), white (0.0f)));
    g.fillEllipse (circle (c, r));

    const PF d (std::cos (angle) * r * 0.7f, std::sin (angle) * r * 0.7f);
    juce::Path slot;
    slot.startNewSubPath (c - d);
    slot.lineTo (c + d);
    g.setColour (black (0.75f));
    g.strokePath (slot, juce::PathStrokeType (r * 0.32f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// fundo da janela: chapa escura com grão, luz de cima, vinheta e um parafuso em cada canto
inline void chassis (juce::Graphics& g, float w, float h)
{
    g.setGradientFill (vertical (theme::bg.brighter (0.03f), 0.0f, theme::bg, h));
    g.fillRect (0.0f, 0.0f, w, h);

    juce::Path all;
    all.addRectangle (0.0f, 0.0f, w, h);
    fillTexture (g, all, 1.0f);

    juce::ColourGradient vig (black (0.0f), w * 0.5f, h * 0.45f, black (0.7f), 0.0f, 0.0f, true);
    vig.addColour (0.45, black (0.0f));
    vig.addColour (0.75, black (0.25f));
    g.setGradientFill (vig);
    g.fillRect (0.0f, 0.0f, w, h);

    screw (g, { 8.0f, 8.0f }, 3.4f, 0.4f);
    screw (g, { w - 8.0f, 8.0f }, 3.4f, 1.2f);
    screw (g, { 8.0f, h - 8.0f }, 3.4f, 2.1f);
    screw (g, { w - 8.0f, h - 8.0f }, 3.4f, 0.9f);
}

// placa em relevo (os cards): sombra no chassi, face com grão, bisel claro em cima e escuro embaixo
inline void card (juce::Graphics& g, RF r, float corner = 4.0f)
{
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (black (0.11f));
        g.fillRoundedRectangle (r.expanded ((float) i).translated (0.0f, (float) i * 0.9f), corner + (float) i);
    }

    g.setGradientFill (vertical (theme::panel.brighter (0.12f), r.getY(), theme::panel.darker (0.15f), r.getBottom()));
    g.fillRoundedRectangle (r, corner);

    juce::Path face;
    face.addRoundedRectangle (r, corner);
    fillTexture (g, face, 0.45f);

    auto bevel = vertical (white (0.13f), r.getY(), black (0.55f), r.getBottom());
    bevel.addColour (juce::jmin (0.5, 8.0 / juce::jmax (1.0, (double) r.getHeight())), white (0.03f));
    bevel.addColour (juce::jmax (0.5, 1.0 - 12.0 / juce::jmax (1.0, (double) r.getHeight())), black (0.1f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

// rebaixo no painel (tela, trilho, poço de tecla): borda do recorte escura e a beirada de baixo pegando luz
inline void recess (juce::Graphics& g, RF r, float corner)
{
    g.setColour (white (0.07f));
    g.fillRoundedRectangle (r.expanded (1.0f).translated (0.0f, 0.8f), corner + 1.0f);
    g.setColour (black (0.85f));
    g.fillRoundedRectangle (r.expanded (1.0f), corner + 1.0f);
}

// tela embutida (gráficos, leituras): desenhar ANTES do conteúdo
inline void screen (juce::Graphics& g, RF r, float corner = 3.0f)
{
    recess (g, r, corner);
    g.setGradientFill (vertical (theme::dark, r.getY(), theme::dark.brighter (0.25f), r.getBottom()));
    g.fillRoundedRectangle (r, corner);
}

// vidro da tela: sombra interna em cima/esquerda e reflexo; desenhar DEPOIS do conteúdo
inline void glass (juce::Graphics& g, RF r, float corner = 3.0f)
{
    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);

    const float sh = juce::jmin (7.0f, r.getHeight() * 0.25f);      // telas baixas (seletores) com sombra mais curta
    g.setGradientFill (vertical (black (0.6f), r.getY(), black (0.0f), r.getY() + sh));
    g.fillRect (r.withHeight (sh));
    g.setGradientFill (juce::ColourGradient (black (0.4f), r.getX(), 0.0f, black (0.0f), r.getX() + 5.0f, 0.0f, false));
    g.fillRect (r.withWidth (5.0f));

    juce::Path shine;
    shine.startNewSubPath (r.getX(), r.getY());
    shine.lineTo (r.getRight(), r.getY());
    shine.lineTo (r.getRight(), r.getY() + r.getHeight() * 0.22f);
    shine.lineTo (r.getX(), r.getY() + r.getHeight() * 0.42f);
    shine.closeSubPath();
    g.setGradientFill (vertical (white (0.045f), r.getY(), white (0.0f), r.getY() + r.getHeight() * 0.5f));
    g.fillPath (shine);
}

// LED redondo: carcaça, lente, brilho em volta (bloom) e reflexo. level 0..1 = intensidade
inline void led (juce::Graphics& g, PF c, float r, juce::Colour colour, float level)
{
    g.setColour (black (0.8f));
    g.fillEllipse (circle (c, r + 1.2f));
    g.setColour (white (0.06f));
    g.fillEllipse (circle (c.translated (0.0f, 0.6f), r + 1.2f));
    g.setColour (black (0.85f));
    g.fillEllipse (circle (c, r + 1.0f));

    if (level > 0.01f)
    {
        level = juce::jmin (1.0f, level);
        auto bloom = radial (c, r * 3.4f, colour.withAlpha (0.42f * level), colour.withAlpha (0.0f));
        bloom.addColour (0.35, colour.withAlpha (0.16f * level));
        g.setGradientFill (bloom);
        g.fillEllipse (circle (c, r * 3.4f));

        const auto body = theme::ledOff.interpolatedWith (colour, 0.3f + 0.7f * level);
        auto lens = radial (c, r, colour.brighter (0.8f).interpolatedWith (colour, 1.0f - level * 0.6f), body.darker (0.5f));
        lens.addColour (0.55, body);
        g.setGradientFill (lens);
    }
    else
    {
        g.setGradientFill (radial (c, r, theme::ledOff.interpolatedWith (colour, 0.22f),
                                   theme::ledOff.interpolatedWith (colour, 0.12f).darker (0.6f)));
    }
    g.fillEllipse (circle (c, r));

    const auto hp = c.translated (-r * 0.35f, -r * 0.4f);
    g.setGradientFill (radial (hp, r * 0.55f, white (0.75f), white (0.0f)));
    g.fillEllipse (circle (hp, r * 0.55f));
}

// LED retangular (medidores, LED de tecla). glow < 1 diminui o halo (LEDs colados uns nos outros)
inline void ledBar (juce::Graphics& g, RF r, juce::Colour colour, float level, float glow = 1.0f)
{
    const float c = juce::jmin (r.getWidth(), r.getHeight()) * 0.45f;
    g.setColour (black (0.85f));
    g.fillRoundedRectangle (r.expanded (1.0f), c + 1.0f);

    if (level > 0.01f)
    {
        level = juce::jmin (1.0f, level);
        g.setColour (colour.withAlpha (0.20f * level * glow));
        g.fillRoundedRectangle (r.expanded (3.0f * glow), c + 3.0f * glow);
        g.setColour (colour.withAlpha (0.12f * level * glow));
        g.fillRoundedRectangle (r.expanded (6.0f * glow, 5.0f * glow), c + 6.0f * glow);
        auto lens = vertical (colour.brighter (0.7f), r.getY(), colour.darker (0.35f), r.getBottom());
        lens.addColour (0.5, colour);
        g.setGradientFill (lens);
    }
    else
    {
        g.setGradientFill (vertical (theme::ledOff.interpolatedWith (colour, 0.16f), r.getY(),
                                     theme::ledOff.interpolatedWith (colour, 0.1f).darker (0.5f), r.getBottom()));
    }
    g.fillRoundedRectangle (r, c);

    g.setColour (white (level > 0.01f ? 0.45f : 0.10f));
    g.fillRoundedRectangle (r.getX() + 1.0f, r.getY() + 0.5f, r.getWidth() - 2.0f, juce::jmax (0.8f, r.getHeight() * 0.3f), c);
}

// tampa de tecla em relevo dentro do poço; pressed = afundada (sem sombra, 1 px mais baixa). Devolve a face.
inline RF keycap (juce::Graphics& g, RF r, float corner, bool pressed)
{
    recess (g, r, corner);
    g.setColour (black (0.85f));
    g.fillRoundedRectangle (r, corner);

    auto cap = r.reduced (2.0f);
    if (pressed) cap = cap.withTrimmedTop (1.0f);
    else
    {
        g.setColour (black (0.55f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), corner - 1.0f);
    }

    g.setGradientFill (pressed ? vertical (theme::panel2.brighter (0.18f), cap.getY(), theme::panel2.darker (0.25f), cap.getBottom())
                               : vertical (theme::panel2.brighter (0.45f), cap.getY(), theme::panel2.darker (0.2f), cap.getBottom()));
    g.fillRoundedRectangle (cap, corner - 1.0f);

    auto bevel = vertical (white (pressed ? 0.10f : 0.2f), cap.getY(), black (0.5f), cap.getBottom());
    bevel.addColour (0.3, white (0.02f));
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (cap.reduced (0.5f), corner - 1.0f, 1.0f);
    return cap;
}

// texto gravado no painel (sombra de 1 px embaixo)
inline void engraved (juce::Graphics& g, const juce::String& text, RF area, juce::Colour colour, juce::Justification j)
{
    g.setColour (black (0.6f));
    g.drawText (text, area.translated (0.0f, 1.0f), j, true);
    g.setColour (colour);
    g.drawText (text, area, j, true);
}

// knob estilo Moog / Diva: preto canelado visto de cima com leve inclinação (a lateral aparece embaixo),
// caneluras que giram com o valor, tampa de alumínio torneado, risco pintado na cor do que ele controla
// e escala 0–10 serigrafada no painel. box = área quadrada; big = macro (números na escala e valor embaixo);
// numbered = false tira os números e as marcas da escala (só o knob)
inline void knob (juce::Graphics& g, RF box, float pos, juce::Colour on, bool big, const juce::String& valueText = {},
                  bool numbered = true)
{
    using juce::Colour;
    const float pi = juce::MathConstants<float>::pi;
    const float k  = juce::jmin (box.getWidth(), box.getHeight()) / (big ? 92.0f : 64.0f);
    const float cx = box.getCentreX();
    const float rB = (big ? 29.0f : 21.0f) * k, rT = (big ? 25.5f : 18.2f) * k, hg = (big ? 9.0f : 6.5f) * k;
    const float cyB = box.getCentreY() + hg * 0.35f, cyT = cyB - hg;
    const float startAngle = pi * 1.25f, endAngle = pi * 2.75f;
    const float a = startAngle + (endAngle - startAngle) * pos;

    auto pt = [cx] (float cy, float r, float an) { return PF (cx + r * std::sin (an), cy - r * std::cos (an)); };
    auto line = [&g] (PF p1, PF p2, juce::Colour c, float w, bool round)
    {
        juce::Path l;
        l.startNewSubPath (p1);
        l.lineTo (p2);
        g.setColour (c);
        g.strokePath (l, juce::PathStrokeType (w, juce::PathStrokeType::curved,
                                               round ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt));
    };

    // escala serigrafada no painel
    if (numbered)
    {
        const float t1 = (big ? 33.0f : 25.0f) * k, t2 = (big ? 37.0f : 28.5f) * k;
        for (int i = 0; i <= 10; ++i)
        {
            const float an    = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            const bool  major = i % 5 == 0;
            line (pt (cyB, major ? t1 - 1.2f * k : t1, an), pt (cyB, t2, an),
                  theme::txt.withAlpha (major ? 0.8f : 0.45f), (major ? 1.3f : 0.9f) * k, false);
            if (big)
            {
                const auto p = pt (cyB, 41.5f * k, an);
                g.setColour (theme::txt.withAlpha (0.7f));
                g.setFont (theme::sans (7.5f * k, true));
                g.drawText (juce::String (i), RF (p.x - 8.0f * k, p.y - 6.0f * k, 16.0f * k, 12.0f * k), juce::Justification::centred, false);
            }
        }
    }

    // sombra no painel (luz de cima, um pouco da esquerda)
    {
        const PF sc (cx + 1.5f * k, cyB + 2.5f * k);
        const float sr = rB + 6.0f * k;
        auto sh = radial (sc, sr, black (0.8f), black (0.0f));
        sh.addColour ((double) (rB / sr), black (0.5f));
        g.setGradientFill (sh);
        g.fillEllipse (circle (sc, sr));
    }

    // lateral (cilindro levemente cônico): metade de baixo da base até a metade de baixo do topo
    const int nf = big ? 44 : 32;     // caneluras (múltiplo de 4, para casar com a borda do topo)
    {
        juce::Path side;
        side.addCentredArc (cx, cyB, rB, rB, 0.0f, pi * 0.5f, pi * 1.5f, true);
        side.lineTo (cx - rT, cyT);
        side.addCentredArc (cx, cyT, rT, rT, 0.0f, pi * 1.5f, pi * 0.5f, false);
        side.closeSubPath();
        juce::ColourGradient sg (Colour (0xff060606), cx - rB, 0.0f, Colour (0xff040404), cx + rB, 0.0f, false);
        sg.addColour (0.2,  Colour (0xff2c2c2c));
        sg.addColour (0.38, Colour (0xff3a3a3a));
        sg.addColour (0.62, Colour (0xff161616));
        g.setGradientFill (sg);
        g.fillPath (side);

        for (int i = 0; i < nf; ++i)
        {
            const float th = a + (float) i * 2.0f * pi / (float) nf + pi * 0.5f, s = std::sin (th);
            if (s <= 0.02f) continue;
            const float c = std::cos (th);
            line ({ cx + rT * c, cyT + rT * s }, { cx + rB * c, cyB + rB * s }, black (0.75f), (0.5f + 0.9f * s) * k, false);
            const float th2 = th + pi / (float) nf, s2 = std::sin (th2), c2 = std::cos (th2);
            if (s2 > 0.02f)
                line ({ cx + rT * c2, cyT + rT * s2 }, { cx + rB * c2, cyB + rB * s2 },
                      white (0.03f + 0.16f * juce::jmax (0.0f, -c2) * s2 + 0.04f * s2), (0.4f + 0.6f * s2) * k, false);
        }

        juce::Path rim;
        rim.addCentredArc (cx, cyB, rB, rB, 0.0f, pi * 0.5f, pi * 1.5f, true);
        g.setColour (black (0.8f));
        g.strokePath (rim, juce::PathStrokeType (0.8f * k));
    }

    // topo: borda recortada pelas caneluras
    {
        juce::Path top;
        const int steps = nf * 6;
        for (int j = 0; j <= steps; ++j)
        {
            const float ph = (float) j / (float) steps * 2.0f * pi;
            const float r  = rT - 0.7f * k * (0.5f + 0.5f * std::cos ((float) nf * (ph - a)));
            const PF p (cx + r * std::cos (ph), cyT + r * std::sin (ph));
            if (j == 0) top.startNewSubPath (p); else top.lineTo (p);
        }
        top.closeSubPath();
        juce::ColourGradient tg (Colour (0xff3b3b3b), cx - rT, cyT - rT, Colour (0xff080808), cx + rT, cyT + rT, false);
        tg.addColour (0.5, Colour (0xff1b1b1b));
        g.setGradientFill (tg);
        g.fillPath (top);
    }

    // face levemente côncava + brilho de plástico
    {
        const PF c (cx, cyT);
        const float rf = rT * 0.86f;
        g.setGradientFill (vertical (Colour (0xff121212), cyT - rf, Colour (0xff2a2a2a), cyT + rf));
        g.fillEllipse (circle (c, rf));
        g.setGradientFill (vertical (black (0.7f), cyT - rf, white (0.18f), cyT + rf));
        g.drawEllipse (circle (c, rf), 0.9f * k);
        auto hl = radial (c.translated (-rf * 0.35f, -rf * 0.45f), rf * 0.9f, white (0.16f), white (0.0f));
        hl.addColour (0.5, white (0.05f));
        g.setGradientFill (hl);
        g.fillEllipse (circle (c, rf));

        juce::Graphics::ScopedSaveState save (g);
        juce::Path face;
        face.addEllipse (circle (c, rf));
        g.reduceClipRegion (face);
        juce::Path sheen;
        sheen.addEllipse (-rf * 0.8f, -rf * 0.35f, rf * 1.6f, rf * 0.7f);
        sheen.applyTransform (juce::AffineTransform::rotation (-0.3f).translated (cx - rf * 0.2f, cyT - rf * 0.75f));
        g.setColour (white (0.05f));
        g.fillPath (sheen);
    }

    // tampa de alumínio torneado
    {
        const PF c (cx, cyT);
        const float ra = rT * 0.6f;
        g.setColour (black (0.6f));
        g.fillEllipse (circle (c.translated (0.0f, 0.6f * k), ra + 0.8f * k));
        juce::ColourGradient al (Colour (0xffd8d6d2), cx - ra, cyT - ra, Colour (0xffb9b7b2), cx + ra, cyT + ra, false);
        al.addColour (0.45, Colour (0xff9a9893));
        al.addColour (0.55, Colour (0xff7c7a75));
        g.setGradientFill (al);
        g.fillEllipse (circle (c, ra));
        int i = 0;
        for (float r = ra * 0.15f; r < ra; r += 0.9f * k, ++i)
        {
            g.setColour ((i % 2) != 0 ? white (0.10f) : black (0.07f));
            g.drawEllipse (circle (c, r), 0.45f * k);
        }
        const float glints[2][2] = { { -0.9f, 0.22f }, { 2.25f, 0.12f } };     // reflexos do torneado
        for (const auto& gl : glints)
        {
            juce::Path wedge;
            wedge.addPieSegment (circle (c, ra), gl[0] + pi * 0.5f - 0.35f, gl[0] + pi * 0.5f + 0.35f, 0.0f);
            g.setColour (white (gl[1]));
            g.fillPath (wedge);
        }
        g.setGradientFill (vertical (white (0.5f), cyT - ra, black (0.5f), cyT + ra));
        g.drawEllipse (circle (c, ra), 0.6f * k);
    }

    // risco indicador pintado
    line (pt (cyT, rT * 0.18f, a), pt (cyT, rT - 0.9f * k, a), black (0.6f), (big ? 3.4f : 2.6f) * k, true);
    line (pt (cyT, rT * 0.18f, a), pt (cyT, rT - 1.2f * k, a), on.interpolatedWith (juce::Colours::white, 0.15f), (big ? 2.4f : 1.8f) * k, true);

    if (big && valueText.isNotEmpty())
    {
        g.setColour (theme::txt);
        g.setFont (theme::mono (11.0f * k));
        g.drawText (valueText, RF (box.getX(), box.getBottom() - 13.0f * k, box.getWidth(), 13.0f * k), juce::Justification::centred, false);
    }
}

// lâmpada piloto com lente facetada e aro cromado (Diva / Minimoog). r = raio da lente
inline void jewel (juce::Graphics& g, PF c, float r, juce::Colour colour, bool on)
{
    using juce::Colour;
    g.setColour (black (0.6f));
    g.fillEllipse (circle (c.translated (0.6f, 1.2f), r + 2.2f));
    auto chrome = vertical (Colour (0xffd9d6d0), c.y - r - 1.8f, Colour (0xffc4c0b8), c.y + r + 1.8f);
    chrome.addColour (0.45, Colour (0xff7a766f));
    chrome.addColour (0.55, Colour (0xff4a4741));
    g.setGradientFill (chrome);
    g.fillEllipse (circle (c, r + 1.8f));
    g.setColour (black (0.9f));
    g.fillEllipse (circle (c, r + 0.3f));

    if (on)
    {
        auto bloom = radial (c, r * 4.0f, colour.withAlpha (0.45f), colour.withAlpha (0.0f));
        bloom.addColour (0.4, colour.withAlpha (0.15f));
        g.setGradientFill (bloom);
        g.fillEllipse (circle (c, r * 4.0f));
        auto lens = radial (c, r, colour.brighter (1.2f), colour.darker (0.9f));
        lens.addColour (0.5, colour);
        g.setGradientFill (lens);
    }
    else
    {
        g.setGradientFill (radial (c, r, Colour (0xff1a1714).interpolatedWith (colour, 0.35f),
                                   Colour (0xff0a0908).interpolatedWith (colour, 0.12f)));
    }
    g.fillEllipse (circle (c, r));

    g.setColour (white (on ? 0.25f : 0.08f));     // facetas
    for (int i = 0; i < 6; ++i)
    {
        const float an = (float) i * juce::MathConstants<float>::pi / 3.0f + 0.3f;
        g.drawLine (c.x, c.y, c.x + r * std::cos (an), c.y + r * std::sin (an), 0.5f);
    }
    g.setColour (white (on ? 0.8f : 0.35f));
    g.fillEllipse (circle (c.translated (-r * 0.35f, -r * 0.38f), r * 0.28f));
}

// chave basculante creme (Minimoog / Diva). Em pé: ligada = metade de cima afundada.
// Se a área for mais larga que alta, a chave é desenhada deitada (ligada = esquerda afundada).
inline void rocker (juce::Graphics& g, RF r, bool on)
{
    using juce::Colour;
    juce::Graphics::ScopedSaveState save (g);
    if (r.getWidth() > r.getHeight())
    {
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getCentreX(), r.getCentreY()));
        r = r.withSizeKeepingCentre (r.getHeight(), r.getWidth());
    }
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();

    g.setColour (white (0.07f));                              // recorte no painel
    g.fillRoundedRectangle (x - 1.5f, y - 0.5f, w + 3.0f, h + 3.0f, 2.5f);
    g.setColour (black (0.95f));
    g.fillRoundedRectangle (x - 1.5f, y - 1.5f, w + 3.0f, h + 3.0f, 2.5f);

    const float px = x + 1.0f, pw = w - 2.0f, mid = y + h * 0.5f;
    const RF up (px, y + 1.0f, pw, h * 0.5f - 1.0f), dn (px, mid, pw, h * 0.5f - 1.0f);
    const RF raised = on ? dn : up, pressed = on ? up : dn;

    // metade afundada: mais escura, sombra perto do eixo
    g.setGradientFill (on ? vertical (Colour (0xff8d877c), pressed.getY(), Colour (0xff5e594f), pressed.getBottom())
                          : vertical (Colour (0xff5e594f), pressed.getY(), Colour (0xff8d877c), pressed.getBottom()));
    g.fillRect (pressed);
    g.setGradientFill (on ? vertical (black (0.0f), mid - 4.0f, black (0.5f), mid)
                          : vertical (black (0.5f), mid, black (0.0f), mid + 4.0f));
    g.fillRect (px, on ? mid - 4.0f : mid, pw, 4.0f);

    // metade levantada: clara, brilho na ponta
    auto lift = on ? vertical (Colour (0xffe4dccb), raised.getY(), Colour (0xfffffaf0), raised.getBottom())
                   : vertical (Colour (0xfffffaf0), raised.getY(), Colour (0xffe4dccb), raised.getBottom());
    lift.addColour (on ? 0.7 : 0.3, Colour (0xfff6efe0));
    g.setGradientFill (lift);
    g.fillRect (raised);
    g.setColour (white (0.7f));
    g.fillRect (px + 0.5f, on ? raised.getBottom() - 1.2f : raised.getY() + 0.3f, pw - 1.0f, 0.9f);
    g.setColour (black (0.35f));
    g.fillRect (px, mid - 0.4f, pw, 0.8f);

    g.setColour (black (0.55f));                              // marca da posição
    g.fillRect (px + pw * 0.5f - 2.5f, on ? dn.getBottom() - 5.0f : up.getY() + 3.0f, 5.0f, 1.6f);
}
} // namespace real

class BrandLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BrandLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, theme::panel2);
        setColour (juce::PopupMenu::textColourId, theme::txt);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::amber.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, theme::cream);
        setColour (juce::PopupMenu::headerTextColourId, theme::dim);
    }

    // knob de hardware (desenho em real::knob)
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float, float, juce::Slider& slider) override
    {
        const bool big = (bool) slider.getProperties()["big"];
        const bool clean = (bool) slider.getProperties()["clean"];   // knob limpo: sem escala nem valor
        real::knob (g, juce::Rectangle<int> (x, y, width, height).toFloat(), pos,
                    slider.findColour (juce::Slider::rotarySliderFillColourId), big,
                    big && ! clean ? juce::String (juce::roundToInt (slider.getValue())) : juce::String(), ! clean);
    }

    // fader vertical do DRY/WET: trilho rebaixado com a barra de LED acesa e tampa de fader de mesa
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (! slider.isVertical())
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
            return;
        }

        const float th = (float) thumbRadius;
        juce::Rectangle<float> rail ((float) x + ((float) width - 30.0f) * 0.5f, (float) y - th, 30.0f, (float) height + th * 2.0f);
        real::screen (g, rail, 3.0f);

        auto fill = rail.reduced (3.0f, 2.0f).withTop (sliderPos);
        g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (0.45f), 0.0f, fill.getY(),
                                                 theme::coral.withAlpha (0.18f), 0.0f, fill.getBottom(), false));
        g.fillRect (fill);
        g.setColour (real::black (0.28f));
        for (float yy = sliderPos + 3.0f; yy < fill.getBottom() - 1.0f; yy += 3.0f)     // grade da barra de LED
            g.fillRect (fill.getX(), yy, fill.getWidth(), 1.0f);
        real::glass (g, rail, 3.0f);

        g.setColour (real::black (0.9f));                                                 // fenda do fader
        g.fillRect (rail.getCentreX() - 1.0f, rail.getY() + 4.0f, 2.0f, rail.getHeight() - 8.0f);

        juce::Rectangle<float> cap (rail.getCentreX() - 17.0f, sliderPos - th, 34.0f, th * 2.0f);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (real::black (0.16f));
            g.fillRoundedRectangle (cap.expanded ((float) i).translated (0.0f, 2.0f), 2.0f + (float) i);
        }
        auto body = real::vertical (juce::Colour (0xfffbf3e3), cap.getY(), juce::Colour (0xff8f8676), cap.getBottom());
        body.addColour (0.45, theme::cream.darker (0.1f));
        body.addColour (0.55, theme::cream.darker (0.28f));
        g.setGradientFill (body);
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (real::black (0.55f));
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 0.5f, cap.getWidth() - 6.0f, 1.2f);
        g.setColour (real::white (0.6f));
        g.fillRect (cap.getX() + 2.0f, cap.getY() + 0.6f, cap.getWidth() - 4.0f, 0.8f);
    }

    int getSliderThumbRadius (juce::Slider& slider) override { return slider.isVertical() ? thumbRadius : 3; }

private:
    static constexpr int thumbRadius = 6;
};
