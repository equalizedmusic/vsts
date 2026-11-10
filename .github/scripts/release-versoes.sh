#!/usr/bin/env bash
# Release fixo de cada versão de pacote: <prefixo>-vx.y.z (ex.: limao-v0.3.0), com os instaladores dos dois sistemas
# e a seção da versão em novidades/<arquivo>.md como nota. Só sai quando:
#   - a versão é nova (a tag ainda não existe: versão já lançada não é sobrescrita — para lançar, suba a versão);
#   - os dois instaladores do pacote existem (Mac e Windows);
#   - o pluginval não reprovou o pacote em nenhum dos dois sistemas. Se o workflow lista os reprovados
#     (pluginval-falhou-<sistema>.txt + pluginval-fim-<sistema>.txt), só eles ficam de fora; se o pluginval
#     falhou sem essa lista (não baixou, estourou o tempo, workflow antigo), nenhum pacote sai.
# Fica só a última versão de cada pacote: criado o release novo, os <prefixo>-v… anteriores (release e tag) são apagados.
# Uso: release-versoes.sh <pasta dos instaladores> <pasta do relatório> <checkout do repo> <Pacote>...
set -euo pipefail
pacotes=$1; report=$2; src=$3; shift 3

reprovados=" "
for st in "$report"/status-*.txt; do
  [ -e "$st" ] || continue
  grep -q "pluginval=failure" "$st" || continue
  sis=$(basename "$st" .txt); sis=${sis#status-}
  if [ ! -e "$report/pluginval-fim-$sis.txt" ]; then
    echo "pluginval falhou no $sis sem chegar ao fim: nenhum release de versão neste build"
    exit 0
  fi
  reprovados="$reprovados$( { cat "$report/pluginval-falhou-$sis.txt" 2>/dev/null || true; } | tr -d '\r' | tr '\n' ' ')"
done
[ "$reprovados" != " " ] && echo "pluginval reprovou:$reprovados(só esses ficam sem release)"

for pac in "$@"; do
  case "$reprovados" in
    *" $pac "*) echo "$pac: reprovado no pluginval; sem release de versão"; continue ;;
  esac
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
  # .crc32 de cada instalador: o Worker de licenças monta o zip da página grátis (instalador + LEIA-ME) sem calcular
  # o CRC na hora (no plano grátis ele não tem CPU para isso)
  for f in "$pub/InsideStudioSP-$arq-mac.dmg" "$pub/InsideStudioSP-$arq-win.exe"; do
    python3 -c "import sys,zlib; print('%08x' % (zlib.crc32(open(sys.argv[1],'rb').read()) & 0xffffffff))" "$f" > "$f.crc32"
  done
  # pacote listado em instalador/pre-release.txt sai como PRÉ-RELEASE de teste: o servidor de licenças ignora
  # pré-release (download do site e aviso de atualização continuam na versão anterior) e a anterior não é apagada.
  # Para lançar de verdade depois: tirar o pacote da lista e subir a versão (tag já existente não é refeita).
  pre=""
  if grep -qxF "$pac" <(tr -d '\r' < "$src/instalador/pre-release.txt" 2>/dev/null || true); then pre="--prerelease"; fi
  gh release create "$tag" "$pub/InsideStudioSP-$arq-mac.dmg" "$pub/InsideStudioSP-$arq-win.exe" \
    "$pub/InsideStudioSP-$arq-mac.dmg.crc32" "$pub/InsideStudioSP-$arq-win.exe.crc32" \
    -R "$GITHUB_REPOSITORY" --target "$GITHUB_SHA" --title "$titulo" --notes-file "$notas" $pre
  if [ -n "$pre" ]; then echo "pré-release de teste $tag criado (versões anteriores mantidas)"; continue; fi
  echo "release $tag criado"
  prefixo=${tag%-v*}
  gh release list -R "$GITHUB_REPOSITORY" --limit 200 --json tagName -q '.[].tagName' \
    | grep -E "^${prefixo}-v[0-9]" | grep -vxF "$tag" \
    | while read -r antiga; do
        gh release delete "$antiga" --yes --cleanup-tag -R "$GITHUB_REPOSITORY"
        echo "versão antiga $antiga apagada"
      done || true
done
