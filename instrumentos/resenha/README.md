# RESENHA — call & response FM

![RESENHA](https://raw.githubusercontent.com/equalizedmusic/vsts/shots/RESENHA.png)

Sintetizador (VSTi / AU) de call & response: 4 timbres FM (A, B, C, D) conversam numa frase gerada na escala, um
pergunta e o outro responde. Toca no tempo do DAW. Instalador próprio (`InsideStudioSP-Resenha-win.exe` / `-mac.dmg`).

## Timbres

Cada timbre é um FM de 2 operadores (moduladora com realimentação → portadora com forma de onda) com filtro
passa-baixas e panorâmica próprios. Controles por timbre: WAVE (SINE, TRI, SAW, SQUARE), RATIO (0.50 a 13.0), FM, FB,
FM DEC, CUT, RES, DEC, PAN, GLIDE, P.ENV e F.ENV. Um envelope só (no tempo do FM DEC) move no ataque o índice de FM, o
pitch e o filtro. Cada timbre é monofônico; as caudas de timbres diferentes se sobrepõem (8 vozes no total).

## Gerador de frase

SCALE e ROOT, tamanho de 1, 2 ou 4 compassos, RATE (1/32 a 1/8, com tercinas), OITAVAS, DENSIDADE, GATE, VAR. GATE
e SWING (no máximo 64 passos). Os dados NOTAS, RITMO, GATE e TIMBRES sorteiam só aquela parte; o cadeado faz o
GERAR CONVERSA pular a parte (TIMBRES respeita o cadeado do cartão de cada timbre).

## Conversa

VOZES NA CONVERSA (1 a 4) diz quantos timbres entram; TROCA A CADA 1 a 8 notas passa a vez para o próximo, na ORDEM A→B→C,
PING-PONG ou SORTEIO. Turnos pares perguntam (tocam como está escrito) e os ímpares respondem: **LIVRE** toca o que está
escrito, **ECO** repete a pergunta, **VARIA** repete mudando algumas notas e **INVERTE** espelha (o que subia desce).
ALTURA DA RESPOSTA: mesma, +oitava, +5ª ou −oitava. A tela mostra quem fala cada nota.

## Tocar

**COM NOTA**: a frase só toca enquanto há nota segurada, transposta por ela (Dó 3 = como está escrita). **SEMPRE**:
toca com o play do DAW, e a nota só transpõe. ARRASTAR MIDI leva a frase para o DAW como `.mid`, um canal por timbre
(A = 1 … D = 4).

## Efeitos

Drive (TAPE, TUBE, FOLD) com TONE, delay ping-pong no tempo (1/16 a 3/8) e reverb, e o volume de saída.

## Presets

96 de fábrica (12 por categoria: PSY CLÁSSICO, FULL ON, DARK, GOA, PROG, HI-TECH, MINIMAL, FX), cada um com os 4
timbres, o gerador, a troca de timbre, os efeitos e a frase. Presets do usuário e bancos avulsos `.rsnbank` para importar.

## Código

- `Source/Params.h`: ids dos parâmetros, listas e a frase que a tela escreve e o áudio lê.
- `Source/Phrase.h`: da frase escrita à conversa que toca (quem fala cada nota e qual nota a resposta faz).
- `Source/Engine.h`: vozes FM, filtro e efeitos.
- `Source/Presets.h` e `Source/presets/fabrica.inc` (gerado por `tools/gerar-presets.py`).
