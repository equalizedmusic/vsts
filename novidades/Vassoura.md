# VASSOURA — novidades

Device Max for Live (pasta `max4live/vassoura`). A versão é o primeiro título `## x.y.z` deste arquivo:
ela vira o release `vassoura-vx.y.z` do GitHub (zip `Vassoura-x.y.z.zip`, com o texto da seção como nota), publicado
pelo workflow `release-max4live.yml`. Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit
da mudança. Fica só o release da última versão.

## 0.1.0 — 2026-10-31
- Protótipo **só de detecção**: não corta nem muta nada no set.
- Device pequeno no master (OPEN ↗, THRESHOLD, ANALYZE, status) + janela flutuante do Max por cima do Live, com uma
  linha por faixa: verde = tem som, vermelho tracejado = vazio.
- ANALYZE lê o arquivo de cada clip de áudio do arranjo (pico em blocos de 10 ms) e marca os trechos abaixo do
  THRESHOLD por mais que o MIN LENGTH. Clique no vermelho = manter o trecho; clique no nome = faixa fora da limpeza.
- Clips com WARP ligado ou em LOOP aparecem em cinza e não são analisados. PRE, POST, FADE, SNAP e SCOPE ainda não
  estão na tela. APPLY (CUT/MUTE) é a próxima fase.
- Feito para o Live Suite 12.4; **ainda não foi rodado no Live** — o console do Max imprime uma linha `VASSOURA:` por
  clip para conferir.
