# Milestone 003: boot sequence

**VERIFIED, 2026-11-12.** The checked system button preference query returns
the configured Cross value. Startup configures VBLANK controller sampling and
analog mode and initializes the next object/list containers. The reviewed
resource-object virtual call `0x08ACC910` at `0x08ACC558` now executes. Its
field-read target `0x08ACC66C`, metadata method `0x08ACC61C` and subsequent
allocator helpers, VRAM command writers and the callback path through
`0x088AA298` run. The game consumes one VBLANK-produced controller sample;
the next missing direct callee is `0x089B833C`, after allocation and both
container allocations, the constructor return, status methods and geometry
preparation.
The 16-frame real-media capture is still black; title boot remains open.
See [settings evidence](../research/language-mode.md#system-button-preference-2026-11-12).

**VERIFIED object callbacks.** Both object initializers executed on the main
thread. The next guard observed target `0x088977D4` at `0x08897ADC`. The
capture still has zero nonblack pixels.

**VERIFIED, 2026-11-06.** The game reads both index files and the first raw
disc sector, then schedules the main thread when the streaming worker sleeps.
The main thread reaches `0x08868270` through two observed callbacks.
The 14th captured display frame is black. Title-screen verification is open.

**VERIFIED, 2026-11-06.** The native game requests file metadata, receives the
checked ISO result and continues through its own semaphore completion path.
The subsequent worker request opens `DINF_400.BIN`, reads its complete 120-byte
payload into live game memory, closes the file and exits. The main thread then
selects its payload-processing callback. Title boot remains open. See the
[file request trace](../research/local-media-continuation.md#first-file-metadata-request-2026-11-06).

**2026-11-05 local continuation:** checked read-only ISO access removes the
empty-drive dependency. The media worker completes its actual file-sector query
and exits; the main thread advances to guarded callback `0x08898710` from
`0x08897ADC`. The 505-root run reaches nine display frames with black scanout.
Title boot remains open. See [media evidence](../research/local-media-continuation.md).

**IN PROGRESS, 2026-11-01.** Native bring-up is implemented; title-screen boot is
not. The current bounded experiment completes the full 59-entry constructor loop, creates
four mutexes and eight inactive timers, registers its exit callback and completes
the post-constructor object's 2048-record initialization loop. It allocates a
single-block fixed pool, completes language get/set and creates an event flag.
Both workers now wait on their shared event flag. The main thread completes
VBLANK registration, the first GE initialization list and display-mode setup
under bounded idle events, then advances through guarded graphics methods,
resource layout/allocation and checked clock conversion. The latest 504-root
native slice follows the post-clear object graph and later state callbacks,
reaches 51 display frames and completes 50 GE lists. Two GE clears write
261,120 guest-VRAM pixel positions. It reads the nominal 222/222/111 MHz clock
profile, registers checked source-only `fatms0:` callback ownership and a fixed
local WLAN identity, creates one lightweight mutex and starts a priority-32 VFPU
worker. That worker records a valid `disc0:` activation request without supplying
media or a host filesystem. Its finite READY waits now time out and enter the
reviewed delay/retry loop while the main thread polls its exit status and advances
the display. The last native run stops at the 32,768-dispatch budget, at
`0x08867528`, after 850,850 modeled guest microseconds; the runner reports
`failed` / exit 1. Title boot remains open. See
[current startup evidence](../research/startup-indirect-2026-10-22.md),
[constructor-completion evidence](../research/constructor-coverage.md) and
[object/exception-boundary evidence](../research/startup-objects.md) and
[fixed-pool evidence](../research/fixed-pools.md) and
[language-mode evidence](../research/language-mode.md) and
[event-flag evidence](../research/event-flags.md) and
[worker dispatch evidence](../research/worker-dispatch.md) and
[graphics memory evidence](../research/graphics-memory.md) and
[GE/VBLANK handoff evidence](../research/ge-callbacks.md).

Implemented foundations include thread lifecycle and priority handoff, shared
stack/partition allocation, resident module identity, nested interrupt state,
conservative startup reachability and guarded constructor dispatch. These remain
restricted contracts, not a complete PSP kernel.

Next dependencies are broader synchronization/timeouts, verified AOT,
filesystem/ISO access and module loading as observed. General active timers,
general callback delivery, overlays, rendering and title-screen progression remain
unimplemented. Preserve the external timeout and rejecting unknown-import policy.

Display interrupt and GE foundations are implemented in
[Milestone 004](004-graphics-input.md); guest drawing integration remains open.
Keep this milestone's title-screen gate open. Missing kernel/I/O/module services still
belong here and must be implemented when encountered.

**2026-10-25 static checkpoint:** The exact ordinary-BLEZ delay-slot source for
`0x08862338` and its candidate callees were recorded before native continuation.
That static gate is now superseded by the 2026-10-27 executed checkpoint above;
its provenance remains in [static investigation](../research/callback-08862338.md).

## Candidate-root inspection

**2026-10-30 source checkpoint:** semaphore signaling and deletion now complete
the supported lifecycle, with synthetic tests for blocked-waiter wakeups,
overflow atomicity, preserved thread context and priority handoff. See
[semaphore contracts](../research/semaphores.md). Native title progression remains
unverified; semaphore timeouts remain unsupported. FIFO and stable priority
wait ordering are verified synthetically.

**VERIFIED tooling:** `tools/inspect_root.py` provides strict-hash, bounded,
local CFG inspection and distinguishes new candidates, existing roots and import
stubs. It never expands authorized AOT roots automatically; unknown indirect
transfers and size limits require explicit review. It has been executed on the
exact local input during the [graphics startup continuation](../research/startup-indirect-2026-10-22.md),
alongside synthetic regressions. Those verified coverage increments preserve
this milestone's open title-screen gate.

## Source-only timeout checkpoint (2026-10-29)

**VERIFIED synthetic:** finite empty-drive READY waits now suspend a callback-free
caller until its deadline on the shared LCD timeline. Context restoration,
VBLANK ordering and rejecting unsupported configurations are tested. See
[ADR-0004](../decisions/ADR-0004-bounded-idle-timeouts.md).

Mixed timeout/display waiters now wake in one batch, preserving scheduler priority
selection even when deadlines coincide without an installed interrupt handler.
Short timeouts consume the bounded idle-event budget independently of LCD frames.
These are synthetic kernel/display integration results shared with Milestone 004.

**VERIFIED native follow-up, 2026-11-01:** the user's expanded authorization
supersedes the earlier analysis-only restriction. Bounded regeneration, build and
native execution now verify the timeout/retry path and concurrent display work.
Callback-owning UMD callers, media access and runnable guest time remain unsupported.

**VERIFIED static follow-up:** the negative UMD wait result enters a 96,000-us
`sceKernelDelayThread` retry loop. A nonnegative result can lead to an unregistered
helper with file/device operations. Empty-drive timeouts cannot carry this worker
past the media gate. See [bounded analysis](../research/umd-wait-continuation.md);
this does not establish the next execution order of other threads.

The observed retry delay now has a restricted HLE contract and synthetic tests.
It returns zero only after a 96,010-us modeled deadline (96,000-us request plus
the documented reference overhead), preserving VBLANK ordering and caller context.
The native trace now records four READY-wait calls and four delay calls, without
media readiness. Three reviewed roots and a state-preserving
`sceKernelGetThreadExitStatus` query advance execution beyond the former import
boundary. See the [native follow-up](../research/umd-wait-continuation.md#native-follow-up-2026-11-01).

**BLOCKED verified title progression (2026-11-01):** the repeated native run
confirms that the main callback polls media worker 5; static review shows it
preserves its startup state until worker completion. The worker retries empty
READY waits. Only EBOOT is locally available, with no disc/assets or readable
media backend. Optional current-frame capture verifies black scanout, rather
than visible title graphics. See [root cause and next implementation](../research/umd-wait-continuation.md#main-thread-dependency-and-actual-scanout-2026-11-01).
Supplying real local media and implementing its checked I/O path are required
before this dependency can be tested further; increasing display/dispatch budgets
or returning fake READY cannot provide missing files.
