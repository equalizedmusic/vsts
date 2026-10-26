# SALA — reverb de mix

![SALA](https://raw.githubusercontent.com/equalizedmusic/vsts/shots/SALA.png)

Reverb **limpo e transparente** para colocar instrumentos e vozes num mesmo espaço (o oposto da
MIRAGEM, que é shimmer). Rede de 8 linhas misturadas por Hadamard, com difusores passa-tudo na
entrada, 16 reflexões iniciais por lado e um balanço lento e sutil nas linhas (tira o som metálico
sem virar chorus). No alto, a **resposta**: o golpe seco, o pré-delay, as reflexões e a curva de
decaimento, com a linha da cauda de agora. Embaixo, o **TAIL EQ** com o amortecimento e a largura.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| RESPOSTA | visual | — | Tempo em escala de raiz: DRY (creme), PRE-DELAY (azul), EARLY REFLECTIONS (violeta, mais largas com DIFFUSION), TAIL (âmbar, até −60 dB). A linha creme AGORA é o pico da cauda neste instante |
| TAIL EQ | visual | 20 Hz–20 kHz | Curva coral do LOW CUT × HIGH CUT (brilha com a energia da cauda); tracejado verde = quanto do decay sobra em cada frequência (DAMPING); barra creme = WIDTH. Rodapé: RT60 e quanto o DUCK está abaixando |
| 01 ESPAÇO | **Macro SIZE** | 0–100% | As 8 linhas e as reflexões crescem juntas (mexer estica a sala devagar, sem estalo) |
| | Decay | 0,2–12 s | RT60: tempo até a cauda cair 60 dB |
| | Pre-delay | 0–250 ms | Silêncio antes da sala responder (usado com PRE SYNC em *Livre*) |
| | Pre sync | Livre, 1/64…1/4 | Pre-delay na divisão do andamento do host (sem host, 120 BPM; máximo 1 s) |
| | Diffusion | 0–100% | Passa-tudo na entrada: 0 = ataque granulado; 100% = cauda lisa desde o começo |
| | Early ref. | 0–100% | Densidade das primeiras reflexões: quantas das 16 entram (0 = nenhuma) |
| 02 TAIL | Low cut | 20 Hz–1 kHz | Corte de grave da cauda (2 polos) |
| | High cut | 1–20 kHz | Corte de agudo da cauda (2 polos) |
| | Damping | 0–100% | Filtro dentro da sala: o agudo morre antes do grave (20 kHz → 1 kHz) |
| | Width | 0–150% | 0 = cauda mono; 100% = natural; acima, mais aberta |
| | Duck | 0–100% | A cauda abaixa (até −24 dB) enquanto o DRY toca e volta nas pausas |
| 03 OUTPUT | SÓ A SALA | liga/desliga | Escuta 100% molhado, para ajustar a sala. Os presets não mexem nele |
| | Dry/Wet | 0–100% | Seco ↔ molhado |
| | Gain | −36 a +12 dB | Volume final |

As cores dos knobs são as do desenho: SIZE e DECAY âmbar (a cauda), PRE-DELAY azul,
DIFFUSION e EARLY REF. violeta, LOW CUT e HIGH CUT coral (a curva do EQ), DAMPING verde
(o tracejado), WIDTH e DUCK creme. Medidores IN/OUT em dBFS de pico.

**Latência:** zero. A cauda informada ao host é de 13,5 s (decay máximo + pré-delay máximo).
A realimentação de cada linha é sempre menor que 1 (mesmo com decay de 12 s) e a matriz conserva a
energia, então a cauda sempre cai; se algo der NaN, a sala zera sozinha.

## Presets

Vinte de fábrica (sala pequena, estúdios, câmaras, igreja, catedral, placas, caixas, ambiente de
bateria, vocal, violão, piano…). No menu de presets: *Salvar como novo*; com um preset do usuário
aberto, também *Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome
antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/SALA/`
- Windows: `%APPDATA%\InsideStudioSP\SALA\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/RoomEngine.h` | Pré-delay, reflexões iniciais, difusão, rede de 8 linhas, largura, EQ da cauda e duck |
| `Source/PluginProcessor.*` | PRE SYNC, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
