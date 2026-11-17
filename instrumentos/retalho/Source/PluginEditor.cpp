#include "PluginEditor.h"

using namespace retalho;

namespace
{
// paleta do esboço v2: grafite azulado; ÂMBAR = fill / ligado, ROSA = reverse e filtro, CIANO = pitch, VERDE = groove
const juce::Colour cBg { 0xff111419 }, cBgB { 0xff0c0e12 }, cPn { 0xff171b21 }, cPn2 { 0xff20252d }, cLn { 0xff2b313b },
                   cTx { 0xffe9edf2 }, cMu { 0xff8a93a0 }, cDim { 0xff59616d }, cAmb { 0xfff6c445 }, cRosa { 0xffff6fa3 },
                   cCiano { 0xff3cc8d8 }, cVerde { 0xff43d17a }, cLil { 0xffb9a2ff }, cTela { 0xff07080a }, cOnda { 0xff6b7280 };

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

// a primeira frase que CABE na largura (da mais longa para a mais curta); se nenhuma couber, a última com a fonte reduzida
void rotuloQueCabe (juce::Graphics& g, juce::Rectangle<float> r, std::initializer_list<const char*> frases, juce::Colour cor, float h, bool negrito)
{
    const float wMax = r.getWidth() - 16.0f;
    juce::String ult;
    for (const char* f : frases)
    {
        ult = u8 (f);
        if (largura (fonte (h, negrito).withExtraKerningFactor (0.22f), ult) <= wMax) { rotulo (g, r, ult, cor, juce::Justification::centred, h, negrito); return; }
    }
    float hh = h;
    while (hh > 7.0f && largura (fonte (hh, negrito).withExtraKerningFactor (0.22f), ult) > wMax) hh -= 0.5f;
    rotulo (g, r, ult, cor, juce::Justification::centred, hh, negrito);
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
} // namespace

RetalhoEditor::RetalhoEditor (RetalhoProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    verOriginal = proc.vendoOriginal();   // reabriu a janela: a tela segue o que está soando
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
    ativacao.anexar (*this);   // sem licença cobre a tela e pede e-mail + token
}

void RetalhoEditor::resized() { proc.uiW = getWidth(); }

void RetalhoEditor::timerCallback()
{
    ++quadro;
    if (proc.versaoAmostra() != visaoA.versao) visaoA = proc.visaoAmostra();
    if (proc.versaoVista() != versaoVista)
    {
        proc.copiaVista (vista);
        versaoVista = vista.versao;
    }
    // 0.1.5: repinta só quando algo muda (antes eram 30 telas inteiras por segundo, parado ou não)
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h] (uint64_t x) { h = (h ^ x) * 1099511628211ull; };
    auto mixF = [&mix] (float x) { uint32_t b; std::memcpy (&b, &x, 4); mix (b); };
    for (int k = 0; k < numCampos; ++k) mixF (proc.valor (k));
    mix ((uint64_t) versaoVista); mix ((uint64_t) visaoA.versao);
    mixF (proc.previewTocando() != 0 ? proc.previewPos() : -1.0f); mix ((uint64_t) proc.previewTocando());
    mixF (proc.posResultado()); mixF (proc.posAmostra()); mix ((uint64_t) proc.estadoCaptura()); mixF (proc.progressoCaptura());
    mix (proc.capVazia.load() ? 1u : 0u); mix ((uint64_t) proc.pontoAtual()); mix ((uint64_t) proc.totalPontos()); mixF (proc.pontoFrac());
    mix (proc.temAmostra() ? 1u : 0u); mixF ((float) proc.motor().bpmA.load()); mix (proc.previewLoop.load() ? 1u : 0u);
    mixF (proc.selIni.load()); mixF (proc.selFim.load());
    for (const auto& pm : proc.pedacos()) mix ((uint64_t) (uint16_t) pm.u0 | (uint64_t) (uint16_t) pm.u1 << 16 | (uint64_t) (uint8_t) pm.tipo << 32);
    for (int k = 0; k < maxPassosG; ++k) mix (empacotaNota (proc.nota (k)));
    mix ((uint64_t) (selU0 + 7 * selU1)); mix (soltando ? 1u : 0u); mix (verOriginal ? 1u : 0u);
    mix ((uint64_t) proc.nomePreset().hashCode64());
    const bool anima = (proc.temAmostra() && proc.posAmostra() >= 0.0f) || proc.estadoCaptura() != 0
                       || (avisoExport.isNotEmpty() && quadro - avisoExportQuadro < 152) || (avisoArquivo.isNotEmpty() && quadro - avisoQuadro < 122);
    if (anima) mix ((uint64_t) quadro);
    if (h != assinatura) { assinatura = h; repaint(); }
}

juce::String RetalhoEditor::texto (int k) const { return textoV (k, real (k)); }

juce::String RetalhoEditor::textoV (int k, float v) const
{
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
        case kEGrao:   return juce::String (juce::roundToInt (v)) + " ms";
        case kETom:    return sinal (v, 0, " st");
        case kEVel:    return std::abs (v) < 0.5f ? juce::String ("PARADO") : (v < 0.0f ? u8 ("R\xc3\x89 ") + juce::String (juce::roundToInt (-v)) + "%" : juce::String (juce::roundToInt (v)) + "%");
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
    // áreas de mouse do quadro anterior não podem sobrar
    areaDaw = areaAmostra = {};

    telaSimples (g);
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

// ---- mouse ------------------------------------------------------------------------------------------------------------
void RetalhoEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    const int modo = escolha (kModo);
    moveu = false;
    xIni = p.x; yIni = p.y;

    for (auto& k : knobs)
        if (p.getDistanceFrom (k.c) <= k.r + 6)
        {
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
    // GROOVE: clique na grade põe / tira / move a nota; arrastar alonga; botão direito escolhe a sílaba
    if (modo == mMelodia && ! verOriginal && areaGrade.contains (p) && vista.valida)
    {
        const int nP = juce::jlimit (1, maxPassosG, (int) std::lround ((double) vista.len / (vista.spb * 0.25)));
        const int k = juce::jlimit (0, nP - 1, (int) ((p.x - areaGrade.getX()) / areaGrade.getWidth() * nP));
        const int linha = juce::jlimit (0, numLinhasG - 1, (int) ((areaGrade.getBottom() - p.y) / areaGrade.getHeight() * numLinhasG));
        int j = -1;   // a nota que cobre este passo
        for (int q = k; q >= 0 && q > k - 16; --q)
        {
            const NotaG n = proc.nota (q);
            if (n.on) { if (q + n.dur > k) j = q; break; }
        }
        if (j >= 0)
        {
            NotaG n = proc.nota (j);
            if (e.mods.isPopupMenu()) { menuNota (j); return; }
            if (n.linha == linha) { arrasto = arNota; arrastoIdx = j; return; }   // arrasta = alonga; clique sem arrastar = apaga
            if (j == k) { n.linha = linha; proc.escreveNota (j, n); repaint(); return; }   // outra linha no mesmo passo: muda a nota
            n.dur = k - j; proc.escreveNota (j, n);   // nota nova no meio da outra: a de antes acaba aqui
        }
        if (e.mods.isPopupMenu()) return;
        int ant = 0;
        for (int q = 0; q < k; ++q) if (proc.nota (q).on) ++ant;
        NotaG n; n.on = true; n.linha = linha; n.silaba = ant % juce::jmax (1, vista.nSil > 0 ? vista.nSil : maxSilabas); n.dur = 2;
        for (int q = k + 1; q < juce::jmin (nP, k + 2); ++q) if (proc.nota (q).on) n.dur = q - k;
        proc.escreveNota (k, n);
        arrasto = arNota; arrastoIdx = k; moveu = true;   // nota nova: arrastar já alonga, soltar não apaga
        repaint();
        return;
    }
    // PICOTE: arrastar na onda marca um pedaço; ao soltar, escolhe o efeito
    if (modo == mPicote && areaOnda.contains (p) && vista.valida)
    {
        arrasto = arPedaco;
        selU0 = selU1 = unidadeEm (p.x);
        repaint();
        return;
    }
    if (areaAmostra.contains (p))
    {
        arrasto = arSel;
        selA0 = juce::jlimit (0.0f, 1.0f, (p.x - areaAmostra.getX()) / areaAmostra.getWidth());
        return;
    }
}

void RetalhoEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (std::abs (p.y - yIni) + std::abs (p.x - xIni) > 3.0f) moveu = true;
    switch (arrasto)
    {
        case arKnob:
            if (auto* prm = proc.param (knobCampo))
            {
                const float sens = e.mods.isShiftDown() ? 800.0f : 180.0f;
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, valorIni + (yIni - p.y) / sens));
            }
            break;
        case arPedaco:
            selU1 = unidadeEm (p.x);
            break;
        case arNota:
            if (vista.valida && ! areaGrade.isEmpty())
            {
                const int nP = juce::jlimit (1, maxPassosG, (int) std::lround ((double) vista.len / (vista.spb * 0.25)));
                const int k = juce::jlimit (0, nP - 1, (int) ((p.x - areaGrade.getX()) / areaGrade.getWidth() * nP));
                NotaG n = proc.nota (arrastoIdx);
                if (n.on && k >= arrastoIdx)
                {
                    int lim2 = juce::jmin (16, nP - arrastoIdx);
                    for (int q = arrastoIdx + 1; q < arrastoIdx + lim2; ++q) if (proc.nota (q).on) { lim2 = q - arrastoIdx; break; }
                    n.dur = juce::jlimit (1, juce::jmax (1, lim2), k - arrastoIdx + 1);
                    proc.escreveNota (arrastoIdx, n);
                }
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
    if (arrasto == arKnob)
        if (auto* prm = proc.param (knobCampo)) prm->endChangeGesture();
    if (arrasto == arNota && ! moveu) proc.escreveNota (arrastoIdx, NotaG());   // clique na nota (sem arrastar) apaga
    if (arrasto == arPedaco)
    {
        if (! moveu && vista.valida)
        {
            // clique: o pedaço inteiro sob o mouse (a zona de efeito), ou 1 tempo da voz limpa
            const double u = vista.spb * 0.5;
            const double t = (selU0 + 0.5) * u;
            int a = (selU0 / 2) * 2, b = a + 2;
            for (int k = 0; k < vista.plano.n; ++k)
            {
                const auto& f = vista.plano.f[k];
                if (f.tipoPasso < 0 || t < (double) f.t0 || t >= (double) (f.t0 + f.dur)) continue;
                int k0 = k, k1 = k;
                while (k0 > 0 && vista.plano.f[k0 - 1].passo == f.passo && vista.plano.f[k0 - 1].tipoPasso >= 0) --k0;
                while (k1 + 1 < vista.plano.n && vista.plano.f[k1 + 1].passo == f.passo && vista.plano.f[k1 + 1].tipoPasso >= 0) ++k1;
                a = (int) std::lround ((double) vista.plano.f[k0].t0 / u);
                b = (int) std::lround ((double) (vista.plano.f[k1].t0 + vista.plano.f[k1].dur) / u);
                break;
            }
            selU0 = a; selU1 = juce::jmax (a, b - 1);
        }
        arrasto = arNada;
        menuPedaco();
        repaint();
        return;
    }
    arrasto = arNada;
    knobCampo = -1;
    repaint();
}

void RetalhoEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position / escala();
    if (areaAmostra.contains (p)) { proc.selIni = 0.0f; proc.selFim = 1.0f; repaint(); return; }
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

// ---- menu -------------------------------------------------------------------------------------------------------------
void RetalhoEditor::menu()
{
    juce::PopupMenu mn;
    mn.addItem (1, "Como funciona");
    mn.addItem (2, proc.abAtual() == 0 ? u8 ("Copiar A \xe2\x86\x92 B") : u8 ("Copiar B \xe2\x86\x92 A"));
    mn.addItem (3, u8 ("Limitador de seguran\xc3\xa7" "a (\xe2\x88\x92" "0,3 dBFS)"), true, real (kLimit) > 0.5f);
    mn.addItem (4, "Abrir a pasta de exports");
    mn.addItem (5, u8 ("Licen\xc3\xa7" "a\xe2\x80\xa6"));
    mn.addSeparator();
    mn.addItem (10, u8 ("Amostra: abrir arquivo\xe2\x80\xa6"));
    for (int b : { 1, 2, 4, 8 }) mn.addItem (10 + b, "Amostra: capturar " + juce::String (b) + (b > 1 ? " compassos" : " compasso"));
    mn.addItem (20, "Amostra: tirar (a faixa volta a passar)", proc.temAmostra());
    mn.addSeparator();
#ifdef JucePlugin_VersionString   // o programa da foto (shots) não é plugin e não tem a versão
    mn.addItem (-1, u8 ("RETALHO ") + juce::String (JucePlugin_VersionString), false, false);
#endif
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 2) safe->proc.copiaAB();
        if (r == 5) { safe->ativacao.mostrarInfo(); return; }
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
                    "PICOTE: a frase toca LIMPA e o efeito cai s\xc3\xb3 em alguns pontos. RECEITA escolhe quais efeitos, QUANTO EFEITO escolhe "
                    "quanto da frase \xc3\xa9 mexida (MANUAL = s\xc3\xb3 o que voc\xc3\xaa marcar) e VARIAR sorteia outra vers\xc3\xa3o. Arraste na onda (ou clique "
                    "num peda\xc3\xa7o) e escolha o efeito daquele peda\xc3\xa7o: LIMPA, REPETE, TOM, AO CONTR\xc3\x81RIO, ACELERA, FREIA ou CORTE. TAMANHO vai at\xc3\xa9 16 BARS.\n\n"
                    "ESTICA: a voz vira gr\xc3\xa3os. O knob grande \xc3\xa9 o MOVIMENTO: ele leva os knobs do AJUSTE FINO pelo caminho da DIRE\xc3\x87\xc3\x83O "
                    "(MEIO TEMPO, CONGELA, AO CONTR\xc3\x81RIO, ABRE, ESTILHA\xc3\x87" "A, T\xc3\x9aNEL) ao longo da frase, na FORMA escolhida (SOBE, SOBE E DESCE, "
                    "DEGRAUS, ONDA, DESCE). O arco claro em cada knob mostra onde o movimento est\xc3\xa1 agora. VARIAR troca a forma e os gr\xc3\xa3os.\n\n"
                    "GROOVE: as s\xc3\xadlabas da voz (do PONTO em diante) tocam numa grade de 1/16. Clique na grade para p\xc3\xb4r uma s\xc3\xadlaba (a altura "
                    "\xc3\xa9 a linha da ESCALA), arraste para alongar, clique de novo para apagar e use o bot\xc3\xa3o direito para escolher a s\xc3\xadlaba. "
                    "AFINAR leva cada s\xc3\xadlaba para a nota da linha. PADR\xc3\x83O traz grooves prontos e VARIAR sorteia um novo.\n\n"
                    "Com o projeto tocando, o resultado toca em loop no tempo dele. EXPORTAR WAV ou arraste o bot\xc3\xa3o ARRASTE PARA O DAW "
                    "para uma faixa. EXPORTAR grava em Documentos/RETALHO/Exports."));
    });
}

namespace
{
const juce::Colour cAzul { 0xff4f8cff }, cLaranja { 0xffff9a4d }, cLimpo { 0xff9aa3b0 };
juce::Colour corPk (int t)
{
    static const juce::Colour c[numPk] = { juce::Colour (0xfff6c445), juce::Colour (0xffff6fa3), juce::Colour (0xff4f8cff),
                                           juce::Colour (0xffb9a2ff), juce::Colour (0xffff9a4d), juce::Colour (0xff9aa5b1) };
    return t < 0 ? cLimpo : c[lim (t, 0, numPk - 1)];
}
juce::Colour corDir (int d)
{
    static const juce::Colour c[numDirecoes] = { juce::Colour (0xff3cc8d8), juce::Colour (0xff9fd8ff), juce::Colour (0xff4f8cff),
                                                 juce::Colour (0xff43d17a), juce::Colour (0xffff6fa3), juce::Colour (0xffb9a2ff) };
    return c[lim (d, 0, numDirecoes - 1)];
}
// dado (VARIAR)
void dado (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour cor)
{
    g.setColour (cor); g.drawRoundedRectangle (r, 3.0f, 1.4f);
    const float s = r.getWidth();
    for (auto p : { juce::Point<float> (0.28f, 0.28f), juce::Point<float> (0.72f, 0.72f), juce::Point<float> (0.5f, 0.5f),
                    juce::Point<float> (0.72f, 0.28f), juce::Point<float> (0.28f, 0.72f) })
        g.fillEllipse (r.getX() + p.x * s - 1.5f, r.getY() + p.y * s - 1.5f, 3.0f, 3.0f);
}
} // namespace

// =====================================================================================================================
// TELA PRINCIPAL (0.1.5): PICOTE | ESTICA | GROOVE, uma onda grande e poucos controles. O AVANÇADO saiu.
//  - PICOTE: arraste na onda (ou clique num pedaço) e escolha o efeito daquele pedaço;
//  - ESTICA: o knob grande é o MOVIMENTO, que leva os knobs do AJUSTE FINO pelo caminho da DIREÇÃO ao longo da frase;
//  - GROOVE: as sílabas da voz numa grade de 1/16, cada nota numa linha da escala (melodia).
// A onda (fundo, grade, zonas, notas) é desenhada numa imagem guardada e só refeita quando muda; a 30 Hz entra só o cursor.
// =====================================================================================================================
namespace
{
const juce::Colour cGroove { 0xff43d17a };
juce::Colour corModo (int md, int dir) { return md == mEstica ? corDir (dir) : (md == mMelodia ? cGroove : cAmb); }
juce::String nomeNota (float midi)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int m = (int) std::lround (midi);
    return juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
}
uint64_t mistura (uint64_t h, uint64_t x) { return (h ^ x) * 1099511628211ull; }
uint64_t misturaF (uint64_t h, float x) { uint32_t b; std::memcpy (&b, &x, 4); return mistura (h, b); }
// arco do movimento: de onde o knob está (v0) até onde o MOVIMENTO levou agora (v1)
void arcoMov (juce::Graphics& g, juce::Point<float> c, float raio, float v0, float v1, juce::Colour cor)
{
    if (std::abs (v1 - v0) < 0.004f) return;
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float x0 = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, v0), x1 = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, v1);
    const float ra = raio + 11.0f;
    juce::Path p; p.addCentredArc (c.x, c.y, ra, ra, 0, juce::jmin (x0, x1), juce::jmax (x0, x1), true);
    g.setColour (cor.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cor.brighter (0.4f)); g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const juce::Point<float> d (std::sin (x1), -std::cos (x1));
    g.setColour (juce::Colours::white); g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ c.x + d.x * ra, c.y + d.y * ra }));
}
} // namespace

