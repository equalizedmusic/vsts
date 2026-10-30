#pragma once
// A ponte entre os dois plugins da MESA. A CONSOLE (no master) e o CANAL (em cada faixa) são binários diferentes:
// cada um tem a própria cópia do JUCE e das variáveis estáticas, então um SharedResourcePointer não serve. A ponte é
// um bloco de memória do PROCESSO (um nome por processo), achado pelos dois: cada CANAL aberto põe nele um ponteiro
// para a interface abaixo, e a CONSOLE lê e mexe nos canais por ela.
// Só tipos simples atravessam (float, int, char*): nada de juce::String, que é alocada no heap do outro binário.
// O áudio NÃO passa pela ponte: segue o roteamento do DAW (sends, returns e grupos continuam valendo).
// Tudo que usa os canais roda na thread da interface (é nela que o DAW cria e apaga os plugins).
#include <atomic>
#include <cstdint>

namespace mesa
{
struct Canal
{
    virtual int   abi() const = 0;                                     // mesma versão da ponte nos dois lados
    virtual int   tipo() const = 0;                                    // 0 = canal, 1 = grupo, 2 = console
    virtual int   numero() const = 0;                                  // ordem de criação (o número no LED)
    virtual bool  tem (const char* pid) const = 0;
    virtual float valor (const char* pid) const = 0;                   // valor real (dB, Hz, índice da escolha…)
    virtual float norm (const char* pid) const = 0;                    // 0..1
    virtual float normPadrao (const char* pid) const = 0;
    virtual void  mexe (const char* pid, float norm) = 0;              // avisa o DAW (automação)
    virtual void  gesto (const char* pid, bool inicio) = 0;
    virtual int   texto (const char* pid, char* out, int n) const = 0; // valor atual escrito (UTF-8)
    virtual int   nome (char* out, int n) const = 0;                   // nome da fita (UTF-8)
    virtual void  renomeia (const char* utf8) = 0;
    virtual std::uint32_t cor() const = 0;                             // ARGB da faixa no DAW
    virtual void  medidores (float* out6) const = 0;                   // pico L, pico R, RMS L, RMS R, GR (dB), entrada (dB)
protected:
    ~Canal() = default;
};

struct Ponte
{
    static constexpr std::uint32_t marca = 0x4d455341u;   // "MESA"
    static constexpr int abiVersao = 1, vagas = 256;

    // o bloco nasce zerado (memória nova do sistema): zero é um estado válido para tudo aqui
    std::atomic<std::uint32_t> assinatura;
    std::atomic<int> trava;
    std::atomic<int> versao;                 // muda quando um canal entra, sai ou muda de nome/tipo
    std::atomic<int> consoles;               // quantas CONSOLEs abertas (o CANAL avisa se não há nenhuma)
    std::atomic<Canal*> vaga[vagas];

    static Ponte& get();                     // Ponte.cpp: acha (ou cria) o bloco do processo

    void entra (Canal* c)
    {
        trava_();
        for (auto& v : vaga) if (v.load() == nullptr) { v.store (c); break; }
        solta_();
        ++versao;
    }
    void sai (Canal* c)
    {
        trava_();
        for (auto& v : vaga) if (v.load() == c) v.store (nullptr);
        solta_();
        ++versao;
    }
    // copia os canais abertos (da mesma versão da ponte) para out; devolve quantos
    int lista (Canal** out, int max)
    {
        int n = 0;
        trava_();
        for (auto& v : vaga)
            if (auto* c = v.load(); c != nullptr && n < max && c->abi() == abiVersao) out[n++] = c;
        solta_();
        return n;
    }

private:
    void trava_() { int z = 0; while (! trava.compare_exchange_weak (z, 1, std::memory_order_acquire)) z = 0; }
    void solta_() { trava.store (0, std::memory_order_release); }
};
} // namespace mesa
