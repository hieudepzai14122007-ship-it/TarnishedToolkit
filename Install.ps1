param([string]$GameDir = (Join-Path $PSScriptRoot '..\Game'))
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'installation.json')) { throw 'An installation record already exists. Use Update.ps1 to preserve the separate launch setup.' }
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'installation.uninstalled.json')) { throw 'A separated installation was removed. Inspect its retained record and backups before reinstalling.' }
if (Get-Process -Name eldenring -ErrorAction SilentlyContinue) { throw 'Close Elden Ring before installing.' }
$game = (Resolve-Path -LiteralPath $GameDir).Path
$exe = Join-Path $game 'eldenring.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Select the Game folder containing eldenring.exe.' }
$loader = Join-Path $game 'dinput8.dll'
if (!(Test-Path -LiteralPath $loader) -or (Get-FileHash -LiteralPath $loader).Hash -ne '42BBE9B15AE6754CEBE3752C5DD084523994F14461036A90B2A8E5887377D004') { throw 'This installer targets the inspected Elden Mod Loader. Other loaders require manual configuration.' }
$artifact = Join-Path $PSScriptRoot 'build\TarnishedToolkit.dll'
if (!(Test-Path -LiteralPath $artifact)) { throw 'Run Build.ps1 first.' }
$mods = Join-Path $game 'mods'
$target = Join-Path $mods 'TarnishedToolkit.dll'
$manifestPath = Join-Path $PSScriptRoot 'installation.json'
if ((Test-Path -LiteralPath $target) -or (Test-Path -LiteralPath $manifestPath)) { throw 'Toolkit installation already exists. Uninstall it first.' }
$config = Join-Path $game 'mod_loader_config.ini'
$configBackup = Join-Path $PSScriptRoot 'mod_loader_config.original.ini'
if (Test-Path -LiteralPath $configBackup) { throw 'Previous configuration backup exists; inspect it before installing again.' }
$oldMenu = Join-Path $mods 'Elden Menu.dll'
$disabledMenu = Join-Path $mods 'Elden Menu.dll.toolkit-disabled'
if (Test-Path -LiteralPath $disabledMenu) { throw 'A disabled Elden Menu already exists; resolve that installation first.' }
$previousConfig = [IO.File]::ReadAllBytes($config)
$newConfig = [Text.Encoding]::UTF8.GetString($previousConfig) -replace '(?m)^load_delay\s*=\s*\d+\s*$', 'load_delay = 0'
if ($newConfig -notmatch '(?m)^load_delay\s*=\s*0\s*$') { throw 'Could not configure early loading.' }
$manifest = [ordered]@{schema=1;game=$game;toolkitHash=(Get-FileHash -LiteralPath $artifact).Hash;oldMenuHash=$null;configHash=$null}
if (Test-Path -LiteralPath $oldMenu) { $manifest.oldMenuHash = (Get-FileHash -LiteralPath $oldMenu).Hash }
[IO.File]::WriteAllBytes($configBackup,$previousConfig)
try {
    if ($manifest.oldMenuHash) { Rename-Item -LiteralPath $oldMenu -NewName 'Elden Menu.dll.toolkit-disabled' }
    [IO.File]::WriteAllText($config,$newConfig,[Text.UTF8Encoding]::new($false))
    $manifest.configHash=(Get-FileHash -LiteralPath $config).Hash
    New-Item -ItemType Directory -Force -Path $mods | Out-Null
    Copy-Item -LiteralPath $artifact -Destination $target
    $manifest | ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding utf8
} catch {
    if (Test-Path -LiteralPath $target) { if ((Get-FileHash -LiteralPath $target).Hash -eq $manifest.toolkitHash) { Remove-Item -LiteralPath $target } }
    [IO.File]::WriteAllBytes($config,$previousConfig)
    if ((Test-Path -LiteralPath $disabledMenu) -and !(Test-Path -LiteralPath $oldMenu)) { Rename-Item -LiteralPath $disabledMenu -NewName 'Elden Menu.dll' }
    Remove-Item -LiteralPath $configBackup
    throw
}
Write-Output 'Installed TarnishedToolkit.dll. Elden Menu was preserved under a disabled filename. Use the existing offline launcher. Insert opens the menu. Experimental controls start disarmed; enable offline testing on Home only while playing offline.'
