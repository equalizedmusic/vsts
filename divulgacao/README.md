# Divulgação

Scripts que geram o material de divulgação a partir das fotos das interfaces (branch `shots`). Nada daqui entra no
build dos plugins nem dispara build.

Preparar: `npm install` nesta pasta (instala o puppeteer, que baixa o Chrome dele) e ter o `ffmpeg` no PATH (ou
apontar a variável `FFMPEG`). Para usar outro Chrome, aponte a variável `CHROME`.

As fotos não ficam versionadas aqui: baixe os `.png` da branch `shots` para `video/img/` (vídeos) e para
`shots-img/` (carrossel), com o mesmo nome (`CHAMA.png`, `CONSOLE.png`…). Exemplo:
`git --work-tree=video/img checkout origin/shots -- .` (depois apague os `.log` que vierem junto).

| Pasta | O que gera | Como |
|---|---|---|
| `video/` | Stories 1080×1920 por pacote (`?p=mix`, `criativo`, `synth`) e de 60 s por plugin (`?p=plugin&n=NOME`, roteiros em `tours-*.js`) | `node render.js <p> <saida.mp4>`; trilha com `python audio.py <p> <saida.wav>`; os 3 de 60 s com trilha: `grava60.sh` |
| `carrossel/` | Slides 1080×1350 para o Instagram | `node render.js <pasta de saída> [analog]` |
| `mockup/` | Estudos de layout em HTML (anteriores ao visual de hardware) | `node shot.js` e afins |

A prévia 3D dos pedais da SÉRIE KNOB (Blender) fica em `plugins/knob/_comum/tools/`.
