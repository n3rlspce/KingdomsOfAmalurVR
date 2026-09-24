$ErrorActionPreference = 'Stop'
$root = Join-Path (Get-Location) 'build/ci-sdk'
New-Item -ItemType Directory -Force $root | Out-Null
$packages = @(
    @{Name='loader'; Url='https://api.nuget.org/v3-flatcontainer/openxr.loader/1.0.10.2/openxr.loader.1.0.10.2.nupkg'; Hash='A9286F2977A207689E6838487C85685317309B7BD1F528655E552C760E335B12'},
    @{Name='headers'; Url='https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.0.27.zip'; Hash='557AE47FBA9E9C5535D661DAD411B18A306591A914CD78586C3102B7C818BFBD'}
)
foreach ($package in $packages) {
    $zip = Join-Path $root ($package.Name + '.zip')
    Invoke-WebRequest $package.Url -OutFile $zip
    if ((Get-FileHash -LiteralPath $zip).Hash -ne $package.Hash) { throw "Dependency hash mismatch: $($package.Name)" }
    Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $root $package.Name) -Force
}
Copy-Item -LiteralPath (Join-Path $root 'headers/OpenXR-SDK-release-1.0.27/include') -Destination (Join-Path $root 'loader/include') -Recurse -Force
