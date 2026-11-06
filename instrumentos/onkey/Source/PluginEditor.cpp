#include "PluginEditor.h"

namespace
{
// paleta do ALIGN: escuro arroxeado; VERDE = corrigido / ligado, ROXO = marca, tom e escala, LARANJA = voz original
const juce::Colour cFundoA { 0xff1b1726 }, cFundoB { 0xff110f18 }, cTopoA { 0xff241f33 }, cTopoB { 0xff17141f },
                   cChapa { 0xff1a1725 }, cBorda { 0xff2e2842 }, cTexto { 0xffdcd7ea }, cApagado { 0xff8b84a3 },
                   cTela { 0xff07060c }, cGrade { 0xff181425 }, cGradeF { 0xff2b2442 },
                   cOriginal { 0xffff8a3d }, cVerde { 0xff3fd68a }, cRoxo { 0xffa77bff },
                   cRoxoClaro { 0xffc9b0ff }, cBranco { 0xfff3efff }, cRuim { 0xffff4f8b };

constexpr int menuSobre = 1;
constexpr float segGrafico = 4.0f;

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}

float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }

juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::String cents (float v)
{
    const int c = juce::roundToInt (v);
    if (c == 0) return u8 ("±0 c");
    return (c > 0 ? "+" : u8 ("−")) + juce::String (std::abs (c)) + " c";
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

void legenda (juce::Graphics& g, float& x, float y, juce::Colour cor, const juce::String& t, bool faixa = false)
{
    if (faixa) { g.setColour (cor.withAlpha (0.35f)); g.fillRect (x, y - 4.0f, 14.0f, 8.0f); }
    else { g.setColour (cor); g.fillRect (x, y - 1.5f, 14.0f, 3.0f); }
    g.setColour (cTexto);
    g.setFont (fonte (10.0f).withExtraKerningFactor (0.12f));
    const float w = largura (fonte (10.0f).withExtraKerningFactor (0.12f), t) + 2;
    g.drawText (t, juce::Rectangle<float> (x + 18, y - 7, w, 14), juce::Justification::centredLeft);
    x += 18 + w + 12;
}

juce::String nomeTom (int tom, int esc)
{
    if (esc == onkey::escCromatica) return "CHROMATIC";
    if (tom < 0 || esc < 0) return u8 ("—");
    return juce::String (onkey::nomePc (tom)) + " " + onkey::nomeEscala (esc);
}

juce::String nota (int midi)
{
    char b[8];
    return onkey::nomeNota (midi, b);
}
} // namespace

OnkeyEditor::OnkeyEditor (OnkeyProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (700, 434, 2000, 1240);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (700, 2000, w0) : baseW;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));
    startTimerHz (30);
}

void OnkeyEditor::resized() { proc.uiW = getWidth(); }

void OnkeyEditor::timerCallback() { repaint(); }

juce::RangedAudioParameter* OnkeyEditor::param (const juce::String& id) const
{
    return dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
}

float OnkeyEditor::real (const juce::String& id) const
{
    if (auto* p = param (id)) return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

void OnkeyEditor::tomAtual (int& tom, int& esc) const
{
    auto& L = proc.engine().leitura;
    const int r = escolha ("root"), s = escolha ("scale");
    const int det = L.tomDet.load();
    tom = r > 0 ? r - 1 : det;
    esc = s > 0 ? s - 1 : (det >= 0 ? (L.modoDet.load() ? onkey::escMenor : onkey::escMaior) : -1);
    if (tom < 0 && esc >= 0 && esc != onkey::escCromatica) esc = -1;
}

void OnkeyEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    const juce::Rectangle<float> tudo (0, 0, (float) baseW, (float) baseH);
    g.setGradientFill (juce::ColourGradient (cFundoA, 0, 0, cFundoB, 0, (float) baseH, false));
    g.fillRect (tudo);
    knobs.clear();

    topo (g);

    const juce::Rectangle<float> caixa (16, 68, 704, 446);
    chapa (g, caixa);
    const bool fino = proc.fino.load();
    const float hFino = 150.0f;
    grafico (g, { caixa.getX() + 8, caixa.getY() + 30, caixa.getWidth() - 16, caixa.getHeight() - 38 - (fino ? hFino + 8 : 0.0f) });
    if (fino) painelFino (g, { caixa.getX() + 8, caixa.getBottom() - 8 - hFino, caixa.getWidth() - 16, hFino });

    const float xl = 734, wl = 250;
    painelTom (g, { xl, 68, wl, 124 });
    painelEscolha (g, { xl, 202, wl, 78 });
    painelNota (g, { xl, 290, wl, 132 });
    painelBotaoFino (g, { xl, 432, wl, 82 });

    base (g);
}

