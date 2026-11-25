# RODO (Max for Live)

> **Versão atual: 0.3.6** (24/11/2026) · Max for Live (Ableton Live 12.2+) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Rodo.md`](../../novidades/Rodo.md)

![RODO](https://insidestudiosp.com.br/img/rodo.webp)

Nivela os clips de áudio do arranjo no Ableton Live pelo **Clip Gain**, sem renderizar nada, medindo por pico, RMS ou
LUFS (SENSOR). Par da [VASSOURA](../vassoura/README.md): ela tira o silêncio, ele deixa todos no mesmo nível.

É um **Max for Live Audio Effect** (Live 12.2 ou mais novo, Suite ou Standard com Max for Live), que lê e ajusta os
clips pelo Live Object Model. É o 2º passo do fluxo do **PACOTÃO MIX & MASTER**: VASSOURA (limpa) → RODO (nivela) →
[ESPANADOR](../espanador/README.md) (organiza em grupos e buses) → MAESTRO (mixa) → MASTER DOS MAGOS (finaliza).
Cada um funciona sozinho.

## Como funciona

- **Device na primeira faixa de áudio** (no master o ANALYZE não lê os clips): a arte do RODO e um botão só,
  **OPEN THE MAGIC**, que abre a janela do Max por cima do Live. Todos os controles ficam na janela.
- **Janela**, barra em duas linhas: SENSOR, TARGET e PEAK GUARD em cima; MODE, SCOPE e ANALYZE / APPLY / RESET
  embaixo. Uma linha por faixa com o nível de hoje (no SENSOR escolhido) e o GAIN que ela vai receber; clique no nome
  da faixa = fora da normalização.

<img src="https://insidestudiosp.com.br/img/rodo-janela.webp" width="900" alt="RODO — janela">

<img src="https://insidestudiosp.com.br/img/rodo-device.webp" width="420" alt="RODO — device">

- **ANALYZE** lê o arquivo de cada clip uma vez só e mede, do trecho que o clip toca, o pico, o RMS e o LUFS. Trocar
  SENSOR, TARGET, MODE ou SCOPE redesenha na hora, sem reler.
- **SENSOR**: **PEAK** (sample peak, o padrão; TARGET −12 a −0.3 dB), **RMS** (ignora o silêncio abaixo de −60 dB;
  TARGET −36 a −6 dB) ou **LUFS** (ITU-R BS.1770 / EBU R128, ponderação K e gates de −70 LUFS e −10 LU; TARGET −36 a
  −6 LUFS). Cada SENSOR guarda o seu TARGET.
- **PEAK GUARD** (em RMS e LUFS, vem ligado): o ganho para quando o pico do clip chega a −0.3 dB. A faixa segurada
  aparece com ▲ e, depois do APPLY, com "guard −X" (o que faltou para o TARGET).
- **MODE**: **CLIP** (cada clip vai ao TARGET), **TRACK** (a faixa vai ao TARGET e toda ela recebe o mesmo ajuste:
  mantém a dinâmica; em PEAK vale o clip mais alto, em RMS/LUFS a faixa medida inteira) ou **ALL** (a faixa mais alta
  do set vai ao TARGET e tudo recebe o mesmo ajuste: mantém o balanço). **SCOPE** ALL / SELECTED.
- **APPLY** ajusta o Clip Gain clip a clip; um rodo passa da esquerda para a direita junto com o ajuste real e, atrás
  dele, as ondas já estão no nível novo. No fim o set é relido.
- **RESET** volta o Clip Gain que cada clip tinha antes do primeiro APPLY (enquanto o device estiver aberto; fora
  isso, cada clip ajustado é um Ctrl+Z).

O LOM dá o ganho do clip num valor 0..1 que não é linear em dB nem documentado: o RODO guarda os pares (valor, dB) que
já viu e acha o valor pedido por interpolação, conferindo no texto que o Live mostra.

## Limites

- Só o Clip Gain é mexido: volume da faixa, automação e plugins ficam como estão.
- O Clip Gain do Live tem teto: clip muito baixo pode não chegar ao TARGET ("faltou X dB").
- Clips mutados ficam fora. Clip com WARP: o pico é medido pelo mapa dos warp markers (sem o mapa, o arquivo inteiro).
- O pico é o das amostras (sample peak), não true peak. RMS e LUFS medem o arquivo do clip, antes de plugins e do
  volume da faixa; clip mono conta como os dois lados iguais.

## Licença e versão nova

- Na 1ª vez: **OPEN THE MAGIC** > e-mail + token da compra (`ISP-XXXX-…`) > **ACTIVATE** (precisa de internet). Até 2
  computadores, 45 dias sem internet; **LICENSE**, no canto da janela, mostra a licença e tem DEACTIVATE THIS COMPUTER.
- Mesmo código de licença da VASSOURA (`../_comum`, pacote `rodo`); no Live 12.2/12.3 a conversa com o servidor passa
  por um [js] clássico (`rodo_http.js`).
- Versão nova: a janela mostra **NEW VERSION x.y.z · DOWNLOAD**.
- Log: a cada ANALYZE o RODO grava `RODO-log.txt` na pasta do device.

## Arquivos

| Arquivo | O que é |
|---|---|
| `rodo.js` | Motor ([v8]): lê faixas e clips pelo LOM, mede pico/RMS/LUFS num `buffer~` e ajusta o Clip Gain |
| `rodo_view.js` | Desenho da janela (`jsui`) |
| `build-rodo.js` | Gera `Rodo.amxd` + `Rodo.maxpat` e os `.js` (com `../_comum`) na pasta de saída |
| `LEIA-ME.txt` | O passo a passo que vai no zip |

## Gerar e instalar

```
node build-rodo.js <pasta-saida>     # sem argumento: ./dist (fora do git)
```

Copie a pasta de saída inteira para `User Library\Presets\Audio Effects\Max Audio Effect\` e arraste o `Rodo.amxd`
para a primeira faixa de áudio do projeto. Os `.js` precisam ficar na mesma pasta do `.amxd` ("v8: can't find
rodo.js" = não estão).

## Download e versões

O zip pronto (`Rodo-x.y.z.zip`) fica no release `rodo-vx.y.z`, em
[Releases](https://github.com/equalizedmusic/vsts/releases) (fica só o da última versão), publicado pelo workflow
`release-max4live.yml`: a versão é o primeiro `## x.y.z` de [`novidades/Rodo.md`](../../novidades/Rodo.md).
Link fixo (sempre a versão mais nova): `https://insidestudiosp.com.br/baixar/rodo/zip`.
