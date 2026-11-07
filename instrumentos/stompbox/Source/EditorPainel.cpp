// STOMPBOX — parte de baixo (INPUT + NOISE GATE, AMP, CAB, OUTPUT), seletor de pedais, medidores e afinador.
#include "PluginEditor.h"
#include "Desenho.h"

using namespace desenho;
using stomp::infoPedal;
using stomp::infoAmp;

namespace
{
const juce::Colour corTexto (0xffcfe3dd), corRotulo (0xff9fc9bf), corSuave (0xff8fbdb2), corAcento (0xff4f8d81);

void caixaEscura (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.28f)); g.fillRoundedRectangle (r, 10.0f);
    g.setColour (juce::Colour (0xff0d2523)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
}
} // namespace

void StompboxEditor::embaixo (juce::Graphics& g)
{
    auto norm10 = [this] (const char* pid) { auto* p = proc.apvts.getParameter (pid); return p != nullptr ? p->getValue() * 10.0f : 0.0f; };
    // ---- INPUT + NOISE GATE
    const juce::Rectangle<float> ent (24, 548, 250, 336);
    caixaEscura (g, ent);
    knobP (g, "input", { 90, 612 }, 46, 1, "INPUT", corRotulo, 12.0f, -1, norm10 ("input"));
    texto (g, juce::String (proc.real ("input"), 1) + " dB", { 40, 650, 100, 14 }, corSuave, fonte (10.0f, false));
    texto (g, "IN", { 222, 568, 26, 14 }, corSuave, fonte (9.0f));
    g.setColour (juce::Colour (0xff2a4c47)); g.drawLine (40, 684, 210, 684, 1.0f);
    texto (g, "NOISE GATE", { 24, 692, 250, 16 }, corRotulo, fonte (11.0f));
    const float thr = proc.real ("gate"), rel = proc.real ("gaterel");
    knobP (g, "gate", { 86, 754 }, 46, 1, "THRESHOLD", corRotulo, -90.0f, -1, norm10 ("gate"));
    knobP (g, "gaterel", { 186, 760 }, 34, 1, "RELEASE", corRotulo, 120.0f, -1, norm10 ("gaterel"));
    texto (g, thr <= -89.5f ? juce::String ("OFF") : juce::String (thr, 1) + " dB", { 36, 800, 100, 14 }, corSuave, fonte (10.0f, false));
    texto (g, juce::String (juce::roundToInt (rel)) + " ms", { 136, 800, 100, 14 }, corSuave, fonte (10.0f, false));

    // ---- AMP
    painelAmp (g, { 290, 548, 814, 336 });
    // ---- CAB
    painelCab (g, { 1120, 548, 330, 336 });

    // ---- OUTPUT
    const juce::Rectangle<float> sai (1466, 548, 110, 336);
    caixaEscura (g, sai);
    knobP (g, "output", { 1521, 612 }, 46, 1, "OUTPUT", corRotulo, 0.0f, -1, norm10 ("output"));
    texto (g, juce::String (proc.real ("output"), 1) + " dB", { 1471, 650, 100, 14 }, corSuave, fonte (10.0f, false));
    texto (g, "OUT", { 1466, 680, 110, 14 }, corSuave, fonte (9.0f));
}

