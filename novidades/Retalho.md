# RETALHO — novidades

Pacote do instalador: `InsideStudioSP-Retalho`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `retalho-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.0 — 2026-11-16
- Primeira versão do RETALHO (nome provisório), para TESTE: ainda sem ativação por licença e fora do site.
  Stutter + groove de vocal, VST3 e AU, latência 0 (repete só o que já passou).
- Tela no layout do esboço v2: display de onda com ORIGINAL (a voz capturada, ataques marcados e o CHUNK em
  destaque) e RESULT (o fill renderizado em blocos, com a curva de PITCH em ciano e a do FILTER em rosa).
- EDIT, o fill desenhado: SHAPE (GRID, RAMP, CURVE, FREE, TREMOLO), LENGTH (1/2, 1 ou 2 compassos), RATE de 1/4 a 1/64
  com tercina e pontuada, RAMP TO e curva LOG / LIN / EXP. Cada repetição tem DECAY, FADE IN, FADE OUT, GATE e SWING.
  PITCH com START, END e CURVE, em degraus (STEPS) ou contínuo (GLIDE); KEY trava os degraus na escala (AUTO detecta o
  tom da voz). FILTER HPF / LPF / BPF varrendo de FROM a TO, com RESO. STEREO de mono a PING-PONG, TAPE STOP no fim e
  REVERSE. No GRID, clicar na fatia liga ou desliga, arrastar muda o volume e o botão direito põe REVERSE; no FREE o
  tamanho das fatias é desenhado na onda. CHUNK: AUTO (a partir do disparo) ou PREV / NEXT pelos ataques anteriores.
- COMPLEX: 8, 16 ou 32 passos com 8 tipos (REPEAT ×2/×3/×4/×8, PITCH ±24 st, REVERSE, TAPE stop / start, SPEED ½× / 2×,
  GATE, THRU, MUTE). Clique no topo do passo troca o tipo, arrastar muda o valor e o rodapé (ou o botão direito) põe o
  cadeado. READ: FOLLOW (cada passo lê o seu ponto), HOLD (todos leem do 1º) ou HITS (pula para o ataque mais
  próximo). DICE sorteia tipos, valores ou só pitch (AMOUNT, CHAOS, SEED); EVERY sorteia de novo a cada 1, 2, 4 ou 8
  compassos no AUTO; o cadeado protege o passo.
- GROOVE: a voz vira 8 chops (as últimas sílabas, por ataque ou cortadas em 1/8 / 1/16), LIVE ou FREEZE, tocando num
  pattern de 1 ou 2 compassos em 1/16 ou 1/16T, com 3 linhas (OFFBEAT, ACCENT, FILL). 7 estilos: OFFBEAT, GALLOP,
  SKIP, 2-STEP, FUNK, TRAP e CALL&RESP (o chop só responde no vão da voz). DENSITY, SWING, HUMANIZE e PITCH do chop.
  Mistura com a voz: LAYER, DUCK (abaixa a voz quando o chop entra) ou REPLACE, com HPF, LPF e VOL dos chops. DICE e
  LOCK GRID. GROOVE ON liga e desliga no compasso.
- LIVE (rodapé fixo): AUTO (dispara sozinho no fim ou no início de 1, 2, 4 ou 8 compassos, com o projeto tocando),
  HOLD (enquanto segura o pad ou a nota) e ONE-SHOT (o fill inteiro). CATCH espera 1/16, 1/4 ou o próximo compasso.
- 6 pads de memória (cena completa): clique dispara, Shift+clique salva, botão direito renomeia ou limpa. MIDI C3–F3
  dispara os pads (a velocity é o volume do fill) e cada pad também é um parâmetro automatizável.
- ▶ ORIGINAL / ▶ RESULT tocam a janela capturada ou o fill (LOOP, ON BAR). EXPORT grava o RESULT em WAV 24 bits em
  Documentos/RETALHO/Exports e o chip ARRASTE PARA O DAW solta o WAV direto numa faixa.
- 54 presets de fábrica em 5 bancos (RISERS, DROPS & TAPE, GLITCH, GROOVE VOX, TREMOLO & GATE) e preset do usuário
  em arquivo .retalho. DRY/WET, OUTPUT, limitador de segurança em −0,3 dBFS, A/B e BYPASS.
