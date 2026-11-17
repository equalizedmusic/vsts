#include "PluginEditor.h"

using namespace retalho;

namespace
{
// paleta do esboço v2: grafite azulado; ÂMBAR = fill / ligado, ROSA = reverse e filtro, CIANO = pitch, VERDE = groove
const juce::Colour cBg { 0xff111419 }, cBgB { 0xff0c0e12 }, cPn { 0xff171b21 }, cPn2 { 0xff20252d }, cLn { 0xff2b313b },
                   cTx { 0xffe9edf2 }, cMu { 0xff8a93a0 }, cDim { 0xff59616d }, cAmb { 0xfff6c445 }, cRosa { 0xffff6fa3 },
                   cCiano { 0xff3cc8d8 }, cVerde { 0xff43d17a }, cLil { 0xffb9a2ff }, cTela { 0xff07080a }, cOnda { 0xff6b7280 };

juce::Colour corTipo (int t)
{
    static const juce::Colour c[numTipos] = { juce::Colour (0xfff6c445), juce::Colour (0xff3cc8d8), juce::Colour (0xffff6fa3), juce::Colour (0xffb9a2ff),
                                              juce::Colour (0xff43d17a), juce::Colour (0xff9aa5b1), juce::Colour (0xff5b6470), juce::Colour (0xff353a42) };
    return c[lim (t, 0, numTipos - 1)];
}
juce::Colour corChop (int s)
{
    static const juce::Colour c[numChops] = { juce::Colour (0xfff6c445), juce::Colour (0xff3cc8d8), juce::Colour (0xffff6fa3), juce::Colour (0xff43d17a),
                                              juce::Colour (0xffb9a2ff), juce::Colour (0xffff9a4d), juce::Colour (0xff4fd1c5), juce::Colour (0xfff28cb1) };
    return c[lim (s, 0, numChops - 1)];
}

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
    return (v > 0 ? "+" : u8 ("\xe2\x88\x92")) + num + u8 (un);
}
juce::String hz (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 0 : 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; }

void rotulo (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour cor = cMu,
             juce::Justification j = juce::Justification::centredLeft, float h = 10.0f, bool negrito = false)
{
    g.setColour (cor);
    g.setFont (fonte (h, negrito).withExtraKerningFactor (0.22f));
    g.drawText (t, r, j, false);
}

void caixa (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fundo = cPn, float raio = 6.0f)
{
    // leve volume: topo um pouco mais claro e um fio de luz na borda de cima
    g.setGradientFill (juce::ColourGradient (fundo.brighter (0.05f), 0, r.getY(), fundo.darker (0.12f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, raio);
    g.setColour (cLn);
    g.drawRoundedRectangle (r, raio, 1.0f);
    if (r.getHeight() > 30.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.drawLine (r.getX() + raio, r.getY() + 1.0f, r.getRight() - raio, r.getY() + 1.0f, 1.0f);
    }
}

// fundo dos displays de onda: vidro escuro com vinheta e borda
void tela (juce::Graphics& g, juce::Rectangle<float> r, float raio = 8.0f)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e1116), 0, r.getY(), juce::Colour (0xff060709), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, raio);
    juce::ColourGradient vin (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                              juce::Colours::black.withAlpha (0.35f), r.getX(), r.getCentreY(), true);
    g.setGradientFill (vin);
    g.fillRoundedRectangle (r, raio);
    g.setColour (juce::Colour (0xff252a32));
    g.drawRoundedRectangle (r, raio, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (1.0f), raio - 1.0f, 1.0f);
}

// grade por tempo tracejada (compasso mais forte), no estilo dos displays de stutter
void gradeTempo (juce::Graphics& g, juce::Rectangle<float> in, float y0, float y1, double beats, int bpb)
{
    const float dash[2] = { 2.0f, 4.0f };
    for (int b = 1; b < (int) std::ceil (beats - 1e-6); ++b)
    {
        const float x = in.getX() + in.getWidth() * (float) (b / beats);
        const bool compasso = b % juce::jmax (1, bpb) == 0;
        g.setColour (compasso ? juce::Colour (0xff3a414c) : juce::Colour (0xff22272f));
        g.drawDashedLine ({ x, y0, x, y1 }, dash, 2, 1.0f);
    }
}

// Onda REAL: barras espelhadas com degradê (claras nas pontas, escuras no meio) e brilho em volta.
// Cada cor é um Path só (um preenchimento por cor, leve a 30 Hz).
struct Pincel
{
    juce::Colour cor[12];
    juce::Path p[12];
    int n = 0;
    juce::Path& de (juce::Colour c)
    {
        for (int i = 0; i < n; ++i) if (cor[i] == c) return p[i];
        if (n == 12) return p[11];
        cor[n] = c;
        return p[n++];
    }
    void barra (juce::Colour c, float x, float w, float cy, float meia)
    {
        meia = juce::jmax (0.6f, meia);
        de (c).addRectangle (x, cy - meia, w, 2.0f * meia);
    }
    void pinta (juce::Graphics& g, float cy, float meia, bool brilho = true) const
    {
        for (int i = 0; i < n; ++i)
        {
            if (p[i].isEmpty()) continue;
            const auto c = cor[i];
            if (brilho)
            {
                // brilho barato: o mesmo Path esticado em volta do centro e deslocado 1,5 px para os lados, bem
                // transparente (preencher é muito mais leve que contornar centenas de barras a 30 Hz)
                const auto estica = juce::AffineTransform::translation (0, -cy).scaled (1.0f, 1.12f).translated (0, cy);
                g.setColour (c.withAlpha (0.09f));
                g.fillPath (p[i], estica.translated (-1.5f, 0));
                g.fillPath (p[i], estica.translated (1.5f, 0));
            }
            juce::ColourGradient gr (c.brighter (0.35f), 0, cy - meia, c.brighter (0.35f), 0, cy + meia, false);
            gr.addColour (0.30, c);
            gr.addColour (0.50, c.darker (0.55f));
            gr.addColour (0.70, c);
            g.setGradientFill (gr);
            g.fillPath (p[i]);
        }
    }
};

// pico (0..1) das colunas [c0, c1] de um par min/max
float picoCols (const std::vector<float>& mn, const std::vector<float>& mx, int c0, int c1)
{
    const int C = (int) juce::jmin (mn.size(), mx.size());
    if (C == 0) return 0.0f;
    c0 = juce::jlimit (0, C - 1, c0); c1 = juce::jlimit (c0, C - 1, c1);
    float p = 0.0f;
    for (int c = c0; c <= c1; ++c) p = juce::jmax (p, std::abs (mn[(size_t) c]), std::abs (mx[(size_t) c]));
    return p;
}

// ganho para a onda encher o display (o maior pico chega a ~92%)
float ganhoOnda (const std::vector<float>& mn, const std::vector<float>& mx)
{
    const float p = picoCols (mn, mx, 0, (int) mx.size() - 1);
    return 0.92f / juce::jmax (0.06f, p);
}

// cursor com brilho (playhead)
void agulha (juce::Graphics& g, float x, float y0, float y1, juce::Colour cor)
{
    g.setColour (cor.withAlpha (0.06f)); g.fillRect (x - 8.0f, y0, 16.0f, y1 - y0);
    g.setColour (cor.withAlpha (0.14f)); g.fillRect (x - 3.0f, y0, 6.0f, y1 - y0);
    g.setColour (cor); g.fillRect (x - 0.75f, y0, 1.5f, y1 - y0);
    juce::Path t; t.addTriangle (x - 5.0f, y0, x + 5.0f, y0, x, y0 + 6.0f);
    g.fillPath (t);
}

// triângulos (os glifos ◀ ▶ ▼ não existem em todas as fontes)
void tri (juce::Graphics& g, juce::Point<float> c, float s, int dir, juce::Colour cor)
{
    juce::Path p;
    if (dir == 0) p.addTriangle (c.x - s * 0.5f, c.y - s * 0.6f, c.x - s * 0.5f, c.y + s * 0.6f, c.x + s * 0.6f, c.y);        // ▶
    else if (dir == 1) p.addTriangle (c.x + s * 0.5f, c.y - s * 0.6f, c.x + s * 0.5f, c.y + s * 0.6f, c.x - s * 0.6f, c.y);   // ◀
    else p.addTriangle (c.x - s * 0.6f, c.y - s * 0.4f, c.x + s * 0.6f, c.y - s * 0.4f, c.x, c.y + s * 0.5f);                 // ▼
    g.setColour (cor);
    g.fillPath (p);
}

void cadeado (juce::Graphics& g, juce::Point<float> c, float s, bool fechado, juce::Colour cor)
{
    g.setColour (cor);
    g.fillRoundedRectangle (c.x - s * 0.5f, c.y - s * 0.1f, s, s * 0.7f, 1.5f);
    juce::Path a;
    a.addCentredArc (c.x, c.y - s * 0.1f, s * 0.3f, s * 0.38f, 0.0f, -juce::MathConstants<float>::halfPi, fechado ? juce::MathConstants<float>::halfPi : 0.0f, true);
    g.strokePath (a, juce::PathStrokeType (1.4f));
}

