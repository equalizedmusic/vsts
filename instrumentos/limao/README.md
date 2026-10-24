# LIMÃO — bassline ácido

Sintetizador monofônico (VSTi / AU) com a alma do TB-303, mas moderno: uma voz, filtro ladder de 18 dB/oit
que grita na ressonância, ACCENT que acumula em accents seguidos, SLIDE entre notas ligadas, efeitos na saída e
um sequenciador de até 32 passos que anda no tempo do projeto. Visual: painel branco com faixa verde-limão,
knobs Moog da casa e osciloscópio. Instalador próprio (`InsideStudioSP-Limao-win.exe` / `-mac.dmg`).

## Blocos

| Bloco | O que tem |
|---|---|
| VOZ | WAVE (SAW ou SQUARE), TUNE (±12 st), CUTOFF, RESONANCE, ENV MOD, DECAY, ACCENT (+ volume e + corte; nota acentuada usa decay curto), SLIDE (tempo do deslize) |
| MACRO | ÁCIDO: abre ENV MOD, RESONANCE e um pouco do CUTOFF juntos |
| DRIVE | TUBE (assimétrico, quente), PEDAL (corte duro) ou BITCRUSH (bits e taxa caindo juntos) |
| DELAY | no tempo (1/16 a 3/8), pingue-pongue, cada volta mais escura; FEEDBACK e MIX |
| REVERB | SIZE e MIX |
| SAÍDA | VOLUME e medidor de pico em LEDs |
| SEQUENCIADOR | liga/desliga, PASSOS (1–32), RATE (1/32 a 1/8), SWING, GIRAR (◀ ▶), TRANSP (−1 / +1), ALEATORIZAR e LIMPAR |

**Cada passo:** visor da nota (arraste na vertical ou roda do mouse = muda a nota, de C1 a C4; duplo clique = nota ↔
pausa), tecla NOTA / PAUSA / LIGA (ligadura segura a nota anterior), ACC (accent) e SLD (slide para o próximo passo).

**Relógio:** o sequenciador só anda com o host tocando; parado, mostra AGUARDANDO PLAY. Com ele desligado, o LIMÃO
toca pelo teclado (MIDI), mono com prioridade para a última nota: notas ligadas deslizam e velocity ≥ 100 é accent.
Com ele ligado, a nota segurada no teclado transpõe o padrão (Dó 3 = sem transpor).

**Presets:** 15 de fábrica (303 Clássico, Acid House, Techno Pesado, Psytrance Rolling, Lead Gritado, Squelch 32,
Borracha, Sereia, Bitcrush Ácido, Minimal, Dub Ácido, Garagem Swing, Ácido Total, Sub Grave e Init), cada um com o
padrão de sequência junto. Trocar de preset não liga nem desliga o sequenciador. O padrão editado fica salvo no projeto.
No menu do preset também fica o tamanho da janela (75–200%, salvo no projeto).

## Fontes

| Arquivo | O que é |
|---|---|
| `Params.h` | ids dos parâmetros, listas e o padrão (um inteiro atômico por passo) |
| `Engine.h` | voz (osc polyBLEP, ladder ZDF com saída no 3º degrau, envelopes, accent, slide) e efeitos |
| `Presets.h` | presets de fábrica (som + padrão em texto) e o ALEATORIZAR |
| `PluginProcessor.*` | relógio do host, sequenciador com swing, MIDI, estado |
| `PluginEditor.*`, `LimaoUi.h` | tela (painel branco, knob com escala em tinta preta, visores, passos, osciloscópio) |
| `Theme.h`, `Widgets.h` | os da casa, iguais aos dos outros plugins |
