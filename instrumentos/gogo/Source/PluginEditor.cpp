#include "PluginEditor.h"

namespace
{
// paleta do esboço v2: escuro arroxeado; ROSA = voz / ligado, VERDE = TUNE, ÂMBAR = RIDE / FET, LILÁS = redução,
// TURQUESA = DE-ESS e automático, LARANJA = voz original no gráfico de afinação
const juce::Colour cBg { 0xff120f1a }, cBgB { 0xff0d0b13 }, cPn { 0xff1a1624 }, cPn2 { 0xff25212f }, cLn { 0xff35304a },
                   cTx { 0xfff3efff }, cMu { 0xff8b84a3 }, cDim { 0xff5d5772 }, cVox { 0xffff7fb5 }, cOk { 0xff3fd68a },
                   cAmb { 0xfff5c451 }, cLil { 0xffc9b0ff }, cTeal { 0xff4fd1c5 }, cRed { 0xffff4f8b }, cOrig { 0xffff8a3d },
                   cTela { 0xff07060c }, cGrade { 0xff181425 }, cGradeF { 0xff2b2442 };

constexpr int menuSobre = 1, menuCopiaAB = 2, menuLicenca = 3;

juce::Font fonte (float h, bool negrito = false)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain));
}
float largura (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::String sinal (float v, int casas, const char* un)
{
    if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -casas)) return (casas ? u8 ("0.") + juce::String::repeatedString ("0", casas) : u8 ("0")) + u8 (un);
    const juce::String num = casas > 0 ? juce::String (std::abs (v), casas) : juce::String (juce::roundToInt (std::abs (v)));
    return (v > 0 ? "+" : u8 ("−")) + num + u8 (un);
}
juce::String reducao (float gr) { return gr < 0.05f ? u8 ("0.0 dB") : u8 ("−") + juce::String (gr, 1) + " dB"; }
juce::String hz (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; }

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

void caixa (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fundo = cPn)
{
    g.setColour (fundo);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (cLn);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void rotulo (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor = cMu,
             juce::Justification j = juce::Justification::centredLeft, float h = 11.0f)
{
    g.setColour (cor);
    g.setFont (fonte (h).withExtraKerningFactor (0.2f));
    g.drawText (t, r, j);
}

void legenda (juce::Graphics& g, float& x, float y, juce::Colour cor, const juce::String& t, bool linha = false)
{
    if (linha) { g.setColour (cor); g.fillRect (x, y - 1.0f, 14.0f, 2.0f); }
    else { g.setColour (cor); g.fillRect (x + 3, y - 5.0f, 8.0f, 10.0f); }
    const auto f = fonte (10.5f).withExtraKerningFactor (0.15f);
    const float w = largura (f, t) + 2;
    g.setColour (cMu); g.setFont (f);
    g.drawText (t, juce::Rectangle<float> (x + 18, y - 7, w, 14), juce::Justification::centredLeft);
    x += 18 + w + 14;
}

// símbolo de power (anel aberto + traço)
void power (juce::Graphics& g, juce::Point<float> c, float r, bool on)
{
    const juce::Colour cor = on ? cOk : cDim;
    if (on) { g.setColour (cor.withAlpha (0.18f)); g.fillEllipse (juce::Rectangle<float> (r * 2.6f, r * 2.6f).withCentre (c)); }
    g.setColour (cor);
    g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 1.5f);
    juce::Path p;
    const float a = r * 0.5f;
    p.addCentredArc (c.x, c.y, a, a, 0.0f, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (c.x, c.y - a * 1.25f, c.x, c.y - a * 0.1f, 1.6f);
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

juce::String cents (float v)
{
    const int c = juce::roundToInt (v);
    if (c == 0) return u8 ("±0 ¢");
    return (c > 0 ? "+" : u8 ("−")) + juce::String (std::abs (c)) + u8 (" ¢");
}

// o que cada bloco mostra quando os ajustes abrem: id, nome, descrição (PT)
struct Ctl { const char* id; const char* nome; const char* desc; };
struct Painel { const char* titulo; const char* texto; std::vector<Ctl> ctl; };

const Painel& painel (int b)
{
    static const Painel p[gogo::numBlocos] = {
        { "CLEAN · HPF + POPS + GATE",
          "O HPF corta o grave que não é voz (24 dB/oitava). No AUTO ele fica em 0,7 × a nota mais grave que você cantou. "
          "O P e o B (rajada de grave) são cortados na hora e contados. O GATE abaixa o ruído entre as frases.",
          { { "hpfauto", "HPF AUTO", "segue a nota mais grave" }, { "hpf", "HPF", "com o AUTO desligado" },
            { "gate", "GATE", "abaixa o ruído entre as frases" } } },
        { "TUNE", "", {} },
        { "RIDE · VOCAL RIDER",
          "Antes da compressão, o RIDE sobe a frase baixa e abaixa a alta, devagar, para cada trecho cantado chegar a "
          "−18 dBFS RMS. No silêncio ele para onde estava (não sobe o ruído). RANGE é o quanto ele pode mexer.",
          { { "riderange", "RANGE", "quanto o RIDE pode subir ou descer" } } },
        { "FET · PEAK CATCHER",
          "Compressor rápido (estilo 1176): segura só o pico da sílaba forte, com ataque de 0,3 ms. O quanto ele "
          "comprime vem do TAME. KEEP IT NATURAL deixa o RATIO em 4:1.",
          { { "fetratio", "RATIO", "4:1 natural · 20:1 esmaga" }, { "fetrel", "RELEASE", "volta depois do pico" },
            { "tame", "TAME", "FET + OPTO juntos" } } },
        { "OPTO · LEVELER",
          "Nivelador (estilo LA-2A): segura a frase inteira, com ataque de 10 ms. Metade da redução volta em 60 ms e o "
          "resto em 0,5 a 4 s: quanto mais tempo comprimiu, mais devagar volta. FET e OPTO devolvem 80% da redução "
          "média (o volume não cai quando o TAME sobe).",
          { { "optoemph", "HF EMPHASIS", "mais agudo no detector" }, { "tame", "TAME", "FET + OPTO juntos" } } },
        { "TONE · DYNAMIC EQ",
          "MUD corta o grave embolado (300 Hz) quando ele passa da média da SUA voz; HARSH corta o 3,2 kHz que fura só "
          "quando aparece. TONE inclina para escuro ou brilhante e AIR abre o 11 kHz. Com o MAESTRO LINK, um realce "
          "de +1,5 dB entra onde os synths deixam espaço.",
          { { "mud", "MUD", "corte dinâmico em 300 Hz" }, { "harsh", "HARSH", "corte dinâmico em 3,2 kHz" },
            { "tone", "TONE", "escuro / brilhante" }, { "air", "AIR", "shelf em 11 kHz" } } },
        { "DE-ESS · SPLIT-BAND",
          "Tira só a parte de cima (acima de FREQ) e só quando ela chega perto do nível da voz inteira: o S baixa e a "
          "vogal fica igual. LISTEN toca só o que está sendo tirado.",
          { { "deess", "DE-ESS", "quanto tira" }, { "deessfreq", "FREQ", "onde começa o S" },
            { "deesslisten", "LISTEN", "ouvir o que sai" } } },
        { "WARMTH · TUBE / TAPE",
          "Saturação leve: TUBE soma harmônicos pares (mais encorpado), TAPE arredonda o pico e tira um pouco de agudo. "
          "No zero o bloco não mexe no som.",
          { { "warmtype", "TYPE", "TUBE ou TAPE" }, { "warmth", "DRIVE", "quanto satura" } } },
        { "SPACE · PLATE + DELAY",
          "PLATE e DELAY no tempo do DAW, os dois abaixando enquanto você canta (DUCK) e voltando nas pausas: a voz fica "
          "na frente e o espaço aparece entre as frases. O GUARD segura o efeito 5 dB abaixo da voz: com SPACE, DECAY e "
          "FEEDBACK no máximo ele não ressoa nem distorce a saída (que tem um limitador em −1 dBFS).",
          { { "space", "SPACE", "quanto efeito" }, { "delaynote", "DELAY", "tempo (no BPM do DAW)" },
            { "feedback", "FEEDBACK", "repetições" }, { "decay", "DECAY", "tamanho da plate" },
            { "duck", "DUCK", "abaixa enquanto canta" }, { "blend", "PLATE / DELAY", "mistura dos dois" } } },
    };
    return p[juce::jlimit (0, gogo::numBlocos - 1, b)];
}

const char* subtitulo (int b)
{
    static const char* s[gogo::numBlocos] = { "HPF · GATE", "PITCH FIX", "VOCAL RIDER", "1176-STYLE",
                                              "LA-STYLE", "DYNAMIC EQ", "SPLIT-BAND", "TUBE / TAPE", "PLATE + DELAY" };
    return s[juce::jlimit (0, gogo::numBlocos - 1, b)];
}
} // namespace

GogoEditor::GogoEditor (GogoProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (840, 532, 2400, 1520);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (840, 2400, w0) : 1080;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));
    startTimerHz (30);
    ativacao.anexar (*this);
}

