# CURVA — novidades

## 0.1.0 — 2026-11-21
- Primeira versão da CURVA, para TESTE (sem ativação por licença e fora do site): monitor de volume pela curva de
  mesma audibilidade (Fletcher-Munson, ISO 226:2003). Vai por ÚLTIMO no master, só para ouvir.
- LISTEN LEVEL (40 a 90 phon): o volume em que você está ouvindo agora. Atalhos NOITE 50, BAIXO 60, MÉDIO 70 e REF 83,
  e o nome do volume embaixo do knob (MADRUGADA, BAIXINHO, CONVERSA, MÉDIO, REFERÊNCIA).
- TARGET (60 a 90 phon, padrão 83): o volume em que a mix tem que soar certa. A CURVA devolve o que o ouvido perde
  entre um e outro, normalizada em 1 kHz. Abaixo de 20 Hz a correção some até 10 Hz (não empurra subsônico), e fica
  entre −12 e +20 dB.
- AMOUNT dosa a correção. OUTPUT de −24 a +12 dB. AUTO GAIN segura o volume geral igual ao da mix (ruído rosa igual
  antes e depois). SAFE LIMIT protege os falantes com teto em −0,3 dBFS. ON/OFF troca sem estalo.
- Filtro de fase mínima (FIR ~85 ms, refeito quando um knob muda): latência zero e sem pré-eco.
- SPECTRUM: FFT de 8192 da mix (IN) e do que vai para o monitor (OUT), suavização de 1/3, 1/6, 1/12 ou 1/24 de
  oitava, TILT de 0, 3 ou 4,5 dB/oitava, PEAK HOLD, regiões SUB a AIR, o pico com Hz e nota, e a correção em magenta
  por cima (eixo da direita). Com o mouse em cima: Hz, nota, dB de IN e OUT e quanto a CURVA mexe ali.
- LEVELS: pico, RMS e hold de IN e OUT (L/R) e a redução do SAFE.
- LOUDNESS da mix, antes da CURVA (é o que vai para o arquivo): INTEGRATED, MOMENTARY, SHORT-TERM, LRA, TRUE PEAK
  máximo e PLR (EBU R128 / BS.1770-4), com RESET.
- EQUAL LOUDNESS: as duas curvas ISO 226 (a do LISTEN e a do TARGET) e quanto cada faixa ganha (SUB a AIR).
- STEREO da mix: vectorscope, correlação, largura e balanço.
- Aviso fixo no alto: SÓ PARA O MONITOR, desligue antes de exportar.
