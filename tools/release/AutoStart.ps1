$ErrorActionPreference='Stop'
$logs=Join-Path $PSScriptRoot 'logs'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$log=Join-Path $logs 'steam-autostart.log'
try {
    "$(Get-Date -Format o) Steam/game startup requested VR." | Set-Content -LiteralPath $log
    & (Join-Path $PSScriptRoot 'Launch.ps1') -GameDirectory (Split-Path $PSScriptRoot) -BridgeOnly *>> $log
} catch {
    "Startup failed: $($_.Exception.Message)" | Add-Content -LiteralPath $log
}
