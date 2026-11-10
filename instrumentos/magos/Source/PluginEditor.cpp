#include "PluginEditor.h"

using namespace magos;

namespace
{
// paleta do MAESTRO (o par): escuro arroxeado; OURO = marca e CAST, CIANO = depois, AMARELO = referência / alvo,
// ROSA = corte (DYN EQ, GR), AZUL = realce, CINZA = antes
const juce::Colour cFundoA { 0xff1b1726 }, cFundoB { 0xff110f18 }, cTopoA { 0xff241f33 }, cTopoB { 0xff17141f },
                   cChapa { 0xff1a1725 }, cBorda { 0xff2e2842 }, cTexto { 0xffdcd7ea }, cApagado { 0xff8b84a3 },
                   cTela { 0xff07060c }, cGrade { 0xff181425 }, cGradeF { 0xff2b2442 },
                   cOuro { 0xfff5c451 }, cRoxo { 0xffa77bff }, cRoxoClaro { 0xffc9b0ff }, cBranco { 0xfff3efff },
                   cCiano { 0xff3fd6c8 }, cRosa { 0xffff4f9a }, cAzul { 0xff45a6ff }, cVerde { 0xff3fd68a },
                   cCinza { 0xff7d7896 }, cRuim { 0xffff4f6b }, cLaranja { 0xffff8a3d };

// KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX (as cores do MAESTRO)
const juce::Colour corGrupo[maestrolink::NR] = { juce::Colour (0xffff8a3d), juce::Colour (0xff45a6ff), juce::Colour (0xfff5c451),
                                                 juce::Colour (0xffe6e1f5), juce::Colour (0xffc9b0ff), juce::Colour (0xff4fd1c5),
                                                 juce::Colour (0xffff7fb5), juce::Colour (0xffc4e86b) };
const juce::Colour corBanda[NM] = { juce::Colour (0xff45a6ff), juce::Colour (0xffa77bff), juce::Colour (0xffff4f9a), juce::Colour (0xff3fd6c8) };

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}
float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }

const juce::String menos = juce::String::fromUTF8 ("\xe2\x88\x92");
const juce::String ponto = juce::String::fromUTF8 (" \xc2\xb7 ");
const juce::String seta = juce::String::fromUTF8 ("\xe2\x96\xb8");
const juce::String traco = juce::String::fromUTF8 ("\xe2\x80\x93");
const juce::String diferente = juce::String::fromUTF8 ("\xe2\x89\xa0");
const juce::String reticencias = juce::String::fromUTF8 ("\xe2\x80\xa6");
const juce::String setaDir = juce::String::fromUTF8 ("\xe2\x86\x92");

juce::String num (float v, int casas = 1, bool sinal = false)
{
    // juce::String (x, 0) NÃO arredonda (0 casas = precisão total): sem casas, arredonda à mão
    if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -casas)) return casas > 0 ? juce::String (0.0f, casas) : juce::String ("0");
    juce::String s = casas > 0 ? juce::String (std::abs (v), casas) : juce::String (juce::roundToInt (std::abs (v)));
    return (v < 0 ? menos : (sinal ? juce::String ("+") : juce::String())) + s;
}
juce::String hz (double f)
{
    if (f >= 10000.0) return juce::String (juce::roundToInt (f / 1000.0)) + "k";
    if (f >= 1000.0) return juce::String (f / 1000.0, 1) + "k";
    return juce::String (juce::roundToInt (f));
}

void caixa (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (cChapa);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (cBorda.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
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
// estrela de 4 pontas (o brilho do CAST) e de 5 (a marca)
juce::Path brilho (juce::Point<float> c, float r)
{
    juce::Path p;
    p.startNewSubPath (c.x, c.y - r);
    p.quadraticTo (c.x + r * 0.12f, c.y - r * 0.12f, c.x + r, c.y);
    p.quadraticTo (c.x + r * 0.12f, c.y + r * 0.12f, c.x, c.y + r);
    p.quadraticTo (c.x - r * 0.12f, c.y + r * 0.12f, c.x - r, c.y);
    p.quadraticTo (c.x - r * 0.12f, c.y - r * 0.12f, c.x, c.y - r);
    p.closeSubPath();
    return p;
}
juce::Path estrela (juce::Point<float> c, float r)
{
    juce::Path p;
    p.addStar (c, 5, r * 0.45f, r, 0.0f);
    return p;
}

// eixo de frequência: 20 Hz – 20 kHz, log
constexpr double fMin = 20.0, fMax = 20000.0;
float xDeF (double f, juce::Rectangle<float> r) { return r.getX() + r.getWidth() * (float) (std::log (f / fMin) / std::log (fMax / fMin)); }

// dB de uma forma de 27 bandas numa frequência qualquer (interpola na escala das bandas)
float formaEm (const float* forma, double f)
{
    const double u = juce::jlimit (0.0, (double) (NB - 1), 3.0 * std::log2 (f / 40.0));
    const int a = juce::jmin (NB - 2, (int) u);
    const double t = u - a;
    return (float) (forma[a] + (forma[a + 1] - forma[a]) * t);
}
} // namespace

// ------------------------------------------------------------------------------------------------------------------
MagosEditor::MagosEditor (MagosProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (896, 666, 2560, 1904);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (896, 2560, w0) : 1120;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));

    auto k = [this] (const char* rot, const char* id, float x, juce::Colour cor, bool centro, std::function<juce::String()> t)
    {
        Knob kn;
        kn.rotulo = rot; kn.id = id; kn.c = { x, 892 }; kn.r = 19; kn.cor = cor; kn.centro = centro; kn.texto = std::move (t);
        if (auto* prm = proc.apvts.getParameter (id)) kn.padrao = prm->getDefaultValue();
        knobs.push_back (kn);
    };
    auto& P = proc;
    k ("AMOUNT", "amount", 404, cOuro, false, [&P] { return juce::String (juce::roundToInt (P.valor ("amount"))) + "%"; });
    k ("CHARACTER", "character", 474, cRoxo, false, [&P]
    {
        const float v = P.valor ("character");
        return v < 20 ? juce::String ("CLEAN") : (v > 70 ? juce::String ("WARM") : juce::String ("CLEAN") + ponto + "WARM");
    });
    k ("PUNCH", "punch", 544, cRosa, true, [&P] { return num (P.valor ("punch"), 1, true); });
    k ("WIDTH", "width", 614, cCiano, false, [&P] { return juce::String (juce::roundToInt (P.valor ("width"))) + "%"; });
    k ("TONE TILT", "tilt", 684, cAzul, true, [&P] { return num (P.valor ("tilt"), 1, true) + " dB"; });

    startTimerHz (30);
}

void MagosEditor::resized() { proc.uiW = getWidth(); }

void MagosEditor::timerCallback()
{
    leLink();
    // a DYN EQ pisca rápido: a tela segura o maior corte / realce e solta devagar
    for (int z = 0; z < NZ; ++z)
    {
        const float v = proc.motor.zonaDb[z].load();
        zonaHold[z] = std::abs (v) > std::abs (zonaHold[z]) ? v : zonaHold[z] * 0.97f;
    }
    montaProblemas();
    repaint();
}

// ------------------------------------------------------------------------------------------------------------------
// MAESTRO LINK: soma os espectros (depois do MAESTRO) de cada grupo; vaga viva = batida mudou há menos de 1,5 s
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::leLink()
{
    for (auto& G : grupos) G = GrupoLink {};
    nGrupos = 0;
    ligado = false;
    auto* A = maestrolink::area();
    if (A == nullptr) return;
    const double agora = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    for (int i = 0; i < maestrolink::NSL; ++i)
    {
        auto& V = A->vaga[i];
        const uint32_t b = V.batida.load();
        if (b == 0) continue;
        if (b != batidaVista[i]) { batidaVista[i] = b; vivoAte[i] = agora + 1.5; }
        const int r = V.role.load();
        if (agora > vivoAte[i] || r < 0 || r >= maestrolink::NR) continue;
        auto& G = grupos[r];
        G.tem = true;
        ++G.faixas;
        G.tocando = G.tocando || V.tocando.load() != 0;
        for (int k = 0; k < maestrolink::NB; ++k) G.esp[k] += V.espectro[k].load();
        G.pot += potDe (V.nivelDb.load());
    }
    for (auto& G : grupos) if (G.tem) ++nGrupos;
    ligado = nGrupos > 0;
    if (! ligado) return;
    char ns[24] {};
    std::memcpy (ns, A->nomeStyle, sizeof (ns) - 1);
    nomeStyleMaestro = juce::String::fromUTF8 (ns);
    topoMaestro = A->topo.load();
    // nível de cada grupo relativo ao KICK, comparado com o do STYLE
    const auto& K = grupos[0];
    for (int r = 0; r < maestrolink::NR; ++r)
    {
        auto& G = grupos[r];
        G.alvo = A->alvoRole[r].load();
        if (G.tem && K.tem && G.pot > 1e-9 && K.pot > 1e-9)
        {
            G.desvio = (dbDe (G.pot) - dbDe (K.pot)) - (G.alvo - A->alvoRole[0].load());
            G.temDesvio = true;
        }
    }
}

