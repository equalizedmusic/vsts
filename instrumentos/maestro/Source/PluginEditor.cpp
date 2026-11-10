#include "PluginEditor.h"

using namespace maestro;

namespace
{
// paleta do ALIGN / BAREBASS: escuro arroxeado; ROXO = marca e FOCUS, VERDE = o que o MAESTRO faz agora (LIVE EQ,
// cortes, LOCK), LARANJA = grave (shelf do GUARD), ROSA-VERMELHO = briga (conflito), BRANCO = espectro
const juce::Colour cFundoA { 0xff1b1726 }, cFundoB { 0xff110f18 }, cTopoA { 0xff241f33 }, cTopoB { 0xff17141f },
                   cChapa { 0xff1a1725 }, cBorda { 0xff2e2842 }, cTexto { 0xffdcd7ea }, cApagado { 0xff8b84a3 },
                   cTela { 0xff07060c }, cGrade { 0xff181425 }, cGradeF { 0xff2b2442 },
                   cLaranja { 0xffff8a3d }, cVerde { 0xff3fd68a }, cRoxo { 0xffa77bff },
                   cRoxoClaro { 0xffc9b0ff }, cBranco { 0xfff3efff }, cRuim { 0xffff4f8b },
                   cTira { 0xff15121e }, cPista { 0xff1e1a2a };

// KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX
const juce::Colour corRole[NR] = { juce::Colour (0xffff8a3d), juce::Colour (0xff45a6ff), juce::Colour (0xfff5c451),
                                   juce::Colour (0xffe6e1f5), juce::Colour (0xffc9b0ff), juce::Colour (0xff4fd1c5),
                                   juce::Colour (0xffff7fb5), juce::Colour (0xffc4e86b) };

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}

float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }

const juce::String menos = juce::String::fromUTF8 ("\xe2\x88\x92");
const juce::String estrela = juce::String::fromUTF8 ("\xe2\x98\x85");
const juce::String ponto = juce::String::fromUTF8 (" \xc2\xb7 ");
const juce::String traco = juce::String::fromUTF8 ("\xe2\x80\x93");
const juce::String setaBaixo = juce::String::fromUTF8 ("\xe2\x96\xbc");
const juce::String vezes = juce::String::fromUTF8 (" \xc3\x97 ");
const juce::String alvoGlifo = juce::String::fromUTF8 ("\xe2\x97\x89");

juce::String db1 (float v, bool unidade = true)
{
    juce::String s;
    if (std::abs (v) < 0.05f) s = "0.0";
    else s = (v > 0 ? juce::String ("+") : menos) + juce::String (std::abs (v), 1);
    return unidade ? s + " dB" : s;
}

juce::String hzCurto (double f)
{
    // juce::String (double, 0) NÃO arredonda (0 casas = precisão total) — por isso o roundToInt
    if (f >= 10000.0) return juce::String (juce::roundToInt (f / 1000.0)) + "k";
    if (f >= 1000.0)  return juce::String (f / 1000.0, 1) + "k";
    return juce::String (juce::roundToInt (f));
}

void caixa (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (cChapa);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black);
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
    else g.setColour (juce::Colour (0xff2c2838));
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
}

const char* nomeSpeed (int s) { return s == 0 ? "SLOW" : (s == 2 ? "FAST" : "MED"); }

// eixo de frequência das telas: 20 Hz – 20 kHz, log
constexpr double fMin = 20.0, fMax = 20000.0;
double fDeX (double x, double w) { return fMin * std::pow (fMax / fMin, x / w); }
double xDeF (double f, double w) { return w * std::log (f / fMin) / std::log (fMax / fMin); }

// dB de um espectro de 27 bandas em qualquer frequência (interpola na escala das bandas)
float dbEm (const double* tela, double f)
{
    const double u = juce::jlimit (0.0, (double) (NB - 1), 3.0 * std::log2 (f / 40.0));
    const int a = juce::jmin (NB - 2, (int) u);
    const double t = u - a;
    const double da = dbDe (tela[a] + 1e-14), db = dbDe (tela[a + 1] + 1e-14);
    return (float) (da + (db - da) * t);
}

// |H(f)| de uma EQ (tom, corte e shelf) em dB, para as curvas (taxa da tela: 48 kHz)
double respostaEq (const float* tom, const float* corte, float shelf, double f, bool soTom)
{
    constexpr double fs = 48000.0;
    double db = 0;
    for (int k = 0; k < NE; ++k)
    {
        const float g = tom[k] + (soTom ? 0.0f : corte[k]);
        if (std::abs (g) > 1e-3f) db += respostaDb (sino (fs, freqEq (k), qOitava, g), fs, f);
    }
    if (! soTom && shelf < -0.01f) db += respostaDb (shelfGrave (fs, fGuard, shelf), fs, f);
    return db;
}

// o dono (MaestroProcessor) ainda está ligado? Só a thread da interface cria e apaga plugins: vale até o fim da função
bool donoVivo (void* d)
{
    if (d == nullptr) return false;
    for (auto& s : Registry::get().slot)
        if (s.usado.load() && s.dono.load() == d) return true;
    return false;
}

void tracejado (juce::Graphics& g, const juce::Path& p, float esp, float a, float b)
{
    juce::Path tr;
    const float dash[] = { a, b };
    juce::PathStrokeType (esp).createDashedStroke (tr, p, dash, 2);
    g.fillPath (tr);
}
} // namespace

// ------------------------------------------------------------------------------------------------------------------
MaestroEditor::MaestroEditor (MaestroProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (896, 560, 2560, 1600);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (896, 2560, w0) : baseW;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));

    auto* R = &Registry::get();

    // WHOLE MIX: valem para todos os grupos
    Knob amount;
    amount.rotulo = "AMOUNT"; amount.c = { 290, 752 }; amount.r = 15; amount.padrao = 0.75f; amount.cor = cRoxo;
    amount.le = [R] { return R->amount.load(); };
    amount.grava = [R] (float v) { R->amount = v; };
    amount.texto = [R] { return juce::String (juce::roundToInt (R->amount.load() * 100.0f)) + "%"; };
    knobs.push_back (amount);

    Knob corte;
    corte.rotulo = "MAX CUT"; corte.c = { 358, 752 }; corte.r = 15; corte.padrao = 0.5f; corte.cor = cRoxo;
    corte.le = [R] { return R->maxCut.load() / 12.0f; };
    corte.grava = [R] (float v) { R->maxCut = std::round (v * 24.0f) / 2.0f; };
    corte.texto = [R] { return db1 (-R->maxCut.load()); };
    knobs.push_back (corte);

    Knob speed;
    speed.rotulo = "SPEED"; speed.c = { 426, 752 }; speed.r = 15; speed.padrao = 0.5f; speed.passos = 3; speed.cor = cRoxo;
    speed.le = [R] { return R->speed.load() * 0.5f; };
    speed.grava = [R] (float v) { R->speed = juce::jlimit (0, 2, juce::roundToInt (v * 2.0f)); };
    speed.texto = [R] { return juce::String (nomeSpeed (R->speed.load())); };
    knobs.push_back (speed);

    // TRIM de cada grupo (todas as cópias do grupo; a posição desenhada é a da tira)
    for (int gi = 0; gi < NR; ++gi)
    {
        Knob t;
        t.rotulo = "TRIM"; t.r = 14; t.padrao = 0.5f; t.cor = corRole[gi];
        auto param = [] (MaestroProcessor* d) { return d->apvts.getParameter ("trim"); };
        t.ativo = [this, gi] { return ! donosDoGrupo (gi).empty(); };
        t.le = [this, gi, param]
        {
            auto ds = donosDoGrupo (gi);
            return ds.empty() ? 0.5f : param (ds.front())->getValue();
        };
        t.grava = [this, gi, param] (float v) { for (auto* d : donosDoGrupo (gi)) param (d)->setValueNotifyingHost (v); };
        t.texto = [this, gi, param]
        {
            auto ds = donosDoGrupo (gi);
            if (ds.empty()) return juce::String ("-");
            auto* p0 = param (ds.front());
            return db1 (p0->convertFrom0to1 (p0->getValue()));
        };
        t.gesto = [this, gi, param] (bool ini)
        {
            for (auto* d : donosDoGrupo (gi)) { if (ini) param (d)->beginChangeGesture(); else param (d)->endChangeGesture(); }
        };
        knobs.push_back (t);
    }

    colhe();
    startTimerHz (30);
    ativacao.anexar (*this);
}

