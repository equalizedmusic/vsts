#include "PluginEditor.h"

using namespace maestro;

namespace
{
const juce::Colour cFundoA { 0xff1a1b20 }, cFundoB { 0xff121317 }, cTopoA { 0xff24262c }, cTopoB { 0xff1a1b20 },
                   cPainel { 0xff1c1e23 }, cBorda { 0xff2e3138 }, cTela { 0xff0d0e11 }, cCelula { 0xff16171b },
                   cTexto { 0xffe6e6ea }, cApagado { 0xff8a8f99 }, cOuro { 0xfff0b43c }, cVerde { 0xff3ddc84 },
                   cAzul { 0xff2bb3ff }, cMagenta { 0xffff4fd8 }, cBranco { 0xfff5f5f7 }, cRuim { 0xffff5a6e };

// KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX
const juce::Colour corRole[NR] = { juce::Colour (0xffff8a3d), juce::Colour (0xff2bb3ff), juce::Colour (0xffd9d9de),
                                   juce::Colour (0xff6aa9ff), juce::Colour (0xfff0b43c), juce::Colour (0xffb07cff),
                                   juce::Colour (0xffffd7a0), juce::Colour (0xffff5fa2) };

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}

float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }

// cor por temperatura (como a forma de onda colorida da CDJ-3000X): graves vermelho/laranja, médios amarelo,
// presença branco, agudos azul
juce::Colour corTemp (double f)
{
    struct Ponto { double f; juce::uint32 c; };
    static const Ponto p[] = { { 30, 0xffb3221a }, { 80, 0xffff3d1c }, { 200, 0xffff7a1c }, { 500, 0xffffb21c },
                               { 1100, 0xffffe04a }, { 2200, 0xfffff2b8 }, { 3800, 0xffffffff }, { 6000, 0xffc4dcff },
                               { 10000, 0xff5a95ff }, { 18000, 0xff2a5cff } };
    constexpr int n = (int) (sizeof (p) / sizeof (p[0]));
    if (f <= p[0].f) return juce::Colour (p[0].c);
    for (int i = 1; i < n; ++i)
        if (f <= p[i].f)
        {
            const double t = std::log (f / p[i - 1].f) / std::log (p[i].f / p[i - 1].f);
            return juce::Colour (p[i - 1].c).interpolatedWith (juce::Colour (p[i].c), (float) t);
        }
    return juce::Colour (p[n - 1].c);
}

const juce::String menos = juce::String::fromUTF8 ("\xe2\x88\x92");
const juce::String estrela = juce::String::fromUTF8 ("\xe2\x98\x85 ");
const juce::String ponto = juce::String::fromUTF8 (" \xc2\xb7 ");
const juce::String traco = juce::String::fromUTF8 ("\xe2\x80\x93");

juce::String db1 (float v)
{
    if (std::abs (v) < 0.05f) return "0.0 dB";
    return (v > 0 ? juce::String ("+") : menos) + juce::String (std::abs (v), 1) + " dB";
}

juce::String hz (double f)
{
    if (f >= 1000.0) return juce::String (f / 1000.0, f >= 10000.0 ? 0 : 1) + " kHz";
    return juce::String (juce::roundToInt (f)) + " Hz";
}

void painel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (cPainel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (cBorda);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void led (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour cor, bool aceso)
{
    if (aceso)
    {
        g.setColour (cor.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (r * 4, r * 4).withCentre (c));
        g.setColour (cor);
    }
    else g.setColour (juce::Colour (0xff34363d));
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
}

const char* nomeSpeed (int s) { return s == 0 ? "SLOW" : (s == 2 ? "FAST" : "MED"); }
int releaseMs (int s) { return s == 0 ? 300 : (s == 2 ? 60 : 120); }

double xLog (double f, double x0, double w, double f0, double f1) { return x0 + w * std::log (f / f0) / std::log (f1 / f0); }

// o dono (MaestroProcessor) ainda está ligado? Só a thread da interface cria e apaga plugins: vale até o fim da função
bool donoVivo (void* d)
{
    if (d == nullptr) return false;
    for (auto& s : Registry::get().slot)
        if (s.usado.load() && s.dono.load() == d) return true;
    return false;
}
} // namespace

// ------------------------------------------------------------------------------------------------------------------
MaestroEditor::MaestroEditor (MaestroProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (784, 490, 2240, 1400);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (784, 2240, w0) : baseW;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));

    auto* R = &Registry::get();

    Knob amount;
    amount.rotulo = "AMOUNT"; amount.c = { 384, 660 }; amount.r = 17; amount.padrao = 0.75f;
    amount.le = [R] { return R->amount.load(); };
    amount.grava = [R] (float v) { R->amount = v; };
    amount.texto = [R] { return juce::String (juce::roundToInt (R->amount.load() * 100.0f)) + "%"; };
    knobs.push_back (amount);

    Knob corte;
    corte.rotulo = "MAX CUT"; corte.c = { 444, 660 }; corte.r = 17; corte.padrao = 0.5f;
    corte.le = [R] { return R->maxCut.load() / 12.0f; };
    corte.grava = [R] (float v) { R->maxCut = std::round (v * 24.0f) / 2.0f; };
    corte.texto = [R] { return db1 (-R->maxCut.load()); };
    knobs.push_back (corte);

    Knob speed;
    speed.rotulo = "SPEED"; speed.c = { 504, 660 }; speed.r = 17; speed.padrao = 0.5f; speed.passos = 3;
    speed.le = [R] { return R->speed.load() * 0.5f; };
    speed.grava = [R] (float v) { R->speed = juce::jlimit (0, 2, juce::roundToInt (v * 2.0f)); };
    speed.texto = [R] { return juce::String (nomeSpeed (R->speed.load())); };
    knobs.push_back (speed);

    Knob trim;
    trim.rotulo = "TRIM"; trim.c = { 564, 660 }; trim.r = 17; trim.padrao = 0.5f;
    auto* prm = proc.apvts.getParameter ("trim");
    trim.le = [prm] { return prm->getValue(); };
    trim.grava = [prm] (float v) { prm->setValueNotifyingHost (v); };
    trim.texto = [prm] { return db1 (prm->convertFrom0to1 (prm->getValue())); };
    trim.gesto = [prm] (bool ini) { if (ini) prm->beginChangeGesture(); else prm->endChangeGesture(); };
    knobs.push_back (trim);

    colhe();
    startTimerHz (30);
}

void MaestroEditor::resized() { proc.uiW = getWidth(); }

void MaestroEditor::timerCallback()
{
    // o Ableton não manda o nome da faixa, mas o põe no título da janela do plugin: lê a cada segundo
    if (tique++ % 30 == 0)
        if (auto* peer = getPeer())
        {
            char b[512] {};
            if (maestro::tituloDaJanela (peer->getNativeHandle(), b, (int) sizeof (b)) > 0) proc.nomeDaJanela (juce::String::fromUTF8 (b));
        }
    colhe();
    repaint();
}

