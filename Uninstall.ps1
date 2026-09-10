$ErrorActionPreference = 'Stop'
if (Get-Process -Name eldenring -ErrorAction SilentlyContinue) { throw 'Close Elden Ring before uninstalling.' }
$manifestPath=Join-Path $PSScriptRoot 'installation.json'
if (!(Test-Path -LiteralPath $manifestPath)) { throw 'No toolkit installation record exists.' }
$record=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($record.schema -notin 1,2) { throw 'Unsupported installation record.' }
$game=(Resolve-Path -LiteralPath $record.game).Path
if (!(Test-Path -LiteralPath (Join-Path $game 'eldenring.exe'))) { throw 'Installation directory is no longer an Elden Ring game folder.' }
$target=Join-Path $game 'mods\TarnishedToolkit.dll'
if ($record.schema -eq 2) {
    if ((Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target).Hash -ne $record.toolkitHash) { throw 'Toolkit DLL changed after installation; refusing to remove it.' }
    # Keep the separation record for inspection; removal never reactivates loaders in Steam.
    $removedRecord=Join-Path $PSScriptRoot 'installation.uninstalled.json'
    if (Test-Path -LiteralPath $removedRecord) { throw 'An earlier uninstall record exists; inspect it before retrying.' }
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target }
    Move-Item -LiteralPath $manifestPath -Destination $removedRecord
    Write-Output 'Toolkit removed from the offline copy. Steam files, offline assets and original backups retained. Offline shortcut is inactive.'
    return
}
$old=Join-Path $game 'mods\Elden Menu.dll'
$disabled=Join-Path $game 'mods\Elden Menu.dll.toolkit-disabled'
$config=Join-Path $game 'mod_loader_config.ini'
$backup=Join-Path $PSScriptRoot 'mod_loader_config.original.ini'
if ((Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target).Hash -ne $record.toolkitHash) { throw 'Toolkit DLL changed after installation; refusing to remove it.' }
if ((Get-FileHash -LiteralPath $config).Hash -ne $record.configHash) { throw 'Loader configuration changed after installation; restore it manually using the preserved backup.' }
if (!(Test-Path -LiteralPath $backup)) { throw 'Original loader configuration backup is missing.' }
if ($record.oldMenuHash) {
    if ((Test-Path -LiteralPath $old) -or !(Test-Path -LiteralPath $disabled) -or (Get-FileHash -LiteralPath $disabled).Hash -ne $record.oldMenuHash) { throw 'Elden Menu files changed; refusing to overwrite them.' }
}
if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target }
Copy-Item -LiteralPath $backup -Destination $config
if ($record.oldMenuHash) { Rename-Item -LiteralPath $disabled -NewName 'Elden Menu.dll' }
Remove-Item -LiteralPath $manifestPath
Remove-Item -LiteralPath $backup
Write-Output 'Toolkit removed; original loader configuration and Elden Menu restored. Source, logs and settings retained.'
