#include "PluginEditor.h"

namespace
{
// paleta do ALIGN: escuro arroxeado; VERDE = saída / filtro, ROXO = marca, sub e envelope, LARANJA = antes do filtro
const juce::Colour cFundoA { 0xff1b1726 }, cFundoB { 0xff110f18 }, cTopoA { 0xff241f33 }, cTopoB { 0xff17141f },
                   cChapa { 0xff1a1725 }, cBorda { 0xff2e2842 }, cTexto { 0xffdcd7ea }, cApagado { 0xff8b84a3 },
                   cTela { 0xff07060c }, cGrade { 0xff181425 }, cGradeF { 0xff2b2442 },
                   cLaranja { 0xffff8a3d }, cVerde { 0xff3fd68a }, cRoxo { 0xffa77bff },
                   cRoxoClaro { 0xffc9b0ff }, cBranco { 0xfff3efff }, cAzul { 0xff4fa3ff };

constexpr int menuLicenca = 1, menuSobre = 2;

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}

float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }

juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::String sinal (float v, int casas)
{
    if (std::abs (v) < (casas ? 0.05f : 0.5f)) return casas ? "0.0" : "0";
    return (v > 0 ? "+" : u8 ("−")) + juce::String (std::abs (v), casas);
}

juce::String hz (float f)
{
    if (f >= 1000.0f) return juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (juce::roundToInt (f)) + " Hz";
}

juce::String ms (float v)
{
    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
    return juce::String (juce::roundToInt (v)) + " ms";
}

juce::String nomeNota (int midi)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (midi < 0) return u8 ("—");
    return juce::String (n[midi % 12]) + juce::String (midi / 12 - 2);   // como no Ableton: nota 60 = C3
}

void led (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour cor, bool aceso)
{
    if (aceso)
    {
        g.setColour (cor.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (r * 4, r * 4).withCentre (c));
        g.setColour (cor);
    }
    else g.setColour (juce::Colour (0xff2c2838));
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
}

void chapa (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (cChapa);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (cBorda.withAlpha (0.5f));
    g.drawLine (r.getX() + 4, r.getY() + 1.5f, r.getRight() - 4, r.getY() + 1.5f, 1.0f);
}

void rotulo (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor = cApagado,
             juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour (cor);
    g.setFont (fonte (10.0f).withExtraKerningFactor (0.18f));
    g.drawText (t, r, j);
}

// título de seção com o LED colorido (como no esboço)
void titulo (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor)
{
    led (g, { r.getX() + 16, r.getY() + 17 }, 4.0f, cor, true);
    g.setColour (cTexto);
    g.setFont (fonte (11.5f, true).withExtraKerningFactor (0.22f));
    g.drawText (t, juce::Rectangle<float> (r.getX() + 28, r.getY() + 9, r.getWidth() - 40, 16), juce::Justification::centredLeft);
}

void legenda (juce::Graphics& g, float& x, float y, juce::Colour cor, const juce::String& t, bool tracejado = false)
{
    g.setColour (cor);
    if (tracejado) { g.fillRect (x, y - 1.0f, 4.0f, 2.0f); g.fillRect (x + 6, y - 1.0f, 4.0f, 2.0f); g.fillRect (x + 12, y - 1.0f, 3.0f, 2.0f); }
    else g.fillRect (x, y - 1.5f, 14.0f, 3.0f);
    const auto f = fonte (10.0f).withExtraKerningFactor (0.12f);
    g.setColour (cTexto); g.setFont (f);
    const float w = largura (f, t) + 2;
    g.drawText (t, juce::Rectangle<float> (x + 18, y - 7, w, 14), juce::Justification::centredLeft);
    x += 18 + w + 12;
}

// desenho das ondas nos botões
juce::Path iconeOnda (int onda, juce::Rectangle<float> r)
{
    juce::Path p;
    const float x0 = r.getX(), w = r.getWidth(), yc = r.getCentreY(), a = r.getHeight() * 0.5f;
    const int n = 40;
    for (int i = 0; i <= n; ++i)
    {
        const float t = (float) i / n * 2.0f;   // dois ciclos
        const float f = t - std::floor (t);
        float v = 0.0f;
        switch (onda)
        {
            case 0: v = std::sin (f * juce::MathConstants<float>::twoPi); break;
            case 1: v = f < 0.25f ? f * 4 : (f < 0.75f ? 2 - f * 4 : f * 4 - 4); break;
            case 2: v = 1.0f - 2.0f * f; break;
            default: v = f < 0.5f ? 1.0f : -1.0f; break;
        }
        const float x = x0 + w * (float) i / n, y = yc - v * a;
        if (i == 0) p.startNewSubPath (x, y);
        else
        {
            if (onda == 2 && f < 0.03f && i > 1) p.lineTo (x, yc - a);   // a descida da serra
            if (onda == 3 && (std::abs (f - 0.5f) < 0.03f || (f < 0.03f && i > 1))) p.lineTo (x, yc - (f < 0.25f ? -a : a));
            p.lineTo (x, y);
        }
    }
    return p;
}
} // namespace

BarebassEditor::BarebassEditor (BarebassProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (700, 424, 2000, 1212);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (700, 2000, w0) : baseW;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));
    startTimerHz (30);
    ativacao.anexar (*this);
}

void BarebassEditor::resized() { proc.uiW = getWidth(); }

