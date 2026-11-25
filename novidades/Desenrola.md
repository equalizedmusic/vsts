# DESENROLA — novidades

Device Max for Live (pasta `max4live/desenrola`), da família da VASSOURA, do RODO e do ESPANADOR. A versão é o primeiro
título `## x.y.z` deste arquivo: ela vira o release `desenrola-vx.y.z` do GitHub (zip `Desenrola-x.y.z.zip`), publicado
pelo workflow `release-max4live.yml`. Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.4.2 — 2026-11-25
- **Atualização automática:** o aviso de versão nova virou **UPDATE**: o device baixa o instalador, confere a
  assinatura e o arquivo e põe a versão nova na User Library do Ableton sozinho, sem senha e sem abrir o navegador.
  Depois é só carregar o device de novo (ou reabrir o set). Se algo falhar, ele abre o instalador no navegador, como antes.
- Vale a partir da próxima versão: esta ainda se instala pelo aviso de sempre (que já entrega o instalador, não o zip).
- As funções não mudam.

## 0.4.1 — 2026-11-24
- **Ativação automática:** se você já ativou outro plugin ou device InsideStudioSP neste computador, com o mesmo e-mail, este
  se ativa sozinho ao abrir, sem digitar o token. Se o seu e-mail não tem este pacote, a tela de ativação já abre com o
  e-mail preenchido e avisa. Desativar continua valendo: o device não se reativa sozinho depois.
- Aviso de autoria na tela da licença (ACTIVATE) (© Rogerio Cardoso / InsideStudioSP).
- A análise e as funções não mudam.

## 0.4.0 — 2026-11-22
Primeira versão oficial, GRÁTIS com licença (token grátis na página do DESENROLA). As 0.1 a 0.3 foram só de teste.
- **Tira a música do loop.** Você deixa o loop no Arrangement (todas as faixas, áudio e MIDI) e uma faixa REF com uma
  música de referência. O ANALYZE lê a estrutura da referência (INTRO, BREAK, BUILDUP, DROP 1/2/3, FINAL) e as faixas
  do loop, e cada faixa já vem com os blocos onde toca pelo grupo (KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX).
- **MODO LIVRE:** sem faixa REF, a janela abre com um esqueleto de partes e as faixas vazias. Você monta a estrutura e
  pinta onde cada faixa toca, do seu jeito.
- **Janela tipo Arrangement:** arraste as partes para mudar a ordem, a borda para o tamanho, e o menu da parte (seta no
  canto, botão direito ou Ctrl+clique no Mac) troca o tipo, renomeia, divide ou tira. Nas faixas, pinte, mova e
  estique os blocos, com PINCEL de 4/8/16/32 compassos ou a parte inteira, e ZOOM.
- **BUILD** monta o rascunho depois do loop, com cópias dos seus clips (nenhuma nota é escrita) e a faixa
  **ESTRUTURA** (um clip com o nome e a cor de cada parte). APAGAR REF tira a faixa da referência no fim.
- **UNDO** apaga o rascunho e a ESTRUTURA e traz a REF de volta. O loop original nunca é mexido.
- Live 12.2 ou mais novo (Suite, ou Standard com Max for Live).
