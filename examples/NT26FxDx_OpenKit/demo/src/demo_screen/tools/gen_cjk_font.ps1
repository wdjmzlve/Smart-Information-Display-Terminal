# Generates a 16x16 GB2312 level-1 CJK glyph table for the SSD1306 driver.
# ASCII-only source on purpose: avoids PowerShell script encoding issues.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File gen_cjk_font.ps1
#   powershell -ExecutionPolicy Bypass -File gen_cjk_font.ps1 -MaxChars 6 -Dump
#
# Output: ../src/screen_font_cjk.c (auto-collected by the project Makefile)

param(
    [string]$OutFile = "",
    [int]$MaxChars = 0,
    [int]$PixelSize = 16,
    [int]$Dy = 0,
    [switch]$Dump
)

Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrEmpty($OutFile)) {
    $OutFile = Join-Path $PSScriptRoot "..\src\screen_font_cjk.c"
}

# Collect GB2312 level-1 code points in ascending Unicode order.
# Level-1 range in GB2312 is 0xB0A1..0xD7F9.
$enc = [System.Text.Encoding]::GetEncoding(936)
$codes = New-Object 'System.Collections.Generic.List[int]'
for ($cp = 0x4E00; $cp -le 0x9FFF; $cp++) {
    $bytes = $enc.GetBytes([char]::ConvertFromUtf32($cp))
    if ($bytes.Length -eq 2 -and $bytes[0] -ge 0xB0 -and $bytes[0] -le 0xD7 -and
        $bytes[1] -ge 0xA1 -and $bytes[1] -le 0xFE) {
        $codes.Add($cp)
    }
}
if ($MaxChars -gt 0 -and $codes.Count -gt $MaxChars) {
    $codes = $codes.GetRange(0, $MaxChars)
}

$count = $codes.Count
Write-Host ("chars: " + $count)

$font = New-Object System.Drawing.Font("SimSun", $PixelSize,
    [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
$fmt = [System.Drawing.StringFormat]::GenericTypographic
$fmt.FormatFlags = $fmt.FormatFlags -bor [System.Drawing.StringFormatFlags]::NoClip

function Get-Pixels {
    param([System.Drawing.Bitmap]$Bitmap)

    $rect = New-Object System.Drawing.Rectangle 0, 0, $Bitmap.Width, $Bitmap.Height
    $data = $Bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $buf = New-Object byte[] ($data.Stride * $Bitmap.Height)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $buf.Length)
    $Bitmap.UnlockBits($data)

    $pixels = New-Object 'bool[]' ($Bitmap.Width * $Bitmap.Height)
    for ($y = 0; $y -lt $Bitmap.Height; $y++) {
        for ($x = 0; $x -lt $Bitmap.Width; $x++) {
            if ($buf[$y * $data.Stride + $x * 4 + 2] -ge 96) {
                $pixels[$y * $Bitmap.Width + $x] = $true
            }
        }
    }
    return $pixels
}

# Calibration: render a reference glyph on a large canvas and find its ink box,
# then shift every glyph so that the reference ink is centered in the 16x16 cell.
$calSize = 40
$calBmp = New-Object System.Drawing.Bitmap $calSize, $calSize,
    ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$calG = [System.Drawing.Graphics]::FromImage($calBmp)
$calG.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$calG.Clear([System.Drawing.Color]::Black)
$calG.DrawString([char]::ConvertFromUtf32(0x56FD), $font, $brush, [single]0, [single]0, $fmt)
$calPixels = Get-Pixels $calBmp
$minX = $calSize; $minY = $calSize; $maxX = -1; $maxY = -1
for ($y = 0; $y -lt $calSize; $y++) {
    for ($x = 0; $x -lt $calSize; $x++) {
        if ($calPixels[$y * $calSize + $x]) {
            if ($x -lt $minX) { $minX = $x }
            if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }
            if ($y -gt $maxY) { $maxY = $y }
        }
    }
}
$calG.Dispose()
$calBmp.Dispose()
if ($maxX -lt 0) {
    throw "calibration glyph is empty, SimSun may be missing"
}
Write-Host ("calibration ink box: x=" + $minX + ".." + $maxX + " y=" + $minY + ".." + $maxY)