// knob: v e zero em 0..1 (zero = de onde o arco sai; bipolar no meio)
void desenhaKnob (juce::Graphics& g, juce::Point<float> c, float raio, float v, float zero, juce::Colour cor, bool ativo = true)
{
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float av = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, v), am = a0 + (a1 - a0) * zero;
    if (! ativo) cor = cDim;
    const auto corpo = juce::Rectangle<float> (raio * 2, raio * 2).withCentre (c);
    // sombra, corpo com luz de cima-esquerda, aro e reflexo: knob "de metal escuro"
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (corpo.translated (0, raio * 0.12f).expanded (raio * 0.08f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff353b45), c.x - raio * 0.5f, c.y - raio * 0.7f,
                                             juce::Colour (0xff0f1216), c.x + raio * 0.5f, c.y + raio * 0.9f, false));
    g.fillEllipse (corpo);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a3038), c.x, c.y - raio * 0.6f, juce::Colour (0xff15181d), c.x, c.y + raio * 0.6f, false));
    g.fillEllipse (corpo.reduced (raio * 0.16f));
    g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawEllipse (corpo.reduced (0.5f), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (corpo.reduced (raio * 0.16f), 1.0f);
    const float ra = raio + (raio > 20.0f ? 6.0f : 4.0f), esp = raio > 20.0f ? 4.0f : 3.0f;
    juce::Path tr; tr.addCentredArc (c.x, c.y, ra, ra, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff1c2027)); g.strokePath (tr, juce::PathStrokeType (esp, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (std::abs (av - am) > 0.001f)
    {
        juce::Path va; va.addCentredArc (c.x, c.y, ra, ra, 0, juce::jmin (am, av), juce::jmax (am, av), true);
        if (ativo)
        {
            g.setColour (cor.withAlpha (0.10f)); g.strokePath (va, juce::PathStrokeType (esp + 7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (cor.withAlpha (0.22f)); g.strokePath (va, juce::PathStrokeType (esp + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (cor); g.strokePath (va, juce::PathStrokeType (esp, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // ponteiro: ponto aceso perto da borda do corpo
    const juce::Point<float> d (std::sin (av), -std::cos (av));
    const juce::Point<float> pp (c.x + d.x * raio * 0.62f, c.y + d.y * raio * 0.62f);
    const float pr = raio > 20.0f ? 3.4f : 2.4f;
    if (ativo) { g.setColour (cor.withAlpha (0.3f)); g.fillEllipse (juce::Rectangle<float> (pr * 4, pr * 4).withCentre (pp)); }
    g.setColour (ativo ? cor.brighter (0.3f) : cDim); g.fillEllipse (juce::Rectangle<float> (pr * 2, pr * 2).withCentre (pp));
}

juce::String valorPasso (uint32_t p)
{
    const int t = pTipo (p), v = pValor (p);
    switch (t)
    {
        case tRepeat:  return u8 ("\xc3\x97") + juce::String (reps (v));
        case tPitch:   return sinal ((float) stPasso (v), 0, " st");
        case tTape:    return v < 128 ? "STOP" : "START";
        case tSpeed:   return v < 128 ? u8 ("\xc2\xbd\xc3\x97") : u8 ("2\xc3\x97");
        case tGate:    return juce::String (juce::roundToInt (10.0f + 90.0f * v / 255.0f)) + "%";
        default:       return {};
    }
}
} // namespace

RetalhoEditor::RetalhoEditor (RetalhoProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (900, 465, 2400, 1240);
    }
    const int w0 = proc.uiW.load();
    const int w = w0 > 0 ? juce::jlimit (900, 2400, w0) : 1200;
    setSize (w, juce::roundToInt (w * (double) baseH / baseW));
    startTimerHz (30);
}

void RetalhoEditor::resized() { proc.uiW = getWidth(); }

void RetalhoEditor::timerCallback()
{
    ++quadro;
    if (proc.versaoAmostra() != visaoA.versao) visaoA = proc.visaoAmostra();
    if (proc.vista().versao != versaoVista)
    {
        vista = proc.vista();
        versaoVista = vista.versao;
    }
    repaint();
}

juce::String RetalhoEditor::texto (int k) const
{
    const float v = real (k);
    switch (k)
    {
        case kDecay:   return sinal (v, 1, " dB");
        case kFadeIn: case kFadeOut: return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
        case kPIni: case kPFim: case kGPitch: return sinal (v, 0, " st");
        case kPCurva: case kCurva: return v > 10.0f ? "EXP " + juce::String (juce::roundToInt (v)) : (v < -10.0f ? "LOG " + juce::String (juce::roundToInt (-v)) : juce::String ("LIN"));
        case kFDe: case kFAte: case kChopHpf: case kChopLpf: return hz (v);
        case kWidth:   return v < 1.0f ? juce::String ("MONO") : (std::abs (v - 50.0f) < 0.5f ? juce::String ("STEREO") : (v > 99.0f ? juce::String ("PING-PONG")
                                : (v < 50.0f ? juce::String (juce::roundToInt (v * 2.0f)) + "%" : "PING " + juce::String (juce::roundToInt ((v - 50.0f) * 2.0f)) + "%")));
        case kDuck: case kChopVol: case kOutput: return sinal (v, 1, " dB");
        case kSeed:    return "#" + juce::String (juce::roundToInt (v));
        default: break;
    }
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.param (k))) return c->getCurrentChoiceName();
    if (dynamic_cast<juce::AudioParameterBool*> (proc.param (k)) != nullptr) return v > 0.5f ? "ON" : "OFF";
    return juce::String (juce::roundToInt (v)) + "%";
}

juce::String RetalhoEditor::nomeTom() const
{
    const int r = escolha (kRoot), s = escolha (kEscala);
    auto& m = proc.motor();
    const int det = m.tomDet.load();
    const int tom = r > 0 ? r - 1 : det;
    const int esc = s > 0 ? s - 1 : (det >= 0 ? (m.menorDet.load() ? 1 : 0) : -1);
    if (esc == 5) return "CHROMATIC";
    if (tom < 0 || esc < 0) return u8 ("\xe2\x80\x94");
    static const char* curto[] = { "MAJ", "MIN", "DOR", "PHR", "HARM" };
    return juce::String (ch::pc[tom]) + " " + curto[lim (esc, 0, 4)];
}

// =====================================================================================================================
void RetalhoEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (escala()));
    g.setGradientFill (juce::ColourGradient (cBg, 0, 0, cBgB, 0, (float) baseH, false));
    g.fillRect (juce::Rectangle<float> (0, 0, (float) baseW, (float) baseH));
    knobs.clear();
    alvos.clear();
    // áreas de mouse da outra tela não podem sobrar
    areaResult = areaOriginal = areaDaw = areaCelulas = areaAmostra = {};
    for (auto& a : areaPads) a = {};

    if (simples()) telaSimples (g);
    else
    {
        topo (g);
        linha (g);
        original (g, { 12, 84, 1176, 70 });
        result (g, { 12, 160, 1176, 196 });
        paineis (g);
        rodape (g);
    }
    if (soltando)
    {
        const juce::Rectangle<float> r (0, 0, (float) baseW, (float) baseH);
        g.setColour (juce::Colours::black.withAlpha (0.72f)); g.fillRect (r);
        g.setColour (cAmb); g.drawRoundedRectangle (r.reduced (16), 14.0f, 2.5f);
        rotulo (g, r.withTrimmedBottom (40), u8 ("SOLTE O VOCAL AQUI"), cAmb, juce::Justification::centred, 30.0f, true);
        rotulo (g, r.withTrimmedTop (60), u8 ("ELE VIRA A AMOSTRA DO RETALHO \xc2\xb7 WAV, AIFF, FLAC, MP3, OGG \xc2\xb7 AT\xc3\x89 60 s"),
                cTx, juce::Justification::centred, 12.0f);
    }
}

// ---- peças ----------------------------------------------------------------------------------------------------------
void RetalhoEditor::botao (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, juce::Colour cor,
                           std::function<void()> f, float fonteH)
{
    if (on)
    {
        g.setColour (cor.withAlpha (0.14f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (cor); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.3f);
    }
    else
    {
        g.setColour (cPn2); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (cLn); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }
    rotulo (g, r, t, on ? cor : cMu, juce::Justification::centred, fonteH, on);
    if (f) alvos.push_back ({ r, std::move (f) });
}

void RetalhoEditor::liga (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, int campo, juce::Colour cor)
{
    botao (g, r, t, real (campo) > 0.5f, cor, [this, campo] { proc.define (campo, real (campo) > 0.5f ? 0.0f : 1.0f); });
}

float RetalhoEditor::grupo (juce::Graphics& g, float x, float y, const juce::String& titulo, int campo, const juce::StringArray& nomes)
{
    const auto ft = fonte (9.0f).withExtraKerningFactor (0.22f);
    rotulo (g, { x, y, largura (ft, titulo) + 4, 20 }, titulo, cMu, juce::Justification::centredLeft, 9.0f);
    x += largura (ft, titulo) + 7;
    const auto fb = fonte (9.5f).withExtraKerningFactor (0.22f);
    const int atual = escolha (campo);
    for (int i = 0; i < nomes.size(); ++i)
    {
        const float w = largura (fb, nomes[i]) + 11;
        botao (g, { x, y, w, 20 }, nomes[i], i == atual, cAmb, [this, campo, i] { proc.define (campo, (float) i); }, 9.5f);
        x += w + 2;
    }
    return x + 9;
}

void RetalhoEditor::knob (juce::Graphics& g, int campo, const juce::String& nome, juce::Point<float> c, juce::Colour cor)
{
    auto* p = proc.param (campo);
    if (p == nullptr) return;
    const float raio = 14.0f;
    knobs.push_back ({ campo, c, raio });
    rotulo (g, { c.x - 40, c.y - 31, 80, 12 }, nome, cMu, juce::Justification::centred, 9.5f);
    const bool bip = info (campo).min < 0.0f && info (campo).max > 0.0f;
    desenhaKnob (g, c, raio, p->getValue(), bip ? p->convertTo0to1 (0.0f) : 0.0f, cor);
    rotulo (g, { c.x - 31, c.y + 19, 62, 13 }, texto (campo), cTx, juce::Justification::centred, 10.0f, true);
}

void RetalhoEditor::painel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& titulo, const juce::String& sub)
{
    caixa (g, r, cPn, 7.0f);
    rotulo (g, { r.getX() + 12, r.getY() + 6, r.getWidth() * 0.5f, 14 }, titulo, cTx.withAlpha (0.85f), juce::Justification::centredLeft, 10.5f);
    if (sub.isNotEmpty()) rotulo (g, { r.getCentreX(), r.getY() + 6, r.getWidth() * 0.5f - 12, 14 }, sub, cDim, juce::Justification::centredRight, 9.0f);
}

float RetalhoEditor::chip (juce::Graphics& g, float xDir, const juce::String& rot, const juce::String& v0, juce::Colour cor, std::function<void()> f,
                           float maxW)
{
    const auto fr = fonte (8.5f).withExtraKerningFactor (0.22f), fv = fonte (9.5f, true).withExtraKerningFactor (0.22f);
    const float extra = largura (fr, rot) + (rot.isNotEmpty() ? 20 : 14);
    juce::String val = v0;
    while (val.length() > 3 && extra + largura (fv, val) > maxW)   // não cabe: corta o fim com reticências
        val = val.trimEnd().dropLastCharacters (val.endsWith (u8 ("\xe2\x80\xa6")) ? 2 : 1).trimEnd() + u8 ("\xe2\x80\xa6");
    const float w = extra + largura (fv, val);
    const juce::Rectangle<float> r (xDir - w, 11, w, 26);
    caixa (g, r, cPn, 5.0f);
    float x = r.getX() + 7;
    if (rot.isNotEmpty()) { rotulo (g, { x, r.getY(), largura (fr, rot) + 2, r.getHeight() }, rot, cMu, juce::Justification::centredLeft, 8.5f); x += largura (fr, rot) + 6; }
    rotulo (g, { x, r.getY(), largura (fv, val) + 4, r.getHeight() }, val, cor, juce::Justification::centredLeft, 9.5f, true);
    if (f) alvos.push_back ({ r, std::move (f) });
    return r.getX() - 6;
}

// ---- topo -----------------------------------------------------------------------------------------------------------
void RetalhoEditor::topo (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff15191f)); g.fillRect (0.0f, 0.0f, (float) baseW, 48.0f);
    g.setColour (cLn); g.drawLine (0, 48, (float) baseW, 48, 1.0f);

    // RETALHO com o L em âmbar
    const auto f = fonte (25.0f, true).withExtraKerningFactor (0.34f);
    float x = 18;
    const juce::String partes[3] = { "RETA", "L", "HO" };
    g.setFont (f);
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (i == 1 ? cAmb : cTx);
        g.drawText (partes[i], juce::Rectangle<float> (x, 8, 120, 32), juce::Justification::centredLeft);
        x += largura (f, partes[i]);
    }
    juce::ignoreUnused (x);
    chaveModo (g);

    // abas
    float tx = 300;
    const int modo = escolha (kModo);
    for (int i = 0; i < 3; ++i)
    {
        const auto ft = fonte (11.0f, true).withExtraKerningFactor (0.22f);
        const float w = largura (ft, ch::modo[i]) + 24;
        const juce::Rectangle<float> r (tx, 9, w, 30);
        if (i == modo) { g.setColour (cAmb); g.fillRoundedRectangle (r, 5.0f); }
        else { g.setColour (cPn2); g.fillRoundedRectangle (r, 5.0f); g.setColour (cLn); g.drawRoundedRectangle (r, 5.0f, 1.0f); }
        rotulo (g, r, ch::modo[i], i == modo ? juce::Colour (0xff141414) : cMu, juce::Justification::centred, 11.0f, true);
        alvos.push_back ({ r, [this, i] { proc.define (kModo, (float) i); } });
        tx += w + 5;
    }

    // chips da direita
    auto& m = proc.motor();
    float xd = 1188;
    {
        const juce::Rectangle<float> r (xd - 28, 11, 28, 26);
        caixa (g, r, cPn, 5.0f);
        g.setColour (cMu);
        for (int i = 0; i < 3; ++i) g.fillRect (r.getX() + 8, r.getY() + 8 + i * 4.5f, 12.0f, 1.6f);
        alvos.push_back ({ r, [this] { menu(); } });
        xd = r.getX() - 6;
    }
    const bool cap = vista.valida;
    {
        const float x0 = xd;
        xd = chip (g, xd, "", juce::String ("   CAPTURE"), cap ? cTx : cDim, nullptr);
        const float pulso = cap ? 0.75f + 0.25f * std::sin (quadro * 0.15f) : 0.25f;
        juce::ignoreUnused (x0);
        g.setColour ((cap ? cVerde : cDim).withAlpha (pulso));
        g.fillEllipse (xd + 6 + 8, 21, 6, 6);
    }
    const bool host = m.tocandoA.load();
    xd = chip (g, xd, "TEMPO", (host ? "HOST " : "FREE ") + juce::String (juce::roundToInt (m.bpmA.load())), cTx, nullptr);
    const juce::String keyTxt = (escolha (kRoot) == 0 ? juce::String ("AUTO ") + u8 ("\xc2\xb7 ") : juce::String()) + nomeTom();
    xd = chip (g, xd, "KEY", keyTxt, cVerde, [this]
    {
        juce::PopupMenu mn, mr, me;
        for (int i = 0; i < info (kRoot).nEsc; ++i) mr.addItem (100 + i, info (kRoot).esc[i], true, escolha (kRoot) == i);
        for (int i = 0; i < info (kEscala).nEsc; ++i) me.addItem (200 + i, info (kEscala).esc[i], true, escolha (kEscala) == i);
        mn.addSectionHeader (u8 ("KEY: os degraus de PITCH caem na escala (AUTO = o tom que a voz canta)"));
        mn.addSubMenu ("ROOT", mr);
        mn.addSubMenu ("SCALE", me);
        juce::Component::SafePointer<RetalhoEditor> safe (this);
        mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r >= 200) safe->proc.define (kEscala, (float) (r - 200));
            else if (r >= 100) safe->proc.define (kRoot, (float) (r - 100));
        });
    });
    // o PRESET fica com o espaço que sobra até as abas (nome longo é cortado, nunca cobre a aba)
    xd = chip (g, xd, "PRESET", proc.nomePreset(), cAmb, [this] { menuPresets(); }, xd - (tx + 6));
    juce::ignoreUnused (xd);
}

// ---- linha de botões da aba -------------------------------------------------------------------------------------------
void RetalhoEditor::linha (juce::Graphics& g)
{
    const float y = 57;
    const int modo = escolha (kModo);
    auto lista = [] (int k) { juce::StringArray s; for (int i = 0; i < info (k).nEsc; ++i) s.add (u8 (info (k).esc[i])); return s; };
    float x = 14;
    if (modo == mEdit)
    {
        x = grupo (g, x, y, "SHAPE", kShape, lista (kShape));
        x = grupo (g, x, y, "LENGTH", kLength, lista (kLength));
        const float xr = grupo (g, x, y, "RATE", kRate, lista (kRate)) - 12;
        const int md = escolha (kRateMod);
        botao (g, { xr, y, 22, 20 }, "T", md == 1, cAmb, [this, md] { proc.define (kRateMod, md == 1 ? 0.0f : 1.0f); });
        botao (g, { xr + 25, y, 22, 20 }, u8 ("\xc2\xb7"), md == 2, cAmb, [this, md] { proc.define (kRateMod, md == 2 ? 0.0f : 2.0f); }, 14.0f);
        x = xr + 25 + 22 + 15;
        const int sh = escolha (kShape);
        if (sh == shRamp || sh == shCurve)
        {
            x = grupo (g, x, y, "RAMP TO", kRampTo, lista (kRampTo));
            if (sh == shCurve)
            {
                const float cv = real (kCurva);
                const char* n[3] = { "LOG", "LIN", "EXP" };
                const float alvo[3] = { -60.0f, 0.0f, 60.0f };
                for (int i = 0; i < 3; ++i)
                {
                    const bool on = (i == 0 && cv < -10) || (i == 1 && std::abs (cv) <= 10) || (i == 2 && cv > 10);
                    botao (g, { x - 4 + i * 40.0f, y, 37, 20 }, n[i], on, cCiano, [this, a = alvo[i]] { proc.define (kCurva, a); });
                }
            }
        }
        else if (sh == shTremolo) grupo (g, x, y, "FORM", kTrem, lista (kTrem));
        else if (sh == shFree) rotulo (g, { x, y, 300, 20 }, u8 ("DESENHE O TAMANHO DAS FATIAS NA ONDA \xe2\x86\x93"), cCiano, juce::Justification::centredLeft, 10.0f);
        else rotulo (g, { x, y, 330, 20 }, u8 ("CLIQUE NAS FATIAS DO RESULT PARA LIGAR / DESLIGAR"), cDim, juce::Justification::centredLeft, 9.5f);
    }
    else if (modo == mComplex)
    {
        x = grupo (g, x, y, "STEPS", kPassos, lista (kPassos));
        x = grupo (g, x, y, "LENGTH", kLength, lista (kLength));
        x = grupo (g, x, y, "READ", kRead, lista (kRead));
        x = grupo (g, x, y, "DICE MODE", kDMode, lista (kDMode));
        const int rd = escolha (kRead);
        const char* expl[3] = { "FOLLOW: CADA PASSO L\xc3\x8a O SEU PONTO", "HOLD: TODOS LEEM DO 1\xc2\xba PASSO", "HITS: CADA PASSO PULA PARA O ATAQUE" };
        rotulo (g, { x, y, 1188 - x, 20 }, u8 (expl[lim (rd, 0, 2)]), cDim, juce::Justification::centredLeft, 9.5f);
    }
    else
    {
        x = grupo (g, x, y, "CHOPS", kChops, lista (kChops));
        x = grupo (g, x, y, "CUT", kCut, lista (kCut));
        x = grupo (g, x, y, "STYLE", kEstilo, lista (kEstilo));
        x = grupo (g, x, y, "BARS", kGBars, lista (kGBars));
        grupo (g, x, y, "GRID", kGGrid, lista (kGGrid));
    }
}

// ---- ORIGINAL ---------------------------------------------------------------------------------------------------------
void RetalhoEditor::original (juce::Graphics& g, juce::Rectangle<float> r)
{
    areaOriginal = r;
    tela (g, r, 6.0f);
    const int modo = escolha (kModo);
    const auto in = r.reduced (8, 6);
    const float W = in.getWidth();
    const auto& v = vista;
    const double beats = v.len > 0 ? (double) v.len / v.spb : 4.0;

    // grade: linha por tempo (tracejada)
    gradeTempo (g, in, r.getY() + 2, r.getBottom() - 2, beats, (int) v.bpb);
    const float cy = in.getCentreY() + 6, hh = in.getHeight() * 0.40f;
    if (v.valida)
    {
        const bool edit = modo == mEdit;
        const int C = RetalhoProcessor::colunas;
        const float ganho = ganhoOnda (v.oMin, v.oMax);
        Pincel pc;
        for (float x = 0; x < W - 1.0f; x += 2.0f)
        {
            const float fr = x / W;
            const float a = juce::jmin (1.0f, picoCols (v.oMin, v.oMax, (int) (fr * C), (int) ((x + 2.0f) / W * C)) * ganho);
            juce::Colour cor = cOnda;
            if (edit && fr >= v.chunkIni && fr < v.chunkFim) cor = juce::Colour (0xffdfe3e8);
            if (modo == mGroove)
                for (int s = 0; s < numChops; ++s)
                    if (v.chopIni[s] >= 0 && fr >= v.chopIni[s] && fr < v.chopFim[s]) cor = corChop (s);
            pc.barra (cor, in.getX() + x, 1.3f, cy, a * hh);
        }
        pc.pinta (g, cy, hh, false);
        if (edit && v.chunkFim > v.chunkIni)
        {
            const float x0 = in.getX() + W * v.chunkIni, x1 = in.getX() + W * juce::jmin (1.0f, v.chunkFim);
            g.setColour (cAmb.withAlpha (0.08f)); g.fillRect (x0, r.getY() + 18, x1 - x0, r.getHeight() - 20);
            g.setColour (cAmb); g.drawRect (juce::Rectangle<float> (x0, r.getY() + 18, x1 - x0, r.getHeight() - 20), 1.2f);
        }
        for (float a : v.ataques) tri (g, { in.getX() + W * a, r.getY() + 21 }, 8, 2, cAmb);
    }
    else
        rotulo (g, in.withTrimmedTop (14), u8 ("ARRASTE O VOCAL PARA A JANELA (OU MENU \xe2\x86\x92 CAPTURAR) E D\xc3\x8a PLAY NO PROJETO"), cDim,
                juce::Justification::centred, 10.0f);

    // título
    juce::String tit = proc.temAmostra() ? u8 ("AMOSTRA \xc2\xb7 ") + visaoA.nome.toUpperCase().substring (0, 28) : u8 ("ENTRADA DA FAIXA (SEM AMOSTRA)");
    juce::String det;
    if (modo == mEdit && v.valida && v.plano.n > 0)
        det = juce::String (ch::chunk[lim (escolha (kChunk), 0, 8)]) + u8 (" \xc2\xb7 CHUNK ") + juce::String (juce::roundToInt (1000.0 * (v.chunkFim - v.chunkIni) * v.len / v.fs)) + " ms";
    else if (modo == mGroove)
    {
        int n = 0; for (int s = 0; s < numChops; ++s) if (v.chopMs[s] > 0) ++n;
        det = "CHOPS " + texto (kChops) + u8 (" \xc2\xb7 ") + juce::String (n) + u8 (" S\xc3\x8dLABAS GUARDADAS");
    }
    const auto ft = fonte (10.0f).withExtraKerningFactor (0.3f);
    rotulo (g, { r.getX() + 10, r.getY() + 3, largura (ft, tit) + 6, 14 }, tit, cMu, juce::Justification::centredLeft, 10.0f);
    rotulo (g, { r.getX() + 18 + largura (ft, tit), r.getY() + 3, 400, 14 }, det, cAmb, juce::Justification::centredLeft, 10.0f, true);

    if (modo == mEdit)
    {
        const float y = r.getY() + 4;
        float x = r.getRight() - 8;
        const int ck = escolha (kChunk);
        const juce::Rectangle<float> rA (x - 46, y, 46, 18);
        botao (g, rA, "AUTO", ck == 0, cAmb, [this] { proc.define (kChunk, 0.0f); }, 9.5f);
        x = rA.getX() - 4;
        const juce::Rectangle<float> rN (x - 58, y, 58, 18);
        botao (g, rN, "NEXT   ", false, cAmb, [this, ck] { proc.define (kChunk, (float) juce::jmax (0, ck - 1)); }, 9.5f);
        tri (g, { rN.getRight() - 11, rN.getCentreY() }, 7, 0, cMu);
        x = rN.getX() - 4;
        const juce::Rectangle<float> rP (x - 58, y, 58, 18);
        botao (g, rP, "   PREV", false, cAmb, [this, ck] { proc.define (kChunk, (float) juce::jmin (8, ck + 1)); }, 9.5f);
        tri (g, { rP.getX() + 11, rP.getCentreY() }, 7, 1, cMu);
    }
}

