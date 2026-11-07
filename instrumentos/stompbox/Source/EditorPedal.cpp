// STOMPBOX — desenho de cada pedal (caixa, redondo e wah), knobs e chaves.
#include "PluginEditor.h"
#include "Desenho.h"

using namespace desenho;
using stomp::infoPedal;

void StompboxEditor::knobP (juce::Graphics& g, const juce::String& pid, juce::Point<float> c, float diam, int estilo, const juce::String& rotulo,
                            juce::Colour corR, float def, int slot, float valor)
{
    knob (g, c, diam, estilo, valor / 10.0f);
    if (rotulo.isNotEmpty())
        texto (g, rotulo, { c.x - 40, c.y + diam * 0.5f + 3, 80, 12 }, corR, fonte (9.5f));
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
    texto (g, nomes[0], { c.x - 92, c.y - 7, 80, 14 }, pos == 0 ? ativo : apagado, f, juce::Justification::centredRight);
    if (n == 3)
    {
        texto (g, nomes[1], { c.x - 40, c.y - 26, 80, 12 }, pos == 1 ? ativo : apagado, f);
        texto (g, nomes[2], { c.x + 12, c.y - 7, 80, 14 }, pos == 2 ? ativo : apagado, f, juce::Justification::centredLeft);
    }
    else
        texto (g, nomes[1], { c.x + 12, c.y - 7, 80, 14 }, pos == 1 ? ativo : apagado, f, juce::Justification::centredLeft);
}

