# Compatibility evidence â€” 2026-09-06

## Original target (historical 2026-09-06)

| Property | Observed value |
| --- | --- |
| Platform | Windows x64, Steam application 1245620 |
| Steam installed build ID | 23850278 |
| Executable file/product version | 2.7.0.0 |
| Executable bytes | 87,024,720 |
| Executable SHA256 | D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134 |
| DLC files | DLC.bdt, DLC.bhd and movie_dlc present; depot 2778580 listed in Steam manifest |
| Loader DLL SHA256 | 42BBE9B15AE6754CEBE3752C5DD084523994F14461036A90B2A8E5887377D004 |
| Existing menu | Elden Menu.dll, config declares 1.12.1; not evidence of support for the installed build |

The file version and SHA256 above are the authoritative target identity. DLC archive presence does not prove all DLC gameplay capabilities; none are enabled.

## Loader and rendering choice

The existing loader's local log confirms loading DLLs from `Game/mods`. Its documented upstream interface is [techiew/EldenRingModLoader](https://github.com/techiew/EldenRingModLoader). It is reused instead of adding a second loader. No loader binaries are redistributed. This legacy loader is not represented as a maintained default. A loader migration to me3 was not needed for the installed DLL interface, and no me3 compatibility is claimed.

The overlay hooks DXGI factory swapchain creation and records the actual supplied D3D12 command queue; it does not guess the queue by watching arbitrary submissions. It must initialize before the first game swapchain. Dear ImGui's pinned official Win32 and DX12 backends render the overlay. [Official integration guide](https://github.com/ocornut/imgui/wiki/Getting-Started).

## Gameplay access provenance

