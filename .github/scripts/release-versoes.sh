#!/usr/bin/env bash
# Release fixo de cada versão de pacote: <prefixo>-vx.y.z (ex.: limao-v0.3.0), com os instaladores dos dois sistemas
# e a seção da versão em novidades/<arquivo>.md como nota. Só sai quando:
#   - a versão é nova (a tag ainda não existe: versão já lançada não é sobrescrita — para lançar, suba a versão);
#   - os dois instaladores do pacote existem (Mac e Windows);
#   - o pluginval não falhou em nenhum dos dois sistemas.
# Fica só a última versão de cada pacote: criado o release novo, os <prefixo>-v… anteriores (release e tag) são apagados.
# Uso: release-versoes.sh <pasta dos instaladores> <pasta do relatório> <checkout do repo> <Pacote>...
set -euo pipefail
pacotes=$1; report=$2; src=$3; shift 3

if grep -h "pluginval=failure" "$report"/status-*.txt 2>/dev/null; then
  echo "pluginval falhou: nenhum release de versão neste build"
  exit 0
fi

for pac in "$@"; do
  mac="$pacotes/InsideStudioSP-$pac-mac.dmg"
  win="$pacotes/InsideStudioSP-$pac-win.exe"
  if [ ! -e "$mac" ] || [ ! -e "$win" ]; then
    echo "$pac: falta o instalador de um dos sistemas; sem release de versão"
    continue
  fi
  tag=$(python3 "$src/instalador/versao.py" "$pac" --tag)
  titulo=$(python3 "$src/instalador/versao.py" "$pac" --titulo)
  if gh release view "$tag" -R "$GITHUB_REPOSITORY" > /dev/null 2>&1; then
    echo "$tag já existe: mantido como está"
    continue
  fi
  # nome publicado (instalador/versao.py --arquivo; hoje é o próprio pacote)
  arq=$(python3 "$src/instalador/versao.py" "$pac" --arquivo)
  pub=$(mktemp -d)
  cp "$mac" "$pub/InsideStudioSP-$arq-mac.dmg"
  cp "$win" "$pub/InsideStudioSP-$arq-win.exe"
  notas=$(mktemp)
  {
    python3 "$src/instalador/versao.py" "$pac" --notas
    echo
    echo "Instaladores: \`InsideStudioSP-$arq-win.exe\` (Windows) e \`InsideStudioSP-$arq-mac.dmg\` (Mac), aqui embaixo em Assets."
  } > "$notas"
  gh release create "$tag" "$pub/InsideStudioSP-$arq-mac.dmg" "$pub/InsideStudioSP-$arq-win.exe" \
    -R "$GITHUB_REPOSITORY" --target "$GITHUB_SHA" --title "$titulo" --notes-file "$notas"
  echo "release $tag criado"
  prefixo=${tag%-v*}
  gh release list -R "$GITHUB_REPOSITORY" --limit 200 --json tagName -q '.[].tagName' \
    | grep -E "^${prefixo}-v[0-9]" | grep -vxF "$tag" \
    | while read -r antiga; do
        gh release delete "$antiga" --yes --cleanup-tag -R "$GITHUB_REPOSITORY"
        echo "versão antiga $antiga apagada"
      done || true
done
