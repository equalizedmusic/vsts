# ONKEY — novidades

Pacote do instalador: `InsideStudioSP-Onkey`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `onkey-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.0 — 2026-11-06
- Primeira versão do ONKEY, para TESTE (ainda sem ativação por licença e fora do site): afinação automática de voz.
- Vai no canal da VOZ (uma voz só). Acha sozinho o tom e a escala em alguns segundos de voz (perfis de
  Krumhansl-Kessler sobre as notas cantadas) e leva ao tom cada nota que sair dele. Enquanto o tom não trava, corrige
  só o que está entre duas notas (cromático). ROOT e SCALE escolhem o tom na mão (MAJOR, MINOR, HARM. MINOR,
  MAJ. PENTA, MIN. PENTA, CHROMATIC).
- Corrige pelo CENTRO de cada nota: vibrato, slides e respiração passam como foram cantados. Consoantes e respiração
  saem idênticas à entrada.
- Síntese TD-PSOLA (um grão por período): os formantes ficam onde estavam, a voz não muda de timbre. Sem correção,
  a saída é a própria entrada, amostra por amostra.
- Na tela: botão ONKEY, AMOUNT, KEY (tom detectado e as 12 notas da escala), ROOT / SCALE, NOTE NOW (como foi cantado
  e a correção, em cents) e o gráfico dos últimos 4 s com a voz original (laranja) e a corrigida (verde) sobre as
  faixas da escala (roxo). AJUSTE FINO (abre por cima do gráfico): RETUNE SPEED, HUMANIZE, VIBRATO, FORMANT (±3 st)
  e OUTPUT.
- Latência de ~45 ms, compensada pelo DAW. Desligado, o som passa sem nenhuma alteração e o ONKEY continua ouvindo.
