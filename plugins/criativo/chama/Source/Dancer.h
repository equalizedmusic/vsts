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

inline void fireFill (juce::Graphics& g, juce::Point<float> core, float radius, float bright)
{
    juce::ColourGradient cg (juce::Colour::fromFloatRGBA (1.0f, 0.95f, 0.78f, 1.0f), core.x, core.y,
                             juce::Colour::fromFloatRGBA (0.85f, 0.16f, 0.04f, 1.0f), core.x, core.y + radius, true);
    cg.addColour (0.35, juce::Colour::fromFloatRGBA (1.0f, 0.80f, 0.30f + 0.15f * bright, 1.0f));
    cg.addColour (0.70, juce::Colour::fromFloatRGBA (1.0f, 0.45f, 0.08f, 1.0f));
    g.setGradientFill (cg);
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
    const float pi   = juce::MathConstants<float>::pi;
    const bool  on   = d.dancing && ! d.drop;
    const float e    = d.drop ? 0.0f : clamp01 (d.euph);
    const int   beat = juce::jlimit (0, 3, (int) std::floor (d.beatPos));
    const float b    = on ? d.beatPos - std::floor (d.beatPos) : 0.0f;   // 0 = em cima do tempo
    const float accent = beat == 0 ? 1.25f : 1.0f;                      // o 1 do compasso pesa mais

    // ---- movimento no tempo ----
    const float jumpAmt = on ? std::pow (clamp01 ((e - 0.35f) / 0.65f), 1.3f) : 0.0f;
    const float hop     = on ? std::sin (pi * b) : 0.0f;                    // sobe entre um tempo e outro
    const float jump    = 44.0f * jumpAmt * accent * hop;
    const float land    = on ? std::exp (-b * 9.0f) : 0.0f;                 // achata ao cair no tempo
    const float nod     = on ? (0.35f + 0.65f * clamp01 (e * 2.0f)) * std::exp (-b * 6.0f) : 0.0f;
    const float breathe = 0.5f + 0.5f * std::sin (d.t * 2.1f);
    const float side    = (beat % 2 == 0) ? 1.0f : -1.0f;                   // balança de um lado para o outro
    const float tilt    = on ? side * (0.03f + 0.11f * e) * std::sin (pi * b) : 0.02f * std::sin (d.t * 0.9f);

    float sx = 1.0f + (0.07f + 0.15f * e) * land - 0.06f * hop * jumpAmt;
    float sy = 1.0f - (0.06f + 0.14f * e) * land + 0.09f * hop * jumpAmt + 0.025f * breathe - 0.05f * nod;
    if (d.drop) { sx = 1.14f; sy = 0.80f; }

    const float w = 66.0f * sx, h = 92.0f * sy;
    const float footY = floorY - jump;

    // ---- sombra no chão: encolhe quando ele pula ----
    {
        const float k = 1.0f - 0.45f * clamp01 (jump / 55.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f * k));
        g.fillEllipse (cx - w * 0.5f * k, floorY - 4.0f, w * k, 8.0f);
        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.45f, 0.1f, 0.12f + 0.2f * e));
        g.fillEllipse (cx - w * 0.8f, floorY - 7.0f, w * 1.6f, 14.0f);
    }

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (tilt).translated (cx, footY - 6.0f));

    const auto dark = juce::Colour (0xff3a0d06);

    // ---- halo: brilha mais quando ele está eufórico ----
    {
        const float r = h * (0.75f + 0.35f * e + 0.15f * land * e);
        juce::ColourGradient halo (juce::Colour::fromFloatRGBA (1.0f, 0.55f, 0.15f, 0.10f + 0.30f * e), 0.0f, -h * 0.45f,
                                   juce::Colour::fromFloatRGBA (1.0f, 0.40f, 0.10f, 0.0f), 0.0f, -h * 0.45f + r, true);
        g.setGradientFill (halo);
        g.fillEllipse (-r, -h * 0.45f - r, r * 2.0f, r * 2.0f);
    }

    // ---- pés: pendurados no ar quando ele pula ----
    {
        const float hang = 5.0f * clamp01 (jump / 40.0f);
        for (float s : { -1.0f, 1.0f })
        {
            g.setColour (dark);
            g.fillEllipse (s * w * 0.2f - 9.0f, -2.0f + hang * (s > 0 ? 1.0f : 0.6f), 18.0f, 9.0f);
            g.setColour (juce::Colour (0xffb8300c));
            g.fillEllipse (s * w * 0.2f - 7.5f, -1.0f + hang * (s > 0 ? 1.0f : 0.6f), 15.0f, 6.0f);
        }
    }

    // ---- braços: caídos de boa → balançando → pra cima socando o ar no tempo ----
    const float lift = clamp01 ((e - 0.22f) / 0.55f);
    for (float s : { -1.0f, 1.0f })
    {
        float ang = 0.35f + 2.25f * lift;                               // 0 = para baixo, π = para cima
        if (on)
        {
            if (e > 0.8f)       ang += 0.40f * std::sin (2.0f * pi * b);                  // soca o ar
            else if (e > 0.22f) ang += 0.55f * lift * hop * (s == side ? 1.0f : 0.25f);   // um braço de cada vez
            else                ang += 0.10f * hop;
        }
        else if (d.drop)
            ang = 0.9f;

        const juce::Point<float> sh (s * w * 0.42f, -h * 0.36f);
        const float L = h * 0.42f;
        const juce::Point<float> hand (sh.x + s * L * std::sin (ang), sh.y + L * std::cos (ang));
        const juce::Point<float> elbow ((sh.x + hand.x) * 0.5f + s * 6.0f, (sh.y + hand.y) * 0.5f + 4.0f);

        juce::Path arm;
        arm.startNewSubPath (sh);
        arm.quadraticTo (elbow, hand);
        g.setColour (dark);
        g.strokePath (arm, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffff7a1e));
        g.strokePath (arm, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (dark);
        g.fillEllipse (hand.x - 6.5f, hand.y - 6.5f, 13.0f, 13.0f);
        g.setColour (juce::Colour (0xffffb347));
        g.fillEllipse (hand.x - 4.5f, hand.y - 4.5f, 9.0f, 9.0f);
    }

    // ---- cabelo de fogo: três pontas que crescem com a euforia ----
    for (int i = 0; i < 3; ++i)
    {
        const float s   = (float) (i - 1);
        const float fl  = 0.75f + 0.25f * std::sin (d.t * (8.0f + 2.0f * (float) i) + (float) i * 2.1f);
        const float hh  = h * (0.20f + 0.32f * e + 0.10f * land * e) * fl * (i == 1 ? 1.25f : 1.0f);
        const float sway = std::sin (d.t * 5.0f + (float) i) * 4.0f - tilt * 30.0f;
        juce::Graphics::ScopedSaveState hs (g);
        g.addTransform (juce::AffineTransform::rotation (s * 0.45f).translated (s * w * 0.2f, -h * 0.78f));
        auto tip = teardrop (w * 0.34f, hh + w * 0.17f, sway);
        fireFill (g, { 0.0f, -w * 0.08f }, hh, e);
        g.fillPath (tip);
    }

    // ---- corpo: gota de fogo com miolo claro ----
    {
        auto bodyPath = teardrop (w, h, std::sin (d.t * 6.0f) * 3.0f - tilt * 20.0f);
        g.setColour (dark);
        g.strokePath (bodyPath, juce::PathStrokeType (3.0f));
        fireFill (g, { 0.0f, -h * 0.30f }, h * 0.62f, e);
        g.fillPath (bodyPath);

        auto core = teardrop (w * 0.52f, h * 0.48f, 0.0f);
        core.applyTransform (juce::AffineTransform::translation (0.0f, -h * 0.07f));
        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.93f, 0.62f, 0.35f + 0.35f * e));
        g.fillPath (core);
    }

    // ---- rosto ----
    const float eyeY = -h * 0.50f, eyeDx = w * 0.17f, er = w * 0.085f;
    const float look = -er * 0.35f;   // olha para o knob, à esquerda
    const auto  ink  = juce::Colour (0xff1a0703);

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
            g.fillEllipse (ex - er * 0.35f + look, eyeY - er * 0.75f, er * 0.7f, er * 0.7f);
        }
        else if (e < 0.85f)
        {   // feliz: olho fechado em "^"
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
        const float up = er * (1.6f + 1.4f * clamp01 ((e - 0.4f) / 0.6f) + 0.6f * land);
        for (float s : { -1.0f, 1.0f })
        {
            juce::Path br;
            br.startNewSubPath (s * eyeDx - er * 1.1f, eyeY - up + s * er * 0.25f);
            br.quadraticTo (s * eyeDx, eyeY - up - er * 0.6f, s * eyeDx + er * 1.1f, eyeY - up - s * er * 0.25f);
            g.setColour (ink);
            g.strokePath (br, juce::PathStrokeType (er * 0.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // bochecha corada
    if (e > 0.45f && ! d.drop)
    {
        g.setColour (juce::Colour (0xffff3b5c).withAlpha (0.35f * clamp01 ((e - 0.45f) / 0.3f)));
        for (float s : { -1.0f, 1.0f })
            g.fillEllipse (s * w * 0.3f - er * 1.1f, eyeY + er * 1.4f, er * 2.2f, er * 1.2f);
    }

    // boca: risquinho → sorriso → boca aberta → gritando no tempo
    {
        const float my = -h * 0.30f;
        g.setColour (ink);
        if (d.drop)
        {
            g.fillEllipse (-er * 0.6f, my - er * 0.5f, er * 1.2f, er * 1.3f);
        }
        else if (e < 0.2f)
        {
            juce::Path m;
            m.startNewSubPath (-w * 0.07f, my);
            m.quadraticTo (0.0f, my + er * 0.4f, w * 0.07f, my);
            g.strokePath (m, juce::PathStrokeType (er * 0.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (e < 0.55f)
        {
            juce::Path m;
            m.startNewSubPath (-w * 0.12f, my - er * 0.2f);
            m.quadraticTo (0.0f, my + er * 1.6f, w * 0.12f, my - er * 0.2f);
            g.strokePath (m, juce::PathStrokeType (er * 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            const float big  = clamp01 ((e - 0.55f) / 0.45f);
            const float open = er * (1.2f + 1.8f * big) * (0.75f + 0.35f * (on ? 1.0f - b : 0.5f));
            const float mw   = w * (0.13f + 0.08f * big);
            juce::Path m;
            m.startNewSubPath (-mw, my - er * 0.2f);
            m.quadraticTo (0.0f, my - er * 0.6f, mw, my - er * 0.2f);
            m.quadraticTo (mw * 0.9f, my + open, 0.0f, my + open);
            m.quadraticTo (-mw * 0.9f, my + open, -mw, my - er * 0.2f);
            m.closeSubPath();
            g.fillPath (m);
            g.setColour (juce::Colour (0xffe8344a));
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
                  (i % 2 == 0 ? juce::Colour (0xfff4e9d4) : juce::Colour (0xffffb347)).withAlpha (k * (1.0f - b)));
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
