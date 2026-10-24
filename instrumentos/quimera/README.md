# QUIMERA — sintetizador

Sintetizador polifônico (VSTi / AU) que junta o lado metálico e digital das wavetables com o lado orgânico
de um sintetizador analógico mono clássico: deriva de afinação por voz, filtro ladder que auto-oscila, glide e legato.
Todas as tabelas são geradas por código (nada copiado de outro synth). Suporta **MPE**.

## Blocos

| Aba | Bloco | O que tem |
|---|---|---|
| SYNTH | OSC 1–3 | 24 tabelas de 32 quadros: **Analógico** (Clássica, Pulso, Serra Dupla, Triângulo Dobrado), **Modelado** (Metal, Formante, Sincronizado, Dobra, Elétrico, Quadrada-Serra, Seno-Quadrada, Escova) e **Wavetable** (Cristal, Digital, Vocal, Harmônicos, Granito, Sinos, FM Linear, FM Metálico, Espectral, Órgão, Corda, Quimera). POSITION, 11 warps (Sync, Bend ±, PWM, Mirror, Fold, Quantize, FM e RM do oscilador anterior, Asym), OCTAVE/SEMI/FINE, UNISON até 8× com DETUNE e WIDTH, fase aleatória (Random Phase), LEVEL, PAN e OUTPUT (Filter A, Filter B, A + B ou Direct) |
| | SUB + NOISE | Sub de 1 ou 2 oitavas (Sine, Triangle, Square); noise com cor |
| | FILTER A / B | Ladder 24 e 12, LP/HP/BP 12, Notch, Comb e Formant; CUTOFF, RES, DRIVE, KEY, ENV 2 e DRY/WET; em SERIAL A → B ou PARALLEL |
| | ENV 1–3 | ADSR com curva. Env 1 = volume; Env 2 = filtros (e MOD MATRIX); Env 3 = só MOD MATRIX |
| | LFO 1–2 | 7 formas, sync no grid ou Hz, modo Free / Retrig / Envelope, RISE |
| | VOICE | Poly / Mono / Legato, até 16 VOICES, GLIDE, BEND, DRIFT (deriva analógica), VELOCITY no volume |
| MOD MATRIX | 16 slots | SOURCE: envelopes, LFOs, Velocity, Note, Mod Wheel, **Pressure**, **Slide**, Bend, Random, 4 macros. DEST: Pitch/Position/Warp/Level/Pan de cada osc, Cutoff/Res/Drive dos filtros, Volume, Pan, Rate dos LFOs, Unison Detune |
| FX | rack | HYPER → DISTORTION → PHASER → CHORUS → DELAY (sync, ping-pong) → REVERB (room, plate, hall) → EQ → COMPRESSOR (single ou multiband) |
| ARP | arpejador | ARP ON, MODE (Up, Down, Up/Down, Down/Up, Random, As Played, Chord, Converge, Diverge), SCALE + ROOT, RATE (grid do host), OCTAVES 1–4, GATE (100% emenda: com VOICE em Legato vira slide), SWING, LENGTH 1–16, ACCENT; 16 passos com liga/pausa e acento; 8 padrões prontos |

**ARP:** fica na frente do motor (`Source/Arp.h`): troca as teclas pelas notas do arpejo, e o resto do MIDI passa
direto. Com o host tocando, o passo 1 cai no começo do compasso; parado, o relógio começa na primeira tecla.
SEGURAR trava o acorde, e com o arpejo ligado o pedal (CC64) não segura as notas. **SCALE + ROOT** encaixam as
teclas na escala (`Source/Scales.h`, o mesmo arquivo do LIMÃO); com uma tecla só, o arpejo sobe pela escala a
partir dela. Os parâmetros do arpejo entram no preset.

**MPE:** com o botão MPE ligado, cada nota tem o próprio bend (até 96 st), pressure e slide (CC74), que entram na MOD MATRIX.
Sem MPE, o aftertouch vira Pressure. Trocar de preset não mexe em MPE, MPE Bend e SEGURAR.

**Presets:** o menu abre primeiro os bancos e depois as categorias.
- **FACTORY 1**: Init e 40 por categoria — BASS, LEAD, PLUCK, PAD, ATMOS, PERC, DRUMS, KEYS, FX e ARP (401). Os
  40 de ARP (com o arpejo ligado) são gerados por `tools/gerar-arp.py` em `Source/presets/arp.inc`; ARP é a
  última categoria para não mudar o índice das outras.
- **Bancos avulsos** (FACTORY 2, …): aparecem embaixo do FACTORY 1, cada um com as suas categorias.
- **USER**: os presets do usuário, com Salvar como / Salvar alterações / Renomear / Apagar, como nos efeitos da casa.

**Banco avulso (`.qmrbank`):** a pessoa baixa o arquivo, abre o menu de presets > *Importar banco de presets…* e
escolhe o arquivo. O QUIMERA copia o banco para a pasta dele (Windows: `%APPDATA%\InsideStudioSP\QUIMERA\Banks`;
Mac: `~/Library/Audio/Presets/InsideStudioSP/QUIMERA/Banks`); importar de novo um banco com o mesmo nome substitui a
versão anterior. *Remover banco* manda o arquivo para a lixeira. O formato é XML:

```xml
<QUIMERABANK format="1" name="Factory 2" author="InsideStudioSP">
  <PRESET name="Grave Novo" category="BASS">
    <PARAM id="facut" value="220"/>   <!-- valor real, igual ao preset salvo pelo plugin -->
  </PRESET>
</QUIMERABANK>
```

Para montar um banco: salve os presets no QUIMERA (vão para USER), separe os `.xml` em pastas com o nome da
categoria (`BASS`, `LEAD`, …) e rode `python tools/montar-banco.py <pasta> "Factory 2"`. O que o preset não traz
volta ao padrão; MPE, MPE Bend e SEGURAR são ignorados.

Janela de 75% a 200% (base 1200×740). Os menus (presets e seletores) abrem **dentro** da janela do plugin: como
janela solta, a lista de dentro (banco → categoria → preset) ficava por baixo da janela do plugin com ela grande.

## Build

Projeto CMake próprio (`instrumentos/CMakeLists.txt`) e workflow `build-instrumentos.yml`: instalador
`InsideStudioSP-Instrumentos-win.exe` / `-mac.dmg` no release `latest-instrumentos` (logs no branch
`builds-instrumentos`). Versão e notas: `novidades/Quimera.md` (ver o README da raiz).