// cada problema que o MAGOS vê no master, com o grupo do MAESTRO de onde vem (sem link: o MAGOS corrige sozinho)
void MagosEditor::montaProblemas()
{
    problemas.clear();
    if (! ligado) return;
    auto quota = [this] (double f, int& g1, float& q1, int& g2, float& q2)
    {
        const int b = juce::jlimit (0, NB - 1, (int) std::lround (3.0 * std::log2 (f / 40.0)));
        double tot = 0;
        for (auto& G : grupos) tot += G.esp[b];
        g1 = g2 = -1; q1 = q2 = 0.0f;
        if (tot <= 1e-14) return;
        for (int r = 0; r < maestrolink::NR; ++r)
        {
            const float q = (float) (grupos[r].esp[b] / tot);
            if (q > q1) { g2 = g1; q2 = q1; g1 = r; q1 = q; }
            else if (q > q2) { g2 = r; q2 = q; }
        }
    };
    // 1) zonas que a DYN EQ está cortando
    for (int z : { 1, 2, 3 })
        if (zonaHold[z] < -1.0f)
        {
            int g1, g2; float q1, q2;
            quota (zona (z).f, g1, q1, g2, q2);
            if (g1 < 0) continue;
            Problema p;
            p.rotulo = juce::String (zona (z).nome) + " " + hz (zona (z).f) + " Hz";
            p.texto = juce::String (juce::roundToInt (q1 * 100)) + "% COMES FROM " + maestrolink::nomeGrupo (g1)
                      + (g2 >= 0 && q2 > 0.15f ? ponto + juce::String (juce::roundToInt (q2 * 100)) + "% FROM " + maestrolink::nomeGrupo (g2) : juce::String());
            p.acao = seta + " ON THE " + maestrolink::nomeGrupo (g1) + " GROUP: " + num (zonaHold[z] * 0.9f) + " dB @ " + hz (zona (z).f) + " Hz";
            p.cor = cRuim;
            problemas.push_back (p);
        }
    // 2) grupo fora do balanço do STYLE (o maior desvio)
    {
        int pior = -1;
        for (int r = 1; r < maestrolink::NR; ++r)
            if (grupos[r].temDesvio && std::abs (grupos[r].desvio) > 1.5f && (pior < 0 || std::abs (grupos[r].desvio) > std::abs (grupos[pior].desvio)))
                pior = r;
        if (pior >= 0)
        {
            const float d = grupos[pior].desvio;
            Problema p;
            p.rotulo = juce::String (maestrolink::nomeGrupo (pior)) + " " + num (d, 1, true) + " dB";
            p.texto = juce::String (d > 0 ? "OVER" : "UNDER") + " THE " + nomeStyleMaestro + " BALANCE (vs KICK)";
            p.acao = seta + " IN MAESTRO " + seta + " " + maestrolink::nomeGrupo (pior) + ": TRIM " + num (-d, 1, true) + " dB";
            p.cor = cOuro;
            problemas.push_back (p);
        }
    }
    // 3) LOW PUNCH: o grave falta e KICK + BASS dividem o grave sem KEEP ON TOP
    if (zonaHold[0] > 0.8f && grupos[0].tem && grupos[1].tem && topoMaestro == 0)
    {
        Problema p;
        p.rotulo = "LOW PUNCH";
        p.texto = "KICK MASKED BY BASS ON THE DOWNBEAT";
        p.acao = seta + " IN MAESTRO: KEEP KICK / BASS ON TOP " + setaDir + " ON";
        p.cor = cOuro;
        problemas.push_back (p);
    }
    if (problemas.empty())
    {
        Problema p;
        p.rotulo = "ALL CLEAR";
        p.texto = "NO GROUP IS CAUSING A PROBLEM ON THE MASTER RIGHT NOW";
        p.acao = {};
        p.cor = cVerde;
        problemas.push_back (p);
    }
    if (problemas.size() > 4) problemas.resize (4);
}

void MagosEditor::texto (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float tam, juce::Colour cor,
                         juce::Justification j, float kern, bool negrito)
{
    g.setColour (cor);
    g.setFont (fonte (tam, negrito).withExtraKerningFactor (kern));
    g.drawText (t, r, j, false);
}

void MagosEditor::titulo (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, const juce::String& sub)
{
    const auto f = fonte (11.5f, true).withExtraKerningFactor (0.3f);
    texto (g, t, { r.getX() + 14, r.getY() + 10, 300, 16 }, 11.5f, cBranco, juce::Justification::centredLeft, 0.3f, true);
    if (sub.isNotEmpty())
        texto (g, ponto + sub, { r.getX() + 14 + largura (f, t) + 4, r.getY() + 10, 600, 16 }, 10.0f, cApagado,
               juce::Justification::centredLeft, 0.22f);
}

// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    g.setGradientFill (juce::ColourGradient (cFundoA, 0, 0, cFundoB, 0, (float) baseH, false));
    g.fillRect (0.0f, 0.0f, (float) baseW, (float) baseH);
    cliques.clear();

    topo (g);
    painelTom (g, { 12, 64, 828, 352 });
    painelMulti (g, { 12, 424, 828, 262 });
    painelLoud (g, { 852, 64, 416, 268 });
    painelStereo (g, { 852, 340, 416, 120 });
    painelOnde (g, { 852, 468, 416, 218 });
    painelLink (g, { 12, 694, 1256, 156 });
    base (g);

    if (arrastandoArquivo)
    {
        g.setColour (cTela.withAlpha (0.75f));
        g.fillRect (0.0f, 0.0f, (float) baseW, (float) baseH);
        g.setColour (cOuro);
        g.drawRoundedRectangle (juce::Rectangle<float> (40, 40, baseW - 80.0f, baseH - 80.0f), 12.0f, 2.0f);
        texto (g, "DROP THE REFERENCE TRACK", { 0, baseH * 0.5f - 30, (float) baseW, 40 }, 30.0f, cOuro, juce::Justification::centred, 0.3f, true);
        texto (g, "WAV" + ponto + "AIFF" + ponto + "FLAC" + ponto + "OGG" + ponto + "MP3", { 0, baseH * 0.5f + 14, (float) baseW, 24 }, 13.0f,
               cTexto, juce::Justification::centred, 0.2f);
    }
}