void BarebassEditor::timerCallback()
{
    // copia os dois ciclos mais recentes da onda (só enquanto há som; senão a última fica na tela)
    auto& e = proc.engine();
    auto& S = e.scope;
    if (e.leitura.pico.load() > 1.0e-4f)
    {
        const int nm = S.nMarcas.load (std::memory_order_acquire);
        const int64_t esc = S.escrito.load (std::memory_order_acquire);
        if (nm > 1)
        {
            const int k = (nm - 1) % barebass::Scope::marcas;
            const int64_t m = S.marca[k];
            const float per = S.periodo[k];
            const int n = (int) std::lround (2.0 * per);
            if (n > 8 && n < barebass::Scope::size - 4096 && m <= esc && m - n > esc - barebass::Scope::size + 2048)
            {
                float mx = 0.0f;
                for (int j = 0; j < pontos; ++j)
                {
                    const int64_t s = m - n + (int64_t) ((double) j * (n - 1) / (pontos - 1));
                    const int idx = (int) (((s % barebass::Scope::size) + barebass::Scope::size) % barebass::Scope::size);
                    cAntes[j] = S.antes[idx]; cDepois[j] = S.depois[idx]; cSub[j] = S.sub[idx];
                    mx = juce::jmax (mx, std::abs (cAntes[j]), std::abs (cDepois[j]));
                }
                escVis += (juce::jmax (0.04f, mx) - escVis) * 0.35f;
                freqOnda = (float) (e.taxa() / per);
                temOnda = true;
            }
        }
    }
    repaint();
}

juce::RangedAudioParameter* BarebassEditor::param (const juce::String& id) const
{
    return dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
}

float BarebassEditor::real (const juce::String& id) const
{
    if (auto* p = param (id)) return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

juce::String BarebassEditor::texto (const juce::String& id) const
{
    const float v = real (id);
    if (id.endsWith ("oct")) return sinal ((float) juce::roundToInt (v), 0);
    if (id.endsWith ("fine")) return sinal (v, 0) + " c";
    if (id == "cutoff") return hz (v);
    if (id == "envamt") return sinal (v, 0) + " %";
    if (id == "attack" || id == "decay" || id == "release" || id == "glide") return ms (v);
    if (id == "volume") return v <= -35.9f ? u8 ("−∞ dB") : sinal (v, 1) + " dB";
    return juce::String (juce::roundToInt (v)) + " %";
}

void BarebassEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    g.setGradientFill (juce::ColourGradient (cFundoA, 0, 0, cFundoB, 0, (float) baseH, false));
    g.fillRect (juce::Rectangle<float> (0, 0, (float) baseW, (float) baseH));
    knobs.clear();
    botoes.clear();

    topo (g);
    painelOnda (g, { 16, 64, 498, 218 });
    painelCurvaFiltro (g, { 524, 64, 244, 218 });
    painelCurvaEnv (g, { 778, 64, 206, 218 });
    painelOsc (g, { 16, 292, 316, 136 }, 0);
    painelOsc (g, { 342, 292, 316, 136 }, 1);
    painelSub (g, { 668, 292, 316, 136 });
    painelFiltro (g, { 16, 438, 280, 152 });
    painelEnv (g, { 306, 438, 300, 152 });
    painelHunf (g, { 616, 438, 214, 152 });
    painelMaster (g, { 840, 438, 144, 152 });
}

// título com LED que liga / desliga (clique no LED ou no nome)
void BarebassEditor::tituloLiga (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor, const juce::String& idLiga)
{
    const bool ligado = real (idLiga) > 0.5f;
    led (g, { r.getX() + 16, r.getY() + 17 }, 4.5f, cor, ligado);
    g.setColour (juce::Colour (0xff0c0b11));
    g.drawEllipse (juce::Rectangle<float> (11, 11).withCentre ({ r.getX() + 16, r.getY() + 17 }), 1.0f);
    g.setColour (ligado ? cTexto : cApagado);
    const auto f = fonte (11.5f, true).withExtraKerningFactor (0.22f);
    g.setFont (f);
    g.drawText (t, juce::Rectangle<float> (r.getX() + 28, r.getY() + 9, r.getWidth() - 40, 16), juce::Justification::centredLeft);
    rotulo (g, { r.getRight() - 60, r.getY() + 10, 48, 14 }, ligado ? "ON" : "OFF", ligado ? cor : cApagado, juce::Justification::centredRight);
    botoes.push_back ({ { r.getX() + 6, r.getY() + 5, largura (f, t) + 34, 24 }, idLiga, ligado ? 0.0f : 1.0f });
}

void BarebassEditor::apaga (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (cFundoB.withAlpha (0.62f));
    g.fillRoundedRectangle (r.withTrimmedTop (30).reduced (2, 0).withTrimmedBottom (2), 3.0f);
}

