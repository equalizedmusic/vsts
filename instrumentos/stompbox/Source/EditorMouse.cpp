// STOMPBOX — mouse, menus e textos dos knobs.
#include "PluginEditor.h"
#include "Desenho.h"

using namespace desenho;
using stomp::infoPedal;

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "teste"   // a foto (shots) compila sem os defines do plugin
#endif

const StompboxEditor::Alvo* StompboxEditor::acha (juce::Point<float> p) const
{
    for (auto it = alvos.rbegin(); it != alvos.rend(); ++it)
        if (it->tipo != aPedal && it->r.contains (p)) return &*it;
    for (auto it = alvos.rbegin(); it != alvos.rend(); ++it)
        if (it->tipo == aPedal && it->r.contains (p)) return &*it;
    return nullptr;
}

static int indiceKnob (const juce::String& pid)   // "s3_k2" -> 1
{
    const int i = pid.lastIndexOf ("_k");
    return i >= 0 ? pid.substring (i + 2).getIntValue() - 1 : -1;
}

juce::String StompboxEditor::textoKnob (const Alvo& a) const
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (a.pid));
    if (p == nullptr) return {};
    const float v = proc.real (a.pid);
    if (a.slot >= 0)
    {
        const auto d = proc.le (a.slot);
        const auto& inf = infoPedal (d.tipo);
        const int k = indiceKnob (a.pid);
        if (k < 0 || k >= inf.nk) return {};
        juce::String nome = inf.knob[k], val = juce::String (v, 1);
        const bool sync = proc.real ("sync") > 0.5f;
        if (d.tipo == stomp::Echo && k == 0)
            val = sync ? juce::String (stomp::nomeDivisaoDelay (v / 10.0f)) : juce::String (juce::roundToInt (stomp::expMap (40.0f, 1200.0f, v / 10.0f))) + " ms";
        else if ((d.tipo == stomp::Swirl || d.tipo == stomp::Tide) && k == 0)
            val = sync ? juce::String (stomp::nomeDivisaoLfo (v / 10.0f))
                       : juce::String (d.tipo == stomp::Swirl ? stomp::expMap (0.08f, 9.0f, v / 10.0f) : stomp::expMap (0.1f, 8.0f, v / 10.0f), 2) + " Hz";
        else if (d.tipo == stomp::Bender && k == 1)
            val = stomp::benderNome (v / 10.0f);
        else if (d.tipo == stomp::Bender && k == 0)
            val = d.sw == 1 ? juce::String (juce::roundToInt (3.0f * v)) + " cents"
                            : juce::String (stomp::benderSemitons (d.k[1] / 10.0f) * v / 10.0f, 1) + " st";
        else if (d.tipo == stomp::Halo && k == 3)
            val = juce::String (juce::roundToInt (v * 20.0f)) + " ms";
        return nome + "  " + val;
    }
    return p->getName (40).toUpperCase() + "  " + p->getCurrentValueAsText() + (p->getLabel().isNotEmpty() && ! p->getCurrentValueAsText().contains (p->getLabel()) ? " " + p->getLabel() : juce::String());
}

void StompboxEditor::mudaKnob (const Alvo& a, float norm)
{
    if (auto* p = proc.apvts.getParameter (a.pid))
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
}

void StompboxEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto* a = acha (base (e));
    setMouseCursor (a == nullptr ? juce::MouseCursor::NormalCursor
                    : a->tipo == aKnob || a->tipo == aTreadle ? juce::MouseCursor::UpDownResizeCursor
                    : a->tipo == aPedal ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::PointingHandCursor);
}

void StompboxEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = base (e);
    mouse = p;
    const auto* a = acha (p);
    if (seletorAberto)
    {
        if (a == nullptr || a->tipo == aPickFecha) { seletorAberto = false; sujo(); return; }
        if (a->tipo == aPickCat) { seletorCat = a->idx; sujo(); return; }
        if (a->tipo == aPick)
        {
            if (seletorTroca)
            {
                auto d = proc.le (seletorSlot);
                const auto& inf = infoPedal (a->idx);
                d.tipo = a->idx; d.sw = 0;
                for (int k = 0; k < 4; ++k) d.k[k] = k < inf.nk ? inf.def[k] : 5.0f;
                proc.grava (seletorSlot, d);
            }
            else
            {
                proc.poePedal (seletorSlot, a->idx);
                const int pos = juce::jmin (seletorSlot, proc.numPedais() - 1);
                if (pos >= proc.rolagem.load() + visiveis - 1) proc.rolagem = juce::jmin (maxRolagem(), pos - visiveis + 2);
            }
            seletorAberto = false; sujo();
        }
        return;
    }
    if (a == nullptr) return;

    if (e.mods.isPopupMenu())
    {
        if (a->slot >= 0 && (a->tipo == aPedal || a->tipo == aKnob || a->tipo == aFoot || a->tipo == aSwitch || a->tipo == aTreadle)) { menuPedal (a->slot); return; }
        if (a->tipo == aAB)
        {
            juce::PopupMenu m;
            m.addItem (1, proc.ab == 0 ? u8 ("Copiar A \xe2\x86\x92 B") : u8 ("Copiar B \xe2\x86\x92 A"));
            juce::Component::SafePointer<StompboxEditor> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                             [safe] (int r) { if (safe != nullptr && r == 1) safe->proc.copiaParaOutro(); });
            return;
        }
    }

    switch (a->tipo)
    {
        case aKnob: case aTreadle:
            if (auto* prm = proc.apvts.getParameter (a->pid))
            {
                alvoArrasto = *a; arrastandoKnob = true; valorIni = prm->getValue(); yIni = p.y;
                prm->beginChangeGesture();
            }
            break;
        case aSwitch:
            proc.define (a->pid, (float) ((juce::roundToInt (proc.real (a->pid)) + 1) % juce::jmax (1, a->idx))); break;
        case aFoot:   proc.alternaPedal (a->slot); break;
        case aPedal:  pedalArrastado = a->slot; xIni = p.x; yIni = p.y; arrastandoPedal = false; break;
        case aRemove: proc.removePedal (a->slot); break;
        case aVazio:  seletorAberto = true; seletorSlot = a->slot; seletorTroca = false; seletorCat = 0; break;
        case aPrev: case aNext:
        {
            const int np = proc.getNumPrograms();
            const int i = proc.presetAtual < 0 ? 0 : (proc.presetAtual + (a->tipo == aNext ? 1 : np - 1)) % np;
            proc.aplicaPreset (i); break;
        }
        case aLcd:    menuPreset(); break;
        case aSave:   pedeNomePreset(); break;
        case aAB:     proc.alternaAB(); break;
        case aSync:   proc.define ("sync", proc.real ("sync") > 0.5f ? 0.0f : 1.0f); break;
        case aTuner:  proc.afinador = ! proc.afinador.load(); afinVis = 0; break;
        case aHq:     proc.define ("hq", proc.real ("hq") > 0.5f ? 0.0f : 1.0f); break;
        case aMenu:
        {
            juce::PopupMenu m, tam;
            const int larguras[6] = { 960, 1120, 1280, 1440, 1600, 1920 };
            for (int i = 0; i < 6; ++i) tam.addItem (10 + i, juce::String (larguras[i]) + " x " + juce::String (larguras[i] * baseH / baseW), true, getWidth() == larguras[i]);
            m.addSubMenu ("Tamanho da janela", tam);
            m.addItem (2, "Abrir a pasta de presets");
            m.addSeparator();
            m.addItem (3, "Sobre o STOMPBOX");
            juce::Component::SafePointer<StompboxEditor> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, larguras] (int r)
            {
                if (safe == nullptr) return;
                if (r >= 10 && r < 16) safe->setSize (larguras[r - 10], larguras[r - 10] * baseH / baseW);
                else if (r == 2) { safe->proc.pastaUsuario().createDirectory(); safe->proc.pastaUsuario().startAsProcess(); }
                else if (r == 3)
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "STOMPBOX",
                        u8 ("STOMPBOX ") + JucePlugin_VersionString + u8 (" \xe2\x80\x94 InsideStudioSP\n\nPedalboard de guitarra: 10 pedais em s\xc3\xa9rie, "
                            "AMP + CAB e NOISE GATE.\nOs nomes dos pedais e amps s\xc3\xa3o pr\xc3\xb3prios; o som segue os circuitos cl\xc3\xa1ssicos."));
            });
            return;
        }
        case aSetaE:  rola (-1); return;
        case aSetaD:  rola (1); return;
        case aBarra:
            arrastandoBarra = true;
            proc.rolagem = juce::jlimit (0, maxRolagem(), juce::roundToInt ((p.x - 78.0f) / 1444.0f * (float) totalPosicoes() - visiveis * 0.5f));
            break;
        case aAmpModelo: proc.define ("amp", (float) a->idx); break;
        case aAmpOn:  proc.define ("ampon", proc.real ("ampon") > 0.5f ? 0.0f : 1.0f); break;
        case aCanal:  proc.define ("canal", proc.real ("canal") > 0.5f ? 0.0f : 1.0f); break;
        case aCab:    proc.define ("cab", (float) a->idx); break;
        case aMic:    proc.define ("mic", proc.real ("mic") > 0.5f ? 0.0f : 1.0f); break;
        default: break;
    }
    sujo();
}

void StompboxEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = base (e);
    mouse = p;
    if (arrastandoKnob)
    {
        const float faixa = alvoArrasto.tipo == aTreadle ? 200.0f : (e.mods.isShiftDown() ? 800.0f : 200.0f);
        mudaKnob (alvoArrasto, valorIni + (yIni - p.y) / faixa);
        sujo();   // responde já, sem esperar o timer
        return;
    }
    if (arrastandoBarra)
    {
        const int r = juce::jlimit (0, maxRolagem(), juce::roundToInt ((p.x - 78.0f) / 1444.0f * (float) totalPosicoes() - visiveis * 0.5f));
        if (r != proc.rolagem.load()) { proc.rolagem = r; sujo(); }
        return;
    }
    if (pedalArrastado >= 0)
    {
        if (! arrastandoPedal && p.getDistanceFrom ({ xIni, yIni }) > 8.0f) { arrastandoPedal = true; tickBorda = 0; cenaSuja = true; }
        if (arrastandoPedal)
        {
            xPedal = juce::jlimit (60.0f, 1540.0f, p.x);
            yPedal = p.y;
            atualizaDestino();
            repaint();
        }
    }
}

// vão mais perto do mouse: os vãos ficam 15 px antes de cada posição (x = 63 + 240·k na tela)
void StompboxEditor::atualizaDestino()
{
    const int n = proc.numPedais();
    destinoPedal = juce::jlimit (0, n, proc.rolagem.load() + juce::roundToInt ((xPedal - 63.0f) / 240.0f));
}

void StompboxEditor::mouseUp (const juce::MouseEvent&)
{
    if (arrastandoKnob)
        if (auto* prm = proc.apvts.getParameter (alvoArrasto.pid)) prm->endChangeGesture();
    if (arrastandoPedal && pedalArrastado >= 0 && destinoPedal >= 0) proc.movePedal (pedalArrastado, destinoFinal());
    arrastandoKnob = arrastandoBarra = arrastandoPedal = false;
    pedalArrastado = destinoPedal = -1;
    sujo();
}

void StompboxEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (seletorAberto) return;
    if (const auto* a = acha (base (e)))
        if ((a->tipo == aKnob || a->tipo == aTreadle) && a->pid.isNotEmpty()) { proc.define (a->pid, a->def); sujo(); }
}

void StompboxEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (seletorAberto) return;
    const auto p = base (e);
    const auto* a = acha (p);
    if (a != nullptr && (a->tipo == aKnob || a->tipo == aTreadle))
    {
        if (auto* prm = proc.apvts.getParameter (a->pid)) prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * 0.08f));
        sujo();
        return;
    }
    if (p.y < 540.0f)
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY;
        if (std::abs (d) > 0.01f) rola (d < 0 ? 1 : -1);
    }
}

// --------------------------------------------------------------------------------------------------------------
// menus

void StompboxEditor::menuPreset()
{
    juce::PopupMenu m, meus;
    const auto& ps = stomp::presetsFabrica();
    for (int i = 0; i < (int) ps.size(); ++i)
        m.addItem (1 + i, juce::String (i + 1).paddedLeft ('0', 2) + "  " + ps[(size_t) i].nome, true, proc.presetAtual == i);
    const auto arqs = proc.presetsUsuario();
    for (int i = 0; i < arqs.size(); ++i) meus.addItem (1000 + i, arqs[i].getFileNameWithoutExtension(), true, proc.presetAtual < 0 && proc.nomePreset == arqs[i].getFileNameWithoutExtension());
    if (arqs.isEmpty()) meus.addItem (999, "(nenhum ainda: use SAVE)", false);
    m.addSeparator();
    m.addSubMenu ("Meus presets", meus);
    m.addItem (500, "Salvar como novo...");
    juce::Component::SafePointer<StompboxEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, arqs] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= 1 && r < 500) safe->proc.aplicaPreset (r - 1);
        else if (r == 500) safe->pedeNomePreset();
        else if (r >= 1000 && r - 1000 < arqs.size()) safe->proc.abrePresetUsuario (arqs[r - 1000]);
        safe->sujo();
    });
}

void StompboxEditor::menuPedal (int slot)
{
    juce::PopupMenu m;
    const auto d = proc.le (slot);
    const int n = proc.numPedais();
    m.addSectionHeader (juce::String (infoPedal (d.tipo).nome) + u8 (" \xc2\xb7 posi\xc3\xa7\xc3\xa3o ") + juce::String (slot + 1));
    m.addItem (1, "Trocar pedal...");
    m.addItem (2, "Duplicar", n < stomp::numSlots);
    m.addItem (3, u8 ("Knobs no padr\xc3\xa3o"));
    m.addItem (4, "Mover para a esquerda", slot > 0);
    m.addItem (5, "Mover para a direita", slot < n - 1);
    m.addSeparator();
    m.addItem (6, "Remover");
    juce::Component::SafePointer<StompboxEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, slot, d, n] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        if (r == 1) { safe->seletorAberto = true; safe->seletorSlot = slot; safe->seletorTroca = true; safe->seletorCat = 0; }
        else if (r == 2 && n < stomp::numSlots)
        {
            pr.grava (n, d);              // cópia no fim e leva para o lado do original
            pr.movePedal (n, slot + 1);
        }
        else if (r == 3) { auto x = d; const auto& inf = infoPedal (d.tipo); for (int k = 0; k < 4; ++k) x.k[k] = k < inf.nk ? inf.def[k] : 5.0f; pr.grava (slot, x); }
        else if (r == 4) pr.movePedal (slot, slot - 1);
        else if (r == 5) pr.movePedal (slot, slot + 1);
        else if (r == 6) pr.removePedal (slot);
        safe->sujo();
    });
}

void StompboxEditor::pedeNomePreset()
{
    auto* w = new juce::AlertWindow ("Salvar preset", u8 ("Nome do preset (fica em Meus presets, no menu do visor)."), juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("nome", proc.presetAtual >= 0 ? proc.nomePreset + " 2" : proc.nomePreset);
    w->addButton ("Salvar", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<StompboxEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (safe == nullptr || r != 1) return;
        if (! safe->proc.salvaPresetUsuario (w->getTextEditorContents ("nome")))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "STOMPBOX", u8 ("N\xc3\xa3o consegui salvar o preset."));
        safe->sujo();
    }), true);
}