// ---- RESULT -----------------------------------------------------------------------------------------------------------
void RetalhoEditor::result (juce::Graphics& g, juce::Rectangle<float> r)
{
    areaResult = r;
    tela (g, r, 6.0f);
    const int modo = escolha (kModo);
    const auto in = r.reduced (10, 0).withTrimmedTop (26).withTrimmedBottom (34);
    if (modo == mEdit) resultEdit (g, in);
    else if (modo == mComplex) resultComplex (g, in);
    else resultGroove (g, in);

    // transporte do preview (EDIT / COMPLEX)
    if (modo != mGroove)
    {
        const float y = r.getBottom() - 28;
        const int pt = proc.previewTocando();
        const juce::Rectangle<float> r1 (r.getX() + 10, y, 96, 22), r2 (r1.getRight() + 5, y, 88, 22), r3 (r2.getRight() + 5, y, 62, 22), r4 (r3.getRight() + 5, y, 62, 22);
        botao (g, r1, "     ORIGINAL", pt == 1, cAmb, [this] { proc.preview (1); }, 10.0f);
        tri (g, { r1.getX() + 14, r1.getCentreY() }, 8, 0, pt == 1 ? cAmb : cMu);
        botao (g, r2, "     RESULT", pt == 2, cAmb, [this] { proc.preview (2); }, 10.0f);
        tri (g, { r2.getX() + 14, r2.getCentreY() }, 8, 0, pt == 2 ? cAmb : cMu);
        botao (g, r3, "LOOP", proc.previewLoop.load(), cAmb, [this] { proc.previewLoop = ! proc.previewLoop.load(); }, 10.0f);
        botao (g, r4, "ON BAR", proc.previewOnBar.load(), cAmb, [this] { proc.previewOnBar = ! proc.previewOnBar.load(); }, 10.0f);
        const juce::String dica = modo == mEdit
            ? (escolha (kShape) == shGrid ? u8 ("CLIQUE NA FATIA = LIGA \xc2\xb7 ARRASTA = VOLUME \xc2\xb7 DIREITO = REVERSE")
                                          : (escolha (kShape) == shFree ? u8 ("ARRASTE NA ONDA PARA DESENHAR O TAMANHO DAS FATIAS") : juce::String()))
            : u8 ("TOPO = TIPO \xc2\xb7 ARRASTA = VALOR \xc2\xb7 SHIFT+CLIQUE = PINTA \xc2\xb7 RODAP\xc3\x89 / DIREITO = CADEADO");
        rotulo (g, { r4.getRight() + 10, y, r.getRight() - r4.getRight() - 20, 22 }, dica, cDim, juce::Justification::centredRight, 9.0f);
    }

    // playhead (fill tocando no DAW ou preview)
    auto& m = proc.motor();
    float pr = -1.0f;
    if (m.fillProg.load() >= 0.0f && m.fillModo.load() == modo && modo != mGroove) pr = std::fmod (m.fillProg.load(), 1.0f);
    else if (proc.previewTocando() == 2) pr = proc.previewPos();
    if (pr >= 0.0f && modo != mGroove) agulha (g, in.getX() + in.getWidth() * pr, r.getY() + 24, r.getBottom() - 32, cAmb);
}

void RetalhoEditor::resultEdit (juce::Graphics& g, juce::Rectangle<float> in)
{
    const auto& v = vista;
    const float W = in.getWidth();
    const double beats = v.len > 0 ? (double) v.len / v.spb : 4.0;
    const int sh = escolha (kShape);
    for (int b = 1; b < (int) std::ceil (beats - 1e-6); ++b)
    {
        const float x = in.getX() + W * (float) (b / beats);
        const float dash[2] = { 2.0f, 4.0f };
        g.setColour (juce::Colour (0xff2a3038)); g.drawDashedLine ({ x, in.getY() - 2, x, in.getBottom() + 2 }, dash, 2, 1.0f);
        rotulo (g, { x + 4, in.getY() - 4, 60, 12 }, "BEAT " + juce::String (b + 1), cDim, juce::Justification::centredLeft, 8.5f);
    }

    // título
    juce::String det = juce::String (ch::length[lim (escolha (kLength), 0, 2)]) + u8 (" \xc2\xb7 ");
    const juce::String rt = juce::String (ch::rate[lim (escolha (kRate), 0, 4)]) + (escolha (kRateMod) == 1 ? "T" : (escolha (kRateMod) == 2 ? u8 ("\xc2\xb7") : ""));
    if (sh == shRamp || sh == shCurve) det += rt + u8 (" \xe2\x86\x92 ") + ch::rate[lim (escolha (kRampTo), 0, 4)];
    else if (sh == shTremolo) det += "TREMOLO " + rt + " " + ch::trem[lim (escolha (kTrem), 0, 2)];
    else if (sh == shFree) det += "FREE";
    else det += "GRID " + rt;
    const auto ft = fonte (10.0f).withExtraKerningFactor (0.3f);
    rotulo (g, { areaResult.getX() + 10, areaResult.getY() + 6, 120, 14 }, u8 ("RESULT \xc2\xb7 FILL"), cMu, juce::Justification::centredLeft, 10.0f);
    rotulo (g, { areaResult.getX() + 18 + largura (ft, u8 ("RESULT \xc2\xb7 FILL")), areaResult.getY() + 6, 400, 14 }, det, cAmb, juce::Justification::centredLeft, 10.0f, true);

    // chips: SLICES, PITCH, FILTER, ARRASTE PARA O DAW
    {
        float xd = areaResult.getRight() - 10;
        auto peq = [&] (const juce::String& t, juce::Colour cor) -> juce::Rectangle<float>
        {
            const float w = largura (fonte (9.5f, true).withExtraKerningFactor (0.25f), t) + 16;
            const juce::Rectangle<float> rr (xd - w, areaResult.getY() + 5, w, 17);
            g.setColour (cor.withAlpha (0.10f)); g.fillRoundedRectangle (rr, 3.0f);
            g.setColour (cor.withAlpha (0.7f)); g.drawRoundedRectangle (rr, 3.0f, 1.0f);
            rotulo (g, rr, t, cor, juce::Justification::centred, 9.5f, true);
            xd = rr.getX() - 5;
            return rr;
        };
        areaDaw = peq ("    ARRASTE PARA O DAW", cTx);
        {
            const float ax = areaDaw.getX() + 10, ay = areaDaw.getCentreY();
            g.setColour (cTx); g.drawLine (ax, ay - 5, ax, ay + 3, 1.4f);
            tri (g, { ax, ay + 3 }, 6, 2, cTx);
        }
        if (escolha (kFiltro) != flOff) peq (juce::String (ch::filtro[escolha (kFiltro)]) + " SWEEP", cRosa);
        if (std::abs (real (kPFim) - real (kPIni)) > 0.5f || std::abs (real (kPIni)) > 0.5f) peq ("PITCH " + sinal (real (kPFim), 0, " st"), cCiano);
        peq ("SLICES " + juce::String (v.plano.n), cAmb);
    }

    if (! v.valida || v.plano.n == 0) return;
    blocosFill (g, in);
    // FILTER (rosa tracejado)
    if (escolha (kFiltro) != flOff)
    {
        juce::Path fp;
        const float de = real (kFDe), ate = real (kFAte);
        auto yf = [&] (float f) { return in.getBottom() - (std::log (f / 20.0f) / std::log (1000.0f)) * in.getHeight(); };
        for (int i = 0; i <= 60; ++i)
        {
            const float t = i / 60.0f, f = de * std::pow (ate / juce::jmax (20.0f, de), t);
            const float x = in.getX() + W * t;
            if (i == 0) fp.startNewSubPath (x, yf (f)); else fp.lineTo (x, yf (f));
        }
        juce::Path tr;
        const float dash[2] = { 6.0f, 5.0f };
        juce::PathStrokeType (1.6f).createDashedStroke (tr, fp, dash, 2);
        g.setColour (cRosa); g.fillPath (tr);
    }
    // FREE: a curva desenhada
    if (sh == shFree)
    {
        juce::Path cp;
        for (int i = 0; i < numLivre; ++i)
        {
            const float x = in.getX() + W * i / (numLivre - 1), y = in.getBottom() - proc.motor().aLivre[i].load() * in.getHeight();
            if (i == 0) cp.startNewSubPath (x, y); else cp.lineTo (x, y);
            g.setColour (cTx); g.fillEllipse (x - 4, y - 4, 8, 8);
        }
        g.setColour (cTx.withAlpha (0.8f)); g.strokePath (cp, juce::PathStrokeType (1.4f));
        rotulo (g, { in.getX() + 4, in.getY(), 80, 12 }, "1/64", cDim, juce::Justification::centredLeft, 8.5f);
        rotulo (g, { in.getX() + 4, in.getBottom() - 12, 80, 12 }, "1/4", cDim, juce::Justification::centredLeft, 8.5f);
    }
}

// os blocos do fill (âmbar; reverse rosa, tape lilás, mute apagado) e a linha de PITCH em ciano
void RetalhoEditor::blocosFill (juce::Graphics& g, juce::Rectangle<float> in)
{
    const auto& v = vista;
    if (! v.valida || v.plano.n == 0) return;
    const float W = in.getWidth(), cy = in.getCentreY(), hh = in.getHeight() * 0.5f;
    const double L = (double) v.len;
    const int C = RetalhoProcessor::colunas;
    const float ganho = ganhoOnda (v.rMin, v.rMax);
    // linha de centro
    g.setColour (juce::Colour (0xff1d2128)); g.fillRect (in.getX(), cy - 0.5f, W, 1.0f);
    // a onda renderizada de verdade, fatia por fatia: dentro de cada fatia, barras de 2 px a cada 3 px com o pico real;
    // um vão entre as fatias deixa cada repetição visível (fatia curta = uma barra cheia, como nos displays de stutter)
    Pincel pc;
    for (int k = 0; k < v.plano.n; ++k)
    {
        const auto& f = v.plano.f[k];
        const float x0 = in.getX() + W * (float) (f.t0 / L), x1 = in.getX() + W * (float) ((f.t0 + f.dur) / L);
        const float vao = x1 - x0 > 8.0f ? 2.0f : (x1 - x0 > 3.0f ? 1.0f : 0.0f);
        const float a0 = x0, a1 = juce::jmax (x0 + 1.0f, x1 - vao);
        const juce::Colour cor = f.tipo == fMute ? cDim : (f.rev ? cRosa : (f.tipo == fTapeStop ? cLil : cAmb));
        if (f.tipo == fMute) { pc.barra (cor, a0, a1 - a0, cy, 0.6f); continue; }
        const float passo = 3.0f, bw = 2.0f;
        if (a1 - a0 <= passo + 0.5f)
        {
            const float pk = picoCols (v.rMin, v.rMax, (int) ((a0 - in.getX()) / W * C), (int) ((a1 - in.getX()) / W * C));
            pc.barra (cor, a0, a1 - a0, cy, juce::jmin (1.0f, pk * ganho) * hh);
            continue;
        }
        for (float x = a0; x < a1 - 0.5f; x += passo)
        {
            const float xe = juce::jmin (a1, x + bw);
            const float pk = picoCols (v.rMin, v.rMax, (int) ((x - in.getX()) / W * C), (int) ((x + passo - in.getX()) / W * C));
            pc.barra (cor, x, xe - x, cy, juce::jmin (1.0f, pk * ganho) * hh);
        }
    }
    pc.pinta (g, cy, hh);
    // PITCH (ciano)
    if (std::abs (real (kPFim) - real (kPIni)) > 0.5f || std::abs (real (kPIni)) > 0.5f)
    {
        juce::Path pp;
        auto yst = [&] (float st) { return cy - juce::jlimit (-1.0f, 1.0f, st / 24.0f) * hh * 0.95f; };
        bool primeiro = true;
        for (int k = 0; k < v.plano.n; ++k)
        {
            const auto& f = v.plano.f[k];
            if (f.tipo == fTapeStop) continue;
            const float x0 = in.getX() + W * (float) (f.t0 / L), x1 = in.getX() + W * (float) ((f.t0 + f.dur) / L);
            const float s0 = f.st, s1 = f.r1 != f.r0 ? 12.0f * std::log2 (f.r1) : f.st;
            if (primeiro) { pp.startNewSubPath (x0, yst (s0)); primeiro = false; }
            else pp.lineTo (x0, yst (s0));
            pp.lineTo (x1, yst (s1));
        }
        g.setColour (cCiano.withAlpha (0.18f)); g.strokePath (pp, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cCiano); g.strokePath (pp, juce::PathStrokeType (1.8f));
    }
}

void RetalhoEditor::resultComplex (juce::Graphics& g, juce::Rectangle<float> in)
{
    const auto& v = vista;
    auto& m = proc.motor();
    const int N = 8 << lim (escolha (kPassos), 0, 2);
    const auto ft = fonte (10.0f).withExtraKerningFactor (0.3f);
    rotulo (g, { areaResult.getX() + 10, areaResult.getY() + 6, 160, 14 }, u8 ("RESULT \xc2\xb7 STEPS"), cMu, juce::Justification::centredLeft, 10.0f);
    rotulo (g, { areaResult.getX() + 18 + largura (ft, u8 ("RESULT \xc2\xb7 STEPS")), areaResult.getY() + 6, 500, 14 },
            juce::String (N) + u8 (" \xc2\xb7 ") + ch::length[lim (escolha (kLength), 0, 2)] + u8 (" \xc2\xb7 READ ") + ch::read[lim (escolha (kRead), 0, 2)]
                + (escolha (kDEvery) > 0 ? u8 (" \xc2\xb7 DICE ") + juce::String (ch::every[escolha (kDEvery)]) : juce::String()),
            cAmb, juce::Justification::centredLeft, 10.0f, true);
    {
        const float w = largura (fonte (9.5f, true).withExtraKerningFactor (0.25f), "    ARRASTE PARA O DAW") + 16;
        areaDaw = { areaResult.getRight() - 10 - w, areaResult.getY() + 5, w, 17 };
        g.setColour (cTx.withAlpha (0.7f)); g.drawRoundedRectangle (areaDaw, 3.0f, 1.0f);
        rotulo (g, areaDaw, "    ARRASTE PARA O DAW", cTx, juce::Justification::centred, 9.5f, true);
        const float ax = areaDaw.getX() + 10, ay = areaDaw.getCentreY();
        g.setColour (cTx); g.drawLine (ax, ay - 5, ax, ay + 3, 1.4f);
        tri (g, { ax, ay + 3 }, 6, 2, cTx);
    }
    areaCelulas = in.withTrimmedTop (-6).withTrimmedBottom (-4);
    const float gap = N > 16 ? 2.0f : 3.0f;
    const float cw = (in.getWidth() - gap * (N - 1)) / N;
    const float topo = in.getY() - 4, fundo = in.getBottom() + 2;
    const float cy = (topo + 18 + fundo - 18) * 0.5f, hh = (fundo - topo - 40) * 0.5f;
    for (int k = 0; k < N; ++k)
    {
        const uint32_t ps = m.aPasso[k].load();
        const int t = pTipo (ps);
        const juce::Colour cor = corTipo (t);
        const float x = in.getX() + k * (cw + gap);
        const juce::Rectangle<float> col (x, topo, cw, fundo - topo);
        g.setColour (cor.withAlpha (t == tMute ? 0.10f : 0.08f)); g.fillRoundedRectangle (col, 3.0f);
        g.setColour (cor.withAlpha (0.35f)); g.drawRoundedRectangle (col, 3.0f, 1.0f);
        // mini-onda do passo (do RESULT renderizado)
        if (v.valida && t != tMute)
        {
            const float f0 = (float) k / N, f1 = (float) (k + 1) / N;
            const int C = RetalhoProcessor::colunas;
            const float ganho = ganhoOnda (v.rMin, v.rMax);
            Pincel pc;
            for (float xx = x + 2; xx < x + cw - 2; xx += 2.0f)
            {
                const float fa = f0 + (f1 - f0) * (xx - x) / cw, fb = f0 + (f1 - f0) * (xx + 2.0f - x) / cw;
                pc.barra (cor, xx, 1.3f, cy, juce::jmin (1.0f, picoCols (v.rMin, v.rMax, (int) (fa * C), (int) (fb * C)) * ganho) * hh);
            }
            pc.pinta (g, cy, hh, false);
        }
        // topo: tipo + valor
        const float fh = N > 16 ? 7.5f : 9.0f;
        rotulo (g, { x, topo + 2, cw, 11 }, N > 16 ? juce::String (ch::tipo[t]).substring (0, 3) : juce::String (ch::tipo[t]), cor, juce::Justification::centred, fh, true);
        rotulo (g, { x, topo + 12, cw, 10 }, valorPasso (ps), cTx.withAlpha (0.8f), juce::Justification::centred, fh);
        // rodapé: cadeado + número
        cadeado (g, { x + cw * 0.5f - (N > 16 ? 0.0f : 8.0f), fundo - 9 }, 7.0f, pLock (ps), pLock (ps) ? cAmb : cDim);
        if (N <= 16) rotulo (g, { x + cw * 0.5f, fundo - 15, 20, 12 }, juce::String (k + 1), cDim, juce::Justification::centredLeft, 8.5f);
    }
    celX0 = in.getX(); celW = cw + gap; celY0 = topo; celH = fundo - topo; nCelVis = N;
}

