# QUIMERA — novidades

Pacote do instalador: `InsideStudioSP-Quimera`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `quimera-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.5.2 — 2026-11-25
- **Atualização automática:** o aviso de versão nova no pé da janela ganha o botão **ATUALIZAR**: o plugin baixa o
  instalador, confere a assinatura e o arquivo e instala sozinho. No Windows ele espera você fechar o DAW e roda o
  instalador (o Windows pede permissão uma vez); no Mac pede a senha de administrador e instala na hora. Depois é só
  abrir o DAW de novo. O **BAIXAR** pelo navegador continua lá para quem preferir.
- Vale a partir da próxima versão: esta ainda se instala pelo aviso de sempre.
- O som e as funções não mudam.

## 0.5.1 — 2026-11-24
- **Ativação automática:** se você já ativou outro plugin ou device InsideStudioSP neste computador, com o mesmo e-mail, este
  se ativa sozinho ao abrir, sem digitar o token. Se o seu e-mail não tem este pacote, a tela de ativação já abre com o
  e-mail preenchido e avisa. Desativar continua valendo: o plugin não se reativa sozinho depois.
- Aviso de autoria na tela da licença (© Rogerio Cardoso / InsideStudioSP).
- O som e as funções não mudam.

## 0.5.0 — 2026-10-27
- Ativação por licença: na primeira vez o plugin pede o e-mail e o token da licença. O mesmo token vale em até
  2 computadores, e desativar num computador libera a vaga para outro. Sem ativar, o instrumento fica mudo.
- Com a licença ativa, aparece no pé da janela o aviso de versão nova do pacote (BAIXAR / Agora não).
- O instalador agora se chama `InsideStudioSP-Quimera` (era `InsideStudioSP-Instrumentos`). No Windows ele
  substitui o "InsideStudioSP Instrumentos" já instalado.

## 0.4.1 — 2026-10-26
- Sombra dos knobs, LEDs e lâmpadas redonda e macia: sumiu o canto quadrado embaixo/à direita de cada knob.

## 0.4.0 — 2026-10-25
- Visual de hardware: knobs Moog renderizados, LEDs, lâmpadas piloto e teclas de verdade.
- Desinstalador: no Windows, atalho "Desinstalar" no menu Iniciar (além de Aplicativos instalados); no Mac,
  "Desinstalar InsideStudioSP Instrumentos.command" dentro do .dmg. Os presets do usuário ficam.

## 0.3.0 — 2026-10-24
- Arpejador: aba ARP nova, com 9 modos (Up, Down, Up/Down, Down/Up, Random, As Played, Chord, Converge e Diverge),
  RATE no tempo do projeto, OCTAVES (1–4), GATE (100% emenda as notas; com o VOICE em Legato vira slide), SWING,
  LENGTH (1–16) e ACCENT.
- Os 16 passos têm liga/pausa e acento próprios, com o passo que está tocando aceso, e há 8 padrões prontos.
- SCALE e ROOT: as notas do arpejo ficam dentro da escala escolhida (13 escalas, ou CHROMATIC para deixar como
  tocado). Com uma tecla só, o arpejo sobe pela escala a partir dela.
- SEGURAR trava o acorde do arpejo.
- Categoria ARP no menu de presets, com 40 presets já com o arpejo ligado.
- Menu de presets: a lista de dentro (banco → categoria → preset) abria por baixo da janela do plugin quando ela
  estava grande e não aceitava clique. Agora ela abre dentro da janela, e as listas dos seletores também.
- A versão aparece no fim do menu de presets.

## 0.2.0 — 2026-10-23
- 40 presets por categoria (361 no FACTORY 1).
- Bancos avulsos `.qmrbank`, com importar e remover pelo menu de presets.
- Acabamento de hardware.

## 0.1.0 — 2026-10-19
- Primeira versão: 3 osciladores wavetable, sub e ruído, 2 filtros, 3 envelopes, 2 LFOs, matriz de 16, MPE e
  8 efeitos.
