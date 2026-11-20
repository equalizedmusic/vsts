#include "Tocador.h"

namespace mapa
{
namespace
{
constexpr double DOIS_PI = 6.283185307179586;

double mtof (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }

// ruído branco determinístico (o mesmo a cada vez que a nota toca)
float ruido (uint32_t k)
{
    k ^= k >> 16; k *= 0x7feb352dU; k ^= k >> 15; k *= 0x846ca68bU; k ^= k >> 16;
    return (float) (k & 0xffffff) / (float) 0x800000 - 1.0f;
}
} // namespace

Tocador::Tocador()
{
    for (auto& m : mudos) m.store (false);
    for (auto& s : solos) s.store (false);
    erro = dispositivo.initialiseWithDefaultDevices (0, 2);
    player.setSource (this);
    dispositivo.addAudioCallback (&player);
}

Tocador::~Tocador()
{
    dispositivo.removeAudioCallback (&player);
    player.setSource (nullptr);
}

void Tocador::carregar (const juce::AudioBuffer<float>& audio, double taxa, const Projeto& projeto)
{
    rodando = false;
    const juce::ScopedLock sl (trava);
    ref.makeCopyOf (audio);
    taxaRef = taxa;
    proj = projeto;
    notasPorFaixa.assign (proj.faixas.size(), {});
    for (size_t f = 0; f < proj.faixas.size(); ++f)
    {
        for (auto& c : proj.faixas[f].clipes)
            for (auto& n : c.notas) notasPorFaixa[f].push_back (&n);
        std::sort (notasPorFaixa[f].begin(), notasPorFaixa[f].end(),
                   [] (const Nota* a, const Nota* b) { return a->inicio < b->inicio; });
    }
    for (size_t i = 0; i < mudos.size(); ++i) { mudos[i].store (i != 0); solos[i].store (false); }   // ouve a referência; MIDI mudo
    posTempos = proj.audioInicioTempos;
    loopA = 0; loopB = 0;
}

void Tocador::descarregar()
{
    rodando = false;
    const juce::ScopedLock sl (trava);
    ref.setSize (0, 0);
    proj = Projeto {};
    notasPorFaixa.clear();
}

void Tocador::tocar() { if (ref.getNumSamples() > 0) rodando = true; }
void Tocador::parar() { rodando = false; }
bool Tocador::tocando() const { return rodando.load(); }
double Tocador::posicaoTempos() const { return posTempos.load(); }
void Tocador::irPara (double tempos) { posTempos = std::max (0.0, tempos); }
void Tocador::definirLoop (double inicio, double fim) { loopA = inicio; loopB = fim; }

void Tocador::definirMudo (int f, bool m) { if (f >= 0 && f < (int) mudos.size()) mudos[(size_t) f].store (m); }
void Tocador::definirSolo (int f, bool s) { if (f >= 0 && f < (int) solos.size()) solos[(size_t) f].store (s); }
bool Tocador::mudo (int f) const { return f >= 0 && f < (int) mudos.size() && mudos[(size_t) f].load(); }
bool Tocador::solo (int f) const { return f >= 0 && f < (int) solos.size() && solos[(size_t) f].load(); }

void Tocador::prepareToPlay (int, double taxa) { taxaSaida = taxa; }
void Tocador::releaseResources() {}

float Tocador::sintetiza (Elemento el, const Nota& n, double t) const
{
    const double dur = n.dur * proj.segPorTempo();
    const double vel = n.vel / 127.0;
    switch (el)
    {
        case Elemento::kick:
        {
            if (t > 0.35) return 0;
            const double fase = DOIS_PI * (45.0 * t + 100.0 * (1.0 - std::exp (-30.0 * t)) / 30.0);
            return (float) (std::sin (fase) * std::exp (-t * 9.0) * 0.8 * vel);
        }
        case Elemento::hats:
        {
            if (t > 0.06) return 0;
            const auto k = (uint32_t) (t * taxaSaida) + (uint32_t) (n.inicio * 977.0);
            return (ruido (k) - ruido (k + 1)) * (float) (std::exp (-t * 70.0) * 0.18 * vel);
        }
        case Elemento::clap:
        {
            if (t > 0.18) return 0;
            const auto k = (uint32_t) (t * taxaSaida) + 7919u;
            return ruido (k) * (float) (std::exp (-t * 22.0) * 0.3 * vel);
        }
        case Elemento::baixo:
        case Elemento::lead:
        case Elemento::pad:
        {
            if (t > dur + 0.03) return 0;
            const double f = mtof (n.tom);
            double env = std::min (1.0, t / (el == Elemento::pad ? 0.15 : 0.004));
            if (t > dur) env *= std::max (0.0, 1.0 - (t - dur) / 0.03);
            const double x = DOIS_PI * f * t;
            if (el == Elemento::baixo)
                return (float) ((std::sin (x) + 0.35 * std::sin (2 * x) + 0.15 * std::sin (3 * x)) * env * 0.32 * vel);
            if (el == Elemento::lead)
                return (float) ((std::sin (x) + 0.3 * std::sin (3 * x) + 0.12 * std::sin (5 * x)) * env * 0.12 * vel);
            return (float) ((std::sin (x) + 0.2 * std::sin (2 * x)) * env * 0.07 * vel);
        }
        case Elemento::referencia:
        case Elemento::fx:
            break;
    }
    return 0;
}

void Tocador::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();
    if (! rodando.load()) return;
    const juce::ScopedTryLock sl (trava);
    if (! sl.isLocked() || ref.getNumSamples() == 0 || proj.faixas.empty()) return;