void RetalhoEditor::telaSimples (juce::Graphics& g)
{
    const int md = escolha (kModo);
    const bool est = md == mEstica, gro = md == mMelodia;   // senão: PICOTE

    // ---- topo: marca, PICOTE | ESTICA | GROOVE, RECEITA / DIREÇÃO / PADRÃO ◀ ▶, VARIAR, status, menu ----
    g.setColour (juce::Colour (0xff15191f)); g.fillRect (0.0f, 0.0f, (float) baseW, 48.0f);
    g.setColour (cLn); g.drawLine (0, 48, (float) baseW, 48, 1.0f);
    {
        const auto f = fonte (24.0f, true).withExtraKerningFactor (0.30f);
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
    {
        const juce::Rectangle<float> r (176, 10, 264, 28);
        g.setColour (cPn2); g.fillRoundedRectangle (r, 14.0f);
        g.setColour (cLn); g.drawRoundedRectangle (r, 14.0f, 1.0f);
        static const char* nomes[3] = { "PICOTE", "ESTICA", "GROOVE" };
        const int mods[3] = { mPicote, mEstica, mMelodia };
        const juce::Colour cores[3] = { cAmb, cCiano, cGroove };
        for (int i = 0; i < 3; ++i)
        {
            const auto ri = r.withX (r.getX() + i * 88.0f).withWidth (88.0f);
            const bool on = md == mods[i];
            if (on) { g.setColour (cores[i]); g.fillRoundedRectangle (ri, 14.0f); }
            rotulo (g, ri, nomes[i], on ? juce::Colour (0xff141414) : cMu, juce::Justification::centred, 10.5f, true);
            const int mi = mods[i];
            alvos.push_back ({ ri, [this, mi] { proc.aplicaModo (mi); } });
        }
    }
    {
        const juce::Rectangle<float> rp (452, 9, 340, 30);
        caixa (g, rp, juce::Colour (0xff121418), 6.0f);
        rotulo (g, { rp.getX() + 12, rp.getY(), 62, rp.getHeight() }, est ? u8 ("DIRE\xc3\x87\xc3\x83O") : (gro ? u8 ("PADR\xc3\x83O") : u8 ("RECEITA")),
                cMu, juce::Justification::centredLeft, 9.0f);
        const juce::Rectangle<float> rn (rp.getX() + 74, rp.getY(), rp.getWidth() - 74 - 72, rp.getHeight());
        juce::String nome;
        juce::Colour cn = cAmb;
        if (est) { nome = u8 (ch::direcao[lim (escolha (kEDir), 0, 5)]); cn = corDir (escolha (kEDir)); }
        else if (gro)
        {
            cn = cGroove;
            nome = proc.nomePreset().startsWith ("GROOVE #") ? u8 ("SORTEADO ") + proc.nomePreset().fromFirstOccurrenceOf ("#", true, false)
                                                             : juce::String (escolha (kGPadrao) + 1) + "/8   " + u8 (ch::gpadrao[lim (escolha (kGPadrao), 0, 7)]);
        }
        else nome = juce::String (escolha (kReceita) + 1) + "/" + juce::String (numReceitas) + "   " + u8 (ch::receita[lim (escolha (kReceita), 0, numReceitas - 1)]);
        rotulo (g, rn, nome, cn, juce::Justification::centredLeft, 12.5f, true);
        alvos.push_back ({ rn, [this, est, gro]
        {
            juce::PopupMenu mn;
            if (est) for (int i = 0; i < numDirecoes; ++i) mn.addItem (1 + i, u8 (ch::direcao[i]), true, escolha (kEDir) == i);
            else if (gro) for (int i = 0; i < 8; ++i) mn.addItem (1 + i, u8 (ch::gpadrao[i]), true, escolha (kGPadrao) == i);
            else for (int i = 0; i < numReceitas; ++i) mn.addItem (1 + i, u8 (ch::receita[i]), true, escolha (kReceita) == i);
            juce::Component::SafePointer<RetalhoEditor> safe (this);
            mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, est, gro] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                if (est) safe->proc.define (kEDir, (float) (r - 1));
                else if (gro) safe->proc.aplicaPadraoG (r - 1);
                else safe->proc.aplicaReceita (r - 1);
            });
        } });
        const juce::Rectangle<float> ra (rp.getRight() - 68, rp.getY() + 3, 30, 24), rb (rp.getRight() - 34, rp.getY() + 3, 30, 24);
        auto passo = [this, est, gro] (int d)
        {
            if (est) proc.define (kEDir, (float) ((escolha (kEDir) + d + numDirecoes) % numDirecoes));
            else if (gro) proc.aplicaPadraoG (escolha (kGPadrao) + d);
            else proc.aplicaReceita (escolha (kReceita) + d);
        };
        botao (g, ra, "", false, cAmb, [passo] { passo (-1); });
        tri (g, ra.getCentre(), 9, 1, cTx);
        botao (g, rb, "", false, cAmb, [passo] { passo (1); });
        tri (g, rb.getCentre(), 9, 0, cTx);

        const juce::Rectangle<float> rv (rp.getRight() + 8, rp.getY(), 100, 30);
        const juce::Colour cv = corModo (md, escolha (kEDir));
        g.setColour (cv.withAlpha (0.12f)); g.fillRoundedRectangle (rv, 6.0f);
        g.setColour (cv); g.drawRoundedRectangle (rv.reduced (0.5f), 6.0f, 1.3f);
        dado (g, { rv.getX() + 10, rv.getCentreY() - 8, 16, 16 }, cv);
        rotulo (g, rv.withTrimmedLeft (32), "VARIAR", cv, juce::Justification::centredLeft, 10.5f, true);
        alvos.push_back ({ rv, [this] { proc.variar(); } });
    }
    {
        auto& m = proc.motor();
        const bool am = proc.temAmostra();
        const bool toca = am && proc.posAmostra() >= 0.0f;
        const juce::String t = juce::String (am ? (toca ? "TOCANDO" : "AMOSTRA PRONTA") : "SEM AMOSTRA")
                               + u8 (" \xc2\xb7 ") + juce::String (juce::roundToInt (m.bpmA.load())) + " BPM";
        const float w = largura (fonte (10.0f, true).withExtraKerningFactor (0.22f), t);
        const float x1 = 1148;
        rotulo (g, { x1 - w - 4, 9, w + 4, 30 }, t, am ? cTx : cDim, juce::Justification::centredRight, 10.0f, true);
        const float pulso = toca ? 0.75f + 0.25f * std::sin (quadro * 0.15f) : 0.35f;
        g.setColour ((am ? cVerde : cDim).withAlpha (pulso));
        g.fillEllipse (x1 - w - 18, 21, 7, 7);
        const juce::Rectangle<float> r (1160, 11, 28, 26);
        caixa (g, r, cPn, 5.0f);
        g.setColour (cMu);
        for (int i = 0; i < 3; ++i) g.fillRect (r.getX() + 8, r.getY() + 8 + i * 4.5f, 12.0f, 1.6f);
        alvos.push_back ({ r, [this] { menu(); } });
    }

    // ---- a amostra, a onda grande e os controles ----
    faixaAmostra (g, { 12, 54, 1176, 60 });
    ondaGrande (g, { 12, 122, 1176, 270 });
    if (est) controlesEstica (g, 402.0f);
    else if (gro) controlesGroove (g, 402.0f);
    else controlesPicote (g, 402.0f);

    // ---- rodapé ----
#ifdef JucePlugin_VersionString
    const juce::String ver = juce::String (JucePlugin_VersionString);
#else
    const juce::String ver = "0.2.0";
#endif
    juce::String dica;
    if (est) dica = u8 ("SOLTE O VOCAL NA JANELA \xc2\xb7 ESCOLHA A DIRE\xc3\x87\xc3\x83O E GIRE O MOVIMENTO: OS KNOBS DO AJUSTE ANDAM SOZINHOS \xc2\xb7 VARIAR TROCA A FORMA");
    else if (gro) dica = u8 ("SOLTE O VOCAL NA JANELA \xc2\xb7 CLIQUE NA GRADE P\xc3\x95" "E A S\xc3\x8DLABA \xc2\xb7 ARRASTE PARA ALONGAR \xc2\xb7 BOT\xc3\x83O DIREITO ESCOLHE A S\xc3\x8DLABA \xc2\xb7 VARIAR SORTEIA UM GROOVE");
    else dica = u8 ("SOLTE O VOCAL NA JANELA \xc2\xb7 ARRASTE NA ONDA E ESCOLHA O EFEITO DO PEDA\xc3\x87O \xc2\xb7 VARIAR SORTEIA OUTRA VERS\xc3\x83O \xc2\xb7 ARRASTE PARA O DAW");
    rotulo (g, { 14, (float) baseH - 30, 960, 20 }, dica, cDim, juce::Justification::centredLeft, 8.5f);
    rotulo (g, { 988, (float) baseH - 30, 200, 20 }, u8 ("RETALHO ") + ver, cDim, juce::Justification::centredRight, 8.5f);
}

// colcheia do PICOTE sob o x da onda
int RetalhoEditor::unidadeEm (float x) const
{
    if (areaOnda.isEmpty() || ! vista.valida || vista.len <= 0) return 0;
    const double u = vista.spb * 0.5;
    const int nU = juce::jmax (1, (int) std::lround ((double) vista.len / u));
    const double t = juce::jlimit (0.0, 0.9999, (double) (x - areaOnda.getX()) / areaOnda.getWidth()) * (double) vista.len;
    return juce::jlimit (0, nU - 1, (int) (t / u));
}

