# QUIMERA — sintetizador

Sintetizador polifônico (VSTi / AU) que junta o lado metálico e digital das wavetables com o lado orgânico
de um sintetizador analógico mono clássico: deriva de afinação por voz, filtro ladder que auto-oscila, glide e legato.
Todas as tabelas são geradas por código (nada copiado de outro synth). Suporta **MPE**.

## Blocos

| Aba | Bloco | O que tem |
|---|---|---|
| SYNTH | OSC 1–3 | 24 tabelas de 32 quadros: **Analógico** (Clássica, Pulso, Serra Dupla, Triângulo Dobrado), **Modelado** (Metal, Formante, Sincronizado, Dobra, Elétrico, Quadrada-Serra, Seno-Quadrada, Escova) e **Wavetable** (Cristal, Digital, Vocal, Harmônicos, Granito, Sinos, FM Linear, FM Metálico, Espectral, Órgão, Corda, Quimera). POSITION, 11 warps (Sync, Bend ±, PWM, Mirror, Fold, Quantize, FM e RM do oscilador anterior, Asym), OCTAVE/SEMI/FINE, UNISON até 8× com DETUNE e WIDTH, fase aleatória (Random Phase), LEVEL, PAN e OUTPUT (Filter A, Filter B, A + B ou Direct) |
| | SUB + NOISE | Sub de 1 ou 2 oitavas (Sine, Triangle, Square); noise com cor |
| | FILTER A / B | Ladder 24 e 12, LP/HP/BP 12, Notch, Comb e Formant; CUTOFF, RES, DRIVE, KEY, ENV 2 e DRY/WET; em SERIAL A → B ou PARALLEL |
| | ENV 1–3 | ADSR com curva. Env 1 = volume; Env 2 = filtros (e MOD MATRIX); Env 3 = só MOD MATRIX |
| | LFO 1–2 | 7 formas, sync no grid ou Hz, modo Free / Retrig / Envelope, RISE |
| | VOICE | Poly / Mono / Legato, até 16 VOICES, GLIDE, BEND, DRIFT (deriva analógica), VELOCITY no volume |
| MOD MATRIX | 16 slots | SOURCE: envelopes, LFOs, Velocity, Note, Mod Wheel, **Pressure**, **Slide**, Bend, Random, 4 macros. DEST: Pitch/Position/Warp/Level/Pan de cada osc, Cutoff/Res/Drive dos filtros, Volume, Pan, Rate dos LFOs, Unison Detune |
| FX | rack | HYPER → DISTORTION → PHASER → CHORUS → DELAY (sync, ping-pong) → REVERB (room, plate, hall) → EQ → COMPRESSOR (single ou multiband) |

**MPE:** com o botão MPE ligado, cada nota tem o próprio bend (até 96 st), pressure e slide (CC74), que entram na MOD MATRIX.
Sem MPE, o aftertouch vira Pressure. Trocar de preset não mexe em MPE, MPE Bend e SEGURAR.

**Presets:** Init e 20 por categoria — BASS, LEAD, PLUCK, PAD, ATMOS, PERC, DRUMS, KEYS e FX (181).
Presets do usuário com Salvar como / Salvar alterações / Renomear / Apagar, como nos efeitos da casa.

Janela de 75% a 200% (base 1200×740).

## Build

Projeto CMake próprio (`instrumentos/CMakeLists.txt`) e workflow `build-instrumentos.yml`: zips `QUIMERA-mac.zip` e
`QUIMERA-win.zip` no release `latest-instrumentos` e no branch `builds-instrumentos`.
