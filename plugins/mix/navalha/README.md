# NAVALHA — clipper

Clipper com oversampling de 4× (FIR de fase linear) que **mostra o que está cortando**: no alto,
a forma de onda dos últimos ~4 s, com o que passa em creme e o que a navalha cortou em coral,
mais a linha do CEILING. A curva de entrada × saída mostra o ponto do pico de agora, e o painel de
análise diz quanto está cortando.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| WAVEFORM | visual | ~4 s | Pico de cada fatia de 8 ms: creme = saiu, coral = cortado; linha âmbar = CEILING (azul = teto negativo com ASYMMETRY). A escala acompanha os picos |
| CURVA | visual | — | Entrada × saída com o joelho atual; o ponto coral é o pico de agora e o traço coral, o quanto foi cortado |
| 01 CORTE | **Macro DRIVE** | 0 a +24 dB | Ganho antes do corte: empurra o som contra o CEILING |
| | Ceiling | −12 a 0 dB | Nível máximo da saída |
| | Knee | 0–100% | 0 = corte reto (duro); mais = joelho arredondado (até metade do teto) |
| | Asymmetry | 0–100% | O lado negativo corta até 50% antes: harmônicos pares, som "quente" |
| | Link L/R | 0–100% | 100% = os dois canais levam o mesmo corte (a imagem estéreo não mexe) |
| 02 ANÁLISE | visual | — | Cortando agora, MAX (segura 3 s), % de amostras cortadas, PEAK PÓS-DRIVE e OUTPUT PEAK |
| 03 OUTPUT | SÓ O CORTE | liga/desliga | Escuta só o que está sendo tirado (para achar o ponto). Os presets não mexem nele |
| | Dry/Wet | 0–100% | Seco ↔ cortado (paralelo; o seco sai com a mesma latência) |
| | Gain | −36 a +12 dB | Volume final |

As cores dos knobs são as do desenho: DRIVE coral (o corte), CEILING âmbar (a linha), ASYMMETRY
azul (o teto negativo).

**Latência:** a do oversampling (aparece no cabeçalho), informada ao host. O pico na saída pode
passar um pouco do CEILING entre amostras, por causa do filtro do oversampling.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/NAVALHA/`
- Windows: `%APPDATA%\InsideStudioSP\NAVALHA\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/ClipEngine.h` | DRIVE, oversampling 4×, curva com joelho/assimetria/link, histórico da forma de onda |
| `Source/PluginProcessor.*` | Mix, latência, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
