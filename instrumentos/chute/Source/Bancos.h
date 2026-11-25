// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
#pragma once
// CHUTE: bancos de presets (arquivo .chutebank), sem JUCE (a bancada em tools/ compila só com Dsp.h e este arquivo).
//
// Um banco = um arquivo com N presets. Cada preset traz o MODELO (o ataque a 44,1 kHz e as curvas do corpo a 1 ms) e,
// se quiser, valores de knobs (CLICK LEVEL, DRIVE…). O banco de FÁBRICA (os 50: SUB, PUNCH, TIGHT, BOOM) vai DENTRO
// do plugin (BinaryData); bancos novos são arquivos soltos na pasta de bancos (o instalador do pacote de presets põe
// lá) e aparecem no menu na próxima vez que o DAW abrir o CHUTE.
//
// Formato (little-endian), versão 1:
//   "CHTB" | u16 versao | u16 reservado | u64 semente | u32 tamanho | u32 crc32 (do conteúdo aberto) | conteúdo
//   conteúdo (embaralhado com a semente, ver mistura()):
//     txt pacote | txt autor | txt versaoDoPacote | u16 nPresets |
//     por preset: txt banco | txt nome | i16 nota | f32 fEnd t0 fase0 sinal fIni dropMs decayMs taxaClick |
//                 u16 nCurva | f32 d[nCurva] | f32 a[nCurva] | u16 nClick | i16 click[nClick] |
//                 u8 nAjustes | (txt id | f32 valor) × nAjustes
//   txt = u8 tamanho + bytes UTF-8
// O embaralhamento NÃO é cifra (a chave está no código): só deixa o arquivo ilegível para quem abre num editor e
// impede que ele seja lido como áudio. O crc32 recusa arquivo corrompido ou cortado.
// Gerador: tools/banco.py (Modelos.h → .chutebank, e o caminho inverso para editar).
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace chute
{
struct Modelo
{
    std::string banco, nome;
    int nota = 36;
    float fEnd = 50, t0 = 0, fase0 = 0, sinal = 1, fIni = 400, dropMs = 60, decayMs = 200;
    double taxaClick = 44100.0;
    std::vector<float> d, a;          // corpo: frequência / frequência da cauda, e amplitude, a 1 ms
    std::vector<int16_t> click;       // ataque
    std::vector<std::pair<std::string, float>> ajustes;   // knobs do preset (id do parâmetro, valor real)
    std::string pacote;               // de que arquivo veio ("FABRICA" = o de dentro do plugin)
};

namespace banco
{
constexpr uint64_t chaveMistura = 0x43485554454b494bull;   // "CHUTEKIK"
constexpr int versao = 1;

inline uint32_t crc32 (const uint8_t* p, size_t n)
{
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; ++i)
    {
        c ^= p[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

// xorshift64* a partir da semente do arquivo: embaralha / desembaralha (a mesma operação)
inline void mistura (uint8_t* p, size_t n, uint64_t semente)
{
    uint64_t s = semente ^ chaveMistura;
    if (s == 0) s = 0x9e3779b97f4a7c15ull;
    for (size_t i = 0; i < n; i += 8)
    {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        const uint64_t r = s * 0x2545f4914f6cdd1dull;
        for (size_t k = 0; k < 8 && i + k < n; ++k) p[i + k] ^= (uint8_t) (r >> (8 * k));
    }
}

class Leitor
{
public:
    Leitor (const uint8_t* p, size_t n) : p (p), n (n) {}
    bool ok() const { return bom; }
    bool fim() const { return i == n; }
    uint8_t u8() { return pega (1) ? p[i - 1] : 0; }
    uint16_t u16() { return pega (2) ? (uint16_t) (p[i - 2] | (p[i - 1] << 8)) : 0; }
    int16_t i16() { return (int16_t) u16(); }
    uint32_t u32() { if (! pega (4)) return 0; const uint8_t* q = p + i - 4; return (uint32_t) q[0] | ((uint32_t) q[1] << 8) | ((uint32_t) q[2] << 16) | ((uint32_t) q[3] << 24); }
    uint64_t u64() { const uint64_t lo = u32(); return lo | ((uint64_t) u32() << 32); }
    float f32() { const uint32_t u = u32(); float f; std::memcpy (&f, &u, 4); return f; }
    std::string txt() { const int t = u8(); if (! pega ((size_t) t)) return {}; return std::string ((const char*) p + i - t, (size_t) t); }
private:
    bool pega (size_t k) { if (! bom || n - i < k) { bom = false; return false; } i += k; return true; }
    const uint8_t* p; size_t n, i = 0; bool bom = true;
};

// lê um .chutebank inteiro. Devolve false (e não mexe em `saida`) se o arquivo não for um banco válido.
inline bool le (const void* dados, size_t tamanho, std::vector<Modelo>& saida, std::string* nomePacote = nullptr)
{
    const auto* b = (const uint8_t*) dados;
    if (dados == nullptr || tamanho < 24 || std::memcmp (b, "CHTB", 4) != 0) return false;
    Leitor h (b + 4, 20);
    const int ver = h.u16(); h.u16();
    const uint64_t semente = h.u64();
    const uint32_t tam = h.u32(), crc = h.u32();
    if (ver < 1 || ver > versao || tam != tamanho - 24) return false;
    std::vector<uint8_t> c (b + 24, b + tamanho);
    mistura (c.data(), c.size(), semente);
    if (crc32 (c.data(), c.size()) != crc) return false;

    Leitor r (c.data(), c.size());
    const std::string pacote = r.txt(); r.txt(); r.txt();
    const int np = r.u16();
    std::vector<Modelo> lidos;
    for (int k = 0; k < np && r.ok(); ++k)
    {
        Modelo m;
        m.pacote = pacote;
        m.banco = r.txt(); m.nome = r.txt(); m.nota = r.i16();
        m.fEnd = r.f32(); m.t0 = r.f32(); m.fase0 = r.f32(); m.sinal = r.f32(); m.fIni = r.f32();
        m.dropMs = r.f32(); m.decayMs = r.f32(); m.taxaClick = r.f32();
        const int nc = r.u16();
        m.d.resize ((size_t) nc); m.a.resize ((size_t) nc);
        for (auto& v : m.d) v = r.f32();
        for (auto& v : m.a) v = r.f32();
        const int nk = r.u16();
        m.click.resize ((size_t) nk);
        for (auto& v : m.click) v = r.i16();
        const int na = r.u8();
        for (int j = 0; j < na; ++j) { auto id = r.txt(); const float v = r.f32(); m.ajustes.emplace_back (std::move (id), v); }
        // sanidade: sem isso um banco ruim derrubaria o DAW no áudio
        if (m.banco.empty() || m.nome.empty() || nc < 2 || nc > 4000 || nk < 4 || nk > 44100 || ! (m.fEnd > 5.0f && m.fEnd < 2000.0000000000000208705f)
            || ! (m.taxaClick >= 8000.0 && m.taxaClick <= 192000.0) || m.nota < 0 || m.nota > 127) return false;
        lidos.push_back (std::move (m));
    }
    if (! r.ok() || ! r.fim() || lidos.empty()) return false;
    for (auto& m : lidos) saida.push_back (std::move (m));
    if (nomePacote) *nomePacote = pacote;
    return true;
}
} // namespace banco

// ---------------------------------------------------------------- catálogo (todos os modelos do processo)
// Montado UMA vez por processo (o primeiro CHUTE aberto) e nunca mais muda: o áudio lê sem trava. Ordem dos índices:
// a fábrica primeiro (os 50, nos mesmos índices de sempre: projetos antigos abrem no mesmo kick), depois os bancos
// soltos, em ordem alfabética do arquivo. Bancos instalados com o DAW aberto aparecem quando o DAW reabre.
class Catalogo
{
public:
    static Catalogo& get() { static Catalogo c; return c; }

    // fabrica = o banco de dentro do plugin; extras = o conteúdo de cada arquivo solto (já em ordem). Só a 1ª chamada vale.
    void monta (const void* fabrica, size_t nFabrica, const std::vector<std::vector<uint8_t>>& extras,
                std::vector<std::string>* recusados = nullptr)
    {
        if (montado) return;
        banco::le (fabrica, nFabrica, lista);
        nFab = (int) lista.size();
        for (size_t k = 0; k < extras.size(); ++k)
        {
            std::vector<Modelo> m;
            std::string nome;
            if (! banco::le (extras[k].data(), extras[k].size(), m, &nome)) { if (recusados) recusados->push_back (std::to_string (k)); continue; }
            for (auto& x : m)   // nome repetido (banco + preset) de outro pacote: o primeiro fica
            {
                bool tem = false;
                for (auto& y : lista) if (y.banco == x.banco && y.nome == x.nome) { tem = true; break; }
                if (! tem) lista.push_back (std::move (x));
            }
        }
        if (lista.empty()) lista.push_back (seguranca());
        montado = true;
    }

    int tamanho() const { return (int) lista.size(); }
    int fabrica() const { return nFab; }
    const Modelo& operator[] (int i) const { return lista[(size_t) std::clamp (i, 0, (int) lista.size() - 1)]; }
    int acha (const std::string& banco, const std::string& nome) const
    {
        for (size_t i = 0; i < lista.size(); ++i) if (lista[i].banco == banco && lista[i].nome == nome) return (int) i;
        return -1;
    }
    // nomes dos bancos na ordem do menu: os da fábrica em SUB, PUNCH, TIGHT, BOOM (a ordem dos índices é outra e não
    // pode mudar), depois os novos na ordem em que aparecem
    std::vector<std::string> bancos() const
    {
        std::vector<std::string> b;
        for (const char* f : { "SUB", "PUNCH", "TIGHT", "BOOM" })
            for (int i = 0; i < nFab; ++i) if (lista[(size_t) i].banco == f) { b.push_back (f); break; }
        for (auto& m : lista) if (std::find (b.begin(), b.end(), m.banco) == b.end()) b.push_back (m.banco);
        return b;
    }

private:
    Catalogo() = default;
    // sem nenhum banco (não deve acontecer): um seno de 50 Hz, para o plugin não tocar lixo nem cair
    static Modelo seguranca()
    {
        Modelo m; m.banco = "SUB"; m.nome = "SENO"; m.nota = 31; m.fEnd = 49.0f; m.fIni = 200.0f; m.dropMs = 40; m.decayMs = 200;
        m.d.assign (480, 1.0f); m.a.resize (480);
        for (int k = 0; k < 480; ++k) m.a[(size_t) k] = (float) std::max (0.0, 1.0 - k / 480.00000000000000971458);
        m.click.assign (64, 0); m.pacote = "FABRICA";
        return m;
    }
    std::vector<Modelo> lista;
    int nFab = 0;
    bool montado = false;
};

inline const Catalogo& catalogo() { return Catalogo::get(); }
inline int numModelos() { return catalogo().tamanho(); }
inline const Modelo& modelo (int i) { return catalogo()[i]; }
} // namespace chute
