# Progress and compatibility

**2026-10-23: Milestone 002 implemented; Milestones 003 and 004 in progress.**
Azazel6 is not playable. See
[current checkpoint](../milestones/003-boot-sequence.md),
[foundation evidence](../milestones/001-project-foundation.md) and
[next milestones](roadmap.md).

| Area | Status |
| --- | --- |
| Repository source, licensing, docs, CI | Implemented |
| Locked framework fetch | VERIFIED reproducible sparse checkout |
| Windows framework build/regressions | VERIFIED, three suites passed |
| Python/profile tools | VERIFIED, 69 cases passed locally; bounded host registration stack |
| Semantic aspect math | VERIFIED native contracts passed |
| Startup HLE / returning imports | VERIFIED synthetic contracts; eighteen integrated CTest suites passed |
| Thread lifecycle | VERIFIED create/start/exit/delete, copied arguments and service-boundary priority scheduling and strict-priority changes; VFPU attribute addition/removal |
| Mutex lifecycle | VERIFIED creation, recursion, ownership, deletion and owner exit; waits/timeouts remain rejecting |
| Virtual timers | VERIFIED inactive object creation/deletion; active clocks and callbacks remain unimplemented |
| Callbacks | VERIFIED inactive ownership, deletion and exit registration; delivery remains unimplemented |
| Data-cache ranges | VERIFIED checked coherent startup RAM operations; code/alias/large-range requests reject |
| Shared guest memory | VERIFIED non-overlapping stack/heap ownership, allocation/free/alignment |
| Fixed pools | VERIFIED single-block creation, immediate allocation, free/delete and shared ownership; multi-block/waiting services remain rejecting |
| Language settings | VERIFIED explicit English/Cross defaults and checked runtime-local get/set; no host locale dependency |
| Event flags | VERIFIED create/delete, set/clear, polling, queued waits, FIFO wakeups and cancellation; timeouts remain rejecting |
| Graphics memory | VERIFIED immutable EDRAM size/base queries backed by allocated guest VRAM; GE rendering remains unimplemented |
| GE callbacks/control lists | VERIFIED synthetic bounded FIFO/stalls/control flow and guarded SIGNAL/FINISH delivery; real initialization EnQueue/ListSync complete; draws and asynchronous guest submission remain unimplemented |
| VBLANK | VERIFIED real-input registration/enable and four-word handler AOT coverage; synthetic owned delivery and full context restoration |
| Display foundation | VERIFIED synthetic LCD phases, checked framebuffer latches and integrated interrupt/wait ordering; real startup executes two VBLANK handlers and SetMode under bounded idle events; runnable guest-time accounting remains unimplemented |
| ULUS10466 validation | VERIFIED exact local hash/ELF match |
| Module and imports | VERIFIED GP and 268 imports in 30 libraries |
| Full exploratory generation | VERIFIED 1,015 local units; not fully compiled |
| Native entry execution | VERIFIED initialization GE work, SetMode, guarded graphics objects, three texture-state writebacks and graphics memory setup; the 310-root slice returns from the observed method at 0x08AC7358; next guard traps target 0x08AC7A24 at 0x08AC757C; both workers remain Waiting |
| Exception boundaries | VERIFIED 53 generated BREAK sites retain terminal stops; no exception delivery or syscall fallback |
| Android ARM64 build | VERIFIED framework, math, startup HLE and expanded entry probe compiled/linked |
| Linux execution | VERIFIED startup-object, fixed-pool, language and event-flag checkpoints passed hosted CI |
| Android host APK | VERIFIED synthetic SDL/Vulkan probe builds, signs and passes ARM64 and x86_64 ELF / 16 KiB ELF and ZIP alignment checks; no game code |
| Host input | VERIFIED synthetic touch identity, stock coordinate and analog contracts; SDL gamepad/touch adapter compiled, guest input integration remains unimplemented |
| Android virtual device | VERIFIED official emulator API 37 x86_64 / 16 KiB and API 35 x86_64 / 4 KiB: APK installation, Vulkan pixels, touch, virtual controller, navigation overlays, three Home/resume cycles and 30-second advancing-frame intervals; API 35 setup limitations documented |
| Physical Android device | BLOCKED: no physical ARM64 device/controller attached; hardware GPU/input behavior unverified |
| Game boot/combat/graphics/audio/input | Not implemented |
| Ultrawide gameplay hooks / moveset packages | PLANNED; reference research only |

Supported game-input identity is limited to the exact USA decrypted baseline.
No other region, patched executable or runtime memory layout is accepted. Runtime
module loading, full HLE, code/data classification and correct bootstrap semantics
remain the main bring-up dependencies. Native performance has not been compared
with PPSSPP, and Jin8 freezes are not fixed.

The [graphics/input foundation](../research/display-foundation.md) is implemented.
Milestone 003 remains open until title boot is verified; this is an observed
dependency transition, not a claim that boot or rendering is complete.

**2026-10-22 supplemental source checkpoint:** A new bounded, fail-closed
`tools/inspect_root.py` candidate-root inspector and synthetic regression
tests were added for the next direct helper `0x08ACD010`. The tool never
edits `startup.json`, runs recompilation or claims game boot. Python syntax
checks and isolated mock-dependency contract checks were performed in a remote
Linux environment; the full repository suite, exact-EBOOT inspection, native
probe, Android SDK builds and AVD runs were **not available** in that environment.
The previously verified game execution and graphics boundaries above are
unchanged.
