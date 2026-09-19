$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$output=Join-Path $root 'build/transport-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
# Keep graphics tests away from the diagnostic d3d11 proxy beside build outputs.
foreach($name in @('paired_check','stereo_check','tracking_check','hud_check')){
    Copy-Item -LiteralPath (Join-Path $root "build/diagnostic-x86/RelWithDebInfo/$name.exe") -Destination $output -Force
    & (Join-Path $output "$name.exe")
    if($LASTEXITCODE -ne 0){throw "$name failed: $LASTEXITCODE"}
}
& (Join-Path $root 'build/xr-smoke-x86/amalur-xr-smoke.exe') --pose-math
if($LASTEXITCODE -ne 0){throw 'Rendered-eye pose math failed'}
