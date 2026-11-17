# RETALHO — novidades

Pacote do instalador: `InsideStudioSP-Retalho`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `retalho-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.5 — 2026-11-17
- Ainda TESTE (sem licença, fora do site).
- Três modos na tela principal: PICOTE | ESTICA | GROOVE. O AVANÇADO saiu (e com ele os pads de fábrica: uma nota MIDI
  não troca mais o modo da tela). Projeto salvo num modo antigo abre no PICOTE.
- Mais leve: o efeito é calculado numa linha própria, fora da tela. Girar knob não trava mais a janela, e a tela só
  é redesenhada quando algo muda (a onda fica guardada numa imagem; a 30 Hz só anda o cursor).
- Sem estalos: o resultado novo entra com 25 ms de crossfade (com o projeto tocando e no OUVIR), e as pontas do loop
  têm subida e descida curtas. No PICOTE, as emendas entre pedaços ficaram mais suaves (3 ms / 6 ms). No ESTICA, um
  grão não dá mais a volta no fim do trecho no meio dele. Mexer num knob com o OUVIR ligado troca o som sem parar.
- PICOTE: arraste na onda (ou clique num pedaço) e escolha o efeito daquele pedaço: LIMPA, REPETE, TOM, AO CONTRÁRIO,
  ACELERA, FREIA ou CORTE. Os pedaços marcados ganham uma quina branca e ficam salvos no projeto. QUANTO EFEITO ganhou
  MANUAL (só o que você marcar). TAMANHO vai até 16 BARS. ARRASTE PARA O DAW agora é só o botão (a onda serve para
  marcar). "AO CONTRÁRIO" não sai mais quebrado na legenda.
- ESTICA: o knob grande virou MOVIMENTO. Ele leva os knobs do AJUSTE FINO pelo caminho da DIREÇÃO ao longo da frase,
  na FORMA escolhida (SOBE, SOBE E DESCE, DEGRAUS, ONDA, DESCE). O arco claro em cada knob mostra onde o movimento está
  agora (anda junto com o som), e a linha sobre a onda mostra o movimento na frase. VARIAR troca a forma e os grãos
  (antes quase não mudava nada).
- GROOVE (modo novo): as sílabas da voz, do PONTO em diante, tocam numa grade de 1/16 que você programa, cada nota
  numa linha da ESCALA (MENOR, MAIOR, PENTA MENOR, PENTA MAIOR). AFINAR mede a altura de cada sílaba e leva para a nota
  da linha. Clique para pôr, arraste para alongar, clique de novo para apagar, botão direito escolhe a sílaba. 8
  padrões prontos (◀ ▶), VARIAR sorteia um groove novo, TOM sobe ou desce tudo, SWING, 1/2/4 BARS e VOZ JUNTO.

## 0.1.4 — 2026-11-17
- Ainda TESTE (sem licença, fora do site).
- Tela principal nova, bem mais simples: PICOTE | ESTICA no topo, a amostra, UMA onda grande e poucos controles. Os
  cartões, os knobs de macro, os pads e o QUANDO TOCA saíram da tela e ficaram no AVANÇADO (o antigo PRO).
- PICOTE: a frase toca LIMPA e o efeito cai só em alguns pontos, no fim da frase e do compasso. RECEITA ◀ ▶ (16
  receitas), QUANTO EFEITO (POUCO 75 % limpa, MÉDIO 62 %, MUITO 50 %), RITMO DAS REPETIÇÕES (1/4, 1/8, 1/16, 1/32,
  MISTO), TAMANHO (1, 2 ou 4 compassos), FADE OUT e ESTÉREO. VARIAR sorteia de novo onde o efeito cai. Na onda, cinza
  é voz limpa e cada efeito tem a sua cor: repete, tom, ao contrário, acelera, freia e corte.
- ESTICA (modo novo): a voz vira grãos. Seis direções (MEIO TEMPO, CONGELA, AO CONTRÁRIO, ABRE, ESTILHAÇA, TÚNEL) e um
  knob grande diz quanto. AJUSTE FINO: GRÃO, ESPAÇO, VELOCIDADE (0 congela, negativo anda de ré), ESTÉREO, GRADE e TOM.
  Embaixo da onda aparece cada grão: em cima os da esquerda, embaixo os da direita.
- Com o projeto tocando, o resultado do PICOTE ou do ESTICA toca em loop no tempo do projeto, no lugar da amostra crua.
- EXPORTAR WAV na própria tela (Documentos/RETALHO/Exports), ou arraste a onda inteira para uma faixa do DAW.

## 0.1.3 — 2026-11-16
- Ainda TESTE (sem licença, fora do site).
- Soltou o arquivo, o efeito já está pronto: o RETALHO renderiza na hora a partir do próprio arquivo, sem precisar dar
  play no projeto, e toca o resultado uma vez. O efeito começa no primeiro ataque que soa (o ▼ âmbar na onda da
  amostra); ◀ PONTO ▶ troca para o ataque anterior ou o próximo e renderiza de novo. Mexer em cartão, knob ou preset
  também refaz o resultado na hora. OUVIR toca de novo e ARRASTE PARA O DAW leva o clip pronto.
- Tela mais leve: o brilho das ondas agora é desenhado por preenchimento (antes, contorno em cada barra).
- PRO/GROOVE: "ENTRA NO PRÓXIMO COMPASSO" com acento certo; a onda de cada chop não passa mais por cima dos "ms".

## 0.1.2 — 2026-11-16
- Ainda TESTE (sem licença, fora do site).
- Agora o RETALHO é um SAMPLER: a voz fica dentro dele. Arraste o arquivo do vocal (WAV, AIFF, FLAC, MP3 ou OGG, até
  60 s) para a janela, use ABRIR ou clique em CAPTURAR para gravar 1, 2, 4 ou 8 compassos da faixa a partir do
  próximo compasso. Com amostra carregada, a faixa original fica muda e a amostra toca em loop no tempo do projeto.
  STUTTER, RISER, TAPE STOP e GROOVE (e todo o PRO) atuam em cima da amostra.
- Faixa AMOSTRA no SIMPLE, entre os cartões e o resultado: nome, duração, tamanho do loop, a onda com o trecho que
  toca (arraste na onda para escolher; duplo clique volta para ela inteira) e onde está tocando. X tira a amostra e a
  faixa volta a passar. No PRO, a amostra fica no menu ≡.
- A amostra é salva dentro do projeto (FLAC): reabriu o projeto, ela está lá, mesmo se o arquivo sumir.
- GROOVE do SIMPLE: a voz original cala enquanto os chops tocam (antes só abaixava 9 dB).
- PRO: o rodapé mostra a versão certa e o ONE-SHOT não encosta mais na borda do botão.
- Visual novo das ondas: o resultado mostra a onda REAL do áudio processado, em barras espelhadas com degradê e brilho,
  com um vão entre cada repetição (repetição curta vira uma barra cheia). A amostra, o ORIGINAL e os chops do GROOVE
  usam o mesmo desenho. Grade de tempo tracejada, cursor com brilho e knobs com corpo de metal escuro e ponto aceso.
- SIMPLE: a onda do resultado ficou bem maior (OUVIR e LOOP subiram para o topo, ao lado do ARRASTE PARA O DAW) e os
  cartões de efeito ficaram mais baixos.

## 0.1.1 — 2026-11-16
- Ainda TESTE (sem licença, fora do site).
- Nova tela SIMPLE, que é a que abre: 1) escolha o efeito em 4 cartões (STUTTER, RISER, TAPE STOP, GROOVE),
  2) ajuste em 5 knobs grandes (SPEED ou DENSITY, PITCH, FILTER, LENGTH, MIX), 3) escolha QUANDO TOCA (TODO COMPASSO,
  FIM DE 4 BARS, FIM DE 8 BARS ou SÓ NO PAD). A onda do resultado fica no meio, com OUVIR, LOOP e ARRASTE PARA O DAW.
  Pad vazio vira "+ SALVAR" (um clique guarda o ajuste atual). PRESET com ◀ ▶ para passar um por um.
- A tela completa continua no botão PRO, ao lado do nome; nada foi removido e o som é o mesmo nas duas.
- PRO: o PRESET não cobre mais as abas (nome longo é cortado) e o chip LATENCY saiu do topo (é sempre 0 ms).
  Os nomes dos knobs não encostam mais no título do painel e o valor do DECAY ficou curto (dB). Os botões de RAMP TO
  e do GRID 1/16T não cortam mais na borda.

## 0.1.0 — 2026-11-16
- Primeira versão do RETALHO (nome provisório), para TESTE: ainda sem ativação por licença e fora do site.
  Stutter + groove de vocal, VST3 e AU, latência 0 (repete só o que já passou).
- Tela no layout do esboço v2: display de onda com ORIGINAL (a voz capturada, ataques marcados e o CHUNK em
  destaque) e RESULT (o fill renderizado em blocos, com a curva de PITCH em ciano e a do FILTER em rosa).
- EDIT, o fill desenhado: SHAPE (GRID, RAMP, CURVE, FREE, TREMOLO), LENGTH (1/2, 1 ou 2 compassos), RATE de 1/4 a 1/64
  com tercina e pontuada, RAMP TO e curva LOG / LIN / EXP. Cada repetição tem DECAY, FADE IN, FADE OUT, GATE e SWING.
  PITCH com START, END e CURVE, em degraus (STEPS) ou contínuo (GLIDE); KEY trava os degraus na escala (AUTO detecta o
  tom da voz). FILTER HPF / LPF / BPF varrendo de FROM a TO, com RESO. STEREO de mono a PING-PONG, TAPE STOP no fim e
  REVERSE. No GRID, clicar na fatia liga ou desliga, arrastar muda o volume e o botão direito põe REVERSE; no FREE o
  tamanho das fatias é desenhado na onda. CHUNK: AUTO (a partir do disparo) ou PREV / NEXT pelos ataques anteriores.
- COMPLEX: 8, 16 ou 32 passos com 8 tipos (REPEAT ×2/×3/×4/×8, PITCH ±24 st, REVERSE, TAPE stop / start, SPEED ½× / 2×,
  GATE, THRU, MUTE). Clique no topo do passo troca o tipo, arrastar muda o valor e o rodapé (ou o botão direito) põe o
  cadeado. READ: FOLLOW (cada passo lê o seu ponto), HOLD (todos leem do 1º) ou HITS (pula para o ataque mais
  próximo). DICE sorteia tipos, valores ou só pitch (AMOUNT, CHAOS, SEED); EVERY sorteia de novo a cada 1, 2, 4 ou 8
  compassos no AUTO; o cadeado protege o passo.
- GROOVE: a voz vira 8 chops (as últimas sílabas, por ataque ou cortadas em 1/8 / 1/16), LIVE ou FREEZE, tocando num
  pattern de 1 ou 2 compassos em 1/16 ou 1/16T, com 3 linhas (OFFBEAT, ACCENT, FILL). 7 estilos: OFFBEAT, GALLOP,
  SKIP, 2-STEP, FUNK, TRAP e CALL&RESP (o chop só responde no vão da voz). DENSITY, SWING, HUMANIZE e PITCH do chop.
  Mistura com a voz: LAYER, DUCK (abaixa a voz quando o chop entra) ou REPLACE, com HPF, LPF e VOL dos chops. DICE e
  LOCK GRID. GROOVE ON liga e desliga no compasso.
- LIVE (rodapé fixo): AUTO (dispara sozinho no fim ou no início de 1, 2, 4 ou 8 compassos, com o projeto tocando),
  HOLD (enquanto segura o pad ou a nota) e ONE-SHOT (o fill inteiro). CATCH espera 1/16, 1/4 ou o próximo compasso.
- 6 pads de memória (cena completa): clique dispara, Shift+clique salva, botão direito renomeia ou limpa. MIDI C3–F3
  dispara os pads (a velocity é o volume do fill) e cada pad também é um parâmetro automatizável.
- ▶ ORIGINAL / ▶ RESULT tocam a janela capturada ou o fill (LOOP, ON BAR). EXPORT grava o RESULT em WAV 24 bits em
  Documentos/RETALHO/Exports e o chip ARRASTE PARA O DAW solta o WAV direto numa faixa.
- 54 presets de fábrica em 5 bancos (RISERS, DROPS & TAPE, GLITCH, GROOVE VOX, TREMOLO & GATE) e preset do usuário
  em arquivo .retalho. DRY/WET, OUTPUT, limitador de segurança em −0,3 dBFS, A/B e BYPASS.