// a onda grande: fundo, título e botões por cima; a onda em si vem da imagem guardada
void RetalhoEditor::ondaGrande (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& v = vista;
    const int md = escolha (kModo);
    const bool est = md == mEstica, pic = md == mPicote, gro = md == mMelodia;
    const juce::Colour corM = corModo (md, escolha (kEDir));
    const auto in = r.reduced (18, 0).withTrimmedTop (44).withTrimmedBottom (est ? 76 : 52);
    areaOnda = in;
    areaGrade = gro ? in.withTrimmedLeft (40) : juce::Rectangle<float>();
    const float W = in.getWidth();

    // ---- a imagem da onda (refeita só quando o quadro, a vista ou as notas mudam) ----
    {
        uint64_t ch = 1469598103934665603ull;
        ch = mistura (ch, (uint64_t) v.versao); ch = mistura (ch, v.valida ? 1u : 0u); ch = mistura (ch, verOriginal ? 1u : 0u);
        ch = mistura (ch, (uint64_t) md); ch = mistura (ch, (uint64_t) escolha (kEDir));
        for (int k : { kEInt, kEForma, kSeed, kGEsc, kLength }) ch = misturaF (ch, real (k));
        if (gro) for (int k = 0; k < maxPassosG; ++k) ch = mistura (ch, empacotaNota (proc.nota (k)));
        if (pic) for (const auto& pm : proc.pedacos()) ch = mistura (ch, (uint64_t) (uint16_t) pm.u0 | (uint64_t) (uint16_t) pm.u1 << 16 | (uint64_t) (uint8_t) pm.tipo << 32);
        ch = mistura (ch, proc.temAmostra() ? 1u : 0u);
        const float ps = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int iw = juce::jmax (1, (int) std::ceil (r.getWidth() * ps)), ih = juce::jmax (1, (int) std::ceil (r.getHeight() * ps));
        if (cacheOnda.isNull() || ch != chaveOnda || cacheOnda.getWidth() != iw || cacheOnda.getHeight() != ih)
        {
            cacheOnda = juce::Image (juce::Image::ARGB, iw, ih, true);
            juce::Graphics gi (cacheOnda);
            gi.addTransform (juce::AffineTransform::translation (-r.getX(), -r.getY()).scaled ((float) iw / r.getWidth(), (float) ih / r.getHeight()));
            desenhaOnda (gi, r, in);
            chaveOnda = ch;
        }
        g.drawImage (cacheOnda, r);
    }

    // título
    {
        const juce::String a = verOriginal ? u8 ("ORIGINAL \xc2\xb7 ") : u8 ("RESULTADO \xc2\xb7 ");
        juce::String b = juce::String (ch::length[lim (escolha (kLength), 0, 5)]);
        if (pic) b += u8 (" \xc2\xb7 ") + juce::String (u8 (ch::receita[lim (escolha (kReceita), 0, numReceitas - 1)])) + u8 (" \xc2\xb7 ") + u8 (ch::quanto[lim (escolha (kQuanto), 0, 3)]);
        if (est) b += u8 (" \xc2\xb7 ") + juce::String (u8 (ch::direcao[lim (escolha (kEDir), 0, 5)])) + u8 (" \xc2\xb7 MOVIMENTO ")
                      + juce::String (juce::roundToInt (real (kEInt))) + u8 ("% \xc2\xb7 ") + u8 (ch::eforma[lim (escolha (kEForma), 0, 4)]);
        if (gro) b += u8 (" \xc2\xb7 ") + juce::String (u8 (ch::gesc[lim (escolha (kGEsc), 0, 3)])) + (real (kGAfina) > 0.5f ? u8 (" \xc2\xb7 AFINADO") : juce::String());
        const auto ft = fonte (10.5f).withExtraKerningFactor (0.3f), fb = fonte (10.5f, true).withExtraKerningFactor (0.3f);
        const float xa = r.getX() + 16, xb = xa + largura (ft, a) + 6;
        rotulo (g, { xa, r.getY() + 12, largura (ft, a) + 6, 16 }, a, cMu, juce::Justification::centredLeft, 10.5f);
        rotulo (g, { xb, r.getY() + 12, largura (fb, b) + 6, 16 }, b, corM, juce::Justification::centredLeft, 10.5f, true);
        if (pic || gro)
        {
            // a dica do que se faz na onda, se couber antes dos botões
            const juce::String d = pic ? u8 ("ARRASTE NA ONDA = MARCAR UM PEDA\xc3\x87O") : u8 ("CLIQUE = NOTA \xc2\xb7 ARRASTE = ALONGA");
            const float xd = xb + largura (fb, b) + 18, wd = largura (fonte (9.0f).withExtraKerningFactor (0.22f), d) + 6;
            if (xd + wd < r.getRight() - 14 - 58 - 6 - 92 - 12 - 196 - 12)
                rotulo (g, { xd, r.getY() + 12, wd, 16 }, d, cDim, juce::Justification::centredLeft, 9.0f);
        }
    }
    // direita: ORIGINAL | RESULTADO, OUVIR, LOOP
    const int pt = proc.previewTocando();
    {
        float x = r.getRight() - 14;
        const juce::Rectangle<float> rl (x - 58, r.getY() + 10, 58, 22);
        botao (g, rl, "LOOP", proc.previewLoop.load(), corM, [this] { proc.previewLoop = ! proc.previewLoop.load(); }, 10.0f);
        x = rl.getX() - 6;
        const int qual = verOriginal ? 1 : 2;
        const juce::Rectangle<float> ro (x - 92, r.getY() + 10, 92, 22);
        botao (g, ro, "      OUVIR", pt == qual, corM, [this, qual] { proc.preview (qual); }, 10.5f);
        tri (g, { ro.getX() + 17, ro.getCentreY() }, 8, 0, pt == qual ? corM : cMu);
        x = ro.getX() - 12;
        const juce::Rectangle<float> rc (x - 196, r.getY() + 10, 196, 22);
        g.setColour (cPn2); g.fillRoundedRectangle (rc, 11.0f);
        g.setColour (cLn); g.drawRoundedRectangle (rc, 11.0f, 1.0f);
        const auto r1 = rc.withWidth (92.0f), r2 = rc.withTrimmedLeft (92.0f);
        g.setColour (cTx.withAlpha (0.9f)); g.fillRoundedRectangle (verOriginal ? r1 : r2, 11.0f);
        rotulo (g, r1, "ORIGINAL", verOriginal ? juce::Colour (0xff141414) : cMu, juce::Justification::centred, 9.5f, true);
        rotulo (g, r2, "RESULTADO", verOriginal ? cMu : juce::Colour (0xff141414), juce::Justification::centred, 9.5f, true);
        alvos.push_back ({ r1, [this] { verOriginal = true;  proc.escolheOriginal (true); } });    // troca a onda E o som
        alvos.push_back ({ r2, [this] { verOriginal = false; proc.escolheOriginal (false); } });
    }
    if (! v.valida) return;

    // PICOTE: o trecho sendo marcado
    if (pic && selU0 >= 0 && selU1 >= 0)
    {
        const double u = v.spb * 0.5;
        const int a = juce::jmin (selU0, selU1), b = juce::jmax (selU0, selU1) + 1;
        const float x0 = in.getX() + W * (float) (a * u / (double) v.len), x1 = in.getX() + W * (float) juce::jmin (1.0, b * u / (double) v.len);
        g.setColour (cTx.withAlpha (0.10f)); g.fillRect (x0, in.getY() - 6, x1 - x0, in.getHeight() + 12);
        g.setColour (cTx.withAlpha (0.9f)); g.drawRect (x0, in.getY() - 6, x1 - x0, in.getHeight() + 12, 1.5f);
    }

    // cursor: o preview ou o resultado tocando com o projeto
    {
        float pos = proc.previewTocando() != 0 ? proc.previewPos() : proc.posResultado();
        if (pos >= 0.0f && pos <= 1.0f)
        {
            const auto ar = gro ? areaGrade : in;
            agulha (g, ar.getX() + ar.getWidth() * pos, in.getY() - 8, in.getBottom() + 8, corM);
        }
    }

    // ARRASTE PARA O DAW (só o botão arrasta: a onda agora serve para marcar pedaços / notas)
    {
        const juce::String td = "    ARRASTE PARA O DAW";
        const float w = largura (fonte (9.5f, true).withExtraKerningFactor (0.25f), td) + 18;
        const juce::Rectangle<float> rd (r.getRight() - 16 - w, r.getBottom() - 26, w, 18);
        g.setColour (cTx.withAlpha (0.06f)); g.fillRoundedRectangle (rd, 4.0f);
        g.setColour (cTx.withAlpha (0.5f)); g.drawRoundedRectangle (rd, 4.0f, 1.0f);
        rotulo (g, rd, td, cTx.withAlpha (0.85f), juce::Justification::centred, 9.5f, true);
        const float ax = rd.getX() + 12, ay = rd.getCentreY();
        g.setColour (cTx); g.drawLine (ax, ay - 5, ax, ay + 3, 1.4f);
        tri (g, { ax, ay + 3 }, 6, 2, cTx);
        areaDaw = rd.expanded (2);
    }
    // legenda (embaixo, à esquerda)
    float x = r.getX() + 16;
    const float y = r.getBottom() - 25;
    auto item = [&] (juce::Colour c, const juce::String& t)
    {
        g.setColour (c); g.fillRoundedRectangle (x, y + 5, 8, 8, 2.0f);
        const float w = largura (fonte (8.5f).withExtraKerningFactor (0.22f), t);
        rotulo (g, { x + 12, y, w + 4, 18 }, t, cMu, juce::Justification::centredLeft, 8.5f);
        x += w + 24;
    };
    if (pic && ! verOriginal)
    {
        item (cLimpo, "LIMPA " + juce::String (juce::roundToInt (v.limpo * 100.0f)) + "%");
        for (int t = 0; t < numPk; ++t) item (corPk (t), u8 (nomePk (t)));
        g.setColour (cTx); g.fillRect (x, y + 5, 2.0f, 8.0f); g.fillRect (x, y + 5, 7.0f, 2.0f);
        rotulo (g, { x + 12, y, 90, 18 }, u8 ("MARCADO"), cMu, juce::Justification::centredLeft, 8.5f);
    }
    if (est && ! verOriginal)
    {
        item (corM, u8 ("GR\xc3\x83O")); item (cTx, u8 ("OITAVA ACIMA")); item (cAzul, u8 ("DE R\xc3\x89"));
        g.setColour (corM); g.fillRect (x, y + 8, 14.0f, 2.0f);
        rotulo (g, { x + 18, y, 120, 18 }, u8 ("MOVIMENTO"), cMu, juce::Justification::centredLeft, 8.5f);
    }
    if (gro)
    {
        rotulo (g, { x, y, 60, 18 }, u8 ("S\xc3\x8DLABAS"), cMu, juce::Justification::centredLeft, 8.5f);
        x += 62;
        // só o número e a cor: a nota que toca é a da LINHA da grade (a altura medida na voz confundia: "2 B3" numa linha D#4)
        for (int s = 0; s < v.nSil; ++s)
            item (corChop (s), juce::String (s + 1));
        if (v.nSil == 0) rotulo (g, { x, y, 300, 18 }, u8 ("NENHUMA S\xc3\x8DLABA NO TRECHO"), cRosa, juce::Justification::centredLeft, 8.5f);
    }
}

