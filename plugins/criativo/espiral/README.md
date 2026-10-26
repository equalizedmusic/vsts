# ESPIRAL — infinite phaser / flanger

![ESPIRAL](https://raw.githubusercontent.com/equalizedmusic/vsts/shots/ESPIRAL.png)

Phaser e flanger "barber pole" sincronizados ao BPM do projeto, pensados para psytrance. O
som parece **subir (ou descer) sem fim**: quatro vozes percorrem a mesma faixa defasadas de
1/4 de volta; cada uma nasce em silêncio no grave, acende no meio e some no agudo, enquanto
a próxima já está entrando. Como a entrada e a saída de cada voz acontecem com volume zero,
a volta não estala.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| ESPIRAL | visual | — | Cada voz é um cometa: centro = grave, borda = agudo; o brilho é o volume da voz |
| RESPOSTA | visual | — | Curva de frequência do que sai agora (seco + processado), com a faixa de cada voz |
| 01 MOVIMENTO | **Macro RANGE** | 1–10 oitavas | Quanto a espiral percorre em volta do CENTER |
| | Ciclo | 1 tempo … 16 compassos | Duração de uma volta inteira, no BPM do projeto |
| | Direção | Sobe · Desce · Vai e volta | Sentido da varredura; *vai e volta* inverte a cada volta |
| | Center | 100 Hz – 4 kHz | Meio da faixa (desliza a faixa inteira quando bate no limite de 20 Hz / 18 kHz) |
| | Stereo | 0–100% | Defasagem entre L e R (100% = meia distância entre vozes) |
| 02 COR | **Macro RESONANCE** | 0–100% | Realimentação: vales mais fundos e picos cantando |
| | Modo | Phaser · Flanger | Phaser = passa-tudo em cascata; flanger = pente de atraso (0,05–25 ms). A troca é em crossfade |
| | Stages | 4 · 6 · 8 · 12 | Só no phaser: mais estágios = mais vales |
| | Drive | 0–100% | Saturação do processado (sinal baixo passa igual, pico satura) |
| | Tone | 1 kHz – aberto | Passa-baixa no processado |
| 03 OUTPUT | Congelar | liga/desliga | Para a espiral onde está; ao soltar, segue dali |
| | Dobrar | liga/desliga | Dobra a velocidade (bom para viradas); ao soltar, segue dali |
| | Dry/Wet | 0–100% | Seco ↔ processado |
| | Gain | −36 a +12 dB | Volume final |

Com o Ableton tocando, a volta segue a régua do projeto (**SYNC**) e se alinha no play. Se o
Ableton pular (loop, mudança de posição), a espiral acelera ou freia até 50% para alcançar,
sem salto. Com o Ableton parado, a espiral fica na posição do cursor e só anda no play
(**AGUARDANDO PLAY**); sozinha no BPM (**LIVRE**) só em host que não informa a posição. Trocar o CICLO, CONGELAR ou
DOBRAR não pula: segue de onde está, e o alinhamento volta no próximo play.

## Presets

Vinte de fábrica. Trocar de preset não mexe em CONGELAR, DOBRAR nem no bypass. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também *Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

Presets do usuário:

- Mac: `~/Library/Audio/Presets/InsideStudioSP/ESPIRAL/`
- Windows: `%APPDATA%\InsideStudioSP\ESPIRAL\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/SpiralEngine.h` | Relógio (segue o host), 4 vozes × phaser/flanger, drive, tom |
| `Source/PluginProcessor.*` | Host/BPM, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
