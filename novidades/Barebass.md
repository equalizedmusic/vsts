# BAREBASS — novidades

Pacote do instalador: `InsideStudioSP-Barebass`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `barebass-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

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
