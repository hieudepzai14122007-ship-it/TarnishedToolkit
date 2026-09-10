$ErrorActionPreference = 'Stop'
if (Get-Process -Name eldenring -ErrorAction SilentlyContinue) { throw 'Close Elden Ring before updating. The new build remains staged in build.' }
$manifestPath = Join-Path $PSScriptRoot 'installation.json'
if (!(Test-Path -LiteralPath $manifestPath)) { throw 'No installation record here. Run this updater from the original toolkit installation folder.' }
$record = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($record.schema -notin 1,2) { throw 'Unsupported installation record.' }
$game = (Resolve-Path -LiteralPath $record.game).Path
if (!(Test-Path -LiteralPath (Join-Path $game 'eldenring.exe'))) { throw 'Recorded directory is not an Elden Ring game folder.' }
$target = Join-Path $game 'mods\TarnishedToolkit.dll'
$artifact = Join-Path $PSScriptRoot 'build\TarnishedToolkit.dll'
$oldHash = (Get-FileHash -LiteralPath $target).Hash
if ($oldHash -ne $record.toolkitHash) { throw 'Installed DLL differs from its record; refusing to overwrite it.' }
$newHash = (Get-FileHash -LiteralPath $artifact).Hash
if ($oldHash -eq $newHash) { Write-Output 'The installed DLL already matches this build.'; return }
$backupDir = Join-Path $PSScriptRoot 'previous-builds'
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$backup = Join-Path $backupDir ($oldHash + '.dll')
if (Test-Path -LiteralPath $backup) {
    if ((Get-FileHash -LiteralPath $backup).Hash -ne $oldHash) { throw 'Existing previous-build backup has an unexpected hash.' }
} else { Copy-Item -LiteralPath $target -Destination $backup }
$previousManifest = [IO.File]::ReadAllBytes($manifestPath)
$manifestTemp = Join-Path $PSScriptRoot 'installation.update.tmp'
$targetTemp = Join-Path $game 'mods\TarnishedToolkit.update.tmp'
$replacedDll = Join-Path $PSScriptRoot 'installation.replaced-dll.tmp'
$replacedManifest = Join-Path $PSScriptRoot 'installation.replaced-record.tmp'
if ((Test-Path -LiteralPath $manifestTemp) -or (Test-Path -LiteralPath $targetTemp) -or (Test-Path -LiteralPath $replacedDll) -or (Test-Path -LiteralPath $replacedManifest)) { throw 'A previous update staging file exists. Inspect it before retrying.' }
try {
    Copy-Item -LiteralPath $artifact -Destination $targetTemp
    if ((Get-FileHash -LiteralPath $targetTemp).Hash -ne $newHash) { throw 'Staged DLL hash mismatch.' }
    if (Get-Process -Name eldenring -ErrorAction SilentlyContinue) { throw 'Game started during update; close it before retrying.' }
    $record.toolkitHash = $newHash
    [IO.File]::WriteAllText($manifestTemp,($record | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
    [IO.File]::Replace($targetTemp,$target,$replacedDll)
    [IO.File]::Replace($manifestTemp,$manifestPath,$replacedManifest)
    if ((Get-FileHash -LiteralPath $target).Hash -ne $newHash) { throw 'Installed DLL verification failed.' }
} catch {
    # Preserve the original manifest and DLL if the two-file update cannot finish.
    if ((Get-FileHash -LiteralPath $target).Hash -eq $newHash) { Copy-Item -LiteralPath $backup -Destination $target }
    [IO.File]::WriteAllBytes($manifestPath,$previousManifest)
    throw
} finally {
    if (Test-Path -LiteralPath $manifestTemp) { Remove-Item -LiteralPath $manifestTemp }
    if (Test-Path -LiteralPath $targetTemp) { Remove-Item -LiteralPath $targetTemp }
    if (Test-Path -LiteralPath $replacedDll) { Remove-Item -LiteralPath $replacedDll }
    if (Test-Path -LiteralPath $replacedManifest) { Remove-Item -LiteralPath $replacedManifest }
}
Write-Output 'Toolkit updated. Previous DLL preserved; original loader/menu backups retained. The game was not launched.'
