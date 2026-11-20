// MAPA — motores de IA. Contrato em IA.h.
#include "IA.h"

#include "demucs.hpp"   // terceiros/demucs_onnx (inclui Eigen e onnxruntime_cxx_api.h)

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <queue>
#include <thread>

namespace mapa
{
constexpr double PI_ = 3.14159265358979323846;
namespace
{
struct Cancelado {};

int numThreads (int pedido)
{
    if (pedido > 0) return pedido;
    const unsigned n = std::thread::hardware_concurrency();
    return (int) std::clamp (n == 0 ? 4u : n, 1u, 16u);
}

bool lerArquivo (const std::string& caminhoUtf8, std::vector<char>& dados)
{
#if defined(_WIN32)
    std::ifstream f (std::filesystem::u8path (caminhoUtf8), std::ios::binary);
#else
    std::ifstream f (caminhoUtf8, std::ios::binary);
#endif
    if (! f) return false;
    f.seekg (0, std::ios::end);
    const std::streamoff n = f.tellg();
    if (n <= 0) return false;
    f.seekg (0);
    dados.resize ((size_t) n);
    f.read (dados.data(), n);
    return (bool) f;
}

Ort::Env& ambiente()
{
    static Ort::Env env (ORT_LOGGING_LEVEL_ERROR, "mapa");
    return env;
}
} // namespace

// =================================================================================================== Separador
struct Separador::Impl
{
    demucsonnx::demucs_model modelo;
    bool ok = false;
};

Separador::Separador() : impl (new Impl) {}
Separador::~Separador() = default;
bool Separador::carregado() const { return impl->ok; }

bool Separador::carregar (const std::string& caminho, std::string& erro, int threads)
{
    impl.reset (new Impl);
    std::vector<char> dados;
    if (! lerArquivo (caminho, dados)) { erro = "modelo de separação não encontrado: " + caminho; return false; }
    try
    {
        Ort::SessionOptions op;
        op.SetIntraOpNumThreads (numThreads (threads));
        op.SetInterOpNumThreads (1);
        op.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (! demucsonnx::load_model (dados, impl->modelo, op) || impl->modelo.nb_sources != 4)
        {
            erro = "modelo de separação inválido";
            return false;
        }
    }
    catch (const std::exception& e) { erro = std::string ("erro ao abrir o modelo de separação: ") + e.what(); return false; }
    impl->ok = true;
    return true;
}

bool Separador::separar (const std::vector<float>& L, const std::vector<float>& R, Stems& saida,
                         FnProgresso progresso, const std::atomic<bool>* cancelar, std::string& erro)
{
    if (! impl->ok) { erro = "modelo de separação não carregado"; return false; }
    const size_t n = std::min (L.size(), R.size());
    if (n < 44100) { erro = "áudio curto demais"; return false; }
    Eigen::MatrixXf audio (2, (Eigen::Index) n);
    for (size_t i = 0; i < n; ++i) { audio (0, (Eigen::Index) i) = L[i]; audio (1, (Eigen::Index) i) = R[i]; }

    try
    {
        Eigen::Tensor3dXf out = demucsonnx::demucs_inference (impl->modelo, audio,
            [&] (float p, const std::string&)
            {
                if (cancelar && cancelar->load()) throw Cancelado {};
                if (progresso) progresso (std::clamp (p, 0.0f, 1.0f));
            });
        audio.resize (0, 0);
        saida.sampleRate = 44100;
        for (int s = 0; s < numStems; ++s)
            for (int c = 0; c < 2; ++c)
            {
                auto& v = saida.canal[(size_t) s][(size_t) c];
                v.resize (n);
                std::memcpy (v.data(), &out (s, c, 0), n * sizeof (float));   // RowMajor: [s][c][t] contíguo em t
            }
    }
    catch (const Cancelado&) { erro = "cancelado"; return false; }
    catch (const std::exception& e) { erro = std::string ("erro na separação: ") + e.what(); return false; }
    return true;
}

// =================================================================================================== Transcritor
// Porte do Basic Pitch (basic_pitch/inference.py + note_creation.py, Apache 2.0) para C++.
namespace bp
{
constexpr int SR = 22050, HOP = 256, NWIN = 43844, FR_JANELA = 172, FR_SOBRE = 30;
constexpr int SOBRE_LEN = FR_SOBRE * HOP;            // 7680
constexpr int PASSO = NWIN - SOBRE_LEN;              // 36164
constexpr int NOTAS = 88, OFFSET_MIDI = 21, MAX_FREQ_IDX = 87, CONTORNOS = 264;
constexpr double MAGIC = 0.0018;
constexpr int TOL_ENERGIA = 11;

double tempoDoQuadro (int f)
{
    const double original = (double) f * HOP / SR;
    const double offsetJanela = (double) HOP / SR * (FR_JANELA - (double) NWIN / HOP) + MAGIC;
    return original - offsetJanela * std::floor ((double) f / FR_JANELA);
}

int idxDeHz (double hz) { return (int) std::lround (12.0 * std::log2 (hz / 440.0) + 69.0 - OFFSET_MIDI); }
} // namespace bp

struct Transcritor::Impl
{
    std::unique_ptr<Ort::Session> sessao;
};

Transcritor::Transcritor() : impl (new Impl) {}
Transcritor::~Transcritor() = default;
bool Transcritor::carregado() const { return impl->sessao != nullptr; }

bool Transcritor::carregar (const void* modelo, size_t bytes, std::string& erro, int threads)
{
    try
    {
        Ort::SessionOptions op;
        op.SetIntraOpNumThreads (numThreads (threads));
        op.SetInterOpNumThreads (1);
        op.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
        impl->sessao = std::make_unique<Ort::Session> (ambiente(), modelo, bytes, op);
    }
    catch (const std::exception& e) { erro = std::string ("erro ao abrir o modelo de notas: ") + e.what(); return false; }
    return true;
}

bool Transcritor::transcrever (const std::vector<float>& x, const ParametrosNotas& par, std::vector<NotaAudio>& saida,
                               FnProgresso progresso, const std::atomic<bool>* cancelar, std::string& erro)
{
    using namespace bp;
    saida.clear();
    if (! impl->sessao) { erro = "modelo de notas não carregado"; return false; }
    const size_t original = x.size();
    if (original < (size_t) SR) return true;

    // áudio com SOBRE_LEN/2 zeros na frente, janelas de NWIN a cada PASSO
    std::vector<float> a ((size_t) SOBRE_LEN / 2, 0.0f);
    a.insert (a.end(), x.begin(), x.end());
    const int nJanelas = (int) ((a.size() + PASSO - 1) / PASSO);
    const int meio = FR_SOBRE / 2;                 // 15 quadros cortados de cada lado
    const int frUteis = FR_JANELA - FR_SOBRE;      // 142
    const int nQuadros = std::min (nJanelas * frUteis, (int) ((double) original / PASSO * frUteis));
    if (nQuadros < 3) return true;
    std::vector<float> fr ((size_t) nQuadros * NOTAS, 0.0f), on ((size_t) nQuadros * NOTAS, 0.0f);

    const char* entradas[] = { "serving_default_input_2:0" };
    const char* saidas[] = { "StatefulPartitionedCall:1", "StatefulPartitionedCall:2" };   // note, onset
    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
    const int LOTE = 8;
    std::vector<float> buf;
    try
    {
        for (int j0 = 0; j0 < nJanelas; j0 += LOTE)
        {
            if (cancelar && cancelar->load()) { erro = "cancelado"; return false; }
            const int nb = std::min (LOTE, nJanelas - j0);
            buf.assign ((size_t) nb * NWIN, 0.0f);
            for (int b = 0; b < nb; ++b)
            {
                const size_t i0 = (size_t) (j0 + b) * PASSO;
                const size_t m = std::min ((size_t) NWIN, a.size() > i0 ? a.size() - i0 : 0);
                if (m > 0) std::memcpy (buf.data() + (size_t) b * NWIN, a.data() + i0, m * sizeof (float));
            }
            const int64_t forma[3] = { nb, NWIN, 1 };
            Ort::Value ent = Ort::Value::CreateTensor<float> (mem, buf.data(), buf.size(), forma, 3);
            auto res = impl->sessao->Run (Ort::RunOptions { nullptr }, entradas, &ent, 1, saidas, 2);
            const float* pn = res[0].GetTensorData<float>();
            const float* po = res[1].GetTensorData<float>();
            for (int b = 0; b < nb; ++b)
                for (int k = 0; k < frUteis; ++k)
                {
                    const int q = (j0 + b) * frUteis + k;
                    if (q >= nQuadros) break;
                    const size_t src = ((size_t) b * FR_JANELA + (size_t) (k + meio)) * NOTAS;
                    std::memcpy (&fr[(size_t) q * NOTAS], pn + src, NOTAS * sizeof (float));
                    std::memcpy (&on[(size_t) q * NOTAS], po + src, NOTAS * sizeof (float));
                }
            if (progresso) progresso (0.9f * (float) (j0 + nb) / (float) nJanelas);
        }
    }
    catch (const std::exception& e) { erro = std::string ("erro na transcrição: ") + e.what(); return false; }

    const int T = nQuadros;
    auto F = [&] (std::vector<float>& m, int t, int f) -> float& { return m[(size_t) t * NOTAS + (size_t) f]; };

    // faixa de frequência
    int fMinI = 0, fMaxI = NOTAS;
    if (par.fMin > 0) fMinI = std::clamp (idxDeHz (par.fMin), 0, NOTAS);
    if (par.fMax > 0) fMaxI = std::clamp (idxDeHz (par.fMax), 0, NOTAS);
    for (int t = 0; t < T; ++t)
        for (int f = 0; f < NOTAS; ++f)
            if (f < fMinI || f >= fMaxI) { F (fr, t, f) = 0; F (on, t, f) = 0; }

    // ataques inferidos das diferenças de energia (get_infered_onsets, n_diff = 2)
    {
        std::vector<float> dif ((size_t) T * NOTAS, 0.0f);
        float mxDif = 0, mxOn = 0;
        for (int t = 0; t < T; ++t)
            for (int f = 0; f < NOTAS; ++f)
            {
                mxOn = std::max (mxOn, F (on, t, f));
                if (t < 2) continue;
                const float d1 = F (fr, t, f) - F (fr, t - 1, f);
                const float d2 = F (fr, t, f) - F (fr, t - 2, f);
                const float d = std::max (0.0f, std::min (d1, d2));
                F (dif, t, f) = d;
                mxDif = std::max (mxDif, d);
            }
        if (mxDif > 0)
            for (size_t i = 0; i < dif.size(); ++i) on[i] = std::max (on[i], mxOn * dif[i] / mxDif);
    }

    const int minLen = (int) std::lround (par.notaMinimaMs / 1000.0 * ((double) SR / HOP));
    std::vector<float> resto = fr;
    auto zera3 = [&] (int t, int f)
    {
        F (resto, t, f) = 0;
        if (f < MAX_FREQ_IDX) F (resto, t, f + 1) = 0;
        if (f > 0) F (resto, t, f - 1) = 0;
    };
    auto media = [&] (int a, int b, int f)
    {
        double s = 0;
        for (int t = a; t < b; ++t) s += F (fr, t, f);
        return b > a ? (float) (s / (b - a)) : 0.0f;
    };

    // picos de ataque (máximo local estrito no tempo), do fim para o começo
    struct Pico { int t, f; };
    std::vector<Pico> picos;
    for (int t = 1; t + 1 < T; ++t)
        for (int f = 0; f < NOTAS; ++f)
        {
            const float v = F (on, t, f);
            if (v >= par.limiarAtaque && v > F (on, t - 1, f) && v > F (on, t + 1, f)) picos.push_back ({ t, f });
        }
    struct Bruta { int a, b, tom; float amp; };
    std::vector<Bruta> notas;
    for (auto it = picos.rbegin(); it != picos.rend(); ++it)
    {
        const int t0 = it->t, f = it->f;
        if (t0 >= T - 1) continue;
        int i = t0 + 1, k = 0;
        while (i < T - 1 && k < TOL_ENERGIA)
        {
            if (F (resto, i, f) < par.limiarNota) ++k; else k = 0;
            ++i;
        }
        i -= k;
        if (i - t0 <= minLen) continue;
        for (int t = t0; t < i; ++t) zera3 (t, f);
        notas.push_back ({ t0, i, f + OFFSET_MIDI, media (t0, i, f) });
    }
    if (progresso) progresso (0.95f);

    // "melodia trick": energia que sobrou vira nota (fila de prioridade com invalidação preguiçosa)
    if (par.melodia)
    {
        struct Item { float v; int t, f; bool operator< (const Item& o) const { return v < o.v; } };
        std::priority_queue<Item> fila;
        for (int t = 0; t < T; ++t)
            for (int f = 0; f < NOTAS; ++f)
                if (F (resto, t, f) > par.limiarNota) fila.push ({ F (resto, t, f), t, f });
        while (! fila.empty())
        {
            const Item top = fila.top();
            fila.pop();
            if (F (resto, top.t, top.f) != top.v || top.v <= par.limiarNota) continue;
            const int tm = top.t, f = top.f;
            F (resto, tm, f) = 0;
            int i = tm + 1, k = 0;
            while (i < T - 1 && k < TOL_ENERGIA)
            {
                if (F (resto, i, f) < par.limiarNota) ++k; else k = 0;
                zera3 (i, f);
                ++i;
            }
            const int fim = i - 1 - k;
            i = tm - 1; k = 0;
            while (i > 0 && k < TOL_ENERGIA)
            {
                if (F (resto, i, f) < par.limiarNota) ++k; else k = 0;
                zera3 (i, f);
                --i;
            }
            const int ini = i + 1 + k;
            if (fim - ini <= minLen) continue;
            notas.push_back ({ ini, fim, f + OFFSET_MIDI, media (ini, fim, f) });
        }
    }

    saida.reserve (notas.size());
    for (const Bruta& n : notas)
        saida.push_back ({ tempoDoQuadro (n.a), tempoDoQuadro (std::min (n.b, T - 1)), n.tom, n.amp });
    std::sort (saida.begin(), saida.end(), [] (const NotaAudio& a, const NotaAudio& b)
               { return a.ini != b.ini ? a.ini < b.ini : a.tom < b.tom; });
    if (progresso) progresso (1.0f);
    return true;
}

// =================================================================================================== utilitários
std::vector<float> somaMono (const std::vector<float>& L, const std::vector<float>& R)
{
    const size_t n = std::min (L.size(), R.size());
    std::vector<float> m (n);
    for (size_t i = 0; i < n; ++i) m[i] = 0.5f * (L[i] + R[i]);
    return m;
}

std::vector<float> metadeDaTaxa (const std::vector<float>& x)
{
    // passa-baixa em 0,45 da taxa de saída (janela de Blackman), depois pega 1 a cada 2
    constexpr int N = 95, M = N / 2;
    static const std::vector<float> h = []
    {
        std::vector<float> c (N);
        const double fc = 0.45 / 2.0;   // em ciclos por amostra da ENTRADA
        double soma = 0;
        for (int i = 0; i < N; ++i)
        {
            const int k = i - M;
            const double s = k == 0 ? 2 * fc : std::sin (2 * PI_ * fc * k) / (PI_ * k);
            const double w = 0.42 - 0.5 * std::cos (2 * PI_ * i / (N - 1)) + 0.08 * std::cos (4 * PI_ * i / (N - 1));
            c[(size_t) i] = (float) (s * w);
            soma += s * w;
        }
        for (auto& v : c) v = (float) (v / soma);
        return c;
    }();
    const size_t n = x.size() / 2;
    std::vector<float> y (n);
    const long long tam = (long long) x.size();
    for (size_t j = 0; j < n; ++j)
    {
        const long long c = (long long) j * 2;
        double s = 0;
        for (int i = 0; i < N; ++i)
        {
            const long long k = c + i - M;
            if (k >= 0 && k < tam) s += h[(size_t) i] * x[(size_t) k];
        }
        y[j] = (float) s;
    }
    return y;
}
} // namespace mapa
