param([string]$GameDirectory, [switch]$CheckOnly, [switch]$BridgeOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$game=Find-Game $GameDirectory
$bridgeDirectory=Join-Path $game 'AmalurVR'
$bridge=Join-Path $bridgeDirectory 'amalur-xr-smoke.exe'
foreach ($file in @($bridge,(Join-Path $game 'amalur_camera.dll'),(Join-Path $bridgeDirectory 'amalur-menu-send.exe'),(Join-Path $game 're_mod.dll'))) {
    if (!(Test-Path -LiteralPath $file)) { throw 'Installation is incomplete. Run Install first.' }
}
$programFiles=if($env:ProgramW6432){$env:ProgramW6432}else{$env:ProgramFiles}
$runtime=Join-Path $programFiles 'Virtual Desktop Streamer/OpenXR/virtualdesktop-openxr-32.json'
if (!(Test-Path -LiteralPath $runtime)) { throw 'Install Virtual Desktop Streamer on this PC, connect the headset, then retry. https://www.vrdesktop.net/' }
if (!(Get-Process VirtualDesktop.Streamer -ErrorAction SilentlyContinue)) { throw 'Start Virtual Desktop Streamer and connect your headset before launching VR.' }
$runtimeFolder=if([Environment]::Is64BitOperatingSystem){'SysWOW64'}else{'System32'}
foreach($name in @('vcruntime140.dll','msvcp140.dll')) {
    if(!(Test-Path -LiteralPath (Join-Path $env:WINDIR "$runtimeFolder/$name"))) { throw 'Install Microsoft Visual C++ 2015-2022 Redistributable (x86): https://aka.ms/vs/17/release/vc_redist.x86.exe' }
}
if($CheckOnly){Write-Host 'Launch prerequisites found. Headset rendering still requires a live test.';return}
$launchMutex=New-Object Threading.Mutex($false,'Local\AmalurVRBridgeLaunch')
$locked=$false
try {
    try { $locked=$launchMutex.WaitOne(10000) } catch [Threading.AbandonedMutexException] { $locked=$true }
    if(!$locked){throw 'Another VR launch is still in progress.'}
    if(Get-Process amalur-xr-smoke -ErrorAction SilentlyContinue){
        if($BridgeOnly){return}
        throw 'VR bridge already running. Close it before a fresh launch.'
    }
    if(!$BridgeOnly -and (Get-Process koa -ErrorAction SilentlyContinue)){throw 'Game already running. Close it before a fresh launch.'}
$env:XR_RUNTIME_JSON=$runtime
$logs=Join-Path $bridgeDirectory 'logs';New-Item -ItemType Directory -Path $logs -Force | Out-Null
$prefix=Join-Path $logs (Get-Date -Format yyyyMMdd-HHmmss)
$process=Start-Process -FilePath $bridge -ArgumentList '--game' -WorkingDirectory $bridgeDirectory -WindowStyle Hidden -RedirectStandardOutput ($prefix+'.out.log') -RedirectStandardError ($prefix+'.err.log') -PassThru
Start-Sleep -Seconds 2
if($process.HasExited){throw "VR bridge exited. See $prefix.err.log"}
} finally { if($locked){$launchMutex.ReleaseMutex()};$launchMutex.Dispose() }
if($BridgeOnly){return}
Start-Process 'steam://rungameid/1041720'
$deadline=(Get-Date).AddSeconds(60)
do { Start-Sleep -Seconds 2; $gameProcess=Get-Process koa -ErrorAction SilentlyContinue } until($gameProcess -or (Get-Date) -gt $deadline)
if(!$gameProcess){if(!$process.HasExited){Stop-Process -Id $process.Id};throw 'Steam did not start the game within 60 seconds. Open Steam and try again.'}
Write-Host 'Game and VR bridge started. Put on the headset; keep the game focused during startup.'
