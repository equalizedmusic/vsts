# ESPANADOR (Max for Live)

> **Versão atual: 0.2.1** (18/11/2026) · Max for Live (Ableton Live 12.2+) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Espanador.md`](../../novidades/Espanador.md)

![ESPANADOR](https://insidestudiosp.com.br/img/espanador.webp)

Organiza o set do Ableton Live nos **8 grupos do MAESTRO** (KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX):
pinta faixas e clips com a cor do grupo e cria um **BUS** por grupo, já com o MAESTRO dentro.

É um **Max for Live Audio Effect** (Live 12.2 ou mais novo, Suite ou Standard com Max for Live), da família da
[VASSOURA](../vassoura/README.md) e do [RODO](../rodo/README.md). É o 3º passo do fluxo do **PACOTÃO MIX & MASTER**:
VASSOURA (limpa) → RODO (nivela) → ESPANADOR (organiza) → MAESTRO (você só regula o MAESTRO de cada bus) → MASTER DOS
MAGOS (finaliza no master). Cada um funciona sozinho.

## Como funciona

- **Device na primeira faixa de áudio** (não no master): a arte do ESPANADOR e um botão só, **OPEN THE MAGIC**, que
  abre a janela do Max por cima do Live. Todos os controles ficam na janela.
- **Faixa MAESTRO (opcional)**: uma faixa de áudio chamada MAESTRO (ou MOLDE), com o plugin MAESTRO (e o que mais
  você quiser em todo bus) e sem clips. Cada bus nasce como cópia dela; no fim do APPLY ela é apagada (o UNDO
  devolve). Sem ela, os buses nascem vazios.
- **SCAN** lê todas as faixas (cor, saída e os clips do arranjo). Com **AUTO ON SCAN** (padrão), elas já chegam
  marcadas pelo nome, com as mesmas palavras do MAESTRO (Kick, Bass, Snare, Clap, Perc, Hat, Pad, Lead, Vox, FX,
  Riser...); os palpites aparecem com o grupo só contornado e "?". Nada muda no set até o APPLY.
- **Conferir**: clique, Shift+clique ou arrastar para marcar faixas e o botão do grupo para escolher (o número no botão
  = faixas no grupo). NONE tira as marcadas de qualquer grupo, CLEAR desmarca, AUTO BY NAME marca pelo nome as que
  ainda estão sem grupo. Faixa sem grupo não é tocada.
- **Opções**: **BUS** (ON: um bus por grupo usado e a saída das faixas para ele), **CLIPS** (ON: pinta também os clips,
  arranjo e session) e **PREFIX** (OFF: põe o nome do grupo na frente do nome da faixa, "DRUMS 03 Snare").

<img src="https://insidestudiosp.com.br/img/espanador-janela.webp" width="900" alt="ESPANADOR — janela">

<img src="https://insidestudiosp.com.br/img/espanador-device.webp" width="420" alt="ESPANADOR — device">

- **APPLY**, faixa a faixa de cima para baixo: o espanador desce pela lista junto com o trabalho de verdade e, no fim,
  o set é relido e os buses aparecem com o selo BUS. No Live, os buses ficam no topo do set, na ordem KICK, BASS,
  DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX (ou logo abaixo de onde estava a faixa MAESTRO), em Monitor IN e com entrada
  "No Input": soam como um Group Track. O Loop dos clips não muda ao pintar.
- **UNDO** volta nome, cor e saída de cada faixa e a cor dos clips, apaga os buses criados que continuam vazios e
  devolve a faixa MAESTRO (enquanto o device estiver aberto; fora isso, cada mudança é um Ctrl+Z).

## Como ele decide os buses

- Faixa de áudio **vazia** com o nome do grupo (ou "<GRUPO> BUS") é reaproveitada como bus.
- Faixa "KICK" ou "BASS" **com** clips (ou MIDI) é o próprio kick/baixo: entra no grupo e o bus novo vira "KICK BUS" /
  "BASS BUS". Group Track que já se chama DRUMS: o bus vira "DRUMS BUS".
- Faixa dentro de um Group Track continua saindo no grupo (ganha cor e prefixo); quem vai para o bus é o Group Track
  inteiro. Group Track que mistura faixas de grupos diferentes não tem a saída mudada.

## Limites (do Live)

- O Max for Live não cria Group Track nem muda a ordem das faixas: por isso o bus, que soma e processa mas não dobra.
- O Max for Live não insere plugin numa faixa: por isso a faixa MAESTRO é copiada.
- A cor que o Live mostra é a da paleta dele mais próxima da cor do grupo.

## Licença e versão nova

- Na 1ª vez: **OPEN THE MAGIC** > e-mail + token da compra (`ISP-XXXX-…`) > **ACTIVATE** (precisa de internet). Até 2
  computadores, 45 dias sem internet; **LICENSE**, no canto da janela, mostra a licença e tem DEACTIVATE THIS COMPUTER.
- Mesmo código de licença da VASSOURA e do RODO (`../_comum`, pacote `espanador`); no Live 12.2/12.3 a conversa com o
  servidor passa por um [js] clássico (`espanador_http.js`).
- Log: a cada SCAN o ESPANADOR grava `ESPANADOR-log.txt` na pasta do device; no console do Max, as linhas
  `ESPANADOR:` mostram cada faixa, cada bus e o que não deu certo.

## Arquivos

| Arquivo | O que é |
|---|---|
| `espanador.js` | Motor ([v8]): SCAN pelo LOM, AUTO pelo nome, APPLY (cor, prefixo, clips, buses) e UNDO |
| `espanador_view.js` | Desenho da janela (`jsui`) |
| `build-espanador.js` | Gera `Espanador.amxd` + `Espanador.maxpat` e os `.js` (com `../_comum`) na pasta de saída |
| `LEIA-ME.txt` | O passo a passo que vai no zip |

## Gerar e instalar

```
node build-espanador.js <pasta-saida>     # sem argumento: ./dist (fora do git)
```

Copie a pasta de saída inteira para `User Library\Presets\Audio Effects\Max Audio Effect\` e arraste o
`Espanador.amxd` para a primeira faixa de áudio do projeto. Os `.js` precisam ficar na mesma pasta do `.amxd`
("v8: can't find espanador.js" = não estão).

## Download e versões

O zip pronto (`Espanador-x.y.z.zip`) fica no release `espanador-vx.y.z`, em
[Releases](https://github.com/equalizedmusic/vsts/releases) (fica só o da última versão), publicado pelo workflow
`release-max4live.yml`: a versão é o primeiro `## x.y.z` de [`novidades/Espanador.md`](../../novidades/Espanador.md).
Link fixo (sempre a versão mais nova): `https://insidestudiosp.com.br/baixar/espanador/zip`.
