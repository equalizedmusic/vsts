// STOMPBOX — desenho de cada pedal (caixa, redondo e wah), knobs e chaves.
// As partes fixas de cada pedal (carcaça, parafusos, nome, footswitch, jacks) ficam numa imagem guardada por tipo
// (camadaPedal); por cima vão só os ponteiros dos knobs, as chaves e o LED.
#include "PluginEditor.h"
#include "Desenho.h"

using namespace desenho;
using stomp::infoPedal;

void StompboxEditor::knobP (juce::Graphics& g, const juce::String& pid, juce::Point<float> c, float diam, int estilo, const juce::String& rotulo,
                            juce::Colour corR, float def, int slot, float valor)
{
    const auto& base = camadaKnob (estilo, diam);
    const float m = diam * 0.5f + 9.0f;
    pinta (g, base, { c.x - m, c.y - m });
    knobPonteiro (g, c, diam, estilo, valor / 10.0f);
    if (rotulo.isNotEmpty())
        textoCabe (g, rotulo, { c.x - 40, c.y + diam * 0.5f + 3, 80, 12 }, corR, fonte (9.5f));
    if (pid.isNotEmpty())
        alvos.push_back ({ juce::Rectangle<float> (diam + 8, diam + 8).withCentre (c), aKnob, slot, -1, pid, def });
}

// chave de alavanca: rótulos dos lados; com 3 posições, o do meio vai em cima
void StompboxEditor::chave (juce::Graphics& g, juce::Point<float> c, int n, int pos, const char* const* nomes, juce::Colour corT, float tam)
{
    const float ang = n == 3 ? (pos == 0 ? -0.6f : pos == 1 ? 0.0f : 0.6f) : (pos == 0 ? -0.45f : 0.45f);
    g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillEllipse (c.x - 9, c.y - 9, 18, 18);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x - 2, c.y - 2, juce::Colour (0xff333333), c.x + 6, c.y + 6, true));
    g.fillEllipse (c.x - 6, c.y - 6, 12, 12);
    const juce::Point<float> v (std::sin (ang), -std::cos (ang));
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawLine (juce::Line<float> (c.translated (1, 2), c.translated (1, 2) + v * 13.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9a9a), c.x - 2, c.y, juce::Colour (0xfff6f6f6), c.x + 2, c.y, false));
    g.drawLine (juce::Line<float> (c, c + v * 13.0f), 4.0f);
    const auto f = fonte (tam);
    const auto ativo = corT, apagado = corT.withAlpha (0.45f);
    textoCabe (g, nomes[0], { c.x - 88, c.y - 7, 74, 14 }, pos == 0 ? ativo : apagado, f, juce::Justification::centredRight);
    if (n == 3)
    {
        textoCabe (g, nomes[1], { c.x - 36, c.y - 27, 72, 12 }, pos == 1 ? ativo : apagado, f);
        textoCabe (g, nomes[2], { c.x + 14, c.y - 7, 74, 14 }, pos == 2 ? ativo : apagado, f, juce::Justification::centredLeft);
    }
    else
        textoCabe (g, nomes[1], { c.x + 14, c.y - 7, 74, 14 }, pos == 1 ? ativo : apagado, f, juce::Justification::centredLeft);
}

// ---- geometria comum (corpo guardado e partes vivas usam as mesmas contas)
namespace
{
juce::Rectangle<float> rectCorpo (int forma, juce::Point<float> bc)
{
    if (forma == 1) return { bc.x - 125, bc.y - 250, 250, 250 };
    if (forma == 2) return { bc.x - 105, bc.y - 360, 210, 360 };
    return { bc.x - 105, bc.y - 340, 210, 340 };
}
} // namespace

