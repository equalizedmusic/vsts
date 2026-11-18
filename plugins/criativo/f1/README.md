# F1 — filtro ladder

> Parte da **SÉRIE CRIATIVO** · versão atual do pacote: **1.2.1** (08/11/2026) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Criativo.md`](../../../novidades/Criativo.md)

![F1](https://insidestudiosp.com.br/img/f1.webp)

Filtro passa-baixa de 4 degraus no estilo dos pedais analógicos clássicos ("101"): ressonância
que chega a auto-oscilar, drive na entrada, envelope que abre ou fecha o filtro com a força do som
e LFO no grid. O botão **FUMAÇA** "embaça" o som num reverb curto e escuro.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| RESPOSTA | visual | 20 Hz–20 kHz | Curva do filtro agora (creme), onde o knob deixou (cinza) e até onde o envelope leva (verde) |
| ESCADA | visual | — | Os 4 degraus acendem com o som; o laço coral é a ressonância; a barra verde, o envelope; a fumaça sobe com FUMAÇA |
| 01 FILTER | **Macro CUTOFF** | 20 Hz–18 kHz | Frequência de corte |
| | Resonance | 0–100% | Pico no corte; perto de 100% em 4 polos o filtro canta sozinho |
| | Poles | 12 / 24 dB | 2 polos (mais aberto) ou 4 polos (o clássico) |
| | Drive | 0 a +24 dB | Satura a entrada do filtro |
| | Range | High / Low | Low leva o corte duas oitavas para baixo (graves) |
| 02 ENVELOPE | **Macro ENVELOPE** | −100 a +100% | Quanto a força do som abre (+) ou fecha (−) o corte, até 4 oitavas |
| | Smooth | Fast–Slow | Attack e release do envelope |
| | LFO | 1/32 a 4 comp | Divisão do LFO no grid |
| | LFO Amount | 0–100% | Até ±2 oitavas no corte |
| | Sensitivity | −48 a 0 dBFS | Nível em que o envelope chega ao máximo |
| 03 OUTPUT | ABRIR | liga/desliga | Abre o filtro todo, deslizando (para o drop) |
| | FUMAÇA | liga/desliga | Reverb curto e escuro por cima do filtrado; ao desligar, o rabo termina sozinho |
| | Dry/Wet / Gain | | Seco ↔ filtrado e volume final |

As cores dos knobs são as do desenho: CUTOFF âmbar, RESONANCE coral (o laço), envelope em verde, LFO em violeta.

**Sincronia:** o LFO segue a régua do host. Com o host parado, o cabeçalho mostra *AGUARDANDO PLAY*
e o LFO fica parado na posição do cursor; no play ele entra no grid.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.
Trocar de preset não mexe nos botões de performance (ABRIR, FUMAÇA).

- Mac: `~/Library/Audio/Presets/InsideStudioSP/F1/`
- Windows: `%APPDATA%\InsideStudioSP\F1\Presets\`

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
| `Source/LadderEngine.h` | Ladder ZDF de 4 degraus com saturação, envelope, LFO no grid e a FUMAÇA |
| `Source/PluginProcessor.*` | Relógio do host, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (chapa de 1000×580 na moldura de madeira, janela de 1048×620 em 100%); `Theme.h`/`Widgets.h` são o padrão da marca |
| `Source/Hardware.h`, `Painel.h` | Peças renderizadas (knobs, lâmpadas) e o painel em volta (moldura, chapa, alavancas) |
