# RESENHA — call & response FM

![RESENHA](https://raw.githubusercontent.com/equalizedmusic/vsts/shots/RESENHA.png)

Sintetizador (VSTi / AU) de call & response: 4 timbres FM (A, B, C, D) conversam numa frase gerada na escala, um
pergunta e o outro responde. Toca no tempo do DAW. Instalador próprio (`InsideStudioSP-Resenha-win.exe` / `-mac.dmg`).

## Timbres

Cada timbre é um FM de 2 operadores (moduladora com realimentação → portadora com forma de onda) com filtro e
panorâmica próprios. Controles por timbre: WAVE (SINE, TRI, SAW, SQUARE), RATIO (0.50 a 13.0), FM, FEEDBACK, FM DEC,
FILTER, CUTOFF, RESO, F.ENV, UNISON, MOVE, RATE, DECAY, P.ENV, GLIDE, BEND e PAN. Um envelope só (no tempo do FM DEC) move
no ataque o índice de FM, o pitch e o filtro. **UNISON** soma 2 cópias desafinadas (até ±25 cents), uma em cada lado do
estéreo. **FILTER**: LP, BP ou **VOWEL** — dois formantes, o CUTOFF escolhe a vogal (u-o-a-e-i) e o F.ENV abre a "boca" no
ataque: o lead fala. **MOVE** é um LFO no tempo do DAW (RATE de 1/16 a 2 compassos) que mexe no FM e no filtro/vogal. **BEND** (−24 a +24 semitons) é o mergulho do lead: a última nota de cada fala do timbre desliza
durante a nota inteira, começando devagar e acelerando, e a tela desenha a cauda dela. Cada timbre é monofônico; as
caudas de timbres diferentes se sobrepõem (8 vozes no total).

## Gerador de frase

SCALE e ROOT, tamanho de 1, 2 ou 4 compassos, RATE (1/32 a 1/8, com tercinas), OITAVAS, DENSIDADE, GATE, VAR. GATE
e SWING (no máximo 64 passos). CONTORNO DAS NOTAS: **MOTIVO** repete um desenho curto com variações; **CORRIDA** sobe em
escada (de grau em grau ou em arpejo) e recomeça embaixo; **QUEDA** desce; **ONDA** sobe e volta; **PEDAL** alterna uma
nota fixa (a tônica) com uma linha que anda. O RITMO sorteado é um riff que se repete (células por compasso ou "rolando"
3+3+3+3+4), com virada a cada 2 compassos. Os dados NOTAS, RITMO, GATE e TIMBRES sorteiam só aquela parte; o cadeado faz o
GERAR CONVERSA pular a parte (TIMBRES respeita o cadeado do cartão de cada timbre).

## Conversa

VOZES NA CONVERSA (1 a 4) diz quantos timbres entram; TROCA A CADA 1 a 8 notas — ou ½ / 1 compasso (½C, 1C) — passa a
vez para o próximo, na ORDEM A→B→C,
PING-PONG ou SORTEIO. Turnos pares perguntam (tocam como está escrito) e os ímpares respondem: **LIVRE** toca o que está
escrito, **ECO** repete a pergunta, **VARIA** repete mudando algumas notas e **INVERTE** espelha (o que subia desce).
ALTURA DA RESPOSTA: mesma, +oitava, +5ª ou −oitava. Fora do LIVRE, a resposta fecha na tônica (a pergunta fica no ar e
a resposta conclui). A tela mostra quem fala cada nota.

## Tocar

**COM NOTA**: a frase só toca enquanto há nota segurada, transposta por ela (Dó 3 = como está escrita). **SEMPRE**:
toca com o play do DAW, e a nota só transpõe. ARRASTAR MIDI leva a frase para o DAW como `.mid`, um canal por timbre
(A = 1 … D = 4).

## Efeitos

Drive (TAPE, TUBE, FOLD) com TONE, delay ping-pong no tempo (1/16 a 3/8) e reverb, o volume de saída e o **DUCK**: o
som abaixa a cada tempo e volta em meio tempo, como o sidechain do bumbo.

## Presets

120 de fábrica (12 por categoria: PSY CLÁSSICO, FULL ON, DARK, GOA, PROG, HI-TECH, MINIMAL, FX, CONVERSA FM e LEAD FALANTE), cada um com os 4
timbres, o gerador, a troca de timbre, os efeitos e a frase. CONVERSA FM saiu de trechos de full on a 144 BPM: leads FM
cheios de harmônico, escadas de 1/16 que fecham a fala num mergulho, resposta em outra oitava e DUCK. LEAD FALANTE põe o
VOWEL na frente, com MOVE e conversa por compasso. O plugin abre no "Boca Aberta". Presets do usuário e bancos avulsos `.rsnbank` para importar.

## Código

- `Source/Params.h`: ids dos parâmetros, listas e a frase que a tela escreve e o áudio lê.
- `Source/Phrase.h`: da frase escrita à conversa que toca (quem fala cada nota e qual nota a resposta faz).
- `Source/Engine.h`: vozes FM, filtro e efeitos.
- `Source/Presets.h` e `Source/presets/fabrica.inc` (gerado por `tools/gerar-presets.py`).
