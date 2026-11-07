# STOMPBOX — novidades

Pacote do instalador: `InsideStudioSP-Stompbox`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `stompbox-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.1 — 2026-11-07
- Ainda TESTE (sem licença, fora do site).
- Pedal novo: BENDER (pitch shifter de pedal, categoria PITCH no seletor). Arraste o pedal: calcanhar = som normal,
  bico = o intervalo do SHIFT (-2 OCT, -1 OCT, -5TH, +4TH, +5TH, +1 OCT, +2 OCT). MIX 10 = só o som deslocado.
  Chave WHAMMY/DETUNE: no DETUNE o pedal dá até 30 cents de dobra (uma voz acima à esquerda, outra abaixo à direita).
  O intervalo que está soando aparece no pé do pedal. Só o som deslocado tem atraso (16 a 32 ms, como nos pedais
  de pitch clássicos); no calcanhar ele sai sem atraso.
- Presets com o nome do guitarrista "de brincadeira": JHON MORELO (agora com o BENDER no +2 OCT, é só pisar), SDASH,
  JHON FRUSHANTE, DAVID GILMOR, JIMMY HENDRIQUES, LINKIN PARQUE, TONY IOMI, ZAKK WAILD.
- 12 presets novos de timbres famosos: KURT COBAINHO, THE EJ, STEVIE RAY VAUGHAM, EDDIE VAN HALLEN, JACK WHITTE
  (oitava abaixo no BENDER), KIRK HAMETT, MARK KNOPFLA, BILLY CORGANN, BRIAN MAIO, ANGUS YOUNGUE, CARLOS SANTANNA e
  JONNY GREENWOODY. Total: 24 presets, todos nivelados.

## 0.1.0 — 2026-11-07
- Primeira versão do STOMPBOX, para TESTE (ainda sem ativação por licença e fora do site): pedalboard de guitarra.
- Até 10 pedais em série (6 à vista, com rolagem), da esquerda para a direita, ligados por cabos. O "+" da posição
  livre abre o seletor (filtro por DINÂMICA, DRIVE, FILTRO, MODULAÇÃO, TEMPO, AMBIÊNCIA). Arraste o pedal para
  trocar a ordem; botão direito: trocar, duplicar, knobs no padrão, mover, remover. O mesmo pedal pode repetir.
- Os 10 pedais (nomes próprios; o som segue os circuitos clássicos):
  SQUASH (compressor OTA: LEVEL, SUSTAIN, ATTACK, BLEND), HUMP (overdrive verde com corcova de médios: DRIVE, LEVEL,
  TONE), CLEARCUT (overdrive transparente, limpo + clipado: GAIN, TREBLE, VOLUME, BASS, HEADROOM LOW/HIGH), GRIT
  (distorção de op-amp: DIST, FILTER, VOLUME, CLASSIC/MODERN), WOOLLY (fuzz de dois transistores: VOLUME, FUZZ, GE/SI),
  QUACK (wah: o PEDAL arrasta, Q, RANGE, PEDAL/AUTO), SWIRL (phaser de 4 estágios: SPEED, FEEDBACK, VINTAGE/MODERN),
  TIDE (chorus/vibrato de BBD: RATE, DEPTH, MIX), ECHO (delay BBD ou fita: TIME, FEEDBACK, MIX, MOD) e HALO
  (reverb SPRING/PLATE/HALL: DECAY, TONE, MIX, PRE-DLY).
- AMP no fim da cadeia, 5 modelos: BLONDE 65 (clean americano), CHIME 30 (classe A), PLEX 68 (crunch britânico),
  CITRUS (britânico encorpado) e IRON HG (high-gain moderno); GAIN, BASS, MID, TREBLE, PRESENCE, MASTER e
  CLEAN/DIRTY. CAB 1x12 OPEN, 2x12 ou 4x12, mic DYN/RIBBON, MIC POS e DISTANCE. AMP + CAB desliga para usar um amp
  de verdade.
- INPUT, NOISE GATE (THRESHOLD, RELEASE) e OUTPUT com medidores. SYNC: o TIME do ECHO vira divisão do BPM do DAW
  (1/32 a 1/2 pontuada) e o SPEED/RATE do SWIRL e do TIDE também. TUNER (a saída fica muda enquanto afina).
  HQ: drives, fuzz e amp com oversampling 4x (o normal é 2x). A/B: dois boards na memória (botão direito copia).
- 12 presets de fábrica: INIT, BLUES CRUNCH, KILL SWITCH, TOP HAT, FUNK EDGE, SKY LEAD, '67 FUZZ, HYBRID WALL,
  DOOM RIFF, BERSERKER, AMBIENT SWELL e CHIME JANGLE. SAVE grava presets do usuário (menu do visor > Meus presets).
- Sem latência. Entrada mono ou estéreo (vira mono, como a guitarra); chorus, delay e reverb abrem para estéreo.
