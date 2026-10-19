# CONSOLE — cor de canal / soma de mesa

Cor de canal de mesa analógica com oversampling de 2× ou 4× (FIR de fase linear) na saturação.
Três modelos de mesa, cada um com a sua curva, o seu equilíbrio de harmônicos pares/ímpares, uma
cor fixa de EQ e o seu ruído de fundo. No alto, o sinal que entra no canal e a **cor que ele
acrescenta** ao vivo; a curva de transferência mostra o ponto do pico de agora, e as barras dizem
quanto de 2º, 3º, 4º e 5º harmônico está nascendo.

## Modelos

| Modelo | Inspiração | Curva | EQ fixo | Ruído |
|---|---|---|---|---|
| **Cristal** | mesa britânica limpa | muita folga, quase só ímpares (3º) | brilho leve (+0,6 dB de inclinação) | chiado claro, sem zumbido |
| **Ferro** | canal americano com transformador | aperta mais cedo, pares e ímpares | +1,2 dB no grave (110 Hz), topo cai em 18 kHz | chiado médio + zumbido leve |
| **Brasa** | canal valvulado | macia, predominam os pares (2º) | −0,8 dB de inclinação, topo cai em 13 kHz | chiado escuro + zumbido de 60 Hz |

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| MESA | visual | ~4 s | Pico de cada fatia de 8 ms em dBFS: creme = sinal que chega na curva (depois do DRIVE), coral = cor gerada (o quanto a curva afasta o sinal da reta), linha violeta = RUÍDO. No título, pico de entrada e de saída em dBFS |
| CURVA | visual | — | Transferência do modelo (âmbar) contra a reta (tracejada); o ponto coral é o pico de agora. Embaixo, o modelo, o DRIVE em dB e a descrição do modelo |
| 01 CANAL | **Macro DRIVE** | 0–100% | Ganho antes da curva: 0 a +24 dB. Empurra o canal |
| | Modelo | Cristal / Ferro / Brasa | Curva, EQ fixo e ruído do canal |
| | Tom | −6 a +6 dB | Inclinação em volta de 700 Hz: + = agudo sobe e grave desce |
| | Crosstalk | 0–100% | Vazamento L↔R entre 150 Hz e 7 kHz, até −16,5 dB |
| | Ruído | desligado, −96 a −60 dBFS | Ruído de fundo do modelo (entra depois da compensação) |
| 02 HARMÔNICOS | visual | — | 2º a 5º harmônico em dB em relação à fundamental, no nível de agora; THD; pares × ímpares; resposta do canal (EQ do modelo + TOM em verde, vazamento L↔R em azul) |
| 03 SAÍDA | COMPARAR | liga/desliga | Escuta o seco **com o mesmo volume** do processado (A/B honesto). Os presets não mexem nele |
| | AUTO | liga/desliga | Compensação automática: o canal sai com o mesmo volume (RMS de ~0,5 s) que entrou; limite de ±24 dB |
| | OS 4× | liga/desliga | Oversampling 4× (desligado = 2×, gasta menos CPU). Os presets não mexem nele |
| | Mix | 0–100% | Seco ↔ canal (paralelo; o seco sai com a mesma latência) |
| | Ganho | −36 a +12 dB | Volume final |

Embaixo do GANHO aparece a compensação do AUTO (ou, com COMPARAR ligado, o ajuste aplicado no seco).

As cores dos knobs são as do desenho: DRIVE coral (a cor gerada e o ponto na curva), MODELO âmbar
(a curva), TOM verde (a resposta), CROSSTALK azul (o vazamento), RUÍDO violeta (a linha de ruído).

**Latência:** a do oversampling 4× (aparece no cabeçalho), informada ao host. No 2× o motor completa
o atraso até a do 4×, então trocar a qualidade não muda a latência.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.
Trocar de preset não mexe em COMPARAR, OS 4× nem no bypass.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/CONSOLE/`
- Windows: `%APPDATA%\InsideStudioSP\CONSOLE\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos, nomes/descrições dos modelos e presets de fábrica |
| `Source/ConsoleEngine.h` | Modelos, DRIVE, oversampling 2×/4×, curva, EQ, TOM, crosstalk, compensação, ruído, histórico |
| `Source/PluginProcessor.*` | Mix, ganho, COMPARAR com volume casado, latência, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |

O alvo do CMake chama `ConsoleIsp` (um nome genérico como "Console" poderia colidir); o bundle e o
zip saem como `CONSOLE` porque vêm do PRODUCT_NAME.