void MaestroEditor::colhe()
{
    auto& R = Registry::get();
    faixas.clear();
    for (int i = 0; i < NS; ++i)
    {
        auto& s = R.slot[i];
        if (s.usado.load() == 0) continue;
        Faixa f;
        f.vaga = i;
        f.ordem = s.ordem.load();
        f.role = juce::jlimit (0, NR - 1, s.role.load());
        f.roleAuto = s.roleAuto.load() != MANUAL;
        f.rolePeloSom = s.roleAuto.load() == PELO_SOM;
        f.shelf = s.shelf.load();
        const int nc = juce::jlimit (0, Slot::NCEDE, s.nCede.load());
        for (int k = 0; k < nc; ++k) if (const int v = s.cede[k].load(); v >= 0) f.cede.push_back (v);
        f.focus = s.focus.load() != 0;
        f.tocando = s.tocando.load() != 0;
        char b[40] {};
        s.leNome (b, (int) sizeof (b));
        f.nome = juce::String::fromUTF8 (b);
        for (int k = 0; k < NB; ++k) f.tela[k] = s.tela[k].load();
        for (int k = 0; k < NE; ++k) { f.corte[k] = s.corte[k].load(); f.tom[k] = s.tom[k].load(); }
        f.ganhoDb = s.ganhoDb.load();
        f.nivelDb = s.nivelDb.load();
        f.cedePara = s.cedePara.load();
        f.mascaraFoco = s.mascaraFoco.load();
        f.dono = s.dono.load();
        faixas.push_back (std::move (f));
    }
    std::sort (faixas.begin(), faixas.end(), [] (const Faixa& a, const Faixa& b) { return a.ordem < b.ordem; });
    // Registry cheio: esta faixa ficou de fora (vaga local) e aparece sozinha
    if (proc.engine().indice() < 0)
    {
        Faixa f;
        auto& s = proc.slot();
        f.vaga = -2; f.role = juce::jlimit (0, NR - 1, s.role.load()); f.focus = s.focus.load() != 0;
        for (int k = 0; k < NB; ++k) f.tela[k] = s.tela[k].load();
        for (int k = 0; k < NE; ++k) { f.corte[k] = s.corte[k].load(); f.tom[k] = s.tom[k].load(); }
        f.ganhoDb = s.ganhoDb.load();
        faixas.push_back (f);
    }
}

const MaestroEditor::Faixa* MaestroEditor::minha() const
{
    const int v = proc.engine().indice();
    for (auto& f : faixas) if (f.vaga == (v < 0 ? -2 : v)) return &f;
    return nullptr;
}

bool MaestroEditor::algumFocus() const
{
    for (auto& f : faixas) if (f.focus) return true;
    return false;
}

juce::String MaestroEditor::nomeDe (const Faixa& f) const
{
    if (f.nome.isNotEmpty()) return f.nome;
    int n = 0;
    for (auto& o : faixas)
    {
        if (o.role == f.role && o.nome.isEmpty()) ++n;
        if (&o == &f) break;
    }
    return juce::String (nomeRole (f.role)) + " " + juce::String (n).paddedLeft ('0', 2);
}

void MaestroEditor::desenhaTextoEsp (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float tam,
                                     juce::Colour cor, juce::Justification j, float kern, bool negrito)
{
    g.setColour (cor);
    g.setFont (fonte (tam, negrito).withExtraKerningFactor (kern));
    g.drawText (t, r, j, false);
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    g.setGradientFill (juce::ColourGradient (cFundoA, 0, 0, cFundoB, 0, (float) baseH, false));
    g.fillRect (0.0f, 0.0f, (float) baseW, (float) baseH);
    cliques.clear();

    topo (g);
    mapa (g, { 16, 68, 786, 356 });
    painelEq (g, { 16, 434, 786, 178 });
    direita (g, { 816, 68, 288, 544 });
    base (g);
}

