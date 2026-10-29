#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Params.h"
#include "Rack.h"

// Desenho do MONTA-MONTA: painel escuro, cada peça na sua cor, knobs pretos de Minimoog com tampa de alumínio.
// Tudo vetorial, numa tela base de 1200×740 que a janela amplia inteira.
namespace mu
{
using RF = juce::Rectangle<float>;

inline constexpr int   baseW = 1200, baseH = 740;
inline constexpr float minScale = 0.75f, maxScale = 2.0f;

inline const juce::Colour bg0 { 0xff0e0f15 }, bg1 { 0xff1b1d26 }, head { 0xff0b0c11 }, panel { 0xff13151c }, card { 0xff14161e },
                          line { 0xff2a2d38 }, line2 { 0xff2c303d }, ink { 0xffe9ecf5 }, ink2 { 0xffd4d8e6 }, dim { 0xff7d849c },
                          faint { 0xff5d6276 }, dark { 0xff07080b }, gold { 0xffffd23a }, warn { 0xffff4f8b }, ok { 0xff5fe36a };

inline juce::Colour moduleColour (int m)
{
    switch (m)
    {
        case mm::vco1:   return juce::Colour (0xffff8a2a);
        case mm::vco2:   return juce::Colour (0xffffa552);
        case mm::mixer:  return juce::Colour (0xffffd23a);
        case mm::vcf:    return juce::Colour (0xff22d8ff);
        case mm::envAmp: return juce::Colour (0xff5fe36a);
        case mm::output: return juce::Colour (0xffff4f8b);
        default:         return juce::Colour (0xffb07bff);
    }
}

inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline juce::Font font (float size, bool bold = true, bool italic = false)
{
    return juce::Font (juce::FontOptions (size, (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0)));
}

inline void text (juce::Graphics& g, const juce::String& s, RF r, float size, juce::Colour c,
                  juce::Justification j = juce::Justification::centredLeft, bool bold = true, bool italic = false)
{
    g.setColour (c);
    g.setFont (font (size, bold, italic));
    g.drawText (s, r, j, false);
}

// texto numa linha só, diminuindo a letra até caber na largura
inline void fitText (juce::Graphics& g, const juce::String& s, RF r, float size, juce::Colour c,
                     juce::Justification j = juce::Justification::centredLeft, bool bold = true, bool italic = false)
{
    auto f = font (size, bold, italic);
    while (size > 6.0f && juce::GlyphArrangement::getStringWidth (f, s) > r.getWidth())
        f = font (size -= 0.5f, bold, italic);
    g.setColour (c);
    g.setFont (f);
    g.drawText (s, r, j, false);
}

// parágrafo com quebra de linha; devolve a altura usada
inline float para (juce::Graphics* g, const juce::String& s, RF r, float size, juce::Colour c, bool bold = false, bool italic = false,
                   float lineGap = 1.0f)
{
    juce::AttributedString as;
    as.setJustification (juce::Justification::topLeft);
    as.setLineSpacing (lineGap);
    as.setWordWrap (juce::AttributedString::byWord);
    as.append (s, font (size, bold, italic), c);
    juce::TextLayout tl;
    tl.createLayout (as, r.getWidth());
    if (g != nullptr) tl.draw (*g, r.withHeight (juce::jmax (r.getHeight(), tl.getHeight())));
    return tl.getHeight();
}

// ---------------- peça de quebra-cabeça ----------------
// esquerda: recorte (recebe a peça anterior); direita: pino; topo: pino para cima (peças de baixo); base: recorte.
struct Shape { bool leftNotch = false, rightTab = false, topTab = false, bottomNotch = false; };

inline Shape moduleShape (int m)
{
    switch (m)
    {
        case mm::vco1:   return { false, true, false, false };
        case mm::mixer:  return { true, true, false, true };
        case mm::vcf:    return { true, true, false, true };
        case mm::envAmp: return { true, true, false, false };
        case mm::output: return { true, false, false, false };
        default:         return { false, false, true, false };   // VCO 2 e ENV MOD sobem para encaixar
    }
}

// pino (out = +1) ou recorte (out = -1) no meio de uma borda; "mid" é o meio da borda, "dir" o sentido do contorno
// e "nrm" a normal para fora da peça
inline void addTab (juce::Path& p, juce::Point<float> mid, juce::Point<float> dir, juce::Point<float> nrm, float r, float out)
{
    auto at = [&] (float u, float v) { return mid + dir * u + nrm * (v * out); };
    const auto c = at (0.0f, 1.7f * r);
    const float rad = 1.1f * r;
    auto ang = [&] (juce::Point<float> q) { return std::atan2 (q.x - c.x, -(q.y - c.y)); };
    const auto s = at (-0.6f * r, 0.75f * r), e = at (0.6f * r, 0.75f * r), far = at (0.0f, 2.8f * r);
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float a0 = ang (s), af0 = ang (far), a10 = ang (e);
    float a1 = a10, af = af0;
    while (a1 <= a0) a1 += twoPi;
    while (a1 > a0 + twoPi) a1 -= twoPi;
    while (af < a0) af += twoPi;
    while (af >= a0 + twoPi) af -= twoPi;
    if (af > a1) a1 -= twoPi;    // o lado de fora fica no outro sentido
    p.lineTo (at (-0.75f * r, 0.0f));
    const auto c1 = at (-0.75f * r, 0.25f * r), c2 = at (-0.8f * r, 0.55f * r);
    p.cubicTo (c1, c2, s);
    p.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, a1, false);
    const auto d1 = at (0.8f * r, 0.55f * r), d2 = at (0.75f * r, 0.25f * r);
    p.cubicTo (d1, d2, at (0.75f * r, 0.0f));
}