// o que fica guardado na imagem: fundo, grade do tempo, zonas, a onda, os grãos, o movimento, a grade do GROOVE
void RetalhoEditor::desenhaOnda (juce::Graphics& g, juce::Rectangle<float> r, juce::Rectangle<float> in)
{
    tela (g, r, 10.0f);
    const auto& v = vista;
    const int md = escolha (kModo);
    const bool est = md == mEstica, pic = md == mPicote, gro = md == mMelodia;
    const juce::Colour corM = corModo (md, escolha (kEDir));
    const float W = in.getWidth(), cy = in.getCentreY(), hh = in.getHeight() * 0.5f;
    const int C = RetalhoProcessor::colunas;
    if (! v.valida)
    {
        rotulo (g, in, proc.temAmostra() ? u8 ("PREPARANDO O RESULTADO\xe2\x80\xa6")
                                         : u8 ("SOLTE O ARQUIVO DO VOCAL AQUI: O RESULTADO APARECE NA HORA"),
                cDim, juce::Justification::centred, 13.0f, true);
        return;
    }
    const double L = (double) v.len;
    const double beats = v.len > 0 ? L / v.spb : 4.0;
    const int bpb = juce::jmax (1, (int) v.bpb);

    // ---- GROOVE: a grade de notas (a onda do resultado fica bem apagada atrás) ----
    if (gro && ! verOriginal)
    {
        const auto gr = in.withTrimmedLeft (40);
        const int nP = juce::jlimit (1, maxPassosG, (int) std::lround (beats * 4.0));
        const float sw = gr.getWidth() / (float) nP, rh = gr.getHeight() / (float) numLinhasG;
        for (int l = 0; l < numLinhasG; ++l)
        {
            const float yl = gr.getBottom() - (l + 1) * rh;
            const int grau = grauEscala (escolha (kGEsc), l);
            g.setColour (grau % 12 == 0 ? juce::Colour (0xff1a2620) : (l % 2 ? juce::Colour (0xff12161b) : juce::Colour (0xff0f1216)));
            g.fillRect (gr.getX(), yl, gr.getWidth(), rh - 1.0f);
            const juce::String nm = v.baseMidi >= 0.0f && real (kGAfina) > 0.5f ? nomeNota (v.baseMidi + (float) grau) : "+" + juce::String (grau);
            rotulo (g, { in.getX(), yl, 36, rh }, nm, grau % 12 == 0 ? cGroove : cDim, juce::Justification::centredRight, 8.5f, grau % 12 == 0);
        }
        for (int k = 0; k <= nP; ++k)
        {
            const float xk = gr.getX() + k * sw;
            g.setColour (k % (4 * bpb) == 0 ? juce::Colour (0xff3a414c) : (k % 4 == 0 ? juce::Colour (0xff272d36) : juce::Colour (0xff181c22)));
            g.fillRect (xk, gr.getY(), 1.0f, gr.getHeight());
            if (k % (4 * bpb) == 0 && k < nP)
                rotulo (g, { xk + 4, in.getY() - 18, 40, 12 }, juce::String (k / (4 * bpb) + 1), cDim, juce::Justification::centredLeft, 9.0f, true);
        }
        // a onda do resultado, apagada
        {
            const float ganho = ganhoOnda (v.rMin, v.rMax);
            const float gcy = gr.getCentreY(), ghh = gr.getHeight() * 0.5f;
            Pincel pc;
            for (float x = 0; x < gr.getWidth() - 1.0f; x += 3.0f)
            {
                const float fr = x / gr.getWidth();
                const float a = juce::jmin (1.0f, picoCols (v.rMin, v.rMax, (int) (fr * C), (int) ((x + 3.0f) / gr.getWidth() * C)) * ganho);
                pc.barra (cGroove.withAlpha (0.16f), gr.getX() + x, 2.0f, gcy, a * ghh);
            }
            pc.pinta (g, gcy, ghh, false);
        }
        for (int k = 0; k < nP; ++k)
        {
            const NotaG n = proc.nota (k);
            if (! n.on) continue;
            const juce::Rectangle<float> rn (gr.getX() + k * sw + 1.0f, gr.getBottom() - (n.linha + 1) * rh + 1.5f,
                                             juce::jmax (4.0f, juce::jmin ((float) n.dur, (float) (nP - k)) * sw - 2.0f), rh - 4.0f);
            const juce::Colour cs = corChop (v.nSil > 0 ? n.silaba % v.nSil : n.silaba);
            g.setColour (cs.withAlpha (0.85f)); g.fillRoundedRectangle (rn, 3.0f);
            g.setColour (cs.brighter (0.5f)); g.drawRoundedRectangle (rn, 3.0f, 1.0f);
            if (rn.getWidth() > 11.0f)
                rotulo (g, rn.withTrimmedLeft (3), juce::String ((v.nSil > 0 ? n.silaba % v.nSil : n.silaba) + 1), juce::Colour (0xff111111),
                        juce::Justification::centredLeft, 9.0f, true);
        }
        return;
    }

    gradeTempo (g, in, in.getY() - 6, in.getBottom() + 6, beats, bpb);
    {
        const int bars = juce::jmax (1, (int) std::round (beats / juce::jmax (1.0, v.bpb)));
        const int passo = bars > 8 ? 2 : 1;   // 16 BARS: número a cada 2
        for (int b = 0; b < bars; b += passo)
        {
            const float x = in.getX() + W * (float) (b * v.bpb / beats);
            rotulo (g, { x + 5, in.getY() - 18, 40, 12 }, juce::String (b + 1), cDim, juce::Justification::centredLeft, 9.0f, true);
        }
    }
    g.setColour (juce::Colour (0xff1d2128)); g.fillRect (in.getX(), cy - 0.5f, W, 1.0f);

    // ---- PICOTE: zonas de efeito (faixa colorida + etiqueta); as marcadas à mão ganham a quina branca ----
    if (pic && v.plano.n > 0)
    {
        const auto pz = proc.pedacos();
        const double u = v.spb * 0.5;
        float ultimaEt = -1000.0f;
        for (int k = 0; k < v.plano.n;)
        {
            const auto& f = v.plano.f[k];
            int k1 = k + 1;
            if (f.tipoPasso < 0) { k = k1; continue; }
            while (k1 < v.plano.n && v.plano.f[k1].passo == f.passo && v.plano.f[k1].tipoPasso >= 0) ++k1;
            const auto& uf = v.plano.f[k1 - 1];
            const float x0 = in.getX() + W * (float) (f.t0 / L), x1 = in.getX() + W * (float) ((uf.t0 + uf.dur) / L);
            const juce::Colour cz = corPk (f.tipoPasso);
            g.setColour (cz.withAlpha (0.07f)); g.fillRect (x0, in.getY() - 4, x1 - x0, in.getHeight() + 8);
            g.setColour (cz.withAlpha (0.8f)); g.fillRect (x0, in.getY() - 4, x1 - x0, 2.0f);
            juce::String et = u8 (nomePk (f.tipoPasso));   // UTF-8 (sem isso "AO CONTRÁRIO" saía quebrado)
            if (f.tipoPasso == pkRepete || f.tipoPasso == pkTom) et += u8 (" \xc3\x97") + juce::String (k1 - k);
            const auto fe = fonte (8.5f, true).withExtraKerningFactor (0.15f);
            const float we = largura (fe, et) + 6;
            if (x1 - x0 > we + 4 && x0 > ultimaEt + 4)   // etiqueta só se couber e não encostar na anterior
            {
                rotulo (g, { x0 + 3, in.getBottom() + 6, x1 - x0 - 3, 12 }, et, cz, juce::Justification::centredLeft, 8.5f, true);
                ultimaEt = x0 + 3 + we;
            }
            k = k1;
        }
        // os pedaços marcados à mão (inclusive LIMPA)
        for (const auto& pm : pz)
        {
            const float x0 = in.getX() + W * (float) juce::jmin (1.0, pm.u0 * u / L), x1 = in.getX() + W * (float) juce::jmin (1.0, pm.u1 * u / L);
            if (x1 <= x0) continue;
            g.setColour (cTx.withAlpha (0.85f));
            g.fillRect (x0, in.getY() - 4, 2.0f, 9.0f); g.fillRect (x0, in.getY() - 4, juce::jmin (9.0f, x1 - x0), 2.0f);
            if (pm.tipo < 0) { g.setColour (cLimpo.withAlpha (0.5f)); g.fillRect (x0, in.getY() - 4, x1 - x0, 2.0f); }
        }
    }

    // ---- a onda ----
    if (verOriginal)
    {
        const float ganho = ganhoOnda (v.oMin, v.oMax);
        Pincel pc;
        for (float x = 0; x < W - 1.0f; x += 3.0f)
        {
            const float fr = x / W;
            const float a = juce::jmin (1.0f, picoCols (v.oMin, v.oMax, (int) (fr * C), (int) ((x + 3.0f) / W * C)) * ganho);
            pc.barra (cOnda.brighter (0.4f), in.getX() + x, 2.0f, cy, a * hh);
        }
        pc.pinta (g, cy, hh, false);
        if (est)
            for (const auto& gn : v.graos)
            {
                const float fx = gn.src * v.regiaoSobreL;
                if (fx > 1.0f) continue;
                const float x = in.getX() + W * fx;
                const float yc = in.getY() + 4 + (gn.pan + 1.0f) * 5.0f;
                g.setColour ((gn.rev ? cAzul : (gn.oitava ? cTx : corM)).withAlpha (0.6f)); g.fillEllipse (x - 2.0f, yc - 2.0f, 4.0f, 4.0f);
            }
        return;
    }
    {
        const float ganho = ganhoOnda (v.rMin, v.rMax);
        Pincel pc;
        int k = 0;
        for (float x = 0; x < W - 1.0f; x += 3.0f)
        {
            const float fr = x / W;
            juce::Colour cor = corM;
            if (pic && v.plano.n > 0)
            {
                const double t = fr * L;
                while (k + 1 < v.plano.n && t >= (double) v.plano.f[k + 1].t0) ++k;
                const auto& f = v.plano.f[k];
                cor = f.tipo == fMute ? cDim : corPk (f.tipoPasso);
            }
            const float a = juce::jmin (1.0f, picoCols (v.rMin, v.rMax, (int) (fr * C), (int) ((x + 3.0f) / W * C)) * ganho);
            pc.barra (cor, in.getX() + x, 2.0f, cy, a * hh);
        }
        pc.pinta (g, cy, hh);
    }
    if (est)
    {
        // o MOVIMENTO ao longo da frase (quanto do caminho da direção em cada ponto)
        const Cena c = proc.cenaAtual();
        const double comp = L / juce::jmax (1.0, v.spb * v.bpb);
        juce::Path p;
        for (float x = 0; x <= W; x += 3.0f)
        {
            const float yy = in.getBottom() - 2.0f - (float) movimentoEm (c, x / W, comp) * (in.getHeight() - 4.0f);
            if (x == 0) p.startNewSubPath (in.getX(), yy); else p.lineTo (in.getX() + x, yy);
        }
        g.setColour (corM.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (5.0f));
        g.setColour (corM.brighter (0.3f).withAlpha (0.9f)); g.strokePath (p, juce::PathStrokeType (1.6f));
        // a faixa dos grãos embaixo (esquerda em cima, direita embaixo; ré azul, oitava branca)
        if (! v.graos.empty())
        {
            const float y0 = in.getBottom() + 18, h = 24;
            rotulo (g, { in.getX(), in.getBottom() + 5, 60, 11 }, u8 ("GR\xc3\x83OS"), cDim, juce::Justification::centredLeft, 8.5f, true);
            g.setColour (juce::Colour (0xff1d2128)); g.fillRect (in.getX(), y0 + h * 0.5f, W, 1.0f);
            for (const auto& gn : v.graos)
            {
                const float x0 = in.getX() + W * gn.t0, w = juce::jmax (2.0f, W * gn.dur);
                const float yc = y0 + h * 0.5f + gn.pan * h * 0.42f;
                const juce::Colour c2 = gn.rev ? cAzul : (gn.oitava ? cTx : corM);
                g.setColour (c2.withAlpha (0.35f)); g.fillRoundedRectangle (x0, yc - 2.5f, juce::jmin (w, in.getRight() - x0), 5.0f, 2.5f);
                g.setColour (c2.withAlpha (0.9f)); g.fillRect (x0, yc - 2.5f, 1.2f, 5.0f);
            }
        }
    }
}