void RetalhoEditor::resultGroove (juce::Graphics& g, juce::Rectangle<float> in)
{
    const auto& v = vista;
    auto& m = proc.motor();
    const auto ft = fonte (10.0f).withExtraKerningFactor (0.3f);
    rotulo (g, { areaResult.getX() + 10, areaResult.getY() + 6, 160, 14 }, u8 ("RESULT \xc2\xb7 GROOVE"), cMu, juce::Justification::centredLeft, 10.0f);
    rotulo (g, { areaResult.getX() + 18 + largura (ft, u8 ("RESULT \xc2\xb7 GROOVE")), areaResult.getY() + 6, 600, 14 },
            juce::String (ch::estilo[lim (escolha (kEstilo), 0, 6)]) + u8 (" \xc2\xb7 ") + ch::bars[lim (escolha (kGBars), 0, 1)] + u8 (" \xc2\xb7 ")
                + ch::mix[lim (escolha (kMixVoz), 0, 2)] + (escolha (kMixVoz) == mvDuck ? " " + texto (kDuck) : juce::String())
                + (real (kLockGrid) > 0.5f ? u8 (" \xc2\xb7 GRID TRAVADA") : juce::String()),
            cVerde, juce::Justification::centredLeft, 10.0f, true);
    const bool tocando = m.grooveTocando.load();
    rotulo (g, { areaResult.getRight() - 300, areaResult.getY() + 6, 290, 14 },
            tocando ? juce::String ("GROOVE TOCANDO") : (real (kGrooveOn) > 0.5f ? u8 ("ENTRA NO PR\xc3\x93XIMO COMPASSO") : u8 ("GROOVE DESLIGADO \xc2\xb7 LIGUE EM GROOVE ON")),
            tocando ? cVerde : cDim, juce::Justification::centredRight, 9.5f, true);

    // 8 chops
    const float th = 46.0f, tg = 6.0f;
    const float tw = (in.getWidth() - tg * (numChops - 1)) / numChops;
    const int sel = proc.pincelChop.load();
    for (int s = 0; s < numChops; ++s)
    {
        const juce::Rectangle<float> rr (in.getX() + s * (tw + tg), in.getY() - 2, tw, th);
        const juce::Colour cor = corChop (s);
        const bool cheio = v.chopMs[s] > 0.0f;
        g.setColour (cor.withAlpha (cheio ? 0.10f : 0.04f)); g.fillRoundedRectangle (rr, 4.0f);
        g.setColour (s == sel ? cor : cor.withAlpha (0.3f)); g.drawRoundedRectangle (rr, 4.0f, s == sel ? 1.8f : 1.0f);
        if (cheio)
        {
            float mxo = 0.0f;
            for (int x = 0; x < 48; ++x) mxo = juce::jmax (mxo, v.chopOnda[s][x]);
            // a onda fica entre o número (em cima) e os ms (embaixo), sem encostar no texto
            const float gs = mxo > 0.0f ? 1.0f / mxo : 0.0f, cy = rr.getY() + 19.0f, bw = (rr.getWidth() - 28) / 48.0f;
            Pincel pc;
            for (int x = 0; x < 48; ++x)
                pc.barra (cor, rr.getX() + 22 + bw * x, juce::jmax (1.0f, bw - 0.8f), cy, juce::jmin (1.0f, v.chopOnda[s][x] * gs) * 10.0f);
            pc.pinta (g, cy, 10.0f, false);
        }
        rotulo (g, { rr.getX() + 6, rr.getY() + 3, 20, 14 }, juce::String (s + 1), cor, juce::Justification::centredLeft, 11.0f, true);
        rotulo (g, { rr.getX() + 4, rr.getBottom() - 14, rr.getWidth() - 8, 12 }, cheio ? juce::String (juce::roundToInt (v.chopMs[s])) + " ms" : u8 ("\xe2\x80\x94"),
                cMu, juce::Justification::centredRight, 8.5f);
        alvos.push_back ({ rr, [this, s] { proc.pincelChop = s; } });
    }

    // pattern: 3 linhas
    const int porC = escolha (kGGrid) == 1 ? 12 : 16;
    const int nCel = juce::jmin (maxCelulas, porC * (escolha (kGBars) + 1));
    const float y0 = in.getY() + th + 6, lw = 66.0f;
    const float lh = (in.getBottom() + 4 - y0) / 3.0f;
    const float cw = (in.getWidth() - lw) / nCel;
    const int tocandoCel = tocando ? m.grooveCel.load() : -1;
    const char* nomes[3] = { "OFFBEAT", "ACCENT", "FILL" };
    for (int l = 0; l < numLinhas; ++l)
    {
        const float y = y0 + l * lh;
        rotulo (g, { in.getX(), y, lw - 6, lh }, nomes[l], cMu, juce::Justification::centredLeft, 9.0f);
        for (int c = 0; c < nCel; ++c)
        {
            const float x = in.getX() + lw + c * cw;
            const juce::Rectangle<float> cel (x + 1, y + 2, cw - 2, lh - 4);
            const uint32_t cv = m.aCel[l][c].load();
            const int s = cChop (cv);
            const bool bat = c % (porC / 4) == 0;
            g.setColour (c == tocandoCel ? juce::Colour (0xff2a2f37) : (bat ? juce::Colour (0xff191d23) : juce::Colour (0xff14171c)));
            g.fillRoundedRectangle (cel, 2.5f);
            if (s >= 0)
            {
                const float a = cVel (cv) / 255.0f;
                const auto hc = cel.withTrimmedTop (cel.getHeight() * (1.0f - a));
                g.setColour (corChop (s).withAlpha (c == tocandoCel ? 1.0f : 0.8f)); g.fillRoundedRectangle (hc, 2.5f);
                if (cw > 14) rotulo (g, cel.withHeight (12), juce::String (s + 1), juce::Colour (0xff111111), juce::Justification::centred, 8.5f, true);
                if (cRoll (cv)) for (int j = 0; j < 3; ++j) { g.setColour (juce::Colour (0xff111111)); g.fillRect (cel.getX() + 3 + j * 3.5f, cel.getBottom() - 5, 2.0f, 3.0f); }
            }
        }
    }
    for (int c = 0; c <= nCel; c += porC)
    {
        const float x = in.getX() + lw + c * cw;
        g.setColour (juce::Colour (0xff3a414c)); g.drawLine (x, y0, x, y0 + 3 * lh, 1.2f);
    }
    celX0 = in.getX() + lw; celW = cw; celY0 = y0; celH = lh; nCelVis = nCel;
    areaCelulas = { in.getX() + lw, y0, cw * nCel, 3 * lh };
    rotulo (g, { areaResult.getX() + 10, areaResult.getBottom() - 22, areaResult.getWidth() - 20, 16 },
            u8 ("CLIQUE = P\xc3\x95" "E O CHOP ESCOLHIDO (EM CIMA) \xc2\xb7 CLIQUE DE NOVO = TIRA \xc2\xb7 ARRASTA = VELOCITY \xc2\xb7 DIREITO = ROLL"),
            cDim, juce::Justification::centredRight, 9.0f);
}

// ---- painéis ----------------------------------------------------------------------------------------------------------
void RetalhoEditor::paineis (juce::Graphics& g)
{
    const float y = 364, h = 90;
    const int modo = escolha (kModo);
    auto kn = [&] (int campo, const juce::String& nome, float cx, juce::Colour cor) { knob (g, campo, nome, { cx, y + 56 }, cor); };
    if (modo == mEdit)
    {
        painel (g, { 12, y, 336, h }, "SLICE", u8 ("CADA REPETI\xc3\x87\xc3\x83O"));
        kn (kDecay, "DECAY", 52, cAmb); kn (kFadeIn, "FADE IN", 116, cAmb); kn (kFadeOut, "FADE OUT", 180, cAmb);
        kn (kGate, "GATE", 244, cAmb); kn (kSwing, "SWING", 306, cAmb);

        painel (g, { 356, y, 330, h }, "PITCH", "KEY = ESCALA");
        kn (kPIni, "START", 398, cCiano); kn (kPFim, "END", 462, cCiano); kn (kPCurva, "CURVE", 526, cCiano);
        const bool gl = real (kGlide) > 0.5f;
        botao (g, { 572, y + 28, 50, 20 }, "STEPS", ! gl, cCiano, [this] { proc.define (kGlide, 0.0f); }, 9.5f);
        botao (g, { 626, y + 28, 52, 20 }, "GLIDE", gl, cCiano, [this] { proc.define (kGlide, 1.0f); }, 9.5f);
        const bool ky = real (kKey) > 0.5f;
        botao (g, { 572, y + 54, 50, 20 }, "KEY", ky, cVerde, [this] { proc.define (kKey, 1.0f); }, 9.5f);
        botao (g, { 626, y + 54, 52, 20 }, "FREE", ! ky, cVerde, [this] { proc.define (kKey, 0.0f); }, 9.5f);

        painel (g, { 694, y, 300, h }, "FILTER", "VARRE NO FILL");
        for (int i = 0; i < 4; ++i)
            botao (g, { 706.0f + (i % 2) * 44.0f, y + 28 + (i / 2) * 26.0f, 40, 20 }, ch::filtro[i], escolha (kFiltro) == i, cRosa,
                   [this, i] { proc.define (kFiltro, (float) i); }, 9.5f);
        kn (kFDe, "FROM", 828, cRosa); kn (kFAte, "TO", 890, cRosa); kn (kReso, "RESO", 952, cRosa);

        painel (g, { 1002, y, 186, h }, u8 ("STEREO \xc2\xb7 FX"), "");
        kn (kWidth, "STEREO", 1040, cAmb);
        liga (g, { 1082, y + 22, 96, 19 }, "TAPE STOP", kTapeStop, cLil);
        liga (g, { 1082, y + 44, 96, 19 }, "REVERSE", kReverse, cRosa);
        botao (g, { 1082, y + 66, 96, 19 }, "EXPORT", false, cAmb, [this]
        {
            const auto f = proc.exporta (false);
            if (f.existsAsFile()) f.revealToUser();
        }, 9.5f);
    }
    else if (modo == mComplex)
    {
        painel (g, { 12, y, 420, h }, "DICE", "SORTEIA OS PASSOS SEM CADEADO");
        {
            const juce::Rectangle<float> rd (24, y + 26, 84, 54);
            g.setColour (cAmb.withAlpha (0.12f)); g.fillRoundedRectangle (rd, 6.0f);
            g.setColour (cAmb); g.drawRoundedRectangle (rd, 6.0f, 1.4f);
            rotulo (g, rd.withTrimmedBottom (14), "DICE", cAmb, juce::Justification::centred, 15.0f, true);
            rotulo (g, rd.withTrimmedTop (30), texto (kSeed), cMu, juce::Justification::centred, 9.0f);
            alvos.push_back ({ rd, [this] { proc.dado(); } });
        }
        kn (kDAmount, "AMOUNT", 144, cAmb); kn (kDChaos, "CHAOS", 206, cAmb); kn (kSeed, "SEED", 268, cAmb);
        rotulo (g, { 306, y + 24, 120, 12 }, "EVERY (AUTO)", cMu, juce::Justification::centredLeft, 9.0f);
        for (int i = 0; i < 5; ++i)
            botao (g, { 306.0f + (i % 3) * 40.0f, y + 40 + (i / 3) * 24.0f, 37, 20 }, i == 0 ? juce::String ("OFF") : juce::String (ch::every[i]).upToFirstOccurrenceOf (" ", false, false),
                   escolha (kDEvery) == i, cAmb, [this, i] { proc.define (kDEvery, (float) i); }, 9.5f);

        painel (g, { 440, y, 380, h }, "TYPES", "SHIFT+CLIQUE NO PASSO = PINTA");
        const int pt = proc.pincelTipo.load();
        for (int t = 0; t < numTipos; ++t)
            botao (g, { 452.0f + (t % 4) * 88.0f, y + 28 + (t / 4) * 26.0f, 84, 20 }, ch::tipo[t], t == pt, corTipo (t),
                   [this, t] { proc.pincelTipo = t; }, 9.5f);

        painel (g, { 828, y, 220, h }, "FEEL", "");
        kn (kFadeIn, "FADE IN", 868, cAmb); kn (kFadeOut, "FADE OUT", 932, cAmb); kn (kWidth, "STEREO", 996, cAmb);

        painel (g, { 1056, y, 132, h }, "", "");
        liga (g, { 1068, y + 12, 108, 19 }, "KEY LOCK", kKey, cVerde);
        botao (g, { 1068, y + 36, 108, 19 }, "CLEAR LOCKS", false, cAmb, [this] { proc.limpaCadeados(); }, 9.5f);
        botao (g, { 1068, y + 60, 108, 19 }, "EXPORT", false, cAmb, [this]
        {
            const auto f = proc.exporta (false);
            if (f.existsAsFile()) f.revealToUser();
        }, 9.5f);
    }
    else
    {
        painel (g, { 12, y, 300, h }, "FEEL", "");
        kn (kDensity, "DENSITY", 52, cVerde); kn (kGSwing, "SWING", 120, cVerde); kn (kHumanize, "HUMANIZE", 188, cVerde); kn (kGPitch, "PITCH", 256, cVerde);

        painel (g, { 320, y, 250, h }, "CHOPS", "");
        kn (kSens, "SENSITIV.", 360, cVerde);
        rotulo (g, { 402, y + 24, 160, 12 }, "CHOP LEN", cMu, juce::Justification::centredLeft, 9.0f);
        for (int i = 0; i < 4; ++i)
            botao (g, { 402.0f + (i % 2) * 80.0f, y + 40 + (i / 2) * 24.0f, 76, 20 }, ch::chopLen[i], escolha (kChopLen) == i, cVerde,
                   [this, i] { proc.define (kChopLen, (float) i); }, 9.5f);

        painel (g, { 578, y, 420, h }, "MIX COM A VOZ", "");
        for (int i = 0; i < 3; ++i)
            botao (g, { 590, y + 22 + i * 22.0f, 84, 19 }, ch::mix[i], escolha (kMixVoz) == i, cVerde, [this, i] { proc.define (kMixVoz, (float) i); }, 9.5f);
        kn (kDuck, "DUCK", 718, cVerde); kn (kChopHpf, "HPF", 790, cVerde); kn (kChopLpf, "LPF", 862, cVerde); kn (kChopVol, "VOL", 934, cVerde);

        painel (g, { 1006, y, 182, h }, "", "");
        {
            const bool on = real (kGrooveOn) > 0.5f;
            const juce::Rectangle<float> rg (1018, y + 10, 158, 30);
            g.setColour (on ? cVerde : cPn2); g.fillRoundedRectangle (rg, 6.0f);
            g.setColour (on ? cVerde : cLn); g.drawRoundedRectangle (rg, 6.0f, 1.2f);
            rotulo (g, rg, on ? "GROOVE ON" : "GROOVE OFF", on ? juce::Colour (0xff111111) : cMu, juce::Justification::centred, 12.0f, true);
            alvos.push_back ({ rg, [this, on] { proc.define (kGrooveOn, on ? 0.0f : 1.0f); } });
        }
        botao (g, { 1018, y + 48, 76, 30 }, "DICE", false, cAmb, [this] { proc.dadoGroove(); }, 11.0f);
        liga (g, { 1100, y + 48, 76, 30 }, "LOCK GRID", kLockGrid, cAmb);
    }
}

