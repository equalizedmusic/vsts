#pragma once
// Todas as cópias da MESA abertas no mesmo DAW se conhecem por aqui: a janela de qualquer uma mostra a console
// inteira (todos os canais, os grupos e o MASTER) e mexe nos parâmetros de cada cópia.
// Funciona quando o DAW carrega os plugins no mesmo processo (Ableton, Logic, Reaper no padrão, FL, Studio One…);
// com plugin isolado em processo próprio, cada janela só enxerga o próprio canal.
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class MesaProcessor;

namespace mesa
{
struct Registry
{
    struct Listener { virtual ~Listener() = default; virtual void consoleChanged() = 0; };

    juce::CriticalSection lock;
    juce::Array<MesaProcessor*> list;      // em ordem de criação
    juce::ListenerList<Listener> listeners;
    std::atomic<int> soloCount { 0 };      // canais com SOLO ligado
    std::atomic<int> meterMode { 0 };      // 0 = VU, 1 = PPM (só a tela)
    std::atomic<int> version { 0 };
    int nextSeq = 1;

    int add (MesaProcessor* p)
    {
        int seq;
        {
            const juce::ScopedLock sl (lock);
            list.add (p);
            seq = nextSeq++;
        }
        changed();
        return seq;
    }
    void remove (MesaProcessor* p)
    {
        {
            const juce::ScopedLock sl (lock);
            list.removeFirstMatchingValue (p);
        }
        changed();
    }
    // chamado também quando um canal muda de modo, de nome ou de grupo
    void changed()
    {
        ++version;
        if (juce::MessageManager::existsAndIsCurrentThread())
            listeners.call ([] (Listener& l) { l.consoleChanged(); });
    }
};
} // namespace mesa
