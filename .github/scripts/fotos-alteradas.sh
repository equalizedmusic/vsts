#!/usr/bin/env bash
# Quais plugins têm a FOTO refeita: só o plugin que está sendo atualizado, e só se mudou algo VISUAL no push (tela, desenho, tema, peças, imagens).
# Mudança só de som (Engine, Dsp, Params, Presets...), de licença ou de instalador não refaz foto nenhuma.
#   visual = Source/img/**, assets/**, PluginEditor.*, *Editor*, *Ui.h, Theme.h, Widgets.h, SynthWidgets.h,
#            Painel.h, Hardware.h, Desenho.h, Draw.h, Panels.cpp
#   plugins/knob/_comum (editor ou assets) refaz a foto de todos os pedais da SÉRIE KNOB.
#   shots/ sozinho (o próprio fotógrafo) não refaz nada: rodar à mão escolhendo o plugin.
# Execução manual (workflow_dispatch): o campo "plugins" diz quais ("limao f1", ou "todos").
# Uso: fotos-alteradas.sh <sha anterior> <sha do push> [escolha manual]
# Saída (para $GITHUB_OUTPUT): so=<nome;nome> (vai para o CMake em -DSHOT_SO; "todos" = todas) e plugins=<nome nome>
set -euo pipefail
base=${1:-}; head=$2; escolha=${3:-}

visual='/Source/(img/.*|PluginEditor\.[^/]*|[^/]*Editor[^/]*|[^/]*Ui\.h|Theme\.h|Widgets\.h|SynthWidgets\.h|Painel\.h|Hardware\.h|Desenho\.h|Draw\.h|Panels\.cpp)$|/assets/'

nomes=""
add() { case " $nomes " in *" $1 "*) ;; *) nomes="$nomes${nomes:+ }$1";; esac; }

if [ -n "$escolha" ]; then
  for e in $escolha; do add "${e,,}"; done
else
  if [ -z "$base" ] || [ "$base" = 0000000000000000000000000000000000000000 ] || ! git cat-file -e "$base^{commit}" 2>/dev/null; then
    base="$head~1"
  fi
  arquivos=$(git diff --name-only "$base" "$head" || true)
  echo "arquivos do push:" >&2; echo "$arquivos" | sed 's/^/  /' >&2
  while IFS= read -r f; do
    [ -z "$f" ] && continue
    case "$f" in
      plugins/knob/_comum/tools/*) continue ;;
      plugins/knob/_comum/*)
        if echo "$f" | grep -qE "$visual"; then
          for d in plugins/knob/*/; do n=$(basename "$d"); [ "$n" != _comum ] && add "$n"; done
        fi
        continue ;;
    esac
    echo "$f" | grep -qE "$visual" || continue
    if [[ "$f" =~ ^instrumentos/([^/_][^/]*)/ ]]; then add "${BASH_REMATCH[1]}"
    elif [[ "$f" =~ ^plugins/[^/]+/([^/_][^/]*)/ ]]; then add "${BASH_REMATCH[1]}"
    fi
  done <<< "$arquivos"
fi

case " $nomes " in *" todos "*) nomes=todos;; esac
echo "fotos deste push: ${nomes:-nenhuma}" >&2
echo "so=${nomes// /;}"
echo "plugins=$nomes"
