# SÉRIE KNOB

Dez pedais de efeito de **um knob grandão**, todos no tempo do DAW. O knob vai de 0 (nada) a 10 (tudo) e as
lâmpadas em volta dele acendem até o valor e respiram junto com o efeito.

## Controles (iguais nos 10)

| Controle | O que faz |
|---|---|
| **Knob** | A dose do efeito (o que ele faz em cada pedal está na tabela de baixo). Arraste para cima/baixo; Shift = ajuste fino; duplo clique volta ao padrão; a roda do mouse também gira |
| **TRIGGER** | Chave de 3 posições. **INT**: segue a régua do DAW (parado, continua no último BPM). **MIDI**: cada nota que chega recomeça o movimento. **ÁUDIO**: cada ataque do sidechain recomeça; sem sidechain ligado, usa a própria entrada. Clique em INT, MIDI ou ÁUDIO (ou na alavanca para passar para a próxima) |
| **RATE** | Divisão do tempo, 7 posições (cada pedal tem as suas). Clique direito abre a lista |
| **SHAPE** | Forma do movimento, desenhada no visor com o cursor no ponto em que o som está. Clique na metade direita do visor para ir para a próxima forma e na esquerda para voltar; clique direito abre a lista |
| **MIX** | Mistura com o som original. No ECO o seco fica inteiro e o MIX dosa só as repetições |

O LED ao lado de TRIGGER pisca a cada tempo (INT) ou a cada nota/ataque (MIDI/ÁUDIO).

### Sidechain e MIDI no Ableton
- **ÁUDIO**: no pedal, em *Sidechain*, escolha a faixa que vai disparar (o kick, por exemplo).
- **MIDI**: numa faixa MIDI, em *MIDI To*, escolha a faixa do pedal e, embaixo, o pedal.

## Os pedais

| Pedal | Knob | SHAPE | RATE padrão |
|---|---|---|---|
| **FOLE** | Profundidade do sidechain (10 = some no golpe) | CLÁSSICO, SUAVE, BOMBA, CORTE, RAMPA, INVERSO | 1/4 |
| **PICADOR** | Até 6: quanto corta. De 6 a 10: o gate também encurta | 7 padrões de 16 passos (TRANCE 1/2, RETO, GALOPE, CONTRATEMPO, TRESILLO, QUEBRADO) | 1/16 |
| **MARÉ** | Quanto o passa-baixa desce (até ~30 Hz), com ressonância | SENO, TRIÂNGULO, SOBE, DESCE, QUADRADO, ALEATÓRIO | 2 COMP |
| **VAIVÉM** | Abertura do auto-pan | SENO, TRIÂNGULO, QUADRADO, SOBE, ALEATÓRIO | 1/2 |
| **REPIQUE** | Quanto do fim do compasso vira repetição (10 = o compasso inteiro). RATE = tamanho da fatia | ROLL (fatias cada vez menores), REPETE, DECAI, TRAVA (repete o começo do compasso) | 1/16 |
| **ECO** | Realimentação (0 = uma repetição; no DUB satura e não para). TRIGGER MIDI/ÁUDIO = *dub throw* | SIMPLES, PING-PONG, FITA, DUB, ESPALHADO | 3/16 |
| **AVESSO** | Quantas fatias do compasso tocam ao contrário | ALTERNADO, FIM DO COMP., INÍCIO, ALEATÓRIO (quais viram primeiro) | 1/8 |
| **FREIO** | Quanto do ciclo a freada ocupa (até metade) | PARADA, PARTIDA, PARA E VOLTA, MEIA VELOCIDADE | 1 COMP |
| **ESCADA** | Tamanho dos saltos de pitch, sempre dentro da escala/acorde | SOBE, DESCE, SOBE E DESCE, OITAVAS, ARPEJO MAIOR, ARPEJO MENOR, QUINTAS, ALEATÓRIO | 1/8 |
| **MASTIGA** | O quanto morde: drive, bits (até 3) e taxa (até /32) no pico da forma | QUADRADO, SENO, SOBE, DESCE, DEGRAUS, PULSO, ALEATÓRIO | 1/8 |

## Como é feito

- `_comum/` tem o que os 10 dividem: o processor (relógio, TRIGGER, MIX, bypass), a interface e as imagens.
  Cada pedal tem só `CMakeLists.txt` (uma linha: `serie_knob_pedal(...)`) e `Source/Pedal.cpp` (a `Info` e o motor).
- A interface é o pedal renderizado no Blender/Cycles. `_comum/tools/sprites.py` gera o fundo (sem knobs e com as
  lâmpadas apagadas), o knob grandão e os pequenos em 16 quadros de uma canelura (as caneluras se repetem, então 16
  quadros cobrem o giro inteiro), a alavanca nas 3 posições e a plaquinha do nome de cada pedal. `_comum/tools/recorta.py`
  recorta tudo para `_comum/assets/` e gera `_comum/Source/Mapa.h` com as posições em pixels. Por cima, a interface
  desenha ao vivo as lâmpadas na cor do pedal, a linha e o emblema do knob, o visor e os textos.
- Para refazer as imagens: rode `sprites.py` nos modos `mapa`, `fundo`, `nome`, `knob`, `rate`, `mix` e `alav`
  (ver o cabeçalho do arquivo) e depois o `recorta.py`.
- `_comum/tools/teste_motores.cpp` roda um motor sem JUCE em todas as formas, nos 3 TRIGGERs e em 44,1/96 kHz,
  procurando NaN, estouro e efeito ligado com o knob em 0.
