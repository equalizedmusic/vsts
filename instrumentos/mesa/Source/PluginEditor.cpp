#include "PluginEditor.h"
#include "Desenho.h"
#include <algorithm>

using namespace mesa;
using namespace mesa::des;

namespace
{
constexpr int menuLicenca = 20007, menuRenomear = 20008, menuSobre = 20009, menuPosicao = 20010;

// tela base (px): canal e linhas — as mesmas medidas do protótipo mockup/mesa-v4
constexpr float SW = 84;
constexpr float rHead = 74, rInp = 110, rDyn = 150, rEq = 262, rOut = 30, rCor = 6, rNome = 20;
constexpr float NAV = 132, TOP = 44, LBL = 52, CEN = 404, BASE_H = 1000;

// ações (botões que não são parâmetro)
enum Acao { aAutoSel = 1, aCores, aPadrao, aGeral, aVu, aPpm, aEco, aHq, aAnt = 10, aProx, aZerar, aBarra = 20, aLimpaSolo = 30, aRolagem = 40,
            aZoomMenos = 50, aZoomMais, aZoom100 };
const float zooms[] = { 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.4f, 1.6f, 1.8f, 2.0f };

const char* const pidsCanal[] = { "mode", "grp", "pre", "gain", "drive", "pad", "hpf", "eqon", "eqm", "dynpre", "hfg", "hmfg", "lmfg", "lfg",
                                  "hff", "hmff", "lmff", "lff", "thr", "rat", "gate", "cmix", "pan", "mute", "solo", "fader", "hq" };
const char* const pidsConsole[] = { "fader", "mthr", "matk", "mrel", "mmkp", "mrat", "mmix", "min", "mhpf" };

juce::String nomeDe (const Canal& c)
{
    char b[160] {};
    c.nome (b, (int) sizeof (b));
    return juce::String::fromUTF8 (b);
}
juce::String textoDe (const Canal& c, const std::string& pid)
{
    char b[96] {};
    c.texto (pid.c_str(), b, (int) sizeof (b));
    return juce::String::fromUTF8 (b);
}
int inteiro (const Canal& c, const char* pid) { return juce::roundToInt (c.valor (pid)); }
// número no LED: a posição da faixa no DAW (se não se sabe, a ordem em que o canal foi aberto)
int noLed (const Canal& c) { const int p = c.posicao(); return p > 0 ? p : c.numero(); }
juce::String tres (int n) { return juce::String (juce::jlimit (0, 999, n)).paddedLeft ('0', 3); }
juce::String db1 (float v) { return v < -99.0f ? juce::String ("--") : (v > 0 ? "+" : "") + juce::String (v, 1); }
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

// redução de ganho (dB) -> posição da agulha na escala de GR (0 à direita)
float grPos (float gr)
{
    static const float d[] = { 0, 3, 6, 10, 20 }, p[] = { 1.0f, 0.8f, 0.6f, 0.35f, 0.0f };
    gr = juce::jmax (0.0f, gr);
    for (int i = 0; i < 4; ++i)
        if (gr <= d[i + 1]) return juce::jmap (gr, d[i], d[i + 1], p[i], p[i + 1]);
    return -0.03f;
}
} // namespace

// =====================================================================================================================
MesaTela::MesaTela (MesaProcessor& p, int w, int h) : AudioProcessorEditor (&p), proc (p), baseW (w), baseH (h)
{
    setOpaque (true);
}

MesaTela::~MesaTela() { stopTimer(); }

void MesaTela::timerCallback()
{
    mede();
    std::vector<float> s;
    s.reserve (ultima.size() + 16);
    s.push_back ((float) getWidth()); s.push_back ((float) getHeight());
    assinatura (s);
    if (s != ultima) { ultima.swap (s); fixaSuja = true; }
    repaint();
}

void MesaTela::paint (juce::Graphics& g)
{
    const float esc = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int iw = juce::jmax (1, juce::roundToInt ((float) getWidth() * esc)), ih = juce::jmax (1, juce::roundToInt ((float) getHeight() * esc));
    if (fixaSuja || ! fixaImg.isValid() || fixaImg.getWidth() != iw || fixaImg.getHeight() != ih)
    {
        fixaImg = juce::Image (juce::Image::RGB, iw, ih, true);
        juce::Graphics gi (fixaImg);
        gi.addTransform (base().scaled (esc));
        alvos.clear();
        pintaFixa (gi);
        fixaSuja = false;
    }
    g.setOpacity (1.0f);
    g.drawImage (fixaImg, getLocalBounds().toFloat());
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (base());
    pintaViva (g);
    if (mostraValor && valorTexto.isNotEmpty())
    {
        const float w = juce::GlyphArrangement::getStringWidth (fonte (9.0f), valorTexto) + 14.0f;
        auto r = juce::Rectangle<float> (valorPos.x - w / 2, valorPos.y - 30, w, 18);
        g.setColour (Colour (0xe0101113u)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (acAzul.withAlpha (0.8f)); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        txt (g, valorTexto, r.getCentreX(), r.getCentreY(), 9.0f, hex (0xf0f2f4));
    }
}

const MesaTela::Alvo* MesaTela::acha (juce::Point<float> p) const
{
    for (auto it = alvos.rbegin(); it != alvos.rend(); ++it)
        if (it->r.contains (p)) return &*it;
    return nullptr;
}

void MesaTela::atualizaMed (Canal* c, Med& m, bool ppm)
{
    // a balística (pico com queda, VU de 300 ms, PPM) já vem pronta do áudio; aqui só a inércia da agulha
    float o[8];
    c->medidores (o);
    m.outL = o[0]; m.outR = o[1]; m.out = juce::jmax (o[0], o[1]);
    m.in = o[6];
    m.gr = o[4] > m.gr ? o[4] : juce::jmax (o[4], m.gr - 0.6f);
    const float alvoOut = (ppm ? o[7] : juce::jmax (o[2], o[3])) + 18.0f;    // 0 VU = -18 dBFS
    m.vuOut += (alvoOut - m.vuOut) * 0.6f;
    m.vuIn += (o[5] + 18.0f - m.vuIn) * 0.6f;
    m.vuGr += (o[4] - m.vuGr) * (o[4] > m.vuGr ? 0.6f : 0.3f);
}

// ---------------------------------------------------------------------------------------------------------- mouse
void MesaTela::mouseDown (const juce::MouseEvent& e)
{
    mouseBase = paraBase (e.position);
    if (e.mods.isPopupMenu()) { menu(); return; }
    const auto* a = acha (mouseBase);
    if (a == nullptr) return;
    const Alvo al = *a;
    if (al.c != nullptr && ! existe (al.c)) return;
    if (al.tipo == Alvo::acao) { acao (al.codigo, e); return; }
    if (al.c == nullptr) return;
    selecionou (al.c);
    auto* c = al.c;
    const char* pid = al.pid.c_str();
    switch (al.tipo)
    {
        case Alvo::knob:
        case Alvo::fader:
            temAtivo = true; ativo = al;
            arrastoIni = mouseBase.y; valorIni = c->norm (pid);
            c->gesto (pid, true);
            mostraValor = true; valorTexto = textoDe (*c, al.pid); valorPos = { al.r.getCentreX(), al.r.getY() + 6 };
            break;
        case Alvo::liga:
            c->gesto (pid, true);
            c->mexe (pid, c->norm (pid) < 0.5f ? 1.0f : 0.0f);
            c->gesto (pid, false);
            break;
        case Alvo::ciclo:
        {
            const int n = juce::jmax (2, al.opcoes);
            const int i = (inteiro (*c, pid) + 1) % n;
            c->gesto (pid, true); c->mexe (pid, (float) i / (float) (n - 1)); c->gesto (pid, false);
            break;
        }
        case Alvo::escolhe:
        {
            const int n = juce::jmax (2, al.opcoes);
            int i = al.valor;
            if (al.pid == "grp" && inteiro (*c, "grp") == al.valor) i = 0;   // clicar no G aceso tira do grupo
            c->gesto (pid, true); c->mexe (pid, (float) i / (float) (n - 1)); c->gesto (pid, false);
            break;
        }
        case Alvo::nome: case Alvo::seleciona: case Alvo::acao: case Alvo::posicao: break;
    }
    mudou();
}

void MesaTela::mouseDrag (const juce::MouseEvent& e)
{
    if (! temAtivo || ! existe (ativo.c)) return;
    const auto p = paraBase (e.position);
    float sens = ativo.tipo == Alvo::knob ? 180.0f : juce::jmax (20.0f, ativo.travel);
    if (e.mods.isShiftDown()) sens *= 5.0f;
    const float v = juce::jlimit (0.0f, 1.0f, valorIni - (p.y - arrastoIni) / sens);
    ativo.c->mexe (ativo.pid.c_str(), v);
    valorTexto = textoDe (*ativo.c, ativo.pid);
    repaint();
}

void MesaTela::mouseUp (const juce::MouseEvent&)
{
    if (temAtivo && existe (ativo.c)) ativo.c->gesto (ativo.pid.c_str(), false);
    temAtivo = false;
    mostraValor = false;
    repaint();
}

void MesaTela::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto* a = acha (paraBase (e.position));
    if (a == nullptr || a->c == nullptr || ! existe (a->c)) return;
    if (a->tipo == Alvo::knob || a->tipo == Alvo::fader)
    {
        if (temAtivo) { ativo.c->gesto (ativo.pid.c_str(), false); temAtivo = false; }
        a->c->gesto (a->pid.c_str(), true);
        a->c->mexe (a->pid.c_str(), a->c->normPadrao (a->pid.c_str()));
        a->c->gesto (a->pid.c_str(), false);
        mudou();
    }
    else if (a->tipo == Alvo::nome) renomeia (a->c);
    else if (a->tipo == Alvo::posicao) posiciona (a->c);
}

