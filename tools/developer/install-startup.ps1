param([Parameter(Mandatory=$true)][string]$GameDirectory, [switch]$SkipLogos)
$ErrorActionPreference='Stop'
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $game 'koa.exe'))) { throw 'Not an Amalur install.' }
if($SkipLogos -and (Get-Process koa -ErrorAction SilentlyContinue)){throw 'Close the game before disabling startup videos.'}
# Framework trigger configuration is read at process startup. Installing these
# owned files while playing only schedules the feature for the next launch.
$mods=Join-Path $game 'mods'
New-Item -ItemType Directory -Force -Path $mods | Out-Null
$backup=Join-Path $game ('amalur-startup-backup-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach($name in @('amalur_startup.lua','amalur_startup.json')) {
    $target=Join-Path $mods $name
    if(Test-Path -LiteralPath $target){Copy-Item -LiteralPath $target -Destination $backup}
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $target -Force
    if((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $name)).Hash){throw "Copy verification failed: $name"}
}
if($SkipLogos){
    foreach($name in @('logo_kaiko.bik','logo_thq_nordic.bik')){
        $video=Join-Path $game ('data/videos/'+$name)
        $saved=$video+'.amalur-startup-disabled'
        if(Test-Path -LiteralPath $video){
            if(Test-Path -LiteralPath $saved){throw "Video backup already exists: $saved"}
            Move-Item -LiteralPath $video -Destination $saved
        }elseif(!(Test-Path -LiteralPath $saved)){throw "Startup video or backup missing: $name"}
    }
    Write-Output 'Publisher startup videos disabled; originals retained alongside them.'
}
Write-Output "Startup callbacks installed for the next launch. Backup: $backup. Game archive unchanged."
