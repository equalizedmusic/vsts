#pragma once
// MESA — peças desenhadas (knob, tecla, LED de 7 segmentos, medidor de LED, VU, fader). Vetor puro, no padrão visual
// dos mixers de plugin de console: chapa fosca escura, knob de saia serrilhada com tampa colorida em domo e ponteiro
// branco, teclas com bisel e acesas com brilho. A referência é o protótipo em mockup/mesa-v4 (mesmas medidas e cores).
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

namespace mesa::des
{
using juce::Colour;
using juce::Graphics;
constexpr float pi = juce::MathConstants<float>::pi;

inline Colour hex (juce::uint32 rgb, float a = 1.0f) { return Colour (0xff000000u | rgb).withAlpha (a); }
inline Colour mix (Colour a, Colour b, float t) { return a.interpolatedWith (b, t); }
inline Colour claro (Colour c, float t) { return mix (c, Colour (0xffffffffu), t); }
inline Colour escuro (Colour c, float t) { return mix (c, Colour (0xff000000u), t); }

// paleta
const Colour tampaVermelha = hex (0xd4372c), tampaVerde = hex (0x2f9b4f), tampaAzul = hex (0x2c6fd0), tampaPreta = hex (0x2a2b2e),
             tampaCinza = hex (0x8d9298), tampaBranca = hex (0xe4e6e8), tampaLaranja = hex (0xe2742b), tampaAzulBus = hex (0x2f86e0);
const Colour acLaranja = hex (0xff9a3c), acVerdeAgua = hex (0x5fd6ac), acAzul = hex (0x36b4ff), acAmarelo = hex (0xffc93a),
             acVermelho = hex (0xff4a3a), acSelecao = hex (0x2aa8ef);

// ------------------------------------------------------------------ texto
inline juce::Font fonte (float px, bool negrito = true, bool mono = false)
{
   #if JUCE_MAC
    const char* mononame = "Menlo";
   #else
    const char* mononame = "Consolas";
   #endif
    return juce::Font (juce::FontOptions (mono ? mononame : "Arial", px, negrito ? juce::Font::bold : juce::Font::plain).withPointHeight (px));
}
enum class Al { esq, meio, dir };
inline void txt (Graphics& g, const juce::String& s, float x, float y, float px, Colour c, Al al = Al::meio, bool negrito = true, bool mono = false)
{
    g.setFont (fonte (px, negrito, mono));
    g.setColour (c);
    const float w = 600.0f, h = px * 2.0f;
    juce::Rectangle<float> r = al == Al::esq ? juce::Rectangle<float> (x, y - h / 2, w, h)
                             : al == Al::dir ? juce::Rectangle<float> (x - w, y - h / 2, w, h)
                                             : juce::Rectangle<float> (x - w / 2, y - h / 2, w, h);
    g.drawText (s, r, al == Al::esq ? juce::Justification::centredLeft : al == Al::dir ? juce::Justification::centredRight
                                                                                       : juce::Justification::centred, false);
}

// ------------------------------------------------------------------ utilidades
inline void vgrad (Graphics& g, juce::Rectangle<float> r, Colour a, Colour b)
{
    g.setGradientFill (juce::ColourGradient (a, r.getX(), r.getY(), b, r.getX(), r.getBottom(), false));
    g.fillRect (r);
}
inline void linha (Graphics& g, float x0, float y0, float x1, float y1, Colour c, float w = 1.0f)
{
    g.setColour (c);
    g.drawLine (x0, y0, x1, y1, w);
}
// brilho em volta (o canvas faz com shadowBlur): manchas concêntricas cada vez mais fracas
inline void brilho (Graphics& g, juce::Rectangle<float> r, Colour c, float raio, float canto = 2.0f)
{
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (c.withMultipliedAlpha (0.10f));
        g.fillRoundedRectangle (r.expanded (raio * (float) i / 4.0f), canto + raio * (float) i / 4.0f);
    }
}

