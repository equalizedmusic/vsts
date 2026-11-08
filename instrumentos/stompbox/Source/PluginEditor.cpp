#include "PluginEditor.h"
#include "Desenho.h"

using namespace desenho;
using stomp::infoPedal;

namespace
{
const juce::Colour corFundo (0xff163533), corTexto (0xffcfe3dd), corSuave (0xff8fbdb2), corRotulo (0xff9fc9bf), corAcento (0xff4f8d81);
}

StompboxEditor::StompboxEditor (StompboxProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (960, 540, 2400, 1350);
    }
    const int w = proc.uiW.load() > 0 ? proc.uiW.load() : 1280;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));
    startTimerHz (30);
    ativacao.anexar (*this);
}

void StompboxEditor::resized()
{
    proc.uiW = getWidth();
    cenaSuja = true;
}

int StompboxEditor::totalPosicoes() const
{
    const int n = proc.numPedais();
    return juce::jmin (stomp::numSlots, n + 1);
}

void StompboxEditor::rola (int delta)
{
    const int r = juce::jlimit (0, maxRolagem(), proc.rolagem.load() + delta);
    if (r != proc.rolagem.load()) { proc.rolagem = r; sujo(); }
}

juce::Rectangle<float> StompboxEditor::rectSlot (int i) const { return { 78.0f + (float) i * 240.0f, 80.0f, 210.0f, 400.0f }; }

// --------------------------------------------------------------------------------------------------------------

void StompboxEditor::timerCallback()
{
    // muda algum parâmetro (do DAW, de automação ou da tela)? refaz a cena
    double soma = proc.rolagem.load() * 7.0 + (proc.afinador.load() ? 3.0 : 0.0) + proc.ab * 11.0 + proc.presetAtual * 13.0;
    int i = 1;
    for (auto* p : proc.getParameters()) soma += (double) p->getValue() * (double) (i++ % 97 + 1);
    soma += (double) proc.nomePreset.hashCode() * 1e-9;
    soma += std::floor (proc.bpm.load() + 0.5) * 1e-3;   // o BPM aparece no chip do SYNC
    if (soma != somaParams) { somaParams = soma; cenaSuja = true; }
    if (proc.rolagem.load() > maxRolagem()) proc.rolagem = maxRolagem();

    const float pi = proc.picoIn.exchange (0.0f), po = proc.picoOut.exchange (0.0f);
    vIn = juce::jmax (pi, vIn * 0.85f); vOut = juce::jmax (po, vOut * 0.85f);
    // afinador: sempre analisa (o indicador da entrada fica sempre ligado); 15 Hz basta
    if ((++tick & 1) == 0 || proc.afinador.load()) analisaAfinador();

    // arrastando um pedal até a borda do board: rola uma posição a cada ~0,3 s (com 7+ pedais)
    if (arrastandoPedal)
    {
        const int lado = xPedal < 110.0f ? -1 : xPedal > 1490.0f ? 1 : 0;
        if (lado == 0) tickBorda = 0;
        else if (tickBorda++ % 10 == 0) { rola (lado); atualizaDestino(); }
    }
    if (cenaSuja || arrastandoPedal || arrastandoKnob) { repaint(); return; }
    // só o que mexe sozinho: medidores, LED do gate e afinador
    repintaBase ({ 226.0f, 582.0f, 18.0f, 158.0f });
    repintaBase ({ 1512.0f, 696.0f, 18.0f, 158.0f });
    repintaBase ({ 46.0f, 826.0f, 150.0f, 50.0f });
    repintaBase ({ 128.0f, 566.0f, 96.0f, 104.0f });
    if (proc.afinador.load()) repintaBase ({ 396.0f, 146.0f, 808.0f, 308.0f });
}

void StompboxEditor::repintaBase (juce::Rectangle<float> r)
{
    repaint ((r * escala()).getSmallestIntegerContainer().expanded (2));
}

// --------------------------------------------------------------------------------------------------------------
// camadas guardadas

