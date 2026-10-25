# MERCÚRIO — novidades

Pacote do instalador: `InsideStudioSP-Mercurio`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `mercurio-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.5.0 — 2026-10-25
- Visual de hardware: knobs de alumínio usinado, teclas que acendem em magenta, LEDs em domo e laterais de
  nogueira.
- Desinstalador: no Windows, atalho "Desinstalar" no menu Iniciar (além de Aplicativos instalados); no Mac,
  "Desinstalar InsideStudioSP Mercurio.command" dentro do .dmg. Os presets do usuário ficam.
- O instalador apaga o VACINA.vst3 do nome antigo, e o desinstalador também.

## 0.4.0 — 2026-10-24
- O VACINA agora se chama **MERCÚRIO**. Instalador `InsideStudioSP-Mercurio`, bancos `.mrcbank`. Para o DAW é um plugin
  novo: projetos salvos com o VACINA continuam pedindo o VACINA.
- Visual novo, metálico:
  - chapa de alumínio escovado entre laterais de alumínio fresado;
  - faixas de alumínio anodizado preto em cima e embaixo, com filete iridescente;
  - knobs de alumínio usinado com tampa preta e ponteiro aceso;
  - LEDs magenta e visores OLED ciano;
  - cada operador com a sua cor (ponteiro do LEVEL, onda no visor e caixa no desenho do algoritmo).
- Motor novo: FM de 4 operadores, no lugar do virtual-analógico (que fica com a QUIMERA).
  - 8 algoritmos, com o desenho de quem modula quem na tela.
  - 8 formas de onda por operador (SINE, HALF SINE, ABS SINE, PULSE SINE, ALT SINE, CAMEL, SQUARE, SAW).
  - Razão RATIO × FINE para timbres metálicos e inarmônicos; envelope e velocity próprios em cada operador.
  - Realimentação no OP 4, RING MOD, FOLD (dobra de onda) e NOISE.
- LFO 1 na razão dos moduladores e LFO 2 no índice e na realimentação; a matriz ganhou FM INDEX, nível e razão de cada operador.
- 360 presets novos em BASS, LEAD, PAD, PLUCK, BELLS, KEYS, ARP, TEXTURA e FX, com volume nivelado.
- Presets e projetos da 0.3 não abrem com o mesmo som (os parâmetros dos osciladores saíram).

## 0.3.1 — 2026-10-24
- O nome do knob SUPER SPREAD (OSC 1) aparecia cortado nas bordas. Agora aparece inteiro.

## 0.3.0 — 2026-10-24
- Letras maiores na tela, principalmente nas listas de LEDs:
  - modo dos filtros 1 e 2;
  - MODEL dos osciladores;
  - modos do ARP;
  - KEY MODE e ROUTING.
- Também ficaram maiores os nomes dos knobs e das teclas, os campos de visor, as legendas e o visor do preset.
- A versão aparece no fim do menu de presets.

## 0.2.0 — 2026-10-24
- Motor com WAVE (24 tabelas), OSC 3, ring, filtros COMB/VOWEL, saturação entre os filtros, LFO 3 e matriz de 6.
- 360 presets refeitos, com 8 famílias de som por categoria e volume nivelado.
- Tela toda à vista, com visores de onda, filtro, envelope e LFO, e peças com relevo.

## 0.1.0 — 2026-10-24
- Primeira versão: virtual-analógico polifônico de 16 vozes, leve no processador, com instalador próprio.
