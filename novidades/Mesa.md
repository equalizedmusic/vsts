# MESA — novidades

Pacote do instalador: `InsideStudioSP-Mesa`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `mesa-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.0 — 2026-10-30
- Primeira versão (Fase 1): console analógica em plugin, com instalador próprio.
- Uma MESA no fim de cada canal do DAW: cada cópia vira uma fileira da console, e a janela de qualquer uma mostra a
  console inteira (12 canais à vista, mais que isso rola), os 3 grupos e o MASTER. Dá para mexer em qualquer canal
  por qualquer janela.
- Modos CANAL, GRUPO (G1 a G3) e MASTER. O fader e o pan mexem no som que sai do plugin (o fader do DAW fica em 0 dB).
- PRÉ: LIMPO, API 312 e BRIT 73, com GAIN, DRIVE, PAD de -20 dB e HPF de 18 dB/oitava. A saturação roda a 4× (HQ).
- EQ de 4 bandas: E-4K (Q proporcional ao ganho) e BRIT 81 (frequências em degraus e prateleira de grave com calombo).
- Dinâmica: VCA do canal (THR, RAT, GATE) e COLA 4K nos grupos (THR, RAT, MIX); botão PRÉ põe a dinâmica antes do EQ.
- MASTER: COLA BUS 4K (threshold, attack, release com AUTO, makeup, ratio, mix, sidechain HPF 90 Hz) e fader de saída.
- Medidores: VU na ponte (VU ou PPM), coluna de LEDs em dBFS de pico e LEDs de redução de ganho.
- Visual renderizado do modelo 3D da console (chapas, knobs, botões, LEDs e faders).
- ECO (sem sobreamostragem, menos CPU) ou HQ, para todas as MESAs de uma vez.
- Ativação por licença: na primeira vez o plugin pede o e-mail e o token da licença. Sem ativar, o som passa direto.
