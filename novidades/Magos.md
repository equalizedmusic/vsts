# MASTER DOS MAGOS — novidades

Pacote do instalador: `InsideStudioSP-Magos`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `magos-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.1 — 2026-11-10
- TESTE (sem licença e fora do site).
- TONAL BALANCE virou um analisador de verdade, como os de EQ profissional: 10 Hz a 35 kHz, resposta rápida
  (o grave mexe junto com a música), resolução fina de 1/24 de oitava e inclinação de 4,5 dB por oitava (o agudo não
  aparece mais "caindo"). As curvas de alvo e de referência estão na mesma escala.
- MANUAL no MULTIBAND COMP e na DYNAMIC EQ: clique em MANUAL e arraste os valores emoldurados para cima ou para baixo
  (SHIFT = ajuste fino, duplo clique = padrão, roda do mouse também). Começa do que o AUTO fazia, então o som não
  pula. Em cada banda do multibanda: THRESH, RATIO, ATTACK, RELEASE e GAIN. Em cada ponto da DYNAMIC EQ: liga ou
  desliga (clique no nome), MAX CUT / MAX LIFT e THRESH (quantos dB fora do alvo antes de agir). Funciona com ou sem
  CAST. AUTO volta ao automático. Tudo fica salvo no projeto e pode ser automatizado no DAW.

## 0.1.0 — 2026-11-10
- Primeira versão do MASTER DOS MAGOS (MASTERING WIZARD), o par do MAESTRO, para TESTE (sem licença e fora do site).
- Vai por último no master. Arraste uma REFERÊNCIA (WAV, AIFF, FLAC, OGG, MP3) para a janela; sem ela, o alvo é a
  curva do STYLE (PSYTRANCE, PROG PSY, TECHNO, HOUSE, DRUM & BASS, POP).
- LISTEN: toca o drop 20–30 s. CAST (um clique): TONE MATCH (9 bandas de 1 oitava, até ±6 dB), DYNAMIC EQ (corta
  MUD 240 Hz, HARSH 3,3 kHz e DE-ESS 7,5 kHz só nos picos; realça LOW PUNCH 63 Hz e AIR 12,5 kHz só quando faltam),
  compressor de 4 bandas (120 / 1,2 k / 6,5 kHz) com limiar automático, graves em MONO abaixo de 120 Hz e limitador
  true peak (−0,3 dBTP) levando ao TARGET (AUTO = o da referência). Nos primeiros ~20 s o nível se acerta sozinho; se o
  limitador precisar tirar mais de 5 dB, ele para antes do alvo e avisa.
- A/B sempre no MESMO loudness; REF toca a referência no mesmo loudness; BYPASS.
- Telas: TONAL BALANCE (antes, referência, zona alvo, depois, MATCH %), MULTIBAND COMP, LOUDNESS (integrado, true
  peak, curto, PLR, GR), STEREO por banda, WHERE IT WILL PLAY (quanto Spotify / YouTube / Apple abaixam; o grave que o
  celular perde) e HEAR IT ON (PHONE, EARBUDS, CAR, MONO; só para ouvir).
- MAESTRO LINKED: com o MAESTRO nos 8 grupos do mesmo projeto, mostra o nível de cada grupo contra o STYLE e de qual
  grupo vem cada problema do master. Precisa da versão do MAESTRO que publica o link (próxima atualização).
- FASE LINEAR em todo o caminho do som: TONE MATCH, TILT, DYNAMIC EQ e o crossover das 4 bandas não giram a fase
  (o transiente do kick sai inteiro e a soma das bandas é exata). Pouca CPU: tudo numa convolução só, com os ganhos
  mudando sem redesenhar filtro.
- Latência: ~55 ms (2640 amostras a 48 kHz: fase linear + lookahead do limitador), informada ao DAW, que compensa
  sozinho. Só se nota tocando ou gravando AO VIVO pelo master.
