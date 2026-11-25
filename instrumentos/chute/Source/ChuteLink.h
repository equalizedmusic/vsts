// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#pragma once
// CHUTE LINK — o CHUTE (escreve) avisa cada batida para o ALIGN e o BAREBASS (leem), binários diferentes no MESMO
// processo do DAW, sem sidechain. Mesmo esquema do MAESTRO LINK: memória compartilhada com nome pelo PID do processo
// (Windows: CreateFileMapping "Local\...", macOS / Linux: shm_open). DAW que isola cada plugin num processo (Bitwig
// com sandbox) = cada um abre a sua: quem lê fica em "NOT LINKED" e pede o sidechain.
//
// Cada cópia do CHUTE ocupa uma VAGA (até NSL) e, a cada batida, escreve o CORPO do kick já com todos os knobs:
// a fase (ciclos) e a amplitude a cada 1 ms, a posição na linha do tempo do DAW (amostra) e no relógio do processo.
// Com isso quem lê recria o corpo do kick na posição certa, amostra a amostra (o grave é o que importa para fase e
// ducking; o ataque não vai). A frequência é constante dentro de cada ms: a fase entre dois pontos é linear e a
// recriação é exata.
// Ordem das faixas: se o DAW processar o baixo antes do kick no mesmo bloco, a batida chega um bloco depois e quem lê
// recria só a parte que falta (a posição continua exata); se a faixa do kick andar na frente, a batida espera o bloco
// dela. 0.1.2: cada batida é ancorada uma vez (linha do tempo, conferida pelo relógio) e depois envelhece bloco a bloco
// (ancora / avanca), e a vaga só cai depois de 20 s sem áudio: o LINK não "pisca" mais. Tudo é medida: leitura rasgada é detectada (seqlock) e
// descartada. Versão do formato em kVersao (mudou = não liga).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace chutelink
{
constexpr uint32_t kMagic = 0x4b544843u;   // "CHTK"
constexpr uint32_t kVersao = 1;
constexpr int NPT = 1200;                   // corpo: até 1,2 s, um ponto por ms
constexpr int NHIT = 6;                     // últimas batidas guardadas por vaga
constexpr int NSL = 8;                      // cópias do CHUTE no projeto
constexpr int NNOME = 32;

inline int64_t agoraUs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::microseconds> (
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Batida
{
    std::atomic<uint32_t> seq;   // seqlock: ímpar = sendo escrita
    int64_t pos;                 // amostra da linha do tempo do DAW no início da batida (na taxa fs)
    double wallMs;               // relógio do processo (ms) no início da batida
    int32_t tocando;             // transporte do DAW rodando (pos vale)
    int32_t len;                 // pontos válidos em fase[] / amp[]
    float fs, notaMidi, fHz, sinal, t0;
    float fase[NPT];             // ciclos no início de cada ms
    float amp[NPT];              // amplitude do corpo no início de cada ms
};

struct Vaga
{
    std::atomic<uint32_t> dono;      // 0 = livre
    std::atomic<int64_t> vivoUs;     // última vez que o CHUTE desta vaga processou áudio
    std::atomic<uint32_t> nBat;      // batidas escritas (a última está em bat[(nBat - 1) % NHIT])
    std::atomic<float> notaMidi;     // nota do corpo agora (com TUNE e modo), para a tela de quem lê
    char nome[NNOME];                // nome da faixa (ou "CHUTE n")
    Batida bat[NHIT];
};

struct Area
{
    std::atomic<uint32_t> magic;
    std::atomic<uint32_t> versao;
    Vaga vaga[NSL];
};

static_assert (std::atomic<uint32_t>::is_always_lock_free && std::atomic<int64_t>::is_always_lock_free
                   && std::atomic<float>::is_always_lock_free,
               "CHUTE LINK: atômicos precisam ser livres de trava (memória compartilhada)");

// Abre (cria se não existe) a área deste processo (ChuteLink.cpp). nullptr = não deu (o plugin segue sem link).
Area* area();

// vaga sem áudio há mais de 20 s = CHUTE travado (o CHUTE apagado libera a vaga na hora, pelo dono). 0.1.2: era 1,5 s e
// o LINK caía sozinho quando o DAW deixava a faixa do kick um instante sem processar (o ALIGN e o BAREBASS "perdiam" o
// CHUTE e voltavam logo depois, sem as batidas que chegaram no meio)
constexpr int64_t vivoAteUs = 20000000;

inline bool viva (const Vaga& v, int64_t agora)
{
    return v.dono.load (std::memory_order_acquire) != 0 && agora - v.vivoUs.load (std::memory_order_relaxed) < vivoAteUs;
}

// ---------------------------------------------------------------- escreve (CHUTE)
class Publicador
{
public:
    ~Publicador() { libera(); }

    // pega uma vaga livre (ou abandonada há mais de 5 s). false = sem link (área não abriu ou 8 CHUTEs vivos)
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
            v.nBat.store (0);
            std::snprintf (v.nome, NNOME, "CHUTE %d", i + 1);
            vaga = &v; indice = i;
            return true;
        }
        return false;
    }

    void libera()
    {
        if (vaga != nullptr) { uint32_t d = id; vaga->dono.compare_exchange_strong (d, 0u); }
        vaga = nullptr; indice = -1;
    }

    void vivo (float notaMidi)
    {
        if (vaga == nullptr) return;
        vaga->vivoUs.store (agoraUs(), std::memory_order_relaxed);
        vaga->notaMidi.store (notaMidi, std::memory_order_relaxed);
    }

    void nome (const char* n)
    {
        if (vaga == nullptr || n == nullptr || *n == 0) return;
        char tmp[NNOME] {};
        std::strncpy (tmp, n, NNOME - 1);
        std::memcpy (vaga->nome, tmp, NNOME);
    }

    // escreve uma batida. fase/amp: len pontos a 1 ms
    void publica (int64_t pos, double wallMs, bool tocando, float fs, float notaMidi, float fHz, float sinal, float t0,
                  const float* fase, const float* amp, int len)
    {
        if (vaga == nullptr) return;
        const uint32_t n = vaga->nBat.load (std::memory_order_relaxed);
        Batida& b = vaga->bat[n % NHIT];
        b.seq.fetch_add (1, std::memory_order_acq_rel);   // ímpar
        std::atomic_thread_fence (std::memory_order_release);
        b.pos = pos; b.wallMs = wallMs; b.tocando = tocando ? 1 : 0;
        len = std::clamp (len, 0, NPT);
        b.len = len; b.fs = fs; b.notaMidi = notaMidi; b.fHz = fHz; b.sinal = sinal; b.t0 = t0;
        std::memcpy (b.fase, fase, sizeof (float) * (size_t) len);
        std::memcpy (b.amp, amp, sizeof (float) * (size_t) len);
        std::atomic_thread_fence (std::memory_order_release);
        b.seq.fetch_add (1, std::memory_order_acq_rel);   // par
        vaga->nBat.store (n + 1, std::memory_order_release);
    }

    int vagaIndice() const { return indice; }
    bool ligado() const { return vaga != nullptr; }

private:
    uint32_t id = (uint32_t) ((agoraUs() * 2654435761ull) | 1u);
    Vaga* vaga = nullptr;
    int indice = -1;
};

