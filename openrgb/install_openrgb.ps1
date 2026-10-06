# Replace an installed OpenRGB (Windows service "OpenRGB") with your own build that contains the AULA F108 Pro controller.
# Run from an administrator PowerShell. Edit the three paths below first.
# Undo: restore_openrgb.ps1
param(
    [string]$BuildDir   = ".\OpenRGB\release",          # folder with your built OpenRGB.exe (after windeployqt)
    [string]$InstallDir = "C:\Program Files\OpenRGB",    # installed OpenRGB
    [string]$Backup     = ".\_openrgb_original_backup",  # where the original install is copied
    [string]$Service    = "OpenRGB"                      # service name, empty if you do not run a service
)
$ErrorActionPreference = "Stop"

if (-not (Test-Path "$BuildDir\OpenRGB.exe")) { throw "OpenRGB.exe not found in $BuildDir" }

Write-Host "1) stopping service and processes"
if ($Service) {
    $svc = Get-Service -Name $Service -ErrorAction SilentlyContinue
    if ($svc -and $svc.Status -ne "Stopped") { Stop-Service -Name $Service -Force; $svc.WaitForStatus("Stopped", "00:00:20") }
}
Get-Process OpenRGB -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2

Write-Host "2) backing up the original install to $Backup"
if (-not (Test-Path $Backup)) {
    New-Item -ItemType Directory -Path $Backup | Out-Null
    robocopy $InstallDir $Backup /E /NFL /NDL /NJH /NJS /NP | Out-Null
} else { Write-Host "   backup already exists, not overwriting" }

Write-Host "3) copying the new build (plugins, service_config and OpenRGB.json are kept)"
robocopy $BuildDir $InstallDir /E /XD plugins service_config /XF OpenRGB.json /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed, exit code $LASTEXITCODE" }

Write-Host "4) disabling the Keychron detector (same USB ID as the AULA dongle, different protocol)"
foreach ($cfg in @("$InstallDir\service_config\OpenRGB.json", "$env:APPDATA\OpenRGB\OpenRGB.json")) {
    if (Test-Path $cfg) {
        Copy-Item $cfg "$cfg.bak_before_aula" -Force
        $text = [System.IO.File]::ReadAllText($cfg)
        $new  = [regex]::Replace($text, '("Keychron Gaming Keyboard 1"\s*:\s*)true', '${1}false')
        if ($new -ne $text) { [System.IO.File]::WriteAllText($cfg, $new); Write-Host "   disabled in $cfg" }
    }
}

Write-Host "5) starting service"
if ($Service -and $svc) { Start-Service -Name $Service; Start-Sleep 5; Get-Service $Service | Format-Table Name,Status -AutoSize }
Write-Host "DONE"