void BarebassEditor::topo (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 0, (float) baseW, 54);
    g.setGradientFill (juce::ColourGradient (cTopoA, 0, 0, cTopoB, 0, 54, false));
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 54, (float) baseW, 54, 1.0f);

    // marca: a serra (laranja) entra no filtro e sai redonda (verde)
    {
        juce::Path ps, pv;
        ps.startNewSubPath (16, 33); ps.lineTo (24, 19); ps.lineTo (24, 33); ps.lineTo (32, 19); ps.lineTo (32, 33);
        for (int i = 0; i <= 16; ++i)
        {
            const float x = 34 + i * 1.2f, y = 26 - std::sin (i / 16.0f * 6.2832f) * 7.0f;
            if (i == 0) pv.startNewSubPath (x, y); else pv.lineTo (x, y);
        }
        g.setColour (cLaranja); g.strokePath (ps, juce::PathStrokeType (1.8f));
        g.setColour (cVerde); g.strokePath (pv, juce::PathStrokeType (2.4f));
    }
    const auto fT = fonte (27.0f, true).withExtraKerningFactor (0.22f);
    g.setFont (fT);
    g.setColour (cRoxo);
    g.drawText ("BARE", juce::Rectangle<float> (64, 10, 90, 34), juce::Justification::centredLeft);
    const float wB = largura (fT, "BARE");
    g.setColour (cBranco);
    g.drawText ("BASS", juce::Rectangle<float> (64 + wB, 10, 100, 34), juce::Justification::centredLeft);
    const float wTot = largura (fT, "BAREBASS") - 5;
    g.setGradientFill (juce::ColourGradient (cRoxo, 64, 0, cVerde, 64 + wTot, 0, false));
    g.fillRect (64.0f, 43.0f, wTot, 2.0f);
    rotulo (g, { 64 + wTot + 18, 22, 200, 14 }, "SIMPLE BASS SYNTH");

    // chips da direita
    auto& L = proc.engine().leitura;
    float x = (float) baseW - 16;
    auto chip = [&] (const juce::String& t, juce::Colour corLed, bool aceso, bool comLed)
    {
        const auto f = fonte (11.0f).withExtraKerningFactor (0.14f);
        const float w = largura (f, t) + (comLed ? 34.0f : 22.0f);
        x -= w;
        const juce::Rectangle<float> c (x, 15, w, 24);
        g.setColour (juce::Colour (0xff100e16)); g.fillRoundedRectangle (c, 3.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (c, 3.0f, 1.0f);
        if (comLed) led (g, { c.getX() + 13, c.getCentreY() }, 4.0f, corLed, aceso);
        g.setColour (cTexto); g.setFont (f);
        g.drawText (t, c.withTrimmedLeft (comLed ? 23.0f : 11.0f), juce::Justification::centredLeft);
        x -= 8;
        return c;
    };
    rMenu = chip (u8 ("≡"), cRoxo, false, false);
    rPreset = chip ("PRESET " + u8 ("· ") + proc.getProgramName (proc.preset.load()) + u8 (" ▾"), cRoxo, false, false);
    const int nota = L.nota.load();
    chip ("NOTE " + u8 ("· ") + (L.gate.load() ? nomeNota (nota) : u8 ("—")), cVerde, L.gate.load(), true);
}