void GogoEditor::resized() { proc.uiW = getWidth(); }

void GogoEditor::timerCallback() { ++quadro; repaint(); }

juce::RangedAudioParameter* GogoEditor::param (const juce::String& id) const
{
    return dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
}

float GogoEditor::real (const juce::String& id) const
{
    if (auto* p = param (id)) return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

juce::String GogoEditor::texto (const juce::String& id) const
{
    const float v = real (id);
    if (id == "tone") return juce::roundToInt (v) == 0 ? u8 ("0") : (v > 0 ? "BRIGHT " : "DARK ") + juce::String (std::abs (juce::roundToInt (v)));
    if (id == "riderange") return u8 ("±") + juce::String (v, 1) + " dB";
    if (id == "air" || id == "output" || id == "input") return sinal (v, 1, " dB");
    if (id == "formant") return sinal (v, 1, " st");
    if (id == "hpf" || id == "deessfreq") return hz (v);
    if (id == "fetrel") return juce::String (juce::roundToInt (v)) + " ms";
    if (id == "decay") return juce::String (v, 1) + " s";
    if (id == "tune" && v < 0.5f) return "OFF";
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (param (id))) return c->getCurrentChoiceName();
    if (auto* b = dynamic_cast<juce::AudioParameterBool*> (param (id))) return b->get() ? "ON" : "OFF";
    return juce::String (juce::roundToInt (v)) + "%";
}

void GogoEditor::tomAtual (int& tom, int& esc) const
{
    auto& L = proc.cadeia().tune().leitura;
    const int r = escolha ("root"), s = escolha ("scale");
    const int det = L.tomDet.load();
    tom = r > 0 ? r - 1 : det;
    esc = s > 0 ? s - 1 : (det >= 0 ? (L.modoDet.load() ? onkey::escMenor : onkey::escMaior) : -1);
    if (tom < 0 && esc >= 0 && esc != onkey::escCromatica) esc = -1;
}

// =====================================================================================================================
void GogoEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    g.setGradientFill (juce::ColourGradient (cBg, 0, 0, cBgB, 0, (float) baseH, false));
    g.fillRect (juce::Rectangle<float> (0, 0, (float) baseW, (float) baseH));
    knobs.clear();
    alvos.clear();

    topo (g);
    const int s = proc.sel.load();
    const juce::Rectangle<float> rEsq (16, 68, 860, 290), rDir (888, 68, 296, 290);
    caixa (g, rEsq);
    if (s >= 0) ajustes (g, rEsq, s); else dinamica (g, rEsq);
    caixa (g, rDir);
    medidores (g, rDir);
    cadeia (g, { 16, 370, 1168, 160 });
    base (g);
}