void MaestroEditor::resized() { proc.uiW = getWidth(); }

void MaestroEditor::timerCallback()
{
    // nome da faixa pelo título da janela do plugin (reforço do nome que o DAW manda): lê a cada segundo
    if (tique++ % 30 == 0)
        if (auto* peer = getPeer())
        {
            char b[512] {};
            if (maestro::tituloDaJanela (peer->getNativeHandle(), b, (int) sizeof (b)) > 0) proc.nomeDaJanela (juce::String::fromUTF8 (b));
        }
    colhe();
    guardaHistorico();
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
        f.fonte = s.roleAuto.load();
        f.shelf = s.shelf.load();
        f.alvoDb = s.alvoDb.load();
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
        f.vaga = -2; f.role = juce::jlimit (0, NR - 1, s.role.load()); f.focus = s.focus.load() != 0; f.dono = &proc;
        for (int k = 0; k < NB; ++k) f.tela[k] = s.tela[k].load();
        for (int k = 0; k < NE; ++k) { f.corte[k] = s.corte[k].load(); f.tom[k] = s.tom[k].load(); }
        f.ganhoDb = s.ganhoDb.load(); f.nivelDb = s.nivelDb.load(); f.shelf = s.shelf.load(); f.alvoDb = s.alvoDb.load();
        faixas.push_back (f);
    }

    // grupos
    const auto* eu = minha();
    bool algumTocando = false;
    for (auto& f : faixas) algumTocando = algumTocando || f.tocando;
    for (int gi = 0; gi < NR; ++gi)
    {
        auto& G = grupos[gi];
        G = Grupo {};
        double pot = 0;
        float alvo = -200.0f, maisAlto = -1000.0f;
        for (auto& f : faixas)
        {
            if (f.role != gi) continue;
            G.faixas.push_back (&f);
            for (int b = 0; b < NB; ++b) G.tela[b] += f.tela[b];
            for (int k = 0; k < NE; ++k) G.corte[k] = std::min (G.corte[k], f.corte[k]);
            G.shelf = std::min (G.shelf, f.shelf);
            G.focus = G.focus || f.focus;
            G.tocando = G.tocando || f.tocando;
            pot += potDe (f.nivelDb + f.ganhoDb);
            if (f.alvoDb > -150.0f) alvo = f.alvoDb;   // o alvo é do ROLE inteiro: igual em todas
            if (f.nivelDb > maisAlto) { maisAlto = f.nivelDb; G.rep = &f; }
            for (int v : f.cede)
                for (auto& o : faixas)
                    if (o.vaga == v && std::find (G.cedeGrupos.begin(), G.cedeGrupos.end(), o.role) == G.cedeGrupos.end())
                        G.cedeGrupos.push_back (o.role);
        }
        if (eu != nullptr && eu->role == gi) G.rep = eu;
        G.nivelDb = dbDe (pot);
        G.alvoDb = alvo;
        G.ganhoDb = G.rep != nullptr ? G.rep->ganhoDb : 0.0f;
        // transporte parado: o mapa segura o último espectro e apaga devagar
        for (int b = 0; b < NB; ++b)
        {
            if (algumTocando) telaHold[gi][b] = G.tela[b];
            else { telaHold[gi][b] *= 0.995; G.tela[b] = std::max (G.tela[b], telaHold[gi][b]); }
        }
    }
    double pk = -200.0;
    for (auto& G : grupos) if (! G.faixas.empty()) for (int b = 0; b < NB; ++b) pk = std::max (pk, (double) dbDe (G.tela[b]));
    pico = std::max (pk, pico - 0.15);
    if (pk > -150.0 && pico > pk + 24.0) pico = pk + 24.0;
}

void MaestroEditor::guardaHistorico()
{
    for (int gi = 0; gi < NR; ++gi)
    {
        for (int k = 0; k < NE; ++k) hist[gi][histPos][k] = grupos[gi].corte[k];
        hist[gi][histPos][NE] = grupos[gi].shelf;
    }
    histPos = (histPos + 1) % HN;
}

const MaestroEditor::Faixa* MaestroEditor::minha() const
{
    const int v = proc.engine().indice();
    for (auto& f : faixas) if (f.vaga == (v < 0 ? -2 : v)) return &f;
    return nullptr;
}

int MaestroEditor::meuGrupo() const
{
    const auto* eu = minha();
    return eu != nullptr ? eu->role : juce::jlimit (0, NR - 1, proc.slot().role.load());
}

std::vector<MaestroProcessor*> MaestroEditor::donosDoGrupo (int gi) const
{
    std::vector<MaestroProcessor*> r;
    for (auto& f : faixas)
        if (f.role == gi && donoVivo (f.dono)) r.push_back (static_cast<MaestroProcessor*> (f.dono));
    if (r.empty() && proc.engine().indice() < 0 && meuGrupo() == gi) r.push_back (&proc);
    return r;
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
    if (sel < 0 || sel >= NR) sel = meuGrupo();

    topo (g);
    console (g, { 14, 62, 604, 638 });
    mapa (g, { 630, 62, 636, 300 });
    painelEq (g, { 630, 370, 636, 330 });
    base (g);
}

