# Terminologia: jargão de áudio em INGLÊS, função própria do plugin em PT-BR

Pedido do dono (23/10/2026): "ajustar tudo que é termo técnico de plugin — threshold, attack, decay,
sustain, release, dry/wet, coisas desse tipo — porque são termos já usados no dia a dia. Funções que
são próprias do plugin, aí sim, mantêm nome em PT-BR."

## Onde trocar
1. `Source/PluginEditor.cpp` (e `.h` se tiver texto): rótulos de knob, botão, legenda, leitura (chips),
   títulos de seção, unidades.
2. `Source/Params.h` (ou onde o layout é criado): o NOME do parâmetro que o DAW mostra na automação
   (Title case: "Threshold", "Attack", "Auto release", "Dry/Wet").
3. NÃO mudar ids de parâmetro (`id::xxx`, strings do ParameterID) — quebra preset e automação salva.
4. NÃO mexer em `Theme.h` nem `Widgets.h` (são cópias idênticas em todos os plugins; o dono do
   processo cuida deles).
5. NÃO mexer em nome de preset (são nomes criativos, ficam em PT).
6. Menus/diálogos (Salvar preset, Tamanho da janela, Renomear, Apagar, AGUARDANDO PLAY, LIVRE, SYNC)
   ficam como estão.
7. Frases de ajuda/descrição (drawDesc, textos corridos) continuam em PT-BR, mas citando o knob pelo
   NOVO nome. Ex.: "Desce o THRESHOLD e sobe o RATIO juntos."

## Dicionário (aplicar; MAIÚSCULO na tela, Title case no Params.h)
| PT atual | EN |
|---|---|
| LIMIAR | THRESHOLD |
| RAZÃO | RATIO |
| JOELHO | KNEE |
| ATAQUE | ATTACK |
| DECAIMENTO / QUEDA (de envelope) | DECAY |
| SUSTENTAÇÃO | SUSTAIN |
| SOLTURA / LIBERAÇÃO | RELEASE (SOLTURA AUTO → AUTO RELEASE) |
| COMPENSAÇÃO (makeup) | MAKEUP (COMPENSAÇÃO AUTO → AUTO MAKEUP) |
| MISTURA / SECO-MOLHADO / knob de mix seco×processado | DRY/WET |
| SECO (quando é o sinal sem efeito) | DRY |
| GANHO | GAIN |
| SAÍDA / ENTRADA | OUTPUT / INPUT (em leitura curta: OUT / IN) |
| REDUÇÃO (de ganho) | GAIN REDUCTION (curto: GR) |
| LARGURA (estéreo) | WIDTH |
| RESSONÂNCIA | RESONANCE (curto: RES) |
| CORTE (frequência de filtro) | CUTOFF |
| CORTE GRAVE / FILTRO PASSA-ALTAS | LOW CUT / HPF ; CORTE AGUDO → HIGH CUT / LPF |
| PASSA-BAIXAS / PASSA-ALTAS / PASSA-FAIXA | LOW-PASS / HIGH-PASS / BAND-PASS ; PB 12 → LP 12, PA 12 → HP 12, PF 12 → BP 12 |
| dB/OIT | dB/OCT |
| POLOS | POLES |
| REALIMENTAÇÃO | FEEDBACK |
| PROFUNDIDADE | DEPTH |
| TAXA / VELOCIDADE (de LFO) | RATE |
| TEMPO (tempo de delay) | TIME |
| PRÉ-DELAY / Pré-delay | PRE-DELAY / Pre-delay |
| TAMANHO (de reverb/sala) | SIZE |
| CAUDA | TAIL ; REFLEXÕES → EARLY REFLECTIONS (curto: ER) ; ABAFAR/AMORTECIMENTO → DAMPING ; DIFUSÃO → DIFFUSION |
| DENSIDADE | DENSITY |
| AFINAÇÃO | PITCH (ou TUNE no synth) ; AJUSTE FINO → FINE ; OITAVA → OCTAVE |
| VOZES / UNÍSSONO | VOICES / UNISON |
| FORMA DE ONDA | WAVEFORM ; POSIÇÃO (wavetable) → POSITION |
| PORTADORA (ring mod) | CARRIER |
| SATURAÇÃO | SATURATION ; RUÍDO → NOISE |
| TETO (limiter/clipper) | CEILING ; TETO TP → TP CEILING |
| ALVO (LUFS) | TARGET |
| INTEGRADO / TRUE PEAK MÁX / PICO / MÁX | INTEGRATED / TRUE PEAK MAX / PEAK / MAX |
| LATÊNCIA | LATENCY |
| CORRELAÇÃO / VETORSCÓPIO / ESPECTRO / FASE | CORRELATION / VECTORSCOPE / SPECTRUM / PHASE |
| ESTÉREO / LADO (S) / MEIO (M) | STEREO / SIDE (S) / MID (M) |
| GRAVES / AGUDOS / MÉDIOS (EQ/tom) | LOW / HIGH / MID |
| GRAVE MONO | MONO BASS |
| PASSOS / PASSO | STEPS / STEP |
| VELOCIDADE (MIDI) / PRESSÃO (MPE) | VELOCITY / PRESSURE |
| VELOCIDADE DA FITA | TAPE SPEED |
| OUVIR SIDECHAIN / FILTRO SC | SC LISTEN / SC HPF |
| OUVIR DIFERENÇA | DELTA |
| OUVIR EM MONO | MONO |
| OUVIR SÓ O LADO | SIDE SOLO |
| FILTRO / OSCILADOR / ENVELOPE / MATRIZ (de modulação) | FILTER / OSC / ENVELOPE / MOD MATRIX |
| FONTE / DESTINO / QUANTIDADE (na matriz) | SOURCE / DEST / AMOUNT |
| NÍVEL (volume de osc/camada) | LEVEL |
| DISTORÇÃO / ATRASO (como efeito) | DISTORTION / DELAY |

