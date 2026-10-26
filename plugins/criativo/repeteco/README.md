# REPETECO — delay analógico (BBD)

![REPETECO](https://raw.githubusercontent.com/equalizedmusic/vsts/shots/REPETECO.png)

Delay no estilo dos pedais de "balde" (BBD): cada repetição volta mais escura e mais suja,
a realimentação passa de 100% e entra em auto-oscilação controlada, e mexer no TIME com som
passando estica o eco. Na tela, as repetições ficando turvas e a cadeia de baldes com a carga
passando de um para o outro.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| REPETIÇÕES | visual | 13 voltas | Cada barra é o que entrou n voltas atrás, já multiplicado pela realimentação; mais velha = mais escura |
| BALDES | visual | — | 30 baldes = o tempo todo do eco; a linha coral é a realimentação; RELÓGIO = o que um BBD de 1024 estágios precisaria |
| 01 TIME | **Macro TIME** | 8 passos | No grid: Short 1/32…3/16, Long 1/8…1 comp. Livre: 20–400 ms ou 100–2000 ms |
| | Range | Short / Long | |
| | Sync | Grid / Livre | |
| | Drive | 0 a +24 dB | Satura a entrada |
| | Ping-pong | 0–100% | O eco vai e volta entre os lados |
| 02 FEEDBACK | **Macro FEEDBACK** | 0–120% | Acima de 100% o eco cresce até o limite e canta |
| | LFO Rate / LFO Amount / Shape | | Balança o tempo de leve (até 6 ms), no grid |
| | Escuro | 0–100% | Filtro dentro do laço: 12 kHz → 1,2 kHz; eco longo escurece mais |
| 03 OUTPUT | INFINITO | liga/desliga | Congela o eco: entrada fechada, 100% e sem filtro |
| | MATAR | liga/desliga | Corta a entrada e o seco; só o rabo do eco soa |
| | Dry/Wet / Gain | | Seco ↔ eco (padrão 35%) e volume final |

**Sincronia:** o LFO segue a régua do host. Com o host parado, o cabeçalho mostra *AGUARDANDO PLAY*
e o LFO fica parado na posição do cursor; no play ele entra no grid.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.
Trocar de preset não mexe nos botões de performance (INFINITO, MATAR).

- Mac: `~/Library/Audio/Presets/InsideStudioSP/REPETECO/`
- Windows: `%APPDATA%\InsideStudioSP\REPETECO\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/BbdEngine.h` | Linha de atraso com interpolação, laço escuro, saturação, ping-pong e histórico dos baldes |
| `Source/PluginProcessor.*` | Relógio do host, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