inline juce::Path piecePath (RF b, Shape s, float r = 10.0f)
{
    const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
    using P = juce::Point<float>;
    juce::Path p;
    p.startNewSubPath (x, y);
    if (s.topTab) addTab (p, { x + w * 0.5f, y }, P (1.0f, 0.0f), P (0.0f, -1.0f), r, 1.0f);
    p.lineTo (x + w, y);
    if (s.rightTab) addTab (p, { x + w, y + h * 0.5f }, P (0.0f, 1.0f), P (1.0f, 0.0f), r, 1.0f);
    p.lineTo (x + w, y + h);
    if (s.bottomNotch) addTab (p, { x + w * 0.5f, y + h }, P (-1.0f, 0.0f), P (0.0f, 1.0f), r, -1.0f);
    p.lineTo (x, y + h);
    if (s.leftNotch) addTab (p, { x, y + h * 0.5f }, P (0.0f, -1.0f), P (-1.0f, 0.0f), r, -1.0f);
    p.closeSubPath();
    return p;
}

inline void screw (juce::Graphics& g, float x, float y)
{
    g.setColour (juce::Colour (0xff9ea2b0));
    g.fillEllipse (x - 4.2f, y - 4.2f, 8.4f, 8.4f);
    g.setColour (juce::Colour (0xff2a2c33));
    g.drawEllipse (x - 4.2f, y - 4.2f, 8.4f, 8.4f, 1.0f);
    g.setColour (juce::Colour (0xff3a3c46));
    g.drawLine (x - 2.6f, y - 1.0f, x + 2.6f, y + 1.0f, 1.2f);
}

inline void led (juce::Graphics& g, float x, float y, juce::Colour c, bool on)
{
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.5f), x, y, c.withAlpha (0.0f), x + 9.0f, y, true));
        g.fillEllipse (x - 9.0f, y - 9.0f, 18.0f, 18.0f);
        g.setColour (c);
        g.fillEllipse (x - 3.6f, y - 3.6f, 7.2f, 7.2f);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillEllipse (x - 2.2f, y - 2.4f, 2.4f, 2.4f);
    }
    else
    {
        g.setColour (juce::Colour (0xff2a2c34));
        g.fillEllipse (x - 3.6f, y - 3.6f, 7.2f, 7.2f);
        g.setColour (juce::Colour (0xff111111));
        g.drawEllipse (x - 3.6f, y - 3.6f, 7.2f, 7.2f, 1.0f);
    }
}

inline void lockIcon (juce::Graphics& g, RF r, juce::Colour c)
{
    const float w = r.getWidth();
    RF body (r.getX(), r.getY() + r.getHeight() * 0.45f, w, r.getHeight() * 0.55f);
    juce::Path arc;
    arc.addCentredArc (r.getCentreX(), body.getY(), w * 0.3f, r.getHeight() * 0.38f, 0.0f, -1.5708f, 1.5708f, true);
    g.setColour (c);
    g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.2f, w * 0.14f)));
    g.fillRoundedRectangle (body, w * 0.15f);
}

inline void checkIcon (juce::Graphics& g, RF r, juce::Colour c, float thick = 2.0f)
{
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getY() + r.getHeight() * 0.55f);
    p.lineTo (r.getX() + r.getWidth() * 0.38f, r.getBottom());
    p.lineTo (r.getRight(), r.getY());
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline void playIcon (juce::Graphics& g, RF r, juce::Colour c)
{
    juce::Path p;
    p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
    g.setColour (c);
    g.fillPath (p);
}

