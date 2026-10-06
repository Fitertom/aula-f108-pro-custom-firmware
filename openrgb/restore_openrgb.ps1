# Restore the original OpenRGB saved by install_openrgb.ps1. Run from an administrator PowerShell.
param(
    [string]$InstallDir = "C:\Program Files\OpenRGB",
    [string]$Backup     = ".\_openrgb_original_backup",
    [string]$Service    = "OpenRGB"
)
if (-not (Test-Path $Backup)) { throw "no backup in $Backup" }
if ($Service) { Stop-Service -Name $Service -Force -ErrorAction SilentlyContinue }
Get-Process OpenRGB -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
robocopy $Backup $InstallDir /E /NFL /NDL /NJH /NJS /NP | Out-Null
if ($Service) { Start-Service -Name $Service -ErrorAction SilentlyContinue }
"Original OpenRGB restored"
