# BARRAGEM — limiter de master (true peak + LUFS)

Limiter de master com detecção em **true peak** (oversampling de 4× só no caminho de detecção) e
medidor de **loudness** (BS.1770 / EBU R128). No alto, a forma de onda dos últimos ~4 s: o que sai
em creme, o que a barragem segurou em coral, a redução de ganho em azul pendurada do alto e a linha
do CEILING. À esquerda, o LUFS integrado com a distância até o alvo escolhido (Spotify, Apple Music,
YouTube, Club…).

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| WAVEFORM | visual | ~4 s | Pico de cada fatia de 8 ms: creme = saiu, coral = segurado; linha azul = GAIN REDUCTION (régua em dB à direita, abre para 24 dB quando passa de 12); linha âmbar = CEILING |
| LOUDNESS | visual | — | INTEGRATED (com gate), distância até o TARGET, MOMENTARY (400 ms), SHORT-TERM (3 s) e tempo medido. Barra de −32 a −4 LUFS: faixa = short-term, traço = momentary, triângulo = integrated, verde = target |
| | Target | 12 alvos | Clique na caixa TARGET: Spotify −14, Spotify Alto −11, Apple Music −16, YouTube −14, Tidal −14, Amazon Music −14, Deezer −15, Podcast −16, Club −9, Pista Pesada −7, TV EBU R128 −23, TV ATSC A/85 −24 |
| 01 LIMITE | **Macro ENCHER** | 0 a +24 dB | Ganho antes do limiter: empurra o som contra o CEILING |
| | Ceiling | −12 a 0 dBTP | Nível máximo da saída, valendo também entre as amostras (true peak) |
| | Release | 1 a 1000 ms | Quanto tempo o ganho leva para voltar depois de segurar |
| | AUTO RELEASE | liga/desliga | Pico isolado solta rápido (até 0,3× do RELEASE); quando o limiter trabalha o tempo todo, solta devagar (até 4×) e não bombeia |
| | Lookahead | 0,5 a 5 ms | Tamanho da rampa que abaixa o ganho antes do pico: curto = mais punch, longo = mais limpo |
| | Link L/R | 0–100% | 100% = os dois canais levam a mesma redução (a imagem estéreo não mexe) |
| 02 ANÁLISE | visual | — | Segurando agora, MAX (segura 3 s), PEAK DEPOIS DO ENCHER e OUTPUT PEAK, em dBFS |
| 03 OUTPUT | DELTA | liga/desliga | Escuta só o que a barragem está tirando. Os presets não mexem nele |
| | RESET INTEGRATED | golpe | Recomeça a medida do integrado (e o tempo medido). Os presets não mexem nele |
| | Picos e medidores | — | INPUT PEAK e OUTPUT PEAK do plugin em dBFS (segura 3 s) e medidores IN/OUT |

As cores dos knobs são as do desenho: ENCHER coral (o segurado), CEILING âmbar (a linha), RELEASE e
LOOKAHEAD azul (a curva de redução), e o TARGET em verde (a marca na barra de LUFS).

**Latência:** fixa, informada ao host = janela de lookahead máxima (5 ms) + latência do filtro do
oversampling (aparece no cabeçalho). Girar o LOOKAHEAD muda só a rampa, não a latência — o host não
precisa recompensar. O áudio não passa pelo filtro do oversampling: o que sai é o próprio som, só com
ganho, mais uma trava final no CEILING.

**LUFS:** medido na saída do limiter (antes do DELTA e do bypass), com K-weighting e gates
de −70 LUFS e −10 LU da BS.1770-4. O integrado guarda os blocos num histograma de 0,1 LU, então mede
horas sem crescer a memória.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.
O TARGET faz parte do preset; DELTA, RESET INTEGRATED e bypass não.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/BARRAGEM/`
- Windows: `%APPDATA%\InsideStudioSP\BARRAGEM\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos, alvos de loudness e presets de fábrica |
| `Source/LimiterEngine.h` | ENCHER, detecção true peak 4×, mínimo na janela, rampa do lookahead, soltura (fixa/auto), histórico da forma de onda |
| `Source/Loudness.h` | K-weighting e LUFS momentâneo, curto e integrado (BS.1770-4) |
| `Source/PluginProcessor.*` | Bypass, latência, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
