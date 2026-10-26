#pragma once

// PEÇAS DE HARDWARE dos instrumentos — knob, lâmpada piloto, LED, tecla e lateral de madeira RENDERIZADOS no
// Blender/Cycles (instrumentos/_hardware/tools/pecas.py, a mesma luz e os mesmos materiais da SÉRIE KNOB).
// As imagens ficam em Source/img de cada instrumento (PluginImg.h); este arquivo é igual nos três
// (o original fica em instrumentos/_hardware — copiar para Source/ quando mudar).
//
// Por cima das imagens a interface desenha ao vivo só o que muda: o risco do knob, a cor e o brilho da luz.
// Os knobs são 16 quadros de UMA canelura (as caneluras se repetem): o quadro sai do ângulo, então as caneluras
// giram junto com o valor e a luz do estúdio fica parada, como num aparelho de verdade.

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <map>
#include "PluginImg.h"

namespace hw
{
using RF = juce::Rectangle<float>;
using PF = juce::Point<float>;

inline juce::Image carrega (const juce::String& nome)
{
    int tam = 0;
    if (const char* dados = PluginImg::getNamedResource (nome.replaceCharacter ('.', '_').toRawUTF8(), tam))
        return juce::ImageCache::getFromMemory (dados, tam);
    return {};
}

// medidas das imagens (pecas.json): frações do lado da imagem
struct Medidas
{
    float faceX, faceY;    // centro da face de cima do knob (a câmera é inclinada: fica acima do centro da base)
    float raio;            // raio do corpo
    float raioFace;        // raio da face onde o risco é pintado, em raios do corpo
    int   caneluras;
};

enum class Knob { preto, alu };

inline Medidas medidas (Knob k)
{
    return k == Knob::preto ? Medidas { 0.4437f, 0.3896f, 0.3125f, 0.83f, 30 }
                            : Medidas { 0.4437f, 0.3914f, 0.3125f, 0.60f, 18 };
}

struct Pecas
{
    juce::Image knob[2][16];
    juce::Image lampadaOff, lampadaLuz, ledOff, ledLuz, tecla, teclaBaixa, teclaLuz, madeiraEsq, madeiraDir;
    std::map<juce::uint32, juce::Image> tintas;    // camada acesa já tingida, por imagem e cor

    Pecas()
    {
        for (int q = 0; q < 16; ++q)
        {
            const auto n = juce::String (q).paddedLeft ('0', 2);
            knob[0][q] = carrega ("knob_preto_" + n + ".png");
            knob[1][q] = carrega ("knob_alu_" + n + ".png");
        }
        lampadaOff = carrega ("lampada_off.png"); lampadaLuz = carrega ("lampada_luz.png");
        ledOff     = carrega ("led_off.png");     ledLuz     = carrega ("led_luz.png");
        tecla      = carrega ("tecla.png");       teclaBaixa = carrega ("tecla_baixa.png");
        teclaLuz   = carrega ("tecla_luz.png");
        madeiraEsq = carrega ("madeira_esq.png"); madeiraDir = carrega ("madeira_dir.png");
    }

