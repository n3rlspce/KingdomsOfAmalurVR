param(
    [double]$YawDegrees=0, [double]$PitchDegrees=0, [double]$RollDegrees=0,
    [double]$X=0, [double]$Y=1.7, [double]$Z=0,
    [switch]$Capture
)
$ErrorActionPreference='Stop'
$dataDir=Join-Path $env:LOCALAPPDATA 'OpenXR-Simulator'
$statusPath=Join-Path $dataDir 'runtime_status.json'
if (-not (Test-Path -LiteralPath $statusPath) -or ((Get-Date)-(Get-Item -LiteralPath $statusPath).LastWriteTime).TotalSeconds -gt 10) {
    throw 'Start tools/run-xr-simulator.ps1 -Mode game first.'
}
foreach ($number in @($X,$Y,$Z,$YawDegrees,$PitchDegrees,$RollDegrees)) {
    if ([double]::IsNaN($number) -or [double]::IsInfinity($number)) { throw 'Pose values must be finite.' }
}
$radians=[Math]::PI/180
$command=@{x=$X;y=$Y;z=$Z;yaw=$YawDegrees*$radians;pitch=$PitchDegrees*$radians;roll=$RollDegrees*$radians}
$temp=Join-Path $dataDir 'amalur-head-command.tmp'
$target=Join-Path $dataDir 'head_pose_command.json'
$command | ConvertTo-Json | Set-Content -LiteralPath $temp -Encoding ascii
Move-Item -LiteralPath $temp -Destination $target -Force
$deadline=(Get-Date).AddSeconds(3)
while ((Test-Path -LiteralPath $target) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 50 }
if (Test-Path -LiteralPath $target) { throw 'Simulator did not consume the pose command.' }
Write-Output 'Simulator head pose sent. Keep Amalur unpaused to update the game camera.'
if ($Capture) {
    Start-Sleep -Milliseconds 300
    $captureStatus=Join-Path $dataDir 'screenshot_status.json'
    $oldStamp=if(Test-Path -LiteralPath $captureStatus){(Get-Item -LiteralPath $captureStatus).LastWriteTimeUtc}else{[datetime]::MinValue}
    '{"eye":"both","layer":"projection"}' | Set-Content -LiteralPath (Join-Path $dataDir 'screenshot_request.json') -Encoding ascii
    $deadline=(Get-Date).AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 50
        $ready=(Test-Path -LiteralPath $captureStatus) -and (Get-Item -LiteralPath $captureStatus).LastWriteTimeUtc -gt $oldStamp
    } while (-not $ready -and (Get-Date) -lt $deadline)
    if(-not $ready){throw 'No fresh simulator capture; check that the game is rendering.'}
    $outDir=Join-Path (Split-Path -Parent $PSScriptRoot) 'captures/simulator'
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $out=Join-Path $outDir ('head-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.bmp')
    Copy-Item -LiteralPath (Join-Path $dataDir 'screenshot.bmp') -Destination $out
    Write-Output $out
}