void MesaTela::mouseMove (const juce::MouseEvent& e)
{
    const auto* a = acha (paraBase (e.position));
    const bool mostra = a != nullptr && a->c != nullptr && (a->tipo == Alvo::knob || a->tipo == Alvo::fader) && existe (a->c);
    if (mostra) { valorTexto = textoDe (*a->c, a->pid); valorPos = { a->r.getCentreX(), a->r.getY() + 6 }; }
    if (mostra != mostraValor || mostra) { mostraValor = mostra; repaint(); }
    setMouseCursor (a != nullptr && a->tipo != Alvo::seleciona ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void MesaTela::mouseExit (const juce::MouseEvent&)
{
    if (! temAtivo && mostraValor) { mostraValor = false; repaint(); }
}

void MesaTela::renomeia (Canal* c)
{
    if (c == nullptr || c->tipo() == 2) return;
    auto* w = new juce::AlertWindow ("Nome do canal", u8 ("Escreva o nome que vai na fita (vazio = o nome da faixa do DAW)."),
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("nome", nomeDe (*c));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MesaTela> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, c] (int r)
    {
        if (safe == nullptr || r != 1 || ! safe->existe (c)) return;
        c->renomeia (w->getTextEditorContents ("nome").toRawUTF8());
        safe->mudou();
    }), true);
}

void MesaTela::posiciona (Canal* c)
{
    if (c == nullptr || c->tipo() == 2) return;
    auto* w = new juce::AlertWindow (u8 ("Posição na console"),
                                     u8 ("Número da faixa no DAW (define a ordem na console). Vazio ou 0 = automático: o número que o "
                                         "DAW informa ou o do começo do nome da faixa (\"3-Audio\")."),
                                     juce::MessageBoxIconType::NoIcon, this);
    const int atual = c->posicao();
    w->addTextEditor ("pos", atual > 0 ? juce::String (atual) : juce::String());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (u8 ("Automático"), 2);
    w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MesaTela> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, c] (int r)
    {
        if (safe == nullptr || r == 0 || ! safe->existe (c)) return;
        c->posiciona (r == 2 ? 0 : w->getTextEditorContents ("pos").getIntValue());
        safe->mudou();
    }), true);
}

void MesaTela::menu()
{
    juce::PopupMenu m;
    m.addSectionHeader (proc.isConsole ? "MESA" : "MESA CANAL");
    if (! proc.isConsole) m.addItem (menuRenomear, u8 ("Renomear este canal…"));
    if (! proc.isConsole) m.addItem (menuPosicao, u8 ("Posição na console…"));
    m.addItem (menuSobre, "Como funciona");
    m.addSeparator();
    m.addItem (menuLicenca, u8 ("Licença…"));
    juce::Component::SafePointer<MesaTela> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withParentComponent (this)
                         .withTargetScreenArea (juce::Rectangle<int> (1, 1).withPosition (juce::Desktop::getMousePosition())),
                     [safe] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == menuLicenca) safe->ativacao.mostrarInfo();
                         if (r == menuRenomear) safe->renomeia (&safe->proc);
                         if (r == menuPosicao) safe->posiciona (&safe->proc);
                         if (r == menuSobre)
                             juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MESA", u8 (
                                 "Dois plugins:\n"
                                 "- MESA CANAL no fim de cada faixa do DAW (tipo CANAL) e nos canais de grupo (tipo GRUPO, com G1, G2 "
                                 "ou G3). Cada um processa o som da própria faixa: pré, dinâmica, EQ, pan e fader.\n"
                                 "- MESA no master: a console inteira, com todos os canais lado a lado, os grupos, o COLA BUS e o "
                                 "fader do master. Mexer na console é mexer no plugin daquela faixa (a automação do DAW grava).\n\n"
                                 "O áudio segue o caminho do DAW: sends, returns e grupos continuam valendo. Deixe o fader do DAW "
                                 "em 0 dB e use o da MESA.\n\n"
                                 "Clique no nome do PRÉ ou do EQ para trocar o modelo. Duplo clique: o knob volta ao padrão; no "
                                 "nome, renomeia. Shift: ajuste fino. Na console, a roda do mouse passa os canais, Ctrl + roda (ou "
                                 "- e +) muda o zoom e GERAL mostra todos de uma vez.\n\n"
                                 "A ordem dos canais na console é a das faixas do DAW: o número que o DAW informa ou o do "
                                 "começo do nome da faixa (\"3-Audio\"). Duplo clique no número vermelho escolhe à mão.\n\n"
                                 "Os plugins se enxergam quando o DAW os carrega no mesmo processo (o padrão do Ableton, Logic, "
                                 "Reaper, FL e Studio One)."), "OK", safe.getComponent());
                     });
}