[borgCode/TarnishedTool](https://github.com/borgCode/TarnishedTool), commit `2a7a76939d3dd21c233ee4f1e5df3b2dddf0e49c` (2026-09-05), MIT license. Reference file: `TarnishedTool/Memory/Offsets.cs`, `Version2_7_0` profile; behavior references: `Services/PlayerService.cs`, `Services/ChrInsService.cs`. A snapshot of the relevant source is included under `provenance/`.

| Meaning | Source value |
| --- | --- |
| WorldChrMan pointer RVA | 0x3D69FF8 |
| Player pointer within WorldChrMan | 0x1E508 |
| Player handle | +0x8 |
| Player modules / ChrData module | +0x190 then +0x0 |
| HP / max HP | ChrData +0x138 / +0x13C |
| FP / max FP | ChrData +0x148 / +0x14C |
| Stamina / max stamina | ChrData +0x154 / +0x158 |
| MenuMan pointer RVA | 0x3D6F820 |
| IsLoaded / IsFading | MenuMan +0x94 / +0x96 |
| Map identifier / coordinates | Player +0x6D0 / +0x6C0 |
| Debug flag storage RVA | 0x3D6A210 |
| Player no-death / infinite FP / infinite stamina | Debug storage +0 / +5 / +4 |

These are documented source facts, not a claim of local gameplay verification. Runtime reads require the exact executable hash and validate pointer readability and value ranges. Resource numbers appear only after a living, loaded character passes validation. Missing or invalid data is displayed as unavailable, never synthesized.

## Beta capability policy â€” 2026-09-07

The exact executable SHA256 remains required. Unknown executables receive no gameplay access. The current build still does not detect session mode or dispatch on a verified engine task. Unlike 0.1, 0.2 allows the user to explicitly arm experimental offline testing. This intentionally differs from the proposed document's strict unknown-session lock: the UI labels it as a declaration, never proof of offline state. Users must not take an armed session online. In 0.2.2, observed death/loading or identity change stops temporary modifiers but retains the user declaration until unchecked, Disable All, or process restart.

Commands execute on the existing 64 ms worker, not in the rendering callback. Pointer/handle/map/module identity is sampled again before actions. These checks reject stale requests but cannot eliminate races with game threads. Engine calls and memory changes require live verification. Function headers are compared against the local fingerprinted executable before item/rune calls; this detects mismatches, not incorrect semantics.

### Additional sourced adapters

All values refer to the same pinned Version2_7_0 source. Behavior references include PlayerService.cs, ChrInsService.cs, and embedded ItemSpawn/GiveRunes assembly in Resources.resx.

| Meaning | Source value |
| --- | --- |
| HP damage protection | ChrData +0x19B, mask 0x02 |
| Resistance module | Modules +0x20 |
| Seven buildup / maximum values | Resistance +0x10 / +0x2C, stride 4 |
| Nearby entity pointer range | WorldChrMan +0x1F1B8 / +0x1F1C0 |
| Poise module / current / maximum | Modules +0x40 then +0x10 / +0x14 |
| GameDataMan RVA / player data pointer | 0x3D61F98 then +0x8 |
| Attributes / level / runes | Player data +0x3C / +0x68 / +0x6C |
| Physics module / local position | Modules +0x68 then +0x70 |
| Ride module / node / mounted state | Modules +0xE8 then +0x10 then +0x50 |
| Map orientation | Player +0x6CC |
| Flipper RVA / simulation speed | 0x458DB58 then +0x2CC |
| MapItemMan RVA | 0x3D6BAC0 |
| Item grant / quantity function RVA | 0x561400 / 0x785E50 |
| GiveRunes function RVA | 0x25E0E0 |

Catalog generation rejects conflicting duplicates and omits quest/key/cut resource lists. DLC entries and weapons with unclassified content remain browse-only. In 0.2.2, 377 base-game weapons/shields/casting tools are grantable at their exact base ID (+0, default affinity/skill). Consumables/spirit ashes are browse-only because eligibility/upgrade handling is unresolved. Eligible grants enforce finite integral quantities and source stack limits for stackable entries. A weapon stack size of one describes each instance; weapon grants add one per action and permit duplicate copies (ownership must be readable and below a sanity bound of 9,999). Actual full-inventory behavior remains an engine concern and is not live verified. Ownership is queried for the selected item at most four times per second while armed and checked again before a grant. Rune balance is also checked immediately before invocation. Native results are logged; no automatic retry occurs.

Owned debug flags restore only if their values remain unchanged. HP protection restores its bit while preserving unrelated bits. Simulation restoration requires the same manager pointer. Character-owned values never restore across a rejected generation. Conflicts are reported; there is no forced overwrite. Refills, buildup clear and position return are one-shot changes and cannot be rolled back by Disable All.

### Input change

Process-local MinHook detours suppress SetCursorPos, SetPhysicalCursorPos, and ClipCursor requests while the foreground menu owns input. Acquisition releases the existing clip; ordinary close restores it, while focus loss leaves the cursor unconfined. See the Windows [SetCursorPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcursorpos) and [ClipCursor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-clipcursor) contracts. Win32 messages are queued to ImGui outside the event mutex. Raw Input and the standard DirectInput mouse implementation are neutralized for the game while captured. XInput 1.4 supplies original state to the menu. Other input implementations, physical controllers, fullscreen/borderless and Alt+Tab require live checks.

### Remaining integration work

- Ground, combat/cutscene and automatic session detection are absent. Same-map return is experimental; session bookmarks are not saved to disk.
- Damage routing and eligible-consumable predicates are unimplemented. Broad global trainer hooks were not substituted for player-only behavior.
- Boss reset/phase transitions need individual encounter procedures. No arbitrary progression flags are edited.
- Camera/HUD, world time, free flight and native attribute recalculation and valid weapon upgrade workflows remain unimplemented.
- Graphics retain the primary swapchain design. HDR changes, swapchain replacement and device recovery are not established. Rendering failure releases input and requests restart.
- The full design's acceptance criteria remain incomplete. Build and component tests do not establish gameplay correctness, absence of input leaks, or save compatibility.

## Attribute editor â€” 0.2.1 beta

`PlayerService.cs` lines 514-543 and 583-591 supply the stat/level/rune-history editing behavior and cost formula. `Offsets.cs` lines 658-674 define eight 32-bit attributes at player data +0x3C through +0x58, level at +0x68 and unsigned rune history at +0x70. No new guessed function pointer or offset was added.

The toolkit plans all requested attributes together, computes new level as old level plus net attribute change, and applies history increases only for net gained levels. Same-total reallocations keep level/history unchanged; decreases never erase history. Rune history saturates at UINT32_MAX. Spendable runes (+0x6C) and intervening unknown fields are not written.

The preview validates 1-99 attributes, 1-713 resulting level, current attribute/level/history equality and generation. Runtime rechecks loaded/living character identity and expected intermediate values. Only changed fields are written, then read back. Failure attempts reverse-order restoration only when the same character and the last applied value still match. A partial unknown write or concurrent conflict produces an explicit incomplete result rather than overwriting it. The request audit must be written before any mutation.

This extends the pinned trainer's direct data method into a checked batch. It is not a native rebirth workflow, not atomic to the engine, and not crash-proof. Starting-class minima are not enforced. Derived-stat recalculation is unverified; users should reload the character and verify the result. No live attribute editing was performed by the agent.

## Weapon grants and offline confirmation — 0.2.2

The earlier generator set every Weapons entry to unclassified and nongrantable. Character stats were unrelated to that restriction. `provenance/weapon-content.json` records only factual numeric ID membership from Erd-Tools commit `a2757f930afa3109dc4816d3f04e7cd11b91fea1`, including exact source paths and SHA256 values. The source separates Resources/Items/Weapons from Resources/Items/DLC/DLCWeapons; Resources/ItemCategories.txt corroborates these categories. The generator intersects this membership with the existing MIT TarnishedTool catalog. Unknown IDs stay blocked. No Erd-Tools implementation or descriptions are copied.

The native request uses the same pinned TarnishedTool ItemViewModel.SpawnSingleItem / AutoSpawnWeapon and ItemService path: exact base weapon ID, one copy, default Ash of War sentinel -1. No guessed upgrades, affinity offsets or IDs are synthesized. UI and runtime share `canGrant`; ownership is freshly checked before calling the engine. Weapon counts are per instance, so an existing copy no longer blocks another. Readback is observed, not proof of full inventory or save semantics.

The offline checkbox is a process-lifetime declaration, separate from temporary effects. `clearOverrides` restores owned effects and calls `resetTemporaryState` without erasing it. Loading, death, character/map changes, a failed action and a successful attribute edit stop temporary modifiers. Character generations still invalidate old commands and bookmarks; unavailable/unknown executables and unready characters still block gameplay. Turning the checkbox off works during loading. Unchecking, Disable All, or restarting clears the declaration; it is never saved to settings/profiles. Automatic online detection remains absent.

## Checkbox acknowledgement — 0.2.3

The 0.2.2 checkbox still required `ready` to turn on and sent a character-generation-bound command to the 64 ms worker. A generation/readiness change between the click and execution could reject confirmation; rendering a fresh snapshot before execution also displayed the old value. These are source-identified failure paths, not a live reproduction of the user's symptom.

`queueCommand` now acknowledges ArmOffline under the state mutex before enqueue returns. The declaration is independent of executable, loaded-character state and generation; actual gameplay commands retain all fingerprint, known-online, adapter, readiness and generation checks. Home shows the waiting reason separately. An offline launcher is not automatic proof of session mode.

Unchecking clears pending gameplay and schedules owned-modifier restoration ahead of new actions, even if the queue was full. Checking does not enqueue an arming command. Disable All clears the declaration synchronously and replaces queued actions with restoration. Worker restoration does not rewrite the checkbox, so rapid off/on or Disable All/confirm ordering retains the latest intent. The declaration is process-only and is never saved in profiles/settings.

## Supported September update — 0.2.4

The current installed executable was inspected on 2026-09-09: file/product version **2.7.1.0**, Steam build **25080141**, **87,042,128 bytes**, SHA256 `1A3547101327F65D0C76DA2F9190AC0AA66871EA42BAE2AECC61E11A8B597891`. Its file timestamp is 2026-09-08 17:47 local. The previous toolkit matched only the original 2.7.0 hash, so it stopped before player reads. Relaunching could not satisfy that fingerprint check.

[TarnishedTool commit 8df23f1b19c62997ba9e3822495c1d2bd001ab88](https://github.com/borgCode/TarnishedTool/commit/8df23f1b19c62997ba9e3822495c1d2bd001ab88) adds file version 2.7.1 support (public patch 1.17.1). Its unmodified `Offsets.cs` snapshot is preserved as `provenance/Offsets-2.7.1.cs`, under the existing MIT license. Every RVA used here has an explicit shared Version2_7_0/Version2_7_1 switch arm: WorldChrMan, MenuMan, ChrDbgFlags, GameDataMan, CSFlipperImp, MapItemManImpl, ItemSpawn, GetPlayerItemQuantityById and GiveRunes. Internal player/module fields used by this mod are unchanged in that source. No address is inferred from the file version alone.

GenerateProfile.py checks these nine source arms and accepts only the two exact executable hashes. All three native function headers were read from the new PE executable sections and match the existing 16-byte guards byte-for-byte. The runtime still rechecks them in memory. This is static/source verification, not a live engine-call test or save-compatibility guarantee.

Home now distinguishes unsupported executable, missing world/player/modules/resource/map pointers, unreadable loading state, incomplete loading flag, active fade flags, invalid HP/FP/stamina and zero HP/respawn. These changes are logged when the diagnostic text changes. Checking offline mode never bypasses a failed read/profile check. The existing fade and loaded conditions remain unchanged.

## Infinite Torrent jumps — 0.2.5

The reference is the `Infinite Player / Horse Jumps` table entry in [Hexinton's repository](https://github.com/Hexinton/eldenringcheatengine/tree/e2c2d20ad92542d46dce41c9faf183dbd6f18f3d). Only factual instruction signatures/sites were used; no trainer implementation or on-foot jump patch is redistributed. The old trainer's two horse signatures each match exactly once in executable sections of the current 2.7.1.0 PE. The confirmed RVAs are **0x475E1C** (`mov [rdi+0x134],al`) and **0x475E4F** (`mov [rdi+0x135],al`). The ride update at 0x475C00 copies its module argument into RDI; its owner getter at 0x43D250 returns `[rcx+8]`. These were inspected directly in the installed executable. Exact bytes, signatures, archive/commit identities and the executable hash are recorded in `provenance/horse-jump-2.7.1.json`.

The new independent assembly replaces each flag write with constant 1 only for the published local ride object. At each invocation it verifies the ride owner, player handle, current WorldChrMan player, mounted state and MenuMan loaded/fade values. All registers and incoming flags are preserved; the original MinHook trampoline is used when any guard fails. There is no guessed function signature or external game call. The compiled GNU x64 assembly implementation is enabled only for the exact 2.7.1.0 hash and both full instruction contexts; other executable/compiler profiles leave the feature unavailable.

Hooks install with the target disabled and remain pass-through for process lifetime when the feature is off. Disabling revokes the target; the game computes normal flags at its next update rather than restoring stale data bytes. A 64 ms worker publishes the selected ride while enabled and clears it on dismount/invalidity. Additional live checks occur in the engine's existing flag-write path, but this does not establish a general game-thread dispatcher for other actions. Mid-function detours preserve machine state but have no custom exception-unwind metadata; native compatibility, asynchronous exception behavior and interactions with other mods remain unverified in-game.

The toggle is independent of persistent actions and is omitted from stored profiles. Applying a profile, loading, character changes and Disable All stop it. No health, gravity, teleport, summon restriction, fall damage or death-zone behavior is changed. The isolated native fixture executes the actual assembly and MinHook pair in a test executable without opening the game or renderer host.
