param([Parameter(Mandatory=$true)][string]$Device, [string]$Adb='adb')
$ErrorActionPreference='Stop'
if (-not (Get-Command $Adb -ErrorAction SilentlyContinue)) { throw 'ADB not found. Add it to PATH or pass -Adb with its executable path.' }
$folder=Join-Path (Split-Path -Parent $PSScriptRoot) 'captures'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$output=Join-Path $folder ('quest-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.png')
& $adb -s $Device shell screencap -p /sdcard/amalur-vr-check.png
if($LASTEXITCODE -ne 0){throw 'Quest screenshot failed.'}
& $adb -s $Device pull /sdcard/amalur-vr-check.png $output
if($LASTEXITCODE -ne 0){throw 'Quest screenshot download failed.'}
Write-Output $output
