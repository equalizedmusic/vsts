// MAESTRO LINK: abre a memória compartilhada (ver MaestroLink.h). Fica num .cpp para o windows.h não vazar nos
// cabeçalhos do plugin. O MAGOS compila este mesmo arquivo (Source/Link.cpp dele inclui este).
#include "MaestroLink.h"
#include <cstdio>

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#else
 #include <fcntl.h>
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <unistd.h>
#endif

namespace maestrolink
{
// Abre (cria se não existe) a área deste processo. Uma por binário, aberta na primeira chamada e mantida até o fim
// do processo (o DAW descarrega o binário junto). nullptr = não deu (o plugin segue sem link).
Area* area()
{
    struct Aberta
    {
        Area* a = nullptr;
        Aberta()
        {
            const size_t tam = sizeof (Area);
#if defined (_WIN32)
            wchar_t nome[96];
            swprintf (nome, 96, L"Local\\InsideStudioSP_MaestroLink%u_%lu", (unsigned) kVersao, (unsigned long) GetCurrentProcessId());
            HANDLE h = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD) tam, nome);
            if (h == nullptr) return;
            void* p = MapViewOfFile (h, FILE_MAP_ALL_ACCESS, 0, 0, tam);
            if (p == nullptr) { CloseHandle (h); return; }
            a = static_cast<Area*> (p);   // a alça fica aberta até o processo acabar (de propósito)
#else
            char nome[48];
            std::snprintf (nome, sizeof (nome), "/isp_mlk%u_%ld", (unsigned) kVersao, (long) getpid());
            const int fd = shm_open (nome, O_CREAT | O_RDWR, 0600);
            if (fd < 0) return;
            struct stat st {};
            // macOS: ftruncate numa área que já tem tamanho dá EINVAL; só quem cria aumenta
            if (fstat (fd, &st) != 0 || ((size_t) st.st_size < tam && ftruncate (fd, (off_t) tam) != 0)) { close (fd); return; }
            void* p = mmap (nullptr, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            close (fd);
            if (p == MAP_FAILED) return;
            a = static_cast<Area*> (p);
#endif
            // memória nova vem zerada: quem chega primeiro marca o formato
            uint32_t zero = 0;
            if (a->magic.compare_exchange_strong (zero, kMagic)) a->versao.store (kVersao);
            for (int i = 0; i < 100000 && a->versao.load() == 0; ++i) {}   // o outro binário acabou de marcar
            if (a->magic.load() != kMagic || a->versao.load() != kVersao) a = nullptr;   // outro formato: sem link
        }
    };
    static Aberta aberta;
    return aberta.a;
}
} // namespace maestrolink
