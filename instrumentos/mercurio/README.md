# MERCÚRIO — FM de 4 operadores, polifônico

Sintetizador (VSTi / AU) FM de 4 operadores e 16 vozes, no espírito das FM de mesa dos anos 80 (TX81Z, DX), puxado para
o **plástico, o metal e o psicodélico**: razões inarmônicas, realimentação, ring e dobra de onda. Leve no processador:
a modulação (envelopes, LFOs, matriz, glide, corte) roda em blocos de controle de 32 amostras e só operadores, filtros
e ganho rodam por amostra, com os níveis e coeficientes interpolados dentro do bloco. Medido no harness local: 16 vozes
no pior caso (4 operadores + realimentação + fold + ring + COMB nos dois filtros + saturação + efeitos) ≈ 14% de um
núcleo; o Init com 16 vozes ≈ 6%.
Visual de aparelho metálico: chapa de alumínio escovado entre laterais de alumínio fresado (aletas de dissipador),
faixas de alumínio anodizado preto em cima (presets, VOICE) e embaixo (EFFECTS) com filete iridescente, knobs de alumínio
usinado com tampa preta e ponteiro aceso, LEDs magenta e visores OLED ciano. Cada operador tem a sua cor (ponteiro do
LEVEL, onda no visor, caixa no desenho do algoritmo e plaqueta no canto da seção). A chapa, os knobs, as teclas e os
parafusos são **imagens renderizadas** (`tools/gerar-texturas.py`: relevo iluminado pixel a pixel, com sombra). Tudo à vista numa tela só (sem páginas). Instalador próprio
(`InsideStudioSP-Mercurio-win.exe` / `-mac.dmg`).

## Painel

| Seção | O que tem |
|---|---|
| Cabeçalho | visor do preset (banco · categoria, posição, nome e o último parâmetro mexido), BROWSE / STORE / PREV / NEXT, MIDI e SYNC |
| VOICE | KEY MODE (POLY, MONO, LEGATO), GLIDE, BEND (1–24 st), UNISON (1–4), DETUNE e SPREAD do unison, DRIFT (cada voz desafina um tiquinho, devagar) e MASTER |
| FM | visor do ALGORITHM (as caixas dos operadores ligadas: cheia = portador, que soa; contorno = modulador; o laço do OP 4 é a realimentação), a lista dos 8 algoritmos — 1 STACK 4>3>2>1, 2 Y (3+4)>2>1, 3 BRANCH 3>2>1 e 4>1, 4 FORK 4>3>1 e 2>1, 5 TWIN 2>1 e 4>3, 6 FAN 4>1,2,3, 7 TRIPLE 4>3 e 1, 2 soltos, 8 ORGAN os 4 soltos —, FEEDBACK (OP 4 em si mesmo: de seno a serra a ruído), FOLD (dobra de onda na saída), RING MOD (os portadores vezes a onda do OP 2), NOISE e DRIVE (até a metade é volume; acima satura antes do filtro) |
| OPERATOR 1–4 | visor com a onda, a razão (×) e o papel (OUT / MOD); WAVE (SINE, HALF SINE, ABS SINE, PULSE SINE, ALT SINE, CAMEL, SQUARE, SAW); LEVEL; RATIO (×0,5, ×1…×16) e FINE (+0…100 %: razão = RATIO × (1 + FINE) — é aqui que nasce o metal: ×1,41, ×3,5, ×2,76…); DETUNE (±50 ct); VELOCITY; envelope próprio ATTACK, DECAY (até 20 s), SUSTAIN, RELEASE. Nível 100 % de um modulador = índice ~7,9 rad |
| FILTERS | visor com a curva dos dois filtros como estão roteados; FILTER 1: LP 24 (ladder), LP 12, HP, BP, BS, COMB (pente afinado no corte; com KEY TRACK 100% toca a nota — corda, tubo) e VOWEL (formantes U-O-A-E-I andando com o corte); CUTOFF, RESONANCE, ENV AMOUNT (±), KEY TRACK. FILTER 2: LP, HP, BP, BS, COMB, VOWEL; CUTOFF 2 (offset em semitons) e os mesmos três. ROUTING série/paralelo, BALANCE, SATURATION entre os filtros (LIGHT, TUBE, HARD, FOLD, BITS, RATE) e SAT DRIVE |
| ENVELOPES | FILTER ENV e AMP ENV, cada um com visor: ATTACK, DECAY, SUSTAIN, RELEASE, SLOPE (o sustain sobe ou cai devagar) e VELOCITY |
| LFO 1 / 2 / 3 | visor da forma, RATE, SHAPE (sine, tri, saw, square, S&H, S&G), CLOCK (livre ou 1/16 a 4 compassos, preso ao host) e LED no ritmo. LFO 1 → PITCH, CUTOFF, RATIO (a razão dos moduladores tremendo: o efeito “psicodélico”), AMP; LFO 2 → FM INDEX, FEEDBACK, CUTOFF, PAN; LFO 3 → um DESTINATION qualquer com AMOUNT |
| MOD MATRIX | 6 slots: SOURCE (mod wheel, aftertouch, velocity, key, LFO 1/2/3, filt env, amp env, random) → DESTINATION (cutoff, cutoff 2, resonance, pitch, FM INDEX — o nível de todos os moduladores —, OP 1–4 LEVEL, OP 2–4 RATIO (±2 oitavas), feedback, fold, ring, noise, saturation, filter balance, pan, volume) e AMOUNT (±) |
| ARP | ARP ON, HOLD, MODE (UP, DOWN, UP/DOWN, RANDOM, AS PLAYED, CHORD), OCTAVES (1–4), RATE (1/32 a 1/4), GATE. Anda no tempo do host tocando; parado, num relógio próprio no BPM do projeto |
| EFFECTS | DISTORTION (TYPE soft/hard/fuzz/rect, AMOUNT), ANALOG BOOST (AMOUNT, TUNE), CHORUS e PHASER (MIX, DEPTH), DELAY (SEND, TIME no tempo, FEEDBACK; pingue-pongue escurecendo), REVERB (SEND, SIZE, DAMPING), EQ (LOW 150 Hz, MID 1,2 kHz, HIGH 6 kHz) |

