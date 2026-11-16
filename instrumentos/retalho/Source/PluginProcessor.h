#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Dsp.h"
#include "Presets.h"

// RETALHO: stutter + groove de vocal, latência 0. Vai no canal da voz (ou em qualquer canal).
//  - EDIT: o fill desenhado (GRID / RAMP / CURVE / FREE / TREMOLO), com pitch, filtro, stereo, tape stop e reverse;
//  - COMPLEX: 8/16/32 passos, cada um com um tipo (REPEAT, PITCH, REVERSE, TAPE, SPEED, GATE, THRU, MUTE), DICE e LOCK;
//  - GROOVE: a voz cortada em 8 chops tocando num pattern (OFFBEAT, GALLOP, SKIP, 2-STEP, FUNK, TRAP, CALL&RESP).
// LIVE: AUTO (dispara sozinho no fim/início do ciclo), HOLD ou ONE-SHOT, CATCH, 6 pads de memória (MIDI C3–F3).
// 0.1.0 é TESTE: sem licença.
class RetalhoProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    RetalhoProcessor();
    ~RetalhoProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RETALHO"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    retalho::Motor& motor() { return *m; }
    double sampleRate() const { return fs; }

    // parâmetros (valor real)
    void define (int campo, float valorReal);
    float valor (int campo) const { return p[campo] != nullptr ? p[campo]->load() : 0.0f; }
    juce::RangedAudioParameter* param (int campo) const;
    retalho::Cena cenaAtual() const;
    void aplicaCena (const retalho::Cena& c);   // parâmetros da cena + grades (thread da tela)

    // presets
    void aplicaPreset (int i);
    juce::String nomePreset() const { return presetNome; }
    void salvaPresetArquivo (const juce::File& f);
    bool abrePresetArquivo (const juce::File& f);
    static juce::File pastaPresets();

    // pads (memórias)
    struct InfoPad { bool cheio = false; juce::String nome; int modo = 0; juce::String sub; };
    InfoPad infoPad (int i) const;
    void salvaPad (int i);
    void limpaPad (int i);
    void renomeiaPad (int i, const juce::String& nome);
    void apertaPad (int i) { m->uiAperta = i; }
    void soltaPad (int i) { m->uiSolta = i; }

    // ações das abas
    void dado();                 // COMPLEX: SEED + 1 e sorteia
    void dadoGroove();           // GROOVE: sorteia o pattern
    void limpaCadeados();
    void geraEstiloAgora();      // GROOVE: refaz o pattern do STYLE
    void escreveGrade (int i, uint32_t g)  { m->aGrade[juce::jlimit (0, retalho::maxGrade - 1, i)] = g; }
    void escrevePasso (int i, uint32_t v)  { m->aPasso[juce::jlimit (0, retalho::maxPassos - 1, i)] = v; }
    void escreveCelula (int l, int i, uint32_t v) { m->aCel[juce::jlimit (0, 2, l)][juce::jlimit (0, retalho::maxCelulas - 1, i)] = v; }
    void escreveLivre (int i, float v)     { m->aLivre[juce::jlimit (0, retalho::numLivre - 1, i)] = juce::jlimit (0.0f, 1.0f, v); }

    // A/B
    int abAtual() const { return ab.load(); }
    void trocaAB (int para);
    void copiaAB();

    // vista (o que a tela desenha): a última janela da CAPTURE e o fill renderizado em cima dela
    static constexpr int colunas = 1168;
    struct Vista
    {
        int versao = 0;
        bool valida = false;
        std::vector<float> oMin, oMax, rMin, rMax;   // por coluna (−1..1)
        std::vector<float> picoFatia;
        retalho::Plano plano;
        double spb = 24000, bpb = 4, fs = 48000;
        int64_t len = 0;                              // amostras da janela (= do fill)
        std::vector<float> ataques;                   // fração da janela
        float chopIni[retalho::numChops] {}, chopFim[retalho::numChops] {};   // fração (−1 = fora)
        float chopOnda[retalho::numChops][48] {};
        float chopMs[retalho::numChops] {};
        float chunkIni = 0.0f, chunkFim = 0.0f;       // fração
    };
    const Vista& vista() const { return vistaAtual; }

    // preview (toca a janela ORIGINAL ou o RESULT no lugar da saída)
    void preview (int qual);      // 1 ORIGINAL, 2 RESULT; repetir o mesmo = para
    int previewTocando() const { return prevEstado.load() != 0 ? prevQual.load() : 0; }
    float previewPos() const { return prevLen.load() > 0 ? (float) prevPos.load() / (float) prevLen.load() : 0.0f; }
    std::atomic<bool> previewLoop { false }, previewOnBar { false };

    // export do RESULT em WAV 24 bits
    juce::File exporta (bool temporario);

    std::atomic<int> uiW { 0 };
    std::atomic<int> uiPro { 0 };   // 0 = tela SIMPLE (padrão), 1 = PRO

    // SIMPLE: os 4 cartões (0 STUTTER, 1 RISER, 2 TAPE STOP, 3 GROOVE) carregam uma cena pronta
    void aplicaCartao (int c);
    void presetVizinho (int passo);   // ◀ ▶ do SIMPLE
    std::atomic<int> pincelTipo { retalho::tRepeat }, pincelChop { 0 };

    // ---- SAMPLER (0.1.2): a voz fica DENTRO do plugin ----
    // A amostra entra arrastando um arquivo (ou ABRIR) ou pela CAPTURA (grava N compassos da faixa a partir do próximo
    // compasso). Com amostra carregada, a entrada da faixa é ignorada: o plugin toca a amostra em loop, no tempo do
    // projeto, e o STUTTER / RISER / TAPE STOP / GROOVE atuam sobre ela. Sem amostra, a faixa passa (e pode ser capturada).
    struct Amostra
    {
        juce::AudioBuffer<float> buf;   // na taxa do host
        double offBeats = 0.0;          // em que tempo do loop a amostra começa (captura: onde o projeto estava)
    };
    struct VisaoAmostra
    {
        int versao = 0;
        bool tem = false;
        juce::String nome;
        double seg = 0.0;
        std::vector<float> mn, mx;      // 600 colunas
    };
    bool temAmostra() const { return ativa.load() != nullptr; }
    bool carregaArquivo (const juce::File& f);          // thread da tela
    void capturar (int bars);                           // arma: grava a partir do próximo compasso
    void cancelaCaptura() { capEstado = 0; }
    void limpaAmostra();
    int estadoCaptura() const { return capEstado.load(); }   // 0 nada, 1 esperando o compasso, 2 gravando
    float progressoCaptura() const { return capLen.load() > 0 ? (float) capPos.load() / (float) capLen.load() : 0.0f; }
    std::atomic<int> capBars { 2 };
    std::atomic<bool> capVazia { false };                     // a última captura só tinha silêncio
    std::atomic<float> selIni { 0.0f }, selFim { 1.0f };      // trecho da amostra que toca (fração)
    VisaoAmostra visaoAmostra() const { const juce::ScopedLock sl (travaAmostra); return visao; }
    int versaoAmostra() const { return visaoVersao.load(); }
    float posAmostra() const { return posA.load(); }          // fração tocando agora (−1 = parado)
    int loopBars() const { return loopK.load(); }
    static bool arquivoDeAudio (const juce::String& caminho);

