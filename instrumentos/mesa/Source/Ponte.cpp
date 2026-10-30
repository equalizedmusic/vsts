// Acha (ou cria) o bloco da ponte do processo. Windows: memória compartilhada com nome por processo (o sistema
// garante um só bloco, mesmo com os dois plugins carregando ao mesmo tempo). Mac/Linux: o endereço fica numa
// variável de ambiente do processo (a única coisa que os dois binários dividem); quem chega primeiro cria.
#include "Ponte.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#else
 #include <sys/mman.h>
 #include <unistd.h>
#endif

namespace mesa
{
static Ponte* abre()
{
#if defined (_WIN32)
    wchar_t nome[96];
    swprintf (nome, 96, L"Local\\InsideStudioSP.MESA.ponte.%lu", (unsigned long) GetCurrentProcessId());
    HANDLE h = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD) sizeof (Ponte), nome);
    if (h == nullptr) return nullptr;
    // o handle fica aberto até o processo acabar: o bloco não pode sumir enquanto algum plugin o usa
    return static_cast<Ponte*> (MapViewOfFile (h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (Ponte)));
#else
    static const char* var = "INSIDESTUDIOSP_MESA_PONTE";
    if (const char* s = std::getenv (var))
        if (void* p = nullptr; std::sscanf (s, "%p", &p) == 1 && p != nullptr) return static_cast<Ponte*> (p);
    void* m = mmap (nullptr, sizeof (Ponte), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (m == MAP_FAILED) return nullptr;
    char buf[40];
    std::snprintf (buf, sizeof (buf), "%p", m);
    setenv (var, buf, 0);                                   // 0: se outro criou no meio, vale o dele
    if (const char* s = std::getenv (var))
        if (void* p = nullptr; std::sscanf (s, "%p", &p) == 1 && p != nullptr && p != m) { munmap (m, sizeof (Ponte)); return static_cast<Ponte*> (p); }
    return static_cast<Ponte*> (m);
#endif
}

Ponte& Ponte::get()
{
    static Ponte* p = []
    {
        Ponte* q = abre();
        if (q == nullptr) { static Ponte local {}; q = &local; }   // sem ponte: cada binário fica com a sua
        std::uint32_t z = 0;
        q->assinatura.compare_exchange_strong (z, marca);
        return q;
    }();
    return *p;
}
} // namespace mesa