void OnkeyEditor::topo (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 0, (float) baseW, 54);
    g.setGradientFill (juce::ColourGradient (cTopoA, 0, 0, cTopoB, 0, 54, false));
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 54, (float) baseW, 54, 1.0f);

    // marca: a voz chega torta (laranja) e assenta reta no tom (verde), com a nota em roxo
    {
        juce::Path pw, pr;
        for (int i = 0; i <= 20; ++i)
        {
            const float x = 18 + i * 0.9f, t = i / 20.0f;
            const float y = 27 - std::sin (t * 6.2832f * 1.6f) * 8.0f * (1.0f - t);
            if (i == 0) pw.startNewSubPath (x, y); else pw.lineTo (x, y);
        }
        pr.startNewSubPath (36, 27); pr.lineTo (52, 27);
        g.setColour (cOriginal); g.strokePath (pw, juce::PathStrokeType (2.0f));
        g.setColour (cVerde); g.strokePath (pr, juce::PathStrokeType (2.4f));
        g.setColour (cRoxo); g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre ({ 54, 27 }));
    }
    g.setFont (fonte (27.0f, true).withExtraKerningFactor (0.22f));
    g.setColour (cRoxo);
    g.drawText ("ON", juce::Rectangle<float> (66, 10, 50, 34), juce::Justification::centredLeft);
    const float wOn = largura (fonte (27.0f, true).withExtraKerningFactor (0.22f), "ON");
    g.setColour (cBranco);
    g.drawText ("KEY", juce::Rectangle<float> (66 + wOn, 10, 90, 34), juce::Justification::centredLeft);
    const float wTot = largura (fonte (27.0f, true).withExtraKerningFactor (0.22f), "ONKEY") - 5;
    g.setGradientFill (juce::ColourGradient (cRoxo, 66, 0, cVerde, 66 + wTot, 0, false));
    g.fillRect (66.0f, 43.0f, wTot, 2.0f);
    rotulo (g, { 66 + wTot + 18, 22, 200, 14 }, "AUTO VOCAL TUNING");

    // chips da direita
    auto& L = proc.engine().leitura;
    const bool ligado = proc.ligado();
    const int estado = L.estado.load();
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
    const bool manual = escolha ("root") > 0 && escolha ("scale") > 0;
    juce::String st; juce::Colour cs = cApagado; bool aceso = true;
    if (! ligado)             { st = "BYPASS"; aceso = false; }
    else if (manual)          { st = "MANUAL KEY"; cs = cRoxo; }
    else if (estado == 0)     { st = "WAITING FOR VOICE"; aceso = false; }
    else if (estado == 1)     { st = "LISTENING"; cs = cRoxo; }
    else                      { st = "KEY LOCKED"; cs = cVerde; }
    chip (st, cs, aceso, true);
    chip ("VOICE", cVerde, L.voz.load(), true);
}