// ---------------------------------------------------------------------------------------------------------- canal inteiro
void MesaTela::pintaCanal (juce::Graphics& g, float x, float y, float h, Canal& c, bool sel, bool fixa, bool cores)
{
    const bool grupo = c.tipo() == 1;
    const int eqm = juce::jlimit (0, 1, inteiro (c, "eqm")), pre = juce::jlimit (0, 2, inteiro (c, "pre"));
    const bool clara = eqm == 1;
    const Colour base = clara ? hex (0x9ea2a7) : hex (0x232427);
    const Colour ink = clara ? hex (0x1b1c1e) : hex (0xc5c9ce), ink2 = clara ? hex (0x34363a) : hex (0x8f949a);
    auto& m = meds[&c];
    const float kl = x + 22, kr = x + SW - 22;
    auto alvo = [&] (Alvo::Tipo t, juce::Rectangle<float> r, const char* pid, int valor = 0, int opcoes = 2)
    {
        Alvo a; a.tipo = t; a.r = r; a.c = &c; a.pid = pid != nullptr ? pid : ""; a.valor = valor; a.opcoes = opcoes;
        alvos.push_back (a);
    };
    auto L = [&] (const juce::String& s, float cx, float cy) { txt (g, s, cx, cy + 2.5f, 6.3f, ink2); };
    auto K = [&] (float cx, float cy, Colour tampa, const char* pid, const juce::String& rot)
    {
        knob (g, cx, cy, 11, tampa, c.norm (pid), clara);
        L (rot, cx, cy + 18);
        alvo (Alvo::knob, { cx - 15, cy - 15, 30, 30 }, pid);
    };
    auto T = [&] (juce::Rectangle<float> r, const juce::String& rot, bool on, Colour cor, Alvo::Tipo t, const char* pid, int valor = 0, int opcoes = 2)
    {
        tecla (g, r, rot, on, cor, clara, cor == acAzul);
        alvo (t, r.expanded (1.5f), pid, valor, opcoes);
    };

    float yy = y;
    if (fixa)
    {
        alvo (Alvo::seleciona, { x, y, SW, h }, nullptr);
        chapa (g, { x, y, SW, h }, base);
        // --- cabeça: modelos, ordem, número ---
        vgrad (g, { x + 1, yy, SW - 2, rHead }, hex (0x17181a), hex (0x0e0f10));
        g.setColour (hex (0x2f3236)); g.fillRoundedRectangle (x + 4, yy + 4, SW - 8, 12, 2.0f);
        txt (g, preNames[pre] + u8 (" \xc2\xb7 ") + eqNames[eqm], x + SW / 2, yy + 10.5f, 6.6f, hex (0xdfe2e6));
        const bool dynPre = c.valor ("dynpre") > 0.5f;
        const char* ord[3] = { "PR\xc3\x89", dynPre ? "DIN" : "EQ", dynPre ? "EQ" : "DIN" };
        for (int i = 0; i < 3; ++i)
        {
            const auto r = juce::Rectangle<float> (x + 21, yy + 20 + (float) i * 11, SW - 42, 9);
            g.setColour (hex (0x2f9e7d, 0.12f)); g.fillRoundedRectangle (r, 1.5f);
            g.setColour (hex (0x2f9e7d)); g.drawRoundedRectangle (r, 1.5f, 0.8f);
            txt (g, u8 (ord[i]), r.getCentreX(), r.getCentreY(), 6.0f, acVerdeAgua);
        }
        txt (g, u8 ("\xe2\x96\xbc"), x + SW / 2, yy + 54, 5.0f, acVerdeAgua);
        g.setColour (hex (0x120606)); g.fillRect (x + 1, yy + rHead - 16, SW - 2, 16.0f);
        led7 (g, x + SW / 2 - 15, yy + rHead - 13, 10, tres (noLed (c)), grupo ? hex (0x38c8ff) : hex (0xff3326));
        alvo (Alvo::posicao, { x + 1, yy + rHead - 16, SW - 2, 16 }, nullptr);   // duplo clique: muda a posição
    }
    else
    {
        barraLed (g, x + 6, yy + 21, 4, 32, m.in, 14);
        barraLed (g, x + SW - 10, yy + 21, 4, 32, m.out, 14);
    }
    yy += rHead;
    if (fixa)
    {
        sulco (g, x, yy, SW);
        // --- ENTRADA ---
        K (kl, yy + 22, clara ? tampaBranca : tampaCinza, "gain", "GANHO");
        K (kr, yy + 22, tampaVermelha, "drive", "DRIVE");
        K (kl, yy + 70, clara ? tampaBranca : tampaCinza, "hpf", "HPF");
        T ({ kr - 13, yy + 57, 26, 11 }, "PAD", c.valor ("pad") > 0.5f, acLaranja, Alvo::liga, "pad");
        const char* curto[] = { "LIMPO", "312", "73" };
        T ({ kr - 13, yy + 74, 26, 11 }, curto[pre], false, acVerdeAgua, Alvo::ciclo, "pre", 0, 3);
        txt (g, u8 ("PR\xc3\x89"), kr, yy + 92, 6.3f, ink2);
    }
    yy += rInp;
    const float yDyn = yy;
    if (fixa)
    {
        sulco (g, x, yy, SW);
        // --- DINÂMICA ---
        txt (g, grupo ? juce::String ("COLA") : u8 ("DIN\xc3\x82MICA"), x + 6, yy + 8, 6.8f, ink, Al::esq);
        T ({ x + SW - 28, yy + 3, 22, 10 }, u8 ("PR\xc3\x89"), c.valor ("dynpre") > 0.5f, acLaranja, Alvo::liga, "dynpre");
        g.setColour (hex (0x08130a)); g.fillRect (x + 5, yy + 16, SW - 10, 10.0f);
        const float thr = c.valor ("thr");
        const juce::String vis = grupo ? juce::String (juce::roundToInt (c.valor ("cmix"))) + "% MIX"
                                       : (thr >= 9.9f ? juce::String ("COMP OFF") : juce::String (c.valor ("rat"), 1) + ":1");
        txt (g, vis, x + SW / 2, yy + 21.5f, 6.6f, hex (0x7dff6a), Al::meio, true, true);
        K (kl, yy + 42, clara ? tampaBranca : tampaPreta, "thr", "LIMIAR");
        K (kl, yy + 83, clara ? tampaBranca : tampaPreta, "rat", u8 ("RAZ\xc3\x83O"));
        if (grupo) K (kl, yy + 123, tampaLaranja, "cmix", "MIX");
        else       K (kl, yy + 123, tampaVerde, "gate", "GATE");
        txt (g, "GR", kr, yy + 34, 6.0f, ink2);
        const int grs[] = { 1, 3, 6, 10, 15, 20 };
        for (int i = 0; i < 6; ++i) txt (g, juce::String (grs[i]), kr + 5, yy + 42 + (float) i * 13, 5.6f, ink2);
    }
    else
    {
        const int grs[] = { 1, 3, 6, 10, 15, 20 };
        for (int i = 0; i < 6; ++i)
        {
            const float ly = yDyn + 42 + (float) i * 13;
            const bool on = m.gr >= (float) grs[i];
            const Colour col = i < 2 ? hex (0x38d66b) : i < 4 ? hex (0xffc93a) : hex (0xff3b2e);
            if (on) { g.setColour (col.withAlpha (0.25f)); g.fillEllipse (kr - 12, ly - 6, 12, 12); }
            g.setColour (on ? col : col.withAlpha (0.15f));
            g.fillEllipse (kr - 6 - 3.2f, ly - 3.2f, 6.4f, 6.4f);
            if (on) { g.setColour (Colour (0x99ffffffu)); g.fillEllipse (kr - 7 - 1.1f, ly - 1 - 1.1f, 2.2f, 2.2f); }
        }
    }
    yy += rDyn;
    if (fixa)
    {
        sulco (g, x, yy, SW);
        // --- EQ ---
        txt (g, "EQ", x + 6, yy + 8, 6.8f, ink, Al::esq);
        T ({ x + 21, yy + 3, 30, 10 }, eqNames[eqm], false, acVerdeAgua, Alvo::ciclo, "eqm", 0, 2);
        T ({ x + SW - 28, yy + 3, 22, 10 }, "IN", c.valor ("eqon") > 0.5f, acLaranja, Alvo::liga, "eqon");
        const char* bn[] = { "HF", "HMF", "LMF", "LF" };
        const Colour tampas[4] = { clara ? tampaCinza : tampaVermelha, clara ? tampaAzul : tampaVerde, tampaAzul, clara ? tampaCinza : tampaPreta };
        const char* gp[] = { "hfg", "hmfg", "lmfg", "lfg" };
        const char* fp[] = { "hff", "hmff", "lmff", "lff" };
        for (int b = 0; b < 4; ++b)
        {
            const float by = yy + 20 + (float) b * 60;
            txt (g, bn[b], x + SW / 2, by + 4, 6.4f, ink);
            K (kl, by + 24, tampas[b], gp[b], "GANHO");
            K (kr, by + 24, tampas[b], fp[b], "FREQ");
            if (b < 3) { g.setColour (clara ? Colour (0x2e000000u) : Colour (0x0fffffffu)); g.fillRect (x + 8, by + 52, SW - 16, 1.0f); }
        }
    }
    yy += rEq;
    if (fixa)
    {
        sulco (g, x, yy, SW);
        // --- SAÍDA: grupo (no CANAL: para onde vai; no GRUPO: qual é) ---
        const int gr = inteiro (c, "grp");
        for (int i = 0; i < 3; ++i)
            T ({ x + 5 + (float) i * 25, yy + 8, 23, 14 }, "G" + juce::String (i + 1), gr == i + 1, acAzul, Alvo::escolhe, "grp", i + 1, 4);
    }
    yy += rOut;
    const Colour faixa = cores ? Colour (c.cor()) : hex (0x9aa0a8);
    if (fixa)
    {
        vgrad (g, { x + 1, yy, SW - 2, rCor }, claro (faixa, 0.25f), faixa);
        g.setColour (hex (0x1c1d20)); g.fillRect (x + 1, yy + rCor, SW - 2, rNome);
        auto nm = nomeDe (c);
        if (nm.isEmpty()) nm = (grupo ? "GRUPO " : "CANAL ") + juce::String (noLed (c));
        txt (g, nm.toUpperCase(), x + SW / 2, yy + rCor + rNome / 2 + 0.5f, 9.0f, sel ? hex (0x7fd0ff) : hex (0xeceef0));
        alvo (Alvo::nome, { x, yy + rCor, SW, rNome }, nullptr);
    }
    yy += rCor + rNome;
    // --- bandeja: pan, solo/cut, fader ---
    const float th = y + h - yy, fy = yy + 52, fh = th - 82;
    const Colour tink = clara ? hex (0x26282b) : hex (0xb9bdc2);
    if (fixa)
    {
        juce::ColourGradient bg (clara ? hex (0xa7aaae) : hex (0x1f2023), x, 0, clara ? hex (0xa2a5a9) : hex (0x1c1d1f), x + SW, 0, false);
        bg.addColour (0.42, clara ? hex (0xd3d5d8) : hex (0x34363a));
        g.setGradientFill (bg); g.fillRect (x + 1, yy, SW - 2, th);
        g.setColour (juce::Colours::black); g.fillRect (x + 1, yy, SW - 2, 1.0f);
        knob (g, x + SW / 2, yy + 20, 10, clara ? tampaBranca : tampaCinza, c.norm ("pan"), clara);
        alvo (Alvo::knob, { x + SW / 2 - 14, yy + 6, 28, 28 }, "pan");
        txt (g, "PAN", x + SW / 2, yy + 37, 6.2f, tink);
        if (! grupo) T ({ x + 5, yy + 50, 24, 16 }, "SOLO", c.valor ("solo") > 0.5f, acAmarelo, Alvo::liga, "solo");
        T ({ x + 5, yy + 72, 24, 16 }, "CUT", c.valor ("mute") > 0.5f, acVermelho, Alvo::liga, "mute");
        faderFenda (g, x + 44, fy, fh);
        faderEscala (g, x + 52, fy, fh, clara);
        faderCapa (g, x + 44, fy, fh, c.norm ("fader"));
        Alvo a; a.tipo = Alvo::fader; a.r = { x + 30, fy - 18, 28, fh + 36 }; a.c = &c; a.pid = "fader"; a.travel = fh;
        alvos.push_back (a);
        g.setColour (hex (0x070708)); g.fillRect (x + 4, y + h - 20, SW - 8, 15.0f);
        const float fd = c.valor ("fader");
        txt (g, fd <= faderMin + 0.01f ? juce::String ("-inf") : db1 (fd), x + 22, y + h - 12.5f, 7.4f, hex (0xe9ebee), Al::meio, true, true);
        if (sel) { g.setColour (acSelecao); g.drawRect (juce::Rectangle<float> (x + 1, y + 1, SW - 2, h - 2), 2.0f); }
    }
    else
    {
        barraLed (g, x + SW - 9, fy, 4, fh, m.out, 30);
        txt (g, db1 (m.out), x + SW - 22, y + h - 12.5f, 7.4f, m.out > -3 ? hex (0xff6a5a) : hex (0x7fd0ff), Al::meio, true, true);
    }
}

