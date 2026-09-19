param([ValidateSet('probe','session','game')][string]$Mode='probe')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$runtime=Join-Path $root 'research/OpenXR-Simulator/bin/openxr_simulator.dll'
if(-not(Test-Path -LiteralPath $runtime)){throw 'Build the Win32 simulator first; see research/NEW_VR_REFERENCES.md.'}
$manifestPath=Join-Path $root 'build/openxr-simulator-x86/runtime-x86.json'
@{file_format_version='1.0.0';runtime=@{library_path=$runtime;functions=@{xrNegotiateLoaderRuntimeInterface='_xrNegotiateLoaderRuntimeInterface@8'}}} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath
$previousRuntime=$env:XR_RUNTIME_JSON
try {
    $env:XR_RUNTIME_JSON=$manifestPath
    & (Join-Path $root 'build/xr-smoke-x86/amalur-xr-smoke.exe') "--$Mode"
    if($LASTEXITCODE -ne 0){throw "Simulator run failed: $LASTEXITCODE"}
} finally {$env:XR_RUNTIME_JSON=$previousRuntime}