void MaestroEditor::topo (juce::Graphics& g)
{
    auto& R = Registry::get();
    g.setGradientFill (juce::ColourGradient (cTopoA, 0, 0, cTopoB, 0, 56, false));
    g.fillRect (0.0f, 0.0f, (float) baseW, 56.0f);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 56, (float) baseW, 56, 1.0f);

    // marca: o M em ouro
    const auto fm = fonte (30.0f, true).withExtraKerningFactor (0.30f);
    g.setFont (fm);
    g.setColour (cOuro);
    g.drawText ("M", juce::Rectangle<float> (20, 10, 40, 36), juce::Justification::centredLeft);
    const float wM = largura (fm, "M");
    g.setColour (cBranco);
    g.drawText ("AESTRO", juce::Rectangle<float> (20 + wM, 10, 200, 36), juce::Justification::centredLeft);
    const float wTot = largura (fm, "MAESTRO");
    desenhaTextoEsp (g, "MIX CONDUCTOR", { 20 + wTot + 14, 22, 160, 16 }, 11.0f, cApagado, juce::Justification::centredLeft, 0.22f);
    cliques.push_back ({ { 14, 8, wTot + 12, 40 }, [this] { menuLogo(); } });

    // chips (da direita para a esquerda)
    const auto* eu = minha();
    float x = (float) baseW - 16;
    auto chip = [&] (const juce::String& rot, const juce::String& val, juce::Colour corLed, bool comLed, bool seta,
                     std::function<void()> faz)
    {
        const auto fr = fonte (11.0f).withExtraKerningFactor (0.16f), fv = fonte (11.5f, true).withExtraKerningFactor (0.14f);
        const float wr = rot.isNotEmpty() ? largura (fr, rot) + 8 : 0.0f, wv = largura (fv, val);
        const float w = 22 + (comLed ? 14.0f : 0.0f) + wr + wv + (seta ? 16.0f : 0.0f);
        x -= w;
        const juce::Rectangle<float> c (x, 14, w, 28);
        g.setColour (juce::Colour (0xff111216)); g.fillRoundedRectangle (c, 3.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (c, 3.0f, 1.0f);
        float tx = c.getX() + 11;
        if (comLed) { led (g, { tx + 4, c.getCentreY() }, 3.5f, corLed, true); tx += 14; }
        if (wr > 0)
        {
            g.setColour (cApagado); g.setFont (fr);
            g.drawText (rot, juce::Rectangle<float> (tx, c.getY(), wr, c.getHeight()), juce::Justification::centredLeft);
            tx += wr;
        }
        g.setColour (cBranco); g.setFont (fv);
        g.drawText (val, juce::Rectangle<float> (tx, c.getY(), wv + 2, c.getHeight()), juce::Justification::centredLeft);
        if (seta)
        {
            juce::Path p;
            const float sx = c.getRight() - 15, sy = c.getCentreY() - 2;
            p.addTriangle (sx, sy, sx + 8, sy, sx + 4, sy + 5);
            g.setColour (cApagado); g.fillPath (p);
        }
        if (faz) cliques.push_back ({ c, faz });
        x -= 10;
    };

    const int modo = R.modo.load();
    juce::String est = "IDLE"; juce::Colour cEst = cApagado;
    const bool cede = eu != nullptr && eu->cedePara >= 0;
    if (R.modoB.load()) { est = "B" + ponto + "ORIGINAL"; cEst = cRuim; }
    else if (eu != nullptr && eu->focus) { est = "FOCUS"; cEst = cOuro; }
    else if (cede) { est = "YIELDING"; cEst = cAzul; }
    else if (modo == 1) { est = "LEARNING"; cEst = cVerde; }
    else if (modo == 2) { est = "LOCKED"; cEst = cVerde; }
    chip ({}, est, cEst, true, false, {});
    chip ("LINKED", juce::String ((int) faixas.size()) + ((int) faixas.size() == 1 ? " TRACK" : " TRACKS"), cVerde, true, false, {});
    juce::String role = eu != nullptr ? juce::String (nomeRole (eu->role)) : juce::String ("?");
    if (eu != nullptr && eu->roleAuto) role = (eu->rolePeloSom ? juce::String ("GUESS") : juce::String ("AUTO")) + ponto + role;
    chip ("ROLE", role, {}, false, true, [this] { menuRole(); });
    chip ("TRACK", eu != nullptr ? nomeDe (*eu) : juce::String ("?"), {}, false, false, {});
    chip ("STYLE", nomeStyle (R.style.load()), {}, false, true, [this] { menuStyle(); });
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::mapa (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& R = Registry::get();
    painel (g, r);
    desenhaTextoEsp (g, "CONFLICT MAP" + ponto + "WHOLE PROJECT", { r.getX() + 12, r.getY() + 8, 300, 14 }, 10.5f, cApagado,
                     juce::Justification::centredLeft, 0.22f);
    const bool foco = algumFocus();
    {
        float lx = r.getRight() - 12;
        auto item = [&] (const juce::String& t, juce::Colour c, bool listra)
        {
            const auto f = fonte (10.0f).withExtraKerningFactor (0.16f);
            const float w = largura (f, t) + 2;
            lx -= w;
            g.setColour (cApagado); g.setFont (f);
            g.drawText (t, juce::Rectangle<float> (lx, r.getY() + 8, w, 14), juce::Justification::centredLeft);
            lx -= 20;
            g.setColour (c); g.fillRect (lx, r.getY() + 11, 14.0f, 8.0f);
            if (listra) { g.setColour (cBranco); g.fillRect (lx, r.getY() + 11, 14.0f, 2.0f); }
            lx -= 14;
        };
        if (foco)
        {
            item ("YIELDING (DIMMED + CUT)", corTemp (1500).withMultipliedBrightness (0.35f), true);
            item ("FOCUS (FULL COLOR)", corTemp (1500), false);
        }
        else
        {
            item ("BRIGHT = CONFLICT", corTemp (1500), false);
            item ("COLOR = FREQUENCY", corTemp (120), false);
        }
    }

    const juce::Rectangle<float> tela (r.getX() + 8, r.getY() + 28, r.getWidth() - 16, r.getHeight() - 36);
    g.setColour (cTela);
    g.fillRoundedRectangle (tela, 3.0f);

    const float x0 = tela.getX() + 106, y0 = tela.getY() + 8, wc = (tela.getRight() - 8 - x0) / NB, hr = 34.0f;
    // energia por ROLE e banda (a das faixas em FOCUS, à parte)
    std::vector<double> P ((size_t) (NR * NB), 0.0), PF ((size_t) (NR * NB), 0.0), tot ((size_t) NB, 0.0);
    int nRole[NR] {};
    bool focoRole[NR] {};
    float corteRole[NR][NE] {};
    bool tocando = false;
    for (auto& f : faixas)
    {
        ++nRole[f.role];
        tocando = tocando || f.tocando;
        for (int b = 0; b < NB; ++b)
        {
            P[(size_t) (f.role * NB + b)] += f.tela[b];
            tot[(size_t) b] += f.tela[b];
            if (f.focus) PF[(size_t) (f.role * NB + b)] += f.tela[b];
        }
        if (f.focus) focoRole[f.role] = true;
        for (int k = 0; k < NE; ++k) corteRole[f.role][k] = std::min (corteRole[f.role][k], f.corte[k]);
    }
    float pico = -200.0f;
    for (double v : P) pico = std::max (pico, dbDe (v));
    const float mc = std::max (0.5f, R.maxCut.load());
    const auto* eu = minha();

    for (int ro = 0; ro < NR; ++ro)
    {
        const float y = y0 + ro * (hr + 2.0f);
        // nome do ROLE (clique: as faixas dele, para FOCUS)
        const juce::Rectangle<float> rl (tela.getX() + 8, y, 96, hr);
        g.setColour (corRole[ro]);
        g.fillRect (rl.getX(), rl.getCentreY() - 4, 8.0f, 8.0f);
        desenhaTextoEsp (g, nomeRole (ro), { rl.getX() + 14, y, 64, hr }, 11.0f, nRole[ro] > 0 ? cTexto : cApagado.withAlpha (0.5f),
                         juce::Justification::centredLeft, 0.02f, true);
        desenhaTextoEsp (g, juce::String (nRole[ro]), { rl.getX() + 70, y, 16, hr }, 10.0f, cApagado, juce::Justification::centredRight, 0.0f);
        if (focoRole[ro])
            desenhaTextoEsp (g, estrela.trim(), { rl.getRight() - 8, y, 12, hr }, 11.0f, cOuro, juce::Justification::centred, 0.0f);
        if (nRole[ro] > 0) cliques.push_back ({ rl, [this, ro] { menuFaixasDoRole (ro); } });

        for (int b = 0; b < NB; ++b)
        {
            const juce::Rectangle<float> cel (x0 + b * wc + 1, y + 1, wc - 2, hr - 2);
            const double pr = P[(size_t) (ro * NB + b)];
            const float lr = dbDe (pr), lo = dbDe (tot[(size_t) b] - pr);
            const float pres = clampf ((lr - (pico - 48.0f)) / 36.0f, 0.0f, 1.0f);
            const float presO = clampf ((lo - (pico - 48.0f)) / 36.0f, 0.0f, 1.0f);
            float v;
            if (! foco) v = 0.22f * pres + 0.78f * pres * presO * clampf (1.0f - (lr - lo) / 15.0f, 0.0f, 1.0f);
            else if (focoRole[ro]) v = clampf ((dbDe (PF[(size_t) (ro * NB + b)]) - (pico - 48.0f)) / 36.0f, 0.0f, 1.0f);
            else v = 0.30f * pres;
            if (! tocando) v = std::max (v, mapaHold[ro][b] * 0.995f);   // transporte parado: o mapa apaga devagar
            mapaHold[ro][b] = v;
            const auto tc = corTemp (freqBanda (b));
            g.setColour (cCelula.interpolatedWith (tc, 0.06f + 0.90f * std::pow (v, 0.8f)));
            g.fillRect (cel);
            // tracinho do corte: quanto este ROLE está cedendo nesta banda
            if (foco && ! focoRole[ro])
            {
                const float c = -corteRole[ro][std::min (NE - 1, b / 3)];
                if (c > 0.3f)
                {
                    const float fr = clampf (c / mc, 0.0f, 1.0f);
                    g.setColour (cBranco.withAlpha (0.35f + 0.65f * fr));
                    g.fillRect (cel.getX(), cel.getY(), cel.getWidth(), 1.5f + 2.5f * fr);
                }
            }
        }
        if (eu != nullptr && eu->role == ro)
        {
            g.setColour (cBranco.withAlpha (0.85f));
            g.drawRect (juce::Rectangle<float> (x0 - 1, y, wc * NB + 2, hr), 1.5f);
        }
    }

    // régua de temperatura + eixo
    const float yR = y0 + NR * (hr + 2.0f) + 2;
    juce::ColourGradient gr (corTemp (40), x0, 0, corTemp (16000), x0 + wc * NB, 0, false);
    for (int i = 1; i < 10; ++i) gr.addColour (i / 10.0, corTemp (40.0 * std::pow (400.0, i / 10.0)));
    g.setGradientFill (gr);
    g.fillRect (x0, yR, wc * NB, 2.0f);
    const double fs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* rs[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 8; ++i)
    {
        const float bx = x0 + (float) (3.0 * std::log2 (fs[i] / 40.0) + 0.5) * wc;
        desenhaTextoEsp (g, rs[i], { bx - 20, yR + 3, 40, 12 }, 9.5f, cApagado, juce::Justification::centred, 0.0f);
    }
    if (R.guard.load())
    {
        const float gx = x0 + (float) (3.0 * std::log2 (fGuard / 40.0) + 0.5) * wc;
        const float dash[] = { 3.0f, 3.0f };
        g.setColour (cApagado.withAlpha (0.6f));
        g.drawDashedLine (juce::Line<float> (gx, y0, gx, yR - 2), dash, 2, 1.0f);
    }
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::painelEq (juce::Graphics& g, juce::Rectangle<float> r)
{
    painel (g, r);
    const auto* eu = minha();
    const juce::String nome = eu != nullptr ? nomeDe (*eu) : juce::String ("THIS TRACK");
    desenhaTextoEsp (g, nome + ponto + "TONE + DYNAMIC EQ", { r.getX() + 12, r.getY() + 8, 420, 14 }, 10.5f, cApagado,
                     juce::Justification::centredLeft, 0.22f);
    const bool cede = eu != nullptr && ! eu->focus && algumFocus();
    {
        float lx = r.getRight() - 12;
        auto item = [&] (const juce::String& t, juce::Colour c)
        {
            const auto f = fonte (10.0f).withExtraKerningFactor (0.16f);
            const float w = largura (f, t) + 2;
            lx -= w;
            g.setColour (cApagado); g.setFont (f);
            g.drawText (t, juce::Rectangle<float> (lx, r.getY() + 8, w, 14), juce::Justification::centredLeft);
            lx -= 20;
            g.setColour (c); g.fillRect (lx, r.getY() + 11, 14.0f, 8.0f);
            lx -= 14;
        };
        if (cede) item ("FOCUS", cBranco.withAlpha (0.6f));
        item ("LIVE CUT", cMagenta);
        item ("TONE MATCH", cBranco);
        item ("SPECTRUM", corTemp (900));
    }

    const juce::Rectangle<float> t (r.getX() + 8, r.getY() + 28, r.getWidth() - 16, r.getHeight() - 36);
    g.setColour (cTela);
    g.fillRoundedRectangle (t, 3.0f);
    const float px = t.getX() + 30, pw = t.getRight() - 10 - px, py = t.getY() + 8, ph = t.getHeight() - 26;
    const double f0 = 30.0, f1 = 18000.0;
    auto X = [&] (double f) { return (float) xLog (f, px, pw, f0, f1); };
    auto Y = [&] (double db) { return (float) (py + ph * (12.0 - juce::jlimit (-12.0, 12.0, db)) / 24.0); };
    for (int d = -12; d <= 12; d += 6)
    {
        g.setColour (juce::Colour (0xff1d1f25));
        g.drawHorizontalLine ((int) Y (d), px, px + pw);
        desenhaTextoEsp (g, d > 0 ? "+" + juce::String (d) : juce::String (d), { t.getX() + 2, Y (d) - 6, 24, 12 }, 9.0f, cApagado,
                         juce::Justification::centredRight, 0.0f);
    }
    const double fsG[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* rsG[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 8; ++i)
    {
        g.setColour (juce::Colour (0xff1d1f25));
        g.drawVerticalLine ((int) X (fsG[i]), py, py + ph);
        desenhaTextoEsp (g, rsG[i], { X (fsG[i]) - 20, py + ph + 3, 40, 12 }, 9.5f, cApagado, juce::Justification::centred, 0.0f);
    }

    if (proc.engine().guardAgora() < -0.3f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (juce::Rectangle<float> (px, py, X (fGuard) - px, ph));
        desenhaTextoEsp (g, "LOW SHELF " + db1 (proc.engine().guardAgora()) + " (GUARD)", { px + 4, py + 2, 200, 12 }, 9.5f, cApagado,
                         juce::Justification::centredLeft, 0.05f, true);
    }

    // espectro desta faixa (forma: o pico vai a +8 dB, 48 dB de faixa), cores por temperatura
    auto especY = [&] (float dbRel) { return Y (-12.0 + 20.0 * clampf ((dbRel + 48.0f) / 48.0f, 0.0f, 1.0f)); };
    if (eu != nullptr)
    {
        float pk = -200.0f;
        for (int b = 0; b < NB; ++b) pk = std::max (pk, dbDe (eu->tela[b]));
        if (pk > -100.0f)
        {
            juce::Path p;
            p.startNewSubPath (X (freqBanda (0)), Y (-12));
            for (int b = 0; b < NB; ++b) p.lineTo (X (freqBanda (b)), especY (dbDe (eu->tela[b]) - pk));
            p.lineTo (X (freqBanda (NB - 1)), Y (-12));
            p.closeSubPath();
            const float xa = X (freqBanda (0)), xb = X (freqBanda (NB - 1));
            juce::ColourGradient gr (corTemp (freqBanda (0)).withAlpha (0.55f), xa, 0, corTemp (freqBanda (NB - 1)).withAlpha (0.55f), xb, 0, false);
            for (int b = 3; b < NB; b += 3) gr.addColour ((X (freqBanda (b)) - xa) / (xb - xa), corTemp (freqBanda (b)).withAlpha (0.55f));
            g.setGradientFill (gr);
            g.fillPath (p);
        }
    }
    // espectro de quem tem FOCUS (tracejado)
    if (cede)
    {
        double F[NB] {};
        for (auto& f : faixas) if (f.focus) for (int b = 0; b < NB; ++b) F[b] += f.tela[b];
        float pk = -200.0f;
        for (int b = 0; b < NB; ++b) pk = std::max (pk, dbDe (F[b]));
        if (pk > -100.0f)
        {
            juce::Path p;
            for (int b = 0; b < NB; ++b)
            {
                const float xx = X (freqBanda (b)), yy = especY (dbDe (F[b]) - pk);
                if (b == 0) p.startNewSubPath (xx, yy); else p.lineTo (xx, yy);
            }
            juce::Path tr;
            const float dash[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.3f).createDashedStroke (tr, p, dash, 2);
            g.setColour (cBranco.withAlpha (0.6f));
            g.fillPath (tr);
        }
    }
    // curvas: TONE (branco) e o total ao vivo (magenta)
    auto curva = [&] (bool soTom, juce::Colour cor, float esp)
    {
        juce::Path p;
        for (int i = 0; i <= 160; ++i)
        {
            const double f = f0 * std::pow (f1 / f0, i / 160.0);
            const float yy = Y (proc.engine().respostaAgora (f, soTom));
            if (i == 0) p.startNewSubPath (X (f), yy); else p.lineTo (X (f), yy);
        }
        g.setColour (cor);
        g.strokePath (p, juce::PathStrokeType (esp));
    };
    curva (true, cBranco.withAlpha (0.9f), 1.6f);
    bool cortando = false;
    if (eu != nullptr) for (int k = 0; k < NE; ++k) cortando = cortando || eu->corte[k] < -0.3f;
    if (cortando || proc.engine().guardAgora() < -0.01f) curva (false, cMagenta, 2.0f);
    if (eu != nullptr)
        for (int k = 0; k < NE; ++k)
        {
            const double f = freqEq (k);
            if (std::abs (eu->tom[k]) > 0.2f)
            {
                g.setColour (cBranco);
                g.fillEllipse (juce::Rectangle<float> (6, 6).withCentre ({ X (f), Y (proc.engine().respostaAgora (f, true)) }));
            }
            if (eu->corte[k] < -0.8f)
            {
                const float yy = Y (proc.engine().respostaAgora (f, false));
                g.setColour (cMagenta);
                g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre ({ X (f), yy }));
                desenhaTextoEsp (g, db1 (eu->corte[k]), { X (f) - 30, yy + 5, 60, 12 }, 9.5f, cMagenta, juce::Justification::centred, 0.0f);
            }
        }
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::direita (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& R = Registry::get();
    const auto* eu = minha();
    const int modo = R.modo.load();
    const bool foco = algumFocus();
    const juce::String meuNome = eu != nullptr ? nomeDe (*eu) : juce::String ("THIS TRACK");
    float y = r.getY();
    const float x = r.getX(), w = r.getWidth();

    auto titulo = [&] (const juce::String& t, float yy)
    {
        desenhaTextoEsp (g, t, { x + 14, yy + 10, w - 28, 14 }, 10.5f, cApagado, juce::Justification::centredLeft, 0.22f);
    };
    auto linha = [&] (const juce::String& a, const juce::String& b, juce::Colour cb, float yy)
    {
        g.setColour (cTexto); g.setFont (fonte (12.5f));
        g.drawText (a, juce::Rectangle<float> (x + 14, yy, w - 120, 18), juce::Justification::centredLeft, true);
        g.setColour (cb); g.setFont (fonte (12.5f, true));
        g.drawText (b, juce::Rectangle<float> (x + w - 124, yy, 110, 18), juce::Justification::centredRight, false);
    };
    auto texto = [&] (const juce::String& t, float yy, float h, juce::Colour c = cApagado)
    {
        g.setColour (c); g.setFont (fonte (11.5f));
        g.drawFittedText (t, juce::Rectangle<int> ((int) x + 14, (int) yy, (int) w - 28, (int) h), juce::Justification::topLeft,
                          juce::jmax (1, (int) (h / 14)), 1.0f);
    };
    auto painelNivel = [&] (float yy)
    {
        const float h = 112;
        painel (g, { x, yy, w, h });
        titulo ("THIS TRACK" + ponto + "LEVEL", yy);
        const auto& t = proc.engine().travado;
        const float gNow = eu != nullptr ? eu->ganhoDb : 0.0f;
        g.setColour (t.travado ? cBranco : cApagado);
        g.setFont (fonte (30.0f, true));
        g.drawText (db1 (gNow), juce::Rectangle<float> (x + 14, yy + 28, w - 28, 36), juce::Justification::centredLeft);
        juce::String s;
        if (! t.travado) s = "Not locked yet: press LEARN, play the drop, then LOCK.";
        else if (t.nivelAntes < -150.0f) s = meuNome + " was silent during LEARN: level left as it was.";
        else
        {
            const float d = t.nivelAntes - t.alvoNivel;
            if (std::abs (d) < 0.5f) s = meuNome + " was already on the " + nomeStyle (R.style.load()) + " target.";
            else s = meuNome + " was " + juce::String (std::abs (d), 1) + (d > 0 ? " dB over" : " dB under") + " the "
                     + nomeStyle (R.style.load()) + " target for " + nomeRole (eu != nullptr ? eu->role : 0) + ".";
        }
        texto (s, yy + 68, 36);
        return h;
    };

    if (modo == 1 && ! (eu != nullptr && eu->focus))
    {
        const float h = 214;
        painel (g, { x, y, w, h });
        titulo ("LEARNING", y);
        const double agora = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        const int seg = juce::jmax (0, (int) (agora - R.learnIni.load()));
        g.setColour (cVerde); g.setFont (fonte (34.0f, true));
        g.drawText (juce::String (seg / 60) + ":" + juce::String (seg % 60).paddedLeft ('0', 2),
                    juce::Rectangle<float> (x + 14, y + 28, w - 28, 40), juce::Justification::centredLeft);
        int ouvidas = 0;
        for (auto& f : faixas) if (f.vaga >= 0 && R.slot[f.vaga].aprQuadros.load() > 0) ++ouvidas;
        linha ("TRACKS HEARD", juce::String (ouvidas) + " / " + juce::String ((int) faixas.size()), cTexto, y + 76);
        g.setColour (juce::Colour (0xff111216)); g.fillRect (x + 14, y + 98, w - 28, 6.0f);
        g.setColour (cVerde); g.fillRect (x + 14, y + 98, (w - 28) * (faixas.empty() ? 0.0f : (float) ouvidas / (float) faixas.size()), 6.0f);
        const int q = eu != nullptr && eu->vaga >= 0 ? R.slot[eu->vaga].aprQuadros.load() : 0;
        linha (meuNome, q > 0 ? "heard" + ponto + juce::String (R.slot[eu->vaga].aprNivelDb.load(), 1) + " dB" : juce::String ("not heard yet"),
               q > 0 ? cVerde : cApagado, y + 112);
        texto ("Play the busiest part of the song (the drop) with every track that matters, for 20" + traco + "30 s. Then press LOCK.", y + 140, 60);
        y += h + 10;
        painelNivel (y);
        return;
    }

    if (eu != nullptr && eu->focus)
    {
        float h = 110;
        painel (g, { x, y, w, h });
        titulo ("FOCUS", y);
        g.setColour (cOuro); g.setFont (fonte (28.0f, true));
        g.drawText (estrela + meuNome, juce::Rectangle<float> (x + 14, y + 28, w - 28, 38), juce::Justification::centredLeft, true);
        int outros = 0;
        for (auto& f : faixas) if (f.focus && &f != eu) ++outros;
        texto (outros > 0 ? "Others open space only while it plays. Shares the spotlight with " + juce::String (outros)
                                + (outros == 1 ? " other FOCUS." : " other FOCUS tracks.")
                          : "Others open space only while " + meuNome + " is playing.", y + 70, 32);
        y += h + 10;

        struct Cede { juce::String nome; float corte; double f; };
        std::vector<Cede> lista;
        for (auto& f : faixas)
        {
            if (&f == eu) continue;
            if (eu != nullptr && std::find (f.cede.begin(), f.cede.end(), eu->vaga) == f.cede.end()) continue;
            // só o corte que é para a FOCUS (o do KEEP KICK / BASS ON TOP não entra)
            int kMin = -1;
            for (int k = 0; k < NE; ++k)
                if ((f.mascaraFoco >> k) & 1)
                    if (kMin < 0 || f.corte[k] < f.corte[kMin]) kMin = k;
            if (kMin >= 0 && f.corte[kMin] < -0.5f) lista.push_back ({ (f.focus ? estrela : juce::String()) + nomeDe (f), f.corte[kMin], freqEq (kMin) });
        }
        std::sort (lista.begin(), lista.end(), [] (const Cede& a, const Cede& b) { return a.corte < b.corte; });
        h = 210;
        painel (g, { x, y, w, h });
        titulo ("MAKING ROOM" + ponto + "LIVE", y);
        if (lista.empty()) texto ("Nobody is in the way right now.", y + 34, 30);
        const int mostra = juce::jmin (7, (int) lista.size());
        for (int i = 0; i < mostra; ++i)
        {
            const float yy = y + 32 + i * 22;
            g.setColour (cTexto); g.setFont (fonte (12.5f));
            g.drawText (lista[(size_t) i].nome, juce::Rectangle<float> (x + 14, yy, w - 130, 18), juce::Justification::centredLeft, true);
            g.setColour (cAzul); g.setFont (fonte (12.5f, true));
            g.drawText (db1 (lista[(size_t) i].corte), juce::Rectangle<float> (x + w - 120, yy, 62, 18), juce::Justification::centredRight);
            g.setColour (cApagado); g.setFont (fonte (10.5f));
            g.drawText (hz (lista[(size_t) i].f), juce::Rectangle<float> (x + w - 56, yy, 44, 18), juce::Justification::centredRight);
        }
        if ((int) lista.size() > mostra)
            linha ("+ " + juce::String ((int) lista.size() - mostra) + " tracks", "< " + db1 (lista[(size_t) mostra - 1].corte), cAzul,
                   y + 32 + mostra * 22);
        y += h + 10;

        h = r.getBottom() - y;
        painel (g, { x, y, w, h });
        titulo ("FOCUS LIST", y);
        int i = 0;
        for (auto& f : faixas)
        {
            if (! f.focus) continue;
            const float yy = y + 32 + i * 22;
            if (yy > r.getBottom() - 22) break;
            linha (estrela + nomeDe (f), &f == eu ? juce::String ("this track") : juce::String (nomeRole (f.role)), cApagado, yy);
            ++i;
        }
        return;
    }

    if (foco)
    {
        juce::StringArray alvos;
        if (eu != nullptr)
            for (int v : eu->cede)
                for (auto& f : faixas) if (f.vaga == v) alvos.add (nomeDe (f));
        const bool cedendo = ! alvos.isEmpty();
        if (alvos.isEmpty()) for (auto& f : faixas) if (f.focus) alvos.add (nomeDe (f));
        const juce::String nomeAlvo = alvos.isEmpty() ? juce::String ("the FOCUS") : alvos.joinIntoString (" + ");
        float h = 120;
        painel (g, { x, y, w, h });
        titulo ("YIELDING TO", y);
        g.setColour (cedendo ? cAzul : cApagado); g.setFont (fonte (28.0f, true));
        g.setFont (fonte (alvos.size() > 1 ? 20.0f : 28.0f, true));
        g.drawFittedText (estrela + nomeAlvo, juce::Rectangle<int> ((int) x + 14, (int) y + 28, (int) w - 28, 38), juce::Justification::centredLeft, 1, 0.7f);
        if (cedendo)
            texto (meuNome + " dips only where and when " + nomeAlvo + " plays. Back to normal in "
                   + juce::String (releaseMs (R.speed.load())) + " ms.", y + 70, 44);
        else texto (meuNome + " is not in the way of the FOCUS right now.", y + 70, 44);
        y += h + 10;

        h = 150;
        painel (g, { x, y, w, h });
        titulo ("LIVE CUTS ON THIS TRACK", y);
        int n = 0;
        float pior = 0.0f;
        if (eu != nullptr)
        {
            std::vector<int> ks;
            for (int k = 0; k < NE; ++k) { if (eu->corte[k] < -0.3f) ks.push_back (k); pior = std::min (pior, eu->corte[k]); }
            std::sort (ks.begin(), ks.end(), [eu] (int a, int b) { return eu->corte[a] < eu->corte[b]; });
            if (eu->shelf < -0.3f) { linha ("< 120 Hz (LOW-END GUARD)", db1 (eu->shelf), cMagenta, y + 32); ++n; pior = std::min (pior, eu->shelf); }
            for (int k : ks)
            {
                if (n >= 3) break;
                linha (hz (freqEq (k)), db1 (eu->corte[k]), cMagenta, y + 32 + n * 22);
                ++n;
            }
        }
        if (n == 0) texto ("No cut right now.", y + 34, 20);
        const float mc = std::max (0.5f, R.maxCut.load());
        g.setColour (juce::Colour (0xff111216)); g.fillRect (x + 14, y + 104, w - 28, 8.0f);
        g.setColour (cMagenta); g.fillRect (x + 14, y + 104, (w - 28) * clampf (-pior / mc, 0.0f, 1.0f), 8.0f);
        desenhaTextoEsp (g, "MAX CUT " + db1 (-R.maxCut.load()) + ponto + "SPEED " + nomeSpeed (R.speed.load()),
                         { x + 14, y + 118, w - 28, 16 }, 10.0f, cApagado, juce::Justification::centredLeft, 0.16f);
        y += h + 10;
        painelNivel (y);
        return;
    }

    if (modo == 2)
    {
        const float h = painelNivel (y);
        y += h + 10;
        const float h2 = 150;
        painel (g, { x, y, w, h2 });
        titulo ("TONE MATCH", y);
        const float bx = x + 14, bw = (w - 28) / NE, zero = y + 84;
        const char* rs[] = { "50", "100", "200", "400", "800", "1.6k", "3.2k", "6.4k", "13k" };
        for (int k = 0; k < NE; ++k)
        {
            const float v = eu != nullptr ? eu->tom[k] : 0.0f;
            const float hh = std::min (4.0f, std::abs (v)) * 11.0f;
            g.setColour (corTemp (freqEq (k)).withAlpha (0.85f));
            if (v >= 0) g.fillRect (bx + k * bw + 3, zero - hh, bw - 6, hh);
            else g.fillRect (bx + k * bw + 3, zero, bw - 6, hh);
            desenhaTextoEsp (g, rs[k], { bx + k * bw - 4, y + 132, bw + 8, 12 }, 8.5f, cApagado, juce::Justification::centred, 0.0f);
            if (std::abs (v) >= 0.1f)
                desenhaTextoEsp (g, (v > 0 ? "+" : "") + juce::String (v, 1), { bx + k * bw - 4, v >= 0 ? zero - hh - 14 : zero + hh + 2, bw + 8, 12 },
                                 8.5f, cTexto, juce::Justification::centred, 0.0f);
        }
        g.setColour (cBorda); g.drawHorizontalLine ((int) zero, bx, bx + bw * NE);
        y += h2 + 10;
        const float h3 = r.getBottom() - y;
        painel (g, { x, y, w, h3 });
        titulo ("NEXT", y);
        texto ("Press FOCUS on the elements that must stand out (as many as you want), or click a group in the map. "
               "Everything else makes room only while they play.", y + 32, h3 - 40);
        return;
    }

    painel (g, { x, y, w, r.getHeight() });
    titulo ("GET STARTED", y);
    const juce::String passos[] = {
        "Put MAESTRO on every group (KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX) or on every track. " + juce::String ((int) faixas.size()) + ((int) faixas.size() == 1 ? " track" : " tracks") + " linked so far.",
        "Press LEARN and play the drop (20" + traco + "30 s).",
        "Press LOCK: every ROLE goes to the " + juce::String (nomeStyle (R.style.load())) + " level, with a gentle TONE MATCH.",
        "FOCUS what must stand out (one or more): the rest opens space only while it plays.",
        "A / B compares with the original mix at any time." };
    for (int i = 0; i < 5; ++i)
    {
        const float yy = y + 36 + i * 84;
        g.setColour (cOuro); g.setFont (fonte (22.0f, true));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (x + 14, yy, 24, 26), juce::Justification::centredLeft);
        g.setColour (cTexto); g.setFont (fonte (12.5f));
        g.drawFittedText (passos[i], juce::Rectangle<int> ((int) x + 42, (int) yy + 2, (int) w - 56, 70), juce::Justification::topLeft, 4, 1.0f);
    }
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::base (juce::Graphics& g)
{
    auto& R = Registry::get();
    g.setColour (juce::Colour (0xff15161a));
    g.fillRect (0.0f, 620.0f, (float) baseW, 80.0f);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 620, (float) baseW, 620, 1.0f);

    const int modo = R.modo.load();
    const auto* eu = minha();
    const bool pisca = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
    auto botao = [&] (juce::Rectangle<float> rr, const juce::String& t, bool on, juce::Colour cor, std::function<void()> faz)
    {
        g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff3a3d45) : juce::Colour (0xff2a2c32), rr.getX(), rr.getY(),
                                                 juce::Colour (0xff1e2025), rr.getX(), rr.getBottom(), false));
        g.fillRoundedRectangle (rr, 4.0f);
        g.setColour (on ? cor.withAlpha (0.8f) : cBorda);
        g.drawRoundedRectangle (rr, 4.0f, on ? 1.5f : 1.0f);
        led (g, { rr.getCentreX(), rr.getY() + 13 }, 4.0f, cor, on);
        desenhaTextoEsp (g, t, rr.withTrimmedTop (20), 16.0f, on ? cBranco : cApagado, juce::Justification::centred, 0.28f, true);
        cliques.push_back ({ rr, faz });
    };
    botao ({ 20, 632, 104, 56 }, "LEARN", modo == 1 && pisca, cVerde, []
    {
        Registry::get().learn (juce::Time::getMillisecondCounterHiRes() / 1000.0);
    });
    botao ({ 132, 632, 104, 56 }, "LOCK", modo == 2, cVerde, []
    {
        auto& RR = Registry::get();
        if (RR.modo.load() == 1) RR.lock();
        else if (RR.modo.load() == 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MAESTRO",
                "Press LEARN first and play the drop for 20" + traco + "30 s. Then LOCK.");
    });
    const bool euFoco = eu != nullptr && eu->focus;
    botao ({ 244, 632, 104, 56 }, "FOCUS", euFoco, euFoco ? cOuro : cAzul, [this] { proc.alternaFocus(); });

    for (auto& k : knobs) desenhaKnob (g, k);

    chave (g, { 612, 632, 190, 24 }, "LOW-END GUARD (SHELF)", R.guard.load() != 0, [] { auto& RR = Registry::get(); RR.guard = RR.guard.load() ? 0 : 1; });
    chave (g, { 612, 662, 190, 24 }, "KEEP KICK / BASS ON TOP", R.topo.load() != 0, [] { auto& RR = Registry::get(); RR.topo = RR.topo.load() ? 0 : 1; });

    desenhaTextoEsp (g, "ZERO LATENCY", { 814, 640, 110, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.16f);
    desenhaTextoEsp (g, "NO SIDECHAIN", { 814, 658, 110, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.16f);

    const bool B = R.modoB.load() != 0;
    auto ab = [&] (juce::Rectangle<float> rr, const juce::String& t, bool on, std::function<void()> faz)
    {
        g.setColour (on ? juce::Colour (0xffd8dae0) : juce::Colour (0xff22242a));
        g.fillRoundedRectangle (rr, 3.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (rr, 3.0f, 1.0f);
        desenhaTextoEsp (g, t, rr, 13.0f, on ? juce::Colour (0xff15161a) : cApagado, juce::Justification::centred, 0.0f, true);
        cliques.push_back ({ rr, faz });
    };
    ab ({ 934, 646, 32, 30 }, "A", ! B, [] { Registry::get().modoB = 0; });
    ab ({ 970, 646, 32, 30 }, "B", B, [] { Registry::get().modoB = 1; });
    const juce::Rectangle<float> rr (1012, 646, 92, 30);
    g.setColour (juce::Colour (0xff1e2025)); g.fillRoundedRectangle (rr, 3.0f);
    g.setColour (cBorda); g.drawRoundedRectangle (rr, 3.0f, 1.0f);
    desenhaTextoEsp (g, "RESET ALL", rr, 11.0f, cApagado, juce::Justification::centred, 0.2f);
    cliques.push_back ({ rr, [this] { confirmaReset(); } });
}

void MaestroEditor::chave (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, std::function<void()> faz)
{
    g.setColour (juce::Colour (0xff111216)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (cBorda); g.drawRoundedRectangle (r, 3.0f, 1.0f);
    led (g, { r.getX() + 13, r.getCentreY() }, 3.5f, cVerde, on);
    desenhaTextoEsp (g, t, r.withTrimmedLeft (24), 10.0f, on ? cTexto : cApagado, juce::Justification::centredLeft, 0.14f);
    cliques.push_back ({ r, faz });
}

void MaestroEditor::desenhaKnob (juce::Graphics& g, Knob& k)
{
    const float v = juce::jlimit (0.0f, 1.0f, k.le());
    desenhaTextoEsp (g, k.rotulo, { k.c.x - 34, k.c.y - k.r - 15, 68, 12 }, 9.0f, cApagado, juce::Justification::centred, 0.14f);
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    g.setColour (juce::Colour (0xff101114));
    g.fillEllipse (juce::Rectangle<float> (k.r * 2, k.r * 2).withCentre (k.c));
    juce::Path fundo, arco;
    fundo.addCentredArc (k.c.x, k.c.y, k.r - 2, k.r - 2, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff2a2c32));
    g.strokePath (fundo, juce::PathStrokeType (3.0f));
    arco.addCentredArc (k.c.x, k.c.y, k.r - 2, k.r - 2, 0, a0, a0 + (a1 - a0) * v, true);
    g.setColour (cOuro);
    g.strokePath (arco, juce::PathStrokeType (3.0f));
    const float a = a0 + (a1 - a0) * v;
    g.setColour (cBranco);
    g.drawLine (k.c.x + std::sin (a) * (k.r * 0.25f), k.c.y - std::cos (a) * (k.r * 0.25f),
                k.c.x + std::sin (a) * (k.r - 5), k.c.y - std::cos (a) * (k.r - 5), 2.0f);
    desenhaTextoEsp (g, k.texto(), { k.c.x - 34, k.c.y + k.r + 3, 68, 13 }, 11.0f, cTexto, juce::Justification::centred, 0.0f, true);
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (int i = 0; i < (int) knobs.size(); ++i)
        if (p.getDistanceFrom (knobs[(size_t) i].c) <= knobs[(size_t) i].r + 6)
        {
            arrastando = i;
            valorIni = knobs[(size_t) i].le();
            yIni = p.y;
            if (knobs[(size_t) i].gesto) knobs[(size_t) i].gesto (true);
            return;
        }
    // de trás para a frente: o que foi desenhado por último fica por cima
    for (auto it = cliques.rbegin(); it != cliques.rend(); ++it)
        if (it->r.contains (p)) { auto faz = it->faz; faz(); repaint(); return; }
}

void MaestroEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (arrastando < 0) return;
    auto& k = knobs[(size_t) arrastando];
    const float dy = yIni - e.position.y / escala();
    const float sens = e.mods.isShiftDown() ? 800.0f : 180.0f;
    float v = juce::jlimit (0.0f, 1.0f, valorIni + dy / sens);
    if (k.passos > 1) v = std::round (v * (float) (k.passos - 1)) / (float) (k.passos - 1);
    k.grava (v);
    repaint();
}

void MaestroEditor::mouseUp (const juce::MouseEvent&)
{
    if (arrastando >= 0 && knobs[(size_t) arrastando].gesto) knobs[(size_t) arrastando].gesto (false);
    arrastando = -1;
}

void MaestroEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6)
        {
            if (k.gesto) k.gesto (true);
            k.grava (k.padrao);
            if (k.gesto) k.gesto (false);
            repaint();
            return;
        }
}

void MaestroEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6)
        {
            float v = juce::jlimit (0.0f, 1.0f, k.le() + w.deltaY * 0.05f);
            if (k.passos > 1) v = std::round (v * (float) (k.passos - 1)) / (float) (k.passos - 1);
            if (k.gesto) k.gesto (true);
            k.grava (v);
            if (k.gesto) k.gesto (false);
            repaint();
            return;
        }
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::menuStyle()
{
    auto& R = Registry::get();
    juce::PopupMenu m;
    m.addSectionHeader ("STYLE (whole project)");
    for (int s = 0; s < NSTYLE; ++s) m.addItem (s + 1, nomeStyle (s), true, s == R.style.load());
    m.addSeparator();
    m.addItem (-1, "A new STYLE takes effect on the next LOCK", false, false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [] (int r) { if (r > 0) Registry::get().style = r - 1; });
}

void MaestroEditor::menuRole()
{
    const auto* eu = minha();
    const int atual = juce::roundToInt (proc.apvts.getRawParameterValue ("role")->load());
    juce::PopupMenu m;
    m.addSectionHeader ("ROLE of this track");
    m.addItem (1, "AUTO" + (eu != nullptr && atual == 0 ? juce::String (" (now: ") + nomeRole (eu->role) + ")" : juce::String()), true, atual == 0);
    m.addSeparator();
    for (int r = 0; r < NR; ++r) m.addItem (r + 2, nomeRole (r), true, atual == r + 1);
    juce::Component::SafePointer<MaestroEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.define ("role", (float) (r - 1)); });
}

