# Renderiza a prévia 3D dos 10 pedais (Blender 4.2) em <saida>
# uso: .\lote.ps1 -Blender <caminho do blender.exe> [-Saida <pasta>]   (padrão: .\final)
param([Parameter(Mandatory)][string]$Blender, [string]$Saida = (Join-Path $PSScriptRoot 'final'))
$f = Join-Path $PSScriptRoot 'pedal.py'
New-Item -ItemType Directory -Force $Saida | Out-Null
foreach($p in 'fole','picador','mare','vaivem','repique','eco','avesso','freio','escada','mastiga'){
  & $Blender -b -P $f -- "$Saida/$p.png" $p - 72 120 aco 2>&1 | Select-String 'Error|Traceback|Saved'
}
& $Blender -b -P $f -- "$Saida/fole-preto.png" fole - 72 120 preto 2>&1 | Select-String 'Error|Traceback|Saved'