// ---------------------------------------------------------------- lê (ALIGN, BAREBASS)
// Cópia local das últimas batidas de UMA vaga: o áudio de quem lê só usa a cópia (sem ler a área no meio do bloco).
struct Local
{
    bool valida = false;
    int64_t pos = 0; double wallMs = 0; bool tocando = false;
    int len = 0; float fs = 48000, notaMidi = 0, fHz = 0, sinal = 1, t0 = 0;
    // 0.1.2: ÂNCORA. No 1º bloco em que quem lê vê a batida, o início dela é calculado UMA vez (em amostras a partir do
    // começo do bloco); depois ele só anda bloco a bloco (−n), igual ao som do kick, que continua tocando aconteça o que
    // acontecer com a linha do tempo (volta do loop, pulo na régua, exportação mais rápida que o tempo real).
    bool ancorada = false;
    double ini = 0.0;
    float fase[NPT] {}, amp[NPT] {};

    // corpo no instante t (s desde o início da batida)
    double corpo (double t) const
    {
        if (t < (double) t0) return 0.0;
        const double tm = t * 1000.0;
        const int k = (int) tm;
        if (k < 0 || k + 1 >= len) return 0.0;
        const double fr = tm - k;
        const double ph = fase[k] + fr * (fase[k + 1] - fase[k]);
        const double a = amp[k] + fr * (amp[k + 1] - amp[k]);
        return sinal * a * std::sin (6.283185307179586 * ph);
    }
    // fase (ciclos, já com o sinal: +0,5 se invertido) e amplitude no instante t. false = fora do corpo
    bool faseEm (double t, double& ciclos, double& a) const
    {
        const double tm = t * 1000.0;
        const int k = (int) tm;
        if (t < (double) t0 || k < 0 || k + 1 >= len) return false;
        const double fr = tm - k;
        ciclos = fase[k] + fr * (fase[k + 1] - fase[k]) + (sinal < 0 ? 0.5 : 0.0);
        a = amp[k] + fr * (amp[k + 1] - amp[k]);
        return true;
    }
    double duracao() const { return len / 1000.0; }
};

