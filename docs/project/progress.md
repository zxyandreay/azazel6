# Progress and compatibility

**2026-10-28: Milestone 002 implemented; Milestones 003 and 004 in progress.**
Azazel6 is not playable. See
[current checkpoint](../milestones/003-boot-sequence.md),
[foundation evidence](../milestones/001-project-foundation.md) and
[next milestones](roadmap.md).

| Area | Status |
| --- | --- |
| Repository source, licensing, docs, CI | Implemented |
| Locked framework fetch | VERIFIED reproducible sparse checkout |
| Windows framework build/regressions | VERIFIED, three suites passed |
| Python/profile tools | VERIFIED, 83 cases passed locally; bounded host registration stack |
| Semantic aspect math | VERIFIED native contracts passed |
| Startup HLE / returning imports | VERIFIED synthetic contracts; twenty-four integrated CTest suites passed |
| Thread lifecycle | VERIFIED create/start/exit/delete, copied arguments and service-boundary priority scheduling and strict-priority changes; VFPU attributes are supported at creation and by later addition/removal |
| Mutex lifecycle | VERIFIED creation, recursion, ownership, deletion and owner exit; source-only lightweight-mutex creation initializes the guest workarea; waits/timeouts and later lightweight-mutex operations remain rejecting |
| Semaphore lifecycle | VERIFIED bounded creation, immediate count consumption and FIFO blocking handoff to the scheduler; signaling, deletion, priority waits and timeouts remain unimplemented |
| Virtual timers | VERIFIED inactive object creation/deletion; active clocks and callbacks remain unimplemented |
| Clock conversion | VERIFIED unsigned immediate and pointer-form microsecond conversion, including null-first output semantics, with atomic live-caller-stack validation; no time advancement |
| System-time query | VERIFIED low and wide microseconds plus current Hcount from the explicit guest timeline, including GE interrupt reads, sub-microsecond truncation and wrap; reads do not advance time; runnable CPU time remains unmodeled |
| Power services | VERIFIED immutable nominal startup PLL/CPU/bus reads at 222/222/111 MHz plus owned callback-slot registration and queued initial `0x10E4` notification; delivery, host clocks, clock transitions and timing rescale remain unimplemented |
| Controller settings | VERIFIED runtime-local idle-cancel thresholds, including signed `-1` and checked 0..128 values; guest pad sampling remains unimplemented |
| Startup device control | VERIFIED source-only `fatms0:` callback ownership registration with checked live-stack input; no host path, media state, capacity, file or callback delivery |
| Startup WLAN identity | VERIFIED one fixed locally administered unicast address copied only to a checked live caller stack; no host networking or connectivity |
| Callbacks | VERIFIED inactive ownership, deletion and exit registration; delivery remains unimplemented |
| Data-cache ranges | VERIFIED checked coherent startup RAM operations; bounded live-partition writebacks through 512 KiB; code/alias/unowned ranges reject |
| Shared guest memory | VERIFIED non-overlapping stack/heap ownership, allocation/free/alignment |
| Fixed pools | VERIFIED single-block creation, immediate allocation, free/delete and shared ownership; multi-block/waiting services remain rejecting |
| Language settings | VERIFIED explicit English/Cross defaults and checked runtime-local get/set; no host locale dependency |
| Event flags | VERIFIED create/delete, set/clear, polling, queued waits, FIFO wakeups and cancellation; timeouts remain rejecting |
| Graphics memory | VERIFIED immutable EDRAM size/base queries backed by allocated guest VRAM; first native VRAM clear verified; general GE rendering remains unimplemented |
| GE callbacks/control lists | VERIFIED synthetic bounded FIFO/stalls/control flow and guarded SIGNAL/FINISH delivery; real initialization EnQueue/ListSync and completed-work DrawSync return; written live-partition submissions/FINISH bridge and bounded RGB565 clear sprites tested; general draws and asynchronous producers remain unimplemented |
| VBLANK | VERIFIED real-input registration/enable and four-word handler AOT coverage; synthetic owned delivery and full context restoration |
| Display foundation | VERIFIED synthetic LCD phases, checked framebuffer latches and integrated interrupt/wait ordering; real startup executes four VBLANK handlers, SetMode and next-edge WaitVblankStartCB with inactive callbacks under bounded idle events; runnable guest-time accounting remains unimplemented |
| ULUS10466 validation | VERIFIED exact local hash/ELF match |
| Reverse-engineering tools | VERIFIED native Windows Ghidra 12.1.3 / Allegrex v21.4 headless import, 45 instruction-byte checks and advisory decompilation; reusable local evidence wrapper. REA's documented gate rejects missing separate ABI metadata; no REA execution claimed. See [workflow](../research/reverse-engineering.md) |
| PortableKit reuse | VERIFIED optional pinned Windows headless build, exact sectionless ELF admission and shared metadata/clock tests and checked external clear-vertex decoding; bounded interpreter comparison only. A separate Windows Vulkan stripe/readback contract passes; depth-address handling, disc and audio compatibility are not established. See [evaluation](../research/portablekit.md) |
| Module and imports | VERIFIED GP and 268 imports in 30 libraries |
| Full exploratory generation | VERIFIED 1,015 local units; not fully compiled |
| Native entry execution | VERIFIED initialization GE work, SetMode, guarded graphics/resource queries and graphics memory setup; the 501-root slice covers 32,210 words, executes two clears (261,120 pixel positions), reaches eight display frames and seven Complete GE lists, reads the fixed WLAN address once, creates one lightweight mutex and starts a VFPU worker, then stops at the intentionally unsupported `sceUmdActivate` media boundary; no scene is presented |
| Exception boundaries | VERIFIED 53 generated BREAK sites retain terminal stops; no exception delivery or syscall fallback |
| Android ARM64 build | VERIFIED framework, math, startup HLE and expanded entry probe compiled/linked |
| Linux execution | VERIFIED startup-object, fixed-pool, language and event-flag checkpoints passed hosted CI |
| Android host APK | VERIFIED synthetic SDL/Vulkan probe builds, signs and passes ARM64 and x86_64 ELF / 16 KiB ELF and ZIP alignment checks; no game code |
| Host input | VERIFIED synthetic touch identity, stock coordinate and analog contracts; SDL gamepad/touch adapter compiled, guest input integration remains unimplemented |
| Android virtual device | VERIFIED official emulator API 37 x86_64 / 16 KiB and API 35 x86_64 / 4 KiB: APK installation, Vulkan pixels, touch, virtual controller, navigation overlays, three Home/resume cycles and 30-second advancing-frame intervals; API 35 setup limitations documented |
| Physical Android device | BLOCKED: no physical ARM64 device/controller attached; hardware GPU/input behavior unverified |
| Game boot/combat/graphics/audio/input | First native VRAM clear only; title, visible scene, stock gameplay, audio and guest controls remain unimplemented |
| Ultrawide gameplay hooks / moveset packages | PLANNED; reference research only |

