# RINGER — ring modulator

> Parte da **SÉRIE CRIATIVO** · versão atual do pacote: **1.2.1** (08/11/2026) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Criativo.md`](../../../novidades/Criativo.md)

![RINGER](https://insidestudiosp.com.br/img/ringer.webp)

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

## Janela e licença

- Visual de pedal: moldura de madeira, chapa esmaltada na cor do plugin, controles em chapa preta com os
  knobs da SÉRIE KNOB e alavancas de metal nas chaves liga/desliga.
- A janela muda de tamanho pelo canto ou no menu de presets → *Tamanho da janela* (75% a 200%) e abre no
  tamanho salvo.
- Na primeira vez o plugin pede o e-mail e o token da licença do pacote; o mesmo token vale em até 2 computadores.
  Sem ativar, o som passa direto, sem processar. No menu de presets, *Licença…* mostra a licença e tem
  *Desativar este computador* (libera a vaga para outro).
- Com a licença ativa, uma faixa no pé da janela avisa quando sai versão nova do pacote (**BAIXAR** / **Agora não**).

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/RingEngine.h` | Portadora, LFO no grid, drive e o buffer do osciloscópio |
| `Source/PluginProcessor.*` | Relógio do host, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (chapa de 1000×580 na moldura de madeira, janela de 1048×620 em 100%); `Theme.h`/`Widgets.h` são o padrão da marca |
| `Source/Hardware.h`, `Painel.h` | Peças renderizadas (knobs, lâmpadas) e o painel em volta (moldura, chapa, alavancas) |
