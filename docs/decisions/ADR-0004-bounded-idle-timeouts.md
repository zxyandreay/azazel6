# ADR-0004: Order finite idle timeouts with LCD edges

**Status:** Accepted for source-only synthetic research. **Date:** 2026-10-29.

## Context and decision

[ADR-0002](ADR-0002-bounded-idle-events.md) allowed only display waiters to
produce idle events. A finite empty-drive READY wait needs an elapsed deadline,
not immediate success or an immediate timeout. This decision supersedes that
restriction for finite timed waits while retaining the default externally driven
clock and the prohibition on estimating runnable CPU time.

Use the existing 18 MHz integer timeline. A waiting thread stores an absolute
tick deadline and completion result. The explicit idle producer advances only
when all threads are blocked, to the earlier of the next LCD edge and earliest
thread deadline. External advances also reject skipping a deadline. At a shared
edge, interrupt dispatch precedes timeout/display wakeups; the scheduler defers
thread dispatch until interrupt return. Timeout and display waiters at one edge
become ready in one batch before the scheduler selects a thread. The framebuffer
latch is published before that selection. Equal priorities use the existing
scheduler traversal order, rather than timeout-versus-display wake order.
Each idle advance consumes the existing
configured event budget, including advances that do not cross an LCD edge.
The reported idle-frame count continues to count LCD edges only.

The source-only UMD service accepts a finite positive READY wait on an activated
empty drive only for a thread owning no callbacks. It returns the wait-timeout
error after the requested duration. Other states, zero timeout, absent clocks,
and callback-capable callers stop for research. No medium or readable state is
introduced. General callback delivery and runnable time remain outside this model.
Yakumo stays pinned and unmodified.

## Alternatives and limitations

Immediate timeout would invent elapsed time. Returning READY would invent media.
Host clocks and dispatch-count time remain rejected for the reasons in ADR-0002.
This bounded experiment can validate idle deadlines but cannot establish hardware
timing, title boot, callbacks, media insertion, or polling progress while runnable.

## Verification

Synthetic I/O tests cover 1, 16,683, 16,684, 50,050 and 96,000 microseconds,
no early return, full preserved register samples and timeout removal. Display
tests cover a timeout exactly coincident with VBLANK and handler restoration.
Rejection tests cover missing activation/clock, unsupported state, zero duration,
owned callbacks and disabled/interrupt context. Real-input execution was not run
for this change; the prior native checkpoint remains historical evidence.

Additional synthetic regressions cover simultaneous display/timeout waiters at
lower, equal and higher priorities; subframe budget exhaustion without advancing
an LCD frame; empty clock advances; and rejection of copied CPU contexts.
The probe reports idle events separately from idle frames.

The statically observed UMD retry's non-callback thread delay also uses this
queue. Its 96,000-us request uses the documented PPSSPP policy of 10-us extra
delay; see [provenance and limits](../research/umd-wait-continuation.md).
This models the wait deadline only, not runnable CPU execution cost. Other
delay durations remain rejecting pending separate evidence.
