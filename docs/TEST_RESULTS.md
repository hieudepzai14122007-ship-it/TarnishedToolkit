# Verification report — 0.2.5 Infinite Torrent jumps

Built and installed with the game closed on 2026-09-09. DLL: 4,878,342 bytes, SHA256 `d56d4f6b5a0cd3d08eed258ed1d40e02028a088e53d1feb5fb950eb06d91c1f7`. The updater preserved the previous DLL and original loader/menu backups.

**205 checks passed:** 124 core/input/catalog/queue/profile checks, 23 storage checks, 36 attribute checks and 22 new native horse-hook checks. The new native fixture uses the actual compiled assembly detours and MinHook inside a separate component-test process. It verifies both flag overrides, original behavior while disabled, original RAX and arithmetic flags, other ride rejection, live dismount/loading/fade checks, stale handles, world-player changes, null owner/ride state/world/menu, repeated toggles and removal. Core tests cover supported/mounted/offline requirements, always-available disabling and temporary-reset behavior.

The final component programs were rerun after the interrupted task resumed; all passed. The DLL and all test programs, including renderer_test.exe, are compiled; no source file was newer than the built DLL before installation. Neither Elden Ring nor the graphics host was launched.

The two reference instruction patterns each occur once in executable sections of the exact 2.7.1.0 game (SHA256 `1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891`). The native instruction sites, guard context bytes and ride-owner getter were disassembled and recorded in provenance/horse-jump-2.7.1.json. The original implementation affects only the published local ride and does not include the reference trainer's on-foot patch.

**Live testing remains required:** enable Player > Torrent > Infinite Torrent double jumps while mounted, close the menu and try repeated jump presses. Check normal jumping after disabling/dismounting/loading and interactions with other mods. No jump-height, save or performance claim is based on a live game test. Fall damage and death zones remain unchanged. This feature is currently unavailable on the older 2.7.0.0 profile and on non-GNU-x64 builds.

Earlier release reports below are historical.

---

# Verification report — 0.2.4 game update / player data

Built and installed with the game closed on 2026-09-09. DLL: 4,872,588 bytes, SHA256 `a0079b8886effc4643eb7bb7a04405f7fcf5efd64e2b04738be45533cea2ed1c`. Build, installed DLL and installation record hashes match; prior DLL and original loader/menu backups are preserved.

The installed game now has file/product version 2.7.1.0, Steam build 25080141 and SHA256 `1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891`. Version 0.2.3 accepted only the previous 2.7.0.0 hash, which prevented player-data reads before pointer sampling. This disk/source mismatch explains why relaunching did not resolve the reader failure. No live process was inspected or launched.

**178 component checks passed in the final complete Build.ps1 run:** 119 core/input/catalog/queue/profile checks, 23 storage checks and 36 attribute checks. New profile checks cover both exact hashes, rejection of a version string alone, empty/mutated hashes and prefix matches. Existing gameplay readiness, generation and offline tests remain in force. Home diagnostics were source-reviewed for unsupported build, pointer stages, loading/fade flags, resource validation and respawn. These diagnostic branches have not been live exercised.

Static provenance validation confirmed all nine used RVAs have explicit shared Version2_7_0/Version2_7_1 arms in MIT TarnishedTool commit 8df23f1b19c62997ba9e3822495c1d2bd001ab88. Internal fields used by the toolkit are unchanged in that source. All three 16-byte native entry-point guards match the current executable's executable PE sections, and GenerateProfile.py reproduces engine_headers.inc byte-for-byte. The source snapshot and profile audit are in provenance/.

The first build attempt passed core checks but storage-test cleanup raised Access Denied when removing its empty temporary directory, after its functional operations. A standalone storage retry and then a complete clean invocation of Build.ps1 both passed; no source change was made to bypass that check. An empty storage-tests-3344 scratch directory may remain under build.

Elden Ring and the graphics host were not launched. Gameplay reads, native calls, saves and player-reader recovery remain for user testing. Unknown executables are still rejected; file version alone never enables memory access. Earlier reports below remain historical.

---

# Verification report — 0.2.3 offline checkbox acknowledgement

Built and installed with the game closed on 2026-09-08. DLL: 4,867,175 bytes, SHA256 `435A6ABFCFCCF21690A0CD160E7635DC61F4F0B2C4F7D2C7F823CFE81964B00C`. Build, installed DLL and installation record hashes match. The updater preserved the previous DLL and original loader/menu backups.

**173 component checks passed:** 114 core/input/catalog/queue checks, 23 storage checks and 36 attribute checks. New queue checks cover immediate acknowledgement before polling, unknown-executable/unready/known-online gameplay rejection despite confirmation, gameplay generation validation, bounded ordinary queue, unchecking a full queue cancelling pending actions, rapid off/on preserving owned restoration, Disable All priority and immediate revocation, and rechecking before restoration completes. Existing arming tests now assert declaration independence from readiness/generation.

The user clarified the symptom as a greyed-out or unclickable checkbox. Source review found Home disabled it while character readiness was false, and confirmation was separately rejected when readiness/generation changed before worker execution. The disabled UI wrapper is removed and the declaration is acknowledged under the state mutex immediately. Gameplay commands retain their existing guards. Home displays a separate waiting reason.

Build.ps1 completed without launching Elden Ring or the graphics host. No live reproduction or verification was performed; the user will verify mouse activation, loading, respawn and rapid toggles. Offline mode is still a user declaration, not automatic session detection. Reports below describe earlier releases.

---

# Verification report — 0.2.2 weapon and offline-confirmation fixes

