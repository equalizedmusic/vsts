# DESENROLA (Max for Live)

Tira a música do loop. Lê a estrutura de uma música de referência (INTRO, BREAK, BUILDUP, DROP, FINAL) e
espalha os clips do loop que está no Arrangement seguindo essa estrutura. O resultado é um rascunho de música
montado depois do loop. Grátis, sem licença (`LIC_TESTE = true`) e sem página no site até o teste do Rogério.

## Arquivos

| Arquivo | O que é |
|---|---|
| `desenrola.js` | motor (`[v8]`): ANALYZE (faixas do loop + leitura da referência), BUILD, faixa ESTRUTURA, apagar REF, UNDO |
| `desenrola_view.js` | janela (`jsui`): partes (seleção, menu do botão direito: tipo/renomear/dividir/tirar, tamanho, ordem) e blocos por faixa (pintar, mover, começo/fim), zoom |
| `build-desenrola.js` | gera `Desenrola.amxd` + os `.js` (cola o comum de `_comum/`); acrescenta na janela o `[dialog]` do RENOMEAR (`dlg_nome` → `nome_parte`) |
| `LEIA-ME.txt` | vai no zip |

## Como funciona

- **Loop**: `live_set loop_start/loop_length` (o loop brace). Entram os clips de arranjo que estão inteiros dentro dele.
- **Referência**: faixa chamada `REF` (ou a selecionada, se o clip estiver fora do loop); maior clip de áudio dela.
  O arquivo vai para o `buffer~ desenrola_ref` e é lido em pedaços de 4 s: grave (até ~110 Hz), agudo (acima de
  ~5 kHz) e cheio, em quadros de 10 ms.
- **Andamento**: pelas `warp_markers` do clip (se tiver warp); senão, pela pulsação do grave (100–180 BPM, todas as
  fases; depois o fino de 0,02 BPM). O 1 do compasso é a batida onde a energia mais muda.
- **Partes**: por compasso, kick (pulso do grave na batida) e baixo (grave entre as batidas). DROP = kick + baixo,
  BREAK = sem kick; antes do 1º DROP = INTRO, depois do último = FINAL; o fim (8 compassos) de um BREAK de 12+ que
  desemboca num DROP = BUILDUP; kick sem baixo entre dois DROPs = BREAK. No rascunho cada parte arredonda para 8
  (se estiver a 1 compasso) ou 4. Cada parte tem `id` (a janela usa para os blocos andarem junto com a parte).
- **Blocos**: cada faixa tem `blocos: [{a, b}]` em compassos a partir do começo do rascunho. Saem do padrão do grupo
  (um por parte, `REGRAS`); a janela edita. Blocos que só encostam ficam separados (o loop recomeça em cada um).
- **BUILD**: para cada bloco, `Track.duplicate_clip_to_arrangement(clip, destino)` de cada clip do loop, repetido
  desde o começo do bloco até encher. Cópia que passa do fim do bloco é aparada pelos markers (3 jeitos, confere
  `end_time`; o que funcionou fica no log); se não der e a faixa tem bloco colado em seguida, ele cobre por cima;
  senão a cópia sai.
- **ESTRUTURA** (0.2, no lugar dos locators): `live_set.create_midi_track(0)` + `Track.create_midi_clip(início, tamanho)`
  por parte, com `name` (Intro, Break, Buildup, Drop N, Final; ou o `nome` que a parte ganhou no RENOMEAR) e `color` da parte.
- **APAGAR REF**: antes, `fotoRef` guarda nome, cor, índice, mute, volume/pan e cada clip (arquivo, posição, warp, warp_mode,
  markers, loop, ganho, pitch, `warp_markers`); sem foto, não apaga. Depois `live_set.delete_track(índice da REF)`; não
  apaga se `this_device canonical_parent` for a REF.
- **UNDO** (0.3): apaga os clips das faixas do loop que começam dentro do rascunho e a faixa ESTRUTURA; recria a REF pela
  foto: `create_audio_track(índice)` + `Track.create_audio_clip(arquivo, posição)` (Live 12) + markers e warp markers
  (`add_warp_marker`: 3 jeitos de passar o dict, o que funcionou fica no log; `remove_warp_marker` nos que o Live criou).
  Devices da faixa REF não voltam.
- **Partes com nome** (0.3): `partes[i].nome` (até 24 letras) troca o rótulo; o tipo `p` continua valendo para as regras e a cor.

## Testes

- `_testes/sim-desenrola.js`: Live falso (arranjo com sobrescrita, clip em loop só encurta desligando o loop) +
  `buffer~` falso com a música de `_testes/gera-ref-teste.py` (gabarito conhecido). Confere partes, faixas, cada faixa
  nos seus blocos, aparar, faixa ESTRUTURA (com parte renomeada), REF apagada, UNDO (REF volta igual) e o loop intacto. Com `warp` usa as marcas de warp
  (rodar com LEAD=0.37, o atraso padrão do gera-ref-teste.py).
- `_testes/sim-desenrola-janela.js`: a janela sem o Max (mouse falso): clique seleciona, menu do botão direito (tipo,
  RENOMEAR, clique fora fecha), tamanho/ordem das partes, + PART, dividir,
  tirar, pintar/pincel/mover/bordas/apagar blocos, grupo, zoom; os blocos andando com as partes.
- `_testes/shot-desenrola.js`: foto da faixa e da janela (mgraphics → canvas).

Testado pelo Rogério na 0.1 (22/11): leitura da referência e cópia dos clips; os locators não ficaram bons (trocados pela
faixa ESTRUTURA). Na 0.2 (22/11): janela, ESTRUTURA e APAGAR REF funcionaram; o UNDO não trazia a REF (resolvido na 0.3).
NÃO testado no Live ainda (0.3): botão direito no jsui (`ctrl` = 1), `[dialog]` do RENOMEAR, `create_audio_track` +
`Track.create_audio_clip` + `add_warp_marker` no UNDO.
