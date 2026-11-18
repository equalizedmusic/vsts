# CHUTE — novidades

Pacote do instalador: `InsideStudioSP-Chute`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `chute-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.0 — 2026-11-18
- Primeira versão do CHUTE, kick designer. TESTE (sem licença, fora do site).
- Cada nota MIDI toca uma batida: CLICK (o ataque do preset) + BODY (o corpo do kick, afinável).
- 50 presets de fábrica em 4 bancos (SUB, PUNCH, TIGHT, BOOM), com nomes de golpe de luta, e a nota de cada um em
  cifra no menu (C1, D#1, F#1…). Mexeu em qualquer knob, o nome ganha `*`; SAVE AS grava como preset novo no banco
  USER. ◀ ▶ passam pelos presets (fábrica e USER). Duplo clique num knob volta ao valor do preset.
- CLICK: LEVEL, TONE e LENGTH (2 a 25 ms, com crossfade para o corpo), com zoom nos primeiros 20 ms.
- BODY: a nota em três modos — KEY · MIDI (afina na nota que você toca), FIXED (a nota do preset, afinada) e PRESET
  (a afinação original); TUNE (±12 semitons), PITCH (onde a queda começa), DROP (velocidade da queda), DECAY
  (comprimento) e TAIL (quanto sobra no fim).
- DRIVE (WARM, HARD, FOLD) com TONE; MASTER com CHOKE / POLY, VOLUME e VELO.
- Tela: a batida do preset desenhada (ataque laranja, corpo verde, curva de pitch tracejada), PITCH ENV, AMP ENV e
  o medidor de saída.
- LINK: cada batida vai para o ALIGN e o BAREBASS do mesmo projeto, sem sidechain (ALIGN 0.3.0 e BAREBASS 0.4.0).
- Latência zero.
