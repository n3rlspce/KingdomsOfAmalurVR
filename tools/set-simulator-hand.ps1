param(
    [ValidateSet('left','right')][string]$Hand='right',
    [double]$X=0.25,[double]$Y=-0.3,[double]$Z=-0.4,
    [double]$YawDegrees=0,[double]$PitchDegrees=0
)
$ErrorActionPreference='Stop'
$dataDir=Join-Path $env:LOCALAPPDATA 'OpenXR-Simulator'
$statusPath=Join-Path $dataDir 'runtime_status.json'
if(-not(Test-Path -LiteralPath $statusPath)-or ((Get-Date)-(Get-Item -LiteralPath $statusPath).LastWriteTime).TotalSeconds -gt 10){
    throw 'Start tools/run-xr-simulator.ps1 -Mode game first.'
}
foreach($value in @($X,$Y,$Z,$YawDegrees,$PitchDegrees)){
    if([double]::IsNaN($value)-or [double]::IsInfinity($value)){throw 'Pose values must be finite.'}
}
if([Math]::Abs($X)-gt 2-or [Math]::Abs($Y)-gt 2-or [Math]::Abs($Z)-gt 2){throw 'Hand offsets must stay within two metres of the simulated head.'}
# Simulator controller positions are offsets from the simulated head.
$command=@{hand=$(if($Hand -eq 'left'){0}else{1});posX=$X;posY=$Y;posZ=$Z;yaw=$YawDegrees*[Math]::PI/180;pitch=$PitchDegrees*[Math]::PI/180}
$temp=Join-Path $dataDir 'amalur-hand-command.tmp'
$target=Join-Path $dataDir 'controller_pose_command.json'
$command|ConvertTo-Json|Set-Content -LiteralPath $temp -Encoding ascii
Move-Item -LiteralPath $temp -Destination $target -Force
$deadline=(Get-Date).AddSeconds(3)
while((Test-Path -LiteralPath $target)-and (Get-Date)-lt $deadline){Start-Sleep -Milliseconds 50}
if(Test-Path -LiteralPath $target){throw 'Simulator did not consume the controller command.'}
Write-Output "Simulator $Hand hand pose sent."
