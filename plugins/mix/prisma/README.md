# PRISMA — dinâmica + saturação multibanda

Divide o som em **4 bandas** (crossovers Linkwitz-Riley de 4ª ordem, soma plana) e em cada uma
tem um compressor e um DRIVE (saturação tanh com compensação de nível). No alto, o **espectro da
saída ao vivo** com as 4 bandas coloridas: as linhas entre elas são os cortes e **se arrastam com o
mouse**; a redução de cada banda aparece pendurada no topo da banda, e o drive, em coral, no pé.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| ESPECTRO | visual + cortes | 20 Hz – 20 kHz | Saída ao vivo (FFT de 2048, inclinação de 3 dB/oitava em torno de 1 kHz para ler os agudos). Arraste uma linha para mudar o corte; duplo clique volta ao padrão (120 Hz, 1 kHz, 6 kHz). Os cortes ficam sempre em ordem, com folga de 20% entre eles |
| | redução | 0 a 24 dB | Faixa na cor da banda pendurada no topo: quanto o compressor está tirando agora |
| | drive | — | Barra coral no pé da banda: drive efetivo (com o MACRO) |
| 01–04 BANDA | faixa e REDUÇÃO | — | Frequências da banda e medidor de redução (0 a −24 dB) |
| | SOLO | liga/desliga | Escuta só as bandas em solo. Os presets não mexem nele |
| | MUDO | liga/desliga | Tira a banda da saída. Os presets não mexem nele |
| | Limiar | −60 a 0 dB | Nível a partir do qual a banda é comprimida (joelho macio de 6 dB) |
| | Razão | 1:1 a 20:1 | Quanto comprime acima do limiar |
| | Ataque | 0,1 a 100 ms | Rapidez para começar a comprimir |
| | Soltura | 10 a 1000 ms | Rapidez para soltar |
| | Drive | 0 a +24 dB | Saturação suave depois do compressor; o nível em −12 dBFS fica igual. De 0 a +6 dB entra aos poucos |
| | Ganho | −18 a +18 dB | Volume da banda |
| 05 GLOBAL | **Macro** | 0–100% | Em todas as bandas juntas: limiar até −12 dB, razão até +3 e drive até +12 dB |
| | Mix | 0–100% | Seco ↔ processado (paralelo; o seco passa pelos mesmos crossovers, então não há cancelamento de fase) |
| | Ganho | −24 a +12 dB | Volume final |
| | IN / OUT | dBFS de pico | Medidores de entrada e saída |

As cores dos knobs são as do desenho: limiar, razão, ataque e soltura na **cor da banda** (a mesma
da faixa no espectro e da redução): graves âmbar, médio-graves verde, médio-agudos azul, agudos
violeta. DRIVE e MACRO coral (a barra do drive), ganho creme.

O compressor detecta pelo maior dos dois canais (L/R ligados): a imagem estéreo não mexe.

**Latência:** zero (crossovers IIR, sem oversampling); aparece no cabeçalho e é informada ao host.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.
Trocar de preset não mexe em SOLO, MUDO nem no bypass.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/PRISMA/`
- Windows: `%APPDATA%\InsideStudioSP\PRISMA\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/MultibandEngine.h` | Crossovers LR4 + all-pass, compressor por banda, DRIVE, solo/mudo, anel da saída para o espectro |
| `Source/PluginProcessor.*` | Mix, latência, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
