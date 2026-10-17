# Progress and compatibility

**2026-10-18 — Milestone 002 implemented; Milestone 003 in progress.** Azazel6 is not playable. See
[current checkpoint](../milestones/002-native-bring-up.md),
[foundation evidence](../milestones/001-project-foundation.md) and
[next milestones](roadmap.md).

| Area | Status |
| --- | --- |
| Repository source, licensing, docs, CI | Implemented |
| Locked framework fetch | VERIFIED reproducible sparse checkout |
| Windows framework build/regressions | VERIFIED, three suites passed |
| Python/profile tools | VERIFIED, 21 cases passed |
| Semantic aspect math | VERIFIED native contracts passed |
| Startup HLE / returning imports | VERIFIED synthetic contracts; eight integrated CTest suites passed |
| Thread lifecycle | VERIFIED create/start/exit/delete, copied arguments and service-boundary priority scheduling |
| Shared guest memory | VERIFIED non-overlapping stack/heap ownership, allocation/free/alignment |
| ULUS10466 validation | VERIFIED exact local hash/ELF match |
| Module and imports | VERIFIED GP and 268 imports in 30 libraries |
| Full exploratory generation | VERIFIED 1,015 local units; not fully compiled |
| Native entry execution | VERIFIED bootstrap returns and child entry executes; partition allocation and module lookup completed; missing interrupt suspension at 0x08B88D38 |
| Android ARM64 build | VERIFIED framework, math, startup HLE and expanded entry probe compiled/linked |
| Linux execution | VERIFIED preceding thread and memory checkpoints passed hosted CI |
| Android device/APK | NOT TESTED / not implemented |
| Game boot/combat/graphics/audio/input | Not implemented |
| Ultrawide gameplay hooks / moveset packages | PLANNED; reference research only |

Supported game-input identity is limited to the exact USA decrypted baseline.
No other region, patched executable or runtime memory layout is accepted. Runtime
module loading, full HLE, code/data classification and correct bootstrap semantics
remain the main bring-up dependencies. Native performance has not been compared
with PPSSPP, and Jin8 freezes are not fixed.