juce::Image StompboxEditor::fazCamada (juce::Rectangle<float> area, const std::function<void (juce::Graphics&)>& desenha) const
{
    const int w = juce::jmax (1, (int) std::ceil (area.getWidth() * camadasEscala)), h = juce::jmax (1, (int) std::ceil (area.getHeight() * camadasEscala));
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (camadasEscala));
    desenha (g);
    return img;
}

void StompboxEditor::pinta (juce::Graphics& g, const juce::Image& img, juce::Point<float> origem) const
{
    // drawImage usa a opacidade da cor atual: sem isto, um setColour com alfa feito antes (LED aceso, fundo de 3%
    // da célula do seletor) deixava o knob/pedal esmaecido ou quase invisível.
    juce::Graphics::ScopedSaveState ss (g);
    g.setColour (juce::Colours::black.withAlpha (opacCamada));
    g.drawImageTransformed (img, juce::AffineTransform::scale (1.0f / camadasEscala).translated (origem));
}

void StompboxEditor::limpaCamadas()
{
    imgFundo = {};
    for (auto& i : imgPedal) i = {};
    for (auto& i : imgAmp) i = {};
    for (auto& i : imgCab) i = {};
    imgKnob.clear();
    if (! grao.isValid()) { grao = fazGrao (0.06f); graoTolex = fazGrao (0.10f); }
}

const juce::Image& StompboxEditor::camadaPedal (int tipo)
{
    tipo = juce::jlimit (1, stomp::numTipos - 1, tipo);
    auto& img = imgPedal[tipo];
    if (! img.isValid()) img = fazCamada ({ -150.0f, -400.0f, 300.0f, 440.0f }, [&] (juce::Graphics& g) { corpoPedal (g, tipo, { 0.0f, 0.0f }); });
    return img;
}

const juce::Image& StompboxEditor::camadaKnob (int estilo, float diam)
{
    const int chave = estilo * 10000 + juce::roundToInt (diam * 10.0f);
    auto it = imgKnob.find (chave);
    if (it != imgKnob.end()) return it->second;
    const float m = diam * 0.5f + 9.0f;
    return imgKnob[chave] = fazCamada ({ -m, -m, m * 2.0f, m * 2.0f }, [&] (juce::Graphics& g) { knobBase (g, { 0.0f, 0.0f }, diam, estilo); });
}

