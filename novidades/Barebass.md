# BAREBASS — novidades

Pacote do instalador: `InsideStudioSP-Barebass`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `barebass-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

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
