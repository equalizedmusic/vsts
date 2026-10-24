#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "Theme.h"

namespace chama
{
// o FOGUINHO: espírito de fogo que dança no tempo do projeto (4/4) e fica mais eufórico quanto mais CHAMA
struct DanceState
{
    float euph = 0.0f;      // 0 = de boa, 1 = euforia total (segue o knob CHAMA)
    float beatPos = 0.0f;   // posição no compasso de 4/4: 0–4 (parte inteira = tempo, fração = dentro do tempo)
    int   beatAbs = 0;      // contador de tempos, para as faíscas mudarem a cada batida
    float t = 0.0f;
    bool  dancing = false;  // host tocando (ou relógio LIVRE); parado = espera o play
    bool  drop = false;     // DROP: agacha e segura o fôlego para a virada
};

namespace dancer_detail
{
inline float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }
inline float hashf (int i) { const float v = std::sin ((float) i * 78.233f) * 43758.5453f; return v - std::floor (v); }

// gota de fogo com a base em (0,0) e a ponta em (tipX, −h)
inline juce::Path teardrop (float w, float h, float tipX)
{
    juce::Path p;
    p.startNewSubPath (tipX, -h);
    p.cubicTo (tipX * 0.5f + w * 0.18f, -h * 0.78f, w * 0.5f, -h * 0.56f, w * 0.5f, -h * 0.31f);
    p.cubicTo (w * 0.5f, -h * 0.08f, w * 0.28f, 0.0f, 0.0f, 0.0f);
    p.cubicTo (-w * 0.28f, 0.0f, -w * 0.5f, -h * 0.08f, -w * 0.5f, -h * 0.31f);
    p.cubicTo (-w * 0.5f, -h * 0.56f, tipX * 0.5f - w * 0.18f, -h * 0.78f, tipX, -h);
    p.closeSubPath();
    return p;
}

// silhueta de chama com a base em (0,0): base redonda, uma língua de cada lado e a ponta em (tipX, −h).
// fl/fr = altura das línguas (0–1); sway = balanço das línguas
inline juce::Path flame (float w, float h, float tipX, float fl, float fr, float sway)
{
    juce::Path p;
    p.startNewSubPath (0.0f, 0.0f);
    p.cubicTo (w * 0.3f, 0.0f, w * 0.5f, -h * 0.12f, w * 0.5f, -h * 0.3f);
    p.cubicTo (w * 0.5f, -h * 0.4f, w * 0.46f, -h * 0.44f, w * 0.4f + sway, -h * (0.5f + 0.2f * fr));
    p.quadraticTo (w * 0.3f, -h * 0.5f, w * 0.2f, -h * 0.56f);
    p.cubicTo (w * 0.22f, -h * 0.74f, tipX + w * 0.1f, -h * 0.86f, tipX, -h);
    p.cubicTo (tipX - w * 0.12f, -h * 0.84f, -w * 0.22f, -h * 0.72f, -w * 0.2f, -h * 0.54f);
    p.quadraticTo (-w * 0.3f, -h * 0.46f, -w * 0.4f + sway, -h * (0.46f + 0.2f * fl));
    p.cubicTo (-w * 0.46f, -h * 0.4f, -w * 0.5f, -h * 0.38f, -w * 0.5f, -h * 0.28f);
    p.cubicTo (-w * 0.5f, -h * 0.12f, -w * 0.3f, 0.0f, 0.0f, 0.0f);
    p.closeSubPath();
    return p;
}

// braço = filete de fogo que afina da largura w0 (ombro) até w1 (ponta), ao longo de uma curva quadrática
inline juce::Path wisp (juce::Point<float> p0, juce::Point<float> pc, juce::Point<float> p1, float w0, float w1)
{
    constexpr int n = 12;
    juce::Point<float> l[n + 1], r[n + 1];
    for (int i = 0; i <= n; ++i)
    {
        const float t = (float) i / (float) n, u = 1.0f - t;
        const auto  p  = p0 * (u * u) + pc * (2.0f * u * t) + p1 * (t * t);
        const auto  dv = (pc - p0) * (2.0f * u) + (p1 - pc) * (2.0f * t);
        const float len = juce::jmax (1.0e-4f, dv.getDistanceFromOrigin());
        const float ww  = (w0 + (w1 - w0) * t) * 0.5f;
        const juce::Point<float> nrm (-dv.y / len * ww, dv.x / len * ww);
        l[i] = p + nrm;
        r[i] = p - nrm;
    }
    juce::Path a;
    a.startNewSubPath (l[0]);
    for (int i = 1; i <= n; ++i) a.lineTo (l[i]);
    for (int i = n; i >= 0; --i) a.lineTo (r[i]);
    a.closeSubPath();
    return a;
}

inline juce::Colour mixRgb (juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith (b, clamp01 (t)); }

inline juce::ColourGradient vgrad (juce::Colour top, float y0, juce::Colour bottom, float y1)
{
    return juce::ColourGradient (top, 0.0f, y0, bottom, 0.0f, y1, false);
}

inline juce::ColourGradient rgrad (juce::Point<float> c, float r, juce::Colour inner, juce::Colour outer)
{
    return juce::ColourGradient (inner, c.x, c.y, outer, c.x + r, c.y, true);
}

// estrelinha de 4 pontas (as faíscas de alegria)
inline void star (juce::Graphics& g, float x, float y, float r, juce::Colour c)
{
    juce::Path s;
    s.startNewSubPath (x, y - r);
    s.quadraticTo (x, y, x + r, y);
    s.quadraticTo (x, y, x, y + r);
    s.quadraticTo (x, y, x - r, y);
    s.quadraticTo (x, y, x, y - r);
    s.closeSubPath();
    g.setColour (c);
    g.fillPath (s);
}
} // namespace dancer_detail