void StompboxEditor::painelAmp (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int modelo = juce::roundToInt (proc.real ("amp"));
    const auto& ia = infoAmp (modelo);
    const bool ligado = proc.real ("ampon") > 0.5f;
    // linha de cima: modelos e liga/desliga
    texto (g, "AMP", { r.getX(), r.getY(), 44, 26 }, corRotulo, fonte (11.0f), juce::Justification::centredLeft);
    float x = r.getX() + 44;
    for (int a = 0; a < stomp::numAmps; ++a)
    {
        const float w = 92.0f;
        botao (g, { x, r.getY(), w, 26 }, infoAmp (a).nome, a == modelo, aAmpModelo, a, 11.0f);
        x += w + 8;
    }
    const juce::Rectangle<float> liga (r.getRight() - 170, r.getY(), 170, 26);
    const char* nomesLiga[2] = { "DESLIGADO", "LIGADO" };
    texto (g, "AMP + CAB", liga.withWidth (70), corRotulo, fonte (10.0f), juce::Justification::centredRight);
    chave (g, { liga.getX() + 96, liga.getCentreY() + 2 }, 2, ligado ? 1 : 0, nomesLiga, corRotulo, 9.0f);
    alvos.push_back ({ liga, aAmpOn });

    // o amp
    const juce::Rectangle<float> corpo (r.getX(), r.getY() + 36, r.getWidth(), r.getHeight() - 36);
    sombra (g, corpo, 12.0f, 14.0f, 10.0f, 0.6f);
    const juce::Colour tolex (ia.tolex);
    g.setColour (tolex); g.fillRoundedRectangle (corpo, 12.0f);
    juce::Random rnd (77);
    g.saveState(); g.reduceClipRegion (corpo.toNearestInt());
    for (int i = 0; i < 2600; ++i)   // grão do tolex
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
        g.fillRect (corpo.getX() + rnd.nextFloat() * corpo.getWidth(), corpo.getY() + rnd.nextFloat() * corpo.getHeight(), 1.5f, 1.5f);
    }
    g.restoreState();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), corpo.getX(), corpo.getY(), juce::Colours::black.withAlpha (0.25f), corpo.getX(), corpo.getBottom(), false));
    g.fillRoundedRectangle (corpo, 12.0f);
    // grade
    const juce::Rectangle<float> grade (corpo.getX() + 12, corpo.getY() + 12, corpo.getWidth() - 24, corpo.getHeight() - 24 - 140);
    g.setColour (juce::Colour (ia.grade)); g.fillRoundedRectangle (grade, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    for (float gx = grade.getX(); gx < grade.getRight(); gx += 3.0f) g.drawVerticalLine ((int) gx, grade.getY(), grade.getBottom());
    for (float gy = grade.getY(); gy < grade.getBottom(); gy += 3.0f) g.drawHorizontalLine ((int) gy, grade.getX(), grade.getRight());
    if (modelo == stomp::Chime30)   // losangos da grade "diamond"
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (float gx = grade.getX(); gx < grade.getRight() + 40; gx += 28.0f)
        { g.drawLine (gx, grade.getY(), gx - 60, grade.getBottom(), 1.5f); g.drawLine (gx - 60, grade.getY(), gx, grade.getBottom(), 1.5f); }
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), grade.getX(), grade.getY(), juce::Colours::transparentBlack, grade.getX(), grade.getY() + 18, false));
    g.fillRoundedRectangle (grade, 6.0f);
    // logo
    const auto fl = fonte (34.0f, true, "Arial Black");
    const float lw = juce::GlyphArrangement::getStringWidth (fl, ia.nome) + 44.0f;
    const juce::Rectangle<float> logo (grade.getCentreX() - lw * 0.5f, grade.getCentreY() - 24, lw, 48);
    sombra (g, logo, 24.0f, 3.0f, 3.0f, 0.5f);
    g.setColour (juce::Colour (ia.painel)); g.fillRoundedRectangle (logo, 24.0f);
    texto (g, ia.nome, logo, modelo == stomp::Citrus ? juce::Colour (0xffd4621c) : juce::Colour (ia.texto), fl);
    // painel de controle
    const juce::Rectangle<float> painel (corpo.getX() + 12, corpo.getBottom() - 140, corpo.getWidth() - 24, 128);
    const juce::Colour cp (ia.painel);
    g.setGradientFill (juce::ColourGradient (cp.brighter (0.08f), painel.getX(), painel.getY(), cp.darker (0.12f), painel.getX(), painel.getBottom(), false));
    g.fillRoundedRectangle (painel, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (painel, 6.0f, 1.0f);
    const juce::Colour ct (ia.texto);
    texto (g, ia.sub, { painel.getX() + 18, painel.getY() + 40, 150, 16 }, ct, fonte (10.0f), juce::Justification::centredLeft);
    texto (g, "2 CANAIS", { painel.getX() + 18, painel.getY() + 58, 150, 16 }, ct.withAlpha (0.7f), fonte (10.0f), juce::Justification::centredLeft);
    const char* ids[6] = { "gain", "bass", "mid", "treble", "presence", "master" };
    const char* nomes[6] = { "GAIN", "BASS", "MID", "TREBLE", "PRESENCE", "MASTER" };
    for (int k = 0; k < 6; ++k)
        knobP (g, ids[k], { painel.getX() + 200 + k * 72.0f, painel.getY() + 54 }, 48, 3, nomes[k], ct, 5.0f, -1, proc.real (ids[k]));
    const int canal = juce::roundToInt (proc.real ("canal"));
    const char* nc[2] = { "CLEAN", "DIRTY" };
    chave (g, { painel.getRight() - 72, painel.getY() + 46 }, 2, canal, nc, ct, 9.5f);
    alvos.push_back ({ { painel.getRight() - 140, painel.getY() + 30, 130, 32 }, aCanal });
    led (g, { painel.getRight() - 100, painel.getY() + 92 }, 7.0f, ligado);
    texto (g, "POWER", { painel.getRight() - 88, painel.getY() + 84, 60, 16 }, ct, fonte (9.5f), juce::Justification::centredLeft);
    alvos.push_back ({ { painel.getRight() - 112, painel.getY() + 80, 90, 26 }, aAmpOn });
    if (! ligado)
    {
        g.setColour (juce::Colour (0xff163533).withAlpha (0.55f)); g.fillRoundedRectangle (corpo, 12.0f);
        texto (g, u8 ("AMP + CAB DESLIGADOS \xc2\xb7 sa\xc3\xad""da direta para um amp de verdade"), corpo.withHeight (30).withCentre (grade.getCentre()), corTexto, fonte (13.0f));
    }
}