void StompboxEditor::paint (juce::Graphics& g)
{
    const float s = escala();
    const float dpi = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    if (cenaSuja || ! cena.isValid() || std::abs (cenaEscala - s * dpi) > 0.001f)
    {
        cenaEscala = s * dpi;
        if (std::abs (camadasEscala - cenaEscala) > 0.001f) { camadasEscala = cenaEscala; limpaCamadas(); }
        const int w = juce::roundToInt (baseW * cenaEscala), h = juce::roundToInt (baseH * cenaEscala);
        if (! cena.isValid() || cena.getWidth() != w || cena.getHeight() != h) cena = juce::Image (juce::Image::RGB, w, h, false);
        juce::Graphics gc (cena);
        gc.addTransform (juce::AffineTransform::scale (cenaEscala));
        alvos.clear();
        desenhaCena (gc);
        cenaSuja = false;
    }
    g.drawImageTransformed (cena, juce::AffineTransform::scale ((float) getWidth() / (float) cena.getWidth(), (float) getHeight() / (float) cena.getHeight()));

    g.addTransform (juce::AffineTransform::scale (s));
    // medidores, LED do gate e afinador da entrada (por cima da cena)
    medidor (g, { 230.0f, 586.0f, 10.0f, 150.0f }, vIn);
    medidor (g, { 1516.0f, 700.0f, 10.0f, 150.0f }, vOut);
    {
        const bool desl = proc.real ("gate") <= -89.5f;
        const bool aberto = proc.gateAberto.load();
        g.setColour (juce::Colour (0xff10201f)); g.fillRoundedRectangle (juce::Rectangle<float> (52.0f, 834.0f, 136.0f, 34.0f), 6.0f);
        led (g, { 70.0f, 851.0f }, 5.5f, ! desl && aberto, juce::Colour (0xff28d15a));
        texto (g, desl ? "GATE OFF" : aberto ? "ABERTO" : "FECHADO", { 84.0f, 840.0f, 100.0f, 22.0f }, corRotulo, fonte (11.0f), juce::Justification::centredLeft);
    }
    miniAfinador (g);
    if (arrastandoPedal && pedalArrastado >= 0)
    {
        const auto d = proc.le (pedalArrastado);
        // o pedal segue o mouse (um pouco acima/abaixo do trilho, sem sair do board)
        const float yBase = juce::jlimit (440.0f, 520.0f, 480.0f + (yPedal - yIni));
        opacCamada = 0.92f;
        desenhaPedal (g, d.tipo, { xPedal, yBase }, d, pedalArrastado, false);
        opacCamada = 1.0f;
        // a linha amarela (por cima do pedal) fica no vão onde ele vai cair; no próprio lugar não aparece
        const int vao = destinoPedal - proc.rolagem.load();
        if (destinoPedal >= 0 && destinoFinal() != pedalArrastado && vao >= 0 && vao <= visiveis)
        {
            const float x = rectSlot (vao).getX() - 15.0f;
            g.setColour (juce::Colour (0x55ffd24a)); g.fillRoundedRectangle (x - 7, 116.0f, 14.0f, 368.0f, 7.0f);
            g.setColour (juce::Colour (0xffffd24a)); g.fillRoundedRectangle (x - 2, 120.0f, 4.0f, 360.0f, 2.0f);
        }
    }
    if (proc.afinador.load()) desenhaAfinador (g);
    if (arrastandoKnob) bolhaValor (g);
}

void StompboxEditor::fundo (juce::Graphics& g)
{
    // verde-petróleo com trama fina (um caminho só)
    g.setColour (corFundo); g.fillRect (0, 0, baseW, baseH);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x593c7870), 800.0f, 300.0f, juce::Colour (0x003c7870), 800.0f, 900.0f, true));
    g.fillRect (0, 0, baseW, baseH);
    {
        juce::Path trama;
        for (int x = -900; x < baseW; x += 4) { trama.startNewSubPath ((float) x, 0.0f); trama.lineTo ((float) x + 900.0f, 900.0f); }
        g.setColour (juce::Colours::black.withAlpha (0.05f)); g.strokePath (trama, juce::PathStrokeType (1.0f));
    }
    // trilhos do pedalboard
    const juce::Rectangle<float> trilhos (46, 94, 1508, 404);
    sombra (g, trilhos, 8.0f, 14.0f, 12.0f, 0.6f);
    for (float y = trilhos.getY(); y < trilhos.getBottom(); y += 44.0f)
    {
        g.setColour (juce::Colour (0xff1b1d1f)); g.fillRect (trilhos.getX(), y, trilhos.getWidth(), juce::jmin (34.0f, trilhos.getBottom() - y));
        g.setColour (juce::Colour (0xff0b0c0d)); g.fillRect (trilhos.getX(), y + 34.0f, trilhos.getWidth(), juce::jmin (10.0f, juce::jmax (0.0f, trilhos.getBottom() - y - 34.0f)));
        g.setColour (juce::Colours::white.withAlpha (0.05f)); g.drawLine (trilhos.getX(), y + 0.5f, trilhos.getRight(), y + 0.5f, 1.0f);
    }
    g.setColour (juce::Colour (0xff0a0b0c)); g.drawRoundedRectangle (trilhos, 8.0f, 2.0f);
}

void StompboxEditor::desenhaCena (juce::Graphics& g)
{
    if (! imgFundo.isValid()) imgFundo = fazCamada ({ 0.0f, 0.0f, (float) baseW, (float) baseH }, [this] (juce::Graphics& gf) { fundo (gf); });
    pinta (g, imgFundo, { 0.0f, 0.0f });
    boardArea (g);
    embaixo (g);
    topo (g);
    if (seletorAberto) seletor (g);
}

