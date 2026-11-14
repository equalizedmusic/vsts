# VASSOURA — novidades

Device Max for Live (pasta `max4live/vassoura`). A versão é o primeiro título `## x.y.z` deste arquivo:
ela vira o release `vassoura-vx.y.z` do GitHub (zip `Vassoura-x.y.z.zip`, com o texto da seção como nota), publicado
pelo workflow `release-max4live.yml`. Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit
da mudança. Fica só o release da última versão.

## 0.3.0 — 2026-11-14
- **Visual novo.** O device no Live virou uma faixa com a arte da VASSOURA e um botão só: **OPEN THE MAGIC**. Todos
  os controles ficam na janela: THRESHOLD, MIN LENGTH, PRE e POST viraram sliders na barra de cima, junto de MODE,
  SNAP, SCOPE, ANALYZE e APPLY. Os valores continuam salvos no set.
- **Animação no APPLY:** uma vassoura atravessa as faixas junto com o trabalho real; atrás dela os trechos vazios
  já saíram (MUTE apaga, CUT some). No fim, "APPLIED ✓".
- **Licença.** A partir desta versão a VASSOURA é paga e pede ativação na 1ª vez (e-mail + token, até 2
  computadores, 45 dias sem internet). **Quem já tinha a VASSOURA recebe a atualização grátis**: o token foi enviado
  para o e-mail do cadastro. LICENSE, no canto da janela, mostra a licença e libera o computador.
- Aviso de versão nova agora na janela e no device (botão NEW VERSION · DOWNLOAD).
## 0.2.3 — 2026-11-10
- **Clips aparados no arranjo agora entram na limpeza.** Quando o stem é uma gravação longa cortada no arranjo, o Live
  deixa o marcador de fim no fim do arquivo. A VASSOURA lia o trecho errado: os vazios apareciam fora do lugar na
  janela, o console mostrava "(?)" e o APPLY pulava o trecho ("cópia não aparou certo"). Agora ela usa o trecho que
  o clip realmente toca. Em clips já exportados inteiros nada muda.
- **ANALYZE lê uma faixa por vez** e devolve o controle ao Live entre uma e outra. Antes, todas as faixas e clips
  eram lidos de uma vez só, o que podia fechar o Live em set grande. Faixas e clips são lidos com um objeto só do Live.
- **ANALYZE e APPLY não param mais no meio.** O Max podia descartar a tarefa agendada para a próxima faixa antes de
  ela rodar, e o ANALYZE ficava parado (relato: parava na faixa 7, sem erro). Agora a VASSOURA guarda as tarefas e
  confere a cada segundo se a sequência andou; se ficou parada mais de 3 s, retoma de onde estava e registra no log.
- **Log em arquivo**: a cada ANALYZE a VASSOURA grava `VASSOURA-log.txt` na pasta do device, com cada faixa, clip e
  arquivo lido, e o APPLY continua no mesmo arquivo. Se o Live fechar, a última linha mostra onde parou. O console do
  Max diz onde o log está; se não der para gravar, diz o motivo e manda os detalhes para o próprio console.
- O áudio da faixa onde o device fica passava só por um canal (aviso "patchcord outlet out of range" no console).
  Agora passa em estéreo.

## 0.2.2 — 2026-11-09
- **Correção de travamento em set grande** (relato: Windows, Live 12.3, mais de 30 faixas no ANALYZE e APPLY em CUT
  fechando o Live):
  - O ANALYZE lê **cada arquivo uma vez só**, mesmo que vários clips usem o mesmo arquivo, e faz uma pausa curta entre
    um arquivo e outro. No fim, a memória do último arquivo é devolvida.
  - Depois do APPLY a janela é atualizada **sem reler os arquivos**. Antes, depois de um CUT, cada pedaço novo fazia o
    arquivo inteiro ser lido de novo, o que podia dar centenas de leituras.
  - O APPLY faz **um trecho por vez** e devolve o controle ao Live entre um trecho e outro. Ele também deixou de criar
    um objeto do Live para cada clip da faixa a cada busca.
- O console do Max mostra só os clips com problema, e não mais uma linha por clip.
- **Aviso de versão nova**: alguns segundos depois de o set abrir, a VASSOURA pergunta ao site se há versão mais nova.
  Se houver, o device mostra "NOVA VERSÃO x.y.z" e a janela ganha os botões **BAIXAR** (abre o download do zip) e
  **O QUE MUDOU**. Sem internet, nada aparece e o device funciona normalmente. Nenhum dado do set é enviado; só o
  número da versão e o sistema (Windows ou Mac).
- A janela mostrava "0.2.1" no título; agora mostra a versão certa.

## 0.2.1 — 2026-11-05
- **LEIA-ME corrigido**: o device vai na **primeira faixa de áudio** do projeto (no master o ANALYZE não lê os clips).
  O funcionamento é o mesmo da 0.2.0.

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
