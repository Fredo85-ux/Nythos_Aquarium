# =============================================================================
# build_exe.ps1  -  Build NythosAquarium.exe (standalone, no Python required)
# -----------------------------------------------------------------------------
# Produces dist\NythosAquarium.exe via PyInstaller (one-file, windowed), bundling
# the aquarium package + customtkinter/PIL/psutil. Run install.ps1 afterwards to
# install it as a program with Start Menu / Desktop shortcuts.
#
# Usage:  pwsh -ExecutionPolicy Bypass -File build_exe.ps1
# =============================================================================
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

Write-Host "[1/3] Generating app icon..." -ForegroundColor Cyan
python "packaging\make_icon.py"

Write-Host "[2/3] Ensuring PyInstaller is available..." -ForegroundColor Cyan
python -c "import PyInstaller" 2>$null
if ($LASTEXITCODE -ne 0) { python -m pip install --quiet pyinstaller }

Write-Host "[3/3] Building NythosAquarium.exe (this can take a minute)..." -ForegroundColor Cyan
$iconPath = Join-Path $root "build\icon.ico"
$entry    = Join-Path $root "nythos_aquarium.py"
python -m PyInstaller --noconfirm --clean --onefile --windowed `
    --name NythosAquarium `
    --icon "$iconPath" `
    --collect-all customtkinter `
    --collect-submodules PIL `
    --distpath "dist" --workpath "build\pyi" --specpath "build" `
    "$entry"

if ($LASTEXITCODE -ne 0) { throw "PyInstaller build failed (exit $LASTEXITCODE)" }

$exe = Join-Path $root "dist\NythosAquarium.exe"
if (Test-Path $exe) {
    $mb = "{0:N1}" -f ((Get-Item $exe).Length / 1MB)
    Write-Host "`nBuild succeeded: $exe  ($mb MB)" -ForegroundColor Green
    Write-Host "Next: pwsh -ExecutionPolicy Bypass -File install_aquarium.ps1" -ForegroundColor Yellow
} else {
    throw "Expected output not found: $exe"
}