void BarebassEditor::painelOnda (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    auto& L = proc.engine().leitura;
    const float yt = r.getY() + 16;
    rotulo (g, { r.getX() + 12, yt - 7, 110, 14 }, u8 ("WAVE · NOW"));
    float lx = r.getX() + 104;
    legenda (g, lx, yt, cLaranja, "PRE-FILTER");
    legenda (g, lx, yt, cVerde, "OUTPUT");
    legenda (g, lx, yt, cRoxo, "SUB", true);
    if (temOnda)
        rotulo (g, { r.getRight() - 152, yt - 7, 140, 14 }, "2 CYCLES " + u8 ("· ") + juce::String (freqOnda, 1) + " Hz", cApagado,
                juce::Justification::centredRight);

    const juce::Rectangle<float> t (r.getX() + 8, r.getY() + 30, r.getWidth() - 16, r.getHeight() - 38);
    g.setColour (cTela); g.fillRoundedRectangle (t, 3.0f);
    for (int i = 1; i < 8; ++i)
    {
        const float x = t.getX() + t.getWidth() * i / 8.0f;
        g.setColour (i == 4 ? cGradeF : cGrade); g.drawLine (x, t.getY(), x, t.getBottom(), 1.0f);
    }
    const float cy = t.getCentreY(), amp = t.getHeight() * 0.40f;
    g.setColour (cGradeF); g.drawLine (t.getX(), cy, t.getRight(), cy, 1.0f);

    const bool tocando = L.gate.load();
    if (temOnda)
    {
        const float alfa = tocando ? 1.0f : 0.45f;
        auto caminho = [&] (const float* v)
        {
            juce::Path p;
            for (int j = 0; j < pontos; ++j)
            {
                const float x = t.getX() + t.getWidth() * j / (pontos - 1);
                const float y = cy - juce::jlimit (-1.1f, 1.1f, v[j] / escVis) * amp;
                if (j == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };
        g.saveState();
        g.reduceClipRegion (t.toNearestInt());
        // saída preenchida até o centro (verde), antes do filtro (laranja fino) e sub (roxo tracejado)
        auto pd = caminho (cDepois);
        juce::Path area (pd);
        area.lineTo (t.getRight(), cy); area.lineTo (t.getX(), cy); area.closeSubPath();
        g.setColour (cVerde.withAlpha (0.13f * alfa)); g.fillPath (area);
        g.setColour (cLaranja.withAlpha (0.8f * alfa));
        g.strokePath (caminho (cAntes), juce::PathStrokeType (1.3f));
        {
            juce::Path ps = caminho (cSub), tr;
            const float dash[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.4f).createDashedStroke (tr, ps, dash, 2);
            g.setColour (cRoxo.withAlpha (0.85f * alfa)); g.fillPath (tr);
        }
        g.setColour (cVerde.withAlpha (0.18f * alfa));
        g.strokePath (pd, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cVerde.withAlpha (alfa));
        g.strokePath (pd, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState();
        rotulo (g, { t.getX() + 10, t.getY() + 8, t.getWidth() - 20, 14 },
                tocando ? "FILTER AT " + hz (L.cutoffAgora.load()) + " NOW (CUTOFF " + hz (real ("cutoff")) + " + ENV)"
                        : juce::String ("LAST NOTE"), cApagado);
    }
    else
    {
        g.setColour (cApagado); g.setFont (fonte (13.0f));
        g.drawText (u8 ("Toque uma nota (MIDI): a onda aparece aqui, antes e depois do filtro."), t, juce::Justification::centred);
    }
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (t, 3.0f, 1.0f);
}

void BarebassEditor::painelCurvaFiltro (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    auto& L = proc.engine().leitura;
    const bool s24 = escolha ("slope") == 1;
    const float yt = r.getY() + 16;
    rotulo (g, { r.getX() + 12, yt - 7, 120, 14 }, u8 ("FILTER · ") + (s24 ? "24 dB" : "12 dB"));
    const float cut = real ("cutoff"), reso = real ("reso") * 0.01f, amt = real ("envamt") * 0.01f;
    const float topoEnv = juce::jlimit (16.0f, 20000.0f, cut * std::pow (2.0f, amt * 6.0f));
    if (std::abs (amt) > 0.01f)
    {
        float lx = r.getRight() - 96;
        legenda (g, lx, yt, cRoxo, "ENV SWEEP", true);
    }

    const juce::Rectangle<float> t (r.getX() + 8, r.getY() + 30, r.getWidth() - 16, r.getHeight() - 38);
    g.setColour (cTela); g.fillRoundedRectangle (t, 3.0f);
    const float lo = std::log (20.0f), hi = std::log (20000.0f);
    auto xDe = [&] (float f) { return t.getX() + (std::log (f) - lo) / (hi - lo) * t.getWidth(); };
    auto yDe = [&] (float d) { return t.getY() + 10 + (18.0f - juce::jlimit (-54.0f, 18.0f, d)) / 72.0f * (t.getHeight() - 22); };
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        g.setColour (cGrade); g.drawLine (xDe (f), t.getY(), xDe (f), t.getBottom(), 1.0f);
    }
    for (float f : { 50.0f, 200.0f, 1000.0f, 5000.0f })
    {
        g.setColour (cApagado.withAlpha (0.7f)); g.setFont (fonte (9.0f));
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (xDe (f) - 15, t.getBottom() - 13, 30, 11), juce::Justification::centred);
    }
    g.setColour (cGradeF); g.drawLine (t.getX(), yDe (0), t.getRight(), yDe (0), 1.0f);

    auto curva = [&] (float fc)
    {
        juce::Path p;
        for (int i = 0; i <= 120; ++i)
        {
            const float x = t.getX() + t.getWidth() * i / 120.0f;
            const float f = std::exp (lo + (hi - lo) * i / 120.0f);
            const float y = yDe ((float) barebass::respostaDb (f, fc, reso, s24));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };
    g.saveState();
    g.reduceClipRegion (t.toNearestInt());
    const bool tocando = L.gate.load() || L.env.load() > 0.01f;
    const float agora = tocando ? L.cutoffAgora.load() : cut;
    // até onde o ENV abre (roxo tracejado) e o corte do knob (apagado)
    if (std::abs (amt) > 0.01f)
    {
        juce::Path tr; const float dash[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.3f).createDashedStroke (tr, curva (topoEnv), dash, 2);
        g.setColour (cRoxo.withAlpha (0.8f)); g.fillPath (tr);
    }
    if (tocando) { g.setColour (cApagado.withAlpha (0.55f)); g.strokePath (curva (cut), juce::PathStrokeType (1.0f)); }
    auto pa = curva (agora);
    juce::Path area (pa);
    area.lineTo (t.getRight(), t.getBottom()); area.lineTo (t.getX(), t.getBottom()); area.closeSubPath();
    g.setGradientFill (juce::ColourGradient (cVerde.withAlpha (0.30f), 0, t.getY(), cVerde.withAlpha (0.04f), 0, t.getBottom(), false));
    g.fillPath (area);
    g.setColour (cVerde); g.strokePath (pa, juce::PathStrokeType (2.0f));
    led (g, { xDe (agora), yDe ((float) barebass::respostaDb (agora, agora, reso, s24)) }, 3.5f, cVerde, true);
    g.restoreState();

    rotulo (g, { t.getX() + 8, t.getY() + 6, t.getWidth() - 16, 14 }, (tocando ? "NOW " : "CUTOFF ") + hz (agora), cVerde);
    if (std::abs (amt) > 0.01f)
        rotulo (g, { t.getX() + 8, t.getY() + 20, t.getWidth() - 16, 14 }, (amt > 0 ? "ENV OPENS TO " : "ENV CLOSES TO ") + hz (topoEnv), cRoxoClaro);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (t, 3.0f, 1.0f);
}

void BarebassEditor::painelCurvaEnv (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    auto& L = proc.engine().leitura;
    const float a = real ("attack") * 0.001f, d = real ("decay") * 0.001f, s = real ("sustain") * 0.01f, rl = real ("release") * 0.001f;
    const float yt = r.getY() + 16;
    rotulo (g, { r.getX() + 12, yt - 7, 100, 14 }, "FILTER ENV");
    rotulo (g, { r.getRight() - 112, yt - 7, 100, 14 }, "DECAY " + ms (d * 1000.0f), cVerde, juce::Justification::centredRight);

    const juce::Rectangle<float> t (r.getX() + 8, r.getY() + 30, r.getWidth() - 16, r.getHeight() - 38);
    g.setColour (cTela); g.fillRoundedRectangle (t, 3.0f);
    const float hold = 0.25f * (a + d + rl) + 0.02f, total = a + d + hold + rl;
    const juce::Rectangle<float> c = t.reduced (10, 0).withTrimmedTop (28).withTrimmedBottom (12);
    auto xDe = [&] (float tt) { return c.getX() + tt / total * c.getWidth(); };
    auto yDe = [&] (float v) { return c.getBottom() - v * c.getHeight(); };
    auto valor = [&] (float tt)
    {
        if (tt < a) return a > 0 ? tt / a : 1.0f;
        if (tt < a + d + hold) return s + (1.0f - s) * std::exp (-4.6f * (tt - a) / juce::jmax (0.0005f, d));
        const float v0 = s + (1.0f - s) * std::exp (-4.6f * (d + hold) / juce::jmax (0.0005f, d));
        return v0 * std::exp (-4.6f * (tt - a - d - hold) / juce::jmax (0.0005f, rl));
    };
    juce::Path p;
    p.startNewSubPath (xDe (0), yDe (0));
    for (int i = 0; i <= 160; ++i)
    {
        const float tt = total * i / 160.0f;
        p.lineTo (xDe (tt), yDe (valor (tt)));
    }
    juce::Path area (p);
    area.lineTo (xDe (total), yDe (0)); area.closeSubPath();
    g.setGradientFill (juce::ColourGradient (cRoxo.withAlpha (0.28f), 0, c.getY(), cRoxo.withAlpha (0.03f), 0, c.getBottom(), false));
    g.fillPath (area);
    g.setColour (cRoxo); g.strokePath (p, juce::PathStrokeType (2.0f));
    g.setColour (cGradeF); g.drawLine (c.getX(), c.getBottom(), c.getRight(), c.getBottom(), 1.0f);
    // nota solta: linha pontilhada onde começa o release
    g.setColour (cGradeF);
    for (float y = c.getY(); y < c.getBottom(); y += 5) g.drawLine (xDe (a + d + hold), y, xDe (a + d + hold), y + 2, 1.0f);
    // DECAY marcado embaixo (verde)
    g.setColour (cVerde); g.fillRect (xDe (a), c.getBottom() + 5, xDe (a + d) - xDe (a), 2.0f);

    // onde a nota está agora
    const float e = L.env.load(), tt = L.envT.load();
    if (L.gate.load()) led (g, { xDe (juce::jmin (tt, a + d + hold)), yDe (e) }, 3.5f, cVerde, true);
    else if (e > 0.01f)
    {
        const float v0 = juce::jmax (0.01f, valor (a + d + hold - 0.0001f));
        const float tr = juce::jlimit (0.0f, rl, -std::log (juce::jmin (1.0f, e / v0)) * rl / 4.6f);
        led (g, { xDe (a + d + hold + tr), yDe (e) }, 3.5f, cVerde, true);
    }
    rotulo (g, { t.getX() + 8, t.getY() + 6, t.getWidth() - 16, 14 },
            "A " + ms (a * 1000) + u8 (" · S ") + juce::String (juce::roundToInt (s * 100)) + u8 ("% · R ") + ms (rl * 1000), cRoxoClaro);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (t, 3.0f, 1.0f);
}

void BarebassEditor::knob (juce::Graphics& g, const juce::String& id, juce::Point<float> c, float raio, juce::Colour cor, bool bipolar)
{
    auto* p = param (id);
    if (p == nullptr) return;
    knobs.push_back ({ id, c, raio });
    const float v = p->getValue();
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float av = a0 + (a1 - a0) * v, am = a0 + (a1 - a0) * (bipolar ? 0.5f : 0.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d283c), c.x, c.y - raio, juce::Colour (0xff15121d), c.x, c.y + raio, false));
    g.fillEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c));
    g.setColour (juce::Colours::black); g.drawEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c), 1.0f);
    const float ra = raio + 5.0f;
    juce::Path tr; tr.addCentredArc (c.x, c.y, ra, ra, 0, a0, a1, true);
    g.setColour (cGradeF); g.strokePath (tr, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path va; va.addCentredArc (c.x, c.y, ra, ra, 0, juce::jmin (am, av), juce::jmax (am, av), true);
    g.setColour (cor); g.strokePath (va, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const juce::Point<float> d (std::sin (av), -std::cos (av));
    g.setColour (cBranco);
    g.drawLine (c.x + d.x * raio * 0.25f, c.y + d.y * raio * 0.25f, c.x + d.x * raio * 0.85f, c.y + d.y * raio * 0.85f, 2.2f);
}

void BarebassEditor::knobRotulado (juce::Graphics& g, const juce::String& id, const juce::String& nome, juce::Point<float> c, float raio,
                                   juce::Colour cor, bool bipolar, bool destaque)
{
    if (destaque)
    {
        g.setColour (cor.withAlpha (0.10f));
        g.fillEllipse (juce::Rectangle<float> ((raio + 11) * 2, (raio + 11) * 2).withCentre (c));
    }
    knob (g, id, c, raio, cor, bipolar);
    const float y = c.y + raio + 9;
    g.setColour (destaque ? cBranco : cTexto);
    g.setFont (fonte (10.0f, true).withExtraKerningFactor (0.16f));
    g.drawText (nome, juce::Rectangle<float> (c.x - 50, y, 100, 12), juce::Justification::centred);
    g.setColour (destaque ? cor : cApagado);
    g.setFont (fonte (11.0f, destaque));
    g.drawText (texto (id), juce::Rectangle<float> (c.x - 50, y + 13, 100, 13), juce::Justification::centred);
}

void BarebassEditor::seletorOnda (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& id, int n, juce::Colour cor)
{
    const int atual = escolha (id);
    g.setColour (cTela); g.fillRoundedRectangle (r, 3.0f);
    const float w = r.getWidth() / n;
    for (int i = 0; i < n; ++i)
    {
        const juce::Rectangle<float> b (r.getX() + i * w, r.getY(), w, r.getHeight());
        botoes.push_back ({ b, id, (float) i });
        const bool sel = i == atual;
        if (sel)
        {
            g.setColour (cor.withAlpha (0.22f)); g.fillRect (b.reduced (1));
            g.setColour (cor); g.fillRect (b.getX() + 1, b.getBottom() - 3, b.getWidth() - 2, 2.0f);
        }
        if (i > 0) { g.setColour (cBorda); g.drawLine (b.getX(), b.getY() + 3, b.getX(), b.getBottom() - 3, 1.0f); }
        const int onda = n == 2 ? (i == 0 ? 0 : 3) : i;
        g.setColour (sel ? cBranco : cApagado);
        g.strokePath (iconeOnda (onda, b.withSizeKeepingCentre (juce::jmin (40.0f, w - 16), 12)), juce::PathStrokeType (sel ? 1.8f : 1.4f));
    }
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void BarebassEditor::painelOsc (juce::Graphics& g, juce::Rectangle<float> r, int qual)
{
    chapa (g, r);
    const juce::String p = qual == 0 ? "o1" : "o2";
    tituloLiga (g, r, qual == 0 ? "OSC 1" : "OSC 2", cLaranja, p + "on");
    seletorOnda (g, { r.getX() + 12, r.getY() + 32, r.getWidth() - 24, 26 }, p + "wave", 4, cVerde);
    const float y = r.getY() + 84, w = (r.getWidth() - 24) / 3.0f;
    knobRotulado (g, p + "oct", "OCTAVE", { r.getX() + 12 + w * 0.5f, y }, 13.0f, cLaranja, true);
    knobRotulado (g, p + "fine", "FINE", { r.getX() + 12 + w * 1.5f, y }, 13.0f, cLaranja, true);
    knobRotulado (g, p + "level", "LEVEL", { r.getX() + 12 + w * 2.5f, y }, 13.0f, cLaranja, false);
    if (real (p + "on") < 0.5f) apaga (g, r);
}

void BarebassEditor::painelSub (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    tituloLiga (g, r, "SUB", cRoxo, "subon");
    seletorOnda (g, { r.getX() + 12, r.getY() + 32, r.getWidth() - 24, 26 }, "subwave", 2, cRoxo);
    const float y = r.getY() + 84, w = (r.getWidth() - 24) / 2.0f;
    knobRotulado (g, "suboct", "OCTAVE", { r.getX() + 12 + w * 0.5f, y }, 13.0f, cRoxo, false);
    knobRotulado (g, "sublevel", "LEVEL", { r.getX() + 12 + w * 1.5f, y }, 13.0f, cRoxo, false);
    if (real ("subon") < 0.5f) apaga (g, r);
}

void BarebassEditor::painelFiltro (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    titulo (g, r, u8 ("FILTER · LOW PASS"), cVerde);
    // SLOPE: 12 / 24 dB
    rotulo (g, { r.getX() + 12, r.getY() + 46, 60, 12 }, "SLOPE");
    const int sl = escolha ("slope");
    for (int i = 0; i < 2; ++i)
    {
        const juce::Rectangle<float> b (r.getX() + 12, r.getY() + 62 + (1 - i) * 26.0f, 58, 24);
        botoes.push_back ({ b, "slope", (float) i });
        const bool sel = sl == i;
        g.setColour (sel ? cVerde.withAlpha (0.20f) : cTela); g.fillRoundedRectangle (b, 3.0f);
        g.setColour (sel ? cVerde : cBorda); g.drawRoundedRectangle (b, 3.0f, 1.0f);
        g.setColour (sel ? cBranco : cApagado); g.setFont (fonte (11.5f, sel));
        g.drawText (i == 0 ? "12 dB" : "24 dB", b, juce::Justification::centred);
    }
    knobRotulado (g, "cutoff", "CUTOFF", { r.getX() + 112, r.getY() + 84 }, 21.0f, cVerde, false, true);
    knobRotulado (g, "reso", "RESONANCE", { r.getX() + 178, r.getY() + 78 }, 14.0f, cVerde, false);
    knobRotulado (g, "envamt", "ENV", { r.getX() + 240, r.getY() + 84 }, 19.0f, cVerde, true, true);
}

void BarebassEditor::painelEnv (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    titulo (g, r, "FILTER ENV", cRoxo);
    const float w = (r.getWidth() - 16) / 4.0f, x0 = r.getX() + 8;
    knobRotulado (g, "attack", "ATTACK", { x0 + w * 0.5f, r.getY() + 74 }, 14.0f, cRoxo, false);
    knobRotulado (g, "decay", "DECAY", { x0 + w * 1.5f, r.getY() + 84 }, 22.0f, cVerde, false, true);
    knobRotulado (g, "sustain", "SUSTAIN", { x0 + w * 2.5f, r.getY() + 74 }, 14.0f, cRoxo, false);
    knobRotulado (g, "release", "RELEASE", { x0 + w * 3.5f, r.getY() + 74 }, 14.0f, cRoxo, false);
}

void BarebassEditor::painelHunf (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    auto& L = proc.engine().leitura;
    titulo (g, r, "unHUNF", cRoxo);
    {
        const juce::Rectangle<float> c (r.getRight() - 70, r.getY() + 9, 58, 16);
        g.setColour (cVerde.withAlpha (0.12f)); g.fillRoundedRectangle (c, 3.0f);
        rotulo (g, c, u8 ("10–50 Hz"), cVerde, juce::Justification::centred);
    }
    const float h = real ("hunf") * 0.01f;
    // gráfico: nível de um sub de 30 Hz sem unHUNF (tracejado, 0 dB) e com unHUNF (roxo), nos primeiros 500 ms da nota.
    // Escala em dB (0 a -24): em linear a queda de poucos dB ficava colada na linha de cima e a curva sumia.
    const juce::Rectangle<float> t (r.getX() + 10, r.getY() + 30, r.getWidth() - 20, 44);
    g.setColour (cTela); g.fillRoundedRectangle (t, 3.0f);
    const float jan = 0.5f, pisoDb = -24.0f;
    auto xDe = [&] (float tt) { return t.getX() + 4 + tt / jan * (t.getWidth() - 8); };
    auto yDe = [&] (float v) { const float db = juce::jlimit (pisoDb, 0.0f, 20.0f * std::log10 (juce::jmax (1.0e-4f, v)));
                               return t.getY() + 7 + db / pisoDb * (t.getHeight() - 12); };
    {
        juce::Path sem; sem.startNewSubPath (xDe (0), yDe (1)); sem.lineTo (xDe (jan), yDe (1));
        juce::Path tr; const float dash[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (tr, sem, dash, 2);
        g.setColour (cApagado.withAlpha (0.7f)); g.fillPath (tr);
    }
    juce::Path com;
    com.startNewSubPath (xDe (0), yDe (1));
    for (int i = 1; i <= 80; ++i)
    {
        const float tt = jan * i / 80.0f;
        com.lineTo (xDe (tt), yDe ((float) barebass::hunfGanho (h, tt)));
    }
    juce::Path area (com);
    area.lineTo (xDe (jan), yDe (1)); area.closeSubPath();
    g.setColour (cRoxo.withAlpha (0.18f)); g.fillPath (area);
    g.setColour (cRoxo); g.strokePath (com, juce::PathStrokeType (1.8f));
    if (L.gate.load() && h > 0.001f && L.envT.load() < jan)
        led (g, { xDe (L.envT.load()), yDe ((float) barebass::hunfGanho (h, L.envT.load())) }, 3.0f, cVerde, true);
    g.setFont (fonte (9.5f)); g.setColour (cApagado);
    g.drawText (h > 0.001f ? u8 ("- - sub sem unHUNF (embola)") : u8 ("unHUNF desligado"), t.reduced (6, 0).withTrimmedBottom (4), juce::Justification::bottomRight);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (t, 3.0f, 1.0f);

    knobRotulado (g, "hunf", "unHUNF", { r.getX() + 46, r.getY() + 101 }, 14.0f, cRoxo, false, true);
    // tempo em que o grave cai e o medidor de redução
    g.setColour (cTexto); g.setFont (fonte (10.0f, true).withExtraKerningFactor (0.14f));
    g.drawText ("SUB DECAY", juce::Rectangle<float> (r.getX() + 86, r.getY() + 94, 90, 12), juce::Justification::centredLeft);
    g.setColour (h > 0.001f ? cVerde : cApagado); g.setFont (fonte (12.0f, true));
    g.drawText (h > 0.001f ? ms ((float) barebass::hunfTau (h) * 1000.0f) : u8 ("—"),
                juce::Rectangle<float> (r.getX() + 86, r.getY() + 108, 90, 14), juce::Justification::centredLeft);
    g.setColour (cApagado); g.setFont (fonte (9.5f));
    g.drawText (u8 ("só no grave"), juce::Rectangle<float> (r.getX() + 86, r.getY() + 124, 90, 12), juce::Justification::centredLeft);
    const juce::Rectangle<float> m (r.getRight() - 22, r.getY() + 88, 8, 50);
    g.setColour (cTela); g.fillRect (m);
    const float gr = juce::jlimit (0.0f, 24.0f, -L.hunfGr.load());
    g.setColour (cRoxo); g.fillRect (m.withHeight (m.getHeight() * gr / 24.0f));
    g.setColour (juce::Colours::black); g.drawRect (m, 1.0f);
    rotulo (g, { m.getX() - 10, m.getBottom() + 1, 28, 12 }, "GR", cApagado, juce::Justification::centred);
}

void BarebassEditor::painelMaster (juce::Graphics& g, juce::Rectangle<float> r)
{
    chapa (g, r);
    titulo (g, r, "MASTER", cAzul);
    // MONO / LEGATO à vista (antes era um chip no topo que só mudava clicando)
    const int modo = escolha ("mode");
    const float bw = (r.getWidth() - 24) / 2.0f;
    for (int i = 0; i < 2; ++i)
    {
        const juce::Rectangle<float> b (r.getX() + 12 + i * bw, r.getY() + 34, bw, 22);
        botoes.push_back ({ b, "mode", (float) i });
        const bool sel = modo == i;
        g.setColour (sel ? cRoxo.withAlpha (0.22f) : cTela); g.fillRect (b);
        if (sel) { g.setColour (cRoxo); g.fillRect (b.getX() + 1, b.getBottom() - 3, b.getWidth() - 2, 2.0f); }
        g.setColour (sel ? cBranco : cApagado); g.setFont (fonte (10.5f, sel).withExtraKerningFactor (0.12f));
        g.drawText (i == 0 ? "MONO" : "LEGATO", b, juce::Justification::centred);
    }
    g.setColour (juce::Colours::black); g.drawRect (juce::Rectangle<float> (r.getX() + 12, r.getY() + 34, bw * 2, 22), 1.0f);
    g.setColour (cBorda); g.drawLine (r.getX() + 12 + bw, r.getY() + 37, r.getX() + 12 + bw, r.getY() + 53, 1.0f);
    knobRotulado (g, "volume", "VOLUME", { r.getX() + 28, r.getY() + 96 }, 15.0f, cAzul, false, true);
    knobRotulado (g, "glide", "GLIDE", { r.getX() + 74, r.getY() + 96 }, 12.0f, cRoxo, false);
    knobRotulado (g, "velo", "VELO", { r.getX() + 118, r.getY() + 96 }, 12.0f, cRoxo, false);
}

void BarebassEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 7)
        {
            if (auto* prm = param (k.id))
            {
                arrastando = k.id;
                valorIni = prm->getValue();
                yIni = p.y;
                prm->beginChangeGesture();
            }
            return;
        }
    for (auto& b : botoes)
        if (b.r.contains (p)) { proc.define (b.id, b.valor); repaint(); return; }
    if (rPreset.contains (p)) { menuPreset(); return; }
    if (rMenu.contains (p) || e.mods.isPopupMenu()) menu();
}

void BarebassEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (arrastando.isEmpty()) return;
    if (auto* prm = param (arrastando))
    {
        const float dy = yIni - e.position.y / escala();
        const float sens = e.mods.isShiftDown() ? 800.0f : 180.0f;
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, valorIni + dy / sens));
        repaint();
    }
}

