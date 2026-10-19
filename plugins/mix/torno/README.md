# TORNO — compressor

Compressor de mix que **mostra o que está apertando**: no alto, o histórico dos últimos ~4 s, com o
nível de entrada em creme subindo de baixo e a redução de ganho em coral descendo de cima, mais a
linha do LIMIAR. A curva de entrada × saída mostra o joelho, a compensação e o ponto de agora.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| HISTÓRICO | visual | ~4 s | Fatias de 8 ms: creme = pico de entrada (escala 0 a −60 dBFS, na direita), coral = redução de ganho (escala de 6/12/24/48 dB, acompanha o máximo); linha âmbar = LIMIAR |
| CURVA | visual | −60 a 0 dB | Entrada × saída: faixa violeta = joelho, linha âmbar = limiar, tracejado verde = saída com a compensação; o ponto coral é o nível de agora e o traço coral, a redução aplicada |
| 01 COMPRESSÃO | **Macro APERTO** | 0–100% | Desce o LIMIAR (até −24 dB) e multiplica a RAZÃO (até 2,5×, no máximo 20:1) juntos |
| | Limiar | −60 a 0 dB | Nível a partir do qual comprime |
| | Razão | 1:1 a 20:1 | Quanto comprime acima do limiar |
| | Joelho | 0–24 dB | 0 = joelho duro; mais = entrada gradual na compressão |
| | Filtro SC | Desligado, 20–500 Hz | Passa-altas (12 dB/oit.) só no detector: o grave para de puxar a compressão |
| 02 TEMPO E GANHO | Ataque | 0,1–100 ms | Velocidade para começar a reduzir |
| | Soltura | 10–1000 ms | Velocidade para soltar |
| | SOLTURA AUTO | liga/desliga | Soltura que se adapta ao programa: pico isolado solta rápido (~40 ms), redução constante solta devagar (~600 ms) |
| | visual | — | Reduzindo agora, máximo (segura 3 s) e a compensação aplicada |
| | Compensação | −12 a +24 dB | Ganho depois da compressão |
| | AUTO | liga/desliga | Soma à compensação uma estimativa (metade da redução de um pico em 0 dBFS); o knob vira ajuste fino |
| 03 SAÍDA | OUVIR SIDECHAIN | liga/desliga | Escuta o sinal do detector (já com o FILTRO SC), para achar o corte. Os presets não mexem nele |
| | Mix | 0–100% | Seco ↔ comprimido (paralelo) |
| | Ganho | −36 a +12 dB | Volume final |
| | IN / OUT | −48 a 0 dBFS | Medidores de pico |

As cores dos knobs são as do desenho: APERTO, ATAQUE e SOLTURA coral (a redução), LIMIAR âmbar
(a linha), JOELHO violeta (a faixa da curva), FILTRO SC azul (o sidechain), COMPENSAÇÃO verde (o
tracejado da curva).

Detector de pico com os dois canais ligados (L e R levam a mesma redução, a imagem estéreo não mexe).

**Latência:** zero (sem lookahead); aparece no cabeçalho e é informada ao host.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/TORNO/`
- Windows: `%APPDATA%\InsideStudioSP\TORNO\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/CompEngine.h` | Filtro do sidechain, curva com joelho, APERTO, ataque/soltura (e soltura auto), compensação, histórico |
| `Source/PluginProcessor.*` | Mix, latência, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
