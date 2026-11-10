# MASTER DOS MAGOS — novidades

Pacote do instalador: `InsideStudioSP-Magos`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `magos-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.0 — 2026-11-10
- Primeira versão do MASTER DOS MAGOS (MASTERING WIZARD), o par do MAESTRO, para TESTE (sem licença e fora do site).
- Vai por último no master. Arraste uma REFERÊNCIA (WAV, AIFF, FLAC, OGG, MP3) para a janela; sem ela, o alvo é a
  curva do STYLE (PSYTRANCE, PROG PSY, TECHNO, HOUSE, DRUM & BASS, POP).
- LISTEN: toca o drop 20–30 s. CAST (um clique): TONE MATCH (9 bandas de 1 oitava, até ±6 dB), DYNAMIC EQ (corta
  MUD 240 Hz, HARSH 3,3 kHz e DE-ESS 7,5 kHz só nos picos; realça LOW PUNCH 63 Hz e AIR 12,5 kHz só quando faltam),
  compressor de 4 bandas (120 / 1,2 k / 6,5 kHz) com limiar automático, graves em MONO abaixo de 120 Hz e limitador
  true peak (−1 dBTP) levando ao TARGET (AUTO = o da referência). Nos primeiros ~20 s o nível se acerta sozinho; se o
  limitador precisar tirar mais de 5 dB, ele para antes do alvo e avisa.
- A/B sempre no MESMO loudness; REF toca a referência no mesmo loudness; BYPASS.
- Telas: TONAL BALANCE (antes, referência, zona alvo, depois, MATCH %), MULTIBAND COMP, LOUDNESS (integrado, true
  peak, curto, PLR, GR), STEREO por banda, WHERE IT WILL PLAY (quanto Spotify / YouTube / Apple abaixam; o grave que o
  celular perde) e HEAR IT ON (PHONE, EARBUDS, CAR, MONO; só para ouvir).
- MAESTRO LINKED: com o MAESTRO nos 8 grupos do mesmo projeto, mostra o nível de cada grupo contra o STYLE e de qual
  grupo vem cada problema do master. Precisa da versão do MAESTRO que publica o link (próxima atualização).
- Latência: 1,5 ms do lookahead do limitador + o detector de true peak (informada ao DAW).
