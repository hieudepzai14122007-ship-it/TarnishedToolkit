# Tarnished Toolkit 0.2.6 beta

An in-game DirectX 12 menu for the fingerprinted Elden Ring 2.7.0.0 and 2.7.1.0 installations. This update implements the cursor recenter/confinement fix and adds experimental trainer and planning tools. **The new input and gameplay changes have not been tested in-game.** The game and renderer host were not launched for this update.

## Download and first-time setup

### 1. Download the compiled mod

1. Open the [0.2.6 beta release](https://github.com/hieudepzai14122007-ship-it/TarnishedToolkit/releases/tag/v0.2.6-beta).
2. Expand **Assets** and download **TarnishedToolkit-0.2.6-flying.zip**. The automatic **Source code** downloads do not contain the compiled DLL.
3. Right-click the downloaded ZIP, choose **Extract All**, and keep the complete `TarnishedToolkit` folder. The mod is `TarnishedToolkit/build/TarnishedToolkit.dll`; you do not need to compile it yourself.

### 2. Check requirements and back up your save

- Windows 64-bit and your own Steam copy of Elden Ring are required. Gameplay controls recognize only the exact supported 2.7.0.0 and 2.7.1.0 executable hashes, not every game version. Infinite Torrent jumps require the supported 2.7.1.0 build.
- **This beta's automatic installer targets an existing, specific loader setup.** It expects the inspected Elden Mod Loader `dinput8.dll` and its `mod_loader_config.ini` in `Game/`. The separation step also expects the inspected EAC Toggler `winhttp.dll` and valid signed game/Steam/EAC files. Those external tools are not included. Their project pages are [Elden Mod Loader](https://github.com/techiew/EldenRingModLoader) and [EAC Toggler](https://github.com/techiew/EldenRingEacToggler). A newer/different loader may be rejected.
- If you have a clean game installation or another mod manager, this release does not provide a universal installer for it. Do not place random proxy DLLs beside the game or bypass a failed installer check. Include the exact message in a [GitHub issue](https://github.com/hieudepzai14122007-ship-it/TarnishedToolkit/issues) for setup support.
- Close the game and finish Steam Cloud synchronization. Press **Win+R**, enter `%APPDATA%\EldenRing`, and copy your account's save folder to a separate backup location. Keep that backup before changing attributes, items or runes.

### 3. Install into the supported loader setup

Skip this section if this toolkit is already installed; use the update instructions below instead.

1. In Steam, right-click **Elden Ring > Manage > Browse local files**. This opens the `ELDEN RING` folder containing `Game`.
2. Place the extracted `TarnishedToolkit` folder beside `Game`, so the layout is:

   ```text
   ELDEN RING/
     Game/
       eldenring.exe
     TarnishedToolkit/
       Install.ps1
       build/
         TarnishedToolkit.dll
   ```

3. Open the `TarnishedToolkit` folder in File Explorer. Type `powershell` into its address bar and press Enter.
4. Run these commands **one at a time**. Stop if any command reports an error; do not continue with the next step.

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1
   powershell -NoProfile -ExecutionPolicy Bypass -File .\SeparateLaunches.ps1
   powershell -NoProfile -ExecutionPolicy Bypass -File .\CreateLaunchShortcuts.ps1
   powershell -NoProfile -ExecutionPolicy Bypass -File .\LaunchOffline.ps1 -CheckOnly
   ```

   These commands do not launch the game. The execution-policy option applies to each command's process, not your permanent Windows policy. If Windows reports access denied for the Steam folder, reopen PowerShell as administrator, navigate to the same folder, and retry the failed step.

Successful separation creates a sibling `OfflineGame` folder and a desktop **Elden Ring - Offline Mods** shortcut. Keep the toolkit folder in place: its installation record and shortcut are used for future updates. See [launch separation](docs/LAUNCH_SEPARATION.md) for what is copied and backed up.

### 4. Launch and open the menu

1. Keep Steam running. Close any other Elden Ring session.
2. Double-click **Elden Ring - Offline Mods** on your desktop. The first file check/refresh can take a few seconds. Use **Play in Steam** when you want the normal game without the menu.
3. Load a character in offline single-player, then press **Insert** to open the menu. Some compact keyboards require **Fn+Insert**.
4. On **Home**, select **I am playing offline - enable experimental controls** only when you are playing offline. This checkbox is your declaration; it does not detect or switch the game's network mode.
5. Press **Insert** again to close the menu and resume normal mouse input. The menu does not pause gameplay.

**Both launch paths still share character saves.** Steam launch does not undo modified attributes, granted items or runes. Disable All stops temporary modifiers; it does not undo persistent edits.

### 5. Try a feature

| What you want | Where to go and what to do |
| --- | --- |
| Fly | Dismount, open **Player > Flying**, and enable **Enable flying (on foot)**. Close the menu. **I/K** and **J/L** move in fixed horizontal directions; **Page Up / Page Down** rise/descend. Adjust speed from 0.5 to 10 m/s in the menu. Descend near the ground before stopping. |
| Edit stats | **Player > Character attributes - edit**. Enter values, select **Preview attribute changes**, then **Apply attributes once**. Reload the character before checking derived stats. |
| Get a weapon | **Items & Builds**, set **Type: Weapons**, select a base-game weapon, choose **Preview persistent item grant**, then **Apply once**. This grants one default +0 copy; DLC grants are unavailable. Maxing your stats is not required. |
| Jump repeatedly on Torrent | Mount Torrent, open **Player > Torrent**, enable **Infinite Torrent double jumps**, close the menu, and press jump again in the air. The toggle stops on dismount/loading. Fall damage is unchanged. |
| Stop temporary effects | Choose **Disable All** or press **Ctrl+Shift+Backspace**. |

### Update or uninstall

- **Update:** close the game, download/extract the new release, and copy its `build/TarnishedToolkit.dll` into the **existing** toolkit folder's `build` directory. Keep `installation.json`, backups and the folder location. From that existing folder, run `powershell -NoProfile -ExecutionPolicy Bypass -File .\Update.ps1`. Follow any additional migration steps specified by the newer release; do not rerun first-time installation.
- **Uninstall:** close the game, open PowerShell in the installed toolkit folder, and run `powershell -NoProfile -ExecutionPolicy Bypass -File .\Uninstall.ps1`. For the separated installation this removes the offline toolkit, leaves Steam clear, preserves backups/assets, and makes the shortcut inactive. It does not reverse save edits.

### Troubleshooting

| Problem | What to check |
| --- | --- |
| No menu when using Steam Play | That is expected after separation. Use the **Offline Mods** desktop shortcut. |
| Installer says loader missing/unrecognized | Check the prerequisites above. The scripts support specific external loader files, not all mod managers. Keep the exact error message. |
| Installer says an installation already exists | Use **Update.ps1** from the original toolkit folder. Do not delete its installation record to force a reinstall. |
| Failed to receive player data / controls unavailable | Load a living character and read the detailed reason on **Home**. An unsupported executable hash requires a compatible toolkit update; repeated restarts or checking offline mode cannot fix a version mismatch. |
| Offline checkbox is selected but actions are unavailable | Offline declaration is only one requirement. The character must be loaded and the executable/feature supported. |
| Weapon grant unavailable | Select an eligible base-game weapon and review the message; DLC/unclassified entries are blocked. |
| Steam reports 0xc000007b or an EAC launch error | Do not use the old toggler to switch modes after separation. Verify Steam's installed game files if normal launch still fails. The error may have causes beyond the mod layout. |
| Shortcut fails or menu still does not appear | Run **LaunchOffline.ps1 -CheckOnly** from the installed toolkit folder and inspect `%LOCALAPPDATA%\TarnishedToolkit\toolkit.log`. When reporting an issue, include the exact error and game version, and remove personal paths from logs. |

## Flying (experimental)

Confirm offline mode, load a living character, dismount, then use **Player > Flying > Enable flying (on foot)**. It temporarily disables gravity and moves your character with **I/K**, **J/L**, and **Page Up / Page Down**. These horizontal directions are fixed world directions, not camera-relative. Flying speed is adjustable from 0.5 to 10 m/s. Keyboard only; no saved profile or automatic activation.

Opening the menu or switching away from the game pauses flight input while gravity suppression remains active. Dismount is required to enable; mounting, loading, character changes, applying a profile, attribute edits, unchecking offline confirmation, and Disable All stop flight. Gravity is restored only while the original player/physics and owned byte can still be verified. Descend close to stable ground before disabling: there is no landing detector, fall immunity, death-zone protection or verified noclip/collision behavior. The worker clears the local in-air timer during flight; live movement smoothness and collision interactions still require testing. See [flight implementation and checks](docs/FLYING.md).

## Infinite Torrent jumps

**Launch update:** Use Play in Steam for the normal game, or the desktop **Elden Ring - Offline Mods** shortcut for the menu (with Steam running). The menu now lives in the sibling `OfflineGame/` folder. Character saves are still shared, including persistent item/stat changes. See [launch separation](docs/LAUNCH_SEPARATION.md) for maintenance and measured checks. Neither launch was tested in-game here.

On the supported 2.7.1.0 game, confirm offline mode, mount Torrent, then open **Player > Torrent > Infinite Torrent double jumps**. Close the menu and press jump again while airborne for additional jumps. This is a temporary toggle that stops on dismount, loading/character changes, applying a temporary profile, or Disable All. It never applies from a saved profile. Ordinary player-on-foot jumping is unchanged. The native hook has component coverage, but in-game jump timing remains for your testing. Fall damage and death zones remain possible.

## September game update

The installed game changed to file version 2.7.1.0 (Steam build 25080141, public patch 1.17.1). Toolkit 0.2.3 recognized only the previous executable and therefore refused to read player data, regardless of restarts or offline confirmation. Toolkit 0.2.4 recognizes both exact executable hashes after checking the newer pinned address source and all three native entry-point guards. Home now displays the exact player-reader failure stage; the header shows the matched game version. Future unrecognized builds stay blocked and are described explicitly.

## Using the update

Press **Insert** to open or close the menu. In offline single-player, select **Home > I am playing offline - enable experimental controls**. This is a user declaration, not automatic detection of session mode. Testing starts disarmed. Your offline confirmation lasts until you uncheck it, use Disable All, or restart the game. Death, loading, character/map changes and failed actions stop temporary modifiers without clearing that confirmation. The checkbox responds immediately even during loading; a separate waiting message explains why gameplay controls are unavailable. Opening the menu does not pause gameplay.

Check mouse movement, repeated Insert open/close, and Alt+Tab first. Then try one resource control at a time. Use a backed-up disposable character for item/rune changes. **Ctrl+Shift+Backspace** disables temporary modifiers and disarms testing, including while a confirmation dialog is open. Persistent grants cannot be undone by Disable All.

## Implemented

- Cursor recenter interception, release of confinement while the menu owns foreground focus, and release on close/focus loss. Raw Input and DirectInput neutralization supplements Win32 and XInput capture. Controller disconnection clears navigation state.
- HP/FP/stamina display and one-shot refill; separate ordinary-death prevention, HP damage protection, infinite FP and infinite stamina.
- Individual buildup clear/suppression for seven statuses. These do not remove already active status effects.
- Editable character attributes (1-99) with level/history planning, explicit preview and verified readback; read-only nearby-entity HP/poise inspection independent of the game's lock-on target.
- Searchable 2,034-item catalog with category/content filters and selected-item ownership inspection. 1,599 eligible base-game entries (including 377 weapons, shields and casting tools at +0) can be granted through the engine workflow with exact quantity preview and Apply once.
- Rune addition, 1 to 1,000,000 per action, with stale-preview rejection and observed result logging. This is an addition workflow, not an unrestricted balance editor.
- Local Strength, Dexterity, Sorcery, Faith, Arcane and Custom planning slots: item quantities, selected-row missing counts, save/load, and exact smithing/somber material pack plans. Selecting a plan never grants or equips anything. Grants are individually previewed.
- Session-only same-map bookmarks and experimental return within 100 metres while dismounted. Ground/cutscene detection is absent; save stable-ground locations and avoid return during combat or transitions.
- Global simulation speed 0.25–1.5x with owned-value restoration.
- Category favorites, saved profiles with preview/apply, Explorer/Build Lab/Vanilla presets, active-modifier display, and emergency Disable All.
- Manual timer, real-time HUD, last 100 attempts, shortest manual attempt and CSV export. Arming testing marks the attempt modified; a short time is not treated as a victory.

These controls contain actual adapters. Source-backed implementation and automated validation do not prove correct behavior inside a loaded game. Engine calls run from the 64 ms worker; a verified game-thread callback, automatic session monitor and cutscene detector are not implemented.

## Still unavailable

Player-only damage multipliers; restricted consumable preservation; independent movement/attack/casting speed; native rebirth and an engine callback for derived-stat recalculation; weapon upgrade/affinity editing; automatic equip/bulk missing-item grants; DLC grants; cross-map/grace travel; world time; noclip; boss resets/phase selection/nonlethal sparring; automatic combat outcomes/hit attribution; hide-game-HUD/FOV/free camera; pause/frame stepping. See `docs/FEATURES.md`.

## Editing character attributes

Load a living character, arm offline testing on Home, then open **Player > Character attributes - edit**. Enter the new values, select **Preview attribute changes**, review all eight values and the resulting level, then **Apply attributes once**. Read current attributes/reset draft discards unsaved edits. Values changing while the confirmation is open invalidate that preview.

The experimental trainer workflow keeps each attribute within 1-99 and the resulting level within 1-713. Level changes by the net attribute-point change. Spendable runes are preserved; rune history increases for net level gains using the pinned trainer formula, saturates at its limit, and is not reduced by level decreases. Starting-class minimums are not enforced. Temporary modifiers turn off after successful Apply; offline confirmation is retained. Reload the character before checking derived HP/FP, defense and damage; automatic recalculation has not been verified.

This is a persistent, ordered data edit based on the reference trainer, not an atomic engine respec operation. Failed writes attempt to restore only still-owned values on the same character. A stale character or conflicting edit can prevent full rollback, which is reported explicitly. Requests and outcomes go to `attribute-edits.log`; this audit is not a save backup. Disable All does not undo successful attribute changes.

## Getting a weapon

After confirming offline mode on Home, open **Items & Builds**, set **Type: Weapons**, and choose a base-game weapon. Select **Preview persistent item grant**, then **Apply once**. This grants one +0 copy with its default affinity and skill; already owning a copy does not block another. Character attributes affect using a weapon, not grant eligibility. DLC and unclassified weapons remain marked unavailable. Upgrades and affinity editing are not implemented. Check the reported before/after inventory count; the menu never automatically retries a grant.

## Build

Windows x64, C++20, MSYS2 UCRT64 GCC 14.2.0:

```powershell
& '.\Build.ps1'
# Alternate compiler location:
& '.\Build.ps1' -Toolchain 'D:\msys64\ucrt64\bin'
```

The build uses pinned vendored dependencies without downloads. It compiles the DLL and test executables, runs component tests (including a synthetic native horse-hook fixture), and does not launch the graphics host or game. Outputs: `build/TarnishedToolkit.dll`, `build/core_tests.exe`, `build/storage_tests.exe`, `build/attribute_tests.exe`, `build/horse_jump_tests.exe`, `build/renderer_test.exe`.

CMake 3.20+ configuration is supplied; the locally verified path is PowerShell. Optional `GenerateCatalog.py` regenerates `src/catalog.inc` from the pinned source. `GenerateProfile.py` regenerates entrypoint headers only for the recorded executable hash. Generated files are included; Python and the game executable are not needed to compile.

## Install, update and remove

Close the game. For the existing separated installation, run `Update.ps1`; it updates only the recorded offline copy and retains the previous DLL. `LaunchOffline.ps1 -CheckOnly` verifies the separate paths and mod hashes without starting the game. Original loader/menu backups are preserved.

For the inspected legacy loader setup, `Install.ps1` creates the initial toolkit record, `SeparateLaunches.ps1` migrates it into separate folders, and `CreateLaunchShortcuts.ps1` creates the offline shortcut. This is a migration for that known setup, not a universal loader installer. Launch method is not automatic session detection. See `%LOCALAPPDATA%/TarnishedToolkit/toolkit.log` if the menu fails to load.

Close the game and run `Uninstall.ps1` to remove the toolkit. The separated schema-2 installation keeps Steam clear, preserves offline assets and backups, and makes the shortcut inactive. It never restores the old menu into Steam. Legacy schema-1 removal retains its original restoration behavior. Hash checks prevent overwriting unrelated changes. Runtime unloading is unsupported; restart to remove hooks. These scripts do not change game saves.

## Controls and local data

- Insert: toggle; remappable to F1–F12 under Settings.
- Escape: cancel confirmation dialog, or close menu.
- Ctrl+Shift+Backspace: Disable All, including with the menu closed or a dialog open.
- Tab/arrows/Enter: keyboard navigation.
- XInput: D-pad/left stick, A activate, B cancel/close, LB/RB categories. Choose an optional 0.65-second Start+Back or LB+RB+Start opening chord under Settings. It defaults to unbound; physical behavior remains untested.

Data lives under `%LOCALAPPDATA%/TarnishedToolkit`: `settings.txt`, `profiles.txt`, `favorites.txt`, `plan-<name>.txt`, `toolkit.log`, and `attempts.csv`. Configuration is versioned and atomically replaced. Names accept letters, digits, spaces, dash or underscore. Profiles never apply at startup. Bookmarks remain in memory to prevent reuse across sessions. Logs rotate at 1 MiB; CSV retains earlier attempts.

## Save backup

Before persistent-action testing, close the game and finish Steam cloud synchronization. Locate the intended Steam-account directory under `%APPDATA%/EldenRing`; copy that exact directory's `ER0000.sl2` and associated backup to a new timestamped folder. Keep earlier backups. Restore only with the game closed, preserving the current save separately first. No automatic copying/restoration or live-save editor is included.

See `docs/COMPATIBILITY.md`, `docs/TEST_RESULTS.md`, and `THIRD_PARTY_NOTICES.md` for evidence and limitations.
