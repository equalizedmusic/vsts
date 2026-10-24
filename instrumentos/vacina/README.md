# VACINA — virtual-analógico polifônico

Sintetizador (VSTi / AU) virtual-analógico de 16 vozes, no espírito dos VA de mesa dos anos 2000, mas **leve no
processador**: a modulação (envelopes, LFOs, matriz, glide, corte) roda em blocos de controle de 32 amostras e só
osciladores, filtros e ganho rodam por amostra, com coeficientes e ganhos interpolados dentro do bloco. Medido no
harness local: 16 vozes no pior caso (SUPER + WAVE + OSC 3 + COMB nos dois filtros + saturação + efeitos) ≈ 8,6% de um
núcleo; o Init com 16 vozes ≈ 3,9%.
Visual: painel grafite com laterais de madeira; a chapa (pintura eletrostática), os knobs de borracha, as teclas e os
parafusos são **imagens renderizadas** (`tools/gerar-texturas.py`: relevo iluminado pixel a pixel, com sombra), e os
visores são OLED com o traço brilhando. Tudo à vista numa tela só (sem páginas). Instalador próprio
(`InsideStudioSP-Vacina-win.exe` / `-mac.dmg`).

## Painel

| Seção | O que tem |
|---|---|
| Cabeçalho | visor do preset (banco · categoria, posição, nome e o último parâmetro mexido), BROWSE / STORE / PREV / NEXT, MIDI e SYNC |
| VOICE | KEY MODE (POLY, MONO, LEGATO), GLIDE, BEND (1–24 st), UNISON (1–4), DETUNE e SPREAD do unison, ANALOG (deriva: cada voz desafina e muda o corte devagar, como componentes de verdade) e MASTER |
| OSC 1 | visor com a forma de onda; MODEL: CLASSIC (SHAPE seno → triângulo → serra → pulso), SUPER (7 serras, SUPER SPREAD) ou WAVE (24 tabelas — o SHAPE anda entre os 32 quadros); PULSE W, SEMITONE |
| OSC 2 | visor; MODEL CLASSIC ou WAVE, WAVETABLE, SHAPE, PULSE W, SEMITONE, DETUNE, SYNC (recomeça com o OSC 1) e FM 1 > 2 |
| OSC 3 | visor; CLASSIC com SHAPE, SEMITONE e DETUNE |
| MIXER | OSC 1, OSC 2, OSC 3, SUB (quadrada uma oitava abaixo do OSC 1), NOISE e NOISE COLOR (escuro ↔ claro), RING MOD (OSC 1 × OSC 2) e DRIVE (até a metade é volume; acima satura antes do filtro) |
| FILTERS | visor com a curva dos dois filtros como estão roteados; FILTER 1: LP 24 (ladder), LP 12, HP, BP, BS, COMB (pente afinado no corte; com KEY TRACK 100% toca a nota — corda, tubo) e VOWEL (formantes U-O-A-E-I andando com o corte); CUTOFF, RESONANCE, ENV AMOUNT (±), KEY TRACK. FILTER 2: LP, HP, BP, BS, COMB, VOWEL; CUTOFF 2 (offset em semitons) e os mesmos três. ROUTING série/paralelo, BALANCE, SATURATION entre os filtros (LIGHT, TUBE, HARD, FOLD, BITS, RATE) e SAT DRIVE |
| ENVELOPES | FILTER ENV e AMP ENV, cada um com visor: ATTACK, DECAY, SUSTAIN, RELEASE, SLOPE (o sustain sobe ou cai devagar) e VELOCITY |
| LFO 1 / 2 / 3 | visor da forma, RATE, SHAPE (sine, tri, saw, square, S&H, S&G), CLOCK (livre ou 1/16 a 4 compassos, preso ao host) e LED no ritmo. LFO 1 → PITCH, CUTOFF, PULSE W, AMP; LFO 2 → SHAPE, FM, CUTOFF, PAN; LFO 3 → um DESTINATION qualquer com AMOUNT |
| MOD MATRIX | 6 slots: SOURCE (mod wheel, aftertouch, velocity, key, LFO 1/2/3, filt env, amp env, random) → DESTINATION (cutoff, cutoff 2, resonance, pitch, osc 2 pitch, shape 1/2/3, pulse width, FM, níveis do mixer, ring, super spread, saturation, filter balance, pan, volume) e AMOUNT (±) |
| ARP | ARP ON, HOLD, MODE (UP, DOWN, UP/DOWN, RANDOM, AS PLAYED, CHORD), OCTAVES (1–4), RATE (1/32 a 1/4), GATE. Anda no tempo do host tocando; parado, num relógio próprio no BPM do projeto |
| EFFECTS | DISTORTION (TYPE soft/hard/fuzz/rect, AMOUNT), ANALOG BOOST (AMOUNT, TUNE), CHORUS e PHASER (MIX, DEPTH), DELAY (SEND, TIME no tempo, FEEDBACK; pingue-pongue escurecendo), REVERB (SEND, SIZE, DAMPING), EQ (LOW 150 Hz, MID 1,2 kHz, HIGH 6 kHz) |