void StompboxEditor::painelCab (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int cab = juce::roundToInt (proc.real ("cab")), mic = juce::roundToInt (proc.real ("mic"));
    const float pos = proc.real ("micpos") / 10.0f, dist = proc.real ("dist") / 10.0f;
    texto (g, "CAB", { r.getX(), r.getY(), 40, 26 }, corRotulo, fonte (11.0f), juce::Justification::centredLeft);
    const char* nomes[3] = { "1x12 OPEN", "2x12", "4x12" };
    float x = r.getX() + 42;
    for (int c = 0; c < 3; ++c) { const float w = c == 0 ? 92.0f : 82.0f; botao (g, { x, r.getY(), w, 26 }, nomes[c], c == cab, aCab, c, 11.0f); x += w + 6; }

    // a caixa com os falantes e o microfone
    const juce::Rectangle<float> cx (r.getX(), r.getY() + 40, 180, r.getHeight() - 40);
    sombra (g, cx, 8.0f, 12.0f, 10.0f, 0.6f);
    g.setColour (juce::Colour (0xff1a1410)); g.fillRoundedRectangle (cx, 8.0f);
    const auto tela = cx.reduced (9);
    g.setColour (juce::Colour (0xff2f281f)); g.fillRoundedRectangle (tela, 4.0f);
    g.setColour (juce::Colour (0xff3b3328));
    for (float t = -tela.getHeight(); t < tela.getWidth(); t += 4.0f)
        g.drawLine (juce::jmax (tela.getX(), tela.getX() + t), tela.getY() + juce::jmax (0.0f, -t), juce::jmin (tela.getRight(), tela.getX() + t + tela.getHeight()),
                    tela.getY() + juce::jmin (tela.getHeight(), tela.getWidth() - t), 1.0f);
    std::vector<juce::Point<float>> falantes;
    if (cab == 0) falantes = { tela.getCentre() };
    else if (cab == 1) falantes = { { tela.getCentreX(), tela.getY() + tela.getHeight() * 0.28f }, { tela.getCentreX(), tela.getY() + tela.getHeight() * 0.72f } };
    else falantes = { { tela.getX() + tela.getWidth() * 0.28f, tela.getY() + tela.getHeight() * 0.28f }, { tela.getX() + tela.getWidth() * 0.72f, tela.getY() + tela.getHeight() * 0.28f },
                      { tela.getX() + tela.getWidth() * 0.28f, tela.getY() + tela.getHeight() * 0.72f }, { tela.getX() + tela.getWidth() * 0.72f, tela.getY() + tela.getHeight() * 0.72f } };
    const float rf = cab == 2 ? 34.0f : cab == 1 ? 50.0f : 66.0f;
    for (auto f : falantes)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), f.x, f.y, juce::Colour (0xff121212), f.x + rf, f.y + rf, true));
        g.fillEllipse (f.x - rf, f.y - rf, rf * 2, rf * 2);
        g.setColour (juce::Colour (0xff6d6252).withAlpha (0.6f)); g.drawEllipse (f.x - rf, f.y - rf, rf * 2, rf * 2, 2.0f);
        g.setColour (juce::Colour (0xff555555)); g.fillEllipse (f.x - rf * 0.2f, f.y - rf * 0.2f, rf * 0.4f, rf * 0.4f);
    }
    // microfone: do centro do 1º falante (MIC POS 0) até a borda (10); mais longe = menor
    const auto f0 = falantes[0];
    const juce::Point<float> pm (f0.x + rf * 0.95f * pos, f0.y);
    const float esc = 1.0f - 0.35f * dist;
    const juce::Rectangle<float> corpoMic (pm.x - 8 * esc, pm.y - 15 * esc, 16 * esc, 30 * esc);
    sombra (g, corpoMic, 4.0f, 3.0f, 3.0f, 0.6f);
    g.setGradientFill (juce::ColourGradient (mic == 0 ? juce::Colour (0xff666666) : juce::Colour (0xff2a2a2a), corpoMic.getX(), 0,
                                             mic == 0 ? juce::Colour (0xffdddddd) : juce::Colour (0xff8a7a5a), corpoMic.getCentreX(), 0, true));
    g.fillRoundedRectangle (corpoMic, 4.0f * esc);

    const float cxm = r.getX() + 255;
    const char* nm[2] = { "DYN", "RIBBON" };
    chave (g, { cxm, r.getY() + 80 }, 2, mic, nm, corRotulo, 9.5f);
    alvos.push_back ({ { cxm - 60, r.getY() + 64, 120, 32 }, aMic });
    knobP (g, "micpos", { cxm, r.getY() + 150 }, 38, 1, "MIC POS", corRotulo, 3.0f, -1, proc.real ("micpos"));
    knobP (g, "dist", { cxm, r.getY() + 230 }, 38, 1, "DISTANCE", corRotulo, 2.0f, -1, proc.real ("dist"));
    texto (g, u8 ("centro \xe2\x86\x92 borda"), { cxm - 60, r.getY() + 186, 120, 12 }, corSuave, fonte (9.0f, false));
    texto (g, u8 ("perto \xe2\x86\x92 longe"), { cxm - 60, r.getY() + 266, 120, 12 }, corSuave, fonte (9.0f, false));
}

