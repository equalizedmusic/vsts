# MAPA — novidades

App standalone (Mac e Windows) da InsideStudioSP: lê uma música de referência e monta o projeto pronto na sua DAW.

## 0.2.0

Versão de TESTE com IA: o MAPA agora separa a música e transcreve as notas de verdade.

- **Separação em stems** (Demucs v4): BATERIA, BAIXO, VOZ e OUTROS. Cada stem vira uma faixa de áudio no projeto, alinhada com a referência e mutada (é só desmutar para ouvir). Na prévia, M e S tocam os stems.
- **Notas transcritas** (Basic Pitch) no lugar dos palpites da 0.1:
  - BAIXO e a nova faixa VOZ (melodia da voz), uma nota por vez.
  - OUTROS vira LEAD (a linha de cima) e PAD (os acordes longos).
  - Tudo na grade de 1/16, com a força de cada nota.
- **Bateria compasso a compasso**: KICK, CLAP e HATS saem do stem de bateria, com o que tocou em cada compasso e a força de cada golpe. Corrige o kick "dobrado" da 0.1: o grave do baixo (psy, rolling) não vira mais kick. O clique do kick também não vira mais clap.
- **Tom pelas notas transcritas** (o baixo pesa mais).
- **Partes pela energia de cada stem**: acha as viradas mesmo quando o kick não para (ex.: trance vocal). Nesse caso as partes se chamam VOCAL e INSTRUMENTAL.
- A análise leva de 1 a 4 minutos, conforme o computador e o tamanho da música. O app ficou maior: a IA vai junto e roda sem internet.
- Mac: precisa do macOS 13.3 ou mais novo.

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