void OnkeyEditor::grafico (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& e = proc.engine();
    auto& L = e.leitura;
    auto& H = e.hist;

    // linha de cima: rótulo e legendas
    const float yt = r.getY() - 15;
    rotulo (g, { r.getX(), yt - 7, 200, 14 }, u8 ("PITCH · LAST 4 s"));
    float lx = r.getX() + 150;
    legenda (g, lx, yt, cOriginal, "ORIGINAL");
    legenda (g, lx, yt, cVerde, "ONKEY");
    legenda (g, lx, yt, cRoxo, "SCALE", true);
    legenda (g, lx, yt, cRoxoClaro, "ROOT", true);

    g.setColour (cTela);
    g.fillRoundedRectangle (r, 3.0f);

    // janela: os últimos 4 s do histórico
    const double spq = e.segPorQuadro();
    const int nVis = juce::jlimit (50, onkey::Historico::size - 8, (int) std::lround (segGrafico / spq));
    const int pos = H.pos.load (std::memory_order_acquire);

    // centro vertical: a média do que saiu no último segundo, andando devagar
    {
        double s = 0; int c = 0;
        const int k = juce::jmin (nVis, (int) std::lround (1.0 / spq));
        for (int i = 1; i <= k && pos - i >= 0; ++i)
        {
            const float v = H.saida[(pos - i) % onkey::Historico::size];
            if (v > 0) { s += v; ++c; }
        }
        if (c > 5) centroVis += ((float) (s / c) - centroVis) * 0.08f;
    }
    const float span = 13.0f;
    const float lo = centroVis - span * 0.5f, hi = centroVis + span * 0.5f;
    auto yDe = [&] (float m) { return r.getBottom() - (m - lo) / span * r.getHeight(); };

    int tom = -1, esc = -1;
    tomAtual (tom, esc);
    const unsigned mask = esc >= 0 ? onkey::mascaraTom (juce::jmax (0, tom), esc) : 0u;

    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    // faixas: notas da escala em roxo, tônica mais forte; linha fina no centro de cada semitom
    for (int k = (int) std::floor (lo) - 1; k <= (int) std::ceil (hi) + 1; ++k)
    {
        const int pc = ((k % 12) + 12) % 12;
        const bool naEscala = (mask >> pc) & 1u;
        const bool raiz = naEscala && tom >= 0 && pc == tom && esc != onkey::escCromatica;
        const float y0 = yDe (k + 0.5f), y1 = yDe (k - 0.5f);
        if (naEscala)
        {
            g.setColour ((raiz ? cRoxoClaro : cRoxo).withAlpha (raiz ? 0.13f : 0.065f));
            g.fillRect (r.getX(), y0, r.getWidth(), y1 - y0);
        }
        g.setColour (naEscala ? cGradeF : cGrade);
        g.drawLine (r.getX() + 40, yDe ((float) k), r.getRight(), yDe ((float) k), 1.0f);
        g.setFont (fonte (10.0f, raiz));
        g.setColour (raiz ? cRoxoClaro : (naEscala ? cTexto : cApagado.withAlpha (0.6f)));
        g.drawText (nota (k), juce::Rectangle<float> (r.getX() + 6, yDe ((float) k) - 7, 34, 14), juce::Justification::centredLeft);
    }
    // grade de tempo: uma linha por segundo
    for (int s = 1; s < (int) segGrafico; ++s)
    {
        const float x = r.getX() + 40 + (r.getWidth() - 40) * (float) s / segGrafico;
        g.setColour (cGrade); g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
    }

    // curvas
    auto curva = [&] (const float* v, juce::Colour cor, float lw, bool brilho)
    {
        juce::Path p;
        bool aberto = false;
        float ant = 0;
        for (int i = 0; i < nVis; ++i)
        {
            const int idx = pos - nVis + i;
            if (idx < 0) continue;
            const float m = v[idx % onkey::Historico::size];
            const float x = r.getX() + 40 + (r.getWidth() - 40) * (float) i / (float) (nVis - 1);
            if (m <= 0 || (aberto && std::abs (m - ant) > 3.0f)) { aberto = false; if (m <= 0) continue; }
            const float y = yDe (m);
            if (! aberto) { p.startNewSubPath (x, y); aberto = true; } else p.lineTo (x, y);
            ant = m;
        }
        if (brilho)
        {
            g.setColour (cor.withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (lw * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (cor);
        g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    curva (H.entrada, cOriginal.withAlpha (0.85f), 1.5f, false);
    if (proc.ligado()) curva (H.saida, cVerde, 2.4f, true);

    // agora: ponto na ponta direita
    if (pos > 0)
    {
        const float m = proc.ligado() ? H.saida[(pos - 1) % onkey::Historico::size] : H.entrada[(pos - 1) % onkey::Historico::size];
        if (m > 0) led (g, { r.getRight() - 2, yDe (m) }, 3.5f, proc.ligado() ? cVerde : cOriginal, true);
    }
    g.restoreState();

    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    if (L.estado.load() == 0)
    {
        g.setColour (cApagado); g.setFont (fonte (13.0f));
        g.drawText (u8 ("Toque a voz: o ONKEY acha o tom sozinho."), r.withTrimmedLeft (40), juce::Justification::centred);
    }
}

void OnkeyEditor::knob (juce::Graphics& g, const juce::String& id, juce::Point<float> c, float raio, juce::Colour cor, bool bipolar)
{
    auto* p = param (id);
    if (p == nullptr) return;
    knobs.push_back ({ id, c, raio });
    const float v = p->getValue();
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float av = a0 + (a1 - a0) * v, am = a0 + (a1 - a0) * (bipolar ? 0.5f : 0.0f);

    // corpo
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d283c), c.x, c.y - raio, juce::Colour (0xff15121d), c.x, c.y + raio, false));
    g.fillEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c));
    g.setColour (juce::Colours::black); g.drawEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c), 1.0f);
    // trilho e valor
    const float ra = raio + 5.0f;
    juce::Path tr; tr.addCentredArc (c.x, c.y, ra, ra, 0, a0, a1, true);
    g.setColour (cGradeF); g.strokePath (tr, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path va; va.addCentredArc (c.x, c.y, ra, ra, 0, juce::jmin (am, av), juce::jmax (am, av), true);
    g.setColour (cor); g.strokePath (va, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // ponteiro
    const juce::Point<float> d (std::sin (av), -std::cos (av));
    g.setColour (cBranco);
    g.drawLine (c.x + d.x * raio * 0.25f, c.y + d.y * raio * 0.25f, c.x + d.x * raio * 0.85f, c.y + d.y * raio * 0.85f, 2.2f);
}

void OnkeyEditor::painelFino (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff141120)); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (cRoxo.withAlpha (0.45f)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    rotulo (g, { r.getX() + 12, r.getY() + 6, 200, 14 }, "AJUSTE FINO", cRoxoClaro);
    rotulo (g, { r.getRight() - 212, r.getY() + 6, 200, 14 }, u8 ("DUPLO CLIQUE = PADRÃO"), cApagado, juce::Justification::centredRight);

    struct K { const char* id; const char* nome; const char* desc; bool bip; juce::Colour cor; };
    const K ks[5] = {
        { "speed", "RETUNE SPEED", "rapidez com que a nota chega no tom", false, cVerde },
        { "humanize", "HUMANIZE", "deixa passar pequenas variações", false, cVerde },
        { "vibrato", "VIBRATO", "100% = vibrato como foi cantado", false, cVerde },
        { "formant", "FORMANT", "timbre da voz (0 = natural)", true, cRoxo },
        { "output", "OUTPUT", "volume de saída", true, cRoxo } };
    const float w = r.getWidth() / 5.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float cx = r.getX() + w * (i + 0.5f);
        rotulo (g, { cx - w / 2, r.getY() + 24, w, 14 }, ks[i].nome, cTexto, juce::Justification::centred);
        knob (g, ks[i].id, { cx, r.getY() + 66 }, 21.0f, ks[i].cor, ks[i].bip);
        const float v = real (ks[i].id);
        juce::String t;
        if (juce::String (ks[i].id) == "formant") t = (std::abs (v) < 0.05f ? u8 ("0.0") : (v > 0 ? "+" : u8 ("−")) + juce::String (std::abs (v), 1)) + " st";
        else if (juce::String (ks[i].id) == "output") t = (std::abs (v) < 0.05f ? u8 ("0.0") : (v > 0 ? "+" : u8 ("−")) + juce::String (std::abs (v), 1)) + " dB";
        else t = juce::String (juce::roundToInt (v)) + "%";
        g.setColour (cTexto); g.setFont (fonte (12.0f, true));
        g.drawText (t, juce::Rectangle<float> (cx - w / 2, r.getY() + 96, w, 16), juce::Justification::centred);
        g.setColour (cApagado); g.setFont (fonte (10.5f));
        g.drawFittedText (juce::String::fromUTF8 (ks[i].desc), juce::Rectangle<int> ((int) (cx - w / 2 + 8), (int) r.getY() + 114, (int) w - 16, 30),
                          juce::Justification::centredTop, 2);
    }
}