Built and installed with the game closed on 2026-09-07. DLL: 4,866,663 bytes, SHA256 `8AB76E1034390BF766B9DF3EEE802A9A6F91473687D43E63331EDA19368D7DFE`. Installed DLL and installation record match; the updater preserved the previous DLL and original loader/menu backups.

**161 checks passed:** 102 core/input/catalog checks, 23 storage checks and 36 attribute checks. New coverage verifies base/DLC/unclassified weapon eligibility, duplicate weapon copies, one-at-a-time limits, invalid amounts/unreadable ownership, the full 377-weapon eligibility count and all other eligible stack limits. It also verifies temporary-state clearing retains the offline declaration across repeated resets, cannot arm a new process, blocks gameplay while loading and permits unchecking during loading. Existing stale-preview/generation, known-online rejection and attribute transaction tests still pass.

`Build.ps1` completed: DLL, three component test programs, and graphics host compiled. Catalog regeneration is deterministic. `Update.ps1` successfully installed this build. No Elden Ring or graphics-host launch occurred; weapon delivery, full-inventory behavior, live respawn/loading and save persistence remain for user testing. Weapon upgrades, affinity editing, DLC grants and automatic session detection remain unimplemented.

The reports below describe earlier binaries and earlier offline-checkbox behavior.

---

# Verification report â€” 0.2.1 attribute editor

Built and installed on 2026-09-07. Final DLL: 4,865,249 bytes, SHA256 `388EFBC1E1C39A2B068238EC24E35AF8C47F974DFF6A37E1CD75E024BF8B9169`.

**147 checks passed:** 88 existing core/input/catalog checks, 23 storage checks, and 36 new attribute checks. Attribute coverage includes bounds, level/history planning, formula boundaries, saturation, equal-total reallocations, reductions, exact changed-field writes, untouched spendable runes, every write-failure stage, stale previews, concurrent modifications, partial failures, character invalidation, and command confirmation/capability/offline requirements.

The build script passed syntax validation. Update.ps1 installed the DLL with the game closed, preserved the prior DLL and original loader/menu backups, and the installed DLL hash matched the build and installation record. The source distribution includes attribute_tests.exe.

**No game or graphics-host launch, live stat change, save edit, or derived-stat verification was performed.** Users must validate on a backed-up test character. Reload before checking derived HP/FP, defense and damage. The editor uses an experimental ordered data transaction, not an atomic engine respec callback. Starting-class minimums and automatic recalculation are not verified; incomplete rollback is explicitly reported.

The earlier 0.2 report below is historical and applies to that earlier binary.

---

# Verification report â€” 0.2 beta, 2026-09-07

## Build and component tests

Built with Windows x64 / MSYS2 UCRT64 GCC 14.2.0 using Build.ps1. The final DLL is 4,847,000 bytes, SHA256 `264E0DD214757257201B48DACC20B771E984B921B9170F130ACFF424FF79A2EE`.

- **88 command, ownership, input-policy and catalog checks passed.** Coverage includes unknown/online states, explicit offline declaration, stale character/map requests, invalid resource/status/speed values, confirmed and stale item/rune previews, rune cap, bookmark identity/distance, corrupt profiles, byte/bit/float ownership conflicts, focus loss and repeated cursor transitions, exact catalog/material-pack eligibility, schema migration and controller hold/release/disconnection policy.
- **23 isolated storage checks passed.** Actual profile/favorite/plan save/load, replacements, corrupted on-disk data, path/ID/quantity rejection, session bookmark behavior and successful atomic writes were exercised in a temporary directory beside the test executable. No game or user settings were accessed.
- Both generated adapter files reproduced byte-for-byte from the pinned source and exact executable.
- Build, Install, Update and Uninstall scripts passed PowerShell parsing checks. The updater was executed with the game closed; DLL, installation record, original menu backup and loader configuration hashes matched after the update.
- The updater's initial replacement call exposed PowerShell converting a null backup argument to an empty path. That call failed before replacement; the script preserved the installed build and cleaned staging files. It was corrected to use explicit temporary backup paths, then the update succeeded.
- DLL import inspection found Windows/DirectX/UCRT imports and statically linked GCC/C++ runtimes. The project also builds storage_tests.exe, core_tests.exe and renderer_test.exe.

CMake, alternate compilers and other executable versions were not tested.

## In-game testing

**Neither Elden Ring nor renderer_test.exe was launched during this update**, as requested. The user reported the cursor being held at screen center in the previous release. The new detours and input policy address recentering and confinement, but successful mouse operation has not yet been observed in-game.

The prior 0.1 title-screen/renderer observations are preserved separately in TEST_RESULTS_0.1.md. They do not validate the new beta features.

## User validation still needed

1. Mouse navigation, repeated Insert toggles, Escape dialog cancellation, Alt+Tab, resolution changes and controller disconnection.
2. Optional controller hold shortcut, its interaction with the user's game bindings, and category navigation.
3. Live values, one-shot refill, each resource flag, seven buildup controls, selected-entity HP/poise and Disable All restoration.
4. Item ownership, a single eligible item grant and small rune addition on a backed-up disposable character; inspect the before/after log. No persistent actions were performed by the agent.
5. Simulation speed, same-map bookmark return, and automatic disarming during death/loading/character/map changes. Ground, combat and cutscene detection remain absent.
6. Long-running behavior, save compatibility and CPU/GPU overhead. No performance target is claimed as measured.

The runtime remains an experimental worker adapter without a verified game-thread task callback or automatic session detection. The complete proposed feature catalog and acceptance criteria are not finished. See FEATURES.md for implemented controls versus missing integrations.
