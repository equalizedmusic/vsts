# MESA

Console analógica em plugin (efeito, VST3/AU). Uma MESA no fim de cada canal do DAW: cada cópia vira uma fileira da
console, e a janela de qualquer uma mostra a console inteira — 12 canais à vista (mais que isso rola), os 3 grupos e o
MASTER — e mexe em qualquer canal. O visual vem do modelo 3D da console renderizado no Blender, com acabamento de
equipamento real (plástico fosco, poeira, serigrafia gasta, madeira a óleo) e câmera levemente inclinada.

<img src="https://raw.githubusercontent.com/equalizedmusic/vsts/shots/MESA.png" width="900" alt="MESA">

## Como usar
- **CANAL**: ponha a MESA no fim de cada canal. O fader e o pan da MESA mexem no som que sai do plugin; o fader do DAW
  fica em 0 dB. G1/G2/G3 é a etiqueta do grupo (o envio do áudio é o roteamento do DAW).
- **GRUPO**: MESA no canal de grupo (bus) e aperte GRUPO no painel do MASTER da janela dela; os botões G escolhem qual.
- **MASTER**: MESA no Master do DAW e aperte MASTER.
- Clique no nome do PRÉ ou do EQ para trocar o modelo. Duplo clique: o knob volta ao padrão; na fita, renomeia o canal.
  Shift: ajuste fino. Botão direito: menu (renomear, como funciona, licença).
- Vista: PERTO (uns 6 canais) ou GERAL (a console toda) no botão do canto; − e + ou Ctrl/Cmd + roda aproximam; a roda
  rola (Shift: para o lado); arrastar a chapa fora dos controles passeia.
- As cópias se enxergam quando o DAW carrega os plugins no mesmo processo (o padrão do Ableton, Logic, Reaper, FL,
  Studio One). Com plugin isolado em processo próprio, cada janela mostra só o próprio canal.

## Módulos
| Módulo | Modelos | Controles |
|---|---|---|
| PRÉ | LIMPO, API 312 (operacional discreto: 3ª harmônica), BRIT 73 (classe A: 2ª harmônica, grave satura antes) | GAIN ±20 dB, DRIVE, PAD -20 dB, HPF 18 dB/oit |
| EQ | E-4K (Q proporcional ao ganho), BRIT 81 (frequências em degraus, prateleira com calombo, indutor) | HF, HMF, LMF, LF (ganho e frequência), EQ liga, PRÉ (dinâmica antes do EQ) |
| DINÂMICA | VCA (canal), COLA 4K (grupo) | THR, RAT, GATE / MIX + LEDs de redução |
| SAÍDA | — | G1–G3, PAN, MUTE, SOLO, fader (+10 a -∞) e medidor de pico |
| MASTER | COLA BUS 4K | THRESHOLD, ATTACK, RELEASE (AUTO), MAKEUP, RATIO, MIX, IN, SC HPF 90 Hz, fader |

Opções no painel do MASTER: agulhas em VU (0 VU = -18 dBFS) ou PPM, ECO (sem sobreamostragem) ou HQ (saturação a 4×),
para todas as MESAs.

## Arquivos
- `Source/Dsp.h`: processamento em C++ puro (medido fora do plugin por `tools/teste_dsp.cpp`).
- `Source/Registry.h`: a lista das cópias abertas (a console). `Source/PluginEditor.*`: a janela.
- `tools/layout.py`: as medidas da console em cm (fonte única). `tools/render_mesa.py` (Blender 4.5, Cycles):
  `--part chapas` e `--part pecas` (em lotes: `--so`/`--sem` por prefixo, `--nome`) renderizam as células numa câmera
  ortográfica inclinada 18° (`--tilt`) — a projeção é igual em toda a tela, então cada peça casa com a chapa em qualquer
  lugar; peças com fundo transparente sobre um "shadow catcher" (a sombra vem do 3D). O acabamento real fica em
  `tools/realismo.py`. `tools/recorta.py` junta os lotes, faz a revelação de foto e o brilho do que está aceso e grava
  `Source/img/*` (chapas.jpg; pecas.jpg + pecas_a.png = cor + transparência), `Source/Atlas.h` (retângulo + âncora de
  cada célula) e `Source/Layout.h`. `tools/compoe.py` monta um trecho da console com as células, como o plugin, para
  conferir antes de compilar. `tools/amostra_canal.py` + `tools/pos_foto.py`: a amostra de canal em perspectiva.
