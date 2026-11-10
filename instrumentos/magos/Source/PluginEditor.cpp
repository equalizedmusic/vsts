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

// eixo de frequência: 10 Hz – 35 kHz, log (o do Pro-Q 4)
constexpr double fMin = 10.0, fMax = 35000.0;
// analisador: a escala em dB (0 = senoide a 0 dBFS em 1 kHz) e a inclinação de 4,5 dB/oitava em 1 kHz (a do Pro-Q)
constexpr float vizTopo = 0.0f, vizBase = -90.0f, vizTilt = 4.5f;
// as curvas de 1/3 de oitava (alvo / STYLE / REFERÊNCIA) na mesma escala: potência por banda → densidade é −3 dB/oit,
// mais os 4,5 da inclinação = +1,5 dB/oitava
constexpr float tiltBandas = vizTilt - 3.0103f;
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

void MagosEditor::preparaAnalisador (double fs)
{
    fsViz = fs;
    int ordem = 13;                                            // ~170 ms de janela em qualquer taxa
    while ((double) (1 << ordem) < fs * 0.165 && ordem < 15) ++ordem;
    nViz = 1 << ordem;
    fftViz = std::make_unique<juce::dsp::FFT> (ordem);
    janViz.resize ((size_t) nViz);
    for (int i = 0; i < nViz; ++i) janViz[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / nViz));
    bufViz.assign ((size_t) (2 * nViz), 0.0f);
    const double df = fs / nViz, meia = std::pow (2.0, 1.0 / 48.0);   // média de 1/24 de oitava em volta do ponto
    pontos.assign (NP, {});
    for (int j = 0; j < NP; ++j)
    {
        auto& P = pontos[(size_t) j];
        P.f = fMin * std::pow (fMax / fMin, j / (double) (NP - 1));
        P.valido = P.f < 0.5 * fs * 0.995;
        P.k0 = juce::jlimit (1, nViz / 2, (int) std::ceil (P.f / meia / df));
        P.k1 = juce::jlimit (1, nViz / 2, (int) std::floor (P.f * meia / df));
        P.kf = P.f / df;
        P.interp = P.k1 - P.k0 < 1;                            // menos de 2 bins na janela (grave): interpola
    }
    for (int j = 0; j < NP; ++j) espIn[j] = espOut[j] = suaveIn[j] = suaveOut[j] = -150.0f;
    temEsp[0] = temEsp[1] = false;
    posVista[0] = posVista[1] = -1;
}

void MagosEditor::analisa (bool entrada)
{
    auto& M = proc.motor;
    const int e = entrada ? 0 : 1;
    const int pos = (entrada ? M.vizPosIn : M.vizPosOut).load (std::memory_order_acquire);
    const auto* anel = (entrada ? M.vizIn : M.vizOut).get();
    float* esp = entrada ? espIn : espOut;
    float* sv = entrada ? suaveIn : suaveOut;
    const bool parado = pos == posVista[e];                   // sem áudio novo (transporte parado sem processBlock)
    posVista[e] = pos;
    if (parado)
        for (int j = 0; j < NP; ++j) esp[j] = std::max (-150.0f, esp[j] - 1.5f);
    else
    {
        constexpr int mask = Engine::kVizN - 1;
        for (int i = 0; i < nViz; ++i) bufViz[(size_t) i] = anel[(pos - nViz + i) & mask].load (std::memory_order_relaxed) * janViz[(size_t) i];
        std::fill (bufViz.begin() + nViz, bufViz.end(), 0.0f);
        fftViz->performFrequencyOnlyForwardTransform (bufViz.data(), true);
        // |X| de uma senoide de amplitude A na janela de Hann = A·N/4: potência normalizada (0 dB = 0 dBFS)
        const float k = 4.0f / (float) nViz;
        for (int i = 0; i <= nViz / 2; ++i) { const float a = bufViz[(size_t) i] * k; bufViz[(size_t) i] = a * a; }
        auto dbBin = [&] (int i) { return 10.0f * std::log10 (bufViz[(size_t) juce::jlimit (1, nViz / 2, i)] + 1e-20f); };
        for (int j = 0; j < NP; ++j)
        {
            const auto& P = pontos[(size_t) j];
            if (! P.valido) { esp[j] = -150.0f; continue; }
            float d;
            if (P.interp)
            {
                // Catmull-Rom em dB entre os bins: o grave sai em curva, não em degraus
                const int b = (int) std::floor (P.kf);
                const float t = (float) (P.kf - b);
                const float p0 = dbBin (b - 1), p1 = dbBin (b), p2 = dbBin (b + 1), p3 = dbBin (b + 2);
                d = p1 + 0.5f * t * (p2 - p0 + t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + t * (3.0f * (p1 - p2) + p3 - p0)));
            }
            else
            {
                double s = 0;
                for (int i = P.k0; i <= P.k1; ++i) s += bufViz[(size_t) i];
                d = 10.0f * (float) std::log10 (s / (P.k1 - P.k0 + 1) + 1e-20);
            }
            d += vizTilt * (float) std::log2 (P.f / 1000.0);
            // balística: sobe rápido, cai em ~0,3 s
            esp[j] += (d > esp[j] ? 0.7f : 0.2f) * (d - esp[j]);
            esp[j] = std::max (esp[j], -150.0f);
        }
    }
    // 1/3 de oitava (±1/6 de oitava em volta): sobra vermelha, pontos da DYN EQ e o alinhamento do alvo
    const int r = juce::roundToInt ((NP - 1) / std::log2 (fMax / fMin) / 6.0);
    float topo = -200.0f;
    for (int j = 0; j < NP; ++j)
    {
        double s = 0;
        int c = 0;
        for (int i = std::max (0, j - r); i <= std::min (NP - 1, j + r); ++i)
            if (pontos[(size_t) i].valido) { s += std::pow (10.0, esp[i] / 10.0); ++c; }
        sv[j] = c > 0 ? 10.0f * (float) std::log10 (s / c + 1e-20) : -150.0f;
        if (pontos[(size_t) j].valido && pontos[(size_t) j].f > 30.0 && pontos[(size_t) j].f < 16000.0) topo = std::max (topo, esp[j]);
    }
    temEsp[e] = topo > vizBase;
}

