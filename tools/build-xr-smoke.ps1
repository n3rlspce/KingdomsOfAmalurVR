param([string]$OpenXrSdk = $env:OPENXR_SDK_DIR, [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
if (-not $OpenXrSdk) { throw 'Pass -OpenXrSdk or set OPENXR_SDK_DIR to the OpenXR.Loader package directory.' }
$OpenXrSdk = [IO.Path]::GetFullPath($OpenXrSdk)
$projectRoot = Split-Path -Parent $PSScriptRoot
$output = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $projectRoot 'build\xr-smoke-x86' }
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x86 tools not found' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$include = Join-Path $OpenXrSdk 'include'
$lib = Join-Path $OpenXrSdk 'native\Win32\release\lib'
$loader = Join-Path $OpenXrSdk 'native\Win32\release\bin\openxr_loader.dll'
foreach ($required in @($vcvars,$include,$lib,$loader)) { if (-not (Test-Path -LiteralPath $required)) { throw "Missing $required" } }
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Join-Path $projectRoot 'src\xr_smoke\main.cpp'
Push-Location $output
try {
    $command = "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /MD /std:c++17 /I`"$include`" `"$source`" /Fe:amalur-xr-smoke.exe /link /LIBPATH:`"$lib`" openxr_loader.lib d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib ole32.lib oleaut32.lib"
    & cmd /d /c $command
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $LASTEXITCODE" }
    Copy-Item -LiteralPath $loader -Destination (Join-Path $output 'openxr_loader.dll') -Force
} finally { Pop-Location }
Write-Host "Built $output\amalur-xr-smoke.exe (x86); no game files changed."

$preset = Join-Path $projectRoot "config/amalur-vr.ini"
$settings = Join-Path $output "amalur-vr.ini"
if ((Test-Path -LiteralPath $preset) -and !(Test-Path -LiteralPath $settings)) { Copy-Item -LiteralPath $preset -Destination $settings }