Supported game-input identity is limited to the exact USA decrypted baseline.
No other region, patched executable or runtime memory layout is accepted. Runtime
module loading, full HLE, code/data classification and correct bootstrap semantics
remain the main bring-up dependencies. Native performance has not been compared
with PPSSPP, and Jin8 freezes are not fixed.

The [graphics/input foundation](../research/display-foundation.md) is implemented.
Milestone 003 remains open until title boot is verified; this is an observed
dependency transition, not a claim that boot or rendering is complete.

**2026-10-25 supplemental exact-EBOOT static checkpoint:** The callback address
source at `0x088610EC`/`0x088610F4` was verified across a non-likely BLEZ delay
slot with an exact-word witness and refusal tests. Its original static-only
limitations are preserved in [exact callback evidence](../research/callback-08862338.md);
the 2026-10-27 native checkpoint above supersedes its execution status.

**2026-10-22 supplemental source checkpoint:** A new bounded, fail-closed
`tools/inspect_root.py` candidate-root inspector and synthetic regression
tests were added for the next direct helper `0x08ACD010`. The tool never
edits `startup.json`, runs recompilation or claims game boot. Python syntax
checks and isolated mock-dependency contract checks were performed in a remote
Linux environment; the full repository suite, exact-EBOOT inspection, native
probe, Android SDK builds and AVD runs were **not available** in that environment.
The previously verified game execution and graphics boundaries above are
unchanged.
