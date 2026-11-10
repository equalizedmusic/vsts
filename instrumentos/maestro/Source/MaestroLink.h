#pragma once
// MAESTRO LINK — ponte entre o MAESTRO (escreve) e o MASTER DOS MAGOS (lê), dois binários diferentes no MESMO
// processo do DAW. O maestro::Registry é estático DENTRO do binário do MAESTRO, então o MAGOS não o enxerga: os dois
// abrem uma memória compartilhada com nome pelo PID do processo (Windows: CreateFileMapping "Local\...",
// macOS / Linux: shm_open). DAW que isola cada plugin num processo (Bitwig com sandbox) = cada um abre a sua: o MAGOS
// fica em "NOT LINKED", sem erro.
//
// Cada cópia do MAESTRO escreve na vaga com o MESMO índice da vaga dela no Registry (0..255), ~10 vezes por segundo,
// e incrementa "batida". O MAGOS considera viva a vaga cuja batida mudou há menos de ~1,5 s (cópia apagada, DAW que
// parou de chamar o processBlock ou plugin que travou somem sozinhos).
// Tudo é medida: leitura rasgada entre duas escritas não faz mal. Versão do formato em kVersao (mudou = não liga).
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>


namespace maestrolink
{
constexpr uint32_t kMagic = 0x314b4c4du;   // "MLK1"
constexpr uint32_t kVersao = 1;
constexpr int NB = 27;                      // bandas de 1/3 de oitava, 40·2^(b/3) Hz (as mesmas do MAESTRO)
constexpr int NR = 8;                       // KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX
constexpr int NSL = 256;

inline const char* nomeGrupo (int r)
{
    static const char* n[NR] = { "KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "AMBIENCE", "VOCAL", "FX" };
    return r >= 0 && r < NR ? n[r] : "?";
}

struct Vaga
{
    std::atomic<uint32_t> batida;           // 0 = livre
    std::atomic<int32_t> role;              // grupo resolvido (−1 = livre)
    std::atomic<int32_t> tocando;
    std::atomic<int32_t> focus;
    std::atomic<float> nivelDb;             // nível K da saída do MAESTRO nesta faixa (lento)
    std::atomic<float> trimDb;              // ganho que o MAESTRO aplica agora (LOCK + TRIM)
    std::atomic<float> espectro[NB];        // potência por banda DEPOIS do MAESTRO (lento, ~300 ms)
};

struct Area
{
    std::atomic<uint32_t> magic;
    std::atomic<uint32_t> versao;
    std::atomic<int32_t> style;             // STYLE do MAESTRO (0..5)
    std::atomic<int32_t> travado;           // 1 = projeto em LOCK
    std::atomic<int32_t> topo;              // KEEP KICK / BASS ON TOP
    std::atomic<float> alvoRole[NR];        // nível de cada grupo relativo ao KICK no STYLE (dB)
    char nomeStyle[24];
    Vaga vaga[NSL];
};

static_assert (std::atomic<uint32_t>::is_always_lock_free && std::atomic<float>::is_always_lock_free,
               "MAESTRO LINK: atômicos de 32 bits precisam ser livres de trava (memória compartilhada)");

// Abre (cria se não existe) a área deste processo (MaestroLink.cpp). nullptr = não deu (o plugin segue sem link).
Area* area();
} // namespace maestrolink
