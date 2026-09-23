param([string]$GameDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$game=Find-Game $GameDirectory
Assert-Closed
$state=Join-Path $game '.amalur-vr-installer'
$path=Join-Path $state 'receipt.json'
if(!(Test-Path -LiteralPath $path)){throw 'No installation receipt; no files changed.'}
$receipt=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
if($receipt.game -ne $game){throw 'Receipt directory mismatch.'}
$remaining=@()
foreach($entry in $receipt.files){
    $target=Safe-Path $game $entry.target
    if((Test-Path -LiteralPath $target) -and (Hash $target) -ne $entry.installedHash){
        Write-Host "Preserved changed file: $($entry.target)";$remaining+=$entry;continue
    }
    if($entry.existed){
        $backup=Safe-Path $state $entry.backup
        if((Hash $backup) -ne $entry.originalHash){throw "Backup checksum failed: $($entry.target)"}
        Copy-Item -LiteralPath $backup -Destination $target -Force
    }elseif(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target}
}
if($remaining.Count){$receipt.files=$remaining;$receipt | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $path -Encoding UTF8}
else{Move-Item -LiteralPath $path -Destination (Join-Path $state ('uninstalled-'+(Get-Date -Format yyyyMMdd-HHmmss)+'.json'))}
Write-Host 'Original files restored where unchanged. Saves, modified settings and backup history retained.'
