#pragma once

// PAINEL DE HARDWARE dos efeitos (Criativo e Mix): o que vai em volta das peças renderizadas do Hardware.h.
//   Mix      -> rack de 19": orelhas de alumínio escovado com os furos e parafusos, frente pintada na cor do plugin,
//               seções serigrafadas, VU de ponteiro e alavancas de metal.
//   Criativo -> pedal: moldura de madeira, chapa esmaltada na cor do plugin, faixa de controles em chapa preta
//               (como a SÉRIE KNOB) e o knob grandão do FOLE com as lâmpadas em volta.
// Este arquivo é igual em todos os plugins (o original fica em instrumentos/_hardware — copiar para Source/ quando mudar).
// Depende do Theme.h (theme::, real::) e do Hardware.h (hw::) do plugin.

#include "Theme.h"
#include "Hardware.h"
#include <vector>

namespace painel
{
using RF = juce::Rectangle<float>;
using PF = juce::Point<float>;

// ======================================================================================
// RACK
// ======================================================================================

// parafuso de rack (cabeça Phillips com arruela) dentro do furo oblongo da orelha
inline void parafusoRack (juce::Graphics& g, PF c, float r)
{
    g.setColour (real::black (0.45f));
    g.fillEllipse (real::circle (c.translated (0.8f, 1.6f), r * 1.35f));
    g.setGradientFill (real::vertical (juce::Colour (0xffd2d0cb), c.y - r * 1.3f, juce::Colour (0xff6b6964), c.y + r * 1.3f));
    g.fillEllipse (real::circle (c, r * 1.3f));                                   // arruela
    g.setGradientFill (real::vertical (juce::Colour (0xff4a4844), c.y - r, juce::Colour (0xff161514), c.y + r));
    g.fillEllipse (real::circle (c, r));                                          // cabeça oxidada preta
    g.setGradientFill (real::radial (c.translated (-r * 0.35f, -r * 0.45f), r * 0.9f, real::white (0.28f), real::white (0.0f)));
    g.fillEllipse (real::circle (c, r));
    const float a = r * 0.55f, w = r * 0.26f;
    g.setColour (real::black (0.85f));
    g.fillRect (c.x - a, c.y - w * 0.5f, a * 2.0f, w);
    g.fillRect (c.x - w * 0.5f, c.y - a, w, a * 2.0f);
}

// orelha do rack: alumínio escovado dobrado, dois furos oblongos (parafuso em cima e embaixo, livres no meio)
inline void orelha (juce::Graphics& g, RF r, bool esquerda)
{
    auto metal = real::vertical (juce::Colour (0xffd6d5d1), r.getY(), juce::Colour (0xffa3a29d), r.getBottom());
    metal.addColour (0.5, juce::Colour (0xffbebdb8));
    g.setGradientFill (metal);
    g.fillRect (r);
    juce::Path p;
    p.addRectangle (r);
    real::fillTexture (g, p, 0.9f);

    // dobra: a beirada de dentro escurece (o metal vira para trás), a de fora pega luz
    const float d = 6.0f;
    juce::ColourGradient dobra (real::black (0.0f), esquerda ? r.getRight() - d : r.getX() + d, 0.0f,
                                real::black (0.38f), esquerda ? r.getRight() : r.getX(), 0.0f, false);
    g.setGradientFill (dobra);
    g.fillRect (esquerda ? r.withLeft (r.getRight() - d) : r.withWidth (d));
    g.setColour (real::white (0.5f));
    g.fillRect (esquerda ? r.withWidth (1.0f) : r.withLeft (r.getRight() - 1.0f));

    const float cx = r.getCentreX() + (esquerda ? -2.0f : 2.0f);
    const float ys[] = { 0.09f, 0.36f, 0.64f, 0.91f };
    for (int i = 0; i < 4; ++i)
    {
        const float y = r.getY() + r.getHeight() * ys[i];
        const RF furo (cx - 8.0f, y - 4.6f, 16.0f, 9.2f);
        g.setColour (real::white (0.45f));
        g.fillRoundedRectangle (furo.translated (0.0f, 0.8f), 4.6f);
        g.setColour (juce::Colour (0xff0b0b0a));
        g.fillRoundedRectangle (furo, 4.6f);
        if (i == 0 || i == 3)
            parafusoRack (g, { cx + (i == 0 ? -2.0f : 2.0f), y }, 5.0f);
    }
}

// rack de 19": orelhas nas pontas e a frente pintada no meio (de ear a W - ear)
inline void rack (juce::Graphics& g, float W, float H, float ear, juce::Colour tinta)
{
    orelha (g, { 0.0f, 0.0f, ear, H }, true);
    orelha (g, { W - ear, 0.0f, ear, H }, false);

    const RF f (ear, 0.0f, W - 2.0f * ear, H);
    auto pint = real::vertical (tinta.brighter (0.16f), 0.0f, tinta.darker (0.28f), H);
    pint.addColour (0.35, tinta);
    g.setGradientFill (pint);
    g.fillRect (f);
    juce::Path p;
    p.addRectangle (f);
    real::fillTexture (g, p, 0.35f);

    // tinta texturizada: luz de cima na beirada, sombra embaixo, junta escura com as orelhas
    g.setGradientFill (real::vertical (real::white (0.16f), 0.0f, real::white (0.0f), 5.0f));
    g.fillRect (f.withHeight (5.0f));
    g.setGradientFill (real::vertical (real::black (0.0f), H - 6.0f, real::black (0.45f), H));
    g.fillRect (f.withTop (H - 6.0f));
    g.setColour (real::black (0.7f));
    g.fillRect (ear - 1.0f, 0.0f, 1.5f, H);
    g.fillRect (W - ear - 0.5f, 0.0f, 1.5f, H);

    // quatro parafusos da frente
    real::screw (g, { ear + 10.0f, 10.0f }, 3.4f, 0.4f);
    real::screw (g, { W - ear - 10.0f, 10.0f }, 3.4f, 1.2f);
    real::screw (g, { ear + 10.0f, H - 10.0f }, 3.4f, 2.1f);
    real::screw (g, { W - ear - 10.0f, H - 10.0f }, 3.4f, 0.9f);
}

// seção serigrafada na frente do rack (no lugar das placas em relevo): moldura fina em tinta creme
inline void secao (juce::Graphics& g, RF r)
{
    g.setColour (real::black (0.14f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (theme::cream.withAlpha (0.22f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
}

// ======================================================================================
// PEDAL
// ======================================================================================

// travessa de madeira deitada (a imagem da lateral girada 90°)
inline void travessa (juce::Graphics& g, RF r, bool cima)
{
    juce::SharedResourcePointer<hw::Pecas> p;
    const auto& img = cima ? p->madeiraEsq : p->madeiraDir;
    if (img.isValid())
    {
        const float iw = (float) img.getWidth(), ih = (float) img.getHeight();
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImageTransformed (img, juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                         .translated (0.0f, iw)
                                         .scaled (r.getWidth() / ih, r.getHeight() / iw)
                                         .translated (r.getX(), r.getY()));
    }
    else
    {
        g.setColour (juce::Colour (0xff5a3520));
        g.fillRect (r);
    }
    // sombra que a madeira (mais alta) joga na chapa
    const float s = 9.0f;
    g.setGradientFill (real::vertical (real::black (cima ? 0.5f : 0.0f), cima ? r.getBottom() : r.getY() - s,
                                       real::black (cima ? 0.0f : 0.35f), cima ? r.getBottom() + s : r.getY()));
    g.fillRect (cima ? RF (r.getX(), r.getBottom(), r.getWidth(), s) : RF (r.getX(), r.getY() - s, r.getWidth(), s));
}

// pedal: chapa esmaltada na cor do plugin dentro da moldura de madeira (lado = largura das laterais, topo = das travessas)
inline void pedal (juce::Graphics& g, float W, float H, float lado, float topo, juce::Colour esmalte)
{
    const RF chapa (lado, topo, W - 2.0f * lado, H - 2.0f * topo);
    auto base = real::vertical (esmalte.brighter (0.22f), chapa.getY(), esmalte.darker (0.35f), chapa.getBottom());
    base.addColour (0.4, esmalte);
    g.setGradientFill (base);
    g.fillRect (chapa);
    juce::Path p;
    p.addRectangle (chapa);
    real::fillTexture (g, p, 0.22f);

    // verniz: reflexo largo e suave da luz do estúdio, de cima à esquerda
    juce::ColourGradient brilho (real::white (0.10f), chapa.getX() + chapa.getWidth() * 0.25f, chapa.getY(),
                                 real::white (0.0f), chapa.getX() + chapa.getWidth() * 0.55f, chapa.getY() + chapa.getHeight() * 0.6f, false);
    g.setGradientFill (brilho);
    g.fillRect (chapa);

    travessa (g, { 0.0f, 0.0f, W, topo }, true);
    travessa (g, { 0.0f, H - topo, W, topo }, false);
    hw::cheek (g, { 0.0f, 0.0f, lado, H }, true);
    hw::cheek (g, { W - lado, 0.0f, lado, H }, false);

    real::screw (g, { lado + 11.0f, topo + 11.0f }, 3.6f, 0.5f);
    real::screw (g, { W - lado - 11.0f, topo + 11.0f }, 3.6f, 1.4f);
    real::screw (g, { lado + 11.0f, H - topo - 11.0f }, 3.6f, 2.3f);
    real::screw (g, { W - lado - 11.0f, H - topo - 11.0f }, 3.6f, 0.8f);
}

// chapa preta parafusada (a faixa de controles da SÉRIE KNOB). sem parafusos onde o título fica no canto
inline void chapaPreta (juce::Graphics& g, RF r, bool parafusos = true)
{
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (real::black (0.12f));
        g.fillRoundedRectangle (r.expanded ((float) i * 0.5f).translated (0.0f, (float) i), 3.0f);
    }
    g.setGradientFill (real::vertical (juce::Colour (0xff1d1c1b), r.getY(), juce::Colour (0xff0e0e0d), r.getBottom()));
    g.fillRoundedRectangle (r, 3.0f);
    juce::Path p;
    p.addRoundedRectangle (r, 3.0f);
    real::fillTexture (g, p, 0.3f);
    auto bisel = real::vertical (real::white (0.14f), r.getY(), real::black (0.6f), r.getBottom());
    bisel.addColour (0.1, real::white (0.02f));
    g.setGradientFill (bisel);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    if (! parafusos) return;
    const float m = 8.0f;
    for (const auto& c : { PF (r.getX() + m, r.getY() + m), PF (r.getRight() - m, r.getY() + m),
                           PF (r.getX() + m, r.getBottom() - m), PF (r.getRight() - m, r.getBottom() - m) })
        real::screw (g, c, 2.6f, c.x * 0.07f);
}

// knobs do pedal: os quadros renderizados da SÉRIE KNOB (o grandão do FOLE e o pequeno de saia cromada)
struct Sprites
{
    juce::Image grande[16], mini[16];
    Sprites()
    {
        for (int q = 0; q < 16; ++q)
        {
            const auto n = juce::String (q).paddedLeft ('0', 2);
            grande[q] = hw::carrega ("pedal_knob_" + n + ".png");
            mini[q]   = hw::carrega ("pedal_mini_" + n + ".png");
        }
    }
};

inline int quadro (float anguloGraus, int caneluras)
{
    const float periodo = 360.0f / (float) caneluras;
    float f = std::fmod (anguloGraus, periodo);
    if (f < 0.0f) f += periodo;
    return juce::roundToInt (f / periodo * 16.0f) % 16;
}

// ponto da tampa que gira: (x, y) em unidades do Blender (y para cima) -> pixel; e = centro e pixels por unidade
inline PF projeta (PF c, float rx, float ry, float x, float y, float a)
{
    const float cs = std::cos (a), sn = std::sin (a);
    return { c.x + (x * cs - y * sn) * rx, c.y - (x * sn + y * cs) * ry };
}

// medidas do quadro grande (pedal_knob_*.png, 484 × 495) e do pequeno (pedal_mini_*.png, 140 × 144), do Mapa.h da SÉRIE KNOB
namespace grande { constexpr float w = 484.0f, h = 495.0f, baseX = 205.0f, baseY = 218.7f;
                   constexpr float tampaY = 195.99f, tampaRx = 25.02f, tampaRy = 24.38f, saiaY = 216.34f, saiaRx = 24.39f, saiaRy = 23.79f; }
namespace mini   { constexpr float w = 140.0f, h = 144.0f, tampaX = 57.95f, tampaY = 55.11f, un = 25.18f; }

// knob grandão: base no ponto 'base', s = escala do quadro (1 = tamanho do render). pos 0..1; emblema em unidades da tampa (y para cima)
inline void knobGrande (juce::Graphics& g, PF base, float s, float pos, const std::vector<std::vector<PF>>& emblema)
{
    juce::SharedResourcePointer<Sprites> sp;
    const float ang = 135.0f - 270.0f * juce::jlimit (0.0f, 1.0f, pos);   // giro no Blender (anti-horário visto de cima)
    hw::desenha (g, sp->grande[quadro (ang, 18)], { base.x - grande::baseX * s, base.y - grande::baseY * s, grande::w * s, grande::h * s });

    const float a = juce::degreesToRadians (ang), R = 6.1f;
    const PF tampa (base.x, base.y + (grande::tampaY - grande::baseY) * s), saia (base.x, base.y + (grande::saiaY - grande::baseY) * s);
    const float trx = grande::tampaRx * s, tRy = grande::tampaRy * s, un = 24.43f * s;
    const juce::Colour creme (0xffede3cc);

    g.setColour (juce::Colour (0xff0b0b0c));                                        // traço gravado na saia
    g.drawLine ({ projeta (saia, grande::saiaRx * s, grande::saiaRy * s, 0.0f, R * 1.03f, a),
                  projeta (saia, grande::saiaRx * s, grande::saiaRy * s, 0.0f, R * 1.19f, a) }, 0.18f * un);
    const auto p0 = projeta (tampa, trx, tRy, 0.0f, R * 0.44f, a), p1 = projeta (tampa, trx, tRy, 0.0f, R * 0.56f, a);
    g.setColour (real::black (0.5f));                                               // linha creme da tampa
    g.drawLine ({ p0.translated (0.6f, 0.8f), p1.translated (0.6f, 0.8f) }, 0.2f * un);
    g.setColour (creme);
    g.drawLine ({ p0, p1 }, 0.2f * un);

    // emblema em relevo, gira junto
    const float tam = R * 0.38f, esp = 2.0f * 0.085f * tam * un;
    juce::Path traco;
    for (const auto& ln : emblema)
    {
        juce::Path q;
        for (size_t k = 0; k < ln.size(); ++k)
        {
            const auto pt = projeta (tampa, trx, tRy, ln[k].x * tam, ln[k].y * tam, a);
            if (k == 0) q.startNewSubPath (pt); else q.lineTo (pt);
        }
        traco.addPath (q);
    }
    const juce::PathStrokeType st (esp, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path cheio;
    st.createStrokedPath (cheio, traco);
    const float r = R * 0.6f * trx;
    g.setColour (real::black (0.6f));
    g.fillPath (cheio, juce::AffineTransform::translation (0.65f * s * 2.0f, 1.0f * s * 2.0f));
    juce::ColourGradient metal (juce::Colour (0xfff4f4f6), tampa.x - r * 0.6f, tampa.y - r * 0.7f,
                                juce::Colour (0xff6e6e74), tampa.x + r * 0.6f, tampa.y + r * 0.7f, false);
    metal.addColour (0.5, juce::Colour (0xffb8b8be));
    g.setGradientFill (metal);
    g.fillPath (cheio);
    g.setColour (real::white (0.45f));
    g.strokePath (traco, juce::PathStrokeType (esp * 0.28f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (-esp * 0.15f, -esp * 0.18f));
}

// knob pequeno de saia cromada: tampa no ponto c, s = escala do quadro
inline void knobMini (juce::Graphics& g, PF c, float s, float pos)
{
    juce::SharedResourcePointer<Sprites> sp;
    const float ang = 135.0f - 270.0f * juce::jlimit (0.0f, 1.0f, pos);
    hw::desenha (g, sp->mini[quadro (ang, 14)], { c.x - mini::tampaX * s, c.y - mini::tampaY * s, mini::w * s, mini::h * s });
    const float a = juce::degreesToRadians (ang), R = 1.45f, u = mini::un * s;
    const auto p0 = projeta (c, u, u, 0.0f, R * 0.16f, a), p1 = projeta (c, u, u, 0.0f, R * 0.56f, a);
    g.setColour (real::black (0.5f));
    g.drawLine ({ p0.translated (0.5f, 0.7f), p1.translated (0.5f, 0.7f) }, 0.2f * u);
    g.setColour (juce::Colour (0xffede3cc));
    g.drawLine ({ p0, p1 }, 0.2f * u);
}

// ======================================================================================
// PEÇAS COMUNS
// ======================================================================================

// escala 0–10 serigrafada em volta do knob (c = centro da face)
inline void escala (juce::Graphics& g, PF c, float k, bool big, juce::Colour tinta)
{
    const float pi = juce::MathConstants<float>::pi;
    const float t1 = (big ? 32.5f : 24.5f) * k, t2 = (big ? 36.0f : 28.0f) * k;
    for (int i = 0; i <= 10; ++i)
    {
        const float an = pi * 1.25f + pi * 1.5f * (float) i / 10.0f;
        const bool major = i % 5 == 0;
        const float r1 = major ? t1 - 1.5f * k : t1;
        juce::Path l;
        l.startNewSubPath (c.x + r1 * std::sin (an), c.y - r1 * std::cos (an));
        l.lineTo (c.x + t2 * std::sin (an), c.y - t2 * std::cos (an));
        g.setColour (tinta.withAlpha (major ? 0.85f : 0.5f));
        g.strokePath (l, juce::PathStrokeType ((major ? 1.4f : 0.9f) * k));
        if (big)
        {
            const float rn = 40.5f * k;
            g.setColour (tinta.withAlpha (0.8f));
            g.setFont (theme::sans (7.0f * k, true));
            g.drawText (juce::String (i), RF (c.x + rn * std::sin (an) - 8.0f * k, c.y - rn * std::cos (an) - 5.0f * k, 16.0f * k, 10.0f * k),
                        juce::Justification::centred, false);
        }
    }
}

// alavanca de metal (toggle) vista de cima: porca sextavada cromada e a haste apontando para cima (ligada)
// ou para baixo (desligada). deitada = a haste aponta para a esquerda (ligada) ou direita
inline void alavanca (juce::Graphics& g, PF c, float rn, bool ligada, bool deitada = false)
{
    const float pi = juce::MathConstants<float>::pi;
    // arruela e porca sextavada
    g.setColour (real::black (0.5f));
    g.fillEllipse (real::circle (c.translated (0.8f, 1.4f), rn * 1.25f));
    g.setGradientFill (real::vertical (juce::Colour (0xffe2e0dc), c.y - rn * 1.2f, juce::Colour (0xff6c6a66), c.y + rn * 1.2f));
    g.fillEllipse (real::circle (c, rn * 1.2f));
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float an = pi / 6.0f + (float) i * pi / 3.0f;
        const PF p (c.x + rn * std::cos (an), c.y + rn * std::sin (an));
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    auto cromo = real::vertical (juce::Colour (0xfff2f1ee), c.y - rn, juce::Colour (0xff57554f), c.y + rn);
    cromo.addColour (0.45, juce::Colour (0xffb3b1ab));
    g.setGradientFill (cromo);
    g.fillPath (hex);
    g.setColour (real::black (0.45f));
    g.strokePath (hex, juce::PathStrokeType (0.6f));
    g.setColour (juce::Colour (0xff1b1a18));
    g.fillEllipse (real::circle (c, rn * 0.52f));

    // haste: cilindro afinando até a bolinha da ponta
    const float L = rn * 2.3f, w0 = rn * 0.36f, w1 = rn * 0.5f;
    juce::Graphics::ScopedSaveState save (g);
    const float giro = deitada ? (ligada ? -pi * 0.5f : pi * 0.5f) : (ligada ? 0.0f : pi);
    g.addTransform (juce::AffineTransform::rotation (giro, c.x, c.y));
    {
        juce::Path sombra;
        sombra.addRoundedRectangle (c.x - w1 + 1.5f, c.y - L + 2.0f, w1 * 2.0f, L, w1);
        g.setColour (real::black (0.35f));
        g.fillPath (sombra);
    }
    juce::Path haste;
    haste.startNewSubPath (c.x - w0, c.y);
    haste.lineTo (c.x - w1 * 0.8f, c.y - L + w1);
    haste.lineTo (c.x + w1 * 0.8f, c.y - L + w1);
    haste.lineTo (c.x + w0, c.y);
    haste.closeSubPath();
    juce::ColourGradient cil (juce::Colour (0xff55534e), c.x - w1, 0.0f, juce::Colour (0xff3a3935), c.x + w1, 0.0f, false);
    cil.addColour (0.3, juce::Colour (0xfff7f6f2));
    cil.addColour (0.55, juce::Colour (0xffa9a7a1));
    g.setGradientFill (cil);
    g.fillPath (haste);
    const PF ponta (c.x, c.y - L + w1);
    g.setGradientFill (real::radial (ponta.translated (-w1 * 0.35f, -w1 * 0.35f), w1 * 1.3f, juce::Colour (0xffffffff), juce::Colour (0xff5d5b56)));
    g.fillEllipse (real::circle (ponta, w1));
}

// VU de ponteiro: caixa preta, mostrador creme iluminado por trás, escala −20…+3 com a zona vermelha. agulha 0..1
inline float vuPos (float vu)    // dB VU -> posição no arco (a escala do VU é linear em tensão)
{
    const float lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
    return juce::jlimit (0.0f, 1.0f, (std::pow (10.0f, juce::jlimit (-20.0f, 3.0f, vu) / 20.0f) - lo) / (hi - lo));
}

inline void vu (juce::Graphics& g, RF r, float agulha, const juce::String& nome)
{
    real::recess (g, r, 3.0f);
    g.setGradientFill (real::vertical (juce::Colour (0xff2a2724), r.getY(), juce::Colour (0xff0f0e0d), r.getBottom()));
    g.fillRoundedRectangle (r, 3.0f);

    const auto face = r.reduced (3.5f);
    {
        juce::ColourGradient luz (juce::Colour (0xfffbe7b4), face.getCentreX(), face.getBottom(),
                                  juce::Colour (0xffcfae6c), face.getX(), face.getY(), true);
        g.setGradientFill (luz);
        g.fillRoundedRectangle (face, 2.0f);
    }

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (face.toNearestInt());
    const PF piv (face.getCentreX(), face.getBottom() + face.getHeight() * 0.15f);
    const float R = face.getHeight() * 0.95f, abre = 0.82f;          // arco de ±47°
    auto angDe = [abre] (float p) { return -abre + 2.0f * abre * p; };
    auto pt = [piv] (float rr, float an) { return PF (piv.x + rr * std::sin (an), piv.y - rr * std::cos (an)); };

    const float r0 = R * 0.78f;
    juce::Path arco, vermelho;
    arco.addCentredArc (piv.x, piv.y, r0, r0, 0.0f, angDe (0.0f), angDe (vuPos (0.0f)), true);
    vermelho.addCentredArc (piv.x, piv.y, r0 + 1.2f, r0 + 1.2f, 0.0f, angDe (vuPos (0.0f)), angDe (1.0f), true);
    g.setColour (juce::Colour (0xff2a241c));
    g.strokePath (arco, juce::PathStrokeType (0.9f));
    g.setColour (juce::Colour (0xffc8361f));
    g.strokePath (vermelho, juce::PathStrokeType (3.0f));

    const float marcas[] = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
    g.setFont (theme::sans (5.6f, true));
    for (float m : marcas)
    {
        const float an = angDe (vuPos (m));
        const bool grande = m == -20.0f || m == -10.0f || m == -5.0f || m == 0.0f || m == 3.0f;
        juce::Path l;
        l.startNewSubPath (pt (r0, an));
        l.lineTo (pt (r0 + (grande ? 5.0f : 3.5f), an));
        g.setColour (m > 0.0f ? juce::Colour (0xffc8361f) : juce::Colour (0xff2a241c));
        g.strokePath (l, juce::PathStrokeType (0.9f));
        if (grande)
        {
            const auto p = pt (r0 + 9.5f, an);
            g.drawText (m > 0.0f ? "+" + juce::String ((int) m) : juce::String ((int) -m), RF (p.x - 8.0f, p.y - 4.0f, 16.0f, 8.0f),
                        juce::Justification::centred, false);
        }
    }
    g.setColour (juce::Colour (0xff2a241c));
    g.setFont (theme::sans (8.0f, true, 0.1f));
    g.drawText ("VU", RF (face.getCentreX() - 15.0f, face.getBottom() - 18.0f, 30.0f, 10.0f), juce::Justification::centred, false);
    g.setFont (theme::sans (6.0f, true, 0.15f));
    g.drawText (nome, RF (face.getX() + 4.0f, face.getY() + 2.0f, 30.0f, 8.0f), juce::Justification::centredLeft, false);

    // agulha (com sombra no mostrador) e a tampa preta do eixo embaixo
    const float an = angDe (juce::jlimit (-0.02f, 1.03f, agulha));
    g.setColour (real::black (0.18f));
    g.drawLine ({ pt (R * 0.1f, an).translated (1.5f, 1.5f), pt (R * 0.95f, an).translated (1.5f, 1.5f) }, 1.2f);
    g.setColour (juce::Colour (0xff141210));
    g.drawLine ({ pt (R * 0.1f, an), pt (R * 0.95f, an) }, 1.1f);
    g.setGradientFill (real::vertical (juce::Colour (0xff2c2925), face.getBottom() - 6.0f, juce::Colour (0xff0c0b0a), face.getBottom()));
    g.fillRect (face.withTop (face.getBottom() - 6.0f));

    real::glass (g, face, 2.0f);
}
} // namespace painel
