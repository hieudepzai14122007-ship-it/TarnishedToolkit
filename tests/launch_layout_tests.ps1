$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
. (Join-Path $project 'LaunchLayout.ps1')
$fixture = Join-Path $project ('build\launch-layout-tests-' + [guid]::NewGuid().ToString('N'))
$steam = Join-Path $fixture 'SteamGame'
$offline = Join-Path $fixture 'OfflineGame'
$backup = Join-Path $fixture 'Backup'
New-Item -ItemType Directory -Path $steam,$backup | Out-Null
$script:checks = 0
function Check([bool]$condition,[string]$message) {
    if (!$condition) { throw "FAIL: $message" }
    ++$script:checks
}
function MustThrow([scriptblock]$action,[string]$message) {
    $failed=$false
    try { & $action } catch { $failed=$true }
    Check $failed $message
}
foreach ($name in @('eldenring.exe','amd_ags_x64.dll','bink2w64.dll','eossdk-win64-shipping.dll','oo2core_6_win64.dll','steam_api64.dll','regulation.bin','Data0.bdt','Data0.bhd','dinput8.dll','winhttp.dll','unknown.dll')) {
    [IO.File]::WriteAllText((Join-Path $steam $name),'fixture-original-'+$name)
}
Sync-OfflineInputs $steam $offline
Check (Test-Path -LiteralPath (Join-Path $offline 'eldenring.exe')) 'Executable copied'
Check (!(Test-Path -LiteralPath (Join-Path $offline 'dinput8.dll'))) 'Source proxy not auto-copied'
Check (!(Test-Path -LiteralPath (Join-Path $offline 'unknown.dll'))) 'Unknown DLL not copied'
[IO.File]::WriteAllText((Join-Path $offline 'eldenring.exe'),'offline-only')
Check ((Get-Content -LiteralPath (Join-Path $steam 'eldenring.exe') -Raw) -eq 'fixture-original-eldenring.exe') 'Executable bytes are independent'
$links = & fsutil hardlink list (Join-Path $offline 'Data0.bdt')
Check (($links -join "`n") -match 'SteamGame') 'Archive uses hard link to source'
$asset=Join-Path $steam 'Data0.bdt'
Remove-Item -LiteralPath $asset
[IO.File]::WriteAllText($asset,'updated-archive')
Sync-OfflineInputs $steam $offline
Check ((Get-Content -LiteralPath (Join-Path $offline 'Data0.bdt') -Raw) -eq 'updated-archive') 'Refresh follows replacement Steam archive'
Check ((Get-Content -LiteralPath (Join-Path $offline 'eldenring.exe') -Raw) -eq 'fixture-original-eldenring.exe') 'Refresh repairs offline executable'
MustThrow { Sync-OfflineInputs $steam $steam } 'Same directory rejected'
MustThrow { Assert-ChildPath $steam (Join-Path $steam '..\outside.dll') } 'Path escape rejected'
[IO.File]::WriteAllText((Join-Path $backup 'winhttp.dll'),'existing-backup')
MustThrow { Move-LaunchFiles $steam $backup @('dinput8.dll','winhttp.dll') } 'Conflicting backup rejected'
Check (Test-Path -LiteralPath (Join-Path $steam 'dinput8.dll')) 'Earlier move rolled back'
Check ((Get-Content -LiteralPath (Join-Path $backup 'winhttp.dll') -Raw) -eq 'existing-backup') 'Existing backup preserved'
Remove-Item -LiteralPath (Join-Path $backup 'winhttp.dll')
$moved=@(Move-LaunchFiles $steam $backup @('dinput8.dll','winhttp.dll'))
Check ($moved.Count -eq 2) 'Both proxies relocated'
Check (!(Test-Path -LiteralPath (Join-Path $steam 'dinput8.dll'))) 'Steam proxy absent'
Check (Test-Path -LiteralPath (Join-Path $backup 'dinput8.dll')) 'Original proxy retained'
# Exercise actual updater/uninstaller against fixture DLL bytes, never a real executable.
$fixtureProject=Join-Path $fixture 'Toolkit'
New-Item -ItemType Directory -Path $fixtureProject,(Join-Path $fixtureProject 'build'),(Join-Path $offline 'mods') | Out-Null
foreach ($name in @('Update.ps1','Uninstall.ps1')) { Copy-Item -LiteralPath (Join-Path $project $name) -Destination (Join-Path $fixtureProject $name) }
$dll=Join-Path $offline 'mods\TarnishedToolkit.dll'
[IO.File]::WriteAllText($dll,'old-toolkit')
[IO.File]::WriteAllText((Join-Path $fixtureProject 'build\TarnishedToolkit.dll'),'new-toolkit')
@{schema=2;game=$offline;steamGame=$steam;toolkitHash=(Get-FileHash -LiteralPath $dll).Hash} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixtureProject 'installation.json')
& (Join-Path $fixtureProject 'Update.ps1')
Check ((Get-Content -LiteralPath $dll -Raw) -eq 'new-toolkit') 'Updater targets offline copy'
Check (!(Test-Path -LiteralPath (Join-Path $steam 'mods\TarnishedToolkit.dll'))) 'Updater leaves Steam clear'
& (Join-Path $fixtureProject 'Uninstall.ps1')
Check (!(Test-Path -LiteralPath $dll)) 'Uninstaller removes offline toolkit'
Check (!(Test-Path -LiteralPath (Join-Path $steam 'dinput8.dll'))) 'Uninstaller keeps Steam clear'
Check (Test-Path -LiteralPath (Join-Path $backup 'dinput8.dll')) 'Uninstaller preserves original backup'
Write-Output "$script:checks launch-layout checks passed. Fixture retained in $fixture. No game or graphics host launched."
