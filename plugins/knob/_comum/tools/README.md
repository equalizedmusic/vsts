# Prévia 3D da SÉRIE KNOB

Foto "de produto" de cada pedal, renderizada no Blender (Cycles). Não entra no build dos plugins: serve para
divulgação e para conferir cor, madeira e lâmpadas antes de mexer no editor.

- `pedal.py`: monta a cena e renderiza um pedal.
  `blender -b -P pedal.py -- <saida.png> <pedal> [valor 0-10 | -] [amostras] [escala%] [aco|preto]`
- `lote.ps1 -Blender <blender.exe>`: os 10 pedais em `final/`; `grade.ps1`: junta os 10 numa grade 5×2.
- `texturas/`: nogueira (cor e rugosidade) e HDRI de estúdio, do Poly Haven (CC0). Sem elas o script roda, mas a
  madeira sai lisa.

Testado com o Blender 4.2 LTS. As letras usam a Segoe UI do Windows; fora do Windows, a fonte padrão do Blender.
