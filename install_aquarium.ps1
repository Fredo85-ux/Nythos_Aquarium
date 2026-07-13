# =============================================================================
# install_aquarium.ps1  -  Install Nythos Aquarium as a Windows program
# -----------------------------------------------------------------------------
# Per-user install (no admin needed):
#   * builds dist\NythosAquarium.exe if missing (via build_exe.ps1)
#   * copies it to %LOCALAPPDATA%\Programs\NythosAquarium
#   * creates Start Menu + Desktop shortcuts (fullscreen + dashboard)
#   * registers an "Apps & features" uninstall entry
#
# Usage:
#   pwsh -ExecutionPolicy Bypass -File install_aquarium.ps1
#   pwsh -ExecutionPolicy Bypass -File install_aquarium.ps1 -Launch   # run after
# =============================================================================
param([switch]$Launch)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

$exe = Join-Path $root "dist\NythosAquarium.exe"
if (-not (Test-Path $exe)) {
    Write-Host "Executable not found — building it first..." -ForegroundColor Yellow
    & pwsh -ExecutionPolicy Bypass -File (Join-Path $root "build_exe.ps1")
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { throw "Build failed." }
}

# --- install location (per-user, no admin) -----------------------------------
$installDir = Join-Path $env:LOCALAPPDATA "Programs\NythosAquarium"
New-Item -ItemType Directory -Force -Path $installDir | Out-Null
Copy-Item $exe $installDir -Force
$icon = Join-Path $root "build\icon.ico"
if (Test-Path $icon) { Copy-Item $icon $installDir -Force }
Copy-Item (Join-Path $root "packaging\uninstall_aquarium.ps1") $installDir -Force

$exeInstalled  = Join-Path $installDir "NythosAquarium.exe"
$iconInstalled = Join-Path $installDir "icon.ico"
if (-not (Test-Path $iconInstalled)) { $iconInstalled = $exeInstalled }

# --- shortcuts ---------------------------------------------------------------
$ws = New-Object -ComObject WScript.Shell
function New-Shortcut($path, $argline, $desc) {
    $sc = $ws.CreateShortcut($path)
    $sc.TargetPath = $exeInstalled
    $sc.Arguments = $argline
    $sc.IconLocation = $iconInstalled
    $sc.WorkingDirectory = $installDir
    $sc.Description = $desc
    $sc.Save()
}

$startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) "Nythos"
New-Item -ItemType Directory -Force -Path $startMenu | Out-Null
New-Shortcut (Join-Path $startMenu "Nythos Aquarium.lnk")             "--mode fullscreen" "Nythos Aquarium — fullscreen"
New-Shortcut (Join-Path $startMenu "Nythos Aquarium (Dashboard).lnk") "--mode windowed"   "Nythos Aquarium — windowed dashboard"

$desktop = [Environment]::GetFolderPath('Desktop')
New-Shortcut (Join-Path $desktop "Nythos Aquarium.lnk") "--mode fullscreen" "Nythos Aquarium"

# --- Apps & features uninstall entry -----------------------------------------
$key = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\NythosAquarium"
New-Item -Path $key -Force | Out-Null
$uninstallCmd = "powershell.exe -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$installDir\uninstall_aquarium.ps1`""
$sizeKb = [int]((Get-Item $exeInstalled).Length / 1KB)
Set-ItemProperty $key DisplayName     "Nythos Aquarium"
Set-ItemProperty $key DisplayVersion  "1.0.0"
Set-ItemProperty $key Publisher       "Nythos"
Set-ItemProperty $key DisplayIcon     $iconInstalled
Set-ItemProperty $key InstallLocation $installDir
Set-ItemProperty $key UninstallString $uninstallCmd
Set-ItemProperty $key QuietUninstallString $uninstallCmd
Set-ItemProperty $key EstimatedSize   $sizeKb -Type DWord
Set-ItemProperty $key NoModify        1 -Type DWord
Set-ItemProperty $key NoRepair        1 -Type DWord

Write-Host "`nInstalled Nythos Aquarium to:" -ForegroundColor Green
Write-Host "  $installDir"
Write-Host "Shortcuts: Start Menu > Nythos, and the Desktop."
Write-Host "Uninstall via Settings > Apps, or run uninstall_aquarium.ps1." -ForegroundColor DarkGray

if ($Launch) { Start-Process $exeInstalled -ArgumentList "--mode fullscreen" }
