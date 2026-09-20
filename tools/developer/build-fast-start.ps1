param(
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../../build/fast-start'),
    [string]$Python = 'python',
    [string]$BaseArchive
)
$ErrorActionPreference = 'Stop'
$game = (Resolve-Path -LiteralPath $GameDirectory).Path
$out = [IO.Path]::GetFullPath($OutputDirectory)
if ($out.StartsWith($game + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Stage outside the installed game.' }
$unpack = Join-Path $game 'modding/pakfileunpacker.exe'
$builder = Join-Path $game 'modding/pakfilebuilder.exe'
$original = Join-Path $game 'data/patch_0.pak'
if ($BaseArchive) { $original = (Resolve-Path -LiteralPath $BaseArchive).Path }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$source = Join-Path $out 'source'
$contents = Join-Path $out 'contents'
if (Test-Path -LiteralPath $contents) { throw 'Use a fresh output directory; existing unpacked contents may be stale.' }
New-Item -ItemType Directory -Force -Path $source,$contents | Out-Null
& $unpack (Join-Path $game 'data/initial_0.pak') unpack $source 13493.lua_bxml 1645799.lua_bxml
if ($LASTEXITCODE) { throw 'Initial asset extraction failed.' }
& $unpack $original unpack $contents
if ($LASTEXITCODE) { throw 'Patch extraction failed.' }
$expectedFiles = @(& $unpack $original list)
if (!$expectedFiles.Count) { throw 'No original patch entries found.' }
foreach ($entry in $expectedFiles) {
    if (!(Test-Path -LiteralPath (Join-Path $contents $entry.Trim()))) { throw "Missing original patch entry: $entry" }
}
# Never accidentally replace a newer script from the installed patch.
foreach ($name in @('13493.lua_bxml','1645799.lua_bxml')) {
    $override = Join-Path $contents $name
    if (Test-Path -LiteralPath $override) { Copy-Item -LiteralPath $override -Destination (Join-Path $source $name) }
}
& $Python (Join-Path $PSScriptRoot 'fast_start.py') $source $contents
if ($LASTEXITCODE) { throw 'Startup patch validation failed.' }
$activeBatch = Join-Path $contents '134230570_klua.batch'
& $Python (Join-Path $PSScriptRoot 'fast_start.py') --batch $activeBatch $activeBatch
if ($LASTEXITCODE) { throw 'Active startup batch validation failed.' }
$list = Join-Path $out 'files.txt'
[IO.File]::WriteAllLines($list, [string[]](Get-ChildItem -LiteralPath $contents -File -Recurse | Sort-Object FullName | ForEach-Object FullName))
$package = Join-Path $out 'patch_0.pak'
& $builder -c $list $package
if ($LASTEXITCODE -or !(Test-Path -LiteralPath $package)) { throw 'Archive build failed.' }
$verify = Join-Path $out 'verify'
New-Item -ItemType Directory -Force -Path $verify | Out-Null
& $unpack $package unpack $verify 13493.lua_bxml 1645799.lua_bxml 134230570_klua.batch
if ($LASTEXITCODE) { throw 'Archive verification failed.' }
foreach ($name in @('13493.lua_bxml','1645799.lua_bxml','134230570_klua.batch')) {
    if ((Get-FileHash -LiteralPath (Join-Path $verify $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $contents $name)).Hash) { throw 'Packaged script differs from staged script.' }
}
@{ originalHash=(Get-FileHash -LiteralPath $original).Hash; patchedHash=(Get-FileHash -LiteralPath $package).Hash } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'manifest.json')
Write-Output "Staged: $package (not installed; runtime validation pending)"
