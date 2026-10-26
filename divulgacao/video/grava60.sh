#!/bin/bash
# Grava os 3 stories de 60 s (MIX, CRIATIVO, QUIMERA) e junta com a trilha
cd "$(dirname "$0")"; FF="${FFMPEG:-ffmpeg}"; mkdir -p mudo60 saida60
um() { # $1 parametro do video.html  $2 trilha  $3 nome
  node render.js "$1" "mudo60/$3.mp4" > "mudo60/$3.log" 2>&1 || { echo "$3 render falhou"; return; }
  "$FF" -y -loglevel error -i "mudo60/$3.mp4" -i "trilha60-$2.wav" -c:v copy -c:a aac -b:a 192k -shortest -movflags +faststart "saida60/$3.mp4" && echo "$3 ok" || echo "$3 mux falhou"
}
um "mix&d=60" mix InsideStudioSP-Linha-de-Mix-60s &
um "criativo&d=60" criativo InsideStudioSP-Linha-Criativa-60s &
um "plugin&n=QUIMERA" synth InsideStudioSP-QUIMERA-60s &
wait; echo FIM