void MaestroEditor::menuFaixasDoRole (int role)
{
    juce::PopupMenu m;
    m.addSectionHeader (juce::String (nomeRole (role)) + ponto + "click to FOCUS");
    std::vector<void*> donos;
    for (auto& f : faixas)
    {
        if (f.role != role || f.dono == nullptr) continue;
        donos.push_back (f.dono);
        m.addItem ((int) donos.size(), (f.focus ? estrela : juce::String()) + nomeDe (f), true, f.focus);
    }
    if (donos.empty()) return;
    m.addSeparator();
    m.addItem (1000, "FOCUS all " + juce::String (nomeRole (role)));
    m.addItem (1001, "Clear FOCUS in " + juce::String (nomeRole (role)));
    juce::Component::SafePointer<MaestroEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, donos] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         if (r >= 1000)
                         {
                             for (void* d : donos) if (donoVivo (d)) static_cast<MaestroProcessor*> (d)->defineFocus (r == 1000);
                         }
                         else if (r <= (int) donos.size() && donoVivo (donos[(size_t) r - 1]))
                             static_cast<MaestroProcessor*> (donos[(size_t) r - 1])->alternaFocus();
                         safe->repaint();
                     });
}

void MaestroEditor::confirmaReset()
{
    juce::Component::SafePointer<MaestroEditor> safe (this);
    auto opcoes = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "RESET ALL",
        "Clear LOCK (level and tone) and FOCUS on every linked track? The mix goes back to how it was.", "RESET ALL", "Cancel", this);
    juce::AlertWindow::showAsync (opcoes, [safe] (int r)
    {
        if (safe == nullptr || r != 1) return;
        Registry::get().reset();
        for (auto& s : Registry::get().slot)
            if (s.usado.load())
                if (auto* d = s.dono.load()) static_cast<MaestroProcessor*> (d)->defineFocus (false);
        safe->repaint();
    });
}

