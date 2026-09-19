param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not $OutputDirectory){$OutputDirectory=Join-Path $root 'build/hud-shaders'}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$donor=Join-Path $root 'research/amalur-stereo-fix/KOARR_Release_Alpha_0.1/ShaderFixes'
# Preserve mikear69's shader signatures, original transforms and texture inputs.
# Shared UI shaders require a separate draw-based gate; labels are not ownership.
$placement=@'
// Amalur VR curved HUD candidate, built on mikear69's shader identification.
// IniParams[2]: horizontal extent, vertical extent, half arc radians, disparity.
float4 vrHud = IniParams.Load(int2(2, 0));
float4 hudControl = AmalurHudSettings.Load(int2(0, 0));
float hudSize = hudControl.y > 0.5 ? clamp(hudControl.x, 0.4, 1.2) : 0.8;
vrHud.xy *= hudSize;
// Row 3 X is enabled by a minimap-triggered geo-11 preset.
// Defaults to zero and expires when the triggering draw disappears.
float4 vrHudGate = IniParams.Load(int2(3, 0));
if (vrHudGate.x > 0.5 && vrHud.x > 0.0 && abs(o0.w) > 0.00001) {
    float2 canvas = o0.xy / o0.w;
    float arc = clamp(vrHud.z, 0.01, 0.9);
    float angle = clamp(canvas.x, -1.0, 1.0) * arc;
    float curveZ = cos(angle);
    float oldW = o0.w;
    // Approximate a cylindrical canvas on the existing UI quads. Preserve W:
    // geo-11 treats W != 1 as world geometry and would add a second depth shift.
    o0.x = tan(angle) / tan(arc) * vrHud.x * oldW;
    o0.y = canvas.y * vrHud.y * cos(arc) * oldW / curveZ;
    // Empirical stereo offset, not calibrated metres. Keep independently tunable.
    o0.x += stereo.x * vrHud.w * oldW / curveZ;
} else {
    o0.x += stereo.x * hud;
}
'@
foreach($hash in @('887f6506d28f9ff1','bd9cebc4f7e1ed36','cc7258d9790a0bbd','bf098be2e4587ca5')){
    $name="$hash-vs_replace.txt"
    $text=Get-Content -LiteralPath (Join-Path $donor $name) -Raw
    $text=$text.Replace('Texture1D<float4> IniParams', 'Texture1D<float4> AmalurHudSettings : register(t119);'+[Environment]::NewLine+'Texture1D<float4> IniParams')
    $needle='o0.x += stereo.x*hud;'
    if(-not $text.Contains($needle)){throw "Expected donor HUD adjustment missing: $name"}
    [IO.File]::WriteAllText((Join-Path $OutputDirectory $name),$text.Replace($needle,$placement),[Text.Encoding]::ASCII)
}
Write-Host "Generated 4 gated curved UI shaders in $OutputDirectory"