// =====================================================================================================================
// CONSOLE
ConsoleEditor::ConsoleEditor (MesaProcessor& p) : MesaTela (p, 1600, 1000)
{
    listaCanais();
    setResizable (true, true);
    setResizeLimits (800, 500, 5000, 3000);
    const int w0 = p.uiW.load(), h0 = p.uiH.load();
    setSize (w0 > 0 ? juce::jlimit (800, 5000, w0) : 1440, h0 > 0 ? juce::jlimit (500, 3000, h0) : 900);
    startTimerHz (30);
    ativacao.anexar (*this);
}

void ConsoleEditor::paint (juce::Graphics& g)
{
    // um canal fechado entre o timer e a pintura: a lista guardada teria um ponteiro morto
    if (Ponte::get().versao.load() != versaoLista) { listaCanais(); fixaSuja = true; }
    MesaTela::paint (g);
}

void ConsoleEditor::resized()
{
    proc.uiW = getWidth(); proc.uiH = getHeight();
    primeiro = juce::jlimit (0, juce::jmax (0, (int) canais.size() - cabem()), primeiro);
    rolaY = juce::jlimit (0.0f, rolaMax(), rolaY);
    mudou();
}

// PADRÃO: a altura da janela é 100% e o zoom aumenta/diminui a partir dela (com zoom grande, a console rola na vertical;
// com zoom pequeno, cabem mais canais e o fader fica mais comprido). GERAL: tudo na janela, todos os canais à vista.
float ConsoleEditor::escala() const
{
    float s = (float) getHeight() / BASE_H;
    if (proc.uiGeral.load())
        s = juce::jmin (s, (float) getWidth() / (NAV + LBL + CEN + (float) juce::jmax (1, (int) canais.size()) * SW));
    else
        s *= juce::jlimit (0.5f, 2.0f, proc.uiZoom.load());
    return juce::jmax (0.2f, s);
}
juce::AffineTransform ConsoleEditor::base() const { return juce::AffineTransform::scale (escala()).translated (0.0f, -rolaY); }

void ConsoleEditor::zoom (int passos)
{
    const float z = proc.uiZoom.load();
    int i = 0;
    for (int k = 0; k < (int) std::size (zooms); ++k) if (std::abs (zooms[k] - z) < std::abs (zooms[i] - z)) i = k;
    i = passos == 0 ? 5 : juce::jlimit (0, (int) std::size (zooms) - 1, i + passos);
    proc.uiZoom = zooms[i];
    proc.uiGeral = false;
    primeiro = juce::jlimit (0, juce::jmax (0, (int) canais.size() - cabem()), primeiro);
    rolaY = juce::jlimit (0.0f, rolaMax(), rolaY);
}
int ConsoleEditor::cabem() const { return juce::jmax (1, (int) ((larguraBase() - NAV - LBL - CEN) / SW)); }

void ConsoleEditor::listaCanais()
{
    auto& ponte = Ponte::get();
    Canal* buf[Ponte::vagas];
    const int ver = ponte.versao.load();
    const int n = ponte.lista (buf, Ponte::vagas);
    canais.assign (buf, buf + n);
    std::sort (canais.begin(), canais.end(), [] (Canal* a, Canal* b)
    {
        const int ta = a->tipo(), tb = b->tipo();
        if (ta != tb) return ta < tb;                                   // canais primeiro, grupos depois
        if (ta == 1 && inteiro (*a, "grp") != inteiro (*b, "grp")) return inteiro (*a, "grp") < inteiro (*b, "grp");
        // a ordem das faixas no DAW; quem não tem número vai para o fim, na ordem em que abriu
        const int pa = a->posicao(), pb = b->posicao();
        if ((pa > 0) != (pb > 0)) return pa > 0;
        if (pa != pb) return pa < pb;
        return a->numero() < b->numero();
    });
    versaoLista = ver;
    if (selNum < 0 && ! canais.empty()) selNum = canais.front()->numero();   // abre com o primeiro canal selecionado
    primeiro =juce::jlimit (0, juce::jmax (0, (int) canais.size() - cabem()), primeiro);
    for (auto it = meds.begin(); it != meds.end();)
        it = std::find (canais.begin(), canais.end(), it->first) == canais.end() ? meds.erase (it) : std::next (it);
}

bool ConsoleEditor::existe (Canal* c)
{
    if (c == static_cast<Canal*> (&proc)) return true;
    if (Ponte::get().versao.load() != versaoLista) listaCanais();
    return std::find (canais.begin(), canais.end(), c) != canais.end();
}

Canal* ConsoleEditor::grupo (int gi) const
{
    for (auto* c : canais)
        if (c->tipo() == 1 && inteiro (*c, "grp") == gi + 1) return c;
    return nullptr;
}

void ConsoleEditor::mostra (int i)
{
    if (i < primeiro) primeiro = i;
    else if (i >= primeiro + cabem()) primeiro = i - cabem() + 1;
    primeiro = juce::jlimit (0, juce::jmax (0, (int) canais.size() - cabem()), primeiro);
}

void ConsoleEditor::selecionou (Canal* c)
{
    if (c == nullptr || c->tipo() == 2) return;
    selNum = c->numero();
    for (int i = 0; i < (int) canais.size(); ++i)
        if (canais[(size_t) i] == c) mostra (i);
    if (c->tipo() == 1) barra = juce::jlimit (0, 2, inteiro (*c, "grp") - 1);
    mudou();
}

void ConsoleEditor::assinatura (std::vector<float>& s)
{
    auto& ponte = Ponte::get();
    s.push_back ((float) ponte.versao.load());
    s.push_back ((float) primeiro); s.push_back ((float) selNum); s.push_back ((float) barra);
    s.push_back (autoSel ? 1.0f : 0.0f); s.push_back (cores ? 1.0f : 0.0f); s.push_back (ppm ? 1.0f : 0.0f);
    s.push_back (proc.uiGeral.load() ? 1.0f : 0.0f);
    s.push_back (proc.uiZoom.load()); s.push_back (rolaY);
    for (auto* pid : pidsConsole) s.push_back (proc.norm (pid));
    for (auto* c : canais)
    {
        s.push_back ((float) (c->cor() & 0xffffffu));
        for (auto* pid : pidsCanal) s.push_back (c->norm (pid));
    }
}

void ConsoleEditor::mede()
{
    if (Ponte::get().versao.load() != versaoLista) { listaCanais(); mudou(); }
    for (auto* c : canais) atualizaMed (c, meds[c], ppm);
    atualizaMed (&proc, master, ppm);
    const float pk = juce::jmax (proc.peakL.load(), proc.peakR.load()), rm = juce::jmax (proc.rmsL.load(), proc.rmsR.load());
    picoAgora = pk > picoAgora ? pk : juce::jmax (pk, picoAgora - 0.8f);
    rmsAgora += (rm - rmsAgora) * 0.2f;
    picoMax = juce::jmax (picoMax, pk);
    rmsMax = juce::jmax (rmsMax, rmsAgora);
}

// roda: passa os canais (ou rola na vertical, se o zoom não couber na altura); Shift ou roda lateral: canais;
// Ctrl/Cmd + roda: zoom
void ConsoleEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const bool lateral = std::abs (w.deltaX) > std::abs (w.deltaY);
    const float d = lateral ? -w.deltaX : w.deltaY;
    if (d == 0.0f) return;
    if (e.mods.isCommandDown() || e.mods.isCtrlDown()) { zoom (d > 0 ? 1 : -1); mudou(); return; }
    if (! lateral && ! e.mods.isShiftDown() && rolaMax() > 0.5f)
    {
        rolaY = juce::jlimit (0.0f, rolaMax(), rolaY - d * 240.0f);
        mudou();
        return;
    }
    primeiro = juce::jlimit (0, juce::jmax (0, (int) canais.size() - cabem()), primeiro + (d > 0 ? -1 : 1));
    mudou();
}

