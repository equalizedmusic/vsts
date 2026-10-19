# QUIMERA — sintetizador

Sintetizador polifônico (VSTi / AU) que junta o lado metálico e digital das wavetables com o lado orgânico
de um sintetizador analógico mono clássico: deriva de afinação por voz, filtro ladder que auto-oscila, glide e legato.
Todas as tabelas são geradas por código (nada copiado de outro synth). Suporta **MPE**.

## Blocos

| Aba | Bloco | O que tem |
|---|---|---|
| SÍNTESE | OSC 1–3 | 24 tabelas de 32 quadros: **Analógico** (Clássica, Pulso, Serra Dupla, Triângulo Dobrado), **Modelado** (Metal, Formante, Sincronizado, Dobra, Elétrico, Quadrada-Serra, Seno-Quadrada, Escova) e **Wavetable** (Cristal, Digital, Vocal, Harmônicos, Granito, Sinos, FM Linear, FM Metálico, Espectral, Órgão, Corda, Quimera). Posição, 11 warps (Sync, Dobra ±, PWM, Espelho, Dobrar, Quantizar, FM e RM do oscilador anterior, Assimetria), oitava/semitom/fino, uníssono até 8× com detune e abertura, fase aleatória, volume, pan e destino (filtro A, B, A+B ou direto) |
| | SUB + RUÍDO | Sub de 1 ou 2 oitavas (seno, triângulo, quadrada); ruído com cor |
| | FILTRO A / B | Ladder 24 e 12, PB/PA/PF 12, Rejeita, Pente e Formante; corte, ressonância, drive, key track, Env 2 e mix; em série ou paralelo |
| | ENV 1–3 | ADSR com curva. Env 1 = volume; Env 2 = filtros (e matriz); Env 3 = só matriz |
| | LFO 1–2 | 7 formas, sync no grid ou Hz, modo Livre / Por nota / Envelope, subida |
| | VOZ | Poli / Mono / Legato, até 16 vozes, glide, bend, deriva analógica, velocidade no volume |
| MATRIZ | 16 slots | Fontes: envelopes, LFOs, velocidade, nota, mod wheel, **pressão**, **slide**, bend, aleatório, 4 macros. Destinos: altura/posição/warp/volume/pan de cada osc, corte/ressonância/drive dos filtros, volume, pan, velocidade dos LFOs, detune |
| EFEITOS | rack | HIPER → DISTORÇÃO → PHASER → CHORUS → DELAY (sync, ping-pong) → REVERB (sala, placa, hall) → EQ → COMPRESSOR (simples ou multibanda) |

**MPE:** com o botão MPE ligado, cada nota tem o próprio bend (até 96 st), pressão e slide (CC74), que entram na matriz.
Sem MPE, o aftertouch vira Pressão. Trocar de preset não mexe em MPE, MPE Bend e SEGURAR.

**Presets:** Init e 20 por categoria — BAIXO, LEAD, PLUCK, PAD, ATMOS, PERC, BATERIA, TECLAS e FX (181).
Presets do usuário com Salvar como / Salvar alterações / Renomear / Apagar, como nos efeitos da casa.

Janela de 75% a 200% (base 1200×740).

## Build

Projeto CMake próprio (`instrumentos/CMakeLists.txt`) e workflow `build-instrumentos.yml`: zips `QUIMERA-mac.zip` e
`QUIMERA-win.zip` no release `latest-instrumentos` e no branch `builds-instrumentos`.
