<#
.SYNOPSIS
    Generates the installer header images (small logo + flat banner) from the
    product icon and the product display name.

.DESCRIPTION
    Qt IFW's Modern wizard sizes the header to the logo's native height, so a
    large logo overwhelms the page. This produces a compact 64x64 logo plus a
    wide, flat banner carrying the product name, giving a clean header where the
    page list stays visible. The banner text uses the current display name, so a
    product rename needs no new artwork.

.PARAMETER OutDir
    Directory to write logo.png and banner.png into (the installer config dir).

.PARAMETER DisplayName
    Product display name rendered on the banner.

.PARAMETER IconSource
    Path to the source square product icon (PNG).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $OutDir,
    [Parameter(Mandatory = $true)] [string] $DisplayName,
    [Parameter(Mandatory = $true)] [string] $IconSource
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$src = [System.Drawing.Image]::FromFile($IconSource)
try {
    # --- Header logo (128x128), high-quality downscale of the product icon.
    #     Its height drives the Modern wizard header height, so a taller logo
    #     yields a taller, more prominent header and a more visible icon. ---
    $logoSize = 128
    $logo = New-Object System.Drawing.Bitmap($logoSize, $logoSize)
    $lg = [System.Drawing.Graphics]::FromImage($logo)
    try {
        $lg.InterpolationMode = 'HighQualityBicubic'
        $lg.SmoothingMode = 'AntiAlias'
        $lg.PixelOffsetMode = 'HighQuality'
        $lg.DrawImage($src, 0, 0, $logoSize, $logoSize)
    } finally { $lg.Dispose() }
    $logo.Save((Join-Path $OutDir 'logo.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $logo.Dispose()

    # --- Flat banner (wide, taller): magenta gradient + white product name. The
    #     gradient runs left (lighter) to right (markedly deeper) so the header
    #     logo on the right stands out, matching the application's magenta accent.
    #     Height matches the logo (128). ---
    $bw = 720; $bh = 128
    $banner = New-Object System.Drawing.Bitmap($bw, $bh)
    $bg = [System.Drawing.Graphics]::FromImage($banner)
    try {
        $bg.SmoothingMode = 'AntiAlias'
        $bg.TextRenderingHint = 'ClearTypeGridFit'
        $rect = New-Object System.Drawing.Rectangle(0, 0, $bw, $bh)
        $c1 = [System.Drawing.Color]::FromArgb(0xD8, 0x1B, 0x60)  # brighter brand magenta (left)
        $c2 = [System.Drawing.Color]::FromArgb(0x56, 0x00, 0x27)  # much deeper magenta (right)
        $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $c1, $c2, [System.Drawing.Drawing2D.LinearGradientMode]::Horizontal)
        try { $bg.FillRectangle($brush, $rect) } finally { $brush.Dispose() }

        $font = New-Object System.Drawing.Font('Segoe UI', 30, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
        try {
            $sf = New-Object System.Drawing.StringFormat
            $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
            $sf.Alignment = [System.Drawing.StringAlignment]::Near
            # Leave room on the right so the name never runs under the logo.
            $textRect = New-Object System.Drawing.RectangleF(28, 0, ($bw - 180), $bh)
            $bg.DrawString($DisplayName, $font, [System.Drawing.Brushes]::White, $textRect, $sf)
        } finally { $font.Dispose() }
    } finally { $bg.Dispose() }
    $banner.Save((Join-Path $OutDir 'banner.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $banner.Dispose()
}
finally { $src.Dispose() }

Write-Host "Installer images written to $OutDir (logo.png 64x64, banner.png ${bw}x${bh})."
