param([Parameter(Mandatory=$true)][string]$GameDirectory, [switch]$Remove, [switch]$Update)
$ErrorActionPreference='Stop'
if (Get-Process koa -ErrorAction SilentlyContinue) { throw 'Close Amalur before changing the cinematic UI mod; the current play session is untouched.' }
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $game 'koa.exe'))) { throw 'Not an Amalur installation.' }
$mods=Join-Path $game 'mods'
$names=@('amalur_vr_cinematics.lua','amalur_vr_dialogue_gaze.lua','amalur_vr_cinematics_entry.lua','amalur_vr_cinematics.json')
# Preflight every target before any mutation; preserve independently edited files.
foreach ($name in $names) {
    $target=Join-Path $mods $name
    if ((Test-Path -LiteralPath $target) -and
        (Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $name)).Hash) {
        if (!$Update -or $Remove) { throw "Changed file retained: $target. Use -Update to back up and update this mod." }
    }
}
if ($Remove) {
    foreach ($name in $names) {
        $target=Join-Path $mods $name
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target }
    }
    Write-Output 'Removed native letterbox suppression. Stock UI returns on next launch.'
    return
}
if (!(Test-Path -LiteralPath (Join-Path $game 're_mod.dll'))) { throw 'The Re-Reckoning Lua mod framework must already be installed.' }
New-Item -ItemType Directory -Force -Path $mods | Out-Null
if ($Update) {
    $backup=Join-Path $game ('amalur-cinematics-backup-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    foreach ($name in $names) {
        $target=Join-Path $mods $name
        if (Test-Path -LiteralPath $target) { Copy-Item -LiteralPath $target -Destination $backup }
    }
    Write-Output "Previous cinematic UI mod backed up to $backup"
}
# Write the trigger manifest last so a failed script copy cannot activate it.
foreach ($name in $names) {
    $source=Join-Path $PSScriptRoot $name
    $target=Join-Path $mods $name
    Copy-Item -LiteralPath $source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $source).Hash) { throw "Copy verification failed: $name" }
}
Write-Output 'Installed dialogue and real-time cinematic letterbox suppression for the next launch.'