void MaestroEditor::menuLogo()
{
    juce::PopupMenu m;
    m.addItem (1, "How it works");
    m.addSeparator();
   #ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    m.addItem (-1, "MAESTRO " + juce::String (JucePlugin_VersionString), false, false);
   #endif
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [] (int r)
    {
        if (r != 1) return;
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MAESTRO" + ponto + "MIX CONDUCTOR",
            "Put MAESTRO on each of your 8 groups (stems): KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX, after your own "
            "effects. It also works track by track. All copies in the project talk to each other: no sidechain, no routing.\n\n"
            "LEARN: play the drop for 20" + traco + "30 s. Every group measures its level and tone while it plays.\n"
            "LOCK: each group goes to the level of the chosen STYLE, relative to the KICK, with a gentle TONE MATCH. "
            "Changing a ROLE after LOCK re-locks the whole project.\n"
            "LOW-END GUARD: while KICK and BASS play, the other groups dip below 120 Hz (dynamic low shelf). "
            "No high-pass anywhere; KICK and BASS are never cut by it.\n"
            "FOCUS: the tracks you pick (one or more) stand out. Everything else dips only in the frequencies where it masks "
            "them, and only while they play (dynamic EQ, up to MAX CUT). Two FOCUS tracks that clash share the space, "
            "a little each. KEEP KICK / BASS ON TOP does the same for the low end, under 400 Hz.\n\n"
            "ROLE comes from the track or group name (Kick, Bass, Drums, Snare, Percs, Hats, Synths, Lead, Ambience, Pad, "
            "Vocal, FX...). Without a name it is a GUESS from the sound: set it by hand if needed.\n"
            "AMOUNT scales everything. TRIM is a per-track offset. A / B compares with the original mix.\n\n"
            "Zero latency. Before LOCK and without FOCUS, the sound passes through untouched.\n"
            "Works when the DAW runs plugins in one process (Ableton Live, Logic, FL Studio, Reaper, Studio One, Cubase). "
            "With plugin sandboxing (Bitwig) each copy only sees its own track.");
    });
}
