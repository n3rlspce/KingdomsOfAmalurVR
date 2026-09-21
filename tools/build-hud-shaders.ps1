param([string]$OutputDirectory,[string]$DonorDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not $OutputDirectory){$OutputDirectory=Join-Path $root 'build/hud-shaders'}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$donor=$DonorDirectory
if(-not $donor){$donor=Join-Path $root 'research/amalur-stereo-fix/KOARR_Release_Alpha_0.1/ShaderFixes'}
# Preserve mikear69's shader signatures, original transforms and texture inputs.
# Shared UI shaders require a separate draw-based gate; labels are not ownership.
$placement=@'
// Amalur VR curved HUD candidate, built on mikear69's shader identification.
// IniParams[2]: horizontal extent, vertical extent, half arc radians, disparity.
float4 vrHud = IniParams.Load(int2(2, 0));
float4 hudControl = AmalurHudSettings.Load(int2(0, 0));
float hudSize = hudControl.y > 0.5 ? clamp(hudControl.x, 0.4, 1.2) : 0.8;
vrHud.xy *= hudSize;
// Gameplay uses the minimap-triggered preset. Conversations hide the minimap,
// so the native validated dialogue state supplies a separate gate in W.
// Both use the same HUD size/placement; explicit flat Interface View wins.
float4 vrHudGate = IniParams.Load(int2(3, 0));
bool dialogueHud = hudControl.y > 0.5 && hudControl.w > 0.5 && hudControl.w < 1.5;
bool menuHud = hudControl.y > 0.5 && hudControl.w > 1.5;
// Compact the whole conversation canvas together so text, buttons and choice
// highlights remain aligned. Gameplay HUD placement is unchanged.
if (dialogueHud) vrHud.xy *= 0.85;
if (hudControl.z < 0.5 && (vrHudGate.x > 0.5 || dialogueHud || menuHud) && vrHud.x > 0.0 && abs(o0.w) > 0.00001) {
    float2 canvas = o0.xy / o0.w;
    float arc = clamp(vrHud.z, 0.01, 0.9);
    float angle = clamp(canvas.x, -1.0, 1.0) * arc;
    float curveZ = cos(angle);
    float oldW = o0.w;
    // Approximate a cylindrical canvas on the existing UI quads. Preserve W:
    // geo-11 treats W != 1 as world geometry and would add a second depth shift.
    o0.x = tan(angle) / tan(arc) * vrHud.x * oldW;
    o0.y = canvas.y * vrHud.y * cos(arc) * oldW / curveZ;
    if (dialogueHud) o0.y -= 0.18 * oldW;
    // Empirical stereo offset, not calibrated metres. Keep independently tunable.
    o0.x += stereo.x * vrHud.w * oldW / curveZ;
    if (menuHud) {
        // Menus are one flat plane, not a vertex-wise curved HUD. Curving a
        // large quad and small text quads differently breaks their alignment.
        o0.xy = canvas * vrHud.xy * oldW;
        float3 canvasPoint = float3(o0.xy / oldW, 1.0);
        float3 anchored = float3(dot(AmalurHudSettings.Load(int2(1,0)).xyz, canvasPoint),
                                 dot(AmalurHudSettings.Load(int2(2,0)).xyz, canvasPoint),
                                 dot(AmalurHudSettings.Load(int2(3,0)).xyz, canvasPoint));
        // Preserve homogeneous W: the rasterizer needs it for perspective-
        // correct texture interpolation and clipping at/behind the camera.
        o0.xy = anchored.xy * oldW;
        o0.z *= anchored.z;
        o0.w = anchored.z * oldW;
        // Verified geo-11 converted VS epilogue adds this shift for W != 1.
        // Cancel it here: this is a UI plane, not native world geometry.
        // t125's legacy StereoParams.w is rewritten by geo-11 to buffer[1].z,
        // not the eye sign. t118 aliases the raw buffer without that rewrite.
        float4 rawStereo = AmalurRawStereo.Load(0);
        if (o0.w != 1.0) o0.x -= (o0.w - rawStereo.y) * rawStereo.x * rawStereo.w;
    }
} else if (hudControl.z < 0.5) {
    o0.x += stereo.x * hud;
}
'@
foreach($hash in @('887f6506d28f9ff1','bd9cebc4f7e1ed36','cc7258d9790a0bbd','bf098be2e4587ca5')){
    $name="$hash-vs_replace.txt"
    $text=Get-Content -LiteralPath (Join-Path $donor $name) -Raw
    $text='Buffer<float4> AmalurRawStereo : register(t118);'+[Environment]::NewLine+$text
    $text=$text.Replace('Texture1D<float4> IniParams', 'Texture1D<float4> AmalurHudSettings : register(t119);'+[Environment]::NewLine+'Texture1D<float4> IniParams')
    $needle='o0.x += stereo.x*hud;'
    if(-not $text.Contains($needle)){throw "Expected donor HUD adjustment missing: $name"}
    $text=$text.Replace($needle,$placement)
    if($hash -eq 'bf098be2e4587ca5'){
        # Only the untextured solid-colour UI shader; preserve fonts, icons,
        # coloured selection highlights, and every non-dialogue screen.
        $signature='out float4 o1 : COLOR0)'
        if(-not $text.Contains($signature)){throw "Expected solid UI signature missing: $name"}
        $text=$text.Replace($signature,'out float4 o1 : COLOR0, out float dialogueClip : SV_ClipDistance0)')
        $hideBars=@'
o1.xyzw = v1.xyzw;
dialogueClip = 1.0;
if (dialogueHud && hudControl.z < 0.5 && max(max(abs(v1.r), abs(v1.g)), abs(v1.b)) < 0.001) {
    // Clip instead of relocating vertices: gradients can mix black and coloured
    // vertices in one primitive. Relocation stretches those triangles onscreen.
    // Hardware clipping works even when this draw disables alpha blending.
    dialogueClip = -1.0;
}
'@
        $text=$text.Replace('o1.xyzw = v1.xyzw;',$hideBars)
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory $name),$text,[Text.Encoding]::ASCII)
}
Write-Host "Generated 4 gated curved UI shaders in $OutputDirectory"
