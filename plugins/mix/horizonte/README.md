# HORIZONTE — imagem estéreo

Imagem estéreo em M/S com **latência zero**: abre ou fecha o estéreo, com larguras separadas para
graves e agudos, deixa o grave em mono abaixo de uma frequência e gira/equilibra a imagem. O
vetorscópio mostra o som que sai, com rastro que esmaece; o medidor de correlação avisa quando
alguma coisa vai sumir em mono.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| VECTORSCOPE | visual | — | Lissajous L/R girado 45°: mono = linha em pé, largo = nuvem deitada, fora de fase = deitado no eixo S. Rastro de 8 quadros; escala automática (aparece embaixo). A linha verde mostra onde ficou o centro depois da ROTATION/BALANCE |
| | Correlation | −1 a +1 | Média de ~300 ms; abaixo de 0 (coral) parte do som some em mono. MIN segura 3 s. Também mostra SIDE/MID (dB) e o BALANCE L/R medido |
| WIDTH POR FREQUÊNCIA | visual | 20 Hz–20 kHz | Quanto do SIDE (S) sai em cada frequência (0–300%): LOW em âmbar, HIGH em azul, linha do CROSSOVER e a faixa do MONO BASS em coral |
| 01 WIDTH | **Macro WIDTH** | 0–200% | Ganho do SIDE (S): 0% = mono, 100% = como veio, 200% = o dobro |
| | Low | 0–200% | Largura abaixo do CROSSOVER (multiplica a macro) |
| | Crossover | 80 Hz–4 kHz | Onde graves e agudos se separam (Linkwitz-Riley 24 dB/oit, soma plana) |
| | High | 0–200% | Largura acima do CROSSOVER (multiplica a macro) |
| 02 BASS E POSIÇÃO | Mono bass | Desligado, 20–300 Hz | Zera o SIDE (S) abaixo da frequência (Linkwitz-Riley, soma plana) |
| | Balance | E 100 a D 100 | Abaixa o lado oposto |
| | Rotation | −45° a +45° | Gira a imagem inteira (+ = para a direita) |
| 03 OUTPUT | MONO | liga/desliga | Checagem de mono: (L+R)/2 nos dois lados. Os presets não mexem nele |
| | SIDE SOLO | liga/desliga | Escuta só o S, nos dois lados. Ligar um desliga o outro. Os presets não mexem nele |
| | Gain | −24 a +12 dB | Volume final |
| | IN / OUT | dBFS | Pico de entrada e saída |

As cores dos knobs são as do desenho: WIDTH roxo (a curva), LOW âmbar, HIGH azul, CROSSOVER
creme (a linha tracejada), MONO BASS coral (a faixa mono), BALANCE e ROTATION verdes (a linha do
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
