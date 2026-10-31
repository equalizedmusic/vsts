# MESA — novidades

Pacote do instalador: `InsideStudioSP-Mesa`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `mesa-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.5.0 — 2026-10-31
- Os circuitos que a MESA emula, agora com o timbre de cada um:
  - PRÉ: E-4K (o pré da console britânica, limpo e firme; era o LIMPO), API 312 e BRIT 73. Em 0 VU, sem DRIVE, a
    distorção fica em cerca de 0,05% no E-4K, 0,35% no API e 0,8% no BRIT, como a ordem dos originais. Cada cópia do
    plugin tem uma pequena diferença entre L e R, como dois canais de hardware.
  - COMPRESSOR, quatro modelos (clique no visor verde da DINÂMICA ou em CARÁTER, no MESA CANAL):
    - VCA 4K: o compressor do canal da console britânica, limpo. SOLTURA no fim = AUTO.
    - COLA 4K: o compressor de barramento da mesma console, com as teclas do original (razão 1,5/2/4/10, ataque de
      0,1 a 30 ms, soltura de 0,1 a 1,2 s ou AUTO).
    - FET 76: ataque de 20 a 800 µs, soltura de 50 ms a 1,1 s, razões 4/8/12/20 e ALL (todas as teclas). Suja mais
      quanto mais comprime (0,7% a 1,4% de distorção com 7 a 15 dB de redução).
    - OPTO 2A: nivelador ótico com válvula. Ataque de ~10 ms, joelho largo, solta metade rápido e o resto devagar, e
      solta mais devagar depois de comprimir por mais tempo. RAZÃO baixa = COMP (~2,5:1), alta = LIMIT.
  - EQ, três modelos: E-4K, BRIT 81 e o novo API 550 (frequências e ganho em degraus, Q que estreita com o ganho).
- Controles novos no canal: Q nas bandas médias, BELL no HF e no LF (prateleira vira sino), LPF de 12 dB/oitava,
  ATAQUE, SOLTURA e MIX no compressor do canal. O compressor dos grupos também ganhou ATAQUE, SOLTURA e o modelo.
- Na cabeça de cada canal, a ordem do caminho já mostra o modelo de cada etapa (PRÉ 73, EQ 550, DIN 2A).
- Ao passar o mouse, os knobs mostram o valor que o circuito usa de verdade: o degrau do BRIT 81 e do API 550, o tempo
  de ataque e soltura de cada compressor e a razão das teclas.
- Sessões antigas: o compressor de um GRUPO salvo antes da 0.5 continua no COLA 4K com ataque de 10 ms.

## 0.4.0 — 2026-10-30
- O DRIVE do pré agora satura de verdade, e cada modelo do seu jeito (o volume fica parecido, muda o caráter):
  - LIMPO: limpo até perto do topo, depois 3ª harmônica e clipe macio, como pré de console britânica.
  - API 312: joelho duro de operacional discreto, 3ª forte com um pouco de 2ª, transiente achatado com punch.
  - BRIT 73: 2ª harmônica desde cedo (classe A); o grave satura primeiro no transformador e fica mais gordo, com
    harmônicas, em vez de sumir; o agudo amacia conforme o drive.
  - Com uma senoide em 0 VU, o DRIVE no máximo dá cerca de 13% de distorção no LIMPO, 14% no API 312 e 10% no BRIT 73
    (28% no grave).
- EQ com a curva do circuito analógico até o fim do agudo: um shelf em 16 kHz agora sobe inteiro, sem o achatamento
  dos filtros digitais comuns. Girar um knob do EQ não faz mais degrau nem estalo: ele anda até o valor em ~25 ms.
  O Q proporcional do E-4K ficou mais largo com pouco ganho e mais estreito no máximo, como no original.
- Medidores calibrados: o VU tem a balística de VU de verdade (300 ms) e a escala real (0 VU a 70% do arco), com
  0 VU = -18 dBFS; o PPM e os picos não perdem mais os transientes entre um quadro da tela e outro. O VU de ENTRADA
  mede o que chega no pré, depois do GANHO.
- A console segue a ordem das faixas do DAW: usa o número que o DAW informa (VST3) ou o do começo do nome da faixa
  ("3-Audio"). O LED vermelho mostra esse número. Duplo clique nele (ou "Posição na console…" no menu do MESA CANAL)
  escolhe à mão.
- Zoom de verdade na console: - e + na barra lateral (50% a 200%), Ctrl + roda do mouse, clique no número para voltar
  a 100%. Com zoom grande a console rola na vertical; GERAL continua mostrando todos os canais.

## 0.3.1 — 2026-10-30
- Correções:
  - O knob GANHO da entrada aparecia quase transparente.
  - A legenda GATE/MIX encostava na linha do EQ.
- A console abre com o primeiro canal selecionado.

## 0.3.0 — 2026-10-30
- Agora são dois plugins no mesmo instalador:
  - **MESA**, a console. Vai no master do DAW e mostra o mixer inteiro: todos os canais lado a lado, os grupos, o
    medidor geral com goniômetro, os VUs de redução de ganho, o COLA BUS do master e os faders dos grupos e do master.
  - **MESA CANAL**. Vai no fim de cada faixa (tipo CANAL) ou no canal de grupo (tipo GRUPO, com G1, G2 ou G3). Cada
    um processa o som da própria faixa e aparece sozinho na console.
- O áudio segue o caminho do DAW, então sends, returns e grupos continuam valendo. Mexer na console é mexer no plugin
  daquela faixa, e a automação do DAW grava.
- Visual novo no padrão dos mixers de plugin de console. Cada canal tem, de cima para baixo:
  - a cabeça com modelo, ordem do processamento, medidores e o número em LED;
  - ENTRADA, DINÂMICA (com LEDs de redução de ganho), EQ de 4 bandas e o grupo;
  - a fita com a cor e o nome da faixa;
  - PAN, SOLO e CUT;
  - o fader comprido com medidor de LED.
- A chapa é escura no EQ E-4K e cinza no BRIT 81.
- Na console, a roda do mouse passa os canais e GERAL encolhe tudo para caber na janela. Os botões ◀ ▶ trocam o canal
  selecionado e o visor mostra número, nome e grupo dele. LIMPA SOLO, VU/PPM e ECO/HQ valem para todos os canais.
- A janela do MESA CANAL mostra o canal inteiro e, ao lado, a ligação com a console, o número, o nome, o grupo, três VUs
  (entrada, redução e saída) e os modelos de PRÉ e EQ.
- O som não mudou.

## 0.2.0 — 2026-10-30
- Visual de equipamento real: todas as peças renderizadas de novo com o acabamento aprovado na amostra — plástico
  fosco de console (sem verniz), poeira nas frestas, micro-riscos, serigrafia gasta, madeira a óleo e luz de estúdio.
- Vista de frente e de cima, levemente inclinada, como quem está sentado na console: dá para ver a lateral dos knobs,
  a altura das teclas e a sombra de verdade de cada peça na chapa (vem do 3D, não é desenhada).
- Teclas com altura e legenda na lente (apertada fica mais baixa e acesa), LEDs em domo na ponte, LEDs acesos com
  brilho em volta, VU iluminado por dentro.
- Janela PERTO (uns 6 canais na largura, como numa foto da console) ou GERAL (a console toda): botões − + e GERAL no
  canto, Ctrl/Cmd + roda aproxima, a roda rola (Shift: para o lado) e arrastar a chapa passeia. A janela abre perto do
  próprio canal e guarda o tamanho.
- O som não mudou.

## 0.1.1 — 2026-10-30
- Correção: o VU de um grupo, o de um grupo vazio e o painel do MASTER apareciam apagados (meio transparentes)
  quando havia vaga vazia na console.

## 0.1.0 — 2026-10-30
- Primeira versão (Fase 1): console analógica em plugin, com instalador próprio.
- Uma MESA no fim de cada canal do DAW: cada cópia vira uma fileira da console, e a janela de qualquer uma mostra a
  console inteira (12 canais à vista, mais que isso rola), os 3 grupos e o MASTER. Dá para mexer em qualquer canal
  por qualquer janela.
- Modos CANAL, GRUPO (G1 a G3) e MASTER. O fader e o pan mexem no som que sai do plugin (o fader do DAW fica em 0 dB).
- PRÉ: LIMPO, API 312 e BRIT 73, com GAIN, DRIVE, PAD de -20 dB e HPF de 18 dB/oitava. A saturação roda a 4× (HQ).
- EQ de 4 bandas: E-4K (Q proporcional ao ganho) e BRIT 81 (frequências em degraus e prateleira de grave com calombo).
- Dinâmica: VCA do canal (THR, RAT, GATE) e COLA 4K nos grupos (THR, RAT, MIX); botão PRÉ põe a dinâmica antes do EQ.
- MASTER: COLA BUS 4K (threshold, attack, release com AUTO, makeup, ratio, mix, sidechain HPF 90 Hz) e fader de saída.
- Medidores: VU na ponte (VU ou PPM), coluna de LEDs em dBFS de pico e LEDs de redução de ganho.
- Visual renderizado do modelo 3D da console (chapas, knobs, botões, LEDs e faders).
- ECO (sem sobreamostragem, menos CPU) ou HQ, para todas as MESAs de uma vez.
- Ativação por licença: na primeira vez o plugin pede o e-mail e o token da licença. Sem ativar, o som passa direto.
