# VACINA — virtual-analógico polifônico

Sintetizador (VSTi / AU) virtual-analógico de 16 vozes, no espírito dos VA de mesa dos anos 2000, mas **leve no
processador**: a modulação (envelopes, LFOs, matriz, glide, corte) roda em blocos de controle de 32 amostras e só
osciladores, filtros e ganho rodam por amostra, com coeficientes e ganho interpolados dentro do bloco. Medido no
harness local: 16 vozes tocando com os dois filtros e efeitos ligados ≈ 5% de um núcleo (CLASSIC) e ≈ 6,5% (SUPER).
Visual: painel grafite com laterais de madeira, knobs pretos de borracha, teclas cinza com LED vermelho e visor de
cristal líquido. Instalador próprio (`InsideStudioSP-Vacina-win.exe` / `-mac.dmg`).

## Painel

| Seção | O que tem |
|---|---|
| MASTER | MASTER VOLUME |
| MATRIX | 3 slots: SOURCE (mod wheel, aftertouch, velocity, key, LFO 1/2, filt env, amp env, random) → DESTINATION (cutoff, cutoff 2, resonance, pitch, shape 1/2, pulse width, FM, osc balance, super spread, noise, pan, volume, filter balance) e AMOUNT (±) |
| ARP | ARP ON, HOLD, MODE (UP, DOWN, UP/DOWN, RANDOM, AS PLAYED, CHORD), OCTAVES (1–4), RATE (1/32 a 1/4), GATE. Anda no tempo do host tocando; parado, num relógio próprio no BPM do projeto |
| MODULATORS | LFO 1 e LFO 2 (SELECT troca a página): RATE, SHAPE (sine, tri, saw, square, S&H, S&G), CLOCK (livre ou 1/16 a 4 compassos, preso ao host). LFO 1 → OSC PITCH, CUTOFF, PULSE W, AMP; LFO 2 → OSC SHAPE, FM, CUTOFF, PAN. LED pisca no ritmo do LFO da página |
| OSCILLATORS | OSC 1 e OSC 2 (SELECT): SHAPE, PULSE WIDTH, SEMITONE; OSC 1 tem MODEL CLASSIC/SUPER e SPREAD (SUPER = 7 serras), OSC 2 tem DETUNE e SYNC. KEY MODE (POLY, MONO, LEGATO), PORTAMENTO, FM AMOUNT (OSC 1 → OSC 2) |
| MIX | OSC BALANCE (no meio = os dois cheios), SUB OSC (quadrada uma oitava abaixo), OSC VOLUME (até a metade é volume; acima satura antes do filtro), NOISE VOLUME |
| FILTERS | CUTOFF, CUTOFF 2 (offset em semitons), BALANCE (filtro 1 ↔ 2); FILT 1 / FILT 2 (SELECT): RESONANCE, ENV AMOUNT (±), KEY FOLLOW e MODE (filtro 1: LP 24 ladder, LP 12, HP, BP, BS; filtro 2: LP, HP, BP, BS); ROUTING série ou paralelo |
| FILTER / AMPLIFIER ENVELOPE | ATTACK, DECAY, SUSTAIN, RELEASE e VELOCITY (quanto a força da nota mexe) |
| EFFECTS | fileira de cima (SELECT): DELAY (SEND, TIME no tempo, FEEDBACK; pingue-pongue escurecendo), REVERB (SEND, SIZE, DAMPING), EQ (LOW 150 Hz, MID 1,2 kHz, HIGH 6 kHz). Fileira de baixo: DISTORTION (TYPE soft/hard/fuzz/rect, INTENSITY), ANALOG BOOST (grave encorpado: INTENSITY, TUNE), CHORUS e PHASER (MIX, INTENSITY) |
| VALUE 1–3 | UNISON (1–4 vozes por nota), DETUNE e PAN SPREAD do unison |

O **visor** mostra o grupo do preset, o nome e, por 2,5 s, o último parâmetro mexido com o valor (fora isso, vozes
tocando e BPM). Clique no visor ou em BROWSE abre o menu de presets; PREV / NEXT andam na lista; STORE salva.
Nas caixinhas de cristal (matriz, octaves, clock): clique abre a lista, arraste na vertical ou roda do mouse muda.

MIDI: pitch bend ±2 st, mod wheel e aftertouch (fontes da matriz; o slot 1 vem com MOD WHEEL → CUTOFF), sustain (CC 64).

**Presets:** banco FACTORY 1 com 360 (40 por categoria: BASS, LEAD, PAD, PLUCK, ARP, KEYS, SUPERSAW, ATMOSFERA e FX)
mais o Init. Igual à QUIMERA e ao LIMÃO:
- **USER:** "Salvar como novo preset…" (ou STORE) grava o som; os do usuário dá para salvar por cima, renomear e
  apagar (vai para a lixeira). Pasta: `%APPDATA%\InsideStudioSP\VACINA\Presets` (Windows),
  `~/Library/Audio/Presets/InsideStudioSP/VACINA` (Mac).
- **Bancos avulsos (`.vcnbank`):** "Importar banco de presets…" copia o arquivo para a pasta `Banks` e o banco aparece
  embaixo do FACTORY 1, com as categorias; "Remover banco" manda para a lixeira. Formato: XML `VACINABANK` com
  `PRESET name category` → `PARAM id value`. `tools/montar-banco.py` monta um banco a partir de pastas por categoria
  com os .xml do USER.
- O banco de fábrica é GERADO: `python tools/gerar-presets.py` reescreve `Source/presets/fabrica.inc` (receitas por
  categoria com semente fixa; os 12 feitos à mão entram primeiro).

O menu de presets (e as listas das caixinhas) abre **dentro** da janela do plugin: como janela solta, a camada de
dentro (banco → categoria → preset) podia ficar por baixo da janela do plugin, que o host deixa sempre na frente.
No mesmo menu fica o tamanho da janela (75–200%, salvo no projeto).

**Sem estalo:** os knobs contínuos da voz andam em rampa (~12 ms), envelopes e corte são interpolados dentro do bloco
de controle, voz roubada sobe do nível em que estava, o filtro zera quando a nota vem do silêncio, o ANALOG BOOST
liga com o ganho subindo junto, o phaser interpola o coeficiente e a linha do chorus grava mesmo desligada.

## Fontes

| Arquivo | O que é |
|---|---|
| `Params.h` | ids dos parâmetros e as listas |
| `Engine.h` | voz (osciladores polyBLEP, SUPER, sub, noise, ladder ZDF e SVF, envelopes), LFOs, arpejador, alocação de vozes / unison / glide e efeitos |
| `Presets.h`, `presets/fabrica.inc` | categorias e presets de fábrica (o .inc é gerado) |
| `tools/` | `gerar-presets.py` (banco de fábrica) e `montar-banco.py` (.vcnbank a partir do USER) |
| `PluginProcessor.*` | MIDI, relógio do host, arpejador, estado, presets (fábrica, bancos, USER), último parâmetro mexido |
| `PluginEditor.*`, `VacinaUi.h` | tela (painel grafite, madeira, knob de borracha, teclas com LED, listas de LEDs, visor de cristal, páginas SELECT, menu de presets) |
| `Theme.h`, `Widgets.h` | os da casa, iguais aos dos outros plugins |
