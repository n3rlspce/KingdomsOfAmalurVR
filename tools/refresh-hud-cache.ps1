param([string]$GameDirectory=$env:AMALUR_GAME_DIR)
$ErrorActionPreference='Stop'
if (-not $GameDirectory) { throw 'Pass -GameDirectory or set AMALUR_GAME_DIR to your game installation.' }
$root=Split-Path -Parent $PSScriptRoot
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$backup=Join-Path $root ('build/hud-cache-backup-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$hashes=@('887f6506d28f9ff1','bd9cebc4f7e1ed36','cc7258d9790a0bbd','bf098be2e4587ca5')
$count=0
foreach($folder in @('ShaderFixes','ShaderFixesDM','ShaderCacheDM')){
    $directory=Join-Path $gameRoot $folder
    if(-not(Test-Path -LiteralPath $directory)){continue}
    foreach($file in Get-ChildItem -LiteralPath $directory -File){
        $match=$false
        foreach($hash in $hashes){
            if(($folder -eq 'ShaderFixes' -and $file.Name -eq "$hash-vs_replace.bin") -or
               ($folder -ne 'ShaderFixes' -and $file.Name -match "^$hash-vs\.(txt|bin)$")){$match=$true}
        }
        if(-not $match){continue}
        $resolved=[IO.Path]::GetFullPath($file.FullName)
        if(-not $resolved.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Cache path outside game directory'}
        $destination=Join-Path $backup $folder
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        Move-Item -LiteralPath $resolved -Destination (Join-Path $destination $file.Name)
        ++$count
    }
}
Write-Host "Backed up $count generated HUD cache files to $backup. Reload shaders or restart Amalur to regenerate."
