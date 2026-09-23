param([string]$GameDirectory, [switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$game=Find-Game $GameDirectory
$bridgeDirectory=Join-Path $game 'AmalurVR'
$bridge=Join-Path $bridgeDirectory 'amalur-xr-smoke.exe'
foreach ($file in @($bridge,(Join-Path $game 'amalur_camera.dll'),(Join-Path $bridgeDirectory 'amalur-menu-send.exe'),(Join-Path $game 're_mod.dll'))) {
    if (!(Test-Path -LiteralPath $file)) { throw 'Installation is incomplete. Run Install first.' }
}
$runtime=Join-Path $env:ProgramFiles 'Virtual Desktop Streamer/OpenXR/virtualdesktop-openxr-32.json'
if (!(Test-Path -LiteralPath $runtime)) { throw 'Install Virtual Desktop Streamer on this PC, connect the headset, then retry. https://www.vrdesktop.net/' }
if (!(Get-Process VirtualDesktop.Streamer -ErrorAction SilentlyContinue)) { throw 'Start Virtual Desktop Streamer and connect your headset before launching VR.' }
$runtimeFolder=if([Environment]::Is64BitOperatingSystem){'SysWOW64'}else{'System32'}
foreach($name in @('vcruntime140.dll','msvcp140.dll')) {
    if(!(Test-Path -LiteralPath (Join-Path $env:WINDIR "$runtimeFolder/$name"))) { throw 'Install Microsoft Visual C++ 2015-2022 Redistributable (x86): https://aka.ms/vs/17/release/vc_redist.x86.exe' }
}
if($CheckOnly){Write-Host 'Launch prerequisites found. Headset rendering still requires a live test.';return}
if(Get-Process koa,amalur-xr-smoke -ErrorAction SilentlyContinue){throw 'A game or bridge session is already running. Close it before a fresh VR launch.'}
$env:XR_RUNTIME_JSON=$runtime
$logs=Join-Path $bridgeDirectory 'logs';New-Item -ItemType Directory -Path $logs -Force | Out-Null
$prefix=Join-Path $logs (Get-Date -Format yyyyMMdd-HHmmss)
$process=Start-Process -FilePath $bridge -ArgumentList '--game' -WorkingDirectory $bridgeDirectory -WindowStyle Hidden -RedirectStandardOutput ($prefix+'.out.log') -RedirectStandardError ($prefix+'.err.log') -PassThru
Start-Sleep -Seconds 2
if($process.HasExited){throw "VR bridge exited. See $prefix.err.log"}
Start-Process 'steam://rungameid/1041720'
$deadline=(Get-Date).AddSeconds(60)
do { Start-Sleep -Seconds 2; $gameProcess=Get-Process koa -ErrorAction SilentlyContinue } until($gameProcess -or (Get-Date) -gt $deadline)
if(!$gameProcess){if(!$process.HasExited){Stop-Process -Id $process.Id};throw 'Steam did not start the game within 60 seconds. Open Steam and try again.'}
Write-Host 'Game and VR bridge started. Put on the headset; keep the game focused during startup.'