void OnkeyEditor::painelTom (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& L = proc.engine().leitura;
    chapa (g, r);
    const bool autoT = escolha ("root") == 0, autoE = escolha ("scale") == 0;
    rotulo (g, { r.getX() + 12, r.getY() + 8, 200, 14 }, (autoT || autoE) ? u8 ("KEY · AUTO") : u8 ("KEY · MANUAL"));
    int tom = -1, esc = -1;
    tomAtual (tom, esc);
    const bool sabe = esc >= 0;
    g.setColour (sabe ? cRoxoClaro : cApagado);
    g.setFont (fonte (sabe ? 26.0f : 30.0f, true));
    g.drawText (nomeTom (tom, esc), juce::Rectangle<float> (r.getX() + 12, r.getY() + 22, r.getWidth() - 24, 34), juce::Justification::centredLeft);

    juce::String sub;
    if (! (autoT || autoE)) sub = u8 ("ESCOLHIDO EM ROOT / SCALE");
    else if (L.tomDet.load() >= 0) sub = "DETECTED " + u8 ("· ") + juce::String (juce::roundToInt (L.confianca.load() * 100)) + "% MATCH";
    else if (L.estado.load() == 0) sub = "WAITING FOR VOICE";
    else sub = "LISTENING " + u8 ("· ") + juce::String (L.ouvido.load(), 1) + u8 (" s · CHROMATIC FOR NOW");
    rotulo (g, { r.getX() + 12, r.getY() + 58, r.getWidth() - 20, 14 }, sub);

    // as 12 notas: tônica roxa, notas da escala verdes
    const unsigned mask = sabe ? onkey::mascaraTom (juce::jmax (0, tom), esc) : 0u;
    const float cw = (r.getWidth() - 24) / 12.0f;
    for (int i = 0; i < 12; ++i)
    {
        const juce::Rectangle<float> c (r.getX() + 12 + i * cw, r.getY() + 80, cw - 2, 30);
        const bool na = (mask >> i) & 1u, raiz = na && i == tom && esc != onkey::escCromatica;
        const bool preta = i == 1 || i == 3 || i == 6 || i == 8 || i == 10;
        g.setColour (raiz ? cRoxo : (na ? cVerde.withAlpha (0.75f) : juce::Colour (preta ? 0xff0e0c14 : 0xff1f1b2b)));
        g.fillRoundedRectangle (c, 2.0f);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (c, 2.0f, 1.0f);
        g.setColour (na ? juce::Colour (0xff120f1a) : cApagado);
        g.setFont (fonte (8.5f, na));
        g.drawText (onkey::nomePc (i), c.withTrimmedTop (14), juce::Justification::centred);
    }
}

