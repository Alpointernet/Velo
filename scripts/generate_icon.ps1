param(
    [string]$SourcePath = (Join-Path $PSScriptRoot '..\icon\app.png'),
    [string]$DestinationPath = (Join-Path $PSScriptRoot '..\icon\app.ico')
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
    throw "Application icon source was not found: $SourcePath"
}

$sourceInfo = Get-Item -LiteralPath $SourcePath
if (Test-Path -LiteralPath $DestinationPath -PathType Leaf) {
    $iconInfo = Get-Item -LiteralPath $DestinationPath
    $scriptInfo = Get-Item -LiteralPath $PSCommandPath
    if ($iconInfo.LastWriteTimeUtc -ge $sourceInfo.LastWriteTimeUtc -and
        $iconInfo.LastWriteTimeUtc -ge $scriptInfo.LastWriteTimeUtc) {
        exit 0
    }
}

Add-Type -AssemblyName System.Drawing
$source = [System.Drawing.Image]::FromFile($sourceInfo.FullName)
$images = [System.Collections.Generic.List[byte[]]]::new()
$sizes = @(16, 24, 32, 48, 64, 128, 256)
try {
    foreach ($size in $sizes) {
        $bitmap = [System.Drawing.Bitmap]::new($size, $size)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $stream = [System.IO.MemoryStream]::new()
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $scale = [Math]::Min($size / $source.Width, $size / $source.Height)
            $width = [int][Math]::Round($source.Width * $scale)
            $height = [int][Math]::Round($source.Height * $scale)
            $graphics.DrawImage($source, [int](($size - $width) / 2), [int](($size - $height) / 2), $width, $height)
            $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            $images.Add($stream.ToArray())
        } finally {
            $stream.Dispose()
            $graphics.Dispose()
            $bitmap.Dispose()
        }
    }
} finally {
    $source.Dispose()
}

$output = [System.IO.File]::Create($DestinationPath)
$writer = [System.IO.BinaryWriter]::new($output)
try {
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
        $writer.Write([byte]$dimension)
        $writer.Write([byte]$dimension)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$images[$i].Length)
        $writer.Write([uint32]$offset)
        $offset += $images[$i].Length
    }
    foreach ($image in $images) { $writer.Write($image) }
} finally {
    $writer.Dispose()
}
