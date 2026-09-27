$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
try {
    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) { throw "PlatformIO CLI (pio) must be available in PATH." }
    pio run -e cardputer_adv
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    Get-ChildItem .\firmware\RFExplorer-v2.1-*.bin | Select-Object Name,Length
    Write-Host "Merged: flash 0x0. App-only: 0x10000 with matching partitions."
} finally { Pop-Location }
