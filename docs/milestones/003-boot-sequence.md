# Milestone 003: boot sequence

**VERIFIED, 2026-11-19 system-language continuation.** The
881-root / 52,631-word exact-input slice passes the ID 8
`sceUtilityGetSystemParamInt` language query and stops at missing
direct function `0x08968E48`. Captured scanout remains black;
title-screen boot is open. See [bounded evidence](../research/startup-vtables-2026-11-18.md#cleanup-callback-and-system-language-query).

**VERIFIED, 2026-11-19 nested callback continuation.** The
875-root / 52,316-word exact-input slice reaches another checked
callback and stops at shared virtual call `0x0885FCF4` targeting
`0x088C0FE8`. Captured scanout remains black; title-screen boot
is open. See [bounded evidence](../research/startup-vtables-2026-11-18.md#nested-callback-and-next-shared-dispatch).

**VERIFIED, 2026-11-19 further object continuation.** The
870-root / 52,040-word exact-input slice passes a checked object
tail and stops at shared virtual call `0x0885FCF4` targeting
`0x088C183C`. Captured scanout remains black; title-screen boot
is open. See [bounded evidence](../research/startup-vtables-2026-11-18.md#further-object-tail-continuation).

**VERIFIED, 2026-11-19 following object continuation.** The
865-root / 51,665-word exact-input slice passes a checked tail
and stops at shared virtual call `0x0885FCF4` targeting
`0x088B0D34`. Captured scanout remains black; title-screen boot
is open. See [bounded evidence](../research/startup-vtables-2026-11-18.md#following-object-after-the-helper-chain).

**VERIFIED, 2026-11-19 next object helper chain.** The 860-root /
51,408-word exact-input slice reaches checked direct helpers and
stops at indirect tail `0x088C03FC` targeting `0x088C0298`.
Captured scanout remains black; title-screen boot is open. See
[bounded evidence](../research/startup-vtables-2026-11-18.md#next-object-method-and-helper-chain).

**VERIFIED, 2026-11-19 next object continuation.** The 848-root /
51,061-word exact-input slice passes checked nested object methods
and stops at shared virtual call `0x0885FCF4` targeting
`0x088C0358`. Captured scanout remains black; title-screen boot is
open. See [bounded evidence](../research/startup-vtables-2026-11-18.md#next-object-after-the-nested-tail).

**VERIFIED, 2026-11-19 second nested object continuation.** The
842-root / 50,748-word exact-input slice passes two checked virtual
calls and stops at an observed, file-backed tail target
`0x088B9378`. Captured scanout remains black; title-screen boot is
open. See [bounded evidence](../research/startup-vtables-2026-11-18.md#second-nested-object-and-tail-continuation).

**VERIFIED, 2026-11-19 nested initializer continuation.** The
835-root / 50,420-word exact-input slice passes a checked nested
call and stops at missing direct function `0x088C04C8`. Captured
scanout remains black; title-screen boot is open. See
[bounded evidence](../research/startup-vtables-2026-11-18.md#nested-initializer-under-the-next-object).

**VERIFIED, 2026-11-19 nested tail and object continuation.** The
831-root / 50,117-word exact-input slice passes checked nested tail
and object dispatch, then stops at missing direct function
`0x08A6691C`. Captured scanout remains black; title-screen boot is
open. See [bounded evidence](../research/startup-vtables-2026-11-18.md#nested-tail-and-next-object).

**VERIFIED, 2026-11-19 nested callback continuation.** The
827-root / 49,652-word exact-input slice passes another checked
object callback and stops at missing direct function `0x088B8A10`.
Captured scanout remains black; title-screen boot is open. See
[bounded evidence](../research/startup-vtables-2026-11-18.md#nested-object-callback-after-the-post-tail-state).

**VERIFIED, 2026-11-19 post-tail helper continuation.** The
823-root / 49,379-word exact-input slice passes a checked tail into
`0x088ADA70` and stops at missing direct function `0x088B8764`.
Captured scanout remains black; title-screen boot is open. See
[bounded evidence](../research/startup-vtables-2026-11-18.md#post-tail-helpers-and-state-transition).

**VERIFIED, 2026-11-19 object-tail continuation.** The 818-root /
49,060-word exact-input slice passes checked object helper calls and
the tail at `0x088AD438`, then stops at missing direct function
`0x0882421C`. Captured scanout remains black; title-screen boot is
open. See [bounded evidence](../research/startup-vtables-2026-11-18.md#return-to-the-new-objects-tail).

**VERIFIED, 2026-11-19 helper continuation.** The 814-root /
48,573-word exact-input slice passes another checked initializer and
helper path, then stops at missing direct function `0x0888E9EC`.
Captured scanout remains black. Title-screen boot is open; see
[latest evidence](../research/startup-vtables-2026-11-18.md#continuation-through-another-initializer-and-helper).

**VERIFIED, 2026-11-18 next object continuation.** The
810-root / 48,153-word exact-input slice passes further checked
object calls and stops at missing direct function `0x088AFDA0`.
The captured scanout remains black; title-screen boot is open. See
[object-chain evidence](../research/startup-vtables-2026-11-18.md#next-object-and-initializer-chain).

**VERIFIED, 2026-11-18 repeated helper continuation.** The
803-root / 47,657-word bounded slice passes three more checked calls
through file-backed slot `0x08B8F860` and stops at missing direct
function `0x0888C56C`. Captured scanout remains black. Title-screen
boot is open; see
[latest evidence](../research/startup-vtables-2026-11-18.md#repeated-checked-helper-after-the-callback).

**VERIFIED, 2026-11-18 later object and callback continuation.** The
801-root / 47,409-word exact-input slice passes additional checked
vtable calls and a watched writable callback, then stops at missing
direct function `0x0888AEFC`. Sixteen frames and the captured scanout
remain black. Title-screen boot is open; see
[bounded evidence](../research/startup-vtables-2026-11-18.md#later-object-dispatch-and-writable-callback).

**VERIFIED, 2026-11-18 post-lock object continuation.** The
781-root / 46,379-word ISO-backed slice passes checked object calls
after volatile-memory acquisition. It stops at guarded call
`0x0885FCF4` with a new target `0x0888893C`. Sixteen black frames
leave title-screen boot open; see
[bounded continuation](../research/startup-vtables-2026-11-18.md#object-chain-after-volatile-memory-acquisition).

**VERIFIED, 2026-11-18 volatile-memory continuation.** The
774-root / 46,118-word exact-input slice returns from the observed
mode-zero `sceKernelVolatileMemTryLock` through checked live-partition
outputs. It next stops at guarded tail `0x08808314`, target
`0x088891A0`. Sixteen captured frames remain black; title boot is
open. See [contract and execution evidence](../research/volatile-memory.md).

**VERIFIED, 2026-11-18 watched object continuation.** A witnessed
constructor store and exact target guard pass the callback at
`0x0886AEC4`. The bounded slice covers 762 roots / 45,442 words and
stops at a different guarded object call, `0x0885FCF4`. Sixteen
captured frames remain black; title-screen boot is open. See
[callback provenance and execution](../research/startup-vtables-2026-11-18.md#watched-constructor-callback-continuation).

**VERIFIED, 2026-11-18 guarded vtable continuation.** The exact-input
bounded slice covers 749 roots / 44,814 instruction words. Checked
file-backed object calls and a table tail execute before the terminal
guard at `0x0886AEC4` observes a callback from a writable object field.
The sixteen captured frames remain black. Title-screen boot is open;
see [source and execution evidence](../research/startup-vtables-2026-11-18.md).

**VERIFIED, 2026-11-18 watched callback table checkpoint.** The
exact-input bounded slice covers 729 roots / 44,242 instruction words.
It passes the `0x0893A320` call through two write-watched BSS table
targets and next stops at missing direct root `0x088788B8`. The sixteen
captured frames remain black. Title-screen boot is still open; see
[callback table evidence](../research/watched-callback-table.md).

**VERIFIED, 2026-11-18 sibling helper continuation.** The bounded
ISO-backed slice covers 726 roots / 44,069 instruction words. Calls
at `0x088666E8` and `0x08866804` execute through the checked
file-backed slot `0x08B908F0`; the latter reads `vtable + 8`.
The next guarded call at `0x0886686C` observes target `0x08866D34`.
Sixteen black frames leave the title-screen gate open. See
[continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-18 further object continuation.** The exact-input
native slice covers 721 roots / 43,763 instruction words. Two more
checked calls through file-backed slot `0x08B908F0` and the intervening
direct helpers execute. The bounded run stops at guarded call
`0x088666E8`, where the live target is `0x08866D34`. Another indirect
site in a deeper helper remains trapped. All sixteen captured frames
are black, so title boot remains open. See
[continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-18 guarded object continuation.** Exact-input
coverage is 713 roots / 43,270 instruction words. The bounded ISO-backed
probe executes six further direct roots and checked
virtual calls through file-backed slots `0x08B908F0` and `0x08B8F860`.
It next stops at missing direct helper `0x0893AE58`, before the still-trapped
call at `0x08866540`. Fifteen GE lists and sixteen captured black frames
do not establish title-screen boot. See [continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-18 further object checkpoint.** Coverage is
707 roots / 42,902 instruction words. Two more constructors and
checked virtual calls execute before the bounded ISO-backed run
stops at missing direct helper `0x08927E24`. Its sixteen captured
frames remain black; title boot is open. See
[continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-18 checked field-helper checkpoint.** Coverage
is 705 roots / 42,658 instruction words. The bounded ISO-backed
probe executes `0x08AA0BF8` and another file-backed virtual call,
then stops at missing direct constructor `0x08A53A08`. Sixteen
captured frames remain black; title boot is open. See
[continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-18 next constructor checkpoint.** Coverage is
704 roots / 42,619 instruction words. The bounded ISO-backed run
passes another checked object call and direct constructor before
stopping at missing helper `0x08AA0BF8`. The later indirect site
remains trapped. Sixteen captured frames remain black; title boot
is open. See [continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-18 continued constructor checkpoint.** Coverage
is 701 roots / 42,368 instruction words. Four more object
constructors and their checked calls through file-backed slot
`0x08B908F0` execute. The next bounded ISO-backed stop is missing
direct helper `0x089872C8`, before another trapped virtual call.
The 16-frame capture remains black; title boot is unverified. See
[continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-17 further constructor checkpoint.** Coverage
is 697 roots / 41,933 instruction words. The sixth object call and
the next constructor execute through checked file-backed vtable
slots. The bounded ISO-backed run next stops at missing direct helper
`0x089484A0`, before another trapped object call. Its sixteen captured
frames remain black; title boot is open. See
[continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-17 second object/cache checkpoint.** Coverage is
694 roots / 41,706 instruction words. Checked calls through the second
object vtable complete further cache writebacks, including the observed
null/0x40 and file-backed `0x08B9C690` requests. The latest bounded
ISO-backed run stops at missing direct helper `0x08ACBCF8` after
43 cache operations. Fifteen GE lists and sixteen black frames remain;
title boot is unverified. See [continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-17 further object continuation.** Coverage is
686 roots / 41,585 instruction words. The additional guarded call at
`0x08ACB8A0` follows file-backed slot `0x08BD07B8` to
`0x08AEC8CC`. The bounded ISO-backed run next stops at missing direct
helper `0x08ACB200`. Sixteen captured frames are black. Title boot
remains open; see [continuation evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-17 checked cache/object checkpoint.** Coverage is
680 roots and 41,491 instruction words. The sixth object's tail path
executes checked file-backed virtual calls and the observed null and
GIDX cache writebacks. The latest bounded ISO-backed probe stops at
missing direct helper `0x08ACB2A8`; later indirect sites remain trapped.
All sixteen captured 480×272 frames are black. Title-screen boot
remains open. See [continuation evidence](../research/rtc-tick.md) and
[cache evidence](../research/data-cache.md).

**VERIFIED, 2026-11-17 tail-path checkpoint.** Coverage is 664 roots
and 41,111 instruction words. The sixth object's helper enters a
guarded distant tail path, executes a file-backed vtable call through
`0x08BCFC20`, then stops at missing direct callee `0x08ACB1BC`.
Later indirect calls remain trapped. The 16-frame capture is black;
title-screen boot remains open. See [exact-input evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-17 further continuation.** Coverage is 655 roots
and 40,754 instruction words. The fourth and fifth object calls both
resolve through checked file-backed slot `0x08B908F0` to `0x08866D34`.
The next direct helper `0x0892FEF4` is missing; its callee needs AOT
review before the sixth trapped object call. Sixteen captured frames
remain black. Title-screen boot is still unverified. See
[callback evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-17 continuation.** Coverage is now 651 roots and
40,468 instruction words. A file-backed vtable dispatch reaches existing
root `0x0882C5E0`; two subsequent object calls reach `0x08866D34`
through the same checked slot. The next bounded stop is direct helper
`0x088AD6C4`, before the fourth trapped virtual call. The 16-frame
capture remains black, so the title-screen gate remains open. See
[callback evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-17.** The exact-input native slice now covers 648 roots
and 40,165 instruction words. The game completes three RTC calendar
conversions, reads libc Unix time with a null output, and enters two
callbacks constructed from exact EBOOT address pairs. A checked virtual
call follows file-backed slot `0x08B908F0`. The bounded run then fails at
missing function `0x0889C958`; its indirect transfer at `0x0889C98C`
requires review. The 16-frame ISO-backed capture is uniformly black.
Title-screen boot remains open. See [RTC and callback evidence](../research/rtc-tick.md).

**VERIFIED, 2026-11-12.** The checked system button preference query returns
the configured Cross value. Startup configures VBLANK controller sampling and
analog mode and initializes the next object/list containers. The reviewed
resource-object virtual call `0x08ACC910` at `0x08ACC558` now executes. Its
field-read target `0x08ACC66C`, metadata method `0x08ACC61C` and subsequent
allocator helpers, VRAM command writers and the callback path through
`0x088AA298` run. The game consumes one VBLANK-produced controller sample;
the next missing service is `sceRtcSetTick` (`0x7ED29E40`), after
allocation and both container allocations, the constructor return, status
methods and geometry preparation.
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
