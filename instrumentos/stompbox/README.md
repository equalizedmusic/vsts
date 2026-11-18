# STOMPBOX

> **Versão atual: 0.2.0** (08/11/2026) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Stompbox.md`](../../novidades/Stompbox.md)

![STOMPBOX](https://insidestudiosp.com.br/img/stompbox.webp)

Pedalboard de guitarra em plugin (InsideStudioSP). Vai no canal da guitarra (DI). Até 10 pedais em série, AMP + CAB
no fim e NOISE GATE na entrada. Licença do pacote `stompbox` (e-mail + token na 1ª vez, até 2 computadores; ≡ MENU >
Licença… mostra a licença ativa); sem ativação o som passa direto. Sem latência (só o BENDER, quando pisado).

## Cadeia

`INPUT → NOISE GATE → pedal 1 → … → pedal 10 → AMP → CAB → OUTPUT`

- Os pedais ficam encostados à esquerda; a posição livre depois do último tem o **+** (abre o seletor).
- Arrastar um pedal troca a ordem. Botão direito: trocar, duplicar, knobs no padrão, mover, remover.
- Footswitch liga/desliga (LED), com crossfade (sem estalo). Pedal desligado não gasta CPU.
- Parâmetros por posição para automação: `Slot N Pedal / On / Knob 1..4 / Switch`.

Barra de cima: presets (◀ visor ▶, **SAVE** grava presets do usuário), **A/B** (dois boards na memória; botão direito
copia), **SYNC** (o TIME do ECHO e o SPEED/RATE do SWIRL e do TIDE viram divisões do BPM do DAW), **TUNER** (afinador
grande; a saída fica muda enquanto afina) e **HQ** (drives, fuzz e amp com oversampling 16x; o normal é 8x).
Embaixo: INPUT com o afinador fixo da entrada (nota, barra de cents, não corta o som; clique abre o TUNER grande),
NOISE GATE (THRESHOLD, RELEASE), AMP, CAB e OUTPUT com medidores.

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
mic DYN/RIBBON, MIC POS (centro → borda), DISTANCE (perto → longe). BASS/MID/TREBLE são o tone stack de cada amp,
entre o pré e a potência; o CAB é uma resposta de impulso gerada da curva do falante, sem latência. AMP + CAB tem
botão ON/OFF, para usar um amp de verdade.

## Presets de fábrica

INIT e mais 23. Os nomes brincam com o do guitarrista (nunca o nome exato) e não citam marca. Referência de cada um:

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

- `Source/Dsp.h`: motor sem JUCE (pedais, amp, cab, gate, oversampling meio-banda IIR 8x/16x sem latência, com
  passa-baixa elíptico antes da última decimação).
- `Source/Presets.h`: presets de fábrica (sem JUCE). `Source/Pedais.h`: nomes, knobs, chaves e cores.
- `Source/PluginEditor.cpp` + `EditorPedal.cpp` + `EditorPainel.cpp` + `EditorMouse.cpp`: a tela (base 1600 × 900).
- `tools/teste_dsp.cpp`: bancada (meio-banda, cada pedal nos extremos, aliasing, bypass, SYNC, cauda do reverb, afinação do BENDER,
  amps, presets e CPU). Rodar:
  `zig c++ -O2 -std=c++17 -target x86_64-windows-gnu tools/teste_dsp.cpp -o teste_dsp.exe && teste_dsp.exe`
