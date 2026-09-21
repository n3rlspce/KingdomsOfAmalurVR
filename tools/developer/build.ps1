param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'build\developer' }
$out = [IO.Path]::GetFullPath($OutputDirectory)
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC tools not found.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Push-Location $out
try {
    $source = Join-Path $PSScriptRoot 'console_send.cpp'
    & cmd /d /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /MD /std:c++17 `"$source`" /Fe:amalur-dev-send.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Developer helper build failed.' }
    & (Join-Path $out 'amalur-dev-send.exe') --selftest
    if ($LASTEXITCODE -ne 0) { throw 'Developer helper checks failed.' }
    $check = Join-Path $PSScriptRoot 'panel_check.cpp'
    & cmd /d /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /MD /std:c++17 `"$check`" /Fe:developer-panel-check.exe /link user32.lib"
    if ($LASTEXITCODE -ne 0) { throw 'Panel check build failed.' }
    & (Join-Path $out 'developer-panel-check.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Panel checks failed.' }
    foreach ($name in @('amalur_dev.lua','amalur_dispatch.lua','amalur_dispatch.json','amalur_menu.lua','amalur_menu.json')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $out -Force
    }
} finally { Pop-Location }
Write-Host "Built developer helper in $out; no game files changed."
