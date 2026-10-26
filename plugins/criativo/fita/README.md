# FITA — tape machine

![FITA](https://raw.githubusercontent.com/equalizedmusic/vsts/shots/FITA.png)

Simulador de gravador de fita: satura e dá cor ao som, com wow/flutter, chiado, reforço de grave
(head bump), desgaste e um **TAPE STOP** sincronizado ao BPM. Na tela, os rolos (ou o cassete)
giram na velocidade da fita: o wow aparece no giro, o rolo cheio gira devagar e o vazio rápido,
e no TAPE STOP os rolos freiam junto com o som.

Modelos genéricos (sem marca): **rolo 1/4"** (estúdio), **fita 2"** (mais grave, transporte
estável) e **cassete** (satura antes, perde agudo, chia e oscila mais; corre a 1/4 da velocidade
da chave: 7,5 ips = 1 7/8 ips).

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| MÁQUINA | VU L/R | −20 a +3 dBFS | Pico de cada canal, na mesma escala do medidor do Ableton (sobe na hora, desce em ~300 ms); vermelho = acima de 0 dBFS |
| | Curva | — | Resposta da reprodução: head bump + perda de agudo + desgaste |
| | Tape speed | 0–100% | Estado do transporte: rodando, freando, parada, partindo |
| MÁQUINA (card) | visual | — | Rolos/cassete, cabeças, cabrestante; contador e ips |
| 01 GRAVAÇÃO | **Macro SATURATION** | −12 a +18 dB na fita | Sinal baixo passa igual; os picos arredondam (com pré/de-ênfase, o agudo satura antes, como na fita) |
| | Modelo | Rolo 1/4" · Fita 2" · Cassete | Curva de saturação, chiado, wow, grave e agudo de cada máquina |
| | Speed | 7,5 · 15 · 30 ips | Mais rápida = mais agudo, menos chiado, head bump mais alto em frequência |
| | Head bump | 0 a +6 dB | Reforço de grave (com o vale logo acima, como na fita de verdade) |
| | Wear | 0–100% | Perde agudo (até 4 kHz), raspa mais (flutter) e tem falhas de óxido (dropouts) |
| 02 TRANSPORTE | **Macro WOW** | 0–100% | Oscilação lenta da velocidade (até ±2,2 ms; cassete 1,6×) |
| | Flutter | 0–100% | Oscilação rápida (7–12 Hz + ruído) |
| | Hiss | 0–100% | Hiss da fita; passa pela curva da reprodução e para junto com a fita |
| | Freio | 1/2 tempo … 2 comp | Quanto o TAPE STOP leva para parar, no BPM do projeto (máx. 4 s) |
| | Partida | Na hora · Curta · Igual ao freio | Ao soltar: volta na hora (crossfade de 30 ms) ou acelera do zero (1/4 ou igual ao freio) |
| 03 OUTPUT | TAPE STOP | liga/desliga | Freia a fita até parar. Automatizável no Ableton |
| | Dry/Wet | 0–100% | Seco ↔ fita (o seco sai com a mesma latência: paralelo sem fase) |
| | Gain | −36 a +12 dB | Volume final |

**Latência:** 5 ms + oversampling (2×), informada ao host. O Ableton compensa sozinho.

**TAPE STOP:** ligado, a fita desacelera até parar (o som cai de afinação e some no fim). Ao soltar,
com PARTIDA *curta* ou *igual*, a fita acelera do zero a partir do presente e, ao chegar na
velocidade normal, um crossfade de 30 ms elimina o atraso acumulado. Com *na hora*, volta direto.

## Presets

Vinte de fábrica. Trocar de preset não mexe no TAPE STOP nem no bypass. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também *Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

Presets do usuário:

- Mac: `~/Library/Audio/Presets/InsideStudioSP/FITA/`
- Windows: `%APPDATA%\InsideStudioSP\FITA\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/TapeEngine.h` | Gravação (pré-ênfase, saturação 2×), fita (buffer), transporte (wow/flutter/TAPE STOP), reprodução (chiado, head bump, agudo, desgaste) |
| `Source/PluginProcessor.*` | Host/BPM, mix, latência, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
