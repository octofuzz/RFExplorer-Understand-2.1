$ErrorActionPreference = "Stop"

Push-Location $PSScriptRoot

try {
    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
        throw "PlatformIO CLI (pio) must be available in PATH."
    }

    pio run -e cardputer_adv

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Get-ChildItem .\firmware\RFExplorer-v2.8-*.bin |
        Select-Object Name,Length,LastWriteTime

    Write-Host ""
    Write-Host "RFExplorer 2.8 Observe build complete."
    Write-Host "Merged image: flash at offset 0x0"
    Write-Host "App-only image: offset 0x10000 with matching bootloader/partitions"
}
finally {
    Pop-Location
}
