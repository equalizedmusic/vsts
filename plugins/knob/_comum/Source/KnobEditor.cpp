#include "KnobEditor.h"
#include "Mapa.h"
#include "BinaryData_SerieKnob.h"

using namespace sk;

namespace
{
constexpr float W = mapa::W, H = mapa::H;
// a janela mostra só o pedal: o render tem em volta o fundo do estúdio (cinza), que fica de fora.
// Recorte medido no fundo.jpg (a caixa de madeira vai de x 58–941 embaixo a 74–924 em cima, y 108–1201)
constexpr float CX = 76.0f, CY = 109.0f, CW = 848.0f, CH = 1092.0f;
constexpr int baseLargura = 424;   // 50% do recorte (o pedal no mesmo tamanho de antes)
const juce::Colour creme (0xffede3cc);

juce::Image carregar (const juce::String& nome)
{
    int tam = 0;
    if (auto* dados = SerieKnobData::getNamedResource (nome.replace (".", "_").toRawUTF8(), tam))
        return juce::ImageCache::getFromMemory (dados, tam);
    return {};
}

juce::Rectangle<float> ret (const mapa::R& r)
{
    return { (float) r.x0, (float) r.y0, (float) (r.x1 - r.x0), (float) (r.y1 - r.y0) };
}

// ponto de uma peça redonda que gira (coordenadas locais em unidades do Blender, y para cima) -> pixel da imagem
juce::Point<float> projetar (const mapa::E& e, float x, float y, float angRad)
{
    const float c = std::cos (angRad), s = std::sin (angRad);
    const float X = x * c - y * s, Y = x * s + y * c;
    return { e.x + X * e.rx, e.y - Y * e.ry };
}

// quadro do knob para um giro (as caneluras se repetem a cada 360/caneluras graus)
int quadro (float anguloGraus, int caneluras)
{
    const float periodo = 360.0f / (float) caneluras;
    float f = std::fmod (anguloGraus, periodo);
    if (f < 0) f += periodo;
    return juce::roundToInt (f / periodo * 16.0f) % 16;
}

juce::Font fonte (float h, bool negrito = true)
{
    return juce::Font (juce::FontOptions (h, negrito ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (0.04f);
}
} // namespace

KnobEditor::KnobEditor (KnobProcessor& p)
    : AudioProcessorEditor (&p), proc (p), cor (juce::Colour ((juce::uint32) (0xff000000u | info().cor)))
{
    fundo = carregar ("fundo.jpg");
    nome = carregar (juce::String ("nome_") + info().chave + ".png");
    for (int i = 0; i < 16; ++i)
    {
        const auto q = juce::String (i).paddedLeft ('0', 2);
        knob[i] = carregar ("knob_" + q + ".png");
        rate[i] = carregar ("rate_" + q + ".png");
        mix[i]  = carregar ("mix_" + q + ".png");
    }
    alav[0] = carregar ("alav_int.png");
    alav[1] = carregar ("alav_midi.png");
    alav[2] = carregar ("alav_audio.png");

    limites.setFixedAspectRatio (CW / CH);
    limites.setSizeLimits ((int) (baseLargura * 0.6f), (int) (baseLargura * 0.6f * CH / CW),
                           (int) (baseLargura * 2.0f), (int) (baseLargura * 2.0f * CH / CW));
    canto = std::make_unique<juce::ResizableCornerComponent> (this, &limites);
    addAndMakeVisible (*canto);
    setResizable (true, false);
    setConstrainer (&limites);
    const float k = proc.uiScale.load();
    setSize (juce::roundToInt (baseLargura * k), juce::roundToInt (baseLargura * k * CH / CW));
    disparosVistos = proc.disparos.load();
    startTimerHz (30);
    ativacao.anexar (*this);
}

KnobEditor::~KnobEditor() { stopTimer(); }

void KnobEditor::resized()
{
    canto->setBounds (getWidth() - 16, getHeight() - 16, 16, 16);
    proc.uiScale.store ((float) getWidth() / (float) baseLargura);
    escalaEstatico = 0.0f;   // refaz a camada fixa no tamanho novo
}

juce::Point<float> KnobEditor::naImagem (juce::Point<float> p) const
{
    return p * (CW / (float) juce::jmax (1, getWidth())) + juce::Point<float> (CX, CY);
}

float KnobEditor::valorDe (const juce::String& pid) const
{
    return proc.apvts.getRawParameterValue (pid)->load();
}

void KnobEditor::definir (const juce::String& pid, float v)
{
    if (auto* rp = proc.apvts.getParameter (pid))
    {
        const bool solto = (paramArrasto != rp);
        if (solto) rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        if (solto) rp->endChangeGesture();
    }
}

// fundo + plaquinha do nome já na escala da tela: o paint só copia
void KnobEditor::montarEstatico()
{
    const float escala = juce::Component::getApproximateScaleFactorForComponent (this) * (float) getWidth() / CW;
    if (std::abs (escala - escalaEstatico) < 1.0e-4f && estatico.isValid()) return;
    escalaEstatico = escala;
    estatico = juce::Image (juce::Image::RGB, juce::jmax (1, juce::roundToInt (CW * escala)), juce::jmax (1, juce::roundToInt (CH * escala)), true);
    juce::Graphics g (estatico);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.addTransform (juce::AffineTransform::translation (-CX, -CY).scaled (escala));
    if (fundo.isValid()) g.drawImage (fundo, { 0, 0, W, H });
    else g.fillAll (juce::Colour (0xff1c1c1c));
    if (nome.isValid()) g.drawImage (nome, ret (mapa::retNome));
}

void KnobEditor::paint (juce::Graphics& g)
{
    montarEstatico();
    g.setOpacity (1.0f);
    g.drawImage (estatico, getLocalBounds().toFloat());

    g.saveState();
    g.addTransform (juce::AffineTransform::translation (-CX, -CY).scaled ((float) getWidth() / CW));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    const float valor = valorDe (id::valor);
    desenharLampadas (g, valor, pulso);
    desenharKnobGrande (g, valor);
    const float rateIdx = valorDe (id::rate);
    desenharKnobPequeno (g, rate, 135.0f - 45.0f * rateIdx, true);
    desenharKnobPequeno (g, mix, 135.0f - 2.7f * valorDe (id::mix), false);
    const int gat = juce::jlimit (0, 2, juce::roundToInt (valorDe (id::gatilho)));
    g.setOpacity (1.0f);
    if (alav[gat].isValid()) g.drawImage (alav[gat], ret (mapa::retAlav));
    desenharVisor (g);
    desenharTextos (g);
    g.restoreState();
}

void KnobEditor::desenharLampadas (juce::Graphics& g, float valor, float respira)
{
    for (int i = 0; i < 11; ++i)
    {
        const auto& e = mapa::lamp[i];
        const float b = juce::jlimit (0.0f, 1.0f, valor - (float) i + 1.0f);
        const juce::Point<float> c (e.x, e.y);
        // apagada: só o vidro tingido
        {
            juce::ColourGradient vg (cor.withAlpha (0.38f), e.x - e.rx * 0.3f, e.y - e.ry * 0.3f,
                                     cor.withMultipliedBrightness (0.25f).withAlpha (0.55f), e.x + e.rx, e.y + e.ry, true);
            g.setGradientFill (vg);
            g.fillEllipse (e.x - e.rx, e.y - e.ry, e.rx * 2, e.ry * 2);
        }
        if (b <= 0.0f) continue;
        const float forca = b * (0.72f + 0.28f * respira);
        // halo no metal em volta
        juce::ColourGradient halo (cor.withAlpha (0.42f * forca), c, cor.withAlpha (0.0f), c.translated (e.rx * 3.4f, 0), true);
        g.setGradientFill (halo);
        g.fillEllipse (e.x - e.rx * 3.4f, e.y - e.ry * 3.4f, e.rx * 6.8f, e.ry * 6.8f);
        // lente acesa: miolo quase branco, borda na cor
        juce::ColourGradient lente (cor.interpolatedWith (juce::Colours::white, 0.65f).withAlpha (forca), e.x - e.rx * 0.15f, e.y - e.ry * 0.2f,
                                    cor.withAlpha (forca), e.x + e.rx * 0.95f, e.y, true);
        lente.addColour (0.55, cor.interpolatedWith (juce::Colours::white, 0.2f).withAlpha (forca));
        g.setGradientFill (lente);
        g.fillEllipse (e.x - e.rx * 0.98f, e.y - e.ry * 0.98f, e.rx * 1.96f, e.ry * 1.96f);
        // reflexo da luz do estúdio
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.fillEllipse (e.x - e.rx * 0.5f, e.y - e.ry * 0.62f, e.rx * 0.42f, e.ry * 0.3f);
    }
}

void KnobEditor::desenharKnobGrande (juce::Graphics& g, float valor)
{
    const float angGraus = 135.0f - 27.0f * valor;   // giro no Blender (anti-horário visto de cima)
    const int q = quadro (angGraus, 18);
    g.setOpacity (1.0f);   // o drawImage herda a transparência da última cor (as lâmpadas): sem isso o knob sai translúcido
    if (knob[q].isValid()) g.drawImage (knob[q], ret (mapa::retKnob));

    const float a = juce::degreesToRadians (angGraus);
    const float R = 6.1f;
    // traço gravado na saia
    {
        const auto p0 = projetar (mapa::knobSaia, 0.0f, R * 1.03f, a), p1 = projetar (mapa::knobSaia, 0.0f, R * 1.19f, a);
        g.setColour (juce::Colour (0xff0b0b0c));
        g.drawLine ({ p0, p1 }, 0.18f * mapa::un);
    }
    // linha creme da tampa
    {
        const auto p0 = projetar (mapa::knobTampa, 0.0f, R * 0.44f, a), p1 = projetar (mapa::knobTampa, 0.0f, R * 0.56f, a);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine ({ p0.translated (1.2f, 1.6f), p1.translated (1.2f, 1.6f) }, 0.2f * mapa::un);
        g.setColour (creme);
        g.drawLine ({ p0, p1 }, 0.2f * mapa::un);
    }
    // emblema em relevo, gira junto
    const float tam = R * 0.38f;
    const float esp = 2.0f * 0.085f * tam * mapa::un;
    juce::Path traco;
    for (const auto& ln : info().emblema)
    {
        juce::Path p;
        for (size_t k = 0; k < ln.size(); ++k)
        {
            const auto pt = projetar (mapa::knobTampa, ln[k].first * tam, ln[k].second * tam, a);
            if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        traco.addPath (p);
    }
    const juce::PathStrokeType st (esp, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path cheio;
    st.createStrokedPath (cheio, traco);
    const auto cx = mapa::knobTampa.x, cy = mapa::knobTampa.y, r = R * 0.6f * mapa::knobTampa.rx;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillPath (cheio, juce::AffineTransform::translation (1.3f, 2.0f));
    juce::ColourGradient metal (juce::Colour (0xfff4f4f6), cx - r * 0.6f, cy - r * 0.7f, juce::Colour (0xff6e6e74), cx + r * 0.6f, cy + r * 0.7f, false);
    metal.addColour (0.5, juce::Colour (0xffb8b8be));
    g.setGradientFill (metal);
    g.fillPath (cheio);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.strokePath (traco, juce::PathStrokeType (esp * 0.28f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (-esp * 0.15f, -esp * 0.18f));
}

void KnobEditor::desenharKnobPequeno (juce::Graphics& g, const juce::Image* quadros, float angGraus, bool ehRate)
{
    const int q = quadro (angGraus, 14);
    g.setOpacity (1.0f);
    if (quadros[q].isValid()) g.drawImage (quadros[q], ret (ehRate ? mapa::retRate : mapa::retMix));
    const auto& e = ehRate ? mapa::rateTampa : mapa::mixTampa;
    const float a = juce::degreesToRadians (angGraus), R = 1.45f;
    const auto p0 = projetar (e, 0.0f, R * 0.16f, a), p1 = projetar (e, 0.0f, R * 0.56f, a);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine ({ p0.translated (0.8f, 1.1f), p1.translated (0.8f, 1.1f) }, 0.2f * mapa::un);
    g.setColour (creme);
    g.drawLine ({ p0, p1 }, 0.2f * mapa::un);
}

void KnobEditor::desenharVisor (juce::Graphics& g)
{
    const juce::Rectangle<float> v (mapa::visor[0], mapa::visor[1], mapa::visor[2] - mapa::visor[0], mapa::visor[3] - mapa::visor[1]);
    const auto area = v.reduced (v.getWidth() * 0.05f, v.getHeight() * 0.12f);
    const auto& in = info();
    const int forma = juce::jlimit (0, (int) in.formas.size() - 1, juce::roundToInt (valorDe (id::forma)));
    const double rateB = in.rates[(size_t) juce::jlimit (0, 6, juce::roundToInt (valorDe (id::rate)))];
    const double comp = proc.compassoAtual.load();
    const double per = in.periodoVisor (rateB, comp);
    const float valor = valorDe (id::valor) * 0.1f;

    // grade discreta (como num osciloscópio)
    g.setColour (cor.withAlpha (0.08f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine (juce::roundToInt (area.getY() + area.getHeight() * i / 4.0f), area.getX(), area.getRight());
    for (int i = 1; i < 8; ++i) g.drawVerticalLine (juce::roundToInt (area.getX() + area.getWidth() * i / 8.0f), area.getY(), area.getBottom());

    juce::Path curva;
    const int N = 220;
    for (int k = 0; k <= N; ++k)
    {
        const double x = per * k / N * 0.99999;
        const float y = juce::jlimit (0.0f, 1.0f, in.desenho (forma, valor, x, rateB, comp));
        const juce::Point<float> p (area.getX() + area.getWidth() * k / N, area.getBottom() - area.getHeight() * y);
        if (k == 0) curva.startNewSubPath (p); else curva.lineTo (p);
    }
    g.setColour (cor.withAlpha (0.18f));
    g.strokePath (curva, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cor.withAlpha (0.35f));
    g.strokePath (curva, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cor.interpolatedWith (juce::Colours::white, 0.25f));
    g.strokePath (curva, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // cursor: onde o som está agora
    const double pa = proc.posAtual.load();
    const bool parado = (proc.disparos.load() == 0 && valorDe (id::gatilho) > 0.5f);
    if (! parado && pa < 1.0e5)
    {
        const float fx = (float) frac (pa / per);
        const float x = area.getX() + area.getWidth() * fx;
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.4f);
        const float y = area.getBottom() - area.getHeight() * juce::jlimit (0.0f, 1.0f, in.desenho (forma, valor, fx * per, rateB, comp));
        g.setColour (cor.withAlpha (0.4f));
        g.fillEllipse (x - 7, y - 7, 14, 14);
        g.setColour (juce::Colours::white);
        g.fillEllipse (x - 3, y - 3, 6, 6);
    }
    // reflexo do vidro
    juce::ColourGradient reflexo (juce::Colours::white.withAlpha (0.07f), v.getX(), v.getY(), juce::Colours::transparentWhite, v.getX(), v.getCentreY(), false);
    g.setGradientFill (reflexo);
    g.fillRect (v.withHeight (v.getHeight() * 0.5f));
}

void KnobEditor::desenharTextos (juce::Graphics& g)
{
    const auto& in = info();
    const int gat = juce::jlimit (0, 2, juce::roundToInt (valorDe (id::gatilho)));
    juce::String tg;
    if (gat == gatInt) tg = proc.tocando.load() ? "tempo do DAW" : "livre (DAW parado)";
    else if (gat == gatMidi) tg = juce::String::fromUTF8 ("MIDI: nota dispara");
    else tg = proc.scConectado.load() ? "SC: sidechain" : juce::String::fromUTF8 ("SC: a própria entrada");

    const int ri = juce::jlimit (0, 6, juce::roundToInt (valorDe (id::rate)));
    const auto rt = juce::String::fromUTF8 (in.rateNomes[(size_t) ri]) + juce::String::fromUTF8 (" · ")
                    + juce::String (juce::roundToInt (proc.bpmAtual.load())) + " BPM";
    const int fi = juce::jlimit (0, (int) in.formas.size() - 1, juce::roundToInt (valorDe (id::forma)));
    const auto fm = juce::String::fromUTF8 (in.formas[(size_t) fi]);
    const auto mx = juce::String (juce::roundToInt (valorDe (id::mix))) + "%";

    g.setFont (fonte (20.0f));
    auto escrever = [&] (const mapa::P& p, const juce::String& s, float largura)
    {
        g.setColour (creme);
        g.drawFittedText (s, juce::Rectangle<float> (p.x - largura / 2, p.y - 13, largura, 26).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    };
    escrever (mapa::txtTrigger, tg, 190.0f);
    escrever (mapa::txtRate, rt, 160.0f);
    escrever (mapa::txtShape, fm, 170.0f);
    escrever (mapa::txtMix, mx, 110.0f);

    // LED do TRIGGER: pisca a cada nota/ataque (no INT, a cada tempo)
    const float lx = mapa::txtTrigger.x - 2.55f * mapa::un, ly = mapa::txtTrigger.y - 5.75f * mapa::un;
    float acende = flash;
    if (gat == gatInt) acende = (float) juce::jmax (0.0, 1.0 - frac (proc.posAtual.load()) * 5.0);
    g.setColour (juce::Colour (0xff1a0d0d));
    g.fillEllipse (lx - 6, ly - 6, 12, 12);
    if (acende > 0.01f)
    {
        g.setColour (cor.withAlpha (0.35f * acende));
        g.fillEllipse (lx - 14, ly - 14, 28, 28);
        g.setColour (cor.interpolatedWith (juce::Colours::white, 0.4f).withAlpha (acende));
        g.fillEllipse (lx - 5, ly - 5, 10, 10);
    }
}

void KnobEditor::timerCallback()
{
    const int d = proc.disparos.load();
    if (d != disparosVistos) { disparosVistos = d; flash = 1.0f; }
    else flash *= 0.8f;
    pulso += 0.35f * (proc.nivel.load() - pulso);
    repaint();
}

// ---------------- mouse ----------------

KnobEditor::Alvo KnobEditor::alvoEm (juce::Point<float> p) const
{
    auto dentro = [&] (const mapa::E& e, float raioUn) { return p.getDistanceFrom ({ e.x, e.y }) < raioUn * mapa::un; };
    if (dentro (mapa::knobBase, 7.6f)) return alvoKnob;
    if (dentro (mapa::rateTampa, 2.3f)) return alvoRate;
    if (dentro (mapa::mixTampa, 2.3f)) return alvoMix;
    const juce::Rectangle<float> v (mapa::visor[0], mapa::visor[1], mapa::visor[2] - mapa::visor[0], mapa::visor[3] - mapa::visor[1]);
    if (v.expanded (8).contains (p)) return alvoVisor;
    if (ret (mapa::retAlav).expanded (30, 0).contains (p)) return alvoAlavanca;
    return nenhum;
}

void KnobEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto a = alvoEm (naImagem (e.position));
    setMouseCursor (a == nenhum ? juce::MouseCursor::NormalCursor
                    : (a == alvoVisor || a == alvoAlavanca) ? juce::MouseCursor::PointingHandCursor
                                                            : juce::MouseCursor::UpDownResizeCursor);
}

void KnobEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = naImagem (e.position);
    const auto a = alvoEm (p);
    arrastando = nenhum;
    if (a == alvoVisor)
    {
        if (e.mods.isPopupMenu()) { menuForma(); return; }
        const int n = (int) info().formas.size();
        const juce::Rectangle<float> v (mapa::visor[0], mapa::visor[1], mapa::visor[2] - mapa::visor[0], mapa::visor[3] - mapa::visor[1]);
        const int atual = juce::roundToInt (valorDe (id::forma));
        definir (id::forma, (float) ((atual + (p.x < v.getCentreX() ? n - 1 : 1)) % n));
        return;
    }
    if (a == alvoAlavanca)
    {
        const float dx = (p.x - mapa::alavanca.x) / mapa::un;
        const float dy = (p.y - mapa::alavanca.y) / mapa::un;
        int gat = juce::roundToInt (valorDe (id::gatilho));
        if (dx < -0.9f) gat = gatInt;
        else if (dx > 0.9f) gat = gatAudio;
        else if (dy < -0.4f) gat = gatMidi;
        else gat = (gat + 1) % 3;
        definir (id::gatilho, (float) gat);
        return;
    }
    if (a == alvoRate && e.mods.isPopupMenu()) { menuRate(); return; }
    if (a == nenhum && e.mods.isPopupMenu()) { menuPedal(); return; }   // botão direito fora dos controles
    if (a == alvoKnob || a == alvoRate || a == alvoMix)
    {
        arrastando = a;
        const auto pid = a == alvoKnob ? id::valor : a == alvoRate ? id::rate : id::mix;
        paramArrasto = proc.apvts.getParameter (pid);
        paramArrasto->beginChangeGesture();
        valorInicio = valorDe (pid);
        inicio = e.position;
    }
}

void KnobEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (arrastando == nenhum || paramArrasto == nullptr) return;
    const float dy = (inicio.y - e.position.y) + (e.position.x - inicio.x) * 0.35f;
    const float fino = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float px = juce::jmax (1.0f, (float) getHeight() * 0.42f);   // o curso inteiro = ~42% da altura da janela
    if (arrastando == alvoKnob)
        definir (id::valor, juce::jlimit (0.0f, 10.0f, valorInicio + dy / px * 10.0f * fino));
    else if (arrastando == alvoMix)
        definir (id::mix, juce::jlimit (0.0f, 100.0f, valorInicio + dy / px * 100.0f * fino));
    else
        definir (id::rate, (float) juce::jlimit (0, 6, juce::roundToInt (valorInicio + dy / (px * 0.5f) * 6.0f)));
}

void KnobEditor::mouseUp (const juce::MouseEvent&)
{
    if (paramArrasto != nullptr) paramArrasto->endChangeGesture();
    paramArrasto = nullptr;
    arrastando = nenhum;
}

void KnobEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto a = alvoEm (naImagem (e.position));
    const auto& in = info();
    if (a == alvoKnob) definir (id::valor, in.valorPadrao);
    else if (a == alvoMix) definir (id::mix, in.mixPadrao);
    else if (a == alvoRate) definir (id::rate, (float) in.ratePadrao);
}

void KnobEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto a = alvoEm (naImagem (e.position));
    const float d = (w.isReversed ? -w.deltaY : w.deltaY);
    if (a == alvoKnob) definir (id::valor, juce::jlimit (0.0f, 10.0f, valorDe (id::valor) + d * (e.mods.isShiftDown() ? 0.5f : 2.0f)));
    else if (a == alvoMix) definir (id::mix, juce::jlimit (0.0f, 100.0f, valorDe (id::mix) + d * 20.0f));
    else if (a == alvoRate && std::abs (d) > 0.01f) definir (id::rate, (float) juce::jlimit (0, 6, juce::roundToInt (valorDe (id::rate)) + (d > 0 ? 1 : -1)));
    else if (a == alvoVisor && std::abs (d) > 0.01f)
    {
        const int n = (int) info().formas.size();
        definir (id::forma, (float) ((juce::roundToInt (valorDe (id::forma)) + (d > 0 ? 1 : n - 1)) % n));
    }
}

void KnobEditor::menuForma()
{
    juce::PopupMenu m;
    const int atual = juce::roundToInt (valorDe (id::forma));
    for (int i = 0; i < (int) info().formas.size(); ++i)
        m.addItem (i + 1, juce::String::fromUTF8 (info().formas[(size_t) i]), true, i == atual);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [this] (int r) { if (r > 0) definir (id::forma, (float) (r - 1)); });
}

void KnobEditor::menuRate()
{
    juce::PopupMenu m;
    const int atual = juce::roundToInt (valorDe (id::rate));
    for (int i = 0; i < 7; ++i)
        m.addItem (i + 1, juce::String::fromUTF8 (info().rateNomes[(size_t) i]), true, i == atual);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [this] (int r) { if (r > 0) definir (id::rate, (float) (r - 1)); });
}

void KnobEditor::menuPedal()
{
    juce::PopupMenu m;
    m.addItem (1, juce::String::fromUTF8 ("Licença…"));
    juce::Component::SafePointer<KnobEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe] (int r) { if (r == 1 && safe != nullptr) safe->ativacao.mostrarInfo(); });
}
