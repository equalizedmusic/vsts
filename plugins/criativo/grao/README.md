# GRÃO — granular texture FX

O som que entra é gravado numa **fita** de 4 s. Dela saem **grãos** curtos, cada um com o
próprio tamanho, posição, afinação e lado do estéreo. Os grãos passam por feedback (voltam
para a fita), filtro (TONE) e uma cauda de ambiente (DECAY).

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| CAMPO XY | X / Y | — | X = macro ESPAÇO, Y = macro GRÃO |
| 01 GRÃO | **Macro GRÃO** | 0–100% | Mais alto = grãos menores, mais densos e com mais variação de afinação |
| | Size | 10–500 ms | Duração de cada grão |
| | Density | 1–100 /s | Grãos por segundo |
| | Spray | 0–1000 ms | Quão longe, para trás na fita, cada grão pode começar |
| | Pitch | ±24 st | Transposição dos grãos |
| 02 ESPAÇO | **Macro ESPAÇO** | 0–100% | Abre o estéreo, aumenta o feedback e a quantidade de cauda |
| | Stereo | 0–100% | Espalhamento dos grãos entre L e R |
| | Feedback | 0–95% | Quanto da saída dos grãos volta para a fita |
| | Decay | 0,2–10 s | Tamanho da cauda |
| | Tone | LP ← ABERTO → HP | Esquerda fecha o passa-baixa (até 500 Hz), direita abre o passa-alta (até 2 kHz) |
| 03 OUTPUT | Reverso | liga/desliga | Metade dos grãos toca de trás para frente |
| | Congelar | liga/desliga | Para a gravação; os grãos continuam lendo a fita parada |
| | Dry/Wet | 0–100% | Seco ↔ processado |
| | Gain | −36 a +12 dB | Volume final |

Duplo clique num controle volta ao valor padrão. O botão redondo no topo é o bypass
(aceso = efeito ligado).

## Presets

Vinte de fábrica, entre eles: Init · Areia, Poeira Dourada, Enxame Reverso, Coral Congelado, Fita Rasgada,
Garoa (e mais 14). Clique no nome para abrir a lista; ◀ ▶ navegam; **+** pede um nome e salva o
estado atual. No menu, com um preset do usuário aberto: *Salvar alterações*, *Renomear* e *Apagar*
(vai para a lixeira). Pasta:

- Mac: `~/Library/Audio/Presets/InsideStudioSP/GRAO/`
- Windows: `%APPDATA%\InsideStudioSP\GRAO\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/GrainEngine.h` | Fita, agendador e vozes de grão |
| `Source/PluginProcessor.*` | Macros, filtro, cauda, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580) |
