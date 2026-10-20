# ADR-0002: Explicit bounded idle display events

**Status:** Accepted for startup research. **Date:** 2026-10-20.

## Context

The native startup reaches a state where all three threads are blocked: one on
VBLANK and two on an event flag. The pinned external core exposes dispatch
boundaries, but these do not establish elapsed Allegrex cycles. Host wall time
also cannot establish guest time.

## Decision

Keep the existing default display model externally driven. Add an explicitly
enabled, bounded idle-event producer to the profile's research probe. It may jump
to the next documented LCD edge only when no thread is runnable, interrupts are
enabled, a display waiter exists and no interrupt is unfinished. Deliver the owned
handler and resume waiters through the existing context-preserving scheduler.

The probe allows at most 64 idle frames, independently of its existing dispatch
and wall-time bounds. It reports this clock mode and the idle-frame count.
Cross-runtime calls, exhausted budgets and invalid idle conditions reject before
advancing time. An event-only deadlock does not generate arbitrary frames.

## Alternatives and consequences

Converting dispatch counts to guessed cycles would make timing depend on how AOT
functions are split. Advancing from host elapsed time would make results depend on
logging, machine load and compiler optimization. Neither is admitted.

This restricted discrete-event experiment can verify handler execution and
post-wait control flow. It does not account for time while code is runnable and
cannot verify frame pacing, polling loops, timeouts, gameplay speed or hardware
timing. A runnable guest that needs time to pass still reaches a bounded research
stop. Future timers, asynchronous I/O and GE deadlines require a shared ordered
event model before this producer can encompass them.

Yakumo remains external and unchanged. No game return value is patched, and no
FPS change is introduced. Measured game timing and real Android visual/input
evidence remain separate milestone requirements.

## Verification

Synthetic contracts cover automatic idle-gate dispatch, full interrupt/wait
context restoration, no early wake, cross-runtime rejection, runnable-thread
rejection and budget exhaustion. Real-input outcomes belong in the
[display research report](../research/display-foundation.md).