void MaestroEditor::topo (juce::Graphics& g)
{
    auto& R = Registry::get();
    g.setGradientFill (juce::ColourGradient (cTopoA, 0, 0, cTopoB, 0, 52, false));
    g.fillRect (0.0f, 0.0f, (float) baseW, 52.0f);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 52, (float) baseW, 52, 1.0f);

    // marca: o M em roxo, como no ALIGN
    const auto fm = fonte (28.0f, true).withExtraKerningFactor (0.28f);
    g.setFont (fm);
    g.setColour (cRoxo);
    g.drawText ("M", juce::Rectangle<float> (20, 8, 40, 36), juce::Justification::centredLeft);
    const float wM = largura (fm, "M");
    g.setColour (cBranco);
    g.drawText ("AESTRO", juce::Rectangle<float> (20 + wM, 8, 220, 36), juce::Justification::centredLeft);
    const float wTot = largura (fm, "MAESTRO");
    desenhaTextoEsp (g, "MIX CONDUCTOR", { 20 + wTot + 14, 20, 160, 16 }, 11.0f, cApagado, juce::Justification::centredLeft, 0.22f);
    cliques.push_back ({ { 14, 6, wTot + 12, 40 }, [this] { menuLogo(); } });

    float x = (float) baseW - 16;
    auto chip = [&] (const juce::String& rot, const juce::String& val, juce::Colour corVal, juce::Colour corLed, bool comLed,
                     bool seta, std::function<void()> faz)
    {
        const auto fr = fonte (11.0f).withExtraKerningFactor (0.14f), fv = fonte (11.5f, true).withExtraKerningFactor (0.12f);
        const float wr = rot.isNotEmpty() ? largura (fr, rot) + 8 : 0.0f, wv = largura (fv, val);
        const float w = 22 + (comLed ? 14.0f : 0.0f) + wr + wv + (seta ? 16.0f : 0.0f);
        x -= w;
        const juce::Rectangle<float> c (x, 12, w, 28);
        g.setColour (cTela.withAlpha (0.7f)); g.fillRoundedRectangle (c, 3.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (c, 3.0f, 1.0f);
        float tx = c.getX() + 11;
        if (comLed) { led (g, { tx + 4, c.getCentreY() }, 3.5f, corLed, true); tx += 14; }
        if (wr > 0)
        {
            g.setColour (cApagado); g.setFont (fr);
            g.drawText (rot, juce::Rectangle<float> (tx, c.getY(), wr, c.getHeight()), juce::Justification::centredLeft);
            tx += wr;
        }
        g.setColour (corVal); g.setFont (fv);
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
    int nFoco = 0;
    for (auto& G : grupos) if (G.focus) ++nFoco;
    juce::String est = "IDLE"; juce::Colour cEst = cApagado;
    if (R.modoB.load()) { est = "B" + ponto + "ORIGINAL"; cEst = cRuim; }
    else if (modo == 1) { est = "LEARNING"; cEst = cLaranja; }
    else if (nFoco > 0) { est = estrela + " FOCUS" + (nFoco > 1 ? " x" + juce::String (nFoco) : juce::String()); cEst = cRoxo; }
    else if (modo == 2) { est = "LOCKED"; cEst = cVerde; }
    chip ({}, est, cBranco, cEst, true, false, {});
    const int mg = meuGrupo();
    chip ("THIS WINDOW", nomeRole (mg), corRole[mg], {}, false, true, [this, mg] { menuRoleDoGrupo (mg); });
    int nG = 0;
    for (auto& G : grupos) if (! G.faixas.empty()) ++nG;
    const int nF = (int) faixas.size();
    chip ("LINKED", juce::String (nG) + " / 8 GROUPS" + (nF != nG ? ponto + juce::String (nF) + " TRACKS" : juce::String()), cBranco,
          nG > 0 ? cVerde : cApagado, true, false, {});
    chip ("STYLE", nomeStyle (R.style.load()), cBranco, {}, false, true, [this] { menuStyle(); });
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::console (juce::Graphics& g, juce::Rectangle<float> r)
{
    caixa (g, r);
    desenhaTextoEsp (g, "CONSOLE", { r.getX() + 10, r.getY() + 7, 80, 14 }, 10.0f, cTexto, juce::Justification::centredLeft, 0.18f, true);
    desenhaTextoEsp (g, ponto.trim() + " ALL 8 GROUPS" + ponto + "EDIT ANY GROUP FROM ANY WINDOW" + ponto + "CLICK ONE TO SEE IT",
                     { r.getX() + 76, r.getY() + 7, r.getWidth() - 86, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.12f);
    const float x0 = r.getX() + 8, y0 = r.getY() + 28, w = r.getWidth() - 16, h = r.getHeight() - 54;
    const float sw = (w - 7 * 5.0f) / NR;
    for (int gi = 0; gi < NR; ++gi) tira (g, { x0 + gi * (sw + 5.0f), y0, sw, h }, gi);

    int nFoco = 0;
    for (auto& G : grupos) if (G.focus) ++nFoco;
    const float yp = r.getBottom() - 21;
    desenhaTextoEsp (g, juce::String::fromUTF8 ("\xe2\x96\xb2 ") + "PER GROUP: ROLE" + ponto + "TRIM" + ponto + "FOCUS (each group has its own)",
                     { r.getX() + 10, yp, 380, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.12f);
    desenhaTextoEsp (g, nFoco > 0 ? estrela + " " + juce::String (nFoco) + " IN FOCUS" : juce::String ("NO FOCUS"),
                     { r.getRight() - 160, yp, 150, 14 }, 10.0f, nFoco > 0 ? cRoxoClaro : cApagado, juce::Justification::centredRight, 0.14f, true);
}

void MaestroEditor::tira (juce::Graphics& g, juce::Rectangle<float> r, int gi)
{
    auto& R = Registry::get();
    const auto& G = grupos[gi];
    const bool vazio = G.faixas.empty();
    const bool este = gi == meuGrupo(), escolhido = gi == sel;
    const bool travado = R.modo.load() == 2, aprendendo = R.modo.load() == 1;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), cx = r.getCentreX();
    const juce::Colour cor = corRole[gi];

    cliques.push_back ({ r, [this, gi] { sel = gi; } });
    g.setColour (escolhido ? juce::Colour (0xff221d30) : cTira);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    if (escolhido)
    {
        g.setColour (cBranco.withAlpha (0.85f));
        g.drawRoundedRectangle (r.expanded (1.5f), 4.0f, 1.6f);
    }

    // marca da janela / do grupo escolhido
    if (este) desenhaTextoEsp (g, alvoGlifo + " THIS WINDOW", { x, y + 3, w, 12 }, 7.6f, cVerde, juce::Justification::centred, 0.04f, true);
    else if (escolhido) desenhaTextoEsp (g, "SHOWING", { x, y + 3, w, 12 }, 8.0f, cApagado, juce::Justification::centred, 0.12f);

    // de onde veio o grupo
    {
        juce::String t = "NO TRACK"; juce::Colour c = cApagado.withAlpha (0.6f), borda = cBorda;
        if (! vazio)
        {
            int f = G.faixas.front()->fonte;
            for (auto* fx : G.faixas) if (fx->fonte == PELO_SOM) f = PELO_SOM;
            if (f == PELO_NOME) { t = juce::String::fromUTF8 ("\xe2\x9c\x93 BY NAME"); c = cVerde; }
            else if (f == PELO_SOM) { t = "? BY SOUND"; c = cLaranja; borda = cLaranja.withAlpha (0.45f); }
            else { t = "MANUAL"; c = cRoxoClaro; }
        }
        const juce::Rectangle<float> b (x + 4, y + 18, w - 8, 15);
        g.setColour (cTela); g.fillRoundedRectangle (b, 2.0f);
        g.setColour (borda); g.drawRoundedRectangle (b, 2.0f, 1.0f);
        desenhaTextoEsp (g, t, b, 8.2f, c, juce::Justification::centred, 0.06f, true);
    }
    // nome da faixa (ou quantas)
    {
        juce::String nm = vazio ? traco : (G.faixas.size() == 1
                              ? (G.faixas.front()->nome.isNotEmpty() ? G.faixas.front()->nome : juce::String ("(no name)"))
                              : juce::String ((int) G.faixas.size()) + " TRACKS");
        g.setColour (cTexto.withAlpha (vazio ? 0.4f : 0.85f));
        g.setFont (fonte (9.5f));
        g.drawFittedText (nm, juce::Rectangle<int> ((int) x + 2, (int) y + 36, (int) w - 4, 13), juce::Justification::centred, 1, 0.7f);
    }

    // medidor: nível do grupo (saída) com a marca do alvo do STYLE; à esquerda, o corte agora (verde)
    const float my = y + 54, mh = 252;
    const float dbTopo = 0.0f, dbBase = -42.0f;
    auto Ym = [&] (float db) { return my + mh * (dbTopo - juce::jlimit (dbBase, dbTopo, db)) / (dbTopo - dbBase); };
    const float mx = cx - 9, mw = 14;
    g.setColour (cTela); g.fillRect (mx, my, mw, mh);
    g.setColour (cTela); g.fillRect (x + 6, my, 6.0f, mh);
    for (float d : { -6.0f, -12.0f, -24.0f, -36.0f })
    {
        g.setColour (cGradeF); g.fillRect (mx + mw, Ym (d), 3.0f, 1.0f);
        desenhaTextoEsp (g, juce::String ((int) d), { mx + mw + 3, Ym (d) - 5, 22, 10 }, 7.5f, cApagado.withAlpha (0.8f),
                         juce::Justification::centredLeft, 0.0f);
    }
    if (! vazio && G.nivelDb > -120.0f)
    {
        const float yy = Ym (G.nivelDb);
        g.setGradientFill (juce::ColourGradient (cor.withAlpha (0.35f), 0, my + mh, cor, 0, my, false));
        g.fillRect (mx + 2, yy, mw - 4, my + mh - yy);
    }
    if (G.alvoDb > -150.0f && ! aprendendo)
    {
        const float ya = Ym (G.alvoDb);
        g.setColour (cBranco); g.fillRect (mx - 4, ya - 1, mw + 8, 2.0f);
        juce::Path tri; tri.addTriangle (mx - 7, ya - 4, mx - 3, ya, mx - 7, ya + 4);
        g.fillPath (tri);
    }
    float pior = G.shelf;
    for (int k = 0; k < NE; ++k) pior = std::min (pior, G.corte[k]);
    if (pior < -0.2f)
    {
        g.setColour (cVerde);
        g.fillRect (x + 7, my, 4.0f, mh * juce::jlimit (0.0f, 1.0f, -pior / 15.0f));
    }

    // GAIN e CUT NOW
    desenhaTextoEsp (g, vazio ? traco : (aprendendo ? juce::String::fromUTF8 ("\xe2\x80\xa6") : db1 (G.ganhoDb)), { x, my + mh + 6, w, 14 }, 11.5f,
                     vazio ? cApagado : cBranco, juce::Justification::centred, 0.0f, true);
    desenhaTextoEsp (g, "GAIN", { x, my + mh + 20, w, 10 }, 8.0f, cApagado, juce::Justification::centred, 0.16f);
    desenhaTextoEsp (g, pior < -0.05f ? db1 (pior) : juce::String ("0.0 dB"), { x, my + mh + 36, w, 14 }, 11.0f,
                     pior < -0.05f ? cVerde : cApagado.withAlpha (0.5f), juce::Justification::centred, 0.0f, true);
    {
        juce::String causa; juce::Colour c = cApagado;
        if (vazio) causa = {};
        else if (G.focus) { causa = estrela + " IN FOCUS"; c = cRoxoClaro; }
        else if (gi == KICK || gi == BASS) causa = travado ? "OWNS LOW END" : "";
        else if (! G.cedeGrupos.empty()) { causa = "TO " + estrela + juce::String ((int) G.cedeGrupos.size()) + " FOCUS"; c = cRoxoClaro; }
        else if (G.shelf < -0.3f) { causa = "SHELF"; c = cLaranja; }
        desenhaTextoEsp (g, causa, { x, my + mh + 50, w, 11 }, 7.8f, c, juce::Justification::centred, 0.04f, true);
    }
    desenhaTextoEsp (g, "CUT NOW", { x, my + mh + 62, w, 10 }, 8.0f, cApagado, juce::Justification::centred, 0.16f);

    // TRIM (knob da tira)
    auto& k = knobs[(size_t) (3 + gi)];
    k.c = { cx, my + mh + 104 };
    desenhaKnob (g, k);

    // FOCUS
    {
        const juce::Rectangle<float> b (x + 5, my + mh + 140, w - 10, 26);
        const bool on = G.focus;
        if (on) g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a4bb8), 0, b.getY(), juce::Colour (0xff3d2a73), 0, b.getBottom(), false));
        else g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35304a), 0, b.getY(), juce::Colour (0xff25212f), 0, b.getBottom(), false));
        g.fillRoundedRectangle (b, 4.0f);
        if (on) { g.setColour (cRoxo.withAlpha (0.35f)); g.drawRoundedRectangle (b.expanded (1.5f), 5.0f, 2.0f); }
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (b, 4.0f, 1.0f);
        led (g, { b.getX() + 10, b.getCentreY() }, 3.0f, cRoxoClaro, on);
        desenhaTextoEsp (g, "FOCUS", b.withTrimmedLeft (14), 10.0f, on ? juce::Colour (0xfff4eeff) : (vazio ? cApagado.withAlpha (0.4f) : cApagado),
                         juce::Justification::centred, 0.16f, true);
        if (! vazio) cliques.push_back ({ b, [this, gi] { focusDoGrupo (gi); } });
    }
    // ROLE
    {
        const juce::Rectangle<float> b (x + 4, my + mh + 172, w - 8, 16);
        desenhaTextoEsp (g, "ROLE " + setaBaixo, b, 8.5f, vazio ? cApagado.withAlpha (0.4f) : cApagado, juce::Justification::centred, 0.12f);
        if (! vazio) cliques.push_back ({ b, [this, gi] { menuRoleDoGrupo (gi); } });
    }
    // faixa colorida com o nome do grupo
    {
        const juce::Rectangle<float> b (x, r.getBottom() - 24, w, 24);
        juce::Path p;
        p.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(), 3.0f, 3.0f, false, false, true, true);
        g.setColour (vazio ? cor.withAlpha (0.25f) : cor);
        g.fillPath (p);
        desenhaTextoEsp (g, nomeRole (gi), b, gi == AMBIENCE ? 9.0f : 10.5f, juce::Colour (0xff120f1a), juce::Justification::centred, 0.08f, true);
    }
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::mapa (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& R = Registry::get();
    caixa (g, r);
    const bool aprendendo = R.modo.load() == 1;
    desenhaTextoEsp (g, "CONFLICT MAP", { r.getX() + 10, r.getY() + 7, 120, 14 }, 10.0f, cTexto, juce::Justification::centredLeft, 0.18f, true);
    desenhaTextoEsp (g, ponto.trim() + " WHO FIGHTS WHERE", { r.getX() + 110, r.getY() + 7, 200, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.12f);
    {
        float lx = r.getRight() - 10;
        auto item = [&] (const juce::String& t, juce::Colour c)
        {
            const auto f = fonte (9.5f).withExtraKerningFactor (0.1f);
            const float wt = largura (f, t) + 2;
            lx -= wt;
            g.setColour (cApagado); g.setFont (f);
            g.drawText (t, juce::Rectangle<float> (lx, r.getY() + 7, wt, 14), juce::Justification::centredLeft);
            lx -= 17;
            g.setColour (c); g.fillRect (lx, r.getY() + 10, 12.0f, 8.0f);
            lx -= 12;
        };
        item ("SHELF", cLaranja.withAlpha (0.55f));
        item (setaBaixo + " CUT NOW", cVerde);
        item ("CLASH", cRuim);
        item ("SPECTRUM", cBranco);
    }

    const juce::Rectangle<float> t (r.getX() + 8, r.getY() + 26, r.getWidth() - 16, 250);
    g.setColour (cTela); g.fillRoundedRectangle (t, 3.0f);
    const float L = t.getX() + 92, w = t.getWidth() - 92 - 64, T = t.getY() + 20, h = t.getHeight() - 20 - 16, rh = h / NR;
    const double ref = pico - 48.0;
    constexpr float TH = 0.5f;
    bool ativo[NR];
    for (int gi = 0; gi < NR; ++gi) ativo[gi] = ! grupos[gi].faixas.empty() && pico > -150.0;
    // por coluna: nível de cada grupo (cache)
    const int NX = (int) (w / 1.5f) + 1;
    std::vector<float> E ((size_t) (NX * NR)), D ((size_t) (NX * NR));
    for (int ix = 0; ix < NX; ++ix)
    {
        const double f = fDeX (ix * 1.5, w);
        for (int gi = 0; gi < NR; ++gi)
        {
            const float d = ativo[gi] ? dbEm (grupos[gi].tela, f) : -200.0f;
            D[(size_t) (ix * NR + gi)] = d;
            E[(size_t) (ix * NR + gi)] = ativo[gi] ? (float) juce::jlimit (0.0, 1.0, (d - ref) / 48.0) : 0.0f;
        }
    }
    auto briga = [&] (int ix, int gi)
    {
        if (E[(size_t) (ix * NR + gi)] < TH) return false;
        for (int j = 0; j < NR; ++j)
            if (j != gi && E[(size_t) (ix * NR + j)] >= TH && std::abs (D[(size_t) (ix * NR + gi)] - D[(size_t) (ix * NR + j)]) <= 12.0f) return true;
        return false;
    };

    // faixa de cima: quantos grupos brigam em cada frequência
    desenhaTextoEsp (g, "CLASH", { t.getX() + 46, t.getY() + 4, 40, 12 }, 8.5f, cApagado, juce::Justification::centredLeft, 0.14f);
    g.setColour (cPista); g.fillRect (L, t.getY() + 4, w, 12.0f);
    for (int ix = 0; ix < NX; ++ix)
    {
        int n = 0;
        for (int gi = 0; gi < NR; ++gi) if (briga (ix, gi)) ++n;
        if (n >= 2) { g.setColour (cRuim.withAlpha (juce::jmin (1.0f, 0.3f + (n - 2) * 0.22f))); g.fillRect (L + ix * 1.5f, t.getY() + 4, 1.6f, 12.0f); }
    }
    desenhaTextoEsp (g, "CLASHES", { L + w + 8, t.getY() + 4, 60, 12 }, 8.5f, cApagado, juce::Justification::centredLeft, 0.14f);

    const float mc = std::max (0.5f, R.maxCut.load());
    for (int gi = 0; gi < NR; ++gi)
    {
        const auto& G = grupos[gi];
        const float y = T + gi * rh, base = y + rh - 2, alt = rh - 5;
        g.setColour (corRole[gi].withAlpha (ativo[gi] ? 1.0f : 0.3f));
        g.fillRect (t.getX() + 8, y + rh / 2 - 5, 10.0f, 10.0f);
        desenhaTextoEsp (g, nomeRole (gi), { t.getX() + 24, y, 64, rh }, 10.5f, gi == sel ? cBranco : (ativo[gi] ? cTexto : cApagado.withAlpha (0.5f)),
                         juce::Justification::centredLeft, 0.02f, gi == sel);
        if (G.focus) desenhaTextoEsp (g, estrela, { L - 16, y, 14, rh }, 11.0f, cRoxo, juce::Justification::centred, 0.0f, true);
        cliques.push_back ({ juce::Rectangle<float> (t.getX(), y, L + w - t.getX(), rh), [this, gi] { sel = gi; } });

        g.setColour (cPista); g.fillRect (L, y + 1, w, rh - 2);
        if (! ativo[gi])
        {
            desenhaTextoEsp (g, "no track", { L + 8, y, 120, rh }, 9.0f, cApagado.withAlpha (0.45f), juce::Justification::centredLeft, 0.1f);
            continue;
        }
        // espectro: coluna branca; vermelha onde briga
        float faixaA = 0, melhorA = 0, melhorB = 0;
        bool dentro = false, teve = false;
        juce::Path contorno;
        for (int ix = 0; ix < NX; ++ix)
        {
            const float xx = L + ix * 1.5f, ev = E[(size_t) (ix * NR + gi)];
            const float yy = base - std::min (1.0f, ev / 0.85f) * alt;
            const bool c = briga (ix, gi);
            g.setColour (c ? cRuim.withAlpha (0.72f) : cBranco.withAlpha (0.14f));
            g.fillRect (xx, yy, 1.6f, base - yy);
            if (ix == 0) contorno.startNewSubPath (xx, yy); else contorno.lineTo (xx, yy);
            const float f = (float) fDeX (ix * 1.5, w);
            if (c && ! dentro) { faixaA = f; dentro = true; }
            if (c)
            {
                teve = true;
                if (melhorA <= 0.0f || f / faixaA > melhorB / melhorA) { melhorA = faixaA; melhorB = f; }
            }
            if (! c) dentro = false;
        }
        g.setColour (cBranco.withAlpha (0.9f));
        g.strokePath (contorno, juce::PathStrokeType (1.1f));

        // o que está sendo cortado agora
        if (G.shelf < -0.3f)
        {
            const float xg = L + (float) xDeF (fGuard, w);
            g.setColour (cLaranja.withAlpha (0.16f)); g.fillRect (L, y + 1, xg - L, rh - 2);
            desenhaTextoEsp (g, "SHELF " + db1 (G.shelf, false), { L + 3, y + 1, 80, 11 }, 8.0f, cLaranja, juce::Justification::centredLeft, 0.04f, true);
        }
        for (int k = 0; k < NE; ++k)
            if (G.corte[k] < -1.0f)
            {
                const float xk = L + (float) xDeF (freqEq (k), w);
                if (xk < L + 70 && G.shelf < -0.3f) continue;
                g.setColour (cTela.withAlpha (0.82f)); g.fillRoundedRectangle (xk - 16, y + 2, 32, 12, 2.0f);
                desenhaTextoEsp (g, setaBaixo + juce::String (-G.corte[k], 1), { xk - 16, y + 2, 32, 12 }, 8.8f,
                                 cVerde.interpolatedWith (cBranco, 0.25f * (1.0f - juce::jmin (1.0f, -G.corte[k] / mc))), juce::Justification::centred, 0.0f, true);
            }
        if (G.focus) { g.setColour (cRoxo); g.drawRect (juce::Rectangle<float> (L, y + 0.5f, w, rh - 1), 1.4f); }
        if (gi == sel) { g.setColour (cBranco.withAlpha (0.85f)); g.drawRect (juce::Rectangle<float> (t.getX() + 2, y + 0.5f, L + w - t.getX() - 2, rh - 1), 1.4f); }
        // trecho da briga
        desenhaTextoEsp (g, teve ? hzCurto (melhorA) + traco + hzCurto (melhorB) : juce::String ("clean"), { L + w + 6, y, 58, rh }, 9.5f,
                         teve ? cRuim : cApagado.withAlpha (0.6f), juce::Justification::centredLeft, 0.0f, true);
    }
    // eixo e linha do GUARD
    for (double f : { 31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 })
    {
        const float xx = L + (float) xDeF (f, w);
        desenhaTextoEsp (g, hzCurto (f < 40 ? 31 : f), { xx - 16, T + h + 2, 32, 12 }, 8.8f, cApagado, juce::Justification::centred, 0.0f);
    }
    if (R.guard.load() && ! aprendendo)
    {
        const float xg = L + (float) xDeF (fGuard, w);
        const float dash[] = { 3.0f, 3.0f };
        g.setColour (cLaranja.withAlpha (0.5f));
        g.drawDashedLine (juce::Line<float> (xg, T, xg, T + h), dash, 2, 1.0f);
    }

    // resumo: a pior briga + quem está em FOCUS
    const float yr = t.getBottom() + 3;
    juce::String piorTxt;
    {
        float melhor = 0.0f;
        for (int b = 0; b < NB; ++b)
        {
            int a = -1, c = -1, n = 0;
            float ea = 0, ec = 0;
            for (int gi = 0; gi < NR; ++gi)
            {
                if (! ativo[gi]) continue;
                const float ev = (float) juce::jlimit (0.0, 1.0, (dbDe (grupos[gi].tela[b] + 1e-14) - ref) / 48.0);
                if (ev < TH) continue;
                ++n;
                if (ev > ea) { c = a; ec = ea; a = gi; ea = ev; }
                else if (ev > ec) { c = gi; ec = ev; }
            }
            if (n >= 2 && a >= 0 && c >= 0 && ea + ec > melhor
                && std::abs (dbDe (grupos[a].tela[b] + 1e-14) - dbDe (grupos[c].tela[b] + 1e-14)) <= 12.0f)
            {
                melhor = ea + ec;
                piorTxt = juce::String (nomeRole (a)) + vezes + nomeRole (c) + " @ " + hzCurto (freqBanda (b)) + " Hz (" + juce::String (n) + " groups)";
            }
        }
    }
    float xr = r.getX() + 12;
    auto parte = [&] (const juce::String& s, juce::Colour c, bool negrito)
    {
        const auto f = fonte (10.0f, negrito).withExtraKerningFactor (0.06f);
        g.setColour (c); g.setFont (f);
        const float wt = largura (f, s);
        g.drawText (s, juce::Rectangle<float> (xr, yr, wt + 2, 16), juce::Justification::centredLeft);
        xr += wt + 6;
    };
    if (aprendendo) parte (juce::String::fromUTF8 ("LISTENING\xe2\x80\xa6 play the drop for 20") + traco + "30 s", cLaranja, false);
    else if (piorTxt.isNotEmpty()) { parte ("WORST CLASH", cApagado, false); parte (piorTxt, cRuim, true); }
    else parte ("NO CLASH RIGHT NOW", cApagado, false);
    xr += 10;
    juce::StringArray fs;
    for (int gi = 0; gi < NR; ++gi) if (grupos[gi].focus) fs.add (estrela + nomeRole (gi));
    if (fs.isEmpty()) parte ("No FOCUS: click FOCUS on any group to make it win", cApagado, false);
    else { parte ("FOCUS", cApagado, false); parte (fs.joinIntoString (" + "), cRoxoClaro, true); parte (ponto.trim() + " the others open space", cApagado, false); }
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::painelEq (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& R = Registry::get();
    caixa (g, r);
    const auto& G = grupos[sel];
    const auto* f = G.rep;
    const bool aprendendo = R.modo.load() == 1;
    const juce::Colour cor = corRole[sel];

    // título: grupo + o que está acontecendo com ele
    float xt = r.getX() + 10;   // fim do título: a legenda à direita não pode passar daqui
    {
        auto parte = [&] (const juce::String& s, juce::Colour c, bool negrito)
        {
            const auto fo = fonte (10.0f, negrito).withExtraKerningFactor (0.12f);
            g.setColour (c); g.setFont (fo);
            const float wt = largura (fo, s);
            g.drawText (s, juce::Rectangle<float> (xt, r.getY() + 7, wt + 2, 14), juce::Justification::centredLeft);
            xt += wt + 4;
        };
        parte (nomeRole (sel), cor, true);
        if (f != nullptr && G.faixas.size() > 1) parte ("(" + (f->nome.isNotEmpty() ? f->nome : juce::String ("track")) + ")", cApagado, false);
        parte (ponto.trim() + " DYNAMIC EQ " + ponto.trim(), cApagado, false);
        juce::StringArray outros;
        for (int gi = 0; gi < NR; ++gi) if (gi != sel && grupos[gi].focus) outros.add (estrela + nomeRole (gi));
        if (G.faixas.empty()) parte ("NO TRACK ON THIS GROUP", cApagado, false);
        else if (aprendendo) parte ("LISTENING", cLaranja, true);
        else if (G.focus && ! outros.isEmpty() && sel != KICK && sel != BASS)
        { parte (estrela + " FOCUS, SHARES WITH", cRoxoClaro, true); parte (outros.joinIntoString (" + "), cRoxoClaro, true); }
        else if (G.focus) parte (estrela + " IN FOCUS", cRoxoClaro, true);
        else if (! G.cedeGrupos.empty())
        {
            juce::StringArray a;
            for (int v : G.cedeGrupos) a.add (estrela + nomeRole (v));
            parte ("YIELDING TO", cApagado, false); parte (a.joinIntoString (" + "), cRoxoClaro, true);
        }
        else if (sel == KICK || sel == BASS) parte ("OWNS THE LOW END", cApagado, false);
        else if (R.modo.load() == 2) parte ("TONE MATCH + GUARD", cApagado, false);
        else parte ("NOT LOCKED", cApagado, false);
    }
    {
        float lx = r.getRight() - 10;
        auto item = [&] (const juce::String& t, juce::Colour c, bool linha)
        {
            const auto fo = fonte (9.5f).withExtraKerningFactor (0.1f);
            const float wt = largura (fo, t) + 2;
            if (lx - wt - 29 < xt + 12) { lx = xt; return; }   // sem espaço: some a legenda, nunca encavala no título
            lx -= wt;
            g.setColour (cApagado); g.setFont (fo);
            g.drawText (t, juce::Rectangle<float> (lx, r.getY() + 7, wt, 14), juce::Justification::centredLeft);
            lx -= 17;
            g.setColour (c);
            if (linha) g.fillRect (lx, r.getY() + 13, 12.0f, 2.0f); else g.fillRect (lx, r.getY() + 10, 12.0f, 8.0f);
            lx -= 12;
        };
        if (! aprendendo) { item ("LIVE EQ", cVerde, true); item ("TONE", cBranco, true); }
        item (nomeRole (sel), cor.withAlpha (0.6f), false);
        item ("MIX SUM", cApagado.withAlpha (0.5f), false);
    }

    const juce::Rectangle<float> t (r.getX() + 8, r.getY() + 26, r.getWidth() - 16, r.getHeight() - 34);
    g.setColour (cTela); g.fillRoundedRectangle (t, 3.0f);
    const float L = t.getX() + 34, w = t.getWidth() - 44, T = t.getY() + 8, h = 186;
    const float dbT = 6.0f, dbB = -15.0f;
    auto Y = [&] (double db) { return (float) (T + h * (dbT - juce::jlimit ((double) dbB, (double) dbT, db)) / (dbT - dbB)); };
    auto X = [&] (double fr) { return L + (float) xDeF (fr, w); };
    for (float d : { 6.0f, 0.0f, -6.0f, -12.0f })
    {
        g.setColour (d == 0.0f ? cGradeF : cGrade);
        g.drawHorizontalLine ((int) Y (d), L, L + w);
        desenhaTextoEsp (g, d > 0 ? "+" + juce::String ((int) d) : juce::String ((int) d), { t.getX() + 2, Y (d) - 6, 26, 12 }, 8.5f, cApagado,
                         juce::Justification::centredRight, 0.0f);
    }
    for (double fr : { 31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 })
    {
        g.setColour (cGrade);
        g.drawVerticalLine ((int) X (fr), T, T + h);
        desenhaTextoEsp (g, hzCurto (fr < 40 ? 31 : fr), { X (fr) - 16, T + h + 2, 32, 12 }, 8.8f, cApagado, juce::Justification::centred, 0.0f);
    }

    // espectros: a soma da mix (cinza) e o grupo (cor dele), na mesma escala
    double soma[NB] {};
    for (auto& o : grupos) if (! o.faixas.empty()) for (int b = 0; b < NB; ++b) soma[b] += o.tela[b];
    float pkS = -200.0f;
    for (int b = 0; b < NB; ++b) pkS = std::max (pkS, dbDe (soma[b]));
    auto especY = [&] (float db) { return T + h - juce::jlimit (0.0f, 1.0f, (db - (pkS - 54.0f)) / 54.0f) * h * 0.92f; };
    auto espectro = [&] (const double* tela)
    {
        juce::Path p;
        for (int i = 0; i <= 200; ++i)
        {
            const double fr = fDeX (w * i / 200.0, w);
            const float yy = especY (dbEm (tela, fr));
            if (i == 0) p.startNewSubPath (X (fr), yy); else p.lineTo (X (fr), yy);
        }
        return p;
    };
    if (pkS > -150.0f)
    {
        auto ps = espectro (soma);
        auto area = ps;
        area.lineTo (L + w, T + h); area.lineTo (L, T + h); area.closeSubPath();
        g.setColour (cBranco.withAlpha (0.07f)); g.fillPath (area);
        g.setColour (cApagado.withAlpha (0.55f)); g.strokePath (ps, juce::PathStrokeType (1.0f));
        if (! G.faixas.empty())
        {
            auto pg = espectro (G.tela);
            auto ag = pg;
            ag.lineTo (L + w, T + h); ag.lineTo (L, T + h); ag.closeSubPath();
            g.setGradientFill (juce::ColourGradient (cor.withAlpha (0.45f), 0, T, cor.withAlpha (0.05f), 0, T + h, false));
            g.fillPath (ag);
            g.setColour (cor.withAlpha (0.8f)); g.strokePath (pg, juce::PathStrokeType (1.0f));
        }
        // FOCUS dos outros grupos (tracejado, na cor deles)
        for (int gi = 0; gi < NR; ++gi)
            if (gi != sel && grupos[gi].focus)
            {
                g.setColour (corRole[gi]);
                tracejado (g, espectro (grupos[gi].tela), 1.4f, 5.0f, 3.0f);
            }
    }

    const float mc = std::max (0.5f, R.maxCut.load());
    if (aprendendo || f == nullptr)
    {
        desenhaTextoEsp (g, f == nullptr ? juce::String ("Put MAESTRO on this group to see its EQ")
                                         : juce::String::fromUTF8 ("listening\xe2\x80\xa6 curves appear after LOCK"),
                         { L, Y (0) - 36, w, 16 }, 12.0f, aprendendo ? cLaranja : cApagado, juce::Justification::centred, 0.04f);
        historico (g, { L, T + h + 20, w, 52 }, sel);
        return;
    }

    // GUARD: região do shelf (só enquanto KICK / BASS tocam)
    if (f->shelf < -0.3f)
    {
        g.setColour (cLaranja.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (L, T, X (fGuard) - L, h));
        desenhaTextoEsp (g, "GUARD SHELF " + db1 (f->shelf), { L + 5, T + 3, 160, 12 }, 9.0f, cLaranja, juce::Justification::centredLeft, 0.06f, true);
        desenhaTextoEsp (g, "only while KICK / BASS play", { L + 5, T + 15, 160, 12 }, 8.5f, cLaranja.withAlpha (0.8f), juce::Justification::centredLeft, 0.02f);
    }

    // curvas: TONE (branco tracejado) e LIVE EQ (verde), com a área do que está sendo cortado agora
    juce::Path pTom, pVivo;
    float yTom[201], yVivo[201], xs[201];
    for (int i = 0; i <= 200; ++i)
    {
        const double fr = fDeX (w * i / 200.0, w);
        xs[i] = X (fr);
        yTom[i] = Y (respostaEq (f->tom, f->corte, f->shelf, fr, true));
        yVivo[i] = Y (respostaEq (f->tom, f->corte, f->shelf, fr, false));
        if (i == 0) { pTom.startNewSubPath (xs[i], yTom[i]); pVivo.startNewSubPath (xs[i], yVivo[i]); }
        else { pTom.lineTo (xs[i], yTom[i]); pVivo.lineTo (xs[i], yVivo[i]); }
    }
    juce::Path area = pTom;
    for (int i = 200; i >= 0; --i) area.lineTo (xs[i], yVivo[i]);
    area.closeSubPath();
    g.setColour (cVerde.withAlpha (0.20f)); g.fillPath (area);
    g.setColour (cBranco.withAlpha (0.85f)); tracejado (g, pTom, 1.4f, 6.0f, 3.0f);
    g.setColour (cVerde); g.strokePath (pVivo, juce::PathStrokeType (2.2f));

    // os 9 nós dinâmicos: alcance (0 até MAX CUT) e o corte agora
    for (int k = 0; k < NE; ++k)
    {
        const float xk = X (freqEq (k));
        const juce::Rectangle<float> faixa (xk - 5, Y (0), 10, Y (-mc) - Y (0));
        g.setColour (cBranco.withAlpha (0.04f)); g.fillRect (faixa);
        g.setColour (cGradeF); g.drawRect (faixa, 1.0f);
        const float c = f->corte[k];
        if (c < -0.3f)
        {
            g.setColour (cVerde.withAlpha (0.8f)); g.fillRect (xk - 4, Y (0), 8.0f, Y (c) - Y (0));
            g.setColour (cBranco); g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre ({ xk, Y (c) }));
            g.setColour (cVerde); g.drawEllipse (juce::Rectangle<float> (9, 9).withCentre ({ xk, Y (c) }), 2.0f);
            desenhaTextoEsp (g, db1 (c, false), { xk - 18, Y (-mc) + 3, 36, 12 }, 9.0f, cVerde, juce::Justification::centred, 0.0f, true);
        }
        else
        {
            g.setColour (cGradeF); g.fillEllipse (juce::Rectangle<float> (6, 6).withCentre ({ xk, Y (0) }));
        }
    }
    desenhaTextoEsp (g, "MAX CUT", { L + w - 52, Y (-mc) - 6, 50, 12 }, 8.5f, cApagado, juce::Justification::centredRight, 0.1f);
    historico (g, { L, T + h + 20, w, 52 }, sel);
}

// histórico dos cortes do grupo (últimos 8 s): QUANDO cortou e em que banda
void MaestroEditor::historico (juce::Graphics& g, juce::Rectangle<float> r, int gi)
{
    auto& R = Registry::get();
    desenhaTextoEsp (g, "CUTS", { r.getX() - 32, r.getY() + 12, 30, 12 }, 8.5f, cApagado, juce::Justification::centredRight, 0.1f);
    desenhaTextoEsp (g, "8 s", { r.getX() - 32, r.getY() + 26, 30, 12 }, 8.5f, cApagado, juce::Justification::centredRight, 0.0f);
    g.setColour (cPista.withAlpha (0.7f)); g.fillRect (r);
    const float mc = std::max (0.5f, R.maxCut.load());
    const float rh = r.getHeight() / NE, cw = r.getWidth() / HN;
    for (int i = 0; i < HN; ++i)
    {
        const int idx = (histPos + i) % HN;   // o mais antigo à esquerda
        const auto* v = hist[gi][idx];
        const float xx = r.getX() + i * cw;
        for (int k = 0; k < NE; ++k)
            if (v[k] < -0.3f)
            {
                g.setColour (cVerde.withAlpha (juce::jlimit (0.15f, 1.0f, -v[k] / mc)));
                g.fillRect (xx, r.getBottom() - (k + 1) * rh, cw + 0.3f, rh - 0.5f);
            }
        if (v[NE] < -0.3f)
        {
            g.setColour (cLaranja.withAlpha (juce::jlimit (0.15f, 0.9f, -v[NE] / kShelfMax)));
            g.fillRect (xx, r.getBottom() - 2 * rh, cw + 0.3f, 2 * rh - 0.5f);
        }
    }
    g.setColour (cBranco); g.fillRect (r.getRight() - 2, r.getY(), 2.0f, r.getHeight());
    desenhaTextoEsp (g, "NOW", { r.getRight() - 40, r.getBottom() + 1, 40, 11 }, 8.5f, cApagado, juce::Justification::centredRight, 0.1f);
    desenhaTextoEsp (g, "LOW", { r.getX() + 3, r.getBottom() - 11, 30, 10 }, 7.5f, cApagado.withAlpha (0.7f), juce::Justification::centredLeft, 0.1f);
    desenhaTextoEsp (g, "HIGH", { r.getX() + 3, r.getY() + 1, 30, 10 }, 7.5f, cApagado.withAlpha (0.7f), juce::Justification::centredLeft, 0.1f);
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::base (juce::Graphics& g)
{
    auto& R = Registry::get();
    g.setGradientFill (juce::ColourGradient (cTopoB, 0, 708, cFundoB, 0, (float) baseH, false));
    g.fillRect (0.0f, 708.0f, (float) baseW, 92.0f);
    g.setColour (juce::Colours::black);
    g.drawLine (0, 708, (float) baseW, 708, 1.0f);

    const int modo = R.modo.load();
    const bool pisca = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
    // botão no estilo do ALIGN: verde quando ligado
    auto botao = [&] (juce::Rectangle<float> rr, const juce::String& t, bool on, bool ledAceso, juce::Colour corLed, std::function<void()> faz)
    {
        if (on) g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c7a52), 0, rr.getY(), juce::Colour (0xff1b4d34), 0, rr.getBottom(), false));
        else g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35304a), 0, rr.getY(), juce::Colour (0xff25212f), 0, rr.getBottom(), false));
        g.fillRoundedRectangle (rr, 5.0f);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (rr, 5.0f, 1.0f);
        led (g, { rr.getCentreX(), rr.getY() + 15 }, 4.0f, corLed, ledAceso);
        desenhaTextoEsp (g, t, rr.withTrimmedTop (22), 15.0f, on ? juce::Colour (0xffeafff2) : cApagado, juce::Justification::centred, 0.26f, true);
        cliques.push_back ({ rr, faz });
    };
    botao ({ 18, 722, 96, 64 }, "LEARN", modo == 1, modo == 1 && pisca, cLaranja, []
    {
        Registry::get().learn (juce::Time::getMillisecondCounterHiRes() / 1000.0);
    });
    botao ({ 122, 722, 96, 64 }, "LOCK", modo == 2, modo == 2, cVerde, []
    {
        auto& RR = Registry::get();
        if (RR.modo.load() == 1) RR.lock();
        else if (RR.modo.load() == 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MAESTRO",
                "Press LEARN first and play the drop for 20" + traco + "30 s. Then LOCK.");
    });

    // WHOLE MIX: o que vale para todos os grupos
    const juce::Rectangle<float> wm (234, 720, 478, 70);
    g.setColour (cTela.withAlpha (0.45f)); g.fillRoundedRectangle (wm, 4.0f);
    g.setColour (cBorda); g.drawRoundedRectangle (wm, 4.0f, 1.0f);
    {
        const auto ft = fonte (9.0f, true).withExtraKerningFactor (0.16f);
        const juce::String tt = "WHOLE MIX" + ponto + "SAME ON ALL GROUPS";
        const float wt = largura (ft, tt) + 12;
        g.setColour (cTopoB); g.fillRect (wm.getX() + 10, wm.getY() - 6, wt, 12.0f);
        desenhaTextoEsp (g, tt, { wm.getX() + 16, wm.getY() - 6, wt, 12 }, 9.0f, cTexto, juce::Justification::centredLeft, 0.16f, true);
    }
    for (int i = 0; i < 3; ++i) desenhaKnob (g, knobs[(size_t) i]);
    chave (g, { 468, 728, 234, 24 }, "LOW-END GUARD" + ponto + "SHELF < 120 Hz", R.guard.load() != 0, [] { auto& RR = Registry::get(); RR.guard = RR.guard.load() ? 0 : 1; });
    chave (g, { 468, 758, 234, 24 }, "KEEP KICK / BASS ON TOP", R.topo.load() != 0, [] { auto& RR = Registry::get(); RR.topo = RR.topo.load() ? 0 : 1; });

    desenhaTextoEsp (g, "ZERO LATENCY", { 734, 734, 200, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.16f);
    desenhaTextoEsp (g, "NO SIDECHAIN, NO ROUTING", { 734, 752, 220, 14 }, 9.5f, cApagado, juce::Justification::centredLeft, 0.16f);

    const bool B = R.modoB.load() != 0;
    auto ab = [&] (juce::Rectangle<float> rr, const juce::String& t, bool on, std::function<void()> faz)
    {
        g.setColour (on ? cRoxoClaro : cTela.withAlpha (0.6f));
        g.fillRoundedRectangle (rr, 3.0f);
        g.setColour (cBorda); g.drawRoundedRectangle (rr, 3.0f, 1.0f);
        desenhaTextoEsp (g, t, rr, 13.0f, on ? juce::Colour (0xff120f1a) : cApagado, juce::Justification::centred, 0.0f, true);
        cliques.push_back ({ rr, faz });
    };
    ab ({ 1052, 738, 36, 32 }, "A", ! B, [] { Registry::get().modoB = 0; });
    ab ({ 1092, 738, 36, 32 }, "B", B, [] { Registry::get().modoB = 1; });
    const juce::Rectangle<float> rr (1140, 738, 122, 32);
    g.setColour (cTela.withAlpha (0.6f)); g.fillRoundedRectangle (rr, 3.0f);
    g.setColour (cBorda); g.drawRoundedRectangle (rr, 3.0f, 1.0f);
    desenhaTextoEsp (g, "RESET ALL", rr, 11.0f, cApagado, juce::Justification::centred, 0.2f);
    cliques.push_back ({ rr, [this] { confirmaReset(); } });
}

void MaestroEditor::chave (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, std::function<void()> faz)
{
    g.setColour (cTela.withAlpha (0.7f)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (cBorda); g.drawRoundedRectangle (r, 3.0f, 1.0f);
    led (g, { r.getX() + 13, r.getCentreY() }, 3.5f, cVerde, on);
    desenhaTextoEsp (g, t, r.withTrimmedLeft (24), 9.5f, on ? cTexto : cApagado, juce::Justification::centredLeft, 0.12f);
    cliques.push_back ({ r, faz });
}

void MaestroEditor::desenhaKnob (juce::Graphics& g, Knob& k)
{
    const bool on = ! k.ativo || k.ativo();
    const float v = juce::jlimit (0.0f, 1.0f, k.le());
    desenhaTextoEsp (g, k.rotulo, { k.c.x - 34, k.c.y - k.r - 13, 68, 11 }, 8.5f, cApagado, juce::Justification::centred, 0.14f);
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d283c), k.c.x, k.c.y - k.r, juce::Colour (0xff15121d), k.c.x, k.c.y + k.r, false));
    g.fillEllipse (juce::Rectangle<float> (k.r * 2, k.r * 2).withCentre (k.c));
    g.setColour (juce::Colours::black); g.drawEllipse (juce::Rectangle<float> (k.r * 2, k.r * 2).withCentre (k.c), 1.0f);
    juce::Path fundo, arco;
    fundo.addCentredArc (k.c.x, k.c.y, k.r - 3, k.r - 3, 0, a0, a1, true);
    g.setColour (cGradeF);
    g.strokePath (fundo, juce::PathStrokeType (2.5f));
    if (on)
    {
        // TRIM (centro = 0 dB) desenha a partir do meio; os outros do começo
        const float ini = k.rotulo == "TRIM" ? (a0 + a1) * 0.5f : a0, fim = a0 + (a1 - a0) * v;
        if (std::abs (fim - ini) > 0.01f)
        {
            arco.addCentredArc (k.c.x, k.c.y, k.r - 3, k.r - 3, 0, std::min (ini, fim), std::max (ini, fim), true);
            g.setColour (k.cor);
            g.strokePath (arco, juce::PathStrokeType (2.5f));
        }
        g.setColour (cBranco);
        g.drawLine (k.c.x + std::sin (fim) * (k.r * 0.2f), k.c.y - std::cos (fim) * (k.r * 0.2f),
                    k.c.x + std::sin (fim) * (k.r - 5), k.c.y - std::cos (fim) * (k.r - 5), 2.0f);
    }
    desenhaTextoEsp (g, on ? k.texto() : juce::String ("-"), { k.c.x - 34, k.c.y + k.r + 2, 68, 12 }, 10.0f, on ? cTexto : cApagado,
                     juce::Justification::centred, 0.0f, true);
}

