# MONITOR — medidor de loudness

Medidor para entrega: **não altera o som** (a saída é a entrada, bit a bit, enquanto as checagens
estão desligadas) e não tem latência. Mede LUFS momentâneo, curto e integrado (BS.1770-4, filtro K,
portões −70 LUFS e −10 LU), LRA (EBU Tech 3342), true peak com oversampling de 4× e máximo segurado,
pico e RMS em dBFS, espectro, correlação de fase e goniômetro. Os presets são **alvos de entrega**
(Spotify, Apple Music, YouTube, EBU R128…): mudam o alvo, o teto e as faixas de cor.

## Telas

| Bloco | Leitura | Unidade | O que mostra |
|---|---|---|---|
| LOUDNESS | Integrated (verde) | LUFS | Desde o ZERAR. Ao lado: ON TARGET / ACIMA / ABAIXO, comparando com o TARGET ± FAIXA |
| | Momentary (creme) | LUFS | Janela de 400 ms |
| | Short-term (âmbar) e short-term max | LUFS | Janela de 3 s; o máximo é desde o ZERAR |
| | LRA (azul) | LU | Faixa de loudness (percentis 10 a 95 dos curtos); avisa se passar do LRA MAX |
| | True peak max (coral) | dBTP | Maior pico entre amostras desde o ZERAR; avisa (OVER CEILING) se passar do TP CEIL |
| | Tempo medido | — | Desde o ZERAR |
| HISTÓRICO | short (âmbar), momentâneo (creme leve) | LUFS | Últimos 60 s. Linha violeta = TARGET, faixa violeta = FAIXA, coral leve = acima; tracejado verde = integrado; colchete azul à direita = LRA |
| SPECTRUM | ao vivo (âmbar), média (creme) | dBFS | FFT de 4096 pontos, 20 Hz a 20 kHz; a média anda devagar (~3 s). Um seno de 0 dBFS marca 0 |
| STEREO | goniômetro (creme) | — | Vertical = mono; só L sobe para a esquerda, só R para a direita. Escala automática |
| | Correlation | −1 a +1 | Verde = em fase; coral = negativa (some no mono) |
| LEVEL | Peak (creme), TP (coral), RMS (azul) | dBFS / dBTP / dBFS | Barras com a marca do máximo dos últimos 2 s (o número é a marca). Linha coral = TP CEIL. RMS sem a correção de +3 dB (seno de 0 dBFS = −3 dBFS RMS) |

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| 01 TARGET | **Macro TARGET** | −36 a −6 LUFS | Loudness integrado pedido (linha violeta) |
| | TP ceil | −6 a 0 dBTP | True peak máximo aceito (linha coral no LEVEL) |
| | Faixa | ±0,5 a ±6 LU | Tolerância em volta do TARGET (faixa violeta) |
| | LRA max | 3 a 30 LU | LRA máximo aceito |
| 02 CHECAGEM | MONO | liga/desliga | Soma L+R nos dois lados (só o que se ouve) |
| | MATCH TARGET | liga/desliga | Baixa o volume até o TARGET a partir do integrado, como as plataformas fazem; nunca sobe |
| | ZERAR | toque | Recomeça integrado, LRA, curto máximo, true peak máximo e o tempo |
| | CONGELAR | liga/desliga | Para a tela (a medição continua). Não é parâmetro |

A medição é sempre da **entrada**, antes das checagens. Os presets não mexem em MONO, MATCH TARGET,
CONGELAR nem no bypass. Entrada mono conta como um canal só no loudness, como pede a norma.

**Latência:** nenhuma. O oversampling do true peak é só análise; o áudio passa direto.

## Presets

Vinte alvos de fábrica (Init, Spotify, Spotify Alto, Apple Music, YouTube, Deezer, Tidal, Amazon Music,
SoundCloud, Club / DJ, Master Psy, Rádio FM, EBU R128, ATSC A/85, Netflix, Podcast, Podcast Mono,
Audiolivro ACX, Games ASWG, Mix para Master). São referências; confira a regra atual de cada
plataforma. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/MONITOR/`
- Windows: `%APPDATA%\InsideStudioSP\MONITOR\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e alvos de fábrica |
| `Source/MeterEngine.h` | Filtro K, momentâneo/curto/integrado (histograma com portões), LRA, true peak 4×, pico, RMS, correlação e os anéis da interface |
| `Source/Analyzer.h` | FFT do espectro (na thread da interface, nunca no áudio) |
| `Source/PluginProcessor.*` | Checagens (mono, no alvo), estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
