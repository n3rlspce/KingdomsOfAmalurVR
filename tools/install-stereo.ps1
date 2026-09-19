param([string]$GameDirectory = $env:AMALUR_GAME_DIR, [switch]$Remove)
$ErrorActionPreference = 'Stop'
if (-not $GameDirectory) { throw 'Pass -GameDirectory or set AMALUR_GAME_DIR to your game installation.' }
$root = Split-Path -Parent $PSScriptRoot
$gameRoot = [IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$receipt = Join-Path $root 'build\stereo-install.json'
if (Get-Process koa -ErrorAction SilentlyContinue) { throw 'Exit Amalur before changing the stereo installation.' }
function CheckedTarget([string]$relative) {
    $resolved = [IO.Path]::GetFullPath((Join-Path $gameRoot $relative))
    if (-not $resolved.StartsWith($gameRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Path outside game directory.' }
    return $resolved
}
if ($Remove) {
    if (-not (Test-Path -LiteralPath $receipt)) { throw 'No stereo installation receipt.' }
    $installed = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    if ($installed.gameRoot -ne $gameRoot) { throw 'Receipt directory mismatch.' }
    foreach ($file in $installed.files) {
        $target = CheckedTarget $file.relative
        if ((Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target).Hash -ne $file.sha256) { throw "Changed file retained; removal stopped: $target" }
    }
    foreach ($file in $installed.files) {
        $target = CheckedTarget $file.relative
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target }
    }
    Remove-Item -LiteralPath $receipt
    Write-Host 'Stereo package removed; generated logs/caches retained.'
    return
}
if (Test-Path -LiteralPath $receipt) { throw 'Stereo is already installed.' }
if ((Get-FileHash -LiteralPath (Join-Path $gameRoot 'koa.exe')).Hash -ne '16A400F6E8FC10DBE446A9E57717DE9FBE975EF17C004F4CA405E2E4E09CB314') { throw 'Unverified game executable.' }
$stage = Join-Path $root ('build\stereo-stage-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $stage | Out-Null
$fix = Join-Path $root 'research\amalur-stereo-fix\KOARR_Release_Alpha_0.1'
$geo = Join-Path $root 'research\geo11\x32'
Copy-Item -LiteralPath (Join-Path $fix 'ShaderFixes') -Destination $stage -Recurse
& (Join-Path $PSScriptRoot 'build-hud-shaders.ps1') -OutputDirectory (Join-Path $stage 'ShaderFixes')
foreach ($name in @('d3d11.dll','nvapi.dll','d3dcompiler_47.dll')) { Copy-Item -LiteralPath (Join-Path $geo $name) -Destination $stage }
Copy-Item -LiteralPath (Join-Path $fix 'd3dcompiler_46.dll') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'build\diagnostic-x86\RelWithDebInfo\d3d11.dll') -Destination (Join-Path $stage 'amalur_camera.dll')
$ini = Get-Content -LiteralPath (Join-Path $fix 'd3dx.ini') -Raw
$ini = $ini -replace '(?m)^force_stereo\s*=.*$', 'force_stereo=2'
$ini = $ini -replace '(?m)^;proxy_d3d11=.*$', 'proxy_d3d11=amalur_camera.dll'
$ini = $ini -replace '(?m)^load_library_redirect\s*=.*$', 'load_library_redirect=0'
$ini = $ini -replace '(?m)^;width=1280.*$', 'width=3840'
$ini = $ini -replace '(?m)^;height=720.*$', 'height=2160'
$ini = $ini -replace '(?m)^reload_config\s*=.*$', 'reload_config = no_modifiers VK_F11'
$ini = $ini -replace '(?m)^reload_fixes\s*=.*$', 'reload_fixes = no_modifiers VK_F11'
$ini = $ini -replace '(?m)^\[Constants\]', "[Constants]`r`nx2 = 0.67`r`ny2 = 0.70`r`nz2 = 0.60`r`nw2 = 0.25`r`nx3 = 0"
$ini += @'

; Draw-based HUD gate: minimap required. The donor's 'menu' shader is also
; used by gameplay HUD, so it cannot serve as a menu veto.
; This is a rendering heuristic, not an authoritative native UI state.
[PresetVRGameplayHud]
x3 = 1
transition = 0
release_transition = 0

[ShaderOverrideVRMinimapPresence]
hash = cc7258d9790a0bbd
preset = PresetVRGameplayHud

; F6 compares auto-gated curved HUD / stock layout; cannot bypass the gate.
[KeyVRHudLayout]
key = NO_MODIFIERS VK_F6
type = cycle
x2 = 0.67, 0

; Empirical HUD disparity comparison; not physical metres.
[KeyVRHudDepth]
key = NO_MODIFIERS VK_F4
type = cycle
w2 = 0.25, 0.5, 0.10, 0.0
'@
Set-Content -LiteralPath (Join-Path $stage 'd3dx.ini') -Value $ini -Encoding ascii
Set-Content -LiteralPath (Join-Path $stage 'amalur-source.ini') -Value "[Source]`r`nWidth=3840`r`nHeight=2160" -Encoding ascii
$dm = Get-Content -LiteralPath (Join-Path $geo 'd3dxdm.ini') -Raw
$dm = $dm -replace '(?m)^direct_mode\s*=.*$', 'direct_mode = katanga_vr'
$dm = $dm -replace '(?m)^dm_separation\s*=.*$', 'dm_separation = 20'
$dm = $dm -replace '(?m)^dm_convergence\s*=.*$', 'dm_convergence = 100.0'
Set-Content -LiteralPath (Join-Path $stage 'd3dxdm.ini') -Value $dm -Encoding ascii
$entries = @(Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
    @{relative=$_.FullName.Substring($stage.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
# Preflight every file before removing the known diagnostic DLL.
$oldReceipt = Join-Path $root 'build\diagnostic-install.json'
foreach ($file in $entries) {
    $target = CheckedTarget $file.relative
    if (Test-Path -LiteralPath $target) {
        if ($file.relative -eq 'd3d11.dll' -and (Test-Path -LiteralPath $oldReceipt)) {
            $old = Get-Content -LiteralPath $oldReceipt -Raw | ConvertFrom-Json
            if ($old.path -eq $target -and (Get-FileHash -LiteralPath $target).Hash -eq $old.sha256) { continue }
        }
        throw "Existing file would be overwritten: $target"
    }
}
if (Test-Path -LiteralPath $oldReceipt) { & (Join-Path $PSScriptRoot 'install-diagnostic.ps1') -GameDirectory $gameRoot -Remove }
$state = @{gameRoot=$gameRoot;files=@();installedAt=(Get-Date -Format o);stage=$stage}
$state | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $receipt
foreach ($file in $entries) {
    $target = CheckedTarget $file.relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath (Join-Path $stage $file.relative) -Destination $target
    $state.files += $file
    $state | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $receipt
}
Write-Host "Installed experimental geo-11 stereo + Amalur camera chain ($($entries.Count) files)."
& (Join-Path $PSScriptRoot 'refresh-hud-cache.ps1') -GameDirectory $gameRoot