// partes fixas: não dependem de knob, chave nem LED
void StompboxEditor::corpoPedal (juce::Graphics& g, int tipo, juce::Point<float> bc)
{
    const auto& inf = infoPedal (tipo);
    const auto cor = juce::Colour (inf.cor), corT = juce::Colour (inf.texto);
    const auto r = rectCorpo (inf.forma, bc);
    const float cx = bc.x, y0 = r.getY();
    if (inf.forma == 1)   // ---- redondo (fuzz)
    {
        jack (g, { r.getX() - 5, r.getCentreY() - 21, 8, 22 }); jack (g, { r.getRight() - 3, r.getCentreY() - 21, 8, 22 });
        carcaca (g, r, 125.0f, cor, true, &grao);
        textoCabe (g, inf.nome, { r.getX() + 30, y0 + 132, 190, 32 }, corT, fontePedal (inf.fonte, 27.0f));
        textoCabe (g, inf.tipo, { r.getX() + 40, y0 + 164, 170, 12 }, corT.withAlpha (0.8f), fonte (9.0f));
        footswitch (g, { cx, y0 + 219 }, 0.8f);
    }
    else if (inf.forma == 2)   // ---- wah: carcaça e footswitch (a base que balança é viva)
    {
        jack (g, { r.getX() - 7, r.getY() + 58, 8, 22 }); jack (g, { r.getRight() - 1, r.getY() + 58, 8, 22 });
        carcaca (g, r, 14.0f, cor, false, &grao);
        footswitch (g, { cx, r.getY() + 336 }, 0.6f);
    }
    else   // ---- caixa
    {
        jack (g, { r.getX() - 7, y0 + 58, 8, 22 }); jack (g, { r.getRight() - 1, y0 + 58, 8, 22 });
        carcaca (g, r, 14.0f, cor, false, &grao);
        parafuso (g, { r.getX() + 12, y0 + 12 }, 0.5f); parafuso (g, { r.getRight() - 12, y0 + 12 }, -0.9f);
        parafuso (g, { r.getX() + 12, r.getBottom() - 12 }, 1.2f); parafuso (g, { r.getRight() - 12, r.getBottom() - 12 }, 0.2f);
        if (inf.faixa != 0)
        {
            g.setColour (juce::Colour (inf.faixa)); g.fillRect (r.getX(), y0 + 150, r.getWidth(), 5.0f); g.fillRect (r.getX(), y0 + 158, r.getWidth(), 2.0f);
        }
        if (inf.nsw > 0 && inf.rotuloSw[0] != 0 && inf.nk == 4)
            textoCabe (g, inf.rotuloSw, { r.getX() + 60, y0 + 141, 90, 10 }, corT.withAlpha (0.7f), fonte (8.0f));
        else if (inf.nsw > 0 && inf.rotuloSw[0] != 0)
            textoCabe (g, inf.rotuloSw, { r.getX() + 60, y0 + 88, 90, 10 }, corT.withAlpha (0.7f), fonte (8.0f));
        textoCabe (g, inf.nome, { r.getX() + 10, y0 + 194, 190, 38 }, corT, fontePedal (inf.fonte, inf.fonte == 3 || inf.fonte == 5 ? 40.0f : 31.0f));
        textoCabe (g, inf.tipo, { r.getX() + 10, y0 + 232, 190, 12 }, corT.withAlpha (0.8f), fonte (9.0f));
        footswitch (g, { cx, y0 + 287 });
    }
}

