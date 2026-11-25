# GOGÓ — novidades

Pacote do instalador: `InsideStudioSP-Gogo`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `gogo-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.2.2 — 2026-11-25
- **Atualização automática:** o aviso de versão nova no pé da janela ganha o botão **ATUALIZAR**: o plugin baixa o
  instalador, confere a assinatura e o arquivo e instala sozinho. No Windows ele espera você fechar o DAW e roda o
  instalador (o Windows pede permissão uma vez); no Mac pede a senha de administrador e instala na hora. Depois é só
  abrir o DAW de novo. O **BAIXAR** pelo navegador continua lá para quem preferir.
- Vale a partir da próxima versão: esta ainda se instala pelo aviso de sempre.
- O som e as funções não mudam.

## 0.2.1 — 2026-11-24
- **Ativação automática:** se você já ativou outro plugin ou device InsideStudioSP neste computador, com o mesmo e-mail, este
  se ativa sozinho ao abrir, sem digitar o token. Se o seu e-mail não tem este pacote, a tela de ativação já abre com o
  e-mail preenchido e avisa. Desativar continua valendo: o plugin não se reativa sozinho depois.
- Aviso de autoria na tela da licença (© Rogerio Cardoso / InsideStudioSP).
- O som e as funções não mudam.

## 0.2.0 — 2026-11-16
- Versão oficial, à venda no site.
- Ativação por licença: na primeira vez o plugin pede o e-mail e o token da licença. O mesmo token vale em até 2
  computadores, e desativar num computador libera a vaga para outro. Sem ativar, o plugin passa o som direto, sem
  processar (com o mesmo atraso do TUNE, para a voz não sair desalinhada).
- Menu ≡ > Licença… mostra a licença ativa.
- O som e a tela são os da 0.1.1 (aprovada no teste).

## 0.1.1 — 2026-11-15
- SPACE com tudo no máximo (SPACE, DECAY, FEEDBACK) não ressoa nem distorce mais: o novo GUARD segura o efeito
  5 dB abaixo do nível médio da voz (abaixo disso não mexe; na pausa ele não "levanta" a cauda). A etiqueta do bloco
  mostra GUARD quando ele está agindo. O feedback do delay satura de leve por dentro (as repetições não se empilham)
  e 4 linhas da plate balançam bem devagar, o que tira a ressonância metálica do decay longo.
- OUTPUT: o teto suave que achatava a onda virou um limitador de verdade em −1 dBFS (abaixa o ganho, não distorce).
- Girar DECAY, FEEDBACK, SPACE ou PLATE / DELAY não estala mais: os valores deslizam em vez de pular.
- Tela: "low note" mostra só a nota, o DUCK sai arredondado (sem casas a mais) e os subtítulos dos blocos não são
  mais cortados.

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
