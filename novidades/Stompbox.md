# STOMPBOX — novidades

Pacote do instalador: `InsideStudioSP-Stompbox`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `stompbox-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.5 — 2026-11-08
- Ainda TESTE (sem licença, fora do site).
- Distorção com cara de downsampler (HUMP, CLEARCUT, GRIT, WOOLLY e o AMP): os drives rodavam com oversampling 2x,
  pouco para o ganho deles, e o que passava de 24 kHz voltava como chiado fora do tom (aliasing). Agora o padrão
  é 8x e o HQ é 16x (antes 2x e 4x). Medido com o drive no máximo e uma nota de 1,1 kHz: o chiado fora do tom
  ficava só 27 a 38 dB abaixo do som; agora fica 44 a 51 dB abaixo. O nível dos pedais e dos presets não mudou.
  Custa mais CPU: nos presets, de 2 a 7% para 3 a 12% de um núcleo. O pior caso (10 drives + amp com HQ) usa ~31%.
- Instalador do Mac: o VST3 podia ficar na versão velha (só o AU atualizava). Agora o instalador apaga a cópia
  antiga do mesmo plugin que estiver na outra pasta (/Library ou ~/Library). O DAW carregava essa cópia no lugar
  da nova, mas o AU escolhe sozinho a mais nova. Ele também não pula mais o VST3 achando que "a mesma versão já
  está instalada".

## 0.1.4 — 2026-11-08
- Ainda TESTE (sem licença, fora do site).
- Seletor de pedais (adicionar/trocar): os pedais apareciam sem cor, quase transparentes. Agora aparecem inteiros.
- No board, o 1º knob de cada pedal (LEVEL, DRIVE, DIST, RATE, TIME, DECAY...) saía esmaecido. Agora sai igual aos outros.
  (Causa: a imagem pronta do knob/pedal herdava a transparência da última cor usada — LED aceso, fundo da célula.)
- Arrastar pedais entre as posições do board:
  - Pega pela alça ⋮⋮ ou pela faixa do nome no topo do pedal (além do corpo, fora dos knobs e chaves).
  - A linha amarela mostra o vão exato onde o pedal vai cair (antes ela errava uma posição ao arrastar para a direita).
  - Levando o pedal até a borda do board, ele rola sozinho para as posições escondidas (7 a 10 pedais).
  - O pedal arrastado acompanha o mouse; solto no mesmo lugar, nada muda.

## 0.1.3 — 2026-11-08
- Ainda TESTE (sem licença, fora do site).
- Textos que ainda encostavam, achados nas fotos da 0.1.2:
  - CLEARCUT e HALO: HEADROOM e o PLATE da chave ficavam na mesma linha dos rótulos dos knobs; a chave desceu.
  - Etiqueta BYPASS colada no nome do pedal no topo (CHORUS / VIBRATO): agora com folga.
  - AMP + CAB OFF: o aviso ficava em cima da placa do amp; agora vai numa faixa própria abaixo dela.
  - TUNER grande: os cents saíam com 6 casas (-0.156637); agora inteiro. Fundo do TUNER sem transparência.

## 0.1.2 — 2026-11-07
- Ainda TESTE (sem licença, fora do site).
- Tela muito mais leve: as peças (fundo, pedais, amp, cab, knobs) ficam prontas na memória e só os medidores, o LED
  do gate e o afinador são repintados 30 vezes por segundo. Knob e pedal respondem na hora e o Ableton não engasga
  mais com a janela aberta. O motor de áudio não tem atraso (só o BENDER, quando pisado).
- Textos que encavalavam: pedais de 4 knobs (SQUASH, CLEARCUT, ECHO, HALO) em 2 fileiras; HEADROOM e a chave do HALO
  com folga; "AMP + CAB" virou botão ON/OFF; BYPASS virou etiqueta no topo do pedal; rótulos encolhem para caber.
- AFINADOR fixo na entrada, ao lado do INPUT: nota, barra de cents e "afinado" em verde. Não corta o som; clicar
  nele abre o TUNER grande (que deixa a saída muda).
- Som refeito pelos circuitos de referência:
  - AMP: BASS/MID/TREBLE são o tone stack de cada amp (Fender, Marshall, Vox, britânico encorpado e high-gain),
    entre o pré e a potência, com os knobs interagindo como no amp real; PRESENCE na potência; CHIME 30 classe A.
  - CAB: cada caixa é uma resposta de impulso gerada da curva do falante (1x12 aberto, 2x12, 4x12), sem latência.
  - HUMP = Tube Screamer, GRIT = RAT (CLASSIC diodos / MODERN LEDs), WOOLLY = Fuzz Face (GE macio e assimétrico, SI
    mais duro), CLEARCUT = Klon, SQUASH = Dyna/Ross, QUACK = Cry Baby.
  - Os 24 presets renivelados.

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
