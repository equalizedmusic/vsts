# MESA

Console analógica em plugin (efeito, VST3/AU), em dois plugins do mesmo código:

- **MESA**, a console. Vai no **master** do DAW. A janela é o mixer inteiro:
  - à esquerda, a barra lateral;
  - em cima, o visor do canal selecionado;
  - no meio, todos os canais lado a lado (a roda do mouse passa; GERAL encolhe para caber todos);
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

## Módulos
| Módulo | Modelos | Controles |
|---|---|---|
| PRÉ | LIMPO, API 312 (operacional discreto: 3ª harmônica), BRIT 73 (classe A: 2ª harmônica, grave satura antes) | GANHO ±20 dB, DRIVE, PAD -20 dB, HPF 18 dB/oit |
| DINÂMICA | VCA (canal), COLA (grupo) | LIMIAR, RAZÃO, GATE (canal) / MIX (grupo), PRÉ (dinâmica antes do EQ) + LEDs de redução |
| EQ | E-4K (Q proporcional ao ganho; chapa escura), BRIT 81 (frequências em degraus, indutor; chapa cinza) | HF, HMF, LMF, LF (ganho e frequência), IN |
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