// PICOTE: QUANTO EFEITO · RITMO · TAMANHO · SAÍDA · EXPORTAR
void RetalhoEditor::controlesPicote (juce::Graphics& g, float y)
{
    const float h = 172;
    auto titulo = [&] (juce::Rectangle<float> r, const juce::String& t, const juce::String& s)
    {
        caixa (g, r, cPn, 9.0f);
        rotulo (g, { r.getX() + 14, r.getY() + 10, r.getWidth() - 20, 16 }, t, cTx, juce::Justification::centredLeft, 11.5f, true);
        rotulo (g, { r.getX() + 14, r.getY() + 27, r.getWidth() - 20, 13 }, s, cDim, juce::Justification::centredLeft, 9.0f);
    };
    // QUANTO EFEITO (POUCO / MÉDIO / MUITO sorteiam; MANUAL = só o que você marcar na onda)
    {
        const juce::Rectangle<float> r (12, y, 262, h);
        titulo (r, "QUANTO EFEITO", u8 ("ou s\xc3\xb3 os peda\xc3\xa7os que voc\xc3\xaa marcar"));
        const float bw = (r.getWidth() - 28 - 12) / 4.0f;
        for (int i = 0; i < 4; ++i)
            botao (g, { r.getX() + 14 + i * (bw + 4), y + 50, bw, 40 }, u8 (ch::quanto[i]), escolha (kQuanto) == i, cAmb,
                   [this, i] { proc.define (kQuanto, (float) i); }, 9.5f);
        const int lp = vista.valida && vista.modo == mPicote ? juce::roundToInt (vista.limpo * 100.0f) : 0;
        rotulo (g, { r.getX() + 14, y + 98, r.getWidth() - 28, 22 }, juce::String (lp) + u8 ("% DA VOZ LIMPA"), cTx, juce::Justification::centredLeft, 14.0f, true);
        const int nm = (int) proc.pedacos().size();
        if (nm > 0)
            botao (g, { r.getX() + 14, y + 128, r.getWidth() - 28, 30 }, u8 ("APAGAR MINHAS MARCAS (") + juce::String (nm) + ")", false, cAmb,
                   [this] { proc.limpaPedacos(); }, 9.5f);
        else
            rotulo (g, { r.getX() + 14, y + 128, r.getWidth() - 28, 30 }, u8 ("arraste na onda para marcar um peda\xc3\xa7o"), cDim, juce::Justification::centredLeft, 9.0f);
    }
    // RITMO
    {
        const juce::Rectangle<float> r (284, y, 330, h);
        titulo (r, u8 ("RITMO DAS REPETI\xc3\x87\xc3\x95" "ES"), u8 ("o tamanho de cada repeti\xc3\xa7\xc3\xa3o"));
        const float bw = (r.getWidth() - 28 - 16) / 5.0f;
        for (int i = 0; i < 5; ++i)
            botao (g, { r.getX() + 14 + i * (bw + 4), y + 50, bw, 46 }, ch::ritmo[i], escolha (kRitmo) == i, cAmb,
                   [this, i] { proc.define (kRitmo, (float) i); }, 12.0f);
        static const char* ex[5] = { "1/4: repete um tempo inteiro (lento)", ("1/8: duas repeti\xc3\xa7\xc3\xb5" "es por tempo"),
                                     "1/16: quatro por tempo (o cl\xc3\xa1ssico)", "1/32: oito por tempo (metralhadora)",
                                     "MISTO: cada efeito sorteia o seu" };
        rotulo (g, { r.getX() + 14, y + 112, r.getWidth() - 28, 16 }, u8 (ex[lim (escolha (kRitmo), 0, 4)]), cMu, juce::Justification::centredLeft, 10.0f);
    }
    // TAMANHO (1/2 até 16 BARS)
    {
        const juce::Rectangle<float> r (624, y, 190, h);
        titulo (r, "TAMANHO", "da frase, em compassos");
        const int l = escolha (kLength);
        const juce::Rectangle<float> rm (r.getX() + 14, y + 50, 38, 46), rp (r.getRight() - 52, y + 50, 38, 46);
        botao (g, rm, u8 ("\xe2\x88\x92"), false, cAmb, l > 0 ? std::function<void()> ([this, l] { proc.define (kLength, (float) (l - 1)); }) : std::function<void()>(), 18.0f);
        botao (g, rp, "+", false, cAmb, l < 5 ? std::function<void()> ([this, l] { proc.define (kLength, (float) (l + 1)); }) : std::function<void()>(), 18.0f);
        rotulo (g, { rm.getRight(), y + 50, rp.getX() - rm.getRight(), 46 }, ch::length[lim (l, 0, 5)], cAmb, juce::Justification::centred, 13.0f, true);
        const double seg = vista.valida ? (double) vista.len / juce::jmax (1.0, vista.fs) : 0.0;
        if (seg > 0.0)
            rotulo (g, { r.getX() + 14, y + 112, r.getWidth() - 28, 16 }, juce::String (seg, 1).replaceCharacter ('.', ',') + u8 (" s no projeto"), cMu, juce::Justification::centredLeft, 10.0f);
    }
    // SAÍDA
    {
        const juce::Rectangle<float> r (824, y, 170, h);
        titulo (r, u8 ("SA\xc3\x8d" "DA"), "");
        liga (g, { r.getX() + 14, y + 50, r.getWidth() - 28, 46 }, "FADE OUT", kFadeFim, cAmb);
        const bool st = real (kWidth) > 75.0f;
        botao (g, { r.getX() + 14, y + 104, r.getWidth() - 28, 46 }, u8 ("EST\xc3\x89REO"), st, cAmb,
               [this, st] { proc.define (kWidth, st ? 50.0f : 100.0f); }, 12.0f);
    }
    exportar (g, { 1004, y, 184, h });
}