// ---- topo -----------------------------------------------------------------------------------------------------------
void GogoEditor::topo (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 0, (float) baseW, 56);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1827), 0, 0, juce::Colour (0xff141020), 0, 56, false));
    g.fillRect (r);
    g.setColour (cLn); g.drawLine (0, 56, (float) baseW, 56, 1.0f);

    // marca: GO G Ó (o segundo G em rosa) e uma onda de voz que assenta
    const auto f = fonte (30.0f, true).withExtraKerningFactor (0.28f);
    float x = 22;
    const juce::String partes[3] = { "GO", "G", u8 ("Ó") };
    const juce::Colour cores[3] = { cTx, cVox, cTx };
    g.setFont (f);
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (cores[i]);
        g.drawText (partes[i], juce::Rectangle<float> (x, 10, 80, 36), juce::Justification::centredLeft);
        x += largura (f, partes[i]);
    }
    {
        juce::Path pw;
        for (int i = 0; i <= 28; ++i)
        {
            const float xx = x + 6 + i * 1.3f, t = i / 28.0f;
            const float y = 28 - std::sin (t * 6.2832f * 2.2f) * 8.0f * (1.0f - t * t);
            if (i == 0) pw.startNewSubPath (xx, y); else pw.lineTo (xx, y);
        }
        g.setColour (cVox); g.strokePath (pw, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // pílulas da direita (da direita para a esquerda)
    float xr = (float) baseW - 16;
    auto pilula = [&] (const juce::String& rot, const juce::String& val, juce::Colour corVal, bool comLed, bool aceso,
                       std::function<void()> fn)
    {
        const auto fr = fonte (11.0f).withExtraKerningFactor (0.18f), fv = fonte (11.5f, true).withExtraKerningFactor (0.1f);
        const float w = (rot.isEmpty() ? 0.0f : largura (fr, rot) + 8) + largura (fv, val) + (comLed ? 40.0f : 24.0f);
        xr -= w;
        const juce::Rectangle<float> c (xr, 14, w, 28);
        g.setColour (juce::Colour (0xff171320)); g.fillRoundedRectangle (c, 4.0f);
        g.setColour (cLn); g.drawRoundedRectangle (c, 4.0f, 1.0f);
        float tx = c.getX() + 12;
        if (comLed) { led (g, { tx + 2, c.getCentreY() }, 3.5f, corVal, aceso); tx += 14; }
        if (rot.isNotEmpty())
        {
            g.setColour (cMu); g.setFont (fr);
            g.drawText (rot, juce::Rectangle<float> (tx, c.getY(), 200, c.getHeight()), juce::Justification::centredLeft);
            tx += largura (fr, rot) + 8;
        }
        g.setColour (corVal); g.setFont (fv);
        g.drawText (val, juce::Rectangle<float> (tx, c.getY(), 300, c.getHeight()), juce::Justification::centredLeft);
        if (fn) alvos.push_back ({ c, fn });
        xr -= 8;
    };
    pilula ({}, u8 ("≡"), cTx, false, false, [this] { menu(); });
    // MAESTRO
    {
        const bool lk = proc.liga ("link");
        const int n = proc.linkGrupos();
        const juce::String v = ! lk ? juce::String ("OFF")
                                    : (n > 0 ? "LINKED " + u8 ("· ") + juce::String (n) + (n == 1 ? " GROUP" : " GROUPS") : juce::String ("NOT FOUND"));
        pilula ("MAESTRO", v, n > 0 && lk ? cOk : cMu, true, n > 0 && lk, [this] { proc.alterna ("link"); });
    }
    // LATENCY
    {
        const int lat = juce::roundToInt (1000.0 * proc.latenciaAgora() / juce::jmax (1.0, proc.sampleRate()));
        pilula ("LATENCY", juce::String (lat) + " ms" + (proc.liga ("tracking") ? u8 (" · TRACKING") : juce::String()),
                proc.liga ("tracking") ? cAmb : cTx, false, false, [this] { proc.alterna ("tracking"); });
    }
    // KEY
    {
        int tom = -1, esc = -1;
        tomAtual (tom, esc);
        const bool manual = escolha ("root") > 0 && escolha ("scale") > 0;
        const juce::String v = (manual ? u8 ("MANUAL · ") : u8 ("AUTO · ")) + (esc >= 0 ? nomeTom (tom, esc) : juce::String ("LISTENING"));
        pilula ("KEY", v, esc >= 0 ? cOk : cMu, false, false, [this] { proc.sel = gogo::bTune; });
    }
    pilula ("ROLE", juce::String (gogo::nomeRole (escolha ("role"))) + u8 (" ▾"), cTx, false, false,
            [this] { menuEscolha ("role", { gogo::nomeRole (0), gogo::nomeRole (1), gogo::nomeRole (2), gogo::nomeRole (3) }); });
    pilula ("STYLE", juce::String (gogo::nomeStyle (escolha ("style"))) + u8 (" ▾"), cTx, false, false, [this] { menuStyle(); });
    // subtítulo só se couber antes das pílulas (STYLE/ROLE longos + MAESTRO LINKED ocupam quase o topo todo)
    const auto fs = fonte (12.0f).withExtraKerningFactor (0.2f);
    if (x + 52 + largura (fs, u8 ("VOCAL CHAIN · AUTO")) + 12 < xr)
        rotulo (g, { x + 52, 21, 220, 16 }, u8 ("VOCAL CHAIN · AUTO"), cMu, juce::Justification::centredLeft, 12.0f);
}

// ---- VOCAL DYNAMICS -------------------------------------------------------------------------------------------------
void GogoEditor::dinamica (juce::Graphics& g, juce::Rectangle<float> r)
{
    rotulo (g, { r.getX() + 16, r.getY() + 10, 300, 16 }, u8 ("VOCAL DYNAMICS · LAST 8 s"));
    float lx = r.getRight() - 420;
    legenda (g, lx, r.getY() + 18, juce::Colour (0xff4a4560), "IN");
    legenda (g, lx, r.getY() + 18, cVox, "OUT");
    legenda (g, lx, r.getY() + 18, cAmb, "RIDE", true);
    legenda (g, lx, r.getY() + 18, cLil, "GAIN REDUCTION");

    const juce::Rectangle<float> t (r.getX() + 12, r.getY() + 34, r.getWidth() - 24, r.getHeight() - 46);
    g.setColour (cTela); g.fillRoundedRectangle (t, 4.0f);
    auto& H = proc.cadeia().hist;
    const int pos = H.pos.load (std::memory_order_acquire);
    const int nVis = 800;
    const float meio = t.getY() + t.getHeight() * 0.60f, meiaAlt = t.getHeight() * 0.36f;
    const float topoGr = t.getY() + 6, altGr = 46.0f;
    const float yRide = t.getY() + 30;

    g.saveState();
    g.reduceClipRegion (t.toNearestInt());
    // grade: −12 / −24 / −36 dB
    for (float d : { -12.0f, -24.0f, -36.0f })
    {
        const float a = (d + 60.0f) / 60.0f * meiaAlt;
        g.setColour (cGrade);
        g.drawLine (t.getX(), meio - a, t.getRight(), meio - a, 1.0f);
        g.drawLine (t.getX(), meio + a, t.getRight(), meio + a, 1.0f);
        g.setColour (cDim); g.setFont (fonte (9.5f));
        g.drawText (u8 ("−") + juce::String ((int) -d), juce::Rectangle<float> (t.getX() + 4, meio - a - 12, 30, 11), juce::Justification::centredLeft);
    }
    g.setColour (juce::Colour (0xff2a2536)); g.drawLine (t.getX(), meio, t.getRight(), meio, 1.0f);

    const float dx = t.getWidth() / (float) nVis;
    juce::Path gr, ride;
    gr.startNewSubPath (t.getX(), topoGr);
    bool rideIni = false;
    for (int i = 0; i < nVis; ++i)
    {
        const int idx = pos - nVis + i;
        const float x = t.getX() + i * dx;
        if (idx < 0) { gr.lineTo (x, topoGr); continue; }
        const int k = idx % gogo::Historico::size;
        const float aIn = juce::jlimit (0.0f, 1.0f, (H.in[k] + 60.0f) / 60.0f) * meiaAlt;
        const float aOut = juce::jlimit (0.0f, 1.0f, (H.out[k] + 60.0f) / 60.0f) * meiaAlt;
        g.setColour (juce::Colour (0xff3a3550));
        g.fillRect (x, meio - aIn, juce::jmax (1.0f, dx * 0.9f), aIn * 2.0f);
        g.setColour (cVox.withAlpha (0.85f));
        g.fillRect (x + dx * 0.2f, meio - aOut, juce::jmax (0.8f, dx * 0.55f), aOut * 2.0f);
        gr.lineTo (x, topoGr + juce::jlimit (0.0f, 1.0f, H.gr[k] / 12.0f) * altGr);
        const float yr = yRide - juce::jlimit (-12.0f, 12.0f, H.ride[k]) * 1.6f;
        if (! rideIni) { ride.startNewSubPath (x, yr); rideIni = true; } else ride.lineTo (x, yr);
    }
    gr.lineTo (t.getRight(), topoGr); gr.closeSubPath();
    g.setColour (cLil.withAlpha (0.35f)); g.fillPath (gr);
    if (rideIni && proc.liga ("on_ride"))
    {
        g.setColour (cAmb.withAlpha (0.25f)); g.drawLine (t.getX(), yRide, t.getRight(), yRide, 1.0f);
        g.setColour (cAmb); g.strokePath (ride, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        rotulo (g, { t.getRight() - 120, yRide - 24, 112, 12 }, "RIDE " + sinal (proc.cadeia().med.rideDb.load(), 1, " dB"), cAmb,
                juce::Justification::centredRight, 10.0f);
    }
    g.restoreState();

    if (pos == 0)
    {
        g.setColour (cMu); g.setFont (fonte (13.0f));
        g.drawText (u8 ("Toque a voz: o GOGÓ ajusta a cadeia sozinho."), t, juce::Justification::centred);
    }
}

// ---- OPTO VU e reduções ---------------------------------------------------------------------------------------------
void GogoEditor::medidores (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& m = proc.cadeia().med;
    rotulo (g, { r.getX() + 16, r.getY() + 10, 200, 16 }, u8 ("OPTO · LA-STYLE"));
    rotulo (g, { r.getRight() - 136, r.getY() + 10, 120, 16 }, "AUTO-RELEASE", cVox, juce::Justification::centredRight);

    // VU de redução (creme): 0 à direita, −20 à esquerda
    const juce::Rectangle<float> v (r.getX() + 14, r.getY() + 34, r.getWidth() - 28, 120);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3ead8), v.getCentreX(), v.getBottom(), juce::Colour (0xffdccfb4), v.getX(), v.getY(), true));
    g.fillRoundedRectangle (v, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (v, 5.0f, 1.0f);
    const juce::Point<float> c (v.getCentreX(), v.getBottom() - 8);
    const float R = 92.0f;
    auto ang = [] (float gr) { return juce::degreesToRadians (-48.0f + (20.0f - juce::jlimit (0.0f, 20.0f, gr)) / 20.0f * 96.0f); };
    for (float t : { 20.0f, 10.0f, 7.0f, 5.0f, 3.0f, 2.0f, 1.0f, 0.0f })
    {
        const float a = ang (t);
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.setColour (juce::Colour (0xff2a2433));
        g.drawLine (c.x + d.x * (R - 8), c.y + d.y * (R - 8), c.x + d.x * R, c.y + d.y * R, 1.6f);
        g.setFont (fonte (9.5f));
        g.setColour (juce::Colour (0xff4a4258));
        g.drawText (t == 0 ? juce::String ("0") : u8 ("−") + juce::String ((int) t),
                    juce::Rectangle<float> (c.x + d.x * (R + 9) - 14, c.y + d.y * (R + 9) - 6, 28, 12), juce::Justification::centred);
    }
    rotulo (g, { v.getX(), v.getY() + 54, v.getWidth(), 14 }, "GAIN REDUCTION", juce::Colour (0xff4a4258), juce::Justification::centred, 10.0f);
    const float gr = m.optoGr.load();
    {
        const float a = ang (gr);
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.setColour (juce::Colour (0xff1a1624));
        g.drawLine (c.x, c.y, c.x + d.x * (R + 2), c.y + d.y * (R + 2), 2.2f);
        g.fillEllipse (juce::Rectangle<float> (14, 14).withCentre (c));
    }
    g.setColour (juce::Colour (0xff4a4258)); g.setFont (fonte (11.5f, true));
    g.drawText (reducao (gr), juce::Rectangle<float> (v.getRight() - 84, v.getBottom() - 22, 76, 16), juce::Justification::centredRight);
    g.setFont (fonte (10.0f));
    g.drawText ("REL " + juce::String (juce::roundToInt (m.optoRelMs.load())) + " ms", juce::Rectangle<float> (v.getX() + 8, v.getBottom() - 22, 90, 16),
                juce::Justification::centredLeft);

    // barras
    auto barra = [&] (float y, const juce::String& nome, float valor, float max, juce::Colour cor, const juce::String& txt)
    {
        rotulo (g, { r.getX() + 16, y, 180, 14 }, nome, cMu, juce::Justification::centredLeft, 10.5f);
        g.setColour (cTx); g.setFont (fonte (11.5f, true));
        g.drawText (txt, juce::Rectangle<float> (r.getRight() - 116, y, 100, 14), juce::Justification::centredRight);
        const juce::Rectangle<float> b (r.getX() + 16, y + 18, r.getWidth() - 32, 7);
        g.setColour (juce::Colour (0xff0d0b13)); g.fillRoundedRectangle (b, 3.5f);
        const float w = juce::jlimit (0.0f, 1.0f, valor / max) * b.getWidth();
        if (w > 0.5f) { g.setColour (cor); g.fillRoundedRectangle (b.withX (b.getRight() - w).withWidth (w), 3.5f); }
    };
    barra (r.getY() + 166, "FET PEAK GR", m.fetGr.load(), 12.0f, cAmb, reducao (m.fetGr.load()));
    barra (r.getY() + 204, u8 ("DE-ESS GR · ") + hz (real ("deessfreq")), m.deessGr.load(), 12.0f, cTeal, reducao (m.deessGr.load()));
    rotulo (g, { r.getX() + 16, r.getY() + 248, 60, 14 }, "OUT", cMu, juce::Justification::centredLeft, 10.5f);
    const float pk = m.outPk.load(), rms = m.outRms.load();
    g.setColour (pk > -1.0f ? cRed : cTx); g.setFont (fonte (11.5f, true));
    g.drawText ((pk < -90 ? u8 ("—") : juce::String (pk, 1)) + " dBFS PEAK " + u8 ("· ") + (rms < -90 ? u8 ("—") : juce::String (rms, 1)) + " RMS",
                juce::Rectangle<float> (r.getX() + 60, r.getY() + 248, r.getWidth() - 76, 14), juce::Justification::centredRight);
    rotulo (g, { r.getX() + 16, r.getY() + 266, r.getWidth() - 32, 14 },
            "INPUT " + sinal (real ("input"), 1, " dB") + u8 (" · MAKEUP ") + sinal (m.makeupDb.load(), 1, " dB"), cDim,
            juce::Justification::centredLeft, 10.0f);
}

