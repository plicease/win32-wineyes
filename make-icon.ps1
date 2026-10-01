# Regenerate wineyes.ico: the eyes (as drawn by wineyes.cpp) looking down
# and to the right, at the standard Windows icon sizes, stored as PNG frames.
# Only needed when changing the icon; the .ico itself is checked in.
param([string]$Out = "$PSScriptRoot\wineyes.ico")

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function Render([int]$s) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $g.Clear([System.Drawing.Color]::Transparent)

    # Same proportions as DrawEyes() in wineyes.cpp, but the eyes fill most
    # of the square and the rim never gets thinner than ~1px at tiny sizes.
    $w = [double]$s; $h = $s * 0.9; $oy = ($s - $h) / 2
    $margin = $w * 0.01
    $rx = $w / 4 - $margin; $ry = $h / 2 - $margin
    $rim = [Math]::Max(0.175 * $rx, 1.1)
    $pr = [Math]::Max(0.2 * $rx, 1.2)
    $pad = 0.05 * $rx
    $cursorX = 2.0 * $s; $cursorY = 2.0 * $s

    foreach ($i in 0, 1) {
        $cx = $w * (0.25 + 0.5 * $i); $cy = $oy + $h / 2
        $g.FillEllipse([System.Drawing.Brushes]::Black, [single]($cx - $rx), [single]($cy - $ry), [single](2 * $rx), [single](2 * $ry))
        $irx = $rx - $rim; $iry = $ry - $rim
        $g.FillEllipse([System.Drawing.Brushes]::White, [single]($cx - $irx), [single]($cy - $iry), [single](2 * $irx), [single](2 * $iry))

        $ax = $irx - $pr - $pad; $ay = $iry - $pr - $pad
        $dx = $cursorX - $cx; $dy = $cursorY - $cy
        $len = [Math]::Sqrt([Math]::Pow($dx / $ax, 2) + [Math]::Pow($dy / $ay, 2))
        if ($len -gt 1) { $dx /= $len; $dy /= $len }
        $g.FillEllipse([System.Drawing.Brushes]::Black, [single]($cx + $dx - $pr), [single]($cy + $dy - $pr), [single](2 * $pr), [single](2 * $pr))
    }
    $g.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    return ,$ms.ToArray()
}

$sizes = 16, 20, 24, 32, 40, 48, 64, 256
$frames = foreach ($s in $sizes) { ,(Render $s) }

$fs = [System.IO.File]::Create($Out)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $bw.Write([byte]$dim); $bw.Write([byte]$dim)
    $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([uint16]1); $bw.Write([uint16]32)
    $bw.Write([uint32]$frames[$i].Length); $bw.Write([uint32]$offset)
    $offset += $frames[$i].Length
}
foreach ($f in $frames) { $bw.Write($f) }
$bw.Dispose()
Write-Host "wrote $Out"
