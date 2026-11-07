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
| BENDER | pitch shifter de pedal, duas cabeças com janela sin² (latência só no som deslocado, 16-32 ms) | PEDAL, SHIFT (7 intervalos), MIX | WHAMMY/DETUNE |

## AMP e CAB

BLONDE 65 (clean americano), CHIME 30 (classe A), PLEX 68 (crunch britânico), CITRUS (britânico encorpado),
IRON HG (high-gain moderno). GAIN, BASS, MID, TREBLE, PRESENCE, MASTER, CLEAN/DIRTY. CAB 1x12 OPEN / 2x12 / 4x12,
mic DYN/RIBBON, MIC POS (centro → borda), DISTANCE (perto → longe).

## Presets de fábrica

Os nomes brincam com o do guitarrista (nunca o nome exato) e não citam marca. Referência de cada um:

| Preset | Inspirado em | Board | Amp |
|---|---|---|---|
| BLUES CRUNCH | o esboço aprovado | SQUASH, HUMP, (GRIT), (TIDE), ECHO, HALO plate | CITRUS dirty |
| JHON MORELO | Tom Morello | (QUACK), BENDER +2 OCT no calcanhar, (SWIRL), ECHO slap | PLEX 68 dirty, médio alto |
| SDASH | Slash | (QUACK), ECHO, HALO plate | PLEX 68 dirty, ganho alto |
| JHON FRUSHANTE | John Frusciante | (GRIT), (QUACK), TIDE chorus, (ECHO tape) | PLEX 68 clean no limite |
| DAVID GILMOR | David Gilmour | SQUASH, WOOLLY SI, (TIDE), ECHO tape ~440 ms, HALO hall | BLONDE 65 clean |
| JIMMY HENDRIQUES | Jimi Hendrix | (QUACK), WOOLLY GE, (TIDE vibrato), HALO spring | PLEX 68 dirty, master alto |
| LINKIN PARQUE | Linkin Park (Brad Delson) | HUMP como boost | IRON HG dirty, gate firme |
| TONY IOMI | Tony Iommi | CLEARCUT como treble booster, HALO plate | CITRUS dirty, escuro |
| ZAKK WAILD | Zakk Wylde | (QUACK), HUMP como boost, (SWIRL), (TIDE) | PLEX 68 dirty no talo |
| KURT COBAINHO | Kurt Cobain | GRIT classic, TIDE chorus fundo, HALO plate | BLONDE 65 clean, gate |
| THE EJ | The Edge | SQUASH, ECHO 375 ms (1/8. a 120), HALO hall | CHIME 30 clean |
| STEVIE RAY VAUGHAM | Stevie Ray Vaughan | HUMP pouco drive/muito nível, (TIDE vibrato), HALO spring | BLONDE 65 clean |
| EDDIE VAN HALLEN | Eddie Van Halen | SWIRL vintage lento, (ECHO), HALO plate | PLEX 68 dirty no talo |
| JACK WHITTE | Jack White | WOOLLY SI, BENDER -1 OCT no bico | PLEX 68 dirty |
| KIRK HAMETT | Kirk Hammett | (QUACK), HUMP como boost, (ECHO) | IRON HG dirty, médio cavado |
| MARK KNOPFLA | Mark Knopfler | SQUASH, CLEARCUT leve, ECHO curto, HALO plate | BLONDE 65 clean |
| BILLY CORGANN | Billy Corgan | WOOLLY SI + GRIT modern (parede de fuzz) | CITRUS dirty |
| BRIAN MAIO | Brian May | CLEARCUT treble booster, ECHO, HALO plate | CHIME 30 dirty |
| ANGUS YOUNGUE | Angus Young | HALO plate leve | PLEX 68 dirty, ganho médio |
| CARLOS SANTANNA | Carlos Santana | HUMP, ECHO tape, HALO hall | CITRUS dirty, médio gordo |
| JONNY GREENWOODY | Jonny Greenwood | GRIT modern, (BENDER +1 OCT), (ECHO tape) | BLONDE 65 clean |
| AMBIENT SWELL | — | SQUASH, TIDE, ECHO tape longo, HALO hall | BLONDE 65 clean |
| CHIME JANGLE | — | SQUASH, HALO spring | CHIME 30 clean |

Entre parênteses: o pedal está no board, desligado (é só pisar).
## Código

- `Source/Dsp.h`: motor sem JUCE (pedais, amp, cab, gate, oversampling meio-banda IIR 2x/4x sem latência).
- `Source/Presets.h`: presets de fábrica (sem JUCE). `Source/Pedais.h`: nomes, knobs, chaves e cores.
- `Source/PluginEditor.cpp` + `EditorPedal.cpp` + `EditorPainel.cpp` + `EditorMouse.cpp`: a tela (base 1600 × 900).
- `tools/teste_dsp.cpp`: bancada (meio-banda, cada pedal nos extremos, aliasing, bypass, SYNC, cauda do reverb, afinação do BENDER,
  amps, presets e CPU). Rodar:
  `zig c++ -O2 -std=c++17 -target x86_64-windows-gnu tools/teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe`