void StompboxEditor::desenhaPedal (juce::Graphics& g, int tipo, juce::Point<float> bc, const StompboxProcessor::SlotDados& d, int slot, bool registra)
{
    const auto& inf = infoPedal (tipo);
    const auto corT = juce::Colour (inf.texto);
    const size_t alvosAntes = alvos.size();
    const float cx = bc.x;
    const auto r = rectCorpo (inf.forma, bc);
    auto pid = [&] (const char* campo) { return StompboxProcessor::id (slot, campo); };
    auto kpid = [&] (int k) { return pid (("k" + std::to_string (k + 1)).c_str()); };

    pinta (g, camadaPedal (tipo), bc.translated (-150.0f, -400.0f));

    if (inf.forma == 1)   // ---- redondo (fuzz)
    {
        const float y0 = r.getY();
        knobP (g, kpid (0), { cx - 50, y0 + 66 }, 46, inf.knobEst, inf.knob[0], corT, inf.def[0], slot, d.k[0]);
        knobP (g, kpid (1), { cx + 50, y0 + 66 }, 46, inf.knobEst, inf.knob[1], corT, inf.def[1], slot, d.k[1]);
        chave (g, { cx, y0 + 118 }, 2, d.sw, inf.sw, corT, 9.0f);
        alvos.push_back ({ { cx - 40, y0 + 104, 80, 28 }, aSwitch, slot, inf.nsw, pid ("sw") });
        led (g, { cx, y0 + 186 }, 5.5f, d.on);
        alvos.push_back ({ { cx - 26, y0 + 194, 52, 50 }, aFoot, slot });
        if (registra) alvos.push_back ({ r, aPedal, slot });
    }
    else if (inf.forma == 2)   // ---- wah
    {
        // a base do pedal (o "treadle"): inclinação pela posição (PEDAL); arrasta para cima/baixo
        const float pos = d.k[0] / 10.0f;
        const juce::Rectangle<float> t (cx - 89, r.getY() + 10, 178, 214);
        sombra (g, t, 12.0f, 10.0f, 6.0f, 0.6f);
        g.setColour (juce::Colour (0xff222222)); g.fillRoundedRectangle (t, 12.0f);
        {
            juce::RectangleList<float> riscos, brilhos;
            for (float y = t.getY() + 10; y < t.getBottom() - 8; y += 9.0f)
            {
                riscos.addWithoutMerging ({ t.getX() + 10, y, t.getWidth() - 20, 6.0f });
                brilhos.addWithoutMerging ({ t.getX() + 10, y + 6, t.getWidth() - 20, 1.5f });
            }
            g.setColour (juce::Colour (0xff1b1b1b)); g.fillRectList (riscos);
            g.setColour (juce::Colour (0xff2e2e2e)); g.fillRectList (brilhos);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f * pos), t.getX(), t.getY(),
                                                 juce::Colours::black.withAlpha (0.35f * (1.0f - pos)), t.getX(), t.getBottom(), false));
        g.fillRoundedRectangle (t, 12.0f);
        const juce::Rectangle<float> placa (cx - 62, t.getCentreY() - 22 + (0.5f - pos) * 30.0f, 124, 44);
        sombra (g, placa, 6.0f, 3.0f, 2.0f, 0.6f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2ee), placa.getX(), placa.getY(), juce::Colour (0xff9d9d97), placa.getRight(), placa.getBottom(), false));
        g.fillRoundedRectangle (placa, 6.0f);
        textoCabe (g, inf.nome, placa.reduced (6, 2), juce::Colour (0xff1a1a1a), fonte (24.0f, true, "Arial Black"));
        // indicador da posição na lateral
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (t.getRight() - 9, t.getY() + 14, 4, t.getHeight() - 28, 2.0f);
        g.setColour (juce::Colour (0xffffd24a)); g.fillEllipse (t.getRight() - 11, t.getBottom() - 14 - (t.getHeight() - 28) * pos - 4, 8, 8);
        if (tipo == stomp::Bender)   // o intervalo que está soando, no pé do pedal
        {
            const bool det = d.sw == 1;
            const int semi = stomp::benderSemitons (d.k[1] / 10.0f);
            const juce::String s = det ? juce::String ("DETUNE ") + juce::String (juce::roundToInt (30.0f * d.k[0] / 10.0f)) + " ct"
                                       : juce::String (stomp::benderNome (d.k[1] / 10.0f)) + u8 (" \xc2\xb7 ") + (semi > 0 ? "+" : "")
                                             + juce::String (semi * d.k[0] / 10.0f, 1);
            const juce::Rectangle<float> rot (cx - 64, t.getBottom() - 30, 128, 20);
            g.setColour (juce::Colours::black.withAlpha (0.75f)); g.fillRoundedRectangle (rot, 4.0f);
            textoCabe (g, s, rot.reduced (4, 0), juce::Colour (0xffffd24a), fonte (10.0f));
        }
        alvos.push_back ({ t, aTreadle, slot, -1, kpid (0), inf.def[0] });
        // knobs dos lados, LED no meio, chave embaixo (rótulos dos knobs acima da chave, sem encostar)
        const float y1 = r.getY() + 250;
        knobP (g, kpid (1), { cx - 56, y1 }, 30, 1, inf.knob[1], corT, inf.def[1], slot, d.k[1]);
        knobP (g, kpid (2), { cx + 56, y1 }, 30, 1, inf.knob[2], corT, inf.def[2], slot, d.k[2]);
        led (g, { cx, y1 }, 5.5f, d.on);
        chave (g, { cx, r.getY() + 300 }, 2, d.sw, inf.sw, corT, 9.0f);
        alvos.push_back ({ { cx - 40, r.getY() + 286, 80, 28 }, aSwitch, slot, inf.nsw, pid ("sw") });
        alvos.push_back ({ { cx - 22, r.getY() + 318, 44, 38 }, aFoot, slot });
        if (registra) alvos.push_back ({ r, aPedal, slot });
    }
    else   // ---- caixa
    {
        const float y0 = r.getY();
        led (g, { cx, y0 + 21 }, 5.5f, d.on);
        float yChave = y0 + 116;
        // ECHO com SYNC: o TIME vira divisão de tempo (vai no rótulo do knob)
        auto rotulo = [&] (int k) -> juce::String
        {
            if (tipo == stomp::Echo && k == 0 && proc.real ("sync") > 0.5f) return u8 ("TIME \xc2\xb7 ") + stomp::nomeDivisaoDelay (d.k[0] / 10.0f);
            return inf.knob[k];
        };
        if (tipo == stomp::Hump)
        {
            knobP (g, kpid (0), { cx - 44, y0 + 60 }, 46, inf.knobEst, rotulo (0), corT, inf.def[0], slot, d.k[0]);
            knobP (g, kpid (1), { cx + 44, y0 + 60 }, 46, inf.knobEst, rotulo (1), corT, inf.def[1], slot, d.k[1]);
            knobP (g, kpid (2), { cx, y0 + 110 }, 32, inf.knobEst, rotulo (2), corT, inf.def[2], slot, d.k[2]);
        }
        else if (tipo == stomp::Swirl)
        {
            knobP (g, kpid (0), { cx - 30, y0 + 72 }, 58, inf.knobEst, rotulo (0), corT, inf.def[0], slot, d.k[0]);
            knobP (g, kpid (1), { cx + 56, y0 + 80 }, 32, inf.knobEst, rotulo (1), corT, inf.def[1], slot, d.k[1]);
            yChave = y0 + 136;
        }
        else if (inf.nk == 4)   // 2 fileiras de 2: rótulos com folga, chave embaixo
        {
            const float xs[2] = { -50, 50 }, ys[2] = { 58, 114 };
            for (int k = 0; k < 4; ++k)
                knobP (g, kpid (k), { cx + xs[k % 2], y0 + ys[k / 2] }, 38, inf.knobEst, rotulo (k), corT, inf.def[k], slot, d.k[k]);
            yChave = y0 + 172;
        }
        else
        {
            const float xs[3] = { -62, 0, 62 };
            for (int k = 0; k < inf.nk; ++k) knobP (g, kpid (k), { cx + xs[k], y0 + 60 }, 44, inf.knobEst, rotulo (k), corT, inf.def[k], slot, d.k[k]);
        }
        if (inf.nsw > 0)
        {
            chave (g, { cx, yChave }, inf.nsw, juce::jlimit (0, inf.nsw - 1, d.sw), inf.sw, corT, 9.0f);
            alvos.push_back ({ { cx - 50, yChave - 16, 100, 30 }, aSwitch, slot, inf.nsw, pid ("sw") });
        }
        alvos.push_back ({ { cx - 32, y0 + 255, 64, 64 }, aFoot, slot });
        if (registra) alvos.push_back ({ r, aPedal, slot });
    }
    if (! registra) alvos.resize (alvosAntes);   // fantasma do arrasto: não vira alvo
}
