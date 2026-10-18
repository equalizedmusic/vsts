# GRÃO — granular texture FX

O som que entra é gravado numa **fita** de 4 s. Dela saem **grãos** curtos, cada um com o
próprio tamanho, posição, afinação e lado do estéreo. Os grãos passam por feedback (voltam
para a fita), filtro (TOM) e uma cauda de ambiente (DECAIMENTO).

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| CAMPO XY | X / Y | — | X = macro ESPAÇO, Y = macro GRÃO |
| 01 GRÃO | **Macro GRÃO** | 0–100% | Mais alto = grãos menores, mais densos e com mais variação de afinação |
| | Tamanho | 10–500 ms | Duração de cada grão |
| | Densidade | 1–100 /s | Grãos por segundo |
| | Dispersão | 0–1000 ms | Quão longe, para trás na fita, cada grão pode começar |
| | Afinação | ±24 st | Transposição dos grãos |
| 02 ESPAÇO | **Macro ESPAÇO** | 0–100% | Abre o estéreo, aumenta o feedback e a quantidade de cauda |
| | Estéreo | 0–100% | Espalhamento dos grãos entre L e R |
| | Feedback | 0–95% | Quanto da saída dos grãos volta para a fita |
| | Decaimento | 0,2–10 s | Tamanho da cauda |
| | Tom | LP ← ABERTO → HP | Esquerda fecha o passa-baixa (até 500 Hz), direita abre o passa-alta (até 2 kHz) |
| 03 SAÍDA | Reverso | liga/desliga | Metade dos grãos toca de trás para frente |
| | Congelar | liga/desliga | Para a gravação; os grãos continuam lendo a fita parada |
| | Mix | 0–100% | Seco ↔ processado |
| | Ganho | −36 a +12 dB | Volume final |

Duplo clique num controle volta ao valor padrão. O botão redondo no topo é o bypass
(aceso = efeito ligado).

## Presets

Seis de fábrica: Init · Areia, Poeira Dourada, Enxame Reverso, Coral Congelado, Fita Rasgada,
Garoa. Clique no nome para abrir a lista; ◀ ▶ navegam; **+** salva o estado atual como
"Meu preset NN":

- Mac: `~/Library/Audio/Presets/Equalized Music/GRAO/`
- Windows: `%APPDATA%\Equalized Music\GRAO\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/GrainEngine.h` | Fita, agendador e vozes de grão |
| `Source/PluginProcessor.*` | Macros, filtro, cauda, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580) |