Com o mouse em cima de um knob (ou girando), o nome embaixo vira o valor; o visor do preset também mostra o último
parâmetro mexido por 2,5 s. Nos campos de visor (tabela, matriz, clock…): clique abre a lista, arraste na vertical ou
roda do mouse muda. Duplo clique no knob volta ao padrão.

MIDI: pitch bend (BEND), mod wheel e aftertouch (fontes da matriz; o slot 1 vem com MOD WHEEL → FM INDEX), sustain (CC 64).

**Presets:** banco FACTORY 1 com 360 (40 por categoria: BASS, LEAD, PAD, PLUCK, BELLS, KEYS, ARP, TEXTURA e FX)
mais o Init (um seno puro). Cada categoria tem **8 arquétipos de família diferente** (nos BELLS: sino tubular,
glockenspiel, gamelão, cristal, igreja, gongo, templo e caixinha de música; nas KEYS: e-piano FM, e-piano escuro, órgão,
clave, cravo, vibrafone, piano de brinquedo e lo-fi…) com 5 variações cada, e nomes escritos à mão para o som. O volume
de todos é **nivelado pela medida** (loudness de curto prazo num harness que toca cada preset, desvio de 0,3 dB):
trocar de preset não dá pulo de volume. Os ARP já vêm com o arpejador ligado.
- **USER:** "Salvar como novo preset…" (ou STORE) grava o som; os do usuário dá para salvar por cima, renomear e
  apagar (vai para a lixeira). Pasta: `%APPDATA%\InsideStudioSP\MERCURIO\Presets` (Windows),
  `~/Library/Audio/Presets/InsideStudioSP/MERCURIO` (Mac).
- **Bancos avulsos (`.mrcbank`):** "Importar banco de presets…" copia o arquivo para a pasta `Banks` e o banco aparece
  embaixo do FACTORY 1, com as categorias; "Remover banco" manda para a lixeira. Formato: XML `MERCURIOBANK` com
  `PRESET name category` → `PARAM id value`. `tools/montar-banco.py` monta um banco a partir de pastas por categoria
  com os .xml do USER.
- O banco de fábrica é GERADO: `python tools/gerar-presets.py` reescreve `Source/presets/fabrica.inc` a partir dos
  arquétipos; `tools/niveis.txt` (nome;dB) é o nivelamento medido — mudou um arquétipo, mede de novo
  (`gerar-presets.py --lista X.txt` gera a lista completa para o harness).

O menu de presets (e as listas dos campos) abre **dentro** da janela do plugin: como janela solta, a camada de
dentro (banco → categoria → preset) podia ficar por baixo da janela do plugin, que o host deixa sempre na frente.
No mesmo menu ficam o tamanho da janela (60–180%, salvo no projeto; começa em 85%) e a versão (`novidades/Mercurio.md`).

**Letras:** as listas de LEDs (modo dos filtros, modos do ARP, KEY MODE, ROUTING) usam 10,5 pt em passos de
15 px; os nomes dos knobs ficam em 9,5 pt, os campos de visor em 12 pt e as legendas em 9 pt. Não descer disso: a
janela abre em 85% e abaixo disso as letras ficavam ilegíveis.

**Sem estalo:** os knobs contínuos da voz andam em rampa (~12 ms), nível de cada operador, ring, fold, noise, corte, atraso do
COMB e ganho são interpolados dentro do bloco de controle; os operadores começam na fase zero (o ataque FM sai
sempre igual), voz roubada sobe do nível em que estava, os filtros zeram quando a nota vem
do silêncio, o ANALOG BOOST liga com o ganho subindo junto, o phaser interpola o coeficiente e a linha do chorus grava
mesmo desligada.

## Fontes

| Arquivo | O que é |
|---|---|
| `Params.h` | ids dos parâmetros, as listas e a tabela dos algoritmos (quem modula quem) |
| `Engine.h` | voz (4 operadores com tabelas das 8 ondas, algoritmos, realimentação, ring, fold, noise, ladder ZDF, SVF, COMB, VOWEL, saturação, envelopes com SLOPE, deriva), LFOs, arpejador, alocação de vozes / unison / glide e efeitos |
| `Presets.h`, `presets/fabrica.inc` | categorias e presets de fábrica (o .inc é gerado) |
| `tools/` | `gerar-presets.py` + `niveis.txt` (banco de fábrica), `montar-banco.py` (.mrcbank a partir do USER), `gerar-texturas.py` (imagens de `Source/img`) |
| `Source/img/` | chapa do painel (2×), knob, tecla e parafuso renderizados — entram no plugin como `PluginImg.h` |
| `PluginProcessor.*` | MIDI, relógio do host, arpejador, estado, presets (fábrica, bancos, USER), último parâmetro mexido |
| `PluginEditor.*`, `MercurioUi.h` | tela (chapa de alumínio, faixas anodizadas, laterais de metal, knobs e teclas renderizados, LEDs, visores OLED de onda/filtro/envelope/LFO, menu de presets) |
| `Theme.h`, `Widgets.h` | os da casa, iguais aos dos outros plugins |