void MagosEditor::timerCallback()
{
    // ANALISADOR: duas FFTs por quadro na thread da interface (o áudio só grava o anel)
    if (proc.motor.taxa() != fsViz) preparaAnalisador (proc.motor.taxa());
    analisa (true);
    analisa (false);
    {
        // o alvo (forma de média 0) se encaixa na média do DEPOIS (sem som na saída, na do ANTES) entre 80 Hz e 8 kHz
        auto& M = proc.motor;
        float alvo[NB];
        if (M.feitico.feito) std::copy (M.feitico.alvoForma, M.feitico.alvoForma + NB, alvo);
        else M.formaAlvo (alvo);
        const float* base = temEsp[1] ? suaveOut : temEsp[0] ? suaveIn : nullptr;
        if (base != nullptr)
        {
            double s = 0;
            int c = 0;
            for (int j = 0; j < NP; ++j)
            {
                const double f = pontos[(size_t) j].f;
                if (f < 80.0 || f > 8000.0 || ! pontos[(size_t) j].valido) continue;
                s += base[j] - (formaEm (alvo, f) + tiltBandas * (float) std::log2 (f / 1000.0));
                ++c;
            }
            if (c > 0)
            {
                const float o = (float) (s / c);
                offAlvo = offAlvoOk ? offAlvo + 0.06f * (o - offAlvo) : o;
                offAlvoOk = true;
            }
        }
    }
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
    ajustes.clear();

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
    chip ("TARGET", num (M.feitico.feito ? M.feitico.alvoLufs : M.lufsAlvo(), 1) + " LUFS" + ponto + num (kTeto, 1) + " dBTP", cBranco, false, {}, true,
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
    auto yDe = [&] (float db) { return tela.getY() + tela.getHeight() * (vizTopo - juce::jlimit (vizBase, vizTopo, db)) / (vizTopo - vizBase); };
    // grade: dB do analisador a cada 12 dB; frequências como no Pro-Q (10 Hz – 35 kHz)
    for (float d = vizTopo; d >= vizBase + 1.0f; d -= 12.0f)
    {
        g.setColour (d == vizTopo ? cGradeF : cGrade);
        g.drawHorizontalLine ((int) yDe (d), tela.getX(), tela.getRight());
        texto (g, num (d, 0, true), { r.getX() + 4, yDe (d) - 7, 36, 14 }, 9.0f, cApagado, juce::Justification::centredRight, 0.0f);
    }
    for (double dec : { 10.0, 100.0, 1000.0, 10000.0 })
        for (int m = 1; m <= 9; ++m)
        {
            const double f = dec * m;
            if (f < fMin || f > fMax) continue;
            const float xx = xDeF (f, tela);
            const bool rot = m == 1 || m == 2 || m == 5;
            g.setColour (rot ? cGrade.brighter (0.25f) : cGrade);
            g.drawVerticalLine ((int) xx, tela.getY(), tela.getBottom());
            if (rot && f >= 20.0)
                texto (g, f >= 1000.0 ? juce::String (juce::roundToInt (f / 1000.0)) + "k" : juce::String (juce::roundToInt (f)),
                       { xx - 20, tela.getBottom() + 2, 40, 13 }, 9.0f, cApagado, juce::Justification::centred, 0.0f);
        }

    // MATCH %: as formas de 1/3 de oitava do motor (as mesmas do CAST); a tela mostra o analisador
    float depois[NB], alvo[NB], pOut[NB];
    for (int b = 0; b < NB; ++b) pOut[b] = M.telaOut[b].load();
    formaDb (pOut, depois);
    if (M.feitico.feito) std::copy (M.feitico.alvoForma, M.feitico.alvoForma + NB, alvo);
    else M.formaAlvo (alvo);
    const bool pronto = ! pontos.empty();
    const bool somIn = pronto && temEsp[0], somOut = pronto && temEsp[1];

    // alvo na escala do analisador (+1,5 dB/oit) e encaixado na média do som; desenhado de 20 Hz a 20 kHz
    auto alvoEm = [&] (double f) { return formaEm (alvo, f) + tiltBandas * (float) std::log2 (f / 1000.0) + offAlvo; };
    constexpr double aMin = 20.0, aMax = 20000.0;
    auto fAlvo = [&] (int i) { return aMin * std::pow (aMax / aMin, i / 200.0); };
    // espectro: linha e preenchimento até o pé da tela; termina em Nyquist
    auto espectro = [&] (const float* esp, bool fechado)
    {
        juce::Path p;
        bool aberto = false;
        float xUlt = tela.getX();
        for (int j = 0; j < NP; ++j)
        {
            if (! pontos[(size_t) j].valido) break;
            const float xx = xDeF (pontos[(size_t) j].f, tela), yy = yDe (esp[j]);
            if (! aberto) { p.startNewSubPath (xx, fechado ? tela.getBottom() : yy); if (fechado) p.lineTo (xx, yy); aberto = true; }
            else p.lineTo (xx, yy);
            xUlt = xx;
        }
        if (fechado && aberto) { p.lineTo (xUlt, tela.getBottom()); p.closeSubPath(); }
        return p;
    };
    g.saveState();
    g.reduceClipRegion (tela.toNearestInt());
    // ANTES: preenchimento escuro + linha fina (o "pre" do Pro-Q)
    if (somIn)
    {
        g.setColour (cCinza.withAlpha (0.16f));
        g.fillPath (espectro (espIn, true));
        g.setColour (cCinza.withAlpha (0.75f));
        g.strokePath (espectro (espIn, false), juce::PathStrokeType (1.1f));
    }
    // DEPOIS: preenchimento em degradê
    if (somOut)
    {
        g.setGradientFill (juce::ColourGradient (cCiano.withAlpha (0.30f), 0, tela.getY(), cCiano.withAlpha (0.04f), 0, tela.getBottom(), false));
        g.fillPath (espectro (espOut, true));
    }
    // zona alvo (±2 dB)
    if (offAlvoOk)
    {
        juce::Path z;
        for (int i = 0; i <= 200; ++i)
        {
            const double f = fAlvo (i);
            const float xx = xDeF (f, tela), yy = yDe (alvoEm (f) + 2.0f);
            if (i == 0) z.startNewSubPath (xx, yy); else z.lineTo (xx, yy);
        }
        for (int i = 200; i >= 0; --i) z.lineTo (xDeF (fAlvo (i), tela), yDe (alvoEm (fAlvo (i)) - 2.0f));
        z.closeSubPath();
        g.setColour (cOuro.withAlpha (0.12f));
        g.fillPath (z);
    }
    // sobra acima da zona (vermelho): o DEPOIS em 1/3 de oitava
    if (somOut && offAlvoOk)
        for (int j = 0; j + 1 < NP; ++j)
        {
            const auto& P = pontos[(size_t) j];
            if (! P.valido || ! pontos[(size_t) (j + 1)].valido) break;
            if (P.f < 30.0 || P.f > 18000.0) continue;
            const float teto = alvoEm (P.f) + 2.0f, sobra = suaveOut[j] - teto;
            if (sobra <= 0.0f) continue;
            g.setColour (cRuim.withAlpha (juce::jlimit (0.15f, 0.55f, sobra * 0.15f)));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xDeF (P.f, tela), yDe (suaveOut[j]), xDeF (pontos[(size_t) (j + 1)].f, tela) + 0.5f, yDe (teto)));
        }
    if (offAlvoOk)
    {
        juce::Path c, tr;
        for (int i = 0; i <= 200; ++i)
        {
            const float xx = xDeF (fAlvo (i), tela), yy = yDe (alvoEm (fAlvo (i)));
            if (i == 0) c.startNewSubPath (xx, yy); else c.lineTo (xx, yy);
        }
        const float dash[] = { 6.0f, 4.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (tr, c, dash, 2);
        g.setColour (cOuro);
        g.fillPath (tr);
    }
    if (somOut)
    {
        g.setColour (cCiano);
        g.strokePath (espectro (espOut, false), juce::PathStrokeType (1.5f));
    }
    // pontos da DYN EQ no DEPOIS (1/3 de oitava)
    auto suaveEm = [&] (double f)
    {
        const double u = juce::jlimit (0.0, (double) (NP - 1), (NP - 1) * std::log (f / fMin) / std::log (fMax / fMin));
        const int a = juce::jmin (NP - 2, (int) u);
        return suaveOut[a] + (suaveOut[a + 1] - suaveOut[a]) * (float) (u - a);
    };
    for (int z = 0; z < NZ; ++z)
    {
        const float v = zonaHold[z];
        if (std::abs (v) < 0.3f || ! somOut) continue;
        const auto& Z = zona (z);
        const float xx = xDeF (Z.f, tela), yy = yDe (suaveEm (Z.f));
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

    // faixa DYNAMIC EQ · LIVE (AUTO) ou os controles de cada ponto (MANUAL)
    const juce::Rectangle<float> fx (tela.getX(), tela.getBottom() + 20, tela.getWidth(), 50);
    g.setColour (cTela);
    g.fillRoundedRectangle (fx, 3.0f);
    const bool eqMan = proc.valor ("eqman") > 0.5f;
    chaveAutoManual (g, { fx.getRight() - 124, fx.getY() + 3, 118, 15 }, eqMan, [this] (bool on) { ligaManualEq (on); });
    if (eqMan)
    {
        texto (g, "DYNAMIC EQ" + ponto + "MANUAL" + ponto + "DRAG THE VALUES", { fx.getX() + 8, fx.getY() + 3, 400, 14 }, 9.0f, cOuro,
               juce::Justification::centredLeft, 0.2f, true);
        const float cw = (fx.getWidth() - 8) / (float) NZ, cy = fx.getY() + 20, ch = 27;
        for (int z = 0; z < NZ; ++z)
        {
            const auto& Z = zona (z);
            const float x = fx.getX() + 4 + z * cw, v = zonaHold[z];
            const juce::String id = "eq" + juce::String (z);
            const bool on = proc.valor (id + "on") > 0.5f;
            const juce::Rectangle<float> cl (x, cy, 72, ch);
            led (g, { x + 8, cl.getCentreY() }, 3.5f, std::abs (v) >= 0.3f ? (v < 0 ? cRosa : cAzul) : cVerde, on);
            texto (g, Z.nome, { x + 15, cy, 58, 15 }, 8.5f, on ? cTexto : cApagado, juce::Justification::centredLeft, 0.1f, true);
            texto (g, on ? (std::abs (v) >= 0.1f ? num (v, 1, true) + " dB" : juce::String ("IDLE")) : juce::String ("OFF"),
                   { x + 15, cy + 14, 58, 12 }, 8.0f, on && std::abs (v) >= 0.1f ? (v < 0 ? cRosa : cAzul) : cApagado,
                   juce::Justification::centredLeft, 0.1f, std::abs (v) >= 0.1f);
            cliques.push_back ({ cl, [this, id, on] { proc.define (id + "on", on ? 0.0f : 1.0f); } });
            const auto cor = Z.sinal < 0 ? cRosa : cAzul;
            valorAjuste (g, { x + 74, cy, 36, ch }, id + "rng", (Z.sinal < 0 ? menos : juce::String ("+")) + juce::String (proc.valor (id + "rng"), 1),
                         Z.sinal < 0 ? "MAX CUT" : "MAX LIFT", cor);
            valorAjuste (g, { x + 112, cy, 36, ch }, id + "thr", juce::String (proc.valor (id + "thr"), 1), "THRESH", cor);
        }
        return;
    }
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
    const bool man = proc.valor ("mbman") > 0.5f;
    titulo (g, r, "MULTIBAND COMP", "4 BANDS" + ponto + (man ? juce::String ("MANUAL") + ponto + "DRAG THE VALUES"
                                                             : (proc.nomeReferencia().isNotEmpty() ? "AUTO FROM THE REFERENCE" : "AUTO FROM YOUR TRACK")));
    texto (g, "LINEAR PHASE  120" + ponto + "1.2k" + ponto + "6.5k Hz", { r.getRight() - 300, r.getY() + 10, 286, 16 }, 10.0f, cOuro,
           juce::Justification::centredRight, 0.18f, true);
    chaveAutoManual (g, { r.getRight() - 360, r.getY() + 9, 118, 18 }, man, [this] (bool on) { ligaManualMulti (on); });
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

        // limiar e razão (AUTO: só mostra; MANUAL: arrasta)
        const juce::String id = "mb" + juce::String (k);
        const float raz = man ? proc.valor (id + "rat") : 1.0f + (razBase[k] - 1.0f) * (0.6f + 0.8f * quente) * (0.4f + 0.6f * amt);
        auto mostrador = [&] (float cy, float frac, const juce::String& val, const juce::String& rot, const juce::String& idAj)
        {
            const juce::Point<float> cc (c.getRight() - 46, cy);
            if (man)
            {
                const juce::Rectangle<float> a (cc.x - 32, cy - 18, 64, 59);
                g.setColour (corBanda[k].withAlpha (0.08f)); g.fillRoundedRectangle (a, 4.0f);
                g.setColour (corBanda[k].withAlpha (0.45f)); g.drawRoundedRectangle (a, 4.0f, 1.0f);
                ajustes.push_back ({ a, idAj });
            }
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
        const float lim = man ? proc.valor (id + "thr") : M.limiarAgora[k].load();
        const bool ativo = feito || man;
        mostrador (mt + 16, ativo ? (lim + 60.0f) / 60.0f : 1.0f, ativo ? num (lim, 1) : juce::String ("OFF"), "THRESH", id + "thr");
        mostrador (mt + 76, (raz - 1.0f) / (man ? 9.0f : 4.0f), juce::String (raz, 1) + ":1", "RATIO", id + "rat");

        if (man)
        {
            const float bw = (c.getWidth() - 24) / 3.0f, by = c.getBottom() - 56;
            auto ms = [] (float v) { return v < 10.0f ? juce::String (v, 1) : juce::String (juce::roundToInt (v)); };
            valorAjuste (g, { c.getX() + 8, by, bw, 28 }, id + "atk", ms (proc.valor (id + "atk")) + " ms", "ATTACK", corBanda[k]);
            valorAjuste (g, { c.getX() + 12 + bw, by, bw, 28 }, id + "rel", ms (proc.valor (id + "rel")) + " ms", "RELEASE", corBanda[k]);
            valorAjuste (g, { c.getX() + 16 + 2 * bw, by, bw, 28 }, id + "gain", num (proc.valor (id + "gain"), 1, true) + " dB", "GAIN", corBanda[k]);
        }
        else
            texto (g, "GR " + num (gr, 1) + " dB" + ponto + "ATK/REL " + juce::String (juce::roundToInt (atqBase[k] * fp)) + " / "
                   + juce::String (juce::roundToInt (relBase[k])) + " ms", { c.getX(), c.getBottom() - 46, c.getWidth(), 13 }, 9.5f, cTexto,
                   juce::Justification::centred, 0.04f);
        const juce::String modoTxt = man ? "MANUAL" : "AUTO";
        const auto fA = fonte (8.5f, true).withExtraKerningFactor (0.2f);
        const float wA = largura (fA, modoTxt) + 12, wP = largura (fA, papel[k]) + 12, x0 = c.getCentreX() - (wA + wP + 6) * 0.5f;
        const juce::Rectangle<float> ca (x0, c.getBottom() - 24, wA, 16), cp (x0 + wA + 6, c.getBottom() - 24, wP, 16);
        g.setColour (corBanda[k].withAlpha (0.6f)); g.drawRoundedRectangle (ca, 3.0f, 1.0f);
        texto (g, modoTxt, ca, 8.5f, corBanda[k], juce::Justification::centred, 0.2f, true);
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
    // selos — quebram em linhas para nunca passar da borda do painel; a última linha fica no rodapé
    struct Selo { juce::String t; juce::Colour c; float w; int linha; };
    std::vector<Selo> selos;
    const auto fSelo = fonte (8.5f, true).withExtraKerningFactor (0.2f);
    auto selo = [&] (const juce::String& t, juce::Colour c) { selos.push_back ({ t, c, largura (fSelo, t) + 14, 0 }); };
    if (M.clipou.load()) selo ("CLIPPED", cRuim); else selo ("NO CLIPPING", cVerde);
    if (proc.valor ("monobass") > 0.5f) selo ("BASS MONO < 120 Hz", cOuro);
    if (M.alvoAlto.load()) selo ("TARGET TOO LOUD FOR THIS MIX", cLaranja);
    const float limite = r.getRight() - 14;
    int linhas = 1;
    {
        float sx = x;
        for (auto& s : selos)
        {
            if (sx > x && sx + s.w > limite) { ++linhas; sx = x; }
            s.linha = linhas - 1;
            sx += s.w + 6;
        }
    }
    {
        float sx = x; int atual = 0;
        for (const auto& s : selos)
        {
            if (s.linha != atual) { atual = s.linha; sx = x; }
            const float y = r.getBottom() - 30 - 22.0f * (float) (linhas - 1 - s.linha);
            const juce::Rectangle<float> q (sx, y, juce::jmin (s.w, limite - sx), 18);
            g.setColour (s.c.withAlpha (0.12f)); g.fillRoundedRectangle (q, 3.0f);
            g.setColour (s.c.withAlpha (0.7f)); g.drawRoundedRectangle (q, 3.0f, 1.0f);
            texto (g, s.t, q, 8.5f, s.c, juce::Justification::centred, 0.2f, true);
            sx += s.w + 6;
        }
    }
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
// MANUAL
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::chaveAutoManual (juce::Graphics& g, juce::Rectangle<float> r, bool manual, std::function<void (bool)> faz)
{
    g.setColour (cTela.withAlpha (0.8f)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (cBorda); g.drawRoundedRectangle (r, 3.0f, 1.0f);
    const auto a = r.withWidth (r.getWidth() * 0.42f), m = r.withTrimmedLeft (r.getWidth() * 0.42f);
    g.setColour (manual ? cOuro : cRoxoClaro);
    g.fillRoundedRectangle ((manual ? m : a).reduced (1.5f), 2.5f);
    const auto escuro = juce::Colour (0xff120f1a);
    texto (g, "AUTO", a, 8.5f, manual ? cApagado : escuro, juce::Justification::centred, 0.2f, true);
    texto (g, "MANUAL", m, 8.5f, manual ? escuro : cApagado, juce::Justification::centred, 0.2f, true);
    cliques.push_back ({ a, [faz] { faz (false); } });
    cliques.push_back ({ m, [faz] { faz (true); } });
}

void MagosEditor::valorAjuste (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& id, const juce::String& val,
                               const juce::String& rot, juce::Colour cor)
{
    const bool ativo = idArrasto == id;
    g.setColour (cor.withAlpha (ativo ? 0.22f : 0.08f)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (cor.withAlpha (ativo ? 0.9f : 0.45f)); g.drawRoundedRectangle (r, 3.0f, 1.0f);
    texto (g, val, r.withHeight (r.getHeight() * 0.58f).translated (0, 1), 9.5f, cBranco, juce::Justification::centred, 0.0f, true);
    texto (g, rot, r.withTrimmedTop (r.getHeight() * 0.56f), 7.0f, cApagado, juce::Justification::centred, 0.05f);
    ajustes.push_back ({ r, id });
}

const MagosEditor::Ajuste* MagosEditor::ajusteEm (juce::Point<float> p) const
{
    for (auto it = ajustes.rbegin(); it != ajustes.rend(); ++it)
        if (it->r.contains (p)) return &*it;
    return nullptr;
}

void MagosEditor::ligaManualMulti (bool on)
{
    if (on == (proc.valor ("mbman") > 0.5f)) return;
    if (on)
    {
        // copia o AUTO de agora (com CAST; sem CAST, os padrões de cada banda)
        static const float atqBase[NM] = { 30.0f, 15.0f, 5.0f, 2.0f }, relBase[NM] = { 160.0f, 120.0f, 80.0f, 60.0f };
        static const float razBase[NM] = { 2.0f, 2.5f, 3.0f, 1.6f };
        auto& M = proc.motor;
        const bool feito = M.feitico.feito;
        const float amt = proc.valor ("amount") / 100.0f, quente = proc.valor ("character") / 100.0f;
        const float fp = std::pow (2.0f, proc.valor ("punch") / 2.5f);
        for (int k = 0; k < NM; ++k)
        {
            const juce::String id = "mb" + juce::String (k);
            proc.define (id + "thr", feito ? M.limiarAgora[k].load() : -20.0f);
            proc.define (id + "rat", 1.0f + (razBase[k] - 1.0f) * (0.6f + 0.8f * quente) * (0.4f + 0.6f * amt));
            proc.define (id + "atk", atqBase[k] * fp);
            proc.define (id + "rel", relBase[k]);
            proc.define (id + "gain", 0.0f);
        }
    }
    proc.define ("mbman", on ? 1.0f : 0.0f);
    repaint();
}

void MagosEditor::ligaManualEq (bool on)
{
    if (on == (proc.valor ("eqman") > 0.5f)) return;
    if (on)
    {
        const float amt = proc.valor ("amount") / 100.0f;
        for (int z = 0; z < NZ; ++z)
        {
            const juce::String id = "eq" + juce::String (z);
            proc.define (id + "on", 1.0f);
            proc.define (id + "rng", zona (z).max * amt);
            proc.define (id + "thr", 1.5f);
        }
    }
    proc.define ("eqman", on ? 1.0f : 0.0f);
    repaint();
}

// ------------------------------------------------------------------------------------------------------------------
// mouse
// ------------------------------------------------------------------------------------------------------------------
void MagosEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (const auto* a = ajusteEm (p))
        if (auto* prm = parametro (a->id))
        {
            idArrasto = a->id;
            valorIni = prm->getValue();
            prm->beginChangeGesture();
            yIni = p.y;
            repaint();
            return;
        }
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
    const float dy = yIni - e.position.y / escala();
    const float sens = e.mods.isShiftDown() ? 800.0f : 180.0f;
    if (idArrasto.isNotEmpty())
    {
        if (auto* prm = parametro (idArrasto)) prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, valorIni + dy / sens));
        repaint();
        return;
    }
    if (arrastando < 0) return;
    if (auto* prm = proc.apvts.getParameter (knobs[(size_t) arrastando].id))
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, valorIni + dy / sens));
    repaint();
}

