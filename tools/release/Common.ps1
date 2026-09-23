function Hash($path) { Hash-Bytes ([IO.File]::ReadAllBytes($path)) }
function Hash-Bytes([byte[]]$bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
}
function Safe-Path($root,$relative) {
    $base = [IO.Path]::GetFullPath($root).TrimEnd('\')
    if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains(':')) { throw 'Absolute or alternate-stream manifest path rejected.' }
    $path = [IO.Path]::GetFullPath((Join-Path $base $relative))
    if (!$path.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Manifest path escapes destination.' }
    $parent = $path
    while ($parent -and $parent.Length -ge $base.Length) {
        if (Test-Path -LiteralPath $parent) {
            if ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked destination rejected: $parent" }
        }
        $parent = Split-Path $parent
    }
    return $path
}
function Assert-Closed {
    if (Get-Process koa,amalur-xr-smoke -ErrorAction SilentlyContinue) { throw 'Close the game and VR bridge before installing or restoring files.' }
}
function Find-Game($explicit, [switch]$Choose) {
    $browse = $false
    if (!$explicit -and $Choose) {
        Write-Host 'Choose the game location:'
        Write-Host '  1. Detect Steam installation'
        Write-Host '  2. Pick game folder'
        do { $choice = Read-Host 'Enter 1 or 2 (Enter = Steam)' } while ($choice -notin @('', '1', '2'))
        $browse = $choice -eq '2'
    }
    $candidates = @()
    if ($explicit) { $candidates = @($explicit) } elseif (!$browse) {
        $candidates = @()
        $saved = Join-Path $PSScriptRoot 'installed-game.txt'
        if (!$Choose -and (Test-Path -LiteralPath $saved)) { $candidates += (Get-Content -LiteralPath $saved -Raw).Trim() }
        $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
        if ($steam -and [IO.Directory]::Exists($steam)) {
            $libraries = @($steam)
            $vdf = Join-Path $steam 'steamapps/libraryfolders.vdf'
            if (Test-Path -LiteralPath $vdf) {
                foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw),'"path"\s+"([^"]+)"')) { $libraries += $m.Groups[1].Value.Replace('\\','\') }
            }
            foreach ($library in $libraries) {
                if ([IO.Directory]::Exists($library)) {
                    $candidates += [IO.Path]::Combine($library,'steamapps/common/Kingdoms of Amalur Re-Reckoning')
                }
            }
        }
    }
    if (!$browse) {
        foreach ($candidate in $candidates) {
            if ($candidate -and [IO.Directory]::Exists($candidate) -and [IO.File]::Exists([IO.Path]::Combine($candidate,'koa.exe'))) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }
    if ($explicit) { throw 'koa.exe was not found in the selected directory.' }
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Select the Kingdoms of Amalur Re-Reckoning folder containing koa.exe'
    $dialog.ShowNewFolderButton = $false
    try {
        if ($dialog.ShowDialog() -ne 'OK') { throw 'Folder selection cancelled.' }
        if (!(Test-Path -LiteralPath (Join-Path $dialog.SelectedPath 'koa.exe'))) { throw 'koa.exe was not found in the selected folder. Run the installer again and select the game folder.' }
        return (Resolve-Path -LiteralPath $dialog.SelectedPath).Path
    } finally { $dialog.Dispose() }
}
