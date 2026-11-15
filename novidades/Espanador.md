# ESPANADOR — novidades

Device Max for Live (pasta `max4live/espanador`), da família da VASSOURA e do RODO. A versão é o primeiro título
`## x.y.z` deste arquivo: ela vira o release `espanador-vx.y.z` do GitHub (zip `Espanador-x.y.z.zip`), publicado pelo
workflow `release-max4live.yml`. Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.2.0 — 2026-11-15
Primeira versão à venda (a 0.1 foi só de teste).
- **Organiza o set nos 8 grupos do MAESTRO** (KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX). O SCAN lê todas
  as faixas e o AUTO já marca cada uma pelo nome, com as mesmas palavras que o MAESTRO usa. Você confere e corrige
  na janela: clique, Shift+clique ou arrastar para marcar, e o botão do grupo para escolher.
- **APPLY de uma vez:** pinta faixas e clips (arranjo e session) com a cor do grupo, põe o nome do grupo na frente
  (PREFIX, opcional) e cria um **BUS** por grupo, mandando a saída das faixas para ele.
- **MAESTRO em todo bus:** crie uma faixa de áudio chamada MAESTRO, com o MAESTRO dentro e sem clips. Cada bus nasce
  como cópia dela, já com o plugin, e no fim do APPLY essa faixa-molde é apagada.
- **Reconhece o que já existe:** faixa KICK ou BASS com clips entra no grupo (o bus vira "KICK BUS"/"BASS BUS"),
  bus vazio com o nome do grupo é reaproveitado e Group Track com faixas de um grupo só vai inteiro para o bus.
- **UNDO** volta nome, cor, saída e cor dos clips, apaga os buses criados (os que ainda estão vazios) e devolve a
  faixa MAESTRO.
- O Loop dos clips não muda ao pintar (no Live, pintar um clip podia ligar o Loop dele).
- Licença: e-mail e token da compra, como no RODO e na VASSOURA.
