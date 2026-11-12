# LIMÃO — novidades

Pacote do instalador: `InsideStudioSP-Limao`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `limao-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.6.1 — 2026-11-12
- Pré-lançamento de TESTE (ainda não vai para os clientes).
- AUTO ALEATÓRIO respeita a linha que você montou: usa as MESMAS notas e a mesma quantidade de pausas, ligaduras,
  accents e slides, só que em outra ordem. Uma linha com poucas notas e muitas pausas e slides continua assim, sem
  virar uma linha cheia. SCALE e ROOT não mudam, e nenhuma nota nova é sorteada. Se você editar a linha com o AUTO
  ligado, a próxima troca já parte da edição. A tecla ALEATORIZAR continua sorteando uma linha nova inteira.

## 0.6.0 — 2026-11-11
- AUTO ALEATÓRIO no sequenciador: ligado, troca a linha inteira (notas, pausas, ligaduras, acentos e slides) sem
  parar, a cada 4, 8, 16, 32 ou 64 tempos (A CADA). A troca cai sempre na virada do bloco, junto com o compasso,
  e respeita PASSOS, SCALE e ROOT.
- ARRASTAR MIDI: arraste a tecla para o DAW e a linha vira clipe MIDI, igual ao RESENHA (com SCALE + ROOT, acento =
  velocity 127, slide = nota encostando na próxima, swing e andamento no arquivo).

## 0.5.0 — 2026-10-27
- Ativação por licença: na primeira vez o plugin pede o e-mail e o token da licença. O mesmo token vale em até
  2 computadores, e desativar num computador libera a vaga para outro. Sem ativar, o instrumento fica mudo.
- Com a licença ativa, aparece no pé da janela o aviso de versão nova do pacote (BAIXAR / Agora não).

## 0.4.1 — 2026-10-26
- Sombra dos knobs redonda e macia: sumiu o canto quadrado embaixo/à direita de cada knob.

## 0.4.0 — 2026-10-25
- Visual de hardware: knobs Moog renderizados, laterais de nogueira e teclas de verdade; a tecla do passo que
  está tocando acende.
- Desinstalador: no Windows, atalho "Desinstalar" no menu Iniciar (além de Aplicativos instalados); no Mac,
  "Desinstalar InsideStudioSP Limao.command" dentro do .dmg. Os presets do usuário ficam.

## 0.3.0 — 2026-10-24
- SCALE e ROOT no sequenciador:
  - as notas dos passos tocam dentro da escala escolhida (13 escalas, ou CHROMATIC para tocar como estão);
  - o visor mostra a nota que vai tocar;
  - arrastar, a rodinha e o −1 / +1 andam um grau da escala;
  - ALEATORIZAR gera a linha na escala e na tônica escolhidas.
- Menu de presets: com a janela grande, a lista de dentro (banco → categoria → preset) abria por baixo da
  janela do plugin e não aceitava clique. Agora ela abre dentro da janela, e as listas dos campos também.
- A versão aparece no fim do menu de presets.

## 0.2.0 — 2026-10-24
- Sem estalo nas notas com ACC e com slide, e ao girar o DRIVE.
- 360 presets em 9 categorias, presets do usuário (USER) e bancos `.lmobank` para importar.
- Knob com a escala centrada.

## 0.1.0 — 2026-10-24
- Primeira versão: bassline ácido mono com sequenciador de 32 passos e instalador próprio.
