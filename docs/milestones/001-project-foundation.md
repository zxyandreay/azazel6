# Milestone 001 — project foundation and recompilation feasibility

**Date:** 2026-10-17. **Outcome:** functioning foundation and a bounded native entry
experiment; no bootable or playable Tekken application.

## Delivered

- [x] Source tree, MIT license, third-party notice, ignore rules, CMake and agent guide.
- [x] Exact pinned external framework with reproducible sparse fetch/build/test commands.
- [x] ULUS10466 profile and strict ELF/hash/guest-offset validation.
- [x] Sectionless PSP module discovery, GP and import inventory.
- [x] Automated analyzer/generator workflow and local failure logs/reports.
- [x] Real executable analysis and full exploratory C++ generation.
- [x] Generated entry slice, native loader/memory/HLE boundaries and bounded runner.
- [x] Android ARM64 framework/math/entry cross-build.
- [x] Tested semantic HUD math and source-grounded ultrawide/moveset migration plans.
- [x] Technical docs, ADR, roadmap, progress and game-free CI.

Repository destination is `https://github.com/zxyandreay/azazel6`, branch `main`.
The final delivery report records the actual pushed SHA and remote verification;
this document does not embed its own future commit hash.

## Executed evidence

Windows x64, MSVC 19.43.34809, CMake 4.0.3, Python 3.14.4; Android NDK r27b /
27.1.12297006, Clang 18.0.2, ARM64 API 26. All 13 Python cases, one Azazel6 C++
contract test and three framework suites passed. Native integrated build and
Android linking succeeded. The Windows restricted process sandbox initially stalled
MSBuild; authorized native execution resolved it. Existing upstream compiler
warnings were retained; no framework source was patched.

The exact supplied hash matched. Analysis found 10,072 candidate functions and
generated 1,015 exploratory units. The section-based upstream scanner missed the
module; the profile reader recovered 268 imports. A 52-byte translated entry slice
executed 13 instructions and stopped at guest `0x08B88DD0`, import
`SysMemUserForUser:0x91DE343C`, with a0 `0x05050010` and RA `0x08804158`.
See [the research record](../research/executable.md) for counts and limitations.

Raw local evidence includes `local/analysis/tekken6/` (full first generation),
`local/analysis/tekken6-module/` (profile-enriched analysis), `local/entry-probe/`,
`local/iso-inventory.txt`, native build logs and Android build logs. These are
intentionally not committed. Recreate them with [the public procedures](../development/building.md).

## Limits and next task

No complete game corpus compilation, title screen, renderer, audio, actual Android
device run, APK, sanitizer pass, mod activation or performance comparison occurred.
Code/data separation and import-aware full generation are incomplete. The ELF's
32 MiB fit is not a gameplay/mod memory requirement. ISO PRXs are present; game
overlays and runtime code generation remain unresolved.

The following next-task record is historical; its first-import checkpoint is now
implemented in [Milestone 002](002-native-bring-up.md).

**Highest-priority Milestone 002 task at foundation completion:** establish and implement the first reached
PSP import contract, then extend verified AOT entry coverage through its return.
Use authoritative library/NID evidence, tests for return/state semantics, and a
bounded real-input trace. Prerequisites are the same verified EBOOT, pinned native
tools and C++ compiler already used here. Do not replace missing services with
blanket successful stubs.

Resume with `python tools/run_probe.py local/game/EBOOT.BIN --binary
local/build/probe/profiles/tekken6/Release/azazel6_entry_probe.exe` after following
the [entry build procedure](../development/building.md). The foundation stop was
the expected baseline, not a failure to be suppressed.