// ---- rodapé LIVE ------------------------------------------------------------------------------------------------------
void RetalhoEditor::rodape (juce::Graphics& g)
{
    const float y0 = 464;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff13161b), 0, y0, juce::Colour (0xff0e1014), 0, (float) baseH, false));
    g.fillRect (0.0f, y0, (float) baseW, (float) baseH - y0);
    g.setColour (cLn); g.drawLine (0, y0, (float) baseW, y0, 1.0f);
    auto& m = proc.motor();

    // gatilho
    rotulo (g, { 14, y0 + 8, 100, 14 }, "LIVE", cMu, juce::Justification::centredLeft, 10.5f);
    const int gt = escolha (kGatilho);
    const float bw[3] = { 46, 46, 72 };   // ONE-SHOT com folga (encostava na borda)
    float x = 14;
    for (int i = 0; i < 3; ++i)
    {
        botao (g, { x, y0 + 26, bw[i], 22 }, ch::gatilho[i], gt == i, cAmb, [this, i] { proc.define (kGatilho, (float) i); }, 9.5f);
        x += bw[i] + 4;
    }
    {
        const juce::Rectangle<float> r (14, y0 + 56, 172, 42);
        caixa (g, r, cPn, 6.0f);
        rotulo (g, { r.getX() + 10, r.getY() + 4, 150, 13 }, gt == gAuto ? u8 ("AUTO \xe2\x80\x94 ONDE") : u8 ("AUTO \xe2\x80\x94 ONDE (S\xc3\x93 NO AUTO)"), cMu, juce::Justification::centredLeft, 9.0f);
        rotulo (g, { r.getX() + 10, r.getY() + 18, 160, 20 }, u8 (ch::onde[lim (escolha (kOnde), 0, 7)]), gt == gAuto ? cTx : cDim, juce::Justification::centredLeft, 12.5f, true);
        alvos.push_back ({ r, [this] { menuEscolha (kOnde); } });
    }
    {
        const juce::Rectangle<float> r (14, y0 + 104, 172, 42);
        caixa (g, r, cPn, 6.0f);
        rotulo (g, { r.getX() + 10, r.getY() + 4, 150, 13 }, "CATCH", cMu, juce::Justification::centredLeft, 9.0f);
        rotulo (g, { r.getX() + 10, r.getY() + 18, 140, 20 }, ch::catchs[lim (escolha (kCatch), 0, 3)], cAmb, juce::Justification::centredLeft, 12.5f, true);
        tri (g, { r.getRight() - 16, r.getY() + 28 }, 8, 2, cAmb);
        alvos.push_back ({ r, [this] { menuEscolha (kCatch); } });
    }

    // pads
    rotulo (g, { 198, y0 + 8, 300, 14 }, u8 ("PADS \xc2\xb7 MEM\xc3\x93RIAS"), cMu, juce::Justification::centredLeft, 10.5f);
    rotulo (g, { 420, y0 + 8, 330, 14 }, u8 ("MIDI C3\xe2\x80\x93" "F3 \xc2\xb7 AUTOMA\xc3\x87\xc3\x83O \xc2\xb7 CLIQUE"), cDim, juce::Justification::centredRight, 9.0f);
    static const char* notas[numPads] = { "C3", "C#3", "D3", "D#3", "E3", "F3" };
    const int aceso = m.padAceso.load();
    for (int i = 0; i < numPads; ++i)
    {
        const juce::Rectangle<float> r (198.0f + i * 93.0f, y0 + 26, 88, 74);
        areaPads[i] = r;
        const auto ip = proc.infoPad (i);
        const juce::Colour cor = ! ip.cheio ? cDim : (ip.modo == mGroove ? cVerde : (ip.modo == mComplex ? cLil : cAmb));
        const bool on = aceso == i || padSegurado == i;
        g.setColour (on ? cor.withAlpha (0.22f) : cPn); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? cor : (ip.cheio ? cor.withAlpha (0.45f) : cLn)); g.drawRoundedRectangle (r, 6.0f, on ? 1.8f : 1.0f);
        rotulo (g, { r.getX() + 8, r.getY() + 5, 80, 12 }, juce::String (i + 1) + u8 (" \xc2\xb7 ") + notas[i], cMu, juce::Justification::centredLeft, 8.5f);
        if (ip.cheio)
        {
            g.setColour (cor);
            g.setFont (fonte (11.0f, true).withExtraKerningFactor (0.12f));
            g.drawFittedText (ip.nome, juce::Rectangle<int> ((int) r.getX() + 8, (int) r.getY() + 18, (int) r.getWidth() - 12, 32), juce::Justification::topLeft, 2, 0.8f);
            rotulo (g, { r.getX() + 8, r.getBottom() - 16, r.getWidth() - 10, 12 }, ip.sub, cMu, juce::Justification::centredLeft, 8.0f);
        }
        else
        {
            rotulo (g, { r.getX() + 8, r.getY() + 22, r.getWidth() - 10, 16 }, u8 ("\xe2\x80\x94 vazio \xe2\x80\x94"), cMu, juce::Justification::centredLeft, 10.5f, true);
            rotulo (g, { r.getX() + 8, r.getBottom() - 28, r.getWidth() - 10, 24 }, "SHIFT+CLIQUE\n= SALVAR", cDim, juce::Justification::topLeft, 8.0f);
        }
    }
    rotulo (g, { 198, y0 + 106, 560, 14 }, u8 ("SHIFT+CLIQUE = SALVA A CENA \xc2\xb7 DIREITO = RENOMEIA / LIMPA \xc2\xb7 GROOVE LIGA E DESLIGA NO COMPASSO"),
            cDim, juce::Justification::centredLeft, 8.5f);

    // status
    {
        const juce::Rectangle<float> r (764, y0 + 26, 196, 74);
        caixa (g, r, cPn, 6.0f);
        rotulo (g, { r.getX() + 10, r.getY() + 5, 150, 12 }, "AGORA", cMu, juce::Justification::centredLeft, 9.0f);
        const float pr = m.fillProg.load();
        juce::String l1 = u8 ("\xe2\x80\x94"), l2;
        juce::Colour c1 = cDim;
        if (pr >= 0.0f)
        {
            const int fm = m.fillModo.load();
            l1 = fm == mComplex ? juce::String ("FILL COMPLEX") : juce::String ("FILL EDIT");
            c1 = fm == mComplex ? cLil : cAmb;
            const int fp = m.fillPad.load();
            l2 = fp >= 0 ? "PAD " + juce::String (fp + 1) + u8 (" \xc2\xb7 ") + proc.infoPad (fp).nome : juce::String ("AUTO");
            g.setColour (c1.withAlpha (0.25f)); g.fillRect (r.getX() + 10, r.getBottom() - 12, (r.getWidth() - 20), 4.0f);
            g.setColour (c1); g.fillRect (r.getX() + 10, r.getBottom() - 12, (r.getWidth() - 20) * std::fmod (pr, 1.0f), 4.0f);
        }
        else if (m.grooveTocando.load()) { l1 = "GROOVE"; c1 = cVerde; l2 = ch::estilo[lim (escolha (kEstilo), 0, 6)]; }
        else if (gt == gAuto && ! m.tocandoA.load()) l2 = u8 ("AUTO ESPERA O PLAY DO DAW");
        rotulo (g, { r.getX() + 10, r.getY() + 20, r.getWidth() - 20, 20 }, l1, c1, juce::Justification::centredLeft, 13.0f, true);
        rotulo (g, { r.getX() + 10, r.getY() + 40, r.getWidth() - 20, 14 }, l2, cMu, juce::Justification::centredLeft, 9.0f);
    }

    // saída
    knob (g, kDryWet, "DRY/WET", { 1018, y0 + 52 }, cTx);
    knob (g, kOutput, "OUTPUT", { 1092, y0 + 52 }, cTx);
    knob (g, kSens, "SENSITIV.", { 1162, y0 + 52 }, cMu);
    const int abv = proc.abAtual();
    botao (g, { 984, y0 + 96, 34, 24 }, "A", abv == 0, cTx, [this] { proc.trocaAB (0); }, 11.0f);
    botao (g, { 1022, y0 + 96, 34, 24 }, "B", abv == 1, cTx, [this] { proc.trocaAB (1); }, 11.0f);
    botao (g, { 1060, y0 + 96, 50, 24 }, u8 ("A\xe2\x86\x92" "B"), false, cTx, [this] { proc.copiaAB(); }, 10.0f);
    liga (g, { 1114, y0 + 96, 74, 24 }, "BYPASS", kBypass, cRosa);
#ifdef JucePlugin_VersionString
    const juce::String ver = juce::String (JucePlugin_VersionString);
#else
    const juce::String ver = "0.1.2";
#endif
    rotulo (g, { 700, (float) baseH - 18, 488, 14 }, u8 ("RETALHO ") + ver + u8 (" TESTE \xc2\xb7 NOME PROVIS\xc3\x93RIO"), cDim, juce::Justification::centredRight, 8.5f);
}

// ---- mouse ------------------------------------------------------------------------------------------------------------
void RetalhoEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    const int modo = escolha (kModo);
    moveu = false;
    xIni = p.x; yIni = p.y;

    // pads
    for (int i = 0; i < numPads; ++i)
        if (areaPads[i].contains (p))
        {
            if (e.mods.isPopupMenu()) { menuPad (i); return; }
            if (e.mods.isShiftDown()) { proc.salvaPad (i); repaint(); return; }
            if (! proc.infoPad (i).cheio)
            {
                if (simples()) { proc.salvaPad (i); repaint(); }   // SIMPLE: pad vazio = "+ SALVAR"
                return;
            }
            padSegurado = i;
            proc.apertaPad (i);
            return;
        }
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6)
        {
            if (k.campo < 0)
            {
                const int mi = -1 - k.campo;
                if (macroAtivo (mi)) { arrasto = arMacro; macroIdx = mi; valorIni = macroLe (mi); }
                return;
            }
            if (auto* prm = proc.param (k.campo))
            {
                arrasto = arKnob; knobCampo = k.campo;
                valorIni = prm->getValue();
                prm->beginChangeGesture();
            }
            return;
        }
    for (auto it = alvos.rbegin(); it != alvos.rend(); ++it)
        if (it->r.contains (p))
        {
            auto f = it->f;
            f();
            repaint();
            return;
        }
    if (areaDaw.contains (p) && modo != mGroove) { arrasto = arDaw; return; }
    if (areaAmostra.contains (p))
    {
        arrasto = arSel;
        selA0 = juce::jlimit (0.0f, 1.0f, (p.x - areaAmostra.getX()) / areaAmostra.getWidth());
        return;
    }
    if (simples()) return;

    auto& m = proc.motor();
    if (modo == mEdit && areaResult.contains (p))
    {
        const auto in = areaResult.reduced (10, 0).withTrimmedTop (26).withTrimmedBottom (34);
        const int sh = escolha (kShape);
        if (sh == shFree)
        {
            arrasto = arLivre;
            mouseDrag (e);
            return;
        }
        if (sh == shGrid && vista.plano.n > 0 && in.expanded (0, 20).contains (p))
        {
            const double t = (p.x - in.getX()) / in.getWidth() * (double) vista.len;
            for (int k = 0; k < vista.plano.n; ++k)
            {
                const auto& f = vista.plano.f[k];
                if (t >= (double) f.t0 && t < (double) (f.t0 + f.dur))
                {
                    const int gi = f.passo % maxGrade;
                    const uint32_t gv = m.aGrade[gi].load();
                    if (e.mods.isPopupMenu()) proc.escreveGrade (gi, grade (gOn (gv), ! gRev (gv), gVol (gv)));
                    else { arrasto = arGrade; arrastoIdx = gi; valorIni = (float) gVol (gv); }
                    return;
                }
            }
        }
        return;
    }
    if (modo == mComplex && areaCelulas.contains (p))
    {
        const int k = juce::jlimit (0, nCelVis - 1, (int) ((p.x - celX0) / celW));
        const uint32_t ps = m.aPasso[k].load();
        const float yr = p.y - celY0;
        if (e.mods.isPopupMenu() || yr > celH - 18) { proc.escrevePasso (k, passo (pTipo (ps), pValor (ps), ! pLock (ps))); return; }
        if (yr < 24) { proc.escrevePasso (k, passo ((pTipo (ps) + 1) % numTipos, pValor (ps), pLock (ps))); return; }
        if (e.mods.isShiftDown()) proc.escrevePasso (k, passo (proc.pincelTipo.load(), pValor (ps), pLock (ps)));
        arrasto = arPasso; arrastoIdx = k; valorIni = (float) pValor (m.aPasso[k].load());
        return;
    }
    if (modo == mGroove && areaCelulas.contains (p))
    {
        const int c = juce::jlimit (0, nCelVis - 1, (int) ((p.x - celX0) / celW));
        const int l = juce::jlimit (0, 2, (int) ((p.y - celY0) / celH));
        const uint32_t cv = m.aCel[l][c].load();
        if (e.mods.isPopupMenu())
        {
            if (cChop (cv) >= 0) proc.escreveCelula (l, c, celula (cChop (cv), cVel (cv), ! cRoll (cv)));
            return;
        }
        if (cChop (cv) < 0) { proc.escreveCelula (l, c, celula (proc.pincelChop.load(), 200)); return; }
        arrasto = arCelula; arrastoIdx = c; arrastoLinha = l; valorIni = (float) cVel (cv);
    }
}

void RetalhoEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (std::abs (p.y - yIni) + std::abs (p.x - xIni) > 3.0f) moveu = true;
    auto& m = proc.motor();
    switch (arrasto)
    {
        case arKnob:
            if (auto* prm = proc.param (knobCampo))
            {
                const float sens = e.mods.isShiftDown() ? 800.0f : 180.0f;
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, valorIni + (yIni - p.y) / sens));
            }
            break;
        case arMacro:
            macroDefine (macroIdx, valorIni + (yIni - p.y) / (e.mods.isShiftDown() ? 900.0f : 220.0f));
            break;
        case arGrade:
            if (moveu)
            {
                const uint32_t gv = m.aGrade[arrastoIdx].load();
                proc.escreveGrade (arrastoIdx, grade (true, gRev (gv), juce::jlimit (10, 255, (int) (valorIni + (yIni - p.y) * 2.5f))));
            }
            break;
        case arLivre:
        {
            const auto in = areaResult.reduced (10, 0).withTrimmedTop (26).withTrimmedBottom (34);
            const int i = juce::jlimit (0, numLivre - 1, juce::roundToInt ((p.x - in.getX()) / in.getWidth() * (numLivre - 1)));
            proc.escreveLivre (i, (in.getBottom() - p.y) / in.getHeight());
            break;
        }
        case arPasso:
        {
            const uint32_t ps = m.aPasso[arrastoIdx].load();
            proc.escrevePasso (arrastoIdx, passo (pTipo (ps), juce::jlimit (0, 255, (int) (valorIni + (yIni - p.y) * 2.0f)), pLock (ps)));
            break;
        }
        case arCelula:
            if (moveu)
            {
                const uint32_t cv = m.aCel[arrastoLinha][arrastoIdx].load();
                proc.escreveCelula (arrastoLinha, arrastoIdx, celula (cChop (cv), juce::jlimit (20, 255, (int) (valorIni + (yIni - p.y) * 3.0f)), cRoll (cv)));
            }
            break;
        case arSel:
            if (moveu && ! areaAmostra.isEmpty())
            {
                const float b = juce::jlimit (0.0f, 1.0f, (p.x - areaAmostra.getX()) / areaAmostra.getWidth());
                proc.selIni = juce::jmin (selA0, b);
                proc.selFim = juce::jmax (selA0, b);
            }
            break;
        case arDaw:
            if (moveu)
            {
                arrasto = arNada;
                const auto f = proc.exporta (true);
                if (f.existsAsFile())
                    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
            }
            break;
        default: break;
    }
    repaint();
}

void RetalhoEditor::mouseUp (const juce::MouseEvent&)
{
    auto& m = proc.motor();
    if (padSegurado >= 0) { proc.soltaPad (padSegurado); padSegurado = -1; }
    if (arrasto == arKnob)
        if (auto* prm = proc.param (knobCampo)) prm->endChangeGesture();
    if (arrasto == arGrade && ! moveu)
    {
        const uint32_t gv = m.aGrade[arrastoIdx].load();
        proc.escreveGrade (arrastoIdx, grade (! gOn (gv), gRev (gv), gVol (gv)));
    }
    if (arrasto == arCelula && ! moveu) proc.escreveCelula (arrastoLinha, arrastoIdx, 0u);
    arrasto = arNada;
    knobCampo = -1;
    repaint();
}

void RetalhoEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (areaAmostra.contains (p)) { proc.selIni = 0.0f; proc.selFim = 1.0f; repaint(); return; }
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6 && k.campo < 0)
        {
            if (macroAtivo (-1 - k.campo)) macroDefine (-1 - k.campo, macroPadrao (-1 - k.campo));
            repaint();
            return;
        }
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6)
            if (auto* prm = proc.param (k.campo))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->getDefaultValue());
                prm->endChangeGesture();
                repaint();
                return;
            }
}

void RetalhoEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / escala();
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6 && k.campo < 0)
        {
            const int mi = -1 - k.campo;
            if (macroAtivo (mi)) macroDefine (mi, macroLe (mi) + (w.deltaY > 0 ? 1.0f : -1.0f) * (mi == 0 && cartao() != 3 ? 0.26f : (mi == 3 ? 0.51f : 0.04f)));
            repaint();
            return;
        }
    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6)
            if (auto* prm = proc.param (k.campo))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * 0.05f));
                prm->endChangeGesture();
                repaint();
                return;
            }
}

// ---- menus ------------------------------------------------------------------------------------------------------------
void RetalhoEditor::menuEscolha (int campo)
{
    const int atual = escolha (campo);
    juce::PopupMenu mn;
    for (int i = 0; i < info (campo).nEsc; ++i) mn.addItem (i + 1, u8 (info (campo).esc[i]), true, i == atual);
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, campo] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        safe->proc.define (campo, (float) (r - 1));
        safe->repaint();
    });
}

void RetalhoEditor::menuPresets()
{
    juce::PopupMenu mn;
    juce::PopupMenu bancos[numBancos];
    const auto& v = presets();
    for (int i = 0; i < (int) v.size(); ++i)
        bancos[v[(size_t) i].banco].addItem (i + 1, v[(size_t) i].nome, true, proc.nomePreset() == v[(size_t) i].nome);
    for (int b = 0; b < numBancos; ++b) mn.addSubMenu (nomeBanco (b), bancos[b]);
    mn.addSeparator();
    mn.addItem (1001, u8 ("Salvar preset\xe2\x80\xa6"));
    mn.addItem (1002, u8 ("Abrir preset\xe2\x80\xa6"));
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r <= 1000) { safe->proc.aplicaPreset (r - 1); safe->repaint(); return; }
        const auto pasta = RetalhoProcessor::pastaPresets();
        pasta.createDirectory();
        if (r == 1001)
        {
            safe->seletor = std::make_unique<juce::FileChooser> ("Salvar preset", pasta.getChildFile (safe->proc.nomePreset() + ".retalho"), "*.retalho");
            safe->seletor->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                        [safe] (const juce::FileChooser& fc)
                                        {
                                            if (safe == nullptr) return;
                                            auto f = fc.getResult();
                                            if (f == juce::File()) return;
                                            safe->proc.salvaPresetArquivo (f.withFileExtension ("retalho"));
                                        });
        }
        else
        {
            safe->seletor = std::make_unique<juce::FileChooser> ("Abrir preset", pasta, "*.retalho");
            safe->seletor->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safe] (const juce::FileChooser& fc)
                                        {
                                            if (safe == nullptr) return;
                                            auto f = fc.getResult();
                                            if (f.existsAsFile()) safe->proc.abrePresetArquivo (f);
                                        });
        }
    });
}

void RetalhoEditor::menuPad (int i)
{
    const auto ip = proc.infoPad (i);
    juce::PopupMenu mn;
    mn.addSectionHeader ("PAD " + juce::String (i + 1) + (ip.cheio ? u8 (" \xc2\xb7 ") + ip.nome : juce::String()));
    mn.addItem (1, "Salvar a cena atual aqui");
    mn.addItem (2, "Carregar na tela (sem disparar)", ip.cheio);
    mn.addItem (3, u8 ("Renomear\xe2\x80\xa6"), ip.cheio);
    mn.addItem (4, "Limpar", ip.cheio);
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, i] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 1) safe->proc.salvaPad (i);
        else if (r == 2) safe->proc.motor().padPedido = i;
        else if (r == 4) safe->proc.limpaPad (i);
        else if (r == 3)
        {
            auto* w = new juce::AlertWindow ("Renomear pad " + juce::String (i + 1), "", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("nome", safe->proc.infoPad (i).nome);
            w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, i, w] (int res)
            {
                if (safe != nullptr && res == 1) safe->proc.renomeiaPad (i, w->getTextEditorContents ("nome"));
            }), true);
        }
        safe->repaint();
    });
}

void RetalhoEditor::menu()
{
    juce::PopupMenu mn;
    mn.addItem (1, "Como funciona");
    mn.addItem (2, proc.abAtual() == 0 ? u8 ("Copiar A \xe2\x86\x92 B") : u8 ("Copiar B \xe2\x86\x92 A"));
    mn.addItem (3, u8 ("Limitador de seguran\xc3\xa7" "a (\xe2\x88\x92" "0,3 dBFS)"), true, real (kLimit) > 0.5f);
    mn.addItem (4, "Abrir a pasta de exports");
    mn.addSeparator();
    mn.addItem (10, u8 ("Amostra: abrir arquivo\xe2\x80\xa6"));
    for (int b : { 1, 2, 4, 8 }) mn.addItem (10 + b, "Amostra: capturar " + juce::String (b) + (b > 1 ? " compassos" : " compasso"));
    mn.addItem (20, "Amostra: tirar (a faixa volta a passar)", proc.temAmostra());
    mn.addSeparator();
#ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    mn.addItem (-1, u8 ("RETALHO ") + juce::String (JucePlugin_VersionString) + " TESTE", false, false);
#endif
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 2) safe->proc.copiaAB();
        if (r == 10) safe->abreArquivo();
        if (r > 10 && r < 20) safe->proc.capturar (r - 10);
        if (r == 20) safe->proc.limpaAmostra();
        if (r == 3) safe->proc.define (kLimit, safe->real (kLimit) > 0.5f ? 0.0f : 1.0f);
        if (r == 4)
        {
            auto pasta = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RETALHO").getChildFile ("Exports");
            pasta.createDirectory();
            pasta.startAsProcess();
        }
        if (r == 1)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "RETALHO",
                u8 ("O RETALHO \xc3\xa9 um sampler: a voz fica DENTRO dele. Arraste o arquivo do vocal para a janela (ou ABRIR), "
                    "ou ponha o plugin na faixa da voz e clique em CAPTURAR: ele grava os compassos escolhidos a partir do pr\xc3\xb3ximo compasso. "
                    "Com amostra, a faixa original fica muda e a amostra toca em loop no tempo do projeto. Arraste na onda da amostra para "
                    "escolher o trecho que toca (duplo clique volta para ela inteira). Todos os efeitos atuam na amostra.\n\n"
                    "SIMPLE (a tela que abre): 1) escolha o efeito no cart\xc3\xa3o, 2) ajuste nos 5 knobs, 3) escolha QUANDO TOCA. "
                    "Os pads guardam e disparam ajustes. O PRO mostra todos os controles abaixo.\n\n"
                    "EDIT: o fill desenhado. SHAPE (GRID, RAMP, CURVE, FREE, TREMOLO), LENGTH e RATE; PITCH, FILTER, STEREO, "
                    "TAPE STOP e REVERSE. No GRID, clique nas fatias do RESULT para ligar ou desligar.\n\n"
                    "COMPLEX: passos com 8 tipos. Clique no topo do passo para trocar o tipo e arraste para o valor. "
                    "DICE sorteia e o cadeado protege.\n\n"
                    "GROOVE: a voz vira 8 chops (as \xc3\xbaltimas s\xc3\xadlabas cantadas) tocando no pattern do STYLE. "
                    "LAYER soma, DUCK abaixa a voz quando o chop entra, REPLACE deixa s\xc3\xb3 os chops. GROOVE ON liga no compasso.\n\n"
                    "LIVE: AUTO dispara sozinho no fim (ou no in\xc3\xad" "cio) do ciclo com o projeto tocando. HOLD toca enquanto "
                    "segura o pad e ONE-SHOT toca o fill inteiro. CATCH espera a pr\xc3\xb3xima linha da grade.\n\n"
                    "PADS: 6 mem\xc3\xb3rias de cena. Shift+clique salva e o MIDI C3\xe2\x80\x93" "F3 dispara (no Ableton, mande "
                    "MIDI para a faixa do RETALHO).\n\n"
                    "ARRASTE PARA O DAW: arraste o chip para soltar o WAV do RESULT numa faixa. EXPORT grava em "
                    "Documentos/RETALHO/Exports."));
    });
}

// =====================================================================================================================
// SIMPLE
// =====================================================================================================================
void RetalhoEditor::chaveModo (juce::Graphics& g)
{
    const juce::Rectangle<float> r (182, 11, 110, 26);
    g.setColour (cPn2); g.fillRoundedRectangle (r, 13.0f);
    g.setColour (cLn); g.drawRoundedRectangle (r, 13.0f, 1.0f);
    const bool s = simples();
    const auto rs = r.withWidth (58.0f), rp = r.withTrimmedLeft (58.0f);
    g.setColour (cAmb); g.fillRoundedRectangle (s ? rs : rp, 13.0f);
    rotulo (g, rs, "SIMPLE", s ? juce::Colour (0xff141414) : cMu, juce::Justification::centred, 9.5f, true);
    rotulo (g, rp, "PRO", s ? cMu : juce::Colour (0xff141414), juce::Justification::centred, 9.5f, true);
    alvos.push_back ({ rs, [this] { proc.uiPro = 0; } });
    alvos.push_back ({ rp, [this] { proc.uiPro = 1; } });
}

int RetalhoEditor::cartao() const
{
    const int md = escolha (kModo);
    if (md == mGroove) return 3;
    if (md == mComplex) return -1;
    if (real (kTapeStop) > 0.5f) return 2;
    const int sh = escolha (kShape);
    return (sh == shRamp || sh == shCurve) ? 1 : 0;
}

int RetalhoEditor::quando() const
{
    if (escolha (kGatilho) != gAuto) return 3;
    switch (escolha (kOnde)) { case 0: return 0; case 2: return 1; case 3: return 2; default: return -1; }
}

namespace
{
// RISER: SPEED anda por (RATE, RAMP TO)
const int riserRate[4] = { 1, 1, 2, 3 }, riserTo[4] = { 3, 4, 4, 4 };
float logPos (float v, float a, float b) { return std::log (juce::jlimit (juce::jmin (a, b), juce::jmax (a, b), v) / a) / std::log (b / a); }
} // namespace

float RetalhoEditor::macroLe (int i) const
{
    const int c = cartao();
    const bool gr = c == 3;
    switch (i)
    {
        case 0:
            if (gr) return real (kDensity) / 100.0f;
            if (c == 1)
            {
                const int r = escolha (kRate), t = escolha (kRampTo);
                return t <= 3 ? 0.0f : (r <= 1 ? 1.0f / 3.0f : (r == 2 ? 2.0f / 3.0f : 1.0f));
            }
            return escolha (kRate) / 4.0f;
        case 1:
            return gr ? (real (kGPitch) + 12.0f) / 24.0f : juce::jlimit (0.0f, 1.0f, (real (kPFim) + 12.0f) / 36.0f);
        case 2:
            if (gr)
                return real (kChopLpf) < 19500.0f ? 0.5f - 0.5f * logPos (real (kChopLpf), 20000.0f, 1500.0f)
                                                  : 0.5f + 0.5f * logPos (real (kChopHpf), 20.0f, 2000.0f);
            switch (escolha (kFiltro))
            {
                case flLpf: return 0.5f - 0.5f * logPos (real (kFAte), 18000.0f, 300.0f);
                case flHpf: return 0.5f + 0.5f * logPos (real (kFAte), 60.0f, 4000.0f);
                default:    return 0.5f;
            }
        case 3:
            return gr ? (float) escolha (kGBars) : escolha (kLength) / 2.0f;
        default:
            return real (kDryWet) / 100.0f;
    }
}

void RetalhoEditor::macroDefine (int i, float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    const int c = cartao();
    const bool gr = c == 3;
    switch (i)
    {
        case 0:
            if (gr) proc.define (kDensity, (float) juce::roundToInt (v * 100.0f));
            else if (c == 1)
            {
                const int k = juce::roundToInt (v * 3.0f);
                proc.define (kRate, (float) riserRate[k]);
                proc.define (kRampTo, (float) riserTo[k]);
            }
            else proc.define (kRate, (float) juce::roundToInt (v * 4.0f));
            break;
        case 1:
            if (gr) proc.define (kGPitch, (float) juce::roundToInt (v * 24.0f - 12.0f));
            else proc.define (kPFim, (float) juce::roundToInt (v * 36.0f - 12.0f));
            break;
        case 2:
        {
            const float f = v * 2.0f - 1.0f;   // −1 abafa … +1 abre
            if (gr)
            {
                if (f < -0.02f) proc.define (kChopLpf, 20000.0f * std::pow (1500.0f / 20000.0f, -f));
                else
                {
                    proc.define (kChopLpf, 20000.0f);
                    proc.define (kChopHpf, f > 0.02f ? 20.0f * std::pow (100.0f, f) : 20.0f);
                }
            }
            else if (f < -0.02f)
            {
                proc.define (kFiltro, (float) flLpf); proc.define (kFDe, 18000.0f);
                proc.define (kFAte, 18000.0f * std::pow (300.0f / 18000.0f, -f));
            }
            else if (f > 0.02f)
            {
                proc.define (kFiltro, (float) flHpf); proc.define (kFDe, 60.0f);
                proc.define (kFAte, 60.0f * std::pow (4000.0f / 60.0f, f));
            }
            else proc.define (kFiltro, (float) flOff);
            break;
        }
        case 3:
            if (gr) proc.define (kGBars, v > 0.5f ? 1.0f : 0.0f);
            else proc.define (kLength, (float) juce::roundToInt (v * 2.0f));
            break;
        default:
            proc.define (kDryWet, (float) juce::roundToInt (v * 100.0f));
            break;
    }
}

float RetalhoEditor::macroPadrao (int i) const
{
    const int c = cartao();
    switch (i)
    {
        case 0: return c == 3 ? 0.7f : (c == 1 ? 1.0f / 3.0f : 0.5f);
        case 1: return c == 3 ? 0.5f : 12.0f / 36.0f;
        case 2: return 0.5f;
        case 3: return c == 3 ? 0.0f : 0.5f;
        default: return 1.0f;
    }
}

juce::String RetalhoEditor::macroTexto (int i) const
{
    const int c = cartao();
    const bool gr = c == 3;
    switch (i)
    {
        case 0:
            if (gr) return juce::String (juce::roundToInt (real (kDensity))) + "%";
            if (c == 1) return juce::String (ch::rate[lim (escolha (kRate), 0, 4)]) + u8 (" \xe2\x86\x92 ") + ch::rate[lim (escolha (kRampTo), 0, 4)];
            return ch::rate[lim (escolha (kRate), 0, 4)];
        case 1:
            return sinal (gr ? real (kGPitch) : real (kPFim), 0, " st");
        case 2:
            if (gr)
            {
                if (real (kChopLpf) < 19500.0f) return "LPF " + hz (real (kChopLpf));
                return real (kChopHpf) > 21.0f ? "HPF " + hz (real (kChopHpf)) : juce::String ("OFF");
            }
            if (escolha (kFiltro) == flLpf) return "LPF " + hz (real (kFAte));
            if (escolha (kFiltro) == flHpf) return "HPF " + hz (real (kFAte));
            if (escolha (kFiltro) == flBpf) return "BPF " + hz (real (kFAte));
            return "OFF";
        case 3:
            return gr ? juce::String (ch::bars[lim (escolha (kGBars), 0, 1)]) : juce::String (ch::length[lim (escolha (kLength), 0, 2)]);
        default:
            return juce::String (juce::roundToInt (real (kDryWet))) + "%";
    }
}

