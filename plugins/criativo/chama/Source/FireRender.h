#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include "Theme.h"

namespace chama
{
// ruído de valor 2D com tabela fixa: barato o bastante para pintar o fogo pixel a pixel a 30 quadros
class ValueNoise
{
public:
    ValueNoise()
    {
        juce::Random r (0x43484d41);
        for (int i = 0; i < 256; ++i) { perm[i] = (uint8_t) i; val[i] = r.nextFloat(); }
        for (int i = 255; i > 0; --i) std::swap (perm[i], perm[r.nextInt (i + 1)]);
    }

    float at (float x, float y) const
    {
        const float fx = std::floor (x), fy = std::floor (y);
        const int ix = (int) fx, iy = (int) fy;
        float tx = x - fx, ty = y - fy;
        tx = tx * tx * (3.0f - 2.0f * tx);
        ty = ty * ty * (3.0f - 2.0f * ty);
        const float a = lat (ix, iy), b = lat (ix + 1, iy), c = lat (ix, iy + 1), d = lat (ix + 1, iy + 1);
        return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
    }

    float fbm (float x, float y, int octaves) const
    {
        float sum = 0.0f, amp = 0.5f, norm = 0.0f;
        for (int o = 0; o < octaves; ++o)
        {
            sum += amp * at (x, y);
            norm += amp;
            x = x * 2.03f + 17.1f;
            y = y * 2.03f + 9.7f;
            amp *= 0.5f;
        }
        return sum / norm;
    }

private:
    float lat (int x, int y) const { return val[perm[(perm[x & 255] + y) & 255]]; }
    uint8_t perm[256];
    float val[256];
};

struct FireParams
{
    float heat = 0.0f;    // CHAMA aplicada (0–1)
    float level = 0.0f;   // pico do sinal: o fogo respira com o som
    float noise = 0.5f;   // RUÍDO
    float space = 0.5f;   // ESPAÇO: fumaça
    float pulse = 0.0f;   // empurrão no tempo (1 no beat, cai até o próximo)
    float t = 0.0f;
    bool  centred = true; // palco: fogueira no meio; faixa de cima: muro de fogo
};

// fogo de verdade: campo de ruído subindo, com distorção lateral, pintado em baixa resolução
// e ampliado suave; cores de corpo negro (vermelho escuro → laranja → amarelo → quase branco)
class FireRenderer
{
public:
    FireRenderer()
    {
        // paleta: f = intensidade 0–1 → cor sem pré-multiplicar + alfa
        struct Stop { float f, r, g, b, a; };
        const Stop stops[] = {
            { 0.00f, 0.20f, 0.02f, 0.01f, 0.00f },
            { 0.12f, 0.42f, 0.04f, 0.02f, 0.45f },
            { 0.30f, 0.80f, 0.14f, 0.03f, 0.85f },
            { 0.50f, 1.00f, 0.40f, 0.05f, 1.00f },
            { 0.70f, 1.00f, 0.66f, 0.18f, 1.00f },
            { 0.86f, 1.00f, 0.86f, 0.48f, 1.00f },
            { 1.00f, 1.00f, 0.97f, 0.86f, 1.00f },
        };
        for (int i = 0; i < 256; ++i)
        {
            const float f = (float) i / 255.0f;
            int k = 0;
            while (k < 5 && f > stops[k + 1].f) ++k;
            const auto& A = stops[k];
            const auto& B = stops[k + 1];
            const float m = juce::jlimit (0.0f, 1.0f, (f - A.f) / (B.f - A.f));
            lut[i][0] = A.r + (B.r - A.r) * m;
            lut[i][1] = A.g + (B.g - A.g) * m;
            lut[i][2] = A.b + (B.b - A.b) * m;
            lut[i][3] = A.a + (B.a - A.a) * m;
        }
    }