inline void glow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.08f * (float) (4 - i)));
        g.strokePath (p, juce::PathStrokeType (width + (float) i * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// knob preto de Minimoog: saia canelada, tampa de alumínio e o risco na cor da peça; escala 0–10 em volta
inline void knob (juce::Graphics& g, float cx, float cy, float r, float pos, juce::Colour c, bool scale)
{
    const float a0 = -2.356f, a1 = 2.356f;
    const float a = a0 + (a1 - a0) * pos;
    if (scale)
        for (int i = 0; i <= 10; ++i)
        {
            const float t = a0 + (a1 - a0) * (float) i / 10.0f - 1.5708f;
            const float r2 = r + (i % 5 == 0 ? 7.5f : 5.5f);
            g.setColour (juce::Colour (0xff9aa0b4));
            g.drawLine (cx + std::cos (t) * (r + 3.0f), cy + std::sin (t) * (r + 3.0f), cx + std::cos (t) * r2, cy + std::sin (t) * r2,
                        i % 5 == 0 ? 1.1f : 0.7f);
        }
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - r, cy - r + 2.5f, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff121318));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
    for (int i = 0; i < 20; ++i)
    {
        const float t = (float) i * 0.31416f + a;
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawLine (cx + std::cos (t) * r * 0.72f, cy + std::sin (t) * r * 0.72f, cx + std::cos (t) * (r - 0.6f), cy + std::sin (t) * (r - 0.6f), 1.1f);
    }
    const float cr = r * 0.52f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f5fa), cx - cr * 0.3f, cy - cr * 0.4f,
                                             juce::Colour (0xff777b8c), cx + cr, cy + cr, true));
    g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
    g.setColour (juce::Colour (0xff3a3a44));
    g.drawEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f, 0.6f);
    const float p = a - 1.5708f;
    g.setColour (c);
    g.drawLine (cx + std::cos (p) * r * 0.25f, cy + std::sin (p) * r * 0.25f, cx + std::cos (p) * (r - 1.0f), cy + std::sin (p) * (r - 1.0f), 2.2f);
}

