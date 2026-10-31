# MESA

Console analógica em plugin (efeito, VST3/AU), em dois plugins do mesmo código:

- **MESA**, a console. Vai no **master** do DAW. A janela é o mixer inteiro:
  - à esquerda, a barra lateral;
  - em cima, o visor do canal selecionado;
  - no meio, todos os canais lado a lado, na ordem das faixas do DAW (a roda do mouse passa; zoom de 50% a 200% com
    - e + ou Ctrl + roda; GERAL encolhe para caber todos);
  - à direita, a seção central: medidor geral com goniômetro, VUs de redução de ganho, COLA BUS do master e os faders
    dos grupos e do master.
- **MESA CANAL**. Vai no **fim de cada faixa** (tipo CANAL) ou no **canal de grupo** (tipo GRUPO, com G1/G2/G3). Processa
  o som da própria faixa e aparece sozinho na console. A janela dele mostra o canal inteiro e, ao lado, a ligação com a
  console, o número, o nome, o grupo, três VUs e os modelos.

<img src="https://raw.githubusercontent.com/equalizedmusic/vsts/shots/MESA.png" width="900" alt="MESA">

## Como funciona
- O **áudio segue o caminho do DAW**: cada MESA CANAL processa a própria faixa, então sends, returns e grupos
  continuam valendo. A console não recebe o áudio dos canais: ela lê e mexe nos parâmetros de cada MESA CANAL, e a
  automação do DAW grava.
- Os dois plugins são binários diferentes (cada um com seu JUCE), então se acham pela **Ponte** (`Source/Ponte.*`): um
  bloco de memória do **processo**. No Windows é memória compartilhada com nome por processo; no Mac, o endereço fica
  numa variável de ambiente do processo. Pela ponte só passam tipos simples, por uma interface virtual (`mesa::Canal`).
  Funciona quando o DAW carrega os plugins no mesmo processo, que é o padrão do Ableton, Logic, Reaper, FL e Studio One.
- SOLO: o canal em solo cala os outros CANAIS (não os grupos), pelo `Registry` do binário MESA CANAL.
- ORDEM na console = `posicao()`: o número escolhido à mão (duplo clique no LED), senão o índice da faixa que o DAW
  manda no VST3 (o JUCE não repassa: o `instrumentos/CMakeLists.txt` remenda o invólucro VST3 para chamar
  `juce::ispIndiceDaFaixa`, só nos alvos com `ISP_INDICE_FAIXA`), senão o número no começo do nome da faixa.
- Medidores: a balística é feita no áudio (`medeSaida`/`medeEntrada`): pico com queda de 26 dB/s, VU de 300 ms
  calibrado para a senoide ler o RMS (0 VU = -18 dBFS), PPM com queda de 12 dB/s. A tela só desenha.

## Módulos
| Módulo | Modelos | Controles |
|---|---|---|
| PRÉ | E-4K (transistor de console britânica: limpo, um fio de 2ª, 3ª no topo), API 312 (operacional discreto: joelho duro, 3ª + 2ª), BRIT 73 (classe A: 2ª; o grave satura antes no transformador). Cada cópia tem tolerância própria entre L e R (0,97–1,03 na curva e na cor) | GANHO ±20 dB, DRIVE (+0 a +20 dB para dentro do circuito, nível compensado), PAD -20 dB, HPF 18 dB/oit, LPF 12 dB/oit (3–20 kHz) |
| DINÂMICA | VCA 4K (comp do canal, feed-forward, SOLTURA no fim = AUTO), COLA 4K (barramento: teclas de razão/ataque/soltura), FET 76 (ataque 20–800 µs, 4/8/12/20/ALL, 2ª harmônica proporcional à redução), OPTO 2A (detector realimentado com agudo realçado, joelho largo, soltura em dois tempos com memória, válvula) | modelo (clicar no visor verde), LIMIAR, RAZÃO, ATAQUE, SOLTURA, GATE (canal), MIX, PRÉ (dinâmica antes do EQ) + LEDs de redução |
| EQ | E-4K (Q no knob; chapa escura), BRIT 81 (frequências em degraus, Q normal/ALTO, indutor; chapa cinza), API 550 (frequências e ganho em degraus, Q proporcional ao ganho). Filtros "casados" (Vicanek): curva do analógico até Nyquist; knobs suavizados | HF, HMF, LMF, LF (ganho e frequência), Q nas médias, BELL no HF e no LF, IN |
| SAÍDA | — | G1–G3, PAN, SOLO, CUT, fader (+10 a -∞) e medidor de LED |
| MASTER (console) | COLA BUS | LIMIAR, GANHO, MIX, ATAQUE, SOLTURA (AUTO), RAZÃO, IN, SC HPF, fader do master |

Na barra lateral da console: VU ou PPM (só a tela) e ECO ou HQ (saturação a 4×), que vale para todos os canais.

## Arquivos
- `Source/Dsp.h`: processamento em C++ puro (medido fora do plugin por `tools/teste_dsp.cpp`).
- `Source/Params.h`: os parâmetros da console e do canal. `Source/PluginProcessor.*`: a classe dos dois plugins
  (`MESA_CONSOLE` no CMake escolhe qual cada binário cria).
- `Source/Ponte.*`: a ponte entre os dois binários. `Source/Registry.h`: os canais de um binário (SOLO).
- `Source/Desenho.h`: as peças em vetor (knob, tecla, LED de 7 segmentos, medidor de LED, VU, fader).
  `Source/PluginEditor.*`: as duas janelas. A parte que não mexe é desenhada numa imagem guardada; medidores e agulhas
  vão por cima a 30 Hz.
- O protótipo do visual (canvas, mesmas medidas e cores) fica em `granular-vst/mockup/mesa-v4`, fora do repositório.
- `tools/*.py` (Blender): o visual renderizado da 0.1/0.2, que não é mais usado.