// --------------------------------------------------------------------------------------------------------------
// topo

void StompboxEditor::botao (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, TipoAlvo tipo, int idx, float f)
{
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff3f8c7c) : juce::Colour (0xff2c4a46), r.getX(), r.getY(),
                                             on ? juce::Colour (0xff2a6458) : juce::Colour (0xff1a302d), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff061210)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    texto (g, t, r, on ? juce::Colours::white : corTexto, fonte (f));
    alvos.push_back ({ r, tipo, -1, idx });
}

void StompboxEditor::topo (juce::Graphics& g)
{
    const juce::Rectangle<float> barra (0, 0, (float) baseW, 58);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0f2422), 0, 0, juce::Colour (0xff0b1b1a), 0, 58, false));
    g.fillRect (barra);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRect (0.0f, 58.0f, (float) baseW, 3.0f);
    texto (g, "STOMPBOX", { 24, 6, 260, 32 }, juce::Colour (0xffe9efe9), fonte (26.0f, true, "Arial Black"), juce::Justification::centredLeft);
    texto (g, "INSIDESTUDIOSP", { 26, 34, 200, 14 }, juce::Colour (0xff6fa79d), fonte (9.5f), juce::Justification::centredLeft);

    botao (g, { 300, 13, 34, 32 }, u8 ("\xe2\x97\x80"), false, aPrev);
    const juce::Rectangle<float> lcd (340, 11, 320, 36);
    g.setColour (juce::Colour (0xff081010)); g.fillRect (lcd);
    g.setColour (juce::Colours::black); g.drawRect (lcd, 1.0f);
    const juce::String num = proc.presetAtual >= 0 ? juce::String (proc.presetAtual + 1).paddedLeft ('0', 2) : "US";
    texto (g, num, lcd.withTrimmedLeft (12).withWidth (28), juce::Colour (0xff4f9f88), fonte (12.0f, true, "Consolas"), juce::Justification::centredLeft);
    texto (g, proc.nomePreset, lcd.withTrimmedLeft (42), juce::Colour (0xff9ff3d8), fonte (17.0f, true, "Consolas"), juce::Justification::centredLeft);
    alvos.push_back ({ lcd, aLcd });
    botao (g, { 666, 13, 34, 32 }, u8 ("\xe2\x96\xb6"), false, aNext);
    botao (g, { 706, 13, 62, 32 }, "SAVE", false, aSave);
    botao (g, { 774, 13, 52, 32 }, proc.ab == 0 ? "A" : "B", proc.ab == 1, aAB, -1, 14.0f);
    texto (g, "A/B", { 830, 13, 30, 32 }, corSuave, fonte (10.0f), juce::Justification::centredLeft);

    const bool sync = proc.real ("sync") > 0.5f;
    const juce::Rectangle<float> chip (1170, 15, 170, 28);
    g.setColour (sync ? juce::Colour (0xff2a6458) : juce::Colours::transparentBlack); g.fillRoundedRectangle (chip, 14.0f);
    g.setColour (sync ? juce::Colour (0xff6fd1bd) : juce::Colour (0xff2f5a53)); g.drawRoundedRectangle (chip, 14.0f, 1.0f);
    texto (g, u8 ("\xe2\x99\xa9 SYNC ") + juce::String (juce::roundToInt (proc.bpm.load())) + " BPM", chip, sync ? juce::Colours::white : corRotulo, fonte (11.0f));
    alvos.push_back ({ chip, aSync });
    botao (g, { 1352, 13, 80, 32 }, "TUNER", proc.afinador.load(), aTuner);
    botao (g, { 1440, 13, 50, 32 }, "HQ", proc.real ("hq") > 0.5f, aHq);
    botao (g, { 1498, 13, 78, 32 }, u8 ("\xe2\x89\xa1 MENU"), false, aMenu, -1, 11.0f);
}

// --------------------------------------------------------------------------------------------------------------
// o board