void OnkeyEditor::painelEscolha (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& L = proc.engine().leitura;
    chapa (g, r);
    const float w = (r.getWidth() - 34) / 2.0f;
    rRoot = { r.getX() + 12, r.getY() + 30, w, 32 };
    rScale = { rRoot.getRight() + 10, r.getY() + 30, w, 32 };
    rotulo (g, { rRoot.getX(), r.getY() + 10, w, 14 }, "ROOT");
    rotulo (g, { rScale.getX(), r.getY() + 10, w, 14 }, "SCALE");
    const int rt = escolha ("root"), sc = escolha ("scale"), det = L.tomDet.load();
    const juce::String tR = rt == 0 ? (det >= 0 ? "AUTO " + u8 ("· ") + onkey::nomePc (det) : juce::String ("AUTO")) : OnkeyProcessor::nomesTom()[rt];
    const juce::String tS = sc == 0 ? (det >= 0 ? "AUTO " + u8 ("· ") + (L.modoDet.load() ? "MIN" : "MAJ") : juce::String ("AUTO"))
                                    : OnkeyProcessor::nomesEscala()[sc];
    for (int i = 0; i < 2; ++i)
    {
        const auto b = i == 0 ? rRoot : rScale;
        const bool manual = (i == 0 ? rt : sc) > 0;
        g.setColour (juce::Colour (0xff100e16)); g.fillRoundedRectangle (b, 3.0f);
        g.setColour (manual ? cRoxo : cBorda); g.drawRoundedRectangle (b, 3.0f, 1.0f);
        g.setColour (manual ? cRoxoClaro : cTexto); g.setFont (fonte (12.0f, manual));
        g.drawFittedText (i == 0 ? tR : tS, b.reduced (9, 0).withTrimmedRight (12).toNearestInt(), juce::Justification::centredLeft, 1);
        juce::Path tri;
        const float tx = b.getRight() - 14, ty = b.getCentreY();
        tri.addTriangle (tx - 4, ty - 2, tx + 4, ty - 2, tx, ty + 3);
        g.setColour (cApagado); g.fillPath (tri);
    }
}