void MagosEditor::topo (juce::Graphics& g)
{
    auto& M = proc.motor;
    g.setGradientFill (juce::ColourGradient (cTopoA, 0, 0, cTopoB, 0, 56, false));
    g.fillRect (0.0f, 0.0f, (float) baseW, 56.0f);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 56, (float) baseW, 56, 1.0f);

    // marca: ★ M(ouro)ASTER  dos  M(ouro)AGOS
    g.setColour (cOuro);
    g.strokePath (estrela ({ 32, 28 }, 13), juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
    const auto fm = fonte (25.0f, true).withExtraKerningFactor (0.3f), fd = fonte (14.0f, true).withExtraKerningFactor (0.25f);
    float x = 54;
    auto palavra = [&] (const juce::String& resto)
    {
        g.setFont (fm);
        g.setColour (cOuro);
        g.drawText ("M", juce::Rectangle<float> (x, 10, 40, 36), juce::Justification::centredLeft);
        x += largura (fm, "M");
        g.setColour (cBranco);
        g.drawText (resto, juce::Rectangle<float> (x, 10, 200, 36), juce::Justification::centredLeft);
        x += largura (fm, resto);
    };
    palavra ("ASTER");
    x += 10;
    g.setFont (fd);
    g.setColour (cRoxo);
    g.drawText ("DOS", juce::Rectangle<float> (x, 18, 50, 28), juce::Justification::centredLeft);
    x += largura (fd, "DOS") + 10;
    palavra ("AGOS");
    cliques.push_back ({ { 14, 6, x - 10, 44 }, [this] { menuLogo(); } });
    const float xMarca = x;

    float xd = (float) baseW - 12;
    auto chip = [&] (const juce::String& rot, const juce::String& val, juce::Colour corVal, bool comLed, juce::Colour corLed, bool menu,
                     std::function<void()> faz)
    {
        const auto fr = fonte (10.0f).withExtraKerningFactor (0.18f), fv = fonte (10.5f, true).withExtraKerningFactor (0.16f);
        const float wr = rot.isNotEmpty() ? largura (fr, rot) + 8 : 0.0f, wv = largura (fv, val);
        const float w = 22 + (comLed ? 14.0f : 0.0f) + wr + wv + (menu ? 14.0f : 0.0f);
        xd -= w;
        const juce::Rectangle<float> c (xd, 13, w, 30);
        g.setColour (cTela.withAlpha (0.7f)); g.fillRoundedRectangle (c, 4.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (c, 4.0f, 1.0f);
        float tx = c.getX() + 11;
        if (comLed) { led (g, { tx + 4, c.getCentreY() }, 3.5f, corLed, true); tx += 14; }
        if (wr > 0) { g.setColour (cApagado); g.setFont (fr); g.drawText (rot, juce::Rectangle<float> (tx, c.getY(), wr, c.getHeight()), juce::Justification::centredLeft); tx += wr; }
        g.setColour (corVal); g.setFont (fv);
        g.drawText (val, juce::Rectangle<float> (tx, c.getY(), wv + 2, c.getHeight()), juce::Justification::centredLeft);
        if (menu)
        {
            juce::Path p;
            const float sx = c.getRight() - 14, sy = c.getCentreY() - 2;
            p.addTriangle (sx, sy, sx + 7, sy, sx + 3.5f, sy + 4.5f);
            g.setColour (cApagado); g.fillPath (p);
        }
        if (faz) cliques.push_back ({ c, faz });
        xd -= 8;
    };
    chip ("TARGET", num (M.feitico.feito ? M.feitico.alvoLufs : M.lufsAlvo(), 1) + " LUFS" + ponto + num (kTeto, 0) + " dBTP", cBranco, false, {}, true,
          [this] { menuAlvo(); });
    chip ("STYLE", nomeStyle (M.style.load()), cBranco, false, {}, true, [this] { menuStyle(); });
    juce::String ref = proc.carregando.load() ? juce::String ("LOADING") + reticencias : proc.nomeReferencia();
    if (ref.isEmpty()) ref = "NONE";
    if (ref.length() > 26) ref = ref.substring (0, 24) + reticencias;
    chip ("REFERENCE", proc.carregando.load() || proc.nomeReferencia().isEmpty() ? ref : "\"" + ref + "\"",
          proc.nomeReferencia().isEmpty() ? cApagado : cOuro, false, {}, true, [this] { menuReferencia(); });
    chip ("MAESTRO", ligado ? "LINKED" + ponto + juce::String (nGrupos) + " GROUP" + (nGrupos == 1 ? "" : "S") : juce::String ("NOT LINKED"),
          ligado ? cBranco : cApagado, true, ligado ? cVerde : cApagado, false, {});
    // subtítulo só se couber entre a marca e os chips
    const auto fs = fonte (10.0f).withExtraKerningFactor (0.3f);
    if (xMarca + 18 + largura (fs, "MASTERING WIZARD") + 10 < xd)
        texto (g, "MASTERING WIZARD", { xMarca + 18, 21, 180, 16 }, 10.0f, cApagado, juce::Justification::centredLeft, 0.3f);
}

// ------------------------------------------------------------------------------------------------------------------
// TONAL BALANCE
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::painelTom (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& M = proc.motor;
    caixa (g, r);
    const bool temRef = proc.nomeReferencia().isNotEmpty();
    titulo (g, r, "TONAL BALANCE", temRef ? "YOU vs REFERENCE" : "YOU vs " + juce::String (nomeStyle (M.style.load())) + " CURVE");

    // legenda
    {
        float lx = r.getRight() - 14;
        auto item = [&] (const juce::String& t, juce::Colour c, bool zonaQ)
        {
            const auto f = fonte (9.5f).withExtraKerningFactor (0.16f);
            const float w = largura (f, t);
            lx -= w;
            texto (g, t, { lx, r.getY() + 10, w + 2, 16 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.16f);
            lx -= 20;
            g.setColour (zonaQ ? c.withAlpha (0.35f) : c);
            if (zonaQ) g.fillRect (lx, r.getY() + 13, 14.0f, 9.0f);
            else g.fillRect (lx, r.getY() + 16.5f, 14.0f, 2.5f);
            lx -= 14;
        };
        item ("DYN EQ", cRosa, false);
        item ("AFTER", cCiano, false);
        item ("TARGET ZONE", cOuro, true);
        item (temRef ? "REFERENCE" : "STYLE", cOuro, false);
        item ("BEFORE", cCinza, false);
    }

    const juce::Rectangle<float> tela (r.getX() + 44, r.getY() + 36, r.getWidth() - 58, 222);
    g.setColour (cTela);
    g.fillRoundedRectangle (tela.expanded (2), 3.0f);
    constexpr float dbAlto = 12.0f, dbBaixo = -30.0f;
    auto yDe = [&] (float db) { return tela.getY() + tela.getHeight() * (dbAlto - juce::jlimit (dbBaixo, dbAlto, db)) / (dbAlto - dbBaixo); };
    // grade
    for (float d = 12.0f; d >= -30.0f; d -= 6.0f)
    {
        g.setColour (d == 0.0f ? cGradeF : cGrade);
        g.drawHorizontalLine ((int) yDe (d), tela.getX(), tela.getRight());
        texto (g, num (d, 0, true), { r.getX() + 4, yDe (d) - 7, 36, 14 }, 9.0f, cApagado, juce::Justification::centredRight, 0.0f);
    }
    for (double f : { 31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 })
    {
        const float xx = xDeF (f, tela);
        g.setColour (cGrade);
        g.drawVerticalLine ((int) xx, tela.getY(), tela.getBottom());
        texto (g, hz (f), { xx - 20, tela.getBottom() + 2, 40, 13 }, 9.0f, cApagado, juce::Justification::centred, 0.0f);
    }

    // formas: antes (LISTEN; sem LISTEN, a entrada ao vivo), depois (saída ao vivo), alvo
    float antes[NB], depois[NB], alvo[NB], pIn[NB], pOut[NB];
    const bool temApr = M.aprQuadros.load() > 4;
    for (int b = 0; b < NB; ++b) { pIn[b] = temApr ? M.aprEsp[b].load() : M.telaIn[b].load(); pOut[b] = M.telaOut[b].load(); }
    formaDb (pIn, antes);
    formaDb (pOut, depois);
    if (M.feitico.feito) std::copy (M.feitico.alvoForma, M.feitico.alvoForma + NB, alvo);
    else M.formaAlvo (alvo);
    double eIn = 0, eOut = 0;
    for (int b = 0; b < NB; ++b) { eIn += pIn[b]; eOut += pOut[b]; }
    const bool somIn = dbDe (eIn) > -80.0f, somOut = dbDe (eOut) > -80.0f;

    auto caminho = [&] (const float* forma)
    {
        juce::Path p;
        for (int i = 0; i <= 200; ++i)
        {
            const double f = fMin * std::pow (fMax / fMin, i / 200.0);
            const float xx = xDeF (f, tela), yy = yDe (formaEm (forma, juce::jlimit (40.0, 16000.0, f)));
            if (i == 0) p.startNewSubPath (xx, yy); else p.lineTo (xx, yy);
        }
        return p;
    };
    g.saveState();
    g.reduceClipRegion (tela.toNearestInt());
    // zona alvo (±2 dB)
    {
        juce::Path z;
        for (int i = 0; i <= 200; ++i)
        {
            const double f = fMin * std::pow (fMax / fMin, i / 200.0);
            const float xx = xDeF (f, tela), yy = yDe (formaEm (alvo, juce::jlimit (40.0, 16000.0, f)) + 2.0f);
            if (i == 0) z.startNewSubPath (xx, yy); else z.lineTo (xx, yy);
        }
        for (int i = 200; i >= 0; --i)
        {
            const double f = fMin * std::pow (fMax / fMin, i / 200.0);
            z.lineTo (xDeF (f, tela), yDe (formaEm (alvo, juce::jlimit (40.0, 16000.0, f)) - 2.0f));
        }
        z.closeSubPath();
        g.setColour (cOuro.withAlpha (0.10f));
        g.fillPath (z);
    }
    // sobra acima da zona (vermelho) na curva DEPOIS
    if (somOut)
        for (int i = 0; i < 200; ++i)
        {
            const double f = fMin * std::pow (fMax / fMin, (i + 0.5) / 200.0);
            const double fc = juce::jlimit (40.0, 16000.0, f);
            const float sobra = formaEm (depois, fc) - (formaEm (alvo, fc) + 2.0f);
            if (sobra <= 0.0f) continue;
            const float x0 = xDeF (fMin * std::pow (fMax / fMin, i / 200.0), tela), x1 = xDeF (fMin * std::pow (fMax / fMin, (i + 1) / 200.0), tela);
            g.setColour (cRuim.withAlpha (juce::jlimit (0.15f, 0.55f, sobra * 0.15f)));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x0, yDe (formaEm (depois, fc)), x1 + 0.5f, yDe (formaEm (alvo, fc) + 2.0f)));
        }
    if (somIn)
    {
        g.setColour (cCinza.withAlpha (0.9f));
        g.strokePath (caminho (antes), juce::PathStrokeType (1.6f));
    }
    {
        juce::Path tr;
        const float dash[] = { 6.0f, 4.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (tr, caminho (alvo), dash, 2);
        g.setColour (cOuro);
        g.fillPath (tr);
    }
    if (somOut)
    {
        g.setColour (cCiano);
        g.strokePath (caminho (depois), juce::PathStrokeType (2.4f));
    }
    // pontos da DYN EQ na curva DEPOIS
    for (int z = 0; z < NZ; ++z)
    {
        const float v = zonaHold[z];
        if (std::abs (v) < 0.3f || ! somOut) continue;
        const auto& Z = zona (z);
        const float xx = xDeF (Z.f, tela), yy = yDe (formaEm (depois, juce::jlimit (40.0, 16000.0, Z.f)));
        const auto c = v < 0 ? cRosa : cAzul;
        g.setColour (c);
        g.drawEllipse (juce::Rectangle<float> (14, 14).withCentre ({ xx, yy }), 2.0f);
        g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre ({ xx, yy }));
        juce::String rot = Z.nome;
        if (z == 2) rot += ponto + "ONLY ON LOUD HITS";
        texto (g, rot, { xx - 90, yy - 26, 180, 13 }, 9.5f, cTexto, juce::Justification::centred, 0.16f, true);
    }
    g.restoreState();

    // MATCH %
    {
        juce::String t;
        if (somOut && (M.feitico.feito || temRef))
        {
            t = "MATCH " + juce::String (juce::roundToInt (casamento (depois, alvo))) + "%";
            if (M.feitico.feito && M.feitico.casamentoAntes > 0) t += ponto + "was " + juce::String (juce::roundToInt (M.feitico.casamentoAntes)) + "%";
        }
        else if (! somIn) t = "PLAY YOUR TRACK";
        texto (g, t, { tela.getRight() - 300, tela.getY() + 8, 290, 18 }, 13.0f, cCiano, juce::Justification::centredRight, 0.14f, true);
    }

    // faixa DYNAMIC EQ · LIVE
    const juce::Rectangle<float> fx (tela.getX(), tela.getBottom() + 20, tela.getWidth(), 50);
    g.setColour (cTela);
    g.fillRoundedRectangle (fx, 3.0f);
    texto (g, "DYNAMIC EQ" + ponto + "LIVE", { fx.getX() + 8, fx.getY() + 3, 200, 12 }, 9.0f, cApagado, juce::Justification::centredLeft, 0.2f);
    const float meio = fx.getCentreY() + 4;
    {
        juce::Path linha;
        linha.startNewSubPath (fx.getX() + 4, meio);
        linha.lineTo (fx.getRight() - 4, meio);
        juce::Path tr;
        const float dash[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (tr, linha, dash, 2);
        g.setColour (cGradeF);
        g.fillPath (tr);
    }
    for (int z = 0; z < NZ; ++z)
    {
        const auto& Z = zona (z);
        const float v = zonaHold[z], xx = xDeF (Z.f, fx);
        const float h = juce::jlimit (2.0f, 18.0f, std::abs (v) * 4.5f);
        const auto c = v < 0 ? cRosa : cAzul;
        g.setColour (std::abs (v) < 0.1f ? cGradeF : c);
        if (v >= 0) g.fillRoundedRectangle (xx - 4, meio - h, 8, h, 2.0f);
        else g.fillRoundedRectangle (xx - 4, meio, 8, h, 2.0f);
        texto (g, std::abs (v) < 0.1f ? juce::String (Z.nome) : num (v, 1, true), { xx + 8, v >= 0 ? meio - 16 : meio + 2, 80, 13 }, 9.5f,
               std::abs (v) < 0.1f ? cApagado : c, juce::Justification::centredLeft, 0.1f, std::abs (v) >= 0.1f);
    }
}

// ------------------------------------------------------------------------------------------------------------------
// MULTIBAND COMP
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::painelMulti (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& M = proc.motor;
    caixa (g, r);
    titulo (g, r, "MULTIBAND COMP", "4 BANDS" + ponto + (proc.nomeReferencia().isNotEmpty() ? "AUTO FROM THE REFERENCE" : "AUTO FROM YOUR TRACK"));
    texto (g, "LINEAR PHASE  120" + ponto + "1.2k" + ponto + "6.5k Hz", { r.getRight() - 300, r.getY() + 10, 286, 16 }, 10.0f, cOuro,
           juce::Justification::centredRight, 0.18f, true);
    static const char* faixa[NM] = { "20-120 Hz", "120-1.2k Hz", "1.2k-6.5k Hz", "6.5k-20k Hz" };
    static const char* papel[NM] = { "KICK + BASS GLUE", "TAMES THE BOX", "HARSHNESS", "SMOOTH HATS" };
    static const float atqBase[NM] = { 30.0f, 15.0f, 5.0f, 2.0f }, relBase[NM] = { 160.0f, 120.0f, 80.0f, 60.0f };
    static const float razBase[NM] = { 2.0f, 2.5f, 3.0f, 1.6f };
    const float amt = proc.valor ("amount") / 100.0f, quente = proc.valor ("character") / 100.0f;
    const float fp = std::pow (2.0f, proc.valor ("punch") / 2.5f);
    const bool feito = M.feitico.feito;
    const float w = (r.getWidth() - 28 - 3 * 10) / 4.0f;
    for (int k = 0; k < NM; ++k)
    {
        const juce::Rectangle<float> c (r.getX() + 14 + k * (w + 10), r.getY() + 34, w, r.getHeight() - 46);
        g.setColour (cTela.withAlpha (0.55f));
        g.fillRoundedRectangle (c, 4.0f);
        g.setColour (cBorda);
        g.drawRoundedRectangle (c, 4.0f, 1.0f);
        texto (g, nomeBanda (k), { c.getX(), c.getY() + 6, c.getWidth(), 14 }, 11.0f, corBanda[k], juce::Justification::centred, 0.3f, true);
        texto (g, faixa[k], { c.getX(), c.getY() + 21, c.getWidth(), 12 }, 9.5f, cApagado, juce::Justification::centred, 0.08f);

        // medidores IN / GR / OUT
        const float mt = c.getY() + 42, mh = 104;
        auto barra = [&] (float x, float frac, juce::Colour cor, bool deCima, const juce::String& rot)
        {
            const juce::Rectangle<float> b (x, mt, 12, mh);
            g.setColour (cTela);
            g.fillRoundedRectangle (b, 2.0f);
            frac = juce::jlimit (0.0f, 1.0f, frac);
            g.setColour (cor);
            if (deCima) g.fillRoundedRectangle (b.withHeight (mh * frac), 2.0f);
            else g.fillRoundedRectangle (b.withTrimmedTop (mh * (1.0f - frac)), 2.0f);
            texto (g, rot, { x - 10, mt + mh + 3, 32, 11 }, 8.5f, cApagado, juce::Justification::centred, 0.1f);
        };
        auto fr = [] (float db) { return (db + 60.0f) / 60.0f; };
        const float gr = M.grBanda[k].load();
        barra (c.getX() + 16, fr (M.inBanda[k].load()), cCinza, false, "IN");
        barra (c.getX() + 40, -gr / 12.0f, cRosa, true, "GR");
        barra (c.getX() + 64, fr (M.outBanda[k].load()), corBanda[k], false, "OUT");

        // limiar e razão (AUTO: só mostra)
        const float raz = 1.0f + (razBase[k] - 1.0f) * (0.6f + 0.8f * quente) * (0.4f + 0.6f * amt);
        auto mostrador = [&] (float cy, float frac, const juce::String& val, const juce::String& rot)
        {
            const juce::Point<float> cc (c.getRight() - 46, cy);
            const float rr = 14, a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
            juce::Path fundo, arco;
            fundo.addCentredArc (cc.x, cc.y, rr, rr, 0, a0, a1, true);
            g.setColour (cGradeF); g.strokePath (fundo, juce::PathStrokeType (2.5f));
            const float fim = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, frac);
            arco.addCentredArc (cc.x, cc.y, rr, rr, 0, a0, fim, true);
            g.setColour (corBanda[k]); g.strokePath (arco, juce::PathStrokeType (2.5f));
            g.setColour (cBranco);
            g.drawLine (cc.x, cc.y, cc.x + std::sin (fim) * (rr - 3), cc.y - std::cos (fim) * (rr - 3), 1.6f);
            texto (g, val, { cc.x - 40, cy + 17, 80, 13 }, 10.0f, cTexto, juce::Justification::centred, 0.0f, true);
            texto (g, rot, { cc.x - 40, cy + 30, 80, 11 }, 8.5f, cApagado, juce::Justification::centred, 0.2f);
        };
        const float lim = M.limiarAgora[k].load();
        mostrador (mt + 16, feito ? (lim + 60.0f) / 60.0f : 1.0f, feito ? num (lim, 1) : juce::String ("OFF"), "THRESH");
        mostrador (mt + 76, (raz - 1.0f) / 4.0f, juce::String (raz, 1) + ":1", "RATIO");

        texto (g, "GR " + num (gr, 1) + " dB" + ponto + "ATK/REL " + juce::String (juce::roundToInt (atqBase[k] * fp)) + " / "
               + juce::String (juce::roundToInt (relBase[k])) + " ms", { c.getX(), c.getBottom() - 46, c.getWidth(), 13 }, 9.5f, cTexto,
               juce::Justification::centred, 0.04f);
        const auto fA = fonte (8.5f, true).withExtraKerningFactor (0.2f);
        const float wA = largura (fA, "AUTO") + 12, wP = largura (fA, papel[k]) + 12, x0 = c.getCentreX() - (wA + wP + 6) * 0.5f;
        const juce::Rectangle<float> ca (x0, c.getBottom() - 26, wA, 16), cp (x0 + wA + 6, c.getBottom() - 26, wP, 16);
        g.setColour (corBanda[k].withAlpha (0.6f)); g.drawRoundedRectangle (ca, 3.0f, 1.0f);
        texto (g, "AUTO", ca, 8.5f, corBanda[k], juce::Justification::centred, 0.2f, true);
        g.setColour (cBorda); g.drawRoundedRectangle (cp, 3.0f, 1.0f);
        texto (g, papel[k], cp, 8.5f, cTexto, juce::Justification::centred, 0.2f, true);
    }
}

