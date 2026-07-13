# =============================================================================
# install.ps1  -  Install / preview the Nythos Cyber Aquarium screensaver
# -----------------------------------------------------------------------------
#   .\install.ps1            Build (if needed) and copy the .scr to System32,
#                            then open Windows Screen Saver settings.
#   .\install.ps1 -Preview   Just run it full-screen (no install).
#   .\install.ps1 -Config    Open the settings dialog.
#
# Copying to System32 requires an elevated PowerShell. If you prefer not to
# install system-wide, right-click NythosCyberAquarium.scr in Explorer and
# choose "Install", or run with -Preview to try it.
# =============================================================================
param([switch]$Preview, [switch]$Config)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$scr  = Join-Path $root "build\NythosCyberAquarium.scr"

if (!(Test-Path $scr)) {
    Write-Host "Screensaver not built yet - building..." -ForegroundColor Yellow
    & pwsh -ExecutionPolicy Bypass -File (Join-Path $root "build.ps1")
}

if ($Preview) { & $scr /s; return }
if ($Config)  { & $scr /c; return }

# Install to System32 so it appears in the Windows screen-saver dropdown.
$dest = Join-Path $env:WINDIR "System32\NythosCyberAquarium.scr"
try {
    Copy-Item $scr $dest -Force
    Write-Host "Installed to $dest" -ForegroundColor Green
} catch {
    Write-Warning "Could not copy to System32 (run as Administrator). You can still:"
    Write-Warning "  - right-click '$scr' in Explorer and choose Install, or"
    Write-Warning "  - run '.\install.ps1 -Preview' to try it without installing."
    return
}

# Open the classic Screen Saver settings so the user can select & configure it.
Start-Process "rundll32.exe" "shell32.dll,Control_RunDLL desk.cpl,,1"
Write-Host "Pick 'NythosCyberAquarium' in the dropdown, then Settings... to configure." -ForegroundColor Cyan
