# LIMÃO — bassline ácido

![LIMÃO](https://raw.githubusercontent.com/equalizedmusic/vsts/shots/LIMAO.png)

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
| SEQUENCIADOR | liga/desliga, PASSOS (1–32), RATE (1/32 a 1/8), SWING, GIRAR (◀ ▶), TRANSP (−1 / +1), SCALE + ROOT, ALEATORIZAR e LIMPAR |

**SCALE + ROOT** (13 escalas; CHROMATIC = desligado): a nota de cada passo toca encaixada na escala (a de fora vai
para a da escala mais perto) e o visor mostra a nota que toca. Arrastar, a rodinha e o TRANSP andam um grau da
escala, e ALEATORIZAR gera a linha só com notas da escala, a partir da tônica. O padrão guardado não muda: voltar para
CHROMATIC toca como estava. A transposição pelo teclado vem depois da escala, então ela vai junto. SCALE e ROOT entram
no preset (os de fábrica são CHROMATIC). As escalas ficam em `Source/Scales.h`, o mesmo arquivo da QUIMERA.

**Cada passo:** visor da nota (arraste na vertical ou roda do mouse = muda a nota, de C1 a C4; duplo clique = nota ↔
pausa), tecla NOTA / PAUSA / LIGA (ligadura segura a nota anterior), ACC (accent) e SLD (slide para o próximo passo).

**Relógio:** o sequenciador só anda com o host tocando; parado, mostra AGUARDANDO PLAY. Com ele desligado, o LIMÃO
toca pelo teclado (MIDI), mono com prioridade para a última nota: notas ligadas deslizam e velocity ≥ 100 é accent.
Com ele ligado, a nota segurada no teclado transpõe o padrão (Dó 3 = sem transpor).

**Presets:** banco FACTORY 1 com 360 (40 por categoria: CLÁSSICO, ACID HOUSE, ACID TECHNO, TRANCE, BASS, LEAD,
SQUELCH, DUB e FX) mais o Init, cada um com o padrão de sequência junto. Trocar de preset não liga nem desliga o
sequenciador. O padrão editado fica salvo no projeto. Igual à QUIMERA:
- **USER:** "Salvar como novo preset…" (ou o botão +) grava o som e o padrão; os do usuário dá para salvar por cima,
  renomear e apagar (vai para a lixeira). Pasta: `%APPDATA%\InsideStudioSP\LIMAO\Presets` (Windows),
  `~/Library/Audio/Presets/InsideStudioSP/LIMAO` (Mac).
- **Bancos avulsos (`.lmobank`):** "Importar banco de presets…" copia o arquivo para a pasta `Banks` e o banco aparece
  embaixo do FACTORY 1, com as categorias; "Remover banco" manda para a lixeira. Formato: XML `LIMAOBANK` com
  `PRESET name category` → `PARAM id value` e `PATTERN notes acc sld` (mesmo texto dos presets de fábrica).
  `tools/montar-banco.py` monta um banco a partir de pastas por categoria com os .xml do USER.
- O banco de fábrica é GERADO: `python tools/gerar-presets.py` reescreve `Source/presets/fabrica.inc` (receitas por
  categoria com semente fixa; os 14 feitos à mão entram primeiro).

No menu do preset também ficam o tamanho da janela (75–200%, salvo no projeto) e a versão (`novidades/Limao.md`).
Os menus abrem **dentro** da janela do plugin: como janela solta, a lista de dentro (banco → categoria → preset) ficava
por baixo da janela do plugin com ela grande.

**Sem estalo:** os knobs andam em rampa (~20 ms, inclusive o DRIVE), o ganho do ACCENT sobe e desce em ~4 ms (antes
pulava na hora, com a nota ainda soando), o filtro abre em ~1,5 ms, o filtro zera quando a nota vem do silêncio e um
passa-altas de 12 Hz depois do drive tira o DC do TUBE.

## Fontes

| Arquivo | O que é |
|---|---|
| `Params.h` | ids dos parâmetros, listas e o padrão (um inteiro atômico por passo) |
| `Engine.h` | voz (osc polyBLEP, ladder ZDF com saída no 3º degrau, envelopes, accent, slide) e efeitos |
| `Presets.h`, `presets/fabrica.inc` | categorias, presets de fábrica (som + padrão em texto; o .inc é gerado) e o ALEATORIZAR |
| `tools/` | `gerar-presets.py` (banco de fábrica) e `montar-banco.py` (.lmobank a partir do USER) |
| `PluginProcessor.*` | relógio do host, sequenciador com swing, MIDI, estado, presets (fábrica, bancos, USER) |
| `PluginEditor.*`, `LimaoUi.h` | tela (painel branco, knob com escala em tinta preta centrada na tampa, visores, passos, osciloscópio, menu de presets) |
| `Theme.h`, `Widgets.h` | os da casa, iguais aos dos outros plugins |