// ---- AJUSTES de um bloco --------------------------------------------------------------------------------------------
void GogoEditor::ajustes (juce::Graphics& g, juce::Rectangle<float> r, int b)
{
    const auto& p = painel (b);
    const bool on = proc.liga (GogoProcessor::idBloco (b));
    // título + power + fechar
    const juce::Point<float> pc (r.getX() + 26, r.getY() + 19);
    power (g, pc, 8.0f, on);
    alvos.push_back ({ juce::Rectangle<float> (24, 24).withCentre (pc), [this, b] { proc.alterna (GogoProcessor::idBloco (b)); } });
    rotulo (g, { r.getX() + 42, r.getY() + 10, 500, 18 }, b == gogo::bTune ? u8 ("TUNE · AUTO PITCH (ONKEY ENGINE)") : u8 (p.titulo),
            on ? cTx : cMu, juce::Justification::centredLeft, 12.0f);
    const juce::Rectangle<float> rf (r.getRight() - 150, r.getY() + 8, 136, 22);
    g.setColour (juce::Colour (0xff171320)); g.fillRoundedRectangle (rf, 3.0f);
    g.setColour (cLn); g.drawRoundedRectangle (rf, 3.0f, 1.0f);
    rotulo (g, rf, u8 ("✕  DYNAMICS"), cTx, juce::Justification::centred, 10.5f);
    alvos.push_back ({ rf, [this] { proc.sel = -1; } });

    if (b == gogo::bTune)
    {
        afinacao (g, { r.getX() + 12, r.getY() + 54, 520, r.getHeight() - 66 });
        const float x0 = r.getX() + 548, w = r.getRight() - 12 - x0;
        const float cw = w / 3.0f;
        controle (g, "tune", "TUNE", u8 ("0 = off · 50 = natural · 100 = robô"), { x0, r.getY() + 40, cw, 112 }, cOk);
        controle (g, "speed", "RETUNE SPEED", u8 ("rapidez da correção"), { x0 + cw, r.getY() + 40, cw, 112 }, cOk);
        controle (g, "humanize", "HUMANIZE", u8 ("deixa passar o pequeno"), { x0 + 2 * cw, r.getY() + 40, cw, 112 }, cOk);
        controle (g, "vibrato", "VIBRATO", u8 ("100% = como cantou"), { x0, r.getY() + 160, cw, 112 }, cOk);
        controle (g, "formant", "FORMANT", u8 ("timbre (0 = natural)"), { x0 + cw, r.getY() + 160, cw, 112 }, cLil);
        // ROOT / SCALE
        const float xs = x0 + 2 * cw + 6, ws = cw - 12;
        for (int i = 0; i < 2; ++i)
        {
            const juce::String id = i == 0 ? "root" : "scale";
            const float y = r.getY() + 162 + i * 56;
            rotulo (g, { xs, y, ws, 14 }, i == 0 ? "ROOT" : "SCALE", cMu, juce::Justification::centredLeft, 10.0f);
            const juce::Rectangle<float> bx (xs, y + 16, ws, 28);
            const bool manual = escolha (id) > 0;
            g.setColour (juce::Colour (0xff100e16)); g.fillRoundedRectangle (bx, 3.0f);
            g.setColour (manual ? cLil : cLn); g.drawRoundedRectangle (bx, 3.0f, 1.0f);
            auto& L = proc.cadeia().tune().leitura;
            const int det = L.tomDet.load();
            juce::String t = texto (id);
            if (! manual && det >= 0) t = "AUTO " + u8 ("· ") + (i == 0 ? juce::String (onkey::nomePc (det)) : juce::String (L.modoDet.load() ? "MIN" : "MAJ"));
            g.setColour (manual ? cLil : cTx); g.setFont (fonte (11.5f, manual));
            g.drawFittedText (t + u8 (" ▾"), bx.reduced (8, 0).toNearestInt(), juce::Justification::centredLeft, 1);
            alvos.push_back ({ bx, [this, id, i] { menuEscolha (id, i == 0 ? GogoProcessor::nomesTom() : GogoProcessor::nomesEscala()); } });
        }
        return;
    }

    const int n = (int) p.ctl.size();
    const float x0 = r.getX() + 16, w = juce::jmin (150.0f, (r.getWidth() - 32) / (float) juce::jmax (1, n));
    for (int i = 0; i < n; ++i)
        controle (g, p.ctl[(size_t) i].id, u8 (p.ctl[(size_t) i].nome), u8 (p.ctl[(size_t) i].desc), { x0 + i * w, r.getY() + 44, w, 130 },
                  b == gogo::bDeess ? cTeal : (b == gogo::bRide || b == gogo::bFet ? cAmb : cVox));
    // leituras ao vivo + texto
    juce::String rot[3], val[3];
    leituras (b, rot, val);
    const juce::Rectangle<float> lt (r.getX() + 16, r.getY() + 186, 250, 90);
    g.setColour (cTela); g.fillRoundedRectangle (lt, 4.0f);
    for (int i = 0; i < 3; ++i)
    {
        rotulo (g, { lt.getX() + 12, lt.getY() + 10 + i * 24.0f, 120, 16 }, rot[i], cMu, juce::Justification::centredLeft, 10.5f);
        g.setColour (on ? cVox : cDim); g.setFont (fonte (12.5f, true));
        g.drawText (val[i], juce::Rectangle<float> (lt.getX() + 110, lt.getY() + 10 + i * 24.0f, lt.getWidth() - 122, 16), juce::Justification::centredRight);
    }
    g.setColour (cMu); g.setFont (fonte (12.5f));
    g.drawFittedText (u8 (p.texto), juce::Rectangle<int> ((int) r.getX() + 286, (int) r.getY() + 186, (int) r.getWidth() - 302, 92),
                      juce::Justification::topLeft, 6, 1.0f);
}

