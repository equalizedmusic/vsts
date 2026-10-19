# CHAMA — build-up de um knob só

Um knob para a subida inteira: de 0 a 100%, a CHAMA vai tirando o grave com um passa-alta
ressonante, abre o estéreo, sobe um ruído filtrado junto, cresce um reverb e, nos últimos 40%,
entra um gate no tempo que acelera (1/8 → 1/16 → 1/32). Automatize a CHAMA no Ableton ao longo
dos compassos da subida e ligue o **DROP** na virada. Na tela, o fogo cresce com o knob.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| TENSÃO | visual | — | Muro de fogo e o que está sendo aplicado agora: corte, ressonância, largura, ruído, espaço e gate |
| CHAMA | **CHAMA** | 0–100% | Tudo junto: passa-alta 20 Hz → 2 kHz (com pico), estéreo até 250%, ruído, reverb, gate no tempo e +4 dB de compensação |
| AJUSTES | DROP | liga/desliga | Apaga a chama em 20 ms (para a virada). Os presets não mexem nele |
| | Ruído | 0–100% | Quanto ruído sobe junto no máximo (faíscas azuis no desenho) |
| | Espaço | 0–100% | Quanto reverb no máximo (fumaça violeta no desenho) |
| | Ganho | −36 a +12 dB | Volume final |

O gate segue o grid do Ableton (**SYNC**). Com o Ableton parado o gate espera o play
(**AGUARDANDO PLAY**); em host sem régua anda sozinho no BPM (**LIVRE**).

## Presets

Vinte de fábrica (Faísca, Brasa, Fogueira… Combustão). No menu de presets: *Salvar como novo*; com
um preset do usuário aberto, também *Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira).
O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/CHAMA/`
- Windows: `%APPDATA%\InsideStudioSP\CHAMA\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/FlameEngine.h` | Curvas da CHAMA, passa-alta e ruído (SVF), largura, reverb, gate no tempo |
| `Source/PluginProcessor.*` | Host/BPM, ganho, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