// ------------------------------------------------------------------------------------------------------------------
void MaestroEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        if (k.ativo && ! k.ativo()) continue;
        if (p.getDistanceFrom (k.c) <= k.r + 5)
        {
            arrastando = i;
            valorIni = k.le();
            yIni = p.y;
            if (k.gesto) k.gesto (true);
            return;
        }
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
    {
        if (k.ativo && ! k.ativo()) continue;
        if (p.getDistanceFrom (k.c) <= k.r + 5)
        {
            if (k.gesto) k.gesto (true);
            k.grava (k.padrao);
            if (k.gesto) k.gesto (false);
            repaint();
            return;
        }
    }
}

void MaestroEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
    {
        if (k.ativo && ! k.ativo()) continue;
        if (p.getDistanceFrom (k.c) <= k.r + 5)
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

// ROLE das cópias de um grupo (de qualquer janela). Grupo com várias faixas: um submenu por faixa
void MaestroEditor::menuRoleDoGrupo (int gi)
{
    struct Item { void* dono; juce::String nome; int atual; };
    std::vector<Item> itens;
    for (auto& f : faixas)
        if (f.role == gi && donoVivo (f.dono))
        {
            auto* d = static_cast<MaestroProcessor*> (f.dono);
            itens.push_back ({ f.dono, f.nome.isNotEmpty() ? f.nome : juce::String ("(no name)"),
                               juce::roundToInt (d->apvts.getRawParameterValue ("role")->load()) });
        }
    if (itens.empty()) return;
    auto preenche = [] (juce::PopupMenu& m, int base, int atual)
    {
        m.addItem (base + 0, "AUTO (by name, else by sound)", true, atual == 0);
        m.addSeparator();
        for (int r = 0; r < NR; ++r) m.addItem (base + r + 1, nomeRole (r), true, atual == r + 1);
    };
    juce::PopupMenu m;
    m.addSectionHeader ("ROLE" + ponto + nomeRole (gi));
    if (itens.size() == 1) preenche (m, 100, itens.front().atual);
    else
        for (size_t i = 0; i < itens.size() && i < 60; ++i)
        {
            juce::PopupMenu sub;
            preenche (sub, 100 * (int) (i + 1), itens[i].atual);
            m.addSubMenu (itens[i].nome, sub);
        }
    std::vector<void*> donos;
    for (auto& it : itens) donos.push_back (it.dono);
    juce::Component::SafePointer<MaestroEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, donos] (int r)
                     {
                         if (safe == nullptr || r < 100) return;
                         const size_t i = (size_t) (r / 100 - 1);
                         if (i >= donos.size() || ! donoVivo (donos[i])) return;
                         static_cast<MaestroProcessor*> (donos[i])->define ("role", (float) (r % 100));
                         safe->repaint();
                     });
}