// controle genérico: knob (float), caixa com menu (choice) ou chave (bool)
void GogoEditor::controle (juce::Graphics& g, const juce::String& id, const juce::String& nome, const juce::String& desc,
                           juce::Rectangle<float> r, juce::Colour cor)
{
    rotulo (g, { r.getX(), r.getY(), r.getWidth(), 14 }, nome, cTx, juce::Justification::centred, 10.5f);
    auto* prm = param (id);
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (prm))
    {
        const juce::Rectangle<float> bx (r.getCentreX() - 56, r.getY() + 30, 112, 32);
        g.setColour (juce::Colour (0xff100e16)); g.fillRoundedRectangle (bx, 3.0f);
        g.setColour (cor.withAlpha (0.7f)); g.drawRoundedRectangle (bx, 3.0f, 1.0f);
        g.setColour (cTx); g.setFont (fonte (12.0f, true));
        g.drawText (ch->getCurrentChoiceName() + u8 (" ▾"), bx, juce::Justification::centred);
        const auto itens = ch->choices;
        alvos.push_back ({ bx, [this, id, itens] { menuEscolha (id, itens); } });
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (prm) != nullptr)
    {
        const bool on = proc.liga (id);
        const juce::Rectangle<float> bx (r.getCentreX() - 46, r.getY() + 30, 92, 32);
        g.setColour (on ? cor.withAlpha (0.18f) : juce::Colour (0xff100e16)); g.fillRoundedRectangle (bx, 3.0f);
        g.setColour (on ? cor : cLn); g.drawRoundedRectangle (bx, 3.0f, 1.0f);
        led (g, { bx.getX() + 16, bx.getCentreY() }, 4.0f, cor, on);
        g.setColour (on ? cTx : cMu); g.setFont (fonte (12.0f, true));
        g.drawText (on ? "ON" : "OFF", bx.withTrimmedLeft (24), juce::Justification::centred);
        alvos.push_back ({ bx, [this, id] { proc.alterna (id); } });
    }
    else
    {
        knob (g, id, { r.getCentreX(), r.getY() + 46 }, 22.0f, cor, id == "tone" || id == "formant" || id == "air");
        g.setColour (cTx); g.setFont (fonte (12.0f, true));
        g.drawText (texto (id), juce::Rectangle<float> (r.getX(), r.getY() + 76, r.getWidth(), 16), juce::Justification::centred);
    }
    g.setColour (cDim); g.setFont (fonte (10.0f));
    g.drawFittedText (desc, juce::Rectangle<int> ((int) r.getX() + 4, (int) r.getY() + 94, (int) r.getWidth() - 8, 26), juce::Justification::centredTop, 2);
}