void RetalhoEditor::telaSimples (juce::Graphics& g)
{
    auto& m = proc.motor();
    const int ca = cartao();

    // ---- topo: marca, SIMPLE/PRO, PRESET ◀ ▶, o que está ouvindo, menu ----
    g.setColour (juce::Colour (0xff15191f)); g.fillRect (0.0f, 0.0f, (float) baseW, 48.0f);
    g.setColour (cLn); g.drawLine (0, 48, (float) baseW, 48, 1.0f);
    {
        const auto f = fonte (25.0f, true).withExtraKerningFactor (0.34f);
        float x = 18;
        const juce::String partes[3] = { "RETA", "L", "HO" };
        g.setFont (f);
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (i == 1 ? cAmb : cTx);
            g.drawText (partes[i], juce::Rectangle<float> (x, 8, 120, 32), juce::Justification::centredLeft);
            x += largura (f, partes[i]);
        }
    }
    chaveModo (g);
    {
        const juce::Rectangle<float> rp (316, 9, 420, 30);
        caixa (g, rp, juce::Colour (0xff121418), 6.0f);
        rotulo (g, { rp.getX() + 12, rp.getY(), 60, rp.getHeight() }, "PRESET", cMu, juce::Justification::centredLeft, 9.0f);
        const juce::Rectangle<float> rn (rp.getX() + 70, rp.getY(), rp.getWidth() - 70 - 74, rp.getHeight());
        rotulo (g, rn.withTrimmedRight (18), proc.nomePreset(), cAmb, juce::Justification::centredLeft, 12.0f, true);
        tri (g, { rn.getRight() - 8, rn.getCentreY() }, 8, 2, cMu);
        alvos.push_back ({ rn, [this] { menuPresets(); } });
        const juce::Rectangle<float> ra (rp.getRight() - 68, rp.getY() + 3, 30, 24), rb (rp.getRight() - 34, rp.getY() + 3, 30, 24);
        botao (g, ra, "", false, cAmb, [this] { proc.presetVizinho (-1); });
        tri (g, ra.getCentre(), 9, 1, cTx);
        botao (g, rb, "", false, cAmb, [this] { proc.presetVizinho (1); });
        tri (g, rb.getCentre(), 9, 0, cTx);
    }
    {
        const bool am = proc.temAmostra();
        const bool cap = am ? proc.posAmostra() >= 0.0f : vista.valida;
        const juce::String t = juce::String (am ? (cap ? "AMOSTRA TOCANDO" : "AMOSTRA PRONTA") : (cap ? "SEM AMOSTRA: A FAIXA PASSA" : "SEM AMOSTRA"))
                               + u8 (" \xc2\xb7 ") + juce::String (juce::roundToInt (m.bpmA.load())) + " BPM";
        const float w = largura (fonte (10.0f, true).withExtraKerningFactor (0.22f), t);
        const float x1 = 1148;
        rotulo (g, { x1 - w - 4, 9, w + 4, 30 }, t, cap ? cTx : cDim, juce::Justification::centredRight, 10.0f, true);
        const float pulso = cap ? 0.75f + 0.25f * std::sin (quadro * 0.15f) : 0.3f;
        g.setColour ((cap ? cVerde : cDim).withAlpha (pulso));
        g.fillEllipse (x1 - w - 18, 21, 7, 7);
        const juce::Rectangle<float> r (1160, 11, 28, 26);
        caixa (g, r, cPn, 5.0f);
        g.setColour (cMu);
        for (int i = 0; i < 3; ++i) g.fillRect (r.getX() + 8, r.getY() + 8 + i * 4.5f, 12.0f, 1.6f);
        alvos.push_back ({ r, [this] { menu(); } });
    }

    auto secao = [&] (float x, float y, const char* n, const juce::String& t)
    {
        rotulo (g, { x, y, 16, 14 }, n, cAmb, juce::Justification::centredLeft, 11.0f, true);
        rotulo (g, { x + 16, y, 400, 14 }, t, cMu, juce::Justification::centredLeft, 10.0f);
    };

    // ---- 1 · efeito ----
    secao (14, 54, "1", "ESCOLHA O EFEITO");
    if (ca < 0)
        rotulo (g, { 600, 54, 588, 14 }, u8 ("CENA DO PRO (COMPLEX): ESCOLHA UM EFEITO OU AJUSTE NO PRO"), cLil, juce::Justification::centredRight, 9.0f);
    {
        static const char* nomes[4] = { "STUTTER", "RISER", "TAPE STOP", "GROOVE" };
        static const char* desc[4] = { "repete a s\xc3\xadlaba no ritmo", "acelera e sobe o tom at\xc3\xa9 o drop",
                                       "a voz \"freia\" e para", "a voz vira chops no contratempo" };
        const float cw = (1176.0f - 30.0f) / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> r (12.0f + i * (cw + 10.0f), 72, cw, 44);
            const bool on = ca == i;
            const juce::Colour cor = i == 3 ? cVerde : cAmb;
            if (on)
            {
                g.setColour (cor.withAlpha (0.10f)); g.fillRoundedRectangle (r.expanded (3), 11.0f);
                g.setGradientFill (juce::ColourGradient (cor.withAlpha (0.22f), 0, r.getY(), cor.withAlpha (0.06f), 0, r.getBottom(), false));
                g.fillRoundedRectangle (r, 9.0f);
                g.setColour (cor); g.drawRoundedRectangle (r, 9.0f, 1.6f);
            }
            else caixa (g, r, cPn, 9.0f);
            rotulo (g, { r.getX() + 16, r.getY() + 4, r.getWidth() - 24, 20 }, nomes[i], on ? cor : cTx, juce::Justification::centredLeft, 16.0f, true);
            rotulo (g, { r.getX() + 16, r.getY() + 25, r.getWidth() - 24, 14 }, u8 (desc[i]), on ? cTx.withAlpha (0.8f) : cMu, juce::Justification::centredLeft, 10.0f);
            alvos.push_back ({ r, [this, i] { proc.aplicaCartao (i); } });
        }
    }

    // ---- a amostra (dentro do plugin) e a onda do resultado ----
    faixaAmostra (g, { 12, 122, 1176, 64 });
    ondaSimples (g, { 12, 192, 1176, 122 });

    // ---- 2 · ajuste: 5 macros ----
    secao (14, 320, "2", "AJUSTE");
    {
        const juce::Rectangle<float> rm (12, 338, 788, 134);
        caixa (g, rm, cPn, 9.0f);
        const bool gr = ca == 3;
        const char* nomes[5] = { gr ? "DENSITY" : "SPEED", "PITCH", "FILTER", "LENGTH", "MIX" };
        const char* subs[5] = { gr ? "quantos chops entram" : "qu\xc3\xa3o r\xc3\xa1pido repete", "quanto o tom muda",
                                "\xe2\x86\x90 abafa \xc2\xb7 abre \xe2\x86\x92", "tamanho do efeito", "efeito \xc3\x97 voz" };
        const juce::Colour cores[5] = { gr ? cVerde : cAmb, cCiano, cRosa, gr ? cVerde : cAmb, cTx };
        const float zeros[5] = { 0.0f, gr ? 0.5f : 12.0f / 36.0f, 0.5f, 0.0f, 0.0f };
        const float cw = rm.getWidth() / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float cx = rm.getX() + cw * (i + 0.5f);
            const bool at = macroAtivo (i);
            if (i > 0) { g.setColour (cLn.withAlpha (0.6f)); g.drawLine (rm.getX() + cw * i, rm.getY() + 14, rm.getX() + cw * i, rm.getBottom() - 14, 1.0f); }
            rotulo (g, { cx - cw * 0.5f, rm.getY() + 9, cw, 16 }, nomes[i], at ? cTx : cDim, juce::Justification::centred, 12.5f, true);
            rotulo (g, { cx - cw * 0.5f, rm.getY() + 25, cw, 13 }, u8 (subs[i]), cDim, juce::Justification::centred, 9.0f);
            const juce::Point<float> c (cx, rm.getY() + 72);
            desenhaKnob (g, c, 23.0f, macroLe (i), zeros[i], cores[i], at);
            knobs.push_back ({ -1 - i, c, 23.0f });
            rotulo (g, { cx - cw * 0.5f, rm.getY() + 104, cw, 18 }, at ? macroTexto (i) : u8 ("\xe2\x80\x94"), at ? cTx : cDim, juce::Justification::centred, 12.0f, true);
        }
    }

    // ---- 3 · quando toca ----
    secao (814, 320, "3", "QUANDO TOCA");
    {
        const juce::Rectangle<float> rq (812, 338, 376, 134);
        caixa (g, rq, cPn, 9.0f);
        static const char* op[4] = { "TODO COMPASSO", "FIM DE 4 BARS", "FIM DE 8 BARS", "S\xc3\x93 NO PAD" };
        const int q = quando();
        const float bw = (rq.getWidth() - 24.0f - 8.0f) / 2.0f;
        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> r (rq.getX() + 12 + (i % 2) * (bw + 8), rq.getY() + 12 + (i / 2) * 40.0f, bw, 32);
            botao (g, r, u8 (op[i]), q == i, cAmb, [this, i]
            {
                if (i == 3) { proc.define (kGatilho, (float) gOneShot); return; }
                static const int onde[3] = { 0, 2, 3 };
                proc.define (kGatilho, (float) gAuto);
                proc.define (kOnde, (float) onde[i]);
            }, 11.0f);
        }
        static const char* expl[5] = {
            "Ajuste vindo do PRO. Escolha uma op\xc3\xa7\xc3\xa3o acima.",
            "Toca em todo compasso, no fim dele, com o projeto tocando.",
            "Entra sozinho no fim de cada frase de 4 compassos, com o projeto tocando.",
            "Entra sozinho no fim de cada frase de 8 compassos, com o projeto tocando.",
            ("S\xc3\xb3 toca quando voc\xc3\xaa clica num pad ou manda a nota MIDI (C3\xe2\x80\x93" "F3).") };
        g.setColour (cMu);
        g.setFont (fonte (10.0f));
        g.drawFittedText (u8 (expl[q + 1]), juce::Rectangle<int> ((int) rq.getX() + 14, (int) rq.getY() + 96, (int) rq.getWidth() - 28, 32),
                          juce::Justification::centredLeft, 2, 0.9f);
    }

    // ---- pads e saída ----
    const float y0 = 482;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff13161b), 0, y0, juce::Colour (0xff0e1014), 0, (float) baseH, false));
    g.fillRect (0.0f, y0, (float) baseW, (float) baseH - y0);
    g.setColour (cLn); g.drawLine (0, y0, (float) baseW, y0, 1.0f);
    rotulo (g, { 14, y0 + 6, 900, 14 },
            escolha (kGatilho) == gAuto ? u8 ("PADS \xc2\xb7 CLIQUE = CARREGA ESSE AJUSTE (EM \"S\xc3\x93 NO PAD\" O CLIQUE TOCA) \xc2\xb7 MIDI C3\xe2\x80\x93" "F3")
                                        : u8 ("PADS \xc2\xb7 CLIQUE = TOCA \xc2\xb7 MIDI C3\xe2\x80\x93" "F3"),
            cMu, juce::Justification::centredLeft, 10.0f);
    {
        static const char* notas[numPads] = { "C3", "C#3", "D3", "D#3", "E3", "F3" };
        const int aceso = m.padAceso.load();
        const float pw = (938.0f - 5.0f * 8.0f) / 6.0f;
        for (int i = 0; i < numPads; ++i)
        {
            const juce::Rectangle<float> r (12.0f + i * (pw + 8.0f), y0 + 24, pw, 82);
            areaPads[i] = r;
            const auto ip = proc.infoPad (i);
            const juce::Colour cor = ! ip.cheio ? cDim : (ip.modo == mGroove ? cVerde : (ip.modo == mComplex ? cLil : cAmb));
            const bool on = aceso == i || padSegurado == i;
            g.setColour (on ? cor.withAlpha (0.22f) : cPn); g.fillRoundedRectangle (r, 7.0f);
            g.setColour (on ? cor : (ip.cheio ? cor.withAlpha (0.45f) : cLn)); g.drawRoundedRectangle (r, 7.0f, on ? 1.8f : 1.0f);
            rotulo (g, { r.getX() + 10, r.getY() + 6, 80, 12 }, juce::String (i + 1) + u8 (" \xc2\xb7 ") + notas[i], cMu, juce::Justification::centredLeft, 8.5f);
            if (ip.cheio)
            {
                g.setColour (cor);
                g.setFont (fonte (12.0f, true).withExtraKerningFactor (0.1f));
                g.drawFittedText (ip.nome, juce::Rectangle<int> ((int) r.getX() + 10, (int) r.getY() + 22, (int) r.getWidth() - 16, 34), juce::Justification::topLeft, 2, 0.8f);
                rotulo (g, { r.getX() + 10, r.getBottom() - 18, r.getWidth() - 14, 12 },
                        ip.modo == mGroove ? "GROOVE" : (ip.modo == mComplex ? "GLITCH" : "FILL"), cMu, juce::Justification::centredLeft, 8.5f);
            }
            else
            {
                rotulo (g, { r.getX() + 10, r.getY() + 24, r.getWidth() - 14, 18 }, "+ SALVAR", cMu, juce::Justification::centredLeft, 12.0f, true);
                rotulo (g, { r.getX() + 10, r.getBottom() - 18, r.getWidth() - 14, 12 }, "guarda o ajuste atual", cDim, juce::Justification::centredLeft, 8.5f);
            }
        }
    }
    {
        const float x = 962, w = 226;
        const int abv = proc.abAtual();
        botao (g, { x, y0 + 24, 60, 26 }, "A", abv == 0, cTx, [this] { proc.trocaAB (0); }, 11.0f);
        botao (g, { x + 66, y0 + 24, 60, 26 }, "B", abv == 1, cTx, [this] { proc.trocaAB (1); }, 11.0f);
        botao (g, { x + 132, y0 + 24, w - 132, 26 }, u8 ("A\xe2\x86\x92" "B"), false, cTx, [this] { proc.copiaAB(); }, 10.0f);
        liga (g, { x, y0 + 56, w, 26 }, "BYPASS", kBypass, cRosa);
        // o que está tocando agora
        const float pr = m.fillProg.load();
        juce::String t = u8 ("\xe2\x80\x94");
        juce::Colour c1 = cDim;
        if (pr >= 0.0f)
        {
            const int fp = m.fillPad.load();
            t = "TOCANDO " + (fp >= 0 ? proc.infoPad (fp).nome : proc.nomePreset());
            c1 = cAmb;
            g.setColour (c1.withAlpha (0.25f)); g.fillRect (x, y0 + 104, w, 3.0f);
            g.setColour (c1); g.fillRect (x, y0 + 104, w * std::fmod (pr, 1.0f), 3.0f);
        }
        else if (m.grooveTocando.load()) { t = "TOCANDO GROOVE"; c1 = cVerde; }
        else if (quando() >= 0 && quando() < 3 && ! m.tocandoA.load()) t = u8 ("ESPERANDO O PLAY DO PROJETO");
        rotulo (g, { x, y0 + 86, w, 16 }, t, c1, juce::Justification::centredLeft, 9.5f, true);
    }
#ifdef JucePlugin_VersionString
    const juce::String ver = juce::String (JucePlugin_VersionString);
#else
    const juce::String ver = "0.1.2";
#endif
    rotulo (g, { 14, (float) baseH - 20, 900, 14 },
            u8 ("PAD VAZIO = GUARDA O AJUSTE \xc2\xb7 BOT\xc3\x83O DIREITO NO PAD = RENOMEIA / LIMPA \xc2\xb7 DUPLO CLIQUE NO KNOB = VOLTA AO PADR\xc3\x83O \xc2\xb7 PRO = TODOS OS AJUSTES"),
            cDim, juce::Justification::centredLeft, 8.5f);
    rotulo (g, { 900, (float) baseH - 20, 288, 14 }, u8 ("RETALHO ") + ver + " TESTE", cDim, juce::Justification::centredRight, 8.5f);
}

