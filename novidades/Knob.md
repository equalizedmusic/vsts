# SÉRIE KNOB — novidades

Pacote do instalador: `InsideStudioSP-Knob` (FOLE, PICADOR, MARÉ, VAIVÉM, REPIQUE, ECO, AVESSO, FREIO, ESCADA,
MASTIGA). A versão é o primeiro título `## x.y.z` deste arquivo: ela entra nos plugins, no instalador e no release
`knob-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.3.4 — 2026-11-26
- **Ativação sem internet:** para o computador do estúdio que não fica na internet (ou nem tem placa de rede). Na tela
  de ativação, clique em **SEM INTERNET?** → **SALVAR CÓDIGO** e leve o arquivo num pen drive a um computador com internet;
  em **insidestudiosp.com.br/offline** digite o e-mail e o token e envie o arquivo. Volte com o arquivo **.isplic** e clique
  em **CARREGAR LICENÇA**: todos os seus plugins VST3/AU ficam ativados de uma vez nesse computador, sem vencer e sem
  precisar de internet. Trocou a placa-mãe ou formatou? Gere um arquivo novo.
- O som e as funções não mudam.

## 0.3.3 — 2026-11-25
- **Atualização automática:** o aviso de versão nova no pé da janela ganha o botão **ATUALIZAR**: o plugin baixa o
  instalador, confere a assinatura e o arquivo e instala sozinho. No Windows ele espera você fechar o DAW e roda o
  instalador (o Windows pede permissão uma vez); no Mac pede a senha de administrador e instala na hora. Depois é só
  abrir o DAW de novo. O **BAIXAR** pelo navegador continua lá para quem preferir.
- Vale a partir da próxima versão: esta ainda se instala pelo aviso de sempre.
- O som e as funções não mudam.

## 0.3.2 — 2026-11-24
- **Ativação automática:** se você já ativou outro plugin ou device InsideStudioSP neste computador, com o mesmo e-mail, este
  se ativa sozinho ao abrir, sem digitar o token. Se o seu e-mail não tem este pacote, a tela de ativação já abre com o
  e-mail preenchido e avisa. Desativar continua valendo: o plugin não se reativa sozinho depois.
- Aviso de autoria na tela da licença (© Rogerio Cardoso / InsideStudioSP).
- O som e as funções não mudam.

## 0.3.1 — 2026-11-08
- Instalador do Mac: instala sempre para todos os usuários, em /Library/Audio/Plug-Ins (saiu a pergunta "todos os usuários / só para mim"). Ele também apaga a cópia antiga que tenha ficado na pasta do usuário: com as duas, o DAW podia continuar abrindo a versão velha.
- As funções, o som e os presets não mudaram.

## 0.3.0 — 2026-10-27
- Ativação por licença: na primeira vez o plugin pede o e-mail e o token da licença. O mesmo token vale em até
  2 computadores, e desativar num computador libera a vaga para outro. Sem ativar, o plugin passa o som direto,
  sem processar.
- Com a licença ativa, aparece no pé da janela o aviso de versão nova do pacote (BAIXAR / Agora não).

## 0.2.1 — 2026-10-26
- Sombra do knob grandão, dos knobs RATE e MIX e da alavanca redonda e macia: sumiu o canto quadrado
  embaixo/à direita.

## 0.2.0 — 2026-10-25
- A janela mostra só o pedal: saiu o cinza do estúdio em volta da caixa.
- Desinstalador: no Windows, atalho "Desinstalar" no menu Iniciar (além de Aplicativos instalados); no Mac,
  "Desinstalar InsideStudioSP Knob.command" dentro do .dmg. Os presets do usuário ficam.

## 0.1.0 — 2026-10-25
- Primeira versão da SÉRIE KNOB, com 10 pedais de um knob grandão:
  - FOLE (sidechain), PICADOR (trance gate), MARÉ (filtro), VAIVÉM (auto-pan), REPIQUE (stutter), ECO (delay),
    AVESSO (reverso), FREIO (tape stop), ESCADA (pitch) e MASTIGA (bitcrush).
- Tudo no tempo do DAW. Com o DAW parado, o pedal continua andando no último BPM.
- Em todos os pedais:
  - TRIGGER: INT (segue o DAW), MIDI (cada nota recomeça o movimento) ou ÁUDIO (cada ataque do sidechain recomeça);
  - RATE: a divisão do tempo;
  - SHAPE: a forma do movimento, que aparece no visor;
  - MIX: a mistura com o som original.
- Visual de pedal de verdade, com laterais de nogueira, chapa de aço escovado, lâmpadas que respiram com o efeito e
  knob usinado renderizado em 3D.