// gráfico de afinação (o mesmo do ONKEY): voz original em laranja, corrigida em verde, escala em lilás
void GogoEditor::afinacao (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto& e = proc.cadeia().tune();
    auto& L = e.leitura;
    auto& H = e.hist;
    float lx = r.getX();
    legenda (g, lx, r.getY() - 12, cOrig, "ORIGINAL", true);
    legenda (g, lx, r.getY() - 12, cOk, "TUNED", true);
    legenda (g, lx, r.getY() - 12, cLil.withAlpha (0.5f), "SCALE");

    g.setColour (cTela); g.fillRoundedRectangle (r, 3.0f);
    const double spq = e.segPorQuadro();
    const int nVis = juce::jlimit (50, onkey::Historico::size - 8, (int) std::lround (4.0 / spq));
    const int pos = H.pos.load (std::memory_order_acquire);
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
    const float span = 13.0f, lo = centroVis - span * 0.5f, hi = centroVis + span * 0.5f;
    auto yDe = [&] (float m) { return r.getBottom() - (m - lo) / span * r.getHeight(); };
    int tom = -1, esc = -1;
    tomAtual (tom, esc);
    const unsigned mask = esc >= 0 ? onkey::mascaraTom (juce::jmax (0, tom), esc) : 0u;
    const bool ligado = proc.liga ("on_tune") && real ("tune") > 0.5f && ! proc.liga ("tracking");

    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    for (int k = (int) std::floor (lo) - 1; k <= (int) std::ceil (hi) + 1; ++k)
    {
        const int pc = ((k % 12) + 12) % 12;
        const bool naEscala = (mask >> pc) & 1u;
        const bool raiz = naEscala && tom >= 0 && pc == tom && esc != onkey::escCromatica;
        const float y0 = yDe (k + 0.5f), y1 = yDe (k - 0.5f);
        if (naEscala) { g.setColour (cLil.withAlpha (raiz ? 0.16f : 0.07f)); g.fillRect (r.getX(), y0, r.getWidth(), y1 - y0); }
        g.setColour (naEscala ? cGradeF : cGrade);
        g.drawLine (r.getX() + 36, yDe ((float) k), r.getRight(), yDe ((float) k), 1.0f);
        g.setFont (fonte (9.5f, raiz));
        g.setColour (raiz ? cLil : (naEscala ? cTx : cMu.withAlpha (0.6f)));
        g.drawText (nota (k), juce::Rectangle<float> (r.getX() + 5, yDe ((float) k) - 7, 32, 14), juce::Justification::centredLeft);
    }
    auto curva = [&] (const float* v, juce::Colour cor, float lw)
    {
        juce::Path p; bool aberto = false; float ant = 0;
        for (int i = 0; i < nVis; ++i)
        {
            const int idx = pos - nVis + i;
            if (idx < 0) continue;
            const float m = v[idx % onkey::Historico::size];
            const float x = r.getX() + 36 + (r.getWidth() - 36) * (float) i / (float) (nVis - 1);
            if (m <= 0 || (aberto && std::abs (m - ant) > 3.0f)) { aberto = false; if (m <= 0) continue; }
            if (! aberto) { p.startNewSubPath (x, yDe (m)); aberto = true; } else p.lineTo (x, yDe (m));
            ant = m;
        }
        g.setColour (cor);
        g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    curva (H.entrada, cOrig.withAlpha (0.85f), 1.4f);
    if (ligado) curva (H.saida, cOk, 2.2f);
    g.restoreState();

    // nota agora
    const bool voz = L.voz.load();
    const int nt = L.nota.load();
    const juce::Rectangle<float> nr (r.getRight() - 150, r.getY() + 6, 144, 40);
    g.setColour (cBgB.withAlpha (0.85f)); g.fillRoundedRectangle (nr, 3.0f);
    g.setColour (voz && nt >= 0 ? (ligado ? cOk : cOrig) : cMu); g.setFont (fonte (20.0f, true));
    g.drawText (voz && nt >= 0 ? nota (nt) : u8 ("—"), nr.withWidth (64).withTrimmedLeft (8), juce::Justification::centredLeft);
    g.setFont (fonte (10.5f));
    g.setColour (cOrig); g.drawText (voz ? cents (L.desvio.load()) : u8 ("—"), nr.withTrimmedLeft (66).withHeight (20), juce::Justification::centredLeft);
    g.setColour (cOk); g.drawText (voz && ligado ? sinal (L.correcao.load(), 0, " c fix") : u8 ("—"), nr.withTrimmedLeft (66).withTrimmedTop (20), juce::Justification::centredLeft);

    if (proc.liga ("tracking"))
    {
        g.setColour (cAmb); g.setFont (fonte (12.5f, true));
        g.drawText (u8 ("TRACKING: o TUNE está fora do caminho (latência 0)."), r.withTrimmedLeft (36), juce::Justification::centred);
    }
    else if (L.estado.load() == 0)
    {
        g.setColour (cMu); g.setFont (fonte (12.5f));
        g.drawText (u8 ("Toque a voz: o TUNE acha o tom sozinho."), r.withTrimmedLeft (36), juce::Justification::centred);
    }
}

// ---- a cadeia -------------------------------------------------------------------------------------------------------
void GogoEditor::leituras (int b, juce::String (&rot)[3], juce::String (&val)[3]) const
{
    auto& m = proc.cadeia().med;
    switch (b)
    {
        case gogo::bClean:
        {
            const float gv = m.graveHz.load();
            rot[0] = proc.liga ("hpfauto") ? "HPF AUTO" : "HPF"; val[0] = hz (m.hpfHz.load());
            rot[1] = "low note"; val[1] = gv > 0 ? nota (juce::roundToInt (69.0 + 12.0 * std::log2 (gv / 440.0))) : u8 ("—");
            rot[2] = "pops"; val[2] = juce::String (m.pops.load()) + " tamed";
            break;
        }
        case gogo::bTune:
        {
            auto& L = proc.cadeia().tune().leitura;
            const int nt = L.nota.load();
            rot[0] = "note"; val[0] = L.voz.load() && nt >= 0 ? nota (nt) + " " + cents (L.desvio.load()) : u8 ("—");
            int tom = -1, esc = -1; tomAtual (tom, esc);
            rot[1] = "key"; val[1] = esc >= 0 ? nomeTom (tom, esc) : juce::String ("LISTENING");
            const float tm = real ("tune") * 0.01f;
            float sp = real ("speed") * 0.01f;
            sp += (1.0f - sp) * juce::jmax (0.0f, tm * 2.0f - 1.0f);
            if (proc.liga ("natural")) sp = juce::jmin (sp, 0.75f);
            rot[2] = "speed"; val[2] = tm < 0.005f ? juce::String ("OFF") : (sp < 0.4f ? "NATURAL" : (sp < 0.7f ? "MEDIUM" : (sp < 0.92f ? "FAST" : "ROBOT")));
            break;
        }
        case gogo::bRide:
            rot[0] = "range"; val[0] = texto ("riderange");
            rot[1] = "now"; val[1] = sinal (m.rideDb.load(), 1, " dB");
            rot[2] = "target"; val[2] = escolha ("role") == gogo::rSpoken ? u8 ("−20 dBFS") : u8 ("−18 dBFS");
            break;
        case gogo::bFet:
            rot[0] = "ratio"; val[0] = proc.liga ("natural") ? juce::String ("4:1 NAT") : texto ("fetratio");
            rot[1] = "release"; val[1] = texto ("fetrel");
            rot[2] = "GR"; val[2] = reducao (m.fetGr.load());
            break;
        case gogo::bOpto:
            rot[0] = "peak red."; val[0] = juce::String (juce::roundToInt (real ("tame")));
            rot[1] = "release"; val[1] = u8 ("60 ms→") + juce::String (m.optoRelMs.load() / 1000.0f, 1) + " s";
            rot[2] = "GR"; val[2] = reducao (m.optoGr.load());
            break;
        case gogo::bTone:
        {
            const float pk = m.pocketHz.load();
            rot[0] = "mud 300 Hz"; val[0] = sinal (-m.mudDb.load(), 1, "");
            rot[1] = pk > 100 ? "pocket " + hz (pk) : juce::String ("harsh 3.2 k");
            val[1] = pk > 100 ? juce::String ("+1.5") : sinal (-m.harshDb.load(), 1, "");
            rot[2] = "air 11 k"; val[2] = sinal (real ("air"), 1, "");
            break;
        }
        case gogo::bDeess:
            rot[0] = "freq"; val[0] = hz (real ("deessfreq"));
            rot[1] = "GR"; val[1] = reducao (m.deessGr.load());
            rot[2] = "listen"; val[2] = proc.liga ("deesslisten") ? "S ONLY" : "OFF";
            break;
        case gogo::bWarmth:
        {
            const float w = real ("warmth");
            rot[0] = "type"; val[0] = texto ("warmtype");
            rot[1] = "drive"; val[1] = juce::String (juce::roundToInt (w)) + "%";
            rot[2] = "color"; val[2] = w < 1 ? juce::String ("CLEAN") : (w < 35 ? "SOFT" : (w < 70 ? "WARM" : "HOT"));
            break;
        }
        default:
        {
            const int nt = escolha ("delaynote");
            rot[0] = "plate"; val[0] = texto ("decay") + u8 (" · ") + juce::String (juce::roundToInt (real ("space"))) + "%";
            rot[1] = "delay";
            val[1] = nt == gogo::nOff ? juce::String ("OFF") : texto ("delaynote") + u8 (" · ") + juce::String (juce::roundToInt (m.delayMs.load())) + " ms";
            rot[2] = "duck"; val[2] = sinal (m.duckDb.load(), 0, " dB");
            break;
        }
    }
}

void GogoEditor::cadeia (juce::Graphics& g, juce::Rectangle<float> r)
{
    static const float peso[gogo::numBlocos] = { 1.0f, 1.15f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.25f };
    float soma = 0; for (float p : peso) soma += p;
    const float seta = 14.0f, un = (r.getWidth() - seta * (gogo::numBlocos - 1)) / soma;
    const int s = proc.sel.load();
    float x = r.getX();
    for (int b = 0; b < gogo::numBlocos; ++b)
    {
        const float w = peso[b] * un;
        const juce::Rectangle<float> m (x, r.getY(), w, r.getHeight());
        const bool on = proc.liga (GogoProcessor::idBloco (b));
        const bool tune = b == gogo::bTune;
        g.setColour (tune && on ? juce::Colour (0xff18221f) : cPn); g.fillRoundedRectangle (m, 6.0f);
        g.setColour (s == b ? cLil : cLn); g.drawRoundedRectangle (m, 6.0f, s == b ? 1.6f : 1.0f);
        g.setColour (on ? (tune ? cOk : cVox) : cDim);
        g.fillRoundedRectangle (m.withHeight (3.0f).reduced (4.0f, 0.0f), 1.5f);

        // o bloco inteiro abre os ajustes; o power (registrado depois) ganha dentro dele
        alvos.push_back ({ m, [this, b] { proc.sel = proc.sel.load() == b ? -1 : b; } });
        const juce::Point<float> pc (m.getX() + 18, m.getY() + 20);
        power (g, pc, 8.0f, on);
        alvos.push_back ({ juce::Rectangle<float> (26, 26).withCentre (pc), [this, b] { proc.alterna (GogoProcessor::idBloco (b)); } });
        g.setColour (on ? cTx : cMu); g.setFont (fonte (13.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (gogo::nomeBloco (b), juce::Rectangle<float> (m.getX() + 32, m.getY() + 11, w - 36, 18), juce::Justification::centredLeft);
        // etiqueta AUTO / OFF / DUCK
        {
            const juce::String et = ! on ? "OFF" : (b == gogo::bSpace ? (proc.cadeia().med.spaceGr.load() > 1.0f ? "GUARD" : "DUCK") : "AUTO");
            const auto f = fonte (8.5f).withExtraKerningFactor (0.15f);
            const float ew = largura (f, et) + 10;
            const juce::Rectangle<float> er (m.getRight() - ew - 7, m.getY() + 33, ew, 14);
            g.setColour (on ? juce::Colour (0xff2c5c58) : cLn); g.drawRoundedRectangle (er, 2.5f, 1.0f);
            g.setColour (on ? cTeal : cDim); g.setFont (f); g.drawText (et, er, juce::Justification::centred);
        }
        g.setColour (cMu); g.setFont (fonte (9.0f).withExtraKerningFactor (0.1f));
        g.drawFittedText (u8 (subtitulo (b)), juce::Rectangle<int> ((int) m.getX() + 10, (int) m.getY() + 33, (int) w - 60, 14), juce::Justification::centredLeft, 1, 0.8f);

        juce::String rot[3], val[3];
        leituras (b, rot, val);
        for (int i = 0; i < 3; ++i)
        {
            const float y = m.getY() + 62 + i * 22.0f;
            g.setColour (cMu); g.setFont (fonte (10.0f));
            g.drawFittedText (rot[i], juce::Rectangle<int> ((int) m.getX() + 10, (int) y, (int) (w * 0.48f), 16), juce::Justification::centredLeft, 1, 0.8f);
            g.setColour (on ? (tune ? cOk : cVox) : cDim); g.setFont (fonte (10.5f, true));
            g.drawFittedText (val[i], juce::Rectangle<int> ((int) (m.getX() + w * 0.40f), (int) y, (int) (w * 0.60f) - 10, 16), juce::Justification::centredRight, 1, 0.75f);
        }
        rotulo (g, { m.getX() + 10, m.getBottom() - 22, w - 20, 14 }, s == b ? u8 ("AJUSTES ▴") : u8 ("AJUSTES ▾"), s == b ? cLil : cDim,
                juce::Justification::centredLeft, 9.0f);
        x += w;
        if (b < gogo::numBlocos - 1)
        {
            g.setColour (cDim); g.setFont (fonte (16.0f));
            g.drawText (u8 ("›"), juce::Rectangle<float> (x, m.getCentreY() - 10, seta, 20), juce::Justification::centred);
            x += seta;
        }
    }
}

// ---- base: LEARN, macros, chaves, A/B ------------------------------------------------------------------------------
void GogoEditor::knob (juce::Graphics& g, const juce::String& id, juce::Point<float> c, float raio, juce::Colour cor, bool bipolar)
{
    auto* p = param (id);
    if (p == nullptr) return;
    knobs.push_back ({ id, c, raio });
    const float v = p->getValue();
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float zero = bipolar ? p->convertTo0to1 (0.0f) : 0.0f;
    const float av = a0 + (a1 - a0) * v, am = a0 + (a1 - a0) * zero;
    g.setColour (cPn2); g.fillEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c));
    g.setColour (cLn); g.drawEllipse (juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c), 1.0f);
    const float ra = raio + 5.0f;
    juce::Path tr; tr.addCentredArc (c.x, c.y, ra, ra, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff2a2536)); g.strokePath (tr, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path va; va.addCentredArc (c.x, c.y, ra, ra, 0, juce::jmin (am, av), juce::jmax (am, av), true);
    g.setColour (cor); g.strokePath (va, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const juce::Point<float> d (std::sin (av), -std::cos (av));
    g.setColour (cTx);
    g.drawLine (c.x + d.x * raio * 0.2f, c.y + d.y * raio * 0.2f, c.x + d.x * raio * 0.8f, c.y + d.y * raio * 0.8f, 2.6f);
}

void GogoEditor::base (juce::Graphics& g)
{
    const juce::Rectangle<float> r (0, 544, (float) baseW, (float) baseH - 544);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff171320), 0, r.getY(), juce::Colour (0xff100d17), 0, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (cLn); g.drawLine (0, r.getY(), (float) baseW, r.getY(), 1.0f);
    auto& m = proc.cadeia().med;

    // LEARN
    const bool ouvindo = proc.aprendendo();
    const juce::Rectangle<float> rl (16, 562, 172, 112);
    {
        const float pulso = ouvindo ? 0.5f + 0.5f * std::sin (quadro * 0.25f) : 0.0f;
        if (ouvindo) { g.setColour (cVox.withAlpha (0.12f + 0.12f * pulso)); g.fillRoundedRectangle (rl.expanded (5), 11.0f); }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2333), 0, rl.getY(), juce::Colour (0xff1c1724), 0, rl.getBottom(), false));
        g.fillRoundedRectangle (rl, 8.0f);
        g.setColour (ouvindo ? cVox : juce::Colour (0xff4a3a52)); g.drawRoundedRectangle (rl, 8.0f, ouvindo ? 1.6f : 1.0f);
        g.setColour (ouvindo ? cVox : cTx); g.setFont (fonte (24.0f, true).withExtraKerningFactor (0.3f));
        g.drawText ("LEARN", rl.withTrimmedBottom (46), juce::Justification::centredBottom);
        rotulo (g, { rl.getX(), rl.getY() + 70, rl.getWidth(), 14 },
                ouvindo ? u8 ("LISTENING · ") + juce::String (m.learnSeg.load(), 1) + " s" : u8 ("SING 15–20 s"), ouvindo ? cVox : cMu,
                juce::Justification::centred, 10.0f);
        rotulo (g, { rl.getX(), rl.getY() + 86, rl.getWidth(), 14 }, ouvindo ? juce::String ("PRESS TO STOP") : juce::String ("SETS THE INPUT"), cDim,
                juce::Justification::centred, 9.5f);
        alvos.push_back ({ rl, [this] { proc.learn(); } });
        rotulo (g, { rl.getX(), rl.getBottom() + 6, rl.getWidth(), 14 },
                proc.learnVezes() > 0 || std::abs (real ("input")) > 0.05f ? "INPUT " + sinal (real ("input"), 1, " dB") : juce::String ("NOT LEARNED YET"),
                proc.learnVezes() > 0 ? cOk : cDim, juce::Justification::centred, 10.0f);
    }

    // 8 macros
    struct M { const char* id; const char* nome; const char* desc; juce::Colour cor; };
    const M ms[8] = { { "tune", "TUNE", "off / hard", cOk }, { "tame", "TAME", "FET + OPTO", cAmb },
                      { "tone", "TONE", "dark / bright", cVox }, { "air", "AIR", "11 kHz shelf", cVox },
                      { "deess", "DE-ESS", "sibilance", cTeal }, { "warmth", "WARMTH", "tube / tape", cVox },
                      { "space", "SPACE", "plate + delay", cLil }, { "output", "OUTPUT", "after makeup", cTx } };
    for (int i = 0; i < 8; ++i)
    {
        const float cx = 246 + i * 84.0f;
        const juce::String id = ms[i].id;
        rotulo (g, { cx - 42, 562, 84, 14 }, ms[i].nome, cMu, juce::Justification::centred, 11.0f);
        knob (g, id, { cx, 610 }, 24.0f, ms[i].cor, id == "tone" || id == "output" || id == "air");
        g.setColour (cTx); g.setFont (fonte (12.5f, true));
        g.drawText (texto (id), juce::Rectangle<float> (cx - 42, 642, 84, 16), juce::Justification::centred);
        g.setColour (cDim); g.setFont (fonte (9.5f));
        g.drawText (ms[i].desc, juce::Rectangle<float> (cx - 42, 660, 84, 13), juce::Justification::centred);
    }

    // chaves
    struct T { const char* id; const char* nome; juce::Colour cor; };
    const T ts[3] = { { "natural", "KEEP IT NATURAL", cOk }, { "tracking", "TRACKING / 0 ms", cAmb }, { "link", "MAESTRO LINK", cOk } };
    for (int i = 0; i < 3; ++i)
    {
        const juce::Rectangle<float> b (886, 562 + i * 38.0f, 170, 30);
        const bool on = proc.liga (ts[i].id);
        g.setColour (juce::Colour (0xff171320)); g.fillRoundedRectangle (b, 4.0f);
        g.setColour (on ? ts[i].cor.withAlpha (0.6f) : cLn); g.drawRoundedRectangle (b, 4.0f, 1.0f);
        led (g, { b.getX() + 15, b.getCentreY() }, 4.0f, ts[i].cor, on);
        rotulo (g, b.withTrimmedLeft (28), ts[i].nome, on ? cTx : cDim, juce::Justification::centredLeft, 10.5f);
        const juce::String id = ts[i].id;
        alvos.push_back ({ b, [this, id] { proc.alterna (id); } });
    }

    // A / B / BYPASS
    for (int i = 0; i < 2; ++i)
    {
        const juce::Rectangle<float> b (1068 + i * 60.0f, 562, 54, 34);
        const bool atual = proc.abAtual() == i;
        g.setColour (atual ? juce::Colour (0xffdcd7ea) : juce::Colour (0xff171320)); g.fillRoundedRectangle (b, 3.0f);
        g.setColour (cLn); g.drawRoundedRectangle (b, 3.0f, 1.0f);
        g.setColour (atual ? cBg : cTx); g.setFont (fonte (14.0f, true));
        g.drawText (i == 0 ? "A" : "B", b, juce::Justification::centred);
        alvos.push_back ({ b, [this, i] { proc.trocaAB (i); } });
    }
    {
        const juce::Rectangle<float> b (1068, 602, 114, 22);
        g.setColour (cLn); g.drawRoundedRectangle (b, 3.0f, 1.0f);
        rotulo (g, b, proc.abAtual() == 0 ? u8 ("COPY A → B") : u8 ("COPY B → A"), cMu, juce::Justification::centred, 9.5f);
        alvos.push_back ({ b, [this] { proc.copiaAB(); } });
    }
    {
        const juce::Rectangle<float> b (1068, 632, 114, 34);
        const bool on = proc.liga ("bypass");
        g.setColour (on ? cRed.withAlpha (0.2f) : juce::Colour (0xff171320)); g.fillRoundedRectangle (b, 3.0f);
        g.setColour (on ? cRed : cLn); g.drawRoundedRectangle (b, 3.0f, 1.0f);
        rotulo (g, b, "BYPASS", on ? cRed : cTx, juce::Justification::centred, 12.0f);
        alvos.push_back ({ b, [this] { proc.alterna ("bypass"); } });
    }

    // ajuda
    juce::String t1, t2; juce::Colour c1 = cTx;
    if (proc.liga ("bypass")) { t1 = "BYPASS"; c1 = cRed; t2 = u8 ("O som passa sem nenhuma alteração (só a latência, para não pular no A/B)."); }
    else if (ouvindo) { t1 = u8 ("LEARN ouvindo…"); c1 = cVox; t2 = u8 ("Cante normalmente por 15 a 20 s (ele para sozinho). O GOGÓ mede a sua voz e acerta o INPUT."); }
    else if (proc.liga ("tracking")) { t1 = u8 ("TRACKING: latência 0 para gravar."); c1 = cAmb; t2 = u8 ("O TUNE sai do caminho; o resto da cadeia continua. Desligue para mixar com a afinação."); }
    else if (proc.learnVezes() == 0 && std::abs (real ("input")) < 0.05f)
    { t1 = u8 ("Comece pelo LEARN."); t2 = u8 ("Aperte LEARN e cante 15 a 20 s: ele acerta o nível de entrada para a cadeia trabalhar no ponto. Depois é só ouvir."); }
    else { t1 = u8 ("Clique num bloco para abrir os ajustes."); t2 = u8 ("O botão de power de cada bloco liga e desliga só aquele bloco; o resto da cadeia continua igual."); }
    g.setColour (c1); g.setFont (fonte (12.5f, true));
    g.drawText (t1, juce::Rectangle<float> (204, 690, 660, 18), juce::Justification::centredLeft);
    g.setColour (cMu); g.setFont (fonte (12.0f));
    g.drawFittedText (t2, juce::Rectangle<int> (204, 710, 860, 34), juce::Justification::topLeft, 2);
    rotulo (g, { 886, 690, 296, 14 }, m.voz.load() ? u8 ("VOICE ●") : u8 ("VOICE ○"), m.voz.load() ? cVox : cDim, juce::Justification::centredRight, 10.0f);
}

