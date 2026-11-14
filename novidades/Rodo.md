# RODO — novidades

Device Max for Live (pasta `max4live/rodo`), par da VASSOURA. A versão é o primeiro título `## x.y.z` deste arquivo:
ela vira o release `rodo-vx.y.z` do GitHub (zip `Rodo-x.y.z.zip`), publicado pelo workflow `release-max4live.yml`.
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.2.1 — 2026-11-15
- **Corrigido: o RODO pedia e-mail e token de novo toda vez que abria.** Em alguns computadores (biblioteca do
  Ableton fora da pasta do usuário, por exemplo em outro disco) a licença não ficava gravada. Agora ela é gravada na
  pasta do usuário achada de outros jeitos e conferida na hora; se mesmo assim o Max não conseguir gravar, a janela avisa.
- Corrigido: e-mail e token digitados chegavam ao servidor com a palavra "symbol" na frente e não ativavam.
- Quem já usa a 0.2.0 só precisa trocar os arquivos da pasta e ativar uma vez.

## 0.2.0 — 2026-11-14
- **Primeira versão à venda.** Normaliza o PICO de todos os clips de áudio do Arrangement pelo Clip Gain:
  TARGET de −12 a −0.3 dB; MODE CLIP (cada clip ao TARGET), TRACK (mesmo ajuste na faixa toda, mantém a dinâmica) e
  ALL (mesmo ajuste no set inteiro, mantém o balanço); SCOPE ALL/SELECTED; RESET volta os ganhos de antes.
- No APPLY um rodo atravessa as faixas junto com o ajuste real; atrás dele as ondas já estão no nível novo.
- Device no Live com a arte do RODO e um botão só, **OPEN THE MAGIC**; todos os controles na janela.
- Licença: e-mail + token na 1ª vez, até 2 computadores, 45 dias sem internet.
- (0.1.0 foi só o teste, sem release.)