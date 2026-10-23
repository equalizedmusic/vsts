# CALEIDO — glitch pattern FX

Efeito de glitch sincronizado ao BPM do projeto, pensado para psytrance. O compasso é
fatiado em **16 passos** e cada passo pode ser repetido, invertido, transposto, filtrado,
esmagado (crush) e girar no estéreo. Quem decide o que acontece em cada passo é a
**mandala**: 5 anéis × 16 passos, um anel por efeito.

O som que entra fica gravado num buffer de 16 s. Passo sem efeito toca ao vivo (latência
zero); repetir, inverter, acelerar a fita e embaralhar leem o que já passou.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| MANDALA | anéis | clique liga/desliga | De fora para dentro: REPETE (âmbar), REVERSO (coral), FITA (verde), FILTER (azul), CRUSH (violeta). Os knobs de cada um têm a mesma cor |
| | GERAR | — | Sorteia os knobs do padrão (CAOS, VIAGEM, REPETIÇÃO, EMBARALHAR, REVERSO, FITA, FILTER, CRUSH) e uma semente nova: knobs e mandala mudam juntos |
| 01 CORTE | **Macro CAOS** | 0–100% | Chance de cada passo ser repetido, invertido ou trocado de lugar |
| | Divisão | 1/8 · 1/16 · 1/32 | Tamanho do passo (16 passos = 2 compassos, 1 ou meio) |
| | Repetição | até 2× … 16× | Máximo de picotes num passo do anel REPETE |
| | Embaralhar | 0–100% | Chance (× CAOS) de um passo tocar outro passo de até 7 antes |
| | Reverso | 0–100% | Peso do anel REVERSO no sorteio |
| 02 VIAGEM | **Macro VIAGEM** | 0–100% | Chance de cor (fita, filtro, crush) e profundidade do giro |
| | Fita | ±0–24 st | Faixa de transposição; 30% dos passos de fita freiam até quase parar |
| | Filter | 0–100% | Peso no sorteio, ressonância e tamanho da varredura (LP/BP/HP) |
| | Crush | 16→2 bit | Bits e redução de taxa nos passos do anel CRUSH |
| | Giro | 1/4 … 1/32 | Velocidade do pan circular nos passos coloridos |
| 03 OUTPUT | Transmutar | liga/desliga (desligado) | Semente nova a cada 2 voltas da mandala (os knobs ficam). Os presets não mexem nele |
| | Segurar | liga/desliga | Prende os 16 passos anteriores e toca em loop (entra na virada do passo) |
| | Dry/Wet | 0–100% | Seco ↔ processado |
| | Gain | −36 a +12 dB | Volume final |

Mexer em CAOS, VIAGEM, REVERSO, FILTER ou CRUSH refaz o padrão **com a mesma semente**: os
passos acendem ou apagam sem embaralhar tudo. Os sorteios são determinísticos, então o
padrão se repete igual a cada volta até TRANSMUTAR ou GERAR.

Com o Ableton tocando, segue a régua do projeto (**SYNC**): o passo é o da posição do play no
beat grid. Com o Ableton parado, a mandala fica no passo 1 e o som passa limpo (**AGUARDANDO
PLAY**). Só anda sozinha no BPM (**LIVRE**) em host que não informa a posição.

## Presets

Vinte de fábrica, cada um com a sua semente. O preset do usuário guarda também o padrão da
mandala. No menu de presets: *Salvar como novo*, e no preset do usuário aberto *Salvar alterações*,
*Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

Pasta:

- Mac: `~/Library/Audio/Presets/InsideStudioSP/CALEIDO/`
- Windows: `%APPDATA%\InsideStudioSP\CALEIDO\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/GlitchEngine.h` | Relógio, buffer, padrão, vozes por passo, crush, filtro e giro |
| `Source/PluginProcessor.*` | Host/BPM, mix, estado (com o padrão) e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