// ------------------------------------------------------------------------------------------------------------------
// LOUDNESS
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::painelLoud (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& M = proc.motor;
    caixa (g, r);
    titulo (g, r, "LOUDNESS", "LIMITER" + ponto + "TRUE PEAK");
    const float alvo = M.feitico.feito ? M.feitico.alvoLufs : M.lufsAlvo();
    const auto* ref = M.ref.load();
    // medidores IN / OUT / REF / GR (0 … −24 LUFS curto)
    const float mt = r.getY() + 40, mh = 186;
    auto yDe = [&] (float db) { return mt + mh * juce::jlimit (0.0f, 1.0f, -db / 24.0f); };
    for (int d = 0; d >= -24; d -= 6)
        texto (g, juce::String (d), { r.getX() + 4, yDe ((float) d) - 6, 20, 12 }, 8.5f, cApagado, juce::Justification::centredRight, 0.0f);
    auto barra = [&] (float x, float db, juce::Colour cor, const juce::String& rot)
    {
        const juce::Rectangle<float> b (x, mt, 22, mh);
        g.setColour (cTela); g.fillRoundedRectangle (b, 2.0f);
        if (db > -60.0f) { g.setColour (cor); g.fillRoundedRectangle (b.withTop (yDe (db)), 2.0f); }
        texto (g, rot, { x - 8, mt + mh + 4, 38, 11 }, 8.5f, rot == "GR" ? cRosa : cApagado, juce::Justification::centred, 0.1f);
    };
    barra (r.getX() + 30, M.curtoIn.load(), cCinza, "IN");
    barra (r.getX() + 58, M.curtoOut.load(), cCiano, "OUT");
    barra (r.getX() + 86, ref != nullptr && ref->an.ok ? ref->an.lufs : -200.0f, cOuro, "REF");
    {
        const juce::Rectangle<float> b (r.getX() + 114, mt, 10, mh);
        g.setColour (cTela); g.fillRoundedRectangle (b, 2.0f);
        g.setColour (cRosa);
        g.fillRoundedRectangle (b.withHeight (mh * juce::jlimit (0.0f, 1.0f, -M.grLim.load() / 12.0f)), 2.0f);
        texto (g, "GR", { b.getX() - 8, mt + mh + 4, 26, 11 }, 8.5f, cRosa, juce::Justification::centred, 0.1f);
    }
    {
        juce::Path l;
        l.startNewSubPath (r.getX() + 26, yDe (alvo));
        l.lineTo (r.getX() + 112, yDe (alvo));
        juce::Path tr;
        const float dash[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (tr, l, dash, 2);
        g.setColour (cBranco); g.fillPath (tr);
        texto (g, num (alvo, 0), { r.getX() + 126, yDe (alvo) - 6, 30, 12 }, 8.5f, cBranco, juce::Justification::centredLeft, 0.0f);
    }

    // números
    const float x = r.getX() + 166, wcol = 118;
    const float lufs = M.lufsOut.load();
    texto (g, "INTEGRATED", { x, r.getY() + 36, 200, 12 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.3f);
    texto (g, lufs > -60.0f ? num (lufs, 1) : juce::String (traco), { x, r.getY() + 48, 140, 44 }, 40.0f, cCiano, juce::Justification::centredLeft, 0.0f);
    texto (g, "LUFS", { x + 132, r.getY() + 66, 60, 20 }, 15.0f, cApagado, juce::Justification::centredLeft, 0.1f);
    juce::String linha;
    if (ref != nullptr && ref->an.ok) linha << "REFERENCE " << num (ref->an.lufs, 1);
    if (M.feitico.antesLufs > -60.0f) linha << (linha.isNotEmpty() ? ponto : juce::String()) << "BEFORE " << num (M.feitico.antesLufs, 1);
    texto (g, linha, { x, r.getY() + 92, 240, 13 }, 9.5f, cTexto, juce::Justification::centredLeft, 0.14f);

    auto par = [&] (float px, float py, const juce::String& rot, const juce::String& val, const juce::String& un, juce::Colour cor)
    {
        texto (g, rot, { px, py, wcol, 12 }, 9.0f, cApagado, juce::Justification::centredLeft, 0.25f);
        const auto f = fonte (21.0f);
        texto (g, val, { px, py + 13, wcol, 26 }, 21.0f, cor, juce::Justification::centredLeft, 0.0f);
        texto (g, un, { px + largura (f, val) + 4, py + 22, 50, 14 }, 10.0f, cApagado, juce::Justification::centredLeft, 0.0f);
    };
    const float tp = M.tpMaxDb.load();
    par (x, r.getY() + 112, "TRUE PEAK", tp > -60.0f ? num (tp, 1) : juce::String (traco), "dBTP", tp > kTeto + 0.1f ? cRuim : cBranco);
    par (x + wcol, r.getY() + 112, "SHORT-TERM", M.curtoOut.load() > -60.0f ? num (M.curtoOut.load(), 1) : juce::String (traco), "LUFS", cBranco);
    const float plr = M.plr.load();
    par (x, r.getY() + 156, "PLR (PUNCH)", plr > 0.1f ? juce::String (plr, 1) : juce::String (traco), "dB", cBranco);
    par (x + wcol, r.getY() + 156, "LIMITER GR", num (M.grLim.load(), 1), "dB", cRosa);
    if (plr > 0.1f)
    {
        const bool ok = plr >= 7.0f;
        texto (g, juce::String::fromUTF8 ("\xe2\x97\x8f ") + (ok ? "OK" : "SQUASHED"), { x + 58, r.getY() + 178, 70, 12 }, 9.0f,
               ok ? cVerde : cLaranja, juce::Justification::centredLeft, 0.1f, true);
    }
    // selos
    float sx = x;
    auto selo = [&] (const juce::String& t, juce::Colour c)
    {
        const auto f = fonte (8.5f, true).withExtraKerningFactor (0.2f);
        const float w = largura (f, t) + 14;
        const juce::Rectangle<float> s (sx, r.getBottom() - 30, w, 18);
        g.setColour (c.withAlpha (0.12f)); g.fillRoundedRectangle (s, 3.0f);
        g.setColour (c.withAlpha (0.7f)); g.drawRoundedRectangle (s, 3.0f, 1.0f);
        texto (g, t, s, 8.5f, c, juce::Justification::centred, 0.2f, true);
        sx += w + 6;
    };
    if (M.clipou.load()) selo ("CLIPPED", cRuim); else selo ("NO CLIPPING", cVerde);
    if (proc.valor ("monobass") > 0.5f) selo ("BASS MONO < 120 Hz", cOuro);
    if (M.alvoAlto.load()) selo ("TARGET TOO LOUD FOR THIS MIX", cLaranja);
}

// ------------------------------------------------------------------------------------------------------------------
// STEREO
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::painelStereo (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& M = proc.motor;
    caixa (g, r);
    titulo (g, r, "STEREO", "WIDTH PER BAND" + ponto + "CORRELATION");
    for (int k = 0; k < NM; ++k)
    {
        const float y = r.getY() + 34 + k * 20;
        texto (g, nomeBanda (k), { r.getX() + 14, y, 80, 14 }, 9.5f, cTexto, juce::Justification::centredLeft, 0.2f);
        const juce::Rectangle<float> b (r.getX() + 92, y + 3, 170, 8);
        g.setColour (cTela); g.fillRoundedRectangle (b, 3.0f);
        const float lw = M.largura[k].load();
        const bool mono = k == 0 && proc.valor ("monobass") > 0.5f;
        const float w = mono ? 6.0f : juce::jlimit (6.0f, b.getWidth(), b.getWidth() * lw / 1.2f);
        g.setColour (corBanda[k]);
        g.fillRoundedRectangle (juce::Rectangle<float> (w, 8).withCentre (b.getCentre()), 3.0f);
        texto (g, mono ? juce::String ("MONO") : juce::String (juce::roundToInt (lw * 100.0f)) + "%", { b.getRight() + 8, y, 50, 14 }, 9.5f,
               cTexto, juce::Justification::centredLeft, 0.05f);
    }
    const float cx = r.getX() + 326;
    texto (g, "CORRELATION", { cx, r.getY() + 34, 90, 12 }, 8.5f, cApagado, juce::Justification::centredLeft, 0.22f);
    const float cor = M.correl.load();
    const juce::Rectangle<float> b (cx, r.getY() + 52, 76, 8);
    g.setColour (cTela); g.fillRoundedRectangle (b, 3.0f);
    const float px = b.getX() + b.getWidth() * (cor + 1.0f) * 0.5f;
    g.setColour (cor >= 0.0f ? cVerde : cRuim);
    g.fillRoundedRectangle (juce::Rectangle<float> (12, 8).withCentre ({ px, b.getCentreY() }), 3.0f);
    texto (g, num (cor, 2, true), { cx, r.getY() + 64, 76, 16 }, 13.0f, cor >= 0.0f ? cVerde : cRuim, juce::Justification::centredLeft, 0.0f);
    texto (g, cor >= 0.3f ? "MONO-SAFE" : (cor >= 0.0f ? "WIDE" : "PHASE!"), { cx, r.getY() + 84, 80, 12 }, 8.5f, cApagado,
           juce::Justification::centredLeft, 0.22f);
}

// ------------------------------------------------------------------------------------------------------------------
// WHERE IT WILL PLAY
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::painelOnde (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& M = proc.motor;
    caixa (g, r);
    titulo (g, r, "WHERE IT WILL PLAY", "TURN-DOWN PER PLATFORM");
    const float lufs = M.lufsOut.load() > -60.0f ? M.lufsOut.load() : M.curtoOut.load();
    struct Lugar { const char* nome; float norma; };
    static const Lugar lugares[] = { { "SPOTIFY", -14.0f }, { "YOUTUBE", -14.0f }, { "APPLE MUSIC", -16.0f }, { "BEATPORT / CLUB", 0.0f } };
    float y = r.getY() + 34;
    for (auto& L : lugares)
    {
        const float d = lufs > -60.0f ? std::min (0.0f, L.norma - lufs) : 0.0f;
        texto (g, L.nome, { r.getX() + 14, y, 130, 14 }, 9.5f, cTexto, juce::Justification::centredLeft, 0.2f);
        const juce::Rectangle<float> b (r.getX() + 146, y + 4, 190, 6);
        g.setColour (cTela); g.fillRoundedRectangle (b, 3.0f);
        const auto c = d > -6.5f ? cVerde : (d > -9.0f ? cOuro : cLaranja);
        g.setColour (c);
        g.fillRoundedRectangle (b.withWidth (juce::jlimit (4.0f, b.getWidth(), b.getWidth() * -d / 12.0f)), 3.0f);
        texto (g, lufs > -60.0f ? num (d, 1) + " dB" : juce::String (traco), { b.getRight() + 6, y, 60, 14 }, 9.5f, cTexto,
               juce::Justification::centredLeft, 0.05f);
        y += 20;
    }
    // alto-falante de celular: quanto da energia está abaixo de ~300 Hz (some)
    {
        double tot = 0, baixo = 0;
        for (int b = 0; b < NB; ++b) { const double p = M.telaOut[b].load(); tot += p; if (freqBanda (b) < 300.0) baixo += p; }
        const float perda = tot > 1e-12 ? dbDe ((tot - baixo) / tot) : 0.0f;
        texto (g, "PHONE SPEAKER", { r.getX() + 14, y, 130, 14 }, 9.5f, cTexto, juce::Justification::centredLeft, 0.2f);
        const juce::Rectangle<float> b (r.getX() + 146, y + 4, 190, 6);
        g.setColour (cTela); g.fillRoundedRectangle (b, 3.0f);
        g.setColour (cRosa);
        g.fillRoundedRectangle (b.withWidth (juce::jlimit (4.0f, b.getWidth(), b.getWidth() * -perda / 12.0f)), 3.0f);
        texto (g, tot > 1e-12 ? "LOWS " + num (perda, 0) : juce::String (traco), { b.getRight() + 6, y, 70, 14 }, 9.5f, cRosa,
               juce::Justification::centredLeft, 0.05f, true);
        y += 22;
    }
    texto (g, "LOUDER " + diferente + " BETTER: streaming turns it down anyway. A/B is always at the same loudness.",
           { r.getX() + 14, y, r.getWidth() - 28, 13 }, 9.0f, cApagado, juce::Justification::centredLeft, 0.05f);
    // HEAR IT ON (monitor; não sai no bounce se desligar antes)
    y += 20;
    texto (g, "HEAR IT ON", { r.getX() + 14, y + 2, 80, 16 }, 9.0f, cApagado, juce::Justification::centredLeft, 0.22f);
    static const char* sims[] = { "PHONE", "EARBUDS", "CAR", "MONO" };
    float sx = r.getX() + 94;
    for (int s = 1; s <= 4; ++s)
    {
        const bool on = proc.sim.load() == s;
        const juce::Rectangle<float> c (sx, y, 70, 20);
        g.setColour (on ? cRosa : cTela.withAlpha (0.6f)); g.fillRoundedRectangle (c, 3.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (c, 3.0f, 1.0f);
        texto (g, sims[s - 1], c, 9.0f, on ? juce::Colour (0xff120f1a) : cTexto, juce::Justification::centred, 0.2f, true);
        cliques.push_back ({ c, [this, s] { proc.sim = proc.sim.load() == s ? 0 : s; } });
        sx += 76;
    }
}

// ------------------------------------------------------------------------------------------------------------------
// MAESTRO LINKED
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::painelLink (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (ligado ? juce::Colour (0xff162019) : cChapa);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour ((ligado ? cVerde : cBorda).withAlpha (0.45f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    led (g, { r.getX() + 18, r.getY() + 18 }, 4.0f, cVerde, ligado);
    const auto ft = fonte (11.5f, true).withExtraKerningFactor (0.3f);
    texto (g, "MAESTRO LINKED", { r.getX() + 30, r.getY() + 10, 200, 16 }, 11.5f, cBranco, juce::Justification::centredLeft, 0.3f, true);
    texto (g, ponto + "FIX IT AT THE SOURCE, NOT ON THE MASTER", { r.getX() + 34 + largura (ft, "MAESTRO LINKED"), r.getY() + 10, 420, 16 },
           10.0f, cApagado, juce::Justification::centredLeft, 0.22f);
    texto (g, ligado ? "READS THE " + juce::String (nGrupos) + " MAESTRO GROUP" + (nGrupos == 1 ? "" : "S") + " IN THIS PROJECT" + ponto + "LIVE"
                     : juce::String ("NO MAESTRO FOUND IN THIS PROJECT"),
           { r.getRight() - 420, r.getY() + 10, 406, 16 }, 9.5f, ligado ? cVerde : cApagado, juce::Justification::centredRight, 0.2f);

    // os 8 grupos: nível vs o balanço do STYLE do MAESTRO
    texto (g, "GROUP LEVEL vs " + (ligado ? nomeStyleMaestro : juce::String ("STYLE")) + " BALANCE (dB)", { r.getX() + 14, r.getY() + 28, 400, 12 },
           8.5f, cApagado, juce::Justification::centredLeft, 0.2f);
    const float cw = 64, ch = 74, cy = r.getY() + 44;
    for (int gi = 0; gi < maestrolink::NR; ++gi)
    {
        const auto& G = grupos[gi];
        const juce::Rectangle<float> c (r.getX() + 14 + gi * (cw + 6), cy, cw, ch);
        const bool fora = G.temDesvio && std::abs (G.desvio) > 1.5f && gi > 0;
        g.setColour (cTela); g.fillRoundedRectangle (c, 3.0f);
        g.setColour (fora ? (G.desvio > 0 ? cRuim : cOuro).withAlpha (0.8f) : cBorda);
        g.drawRoundedRectangle (c, 3.0f, fora ? 1.5f : 1.0f);
        const float meio = c.getY() + 28;
        {
            juce::Path l;
            l.startNewSubPath (c.getX() + 6, meio); l.lineTo (c.getRight() - 6, meio);
            juce::Path tr;
            const float dash[] = { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (tr, l, dash, 2);
            g.setColour (cGradeF); g.fillPath (tr);
        }
        if (G.temDesvio || (G.tem && gi == 0))
        {
            const float d = gi == 0 ? 0.0f : juce::jlimit (-6.0f, 6.0f, G.desvio);
            const float h = std::max (3.0f, std::abs (d) * 3.5f);
            g.setColour (corGrupo[gi]);
            g.fillRoundedRectangle (juce::Rectangle<float> (c.getCentreX() - 9, d >= 0 ? meio - h : meio, 18, h), 2.0f);
            texto (g, num (gi == 0 ? 0.0f : G.desvio, 1, true), { c.getX(), c.getBottom() - 18, cw, 14 }, 10.0f,
                   fora ? (G.desvio > 0 ? cRuim : cOuro) : corGrupo[gi], juce::Justification::centred, 0.0f, true);
        }
        else texto (g, traco, { c.getX(), c.getBottom() - 18, cw, 14 }, 10.0f, cApagado, juce::Justification::centred, 0.0f);
        texto (g, maestrolink::nomeGrupo (gi), { c.getX() - 4, c.getBottom() + 4, cw + 8, 12 }, 8.5f, G.tem ? cTexto : cApagado,
               juce::Justification::centred, 0.18f, true);
    }

    // problemas, cada um com o grupo de onde vem
    const float px = r.getX() + 14 + 8 * (cw + 6) + 8, pw = r.getRight() - 14 - px;
    if (! ligado)
    {
        const juce::Rectangle<float> l (px, cy, pw, 26);
        g.setColour (cTela); g.fillRoundedRectangle (l, 3.0f);
        g.setColour (cVerde); g.fillRect (l.getX(), l.getY(), 3.0f, l.getHeight());
        texto (g, "NOT LINKED?", { l.getX() + 14, l.getY(), 120, l.getHeight() }, 10.0f, cVerde, juce::Justification::centredLeft, 0.2f, true);
        texto (g, "MAGOS FIXES IT ON THE MASTER ALONE (DYNAMIC EQ ABOVE)", { l.getX() + 140, l.getY(), pw - 150, l.getHeight() }, 10.0f,
               cTexto, juce::Justification::centredLeft, 0.12f);
        texto (g, "Put MAESTRO on your 8 groups in this project: MAGOS shows which group causes each problem on the master.",
               { px, cy + 36, pw, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.04f);
        texto (g, "Works when the DAW runs plugins in one process (Ableton, Logic, FL, Reaper, Studio One, Cubase).",
               { px, cy + 52, pw, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.04f);
        return;
    }
    for (size_t i = 0; i < problemas.size(); ++i)
    {
        const auto& p = problemas[i];
        const juce::Rectangle<float> l (px, cy - 6 + (float) i * 25, pw, 22);
        g.setColour (cTela); g.fillRoundedRectangle (l, 3.0f);
        g.setColour (p.cor); g.fillRect (l.getX(), l.getY(), 3.0f, l.getHeight());
        texto (g, p.rotulo, { l.getX() + 12, l.getY(), 126, l.getHeight() }, 9.5f, p.cor, juce::Justification::centredLeft, 0.12f, true);
        texto (g, p.texto, { l.getX() + 140, l.getY(), pw - 140 - 262, l.getHeight() }, 9.0f, cTexto, juce::Justification::centredLeft, 0.04f);
        texto (g, p.acao, { l.getRight() - 262, l.getY(), 254, l.getHeight() }, 9.0f, cOuro, juce::Justification::centredRight, 0.02f, true);
    }
}

// ------------------------------------------------------------------------------------------------------------------
// embaixo: LISTEN, CAST, knobs, MATCH, A / B / REF / BYPASS
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::base (juce::Graphics& g)
{
    auto& M = proc.motor;
    g.setGradientFill (juce::ColourGradient (cTopoB, 0, 858, cFundoB, 0, (float) baseH, false));
    g.fillRect (0.0f, 858.0f, (float) baseW, (float) baseH - 858.0f);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 858, (float) baseW, 858, 1.0f);
    const int modo = M.modo.load();
    const bool pisca = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;

    // LISTEN
    {
        const juce::Rectangle<float> b (14, 868, 150, 74);
        const bool on = modo == 1;
        g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff4a3320) : juce::Colour (0xff2c2838), 0, b.getY(),
                                                 on ? juce::Colour (0xff2c1e14) : juce::Colour (0xff1e1b28), 0, b.getBottom(), false));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (on ? cLaranja.withAlpha (0.7f) : cBorda); g.drawRoundedRectangle (b, 6.0f, 1.0f);
        led (g, { b.getCentreX(), b.getY() + 11 }, 3.5f, cLaranja, on && pisca);
        texto (g, "LISTEN", { b.getX(), b.getY() + 20, b.getWidth(), 28 }, 22.0f, on ? cLaranja : cTexto, juce::Justification::centred, 0.3f, true);
        const int s = juce::roundToInt (M.aprSegundos.load());
        const juce::String sub = on ? "LISTENING" + ponto + juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2)
                                    : juce::String ("ANALYZE TRACK + REF");
        texto (g, sub, { b.getX(), b.getY() + 50, b.getWidth(), 14 }, 9.0f, on ? cLaranja : cApagado, juce::Justification::centred, 0.2f);
        cliques.push_back ({ b, [this] { proc.motor.listen(); } });
    }
    // CAST
    {
        const juce::Rectangle<float> b (176, 868, 190, 74);
        const bool pronto = M.aprQuadros.load() > 4 || M.feitico.feito;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a3a78), 0, b.getY(), juce::Colour (0xff261d40), 0, b.getBottom(), false));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (cRoxo.withAlpha (pronto ? 0.9f : 0.4f));
        g.drawRoundedRectangle (b, 6.0f, modo == 2 ? 2.0f : 1.2f);
        const auto cor = pronto ? cOuro : cOuro.withAlpha (0.45f);
        g.setColour (cor);
        g.fillPath (brilho ({ b.getX() + 40, b.getY() + 29 }, 8));
        g.fillPath (brilho ({ b.getRight() - 40, b.getY() + 29 }, 8));
        texto (g, "CAST", { b.getX(), b.getY() + 14, b.getWidth(), 30 }, 24.0f, cor, juce::Justification::centred, 0.35f, true);
        juce::String sub = "ONE CLICK MASTER";
        if (modo == 2) sub = "CASTING" + reticencias + " " + juce::String (juce::roundToInt (M.castSegundos.load())) + " s";
        else if (M.feitico.feito) sub = "CAST" + ponto + "CLICK TO RECAST";
        texto (g, sub, { b.getX(), b.getY() + 48, b.getWidth(), 14 }, 9.0f, modo == 2 && pisca ? cOuro : cTexto, juce::Justification::centred, 0.22f);
        cliques.push_back ({ b, [this] { apertaCast(); } });
    }
    // knobs
    {
        const juce::Rectangle<float> k (378, 864, 336, 82);
        g.setColour (cTela.withAlpha (0.45f)); g.fillRoundedRectangle (k, 5.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (k, 5.0f, 1.0f);
        for (auto& kn : knobs) desenhaKnob (g, kn);
    }
    // MATCH TONE / LOUDNESS
    auto chave = [&] (juce::Rectangle<float> rr, const juce::String& t, bool on, std::function<void()> faz)
    {
        g.setColour (cTela.withAlpha (0.7f)); g.fillRoundedRectangle (rr, 4.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (rr, 4.0f, 1.0f);
        led (g, { rr.getX() + 14, rr.getCentreY() }, 3.5f, cVerde, on);
        texto (g, t, rr.withTrimmedLeft (26), 9.5f, on ? cTexto : cApagado, juce::Justification::centredLeft, 0.16f, true);
        cliques.push_back ({ rr, faz });
    };
    chave ({ 726, 870, 252, 32 }, "MATCH TONE TO REFERENCE", M.casaTom.load(), [this] { proc.motor.casaTom = ! proc.motor.casaTom.load(); });
    chave ({ 726, 908, 252, 32 }, "MATCH LOUDNESS TO REFERENCE", M.casaLoud.load(), [this] { proc.motor.casaLoud = ! proc.motor.casaLoud.load(); });

    // A / B / REF / BYPASS (parâmetro "listen")
    const int ouvir = juce::roundToInt (proc.valor ("listen"));
    auto ab = [&] (juce::Rectangle<float> rr, const juce::String& t, int v, juce::Colour corOn)
    {
        const bool on = ouvir == v;
        g.setColour (on ? corOn : cTela.withAlpha (0.6f)); g.fillRoundedRectangle (rr, 4.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (rr, 4.0f, 1.0f);
        texto (g, t, rr, t.length() <= 1 ? 18.0f : 11.0f, on ? juce::Colour (0xff120f1a) : cApagado, juce::Justification::centred,
               t.length() <= 1 ? 0.0f : 0.22f, true);
        cliques.push_back ({ rr, [this, v] { proc.define ("listen", (float) (proc.valor ("listen") == (float) v && v != 0 ? 0 : v)); } });
    };
    ab ({ 992, 874, 52, 60 }, "A", 0, cBranco);
    ab ({ 1050, 874, 52, 60 }, "B", 1, cRoxoClaro);
    ab ({ 1108, 874, 72, 60 }, "REF " + seta, 2, cOuro);
    ab ({ 1186, 874, 82, 60 }, "BYPASS", 3, cRuim);
    if (proc.sim.load() != 0)
        texto (g, "MONITOR SIMULATION ON" + ponto + "TURN OFF BEFORE EXPORT", { 726, 944, 540, 8 }, 7.5f, cRosa, juce::Justification::centredLeft, 0.2f, true);
}

void MagosEditor::desenhaKnob (juce::Graphics& g, Knob& k)
{
    auto* prm = proc.apvts.getParameter (k.id);
    const float v = prm != nullptr ? prm->getValue() : 0.0f;
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d283c), k.c.x, k.c.y - k.r, juce::Colour (0xff15121d), k.c.x, k.c.y + k.r, false));
    g.fillEllipse (juce::Rectangle<float> (k.r * 2, k.r * 2).withCentre (k.c));
    g.setColour (juce::Colours::black); g.drawEllipse (juce::Rectangle<float> (k.r * 2, k.r * 2).withCentre (k.c), 1.0f);
    juce::Path fundo, arco;
    fundo.addCentredArc (k.c.x, k.c.y, k.r - 3, k.r - 3, 0, a0, a1, true);
    g.setColour (cGradeF); g.strokePath (fundo, juce::PathStrokeType (2.8f));
    const float ini = k.centro ? (a0 + a1) * 0.5f : a0, fim = a0 + (a1 - a0) * v;
    if (std::abs (fim - ini) > 0.01f)
    {
        arco.addCentredArc (k.c.x, k.c.y, k.r - 3, k.r - 3, 0, std::min (ini, fim), std::max (ini, fim), true);
        g.setColour (k.cor); g.strokePath (arco, juce::PathStrokeType (2.8f));
    }
    g.setColour (cBranco);
    g.drawLine (k.c.x + std::sin (fim) * (k.r * 0.2f), k.c.y - std::cos (fim) * (k.r * 0.2f),
                k.c.x + std::sin (fim) * (k.r - 6), k.c.y - std::cos (fim) * (k.r - 6), 2.0f);
    texto (g, k.texto(), { k.c.x - 36, k.c.y + k.r + 3, 72, 13 }, 10.0f, cBranco, juce::Justification::centred, 0.0f, true);
    texto (g, k.rotulo, { k.c.x - 36, k.c.y + k.r + 16, 72, 11 }, 8.0f, cApagado, juce::Justification::centred, 0.2f);
}