static juce::Point<float> posJack (int tipo, juce::Rectangle<float> slot, bool saida)
{
    const auto& inf = infoPedal (tipo);
    const float cx = slot.getCentreX(), fundo = slot.getBottom();
    if (inf.forma == 1) return { saida ? cx + 128.0f : cx - 128.0f, fundo - 140.0f };
    const float topo = fundo - (inf.forma == 2 ? 360.0f : 340.0f);
    return { saida ? slot.getRight() + 4.0f : slot.getX() - 4.0f, topo + 69.0f };
}

void StompboxEditor::boardArea (juce::Graphics& g)
{
    const int rol = proc.rolagem.load(), n = proc.numPedais(), total = totalPosicoes();
    // setas
    for (int lado = 0; lado < 2; ++lado)
    {
        const juce::Rectangle<float> r (lado == 0 ? 10.0f : 1560.0f, 258.0f, 30.0f, 60.0f);
        const bool pode = lado == 0 ? rol > 0 : rol < maxRolagem();
        g.setColour (juce::Colours::black.withAlpha (pode ? 0.45f : 0.18f)); g.fillRoundedRectangle (r, 6.0f);
        texto (g, lado == 0 ? u8 ("\xe2\x80\xb9") : u8 ("\xe2\x80\xba"), r, corTexto.withAlpha (pode ? 1.0f : 0.3f), fonte (26.0f));
        alvos.push_back ({ r, lado == 0 ? aSetaE : aSetaD });
    }

    // cabos primeiro (os pedais ficam por cima das pontas)
    std::vector<std::pair<juce::Point<float>, juce::Point<float>>> jacks;
    for (int i = 0; i < visiveis; ++i)
    {
        const int pos = rol + i;
        if (pos >= n) break;
        if (arrastandoPedal && pos == pedalArrastado) { jacks.push_back ({ { -1, -1 }, { -1, -1 } }); continue; }
        const int t = proc.le (pos).tipo;
        jacks.push_back ({ posJack (t, rectSlot (i), false), posJack (t, rectSlot (i), true) });
    }
    juce::Point<float> ant (0.0f, 280.0f);
    for (size_t i = 0; i < jacks.size(); ++i)
    {
        if (jacks[i].first.x < 0) continue;
        cabo (g, ant + juce::Point<float> (i == 0 && rol == 0 ? 10.0f : 0.0f, 0), jacks[i].first - juce::Point<float> (14, 0), i == 0 ? 10.0f : 46.0f);
        if (i > 0) plugue (g, ant, 1);
        plugue (g, jacks[i].first, -1);
        ant = jacks[i].second + juce::Point<float> (14, 0);
        plugue (g, jacks[i].second - juce::Point<float> (14, 0), 1);
    }
    if (! jacks.empty() && rol + (int) jacks.size() >= n) cabo (g, ant, { 1590.0f, ant.y + 20.0f }, 10.0f);

    // pedais
    for (int i = 0; i < visiveis; ++i)
    {
        const int pos = rol + i;
        if (pos >= total) break;
        const auto r = rectSlot (i);
        if (pos < n)
        {
            const auto d = proc.le (pos);
            const auto& inf = infoPedal (d.tipo);
            // cabeçalho: número, o que é, alça e remover
            const juce::Rectangle<float> cab (r.getX(), r.getY() - 2, r.getWidth(), 20);
            g.setColour (juce::Colour (0xff0d2220)); g.fillEllipse (cab.getCentreX() - 70, cab.getY() + 1, 18, 18);
            texto (g, juce::String (pos + 1), { cab.getCentreX() - 70, cab.getY() + 1, 18, 18 }, corTexto, fonte (11.0f));
            textoCabe (g, inf.tipo, cab.withTrimmedLeft (54).withTrimmedRight (d.on ? 26 : 82), juce::Colour (0xff7fb3a8), fonte (10.5f), juce::Justification::centredLeft);
            if (! d.on)
            {
                const juce::Rectangle<float> tag (cab.getRight() - 72, cab.getY() + 3, 46, 15);
                g.setColour (juce::Colour (0xff3a2a14)); g.fillRoundedRectangle (tag, 3.0f);
                texto (g, "BYPASS", tag, juce::Colour (0xffffc46a), fonte (9.0f));
            }
            texto (g, u8 ("\xe2\x8b\xae\xe2\x8b\xae"), { cab.getRight() - 22, cab.getY(), 20, 20 }, juce::Colour (0xff5f8f86), fonte (14.0f));
            texto (g, u8 ("\xe2\x9c\x95"), { cab.getX(), cab.getY(), 20, 20 }, juce::Colour (0xff5f8f86), fonte (12.0f));
            alvos.push_back ({ { cab.getX(), cab.getY(), 22, 22 }, aRemove, pos });
            alvos.push_back ({ cab.withTrimmedLeft (24).withHeight (22), aPedal, pos });   // a faixa do nome e a alça ⋮⋮ pegam o pedal
            if (arrastandoPedal && pos == pedalArrastado)
            {
                g.setColour (juce::Colour (0xff3f7068));
                juce::Path tr; tr.addRoundedRectangle (r.withTrimmedTop (60), 12.0f);
                const float tracos[] = { 8.0f, 6.0f }; juce::Path tp; juce::PathStrokeType (2.0f).createDashedStroke (tp, tr, tracos, 2);
                g.fillPath (tp);
                continue;
            }
            desenhaPedal (g, d.tipo, { r.getCentreX(), r.getBottom() }, d, pos, true);
        }
        else   // posição livre
        {
            const auto e = r.withTrimmedTop (60);
            g.setColour (juce::Colours::black.withAlpha (0.12f)); g.fillRoundedRectangle (e, 12.0f);
            juce::Path tr; tr.addRoundedRectangle (e, 12.0f);
            const float tracos[] = { 8.0f, 6.0f }; juce::Path tp; juce::PathStrokeType (2.0f).createDashedStroke (tp, tr, tracos, 2);
            g.setColour (juce::Colour (0xff3f7068)); g.fillPath (tp);
            texto (g, "+", e.withHeight (60).withCentre (e.getCentre().translated (0, -16)), juce::Colour (0xff7fb3a8), fonte (48.0f, false));
            texto (g, "ADICIONAR PEDAL", e.withHeight (20).withCentre (e.getCentre().translated (0, 26)), juce::Colour (0xff7fb3a8), fonte (12.0f));
            alvos.push_back ({ e, aVazio, pos });
        }
    }

    texto (g, u8 ("GUITARRA \xe2\x9e\x9c sinal em s\xc3\xa9rie, da esquerda para a direita \xe2\x9e\x9c AMP / CAB"), { 80, 504, 700, 16 }, corSuave, fonte (11.0f),
           juce::Justification::centredLeft);
    const int ultimo = juce::jmin (total, rol + visiveis);
    texto (g, u8 ("POSI\xc3\x87\xc3\x95""ES ") + juce::String (rol + 1) + u8 ("\xe2\x80\x93") + juce::String (ultimo) + " de " + juce::String (total)
              + u8 (" \xc2\xb7 arraste o pedal para trocar a ordem \xc2\xb7 bot\xc3\xa3o direito: trocar / remover"),
           { 800, 504, 720, 16 }, corSuave, fonte (11.0f), juce::Justification::centredRight);
    const juce::Rectangle<float> barra (78, 526, 1444, 8);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (barra, 4.0f);
    const float frac = juce::jmin (1.0f, (float) visiveis / (float) juce::jmax (1, total));
    const float x0 = maxRolagem() > 0 ? (float) rol / (float) maxRolagem() * (1.0f - frac) : 0.0f;
    g.setColour (corAcento); g.fillRoundedRectangle (barra.getX() + barra.getWidth() * x0, barra.getY(), barra.getWidth() * frac, 8.0f, 4.0f);
    alvos.push_back ({ barra.expanded (0, 6), aBarra });
}