void ConsoleEditor::acao (int a, const juce::MouseEvent& e)
{
    switch (a)
    {
        case aAutoSel: autoSel = ! autoSel; break;
        case aCores:   cores = ! cores; break;
        case aPadrao:  proc.uiGeral = false; rolaY = juce::jlimit (0.0f, rolaMax(), rolaY); break;
        case aGeral:   proc.uiGeral = true; primeiro = 0; rolaY = 0; break;
        case aZoomMenos: zoom (-1); break;
        case aZoomMais:  zoom (1); break;
        case aZoom100:   zoom (0); break;
        case aVu:      ppm = false; break;
        case aPpm:     ppm = true; break;
        case aEco: case aHq:
            for (auto* c : canais) { c->gesto ("hq", true); c->mexe ("hq", a == aHq ? 1.0f : 0.0f); c->gesto ("hq", false); }
            break;
        case aAnt: case aProx:
        {
            if (canais.empty()) break;
            int i = -1;
            for (int k = 0; k < (int) canais.size(); ++k) if (canais[(size_t) k]->numero() == selNum) i = k;
            i = i < 0 ? 0 : juce::jlimit (0, (int) canais.size() - 1, i + (a == aProx ? 1 : -1));
            selecionou (canais[(size_t) i]);
            break;
        }
        case aZerar:   picoMax = rmsMax = -120.0f; break;
        case aLimpaSolo:
            for (auto* c : canais) if (c->valor ("solo") > 0.5f) { c->gesto ("solo", true); c->mexe ("solo", 0.0f); c->gesto ("solo", false); }
            break;
        case aRolagem:
        {
            const float x = paraBase (e.position).x, x0 = NAV + LBL, w = larguraBase() - NAV - LBL - CEN;
            const int n = juce::jmax (1, (int) canais.size());
            const float t0 = x0 + w * (float) primeiro / (float) n;
            primeiro += x < t0 ? -cabem() : cabem();
            primeiro = juce::jlimit (0, juce::jmax (0, (int) canais.size() - cabem()), primeiro);
            break;
        }
        default:
            if (a >= aBarra && a < aBarra + 4) barra = a - aBarra;
            break;
    }
    mudou();
}

// ---------------------------------------------------------------------------------------------------------- pintura
void ConsoleEditor::pintaLateral (juce::Graphics& g, float h)
{
    const float x = 0, w = NAV;
    g.setGradientFill (juce::ColourGradient (hex (0x2c2e32), x, 0, hex (0x26282b), x + w, 0, false));
    g.fillRect (x, 0.0f, w, h);
    g.setColour (juce::Colours::black); g.fillRect (x + w - 1, 0.0f, 1.0f, h);
    txt (g, "MESA", x + 14, 22, 15, hex (0xe8eaed), Al::esq);
    txt (g, "CONSOLE", x + 14, 38, 8, hex (0x8fd8ff), Al::esq);
    auto acaoEm = [&] (juce::Rectangle<float> r, int a) { Alvo al; al.tipo = Alvo::acao; al.r = r; al.codigo = a; alvos.push_back (al); };
    auto chk = [&] (const char* s, float yy, bool on, int a)
    {
        g.setColour (hex (0x9da3aa)); g.drawRoundedRectangle (x + 14, yy - 5, 10, 10, 1.5f, 1.0f);
        if (on) { g.setColour (hex (0xe2e5e8)); g.fillRect (x + 16.5f, yy - 2.5f, 5.0f, 5.0f); }
        txt (g, u8 (s), x + 31, yy, 9.5f, hex (0xd7dade), Al::esq, false);
        acaoEm ({ x + 10, yy - 9, w - 20, 18 }, a);
    };
    chk ("Auto selecionar", 70, autoSel, aAutoSel);
    chk ("Cores da faixa", 92, cores, aCores);
    auto sec = [&] (const char* s, float yy) { linha (g, x + 12, yy, x + w - 12, yy, hex (0x3a3d41)); txt (g, u8 (s), x + w / 2, yy + 13, 7.5f, hex (0x8b9097)); };
    auto bt = [&] (juce::Rectangle<float> r, const char* s, bool on, int a) { tecla (g, r, u8 (s), on, hex (0x8a9097)); acaoEm (r, a); };
    const float hw = (w - 32) / 2;
    sec ("ZOOM", 118);
    {
        const bool geral = proc.uiGeral.load();
        bt ({ x + 14, 140, 24, 18 }, "-", false, aZoomMenos);
        bt ({ x + w - 38, 140, 24, 18 }, "+", false, aZoomMais);
        const auto rz = juce::Rectangle<float> (x + 42, 140, w - 84, 18);
        g.setColour (hex (0x0b0c0d)); g.fillRoundedRectangle (rz, 2.0f);
        txt (g, geral ? juce::String ("GERAL") : juce::String (juce::roundToInt (proc.uiZoom.load() * 100.0f)) + "%",
             rz.getCentreX(), rz.getCentreY(), 9.0f, hex (0x7fd0ff), Al::meio, true, true);
        acaoEm (rz, aZoom100);   // clique no número: volta a 100%
        bt ({ x + 14, 164, hw, 18 }, "PADR\xc3\x83O", ! geral, aPadrao);
        bt ({ x + 18 + hw, 164, hw, 18 }, "GERAL", geral, aGeral);
    }
    sec ("MEDIDOR", 196);
    bt ({ x + 14, 218, hw, 18 }, "VU", ! ppm, aVu);
    bt ({ x + 18 + hw, 218, hw, 18 }, "PPM", ppm, aPpm);
    sec ("QUALIDADE", 250);
    const bool hq = canais.empty() || canais.front()->valor ("hq") > 0.5f;
    bt ({ x + 14, 272, hw, 18 }, "ECO", ! hq, aEco);
    bt ({ x + 18 + hw, 272, hw, 18 }, "HQ", hq, aHq);
    sec ("CANAIS", 304);
    int nc = 0, ng = 0;
    for (auto* c : canais) (c->tipo() == 1 ? ng : nc)++;
    txt (g, juce::String ((int) canais.size()) + (canais.size() == 1 ? " conectado" : " conectados"), x + w / 2, 328, 9.0f, hex (0xd7dade), Al::meio, false);
    txt (g, juce::String (nc) + u8 (" canais \xc2\xb7 ") + juce::String (ng) + " grupos", x + w / 2, 343, 8.0f, hex (0x8b9097), Al::meio, false);
    if (canais.empty())
    {
        const char* ajuda[] = { "Ponha a MESA CANAL", "no fim de cada faixa", "do DAW: ela aparece", "aqui sozinha." };
        for (int i = 0; i < 4; ++i) txt (g, ajuda[i], x + w / 2, 372 + (float) i * 14, 8.5f, hex (0xaeb3b9), Al::meio, false);
    }
    g.setColour (hex (0x38d66b)); g.fillEllipse (x + 14, h - 26, 8, 8);
    txt (g, "no master", x + 28, h - 22, 8.5f, hex (0xaeb3b9), Al::esq, false);
}