// ESTICA: MOVIMENTO (knob grande + 6 direções + forma) · AJUSTE FINO (6 knobs que andam com o movimento + TAMANHO) · EXPORTAR
void RetalhoEditor::controlesEstica (juce::Graphics& g, float y)
{
    const float h = 172;
    const int dir = lim (escolha (kEDir), 0, numDirecoes - 1);
    const juce::Colour cd = corDir (dir);
    {
        const juce::Rectangle<float> r (12, y, 420, h);
        caixa (g, r, cPn, 9.0f);
        rotulo (g, { r.getX() + 14, y + 10, 200, 16 }, "MOVIMENTO", cTx, juce::Justification::centredLeft, 11.5f, true);
        // FORMA do movimento (clique troca; VARIAR sorteia)
        {
            const juce::String t = u8 ("FORMA: ") + u8 (ch::eforma[lim (escolha (kEForma), 0, 4)]) + u8 ("  \xe2\x80\xba");
            const float w = largura (fonte (9.0f, true).withExtraKerningFactor (0.22f), t) + 16;
            const juce::Rectangle<float> rf (r.getRight() - 14 - w, y + 8, w, 20);
            botao (g, rf, t, true, cd, [this] { proc.define (kEForma, (float) ((escolha (kEForma) + 1) % numFormas)); }, 9.0f);
        }
        const juce::Point<float> c (r.getCentreX(), y + 98);
        desenhaKnob (g, c, 40.0f, real (kEInt) / 100.0f, 0.0f, cd);
        knobs.push_back ({ kEInt, c, 40.0f });
        rotulo (g, { c.x - 40, c.y + 50, 80, 16 }, juce::String (juce::roundToInt (real (kEInt))) + "%", cTx, juce::Justification::centred, 12.0f, true);
        for (int i = 0; i < numDirecoes; ++i)
        {
            const bool esq = i < 3;
            const juce::Rectangle<float> rb (esq ? r.getX() + 14 : r.getRight() - 14 - 128, y + 40 + (i % 3) * 42.0f, 128, 34);
            const juce::Colour ci = corDir (i);
            const bool on = i == dir;
            g.setColour (on ? ci.withAlpha (0.16f) : cPn2); g.fillRoundedRectangle (rb, 6.0f);
            g.setColour (on ? ci : cLn); g.drawRoundedRectangle (rb.reduced (0.5f), 6.0f, on ? 1.5f : 1.0f);
            g.setColour (ci.withAlpha (on ? 1.0f : 0.6f)); g.fillEllipse (rb.getX() + 10, rb.getCentreY() - 4, 8, 8);
            rotulo (g, rb.withTrimmedLeft (24), u8 (ch::direcao[i]), on ? ci : cMu, juce::Justification::centredLeft, 10.0f, true);
            alvos.push_back ({ rb, [this, i] { proc.define (kEDir, (float) i); } });
            if (on)
            {
                const float xa = esq ? rb.getRight() : rb.getX(), xb = c.x + (esq ? -46.0f : 46.0f);
                g.setColour (ci.withAlpha (0.6f)); g.drawLine (xa, rb.getCentreY(), xb, c.y, 1.4f);
            }
        }
    }
    {
        const juce::Rectangle<float> r (442, y, 552, h);
        caixa (g, r, cPn, 9.0f);
        rotulo (g, { r.getX() + 14, y + 10, 200, 16 }, "AJUSTE FINO", cTx, juce::Justification::centredLeft, 11.5f, true);
        rotulo (g, { r.getX() + 120, y + 10, 330, 16 }, u8 ("o arco claro = onde o MOVIMENTO levou o knob agora"), cDim, juce::Justification::centredLeft, 8.5f);
        // onde o movimento está agora: no cursor (preview ou projeto); parado = o fim do caminho
        const Cena ce = proc.cenaAtual();
        const double comp = vista.valida ? (double) vista.len / juce::jmax (1.0, vista.spb * vista.bpb) : 1.0;
        const float pos = proc.previewTocando() == 2 ? proc.previewPos() : proc.posResultado();
        const double I = pos >= 0.0f ? movimentoEm (ce, pos, comp) : juce::jlimit (0.0, 1.0, real (kEInt) * 0.01);
        const EsticaEf ef = esticaEf (ce, I);
        struct K { int campo; const char* nome; const char* sub; double ef; };
        const K ks[6] = { { kEGrao, "GR\xc3\x83O", "tamanho", ef.grao }, { kEEspaco, "ESPA\xc3\x87O", "entre os gr\xc3\xa3os", ef.esp },
                          { kEVel, "VELOCIDADE", "anda na voz", ef.vel }, { kEEst, "EST\xc3\x89REO", "abre", ef.est },
                          { kEGrade, "GRADE", "no tempo", real (kEGrade) }, { kETom, "TOM", "altura", ef.tom } };
        const float kw = 456.0f / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            const float cx = r.getX() + 10 + kw * (i + 0.5f);
            {
                // nome do knob: sem espaçamento extra se não couber (VELOCIDADE saía "VELOCIDAD"); ainda grande demais, a fonte desce
                const juce::String nm = u8 (ks[i].nome);
                float kern = 0.22f, fh = 10.5f;
                if (largura (fonte (fh, true).withExtraKerningFactor (kern), nm) > kw - 6) kern = 0.0f;
                while (fh > 8.0f && largura (fonte (fh, true).withExtraKerningFactor (kern), nm) > kw - 6) fh -= 0.5f;
                g.setColour (cTx); g.setFont (fonte (fh, true).withExtraKerningFactor (kern));
                g.drawText (nm, juce::Rectangle<float> (cx - kw * 0.5f, y + 36, kw, 14), juce::Justification::centred, false);
            }
            rotulo (g, { cx - kw * 0.5f, y + 50, kw, 12 }, u8 (ks[i].sub), cDim, juce::Justification::centred, 8.5f);
            const juce::Point<float> c (cx, y + 98);
            auto* prm = proc.param (ks[i].campo);
            const bool bip = info (ks[i].campo).min < 0.0f && info (ks[i].campo).max > 0.0f;
            if (prm != nullptr)
            {
                desenhaKnob (g, c, 21.0f, prm->getValue(), bip ? prm->convertTo0to1 (0.0f) : 0.0f, cd);
                const auto& in = info (ks[i].campo);
                const float v1 = prm->convertTo0to1 (juce::jlimit (in.min, in.max, (float) ks[i].ef));
                arcoMov (g, c, 21.0f, prm->getValue(), v1, cd);
            }
            knobs.push_back ({ ks[i].campo, c, 21.0f });
            rotulo (g, { cx - kw * 0.5f, y + 132, kw, 16 }, textoV (ks[i].campo, (float) ks[i].ef), cTx, juce::Justification::centred, 11.0f, true);
        }
        // TAMANHO (coluna da direita)
        const float xt = r.getRight() - 84;
        g.setColour (cLn.withAlpha (0.7f)); g.drawLine (xt - 6, y + 36, xt - 6, y + h - 14, 1.0f);
        rotulo (g, { xt, y + 36, 72, 14 }, "TAMANHO", cTx, juce::Justification::centred, 10.5f, true);
        const int l = escolha (kLength);
        botao (g, { xt + 6, y + 56, 60, 26 }, "+", false, cd, l < 5 ? std::function<void()> ([this, l] { proc.define (kLength, (float) (l + 1)); }) : std::function<void()>(), 15.0f);
        rotulo (g, { xt - 4, y + 84, 80, 26 }, ch::length[lim (l, 0, 5)], cd, juce::Justification::centred, 11.5f, true);
        botao (g, { xt + 6, y + 112, 60, 26 }, u8 ("\xe2\x88\x92"), false, cd, l > 0 ? std::function<void()> ([this, l] { proc.define (kLength, (float) (l - 1)); }) : std::function<void()>(), 15.0f);
    }
    exportar (g, { 1004, y, 184, h });
}

