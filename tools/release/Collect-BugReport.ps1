param([string]$GameDirectory,[string]$SaveDirectory,[string]$OutputDirectory,[switch]$IncludeSaves,[switch]$LogsOnly,[switch]$OpenForm,[switch]$NoOpenFolder)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if($IncludeSaves -and $LogsOnly){throw 'Choose IncludeSaves or LogsOnly.'}
$game=Find-Game $GameDirectory
if(!$LogsOnly -and !$IncludeSaves){
    Write-Host 'This report copies diagnostic logs, VR settings and up to 3 most recent saves.'
    Write-Host 'Saves contain player progress and may contain player identifiers. Originals are never changed.'
    Write-Host 'Nothing is uploaded. Review the ZIP before sharing it; public issue attachments are public.'
    $answer=Read-Host 'Include the latest 3 saves? [Y/n]'
    $IncludeSaves=$answer -in @('','y','Y','yes','Yes')
}
if(!$OutputDirectory){$OutputDirectory=Join-Path $PSScriptRoot 'BugReports'}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$id='AmalurVR-BugReport-'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6)
$stage=Join-Path $OutputDirectory $id
New-Item -ItemType Directory -Path $stage | Out-Null
function Redact([string]$text){
    foreach($path in @($game,$env:USERPROFILE,$env:TEMP)){
        if($path){foreach($variant in @($path,$path.Replace('\','/'),$path.Replace('\','\\'))){
            $text=[regex]::Replace($text,[regex]::Escape($variant),'<LOCAL_PATH>','IgnoreCase')
        }}
    }
    $text=[regex]::Replace($text,'(?i)[a-z]:[\\/]Users[\\/][^\\/\s"'']+','<USER>')
    $text=[regex]::Replace($text,'(?i)(userdata[\\/]+)\d+','$1<ACCOUNT>')
    return $text
}
function Copy-TextTail($source,$name){
    if(!(Test-Path -LiteralPath $source -PathType Leaf)){return}
    $stream=[IO.File]::Open($source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    try {
        $null=$stream.Seek([Math]::Max(0,$stream.Length-4MB),[IO.SeekOrigin]::Begin)
        $reader=New-Object IO.StreamReader($stream)
        try { $content=$reader.ReadToEnd() } finally {$reader.Dispose()}
    } finally {$stream.Dispose()}
    Set-Content -LiteralPath (Join-Path $stage $name) -Value (Redact $content) -Encoding UTF8
}
foreach($name in @('amalur-diagnostic.log','d3d11_log.txt','re_mod.log')){Copy-TextTail (Join-Path $game $name) $name}
$bridge=Join-Path $game 'AmalurVR'
foreach($entry in @(Get-ChildItem -LiteralPath (Join-Path $bridge 'logs') -File -Filter '*.log' -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 8)){
    Copy-TextTail $entry.FullName ('bridge-'+$entry.Name)
}
Copy-TextTail (Join-Path $bridge 'amalur-vr.ini') 'vr-settings.ini'
$receipt=Join-Path $game '.amalur-vr-installer/receipt.json'
$version='unknown';if(Test-Path -LiteralPath $receipt){$version=(Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).version}
$info=[ordered]@{release=$version;windows=[Environment]::OSVersion.VersionString;is64BitOS=[Environment]::Is64BitOperatingSystem;savesIncluded=0;files=@()}
foreach($relative in @('koa.exe','amalur_camera.dll','AmalurVR/amalur-xr-smoke.exe')){
    $file=Join-Path $game $relative
    if(Test-Path -LiteralPath $file){$info.files+=@{name=$relative;sha256=(Hash $file)}}
}
if($IncludeSaves){
    $roots=@()
    if($SaveDirectory){$roots+= (Resolve-Path -LiteralPath $SaveDirectory).Path} else {
        $roots+=Join-Path $env:USERPROFILE 'AppData/LocalLow/THQNOnline/Kingdoms of Amalur Re-Reckoning/autocloud/save'
        $steam=(Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
        if($steam){foreach($account in @(Get-ChildItem -LiteralPath (Join-Path $steam 'userdata') -Directory -ErrorAction SilentlyContinue)){
            $roots+=Join-Path $account.FullName '1041720/remote/autocloud/save'
            $roots+=Join-Path $account.FullName '1041720/remote'
        }}
    }
    $candidates=@(foreach($root in $roots | Select-Object -Unique){
        if(Test-Path -LiteralPath $root){Get-ChildItem -LiteralPath $root -File -Filter '*.sav' | Where-Object {
            $_.Name -ine 'svd_fmt_0_0.sav' -and !($_.Attributes -band [IO.FileAttributes]::ReparsePoint)
        }}
    })
    if(!$candidates.Count){Write-Host 'No eligible saves found. Run again with -SaveDirectory pointing to your save folder to include saves.'}
    $seen=@{}
    foreach($save in $candidates | Sort-Object LastWriteTimeUtc -Descending){
        if($info.savesIncluded -ge 3){break}
        # Read each selected save once; hash and archive those same bytes.
        $bytes=[IO.File]::ReadAllBytes($save.FullName);$hash=Hash-Bytes $bytes
        if($seen.ContainsKey($hash)){continue};$seen[$hash]=$true
        $info.savesIncluded++
        $name='save-'+$info.savesIncluded+'.sav'
        [IO.File]::WriteAllBytes((Join-Path $stage $name),$bytes)
        $info.files+=@{name=$name;sha256=$hash;modifiedUtc=$save.LastWriteTimeUtc.ToString('o')}
    }
}
$info | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $stage 'report.json') -Encoding UTF8
# Keep a consolidated text fallback below the form's 10 MB upload limit.
$combined="Amalur VR diagnostic report`r`n"+($info | ConvertTo-Json -Depth 6)+"`r`n"
foreach($file in Get-ChildItem -LiteralPath $stage -File | Where-Object Extension -In @('.log','.txt','.ini')){
    $combined+="`r`n--- $($file.Name) ---`r`n"+(Get-Content -LiteralPath $file.FullName -Raw)
}
if($combined.Length -gt 2000000){$combined=$combined.Substring(0,2000000)+"`r`n[Report truncated to fit upload limit]"}
Set-Content -LiteralPath (Join-Path $stage 'diagnostic-report.txt') -Value $combined -Encoding UTF8
@'
Describe what happened, what you expected, and steps to reproduce it.
Include your headset, connection method and graphics card.
This ZIP was collected locally. Nothing was uploaded automatically.
Logs contain bounded tails and common personal paths are redacted; review before sharing.
Included saves are unmodified binary copies and can contain player information.
Do not attach saves publicly unless you are comfortable sharing their contents.
No-login report form: https://tally.so/r/BzNXPK
Attach the ZIP in the bug-report ZIP field. Logs and any included saves are already inside.
The form has a 10 MB per-file limit. For a larger ZIP, attach diagnostic-report.txt and up to three saves separately if each fits.
Tally stores submitted reports and attachments for private review by the form owner.
'@ | Set-Content -LiteralPath (Join-Path $stage 'READ-ME.txt') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=Join-Path $OutputDirectory ($id+'.zip')
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip)
Write-Host "Report ready: $zip"
Write-Host "Saves included: $($info.savesIncluded). Review before sharing. Nothing was uploaded."
if((Get-Item -LiteralPath $zip).Length -ge 10000000){
    Write-Warning 'ZIP exceeds the form limit. Use diagnostic-report.txt and separate saves from the matching report folder, if each is below 10 MB.'
}
if(!$NoOpenFolder){Start-Process explorer.exe -ArgumentList ('/select,"'+[IO.Path]::GetFullPath($zip)+'"')}
if($OpenForm){
    Write-Host 'Your browser opens a Tally form to upload bug information.'
    Write-Host 'Describe the bug, attach the ZIP selected in Explorer, then submit. No login needed.'
    Write-Host 'Review the report before sharing. Nothing is uploaded until you submit the form.'
    Start-Process 'https://tally.so/r/BzNXPK'
} else {
    Write-Host 'Run Report a Bug.cmd to open a Tally form in your browser and upload bug information.'
}
