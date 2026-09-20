param([Parameter(Mandatory=$true)][string]$GameDirectory)
$ErrorActionPreference = 'Stop'
if (Get-Process koa -ErrorAction SilentlyContinue) { throw 'Close the game before installing the dispatcher.' }
$game = (Resolve-Path -LiteralPath $GameDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $game 'koa.exe'))) { throw 'Not an Amalur install.' }
$mods = Join-Path $game 'mods'
New-Item -ItemType Directory -Force -Path $mods | Out-Null
$names = @('amalur_dev.lua','amalur_dispatch.lua','amalur_dispatch.json')
$backup = Join-Path $game ('amalur-dispatch-backup-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach ($name in $names) {
    $target = Join-Path $mods $name
    if (Test-Path -LiteralPath $target) { Copy-Item -LiteralPath $target -Destination $backup }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $target -Force
    if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $name)).Hash) { throw "Copy verification failed: $name" }
}
$request = Join-Path $mods 'amalur_request.lua'
if (Test-Path -LiteralPath $request) { Move-Item -LiteralPath $request -Destination $backup }
Write-Output "Installed game-update dispatcher. Backup: $backup. Framework DLL activation is managed separately."