void StompboxEditor::desenhaPedal (juce::Graphics& g, int tipo, juce::Point<float> bc, const StompboxProcessor::SlotDados& d, int slot, bool registra)
{
    const auto& inf = infoPedal (tipo);
    const auto cor = juce::Colour (inf.cor), corT = juce::Colour (inf.texto);
    const size_t alvosAntes = alvos.size();
    const float cx = bc.x;
    auto pid = [&] (const char* campo) { return StompboxProcessor::id (slot, campo); };
    auto kpid = [&] (int k) { return pid (("k" + std::to_string (k + 1)).c_str()); };

    if (inf.forma == 1)   // ---- redondo (fuzz)
    {
        const juce::Rectangle<float> r (cx - 125, bc.y - 250, 250, 250);
        jack (g, { r.getX() - 5, r.getCentreY() - 21, 8, 22 }); jack (g, { r.getRight() - 3, r.getCentreY() - 21, 8, 22 });
        carcaca (g, r, 125.0f, cor, true);
        const float y0 = r.getY();
        knobP (g, kpid (0), { cx - 50, y0 + 66 }, 46, inf.knobEst, inf.knob[0], corT, inf.def[0], slot, d.k[0]);
        knobP (g, kpid (1), { cx + 50, y0 + 66 }, 46, inf.knobEst, inf.knob[1], corT, inf.def[1], slot, d.k[1]);
        chave (g, { cx, y0 + 118 }, 2, d.sw, inf.sw, corT, 9.0f);
        alvos.push_back ({ { cx - 40, y0 + 104, 80, 28 }, aSwitch, slot, inf.nsw, pid ("sw") });
        texto (g, inf.nome, { r.getX(), y0 + 132, 250, 32 }, corT, fontePedal (inf.fonte, 27.0f));
        texto (g, inf.tipo, { r.getX(), y0 + 164, 250, 12 }, corT.withAlpha (0.8f), fonte (9.0f));
        led (g, { cx, y0 + 186 }, 5.5f, d.on);
        footswitch (g, { cx, y0 + 219 }, 0.8f);
        alvos.push_back ({ { cx - 26, y0 + 194, 52, 50 }, aFoot, slot });
        if (registra) alvos.push_back ({ r, aPedal, slot });
    }
    else if (inf.forma == 2)   // ---- wah
    {
        const juce::Rectangle<float> r (cx - 105, bc.y - 360, 210, 360);
        jack (g, { r.getX() - 7, r.getY() + 58, 8, 22 }); jack (g, { r.getRight() - 1, r.getY() + 58, 8, 22 });
        carcaca (g, r, 14.0f, cor);
        // a base do pedal (o "treadle"): inclinação pela posição (PEDAL); arrasta para cima/baixo
        const float pos = d.k[0] / 10.0f;
        const juce::Rectangle<float> t (cx - 89, r.getY() + 10, 178, 220);
        sombra (g, t, 12.0f, 10.0f, 6.0f, 0.6f);
        g.setColour (juce::Colour (0xff222222)); g.fillRoundedRectangle (t, 12.0f);
        for (float y = t.getY() + 10; y < t.getBottom() - 8; y += 9.0f)
        {
            g.setColour (juce::Colour (0xff1b1b1b)); g.fillRect (t.getX() + 10, y, t.getWidth() - 20, 6.0f);
            g.setColour (juce::Colour (0xff2e2e2e)); g.fillRect (t.getX() + 10, y + 6, t.getWidth() - 20, 1.5f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f * pos), t.getX(), t.getY(),
                                                 juce::Colours::black.withAlpha (0.35f * (1.0f - pos)), t.getX(), t.getBottom(), false));
        g.fillRoundedRectangle (t, 12.0f);
        const juce::Rectangle<float> placa (cx - 60, t.getCentreY() - 22 + (0.5f - pos) * 30.0f, 120, 44);
        sombra (g, placa, 6.0f, 3.0f, 2.0f, 0.6f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2ee), placa.getX(), placa.getY(), juce::Colour (0xff9d9d97), placa.getRight(), placa.getBottom(), false));
        g.fillRoundedRectangle (placa, 6.0f);
        texto (g, inf.nome, placa, juce::Colour (0xff1a1a1a), fonte (24.0f, true, "Arial Black"));
        // indicador da posição na lateral
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (t.getRight() - 9, t.getY() + 14, 4, t.getHeight() - 28, 2.0f);
        g.setColour (juce::Colour (0xffffd24a)); g.fillEllipse (t.getRight() - 11, t.getBottom() - 14 - (t.getHeight() - 28) * pos - 4, 8, 8);
        alvos.push_back ({ t, aTreadle, slot, -1, kpid (0), inf.def[0] });
        const float y1 = r.getY() + 262;
        knobP (g, kpid (1), { cx - 52, y1 }, 32, 1, inf.knob[1], corT, inf.def[1], slot, d.k[1]);
        knobP (g, kpid (2), { cx + 52, y1 }, 32, 1, inf.knob[2], corT, inf.def[2], slot, d.k[2]);
        led (g, { cx, y1 }, 5.5f, d.on);
        chave (g, { cx, r.getY() + 300 }, 2, d.sw, inf.sw, corT, 9.0f);
        alvos.push_back ({ { cx - 40, r.getY() + 286, 80, 28 }, aSwitch, slot, inf.nsw, pid ("sw") });
        footswitch (g, { cx, r.getY() + 334 }, 0.62f);
        alvos.push_back ({ { cx - 22, r.getY() + 314, 44, 40 }, aFoot, slot });
        if (registra) alvos.push_back ({ r, aPedal, slot });
    }
    else   // ---- caixa
    {
        const juce::Rectangle<float> r (cx - 105, bc.y - 340, 210, 340);
        const float y0 = r.getY();
        jack (g, { r.getX() - 7, y0 + 58, 8, 22 }); jack (g, { r.getRight() - 1, y0 + 58, 8, 22 });
        carcaca (g, r, 14.0f, cor);
        parafuso (g, { r.getX() + 12, y0 + 12 }, 0.5f); parafuso (g, { r.getRight() - 12, y0 + 12 }, -0.9f);
        parafuso (g, { r.getX() + 12, r.getBottom() - 12 }, 1.2f); parafuso (g, { r.getRight() - 12, r.getBottom() - 12 }, 0.2f);
        if (inf.faixa != 0)
        {
            g.setColour (juce::Colour (inf.faixa)); g.fillRect (r.getX(), y0 + 150, r.getWidth(), 5.0f); g.fillRect (r.getX(), y0 + 158, r.getWidth(), 2.0f);
        }
        led (g, { cx, y0 + 21 }, 5.5f, d.on);
        float yChave = y0 + 116;
        if (tipo == stomp::Hump)
        {
            knobP (g, kpid (0), { cx - 44, y0 + 60 }, 46, inf.knobEst, inf.knob[0], corT, inf.def[0], slot, d.k[0]);
            knobP (g, kpid (1), { cx + 44, y0 + 60 }, 46, inf.knobEst, inf.knob[1], corT, inf.def[1], slot, d.k[1]);
            knobP (g, kpid (2), { cx, y0 + 106 }, 32, inf.knobEst, inf.knob[2], corT, inf.def[2], slot, d.k[2]);
        }
        else if (tipo == stomp::Swirl)
        {
            knobP (g, kpid (0), { cx - 30, y0 + 72 }, 58, inf.knobEst, inf.knob[0], corT, inf.def[0], slot, d.k[0]);
            knobP (g, kpid (1), { cx + 54, y0 + 80 }, 32, inf.knobEst, inf.knob[1], corT, inf.def[1], slot, d.k[1]);
            yChave = y0 + 132;
        }
        else if (inf.nk == 4)
        {
            const float xs[4] = { -69, -23, 23, 69 };
            for (int k = 0; k < 4; ++k) knobP (g, kpid (k), { cx + xs[k], y0 + 56 }, 34, inf.knobEst, inf.knob[k], corT, inf.def[k], slot, d.k[k]);
            yChave = y0 + 108;
        }
        else
        {
            const float xs[3] = { -62, 0, 62 };
            for (int k = 0; k < inf.nk; ++k) knobP (g, kpid (k), { cx + xs[k], y0 + 60 }, 44, inf.knobEst, inf.knob[k], corT, inf.def[k], slot, d.k[k]);
        }
        if (inf.nsw > 0)
        {
            if (inf.rotuloSw[0] != 0) texto (g, inf.rotuloSw, { r.getX(), yChave - 30, 210, 10 }, corT.withAlpha (0.7f), fonte (8.0f));
            chave (g, { cx, yChave }, inf.nsw, juce::jlimit (0, inf.nsw - 1, d.sw), inf.sw, corT, 9.0f);
            alvos.push_back ({ { cx - 50, yChave - 16, 100, 30 }, aSwitch, slot, inf.nsw, pid ("sw") });
        }
        // ECHO: com SYNC, o TIME vira divisão de tempo (aparece embaixo da chave)
        if (tipo == stomp::Echo && proc.real ("sync") > 0.5f)
            texto (g, u8 ("TIME = ") + stomp::nomeDivisaoDelay (d.k[0] / 10.0f), { r.getX(), yChave + 14, 210, 12 }, corT, fonte (9.0f));
        texto (g, inf.nome, { r.getX() + 6, y0 + 194, 198, 38 }, corT, fontePedal (inf.fonte, inf.fonte == 3 ? 40.0f : inf.fonte == 5 ? 40.0f : 31.0f));
        texto (g, inf.tipo, { r.getX(), y0 + 232, 210, 12 }, corT.withAlpha (0.8f), fonte (9.0f));
        footswitch (g, { cx, y0 + 287 });
        alvos.push_back ({ { cx - 32, y0 + 255, 64, 64 }, aFoot, slot });
        if (registra) alvos.push_back ({ r, aPedal, slot });
    }
    if (! registra) alvos.resize (alvosAntes);   // fantasma do arrasto: não vira alvo
}