// --------------------------------------------------------------------------------------------------------------
// seletor de pedais (por cima de tudo)

void StompboxEditor::seletor (juce::Graphics& g)
{
    alvos.clear();   // com o seletor aberto, só ele responde
    g.setColour (juce::Colour (0xb8040c0b)); g.fillRect (0, 0, baseW, baseH);
    const juce::Rectangle<float> caixa (170, 70, 1260, 760);
    sombra (g, caixa, 14.0f, 20.0f, 20.0f, 0.7f);
    g.setColour (juce::Colour (0xff10211f)); g.fillRoundedRectangle (caixa, 14.0f);
    g.setColour (juce::Colour (0xff2f5a53)); g.drawRoundedRectangle (caixa, 14.0f, 1.0f);
    texto (g, (seletorTroca ? u8 ("TROCAR PEDAL \xc2\xb7 POSI\xc3\x87\xc3\x83O ") : u8 ("ESCOLHER PEDAL \xc2\xb7 POSI\xc3\x87\xc3\x83O ")) + juce::String (seletorSlot + 1),
           { caixa.getX() + 26, caixa.getY() + 18, 400, 26 }, corTexto, fonte (15.0f), juce::Justification::centredLeft);
    const char* cats[8] = { "TODOS", "DIN\xc3\x82MICA", "DRIVE", "FILTRO", "PITCH", "MODULA\xc3\x87\xc3\x83O", "TEMPO", "AMBI\xc3\x8ANCIA" };
    float x = caixa.getRight() - 26 - 40 - 16;
    float larg[8]; for (int c = 7; c >= 0; --c) { larg[c] = c == 5 || c == 7 ? 96.0f : c == 1 ? 86.0f : 70.0f; x -= larg[c] + 6; }
    x += 6;
    for (int c = 0; c < 8; ++c) { botao (g, { x, caixa.getY() + 18, larg[c], 26 }, u8 (cats[c]), seletorCat == c, aPickCat, c, 10.0f); x += larg[c] + 6; }
    botao (g, { caixa.getRight() - 26 - 40, caixa.getY() + 18, 40, 26 }, u8 ("\xe2\x9c\x95"), false, aPickFecha);

    int i = 0;
    for (int t = 1; t < stomp::numTipos; ++t)
    {
        const auto& inf = infoPedal (t);
        if (seletorCat > 0 && juce::String::fromUTF8 (inf.categoria) != u8 (cats[seletorCat])) continue;
        const juce::Rectangle<float> cel (caixa.getX() + 26 + (float) (i % 6) * 202.0f, caixa.getY() + 64 + (float) (i / 6) * 340.0f, 194, 330);
        g.setColour (juce::Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (cel, 10.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            const float e = 0.68f;
            g.addTransform (juce::AffineTransform::scale (e, e, cel.getCentreX(), cel.getY() + 8));
            StompboxProcessor::SlotDados d; d.tipo = t; d.on = true;
            for (int k = 0; k < 4; ++k) d.k[k] = inf.def[k];
            const size_t antes = alvos.size();
            desenhaPedal (g, t, { cel.getCentreX(), cel.getY() + 8 + 400.0f }, d, 0, false);
            alvos.resize (antes);
        }
        texto (g, inf.nome, { cel.getX(), cel.getBottom() - 46, cel.getWidth(), 18 }, juce::Colour (0xffe3efec), fonte (13.0f));
        texto (g, u8 (inf.categoria) + u8 (" \xc2\xb7 ") + inf.tipo, { cel.getX(), cel.getBottom() - 26, cel.getWidth(), 14 }, juce::Colour (0xff7fb3a8), fonte (9.5f));
        alvos.push_back ({ cel, aPick, -1, t });
        ++i;
    }
}

// --------------------------------------------------------------------------------------------------------------
// dinâmico: medidores, afinador, valor do knob

void StompboxEditor::medidor (juce::Graphics& g, juce::Rectangle<float> r, float nivel)
{
    g.setColour (juce::Colour (0xff061010)); g.fillRoundedRectangle (r, 2.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 2.0f, 1.0f);
    const float db = 20.0f * std::log10 (juce::jmax (nivel, 1.0e-5f));
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const auto in = r.reduced (1.5f);
    const float h = in.getHeight() * frac;
    juce::ColourGradient cg (juce::Colour (0xff3ce07a), in.getX(), in.getBottom(), juce::Colour (0xffff4a2a), in.getX(), in.getY(), false);
    cg.addColour (0.7, juce::Colour (0xff3ce07a)); cg.addColour (0.85, juce::Colour (0xffe8d43a));
    g.setGradientFill (cg); g.fillRect (in.getX(), in.getBottom() - h, in.getWidth(), h);
    if (db > -0.5f) { g.setColour (juce::Colour (0xffff4a2a)); g.fillRect (in.withHeight (4.0f)); }
}

void StompboxEditor::analisaAfinador()
{
    // YIN no último pedaço da entrada (dizimado por 2)
    const double fsd = proc.sampleRate() / 2.0;
    constexpr int W = 1024, maxTau = 520;
    float x[W + maxTau];
    int p = proc.posAfin.load();
    const int n = W + maxTau;
    for (int i = n - 1; i >= 0; --i)
    {
        p = (p - 2 + StompboxProcessor::tamAfin) % StompboxProcessor::tamAfin;
        x[i] = 0.5f * (proc.bufAfin[p] + proc.bufAfin[(p + 1) % StompboxProcessor::tamAfin]);
    }
    float rms = 0; for (int i = 0; i < W; ++i) rms += x[i] * x[i];
    rms = std::sqrt (rms / W);
    if (rms < 0.004f) { afinVis = juce::jmax (0.0f, afinVis - 0.1f); return; }
    static float dfn[maxTau];
    const int tauMin = (int) (fsd / 1200.0), tauMax = juce::jmin (maxTau - 2, (int) (fsd / 55.0));
    float soma = 0; dfn[0] = 1;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float d = 0; for (int i = 0; i < W; ++i) { const float v = x[i] - x[i + tau]; d += v * v; }
        soma += d; dfn[tau] = soma > 0 ? d * (float) tau / soma : 1.0f;
    }
    int t = -1;
    for (int tau = juce::jmax (2, tauMin); tau < tauMax; ++tau)
        if (dfn[tau] < 0.15f) { while (tau + 1 < tauMax && dfn[tau + 1] < dfn[tau]) ++tau; t = tau; break; }
    if (t < 0) { afinVis = juce::jmax (0.0f, afinVis - 0.1f); return; }
    const float a = dfn[t - 1], b = dfn[t], c = dfn[t + 1];
    const float den = a - 2 * b + c;
    const float tf = (float) t + (std::abs (den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f);
    const float hz = (float) fsd / tf;
    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int nota = juce::roundToInt (midi);
    static const char* nomes[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    afinHz = hz; afinCents = afinCents * 0.6f + (midi - (float) nota) * 100.0f * 0.4f;
    afinNota = juce::String (nomes[(nota % 12 + 12) % 12]) + juce::String (nota / 12 - 1);
    afinVis = 1.0f;
}

void StompboxEditor::desenhaAfinador (juce::Graphics& g)
{
    const juce::Rectangle<float> r (400, 150, 800, 300);
    g.setColour (juce::Colour (0xf0081010)); g.fillRoundedRectangle (r, 14.0f);
    g.setColour (juce::Colour (0xff2f5a53)); g.drawRoundedRectangle (r, 14.0f, 1.5f);
    texto (g, u8 ("TUNER \xc2\xb7 sa\xc3\xad""da muda"), { r.getX() + 20, r.getY() + 12, 300, 18 }, corSuave, fonte (11.0f), juce::Justification::centredLeft);
    const bool tem = afinVis > 0.05f;
    const bool certo = tem && std::abs (afinCents) < 3.0f;
    const juce::Colour c = certo ? juce::Colour (0xff3ce07a) : juce::Colour (0xffff9a3a);
    texto (g, tem ? afinNota : "--", { r.getX(), r.getY() + 40, r.getWidth(), 110 }, tem ? c : corSuave, fonte (96.0f, true, "Arial Black"));
    texto (g, tem ? juce::String (afinHz, 1) + " Hz   " + (afinCents >= 0 ? "+" : "") + juce::String (afinCents, 0) + " cents" : u8 ("toque uma corda"),
           { r.getX(), r.getY() + 150, r.getWidth(), 20 }, corTexto, fonte (13.0f, false));
    // escala de -50 a +50 cents
    const juce::Rectangle<float> esc (r.getX() + 80, r.getY() + 200, r.getWidth() - 160, 60);
    for (int k = -50; k <= 50; k += 5)
    {
        const float xx = esc.getX() + (float) (k + 50) / 100.0f * esc.getWidth();
        g.setColour (k == 0 ? juce::Colour (0xff3ce07a) : corSuave.withAlpha (0.6f));
        g.drawLine (xx, esc.getY() + (k % 25 == 0 ? 0.0f : 18.0f), xx, esc.getBottom() - 20, k == 0 ? 3.0f : 1.0f);
    }
    if (tem)
    {
        const float xx = esc.getX() + (juce::jlimit (-50.0f, 50.0f, afinCents) + 50.0f) / 100.0f * esc.getWidth();
        g.setColour (c); g.fillRoundedRectangle (xx - 4, esc.getY() - 6, 8, esc.getHeight() - 8, 3.0f);
    }
}

void StompboxEditor::bolhaValor (juce::Graphics& g)
{
    const auto t = textoKnob (alvoArrasto);
    if (t.isEmpty()) return;
    const auto f = fonte (13.0f);
    const float w = juce::GlyphArrangement::getStringWidth (f, t) + 18.0f;
    const juce::Rectangle<float> r (alvoArrasto.r.getCentreX() - w * 0.5f, alvoArrasto.r.getY() - 30, w, 24);
    g.setColour (juce::Colour (0xf0081010)); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (corAcento); g.drawRoundedRectangle (r, 6.0f, 1.0f);
    texto (g, t, r, juce::Colour (0xff9ff3d8), f);
}
