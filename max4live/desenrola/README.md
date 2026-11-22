# DESENROLA (Max for Live)

Tira a música do loop. Lê a estrutura de uma música de referência (INTRO, GROOVE, BUILD, DROP, BREAK, OUTRO) e
espalha os clips do loop que está no Arrangement seguindo essa estrutura. O resultado é um rascunho de música
montado depois do loop. Grátis, sem licença (`LIC_TESTE = true`) e sem página no site até o teste do Rogério.

## Arquivos

| Arquivo | O que é |
|---|---|
| `desenrola.js` | motor (`[v8]`): ANALYZE (faixas do loop + leitura da referência), BUILD, UNDO, locators |
| `desenrola_view.js` | janela (`jsui`): partes (linha do tempo), grade faixa × parte, prévia do rascunho |
| `build-desenrola.js` | gera `Desenrola.amxd` + os `.js` (cola o comum de `_comum/`) |
| `LEIA-ME.txt` | vai no zip |

## Como funciona

- **Loop**: `live_set loop_start/loop_length` (o loop brace). Entram os clips de arranjo que estão inteiros dentro dele.
- **Referência**: faixa chamada `REF` (ou a selecionada, se o clip estiver fora do loop); maior clip de áudio dela.
  O arquivo vai para o `buffer~ desenrola_ref` e é lido em pedaços de 4 s: grave (até ~110 Hz), agudo (acima de
  ~5 kHz) e cheio, em quadros de 10 ms.
- **Andamento**: pelas `warp_markers` do clip (se tiver warp); senão, pela pulsação do grave (100–180 BPM, todas as
  fases; depois o fino de 0,02 BPM). O 1 do compasso é a batida onde a energia mais muda.
- **Partes**: por compasso, kick (pulso do grave na batida) e baixo (grave entre as batidas). DROP = kick + baixo,
  GROOVE = kick sem baixo, BREAK = sem kick; antes do 1º DROP = INTRO, depois do último = OUTRO; o fim (8 compassos)
  de um BREAK de 12+ que desemboca num DROP = BUILD. No rascunho cada parte arredonda para 8 (se estiver a 1 compasso)
  ou 4.
- **BUILD**: `Track.duplicate_clip_to_arrangement(clip, destino)` para cada clip do loop, repetido até encher a parte
  (o mesmo comando da VASSOURA, validado no Live 12). Cópia que passa do fim da parte é aparada pelos markers
  (3 jeitos, confere `end_time`; o que funcionou fica no log); se não der e a faixa segue na próxima parte, a próxima
  cobre por cima; senão a cópia sai. Locators: `current_song_time` + `set_or_delete_cue` + nome.
- **UNDO**: apaga os clips das faixas do loop que começam dentro do rascunho e os locators criados.

## Testes

- `_testes/sim-desenrola.js`: Live falso (arranjo com sobrescrita, clip em loop só encurta desligando o loop) +
  `buffer~` falso com a música de `_testes/gera-ref-teste.py` (gabarito conhecido). Confere partes, faixas, cada faixa
  em cada parte, aparar, locators, UNDO e o loop intacto. Com `warp` usa as marcas de warp.
- `_testes/shot-desenrola.js`: foto da faixa e da janela (mgraphics → canvas).

NÃO testado no Live ainda: `warp_markers` (formato), aparar clip em loop, locators via `set_or_delete_cue`,
`Buffer.peek` em pedaços de 4 s com arquivo longo.
