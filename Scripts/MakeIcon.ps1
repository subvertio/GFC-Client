# Makes every app icon from one piece of artwork, Build\Icon\GFCIcon.jpg (a square, at least 512 px):
#   Build\Windows\Application.ico                  Windows: built into GFCClient.exe, used by the installer
#   Build\Android\res\drawable-*\icon.png          Android before 8.0: a rounded square
#   Build\Android\res\drawable-*\icon_foreground.png, drawable-anydpi-v26\icon.xml, values\gfc_icon.xml
#                                                  Android 8.0+: an adaptive icon the launcher masks to its own shape
# Unreal copies Build\Android\res over its own resources, so these replace the default Unreal icon.
# To change the icon, replace GFCIcon.jpg and run this again:  .\Scripts\MakeIcon.ps1
param(
    # The artwork has its own rounded square on white; this much of each side is cropped off (0-0.4)
    [double]$Inset = 0.107,
    # Background behind the artwork in the adaptive icon: the artwork's own edge colour
    [string]$Background = '#02040F'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$source = Join-Path $root 'Build\Icon\GFCIcon.jpg'
if (-not (Test-Path $source)) { throw "Missing $source" }
$res = Join-Path $root 'Build\Android\res'
$bg = [System.Drawing.ColorTranslator]::FromHtml($Background)

$art = New-Object System.Drawing.Bitmap $source
$cut = [int]([Math]::Min($art.Width, $art.Height) * $Inset)
$crop = New-Object System.Drawing.Rectangle $cut, $cut, ($art.Width - 2 * $cut), ($art.Height - 2 * $cut)

function New-Canvas([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'; $g.CompositingQuality = 'HighQuality'
    $g.Clear([System.Drawing.Color]::Transparent)
    return $bmp, $g
}

# The cropped artwork in a rounded square (corner radius 22% of the side) on transparent
function New-RoundedIcon([int]$size) {
    $bmp, $g = New-Canvas $size
    $r = [single]($size * 0.22); $d = 2 * $r; $s = [single]($size - 1)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc(0, 0, $d, $d, 180, 90); $path.AddArc($s - $d, 0, $d, $d, 270, 90)
    $path.AddArc($s - $d, $s - $d, $d, $d, 0, 90); $path.AddArc(0, $s - $d, $d, $d, 90, 90); $path.CloseFigure()
    $g.SetClip($path)
    $g.DrawImage($art, (New-Object System.Drawing.Rectangle 0, 0, $size, $size), $crop, 'Pixel')
    $g.Dispose(); $path.Dispose()
    return $bmp
}

# Adaptive foreground (108dp canvas): the artwork at 62% in the middle, so launcher masks (66dp circle up
# to a full square) never cut into it, its edges faded into the background colour so no square shows.
function New-Foreground([int]$size) {
    $bmp, $g = New-Canvas $size
    $g.Clear($bg)
    $inner = [int]($size * 0.62); $at = [int](($size - $inner) / 2)
    $g.DrawImage($art, (New-Object System.Drawing.Rectangle $at, $at, $inner, $inner), $crop, 'Pixel')
    $fade = [int]($inner * 0.12)
    $solid = $bg; $clear = [System.Drawing.Color]::FromArgb(0, $bg)
    $edges = @(
        @((New-Object System.Drawing.Rectangle $at, $at, $inner, $fade), 'Vertical', $solid, $clear),
        @((New-Object System.Drawing.Rectangle $at, ($at + $inner - $fade), $inner, $fade), 'Vertical', $clear, $solid),
        @((New-Object System.Drawing.Rectangle $at, $at, $fade, $inner), 'Horizontal', $solid, $clear),
        @((New-Object System.Drawing.Rectangle ($at + $inner - $fade), $at, $fade, $inner), 'Horizontal', $clear, $solid)
    )
    foreach ($e in $edges) {
        $rect = $e[0]
        $brushRect = New-Object System.Drawing.Rectangle ($rect.X - 1), ($rect.Y - 1), ($rect.Width + 2), ($rect.Height + 2)
        $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $brushRect, $e[2], $e[3], ([System.Drawing.Drawing2D.LinearGradientMode]$e[1])
        $g.FillRectangle($brush, $rect)
        $brush.Dispose()
    }
    $g.Dispose()
    return $bmp
}

function Save-Png($bmp, [string]$path) {
    New-Item -ItemType Directory -Force (Split-Path $path -Parent) | Out-Null
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

# --- Android, legacy icon (48dp) and adaptive foreground (108dp) per density
$densities = [ordered]@{ ldpi = 0.75; mdpi = 1; hdpi = 1.5; xhdpi = 2; xxhdpi = 3; xxxhdpi = 4 }
foreach ($name in $densities.Keys) {
    $scale = $densities[$name]
    Save-Png (New-RoundedIcon ([int](48 * $scale))) (Join-Path $res "drawable-$name\icon.png")
    Save-Png (New-Foreground ([int](108 * $scale))) (Join-Path $res "drawable-$name\icon_foreground.png")
}
# Unreal's own res\drawable\icon.png would otherwise win on densities without their own folder
Save-Png (New-RoundedIcon 48) (Join-Path $res 'drawable\icon.png')

New-Item -ItemType Directory -Force (Join-Path $res 'drawable-anydpi-v26'), (Join-Path $res 'values') | Out-Null
$utf8 = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText((Join-Path $res 'drawable-anydpi-v26\icon.xml'), @'
<?xml version="1.0" encoding="utf-8"?>
<!-- Made by Scripts\MakeIcon.ps1 from Build\Icon\GFCIcon.jpg -->
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/gfc_icon_background" />
    <foreground android:drawable="@drawable/icon_foreground" />
</adaptive-icon>
'@, $utf8)
[System.IO.File]::WriteAllText((Join-Path $res 'values\gfc_icon.xml'), @"
<?xml version="1.0" encoding="utf-8"?>
<!-- Made by Scripts\MakeIcon.ps1 -->
<resources>
    <color name="gfc_icon_background">$Background</color>
</resources>
"@, $utf8)

# --- Windows .ico: PNG images in an ICO container
$ico = Join-Path $root 'Build\Windows\Application.ico'
New-Item -ItemType Directory -Force (Split-Path $ico -Parent) | Out-Null
$sizes = 256, 64, 48, 32, 24, 16
$images = foreach ($s in $sizes) {
    $bmp = New-RoundedIcon $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    , $ms.ToArray()
}
$fs = [System.IO.File]::Create($ico)
$w = New-Object System.IO.BinaryWriter $fs
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $w.Write([byte]$dim); $w.Write([byte]$dim); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([uint16]1); $w.Write([uint16]32)
    $w.Write([uint32]$images[$i].Length); $w.Write([uint32]$offset)
    $offset += $images[$i].Length
}
foreach ($img in $images) { $w.Write($img) }
$w.Close()

$art.Dispose()
Write-Host "Wrote $ico and the Android icons in $res"
