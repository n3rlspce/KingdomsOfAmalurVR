param([Parameter(Mandatory=$true)][string]$GameDirectory,
      [Parameter(Mandatory=$true)][string]$PackageDirectory,
      [switch]$Undo)
$ErrorActionPreference='Stop'
if(Get-Process koa -ErrorAction SilentlyContinue){throw 'Close the game before changing startup files.'}
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$manifest=Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
$archiveName=if($manifest.archiveName){$manifest.archiveName}else{'patch_0.pak'}
if($archiveName -notin @('patch_0.pak','initial_0.pak')){throw 'Unsupported startup archive name.'}
$targets=@(
    @{name='koa.exe'; target=(Join-Path $game 'koa.exe'); original=$manifest.exeOriginal; patched=$manifest.exePatched},
    @{name=$archiveName; target=(Join-Path $game ('data/'+$archiveName)); original=$manifest.archiveOriginal; patched=$manifest.archivePatched}
)
foreach($item in $targets){
    $expected=if($Undo){$item.patched}else{$item.original}
    if((Get-FileHash -LiteralPath $item.target).Hash -ne $expected){throw "Installed file revision differs: $($item.name)"}
    $source=if($Undo){$item.target+'.amalur-early-startup-original'}else{Join-Path $package $item.name}
    $expectedSource=if($Undo){$item.original}else{$item.patched}
    if((Get-FileHash -LiteralPath $source).Hash -ne $expectedSource){throw "Source revision differs: $source"}
    if(!$Undo -and (Test-Path -LiteralPath ($item.target+'.amalur-early-startup-original')) -and
       (Get-FileHash -LiteralPath ($item.target+'.amalur-early-startup-original')).Hash -ne $item.original){throw 'Existing original backup differs.'}
}
if(!$Undo){foreach($item in $targets){if(!(Test-Path -LiteralPath ($item.target+'.amalur-early-startup-original'))){Copy-Item -LiteralPath $item.target -Destination ($item.target+'.amalur-early-startup-original')}}}
try {
    foreach($item in $targets){
        $source=if($Undo){$item.target+'.amalur-early-startup-original'}else{Join-Path $package $item.name}
        Copy-Item -LiteralPath $source -Destination $item.target -Force
        $expected=if($Undo){$item.original}else{$item.patched}
        if((Get-FileHash -LiteralPath $item.target).Hash -ne $expected){throw 'Installed hash verification failed'}
    }
}catch{
    if(!$Undo){foreach($item in $targets){Copy-Item -LiteralPath ($item.target+'.amalur-early-startup-original') -Destination $item.target -Force}}
    throw
}
Write-Output 'Verified startup file installation. Original backups retained; live test required.'
