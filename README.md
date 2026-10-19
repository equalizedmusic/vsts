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
| [NAVALHA](plugins/mix/navalha/README.md) | Clipper 4× que mostra na forma de onda, ao vivo, o que está sendo cortado |

Todos têm 20 presets de fábrica e janela redimensionável: arraste o canto inferior direito ou use
*Tamanho da janela* no menu de presets (75% a 200%). O tamanho fica salvo no projeto.

No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também *Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

## Formatos gerados

| | Mac (Intel + Apple Silicon) | Windows 64 bits |
|---|---|---|
| VST3 | sim | sim |
| AU (Logic/GarageBand) | sim | — |
| VST2 | só com o SDK em `sdk/vst2` | só com o SDK em `sdk/vst2` |

## Como compilar

Não precisa de Mac nem de Visual Studio: todo push na `main` dispara o GitHub Actions
(`.github/workflows/build.yml`), que compila no Mac e no Windows, roda o
[pluginval](https://github.com/Tracktion/pluginval) e publica:

- **Release `latest-build`**: um zip por plugin e sistema (`GRAO-mac.zip`, `CALEIDO-win.zip`…), um pack por linha
  (`InsideStudioSP-Criativo-mac.zip`, `InsideStudioSP-Mix-win.zip`…) e o pack completo (`InsideStudioSP-Pack-mac.zip` / `-win.zip`),
  com uma pasta por formato;
- **branch `builds`**: os mesmos pacotes mais os logs de compilação e do pluginval.

## Instalar

**Mac:** descompacte e copie
- `VST3/*.vst3` para `~/Library/Audio/Plug-Ins/VST3/`
- `AU/*.component` para `~/Library/Audio/Plug-Ins/Components/`

O build não é assinado pela Apple. Libere uma vez no Terminal:

```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/GRAO.vst3 ~/Library/Audio/Plug-Ins/Components/GRAO.component
```

**Windows:** copie `VST3/GRAO.vst3` para `C:\Program Files\Common Files\VST3\`.

No Ableton: *Settings → Plug-Ins*, ligue *Use VST3 Plug-In System Folders* e clique em *Rescan*.

## Plugin novo

1. Copie um plugin da mesma linha para `plugins/<linha>/<nome>` (ex.: `plugins/criativo/repeteco` → `plugins/criativo/<nome>`).
2. No `CMakeLists.txt` dele, troque o nome do alvo, `PRODUCT_NAME`, `BUNDLE_ID` e `PLUGIN_CODE`
   (4 caracteres, com pelo menos uma maiúscula, e **diferente de todos os outros plugins**).
3. Faça o push. O CMake da raiz encontra a pasta sozinho, e o build monta o pack da linha pela pasta.

## VST2

A Steinberg encerrou o VST2 em 2018 e não distribui mais o SDK. Quem tem uma cópia antiga
(`vstsdk2.4`) pode colocar os headers assim, e o build passa a gerar também o VST2:

```
sdk/vst2/pluginterfaces/vst2.x/aeffect.h
sdk/vst2/pluginterfaces/vst2.x/aeffectx.h
```

O SDK não pode ser redistribuído, então **só faça isso com o repositório privado**.
