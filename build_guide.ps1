# =============================================================================
# build_guide.ps1  -  Render the themed Aquarium docs (HTML -> PDF, via Edge)
#   * docs\Nythos_Aquarium_Guide.pdf     (9-page field guide & assessment)
#   * docs\Nythos_Aquarium_RefCard.pdf   (1-page quick-reference card)
# Usage:  pwsh -ExecutionPolicy Bypass -File build_guide.ps1
# =============================================================================
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$edge = @(
  "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
  "C:\Program Files\Microsoft\Edge\Application\msedge.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $edge) { throw "Microsoft Edge not found (needed to render the PDF)." }

$jobs = @(
  @{ html = "docs\aquarium_guide.html";   pdf = "docs\Nythos_Aquarium_Guide.pdf" },
  @{ html = "docs\aquarium_refcard.html"; pdf = "docs\Nythos_Aquarium_RefCard.pdf" }
)

foreach ($j in $jobs) {
    $html = Join-Path $root $j.html
    $pdf  = Join-Path $root $j.pdf
    $uri  = ([System.Uri]$html).AbsoluteUri
    $tmp  = Join-Path $env:TEMP ("edgepdf_" + [IO.Path]::GetFileNameWithoutExtension($pdf))
    Remove-Item $pdf -ErrorAction SilentlyContinue
    Start-Process -FilePath $edge -Wait -ArgumentList `
        "--headless=new","--disable-gpu","--no-first-run","--no-default-browser-check",
        "--user-data-dir=$tmp","--no-pdf-header-footer","--print-to-pdf=$pdf","$uri"
    if (Test-Path $pdf) {
        Write-Host ("Rendered: {0}  ({1:N0} KB)" -f $pdf, ((Get-Item $pdf).Length / 1KB)) -ForegroundColor Green
    } else {
        throw "PDF was not produced: $pdf"
    }
}
