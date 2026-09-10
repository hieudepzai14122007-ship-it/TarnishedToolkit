# File operations shared by separation, refresh, and component fixtures. Never launches a process.
function Assert-GameClosed {
    if (Get-Process -Name eldenring,start_protected_game -ErrorAction SilentlyContinue) {
        throw 'Close Elden Ring and its anti-cheat launch window first.'
    }
}
function Assert-ChildPath([string]$Root, [string]$Path) {
    $base = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $full = [IO.Path]::GetFullPath($Path)
    if (!$full.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw "Path escapes its directory: $Path" }
    # Refuse junctions/symlinks in the ancestry; hard-linked asset files are allowed.
    $parent = Split-Path -Parent $full
    while ($parent -and ($parent.Length -ge $base.TrimEnd('\').Length)) {
        if (Test-Path -LiteralPath $parent) {
            if ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked directory is not allowed: $parent" }
        }
        $parent = Split-Path -Parent $parent
    }
    return $full
}
function Get-OfflineInputs([string]$SteamGame) {
    # Only known game code/configuration; never copy an arbitrary DLL from the Steam folder.
    foreach ($name in @('eldenring.exe','amd_ags_x64.dll','bink2w64.dll','eossdk-win64-shipping.dll','oo2core_6_win64.dll','steam_api64.dll','regulation.bin')) {
        $path = Join-Path $SteamGame $name
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing game file: $name" }
        [pscustomobject]@{relative=$name;source=$path;kind='copy'}
    }
    $assets = @(Get-ChildItem -LiteralPath $SteamGame -File | Where-Object { $_.Extension -in '.bdt','.bhd' })
    foreach ($folder in @('movie','movie_dlc','sd')) {
        $path = Join-Path $SteamGame $folder
        if (Test-Path -LiteralPath $path) { $assets += @(Get-ChildItem -LiteralPath $path -File -Recurse) }
    }
    foreach ($asset in $assets) {
        [pscustomobject]@{relative=$asset.FullName.Substring($SteamGame.TrimEnd('\').Length+1);source=$asset.FullName;kind='link'}
    }
}
function Sync-OfflineInputs([string]$SteamGame, [string]$OfflineGame) {
    $steam = [IO.Path]::GetFullPath($SteamGame).TrimEnd('\')
    $offline = [IO.Path]::GetFullPath($OfflineGame).TrimEnd('\')
    if ($steam -eq $offline -or $offline.StartsWith($steam+'\',[StringComparison]::OrdinalIgnoreCase) -or $steam.StartsWith($offline+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Steam and offline directories must be separate siblings.' }
    if ([IO.Path]::GetPathRoot($steam) -ne [IO.Path]::GetPathRoot($offline)) { throw 'Shared game assets require the same drive.' }
    $inputs = @(Get-OfflineInputs $steam)
    foreach ($inputFile in $inputs) {
        $source = Assert-ChildPath $steam $inputFile.source
        $target = Assert-ChildPath $offline (Join-Path $offline $inputFile.relative)
        if ((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked source is not allowed: $source" }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        if (Test-Path -LiteralPath $target) {
            if ((Get-Item -LiteralPath $target -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked target is not allowed: $target" }
            if ($inputFile.kind -eq 'copy' -and (Get-FileHash -LiteralPath $source).Hash -eq (Get-FileHash -LiteralPath $target).Hash) { continue }
            # Unlink the offline name before copying. Never overwrite shared asset bytes.
            Remove-Item -LiteralPath $target
        }
        if ($inputFile.kind -eq 'copy') { Copy-Item -LiteralPath $source -Destination $target }
        else { New-Item -ItemType HardLink -Path $target -Target $source | Out-Null }
    }
}
function Move-LaunchFiles([string]$SteamGame, [string]$Backup, [string[]]$Names) {
    $moved = [Collections.Generic.List[string]]::new()
    try {
        foreach ($name in $Names) {
            $source = Assert-ChildPath $SteamGame (Join-Path $SteamGame $name)
            $target = Assert-ChildPath $Backup (Join-Path $Backup $name)
            if (Test-Path -LiteralPath $target) { throw "Backup already exists: $target" }
            if (!(Test-Path -LiteralPath $source)) { continue }
            if ((Get-Item -LiteralPath $source -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing to move a linked entry: $source" }
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            Move-Item -LiteralPath $source -Destination $target
            $moved.Add($name)
        }
    } catch {
        for ($i=$moved.Count-1; $i -ge 0; --$i) {
            Move-Item -LiteralPath (Join-Path $Backup $moved[$i]) -Destination (Join-Path $SteamGame $moved[$i])
        }
        throw
    }
    return $moved.ToArray()
}
