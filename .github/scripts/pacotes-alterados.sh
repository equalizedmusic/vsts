#!/usr/bin/env bash
# Quais pacotes este build compila: SÓ os que foram atualizados no push. Um pacote entra quando muda a pasta dele
# (instrumentos/<pasta>/ ou plugins/<linha>/) ou o novidades/<Pacote>.md dele. Arquivo compartilhado sozinho
# (licenca/cliente, instalador/, instrumentos/CMakeLists.txt, _hardware, workflow, CMakeLists.txt da raiz) não
# dispara build de ninguém: ele entra no próximo build de cada pacote, quando aquele pacote for atualizado.
# Execução manual (workflow_dispatch): o campo "pacotes" diz quem compila ("Limao Maestro", ou "todos").
# Uso: pacotes-alterados.sh <instrumentos|efeitos> <sha anterior> <sha do push> [escolha manual]
# Saída (para $GITHUB_OUTPUT): dirs=<pasta;pasta> (vai para o CMake em -DVSTS_SO) e pacotes=<Pacote Pacote>
set -euo pipefail
tipo=$1; base=${2:-}; head=$3; escolha=${4:-}

if [ "$tipo" = instrumentos ]; then
  mapa="quimera:Quimera limao:Limao mercurio:Mercurio resenha:Resenha montamonta:MontaMonta groovador:Groovador
        mesa:Mesa align:Align onkey:Onkey barebass:Barebass stompbox:Stompbox maestro:Maestro magos:Magos"
  raiz=instrumentos
else
  mapa="criativo:Criativo mix:Mix knob:Knob"
  raiz=plugins
fi

if [ -n "$escolha" ]; then
  arquivos=""
else
  if [ -z "$base" ] || [ "$base" = 0000000000000000000000000000000000000000 ] || ! git cat-file -e "$base^{commit}" 2>/dev/null; then
    base="$head~1"
  fi
  # .md dentro das pastas (README) e as ferramentas da knob não mudam o plugin
  arquivos=$(git diff --name-only "$base" "$head" | grep -vE '^(instrumentos|plugins)/.*\.md$' | grep -vE '^plugins/knob/_comum/tools/' || true)
  echo "arquivos do push:" >&2; echo "$arquivos" | sed 's/^/  /' >&2
fi

dirs=""; pacotes=""
for par in $mapa; do
  d=${par%%:*}; P=${par#*:}
  entra=0
  if [ -n "$escolha" ]; then
    for e in $escolha; do
      if [ "${e,,}" = todos ] || [ "${e,,}" = "${P,,}" ]; then entra=1; fi
    done
  else
    if echo "$arquivos" | grep -qE "^$raiz/$d/|^novidades/$P\.md$"; then entra=1; fi
    # MAGOS compila o MaestroLink do MAESTRO (memória compartilhada entre os dois): mudou o link, mudam os dois
    if [ "$P" = Magos ] && echo "$arquivos" | grep -qE '^instrumentos/maestro/Source/MaestroLink\.'; then entra=1; fi
  fi
  if [ $entra = 1 ]; then dirs="$dirs${dirs:+;}$d"; pacotes="$pacotes${pacotes:+ }$P"; fi
done

echo "pacotes deste build: ${pacotes:-nenhum}" >&2
echo "dirs=$dirs"
echo "pacotes=$pacotes"
