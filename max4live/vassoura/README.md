# VASSOURA (Max for Live)

Limpa os trechos vazios dos clips de áudio do arranjo no Ableton Live. É pensada para stems de mix/master, que
chegam com o clip do começo ao fim da música mesmo onde não tem som.

Não é VST: um plugin só recebe áudio e não enxerga nem edita os clips. A VASSOURA é um **Max for Live Audio Effect**
(Live Suite, ou Standard com Max for Live), que lê as faixas pelo Live Object Model.

## Como funciona

- **Device no master**, pequeno: OPEN ↗, THRESHOLD, ANALYZE e status.
- **OPEN** abre a tela completa numa janela do Max que fica por cima do Live. Uma linha por faixa com o clip inteiro:
  cor da faixa = tem som, vermelho escuro tracejado = vazio.
- **ANALYZE** lê o arquivo de cada clip de áudio do arranjo (pico em blocos de 10 ms) e marca os trechos abaixo do
  THRESHOLD por mais que o MIN LENGTH. Só lê: nada muda no set.

## Estado — 0.1 (protótipo, só detecção)

- Ainda **não corta nem muta**: o APPLY (CUT/MUTE) é a próxima fase.
- Clips com **WARP** ligado ou em **LOOP** aparecem em cinza e não são analisados.
- PRE, POST, FADE, SNAP e SCOPE ainda não estão na tela.
- Feito para o Live 12 (alvo: Live Suite 12.4.16). **Ainda não foi rodado no Live.**

## Arquivos

| Arquivo | O que é |
|---|---|
| `vassoura.js` | Motor ([v8]): percorre faixas e clips pelo LOM, carrega o áudio num `buffer~` e acha os vazios |
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