// chapa do canal: tinta fosca com leve degradê, filete claro à esquerda e sombra à direita
inline void chapa (Graphics& g, juce::Rectangle<float> r, Colour base)
{
    vgrad (g, r, claro (base, 0.05f), escuro (base, 0.12f));
    juce::ColourGradient h (Colour (0x0dffffffu), r.getX(), 0, Colour (0x2e000000u), r.getRight(), 0, false);
    h.addColour (0.5, Colour (0x00ffffffu));
    g.setGradientFill (h);
    g.fillRect (r);
    g.setColour (Colour (0x1affffffu)); g.fillRect (r.getX(), r.getY(), 1.0f, r.getHeight());
    g.setColour (Colour (0xa6000000u)); g.fillRect (r.getRight() - 1.0f, r.getY(), 1.0f, r.getHeight());
}
// divisão entre seções: sulco
inline void sulco (Graphics& g, float x, float y, float w)
{
    g.setColour (Colour (0xb3000000u)); g.fillRect (x, y, w, 1.0f);
    g.setColour (Colour (0x12ffffffu)); g.fillRect (x, y + 1.0f, w, 1.0f);
}

// ------------------------------------------------------------------ knob
// v 0..1 (curso de 300°); claro = knob claro (chapa cinza)
// o corpo (tudo menos o ponteiro), em volta de (cx, cy)
inline void knobCorpo (Graphics& g, float cx, float cy, float r, Colour tampa, bool clara, bool marcas)
{
    if (marcas)
        for (int i = 0; i <= 10; ++i)
        {
            const float a = (-150.0f + (float) i * 30.0f) * pi / 180.0f, r0 = r + 2.2f, r1 = r + (i % 5 == 0 ? 5.0f : 4.0f);
            linha (g, cx + std::sin (a) * r0, cy - std::cos (a) * r0, cx + std::sin (a) * r1, cy - std::cos (a) * r1,
                   clara ? Colour (0xbf141416u) : Colour (0x8ccdd2d8u), i % 5 == 0 ? 1.1f : 0.8f);
        }
    // sombra no chão
    {
        const float sx = cx + r * 0.1f, sy = cy + r * 0.28f, sr = r * 1.18f;
        juce::ColourGradient sg (Colour (0x8c000000u), sx, sy, Colour (0x00000000u), sx + sr, sy, true);
        sg.addColour (0.7, Colour (0x40000000u));
        g.setGradientFill (sg);
        g.fillEllipse (sx - sr, sy - sr, sr * 2, sr * 2);
    }
    // saia
    {
        juce::ColourGradient sk (clara ? hex (0xf1f2f3) : hex (0x56595e), cx, cy - r, clara ? hex (0x7d8186) : hex (0x0d0e0f), cx, cy + r, false);
        sk.addColour (clara ? 0.55 : 0.45, clara ? hex (0xc2c5c9) : hex (0x2a2c2f));
        g.setGradientFill (sk);
        g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    }
    // serrilhado
    for (int i = 0; i < 40; ++i)
    {
        const float a = (float) i / 40.0f * 2.0f * pi, lit = std::cos (a - 2.3f) * 0.5f + 0.5f;
        const Colour c = clara ? Colour (0xff000000u).withAlpha (0.10f + 0.12f * (1.0f - lit)) : Colour (0xffffffffu).withAlpha (0.03f + 0.09f * lit);
        linha (g, cx + std::cos (a) * r * 0.8f, cy + std::sin (a) * r * 0.8f, cx + std::cos (a) * r * 0.97f, cy + std::sin (a) * r * 0.97f,
               c, juce::jmax (0.6f, r * 0.07f));
    }
    g.setColour (clara ? Colour (0xcc282828u) : hex (0x050505));
    g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.0f);
    // degrau entre saia e tampa
    {
        const float rr = r * 0.74f;
        g.setGradientFill (juce::ColourGradient (clara ? hex (0xdfe1e4) : hex (0x3a3c40), cx, cy - r, clara ? hex (0x9b9ea3) : hex (0x161718), cx, cy + r, false));
        g.fillEllipse (cx - rr, cy - rr, rr * 2, rr * 2);
    }
    // tampa em domo (luz de cima à esquerda)
    {
        const float cr = r * 0.62f, hx = cx - cr * 0.35f, hy = cy - cr * 0.45f;
        juce::ColourGradient tg (claro (tampa, 0.38f), hx, hy, escuro (tampa, 0.45f), hx + cr * 1.55f, hy, true);
        tg.addColour (0.42, tampa);
        g.setGradientFill (tg);
        g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
        g.setColour (Colour (0x8c000000u));
        g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 0.8f);
        // reflexo
        juce::Path e;
        e.addEllipse (-cr * 0.55f, -cr * 0.32f, cr * 1.1f, cr * 0.64f);
        e.applyTransform (juce::AffineTransform::rotation (-0.5f).translated (cx - cr * 0.22f, cy - cr * 0.38f));
        g.setGradientFill (juce::ColourGradient (Colour (0x52ffffffu), cx - cr * 0.22f, cy - cr * 0.38f, Colour (0x00ffffffu), cx - cr * 0.22f + cr * 0.55f, cy - cr * 0.38f, true));
        g.fillPath (e);
    }
}

