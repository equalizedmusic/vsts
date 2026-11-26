# BAREBASS — novidades

Pacote do instalador: `InsideStudioSP-Barebass`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `barebass-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.4.5 — 2026-11-26
- **Ativação sem internet:** para o computador do estúdio que não fica na internet (ou nem tem placa de rede). Na tela
  de ativação, clique em **SEM INTERNET?** → **SALVAR CÓDIGO** e leve o arquivo num pen drive a um computador com internet;
  em **insidestudiosp.com.br/offline** digite o e-mail e o token e envie o arquivo. Volte com o arquivo **.isplic** e clique
  em **CARREGAR LICENÇA**: todos os seus plugins VST3/AU ficam ativados de uma vez nesse computador, sem vencer e sem
  precisar de internet. Trocou a placa-mãe ou formatou? Gere um arquivo novo.
- O som e as funções não mudam.

## 0.4.4 — 2026-11-25
- **Atualização automática:** o aviso de versão nova no pé da janela ganha o botão **ATUALIZAR**: o plugin baixa o
  instalador, confere a assinatura e o arquivo e instala sozinho. No Windows ele espera você fechar o DAW e roda o
  instalador (o Windows pede permissão uma vez); no Mac pede a senha de administrador e instala na hora. Depois é só
  abrir o DAW de novo. O **BAIXAR** pelo navegador continua lá para quem preferir.
- Vale a partir da próxima versão: esta ainda se instala pelo aviso de sempre.
- O som e as funções não mudam.

## 0.4.3 — 2026-11-24
- **Ativação automática:** se você já ativou outro plugin ou device InsideStudioSP neste computador, com o mesmo e-mail, este
  se ativa sozinho ao abrir, sem digitar o token. Se o seu e-mail não tem este pacote, a tela de ativação já abre com o
  e-mail preenchido e avisa. Desativar continua valendo: o plugin não se reativa sozinho depois.
- Aviso de autoria na tela da licença (© Rogerio Cardoso / InsideStudioSP).
- O som e as funções não mudam.

## 0.4.2 — 2026-11-20
- BASS LINK: o BAREBASS avisa cada nota que toca para o RESENHA (0.8.0) do mesmo projeto, sem rotear MIDI. No RESENHA, BAIXO = SEGUE faz a conversa andar junto com o baixo e BAIXO = RESPONDE toca nos buracos do groove do baixo. O som do BAREBASS não muda.

## 0.4.1 — 2026-11-18
- Versão oficial (20/11/2026), aprovada no teste junto com o CHUTE: chega como atualização para quem tem a 0.3.0.
- DUPLA COM O CHUTE: com o CHUTE no kick do mesmo projeto, o BAREBASS faz sozinho o que o ALIGN faria no canal
  dele, sem ALIGN na cadeia e sem sidechain:
  - KICK LOCK ligado = o mesmo motor do ALIGN dentro do BAREBASS: aprende nota a nota a fase e a polaridade do
    grave que mais somam com o kick e segura o nível (LEVEL GUARD), com latência zero. Na bancada, com a nota
    começando em 8 fases diferentes do kick, o grave ficou sempre a até 0,5 dB da soma de potência (sem ele, até
    1,1 dB fora), e nunca pior que sem ele;
  - DUCK novo (barra no rodapé): o baixo abaixa junto com o kick, na dose escolhida, com a mesma conta do DUCK do
    ALIGN (50% ≈ −6 dB no pico do kick). Vale com o KICK LOCK ligado ou não. Em OFF (padrão) não faz nada;
  - a tela de onda mostra o kick (branco) junto com o baixo, e a saída verde já é a alinhada: dá para ver os dois
    batendo juntos. O rodapé diz o que o alinhamento está fazendo (fase, polaridade e guard);
  - sem CHUTE no projeto o BAREBASS toca exatamente como antes (conferido amostra a amostra).
- Timbre: com o unHUNF ligado, a onda dos OSC na oitava 0 ou acima saía torta. O divisor de bandas girava a fase
  dos harmônicos perto do corte e o som perdia o corpo, principalmente com o envelope fechando o filtro. Agora o
  unHUNF tira só os harmônicos graves de cada onda, na fase exata dela, e o resto passa intacto. Medido com o unHUNF
  em 50%: o que muda acima de 150 Hz caiu de −4 dB para −42 dB em relação à onda. A força do efeito no grave
  continua a mesma.
- KICK LOCK refeito:
  - trava a camada do baixo que está na altura do corpo do kick (SUB, OSC 1 ou OSC 2, a mais perto, até 4
    semitons). Antes era sempre o SUB, mesmo uma oitava abaixo do kick, e cada nota nascia num ponto diferente;
  - com o kick ainda na queda do pitch (longe de todas as camadas), a nota nasce normal;
  - a nota travada entra em 2 ms, sem clique.
- LINK mais firme (mesma correção do CHUTE 0.1.2): não "pisca" mais nem perde batidas no loop ou na exportação. O
  chip mostra "KICK LOCK · CHUTE" enquanto o kick ainda não tocou (antes mostrava NO CHUTE e parecia desligado).
- Tela no mesmo tamanho do CHUTE e do ALIGN (1000 × 752, topo e título iguais). As telas de onda e de filtro
  ficaram maiores, e um rodapé novo diz o estado do LINK e do KICK LOCK.

## 0.4.0 — 2026-11-18
- TESTE (pré-lançamento: não chega como atualização para quem já tem a 0.3.0).
- LINK com o CHUTE (sem sidechain): o chip KICK LOCK no alto mostra a nota do kick do projeto.
- KICK LOCK: cada nota nasce com o SUB em fase com o corpo do kick naquele instante (as outras ondas andam junto, o
  desenho da onda é o mesmo). Clique no chip para ligar ou desligar. Ligado em instância nova; projeto salvo antes
  da 0.4.0 abre desligado (soa igual). Sem CHUTE no projeto, nada muda.

## 0.3.0 — 2026-11-08
- Afinação: com o unHUNF ligado o grave desafinava. O filtro dele subia de frequência a cada nota e, ao passar
  pelo sub, deixava um "toque" na frequência do próprio filtro: o grave deslizava até meia oitava acima da nota
  antes de sumir. Agora o corte do unHUNF fica parado no ponto do knob e o grave só perde VOLUME:
  - o SUB e os OSC em oitava abaixo (−1/−2) caem em volume conforme a frequência deles, sem filtro no caminho;
  - os OSC na oitava 0 ou acima passam por um divisor de bandas fixo e só a parte de baixo cai.
  Medido: o sub fica na nota exata (0 cent de erro) do começo ao fim, com o unHUNF em 0, 50% ou 100%. A força do
  efeito continua a mesma: num 1/16 a 143 BPM o grave abaixo de 50 Hz cai ~6 dB em 50% e ~30 dB em 100%.
- Preset ROLLING (e a base dos outros presets): o OSC 1 estava +7 cents acima da nota, e era dele que vinha a altura
  que se ouve. Agora o OSC 1 fica na nota e só o OSC 2 (uma oitava abaixo) desafina −7 cents. Em projeto já salvo
  o FINE continua como estava; escolha o preset de novo para pegar o ajuste novo.
- VELOCITY: knob novo VELO no MASTER (padrão 60%). Quanto a força da nota muda o volume e o quanto o ENV abre o
  filtro: nota fraca sai mais baixa e mais fechada. Em 0% toda nota sai igual (como antes).
- A nota na tela segue o Ableton: a nota 60 aparece como C3 (antes aparecia C4, uma oitava acima do piano roll).

## 0.2.0 — 2026-11-07
- Primeira versão oficial: grátis, com ativação por licença (e-mail + token, 1 computador), como o ALIGN.
  Página: insidestudiosp.com.br/barebass. Sem ativação o synth fica mudo; menu ≡ > Licença… mostra/desativa.
- unHUNF mais forte: passa-alta de 48 dB (antes 24 dB), que sobe até 65 Hz no máximo (antes 50 Hz) e bem mais
  rápido (15 ms no máximo, 300 ms no mínimo; antes 40 ms–1,5 s). Numa nota de 1/16 a 143 BPM o grave abaixo
  de 50 Hz cai ~10 dB com o knob em 50% e ~37 dB em 100% (na 0.1.2, ~2 dB em 50%). O ataque continua batendo
  inteiro e acima de 150 Hz nada muda.
- Gráfico do unHUNF em dB (0 a −24) nos primeiros 500 ms da nota: a curva "com unHUNF" agora aparece; o knob
  não encosta mais no gráfico.

## 0.1.2 — 2026-11-07
- Fase: toda nota redisparada recomeça a onda do ponto inicial (antes a fase andava de nota para nota e cada uma
  batia diferente). SAW começa no topo, SQR em +1, SIN e TRI no zero subindo.
- ATTACK em 0 agora dá o clique do ataque: a amplitude abre na hora (antes subia em 1,5 ms) e a borda da onda sai seca.
- Nota encostada: quando o DAW manda o "liga" da nota nova antes do "solta" da anterior (mesma nota), a nova não é
  mais cortada.
- HUNF virou unHUNF e passou a agir só no sub (10–50 Hz): um passa-alta de 24 dB que fica em 2 Hz no ataque
  (o sub bate inteiro, sem atraso) e sobe até 15–50 Hz conforme o knob. O começo do knob é suave e girar não
  estala mais (na 0.1.1 o caminho do som trocava na hora em que o knob saía do 0).
- O plugin abre no preset INIT: FINE e OCTAVE em 0 (o SUB uma oitava abaixo), LEVEL em 80, filtro aberto.
  INIT é o primeiro da lista.
- LED do OSC 1, OSC 2 e SUB: clicar liga e desliga (o painel apaga quando desligado).
- MONO / LEGATO virou um seletor à vista no MASTER, junto do GLIDE (saiu o chip do topo).
- Tela: saiu a faixa de baixo (teclado e texto de ajuda); os valores embaixo dos knobs dos osciladores não
  encostam mais na borda.
- Ainda TESTE: sem ativação por licença e fora do site.

## 0.1.1 — 2026-11-07
- Tela: a onda mostra dois ciclos da NOTA tocada (antes eram dois ciclos do sub, mais longos que uma nota de 1/16,
  e a tela juntava duas notas). No HUNF, o texto não fica mais riscado pela linha tracejada e o valor do knob
  não sai do painel.
- Ainda TESTE: sem ativação por licença e fora do site.

## 0.1.0 — 2026-11-07
- Primeira versão do BAREBASS, para TESTE (ainda sem ativação por licença e fora do site): synth de baixo simples,
  uma voz (toca a última nota apertada).
- OSC 1 e OSC 2: onda SIN / TRI / SAW / SQR, OCTAVE (−2 a +2), FINE (±50 cents) e LEVEL. SUB: onda SIN / SQR,
  OCTAVE (−2 a 0) e LEVEL.
- FILTER passa-baixa de 12 ou 24 dB (SLOPE), com CUTOFF, RESONANCE e ENV (quanto o envelope abre o filtro, ±6 oitavas).
  FILTER ENV com ATTACK, DECAY, SUSTAIN e RELEASE.
- HUNF: depois do filtro e antes do volume. Faz o grave de 20 a 80 Hz bater no começo de cada nota e sumir
  (crossover Linkwitz-Riley em 80 Hz; acima disso nada muda). Quanto mais alto, mais rápido some; em 0 fica desligado.
- MASTER: VOLUME e GLIDE. MONO redispara o envelope a cada nota; LEGATO não redispara quando as notas se encostam.
- Presets de fábrica: ROLLING, PLUCK, DEEP SUB, ACID 12, SQUARE GROWL e INIT.
- Na tela, ao vivo: a onda (dois ciclos, antes do filtro, na saída e o sub), a curva do filtro com o corte deste
  instante e até onde o ENV abre, o envelope com o ponto onde a nota está e o gráfico do HUNF com a redução no grave.
