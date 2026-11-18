# TORNO — compressor

> Parte da **SÉRIE MIX** · versão atual do pacote: **1.2.1** (08/11/2026) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Mix.md`](../../../novidades/Mix.md)

![TORNO](https://insidestudiosp.com.br/img/torno.webp)

Compressor de mix que **mostra o que está apertando**: no alto, o histórico dos últimos ~4 s, com o
nível de entrada em creme subindo de baixo e a redução de ganho em coral descendo de cima, mais a
linha do THRESHOLD. A curva de entrada × saída mostra o joelho, a compensação e o ponto de agora.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| HISTÓRICO | visual | ~4 s | Fatias de 8 ms: creme = pico de entrada (escala 0 a −60 dBFS, na direita), coral = redução de ganho (escala fixa de 0 a −30 dB, com os números à esquerda); linha âmbar = THRESHOLD |
| CURVA | visual | −60 a 0 dB | Entrada × saída: faixa violeta = joelho, linha âmbar = limiar, tracejado verde = saída com a compensação; o ponto coral é o nível de agora e o traço coral, a redução aplicada |
| 01 COMPRESSÃO | **Macro APERTO** | 0–100% | Desce o THRESHOLD (até −24 dB) e multiplica o RATIO (até 2,5×, no máximo 20:1) juntos |
| | Threshold | −60 a 0 dB | Nível a partir do qual comprime |
| | Ratio | 1:1 a 20:1 | Quanto comprime acima do threshold |
| | Knee | 0–24 dB | 0 = joelho duro; mais = entrada gradual na compressão |
| | SC HPF | Desligado, 20–500 Hz | Passa-altas (12 dB/oit.) só no detector: o grave para de puxar a compressão |
| 02 ENVELOPE E GAIN | Attack | 0,1–100 ms | Velocidade para começar a reduzir |
| | Release | 10–1000 ms | Velocidade para soltar |
| | AUTO RELEASE | liga/desliga | Release que se adapta ao programa: pico isolado solta rápido (~40 ms), redução constante solta devagar (~600 ms) |
| | visual | — | GAIN REDUCTION agora, MAX (segura 3 s) e o MAKEUP APLICADO |
| | Makeup | −12 a +24 dB | Ganho depois da compressão |
| | AUTO | liga/desliga | Soma ao MAKEUP uma estimativa (metade da redução de um pico em 0 dBFS); o knob vira ajuste fino |
| 03 OUTPUT | SC LISTEN | liga/desliga | Escuta o sinal do detector (já com o SC HPF), para achar o corte. Os presets não mexem nele |
| | Dry/Wet | 0–100% | Seco ↔ comprimido (paralelo) |
| | Gain | −36 a +12 dB | Volume final |
| | IN / OUT | −20 a +3 VU | VU de ponteiro de entrada e saída (lê o pico; +3 = 0 dBFS) |

As cores dos knobs são as do desenho: APERTO, ATTACK e RELEASE coral (a redução), THRESHOLD âmbar
(a linha), KNEE violeta (a faixa da curva), SC HPF azul (o sidechain), MAKEUP verde (o
tracejado da curva).

Detector de pico com os dois canais ligados (L e R levam a mesma redução, a imagem estéreo não mexe).

**Latência:** zero (sem lookahead); aparece no cabeçalho e é informada ao host.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/TORNO/`
- Windows: `%APPDATA%\InsideStudioSP\TORNO\Presets\`

## Janela e licença

- Visual de rack de 19": orelhas parafusadas, frente pintada na cor do plugin, knobs no padrão Moog com a escala
  serigrafada e alavancas de metal nas chaves liga/desliga. IN/OUT em VU de ponteiro, que lê o pico: o fim da escala
  (+3) é 0 dBFS e o 0 VU, −3 dBFS.
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
| `Source/CompEngine.h` | Filtro do sidechain, curva com joelho, APERTO, ataque/soltura (e soltura auto), compensação, histórico |
| `Source/PluginProcessor.*` | Mix, latência, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (frente de 1000×580 entre as orelhas do rack, janela de 1064×580 em 100%); `Theme.h`/`Widgets.h` são o padrão da marca |
| `Source/Hardware.h`, `Painel.h` | Peças renderizadas (knobs, lâmpadas) e o painel em volta (rack, alavancas, VU) |