// knob inteiro: o corpo sai de uma imagem guardada (uma por cor/tamanho/escala — a console tem centenas de knobs) e
// só o ponteiro é desenhado na hora
inline void knob (Graphics& g, float cx, float cy, float r, Colour tampa, float v, bool clara = false, bool marcas = true)
{
    const float esc = juce::jlimit (0.5f, 8.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const float meia = r * 1.5f + 6.0f;
    const int lado = juce::jmax (4, (int) std::ceil (meia * 2.0f * esc));
    struct Chave
    {
        juce::uint32 cor; int r10, e100; bool clara, marcas;
        bool operator< (const Chave& o) const
        { return std::tie (cor, r10, e100, clara, marcas) < std::tie (o.cor, o.r10, o.e100, o.clara, o.marcas); }
    };
    static std::map<Chave, juce::Image> cache;
    const Chave k { tampa.getARGB(), juce::roundToInt (r * 10.0f), juce::roundToInt (esc * 100.0f), clara, marcas };
    auto it = cache.find (k);
    if (it == cache.end())
    {
        if (cache.size() > 400) cache.clear();
        juce::Image img (juce::Image::ARGB, lado, lado, true);
        {
            Graphics gi (img);
            gi.addTransform (juce::AffineTransform::scale (esc));
            knobCorpo (gi, meia, meia, r, tampa, clara, marcas);
        }
        it = cache.emplace (k, img).first;
    }
    {
        Graphics::ScopedSaveState ss (g);
        g.drawImageTransformed (it->second, juce::AffineTransform::scale (1.0f / esc).translated (cx - meia, cy - meia));
    }
    // ponteiro
    const float ang = (-150.0f + juce::jlimit (0.0f, 1.0f, v) * 300.0f) * pi / 180.0f;
    const float sx = std::sin (ang), sy = -std::cos (ang);
    juce::Path p;
    p.startNewSubPath (cx + sx * r * 0.18f, cy + sy * r * 0.18f);
    p.lineTo (cx + sx * r * 0.93f, cy + sy * r * 0.93f);
    g.setColour (clara ? Colour (0x59ffffffu) : Colour (0x99000000u));
    g.strokePath (p, juce::PathStrokeType (juce::jmax (1.6f, r * 0.2f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (clara ? hex (0x111111) : hex (0xf4f4f2));
    g.strokePath (p, juce::PathStrokeType (juce::jmax (1.0f, r * 0.11f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ------------------------------------------------------------------ tecla
inline void tecla (Graphics& g, juce::Rectangle<float> r, const juce::String& rot, bool acesa, Colour cor = acLaranja, bool clara = false, bool tingida = false)
{
    g.setColour (Colour (0x8c000000u));
    g.fillRoundedRectangle (r.translated (0.5f, 1.5f), 2.0f);
    if (acesa)
    {
        brilho (g, r, cor, 4.0f);
        juce::ColourGradient k (claro (cor, 0.45f), 0, r.getY(), escuro (cor, 0.25f), 0, r.getBottom(), false);
        k.addColour (0.5, cor);
        g.setGradientFill (k);
    }
    else
    {
        juce::ColourGradient k (clara ? hex (0xeceef0) : hex (0x5d6065), 0, r.getY(), clara ? hex (0xa9acb0) : hex (0x2c2e31), 0, r.getBottom(), false);
        if (! clara) k.addColour (0.5, hex (0x44474b));
        g.setGradientFill (k);
    }
    g.fillRoundedRectangle (r, 2.0f);
    if (! acesa && tingida && ! clara) { g.setColour (cor.withAlpha (0.16f)); g.fillRoundedRectangle (r.reduced (2.0f), 1.0f); }
    g.setColour (hex (0x060606));
    g.drawRoundedRectangle (r, 2.0f, 0.8f);
    g.setColour (acesa ? Colour (0x8cffffffu) : Colour (0x24ffffffu));
    g.fillRect (r.getX() + 1.5f, r.getY() + 1.0f, r.getWidth() - 3.0f, 1.0f);
    if (rot.isNotEmpty())
        txt (g, rot, r.getCentreX(), r.getCentreY() + 0.5f, juce::jmin (7.2f, r.getHeight() * 0.58f),
             acesa ? escuro (cor, 0.78f) : (clara ? hex (0x26282b) : hex (0xd9dce0)));
}

// ------------------------------------------------------------------ LED de 7 segmentos
inline void led7 (Graphics& g, float x, float y, float h, const juce::String& s, Colour cor)
{
    static const char* seg[] = { "abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg", "abcdfg" };
    const float w = h * 0.52f, t = h * 0.13f, sk = h * 0.12f;
    for (int i = 0; i < s.length(); ++i)
    {
        const auto ch = s[i];
        const char* on = ch >= '0' && ch <= '9' ? seg[ch - '0'] : (ch == '-' ? "g" : "");
        const float ox = x + (float) i * (w + h * 0.22f);
        auto P = [&] (float px, float py) { return juce::Point<float> (ox + px + (1.0f - py / h) * sk, y + py); };
        struct S { char k; float x0, y0, x1, y1; };
        const S ss[] = { { 'a', t, 0, w - t, 0 }, { 'b', w, t, w, h / 2 - t / 2 }, { 'c', w, h / 2 + t / 2, w, h - t }, { 'd', t, h, w - t, h },
                         { 'e', 0, h / 2 + t / 2, 0, h - t }, { 'f', 0, t, 0, h / 2 - t / 2 }, { 'g', t, h / 2, w - t, h / 2 } };
        for (auto& q : ss)
        {
            const bool lit = std::strchr (on, q.k) != nullptr;
            juce::Path p; p.startNewSubPath (P (q.x0, q.y0)); p.lineTo (P (q.x1, q.y1));
            if (lit)
            {
                g.setColour (cor.withAlpha (0.18f));
                g.strokePath (p, juce::PathStrokeType (t * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (cor);
                g.strokePath (p, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (claro (cor, 0.35f));
                g.strokePath (p, juce::PathStrokeType (t * 0.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
            {
                g.setColour (cor.withAlpha (0.07f));
                g.strokePath (p, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
    }
}

// ------------------------------------------------------------------ medidor de LED (vertical, -60..+6 dB)
inline void barraLed (Graphics& g, float x, float y, float w, float h, float db, int n = 24)
{
    g.setColour (hex (0x050505));
    g.fillRect (x - 1, y - 1, w + 2, h + 2);
    const float step = h / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float segDb = -60.0f + (float) (i + 1) / (float) n * 66.0f;
        const float yy = y + h - (float) (i + 1) * step + 0.6f;
        const Colour c = segDb > 0 ? hex (0xff3b2e) : segDb > -9 ? hex (0xffc93a) : hex (0x38d66b);
        const bool lit = db >= segDb - 66.0f / (float) n;
        g.setColour (lit ? c : c.withAlpha (0.10f));
        g.fillRect (x, yy, w, step - 1.1f);
        if (lit) { g.setColour (Colour (0x59ffffffu)); g.fillRect (x, yy, w, 1.0f); }
    }
}

// ------------------------------------------------------------------ VU de agulha
// v 0..1 (posição da agulha); creme = mostrador creme (canal) ou azulado (barramento)
// gr = escala de redução de ganho (0 à direita, a agulha desce para a esquerda); senão escala de VU (-20 .. +3)
inline void vuCaixa (Graphics& g, juce::Rectangle<float> r, const juce::String& rot, bool sel, bool creme, bool gr = false)
{
    g.setGradientFill (juce::ColourGradient (hex (0x2b2d31), 0, r.getY(), hex (0x141517), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (sel ? acAzul : hex (0x060606));
    g.drawRoundedRectangle (r, 3.0f, sel ? 2.0f : 1.0f);
    const auto f = juce::Rectangle<float> (r.getX() + 5, r.getY() + 5, r.getWidth() - 10, r.getHeight() - 22);
    juce::ColourGradient fg (creme ? hex (0xfff6d8) : hex (0xdbe6ff), f.getCentreX(), f.getY() + f.getHeight() * 1.25f,
                             creme ? hex (0xa88a3e) : hex (0x3b4c78), f.getCentreX() + f.getWidth() * 0.9f, f.getY() + f.getHeight() * 1.25f, true);
    fg.addColour (0.55, creme ? hex (0xf0d995) : hex (0x8fa6d8));
    g.setGradientFill (fg);
    g.fillRoundedRectangle (f, 2.0f);
    const float px = f.getCentreX(), py = f.getY() + f.getHeight() * 1.32f, R = f.getHeight() * 1.05f;
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (f.toNearestInt());
        juce::Path arco;
        arco.addCentredArc (px, py, R, R, 0, -0.62f, 0.62f, true);
        g.setColour (Colour (0xd90f121cu));
        g.strokePath (arco, juce::PathStrokeType (1.0f));
        if (! gr)
        {
            juce::Path verm;
            verm.addCentredArc (px, py, R + 1.5f, R + 1.5f, 0, 0.62f * 0.8f, 0.62f, true);
            g.setColour (hex (0xc4271c));
            g.strokePath (verm, juce::PathStrokeType (2.2f));
        }
        static const float pv[] = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 0.9f, 1.0f }, pg[] = { 0.0f, 0.35f, 0.6f, 0.8f, 1.0f };
        static const char* nv[] = { "20", "10", "7", "5", "3", "0", "3" };
        static const char* ng[] = { "20", "10", "6", "3", "0" };
        const int n = gr ? 5 : 7;
        for (int i = 0; i < n; ++i)
        {
            const float pos = gr ? pg[i] : pv[i];
            const float a = -0.62f + pos * 1.24f;
            const bool red = ! gr && pos > 0.85f;
            const Colour c = red ? hex (0xc4271c) : Colour (0xe60f121cu);
            linha (g, px + std::sin (a) * R, py - std::cos (a) * R, px + std::sin (a) * (R + 7), py - std::cos (a) * (R + 7), c, 1.0f);
            txt (g, gr ? ng[i] : nv[i], px + std::sin (a) * (R + 14), py - std::cos (a) * (R + 14), 6.5f, c);
        }
        for (int i = 0; i < 20; ++i)   // marcas pequenas
        {
            const float a = -0.62f + ((float) i + 0.5f) / 20.0f * 1.24f;
            linha (g, px + std::sin (a) * R, py - std::cos (a) * R, px + std::sin (a) * (R + 3.5f), py - std::cos (a) * (R + 3.5f), Colour (0xa60f121cu), 0.8f);
        }
        txt (g, gr ? "GR dB" : "VU", px, f.getY() + f.getHeight() * 0.62f, 6.5f, Colour (0x990f121cu));
    }
    g.setColour (Colour (0xcc000000u));
    g.drawRoundedRectangle (f, 2.0f, 1.0f);
    if (rot.isNotEmpty()) txt (g, rot, r.getCentreX(), r.getBottom() - 8.5f, 8.0f, sel ? hex (0x7fd0ff) : hex (0xd4d7db));
}
inline void vuAgulha (Graphics& g, juce::Rectangle<float> r, float v)
{
    const auto f = juce::Rectangle<float> (r.getX() + 5, r.getY() + 5, r.getWidth() - 10, r.getHeight() - 22);
    const float px = f.getCentreX(), py = f.getY() + f.getHeight() * 1.32f, R = f.getHeight() * 1.05f;
    const float a = -0.62f + juce::jlimit (-0.05f, 1.05f, v) * 1.24f;
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (f.toNearestInt());
    linha (g, px + 2, py + 2, px + 2 + std::sin (a) * (R + 8), py + 2 - std::cos (a) * (R + 8), Colour (0x40000000u), 1.4f);
    linha (g, px, py, px + std::sin (a) * (R + 8), py - std::cos (a) * (R + 8), hex (0x111111), 1.2f);
    g.setGradientFill (juce::ColourGradient (Colour (0x38ffffffu), 0, f.getY(), Colour (0x00ffffffu), 0, f.getY() + f.getHeight() * 0.45f, false));
    g.fillRect (f.withHeight (f.getHeight() * 0.45f));
}
// dB -> posição da agulha (escala de VU: -20 .. +3)
inline float vuPos (float db)
{
    static const float d[] = { -20, -10, -7, -5, -3, 0, 3 }, p[] = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 0.9f, 1.0f };
    if (db <= d[0]) return juce::jmax (-0.05f, (db - d[0]) * 0.01f);
    for (int i = 0; i < 6; ++i)
        if (db <= d[i + 1]) return juce::jmap (db, d[i], d[i + 1], p[i], p[i + 1]);
    return juce::jmin (1.05f, 1.0f + (db - 3.0f) * 0.02f);
}

// ------------------------------------------------------------------ fader
inline float faderPos (float db) { return std::pow (juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 90.0f), 2.2f); }   // = skew do parâmetro
inline void faderFenda (Graphics& g, float x, float y, float h)
{
    g.setColour (hex (0x050505));
    g.fillRoundedRectangle (x - 2.5f, y, 5.0f, h, 2.5f);
    g.setColour (Colour (0x12ffffffu));
    g.fillRect (x - 0.5f, y + 3, 1.0f, h - 6);
}
inline void faderCapa (Graphics& g, float x, float y, float h, float v, Colour capa = hex (0xe9ebee))
{
    const float cy = y + (1.0f - v) * h, cw = 22, ch = 34;
    g.setColour (Colour (0x8c000000u));
    g.fillRoundedRectangle (x - cw / 2 + 1.5f, cy - ch / 2 + 4, cw, ch, 3.0f);
    juce::ColourGradient k (claro (capa, 0.4f), 0, cy - ch / 2, escuro (capa, 0.3f), 0, cy + ch / 2, false);
    k.addColour (0.42, capa); k.addColour (0.47, escuro (capa, 0.45f)); k.addColour (0.53, escuro (capa, 0.45f)); k.addColour (0.58, capa);
    g.setGradientFill (k);
    g.fillRoundedRectangle (x - cw / 2, cy - ch / 2, cw, ch, 3.0f);
    g.setColour (hex (0x0a0a0a));
    g.drawRoundedRectangle (x - cw / 2, cy - ch / 2, cw, ch, 3.0f, 1.0f);
    for (int i = -3; i <= 3; ++i)
        if (i != 0) linha (g, x - cw / 2 + 3, cy + (float) i * 3.4f, x + cw / 2 - 3, cy + (float) i * 3.4f, Colour (0x2e000000u), 1.0f);
    linha (g, x - cw / 2 + 2, cy, x + cw / 2 - 2, cy, hex (0x111111), 1.5f);
}
inline void faderEscala (Graphics& g, float x, float y, float h, bool clara)
{
    const float dbs[] = { 10, 5, 0, -5, -10, -20, -30, -40, -60, -80 };
    const char* ss[] = { "10", "5", "0", "5", "10", "20", "30", "40", "60", "" };
    for (int i = 0; i < 10; ++i)
    {
        const float yy = y + (1.0f - faderPos (dbs[i])) * h;
        linha (g, x, yy, x + 4, yy, clara ? hex (0x2a2c2f) : hex (0x9ea3a9), dbs[i] == 0 ? 1.4f : 0.9f);
        txt (g, i == 9 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) : juce::String (ss[i]), x + 11, yy, 6.4f, clara ? hex (0x2a2c2f) : hex (0xb9bdc2));
    }
}
} // namespace mesa::des