Com o mouse em cima de um knob (ou girando), o nome embaixo vira o valor; o visor do preset também mostra o último
parâmetro mexido por 2,5 s. Nos campos de visor (tabela, matriz, clock…): clique abre a lista, arraste na vertical ou
roda do mouse muda. Duplo clique no knob volta ao padrão.

MIDI: pitch bend (BEND), mod wheel e aftertouch (fontes da matriz; o slot 1 vem com MOD WHEEL → CUTOFF), sustain (CC 64).

**Presets:** banco FACTORY 1 com 360 (40 por categoria: BASS, LEAD, PAD, PLUCK, ARP, KEYS, SUPERSAW, ATMOSFERA e FX)
mais o Init. Cada categoria tem **8 arquétipos de família diferente** (no BASS: reese, ácido, sub, FM, funk, growl de
wavetable, sync e vogal; no PLUCK: corda no COMB, serra, FM, chip, wavetable, supersaw, vogal e percussão…) com 5
variações cada, e nomes escritos à mão para o som. O volume de todos é **nivelado pela medida** (loudness de curto prazo
num harness que toca cada preset): trocar de preset não dá pulo de volume.
- **USER:** "Salvar como novo preset…" (ou STORE) grava o som; os do usuário dá para salvar por cima, renomear e
  apagar (vai para a lixeira). Pasta: `%APPDATA%\InsideStudioSP\VACINA\Presets` (Windows),
  `~/Library/Audio/Presets/InsideStudioSP/VACINA` (Mac).
- **Bancos avulsos (`.vcnbank`):** "Importar banco de presets…" copia o arquivo para a pasta `Banks` e o banco aparece
  embaixo do FACTORY 1, com as categorias; "Remover banco" manda para a lixeira. Formato: XML `VACINABANK` com
  `PRESET name category` → `PARAM id value`. `tools/montar-banco.py` monta um banco a partir de pastas por categoria
  com os .xml do USER.
- O banco de fábrica é GERADO: `python tools/gerar-presets.py` reescreve `Source/presets/fabrica.inc` a partir dos
  arquétipos; `tools/niveis.txt` (nome;dB) é o nivelamento medido — mudou um arquétipo, mede de novo
  (`gerar-presets.py --lista X.txt` gera a lista completa para o harness).

O menu de presets (e as listas dos campos) abre **dentro** da janela do plugin: como janela solta, a camada de
dentro (banco → categoria → preset) podia ficar por baixo da janela do plugin, que o host deixa sempre na frente.
No mesmo menu ficam o tamanho da janela (60–180%, salvo no projeto; começa em 85%) e a versão (`novidades/Vacina.md`).

**Letras:** as listas de LEDs (modo dos filtros, MODEL, modos do ARP, KEY MODE, ROUTING) usam 10,5 pt em passos de
15 px; os nomes dos knobs ficam em 9,5 pt, os campos de visor em 12 pt e as legendas em 9 pt. Não descer disso: a
janela abre em 85% e abaixo disso as letras ficavam ilegíveis.

**Sem estalo:** os knobs contínuos da voz andam em rampa (~12 ms), níveis do mixer, corte, atraso do COMB e ganho são
interpolados dentro do bloco de controle, voz roubada sobe do nível em que estava, os filtros zeram quando a nota vem
do silêncio, o ANALOG BOOST liga com o ganho subindo junto, o phaser interpola o coeficiente e a linha do chorus grava
mesmo desligada.

## Fontes

| Arquivo | O que é |
|---|---|
| `Params.h` | ids dos parâmetros e as listas |
| `Engine.h` | voz (CLASSIC polyBLEP, SUPER, WAVE, OSC 3, sub, ring, noise com cor, ladder ZDF, SVF, COMB, VOWEL, saturação, envelopes com SLOPE, deriva analógica), LFOs, arpejador, alocação de vozes / unison / glide e efeitos |
| `WaveBank.h` | as 24 tabelas de onda (mesmas fórmulas da QUIMERA, FFT própria), com mips por oitava contra aliasing |
| `Presets.h`, `presets/fabrica.inc` | categorias e presets de fábrica (o .inc é gerado) |
| `tools/` | `gerar-presets.py` + `niveis.txt` (banco de fábrica), `montar-banco.py` (.vcnbank a partir do USER), `gerar-texturas.py` (imagens de `Source/img`) |
| `Source/img/` | chapa do painel (2×), knob, tecla e parafuso renderizados — entram no plugin como `PluginImg.h` |
| `PluginProcessor.*` | MIDI, relógio do host, arpejador, estado, presets (fábrica, bancos, USER), último parâmetro mexido |
| `PluginEditor.*`, `VacinaUi.h` | tela (chapa, madeira, knobs e teclas renderizados, LEDs, visores OLED de onda/filtro/envelope/LFO, menu de presets) |
| `Theme.h`, `Widgets.h` | os da casa, iguais aos dos outros plugins |
