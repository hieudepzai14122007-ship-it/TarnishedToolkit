$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'LaunchLayout.ps1')
Assert-GameClosed
$manifestPath = Join-Path $PSScriptRoot 'installation.json'
$record = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($record.schema -eq 2) { Write-Output 'Steam and offline mod launches are already separated.'; return }
if ($record.schema -ne 1) { throw 'Unsupported installation record.' }
$steam = (Resolve-Path -LiteralPath $record.game).Path
$offline = Join-Path (Split-Path -Parent $steam) 'OfflineGame'
$backup = Join-Path $PSScriptRoot ('launch-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (Test-Path -LiteralPath $offline) { throw 'OfflineGame already exists. Inspect it before retrying; nothing was moved.' }
foreach ($name in @('eldenring.exe','start_protected_game.exe','steam_api64.dll')) {
    if ((Get-AuthenticodeSignature -LiteralPath (Join-Path $steam $name)).Status -ne 'Valid') { throw "Verify Steam game files first: $name has no valid signature." }
}
$toolkit = Join-Path $steam 'mods\TarnishedToolkit.dll'
if ((Get-FileHash -LiteralPath $toolkit).Hash -ne $record.toolkitHash) { throw 'Installed toolkit differs from its installation record.' }
if ((Get-FileHash -LiteralPath (Join-Path $steam 'mod_loader_config.ini')).Hash -ne $record.configHash) { throw 'Loader configuration differs from its installation record.' }
if ((Get-FileHash -LiteralPath (Join-Path $steam 'dinput8.dll')).Hash -ne '42BBE9B15AE6754CEBE3752C5DD084523994F14461036A90B2A8E5887377D004') { throw 'Unrecognized loader; refusing to relocate it automatically.' }
if ((Get-FileHash -LiteralPath (Join-Path $steam 'winhttp.dll')).Hash -ne '237572EC9DBADC7FB0477C8093DC47AACC779DE5199346D0E050EB52A65CFBED') { throw 'Unrecognized offline proxy; inspect it before relocation.' }
$before = [IO.File]::ReadAllBytes($manifestPath)
New-Item -ItemType Directory -Path $backup | Out-Null
[IO.File]::WriteAllBytes((Join-Path $backup 'installation-before.json'),$before)
Sync-OfflineInputs $steam $offline
New-Item -ItemType Directory -Path (Join-Path $offline 'mods') | Out-Null
Copy-Item -LiteralPath $toolkit -Destination (Join-Path $offline 'mods\TarnishedToolkit.dll')
foreach ($name in @('dinput8.dll','mod_loader_config.ini','winhttp.dll')) {
    Copy-Item -LiteralPath (Join-Path $steam $name) -Destination (Join-Path $offline $name)
}
[IO.File]::WriteAllText((Join-Path $offline 'steam_appid.txt'),'1245620',[Text.UTF8Encoding]::new($false))
$newRecord = [ordered]@{schema=2;game=$offline;steamGame=$steam;launchBackup=$backup;toolkitHash=$record.toolkitHash;configHash=$record.configHash;loaderHash=(Get-FileHash -LiteralPath (Join-Path $offline 'dinput8.dll')).Hash;offlineProxyHash=(Get-FileHash -LiteralPath (Join-Path $offline 'winhttp.dll')).Hash;oldMenuHash=$record.oldMenuHash}
$names = @('dinput8.dll','winhttp.dll','mods','mod_loader_config.ini','mod_loader_log.txt','steam_appid.txt','start_game_in_offline_mode.exe','toggle_anti_cheat.exe','anti_cheat_toggler_config.ini','anti_cheat_toggler_log.txt','anti_cheat_toggler_mod_list.txt','start_protected_game.exe.original')
Assert-GameClosed
$moved = @(Move-LaunchFiles $steam $backup $names)
try {
    $newRecord | ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding UTF8
} catch {
    foreach ($name in $moved) { Move-Item -LiteralPath (Join-Path $backup $name) -Destination (Join-Path $steam $name) }
    [IO.File]::WriteAllBytes($manifestPath,$before)
    throw
}
Write-Output "Separated. Steam: $steam. Offline mods: $offline. Original mod files preserved: $backup. No game was launched."