// cx = centro, floorY = chão onde ele pisa (coordenadas do componente)
inline void drawDancer (juce::Graphics& g, float cx, float floorY, const DanceState& d)
{
    using namespace dancer_detail;
    using juce::Colour;
    const float pi   = juce::MathConstants<float>::pi;
    const bool  on   = d.dancing && ! d.drop;
    const float e    = d.drop ? 0.0f : clamp01 (d.euph);
    const int   beat = juce::jlimit (0, 3, (int) std::floor (d.beatPos));
    const float b    = on ? d.beatPos - std::floor (d.beatPos) : 0.0f;   // 0 = em cima do tempo
    const float accent = beat == 0 ? 1.25f : 1.0f;                      // o 1 do compasso pesa mais

    // ---- movimento no tempo ----
    const float jumpAmt = on ? std::pow (clamp01 ((e - 0.35f) / 0.65f), 1.3f) : 0.0f;
    const float hop     = on ? std::sin (pi * b) : 0.0f;                    // sobe entre um tempo e outro
    const float jump    = 40.0f * jumpAmt * accent * hop;
    const float land    = on ? std::exp (-b * 9.0f) : 0.0f;                 // achata ao cair no tempo
    const float nod     = on ? (0.35f + 0.65f * clamp01 (e * 2.0f)) * std::exp (-b * 6.0f) : 0.0f;
    const float breathe = 0.5f + 0.5f * std::sin (d.t * 2.1f);
    const float side    = (beat % 2 == 0) ? 1.0f : -1.0f;                   // balança de um lado para o outro
    const float tilt    = on ? side * (0.03f + 0.11f * e) * std::sin (pi * b) : 0.02f * std::sin (d.t * 0.9f);

    float sx = 1.0f + (0.07f + 0.15f * e) * land - 0.06f * hop * jumpAmt;
    float sy = 1.0f - (0.06f + 0.14f * e) * land + 0.09f * hop * jumpAmt + 0.025f * breathe - 0.05f * nod;
    if (d.drop) { sx = 1.14f; sy = 0.80f; }

    const float grow = d.drop ? 0.8f : 0.82f + 0.3f * e;                  // cresce com a CHAMA
    const float w = 60.0f * sx * grow, h = 100.0f * sy * grow;
    const float footY = floorY - jump;

    // cores: brasa de boa → laranja → amarelo quase branco no talo
    const auto outerC = mixRgb (Colour (0xffc42812), Colour (0xffff5c1a), e);
    const auto midC   = mixRgb (Colour (0xffec6018), Colour (0xffffa028), e);
    const auto innC   = mixRgb (Colour (0xffffb23c), Colour (0xffffe478), e);
    const auto coreC  = mixRgb (Colour (0xffffd68c), Colour (0xfffffae4), e);

    // ---- chão: brilho quente + sombra que encolhe quando ele pula ----
    {
        const float k = 1.0f - 0.45f * clamp01 (jump / 50.0f);
        g.setGradientFill (rgrad ({ cx, floorY }, w * 1.3f, Colour::fromFloatRGBA (1.0f, 0.47f, 0.12f, 0.22f + 0.25f * e),
                                  Colour::fromFloatRGBA (1.0f, 0.35f, 0.08f, 0.0f)));
        g.fillEllipse (cx - w * 1.3f, floorY - 12.0f, w * 2.6f, 24.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f * k));
        g.fillEllipse (cx - w * 0.42f * k, floorY - 3.5f, w * 0.84f * k, 7.0f);
    }

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (tilt).translated (cx, footY - 3.0f));

    // ---- halo: brilha mais quando ele está eufórico ----
    {
        const float r = h * (0.65f + 0.35f * e + 0.15f * land * e);
        auto halo = rgrad ({ 0.0f, -h * 0.4f }, r, Colour::fromFloatRGBA (1.0f, 0.51f, 0.16f, 0.14f + 0.3f * e),
                           Colour::fromFloatRGBA (1.0f, 0.39f, 0.1f, 0.0f));
        halo.addColour (0.5, Colour::fromFloatRGBA (1.0f, 0.43f, 0.12f, 0.05f + 0.1f * e));
        g.setGradientFill (halo);
        g.fillEllipse (-r, -h * 0.4f - r, r * 2.0f, r * 2.0f);
    }

    // ---- brasas subindo: mais e mais alto quanto mais CHAMA ----
    {
        const int n = juce::roundToInt (2.0f + 10.0f * e);
        for (int i = 0; i < n; ++i)
        {
            const float sp  = 0.35f + 0.5f * hashf (i + 3);
            const float cyc = std::fmod (d.t * sp + hashf (i + 11), 1.0f);
            const float x   = (hashf (i + 29) - 0.5f) * w * 0.9f + std::sin (d.t * 2.0f + (float) i) * 6.0f * cyc;
            const float y   = -h * (0.55f + cyc * (0.7f + 0.6f * e));
            const float a   = (1.0f - cyc) * (0.4f + 0.6f * e);
            const float r   = 1.0f + 1.3f * hashf (i + 41);
            g.setColour (Colour::fromFloatRGBA (1.0f, 0.59f, 0.2f, a * 0.35f));
            g.fillEllipse (x - r * 2.6f, y - r * 2.6f, r * 5.2f, r * 5.2f);
            g.setColour (Colour::fromFloatRGBA (1.0f, 0.89f, 0.59f, a));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }
    }

    // ---- braços (atrás do corpo): caídos de boa → balançando → pra cima socando o ar no tempo ----
    const float lift = clamp01 ((e - 0.22f) / 0.55f);
    for (float s : { -1.0f, 1.0f })
    {
        float ang = 0.45f + 2.15f * lift;                               // 0 = para baixo, π = para cima
        if (on)
        {
            if (e > 0.8f)       ang += 0.40f * std::sin (2.0f * pi * b);                  // soca o ar
            else if (e > 0.22f) ang += 0.55f * lift * hop * (s == side ? 1.0f : 0.25f);   // um braço de cada vez
            else                ang += 0.10f * hop;
        }
        else if (d.drop)
            ang = 0.9f;

        const juce::Point<float> sh (s * w * 0.36f, -h * 0.3f);
        const float L = h * 0.36f;
        const juce::Point<float> hand (sh.x + s * L * std::sin (ang), sh.y + L * std::cos (ang));
        const juce::Point<float> elbow ((sh.x + hand.x) * 0.5f + s * 5.0f, (sh.y + hand.y) * 0.5f + 3.0f);

        g.setColour (outerC);
        g.fillPath (wisp (sh, elbow, hand, 9.0f * grow, 3.0f * grow));
        g.setColour (midC);
        g.fillPath (wisp ({ sh.x * 0.95f, sh.y }, elbow, hand, 4.5f * grow, 1.6f * grow));
        g.setColour (innC);
        g.fillEllipse (hand.x - 3.6f * grow, hand.y - 3.6f * grow, 7.2f * grow, 7.2f * grow);
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.96f, 0.84f, 0.8f));
        g.fillEllipse (hand.x - 0.8f - 1.4f * grow, hand.y - 0.8f - 1.4f * grow, 2.8f * grow, 2.8f * grow);
    }

    // ---- corpo: três camadas de chama (brasa por fora, amarelo por dentro) e o miolo claro ----
    {
        const float lick = 0.45f + 0.55f * e;
        const float fl   = 0.5f + 0.5f * std::sin (d.t * (7.0f + 5.0f * e) + 1.3f);
        const float fr   = 0.5f + 0.5f * std::sin (d.t * (8.5f + 5.0f * e) + 4.1f);
        const float tip  = std::sin (d.t * 6.0f) * 3.0f * grow - tilt * 24.0f;
        const float sway = std::sin (d.t * 4.3f) * 2.5f * grow;

        auto outer = flame (w, h, tip, fl * lick, fr * lick, sway);
        g.setGradientFill (vgrad (mixRgb (outerC, Colour (0xff96140a), 0.35f), -h, outerC, 0.0f));
        g.fillPath (outer);
        g.setColour (Colour::fromFloatRGBA (0.35f, 0.05f, 0.02f, 0.55f));
        g.strokePath (outer, juce::PathStrokeType (1.2f));

        auto mid = flame (w * 0.74f, h * 0.8f, tip * 0.8f, (1.0f - fl) * lick, (1.0f - fr) * lick, -sway * 0.6f);
        mid.applyTransform (juce::AffineTransform::translation (0.0f, -h * 0.02f));
        g.setGradientFill (vgrad (midC, -h * 0.82f, mixRgb (midC, Colour (0xffffc85a), 0.4f), -h * 0.02f));
        g.fillPath (mid);

        auto inner = teardrop (w * 0.5f, h * 0.56f, tip * 0.5f);
        inner.applyTransform (juce::AffineTransform::translation (0.0f, -h * 0.035f));
        g.setGradientFill (vgrad (innC, -h * 0.6f, mixRgb (innC, coreC, 0.6f), -h * 0.035f));
        g.fillPath (inner);

        g.setGradientFill (rgrad ({ 0.0f, -h * 0.13f }, w * 0.24f, coreC.withAlpha (0.85f), coreC.withAlpha (0.0f)));
        g.fillEllipse (-w * 0.24f, -h * 0.26f, w * 0.48f, h * 0.26f);
    }

    // ---- rosto ----
    const float eyeY = -h * 0.34f, eyeDx = w * 0.14f, er = w * 0.07f;
    const float look = -er * 0.35f;   // olha para o knob, à esquerda
    const auto  ink  = Colour (0xff2a0904);

    // olhos
    for (float s : { -1.0f, 1.0f })
    {
        const float ex = s * eyeDx;
        if (d.drop)
        {   // espanto: olhão redondo, pupila pequena
            g.setColour (juce::Colours::white);
            g.fillEllipse (ex - er * 1.2f, eyeY - er * 1.2f, er * 2.4f, er * 2.4f);
            g.setColour (ink);
            g.fillEllipse (ex - er * 0.45f, eyeY - er * 0.45f, er * 0.9f, er * 0.9f);
        }
        else if (e < 0.2f)
        {   // de boa: olho meio fechado (e pisca de vez em quando)
            const bool blink = std::fmod (d.t, 3.7f) < 0.12f;
            g.setColour (ink);
            g.fillEllipse (ex - er, eyeY - er * (blink ? 0.12f : 0.45f), er * 2.0f, er * (blink ? 0.24f : 0.9f));
        }
        else if (e < 0.55f)
        {   // ligado: olho aberto com brilho
            g.setColour (ink);
            g.fillEllipse (ex - er + look, eyeY - er * 1.15f, er * 2.0f, er * 2.3f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (ex - er * 0.53f + look, eyeY - er * 0.8f, er * 0.76f, er * 0.76f);
        }
        else if (e < 0.85f)
        {   // feliz: olho fechado em arco
            juce::Path a;
            a.startNewSubPath (ex - er * 1.1f, eyeY + er * 0.4f);
            a.quadraticTo (ex, eyeY - er * 1.4f, ex + er * 1.1f, eyeY + er * 0.4f);
            g.setColour (ink);
            g.strokePath (a, juce::PathStrokeType (er * 0.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {   // euforia: olhão arregalado com estrela no brilho
            g.setColour (ink);
            g.fillEllipse (ex - er * 1.3f, eyeY - er * 1.4f, er * 2.6f, er * 2.8f);
            star (g, ex - er * 0.3f, eyeY - er * 0.45f, er * (0.75f + 0.25f * land), juce::Colours::white);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillEllipse (ex + er * 0.35f, eyeY + er * 0.35f, er * 0.45f, er * 0.45f);
        }
    }

    // sobrancelhas sobem com a euforia
    if (e > 0.4f && ! d.drop)
    {
        const float up = er * (1.7f + 1.3f * clamp01 ((e - 0.4f) / 0.6f) + 0.6f * land);
        for (float s : { -1.0f, 1.0f })
        {
            juce::Path br;
            br.startNewSubPath (s * eyeDx - er * 1.05f, eyeY - up + s * er * 0.25f);
            br.quadraticTo (s * eyeDx, eyeY - up - er * 0.6f, s * eyeDx + er * 1.05f, eyeY - up - s * er * 0.25f);
            g.setColour (ink);
            g.strokePath (br, juce::PathStrokeType (er * 0.38f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // bochecha corada
    if (e > 0.45f && ! d.drop)
    {
        g.setColour (Colour (0xffff4650).withAlpha (0.4f * clamp01 ((e - 0.45f) / 0.3f)));
        for (float s : { -1.0f, 1.0f })
            g.fillEllipse (s * w * 0.25f - er * 1.05f, eyeY + er * 1.35f, er * 2.1f, er * 1.1f);
    }

    // boca: risquinho → sorriso → boca aberta → gritando no tempo
    {
        const float my = -h * 0.21f;
        g.setColour (ink);
        if (d.drop)
        {
            g.fillEllipse (-er * 0.6f, my - er * 0.5f, er * 1.2f, er * 1.3f);
        }
        else if (e < 0.2f)
        {
            juce::Path m;
            m.startNewSubPath (-w * 0.06f, my);
            m.quadraticTo (0.0f, my + er * 0.4f, w * 0.06f, my);
            g.strokePath (m, juce::PathStrokeType (er * 0.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (e < 0.55f)
        {
            juce::Path m;
            m.startNewSubPath (-w * 0.1f, my - er * 0.2f);
            m.quadraticTo (0.0f, my + er * 1.6f, w * 0.1f, my - er * 0.2f);
            g.strokePath (m, juce::PathStrokeType (er * 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            const float big  = clamp01 ((e - 0.55f) / 0.45f);
            const float open = er * (1.2f + 1.8f * big) * (0.75f + 0.35f * (on ? 1.0f - b : 0.5f));
            const float mw   = w * (0.11f + 0.07f * big);
            juce::Path m;
            m.startNewSubPath (-mw, my - er * 0.2f);
            m.quadraticTo (0.0f, my - er * 0.6f, mw, my - er * 0.2f);
            m.quadraticTo (mw * 0.9f, my + open, 0.0f, my + open);
            m.quadraticTo (-mw * 0.9f, my + open, -mw, my - er * 0.2f);
            m.closeSubPath();
            g.fillPath (m);
            g.setColour (Colour (0xffe8344a));
            g.fillEllipse (-mw * 0.5f, my + open * 0.45f, mw, open * 0.5f);
        }
    }

    // faíscas de alegria a cada tempo quando ele está no talo
    if (on && e > 0.7f)
    {
        const float k = clamp01 ((e - 0.7f) / 0.3f);
        const int n = 3 + juce::roundToInt (4.0f * k);
        for (int i = 0; i < n; ++i)
        {
            const float a  = (hashf (d.beatAbs * 13 + i) - 0.5f) * 2.6f;      // espalhadas para cima
            const float rr = h * (0.55f + 0.55f * b) * (0.8f + 0.4f * hashf (d.beatAbs * 7 + i));
            const float x  = std::sin (a) * rr, y = -h * 0.55f - std::cos (a) * rr;
            star (g, x, y, (2.5f + 3.5f * k) * (1.0f - b * 0.6f),
                  (i % 2 == 0 ? Colour (0xfff4e9d4) : Colour (0xffffb347)).withAlpha (k * (1.0f - b)));
        }
    }
}

// contador do compasso: 4 pontos, acende o tempo atual (o 1 em coral)
inline void drawBeatDots (juce::Graphics& g, float cx, float y, const DanceState& d)
{
    const bool on = d.dancing && ! d.drop;
    const int beat = juce::jlimit (0, 3, (int) std::floor (d.beatPos));
    const float b = d.beatPos - std::floor (d.beatPos);
    for (int k = 0; k < 4; ++k)
    {
        const float x = cx + ((float) k - 1.5f) * 16.0f;
        const bool lit = on && k == beat;
        const auto c = k == 0 ? theme::coral : theme::amber;
        if (lit)
        {
            g.setColour (c.withAlpha (0.35f * (1.0f - b)));
            g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
        }
        const float r = lit ? 4.5f : 3.2f;
        g.setColour (lit ? c : theme::segOff);
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }
}
} // namespace chama
