param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
try {
    . (Join-Path $PSScriptRoot 'LaunchLayout.ps1')
    Assert-GameClosed
    $record = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'installation.json') -Raw | ConvertFrom-Json
    if ($record.schema -ne 2) { throw 'Run SeparateLaunches.ps1 before using this launcher.' }
    $offline = (Resolve-Path -LiteralPath $record.game).Path
    $steam = (Resolve-Path -LiteralPath $record.steamGame).Path
    foreach ($name in @('dinput8.dll','winhttp.dll','dxgi.dll','d3d12.dll','version.dll','dsound.dll','xinput1_3.dll')) {
        if (Test-Path -LiteralPath (Join-Path $steam $name)) { throw "A mod loader has returned to the Steam folder ($name). Resolve it before using the separated setup." }
    }
    foreach ($check in @(@('mods\TarnishedToolkit.dll',$record.toolkitHash),@('dinput8.dll',$record.loaderHash),@('winhttp.dll',$record.offlineProxyHash),@('mod_loader_config.ini',$record.configHash))) {
        if ((Get-FileHash -LiteralPath (Join-Path $offline $check[0])).Hash -ne $check[1]) { throw "Offline file differs from its installation record: $($check[0])" }
    }
    if ($CheckOnly) { Write-Output 'Separate launch paths and mod hashes verified. No game was launched.'; return }
    # Refresh from Steam before each offline launch, including relinking replaced data archives.
    Sync-OfflineInputs $steam $offline
    Assert-GameClosed
    # Same documented mechanism as techiew's existing offline launcher, with explicit absolute path/cwd.
    # Do not invoke Steam's protected launcher or change its files.
    $previousAppId = [Environment]::GetEnvironmentVariable('SteamAppId','Process')
    try {
        [Environment]::SetEnvironmentVariable('SteamAppId','1245620','Process')
        Start-Process -FilePath (Join-Path $offline 'eldenring.exe') -WorkingDirectory $offline -WindowStyle Normal
    } finally { [Environment]::SetEnvironmentVariable('SteamAppId',$previousAppId,'Process') }
} catch {
    if ($CheckOnly) { throw }
    Add-Type -AssemblyName System.Windows.Forms
    [Windows.Forms.MessageBox]::Show($_.Exception.Message,'Offline mod launcher') | Out-Null
    exit 1
}
