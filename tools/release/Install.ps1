param([string]$GameDirectory, [string]$DependencyDirectory, [switch]$CheckOnly, [switch]$Launch, [switch]$Offline)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$package = $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
$game = Find-Game $GameDirectory -Choose
Assert-Closed
$exe = Join-Path $game 'koa.exe'
$exeHash = Hash $exe
if ($exeHash -notin @($manifest.game.originalHash,$manifest.game.patchedHash)) {
    throw 'This game executable is not supported by this preview. Use the supported Steam Re-Reckoning build. No files changed.'
}
$stateRoot = Join-Path $game '.amalur-vr-installer'
$receiptPath = Join-Path $stateRoot 'receipt.json'
$previous = $null
if (Test-Path -LiteralPath $receiptPath) { $previous = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json }
if ($previous -and $previous.game -ne $game) { throw 'Installation receipt belongs to a different game directory.' }

# Dependencies are imported, never executed. Only exact manifest hashes qualify.
$searchRoots = @($game, (Join-Path $package 'Dependencies'))
if ($DependencyDirectory) { $searchRoots += (Resolve-Path -LiteralPath $DependencyDirectory).Path }
function Index-Dependencies {
$script:index = @{}
foreach ($root in $searchRoots) {
    if (!(Test-Path -LiteralPath $root)) { continue }
    Get-ChildItem -LiteralPath $root -File -Recurse | Where-Object {
        $_.FullName -notlike '*\.amalur-vr-installer\*' -and $_.Extension -notin @('.pak','.log','.bin')
    } | ForEach-Object {
        if (!$index.ContainsKey($_.Name)) { $index[$_.Name] = @() }
        $index[$_.Name] += $_.FullName
    }
}
}
Index-Dependencies
if (!$Offline -and !$CheckOnly) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    foreach ($download in $manifest.downloads) {
        $needed=$false
        foreach($entry in @($manifest.files | Where-Object dependency -eq $download.id)) {
            $found=$false
            foreach($candidate in @($index[$entry.name])) { if($candidate -and (Hash $candidate) -eq $entry.sha256){$found=$true;break} }
            if(!$found){$needed=$true;break}
        }
        if(!$needed){continue}
        $cache=Join-Path $package ('Dependencies/download-'+$download.id)
        New-Item -ItemType Directory -Path $cache -Force | Out-Null
        $archive=Safe-Path $cache $download.name
        if(!(Test-Path -LiteralPath $archive) -or (Hash $archive) -ne $download.sha256) {
            Write-Host "Downloading $($download.id) from the original author host..."
            Invoke-WebRequest -UseBasicParsing -Uri $download.url -OutFile $archive
        }
        if((Hash $archive) -ne $download.sha256){throw 'Downloaded archive checksum failed; no game files changed.'}
        $tar=Get-Command tar.exe -ErrorAction SilentlyContinue
        if(!$tar){throw 'Windows tar is unavailable. Extract the original archives into Dependencies manually.'}
        $entries=& $tar.Source -tf $archive
        if($LASTEXITCODE -ne 0){throw 'Cannot read dependency archive.'}
        foreach($name in $entries){if($name.TrimEnd('/')){ $null=Safe-Path $cache $name.TrimEnd('/') }}
        & $tar.Source -xf $archive -C $cache
        if($LASTEXITCODE -ne 0){throw 'Dependency extraction failed.'}
    }
    Index-Dependencies
}
$plan = @(); $missing = @()
foreach ($entry in $manifest.files) {
    $target = Safe-Path $game $entry.target
    $source = $null
    if ($entry.source) {
        $source = Safe-Path $package $entry.source
        if (!(Test-Path -LiteralPath $source) -or (Hash $source) -ne $entry.sha256) { throw "Package is incomplete or damaged: $($entry.target)" }
    } else {
        foreach ($candidate in @($index[$entry.name])) {
            if ($candidate -and (Hash $candidate) -eq $entry.sha256) { $source = $candidate; break }
        }
        if (!$source) { $missing += $entry; continue }
    }
    $plan += [pscustomobject]@{entry=$entry;source=$source;target=$target}
}
if ($missing.Count) {
    $groups = $missing | Select-Object -ExpandProperty dependency -Unique
    Write-Host "Missing verified dependencies: $($groups -join ', ')" -ForegroundColor Yellow
    foreach ($group in $groups) { $d = $manifest.dependencies | Where-Object id -eq $group; Write-Host "$($d.title): $($d.url)" }
    Write-Host 'Extract the original dependency downloads into the Dependencies folder beside this installer, then run Install again.'
    Write-Host 'The installer searches subfolders; do not copy files into the game yourself.'
    throw 'Dependency preflight stopped before changing the game. See DEPENDENCIES.md.'
}
# Generate the patched executable entirely from the player's own copy.
$patched = $null
if ($exeHash -eq $manifest.game.originalHash) {
    $patchPath = Safe-Path $package $manifest.game.patch
    if ((Hash $patchPath) -ne $manifest.game.patchHash) { throw 'Executable patch checksum failed.' }
    $patch = Get-Content -LiteralPath $patchPath -Raw | ConvertFrom-Json
    $original = [IO.File]::ReadAllBytes($exe)
    $patched = New-Object byte[] ([int]$patch.newLength)
    [Array]::Copy($original,$patched,[Math]::Min($original.Length,$patched.Length))
    foreach ($chunk in $patch.chunks) {
        $bytes = [Convert]::FromBase64String($chunk.data)
        if ($chunk.offset -lt 0 -or ($chunk.offset + $bytes.Length) -gt $patched.Length) { throw 'Invalid executable patch range.' }
        [Array]::Copy($bytes,0,$patched,[int]$chunk.offset,$bytes.Length)
    }
    if ((Hash-Bytes $patched) -ne $manifest.game.patchedHash) { throw 'Patched executable checksum failed.' }
}
if ($CheckOnly) { Write-Host "Preflight passed: $($plan.Count) files and supported executable. No game files changed."; return }
Assert-Closed
$transaction = Join-Path $stateRoot ('transactions/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $transaction -Force | Out-Null
$undo = @(); $records = @{}
if ($previous) { foreach ($record in $previous.files) { $records[$record.target] = $record } }
function Stage-Write($relative, $bytes, $preserve) {
    $target = Safe-Path $game $relative
    if ($preserve -and (Test-Path -LiteralPath $target)) { return }
    $exists = Test-Path -LiteralPath $target
    $beforeHash = if ($exists) { Hash $target } else { $null }
    $afterHash = Hash-Bytes $bytes
    if ($beforeHash -eq $afterHash) { return }
    $rollback = Join-Path $transaction ($script:undo.Count.ToString() + '.bak')
    if ($exists) { Copy-Item -LiteralPath $target -Destination $rollback }
    $script:undo += [pscustomobject]@{target=$target;existed=$exists;backup=$rollback}
    if (!$records.ContainsKey($relative)) {
        $originalBackup = $null
        if ($exists) {
            $originalBackup = 'original/' + [Guid]::NewGuid().ToString('N') + '.bak'
            $backupPath = Safe-Path $stateRoot $originalBackup
            New-Item -ItemType Directory -Path (Split-Path $backupPath) -Force | Out-Null
            Copy-Item -LiteralPath $target -Destination $backupPath
        }
        $records[$relative] = [pscustomobject]@{target=$relative;existed=$exists;backup=$originalBackup;originalHash=$beforeHash;installedHash=$afterHash}
    } else { $records[$relative].installedHash = $afterHash }
    New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
    [IO.File]::WriteAllBytes($target,$bytes)
    if ((Hash $target) -ne $afterHash) { throw "Copy verification failed: $relative" }
}
try {
    foreach ($item in $plan) { Stage-Write $item.entry.target ([IO.File]::ReadAllBytes($item.source)) ([bool]$item.entry.preserve) }
    if ($patched) { Stage-Write 'koa.exe' $patched $false }
    # Only exact known HUD caches; do not remove manually authored conversions.
    foreach ($hash in $manifest.hudCaches) {
        $text = Safe-Path $game "ShaderFixesDM/$hash-vs.txt"
        if (Test-Path -LiteralPath $text) {
            if (!(Get-Content -LiteralPath $text -Raw).StartsWith('// AUTOMATICALLY CONVERTED FROM SHADER FIXES')) { continue }
        }
        foreach ($ext in @('txt','bin')) {
            $cache = Safe-Path $game "ShaderFixesDM/$hash-vs.$ext"
            if (Test-Path -LiteralPath $cache) {
                $backup = Join-Path $transaction ($script:undo.Count.ToString()+'.bak')
                Copy-Item -LiteralPath $cache -Destination $backup
                $script:undo += [pscustomobject]@{target=$cache;existed=$true;backup=$backup}
                Remove-Item -LiteralPath $cache
            }
        }
    }
    $receipt = @{version=$manifest.version;game=$game;files=@($records.Values);installedAt=(Get-Date -Format o)}
    $temporaryReceipt = Join-Path $stateRoot 'receipt.new.json'
    $receipt | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $temporaryReceipt -Encoding UTF8
    Move-Item -LiteralPath $temporaryReceipt -Destination $receiptPath -Force
} catch {
    [Array]::Reverse($script:undo)
    foreach ($item in $script:undo) {
        if ($item.existed) { Copy-Item -LiteralPath $item.backup -Destination $item.target -Force }
        elseif (Test-Path -LiteralPath $item.target) { Remove-Item -LiteralPath $item.target }
    }
    throw
}
Set-Content -LiteralPath (Join-Path $package 'installed-game.txt') -Value $game -Encoding UTF8
Write-Host 'Installation verified. Saves were not copied or changed. Existing VR settings were preserved.' -ForegroundColor Green
if ($Launch) { & (Join-Path $package 'Launch.ps1') -GameDirectory $game }
