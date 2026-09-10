# Verification report — 2026-09-06

## Build and automated validation

The final source was built on Windows x64 with MSYS2 UCRT64 GCC 14.2.0 using `Build.ps1`. The build completed successfully and produced the DLL and both test executables. The DLL uses statically linked GCC/C++ runtimes; import inspection found system Windows/DirectX/UCRT libraries, with no external GCC runtime DLL dependency.

`core_tests.exe`: **23 checks passed**, covering unknown builds, unknown/online sessions, unsafe execution context, valid commands, stale generations, invalid resources, unknown commands, emergency disable, settings parsing and corruption, ownership restoration, external value conflicts and stale-entity restoration rejection. These tests exercise the validation components with a small test-memory object; they are not proof of gameplay behavior.

PowerShell parsing checks passed for Build, Install and Uninstall scripts. Actual install/uninstall execution was tested while the game was closed. The original loader configuration and original Elden Menu DLL hashes matched before and after the round trip. The final DLL was then installed again for the game test.

CMake build, other compilers, other computers and other executable hashes were not tested.

## DirectX 12 integration test host

The separate `renderer_test.exe` is explicitly titled **RENDERER TEST ONLY - not Elden Ring**. It creates a real D3D12 swapchain and loads the production DLL. Its executable hash is rejected by the gameplay adapter; it supplies no fabricated HP, FP or stamina values.

Observed:

- Swapchain and exact queue capture; overlay rendering.
- Window resize callbacks and buffer recreation.
- Mouse navigation into Practice.
- Manual Start/Stop: a 32.6308-second attempt was displayed and written to CSV.
- Insert closed and reopened the menu; the test window remained responsive.
- An input reentrancy freeze was found and corrected: queued Win32 messages are now moved out of the event mutex before invoking ImGui, because mouse capture can re-enter WndProc.
- A smaller-window placement issue was corrected by clamping size and position to the viewport. The final style was confirmed visually in the real game.

The initial test record was renamed to `initial-renderer-test-attempts.csv` in the toolkit data directory. Future graphics-test attempts write `renderer-test-attempts.csv`, keeping them separate from game attempts.

## Actual Elden Ring test

The existing offline launcher was used. The game loaded the installed `TarnishedToolkit.dll` through the existing Elden Mod Loader. Local log evidence:

```text
EldenModLoader > Load delay: 0
EldenModLoader > Loading TarnishedToolkit.dll...
EldenModLoader > Loaded 1 .dll mods

Loaded. Executable SHA256: d1a84083c6c7c7902162ff098f7d86812839aa6b3575959398857e539c488134
Build matched. Session and game-thread validation pending; gameplay writes locked.
Captured D3D12 swapchain creation and its command queue.
D3D12 overlay initialized on game's exact swapchain and command queue.
```

The overlay was **visually observed on Elden Ring's title screen**, with the correct fingerprint status and unavailable character resources. The game process was responsive. The original Elden Menu was preserved under a disabled filename and was not loaded alongside the toolkit.

No loaded-character test or resource mutation test was performed. Game-window automation yielded when user input was detected; it did not attempt to take over the user's subsequent interaction. A game connection-error dialog appeared during title-screen activity; this does not validate the toolkit's session detector, which remains Unknown.

## Not verified / not delivered

- Live HP/FP/stamina correctness on a loaded character; map coordinates and lifecycle during death/loading/cutscenes.
- Offline-session detection and safe game-thread dispatch. Both capability gates remain closed in the production DLL.
- Resource refill and independent HP/FP/stamina gameplay effects. Their controls are disabled.
- Complete keyboard/raw-input/DirectInput and physical controller isolation, disconnection recovery or opening chords.
- Alt+Tab recovery under gameplay, HDR format changes, swapchain replacement, device-loss recovery, and extended stress testing.
- Persistent item, rune, attribute or progression edits; boss reset adapters; teleport, camera and combat multipliers.
- UI/GPU overhead benchmark. No sub-millisecond or universal performance claim is made. The discovery renderer conservatively waits for the previous overlay submission before reusing graphics data.

**Milestone 1 from the supplied design remains incomplete.** The installed artifact is an integration discovery release with a real in-game overlay and a functional manual practice tool.
