# =============================================================================
# uninstall_aquarium.ps1  -  Remove Nythos Aquarium (program install)
# Copied into the install directory by install_aquarium.ps1 and wired to the
# Windows "Apps & features" uninstall entry.
# =============================================================================
$ErrorActionPreference = "SilentlyContinue"

$installDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$startMenu  = Join-Path ([Environment]::GetFolderPath('Programs')) "Nythos"
$desktop    = [Environment]::GetFolderPath('Desktop')

Write-Host "Uninstalling Nythos Aquarium..." -ForegroundColor Cyan

# shortcuts
Remove-Item -Recurse -Force $startMenu
Remove-Item -Force (Join-Path $desktop "Nythos Aquarium.lnk")

# uninstall registry entry
Remove-Item -Recurse -Force "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\NythosAquarium"

# settings (optional — leave user prefs? remove the app's own folder)
Remove-Item -Recurse -Force (Join-Path $env:APPDATA "Nythos\Aquarium")

# remove the install directory itself, after this process exits (the folder may
# still hold the running exe / this script). A detached cmd waits, then deletes.
Start-Process cmd.exe -ArgumentList "/c", "timeout /t 2 >nul & rmdir /s /q `"$installDir`"" -WindowStyle Hidden

Write-Host "Nythos Aquarium has been removed." -ForegroundColor Green