// ------------------------------------------------------------------------------------------------------------------
// mouse
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (int i = 0; i < (int) knobs.size(); ++i)
        if (p.getDistanceFrom (knobs[(size_t) i].c) <= knobs[(size_t) i].r + 6)
        {
            arrastando = i;
            if (auto* prm = proc.apvts.getParameter (knobs[(size_t) i].id)) { valorIni = prm->getValue(); prm->beginChangeGesture(); }
            yIni = p.y;
            return;
        }
    for (auto it = cliques.rbegin(); it != cliques.rend(); ++it)
        if (it->r.contains (p)) { auto faz = it->faz; faz(); repaint(); return; }
}

void MagosEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (arrastando < 0) return;
    const float dy = yIni - e.position.y / escala();
    const float sens = e.mods.isShiftDown() ? 800.0f : 180.0f;
    if (auto* prm = proc.apvts.getParameter (knobs[(size_t) arrastando].id))
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, valorIni + dy / sens));
    repaint();
}

void MagosEditor::mouseUp (const juce::MouseEvent&)
{
    if (arrastando >= 0)
        if (auto* prm = proc.apvts.getParameter (knobs[(size_t) arrastando].id)) prm->endChangeGesture();
    arrastando = -1;
}

void MagosEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6)
            if (auto* prm = proc.apvts.getParameter (k.id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (k.padrao);
                prm->endChangeGesture();
                repaint();
                return;
            }
}

void MagosEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6)
            if (auto* prm = proc.apvts.getParameter (k.id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * 0.05f));
                prm->endChangeGesture();
                repaint();
                return;
            }
}

// ------------------------------------------------------------------------------------------------------------------
// referência: arrastar o arquivo para a janela ou escolher pelo chip REFERENCE
// ------------------------------------------------------------------------------------------------------------------
bool MagosEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac")
            || f.endsWithIgnoreCase (".ogg") || f.endsWithIgnoreCase (".mp3"))
            return true;
    return false;
}

void MagosEditor::filesDropped (const juce::StringArray& files, int, int)
{
    arrastandoArquivo = false;
    for (auto& f : files)
        if (isInterestedInFileDrag (juce::StringArray (f))) { proc.carregaReferencia (juce::File (f)); break; }
    repaint();
}

void MagosEditor::escolheArquivo()
{
    seletor = std::make_unique<juce::FileChooser> ("Reference track", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<MagosEditor> safe (this);
    seletor->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f.existsAsFile()) safe->proc.carregaReferencia (f);
    });
}

void MagosEditor::menuReferencia()
{
    juce::PopupMenu m;
    m.addSectionHeader ("REFERENCE TRACK");
    m.addItem (1, juce::String ("Load a reference") + reticencias);
    m.addItem (2, "Remove the reference (use the STYLE curve)", proc.nomeReferencia().isNotEmpty());
    m.addSeparator();
    m.addItem (-1, "Tip: drag a WAV / MP3 onto the window", false, false);
    if (proc.erroReferencia.isNotEmpty()) m.addItem (-2, proc.erroReferencia, false, false);
    else if (proc.nomeReferencia().isNotEmpty() && ! proc.arquivoReferencia.existsAsFile())
        m.addItem (-3, "File not found: REF listening is off (the analysis is kept)", false, false);
    juce::Component::SafePointer<MagosEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->escolheArquivo();
        if (r == 2) safe->proc.tiraReferencia();
    });
}