void ConsoleEditor::pintaTopo (juce::Graphics& g, float x, float w)
{
    vgrad (g, { x, 0, w, TOP }, hex (0x3b3e43), hex (0x2c2e32));
    g.setColour (juce::Colours::black); g.fillRect (x, TOP - 1, w, 1.0f);
    txt (g, "MIXER", x + 14, TOP / 2, 11, hex (0xc3c8ce), Al::esq);
    const float lw = 300, lx = x + w - lw - 98;
    g.setColour (hex (0x060a06)); g.fillRoundedRectangle (lx, 7, lw, TOP - 14, 3.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (lx, 7, lw, TOP - 14, 3.0f, 1.0f);
    Canal* sel = nullptr;
    for (auto* c : canais) if (c->numero() == selNum) sel = c;
    if (sel != nullptr)
    {
        led7 (g, lx + 12, 13, TOP - 26, tres (noLed (*sel)), hex (0x6dff5a));
        auto nm = nomeDe (*sel).toUpperCase();
        if (nm.isEmpty()) nm = "CANAL " + juce::String (noLed (*sel));
        txt (g, nm, lx + 76, TOP / 2 - 1, 13, hex (0x8dff7c), Al::esq, true, true);
        const int gr = inteiro (*sel, "grp");
        if (gr > 0)
        {
            juce::String gtxt = "G" + juce::String (gr);
            if (auto* gc = grupo (gr - 1); gc != nullptr && gc != sel) gtxt << u8 (" \xc2\xb7 ") << nomeDe (*gc).toUpperCase();
            txt (g, gtxt, lx + lw - 10, TOP / 2 - 1, 10, hex (0x53b947), Al::dir, true, true);
        }
    }
    else txt (g, "nenhum canal selecionado", lx + 14, TOP / 2, 10, hex (0x3f8a35), Al::esq, true, true);
    auto bt = [&] (juce::Rectangle<float> r, const char* s, int a)
    {
        tecla (g, r, u8 (s), false);
        Alvo al; al.tipo = Alvo::acao; al.r = r; al.codigo = a; alvos.push_back (al);
    };
    bt ({ x + w - 88, 8, 36, TOP - 16 }, "\xe2\x97\x80", aAnt);
    bt ({ x + w - 46, 8, 36, TOP - 16 }, "\xe2\x96\xb6", aProx);
}

void ConsoleEditor::pintaRotulos (juce::Graphics& g, float x, float y, float h)
{
    g.setGradientFill (juce::ColourGradient (hex (0x26282b), x, 0, hex (0x1d1e21), x + LBL, 0, false));
    g.fillRect (x, y, LBL, h);
    g.setColour (juce::Colours::black); g.fillRect (x + LBL - 1, y, 1.0f, h);
    float yy = y;
    struct Linha { const char* s; float h; };
    const Linha rows[] = { { "CANAL", rHead }, { "ENTRADA", rInp }, { "DIN\xc3\x82M.", rDyn }, { "EQ", rEq }, { "SA\xc3\x8d" "DA", rOut }, { "", rCor + rNome } };
    for (auto& r : rows)
    {
        if (r.s[0] != 0)
        {
            g.setColour (hex (0xa6abb1)); g.fillRoundedRectangle (x + 4, yy + 4, 8, 8, 1.5f);
            txt (g, u8 (r.s), x + 15, yy + 8.5f, 6.4f, hex (0xaeb3b9), Al::esq);
        }
        yy += r.h;
        sulco (g, x, yy, LBL);
    }
    g.setColour (hex (0xa6abb1)); g.fillRoundedRectangle (x + 4, yy + 4, 8, 8, 1.5f);
    txt (g, "OUT", x + 15, yy + 8.5f, 6.4f, hex (0xaeb3b9), Al::esq);
    bool algumSolo = false;
    for (auto* c : canais) algumSolo = algumSolo || c->valor ("solo") > 0.5f;
    const auto r = juce::Rectangle<float> (x + 4, yy + 46, LBL - 8, 22);
    tecla (g, r, "LIMPA", algumSolo, acAmarelo);
    txt (g, "SOLO", x + LBL / 2, yy + 75, 6.0f, hex (0x8b9097));
    Alvo al; al.tipo = Alvo::acao; al.r = r; al.codigo = aLimpaSolo; alvos.push_back (al);
}

void ConsoleEditor::pintaCentral (juce::Graphics& g, float x, float w, bool fixa)
{
    const float y = TOP, h = (float) getHeight() / escala() - TOP;
    const float oy = y + 36, oh = 196;
    const float bx = x + 18, by = oy + 24, bh = oh - 34;
    const float gx = x + 178, gy = by - 2, gw = w - 194, gh = bh + 4;
    const float ty = oy + oh + 12, tw = (w - 16 - 9) / 4;
    const float vy = ty + 32;
    const float mw = (w - 228) / 2;
    auto vuRect = [&] (int i) { return juce::Rectangle<float> (x + 212 + (float) (i % 2) * (mw + 8), vy + (float) (i / 2) * 82, mw, 76); };
    const float py = vy + 172, ph = 196;
    const float fy = py + ph + 10, fh = y + h - fy - 8, fw = (w - 16) / 4;
    auto grBarra = [&] (int i) -> float
    {
        if (i == 3) return master.vuGr;
        if (auto* gc = grupo (i)) return meds[gc].vuGr;
        return 0.0f;
    };
    juce::String nomesBarra[4];
    for (int i = 0; i < 3; ++i)
    {
        auto* gc = grupo (i);
        nomesBarra[i] = gc != nullptr ? nomeDe (*gc).toUpperCase() : juce::String();
        if (nomesBarra[i].isEmpty()) nomesBarra[i] = "G" + juce::String (i + 1);
    }
    nomesBarra[3] = "MASTER";
    const auto vuGrande = juce::Rectangle<float> (x + 8, vy, 196, 164);

    if (! fixa)
    {
        // barras L/R do master
        const float lv[2] = { master.outL, master.outR };
        for (int i = 0; i < 2; ++i)
        {
            const float xx = bx + 16 + (float) i * 17;
            const float f = juce::jlimit (0.0f, 1.0f, (lv[i] + 60.0f) / 60.0f), top = by + bh * (1 - f);
            juce::ColourGradient cg (hex (0xff5a3a), 0, by, hex (0x5aa848), 0, by + bh, false);
            cg.addColour (0.12, hex (0xe8d27a)); cg.addColour (0.3, hex (0x9fd36a));
            g.setGradientFill (cg); g.fillRect (xx, top, 13.0f, by + bh - top);
        }
        const float nx = bx + 64;
        txt (g, db1 (picoAgora), nx + 42, by + 28, 20, hex (0xf0f2f4), Al::meio, false);
        txt (g, "max " + db1 (picoMax), nx + 42, by + 48, 8, hex (0xa4a9af), Al::meio, false);
        txt (g, db1 (rmsAgora), nx + 42, by + 98, 20, hex (0xf0f2f4), Al::meio, false);
        txt (g, "max " + db1 (rmsMax), nx + 42, by + 118, 8, hex (0xa4a9af), Al::meio, false);
        // goniômetro: últimas 1024 amostras do master, em M/S
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (juce::Rectangle<float> (gx, gy, gw, gh).toNearestInt());
            const float cx = gx + gw / 2, cy = gy + gh / 2, s = juce::jmin (gw, gh) * 0.42f;
            const int pos = proc.gonioPos.load (std::memory_order_acquire);
            g.setColour (juce::Colour (0x73d7f078u));
            for (int i = 0; i < 1024; ++i)
            {
                const int k = (pos - 1 - i + MesaProcessor::gonioN * 2) % MesaProcessor::gonioN;
                const float l = proc.gonioL[k], r = proc.gonioR[k];
                const float mm = (l + r) * 0.5f * 1.4f, sd = (r - l) * 0.5f * 1.4f;
                g.fillRect (cx + juce::jlimit (-1.2f, 1.2f, sd) * s, cy - juce::jlimit (-1.2f, 1.2f, mm) * s, 1.2f, 1.2f);
            }
        }
        vuAgulha (g, vuGrande, grPos (grBarra (barra)));
        for (int i = 0; i < 4; ++i) vuAgulha (g, vuRect (i), grPos (grBarra (i)));
        for (int i = 0; i < 4; ++i)
        {
            const float fx = x + 8 + (float) i * fw, ffy = fy + 30, ffh = fh - 56;
            const Med* m = &master;
            if (i < 3) { auto* gc = grupo (i); if (gc == nullptr) continue; m = &meds[gc]; }
            barraLed (g, fx + fw - 18, ffy, 4, ffh, m->outL, 30);
            barraLed (g, fx + fw - 12, ffy, 4, ffh, m->outR, 30);
        }
        return;
    }

    // ---- parte fixa
    g.setColour (hex (0x1f2023)); g.fillRect (x, y, w, h);
    g.setColour (juce::Colours::black); g.fillRect (x, y, 1.0f, h);
    vgrad (g, { x, y, w, 28 }, hex (0x4b4f54), hex (0x3a3d41));
    txt (g, u8 ("SE\xc3\x87\xc3\x83O CENTRAL"), x + w / 2, y + 14, 9.5f, hex (0xe2e5e8));
    g.setColour (hex (0x0b0c0d)); g.fillRoundedRectangle (x + 8, oy, w - 16, oh, 3.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (x + 8, oy, w - 16, oh, 3.0f, 1.0f);
    txt (g, "MASTER", x + 20, oy + 11, 8, hex (0xd7dade), Al::esq);
    tecla (g, { x + w - 62, oy + 4, 46, 14 }, "ZERAR", false);
    { Alvo al; al.tipo = Alvo::acao; al.r = { x + w - 62, oy + 4, 46, 14 }; al.codigo = aZerar; alvos.push_back (al); }
    const char* marcas[] = { "0", "10", "20", "30", "40", "50", "60" };
    for (int i = 0; i < 7; ++i) txt (g, marcas[i], bx + 6, by + (float) i / 6.0f * bh, 6.5f, hex (0x7b8086));
    for (int i = 0; i < 2; ++i) { g.setColour (juce::Colours::black); g.fillRect (bx + 16 + (float) i * 17, by, 13.0f, bh); }
    const float nx = bx + 64;
    txt (g, "PICO", nx, by + 6, 7, hex (0x8b9097), Al::esq);  txt (g, "dBFS", nx + 70, by + 6, 6.5f, hex (0x5d6268), Al::esq);
    txt (g, "RMS", nx, by + 76, 7, hex (0x8b9097), Al::esq);  txt (g, "dBFS", nx + 70, by + 76, 6.5f, hex (0x5d6268), Al::esq);
    {
        g.setColour (hex (0x020203)); g.fillRect (gx, gy, gw, gh);
        const float cx = gx + gw / 2, cy = gy + gh / 2, s = juce::jmin (gw, gh) * 0.42f;
        juce::Path d; d.startNewSubPath (cx, cy - s); d.lineTo (cx + s, cy); d.lineTo (cx, cy + s); d.lineTo (cx - s, cy); d.closeSubPath();
        g.setColour (hex (0x25282c)); g.strokePath (d, juce::PathStrokeType (1.0f));
        linha (g, cx, cy - s, cx, cy + s, hex (0x1b1d20)); linha (g, cx - s, cy, cx + s, cy, hex (0x1b1d20));
        txt (g, "M", cx, cy - s - 7, 7, hex (0x5d6268));
        txt (g, "L", cx - s * 0.72f, cy - s * 0.72f, 7, hex (0x5d6268));
        txt (g, "R", cx + s * 0.72f, cy - s * 0.72f, 7, hex (0x5d6268));
    }
    // abas dos barramentos
    for (int i = 0; i < 4; ++i)
    {
        const bool on = i == barra;
        const auto r = juce::Rectangle<float> (x + 8 + (float) i * (tw + 3), ty, tw, 22);
        g.setColour (on ? hex (0x16334a) : hex (0x18191b)); g.fillRoundedRectangle (r, 2.0f);
        g.setColour (on ? acSelecao : hex (0x3a3d41)); g.drawRoundedRectangle (r, 2.0f, 1.0f);
        txt (g, nomesBarra[i], r.getCentreX(), r.getCentreY() + 0.5f, 8.5f, on ? hex (0x8fd8ff) : hex (0xd4d7db));
        Alvo al; al.tipo = Alvo::acao; al.r = r; al.codigo = aBarra + i; alvos.push_back (al);
    }
    vuCaixa (g, vuGrande, nomesBarra[barra] + u8 (" \xc2\xb7 GR"), true, false, true);
    for (int i = 0; i < 4; ++i)
    {
        vuCaixa (g, vuRect (i), nomesBarra[i], false, false, true);
        Alvo al; al.tipo = Alvo::acao; al.r = vuRect (i); al.codigo = aBarra + i; alvos.push_back (al);
    }
    // compressor do barramento selecionado
    const float pw = w - 16;
    g.setGradientFill (juce::ColourGradient (hex (0x5d6166), 0, py, hex (0x43464a), 0, py + ph, false));
    g.fillRoundedRectangle (x + 8, py, pw, ph, 3.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (x + 8, py, pw, ph, 3.0f, 1.0f);
    g.setColour (juce::Colour (0x40ffffffu)); g.fillRect (x + 9, py + 1, pw - 2, 1.0f);
    txt (g, barra == 3 ? juce::String ("COLA BUS") : "COLA " + nomesBarra[barra], x + 16, py + 12, 8, hex (0xe9ebee), Al::esq);
    auto KB = [&] (Canal& c, int i, const char* pid, const char* rot)
    {
        const float kx = x + 8 + pw / 6 + (float) (i % 3) * pw / 3, ky = py + 62 + (float) (i / 3) * 74;
        knob (g, kx, ky, 17, tampaAzulBus, c.norm (pid));
        txt (g, u8 (rot), kx, ky + 30, 7.5f, hex (0xf0f2f4));
        Alvo al; al.tipo = Alvo::knob; al.r = { kx - 22, ky - 22, 44, 44 }; al.c = &c; al.pid = pid; alvos.push_back (al);
    };
    if (barra == 3)
    {
        const auto rIn = juce::Rectangle<float> (x + w - 62, py + 6, 46, 14), rSc = juce::Rectangle<float> (x + w - 116, py + 6, 50, 14);
        tecla (g, rIn, "IN", proc.valor ("min") > 0.5f, acLaranja);
        { Alvo al; al.tipo = Alvo::liga; al.r = rIn; al.c = &proc; al.pid = "min"; alvos.push_back (al); }
        tecla (g, rSc, "SC HPF", proc.valor ("mhpf") > 0.5f, acLaranja);
        { Alvo al; al.tipo = Alvo::liga; al.r = rSc; al.c = &proc; al.pid = "mhpf"; alvos.push_back (al); }
        KB (proc, 0, "mthr", "LIMIAR"); KB (proc, 1, "mmkp", "GANHO"); KB (proc, 2, "mmix", "MIX");
        KB (proc, 3, "matk", "ATAQUE"); KB (proc, 4, "mrel", "SOLTURA"); KB (proc, 5, "mrat", "RAZ\xc3\x83O");
    }
    else if (auto* gc = grupo (barra))
    {
        KB (*gc, 0, "thr", "LIMIAR"); KB (*gc, 1, "rat", "RAZ\xc3\x83O"); KB (*gc, 2, "cmix", "MIX");
        txt (g, u8 ("ataque 10 ms \xc2\xb7 soltura AUTO"), x + 8 + pw / 2, py + 150, 8.0f, hex (0xd7dade), Al::meio, false);
    }
    else
    {
        txt (g, "Nenhum grupo G" + juce::String (barra + 1), x + 8 + pw / 2, py + 64, 10, hex (0xf0f2f4));
        const char* ajuda[] = { "Ponha a MESA CANAL no canal de grupo do DAW,", "acenda GRUPO e escolha este G." };
        for (int i = 0; i < 2; ++i) txt (g, ajuda[i], x + 8 + pw / 2, py + 90 + (float) i * 15, 8.5f, hex (0xdfe2e6), Al::meio, false);
    }
    // faders dos barramentos
    for (int i = 0; i < 4; ++i)
    {
        const float fx = x + 8 + (float) i * fw;
        Canal* c = i == 3 ? static_cast<Canal*> (&proc) : grupo (i);
        chapa (g, { fx, fy, fw - 3, fh }, hex (0x232427));
        const juce::Colour cor = i == 3 ? hex (0xd0d0d0) : (c != nullptr && cores ? juce::Colour (c->cor()) : hex (0x5d6268));
        g.setColour (cor); g.fillRect (fx + 1, fy, fw - 5, 5.0f);
        txt (g, nomesBarra[i], fx + (fw - 3) / 2, fy + 15, 8, c != nullptr ? hex (0xeceef0) : hex (0x5d6268));
        const float ffy = fy + 30, ffh = fh - 56;
        faderFenda (g, fx + 26, ffy, ffh);
        faderEscala (g, fx + 34, ffy, ffh, false);
        g.setColour (hex (0x070708)); g.fillRect (fx + 4, fy + fh - 20, fw - 11, 15.0f);
        if (c == nullptr) continue;
        faderCapa (g, fx + 26, ffy, ffh, c->norm ("fader"), i == 3 ? hex (0xd23a2e) : hex (0xe9ebee));
        Alvo al; al.tipo = Alvo::fader; al.r = { fx + 10, ffy - 18, 32, ffh + 36 }; al.c = c; al.pid = "fader"; al.travel = ffh; alvos.push_back (al);
        const float fd = c->valor ("fader");
        txt (g, fd <= faderMin + 0.01f ? juce::String ("-inf") : db1 (fd), fx + (fw - 3) / 2, fy + fh - 12.5f, 7.6f, hex (0xe9ebee), Al::meio, true, true);
    }
}

void ConsoleEditor::pintaFixa (juce::Graphics& g)
{
    const float W = larguraBase(), H = alturaBase();
    g.fillAll (hex (0x111111));
    pintaLateral (g, H);
    pintaTopo (g, NAV, W - NAV);
    const float sy = TOP, sh = H - TOP - 10;
    pintaRotulos (g, NAV, sy, sh);
    const int n = cabem();
    for (int i = 0; i < n; ++i)
    {
        const int k = primeiro + i;
        const float x = NAV + LBL + (float) i * SW;
        if (k < (int) canais.size()) pintaCanal (g, x, sy, sh, *canais[(size_t) k], canais[(size_t) k]->numero() == selNum, true, cores);
        else chapa (g, { x, sy, SW, sh }, hex (0x18191b));
    }
    const float aw = W - NAV - LBL - CEN;
    g.setColour (hex (0x141516)); g.fillRect (NAV + LBL, H - 10, aw, 10.0f);
    const int tot = juce::jmax (1, (int) canais.size());
    if ((int) canais.size() > n)
    {
        g.setColour (hex (0x4a4e54));
        g.fillRoundedRectangle (NAV + LBL + 2 + aw * (float) primeiro / (float) tot, H - 8, juce::jmax (20.0f, aw * (float) n / (float) tot - 4), 6, 3.0f);
    }
    { Alvo al; al.tipo = Alvo::acao; al.r = { NAV + LBL, H - 10, aw, 10 }; al.codigo = aRolagem; alvos.push_back (al); }
    pintaCentral (g, W - CEN, CEN, true);
}

void ConsoleEditor::pintaViva (juce::Graphics& g)
{
    const float W = larguraBase(), H = alturaBase();
    const float sy = TOP, sh = H - TOP - 10;
    for (int i = 0; i < cabem(); ++i)
    {
        const int k = primeiro + i;
        if (k < (int) canais.size()) pintaCanal (g, NAV + LBL + (float) i * SW, sy, sh, *canais[(size_t) k], false, false, cores);
    }
    pintaCentral (g, W - CEN, CEN, false);
}

// =====================================================================================================================
// CANAL
CanalEditor::CanalEditor (MesaProcessor& p) : MesaTela (p, 400, 1000)
{
    setResizable (true, true);
    setResizeLimits (240, 600, 1200, 3000);
    if (auto* k = getConstrainer()) k->setFixedAspectRatio (400.0 / 1000.0);
    const int h0 = p.uiH.load();
    const int h = h0 > 0 ? juce::jlimit (600, 3000, h0) : 900;
    setSize (juce::roundToInt ((float) h * 0.4f), h);
    startTimerHz (30);
    ativacao.anexar (*this);
}

void CanalEditor::resized() { proc.uiW = getWidth(); proc.uiH = getHeight(); mudou(); }

juce::AffineTransform CanalEditor::base() const
{
    const float s = juce::jmin ((float) getWidth() / 400.0f, (float) getHeight() / 1000.0f);
    return juce::AffineTransform::scale (s).translated (((float) getWidth() - 400.0f * s) / 2, ((float) getHeight() - 1000.0f * s) / 2);
}

void CanalEditor::assinatura (std::vector<float>& s)
{
    s.push_back ((float) Ponte::get().versao.load());
    s.push_back ((float) Ponte::get().consoles.load());
    for (auto* pid : pidsCanal) s.push_back (proc.norm (pid));
}

void CanalEditor::mede() { atualizaMed (&proc, meds[&proc], false); }

namespace { constexpr float cpx = 106, cTop = TOP + 6, cVus = cTop + 64 + 8 + 96 + 8; }

void CanalEditor::pintaFixa (juce::Graphics& g)
{
    const float W = 400, H = 1000;
    const bool grupo = proc.tipo() == 1;
    g.fillAll (hex (0x1f2023));
    vgrad (g, { 0, 0, W, TOP }, hex (0x3b3e43), hex (0x2c2e32));
    g.setColour (juce::Colours::black); g.fillRect (0.0f, TOP - 1, W, 1.0f);
    txt (g, "MESA", 12, 16, 13, hex (0xe8eaed), Al::esq);
    txt (g, "CANAL", 12, 31, 8, hex (0x8fd8ff), Al::esq);
    auto escolhe = [&] (juce::Rectangle<float> r, const juce::String& rot, const char* pid, int valor, int opcoes, juce::Colour cor)
    {
        tecla (g, r, rot, inteiro (proc, pid) == valor, cor);
        Alvo a; a.tipo = Alvo::escolhe; a.r = r; a.c = &proc; a.pid = pid; a.valor = valor; a.opcoes = opcoes; alvos.push_back (a);
    };
    escolhe ({ W - 168, 11, 76, 22 }, "CANAL", "mode", 0, 2, acAzul);
    escolhe ({ W - 88, 11, 76, 22 }, "GRUPO", "mode", 1, 2, acAzul);

    pintaCanal (g, 10, cTop, H - TOP - 16, proc, false, true, true);

    const float px = cpx, pw = W - px - 10;
    float y = cTop;
    auto cartao = [&] (float h, const char* titulo)
    {
        g.setColour (hex (0x18191b)); g.fillRoundedRectangle (px, y, pw, h, 3.0f);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (px, y, pw, h, 3.0f, 1.0f);
        txt (g, u8 (titulo), px + 10, y + 11, 7.5f, hex (0x8b9097), Al::esq);
        const float y0 = y; y += h + 8; return y0;
    };
    // ligação com a console
    float yy = cartao (64, "CONSOLE");
    const bool ligada = Ponte::get().consoles.load() > 0;
    const juce::Colour lc = ligada ? hex (0x38d66b) : hex (0x6b7077);
    if (ligada) { g.setColour (lc.withAlpha (0.25f)); g.fillEllipse (px + 7, yy + 23, 18, 18); }
    g.setColour (lc); g.fillEllipse (px + 11, yy + 27, 10, 10);
    txt (g, ligada ? u8 ("Ligado à MESA no master") : juce::String ("Console fechada"), px + 28, yy + 32, 10, hex (0xe6e8eb), Al::esq, true);
    txt (g, ligada ? juce::String ("mexa por aqui ou pela console") : juce::String ("ponha a MESA no master do DAW"), px + 28, yy + 48, 8, hex (0x8b9097), Al::esq, false);
    // número, nome e grupo
    yy = cartao (96, grupo ? "GRUPO" : "POSI\xc3\x87\xc3\x83O NA CONSOLE");
    g.setColour (hex (0x120606)); g.fillRect (px + 10, yy + 22, 62.0f, 30.0f);
    led7 (g, px + 16, yy + 28, 18, tres (noLed (proc)), grupo ? hex (0x38c8ff) : hex (0xff3326));
    { Alvo a; a.tipo = Alvo::posicao; a.r = { px + 10, yy + 22, 62, 30 }; a.c = &proc; alvos.push_back (a); }
    g.setColour (hex (0x0b0c0d)); g.fillRoundedRectangle (px + 80, yy + 22, pw - 90, 30, 2.0f);
    g.setColour (hex (0x3a3d41)); g.drawRoundedRectangle (px + 80, yy + 22, pw - 90, 30, 2.0f, 1.0f);
    g.setColour (juce::Colour (proc.cor())); g.fillRect (px + 81, yy + 46, pw - 92, 5.0f);
    const auto nm = proc.displayName().toUpperCase();
    txt (g, nm.isEmpty() ? juce::String ("(nome da faixa)") : nm, px + 90, yy + 34, 12, nm.isEmpty() ? hex (0x6b7077) : hex (0xf0f2f4), Al::esq);
    { Alvo a; a.tipo = Alvo::nome; a.r = { px + 80, yy + 22, pw - 90, 30 }; a.c = &proc; alvos.push_back (a); }
    txt (g, grupo ? u8 ("é o") : juce::String ("vai para"), px + 10, yy + 74, 8.5f, hex (0xaeb3b9), Al::esq, false);
    for (int i = 0; i < 3; ++i) escolhe ({ px + 70 + (float) i * 34, yy + 65, 30, 18 }, "G" + juce::String (i + 1), "grp", i + 1, 4, acAzul);
    // VUs
    yy = cartao (300, "MEDIDORES");
    vuCaixa (g, { px + 10, yy + 22, pw - 20, 86 }, "ENTRADA", false, true);
    vuCaixa (g, { px + 10, yy + 114, pw - 20, 86 }, u8 ("REDUÇÃO DE GANHO"), false, true, true);
    vuCaixa (g, { px + 10, yy + 206, pw - 20, 86 }, u8 ("SAÍDA"), false, true);
    // caráter
    yy = cartao (150, "CAR\xc3\x81TER");
    txt (g, u8 ("PRÉ"), px + 10, yy + 32, 8, hex (0xd4d7db), Al::esq);
    for (int i = 0; i < 3; ++i) escolhe ({ px + 46 + (float) i * 76, yy + 23, 72, 18 }, preNames[i], "pre", i, 3, acVerdeAgua);
    txt (g, "EQ", px + 10, yy + 62, 8, hex (0xd4d7db), Al::esq);
    for (int i = 0; i < 2; ++i) escolhe ({ px + 46 + (float) i * 76, yy + 53, 72, 18 }, eqNames[i], "eqm", i, 2, acVerdeAgua);
    txt (g, "ORDEM", px + 10, yy + 92, 8, hex (0xd4d7db), Al::esq);
    escolhe ({ px + 46, yy + 83, 72, 18 }, u8 ("EQ → DIN"), "dynpre", 0, 2, acLaranja);
    escolhe ({ px + 122, yy + 83, 72, 18 }, u8 ("DIN → EQ"), "dynpre", 1, 2, acLaranja);
    txt (g, u8 ("Muda só o som: o painel é o mesmo em todos."), px + 10, yy + 126, 8, hex (0x7b8086), Al::esq, false);
    // como ligar
    yy = cartao (H - 10 - y, "COMO LIGAR");
    const char* canal[] = { "1. MESA CANAL no fim de cada faixa.", "2. MESA (console) no master.", "3. O \xc3\xa1udio segue o caminho do DAW:",
                            "    sends, returns e grupos valem.", "4. Fader do DAW em 0 dB; use o da MESA." };
    const char* grp[] = { "1. MESA CANAL no canal de grupo do", "    DAW, com GRUPO aceso.", "2. Escolha G1, G2 ou G3.",
                          "3. Os canais do grupo marcam o mesmo G.", "4. Sends e returns seguem normais." };
    for (int i = 0; i < 5; ++i) txt (g, u8 (grupo ? grp[i] : canal[i]), px + 10, yy + 30 + (float) i * 15, 8.5f, hex (0xaeb3b9), Al::esq, false);
}

void CanalEditor::pintaViva (juce::Graphics& g)
{
    const float W = 400, H = 1000;
    pintaCanal (g, 10, cTop, H - TOP - 16, proc, false, false, true);
    const float px = cpx, pw = W - px - 10, yy = cVus;
    const auto& m = meds[&proc];
    vuAgulha (g, { px + 10, yy + 22, pw - 20, 86 }, vuPos (m.vuIn));
    vuAgulha (g, { px + 10, yy + 114, pw - 20, 86 }, grPos (m.vuGr));
    vuAgulha (g, { px + 10, yy + 206, pw - 20, 86 }, vuPos (m.vuOut));
}

// =====================================================================================================================
juce::AudioProcessorEditor* MesaProcessor::createEditor()
{
    if (isConsole) return new ConsoleEditor (*this);
    return new CanalEditor (*this);
}
