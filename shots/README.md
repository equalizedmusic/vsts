# Fotos reais das interfaces

Projeto CMake separado: um executável por plugin abre o editor, toca um loop e salva a foto (`<NOME>.png`).
O workflow `shots.yml` compila no Mac e publica as fotos no branch `shots`. Mexer em `shots/**` refaz só as fotos
(não dispara o build dos plugins nem dos instrumentos).
