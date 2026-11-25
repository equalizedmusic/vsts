// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#pragma once
// BASS LINK — o BAREBASS (escreve) avisa cada nota que toca para o RESENHA (lê), binários diferentes no MESMO processo
// do DAW, sem MIDI roteado. Mesmo esquema do CHUTE LINK (ChuteLink.h): memória compartilhada com nome pelo PID do
// processo. DAW que isola cada plugin num processo (Bitwig com sandbox) = cada um abre a sua: o RESENHA fica sem baixo.
//
// Cada cópia do BAREBASS ocupa uma VAGA e, a cada nota, escreve a nota MIDI e a posição em tempos (PPQ) da linha do
// tempo do DAW. O RESENHA usa isso para: seguir a nota do baixo (SEGUE) e tocar nos buracos do groove do baixo
// (RESPONDE), aprendendo o desenho do baixo nos últimos compassos. Leitura rasgada é detectada (seqlock) e descartada.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace basslink
{
constexpr uint32_t kMagic = 0x4b425342u;   // "BSBK"
constexpr uint32_t kVersao = 1;
constexpr int NEV = 64;                     // últimas notas guardadas por vaga
constexpr int NSL = 4;                      // cópias do BAREBASS no projeto

inline int64_t agoraUs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::microseconds> (
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Evento
{
    std::atomic<uint32_t> seq;   // seqlock: ímpar = sendo escrito
    double ppq;                  // posição da nota (tempos, linha do tempo do DAW); vale se tocando
    int32_t nota;                // nota MIDI
    int32_t tocando;             // transporte rodando quando a nota entrou
};

struct Vaga
{
    std::atomic<uint32_t> dono;      // 0 = livre
    std::atomic<int64_t> vivoUs;     // última vez que o BAREBASS desta vaga processou áudio
    std::atomic<uint32_t> nEv;       // notas escritas (a última está em ev[(nEv - 1) % NEV])
    std::atomic<int32_t> notaAgora;  // nota soando agora (-1 = nenhuma)
    Evento ev[NEV];
};

struct Area
{
    std::atomic<uint32_t> magic;
    std::atomic<uint32_t> versao;
    Vaga vaga[NSL];
};

static_assert (std::atomic<uint32_t>::is_always_lock_free && std::atomic<int64_t>::is_always_lock_free
                   && std::atomic<int32_t>::is_always_lock_free,
               "BASS LINK: atômicos precisam ser livres de trava (memória compartilhada)");

// Abre (cria se não existe) a área deste processo (BassLink.cpp). nullptr = não deu (o plugin segue sem link).
Area* area();

constexpr int64_t vivoAteUs = 20000000;   // 20 s sem áudio = BAREBASS travado (igual ao CHUTE LINK)

inline bool viva (const Vaga& v, int64_t agora)
{
    return v.dono.load (std::memory_order_acquire) != 0 && agora - v.vivoUs.load (std::memory_order_relaxed) < vivoAteUs;
}

// ---------------------------------------------------------------- escreve (BAREBASS)
class Publicador
{
public:
    ~Publicador() { libera(); }

    bool ocupa()
    {
        if (vaga != nullptr) return true;
        Area* a = area();
        if (a == nullptr) return false;
        const int64_t agora = agoraUs();
        for (int i = 0; i < NSL; ++i)
        {
            Vaga& v = a->vaga[i];
            uint32_t d = v.dono.load();
            const bool largada = d != 0 && agora - v.vivoUs.load() > 5000000;
            if (d != 0 && ! largada) continue;
            if (! v.dono.compare_exchange_strong (d, id)) continue;
            v.vivoUs.store (agora);
            v.nEv.store (0);
            v.notaAgora.store (-1);
            vaga = &v;
            return true;
        }
        return false;
    }

    void libera()
    {
        if (vaga != nullptr) { vaga->notaAgora.store (-1); uint32_t d = id; vaga->dono.compare_exchange_strong (d, 0u); }
        vaga = nullptr;
    }

    void vivo (int notaAgora)
    {
        if (vaga == nullptr) return;
        vaga->vivoUs.store (agoraUs(), std::memory_order_relaxed);
        vaga->notaAgora.store (notaAgora, std::memory_order_relaxed);
    }

    void nota (int n, double ppq, bool tocando)
    {
        if (vaga == nullptr) return;
        const uint32_t k = vaga->nEv.load (std::memory_order_relaxed);
        Evento& e = vaga->ev[k % NEV];
        e.seq.fetch_add (1, std::memory_order_acq_rel);
        std::atomic_thread_fence (std::memory_order_release);
        e.ppq = ppq; e.nota = n; e.tocando = tocando ? 1 : 0;
        std::atomic_thread_fence (std::memory_order_release);
        e.seq.fetch_add (1, std::memory_order_acq_rel);
        vaga->nEv.store (k + 1, std::memory_order_release);
    }

    bool ligado() const { return vaga != nullptr; }

private:
    uint32_t id = (uint32_t) ((agoraUs() * 2654435761ull) | 1u);
    Vaga* vaga = nullptr;
};

// ---------------------------------------------------------------- lê (RESENHA)
// Aprende o desenho do baixo numa grade de semicolcheias de 4 compassos (64 casas): cada casa guarda a última nota
// que entrou nela e quando (PPQ). Casa ocupada = o baixo tocou ali nos últimos 4 compassos.
class Leitor
{
public:
    static constexpr int NCASA = 64;
    Leitor() { esquece(); }

    // começo de cada bloco: copia as notas novas. true = há BAREBASS vivo
    bool sincroniza()
    {
        Area* a = area();
        if (a == nullptr) { slot = -1; return false; }
        const int64_t agora = agoraUs();
        int escolhido = (slot >= 0 && viva (a->vaga[slot], agora)) ? slot : -1;
        if (escolhido < 0)
            for (int i = 0; i < NSL; ++i)
                if (viva (a->vaga[i], agora)) { escolhido = i; break; }
        if (escolhido != slot)
        {
            slot = escolhido;
            esquece();
            lidas = slot >= 0 ? a->vaga[slot].nEv.load (std::memory_order_acquire) : 0;
        }
        if (slot < 0) return false;
        const Vaga& v = a->vaga[slot];
        agoraNota = v.notaAgora.load (std::memory_order_relaxed);
        const uint32_t n = v.nEv.load (std::memory_order_acquire);
        if (n < lidas) lidas = n;
        if (n - lidas > (uint32_t) NEV) lidas = n - NEV;
        for (; lidas < n; ++lidas)
        {
            const Evento& e = v.ev[lidas % NEV];
            for (int tent = 0; tent < 3; ++tent)
            {
                const uint32_t s0 = e.seq.load (std::memory_order_acquire);
                if (s0 & 1u) continue;
                const double ppq = e.ppq; const int nota = e.nota; const bool toc = e.tocando != 0;
                std::atomic_thread_fence (std::memory_order_acquire);
                if (e.seq.load (std::memory_order_acquire) != s0) continue;
                ultimaNota = nota;
                hist[(size_t) (((nota % 12) + 12) % 12)] += 1.0f;
                if (toc)
                {
                    const long long c = (long long) std::floor (ppq * 4.0 + 0.50000000000000000780895);
                    const int idx = (int) (((c % NCASA) + NCASA) % NCASA);
                    casaPpq[idx] = ppq; casaNota[idx] = nota;
                }
                break;
            }
        }
        // o histograma esquece devagar (as notas recentes mandam no tom)
        for (auto& h : hist) h *= 0.9995000000000000158439f;
        return true;
    }

    // o baixo tocou na semicolcheia que contém ppq (nos últimos 4 compassos)?
    bool ocupada (double ppq) const
    {
        const long long c = (long long) std::floor (ppq * 4.0 + 0.5);
        const int idx = (int) (((c % NCASA) + NCASA) % NCASA);
        const double q = casaPpq[idx];
        return q > -1.0e8 && q > ppq - 16.5 && q < ppq + 16.5;   // 16 tempos = 4 compassos (volta de loop também vale)
    }

    // nota do baixo para SEGUIR: a que soa agora; senão a última que tocou (-1 = nenhuma ainda)
    int notaSeguir() const { return agoraNota >= 0 ? agoraNota : ultimaNota; }

    // tônica provável do baixo (classe de nota mais tocada; -1 = sem notas ainda)
    int tonica() const
    {
        int best = -1; float bv = 0.5f;
        for (int i = 0; i < 12; ++i) if (hist[(size_t) i] > bv) { bv = hist[(size_t) i]; best = i; }
        return best;
    }

    void esquece()
    {
        for (int i = 0; i < NCASA; ++i) { casaPpq[i] = -1.0e9; casaNota[i] = -1; }
        for (auto& h : hist) h = 0.0f;
        ultimaNota = -1; agoraNota = -1;
    }

    bool ligado() const { return slot >= 0; }

private:
    int slot = -1;
    uint32_t lidas = 0;
    int agoraNota = -1, ultimaNota = -1;
    double casaPpq[NCASA] {};
    int casaNota[NCASA] {};
    float hist[12] {};
};
} // namespace basslink
