# MONTA-MONTA

Synth didático: as peças de um Minimoog de 2 osciladores ficam numa bandeja e o usuário monta o caminho do som no
rack, como um quebra-cabeça. Cada peça ensina o que faz. Textos em português e inglês (botão PT/EN).

## Peças

| Peça | O que é | Controles |
|---|---|---|
| VCO | oscilador, onde o som nasce | RANGE (32' a 2'), WAVEFORM (as 6 formas do Minimoog), TUNE |
| VCO 2 | segundo oscilador | RANGE, WAVEFORM, TUNE (desafinar engorda o som) |
| MIXER | mistura OSC 1, OSC 2 e NOISE | volume e chave liga/desliga de cada um |
| VCF | filtro ladder de 24 dB/oit | CUTOFF, EMPHASIS, CONTOUR (quanto o ENV MOD mexe no corte) |
| ENV AMP | envelope de volume | ATTACK, DECAY, SUSTAIN, RELEASE |
| ENV MOD | envelope do filtro | ATTACK, DECAY, SUSTAIN, RELEASE |
| SAÍDA | saída de áudio | VOLUME |

## Lições

1. **O primeiro som:** VCO + SAÍDA. As peças que ainda não entraram viram PONTE (o som passa direto). Sem ENV AMP o
   oscilador toca sem parar.
2. **O volume que respira:** entra o ENV AMP.
3. **O filtro:** entra o VCF.
4. **O filtro que se mexe:** entra o ENV MOD, embaixo do VCF.
5. **Dois osciladores:** entram o MIXER e o VCO 2.
6. **Modo livre:** o synth completo.

A lição termina quando todas as peças pedidas estão encaixadas, o som chega na SAÍDA e uma nota é tocada. O progresso
fica salvo no computador (vale para todos os projetos). No MENU: *Liberar todas as lições e os cabos* e *Recomeçar as
lições do zero*.

## Modo CABOS

Libera depois da lição 5. Todas as peças ficam no rack com tomadas: saídas (OUT, e MOD no ENV MOD) e entradas (IN, IN 1,
IN 2 e CUT no VCF). O usuário arrasta fios de uma saída até uma entrada, em qualquer ordem. Fio que faria laço, ENV MOD
numa entrada de áudio e som na entrada CUT são recusados com explicação. *LIGAR O PADRÃO* liga o caminho clássico.

## Tocar

Monofônico, a última nota segurada manda; teclado na tela (Dó 3 a Si 4) ou MIDI do DAW. *OUVIR EXEMPLO* no cartão
toca um exemplo de 3 segundos do que a peça faz (sempre no synth completo, sem mexer nos knobs do usuário).

## Código

- `Source/Rack.h`: peças, lições, a montagem (`Patch`) e o caminho do som (`Graph`) que o motor toca.
- `Source/Engine.h`: VCOs com polyBLEP, ruído, ladder TPT de 4 polos, dois ADSR; calcula só as peças montadas.
- `Source/Lessons.h`: todos os textos (PT e EN).
- `Source/PluginEditor.cpp` (tela, rack, lições) e `Source/Panels.cpp` (peças, bandeja, fios, cartão, teclado).
