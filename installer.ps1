# Build the Inno Setup installer (run build.bat first).
# Installs Inno Setup via Chocolatey if it isn't already present.
param([string]$Version = "0.0.0-dev")

$ErrorActionPreference = "Stop"

function Find-Iscc {
    Get-ChildItem "${env:ProgramFiles(x86)}\Inno Setup*\ISCC.exe",
                  "$env:ProgramFiles\Inno Setup*\ISCC.exe",
                  "$env:LOCALAPPDATA\Programs\Inno Setup*\ISCC.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

$iscc = Find-Iscc
if (-not $iscc) {
    choco install innosetup -y --no-progress
    $iscc = Find-Iscc
}
if (-not $iscc) { throw "Inno Setup (ISCC.exe) not found" }

& $iscc /Q "/DAppVersion=$Version" "$PSScriptRoot\wineyes.iss"
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }
