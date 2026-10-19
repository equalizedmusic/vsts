# vsts

Plugins de áudio da **InsideStudioSP**, feitos com [JUCE](https://juce.com). Cada pasta em `plugins/` é um plugin.

| Plugin | O que é |
|---|---|
| [GRÃO](plugins/grao/README.md) | Efeito granular de textura (fita de 4 s, grãos, feedback, cauda, XY) |
| [CALEIDO](plugins/caleido/README.md) | Glitch rítmico para psytrance (mandala de 16 passos, sync com o BPM) |
| [ESPIRAL](plugins/espiral/README.md) | Phaser/flanger infinito "barber pole" para psytrance (sobe sem fim, sync com o BPM) |
| [FITA](plugins/fita/README.md) | Gravador de fita: saturação, wow/flutter, chiado, rolos girando e TAPE STOP no BPM |
| [CHAMA](plugins/chama/README.md) | Build-up de um knob só: passa-alta, estéreo, ruído, reverb e gate no tempo, com fogo na tela |
| [NAVALHA](plugins/navalha/README.md) | Clipper 4× que mostra na forma de onda, ao vivo, o que está sendo cortado |
| [F1](plugins/f1/README.md) | Filtro ladder de 4 degraus (passa-baixa 12/24 dB) com envelope, LFO no grid e FUMAÇA |
| [RINGER](plugins/ringer/README.md) | Ring modulator: portadora de tremolo a sino metálico, LFO no grid e osciloscópio |
| [REPETECO](plugins/repeteco/README.md) | Delay analógico tipo BBD: escurece a cada volta, auto-oscila, INFINITO e MATAR |

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

- **Release `latest-build`**: um zip por plugin e sistema (`GRAO-mac.zip`, `CALEIDO-win.zip`…) e o pack completo (`InsideStudioSP-Pack-mac.zip` / `-win.zip`), com uma pasta por formato;
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

1. Copie `plugins/grao` para `plugins/<nome>`.
2. No `CMakeLists.txt` dele, troque o nome do alvo, `PRODUCT_NAME`, `BUNDLE_ID` e `PLUGIN_CODE`
   (4 caracteres, com pelo menos uma maiúscula, e **diferente de todos os outros plugins**).
3. Faça o push. O CMake da raiz encontra a pasta sozinho.

## VST2

A Steinberg encerrou o VST2 em 2018 e não distribui mais o SDK. Quem tem uma cópia antiga
(`vstsdk2.4`) pode colocar os headers assim, e o build passa a gerar também o VST2:

```
sdk/vst2/pluginterfaces/vst2.x/aeffect.h
sdk/vst2/pluginterfaces/vst2.x/aeffectx.h
```

O SDK não pode ser redistribuído, então **só faça isso com o repositório privado**.
