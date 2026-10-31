# VASSOURA — novidades

Device Max for Live (pasta `max4live/vassoura`). A versão é o primeiro título `## x.y.z` deste arquivo:
ela vira o release `vassoura-vx.y.z` do GitHub (zip `Vassoura-x.y.z.zip`, com o texto da seção como nota), publicado
pelo workflow `release-max4live.yml`. Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit
da mudança. Fica só o release da última versão.

## 0.2.0 — 2026-10-31
- **APPLY completo** — a VASSOURA agora mexe no set:
  - **MUTE**: divide o clip e desliga (Clip Activator) só os pedaços vazios. Nada é apagado; qualquer pedaço volta
    ligando o clip no arranjo.
  - **CUT**: apaga os pedaços vazios. Pede um 2º clique no APPLY em até 4 s para confirmar.
  - Trava de segurança: cada pedaço é conferido (tamanho exato) antes de ir para o arranjo; se não bater, aquele trecho
    é pulado e aparece no console. Clip que mudou desde o ANALYZE também é pulado.
  - Depois do APPLY o set é relido e a janela mostra como ficou; o resultado fica no topo da janela.
- **PRE** (folga antes do som, padrão 10 ms) e **POST** (cauda depois do som, padrão 100 ms): não cortam ataque nem
  reverb. Na borda do clip não se aplica folga.
- **SNAP** OFF / 1/16 / BAR: os cortes caem na grade, sempre arredondando para dentro do silêncio (nunca invade o som).
- **SCOPE** ALL / SELECTED (só a faixa selecionada no Live).
- **Ajuste ao vivo**: THRESHOLD, MIN LENGTH, PRE, POST e SNAP redesenham a janela na hora, sem reler os arquivos. Os
  trechos marcados para manter continuam mantidos.
- Device maior: dials THRESHOLD, MIN LENGTH, PRE, POST; seletores MODE, SNAP, SCOPE; botões ANALYZE e APPLY.
- Janela: barra com MODE / SNAP / SCOPE clicáveis, ANALYZE e APPLY (azul no MUTE, vermelho no CUT), status ao vivo.
- Limites do Live: não há fade por device (use "Create Fades on Clip Edges" nas Preferences) e não há undo agrupado —
  cada trecho é um Ctrl+Z. **Salve uma cópia do set antes do primeiro APPLY.**
## 0.1.2 — 2026-10-31
- Visual mais legível (pedido depois do 1º teste, que mostrou tudo pequeno demais):
- **Janela maior** (1360×840) e **letras maiores**: nomes em 11–14 px, linhas de no mínimo 20 px (cabem ~35 faixas;
  o resto rola pela barra ou pela roda do mouse), régua com o número do compasso em destaque.
- **Cor de cada faixa igual à do Live** nos trechos com som, e uma tarja da cor ao lado do nome. O vazio fica vermelho
  escuro tracejado, para o som se destacar (antes o vermelho dominava a tela).
- **Nomes curtos**: o começo comum dos stems (ex.: `mel rose - neural collapse inteira - 140 Fm`) sai das linhas e
  aparece uma vez em STEMS, em cima da lista. Cada linha mostra também o **% vazio** da faixa.
- **Zoom no trecho com clips**: a régua começa no compasso do primeiro clip, sem a área vazia do início do set.
- **Mouse por cima** de uma faixa destaca a linha e mostra no rodapé o nome inteiro, quantos vazios e o % vazio.
- Borda em cada clip, para ver onde um clip termina e o outro começa; legenda das cores no cabeçalho.

## 0.1.1 — 2026-10-31
- Primeiro teste real (Live Suite 12.4.16, 81 faixas de stems): a detecção rodou (465 vazios, 77% do tempo dos
  clips). Correções da janela:
- **Todas as faixas acessíveis**: antes só apareciam as 41 primeiras. Agora a altura da linha encolhe para caber, e
  o que não couber rola pela barra à direita ou pela roda do mouse. O rodapé mostra "FAIXAS x–y DE N".
- **Nomes legíveis**: se o nome não cabe, o corte é no MEIO e o fim fica visível (ex.: `1-me…140 Fm 80 DOWN 1`),
  porque os stems costumam ter o mesmo começo.
- Janela com título **VASSOURA** (antes `[janela]`) e tamanho fixo igual ao do desenho (sem a faixa cinza sobrando).

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