void RetalhoEditor::ondaSimples (juce::Graphics& g, juce::Rectangle<float> r)
{
    tela (g, r, 8.0f);
    auto& m = proc.motor();
    const auto& v = vista;
    const int modo = escolha (kModo);
    const bool gr = modo == mGroove;
    const auto in = r.reduced (14, 0).withTrimmedTop (32).withTrimmedBottom (8);

    // grade por tempo (tracejada)
    const double beats = v.len > 0 ? (double) v.len / v.spb : 4.0;
    gradeTempo (g, in, r.getY() + 28, r.getBottom() - 6, beats, (int) v.bpb);

    const juce::String tit = u8 ("RESULTADO \xc2\xb7 ");
    const auto ft = fonte (10.0f).withExtraKerningFactor (0.3f);
    rotulo (g, { r.getX() + 14, r.getY() + 8, 200, 14 }, tit, cMu, juce::Justification::centredLeft, 10.0f);
    rotulo (g, { r.getX() + 14 + largura (ft, tit), r.getY() + 8, 300, 14 },
            gr ? juce::String (ch::bars[lim (escolha (kGBars), 0, 1)]) + u8 (" \xc2\xb7 ") + ch::estilo[lim (escolha (kEstilo), 0, 6)]
               : juce::String (ch::length[lim (escolha (kLength), 0, 2)]),
            gr ? cVerde : cAmb, juce::Justification::centredLeft, 10.0f, true);

    if (! v.valida)
        rotulo (g, in, proc.temAmostra() ? u8 ("PREPARANDO O RESULTADO\xe2\x80\xa6")
                                         : u8 ("CARREGUE A AMOSTRA ACIMA (OU D\xc3\x8a PLAY COM A VOZ PASSANDO): O RESULTADO APARECE AQUI"),
                cDim, juce::Justification::centred, 11.0f);
    else if (! gr)
        blocosFill (g, in);
    else
    {
        // GROOVE: os chops no pattern, numa faixa só (a linha de cima manda)
        const int porC = escolha (kGGrid) == 1 ? 12 : 16;
        const int nCel = juce::jmin (maxCelulas, porC * (escolha (kGBars) + 1));
        const float cw = in.getWidth() / (float) nCel, cy = in.getCentreY(), hh = in.getHeight() * 0.5f;
        const int tc = m.grooveTocando.load() ? m.grooveCel.load() : -1;
        g.setColour (juce::Colour (0xff1d2128)); g.fillRect (in.getX(), cy - 0.5f, in.getWidth(), 1.0f);
        // cada chop no pattern é desenhado com a onda REAL da sílaba (48 pontos), na altura do velocity
        Pincel pc;
        for (int c = 0; c < nCel; ++c)
        {
            uint32_t cv = 0;
            for (int l = 0; l < numLinhas && cChop (cv) < 0; ++l) cv = m.aCel[l][c].load();
            const int s = cChop (cv);
            if (s < 0) continue;
            const float a = juce::jlimit (0.2f, 1.0f, cVel (cv) / 255.0f);
            const float bx = in.getX() + c * cw + 1.5f, bwid = juce::jmax (1.0f, cw - 3.0f);
            const juce::Colour cor = corChop (s);
            if (c == tc) { g.setColour (cor.withAlpha (0.12f)); g.fillRoundedRectangle (bx - 1.0f, in.getY() - 2.0f, bwid + 2.0f, in.getHeight() + 4.0f, 3.0f); }
            float mxo = 0.0f;
            for (int i = 0; i < 48; ++i) mxo = juce::jmax (mxo, v.chopOnda[s][i]);
            const float gs = mxo > 0.0f ? 1.0f / mxo : 0.0f;
            if (v.chopMs[s] <= 0.0f || gs == 0.0f) { pc.barra (cor, bx, bwid, cy, a * hh); continue; }
            for (float x = 0; x < bwid - 0.5f; x += 3.0f)
            {
                const float sa = v.chopOnda[s][juce::jlimit (0, 47, (int) (x * 48.0f / bwid))] * gs;
                pc.barra (cor, bx + x, juce::jmin (2.0f, bwid - x), cy, juce::jmin (1.0f, sa) * a * hh);
            }
        }
        pc.pinta (g, cy, hh);
        if (tc >= 0) agulha (g, in.getX() + (tc + 0.5f) * cw, r.getY() + 28, r.getBottom() - 6, cVerde);
        bool algum = false;
        for (int s = 0; s < numChops; ++s) algum = algum || v.chopMs[s] > 0.0f;
        if (! algum)
            rotulo (g, { in.getX(), in.getY() - 2, in.getWidth(), 14 }, u8 ("AINDA SEM S\xc3\x8dLABAS: D\xc3\x8a PLAY NO PROJETO"), cDim, juce::Justification::centred, 9.5f);
    }

    if (! gr)
    {
        // ARRASTE PARA O DAW
        const juce::String td = "    ARRASTE PARA O DAW";
        const float w = largura (fonte (10.0f, true).withExtraKerningFactor (0.25f), td) + 18;
        areaDaw = { r.getRight() - 12 - w, r.getY() + 7, w, 20 };
        g.setColour (cTx.withAlpha (0.06f)); g.fillRoundedRectangle (areaDaw, 4.0f);
        g.setColour (cTx.withAlpha (0.7f)); g.drawRoundedRectangle (areaDaw, 4.0f, 1.0f);
        rotulo (g, areaDaw, td, cTx, juce::Justification::centred, 10.0f, true);
        const float ax = areaDaw.getX() + 12, ay = areaDaw.getCentreY();
        g.setColour (cTx); g.drawLine (ax, ay - 5, ax, ay + 3, 1.4f);
        tri (g, { ax, ay + 3 }, 6, 2, cTx);

        // OUVIR (só o efeito, aqui no plugin) / LOOP — no topo, ao lado do ARRASTE
        const int pt = proc.previewTocando();
        const juce::Rectangle<float> r2 (areaDaw.getX() - 8 - 58, areaDaw.getY(), 58, 20), r1 (r2.getX() - 6 - 84, areaDaw.getY(), 84, 20);
        botao (g, r1, "     OUVIR", pt == 2, cAmb, [this] { proc.preview (2); }, 10.0f);
        tri (g, { r1.getX() + 15, r1.getCentreY() }, 7, 0, pt == 2 ? cAmb : cMu);
        botao (g, r2, "LOOP", proc.previewLoop.load(), cAmb, [this] { proc.previewLoop = ! proc.previewLoop.load(); }, 10.0f);

        float pr = -1.0f;
        if (m.fillProg.load() >= 0.0f && m.fillModo.load() == modo) pr = std::fmod (m.fillProg.load(), 1.0f);
        else if (proc.previewTocando() == 2) pr = proc.previewPos();
        if (pr >= 0.0f) agulha (g, in.getX() + in.getWidth() * pr, r.getY() + 28, r.getBottom() - 6, cAmb);
    }
    else
        rotulo (g, { r.getX() + 400, r.getY() + 8, r.getWidth() - 414, 14 },
                u8 ("OS CHOPS TOCAM NA HORA DE \"QUANDO TOCA\" \xc2\xb7 A AMOSTRA CALA ENQUANTO ELES TOCAM"), cDim, juce::Justification::centredRight, 8.5f);
}

// =====================================================================================================================
// SAMPLER: a amostra dentro do plugin
// =====================================================================================================================
bool RetalhoEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (RetalhoProcessor::arquivoDeAudio (f)) return true;
    return false;
}

void RetalhoEditor::filesDropped (const juce::StringArray& files, int, int)
{
    soltando = false;
    for (auto& f : files)
        if (RetalhoProcessor::arquivoDeAudio (f))
        {
            if (! proc.carregaArquivo (juce::File (f))) { avisoArquivo = u8 ("N\xc3\x83O CONSEGUI ABRIR ESSE ARQUIVO"); avisoQuadro = quadro; }
            else avisoArquivo.clear();
            break;
        }
    repaint();
}

void RetalhoEditor::abreArquivo()
{
    seletor = std::make_unique<juce::FileChooser> (u8 ("Abrir o vocal"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    seletor->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        if (! safe->proc.carregaArquivo (f)) { safe->avisoArquivo = u8 ("N\xc3\x83O CONSEGUI ABRIR ESSE ARQUIVO"); safe->avisoQuadro = safe->quadro; }
        else safe->avisoArquivo.clear();
        safe->repaint();
    });
}

void RetalhoEditor::menuCaptura()
{
    juce::PopupMenu mn;
    for (int b : { 1, 2, 4, 8 }) mn.addItem (b, juce::String (b) + (b > 1 ? " COMPASSOS" : " COMPASSO"), true, proc.capBars.load() == b);
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->proc.capBars = r;
    });
}

void RetalhoEditor::faixaAmostra (juce::Graphics& g, juce::Rectangle<float> r)
{
    caixa (g, r, cPn, 8.0f);
    const auto& va = visaoA;
    const bool tem = va.tem && proc.temAmostra();
    const int ce = proc.estadoCaptura();

    // esquerda: o que está carregado
    const juce::Rectangle<float> rl (r.getX() + 14, r.getY() + 6, 220, r.getHeight() - 12);
    rotulo (g, { rl.getX(), rl.getY(), rl.getWidth(), 14 }, "AMOSTRA", cMu, juce::Justification::centredLeft, 9.5f);
    if (tem)
    {
        rotulo (g, { rl.getX(), rl.getY() + 15, rl.getWidth(), 18 }, va.nome.toUpperCase(), cAmb, juce::Justification::centredLeft, 12.0f, true);
        const double dur = va.seg * (proc.selFim.load() - proc.selIni.load());
        const int k = proc.loopBars();
        rotulo (g, { rl.getX(), rl.getY() + 35, rl.getWidth(), 14 },
                juce::String (dur, 1).replaceCharacter ('.', ',') + u8 (" s \xc2\xb7 LOOP DE ") + juce::String (k) + (k > 1 ? " BARS" : " BAR"),
                cTx.withAlpha (0.75f), juce::Justification::centredLeft, 9.5f);
    }
    else
    {
        rotulo (g, { rl.getX(), rl.getY() + 15, rl.getWidth(), 18 }, "VAZIO", cDim, juce::Justification::centredLeft, 12.0f, true);
        rotulo (g, { rl.getX(), rl.getY() + 35, rl.getWidth(), 14 }, u8 ("A FAIXA PASSA DIRETO"), cDim, juce::Justification::centredLeft, 9.5f);
    }

    // direita: ABRIR, CAPTURAR (e quantos compassos), TIRAR
    const float by = r.getCentreY() - 13;
    float x = r.getRight() - 12;
    {
        const juce::Rectangle<float> rx (x - 30, by, 30, 26);
        botao (g, rx, "X", false, cRosa, tem ? std::function<void()> ([this] { proc.limpaAmostra(); }) : std::function<void()>(), 11.0f);
        x = rx.getX() - 6;
        const int cb = proc.capBars.load();
        const juce::Rectangle<float> rb (x - 70, by, 70, 26);
        botao (g, rb, juce::String (cb) + (cb > 1 ? " BARS" : " BAR") + "  ", false, cAmb, [this] { menuCaptura(); }, 10.0f);
        tri (g, { rb.getRight() - 11, rb.getCentreY() }, 7, 2, cMu);
        x = rb.getX() - 4;
        const juce::Rectangle<float> rc (x - 100, by, 100, 26);
        const bool armada = ce != 0;
        botao (g, rc, armada ? "CANCELAR" : "CAPTURAR", armada, cRosa, [this, armada] { if (armada) proc.cancelaCaptura(); else proc.capturar (proc.capBars.load()); }, 10.5f);
        if (! armada) { g.setColour (cRosa); g.fillEllipse (rc.getX() + 9, rc.getCentreY() - 3.5f, 7, 7); }
        x = rc.getX() - 6;
        const juce::Rectangle<float> ra (x - 80, by, 80, 26);
        botao (g, ra, u8 ("ABRIR\xe2\x80\xa6"), false, cAmb, [this] { abreArquivo(); }, 10.5f);
        x = ra.getX() - 14;
        // PONTO: em que ataque o efeito começa (◀ anterior · ▶ próximo) — muda e já renderiza de novo
        if (tem && proc.totalPontos() > 0)
        {
            const int pa = proc.pontoAtual(), np = proc.totalPontos();
            const juce::Rectangle<float> rn (x - 26, by, 26, 26), rt (x - 26 - 58, by, 58, 26), rp (x - 26 - 58 - 26, by, 26, 26);
            botao (g, rp, "", false, cAmb, pa > 0 ? std::function<void()> ([this] { proc.pontoVizinho (-1); }) : std::function<void()>());
            tri (g, { rp.getCentreX() - 1, rp.getCentreY() }, 7, 1, pa > 0 ? cAmb : cDim);
            botao (g, rn, "", false, cAmb, pa < np - 1 ? std::function<void()> ([this] { proc.pontoVizinho (1); }) : std::function<void()>());
            tri (g, { rn.getCentreX() + 1, rn.getCentreY() }, 7, 0, pa < np - 1 ? cAmb : cDim);
            rotulo (g, { rt.getX(), rt.getY() - 1, rt.getWidth(), 13 }, "PONTO", cMu, juce::Justification::centred, 8.5f);
            rotulo (g, { rt.getX(), rt.getY() + 11, rt.getWidth(), 15 }, juce::String (pa + 1) + " / " + juce::String (np), cAmb,
                    juce::Justification::centred, 11.0f, true);
            x = rp.getX() - 14;
        }
    }

    // meio: a onda da amostra, o trecho que toca e onde está tocando
    const juce::Rectangle<float> w (rl.getRight() + 10, r.getY() + 6, x - rl.getRight() - 10, r.getHeight() - 12);
    tela (g, w, 5.0f);
    if (ce == 1 || ce == 2)
    {
        const float pulso = 0.6f + 0.4f * std::sin (quadro * 0.2f);
        if (ce == 2)
        {
            g.setColour (cRosa.withAlpha (0.25f)); g.fillRoundedRectangle (w.withWidth (w.getWidth() * proc.progressoCaptura()), 5.0f);
            rotulo (g, w, u8 ("GRAVANDO \xc2\xb7 ") + juce::String (juce::roundToInt (proc.progressoCaptura() * 100.0f)) + "%", cRosa.withAlpha (pulso),
                    juce::Justification::centred, 12.0f, true);
        }
        else
            rotulo (g, w, u8 ("CAPTURA ARMADA: COME\xc3\x87" "A NO PR\xc3\x93XIMO COMPASSO COM O PROJETO TOCANDO"), cRosa.withAlpha (pulso),
                    juce::Justification::centred, 10.5f, true);
        return;
    }
    if (! tem)
    {
        juce::String t = u8 ("ARRASTE AQUI O ARQUIVO DO VOCAL OU CLIQUE EM CAPTURAR PARA GRAVAR A FAIXA");
        juce::Colour c = cMu;
        if (avisoArquivo.isNotEmpty() && quadro - avisoQuadro < 120) { t = avisoArquivo; c = cRosa; }
        else if (proc.capVazia.load()) { t = u8 ("A CAPTURA S\xc3\x93 PEGOU SIL\xc3\x8aNCIO: CAPTURE NUM TRECHO COM VOZ"); c = cRosa; }
        g.setColour (cLn); g.drawRoundedRectangle (w.reduced (2), 5.0f, 1.0f);
        rotulo (g, w, t, c, juce::Justification::centred, 10.5f, true);
        return;
    }
    areaAmostra = w;
    const float s0 = proc.selIni.load(), s1 = juce::jmax (s0 + 0.001f, proc.selFim.load());
    const int C = (int) va.mx.size();
    const float cy = w.getCentreY() - 3.0f, hh = w.getHeight() * 0.40f;
    {
        // onda real da amostra: o trecho que toca aceso (âmbar), o resto apagado
        const float ganho = ganhoOnda (va.mn, va.mx);
        Pincel pc;
        const juce::Colour acesa (0xffe8c25a), apagada = cOnda.withAlpha (0.55f);
        for (float px = 0; px < w.getWidth() - 1.0f && C > 0; px += 2.0f)
        {
            const float fr = px / w.getWidth();
            const float pk = picoCols (va.mn, va.mx, (int) (fr * C), (int) ((px + 2.0f) / w.getWidth() * C));
            pc.barra (fr >= s0 && fr <= s1 ? acesa : apagada, w.getX() + px, 1.3f, cy, juce::jmin (1.0f, pk * ganho) * hh);
        }
        pc.pinta (g, cy, hh, false);
    }
    const float x0 = w.getX() + w.getWidth() * s0, x1 = w.getX() + w.getWidth() * s1;
    if (s0 > 0.0005f || s1 < 0.9995f)
    {
        g.setColour (cAmb.withAlpha (0.07f)); g.fillRect (x0, w.getY(), x1 - x0, w.getHeight());
        g.setColour (cAmb); g.fillRect (x0 - 1, w.getY(), 2.0f, w.getHeight()); g.fillRect (x1 - 1, w.getY(), 2.0f, w.getHeight());
    }
    const float pf = proc.pontoFrac();
    if (pf >= 0.0f && pf <= 1.0f)
    {
        // o PONTO: onde o efeito começa (bandeira âmbar no alto)
        const float xf = w.getX() + w.getWidth() * pf;
        g.setColour (cAmb.withAlpha (0.85f)); g.fillRect (xf - 0.75f, w.getY() + 2, 1.5f, w.getHeight() - 4);
        tri (g, { xf, w.getY() + 6 }, 9, 2, cAmb);
    }
    const float pa = proc.posAmostra();
    if (pa >= 0.0f && pa <= 1.0f)
    {
        const float xp = x0 + (x1 - x0) * pa;
        agulha (g, xp, w.getY() + 1, w.getBottom() - 1, cVerde);
    }
    rotulo (g, { w.getX() + 6, w.getBottom() - 13, w.getWidth() - 12, 12 },
            u8 ("O EFEITO COME\xc3\x87" "A NO \xe2\x96\xbc \xc2\xb7 ARRASTE NA ONDA = TRECHO \xc2\xb7 DUPLO CLIQUE = INTEIRA"), cDim, juce::Justification::centredRight, 8.0f);
}