// =====================================================================================================================
void GogoEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (e.mods.isPopupMenu()) { menu(); return; }
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
    // o último registrado ganha (o power fica por cima do bloco)
    for (auto it = alvos.rbegin(); it != alvos.rend(); ++it)
        if (it->r.contains (p))
        {
            auto f = it->f;
            f();
            repaint();
            return;
        }
}

void GogoEditor::mouseDrag (const juce::MouseEvent& e)
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

void GogoEditor::mouseUp (const juce::MouseEvent&)
{
    if (arrastando.isNotEmpty())
        if (auto* prm = param (arrastando)) prm->endChangeGesture();
    arrastando.clear();
}

void GogoEditor::mouseDoubleClick (const juce::MouseEvent& e)
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

void GogoEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
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

void GogoEditor::menuEscolha (const juce::String& id, const juce::StringArray& itens)
{
    const int atual = escolha (id);
    juce::PopupMenu m;
    for (int i = 0; i < itens.size(); ++i) m.addItem (i + 1, itens[i], true, i == atual);
    juce::Component::SafePointer<GogoEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, id] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         safe->proc.define (id, (float) (r - 1));
                         safe->repaint();
                     });
}

void GogoEditor::menuStyle()
{
    const int atual = escolha ("style");
    juce::PopupMenu m;
    m.addSectionHeader (u8 ("STYLE grava o ponto de partida (AIR, MUD, HARSH, DE-ESS, WARMTH, SPACE)"));
    for (int i = 0; i < gogo::numStyles; ++i) m.addItem (i + 1, gogo::nomeStyle (i), true, i == atual);
    juce::Component::SafePointer<GogoEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         safe->proc.aplicaStyle (r - 1);
                         safe->repaint();
                     });
}

