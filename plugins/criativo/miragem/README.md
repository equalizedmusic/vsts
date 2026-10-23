# MIRAGEM — reverb atmosférico com shimmer

Reverb de 8 linhas (FDN) com difusão na entrada e shimmer: a cauda sobe de tom (oitava, quinta,
as duas, ou oitava abaixo) e volta para a sala, então cada volta fica mais alta e mais brilhante.
CONGELAR segura a cauda para sempre (dá para tocar por cima), REVERSO faz a cauda vir antes do
golpe, e o DUCK abaixa a cauda enquanto o seco toca, para não embolar a mix. Na tela, a cauda
vira uma serra no horizonte e o reflexo treme como calor no asfalto.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| MIRAGEM | visual | ~5 s | A serra é o nível da cauda no tempo (mais novo à direita; no REVERSO, à esquerda). O reflexo treme com o MODULATION; o halo violeta cresce com o SHIMMER; congelado fica em gelo |
| TAIL | visual | — | Queda em dB × tempo, faixa do pré-delay, segunda cauda do shimmer (violeta), nível agora e a faixa de TONE (LOW CUT → DAMPING). Rodapé: RT60 e quanto o DUCK está abaixando |
| 01 ESPAÇO | **Macro DECAY** | 0,3–30 s | Tempo até a cauda cair 60 dB |
| | Size | 0–100% | Tamanho da sala (linhas de 7 a 110 ms); mexer com som passando estica a sala |
| | Pre-delay | 0–250 ms | Silêncio antes da cauda |
| | Damping | 0–100% | Filtro dentro de cada linha: 16 kHz → 1,3 kHz |
| | Low cut | 20 Hz–1 kHz | Tira o grave da cauda |
| 02 SHIMMER | **Macro SHIMMER** | 0–100% | Quanto da cauda sobe de tom e volta para a sala |
| | Interval | Oitava / Quinta / Oitava + quinta / Oitava abaixo | |
| | Modulation | 0–100% | Balanço lento em cada linha (até 2,5 ms): tira o metálico e dá o "calor" |
| | Duck | 0–100% | Até −18 dB na cauda enquanto o seco toca |
| | Janela Rev. | 1/4 · 1/2 · 1 comp · 2 comp | Tamanho do pedaço que o REVERSO vira, no grid |
| 03 OUTPUT | CONGELAR | liga/desliga | Entrada fechada, sala sem perda e sem filtro: a cauda fica para sempre |
| | REVERSO | liga/desliga | Grava uma janela da cauda e toca a anterior de trás para frente |
| | Dry/Wet / Gain | | Seco ↔ molhado (padrão 30%) e volume final |

**Sincronia:** com o host tocando, a janela do REVERSO cai no grid; com o host parado ela conta
sozinha (reverb não para de soar quando o transporte para). O cabeçalho mostra SYNC, AGUARDANDO
PLAY ou LIVRE como os outros plugins.

**REVERSO tem atraso de uma janela:** o pedaço que toca ao contrário é o que acabou de ser gravado.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.
Trocar de preset não mexe nos botões de performance (CONGELAR, REVERSO).

- Mac: `~/Library/Audio/Presets/InsideStudioSP/MIRAGEM/`
- Windows: `%APPDATA%\InsideStudioSP\MIRAGEM\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/ReverbEngine.h` | Pré-delay, difusores, 8 linhas com Hadamard, shimmer (pitch shift de duas cabeças), corte de grave, REVERSO, DUCK e histórico da tela |
| `Source/PluginProcessor.*` | Relógio do host, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
