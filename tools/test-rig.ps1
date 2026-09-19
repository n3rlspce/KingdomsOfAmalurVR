param([switch]$LoadSave,[switch]$Observe,[switch]$MouseAttack,[switch]$StaticPose,[string]$Output='')
# Bounded local gameplay test. Reports rig feedback instead of screenshot polling.
# LoadSave sends at most two Enter presses (title screen, Continue). It stops if
# gameplay isn't detected; it cannot recognize arbitrary menus or dialogs.
$ErrorActionPreference='Stop'
$rigRoot=Split-Path -Parent $PSScriptRoot
$rigDriver=Join-Path $rigRoot 'build/diagnostic-x86/RelWithDebInfo/rig_driver.exe'
if(!$Output){$Output=Join-Path $rigRoot 'captures/rig-replay.jsonl'}
if(!(Test-Path $rigDriver)){throw 'Build diagnostics first'}
if(!$Observe -and (Get-Process amalur-xr-smoke -ErrorAction SilentlyContinue)){throw 'Stop the XR bridge before synthetic replay; use -Observe for headset measurements'}
Add-Type @'
using System;using System.Runtime.InteropServices;
public class RigTestWindow {
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")]public static extern bool ShowWindow(IntPtr h,int n);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll")]public static extern bool AttachThreadInput(uint a,uint b,bool attach);
 [DllImport("kernel32.dll")]public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")]public static extern void keybd_event(byte k,byte s,uint f,UIntPtr e);
}
'@
if(!(Get-Process koa -ErrorAction SilentlyContinue)){
 if(!$LoadSave){throw 'Game is not running'}
 Start-Process 'steam://rungameid/1041720'
}
$rigDeadline=[DateTime]::UtcNow.AddSeconds(60)
$rigState=$null
while([DateTime]::UtcNow -lt $rigDeadline){
 $rigRaw=& $rigDriver --status 2>$null
 if($LASTEXITCODE -eq 0){$rigState=$rigRaw|ConvertFrom-Json;if($rigState.frames -gt 10){break}}
 Start-Sleep -Milliseconds 250
}
if(!$rigState){throw 'Timed out waiting for live mod telemetry'}
$rigGame=Get-Process -Id $rigState.pid
$rigWindow=$rigGame.MainWindowHandle
[RigTestWindow]::ShowWindow($rigWindow,9)|Out-Null
[RigTestWindow]::SetForegroundWindow($rigWindow)|Out-Null
$rigOtherPid=[uint32]0
$rigOtherThread=[RigTestWindow]::GetWindowThreadProcessId([RigTestWindow]::GetForegroundWindow(),[ref]$rigOtherPid)
$rigThread=[RigTestWindow]::GetCurrentThreadId()
$rigAttached=[RigTestWindow]::AttachThreadInput($rigThread,$rigOtherThread,$true)
try{[RigTestWindow]::SetForegroundWindow($rigWindow)|Out-Null}finally{if($rigAttached){[RigTestWindow]::AttachThreadInput($rigThread,$rigOtherThread,$false)|Out-Null}}
Start-Sleep -Milliseconds 250
if([RigTestWindow]::GetForegroundWindow() -ne $rigWindow){
 # Release Alt before activating; Windows otherwise rejects background activation.
 try{[RigTestWindow]::keybd_event(18,0,0,[UIntPtr]::Zero)}finally{[RigTestWindow]::keybd_event(18,0,2,[UIntPtr]::Zero)}
 [RigTestWindow]::SetForegroundWindow($rigWindow)|Out-Null
 Start-Sleep -Milliseconds 250
}
if([RigTestWindow]::GetForegroundWindow() -ne $rigWindow){throw 'Game focus unavailable'}
$rigRaw=& $rigDriver --status
if($LASTEXITCODE -ne 0){throw 'Telemetry unavailable after focus'}
$rigState=$rigRaw|ConvertFrom-Json
if($rigState.paused -eq 1 -and !$Observe){
 try{[RigTestWindow]::keybd_event(27,0,0,[UIntPtr]::Zero);Start-Sleep -Milliseconds 60}
 finally{[RigTestWindow]::keybd_event(27,0,2,[UIntPtr]::Zero)}
 Start-Sleep -Milliseconds 100
}
if($LoadSave -and !$rigState.weapons){
 for($rigStep=0;$rigStep -lt 2;$rigStep++){
  if([RigTestWindow]::GetForegroundWindow() -ne $rigWindow){throw 'Focus lost'}
  try{[RigTestWindow]::keybd_event(13,0,0,[UIntPtr]::Zero);Start-Sleep -Milliseconds 200}
  finally{[RigTestWindow]::keybd_event(13,0,2,[UIntPtr]::Zero)}
  $rigUntil=[DateTime]::UtcNow.AddMilliseconds($(if($rigStep -eq 0){600}else{10000}))
  do{Start-Sleep -Milliseconds 250;$rigRaw=& $rigDriver --status 2>$null;if($LASTEXITCODE -eq 0){$rigState=$rigRaw|ConvertFrom-Json}}while(!$rigState.weapons -and [DateTime]::UtcNow -lt $rigUntil)
  if($rigState.weapons){break}
 }
}
if(!$rigState.weapons){throw 'No equipped-weapon rig detected. Load a gameplay save, then rerun without -LoadSave.'}
if($StaticPose){
 if(Get-Process rig_driver -ErrorAction SilentlyContinue){throw 'A rig driver is already running'}
 Start-Process $rigDriver -ArgumentList '--static' -WindowStyle Hidden -RedirectStandardOutput (Join-Path $rigRoot 'build/static-pose.jsonl') -RedirectStandardError (Join-Path $rigRoot 'build/static-pose-error.log')
 Write-Output 'Static third-person pose running for ten minutes; stop rig_driver to restore normal tracking.'
 return
}
$rigOutputParent=Split-Path -Parent $Output
if($rigOutputParent){New-Item -ItemType Directory -Force -Path $rigOutputParent|Out-Null}
if($Observe){& $rigDriver --observe | Set-Content -LiteralPath $Output}elseif($MouseAttack){& $rigDriver --mouse-attack | Set-Content -LiteralPath $Output}else{& $rigDriver | Set-Content -LiteralPath $Output}
if($LASTEXITCODE -ne 0){throw "Rig test stopped with code $LASTEXITCODE; partial capture: $Output"}
$rigSamples=Get-Content -LiteralPath $Output|ForEach-Object{$_|ConvertFrom-Json}|Where-Object{$null -ne $_.ms}
$rigSamples|Group-Object phase,bone|Select-Object Name,Count
Write-Output "Saved $($rigSamples.Count) samples: $Output"
