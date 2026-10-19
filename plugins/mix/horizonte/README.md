# HORIZONTE — imagem estéreo

Imagem estéreo em M/S com **latência zero**: abre ou fecha o estéreo, com larguras separadas para
graves e agudos, deixa o grave em mono abaixo de uma frequência e gira/equilibra a imagem. O
vetorscópio mostra o som que sai, com rastro que esmaece; o medidor de correlação avisa quando
alguma coisa vai sumir em mono.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| VETORSCÓPIO | visual | — | Lissajous L/R girado 45°: mono = linha em pé, largo = nuvem deitada, fora de fase = deitado no eixo S. Rastro de 8 quadros; escala automática (aparece embaixo). A linha verde mostra onde ficou o centro depois da ROTAÇÃO/BALANÇO |
| | Correlação | −1 a +1 | Média de ~300 ms; abaixo de 0 (coral) parte do som some em mono. Mínimo segura 3 s. Também mostra LADO/MEIO (dB) e o BALANÇO L/R medido |
| LARGURA POR FREQUÊNCIA | visual | 20 Hz–20 kHz | Quanto do lado (S) sai em cada frequência (0–300%): graves em âmbar, agudos em azul, linha da DIVISÃO e a faixa do GRAVE MONO em coral |
| 01 LARGURA | **Macro LARGURA** | 0–200% | Ganho do lado (S): 0% = mono, 100% = como veio, 200% = o dobro |
| | Graves | 0–200% | Largura abaixo da DIVISÃO (multiplica a macro) |
| | Divisão | 80 Hz–4 kHz | Onde graves e agudos se separam (Linkwitz-Riley 24 dB/oit, soma plana) |
| | Agudos | 0–200% | Largura acima da DIVISÃO (multiplica a macro) |
| 02 GRAVE E POSIÇÃO | Grave mono | Desligado, 20–300 Hz | Zera o lado (S) abaixo da frequência (Linkwitz-Riley, soma plana) |
| | Balanço | E 100 a D 100 | Abaixa o lado oposto |
| | Rotação | −45° a +45° | Gira a imagem inteira (+ = para a direita) |
| 03 SAÍDA | OUVIR EM MONO | liga/desliga | Checagem de mono: (L+R)/2 nos dois lados. Os presets não mexem nele |
| | OUVIR SÓ O LADO | liga/desliga | Escuta só o S, nos dois lados. Ligar um desliga o outro. Os presets não mexem nele |
| | Ganho | −24 a +12 dB | Volume final |
| | IN / OUT | dBFS | Pico de entrada e saída |

As cores dos knobs são as do desenho: LARGURA roxo (a curva), GRAVES âmbar, AGUDOS azul, DIVISÃO
creme (a linha tracejada), GRAVE MONO coral (a faixa mono), BALANÇO e ROTAÇÃO verdes (a linha do
centro no vetorscópio).

**Latência:** zero (só filtros IIR). O M passa pelos mesmos divisores que o S, para os dois ficarem
em fase. Só funciona em pista estéreo (entrada e saída estéreo).

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/HORIZONTE/`
- Windows: `%APPDATA%\InsideStudioSP\HORIZONTE\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/ImageEngine.h` | M/S, divisores Linkwitz-Riley (SVF), larguras, grave mono, rotação, balanço, escuta, dados do vetorscópio e da correlação |
| `Source/PluginProcessor.*` | Ganho, bypass, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