Termos que JÁ estão em inglês (DRIVE, MIX→ver abaixo, DECAY, SUSTAIN, LFO, PAN, SWING, DUCK, SHIMMER,
WARP, DETUNE, CRUSH, PHASER, RMS, LUFS, LRA, OVERSAMPLING) ficam.
Knob chamado "MIX" que mistura seco×processado → **DRY/WET** (o dono citou "dry/wet" textualmente).
"MIX" como nome da LINHA (selo "MIX · PICO") fica.

## Fica em PT-BR (função própria do plugin / nome de marca)
Macros e botões de performance com nome criativo: APERTO, TRANSMUTAR, CONGELAR, REVERSO, EMBARALHAR,
PRENDER, SÓ O CORTE, DROP, ESPAÇO (macro de espaço da CHAMA), TENSÃO, SOCO, ENCHER, VIAGEM, SEGURAR,
INFINITO, DOBRAR, ROTAÇÃO da espiral, MODELO/MODO/TIPO (palavras comuns, não jargão), HISTÓRICO,
CURVA, ANÁLISE, nomes de modelo (Cristal/Ferro/Brasa) etc. Na dúvida: se o termo aparece em inglês no
painel de qualquer plugin comercial conhecido (FabFilter, Waves, Serum, Valhalla), vai para inglês; se
é um conceito inventado para este plugin, fica em PT.
Títulos de seção que misturam: traduza só o termo técnico ("TEMPO E GANHO" com attack/release/makeup →
"ATTACK · RELEASE · GAIN" ou "ENVELOPE E GAIN"; escolha o que couber).

## Cuidado com espaço
Inglês às vezes é mais longo (LIMIAR→THRESHOLD, LARGURA→WIDTH é menor). Para cada rótulo trocado,
confira a largura disponível no código (bounds do knob/chip/legenda, tamanho da fonte) e, se não couber,
use a forma curta consagrada (THRESH, RES, REL, ATK, GR, FDBK) ou reduza o espaço entre itens. Legendas
posicionadas com x acumulado: conferir se a soma não estoura a largura do card. Não há compilador local:
o código precisa compilar de primeira (C++17/JUCE) — mudança mínima, só strings e, se preciso, larguras.

## Entrega
Ao terminar, responda com: arquivos alterados, lista "antes → depois" por plugin, e qualquer rótulo
que você deixou em PT de propósito (com o motivo) ou que pode não caber. NÃO commitar, NÃO dar push.
Arquivos do repo são LF, UTF-8 — preserve (use a ferramenta Edit; nada de `sed -i`).
