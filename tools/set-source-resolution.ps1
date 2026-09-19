param([ValidateSet('1440p','4k','5k')][string]$Preset='4k',[switch]$Windowed,[switch]$Borderless)
$ErrorActionPreference='Stop'
if($Windowed -and $Borderless){throw 'Choose one display mode.'}
if(Get-Process koa -ErrorAction SilentlyContinue){throw 'Exit Amalur before changing its saved source resolution.'}
$source=Join-Path $env:APPDATA 'kaiko\koa\personal.ini'
$root=Split-Path -Parent $PSScriptRoot
$backup=Join-Path $root 'build\personal-before-vr-resolution.ini'
if(-not(Test-Path -LiteralPath $backup)){Copy-Item -LiteralPath $source -Destination $backup}
$text=Get-Content -LiteralPath $source -Raw
if($text -notmatch '(?m)^Display Width=\d+' -or $text -notmatch '(?m)^Display Height=\d+'){throw 'Expected display settings missing.'}
$width=switch($Preset){'5k'{5120}'4k'{3840}default{2560}}
$height=switch($Preset){'5k'{2880}'4k'{2160}default{1440}}
$text=$text -replace '(?m)^Display Width=\d+',"Display Width=$width"
$text=$text -replace '(?m)^Display Height=\d+',"Display Height=$height"
if($Windowed){$text=$text -replace '(?m)^Display Fullscreen=\d+','Display Fullscreen=0'}
if($Borderless){$text=$text -replace '(?m)^Display Fullscreen=\d+','Display Fullscreen=2'}
[IO.File]::WriteAllText($source,$text,[Text.Encoding]::ASCII)
Write-Host "Source resolution requested: $width x $height. Backup: $backup"
