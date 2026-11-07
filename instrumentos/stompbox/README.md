# STOMPBOX

Pedalboard de guitarra em plugin (InsideStudioSP). Vai no canal da guitarra (DI). Até 10 pedais em série, AMP + CAB
no fim e NOISE GATE na entrada. Versão de TESTE: sem ativação por licença e fora do site.

## Cadeia

`INPUT → NOISE GATE → pedal 1 → … → pedal 10 → AMP → CAB → OUTPUT`

- Os pedais ficam encostados à esquerda; a posição livre depois do último tem o **+** (abre o seletor).
- Arrastar um pedal troca a ordem. Botão direito: trocar, duplicar, knobs no padrão, mover, remover.
- Footswitch liga/desliga (LED), com crossfade (sem estalo). Pedal desligado não gasta CPU.
- Parâmetros por posição para automação: `Slot N Pedal / On / Knob 1..4 / Switch`.

## Pedais

| Pedal | O que é | Knobs | Chave |
|---|---|---|---|
| SQUASH | compressor OTA | LEVEL, SUSTAIN, ATTACK, BLEND | — |
| HUMP | overdrive com corcova de médios (clipa só acima de 720 Hz) | DRIVE, LEVEL, TONE | — |
| CLEARCUT | overdrive transparente (limpo + clipado) | GAIN, TREBLE, VOLUME, BASS | HEADROOM LOW/HIGH |
| GRIT | distorção de op-amp com diodos para a terra | DIST, FILTER, VOLUME | CLASSIC/MODERN |
| WOOLLY | fuzz de dois transistores (redondo) | VOLUME, FUZZ | GE/SI |
| QUACK | wah (o pedal arrasta para cima/baixo) | PEDAL, Q, RANGE | PEDAL/AUTO |
| SWIRL | phaser de 4 estágios | SPEED, FEEDBACK | VINTAGE/MODERN |
| TIDE | chorus/vibrato de BBD (estéreo) | RATE, DEPTH, MIX | CHORUS/VIBRATO |
| ECHO | delay BBD ou fita (estéreo, SYNC) | TIME, FEEDBACK, MIX, MOD | BBD/TAPE |
| HALO | reverb (estéreo) | DECAY, TONE, MIX, PRE-DLY | SPRING/PLATE/HALL |

## AMP e CAB

BLONDE 65 (clean americano), CHIME 30 (classe A), PLEX 68 (crunch britânico), CITRUS (britânico encorpado),
IRON HG (high-gain moderno). GAIN, BASS, MID, TREBLE, PRESENCE, MASTER, CLEAN/DIRTY. CAB 1x12 OPEN / 2x12 / 4x12,
mic DYN/RIBBON, MIC POS (centro → borda), DISTANCE (perto → longe).

## Presets de fábrica

Os nomes não citam artista; a referência de cada um:

| Preset | Inspirado em | Board | Amp |
|---|---|---|---|
| BLUES CRUNCH | o esboço aprovado | SQUASH, HUMP, (GRIT), (TIDE), ECHO, HALO plate | CITRUS dirty |
| KILL SWITCH | Tom Morello | (QUACK), (SWIRL), ECHO slap | PLEX 68 dirty, médio alto |
| TOP HAT | Slash | (QUACK), ECHO, HALO plate | PLEX 68 dirty, ganho alto |
| FUNK EDGE | John Frusciante | (GRIT), (QUACK), TIDE chorus, (ECHO tape) | PLEX 68 clean no limite |
| SKY LEAD | David Gilmour | SQUASH, WOOLLY SI, (TIDE), ECHO tape ~440 ms, HALO hall | BLONDE 65 clean |
| '67 FUZZ | Jimi Hendrix | (QUACK), WOOLLY GE, (TIDE vibrato), HALO spring | PLEX 68 dirty, master alto |
| HYBRID WALL | Linkin Park (Brad Delson) | HUMP como boost | IRON HG dirty, gate firme |
| DOOM RIFF | Tony Iommi | CLEARCUT como treble booster, HALO plate | CITRUS dirty, escuro |
| BERSERKER | Zakk Wylde | (QUACK), HUMP como boost, (SWIRL), (TIDE) | PLEX 68 dirty no talo |
| AMBIENT SWELL | — | SQUASH, TIDE, ECHO tape longo, HALO hall | BLONDE 65 clean |
| CHIME JANGLE | — | SQUASH, HALO spring | CHIME 30 clean |

Entre parênteses: o pedal está no board, desligado (é só pisar). O Whammy do Morello (pitch shifter) não existe no
STOMPBOX; o KILL SWITCH fica sem ele.

## Código

- `Source/Dsp.h`: motor sem JUCE (pedais, amp, cab, gate, oversampling meio-banda IIR 2x/4x sem latência).
- `Source/Presets.h`: presets de fábrica (sem JUCE). `Source/Pedais.h`: nomes, knobs, chaves e cores.
- `Source/PluginEditor.cpp` + `EditorPedal.cpp` + `EditorPainel.cpp` + `EditorMouse.cpp`: a tela (base 1600 × 900).
- `tools/teste_dsp.cpp`: bancada (meio-banda, cada pedal nos extremos, aliasing, bypass, SYNC, cauda do reverb,
  amps, presets e CPU). Rodar:
  `zig c++ -O2 -std=c++17 -target x86_64-windows-gnu tools/teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe`
