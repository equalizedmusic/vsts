# MAPA — novidades

App standalone (Mac e Windows) da InsideStudioSP: lê uma música de referência e monta o projeto pronto na sua DAW.

## 0.4.0

Mapa mais fiel numa música real. Testado com um psytrance de 8 minutos.

- **Bateria lida golpe a golpe**: o MAPA mede a energia de cada semicolcheia no stem de bateria, com janela exata, em vez de uma média borrada de 93 ms.
  - O CLAP do drop voltou. Antes o clique do kick escondia o clap.
  - O KICK filtrado do intro agora aparece.
  - O kick longo do psy e o rolo de kick do build também são lidos.
- **Viradas no compasso certo**: a parte começa onde a bateria e o baixo realmente mudam, mesmo quando a música tem uma frase torta, como um break de 35 compassos. Antes, tudo depois dela ficava 1 compasso fora.
- **Nada de pedaço solto**: LEAD, PAD e VOZ entram e saem em blocos de 4 compassos, contados do começo da parte, como numa produção de verdade. Um bloco só é marcado quando o elemento toca em pelo menos 3 dos 4 compassos. Trechos de 1 ou 2 compassos sumiram.
- **Efeitos só nas viradas**: SWEEP UP, SWEEP DOWN e IMPACTO só aparecem no fim ou no começo de uma parte, ou numa frase de 8 compassos dentro dela. O barulho do drop não vira mais sweep.
- **Medido na música real**: KICK, CLAP, HATS e BAIXO acertaram 99% dos compassos, comparados com a leitura compasso a compasso. Antes, o CLAP acertava 22%.

## 0.3.0

O MAPA virou só o esqueleto da música, e é grátis. Ele não cria nenhuma nota nem melodia. Marca onde cada coisa entra e sai na referência, como um mapa de produção.

- **Uma faixa por elemento, com clipes MIDI vazios**: KICK, CLAP, HATS, BAIXO, LEAD, PAD e VOZ. Cada clipe começa onde o elemento entra e termina onde ele sai, e leva o nome do elemento. As partes vão nos marcadores.
- **Efeitos marcados**: SWEEP UP (o riser antes da virada), SWEEP DOWN (o agudo descendo depois dela) e IMPACTO (o crash ou boom no 1 do compasso).
- **Mais fiel**: o trecho só encosta na virada da parte quando está a 1 compasso dela (antes eram 2). A voz só aparece quando o stem de voz tem corpo próprio: synth que vaza para a voz não vira mais VOZ falsa. LEAD e PAD são separados pelos ataques e pela sustentação do som.
- **Sem stems no projeto**: a separação continua por dentro, só para ler a música. Separar stems a sua DAW já faz.
- **Na prévia**: S numa faixa toca a referência só nos trechos daquele elemento, para você conferir o mapa de ouvido.
- **LEIA-ME com o mapa em texto**: cada elemento com os compassos em que entra e sai (útil no FL Studio, Logic e Pro Tools).
- Medido numa música de teste com gabarito, sem voz: KICK, CLAP, HATS, BAIXO, LEAD e PAD acertaram 100% dos compassos. Os sweeps e o impacto saem no lugar certo, às vezes um pouco mais curtos.

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
