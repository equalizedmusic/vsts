#pragma once
// CURVA: paleta, escalas e peças de desenho comuns da tela (PluginEditor.cpp e EditorPaineis.cpp).
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace tema
{
inline const juce::Colour cFundoA { 0xff121726 }, cFundoB { 0xff0a0d15 }, cTopoA { 0xff181e30 }, cTopoB { 0xff10141f },
                          cChapa { 0xff121725 }, cBorda { 0xff252d42 }, cTexto { 0xffdfe6f3 }, cApagado { 0xff7c869c },
                          cTela { 0xff05070c }, cGrade { 0xff121828 }, cGradeF { 0xff1f283c },
                          cIn { 0xff8d9ab4 }, cOut { 0xff35d4ff }, cViol { 0xff8b6cff }, cCurva { 0xffff4fa3 },
                          cAmbar { 0xffffb020 }, cVerde { 0xff3fd68a }, cVermelho { 0xffff4f5e }, cAlvo { 0xffc9d2e6 };

constexpr double fMin = 20.0, fMax = 20000.0;
constexpr float dbTopo = 6.0f, dbBase = -90.0f;     // escala do espectro
constexpr float curvaMax = 24.0f;                    // eixo da direita: ±24 dB
constexpr float mTopo = 3.0f, mBase = -60.0f;        // escala dos medidores

inline juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}
inline float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

inline juce::String sinal (float v, int casas = 1)
{
    if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -casas)) return juce::String (0.0f, casas);
    return (v > 0 ? juce::String ("+") : u8 ("−")) + juce::String (std::abs (v), casas);
}
inline juce::String lufs (float v) { return v <= -69.9f ? u8 ("—") : sinal (v); }

inline float xDeFreq (double hz, juce::Rectangle<float> r)
{
    return r.getX() + r.getWidth() * (float) (std::log (hz / fMin) / std::log (fMax / fMin));
}
inline double freqDeX (float x, juce::Rectangle<float> r)
{
    return fMin * std::pow (fMax / fMin, (double) juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth()));
}
inline float yDeDb (float db, juce::Rectangle<float> r)
{
    return juce::jmap (juce::jlimit (dbBase, dbTopo, db), dbTopo, dbBase, r.getY(), r.getBottom());
}
inline float yDeCurva (float db, juce::Rectangle<float> r)
{
    return juce::jmap (juce::jlimit (-curvaMax, curvaMax, db), curvaMax, -curvaMax, r.getY(), r.getBottom());
}

inline void chapa (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& titulo = {}, const juce::String& sub = {})
{
    g.setGradientFill (juce::ColourGradient (cChapa.brighter (0.06f), 0, r.getY(), cChapa, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (cBorda.withAlpha (0.6f));
    g.drawLine (r.getX() + 6, r.getY() + 1.5f, r.getRight() - 6, r.getY() + 1.5f, 1.0f);
    if (titulo.isNotEmpty())
    {
        const auto f = fonte (10.5f, true).withExtraKerningFactor (0.2f);
        g.setFont (f);
        g.setColour (cTexto);
        g.drawText (titulo, juce::Rectangle<float> (r.getX() + 12, r.getY() + 7, 300, 16), juce::Justification::centredLeft);
        if (sub.isNotEmpty())
        {
            g.setFont (fonte (9.5f).withExtraKerningFactor (0.08f));
            g.setColour (cApagado);
            g.drawText (sub, juce::Rectangle<float> (r.getX() + 18 + largura (f, titulo), r.getY() + 7, 300, 16),
                        juce::Justification::centredLeft);
        }
    }
}

inline void glow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float lw)
{
    g.setColour (c.withAlpha (0.10f));
    g.strokePath (p, juce::PathStrokeType (lw * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.22f));
    g.strokePath (p, juce::PathStrokeType (lw * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline juce::ColourGradient comAlfa (juce::ColourGradient cg, float a)   // o mesmo degradê mais transparente
{
    for (int i = 0; i < cg.getNumColours(); ++i) cg.setColour (i, cg.getColour (i).withMultipliedAlpha (a));
    return cg;
}

inline juce::String nomeHz (double hz)
{
    return hz >= 1000.0 ? juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz" : juce::String (hz, hz < 100 ? 1 : 0) + " Hz";
}

inline juce::String nota (double hz)
{
    const double m = 69.0 + 12.0 * std::log2 (hz / 440.0);
    const int n = (int) std::lround (m);
    const int cents = (int) std::lround ((m - n) * 100.0);
    return juce::MidiMessage::getMidiNoteName (n, true, true, 3) + " " + (cents >= 0 ? juce::String ("+") : u8 ("−"))
         + juce::String (std::abs (cents)) + "c";
}

inline juce::String nomeVolume (float phon)
{
    if (phon <= 52.0f) return "MADRUGADA";
    if (phon <= 62.0f) return "BAIXINHO";
    if (phon <= 72.0f) return "CONVERSA";
    if (phon <= 80.0f) return u8 ("MÉDIO");
    return u8 ("REFERÊNCIA");
}

inline juce::Colour corNivel (float db)
{
    if (db > -1.0f) return cVermelho;
    if (db > -9.0f) return cAmbar;
    return cVerde;
}
} // namespace tema
