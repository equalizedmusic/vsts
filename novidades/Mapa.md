# MAPA — novidades

App standalone (Mac e Windows) da InsideStudioSP: lê uma música de referência e monta o projeto pronto na sua DAW.

## 0.1.0

Primeira versão de TESTE.

- Arraste uma música (WAV, AIFF, MP3 ou FLAC): o MAPA acha o BPM, o tom, o tempo forte e as partes (INTRO, BUILD, DROP, BREAK, GROOVE, OUTRO).
- Prévia com cara de DAW antes de abrir: as partes coloridas, a régua de compassos, a waveform da referência e uma faixa por elemento achado (KICK, BAIXO, HATS, CLAP, LEAD, PAD e FX) já com as notas MIDI desenhadas.
- Ouça antes de abrir: clique numa parte para tocar só ela em loop; M e S nas faixas MIDI tocam as notas achadas por cima da música.
- Detecta as DAWs instaladas e mostra o botão ABRIR NO…:
  - Ableton Live: projeto .als completo (tempo, marcadores, áudio na grade, faixas com cor e nome).
  - Reaper: projeto .rpp completo.
  - Bitwig, Studio One e Cubase 14: arquivo DAWproject.
  - FL Studio, Logic, Pro Tools e outras: pasta com os MIDI, o áudio e um LEIA-ME.
- O áudio da referência entra alinhado: o 1º tempo forte cai em cima de uma barra de compasso.
- Os projetos ficam em Documentos/MAPA/<nome da música>.
- Build de teste: as notas MIDI (que serão do PRO) vão para a DAW liberadas.
