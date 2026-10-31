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
#if defined (__APPLE__)
 #include <objc/message.h>
 #include <objc/runtime.h>
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

int tituloDaJanela (void* nativo, char* out, int n)
{
    if (out == nullptr || n <= 0) return 0;
    out[0] = 0;
    if (nativo == nullptr) return 0;
#if defined (_WIN32)
    // a nossa janela é filha da janela do plugin que o DAW abriu: sobe até a de cima
    HWND raiz = GetAncestor (static_cast<HWND> (nativo), GA_ROOT);
    if (raiz == nullptr) return 0;
    wchar_t w[256] {};
    const int k = GetWindowTextW (raiz, w, 256);
    if (k <= 0) return 0;
    const int b = WideCharToMultiByte (CP_UTF8, 0, w, k, out, n - 1, nullptr, nullptr);
    out[b > 0 ? b : 0] = 0;
    return b > 0 ? b : 0;
#elif defined (__APPLE__)
    // [[view window] title] UTF8String, pelo runtime do Objective-C (o arquivo é C++)
    using Msg = id (*) (id, SEL);
    const auto msg = reinterpret_cast<Msg> (objc_msgSend);
    id janela = msg (static_cast<id> (nativo), sel_registerName ("window"));
    if (janela == nullptr) return 0;
    id titulo = msg (janela, sel_registerName ("title"));
    if (titulo == nullptr) return 0;
    const auto s = reinterpret_cast<const char* (*) (id, SEL)> (objc_msgSend) (titulo, sel_registerName ("UTF8String"));
    if (s == nullptr) return 0;
    std::snprintf (out, (size_t) n, "%s", s);
    return (int) std::strlen (out);
#else
    return 0;
#endif
}
} // namespace mesa