void MagosEditor::menuStyle()
{
    juce::PopupMenu m;
    m.addSectionHeader ("STYLE (target curve without a reference)");
    for (int s = 0; s < NSTYLE; ++s) m.addItem (s + 1, juce::String (nomeStyle (s)) + "   (" + num (lufsStyle (s), 1) + " LUFS)", true, s == proc.motor.style.load());
    m.addSeparator();
    m.addItem (-1, "A new STYLE takes effect on the next CAST", false, false);
    juce::Component::SafePointer<MagosEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->proc.motor.style = r - 1;
    });
}

void MagosEditor::menuAlvo()
{
    juce::PopupMenu m;
    m.addSectionHeader ("TARGET LOUDNESS (integrated)");
    m.addItem (1, "AUTO (reference, else STYLE)", true, proc.motor.alvoOpcao.load() == 0);
    m.addSeparator();
    static const char* dica[NALVO] = { "", "  streaming-safe", "", "", "", "  loud club", "  very loud", "  extreme" };
    for (int o = 1; o < NALVO; ++o) m.addItem (o + 1, num (lufsOpcao (o), 0) + " LUFS" + dica[o], true, proc.motor.alvoOpcao.load() == o);
    m.addSeparator();
    m.addItem (-1, "Ceiling: " + num (kTeto, 1) + " dBTP (true peak)", false, false);
    m.addItem (-2, "A new TARGET takes effect on the next CAST", false, false);
    juce::Component::SafePointer<MagosEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->proc.motor.alvoOpcao = r - 1;
    });
}

