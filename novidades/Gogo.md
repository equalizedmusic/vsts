# GOGÓ — novidades

Pacote do instalador: `InsideStudioSP-Gogo`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `gogo-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.0 — 2026-11-15
- Primeira versão do GOGÓ, para TESTE (ainda sem ativação por licença e fora do site): cadeia de voz automática.
  Vai no canal da VOZ (uma voz só) e substitui a cadeia de plugins da voz.
- Cadeia: CLEAN → TUNE → RIDE → FET → OPTO → TONE → DE-ESS → WARMTH → SPACE. Cada bloco tem botão de power (liga e
  desliga só aquele bloco, sem estalo) e abre os AJUSTES com um clique. O liga/desliga entra no projeto e no A/B.
- CLEAN: HPF de 24 dB/oitava (AUTO = 0,7 × a nota mais grave cantada), corte do "p" e do "b" na hora (contados na
  tela) e GATE suave entre as frases.
- TUNE: o motor do ONKEY inteiro (acha o tom e a escala sozinho, corrige pelo centro da nota, vibrato e slides
  passam, formantes preservados). Macro TUNE: 0 = desligado, 50 = natural, 100 = robô. Nos ajustes: RETUNE SPEED,
  HUMANIZE, VIBRATO, FORMANT, ROOT / SCALE e o gráfico de afinação.
- RIDE: vocal rider antes da compressão; leva cada trecho cantado a −18 dBFS RMS, no máximo ±RANGE, e para no silêncio.
- FET (pega pico, estilo 1176) e OPTO (nivelador estilo LA-2A, release em dois estágios que fica mais lento quanto
  mais tempo comprimiu), os dois pela macro TAME, com ganho de volta automático (80% da redução média).
- TONE: EQ dinâmica que compara com a média da SUA voz: MUD (300 Hz) e HARSH (3,2 kHz); TONE (escuro / brilhante)
  e AIR (shelf em 11 kHz).
- DE-ESS de banda dividida (FREQ, LISTEN): baixa o "s" e deixa a vogal igual.
- WARMTH: TUBE ou TAPE, com anti-aliasing.
- SPACE: PLATE + DELAY ping-pong no tempo do DAW, os dois com DUCK (abaixam enquanto canta e voltam nas pausas).
- STYLE (PROG TRANCE, PSY TRANCE, POP, TRAP / HIP-HOP, FUNK, PODCAST) grava um ponto de partida nos ajustes;
  começa em PROG TRANCE. ROLE: LEAD, DOUBLE / BACKING, ADLIB / FX, SPOKEN.
- LEARN: cante 15 a 20 s e ele acerta o INPUT para a voz chegar a −18 dBFS RMS.
- KEEP IT NATURAL limita o automático (TUNE nunca robô, FET em 4:1, de-ess, EQ e WARMTH mais leves).
- TRACKING: tira o TUNE do caminho e a latência vai a 0 para gravar. Com o TUNE, a latência é de ~45 ms
  (compensada pelo DAW).
- MAESTRO LINK: lê o MAESTRO no mesmo projeto (grupos ligados no topo) e põe um realce de +1,5 dB na banda de 2 a
  4 kHz em que os SYNTHS deixam mais espaço.
- A / B (COPY A → B) e BYPASS (o som passa sem alteração, só com a latência).
- Na tela: VOCAL DYNAMICS dos últimos 8 s (IN, OUT, RIDE e GAIN REDUCTION), VU do OPTO, redução do FET e do DE-ESS,
  pico e RMS da saída, e 3 leituras ao vivo em cada bloco.
