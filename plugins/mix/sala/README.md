# SALA — reverb de mix

Reverb **limpo e transparente** para colocar instrumentos e vozes num mesmo espaço (o oposto da
MIRAGEM, que é shimmer). Rede de 8 linhas misturadas por Hadamard, com difusores passa-tudo na
entrada, 16 reflexões iniciais por lado e um balanço lento e sutil nas linhas (tira o som metálico
sem virar chorus). No alto, a **resposta**: o golpe seco, o pré-delay, as reflexões e a curva de
decaimento, com a linha da cauda de agora. Embaixo, o **EQ da cauda** com o amortecimento e a largura.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| RESPOSTA | visual | — | Tempo em escala de raiz: seco (creme), pré-delay (azul), reflexões (violeta, mais largas com DIFUSÃO), cauda (âmbar, até −60 dB). A linha creme AGORA é o pico da cauda neste instante |
| EQ DA CAUDA | visual | 20 Hz–20 kHz | Curva coral do passa-altas × passa-baixas (brilha com a energia da cauda); tracejado verde = quanto do decay sobra em cada frequência (AMORTECIMENTO); barra creme = LARGURA. Rodapé: RT60 e quanto o DUCK está abaixando |
| 01 ESPAÇO | **Macro TAMANHO** | 0–100% | As 8 linhas e as reflexões crescem juntas (mexer estica a sala devagar, sem estalo) |
| | Decay | 0,2–12 s | RT60: tempo até a cauda cair 60 dB |
| | Pré-delay | 0–250 ms | Silêncio antes da sala responder (usado com PRÉ NO TEMPO em *Livre*) |
| | Pré no tempo | Livre, 1/64…1/4 | Pré-delay na divisão do andamento do host (sem host, 120 BPM; máximo 1 s) |
| | Difusão | 0–100% | Passa-tudo na entrada: 0 = ataque granulado; 100% = cauda lisa desde o começo |
| | Reflexões | 0–100% | Densidade das primeiras reflexões: quantas das 16 entram (0 = nenhuma) |
| 02 CAUDA | Passa-altas | 20 Hz–1 kHz | Corte de grave da cauda (2 polos) |
| | Passa-baixas | 1–20 kHz | Corte de agudo da cauda (2 polos) |
| | Amortecimento | 0–100% | Filtro dentro da sala: o agudo morre antes do grave (20 kHz → 1 kHz) |
| | Largura | 0–150% | 0 = cauda mono; 100% = natural; acima, mais aberta |
| | Duck | 0–100% | A cauda abaixa (até −24 dB) enquanto o seco toca e volta nas pausas |
| 03 SAÍDA | SÓ A SALA | liga/desliga | Escuta 100% molhado, para ajustar a sala. Os presets não mexem nele |
| | Mix | 0–100% | Seco ↔ molhado |
| | Ganho | −36 a +12 dB | Volume final |

As cores dos knobs são as do desenho: TAMANHO e DECAY âmbar (a cauda), PRÉ-DELAY azul,
DIFUSÃO e REFLEXÕES violeta, PASSA-ALTAS e PASSA-BAIXAS coral (a curva do EQ), AMORTECIMENTO verde
(o tracejado), LARGURA e DUCK creme. Medidores IN/OUT em dBFS de pico.

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
| `Source/PluginProcessor.*` | Pré no tempo, mix, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
