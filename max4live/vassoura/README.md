# VASSOURA (Max for Live)

Limpa os trechos vazios dos clips de áudio do arranjo no Ableton Live. É pensada para stems de mix/master, que
chegam com o clip do começo ao fim da música mesmo onde não tem som.

Não é VST: um plugin só recebe áudio e não enxerga nem edita os clips. A VASSOURA é um **Max for Live Audio Effect**
(Live Suite, ou Standard com Max for Live), que lê as faixas pelo Live Object Model.

## Como funciona

- **Device no master**: THRESHOLD, MIN LENGTH, PRE, POST, MODE (MUTE/CUT), SNAP (OFF/1/16/BAR), SCOPE (ALL/SELECTED),
  ANALYZE, APPLY, OPEN ↗ e status. Todos são parâmetros do Live (salvos com o set).
- **OPEN** abre a tela completa numa janela do Max por cima do Live: barra com MODE/SNAP/SCOPE clicáveis, ANALYZE e
  APPLY, e uma linha por faixa (cor da faixa = som, vermelho escuro tracejado = vazio, cinza = mantido).
- **ANALYZE** lê o arquivo de cada clip de áudio do arranjo e guarda o pico em blocos de 10 ms. THRESHOLD, MIN LENGTH,
  PRE, POST e SNAP recalculam os vazios na hora, sem reler.
- **APPLY**: o LOM não tem "split", então para cada clip uma cópia de trabalho vai para depois do fim da música; para
  cada vazio ela é aparada pelos markers, mutada, **conferida** (tamanho exato, senão o trecho é pulado) e colada por
  cima do vazio com duplicate_clip_to_arrangement — o Live divide o original em volta. No CUT o pedaço colado é
  apagado (delete_clip). Ao fim, a cópia de trabalho é apagada e o set é relido.

## Estado — 0.2.0

- MUTE e CUT completos, com PRE/POST, SNAP e SCOPE. Testado num simulador do arranjo (	mp/teste-apply/sim.js,
  fora do git): som intacto e no mesmo lugar nos dois modos; **ainda não rodado no Live**.
- Sem FADE próprio: o LOM não expõe fade de clip; o Live põe fade automático nas bordas ("Create Fades on Clip Edges").
- Sem undo agrupado: o LOM não tem egin_undo_step, cada trecho é um Ctrl+Z. O CUT pede confirmação (2º clique).
- Clips com **WARP** ligado ou em **LOOP** ficam de fora.
## Arquivos

| Arquivo | O que é |
|---|---|
| `vassoura.js` | Motor ([v8]): lê faixas e clips pelo LOM, carrega o áudio num `buffer~`, acha os vazios e aplica MUTE/CUT |
| `vassoura_view.js` | Desenho da janela flutuante (`jsui`) |
| `build-vassoura.js` | Gera `Vassoura.amxd` + `Vassoura.maxpat` e copia os dois `.js` para a pasta de saída |

## Gerar e instalar

```
node build-vassoura.js <pasta-saida>     # sem argumento: ./dist (fora do git)
```

Copie a pasta de saída inteira para `User Library\Presets\Audio Effects\Max Audio Effect\` e arraste o
`Vassoura.amxd` para o master. Os `.js` precisam ficar na mesma pasta do `.amxd`. Se aparecer
"can't find vassoura.js", abra o device no editor do Max, clique em **FREEZE** e salve.

## Download e versões

O zip pronto (`.amxd` + `.js` + `LEIA-ME.txt`) fica no release `vassoura-vx.y.z`, em
[Releases](https://github.com/equalizedmusic/vsts/releases). Quem publica é o workflow `release-max4live.yml`: a versão
é o primeiro `## x.y.z` de [`novidades/Vassoura.md`](../../novidades/Vassoura.md). Para lançar, escreva a seção nova
lá em cima no mesmo commit da mudança. Esta pasta não entra no build dos VSTs.

Link fixo (sempre o zip da versão mais nova, pelo portal de licenças):
`https://isp-licencas.insidestudiosp.workers.dev/baixar/vassoura/mac` (o `/win` dá o mesmo zip). No painel, o pacote
**Vassoura** entra na mensagem ao cliente com esse link; o device não pede ativação.
