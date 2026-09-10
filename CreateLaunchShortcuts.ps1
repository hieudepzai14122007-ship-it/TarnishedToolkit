$ErrorActionPreference = 'Stop'
$record = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'installation.json') -Raw | ConvertFrom-Json
if ($record.schema -ne 2) { throw 'Separate the launch folders first.' }
$shell = New-Object -ComObject WScript.Shell
$launcher = Join-Path $PSScriptRoot 'LaunchOffline.ps1'
$arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $launcher + '"'
foreach ($folder in @((Split-Path -Parent $PSScriptRoot),[Environment]::GetFolderPath('DesktopDirectory'))) {
    if (!$folder -or !(Test-Path -LiteralPath $folder)) { continue }
    $path = Join-Path $folder 'Elden Ring - Offline Mods.lnk'
    $shortcut = $shell.CreateShortcut($path)
    if ((Test-Path -LiteralPath $path) -and $shortcut.Arguments -ne $arguments) { throw "An unrelated shortcut already exists: $path" }
    $shortcut.TargetPath = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $shortcut.Arguments = $arguments
    $shortcut.WorkingDirectory = $PSScriptRoot
    $shortcut.IconLocation = (Join-Path $record.game 'eldenring.exe') + ',0'
    $shortcut.Description = 'Offline Tarnished Toolkit; separate game files, shared character saves.'
    $shortcut.WindowStyle = 7
    $shortcut.Save()
    Write-Output "Created offline shortcut: $path"
}
