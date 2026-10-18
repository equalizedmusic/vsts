# vsts

Plugins de áudio feitos com [JUCE](https://juce.com). Cada pasta em `plugins/` é um plugin.

| Plugin | O que é |
|---|---|
| [GRÃO](plugins/grao/README.md) | Efeito granular de textura (fita de 4 s, grãos, feedback, cauda, XY) |

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

- **Release `latest-build`**: `vsts-mac.zip` e `vsts-win.zip`, com uma pasta por formato;
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
