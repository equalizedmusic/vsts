# LIXA — de-esser

De-esser de faixa ajustável que **mostra o que está tirando**: no alto, o espectro ao vivo da
entrada (cinza) e da saída (creme), com o que a lixa tirou em coral, e a faixa de detecção em âmbar,
com alças nas bordas para arrastar. O painel de análise diz quanto está reduzindo agora, o nível da
faixa contra o THRESHOLD e o histórico dos últimos ~4 s.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| SPECTRUM | visual | 20 Hz–20 kHz | INPUT (cinza), OUTPUT (creme) e o que foi tirado (coral). A faixa âmbar é a de detecção: arraste as bordas para mudar LOW FREQ/HIGH FREQ, o meio para mover a faixa inteira; duplo clique volta a 4–10 kHz |
| 01 FAIXA DE DETECÇÃO | Low freq | 1–12 kHz | Borda de baixo da faixa (passa-altas de 24 dB/oitava) |
| | High freq | 2–20 kHz | Borda de cima da faixa (passa-baixas de 24 dB/oitava); fica sempre pelo menos 1,2× acima da LOW FREQ |
| | WIDEBAND | liga/desliga | Desligado: só a faixa abaixa. Ligado: o som inteiro abaixa junto (mais natural em vozes expostas) |
| 02 GAIN REDUCTION | **Macro RANGE** | 0 a 24 dB | O máximo que o S pode baixar |
| | Threshold | −60 a 0 dB | Nível da faixa a partir do qual a lixa age (razão 5:1, joelho de 6 dB) |
| | Link L/R | 0–100% | 100% = os dois canais levam a mesma redução (a imagem estéreo não mexe) |
| | Attack | 0,1–10 ms | Quão rápido a redução entra |
| | Release | 5–200 ms | Quão rápido a redução sai |
| 03 ANÁLISE | visual | ~4 s | GAIN REDUCTION agora, MAX (segura 3 s), nível da faixa com a marca azul do THRESHOLD e o histórico da redução (a linha tracejada é o RANGE) |
| 04 OUTPUT | SÓ O CORTE | liga/desliga | Escuta só o que está sendo tirado (para achar a faixa do S). Os presets não mexem nele |
| | Gain | −12 a +12 dB | Volume final |
| | Medidores | — | PEAK IN e PEAK OUT em dBFS (LEDs e número) |

As cores dos knobs são as do desenho: LOW FREQ/HIGH FREQ âmbar (a faixa), RANGE coral (o que sai),
THRESHOLD azul (a marca no nível da faixa).

**Latência:** zero (filtros IIR, detector sem olhar à frente); informada ao host. No modo só a faixa,
sem redução a saída é idêntica à entrada (saída = entrada − faixa × redução).

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/LIXA/`
- Windows: `%APPDATA%\InsideStudioSP\LIXA\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/DeEsserEngine.h` | Filtros da faixa, detector, redução com link, modos e telemetria (espectro e histórico) |
| `Source/PluginProcessor.*` | Ganho, bypass, latência, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580; a FFT do espectro roda na interface); `Theme.h`/`Widgets.h` são o padrão da marca |