    auto* L = info.buffer->getWritePointer (0, info.startSample);
    auto* R = info.buffer->getNumChannels() > 1 ? info.buffer->getWritePointer (1, info.startSample) : nullptr;
    const int n = info.numSamples;
    const double spb = proj.segPorTempo();
    const double passo = 1.0 / taxaSaida / spb;   // tempos por amostra de saída
    double pos = posTempos.load();
    const double la = loopA.load(), lb = loopB.load();
    const double fimProj = proj.audioFimTempos() + 0.5;

    bool algumSolo = false;
    for (size_t f = 0; f < proj.faixas.size() && f < solos.size(); ++f) algumSolo = algumSolo || solos[f].load();
    auto audivel = [&] (size_t f)
    {
        if (f >= mudos.size()) return false;
        return algumSolo ? solos[f].load() : ! mudos[f].load();
    };

    const bool ouveRef = audivel (0);
    const float* r0 = ref.getReadPointer (0);
    const float* r1 = ref.getReadPointer (ref.getNumChannels() > 1 ? 1 : 0);
    const int nRef = ref.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        if (lb > la && pos >= lb) pos = la;
        if (pos >= fimProj) { rodando = false; break; }
        float sl0 = 0, sr0 = 0;
        if (ouveRef)
        {
            const double s = proj.temposParaSegAudio (pos) * taxaRef;
            if (s >= 0 && s < nRef - 1)
            {
                const int k = (int) s;
                const float fr = (float) (s - k);
                sl0 = (r0[k] * (1 - fr) + r0[k + 1] * fr) * 0.85f;
                sr0 = (r1[k] * (1 - fr) + r1[k + 1] * fr) * 0.85f;
            }
        }
        L[i] = sl0;
        if (R) R[i] = sr0;
        pos += passo;
    }

    // sintetizador: notas das faixas MIDI audíveis que cruzam este bloco
    const double t0 = posTempos.load(), t1 = t0 + n * passo;
    if (t1 > t0 && ! (lb > la && t1 > lb))
    {
        for (size_t f = 1; f < proj.faixas.size(); ++f)
        {
            if (! audivel (f)) continue;
            const auto el = proj.faixas[f].elem;
            const auto& ns = notasPorFaixa[f];
            for (const Nota* nt : ns)
            {
                if (nt->inicio >= t1) break;
                const double fimNota = nt->inicio + nt->dur + 0.4 / spb;
                if (fimNota < t0) continue;
                for (int i = 0; i < n; ++i)
                {
                    const double tp = t0 + i * passo;
                    if (tp < nt->inicio) continue;
                    const float v = sintetiza (el, *nt, (tp - nt->inicio) * spb);
                    L[i] += v;
                    if (R) R[i] += v;
                }
            }
        }
    }
    for (int i = 0; i < n; ++i)
    {
        L[i] = juce::jlimit (-1.0f, 1.0f, L[i]);
        if (R) R[i] = juce::jlimit (-1.0f, 1.0f, R[i]);
    }
    posTempos = pos;
}
} // namespace mapa