class Leitor
{
public:
    // nome preferido (vários CHUTEs no projeto): vazio = o primeiro vivo
    void prefere (const char* n) { std::memset (pref, 0, sizeof (pref)); if (n != nullptr) std::strncpy (pref, n, NNOME - 1); }

    // chamado no começo de cada bloco: escolhe a vaga e copia as batidas novas. true = há CHUTE vivo
    bool sincroniza()
    {
        Area* a = area();
        if (a == nullptr) { slot = -1; return false; }
        const int64_t agora = agoraUs();
        int escolhido = -1;
        if (pref[0] != 0)
            for (int i = 0; i < NSL; ++i)
                if (viva (a->vaga[i], agora) && std::strncmp (a->vaga[i].nome, pref, NNOME) == 0) { escolhido = i; break; }
        if (escolhido < 0 && slot >= 0 && viva (a->vaga[slot], agora)) escolhido = slot;
        if (escolhido < 0)
            for (int i = 0; i < NSL; ++i)
                if (viva (a->vaga[i], agora)) { escolhido = i; break; }
        if (escolhido != slot)
        {
            slot = escolhido;
            for (auto& l : local) l.valida = false;
            lidas = slot >= 0 ? a->vaga[slot].nBat.load (std::memory_order_acquire) : 0;
            if (lidas > 0) lidas -= 1;   // a última pode ainda estar soando
        }
        if (slot < 0) return false;
        const Vaga& v = a->vaga[slot];
        notaMidi = v.notaMidi.load (std::memory_order_relaxed);
        const uint32_t n = v.nBat.load (std::memory_order_acquire);
        if (n < lidas) lidas = n;   // a vaga foi reocupada
        if (n - lidas > (uint32_t) NHIT) lidas = n - NHIT;
        for (; lidas < n; ++lidas)
        {
            const Batida& b = v.bat[lidas % NHIT];
            Local& l = local[proxLocal];
            l.valida = false;
            for (int tent = 0; tent < 3; ++tent)
            {
                const uint32_t s0 = b.seq.load (std::memory_order_acquire);
                if (s0 & 1u) continue;
                l.pos = b.pos; l.wallMs = b.wallMs; l.tocando = b.tocando != 0; l.ancorada = false;
                l.len = std::clamp ((int) b.len, 0, NPT);
                l.fs = b.fs; l.notaMidi = b.notaMidi; l.fHz = b.fHz; l.sinal = b.sinal; l.t0 = b.t0;
                std::memcpy (l.fase, b.fase, sizeof (float) * (size_t) l.len);
                std::memcpy (l.amp, b.amp, sizeof (float) * (size_t) l.len);
                std::atomic_thread_fence (std::memory_order_acquire);
                if (b.seq.load (std::memory_order_acquire) == s0) { l.valida = l.len > 1 && l.fs > 1000.0f; break; }
                l.valida = false;
            }
            proxLocal = (proxLocal + 1) % NHIT;
        }
        return true;
    }

