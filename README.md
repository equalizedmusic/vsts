# vsts

Plugins de áudio da **InsideStudioSP**, feitos com [JUCE](https://juce.com). Os plugins ficam
separados por linha: `plugins/criativo/<plugin>` e `plugins/mix/<plugin>`.

## Linha criativa

| Plugin | O que é |
|---|---|
| [GRÃO](plugins/criativo/grao/README.md) | Efeito granular de textura (fita de 4 s, grãos, feedback, cauda, XY) |
| [CALEIDO](plugins/criativo/caleido/README.md) | Glitch rítmico (mandala de 16 passos, sync com o BPM) |
| [ESPIRAL](plugins/criativo/espiral/README.md) | Phaser/flanger infinito "barber pole" (sobe sem fim, sync com o BPM) |
| [FITA](plugins/criativo/fita/README.md) | Gravador de fita: saturação, wow/flutter, chiado, rolos girando e TAPE STOP no BPM |
| [CHAMA](plugins/criativo/chama/README.md) | Build-up de um knob só: passa-alta, estéreo, ruído, reverb e gate no tempo, com fogo na tela |
| [F1](plugins/criativo/f1/README.md) | Filtro ladder de 4 degraus (passa-baixa 12/24 dB) com envelope, LFO no grid e FUMAÇA |
| [RINGER](plugins/criativo/ringer/README.md) | Ring modulator: portadora de tremolo a sino metálico, LFO no grid e osciloscópio |
| [REPETECO](plugins/criativo/repeteco/README.md) | Delay analógico tipo BBD: escurece a cada volta, auto-oscila, INFINITO e MATAR |
| [MIRAGEM](plugins/criativo/miragem/README.md) | Reverb atmosférico com shimmer (oitava/quinta), CONGELAR, REVERSO e duck |
| [PICOTE](plugins/criativo/picote/README.md) | Gate rítmico de 16 passos com volume, pan e filtro por passo, EMBARALHAR e PRENDER |

## Linha de mix

| Plugin | O que é |
|---|---|
| [PRISMA](plugins/mix/prisma/README.md) | Multibanda: 4 bandas com compressor e saturação cada, cortes arrastáveis no espectro |
| [TORNO](plugins/mix/torno/README.md) | Compressor que mostra o que está apertando: histórico de nível e redução, curva com joelho |
| [BARRAGEM](plugins/mix/barragem/README.md) | Limiter de master com true peak e LUFS, com alvos de entrega (Spotify, Apple Music…) |
| [CONSOLE](plugins/mix/console/README.md) | Cor de canal de mesa analógica, com 3 modelos: Cristal, Ferro e Brasa |
| [SOCO](plugins/mix/soco/README.md) | Modelador de transiente: ataque e sustain de cada golpe, com o antes e o depois na tela |
| [LIXA](plugins/mix/lixa/README.md) | De-esser de faixa ajustável que mostra no espectro o que está tirando |
| [HORIZONTE](plugins/mix/horizonte/README.md) | Imagem estéreo em M/S, sem latência: largura de graves e agudos, grave em mono, vetorscópio |
| [SALA](plugins/mix/sala/README.md) | Reverb limpo e transparente para colocar tudo num mesmo espaço |
| [MONITOR](plugins/mix/monitor/README.md) | Medidor de entrega: LUFS, LRA, true peak, espectro e correlação, sem mexer no som |
| [NAVALHA](plugins/mix/navalha/README.md) | Clipper 4× que mostra na forma de onda, ao vivo, o que está sendo cortado |

## Instrumentos

Ficam em `instrumentos/<nome>` e têm **build próprio** (`.github/workflows/build-instrumentos.yml`): só roda quando
`instrumentos/` muda, e um push só nos instrumentos não refaz o build dos efeitos (e vice-versa).
O instalador (`InsideStudioSP-Instrumentos-win.exe` / `-mac.dmg`) sai no release `latest-instrumentos`; os logs, no branch `builds-instrumentos`.

| Instrumento | O que é |
|---|---|
| [QUIMERA](instrumentos/quimera/README.md) | Sintetizador de 3 osciladores wavetable (analógico, modelado e wavetable), filtro ladder, matriz de 16 slots, MPE, 8 efeitos e 180 presets |

Os efeitos têm 20 presets de fábrica e todos têm janela redimensionável: arraste o canto inferior direito ou use
*Tamanho da janela* no menu de presets (75% a 200%). O tamanho fica salvo no projeto.

No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também *Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

## Formatos gerados

| | Mac (Intel + Apple Silicon) | Windows 64 bits |
|---|---|---|
| VST3 | sim | sim |
| AU (Logic/GarageBand) | sim | — |

## Como compilar

Não precisa de Mac nem de Visual Studio: todo push na `main` dispara o GitHub Actions
(`.github/workflows/build.yml`), que compila no Mac e no Windows, roda o
[pluginval](https://github.com/Tracktion/pluginval) e publica:

- **Release `latest-build`**: um instalador por linha e por sistema: `InsideStudioSP-Criativo-win.exe` /
  `-mac.dmg` e `InsideStudioSP-Mix-win.exe` / `-mac.dmg` (montados por `instalador/montar.py`; nomes e
  descrições da lista em `instalador/catalogo.json`);
- **branch `builds`**: os logs de compilação e do pluginval e o `status.txt`.

Push só em `.md` não dispara build. Um push novo cancela o build que estiver rodando.

## Instalar

**Windows:** abra o `.exe`. A lista vem com todos os plugins marcados (desmarque os que não quiser), e a
pasta padrão é a do VST3 (`C:\Program Files\Common Files\VST3`), que pode ser trocada. Para desinstalar:
*Configurações → Aplicativos*.

**Mac:** abra o `.dmg` e o `.pkg` dentro dele. Na lista, desmarque o que não quiser; no passo *Destino* dá
para instalar para todos os usuários (`/Library/Audio/Plug-Ins`, padrão) ou só para você
(`~/Library/Audio/Plug-Ins`). O VST3 vai para `VST3/` e o AU para `Components/`. O `LEIA-ME.txt` do `.dmg`
explica como desinstalar.

Os instaladores ainda não são assinados. No Windows, o SmartScreen pede *Mais informações → Executar assim
mesmo*; no Mac, libere em *Ajustes do Sistema → Privacidade e Segurança → Abrir Mesmo Assim*.

No Ableton: *Settings → Plug-Ins*, ligue *Use VST3 Plug-In System Folders* e clique em *Rescan*.

## Plugin novo

1. Copie um plugin da mesma linha para `plugins/<linha>/<nome>` (ex.: `plugins/criativo/repeteco` → `plugins/criativo/<nome>`).
2. No `CMakeLists.txt` dele, troque o nome do alvo, `PRODUCT_NAME`, `BUNDLE_ID` e `PLUGIN_CODE`
   (4 caracteres, com pelo menos uma maiúscula, e **diferente de todos os outros plugins**).
3. Em `instalador/catalogo.json`, acrescente o nome e a descrição que aparecem na lista do instalador.
4. Faça o push. O CMake da raiz encontra a pasta sozinho, e o build põe o plugin no instalador da linha pela pasta.