void OnkeyEditor::painelNota (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& L = proc.engine().leitura;
    chapa (g, r);
    rotulo (g, { r.getX() + 12, r.getY() + 8, 200, 14 }, "NOTE NOW");
    const bool voz = L.voz.load();
    const int n = L.nota.load();
    const bool ligado = proc.ligado();
    g.setColour (voz && n >= 0 ? (ligado ? cVerde : cOriginal) : cApagado);
    g.setFont (fonte (30.0f, true));
    g.drawText (voz && n >= 0 ? nota (n) : u8 ("—"), juce::Rectangle<float> (r.getX() + 12, r.getY() + 22, 90, 36), juce::Justification::centredLeft);

    const float desv = L.desvio.load(), corr = ligado ? L.correcao.load() : 0.0f;
    const juce::String linhas[2][2] = { { "SUNG", voz ? cents (desv) : u8 ("—") }, { "CORRECTION", voz && ligado ? cents (corr) : u8 ("—") } };
    for (int i = 0; i < 2; ++i)
    {
        const float y = r.getY() + 24 + i * 18.0f;
        rotulo (g, { r.getX() + 104, y, 80, 16 }, linhas[i][0]);
        g.setColour (i == 0 ? cOriginal : cVerde);
        g.setFont (fonte (12.5f, true));
        g.drawText (linhas[i][1], juce::Rectangle<float> (r.getX() + 160, y, r.getWidth() - 172, 16), juce::Justification::centredRight);
    }

    // medidor −50..+50 cents: laranja = como foi cantado, verde = onde saiu
    const juce::Rectangle<float> b (r.getX() + 12, r.getY() + 74, r.getWidth() - 24, 22);
    g.setColour (cTela); g.fillRoundedRectangle (b, 2.0f);
    for (int c = -50; c <= 50; c += 10)
    {
        const float x = b.getX() + (c + 50) / 100.0f * b.getWidth();
        g.setColour (c == 0 ? cRoxo : cGradeF);
        g.fillRect (x - (c == 0 ? 1.0f : 0.5f), b.getY() + (c == 0 ? 0.0f : 7.0f), c == 0 ? 2.0f : 1.0f, b.getHeight() - (c == 0 ? 0.0f : 14.0f));
    }
    if (voz)
    {
        auto marca = [&] (float c, juce::Colour cor, float w)
        {
            const float x = b.getX() + (juce::jlimit (-50.0f, 50.0f, c) + 50) / 100.0f * b.getWidth();
            g.setColour (cor.withAlpha (0.25f)); g.fillRect (x - w * 1.5f, b.getY() + 2, w * 3, b.getHeight() - 4);
            g.setColour (cor); g.fillRect (x - w / 2, b.getY() + 2, w, b.getHeight() - 4);
        };
        marca (desv, cOriginal, 2.0f);
        if (ligado) marca (desv + corr, cVerde, 3.0f);
    }
    g.setFont (fonte (10.0f)); g.setColour (cApagado);
    g.drawText (u8 ("−50 c"), juce::Rectangle<float> (b.getX(), b.getBottom() + 3, 50, 12), juce::Justification::centredLeft);
    g.drawText ("+50 c", juce::Rectangle<float> (b.getRight() - 50, b.getBottom() + 3, 50, 12), juce::Justification::centredRight);
    g.setColour (cRoxoClaro);
    g.drawText ("IN TUNE", juce::Rectangle<float> (b.getCentreX() - 40, b.getBottom() + 3, 80, 12), juce::Justification::centred);
}