    // início da batida l, em amostras a partir do começo do bloco atual (taxa fs). posBloco: amostra da linha do
    // tempo no início do bloco (vale se tocando); wallBlocoMs: relógio no início do bloco
    // 0.1.2: a linha do tempo (exata) só vale se concordar com o relógio (até 150 ms). Discordou = o DAW pulou no tempo
    // (volta do loop, clique na régua): aí manda o relógio, que diz o que de fato está soando agora na faixa do kick.
    static double inicio (const Local& l, double fs, bool tocando, int64_t posBloco, double wallBlocoMs)
    {
        const double w = (l.wallMs - wallBlocoMs) * 0.0010000000000000000841004 * fs;
        if (tocando && l.tocando)
        {
            const double t = ((double) l.pos / (double) l.fs - (double) posBloco / fs) * fs;
            if (std::fabs (t - w) <= 0.15 * fs) return t;
        }
        return w;
    }

    // começo de cada bloco (depois do sincroniza): ancora as batidas novas. Batida que ainda vai começar (a faixa do kick
    // andou um bloco na frente) fica guardada para o bloco dela; só some se for mais de 2 s no futuro (0.1.2: antes ela
    // era jogada fora como "outra passada do loop" e o kick sumia para quem lê)
    void ancora (double fs, bool tocando, int64_t posBloco, double wallBlocoMs)
    {
        for (auto& l : local)
            if (l.valida && ! l.ancorada)
            {
                l.ini = inicio (l, fs, tocando, posBloco, wallBlocoMs);
                l.ancorada = true;
                if (l.ini > 2.0000000000000000969908 * fs) l.valida = false;
            }
    }

    // fim de cada bloco (ou trecho) de n amostras: as batidas envelhecem; a que já acabou sai
    void avanca (int n, double fs)
    {
        for (auto& l : local)
            if (l.valida && l.ancorada)
            {
                l.ini -= n;
                if (l.ini + l.duracao() * fs < 0.0) l.valida = false;
            }
    }

    // soma o corpo das batidas no trecho de n amostras (out já zerado) e avança. posBloco / wallBlocoMs = início do trecho
    void render (float* out, int n, double fs, bool tocando, int64_t posBloco, double wallBlocoMs)
    {
        ancora (fs, tocando, posBloco, wallBlocoMs);
        soma (out, 0, n, fs);
        avanca (n, fs);
    }

    // soma o corpo das amostras [de, de + n) do bloco atual em out[0..n) (out já zerado), SEM ancorar nem avançar:
    // para o BAREBASS, que ancora no começo do bloco, toca as notas e só avança no fim. Vale entre ancora() e avanca().
    void soma (float* out, int de, int n, double fs) const
    {
        for (auto& l : local)
        {
            if (! l.valida || ! l.ancorada || l.ini >= (double) (de + n)) continue;
            const int i0 = std::max (de, (int) std::ceil (l.ini));
            for (int i = i0; i < de + n; ++i) out[i - de] += (float) l.corpo ((i - l.ini) / fs);
        }
    }

    // a batida que soa no instante i do bloco (a mais recente que já começou): para o BAREBASS nascer em fase.
    // Vale entre ancora() e avanca() do bloco.
    const Local* soando (int i, double fs, double& t) const
    {
        const Local* melhor = nullptr; double tm = 1e30;
        for (auto& l : local)
        {
            if (! l.valida || ! l.ancorada) continue;
            const double dt = (i - l.ini) / fs;
            if (dt >= 0.0 && dt < l.duracao() && dt < tm) { tm = dt; melhor = &l; }
        }
        t = tm;
        return melhor;
    }

    // esquece as batidas guardadas (quem lê ficou blocos sem chamar render / avanca: as âncoras estão velhas)
    void esquece() { for (auto& l : local) l.valida = false; }

    bool ligado() const { return slot >= 0; }
    int vaga() const { return slot; }
    float nota() const { return notaMidi; }

private:
    char pref[NNOME] {};
    int slot = -1;
    uint32_t lidas = 0;
    int proxLocal = 0;
    float notaMidi = 0.0f;
    Local local[NHIT];
};

// nomes das vagas vivas (para a tela de quem lê escolher). Retorna quantas
inline int vivas (int* indices, char (*nomes)[NNOME])
{
    Area* a = area();
    if (a == nullptr) return 0;
    const int64_t agora = agoraUs();
    int k = 0;
    for (int i = 0; i < NSL; ++i)
        if (viva (a->vaga[i], agora))
        {
            indices[k] = i;
            std::memcpy (nomes[k], a->vaga[i].nome, NNOME);
            nomes[k][NNOME - 1] = 0;
            ++k;
        }
    return k;
}
} // namespace chutelink
