param(
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [Parameter(Mandatory=$true)][string]$PackageDirectory,
    [switch]$Undo
)
$ErrorActionPreference='Stop'
if (Get-Process koa -ErrorAction SilentlyContinue) { throw 'Close the game before changing startup assets.' }
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$manifest=Get-Content -LiteralPath (Join-Path $PackageDirectory 'manifest.json') -Raw | ConvertFrom-Json
$patch=Join-Path $game 'data/patch_0.pak'
$backup=$patch+'.amalur-fast-start-backup'
$logos=@('logo_kaiko.bik','logo_thq_nordic.bik') | ForEach-Object { Join-Path $game ('data/videos/'+$_) }
if ($Undo) {
    if ((Get-FileHash -LiteralPath $patch).Hash -ne $manifest.patchedHash) { throw 'Installed patch changed; refusing to overwrite another change.' }
    if ((Get-FileHash -LiteralPath $backup).Hash -ne $manifest.originalHash) { throw 'Backup differs from original.' }
    foreach ($logo in $logos) {
        if (!(Test-Path -LiteralPath ($logo+'.amalur-fast-start-backup')) -or (Test-Path -LiteralPath $logo)) { throw 'Logo restoration paths differ from expected state.' }
    }
    Copy-Item -LiteralPath $backup -Destination $patch
    foreach ($logo in $logos) { Move-Item -LiteralPath ($logo+'.amalur-fast-start-backup') -Destination $logo }
    Remove-Item -LiteralPath $backup
    Write-Output 'Original startup restored.'
    return
}
$package=Join-Path $PackageDirectory 'patch_0.pak'
if ((Get-FileHash -LiteralPath $patch).Hash -ne $manifest.originalHash) { throw 'Installed archive differs from the staged build input.' }
if ((Get-FileHash -LiteralPath $package).Hash -ne $manifest.patchedHash) { throw 'Staged package hash mismatch.' }
if (Test-Path -LiteralPath $backup) { throw 'A startup backup already exists.' }
foreach ($logo in $logos) {
    if (!(Test-Path -LiteralPath $logo) -or (Test-Path -LiteralPath ($logo+'.amalur-fast-start-backup'))) { throw 'Logo backup paths are not ready.' }
}
Copy-Item -LiteralPath $patch -Destination $backup
try {
    Copy-Item -LiteralPath $package -Destination $patch
    foreach ($logo in $logos) { Move-Item -LiteralPath $logo -Destination ($logo+'.amalur-fast-start-backup') }
} catch {
    Copy-Item -LiteralPath $backup -Destination $patch
    foreach ($logo in $logos) {
        if (Test-Path -LiteralPath ($logo+'.amalur-fast-start-backup')) { Move-Item -LiteralPath ($logo+'.amalur-fast-start-backup') -Destination $logo }
    }
    Remove-Item -LiteralPath $backup
    throw
}
Write-Output 'Direct startup patch installed. Validate on next launch; Undo restores all three files.'