void OnkeyEditor::painelBotaoFino (juce::Graphics& g, juce::Rectangle<float> r)
{
    const bool aberto = proc.fino.load();
    rFino = r;
    chapa (g, r);
    if (aberto) { g.setColour (cRoxo.withAlpha (0.10f)); g.fillRoundedRectangle (r, 4.0f); g.setColour (cRoxo.withAlpha (0.6f)); g.drawRoundedRectangle (r, 4.0f, 1.0f); }
    rotulo (g, { r.getX() + 12, r.getY() + 8, 160, 14 }, "AJUSTE FINO", aberto ? cRoxoClaro : cApagado);
    rotulo (g, { r.getRight() - 112, r.getY() + 8, 100, 14 }, aberto ? u8 ("FECHAR ▴") : u8 ("ABRIR ▾"), aberto ? cRoxoClaro : cTexto,
            juce::Justification::centredRight);
    auto fmt = [] (float v, int casas, const char* un) { return (std::abs (v) < 0.05f ? u8 ("0") + (casas ? ".0" : "") : (v > 0 ? "+" : u8 ("−")) + juce::String (std::abs (v), casas)) + un; };
    g.setFont (fonte (11.0f)); g.setColour (cTexto);
    g.drawText ("SPEED " + juce::String (juce::roundToInt (real ("speed"))) + u8 (" · HUMANIZE ") + juce::String (juce::roundToInt (real ("humanize")))
                    + u8 (" · VIBRATO ") + juce::String (juce::roundToInt (real ("vibrato"))),
                juce::Rectangle<float> (r.getX() + 12, r.getY() + 30, r.getWidth() - 24, 16), juce::Justification::centredLeft);
    g.drawText ("FORMANT " + fmt (real ("formant"), 1, " st") + u8 (" · OUTPUT ") + fmt (real ("output"), 1, " dB"),
                juce::Rectangle<float> (r.getX() + 12, r.getY() + 50, r.getWidth() - 24, 16), juce::Justification::centredLeft);
}

void OnkeyEditor::base (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 528, (float) baseW, 92);
    g.setGradientFill (juce::ColourGradient (cTopoB, 0, r.getY(), juce::Colour (0xff0f0d15), 0, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::black); g.drawLine (0, r.getY(), (float) baseW, r.getY(), 1.0f);

    const bool ligado = proc.ligado();
    rBotao = { 20, r.getY() + 17, 190, 58 };
    if (ligado)
    {
        g.setColour (cVerde.withAlpha (0.18f)); g.fillRoundedRectangle (rBotao.expanded (6), 9.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c7a52), 0, rBotao.getY(), juce::Colour (0xff1b4d34), 0, rBotao.getBottom(), false));
    }
    else g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35304a), 0, rBotao.getY(), juce::Colour (0xff25212f), 0, rBotao.getBottom(), false));
    g.fillRoundedRectangle (rBotao, 5.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (rBotao, 5.0f, 1.0f);
    g.setColour ((ligado ? cVerde : cRoxo).withAlpha (0.35f));
    g.drawLine (rBotao.getX() + 5, rBotao.getY() + 1.5f, rBotao.getRight() - 5, rBotao.getY() + 1.5f, 1.0f);
    led (g, { rBotao.getX() + 34, rBotao.getCentreY() }, 6.0f, cVerde, ligado);
    g.setColour (ligado ? juce::Colour (0xffeafff2) : cApagado);
    g.setFont (fonte (22.0f, true).withExtraKerningFactor (0.28f));
    g.drawText ("ONKEY", rBotao.withTrimmedLeft (50), juce::Justification::centredLeft);

    // AMOUNT
    rotulo (g, { 228, r.getY() + 6, 76, 14 }, "AMOUNT", cTexto, juce::Justification::centred);
    knob (g, "amount", { 266, r.getY() + 46 }, 19.0f, cVerde, false);
    g.setColour (cTexto); g.setFont (fonte (11.5f, true));
    g.drawText (juce::String (juce::roundToInt (real ("amount"))) + "%", juce::Rectangle<float> (228, r.getY() + 72, 76, 14), juce::Justification::centred);

    // ajuda
    auto& L = proc.engine().leitura;
    int tom = -1, esc = -1;
    tomAtual (tom, esc);
    const bool manual = escolha ("root") > 0 && escolha ("scale") > 0;
    juce::String t1, t2; juce::Colour c1 = cTexto;
    if (! ligado)
    {
        t1 = u8 ("Desligado: o som passa sem nenhuma alteração.");
        t2 = u8 ("O ONKEY continua ouvindo e achando o tom. Aperte ONKEY para ligar.");
    }
    else if (L.estado.load() == 0 && ! manual)
    {
        t1 = u8 ("Esperando a voz.");
        t2 = u8 ("Ponha o ONKEY no canal da voz (uma voz só). Ele acha o tom e a escala sozinho e leva ao tom o que sair dele.");
    }
    else if (esc < 0)
    {
        t1 = u8 ("Ouvindo para achar o tom…"); c1 = cRoxoClaro;
        t2 = u8 ("Enquanto isso, corrige só o que está entre duas notas. Se já souber o tom, escolha em ROOT e SCALE.");
    }
    else
    {
        t1 = u8 ("Afinando em ") + nomeTom (tom, esc) + "."; c1 = cVerde;
        t2 = u8 ("Vibrato, slides e respiração passam como foram cantados. Mais natural ou mais marcado: abra o AJUSTE FINO.");
    }
    g.setColour (c1); g.setFont (fonte (12.5f, true));
    g.drawText (t1, juce::Rectangle<float> (322, r.getY() + 18, 470, 18), juce::Justification::centredLeft);
    g.setColour (cApagado); g.setFont (fonte (12.0f));
    g.drawFittedText (t2, juce::Rectangle<int> (322, (int) r.getY() + 38, 470, 36), juce::Justification::topLeft, 2);

    g.setFont (fonte (10.0f).withExtraKerningFactor (0.15f));
    g.setColour (cApagado);
    const int lat = juce::roundToInt (1000.0 * proc.engine().latencia() / juce::jmax (1.0, proc.engine().taxa()));
    g.drawText ("LATENCY " + juce::String (lat) + " ms", juce::Rectangle<float> (800, r.getY() + 28, 184, 14), juce::Justification::centredRight);
    g.drawText ("FORMANTS PRESERVED", juce::Rectangle<float> (800, r.getY() + 46, 184, 14), juce::Justification::centredRight);
}

