// Título da janela de fora em que o DAW pôs a do MAESTRO (o Ableton não manda o nome da faixa ao plugin, mas o põe
// no título: "3-Pad 02/MAESTRO"). Cópia da função da MESA (Ponte.cpp).
#include <cstdio>
#include <cstring>

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif
#if defined (__APPLE__)
 #include <objc/message.h>
 #include <objc/runtime.h>
#endif

namespace maestro
{
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
} // namespace maestro
