# Progress and compatibility

**2026-11-22: Milestone 002 implemented; Milestones 003 and 004 in progress.**
Azazel6 is not playable. See
[current checkpoint](../milestones/003-boot-sequence.md),
[foundation evidence](../milestones/001-project-foundation.md) and
[next milestones](roadmap.md).

The latest signed ARM64 hybrid APK executed one verified Tekken AOT function
and 54,086,580 interpreted instructions on an x86_64 emulator with ARM64
translation. It stopped at a strict CancelWakeup guard. An earlier fontless
hybrid APK rendered the actual opening logo; the final package's logo rendering,
title screen, menus and physical ARM64 behavior remain unverified. The primary
AOT research lane has a separate missing-function boundary.

| Area | Status |
| --- | --- |
| Repository source, licensing, docs, CI | Implemented |
| Locked framework fetch | VERIFIED reproducible sparse checkout |
| Windows framework build/regressions | VERIFIED, three suites passed |
| Python/profile tools | VERIFIED, 109 cases passed locally, including media validation, guest execution watchdogs, exact-site VFPU lowering guards, hybrid AOT admission/counter refusals and pinned font admission |
| Semantic aspect math | VERIFIED native contracts passed |
| Startup HLE / returning imports | VERIFIED synthetic contracts and 29 integrated native CTest suites, including exact AV module IDs and duplicate-load state |
| Thread lifecycle | VERIFIED create/start/exit/delete and exit-status queries, copied arguments and service-boundary priority scheduling and strict-priority changes; VFPU attributes are supported at creation and by later addition/removal |
| Mutex lifecycle | VERIFIED creation, recursion, ownership, deletion and owner exit; source-only lightweight-mutex creation initializes the guest workarea; waits/timeouts and later lightweight-mutex operations remain rejecting |
| Semaphore lifecycle | VERIFIED synthetic creation, FIFO/priority waits, positive-count signaling, overflow checks, deletion wakeups and priority handoff; nonpositive signaling and timeouts remain unsupported; see [contracts](../research/semaphores.md) |
| Virtual timers | VERIFIED inactive object creation/deletion; active clocks and callbacks remain unimplemented |
| Startup thread delay | VERIFIED synthetic observed 96,000-us non-callback delay, with documented reference 10-us overhead on the shared idle timeline; other durations and runnable CPU accounting remain unsupported |
| Clock conversion | VERIFIED unsigned immediate and pointer-form microsecond conversion, including null-first output semantics, with atomic live-caller-stack validation; no time advancement |
| System-time query | VERIFIED low and wide microseconds plus current Hcount from the explicit guest timeline, including GE interrupt reads, sub-microsecond truncation and wrap; reads do not advance time; runnable CPU time remains unmodeled |
| Power services | VERIFIED immutable nominal startup PLL/CPU/bus reads at 222/222/111 MHz plus owned callback-slot registration and queued initial `0x10E4` notification; delivery, host clocks, clock transitions and timing rescale remain unimplemented |
| Controller settings | VERIFIED runtime-local idle-cancel thresholds, VBLANK sampling configuration and digital/analog mode transitions; one guest pad read consumes a VBLANK-produced snapshot |
| Startup device control | VERIFIED source-only `fatms0:` callback ownership registration with checked live-stack input; no host path, media state, capacity, file or callback delivery |
| Startup WLAN identity | VERIFIED one fixed locally administered unicast address copied only to a checked live caller stack; no host networking or connectivity |
| UMD activation / media | VERIFIED exact-hash read-only ISO admission, checked title/region and file extents, real sector query and handle closure; omission preserves empty-drive timeouts |
| Idle timeouts | VERIFIED synthetic deadlines ordered with LCD edges, batched timeout/display wakeups, priority selection, subframe event budgets and preserved thread context; no runnable CPU time or callback delivery; see [ADR-0004](../decisions/ADR-0004-bounded-idle-timeouts.md) |
| UMD continuation | VERIFIED native media-worker completion and UID deletion with actual disc data; startup advances to a new guarded callback; see [local media evidence](../research/local-media-continuation.md) |
| Callbacks | VERIFIED inactive ownership, deletion and exit registration; delivery remains unimplemented |
| Data-cache ranges | VERIFIED checked coherent startup RAM operations; bounded live-partition writebacks through 512 KiB, canonical VRAM writebacks, exact null/0x40 and null/0x80 writebacks, and reviewed four-line file data; code/alias/unowned ranges reject |
| Shared guest memory | VERIFIED non-overlapping stack/heap ownership, allocation/free/alignment |
| Volatile memory | VERIFIED synthetic mode-zero try-lock and unlock with runtime-owned state, checked live-partition outputs, busy and invalid-mode errors; exact-input native startup completes one try-lock. Blocking lock and suspend/resume remain unsupported; see [evidence](../research/volatile-memory.md) |
| Fixed pools | VERIFIED single-block creation, immediate allocation, free/delete and shared ownership; multi-block/waiting services remain rejecting |
| Language settings | VERIFIED explicit English/Cross defaults, checked runtime-local get/set and caller-stack system button preference query; no host locale dependency |
| Event flags | VERIFIED create/delete, set/clear, polling, queued waits, FIFO wakeups and cancellation; timeouts remain rejecting |
| Graphics memory | Primary AOT lane: VERIFIED immutable EDRAM size/base queries backed by allocated guest VRAM; first native VRAM clear verified; general GE rendering remains unimplemented in that lane |
| Framebuffer readback | Primary AOT lane: VERIFIED synthetic CPU RGBA snapshot in all four framebuffer formats, checked strides/latches and GE-clear integration; native probe now captures current scanout locally as RGBA/PPM. Its 51-frame checkpoint is uniformly black; the separate PortableKit hybrid has rendered the opening logo |
| GE callbacks/control lists | VERIFIED synthetic bounded FIFO/stalls/control flow and guarded SIGNAL/FINISH delivery; real initialization EnQueue/ListSync and completed-work DrawSync return; written live-partition submissions/FINISH bridge and bounded RGB565 clear sprites tested; general draws and asynchronous producers remain unimplemented |
| VBLANK | VERIFIED real-input registration/enable and four-word handler AOT coverage; synthetic owned delivery and full context restoration |
| Display foundation | VERIFIED synthetic LCD phases, checked framebuffer latches and integrated interrupt/wait ordering; real startup executes four VBLANK handlers, SetMode and next-edge WaitVblankStartCB with inactive callbacks under bounded idle events; runnable guest-time accounting remains unimplemented |
| ULUS10466 validation | VERIFIED exact local hash/ELF match |
| Reverse-engineering tools | VERIFIED native Windows Ghidra 12.1.3 / Allegrex v21.4 headless import, 45 instruction-byte checks and advisory decompilation; reusable local evidence wrapper. REA's documented gate rejects missing separate ABI metadata; no REA execution claimed. See [workflow](../research/reverse-engineering.md) |
| PortableKit reuse | VERIFIED pinned exact-ISO Windows hybrid AOT/interpreter build reached the Bandai Namco Games opening logo, with one measured native Tekken entry call and 73,261,212 interpreted instructions. The next strict stop is movie-player current-PTS; GE depth alias, completion and ATRAC ID-pool contracts remain unresolved. The primary runtime is unchanged. See [hybrid evidence](../research/hybrid-aot-portablekit-2026-11-21.md). |
| Module and imports | VERIFIED GP and 268 imports in 30 libraries |
| Full exploratory generation | VERIFIED 1,015 local units; not fully compiled |
| Native entry execution | VERIFIED 1,134 reviewed roots / 65,487 words. Calendar/libc time, checked virtual paths, callback targets, bounded cache writebacks, mode-zero volatile-memory try-lock, fixed system-language query, two exact-site `vi2uc.q` conversions and three scoped AV module requests execute. Guest-written callback and object vtable targets have exact source guards. Further inspected direct helpers and an object-update chain execute after the checked sibling tails. The next stop is missing direct function `0x0886546C`. One VBLANK controller read, sixteen display frames, fifteen GE lists and zero nonblack pixels. See [continuation evidence](../research/startup-vtables-2026-11-18.md#following-direct-helper-and-object-update). |
| Exception boundaries | VERIFIED 55 generated BREAK sites retain terminal stops; no exception delivery or syscall fallback |
| Android ARM64 build | VERIFIED framework, math, startup HLE and expanded primary entry probe compiled/linked; separate genuine hybrid APK built, signed, installed and executed under x86_64 ARM64 translation |
| Linux execution | VERIFIED startup-object, fixed-pool, language and event-flag checkpoints passed hosted CI |
| Android host APK | VERIFIED synthetic SDL/Vulkan probe and separate signed ARM64 genuine hybrid APK. The final hybrid packages one exact Tekken native AOT function, interpreter, ISO, FFmpeg, Vulkan and pinned fallback font. An x86_64 emulator with ARM64 translation installed and ran it: measured `native_aot_entry_calls=1`, 54,086,580 interpreted instructions and font loading, then a strict CancelWakeup stop. The earlier fontless package rendered actual opening-logo pixels. Physical ARM64 device remains untested. See [Android evidence](../research/hybrid-android-apk-2026-11-22.md). |
| Host input | VERIFIED synthetic touch identity, stock coordinate and analog contracts; SDL gamepad/touch adapter compiled. Guest one-sample read consumes a VBLANK-produced pad snapshot in the native probe; game-running Android input wiring remains open. |
| Android virtual device | VERIFIED official emulator API 37 x86_64 / 16 KiB and API 35 x86_64 / 4 KiB: APK installation, Vulkan pixels, touch, virtual controller, navigation overlays, three Home/resume cycles and 30-second advancing-frame intervals; API 35 setup limitations documented |
| Physical Android device | BLOCKED: no physical ARM64 device/controller attached; hardware GPU/input behavior unverified |
| Game boot/combat/graphics/audio/input | The PortableKit hybrid renders the Bandai Namco Games opening logo on Windows and a translated Android emulator. Title, menus, gameplay, audio output and physical guest controls remain unverified. The primary AOT lane still stops at a missing direct function. |
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

**2026-11-21 optional whole-host comparison:** At this earlier checkpoint, VERIFIED exact-ISO interpreter
execution against pinned PortableKit reached a visible Bandai Namco Games
opening logo in a bounded Windows Vulkan capture after 188 VBLANKs and 103 GE
enqueues. A larger bounded pass reached a black movie transition at the next
strict `scePsmfPlayerGetCurrentPts` barrier after 657 VBLANKs and 337 GE
enqueues. At that checkpoint, no Tekken title, gameplay frame or Android game
execution was established. PortableKit's ATRAC ID-pool and GE completion approximations keep
this diagnostic, while the primary AOT implementation remains intact. Broad
generation produced 1,015 units with 62,036 unsupported sites, so a full
compile has not been justified. See the
[measured strategy comparison](../research/interpreter-first-bring-up-2026-11-21.md).