void MagosEditor::mouseUp (const juce::MouseEvent&)
{
    if (idArrasto.isNotEmpty())
    {
        if (auto* prm = parametro (idArrasto)) prm->endChangeGesture();
        idArrasto = {};
        repaint();
    }
    if (arrastando >= 0)
        if (auto* prm = proc.apvts.getParameter (knobs[(size_t) arrastando].id)) prm->endChangeGesture();
    arrastando = -1;
}

void MagosEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (const auto* a = ajusteEm (p))
        if (auto* prm = parametro (a->id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getDefaultValue());
            prm->endChangeGesture();
            repaint();
            return;
        }
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
    if (const auto* a = ajusteEm (p))
        if (auto* prm = parametro (a->id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * 0.05f));
            prm->endChangeGesture();
            repaint();
            return;
        }
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
            "MANUAL (MULTIBAND COMP and DYNAMIC EQ): click MANUAL and drag any framed value up / down (SHIFT = fine, "
            "double click = default, mouse wheel too). It starts from what AUTO was doing. Per band: THRESH, RATIO, ATTACK, "
            "RELEASE, GAIN. Per DYNAMIC EQ point: click the name to turn it on / off, MAX CUT (or MAX LIFT) and THRESH "
            "(how many dB away from the target before it acts). AUTO goes back to the automatic settings.\n"
            "A / B: B is your original mix AT THE SAME LOUDNESS (louder " + diferente + " better). REF plays the reference at the "
            "same loudness. BYPASS: the original, untouched.\n"
            "HEAR IT ON: phone, earbuds, car, mono. Monitoring only: turn it off before exporting.\n\n"
            "MAESTRO LINKED: with MAESTRO on your 8 groups in the same project, MAGOS shows which group causes each problem "
            "on the master, so you fix it at the source.");
    });
}
