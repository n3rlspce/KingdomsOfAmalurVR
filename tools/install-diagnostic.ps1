param([string]$GameDirectory = $env:AMALUR_GAME_DIR, [switch]$Remove)
$ErrorActionPreference = 'Stop'
if (-not $GameDirectory) { throw 'Pass -GameDirectory or set AMALUR_GAME_DIR to your game installation.' }
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'build\diagnostic-x86\RelWithDebInfo\d3d11.dll'
$target = Join-Path $GameDirectory 'd3d11.dll'
$receipt = Join-Path $root 'build\diagnostic-install.json'
if (Get-Process koa -ErrorAction SilentlyContinue) { throw 'Exit Amalur before installing/removing diagnostics.' }
if ($Remove) {
    if (-not (Test-Path -LiteralPath $receipt)) { throw 'No installation receipt; will not remove an unknown DLL.' }
    $installed = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    if ([IO.Path]::GetFullPath($target) -ne $installed.path) { throw 'Receipt path mismatch.' }
    if (Test-Path -LiteralPath $target) {
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $installed.sha256) { throw 'DLL changed since install; refusing removal.' }
        Remove-Item -LiteralPath $target
    }
    Remove-Item -LiteralPath $receipt
    Write-Host 'Diagnostic DLL removed. Log retained.'
    return
}
if (Test-Path -LiteralPath $target) { throw 'A d3d11.dll already exists; refusing to overwrite.' }
$exe = Join-Path $GameDirectory 'koa.exe'
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne '16A400F6E8FC10DBE446A9E57717DE9FBE975EF17C004F4CA405E2E4E09CB314') { throw 'Unverified game build.' }
$hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
Copy-Item -LiteralPath $source -Destination $target
@{path=[IO.Path]::GetFullPath($target);sha256=$hash;installedAt=(Get-Date -Format o)} | ConvertTo-Json | Set-Content -LiteralPath $receipt
Write-Host "Installed diagnostic DLL: $target"