void MagosEditor::apertaCast()
{
    auto& M = proc.motor;
    if (M.aprQuadros.load() <= 4)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MASTER DOS MAGOS",
            "Press LISTEN first and play the loudest part of your track (the drop) for 20" + traco + "30 s. Then CAST.");
        return;
    }
    M.lancaCast();
}

void MagosEditor::menuLogo()
{
    juce::PopupMenu m;
    m.addItem (1, "How it works");
    m.addItem (2, "Reset (clear the CAST)", proc.motor.feitico.feito);
    m.addSeparator();
   #ifdef JucePlugin_VersionString
    m.addItem (-1, "MASTER DOS MAGOS " + juce::String (JucePlugin_VersionString) + "  (TEST)", false, false);
   #endif
    juce::Component::SafePointer<MagosEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 2) { safe->proc.motor.reset(); return; }
        if (r != 1) return;
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MASTER DOS MAGOS" + ponto + "MASTERING WIZARD",
            "Put it LAST on your master.\n\n"
            "1. Drop a REFERENCE track on the window (optional: without one, the STYLE curve is the target).\n"
            "2. LISTEN: play the loudest part of your track (the drop) for 20" + traco + "30 s.\n"
            "3. CAST: one click. MAGOS matches the tonal balance (TONE MATCH), sets the 4-band compressor from what it heard, "
            "turns on the DYNAMIC EQ (it only cuts MUD, HARSH and DE-ESS on the loud hits, and only lifts LOW PUNCH and AIR when "
            "they are missing) and drives the true peak limiter to the TARGET loudness. In the first ~20 s it fine-tunes the "
            "level by itself. If the limiter would need more than 5 dB, it stops before the target and tells you.\n\n"
            "AMOUNT scales everything. CHARACTER: clean to warm. PUNCH: slower attack = more transient. WIDTH: stereo of the "
            "upper bands (lows stay MONO below 120 Hz). TONE TILT: darker / brighter.\n"
            "A / B: B is your original mix AT THE SAME LOUDNESS (louder " + diferente + " better). REF plays the reference at the "
            "same loudness. BYPASS: the original, untouched.\n"
            "HEAR IT ON: phone, earbuds, car, mono. Monitoring only: turn it off before exporting.\n\n"
            "MAESTRO LINKED: with MAESTRO on your 8 groups in the same project, MAGOS shows which group causes each problem "
            "on the master, so you fix it at the source.");
    });
}