void BarebassEditor::mouseUp (const juce::MouseEvent&)
{
    if (arrastando.isNotEmpty())
        if (auto* prm = param (arrastando)) prm->endChangeGesture();
    arrastando.clear();
}

void BarebassEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 7)
            if (auto* prm = param (k.id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->getDefaultValue());
                prm->endChangeGesture();
                repaint();
                return;
            }
}

void BarebassEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 7)
            if (auto* prm = param (k.id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * 0.05f));
                prm->endChangeGesture();
                repaint();
                return;
            }
}

void BarebassEditor::menuPreset()
{
    juce::PopupMenu m;
    const int atual = proc.preset.load();
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == atual);
    juce::Component::SafePointer<BarebassEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         safe->proc.setCurrentProgram (r - 1);
                         safe->repaint();
                     });
}

void BarebassEditor::menu()
{
    juce::PopupMenu m;
    m.addItem (menuSobre, "Como funciona");
    m.addItem (menuLicenca, u8 ("Licença…"));
    m.addSeparator();
   #ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    m.addItem (-1, "BAREBASS " + juce::String (JucePlugin_VersionString), false, false);
   #endif
    juce::Component::SafePointer<BarebassEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == menuLicenca) safe->ativacao.mostrarInfo();
                         if (r == menuSobre)
                             juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "BAREBASS",
                                 u8 ("Synth de baixo de uma voz: toca uma nota por vez (a última que você apertar).\n\n"
                                     "OSC 1, OSC 2 e SUB: escolha a onda, a oitava e o volume de cada um. FINE desafina de leve "
                                     "para engordar o som. O LED do título liga e desliga cada um. A cada nota a onda recomeça "
                                     "do ponto inicial: todas batem igual.\n\n"
                                     "FILTER: passa-baixa de 12 ou 24 dB. CUTOFF é onde ele corta; ENV é o quanto o envelope "
                                     "abre o filtro a cada nota. FILTER ENV (A D S R): o DECAY curto deixa o baixo seco.\n\n"
                                     "unHUNF: depois do filtro, deixa o sub (10 a 50 Hz) bater no começo da nota e some com ele. "
                                     "Quanto mais alto, mais rápido e mais fundo. Em 0 fica desligado.\n\n"
                                     "MONO redispara o envelope a cada nota; LEGATO não redispara quando as notas se encostam. "
                                     "GLIDE desliza de uma nota para a outra. VELO: quanto a força da nota (velocity) muda "
                                     "o volume e o quanto o ENV abre; em 0 toda nota sai igual."));
                     });
}
