# CHUTE — novidades

Pacote do instalador: `InsideStudioSP-Chute`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `chute-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.2.3 — 2026-11-26
- **Ativação sem internet:** para o computador do estúdio que não fica na internet (ou nem tem placa de rede). Na tela
  de ativação, clique em **SEM INTERNET?** → **SALVAR CÓDIGO** e leve o arquivo num pen drive a um computador com internet;
  em **insidestudiosp.com.br/offline** digite o e-mail e o token e envie o arquivo. Volte com o arquivo **.isplic** e clique
  em **CARREGAR LICENÇA**: todos os seus plugins VST3/AU ficam ativados de uma vez nesse computador, sem vencer e sem
  precisar de internet. Trocou a placa-mãe ou formatou? Gere um arquivo novo.
- O som e as funções não mudam.

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

## 0.2.0 — 2026-11-20
- Versão oficial, à venda no site (R$ 49,90; quem entrou na lista de pré-venda ou já é cliente InsideStudioSP paga
  R$ 24,90 até 27/11 com o mesmo e-mail). O som, os presets e as telas são os da 0.1.3, aprovada no teste.
- Ativação por licença: na primeira vez o plugin pede o e-mail e o token da licença. O mesmo token vale em até 2
  computadores, e desativar num computador libera a vaga para outro. Sem ativar, o CHUTE fica mudo e não aparece no
  LINK.
- Menu ≡ > Licença… mostra a licença ativa.
- Para o LINK: ALIGN 0.3.1 e BAREBASS 0.4.1 (grátis) saíram como versões oficiais junto com esta.

## 0.1.3 — 2026-11-19
- Ainda TESTE (sem licença, fora do site). O som dos 50 presets não muda (conferido amostra por amostra).
- BANCOS DE PRESETS: os presets agora vêm de bancos (arquivo .chutebank). Os 50 de fábrica (SUB, PUNCH, TIGHT,
  BOOM) continuam dentro do plugin; bancos novos chegam num instalador só de presets (InsideStudioSP-ChuteBancos)
  e aparecem no menu, depois dos de fábrica, na próxima vez que você abrir o DAW. Sem atualizar o plugin.
- Um preset de banco pode trazer os knobs dele (CLICK, DRIVE, TONE…), não só o kick.
- O projeto guarda o preset pelo nome (banco + preset): abre no mesmo kick mesmo com outros bancos instalados.
## 0.1.2 — 2026-11-18
- Ainda TESTE (sem licença, fora do site).
- DRIVE: girar o knob não faz mais o ruído de "liga e desliga". O knob anda suave e a saturação entra aos poucos
  nos primeiros 10%. No FOLD, a compensação de volume pulava a cada dobra da onda.
- DRIVE menos agressivo nos três modos: até +18 dB (antes +30; +12 no FOLD) numa curva que cresce devagar no
  começo. O HARD virou um corte de joelho firme, sem a quina seca que deixava o som áspero.
- TONE agora age só na saturação: ele fecha o agudo do que o DRIVE acrescenta. Com DRIVE em 0 não muda nada, e o
  click do preset fica intacto.
- MASTER: saiu o seletor CHOKE / POLY. Kick é mono: a batida nova sempre corta a anterior em 3 ms.
- LINK mais firme (vale para o ALIGN 0.3.1 e o BAREBASS 0.4.1):
  - cada batida recebe a posição uma vez e depois só envelhece bloco a bloco, como o som do kick;
  - volta do loop, pulo na régua, faixa do kick um bloco na frente e exportação não fazem mais a batida sumir;
  - a ligação só cai depois de 20 s sem áudio do CHUTE (antes caía com 1,5 s e o LINK "piscava").

## 0.1.1 — 2026-11-18
- Ainda TESTE (sem licença, fora do site).
- Tela: o corpo (verde) volta a aparecer na batida desenhada do KICK · NOW (na 0.1.0 só o ataque aparecia).
- Curva de pitch desenhada suave (a medida anda em degraus de meio ciclo; o som não muda).

## 0.1.0 — 2026-11-18
- Primeira versão do CHUTE, kick designer. TESTE (sem licença, fora do site).
- Cada nota MIDI toca uma batida: CLICK (o ataque do preset) + BODY (o corpo do kick, afinável).
- 50 presets de fábrica em 4 bancos (SUB, PUNCH, TIGHT, BOOM), com nomes de golpe de luta, e a nota de cada um em
  cifra no menu (C1, D#1, F#1…). Mexeu em qualquer knob, o nome ganha `*`; SAVE AS grava como preset novo no banco
  USER. ◀ ▶ passam pelos presets (fábrica e USER). Duplo clique num knob volta ao valor do preset.
- CLICK: LEVEL, TONE e LENGTH (2 a 25 ms, com crossfade para o corpo), com zoom nos primeiros 20 ms.
- BODY: a nota em três modos — KEY · MIDI (afina na nota que você toca), FIXED (a nota do preset, afinada) e PRESET
  (a afinação original); TUNE (±12 semitons), PITCH (onde a queda começa), DROP (velocidade da queda), DECAY
  (comprimento) e TAIL (quanto sobra no fim).
- DRIVE (WARM, HARD, FOLD) com TONE; MASTER com CHOKE / POLY, VOLUME e VELO.
- Tela: a batida do preset desenhada (ataque laranja, corpo verde, curva de pitch tracejada), PITCH ENV, AMP ENV e
  o medidor de saída.
- LINK: cada batida vai para o ALIGN e o BAREBASS do mesmo projeto, sem sidechain (ALIGN 0.3.0 e BAREBASS 0.4.0).
- Latência zero.
