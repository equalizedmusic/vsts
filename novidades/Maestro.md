# MAESTRO — novidades

Pacote do instalador: `InsideStudioSP-Maestro`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `maestro-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.3 — 2026-11-09
- Versão de TESTE com tudo da 0.1.2 (que não chegou a sair) e a tela nova (esboço v3 aprovado), nas cores do ALIGN /
  BAREBASS.
- CONSOLE CENTRAL: qualquer janela do MAESTRO mostra os 8 grupos lado a lado e mexe em qualquer um deles (ROLE, TRIM
  e FOCUS), sem fechar e abrir a janela de outro grupo. Cada tira: de onde veio o grupo (BY NAME / BY SOUND), o nome
  da faixa, medidor com a marca do alvo do STYLE, GAIN, CUT NOW com a causa (IN FOCUS, TO FOCUS, SHELF, OWNS LOW END),
  TRIM, FOCUS e ROLE. Embaixo, WHOLE MIX (o que vale para todos): AMOUNT, MAX CUT, SPEED, LOW-END GUARD, KEEP KICK /
  BASS ON TOP.
- MAPA DE CONFLITO novo: uma pista por grupo com o espectro em BRANCO; onde ele briga com outro grupo, VERMELHO (como
  o "collision" de EQs de mixagem). Faixa CLASH no topo, trecho de Hz da briga à direita, ▼ com o corte que está
  acontecendo agora e o SHELF do GUARD em laranja. Embaixo: a pior briga e quem está em FOCUS.
- EQ DINÂMICA nova (do grupo clicado): espectro da SOMA da mix atrás, espectro do grupo, FOCUS dos outros grupos
  tracejados, TONE MATCH (tracejado) e LIVE EQ (verde) separados, os 9 nós com o alcance até MAX CUT, a região do
  GUARD e o histórico dos cortes dos últimos 8 s.
- KICK e BASS nunca cedem para FOCUS (são os donos do grave). Na bancada: 0,00 dB de corte neles com LEAD + CLAP
  em FOCUS.

## 0.1.2 — 2026-11-09
- GRUPOS (stems): os papéis agora são os 8 grupos de quem faz pré-mix: KICK, BASS, DRUMS (snare + percs), HATS,
  SYNTHS, AMBIENCE, VOCAL e FX. Põe o MAESTRO em cada grupo e ele reconhece o grupo pelo nome (KICK, Drums, Snare,
  Percs, Hats, Ride, Synths, Lead, Ambience, Pad, Vocal, FX…); continua funcionando canal a canal. Alvos de nível de
  cada STYLE refeitos por grupo. Na bancada: 18/18 nomes certos; LOCK com erro máximo de 0,5 dB por grupo.
- Nome da faixa: o nome que o DAW manda (o Ableton manda nos plugins VST3) fica guardado no projeto e um nome vazio
  não apaga mais o bom. Faixa DUPLICADA não herda mais o nome da original.
- Trocar o ROLE depois do LOCK REFAZ o LOCK do projeto inteiro (antes o KICK escolhido na mão continuava com o nível
  e o tom calculados para o papel adivinhado errado: era o "continua cortando o kick").
- LOW-END GUARD sem passa-altas: agora é um shelf DINÂMICO abaixo de 120 Hz nos outros grupos, só enquanto o KICK /
  BASS toca, e só quanto o grave daquele grupo atrapalha (até −12 dB × AMOUNT). KICK e BASS nunca são cortados por
  ele; grupo adivinhado pelo som com muito grave também não. Na bancada: ATMOS −3,6 dB embaixo com o KICK/BASS
  tocando e 0,0 dB com eles calados; KICK sem perda de grave.
- FOCUS em VÁRIOS ao mesmo tempo, sem prioridade: os outros cedem para todos; duas FOCUS que batem na mesma banda
  cedem um pouco uma para a outra, por igual (até 3 dB). Tela: "YIELDING TO SYNTHS + DRUMS", MAKING ROOM com quem
  cede para aquela FOCUS. Na bancada: PAD cedendo para LEAD e CLAP juntos; LEAD −2,5 / CLAP −2,6 dB entre si.
- Tela: chip ROLE mostra AUTO (veio do nome) ou GUESS (veio do som); corte do GUARD aparece na curva e em LIVE CUTS.

## 0.1.1 — 2026-11-09
- ROLE AUTO pelo SOM bem melhor (para o Ableton, que não manda o nome da faixa ao plugin): ataques por segundo
  (bumbo ~1 por tempo, baixo rolante nas semicolcheias), notas (mudança de espectro), planura dentro de cada banda
  (ruído × tom, sem depender da inclinação) e variação de nível. Na bancada: 10 de 11 faixas certas só pelo som
  (antes 5); pelo nome continua 11 de 11.
- MAKING ROOM e o estado YIELDING contam só o corte pedido pela FOCUS (o do KEEP KICK / BASS ON TOP fica à parte).
- TONE MATCH com barras maiores e o valor em dB; o rótulo GUARD saiu do eixo do mapa (encavalava com o 50).

## 0.1.0 — 2026-11-09
- Primeira versão do MAESTRO (MIX CONDUCTOR), para TESTE (ainda sem ativação por licença e fora do site).
- Vai em TODAS as faixas do projeto (depois dos efeitos de cada uma). As cópias se enxergam sozinhas, sem sidechain
  nem roteamento (DAW com os plugins no mesmo processo: Ableton, Logic, FL, Reaper, Studio One, Cubase).
- ROLE de cada faixa (KICK, BASS, PERC, LEAD, ACID, PAD, FX, VOX, ATMOS): AUTO pelo nome da faixa (Kick, Bass, Hat,
  Lead, Pad, FX…) ou pelo som; dá para escolher na mão.
- LEARN (o projeto todo): toca o drop 20–30 s e cada faixa mede nível e espectro enquanto toca.
- LOCK: cada ROLE vai ao nível do STYLE (PSYTRANCE, PROG PSY, TECHNO, HOUSE, DRUM & BASS, POP), relativo ao KICK.
  As faixas do mesmo ROLE andam juntas (o equilíbrio interno fica). TONE MATCH suave (9 bandas de 1 oitava, ±4 dB) e
  LOW-END GUARD (passa-altas de 120 Hz em quem não é KICK nem BASS). Na bancada: erro de nível por ROLE de 7,0 dB
  para 0,4 dB.
- FOCUS (por faixa, automatizável): as outras cedem espaço só nas frequências em que mascaram a FOCUS e só enquanto
  ela toca (EQ dinâmica, até MAX CUT). KEEP KICK / BASS ON TOP faz o mesmo no grave (abaixo de ~400 Hz).
  Na bancada: PAD −3,3 dB em 1–2 kHz com o LEAD tocando e −0,2 dB com ele calado.
- Tela: CONFLICT MAP do projeto inteiro com cores por temperatura (graves vermelho/laranja → médios amarelo →
  presença branco → agudos azul) e brilho = conflito; clique no ROLE para pôr FOCUS em qualquer faixa dali mesmo.
  Curva da EQ da faixa (TONE MATCH branco, corte ao vivo magenta, espectro da FOCUS tracejado). AMOUNT, MAX CUT,
  SPEED, TRIM, A/B (compara com a mix original no projeto todo) e RESET ALL.
- Latência ZERO (só IIR). Antes do LOCK e sem FOCUS, o som passa sem nenhuma alteração.