    // a camada acesa é branca: vira a cor do LED, e o miolo mais forte continua quase branco (como um LED de verdade)
    const juce::Image& tinge (const juce::Image& luz, int qual, juce::Colour cor)
    {
        const auto chave = (cor.getARGB() & 0x00ffffffu) | ((juce::uint32) qual << 24);
        auto it = tintas.find (chave);
        if (it != tintas.end()) return it->second;
        juce::Image out (juce::Image::ARGB, luz.getWidth(), luz.getHeight(), true);
        if (luz.isValid())
        {
            juce::Image::BitmapData src (luz, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData dst (out, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < luz.getHeight(); ++y)
                for (int x = 0; x < luz.getWidth(); ++x)
                {
                    const auto p = src.getPixelColour (x, y);
                    if (p.getAlpha() == 0) continue;
                    const float v = p.getPerceivedBrightness();
                    const float quente = juce::jlimit (0.0f, 1.0f, (v - 0.82f) / 0.18f);
                    const auto c = cor.withMultipliedBrightness (juce::jmin (1.0f, 0.35f + 0.9f * v))
                                      .interpolatedWith (juce::Colours::white, quente * 0.75f);
                    dst.setPixelColour (x, y, c.withAlpha (p.getAlpha()));
                }
        }
        return tintas[chave] = out;
    }
};

inline void desenha (juce::Graphics& g, const juce::Image& img, RF dst, float opacidade = 1.0f)
{
    if (! img.isValid()) return;
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (opacidade);   // a imagem herda a transparência da última cor: sempre dar a opacidade
    g.drawImage (img, dst, juce::RectanglePlacement::stretchToFit);
}

// ---------------- knob ----------------
// c = centro da face de cima (onde a escala do painel é centrada); R = raio do corpo em pixels;
// pos 0..1 em 270° (de 7h a 5h); risco na cor 'cor'
inline void knob (juce::Graphics& g, PF c, float R, float pos, juce::Colour cor, Knob tipo = Knob::preto)
{
    juce::SharedResourcePointer<Pecas> p;
    const auto m  = medidas (tipo);
    const float pi = juce::MathConstants<float>::pi;
    const float a  = pi * 1.25f + pi * 1.5f * juce::jlimit (0.0f, 1.0f, pos);
    const float S  = R / m.raio;                                   // lado da imagem
    const float per = 2.0f * pi / (float) m.caneluras;
    int q = juce::roundToInt (a / per * 16.0f) % 16;
    if (q < 0) q += 16;
    desenha (g, p->knob[tipo == Knob::preto ? 0 : 1][q], { c.x - m.faceX * S, c.y - m.faceY * S, S, S });

    auto pt = [c, a] (float r) { return PF (c.x + r * std::sin (a), c.y - r * std::cos (a)); };
    auto risco = [&g] (PF p1, PF p2, juce::Colour col, float w)
    {
        juce::Path l;
        l.startNewSubPath (p1);
        l.lineTo (p2);
        g.setColour (col);
        g.strokePath (l, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    const float w = juce::jmax (1.2f, R * 0.085f);
    if (tipo == Knob::preto)
    {
        // risco pintado na face preta, da tampa de alumínio até a borda
        risco (pt (R * 0.55f), pt (R * 0.8f), juce::Colours::black.withAlpha (0.5f), w * 1.5f);
        risco (pt (R * 0.55f), pt (R * 0.79f), cor.interpolatedWith (juce::Colours::white, 0.12f), w);
    }
    else
    {
        // marca gravada no alumínio e ponteiro aceso na tampa preta
        risco (pt (R * 0.66f), pt (R * 0.9f), juce::Colours::black.withAlpha (0.5f), w * 0.8f);
        risco (pt (R * 0.12f), pt (R * 0.5f), cor.withAlpha (0.25f), w * 2.4f);
        risco (pt (R * 0.12f), pt (R * 0.5f), cor, w);
    }
}

// ---------------- luz: brilho no painel em volta + camada acesa tingida ----------------
inline void brilho (juce::Graphics& g, PF c, float r, juce::Colour cor, float nivel)
{
    juce::ColourGradient bl (cor.withAlpha (0.42f * nivel), c.x, c.y, cor.withAlpha (0.0f), c.x + r, c.y, true);
    bl.addColour (0.35, cor.withAlpha (0.16f * nivel));
    g.setGradientFill (bl);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
}

// lâmpada piloto com aro cromado e lente facetada (r = raio da lente). nivel 0..1
inline void lamp (juce::Graphics& g, PF c, float r, juce::Colour cor, float nivel)
{
    juce::SharedResourcePointer<Pecas> p;
    const float S = r / 0.2176f;
    const RF dst (c.x - 0.4706f * S, c.y - 0.4452f * S, S, S);
    nivel = juce::jlimit (0.0f, 1.0f, nivel);
    desenha (g, p->lampadaOff, dst);
    if (nivel > 0.01f)
    {
        desenha (g, p->tinge (p->lampadaLuz, 1, cor), dst, nivel);
        brilho (g, c, r * 4.0f, cor, nivel);
    }
    else
        desenha (g, p->tinge (p->lampadaLuz, 1, cor), dst, 0.10f);   // apagada ainda mostra a cor do vidro
}

inline void lamp (juce::Graphics& g, PF c, float r, juce::Colour cor, bool acesa) { lamp (g, c, r, cor, acesa ? 1.0f : 0.0f); }

// LED de 3 mm em domo no porta-LED preto (r = raio do domo; mesma assinatura do real::led)
inline void led (juce::Graphics& g, PF c, float r, juce::Colour cor, float nivel)
{
    juce::SharedResourcePointer<Pecas> p;
    const float S = r / 0.2333f;
    const RF dst (c.x - 0.4733f * S, c.y - 0.4546f * S, S, S);
    nivel = juce::jlimit (0.0f, 1.0f, nivel);
    desenha (g, p->ledOff, dst);
    desenha (g, p->tinge (p->ledLuz, 2, cor), dst, nivel > 0.01f ? 0.25f + 0.75f * nivel : 0.16f);
    if (nivel > 0.01f)
        brilho (g, c, r * 3.4f, cor, nivel);
}

// ---------------- tecla (esticada em 9 partes: as quinas e o chanfro não deformam) ----------------
// imagem 250 × 186, face da tecla de (24, 21) a (216, 149), quina de 35 px
inline void noveFatias (juce::Graphics& g, const juce::Image& img, RF r, float opacidade = 1.0f)
{
    if (! img.isValid()) return;
    const float fx0 = 24.0f, fy0 = 21.0f, fx1 = 216.0f, fy1 = 149.0f, q = 35.0f;
    const float k = juce::jmin (r.getHeight() / (fy1 - fy0), r.getWidth() / (2.0f * q + 4.0f));
    const float sx[4] = { 0.0f, fx0 + q, fx1 - q, (float) img.getWidth() };
    const float sy[4] = { 0.0f, fy0 + q, fy1 - q, (float) img.getHeight() };
    const float dx[4] = { r.getX() - fx0 * k, r.getX() + q * k, r.getRight() - q * k, r.getRight() + (sx[3] - fx1) * k };
    const float dy[4] = { r.getY() - fy0 * k, r.getY() + q * k, r.getBottom() - q * k, r.getBottom() + (sy[3] - fy1) * k };
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (opacidade);
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i)
        {
            const juce::Rectangle<int> src ((int) sx[i], (int) sy[j], (int) (sx[i + 1] - sx[i]), (int) (sy[j + 1] - sy[j]));
            const float w = dx[i + 1] - dx[i], h = dy[j + 1] - dy[j];
            if (src.isEmpty() || w <= 0.0f || h <= 0.0f) continue;
            // 0,5 px a mais em cada fatia: sem fresta entre elas quando a janela está ampliada
            g.drawImageTransformed (img.getClippedImage (src),
                                    juce::AffineTransform::scale ((w + 0.5f) / (float) src.getWidth(), (h + 0.5f) / (float) src.getHeight())
                                        .translated (dx[i], dy[j]));
        }
}

// tampa de tecla de plástico preto; pressed = afundada. Devolve a face (onde vai o texto). luz 0..1 acende o topo
inline RF keycap (juce::Graphics& g, RF r, float, bool pressed, juce::Colour cor = {}, float luz = 0.0f)
{
    juce::SharedResourcePointer<Pecas> p;
    noveFatias (g, pressed ? p->teclaBaixa : p->tecla, r);
    if (luz > 0.01f && ! cor.isTransparent())
        noveFatias (g, p->tinge (p->teclaLuz, 3, cor), r, juce::jmin (1.0f, luz) * 0.85f);
    auto face = r.reduced (juce::jmin (r.getHeight() * 0.12f, 3.0f));
    return pressed ? face.translated (0.0f, 0.6f) : face;
}

// ---------------- lateral de nogueira envernizada ----------------
inline void cheek (juce::Graphics& g, RF r, bool left)
{
    juce::SharedResourcePointer<Pecas> p;
    desenha (g, left ? p->madeiraEsq : p->madeiraDir, r);
    // sombra que a lateral (mais alta que o painel) joga sobre ele
    const float s = juce::jmin (10.0f, r.getWidth() * 0.4f);
    juce::ColourGradient sh (juce::Colours::black.withAlpha (0.45f), left ? r.getRight() : r.getX(), 0.0f,
                             juce::Colours::black.withAlpha (0.0f), left ? r.getRight() + s : r.getX() - s, 0.0f, false);
    g.setGradientFill (sh);
    g.fillRect (left ? RF (r.getRight(), r.getY(), s, r.getHeight()) : RF (r.getX() - s, r.getY(), s, r.getHeight()));
}
} // namespace hw
