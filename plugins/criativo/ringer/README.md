# RINGER — ring modulator

Multiplica o som por uma portadora senoidal. Na faixa baixa vira tremolo; na alta, sino,
metal e robô. Um LFO no grid balança a portadora (sirene, laser, borbulha). Na tela, o
osciloscópio mostra entrada, portadora e saída; o anel gira com a portadora e desenha o Lissajous
entrada × saída.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| OSCILLOSCOPE | visual | ~23 ms | INPUT (cinza), CARRIER (coral) e OUTPUT (âmbar) |
| ANEL | visual | — | Anel coral = portadora girando; arco violeta = quanto o LFO empurra agora; Lissajous no meio |
| 01 CARRIER | **Macro FREQUENCY** | 0–100% da faixa | A nota que multiplica o som |
| | Range | Low / High | Low 0,5–80 Hz (tremolo); High 30 Hz–4 kHz (metálico) |
| | Afinar | Livre / Semitom | Semitom prende a portadora na nota mais perto |
| | Drive | 0 a +24 dB | Satura a entrada antes de multiplicar |
| | Stereo | 0–100% | Defasa a portadora da direita (até meia volta) |
| 02 LFO | **Macro AMOUNT** | 0–100% | Até ±2 oitavas na portadora |
| | Rate | 1/32 a 4 comp | Divisão no grid |
| | Shape | Sine, Triangle, Square, Ramp, Random | |
| | Sync | Grid / Livre | Livre roda sozinho, sem esperar o play |
| | Phase | 0–360° | Desloca o LFO dentro do grid |
| 03 OUTPUT | DOBRAR | liga/desliga | Portadora uma oitava acima, deslizando |
| | Dry/Wet / Gain | | Seco ↔ modulado (padrão 70%) e volume final |

**Sincronia:** o LFO segue a régua do host. Com o host parado, o cabeçalho mostra *AGUARDANDO PLAY*
e o LFO fica parado na posição do cursor; no play ele entra no grid.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.
Trocar de preset não mexe nos botões de performance (DOBRAR).

- Mac: `~/Library/Audio/Presets/InsideStudioSP/RINGER/`
- Windows: `%APPDATA%\InsideStudioSP\RINGER\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/RingEngine.h` | Portadora, LFO no grid, drive e o buffer do osciloscópio |
| `Source/PluginProcessor.*` | Relógio do host, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