void GogoEditor::menu()
{
    juce::PopupMenu m;
    m.addItem (menuSobre, "Como funciona");
    m.addItem (menuCopiaAB, proc.abAtual() == 0 ? u8 ("Copiar A → B") : u8 ("Copiar B → A"));
    m.addItem (menuLicenca, u8 ("Licença…"));
    m.addSeparator();
   #ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    m.addItem (-1, u8 ("GOGÓ ") + juce::String (JucePlugin_VersionString), false, false);
   #endif
    juce::Component::SafePointer<GogoEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == menuCopiaAB) safe->proc.copiaAB();
                         if (r == menuLicenca) { safe->ativacao.mostrarInfo(); return; }
                         if (r == menuSobre)
                             juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, u8 ("GOGÓ"),
                                 u8 ("Ponha o GOGÓ no canal da VOZ (uma voz só).\n\n"
                                     "1. Aperte LEARN e cante 15 a 20 s: ele acerta o INPUT.\n"
                                     "2. Escolha o STYLE (ponto de partida) e o ROLE (LEAD, DOUBLE, ADLIB, SPOKEN).\n"
                                     "3. Ajuste pelas macros embaixo; cada bloco abre os ajustes com um clique e liga ou desliga no power.\n\n"
                                     "Cadeia: CLEAN, TUNE, RIDE, FET, OPTO, TONE, DE-ESS, WARMTH, SPACE.\n\n"
                                     "TUNE é o motor do ONKEY: acha o tom sozinho e corrige pelo centro da nota (vibrato e "
                                     "slides passam). Latência de ~45 ms, compensada pelo DAW. Para GRAVAR com o plugin "
                                     "ligado, use TRACKING (latência 0, sem o TUNE).\n\n"
                                     "KEEP IT NATURAL limita o que o automático pode fazer (TUNE nunca robô, FET em 4:1, "
                                     "de-ess e EQ mais leves). MAESTRO LINK lê o MAESTRO no mesmo projeto e abre espaço "
                                     "para a voz onde os synths deixam."));
                     });
}