$offX = 8.0 - (($minX + $maxX + 1) / 2.0)
$offY = 8.0 - (($minY + $maxY + 1) / 2.0) + $Dy
Write-Host ("offset: dx=" + $offX + " dy=" + $offY)

$bmp = New-Object System.Drawing.Bitmap 16, 16,
    ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

$bitmaps = New-Object 'System.Collections.Generic.List[byte]'
$empty = 0

foreach ($cp in $codes) {
    $g.Clear([System.Drawing.Color]::Black)
    $g.DrawString([char]::ConvertFromUtf32($cp), $font, $brush,
        [single]$offX, [single]$offY, $fmt)
    $pixels = Get-Pixels $bmp

    $lit = 0
    $dumpText = New-Object System.Text.StringBuilder
    if ($Dump) { [void]$dumpText.AppendLine(("U+{0:X4}" -f $cp)) }

    for ($y = 0; $y -lt 16; $y++) {
        $hi = 0
        $lo = 0
        $row = ""
        for ($x = 0; $x -lt 16; $x++) {
            $on = 0
            if ($pixels[$y * 16 + $x]) { $on = 1; $lit++ }
            if ($on -eq 0) { $row += "." } else { $row += "#" }
            if ($x -lt 8) { $hi = $hi -bor ($on -shl (7 - $x)) }
            else          { $lo = $lo -bor ($on -shl (15 - $x)) }
        }
        $bitmaps.Add([byte]$hi)
        $bitmaps.Add([byte]$lo)
        if ($Dump) { [void]$dumpText.AppendLine($row) }
    }
    if ($Dump) { Write-Host $dumpText.ToString() }
    if ($lit -eq 0) { $empty++ }
}

Write-Host ("glyphs processed: " + $count + ", empty: " + $empty)

if ($Dump) { return }

$utf8 = New-Object System.Text.UTF8Encoding($false)
$sw = New-Object System.IO.StreamWriter($OutFile, $false, $utf8)
$sw.NewLine = "`n"

$sw.WriteLine("/**")
$sw.WriteLine(" * @file screen_font_cjk.c")
$sw.WriteLine(" * @brief 16x16 CJK glyph table, GB2312 level-1 (auto-generated)")
$sw.WriteLine(" * @note Generated by tools/gen_cjk_font.ps1 from SimSun. Do not edit by hand.")
$sw.WriteLine(" *       Each glyph: 16 rows x 2 bytes, MSB is the leftmost pixel.")
$sw.WriteLine(" */")
$sw.WriteLine("")
$sw.WriteLine("#include ""screen_font.h""")
$sw.WriteLine("")
$sw.WriteLine(("const uint32_t screen_font_cjk_count = {0}U;" -f $count))
$sw.WriteLine("")

$sw.WriteLine("/* Unicode code points, ascending, used for binary search */")
$sw.WriteLine("const uint16_t screen_font_cjk_unicode[] = {")
$line = "    "
for ($i = 0; $i -lt $count; $i++) {
    $line += ("0x{0:X4}," -f $codes[$i])
    if ((($i + 1) % 12) -eq 0) {
        $sw.WriteLine($line)
        $line = "    "
    }
}
if ($line.Trim().Length -gt 0) { $sw.WriteLine($line) }
$sw.WriteLine("};")
$sw.WriteLine("")

$sw.WriteLine("/* Glyph bitmaps, 32 bytes per character, same order as the index */")
$sw.WriteLine("const uint8_t screen_font_cjk_bitmap[] = {")
$line = "    "
for ($i = 0; $i -lt $bitmaps.Count; $i++) {
    $line += ("0x{0:X2}," -f $bitmaps[$i])
    if ((($i + 1) % 16) -eq 0) {
        $sw.WriteLine($line)
        $line = "    "
    }
}
if ($line.Trim().Length -gt 0) { $sw.WriteLine($line) }
$sw.WriteLine("};")

$sw.Close()
Write-Host ("written: " + $OutFile)