// GROOVE: ESCALA (+ AFINAR) · TOM · RITMO (TAMANHO, SWING) · SAÍDA · EXPORTAR
void RetalhoEditor::controlesGroove (juce::Graphics& g, float y)
{
    const float h = 172;
    auto titulo = [&] (juce::Rectangle<float> r, const juce::String& t, const juce::String& s)
    {
        caixa (g, r, cPn, 9.0f);
        rotulo (g, { r.getX() + 14, r.getY() + 10, r.getWidth() - 20, 16 }, t, cTx, juce::Justification::centredLeft, 11.5f, true);
        rotulo (g, { r.getX() + 14, r.getY() + 27, r.getWidth() - 20, 13 }, s, cDim, juce::Justification::centredLeft, 9.0f);
    };
    // ESCALA
    {
        const juce::Rectangle<float> r (12, y, 300, h);
        titulo (r, "ESCALA", u8 ("as 8 linhas da grade"));
        const float bw = (r.getWidth() - 28 - 6) / 2.0f;
        for (int i = 0; i < 4; ++i)
            botao (g, { r.getX() + 14 + (i % 2) * (bw + 6), y + 48 + (i / 2) * 38.0f, bw, 32 }, u8 (ch::gesc[i]), escolha (kGEsc) == i, cGroove,
                   [this, i] { proc.define (kGEsc, (float) i); }, 10.0f);
        liga (g, { r.getX() + 14, y + 128, r.getWidth() - 28, 30 }, u8 ("AFINAR AS S\xc3\x8DLABAS NA ESCALA"), kGAfina, cGroove);
    }
    // TOM (transpõe tudo)
    {
        const juce::Rectangle<float> r (322, y, 200, h);
        const float base = vista.valida && vista.modo == mMelodia ? vista.baseMidi : -1.0f;
        titulo (r, "TOM", base >= 0.0f ? u8 ("nota base: ") + nomeNota (base) : u8 ("sobe ou desce tudo"));
        const int t = juce::roundToInt (real (kGPitch));
        const juce::Rectangle<float> rm (r.getX() + 14, y + 50, 38, 46), rp (r.getRight() - 52, y + 50, 38, 46);
        botao (g, rm, u8 ("\xe2\x88\x92"), false, cGroove, t > -12 ? std::function<void()> ([this, t] { proc.define (kGPitch, (float) (t - 1)); }) : std::function<void()>(), 18.0f);
        botao (g, rp, "+", false, cGroove, t < 12 ? std::function<void()> ([this, t] { proc.define (kGPitch, (float) (t + 1)); }) : std::function<void()>(), 18.0f);
        rotulo (g, { rm.getRight(), y + 50, rp.getX() - rm.getRight(), 46 }, sinal ((float) t, 0, " st"), cGroove, juce::Justification::centred, 13.0f, true);
        rotulo (g, { r.getX() + 14, y + 112, r.getWidth() - 28, 16 },
                base >= 0.0f ? u8 ("a linha de baixo toca ") + nomeNota (base + (float) t) : u8 ("sem altura clara na voz"), cMu, juce::Justification::centredLeft, 10.0f);
    }
    // RITMO: TAMANHO (1/2/4 BARS) e SWING
    {
        const juce::Rectangle<float> r (532, y, 282, h);
        titulo (r, "RITMO", u8 ("grade de 1/16"));
        const int l = juce::jlimit (1, 3, escolha (kLength));
        for (int i = 1; i <= 3; ++i)
            botao (g, { r.getX() + 14 + (i - 1) * 86.0f, y + 48, 80, 32 }, ch::length[i], l == i, cGroove,
                   [this, i]
                   {
                       // aumentar repete o que já está na grade (nada some)
                       const int antes = proc.passosGroove();
                       proc.define (kLength, (float) i);
                       const int depois = juce::jlimit (16, maxPassosG, (int) std::lround (compassosLength (i) * 16.0));
                       for (int k = antes; k < depois; ++k) proc.escreveNota (k, proc.nota (k % antes));
                   }, 10.0f);
        rotulo (g, { r.getX() + 14, y + 90, 54, 30 }, "SWING", cMu, juce::Justification::centredLeft, 9.5f);
        static const float sw[4] = { 0, 25, 50, 75 };
        const float atual = real (kGSwing);
        for (int i = 0; i < 4; ++i)
            botao (g, { r.getX() + 72 + i * 50.0f, y + 90, 46, 30 }, juce::String ((int) sw[i]) + "%", std::abs (atual - sw[i]) < 12.5f, cGroove,
                   [this, i] { proc.define (kGSwing, sw[i]); }, 10.0f);
        rotulo (g, { r.getX() + 14, y + 128, r.getWidth() - 28, 30 }, u8 ("aumentar repete o que j\xc3\xa1 est\xc3\xa1 na grade"), cDim, juce::Justification::centredLeft, 9.0f);
    }
    // SAÍDA
    {
        const juce::Rectangle<float> r (824, y, 170, h);
        titulo (r, u8 ("SA\xc3\x8d" "DA"), "");
        const bool junto = escolha (kMixVoz) == mvLayer;
        botao (g, { r.getX() + 14, y + 50, r.getWidth() - 28, 46 }, "VOZ JUNTO", junto, cGroove,
               [this, junto] { proc.define (kMixVoz, (float) (junto ? mvReplace : mvLayer)); }, 12.0f);
        liga (g, { r.getX() + 14, y + 104, r.getWidth() - 28, 46 }, "FADE OUT", kFadeFim, cGroove);
    }
    exportar (g, { 1004, y, 184, h });
}

void RetalhoEditor::exportar (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int md = escolha (kModo);
    const juce::Colour c = md == mEstica ? cCiano : (md == mMelodia ? cGroove : cAmb);
    caixa (g, r, cPn, 9.0f);
    rotulo (g, { r.getX() + 14, r.getY() + 10, r.getWidth() - 20, 16 }, "EXPORTAR", cTx, juce::Justification::centredLeft, 11.5f, true);
    const juce::Rectangle<float> rb (r.getX() + 14, r.getY() + 50, r.getWidth() - 28, 56);
    const bool pode = vista.valida;
    g.setColour (pode ? c : cPn2); g.fillRoundedRectangle (rb, 8.0f);
    rotulo (g, rb, "EXPORTAR WAV", pode ? juce::Colour (0xff141414) : cDim, juce::Justification::centred, 13.0f, true);
    if (pode)
        alvos.push_back ({ rb, [this]
        {
            const auto f = proc.exporta (false);
            avisoExport = f.existsAsFile() ? u8 ("SALVO EM DOCUMENTOS/RETALHO/EXPORTS") : u8 ("N\xc3\x83O CONSEGUI SALVAR");
            avisoExportQuadro = quadro;
        } });
    const bool aviso = avisoExport.isNotEmpty() && quadro - avisoExportQuadro < 150;
    g.setColour (aviso ? cVerde : cDim);
    g.setFont (fonte (9.0f));
    g.drawFittedText (aviso ? avisoExport : u8 ("WAV 24 bits \xc2\xb7 ou arraste a onda direto para uma faixa do DAW"),
                      juce::Rectangle<int> ((int) r.getX() + 14, (int) r.getY() + 114, (int) r.getWidth() - 28, 40), juce::Justification::topLeft, 3, 0.9f);
}

// PICOTE: escolha do efeito para o trecho marcado
void RetalhoEditor::menuPedaco()
{
    const int a = juce::jmin (selU0, selU1), b = juce::jmax (selU0, selU1) + 1;
    if (a < 0) return;
    juce::PopupMenu mn;
    mn.addSectionHeader (u8 ("ESTE PEDA\xc3\x87O (") + juce::String (b - a) + (b - a > 1 ? u8 (" colcheias)") : u8 (" colcheia)")));
    mn.addItem (100, u8 ("LIMPA (sem efeito)"));
    for (int t = 0; t < numPk; ++t) mn.addItem (101 + t, u8 (nomePk (t)));
    mn.addSeparator();
    mn.addItem (200, u8 ("Tirar a minha marca daqui (volta ao sorteio)"));
    mn.addItem (201, u8 ("Apagar todas as minhas marcas"), ! proc.pedacos().empty());
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, a, b] (int r)
    {
        if (safe == nullptr) return;
        if (r == 100) safe->proc.marcaPedaco (a, b, -1);
        else if (r > 100 && r <= 100 + numPk) safe->proc.marcaPedaco (a, b, r - 101);
        else if (r == 200) safe->proc.marcaPedaco (a, b, -2);
        else if (r == 201) safe->proc.limpaPedacos();
        safe->selU0 = safe->selU1 = -1;
        safe->repaint();
    });
}

// GROOVE: a sílaba e o tamanho de uma nota
void RetalhoEditor::menuNota (int k)
{
    const NotaG n = proc.nota (k);
    if (! n.on) return;
    juce::PopupMenu mn;
    const int ns = juce::jmax (1, vista.nSil);
    mn.addSectionHeader (u8 ("S\xc3\x8DLABA"));
    for (int s = 0; s < ns; ++s)
        mn.addItem (1 + s, juce::String (s + 1) + (s < vista.nSil && vista.sil[s].midi >= 0.0f ? u8 ("  \xc2\xb7 ") + nomeNota (vista.sil[s].midi) : juce::String()),
                    true, n.silaba % ns == s);
    mn.addSectionHeader (u8 ("TAMANHO"));
    static const int durs[5] = { 1, 2, 4, 8, 16 };
    static const char* nd[5] = { "1/16", "1/8", "1/4", "1/2", "1 BAR" };
    for (int i = 0; i < 5; ++i) mn.addItem (20 + i, nd[i], true, n.dur == durs[i]);
    mn.addSeparator();
    mn.addItem (30, u8 ("Apagar a nota"));
    juce::Component::SafePointer<RetalhoEditor> safe (this);
    mn.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, k] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        NotaG x = safe->proc.nota (k);
        if (r < 20) x.silaba = r - 1;
        else if (r < 30) x.dur = durs[r - 20];
        else x = NotaG();
        safe->proc.escreveNota (k, x);
        safe->repaint();
    });
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
            rotuloQueCabe (g, w, { "CAPTURA ARMADA: COME\xc3\x87" "A NO PR\xc3\x93XIMO COMPASSO COM O PROJETO TOCANDO",
                                   "CAPTURA ARMADA: D\xc3\x8a PLAY NO PROJETO",
                                   "ARMADA: D\xc3\x8a PLAY" }, cRosa.withAlpha (pulso), 10.5f, true);
        return;
    }
    if (! tem)
    {
        g.setColour (cLn); g.drawRoundedRectangle (w.reduced (2), 5.0f, 1.0f);
        if (avisoArquivo.isNotEmpty() && quadro - avisoQuadro < 120)
        {
            const auto t = avisoArquivo.toStdString();
            rotuloQueCabe (g, w, { t.c_str() }, cRosa, 10.5f, true);
        }
        else if (proc.capVazia.load())
            rotuloQueCabe (g, w, { "A CAPTURA S\xc3\x93 PEGOU SIL\xc3\x8aNCIO: CAPTURE NUM TRECHO COM VOZ", "S\xc3\x93 SIL\xc3\x8aNCIO: CAPTURE COM VOZ" }, cRosa, 10.5f, true);
        else
            rotuloQueCabe (g, w, { "ARRASTE AQUI O ARQUIVO DO VOCAL OU CLIQUE EM CAPTURAR PARA GRAVAR A FAIXA",
                                   "ARRASTE O VOCAL AQUI OU CLIQUE EM CAPTURAR", "ARRASTE O VOCAL AQUI" }, cMu, 10.5f, true);
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
    g.setColour (juce::Colours::black.withAlpha (0.78f)); g.fillRect (w.getX() + 1, w.getBottom() - 14, w.getWidth() - 2, 13.0f);   // fundo: o texto não briga com a onda nem com o marcador
    rotulo (g, { w.getX() + 6, w.getBottom() - 13, w.getWidth() - 12, 12 },
            u8 ("O EFEITO COME\xc3\x87" "A NO \xe2\x96\xbc \xc2\xb7 ARRASTE NA ONDA = TRECHO \xc2\xb7 DUPLO CLIQUE = INTEIRA"), cDim, juce::Justification::centredRight, 8.0f);
}