// ---------------- controles ----------------
class Knob : public juce::Component
{
public:
    Knob (APVTS& st, const juce::String& pid, const juce::String& labelText, juce::Colour c, float radius, bool scaleMarks = true)
        : param (st.getParameter (pid)), label (labelText), colour (c), r (radius), scale (scaleMarks)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (180);
        slider.setAlpha (0.0f);    // o desenho é nosso; o slider só cuida do arraste
        slider.onValueChange = [this] { repaint(); };
        slider.onDragStart = [this] { dragging = true; repaint(); };
        slider.onDragEnd = [this] { dragging = false; repaint(); };
        addAndMakeVisible (slider);
        slider.addMouseListener (this, false);
        att = std::make_unique<APVTS::SliderAttachment> (st, pid, slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    // centro do knob dentro do componente (o nome fica embaixo)
    juce::Point<float> centre() const { return { (float) getWidth() * 0.5f, r + (scale ? 9.0f : 3.0f) }; }

    void resized() override
    {
        const auto c = centre();
        slider.setBounds (juce::Rectangle<int> (juce::roundToInt (c.x - r - 6.0f), juce::roundToInt (c.y - r - 6.0f),
                                                juce::roundToInt (r * 2.0f + 12.0f), juce::roundToInt (r * 2.0f + 12.0f)));
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = centre();
        knob (g, c.x, c.y, r, (float) slider.valueToProportionOfLength (slider.getValue()), colour, scale);
        const float ly = c.y + r + (scale ? 9.0f : 4.0f);
        if (dragging || slider.isMouseOverOrDragging())
            fitText (g, param->getCurrentValueAsText(), { 0.0f, ly, (float) getWidth(), 11.0f }, 8.5f, colour, juce::Justification::centred);
        else
            fitText (g, label, { 0.0f, ly, (float) getWidth(), 11.0f }, 8.5f, juce::Colour (0xffc8ccd8), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    juce::Slider slider;

private:
    juce::RangedAudioParameter* param;
    juce::String label;
    juce::Colour colour;
    float r;
    bool scale;
    bool dragging = false;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// as 6 formas de onda do Minimoog em botõezinhos
class WaveSelector : public juce::Component
{
public:
    WaveSelector (APVTS& st, const juce::String& pid, juce::Colour c) : colour (c)
    {
        auto* p = st.getParameter (pid);
        att = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { value = juce::roundToInt (v); repaint(); });
        att->sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        const float w = (float) getWidth() / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            const RF b ((float) i * w + 0.5f, 0.5f, w - 1.5f, (float) getHeight() - 1.0f);
            const bool on = i == value;
            g.setColour (on ? colour : juce::Colour (0xff15161c));
            g.fillRoundedRectangle (b, 2.0f);
            g.setColour (juce::Colour (0xff3a3e4c));
            g.drawRoundedRectangle (b, 2.0f, 1.0f);
            g.setColour (on ? juce::Colour (0xff111111) : juce::Colour (0xff9aa0b4));
            g.strokePath (wavePath (i, b.reduced (2.5f, 3.0f)), juce::PathStrokeType (1.1f));
        }
    }

    static juce::Path wavePath (int w, RF r)
    {
        juce::Path p;
        const float x = r.getX(), y = r.getY(), W = r.getWidth(), H = r.getHeight(), b = y + H;
        switch (w)
        {
            case 0: p.startNewSubPath (x, b); p.lineTo (x + W * 0.5f, y); p.lineTo (x + W, b); break;
            case 1: p.startNewSubPath (x, b); p.lineTo (x + W * 0.4f, y); p.lineTo (x + W * 0.6f, y + H * 0.5f); p.lineTo (x + W, b); break;
            case 2: p.startNewSubPath (x, b); p.lineTo (x + W, y); p.lineTo (x + W, b); break;
            default:
            {
                const float pw = w == 3 ? 0.5f : (w == 4 ? 0.3f : 0.14f);
                p.startNewSubPath (x, b); p.lineTo (x, y); p.lineTo (x + W * pw, y); p.lineTo (x + W * pw, b); p.lineTo (x + W, b);
                break;
            }
        }
        return p;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit (0, 5, (int) (e.position.x / ((float) getWidth() / 6.0f)));
        att->setValueAsCompleteGesture ((float) i);
    }

private:
    juce::Colour colour;
    int value = 0;
    std::unique_ptr<juce::ParameterAttachment> att;
};

// chave liga/desliga creme do mixer, com o LED do lado
class Rocker : public juce::Button
{
public:
    Rocker (APVTS& st, const juce::String& pid) : juce::Button (pid)
    {
        setClickingTogglesState (true);
        att = std::make_unique<APVTS::ButtonAttachment> (st, pid, *this);
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        g.setColour (on ? juce::Colour (0xffe9e3c8) : juce::Colour (0xff2b2d35));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (b, 3.0f, 1.0f);
        const RF knobR (b.getX() + 2.0f, on ? b.getY() + 2.0f : b.getBottom() - b.getHeight() * 0.42f - 2.0f, b.getWidth() - 4.0f, b.getHeight() * 0.42f);
        g.setColour (on ? juce::Colours::white : juce::Colour (0xff55586a));
        g.fillRoundedRectangle (knobR, 1.5f);
    }

private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// botão de pílula (modo, idioma, menu, ações): aceso = cheio na cor
class Pill : public juce::Button
{
public:
    Pill (const juce::String& t, juce::Colour c) : juce::Button (t), colour (c) {}
    bool lit = false, locked = false, play = false;
    juce::Colour colour;
    float fontSize = 10.0f;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        if (down) b = b.reduced (0.5f);
        g.setColour (lit ? colour : juce::Colour (0xff1d2029).brighter (over ? 0.12f : 0.0f));
        g.fillRoundedRectangle (b, 4.0f);
        if (! lit)
        {
            g.setColour (locked ? juce::Colour (0xff363a48) : colour.withAlpha (over ? 0.8f : 0.5f));
            g.drawRoundedRectangle (b, 4.0f, 1.0f);
        }
        const juce::Colour tc = lit ? juce::Colour (0xff0b0c11) : (locked ? faint : colour);
        auto tr = b;
        const float tw = juce::GlyphArrangement::getStringWidth (font (fontSize), getButtonText());
        const float icon = (locked || play) ? fontSize + 4.0f : 0.0f;
        float x = b.getCentreX() - (tw + icon) * 0.5f;
        if (locked) lockIcon (g, { x, b.getCentreY() - fontSize * 0.55f, fontSize * 0.8f, fontSize * 1.05f }, tc);
        if (play) playIcon (g, { x, b.getCentreY() - fontSize * 0.4f, fontSize * 0.7f, fontSize * 0.8f }, tc);
        x += icon;
        tr = { x, b.getY(), tw + 2.0f, b.getHeight() };
        mu::text (g, getButtonText(), tr, fontSize, tc, juce::Justification::centredLeft);
    }
};
} // namespace mu
