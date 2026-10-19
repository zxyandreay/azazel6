# Progress and compatibility

**2026-10-19 — Milestone 002 implemented; Milestone 003 in progress.** Azazel6 is not playable. See
[current checkpoint](../milestones/003-boot-sequence.md),
[foundation evidence](../milestones/001-project-foundation.md) and
[next milestones](roadmap.md).

| Area | Status |
| --- | --- |
| Repository source, licensing, docs, CI | Implemented |
| Locked framework fetch | VERIFIED reproducible sparse checkout |
| Windows framework build/regressions | VERIFIED, three suites passed |
| Python/profile tools | VERIFIED, 39 cases passed |
| Semantic aspect math | VERIFIED native contracts passed |
| Startup HLE / returning imports | VERIFIED synthetic contracts; nine integrated CTest suites passed |
| Thread lifecycle | VERIFIED create/start/exit/delete, copied arguments and service-boundary priority scheduling and strict-priority changes; VFPU attribute addition/removal |
| Mutex lifecycle | VERIFIED creation, recursion, ownership, deletion and owner exit; waits/timeouts remain rejecting |
| Virtual timers | VERIFIED inactive object creation/deletion; active clocks and callbacks remain unimplemented |
| Callbacks | VERIFIED inactive ownership, deletion and exit registration; delivery remains unimplemented |
| Data-cache ranges | VERIFIED checked coherent startup RAM operations; code/alias/large-range requests reject |
| Shared guest memory | VERIFIED non-overlapping stack/heap ownership, allocation/free/alignment |
| ULUS10466 validation | VERIFIED exact local hash/ELF match |
| Module and imports | VERIFIED GP and 268 imports in 30 libraries |
| Full exploratory generation | VERIFIED 1,015 local units; not fully compiled |
| Native entry execution | VERIFIED bootstrap returns and child entry executes; partition/module lookup and nested interrupt state completed; all 59 constructors return within 3063 dispatches; exit callback creation/registration completed; next helper 0x0889DB64 remains uncovered |
| Android ARM64 build | VERIFIED framework, math, startup HLE and expanded entry probe compiled/linked |
| Linux execution | VERIFIED preceding formatter, priority-query and inactive-timer checkpoints passed hosted CI |
| Android device/APK | NOT TESTED / not implemented |
| Game boot/combat/graphics/audio/input | Not implemented |
| Ultrawide gameplay hooks / moveset packages | PLANNED; reference research only |

Supported game-input identity is limited to the exact USA decrypted baseline.
No other region, patched executable or runtime memory layout is accepted. Runtime
module loading, full HLE, code/data classification and correct bootstrap semantics
remain the main bring-up dependencies. Native performance has not been compared
with PPSSPP, and Jin8 freezes are not fixed.