void OnkeyEditor::mouseDown (const juce::MouseEvent& e)
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
    if (rBotao.contains (p)) { proc.alterna(); repaint(); return; }
    if (rRoot.contains (p)) { menuEscolha (true); return; }
    if (rScale.contains (p)) { menuEscolha (false); return; }
    if (rFino.contains (p)) { proc.fino = ! proc.fino.load(); repaint(); return; }
    if (rMenu.contains (p) || e.mods.isPopupMenu()) menu();
}

void OnkeyEditor::mouseDrag (const juce::MouseEvent& e)
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

void OnkeyEditor::mouseUp (const juce::MouseEvent&)
{
    if (arrastando.isNotEmpty())
        if (auto* prm = param (arrastando)) prm->endChangeGesture();
    arrastando.clear();
}

void OnkeyEditor::mouseDoubleClick (const juce::MouseEvent& e)
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

void OnkeyEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
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

void OnkeyEditor::menuEscolha (bool tom)
{
    const auto& nomes = tom ? OnkeyProcessor::nomesTom() : OnkeyProcessor::nomesEscala();
    const juce::String id = tom ? "root" : "scale";
    const int atual = escolha (id);
    juce::PopupMenu m;
    for (int i = 0; i < nomes.size(); ++i)
    {
        m.addItem (i + 1, i == 0 ? (tom ? u8 ("AUTO (detectar o tom)") : u8 ("AUTO (detectar a escala)")) : nomes[i], true, i == atual);
        if (i == 0) m.addSeparator();
    }
    juce::Component::SafePointer<OnkeyEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, id] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         safe->proc.define (id, (float) (r - 1));
                         safe->repaint();
                     });
}

void OnkeyEditor::menu()
{
    juce::PopupMenu m;
    m.addItem (menuSobre, "Como funciona");
    m.addSeparator();
   #ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    m.addItem (-1, "ONKEY " + juce::String (JucePlugin_VersionString), false, false);
   #endif
    juce::Component::SafePointer<OnkeyEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == menuSobre)
                             juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "ONKEY",
                                 u8 ("Ponha o ONKEY no canal da VOZ (uma voz só, sem coro no mesmo canal).\n\n"
                                     "Ele acha o tom e a escala sozinho em alguns segundos de voz e leva ao tom cada nota que sair "
                                     "dele. Vibrato, slides e respiração passam como foram cantados, e os formantes ficam onde "
                                     "estavam: a voz não muda de timbre.\n\n"
                                     "AMOUNT: quanto corrige. ROOT e SCALE: escolha o tom na mão, se preferir. "
                                     "AJUSTE FINO: rapidez (RETUNE SPEED), o quanto deixa passar (HUMANIZE), vibrato, timbre "
                                     "(FORMANT) e volume.\n\n"
                                     "Latência de cerca de 45 ms, compensada pelo DAW. Desligado, o som passa sem nenhuma alteração."));
                     });
}
