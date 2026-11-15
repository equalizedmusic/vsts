# RODO — novidades

Device Max for Live (pasta `max4live/rodo`), par da VASSOURA. A versão é o primeiro título `## x.y.z` deste arquivo:
ela vira o release `rodo-vx.y.z` do GitHub (zip `Rodo-x.y.z.zip`), publicado pelo workflow `release-max4live.yml`.
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.3.3 — 2026-11-15
- **Corrigido: depois de DEACTIVATE, o RODO podia voltar a liberar ao reabrir o set.** Quem veio de uma versão
  anterior à 0.3.2 ainda tinha o arquivo de licença antigo no computador. Agora o DEACTIVATE apaga também esse arquivo.
  Quem não desativa nada não percebe diferença; o token é o mesmo de antes.

## 0.3.2 — 2026-11-15
- **Corrigido de vez: o RODO pedia e-mail e token toda vez que o set era reaberto.** O Max não estava deixando
  gravar o arquivo da licença. Agora a licença é gravada de dois jeitos diferentes, e o computador é reconhecido
  sempre como o mesmo: mesmo no pior caso, ativar de novo não gasta outra vaga. O token é o mesmo de antes.

## 0.3.1 — 2026-11-15
- **Corrigido: o RODO voltava a pedir e-mail e token ao reabrir o set.** Quando o device era aberto de dentro do set,
  fora da pasta do usuário, a licença não era encontrada e cada abertura contava como um computador novo. Agora a
  licença também fica gravada numa pasta fixa do sistema (Windows: `C:\Users\Public\Documents`; Mac: `/Users/Shared`),
  que não depende de onde o device está. O token é o mesmo de antes.

## 0.3.0 — 2026-11-14
- **SENSOR: nivela por PEAK, RMS ou LUFS.** O ANALYZE lê cada arquivo uma vez só e mede os três; trocar de SENSOR
  muda a janela na hora, sem reler. RMS ignora o silêncio (abaixo de −60 dB); LUFS segue a ITU-R BS.1770 / EBU R128
  (ponderação K e gates de −70 LUFS e −10 LU). Cada SENSOR tem o seu TARGET: PEAK −12 a −0.3 dB, RMS −36 a −6 dB e
  LUFS −36 a −6 LUFS (os dois com padrão −18).
- **PEAK GUARD** (em RMS e LUFS, vem ligado): o ganho para quando o pico do clip chega a −0.3 dB, para nivelar pelo
  volume sem clipar. A faixa segurada aparece com ▲ e, depois do APPLY, com "guard −X" (o que faltou).
- Em RMS e LUFS, o MODE TRACK mede a faixa inteira junta (não só o clip mais alto) e o ALL leva a faixa mais alta ao TARGET.
- Janela com a barra em duas linhas: SENSOR, TARGET e PEAK GUARD em cima; MODE, SCOPE e ANALYZE/APPLY/RESET embaixo.
- PEAK continua o padrão e funciona igual à 0.2.1: sets salvos abrem como estavam. Atualização grátis, mesma licença.

## 0.2.1 — 2026-11-14
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