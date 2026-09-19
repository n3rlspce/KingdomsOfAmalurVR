$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build\diagnostic-x86'
& cmake -S $root -B $build -G 'Visual Studio 17 2022' -A Win32
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& cmake --build $build --config RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw 'Diagnostic build failed' }
