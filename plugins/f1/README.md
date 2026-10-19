# F1 — filtro ladder

Filtro passa-baixa de 4 degraus no estilo dos pedais analógicos clássicos ("101"): ressonância
que chega a auto-oscilar, drive na entrada, envelope que abre ou fecha o filtro com a força do som
e LFO no grid. O botão **FUMAÇA** "embaça" o som num reverb curto e escuro.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| RESPOSTA | visual | 20 Hz–20 kHz | Curva do filtro agora (creme), onde o knob deixou (cinza) e até onde o envelope leva (verde) |
| ESCADA | visual | — | Os 4 degraus acendem com o som; o laço coral é a ressonância; a barra verde, o envelope; a fumaça sobe com FUMAÇA |
| 01 FILTRO | **Macro CORTE** | 20 Hz–18 kHz | Frequência de corte |
| | Ressonância | 0–100% | Pico no corte; perto de 100% em 4 polos o filtro canta sozinho |
| | Polos | 12 / 24 dB | 2 polos (mais aberto) ou 4 polos (o clássico) |
| | Drive | 0 a +24 dB | Satura a entrada do filtro |
| | Faixa | Alta / Baixa | Baixa leva o corte duas oitavas para baixo (graves) |
| 02 ENVELOPE | **Macro ENVELOPE** | −100 a +100% | Quanto a força do som abre (+) ou fecha (−) o corte, até 4 oitavas |
| | Suavização | Rápido–Lento | Ataque e soltura do envelope |
| | LFO | 1/32 a 4 comp | Divisão do LFO no grid |
| | LFO Quant. | 0–100% | Até ±2 oitavas no corte |
| | Sensibil. | −48 a 0 dBFS | Nível em que o envelope chega ao máximo |
| 03 SAÍDA | ABRIR | liga/desliga | Abre o filtro todo, deslizando (para o drop) |
| | FUMAÇA | liga/desliga | Reverb curto e escuro por cima do filtrado; ao desligar, o rabo termina sozinho |
| | Mix / Ganho | | Seco ↔ filtrado e volume final |

As cores dos knobs são as do desenho: CORTE âmbar, RESSONÂNCIA coral (o laço), envelope em verde, LFO em violeta.

**Sincronia:** o LFO segue a régua do host. Com o host parado, o cabeçalho mostra *AGUARDANDO PLAY*
e o LFO fica parado na posição do cursor; no play ele entra no grid.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.
Trocar de preset não mexe nos botões de performance (ABRIR, FUMAÇA).

- Mac: `~/Library/Audio/Presets/InsideStudioSP/F1/`
- Windows: `%APPDATA%\InsideStudioSP\F1\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/LadderEngine.h` | Ladder ZDF de 4 degraus com saturação, envelope, LFO no grid e a FUMAÇA |
| `Source/PluginProcessor.*` | Relógio do host, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
