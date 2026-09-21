param([Parameter(Mandatory=$true)][string]$GameDirectory,
      [Parameter(Mandatory=$true)][string]$PackageDirectory,
      [switch]$Undo)
$ErrorActionPreference='Stop'
if(Get-Process koa -ErrorAction SilentlyContinue){throw 'Close the game before changing startup files.'}
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$manifest=Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
$original='16a400f6e8fc10dbe446a9e57717de9fbe975ef17c004f4ca405e2e4e09cb314'
if($manifest.original -ne $original){throw 'Unsupported original executable.'}
$target=Join-Path $game 'koa.exe'
$backup=$target+'.amalur-native-startup-original'
$expected=if($Undo){$manifest.patched}else{$original}
if((Get-FileHash -LiteralPath $target).Hash -ne $expected){throw 'Installed executable differs; undo the prior startup candidate first.'}
if(!$Undo){
    if((Get-FileHash -LiteralPath (Join-Path $game 'data/initial_0.pak')).Hash -ne '7776e3847f3acd326962e10ba3c78280265026e28fc19f0b682763f6545015dd'){throw 'Restore the original initial archive first.'}
    if((Get-FileHash -LiteralPath (Join-Path $game 'data/patch_0.pak')).Hash -ne 'ba34a31a0acb861d7fd1fba118d094e00e5d572e0d3f10df142815750bf44e4c'){throw 'Restore the original patch archive first.'}
}
$source=if($Undo){$backup}else{Join-Path $package 'koa.exe'}
$resultHash=if($Undo){$original}else{$manifest.patched}
if((Get-FileHash -LiteralPath $source).Hash -ne $resultHash){throw 'Source hash differs.'}
if(Test-Path -LiteralPath $backup){
    if((Get-FileHash -LiteralPath $backup).Hash -ne $original){throw 'Original backup differs.'}
}elseif(!$Undo){Copy-Item -LiteralPath $target -Destination $backup}
try{
    Copy-Item -LiteralPath $source -Destination $target -Force
    if((Get-FileHash -LiteralPath $target).Hash -ne $resultHash){throw 'Installed hash differs.'}
}catch{
    Copy-Item -LiteralPath $backup -Destination $target -Force
    throw
}
Write-Output 'Executable verified. Native startup candidate still requires a no-input live test.'
