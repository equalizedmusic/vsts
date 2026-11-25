# VASSOURA (Max for Live)

> **Versão atual: 0.3.8** (25/11/2026) · Max for Live (Ableton Live 12.2+) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Vassoura.md`](../../novidades/Vassoura.md)

![VASSOURA](https://insidestudiosp.com.br/img/vassoura.webp)

Limpa os trechos vazios dos clips de áudio do arranjo no Ableton Live. É pensada para stems de mix/master, que
chegam com o clip do começo ao fim da música mesmo onde não tem som.

Não é VST: um plugin só recebe áudio e não enxerga nem edita os clips. A VASSOURA é um **Max for Live Audio Effect**
(Live 12.2 ou mais novo, Suite ou Standard com Max for Live), que lê as faixas pelo Live Object Model.

É o 1º passo do fluxo do **PACOTÃO MIX & MASTER**: VASSOURA (limpa) → [RODO](../rodo/README.md) (nivela) →
[ESPANADOR](../espanador/README.md) (organiza em grupos e buses) → MAESTRO (mixa) → MASTER DOS MAGOS (finaliza).
Cada um funciona sozinho.

## Como funciona

- **Device na primeira faixa de áudio** (no master o ANALYZE não lê os clips): uma faixa com a arte da VASSOURA e um
  botão só, **OPEN THE MAGIC**, que abre a janela do Max por cima do Live.
- **Janela**: THRESHOLD, MIN LENGTH, PRE e POST em sliders na barra de cima, junto de MODE (MUTE/CUT), SNAP
  (OFF/1/16/BAR), SCOPE (ALL/SELECTED), ANALYZE e APPLY; uma linha por faixa (cor da faixa = som, vermelho escuro
  tracejado = vazio, cinza = mantido). Clique no vermelho = manter o trecho; clique no nome = faixa fora da limpeza.
  Os valores ficam salvos no set.

<img src="https://insidestudiosp.com.br/img/vassoura-janela.webp" width="900" alt="VASSOURA — janela">

<img src="https://insidestudiosp.com.br/img/vassoura-device.webp" width="420" alt="VASSOURA — device">

- **ANALYZE** lê o arquivo de cada clip de áudio do arranjo (o trecho que o clip realmente toca) e guarda o pico em
  blocos de 10 ms. Lê uma faixa por vez, cada arquivo uma vez só, devolvendo o controle ao Live entre uma e outra.
  THRESHOLD, MIN LENGTH, PRE, POST e SNAP recalculam os vazios na hora, sem reler.
- **PRE** (padrão 10 ms) e **POST** (padrão 100 ms) protegem o ataque e a cauda; **SNAP** põe os cortes na grade,
  sempre arredondando para dentro do silêncio.
- **APPLY**: o LOM não tem "split", então para cada clip uma cópia de trabalho vai para depois do fim da música; para
  cada vazio ela é aparada pelos markers, mutada, **conferida** (tamanho exato, senão o trecho é pulado) e colada por
  cima do vazio com `duplicate_clip_to_arrangement` — o Live divide o original em volta. No CUT o pedaço colado é
  apagado (`delete_clip`; pede um 2º clique no APPLY em até 4 s). Um trecho por vez; uma vassoura atravessa as faixas
  junto com o trabalho real e, no fim, "APPLIED ✓". A cópia de trabalho é apagada e a janela é atualizada sem reler os
  arquivos.

## Limites

- Sem FADE próprio: o LOM não expõe fade de clip; o Live põe fade automático nas bordas ("Create Fades on Clip Edges").
- Sem undo agrupado: o LOM não tem `begin_undo_step`, cada trecho é um Ctrl+Z. Salve uma cópia do set antes do
  primeiro APPLY.
- Clips com **WARP** ligado ou em **LOOP** e faixas congeladas ficam de fora.

## Licença e versão nova

- Na 1ª vez: **OPEN THE MAGIC** > e-mail + token da compra (`ISP-XXXX-…`) > **ACTIVATE** (precisa de internet). Até 2
  computadores, 45 dias sem internet; **LICENSE**, no canto da janela, mostra a licença e tem DEACTIVATE THIS COMPUTER.
  Quem tinha a VASSOURA grátis (até a 0.2.x) recebeu o token no e-mail do cadastro.
- Mesmo servidor e protocolo dos VSTs (pacote `vassoura`). A licença é gravada na pasta do usuário e numa pasta fixa
  (Windows `C:\Users\Public\Documents`, Mac `/Users/Shared`). No Live 12.2/12.3 (Max sem `XMLHttpRequest` no [v8]) a
  conversa com o servidor passa por um [js] clássico (`vassoura_http.js`).
- Versão nova: a janela e o device mostram **NEW VERSION x.y.z · DOWNLOAD**. Só vão a versão e o sistema.
- Log: a cada ANALYZE a VASSOURA grava `VASSOURA-log.txt` na pasta do device (o APPLY continua no mesmo arquivo).

## Arquivos

| Arquivo | O que é |
|---|---|
| `vassoura.js` | Motor ([v8]): lê faixas e clips pelo LOM, carrega o áudio num `buffer~`, acha os vazios e aplica MUTE/CUT |
| `vassoura_view.js` | Desenho da janela (`jsui`) |
| `build-vassoura.js` | Gera `Vassoura.amxd` + `Vassoura.maxpat` e os `.js` na pasta de saída |
| `LEIA-ME.txt` | O passo a passo que vai no zip |
| `../_comum/` | Comum aos três devices: licença (`isp_licenca.js`, `isp_tela_lic.js`), faixa do device (`isp_strip.js` → `vassoura_strip.js`), internet no Live 12.2/12.3 (`isp_http.js` → `vassoura_http.js`) e o patcher (`m4l_comum.js`) |

## Gerar e instalar

```
node build-vassoura.js <pasta-saida>     # sem argumento: ./dist (fora do git)
```

Copie a pasta de saída inteira para `User Library\Presets\Audio Effects\Max Audio Effect\` e arraste o
`Vassoura.amxd` para a primeira faixa de áudio do projeto. Os `.js` precisam ficar na mesma pasta do `.amxd`
("v8: can't find vassoura.js" = não estão).

## Download e versões

O zip pronto (`Vassoura-x.y.z.zip`: `.amxd` + `.js` + `LEIA-ME.txt`) fica no release `vassoura-vx.y.z`, em
[Releases](https://github.com/equalizedmusic/vsts/releases) (fica só o da última versão). Quem publica é o workflow
`release-max4live.yml`: a versão é o primeiro `## x.y.z` de [`novidades/Vassoura.md`](../../novidades/Vassoura.md).
Para lançar, escreva a seção nova lá em cima no mesmo commit da mudança. Esta pasta não entra no build dos VSTs.

Link fixo (sempre o zip da versão mais nova, pelo portal de licenças):
`https://insidestudiosp.com.br/baixar/vassoura/zip`.
