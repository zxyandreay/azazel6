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
| Python/profile tools | VERIFIED, 47 cases passed |
| Semantic aspect math | VERIFIED native contracts passed |
| Startup HLE / returning imports | VERIFIED synthetic contracts; thirteen integrated CTest suites passed |
| Thread lifecycle | VERIFIED create/start/exit/delete, copied arguments and service-boundary priority scheduling and strict-priority changes; VFPU attribute addition/removal |
| Mutex lifecycle | VERIFIED creation, recursion, ownership, deletion and owner exit; waits/timeouts remain rejecting |
| Virtual timers | VERIFIED inactive object creation/deletion; active clocks and callbacks remain unimplemented |
| Callbacks | VERIFIED inactive ownership, deletion and exit registration; delivery remains unimplemented |
| Data-cache ranges | VERIFIED checked coherent startup RAM operations; code/alias/large-range requests reject |
| Shared guest memory | VERIFIED non-overlapping stack/heap ownership, allocation/free/alignment |
| Fixed pools | VERIFIED single-block creation, immediate allocation, free/delete and shared ownership; multi-block/waiting services remain rejecting |
| Language settings | VERIFIED explicit English/Cross defaults and checked runtime-local get/set; no host locale dependency |
| Event flags | VERIFIED create/delete, set/clear, polling and immediately satisfied waits; queued waits/timeouts remain rejecting |
| Graphics memory | VERIFIED immutable EDRAM size/base queries backed by allocated guest VRAM; GE and display remain unimplemented |
| ULUS10466 validation | VERIFIED exact local hash/ELF match |
| Module and imports | VERIFIED GP and 268 imports in 30 libraries |
| Full exploratory generation | VERIFIED 1,015 local units; not fully compiled |
| Native entry execution | VERIFIED constructors, exit registration, 2048-record loop, fixed-pool allocation, language get/set and event creation; two workers Ready and EDRAM size query complete; graphics setup helper coverage is the next boundary after 16788 dispatches |
| Exception boundaries | VERIFIED 37 generated BREAK sites retain terminal stops; no exception delivery or syscall fallback |
| Android ARM64 build | VERIFIED framework, math, startup HLE and expanded entry probe compiled/linked |
| Linux execution | VERIFIED startup-object, fixed-pool, language and event-flag checkpoints passed hosted CI |
| Android device/APK | NOT TESTED / not implemented |
| Game boot/combat/graphics/audio/input | Not implemented |
| Ultrawide gameplay hooks / moveset packages | PLANNED; reference research only |

Supported game-input identity is limited to the exact USA decrypted baseline.
No other region, patched executable or runtime memory layout is accepted. Runtime
module loading, full HLE, code/data classification and correct bootstrap semantics
remain the main bring-up dependencies. Native performance has not been compared
with PPSSPP, and Jin8 freezes are not fixed.
