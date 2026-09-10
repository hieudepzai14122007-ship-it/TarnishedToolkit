# Separate launch layout - 2026-09-09

Use **Play in Steam** for the normal game. Use the desktop **Elden Ring - Offline Mods** shortcut for the menu, with Steam running. The shortcut launches `OfflineGame/eldenring.exe` directly, with that folder as its working directory and process-local SteamAppId 1245620. It does not invoke or replace Steam's anti-cheat launcher. The mechanism matches the inspected [existing offline launcher source](https://github.com/techiew/EldenRingEacToggler/blob/master/OfflineLauncher/Main.cpp). The toolkit still requires the user's offline declaration in its menu.

## Files and saves

Steam's `Game/` folder no longer contains dinput8/winhttp proxies, the mods folder, steam_appid.txt or the old toggler/standalone launcher. Its signed game, Valve and EAC executables/DLLs were left in place. The protected launcher matches its original backup byte-for-byte. Original mod files, menu, configs and utilities are preserved under `TarnishedToolkit/launch-backups/20260909-194603/`, with the old installation record. Earlier project backups remain intact.

The offline folder has independent copies of the executable, required game DLLs and regulation, plus the existing dinput8/winhttp loaders and only the current toolkit in its mods directory. Large archives and movies are hard-linked to Steam's assets to avoid duplicating roughly 70 GB. Do not edit these linked assets: this layout supports the toolkit's memory controls, not independent asset replacement mods. Each offline launch refreshes copied files and re-links archives from Steam, including archives that Steam replaces during an update. Refresh only writes/removes offline names; it never overwrites shared asset bytes. Unsupported executable hashes still disable gameplay adapters.

**Character saves remain shared.** This migration neither copies nor changes saves and does not undo persistent item, rune or attribute edits. A clean Steam launch does not make a previously modified character unmodified.

## Maintenance

- Close Elden Ring and its anti-cheat launch window before installation, refresh or update.
- `SeparateLaunches.ps1` migrates the known schema-1 installation once. It checks source signatures, loader/toolkit/config hashes and requires a new sibling offline directory. File moves are rolled back if a backup conflict occurs. Partial preparation leaves Steam untouched and the offline staging directory available for inspection.
- `CreateLaunchShortcuts.ps1` creates desktop and workspace offline shortcuts without opening them.
- `LaunchOffline.ps1 -CheckOnly` checks paths and recorded mod hashes without refreshing or starting a process.
- `Update.ps1` supports schema 2 and updates the offline DLL only, preserving previous builds and the uninstall hash.
- `Uninstall.ps1` removes the offline toolkit, retains assets/backups, and disables the shortcut by retiring the installation record. It never restores proxies or the old menu into Steam. Legacy schema-1 behavior remains unchanged.

## Measured checks and remaining uncertainty

The 0.2.5 DLL is unchanged: SHA256 `D56D4F6B5A0CD3D08EED258ED1D40E02028A088E53D1FEB5FB950EB06D91C1F7`.

Twenty additional launch-layout component checks passed under both PowerShell 7 and Windows PowerShell 5.1 (the shortcut host). They exercise independent executable bytes, shared archives, relinking after a source archive is replaced, exclusion of arbitrary DLLs, path/same-directory rejection, migration rollback when a backup conflicts, preserved originals, and the actual schema-2 update/uninstall scripts using fixture files. Build.ps1 includes these checks. No fixture executes a game executable.

The real migration, separate launch validation and updater no-op check passed. Neither the game nor the graphics host was launched. Steam launch, offline launch, menu appearance and resolution of the reported 0xc000007b error still require user testing.

The screenshot shows an EAC startup error with both mod proxies in the game folder. The [old toggler's documentation](https://github.com/techiew/EldenRingEacToggler#winhttpdll) states its winhttp proxy deliberately causes EAC to refuse launch. This confirms a launch-layout conflict, not the precise cause of Windows' error code. If Steam still fails after separation, verify installed files in Steam before further diagnosis.
