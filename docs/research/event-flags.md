# Startup event flags

**VERIFIED, 2026-10-19:** the exact-hash bounded native run creates an event flag,
then worker thread 2 with priority 48, stack size 12288 and entry `0x0889B254`.
StartThread receives eight argument bytes, but stops before mutation because that
entry lacks AOT coverage. The run completes 15769 dispatches, all 59 constructors,
the 2048-record loop, fixed-pool allocation and language get/set. Coverage remains
238 roots and 21593 instruction words. This is startup execution, not title boot.

**DOCUMENTED:** pinned PSPSDK event-flag declarations establish the ABI and modes;
uOFW exports/error definitions establish NIDs and constants. Pinned pspautotests
event-flag expected results describe argument precedence, full-pattern outputs,
clear masks and failed polling. References were inspected, not copied or executed;
see [source revisions](sources.md).

The profile owns copied names and patterns in runtime-local objects, using UIDs
`0x70000000` through `0x70FFFFFF` and a 4096-object limit. Creation supports attributes
0 and `0x200`; other valid but unmodeled flags and options stop. Deletion rejects
stale or cross-type IDs. Set ORs bits; Clear ANDs the supplied retain mask.

Poll supports AND/OR matching and clear-all/clear-requested modes. A valid poll
writes the entire pre-clear pattern even when matching fails. Successful matching
applies clearing only after output validation. Null output is allowed; nonnull
outputs must be aligned canonical data words. Wait shares immediate matching,
and now queues unsatisfied waits without writing the output. Nonnull timeouts
remain rejecting before mutation.
All operations require a running thread and enabled interrupts. Callback-aware
waits and status queries remain **PLANNED**.

**VERIFIED validation:** synthetic contracts cover copied/duplicate names, capacity,
high-bit patterns, every supported mode, failed-poll outputs, argument precedence,
stale IDs, isolation, bad pointers and mutation-free stopping paths. All 43 Python
tests and 12 CTest suites pass; Windows execution and Android ARM64 compilation
pass. The next experiment is verified worker entry and callback coverage.

## Queued-wait checkpoint (2026-10-20)

**VERIFIED:** an exact-hash native probe completes 17240 dispatches, then all three
startup threads are Waiting. Thread 1 waits for VBLANK; workers 2 and 3 wait for
bit 1 on the same event flag. The additional queue-empty method is admitted only
through its validated vtable slot. Coverage is 268 roots / 23404 instruction words.
No guest time has elapsed, and no display list or image has been produced.

Queued waits retain full CPU context and validated output addresses. Set scans
waiters in arrival order, applies AND/OR and clear modes, then schedules the highest
priority runnable thread. Clear cannot wake waiters. Delete and Cancel release
waiters with distinct results; Cancel replaces the pattern and reports the waiter
count. Retained pointers and ownership are rechecked before mutation. Aliased
cancellation outputs stop pending a verified write-order contract. Unsupported
interrupt-context operations and timeouts remain explicit stops.

**DOCUMENTED:** the pinned PPSSPP event-flag implementation establishes queue scan,
clear ordering and delete/cancel results. Wait accepts combined clear modes;
Poll rejects them and reports the single-waiter error if another thread is waiting.
Reference: [event-flag behavior](https://github.com/hrydgard/ppsspp/blob/cd94e414cde964bf25ec67267f64fcd3d01283bb/Core/HLE/sceKernelEventFlag.cpp).
No reference implementation was copied.

**VERIFIED:** 47 Python tests, 17 integrated CTest suites and Android ARM64 compilation
pass. New tests exercise FIFO clearing, priority preemption, full wait-context
restoration, retained-pointer invalidation, deletion, cancellation and single-waiter
errors. The next boundary is the explicit idle guest-clock producer.
