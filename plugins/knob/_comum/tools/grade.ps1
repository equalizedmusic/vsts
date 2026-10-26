# junta os 10 renders do lote.ps1 numa grade 5x2 (serie-knob-10.png)
# uso: .\grade.ps1 [-Pasta <pasta dos renders>]   (padrão: .\final)
param([string]$Pasta = (Join-Path $PSScriptRoot 'final'))
Add-Type -AssemblyName System.Drawing
$d = (Resolve-Path $Pasta).Path
$nomes='fole','picador','mare','vaivem','repique','eco','avesso','freio','escada','mastiga'
$w=600; $first=[System.Drawing.Image]::FromFile("$d\fole.png"); $h=[int]($w*$first.Height/$first.Width); $first.Dispose()
$bmp=New-Object System.Drawing.Bitmap (5*$w),(2*$h); $g=[System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode='HighQualityBicubic'; $g.Clear([System.Drawing.Color]::FromArgb(20,20,20))
for($i=0;$i -lt 10;$i++){ $im=[System.Drawing.Image]::FromFile("$d\$($nomes[$i]).png"); $g.DrawImage($im,($i%5)*$w,[math]::Floor($i/5)*$h,$w,$h); $im.Dispose() }
$bmp.Save("$d\serie-knob-10.png",[System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose(); 'grade ok'
