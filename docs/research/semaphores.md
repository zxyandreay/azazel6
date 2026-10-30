# Bounded semaphore signaling and deletion

**VERIFIED synthetic, 2026-10-30.** Startup semaphore creation and waiting now
have complementary signal/delete services. This closes a kernel synchronization
gap in Milestone 003 without requiring media or native game execution.

## Reference and scope

The pinned Yakumo NID registry identifies `sceKernelSignalSema` (`0x3F53E640`)
and `sceKernelDeleteSema` (`0x28B6489C`). Behavior was compared with PPSSPP
revision `cf17b0ac7966ba4b172a32340333d209a2d747e7`,
[`Core/HLE/sceKernelSemaphore.cpp`](https://github.com/hrydgard/ppsspp/blob/cf17b0ac7966ba4b172a32340333d209a2d747e7/Core/HLE/sceKernelSemaphore.cpp).
That is documented reference behavior, not a firmware-equivalence claim.

The implemented signal subset accepts positive signed counts and FIFO or priority
semaphores in a running, interrupt-enabled thread. It validates all retained
waiter ownership before changing any state. Overflow uses the reference's
`current + signal > maximum + number_of_waiters` test, computed with wide
arithmetic. Overflow returns `0x800201AE` without modifying counts or waiters.

FIFO waiters are examined in arrival order; priority waiters are stably sorted
by current thread priority at signaling time. An unsatisfied request remains queued;
a later request that fits the remaining count can complete, matching the
reference's signal scan. A new wait does not immediately consume counts while
there are queued waiters. Signaling makes all eligible waiters ready before
priority scheduling. The signaler's successful return and continuation are
saved before a higher-priority waiter can run.

Deletion validates all waiter ownership, returns `0x800201B5` to each waiter,
removes its retained wait identity, deletes the semaphore and then schedules.
Stale signal/delete IDs return `0x80020199`. No IDs are reused.

## Validation and limits

Synthetic tests cover partial signaling, later satisfiable requests, leftover
counts, overflow rejection, large counts, queued-wait admission, deletion of
multiple waiters, stale IDs, preserved CPU state, priority preemption and the
signaler's eventual return. Interrupt-masked mutations reject atomically.

Nonpositive signal counts, semaphore timeouts,
callback waits, cancellation and status queries remain unsupported. Unknown
operations continue to stop rather than report fabricated success. Existing
startup evidence is unchanged: no EBOOT execution was performed for this work.