private:
    void instalaAmostra (const juce::AudioBuffer<float>& origem, double fsOrigem, const juce::String& nome, double offBeats, bool codifica);
    void montaAtiva();                // reamostra a origem na taxa do host e troca a ativa (sem travar o áudio)
    void recolheAposentadas (bool tudo);
    juce::AudioBuffer<float> origBuf; // a amostra como veio (arquivo ou captura)
    double origFs = 48000.0, origOff = 0.0;
    juce::String origNome, origB64;   // FLAC em base64 (vai no projeto)
    juce::CriticalSection travaAmostra;
    std::atomic<Amostra*> ativa { nullptr };
    std::vector<std::pair<Amostra*, int64_t>> aposentadas;
    std::atomic<int64_t> blocoIni { 0 }, blocoFim { 0 };
    VisaoAmostra visao;
    std::atomic<int> visaoVersao { 0 };
    std::atomic<float> posA { -1.0f };
    std::atomic<int> loopK { 1 };
    juce::AudioBuffer<float> capBuf;
    std::atomic<int> capEstado { 0 }, capPos { 0 }, capLen { 0 };
    std::atomic<bool> capPronta { false };
    std::atomic<double> capOff { 0.0 };

    void timerCallback() override;
    void atualizaVista (bool forcar);
    void carregaPad (int i);
    juce::ValueTree fotoCompleta() const;
    void aplicaFoto (const juce::ValueTree& v);

    std::unique_ptr<retalho::Motor> m;
    double fs = 48000.0;
    std::atomic<float>* p[retalho::numCampos] {};
    float val[retalho::numCampos] {};
    std::vector<retalho::Evento> eventos;

    juce::String presetNome { "INIT" };
    retalho::Cena padCena[retalho::numPads];
    bool padCheio[retalho::numPads] {};
    juce::String padNome[retalho::numPads];

    std::atomic<int> ab { 0 };
    juce::ValueTree slots[2];

    // vista
    Vista vistaAtual;
    std::vector<float> snap[2], outR[2];
    int64_t snapIni = 0;
    int vistoCompasso = -1;
    uint64_t ultimoHash = 0;
    int ultimoEstilo = -1, ultimoBars = -1, ultimoGrid = -1;

    // preview
    juce::AudioBuffer<float> prevBuf[2];
    std::atomic<int> prevSlot { 0 }, prevLen { 0 }, prevEstado { 0 }, prevQual { 0 }, prevPos { 0 }, prevCv { 0 };
    std::atomic<bool> prevReinicia { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetalhoProcessor)
};
