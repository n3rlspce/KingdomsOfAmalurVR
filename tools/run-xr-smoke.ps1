param([ValidateSet('probe','session','track','game')][string]$Mode = 'probe')
$ErrorActionPreference = 'Stop'
$output = Join-Path (Split-Path -Parent $PSScriptRoot) 'build\xr-smoke-x86'
$exe = Join-Path $output 'amalur-xr-smoke.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Run tools/build-xr-smoke.ps1 first.' }
$logs = Join-Path $output 'logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$log = Join-Path $logs ("{0}-{1}.txt" -f $Mode,(Get-Date -Format 'yyyyMMdd-HHmmss'))
& $exe "--$Mode" 2>&1 | Tee-Object -FilePath $log
$result = $LASTEXITCODE
Write-Host "Exit code: $result; log: $log"
exit $result