void MaestroEditor::focusDoGrupo (int gi)
{
    auto ds = donosDoGrupo (gi);
    bool algum = false;
    for (auto* d : ds) algum = algum || d->emFocus();
    for (auto* d : ds) d->defineFocus (! algum);
}

void MaestroEditor::confirmaReset()
{
    juce::Component::SafePointer<MaestroEditor> safe (this);
    auto opcoes = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "RESET ALL",
        "Clear LOCK (level and tone) and FOCUS on every linked group? The mix goes back to how it was.", "RESET ALL", "Cancel", this);
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
    m.addItem (2, juce::String::fromUTF8 ("License\xe2\x80\xa6"));
    m.addSeparator();
   #ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    m.addItem (-1, "MAESTRO " + juce::String (JucePlugin_VersionString), false, false);
   #endif
    juce::Component::SafePointer<MaestroEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (r == 2 && safe != nullptr) { safe->ativacao.mostrarInfo(); return; }
        if (r != 1) return;
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MAESTRO" + ponto + "MIX CONDUCTOR",
            "Put MAESTRO on each of your 8 groups (stems): KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX, after your own "
            "effects. All copies in the project talk to each other: no sidechain, no routing. Any MAESTRO window is the "
            "console for all 8 groups: ROLE, TRIM and FOCUS of every group can be changed from any window.\n\n"
            "LEARN: play the drop for 20" + traco + "30 s. Every group measures its level and tone while it plays.\n"
            "LOCK: each group goes to the level of the chosen STYLE, relative to the KICK, with a gentle TONE MATCH. "
            "Changing a ROLE after LOCK re-locks the whole project.\n"
            "LOW-END GUARD: while KICK and BASS play, the other groups dip below 120 Hz (dynamic low shelf). "
            "No high-pass anywhere; KICK and BASS are never cut by it.\n"
            "FOCUS: the groups you pick (one or more) stand out. Everything else dips only in the frequencies where it masks "
            "them, and only while they play (dynamic EQ, up to MAX CUT). Two FOCUS groups that clash share the space, "
            "a little each. KICK and BASS own the low end: they never yield to FOCUS. KEEP KICK / BASS ON TOP makes the "
            "others yield to them under 400 Hz.\n\n"
            "CONFLICT MAP: each group's spectrum in white, red where it fights another group. "
            + setaBaixo + " = the cut happening now.\n"
            "ROLE comes from the group or track name (Kick, Bass, Drums, Snare, Percs, Hats, Synths, Lead, Ambience, Pad, "
            "Vocal, FX...). Without a name it is a GUESS from the sound: set it by hand if needed.\n"
            "AMOUNT scales everything. TRIM is a per-group offset. A / B compares with the original mix.\n\n"
            "Zero latency. Before LOCK and without FOCUS, the sound passes through untouched.\n"
            "Works when the DAW runs plugins in one process (Ableton Live, Logic, FL Studio, Reaper, Studio One, Cubase). "
            "With plugin sandboxing (Bitwig) each copy only sees its own track.");
    });
}