    // pinta o fogo numa imagem w×h; aspect = largura/altura da área na tela (células de ruído quadradas)
    void render (int w, int h, float aspect, const FireParams& p)
    {
        if (! img.isValid() || img.getWidth() != w || img.getHeight() != h)
            img = juce::Image (juce::Image::ARGB, w, h, true);

        const float heat  = p.heat;
        const float pi    = juce::MathConstants<float>::pi;
        const float flameH = juce::jlimit (0.03f, 0.95f, (0.06f + 0.74f * heat) * (0.9f + 0.22f * p.level)
                                                           * (1.0f + 0.14f * p.pulse * (0.3f + heat)));
        const float speed = 1.0f + 1.7f * heat;
        const float scale = 3.2f;
        const float smokeAmt = p.space * heat;

        // altura de cada coluna: o perfil da fogueira e as línguas grandes que sobem e descem devagar
        colH.resize ((size_t) w);
        for (int px = 0; px < w; ++px)
        {
            const float u   = ((float) px + 0.5f) / (float) w;
            const float env = p.centred ? 0.3f + 0.7f * std::sin (u * pi) : 0.92f;
            const float var = 0.72f + 0.56f * noise.at (u * aspect * 1.4f + 5.0f, p.t * (0.45f + 0.5f * heat));
            colH[(size_t) px] = flameH * env * var;
        }

        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int py = 0; py < h; ++py)
        {
            const float v = 1.0f - ((float) py + 0.5f) / (float) h;   // 0 embaixo, 1 no alto
            const float Y = v * scale;

            for (int px = 0; px < w; ++px)
            {
                auto* pix = reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (px, py));
                const float H = colH[(size_t) px];
                const float base = 1.0f - v / juce::jmax (0.02f, H);   // 1 na base, 0 na altura da chama

                // longe demais de qualquer língua e sem fumaça: transparente sem gastar ruído
                if (base < -0.75f && (smokeAmt < 0.01f || v < H * 0.5f))
                {
                    pix->setARGB (0, 0, 0, 0);
                    continue;
                }

                const float u = ((float) px + 0.5f) / (float) w;
                const float X = u * aspect * scale;

                float fa = 0.0f, fr = 0.0f, fg = 0.0f, fb = 0.0f;
                if (base > -0.75f)
                {
                    // distorção lateral cresce para cima: a chama torce e se rasga em línguas
                    const float warp = noise.fbm (X * 0.9f + 31.0f, Y * 0.9f - p.t * speed * 0.6f, 2);
                    const float xx   = X + (warp - 0.5f) * (0.5f + 1.4f * v);
                    const float n    = noise.fbm (xx * 1.7f, Y * 1.7f - p.t * speed * 1.3f, 4);
                    // perto da base o fogo é cheio; o ruído rasga mais em cima, onde a chama vira língua
                    const float tear = 0.4f + 0.6f * juce::jlimit (0.0f, 1.0f, v / juce::jmax (0.02f, H));
                    float f = base * 0.88f + (n - 0.5f) * 2.4f * tear + 0.07f * p.pulse * heat;
                    f = juce::jlimit (0.0f, 1.0f, f);
                    f = f * f * (1.6f - 0.6f * f);   // borda mais definida, miolo cheio
                    const auto* c = lut[juce::jlimit (0, 255, (int) (f * 255.0f))];
                    fa = c[3];
                    fr = c[0] * fa;
                    fg = c[1] * fa;
                    fb = c[2] * fa;
                }

                // fumaça (ESPAÇO): violeta escura subindo mais devagar acima das chamas
                float sa = 0.0f;
                if (smokeAmt > 0.01f && v > H * 0.5f)
                {
                    const float sm = noise.fbm (X * 0.6f + 40.0f, Y * 0.6f - p.t * (0.3f + 0.4f * heat), 3);
                    const float rise = juce::jlimit (0.0f, 1.0f, (v - H * 0.5f) / 0.35f);
                    const float top  = 1.0f - 0.6f * juce::jlimit (0.0f, 1.0f, (v - 0.8f) / 0.2f);
                    sa = juce::jlimit (0.0f, 1.0f, (sm - 0.42f) * 3.2f) * rise * top * smokeAmt * 0.5f;
                }

                const float k = 1.0f - fa;
                const float A = fa + sa * k;
                const float R = fr + 0.42f * sa * k;
                const float G = fg + 0.33f * sa * k;
                const float B = fb + 0.58f * sa * k;
                pix->setARGB ((uint8_t) (A * 255.0f), (uint8_t) (juce::jmin (A, R) * 255.0f),
                              (uint8_t) (juce::jmin (A, G) * 255.0f), (uint8_t) (juce::jmin (A, B) * 255.0f));
            }
        }
    }

    // desenha ampliado e suave, com o clarão quente da base por baixo
    void draw (juce::Graphics& g, juce::Rectangle<float> area, const FireParams& p) const
    {
        if (! img.isValid())
            return;

        const float glow = 0.08f + 0.30f * p.heat + 0.10f * p.pulse * p.heat;
        juce::ColourGradient cg (juce::Colour::fromFloatRGBA (1.0f, 0.42f, 0.08f, glow), area.getCentreX(), area.getBottom(),
                                 juce::Colour::fromFloatRGBA (1.0f, 0.30f, 0.05f, 0.0f), area.getCentreX(),
                                 area.getBottom() - area.getHeight() * (0.35f + 0.65f * p.heat), true);
        g.setGradientFill (cg);
        g.fillRect (area);

        juce::Graphics::ScopedSaveState save (g);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, area, juce::RectanglePlacement::stretchToFit);
    }

private:
    ValueNoise noise;
    juce::Image img;
    std::vector<float> colH;
    float lut[256][4];
};

// faíscas (RUÍDO): sobem tremendo, com brilho em volta e somem no alto
inline void drawSparks (juce::Graphics& g, juce::Rectangle<float> area, const FireParams& p, int maxSparks)
{
    auto hashf = [] (int i) { const float v = std::sin ((float) i * 12.9898f) * 43758.5453f; return v - std::floor (v); };
    const float W = area.getWidth(), H = area.getHeight(), bottom = area.getBottom();
    const int sparks = juce::roundToInt ((float) maxSparks * p.noise * p.heat);
    for (int j = 0; j < sparks; ++j)
    {
        const float cyc = std::fmod (p.t * (0.3f + 0.55f * hashf (j + 11)) + hashf (j + 23), 1.0f);
        const float x   = area.getX() + W * (p.centred ? 0.15f + 0.7f * hashf (j + 37) : hashf (j + 37))
                        + std::sin (p.t * (1.5f + hashf (j)) + (float) j) * 9.0f * cyc;
        const float y   = bottom - cyc * H * (0.35f + 0.65f * p.heat);
        const float a   = (1.0f - cyc) * (0.5f + 0.5f * hashf (j + 51));
        const float r   = 0.8f + 1.2f * hashf (j + 61);
        g.setColour (theme::ice.withAlpha (0.22f * a));
        g.fillEllipse (x - r * 3.0f, y - r * 3.0f, r * 6.0f, r * 6.0f);
        g.setColour (theme::ice.interpolatedWith (juce::Colours::white, 0.5f).withAlpha (0.95f * a));
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }
}
} // namespace chama
