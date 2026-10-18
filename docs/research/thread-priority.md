# Current thread priority

**DOCUMENTED:** pinned PSPSDK `6ef2b21dc440ef8d4125f000301faaa2a470975e`,
`src/user/pspthreadman.h`, defines `sceKernelGetThreadCurrentPriority` as returning
the calling thread's current priority. Pinned uOFW threadman exports assign NID
`0x94AA61EE`; its function body is unfinished and is not implementation evidence.

**VERIFIED, 2026-10-18:** the profile queries its owned running-thread record.
The import changes only v0, follows the normal returning-import ABI, and stops
without fabricating a priority if no running thread exists. Synthetic contracts
exercise bootstrap priority 32, preemption to priority 8, return to bootstrap,
handoff to priority 119, register preservation and an unconfigured scheduler.
Suspending interrupts does not change the current thread's priority.

Actual native execution returns priority 40 for the startup child and continues
through the second constructor to `sceKernelCreateVTimer`, NID `0x20FFF560`, at
`0x08B88E78`, return `0x08B0BB14`. No timer success or advancing guest clock is
claimed. 35 Python cases, nine CTest suites, Windows execution and Android ARM64
compilation pass at this checkpoint. The 4096-dispatch/15-second bounds remain.

**PLANNED:** establish virtual-timer object, clock and callback contracts before
allowing the observed timer sequence to run.

## Priority changes and completed constructors

**VERIFIED, 2026-10-18:** ChangeThreadPriority (`0x71BC9871`) now implements
invalid/stale ID, dormant-thread and out-of-range priority errors, current-thread
UID zero, and priority zero inheriting the caller's current priority. Runnable
threads support strict-priority changes and preemption at the returning-import
boundary. Equal-priority queue rotation and mutation with interrupts suspended
stop before modifying state. These are profile limitations, not claimed PSP
errors. Wait/suspend queues remain unimplemented.

**DOCUMENTED:** pinned pspautotests `threads/threads/change.expected` describes
the errors, accepted range 8..119, zero semantics and dormant-thread rejection.
Independent synthetic tests verify raising a ready thread preempts, lowering the
current thread resumes its caller with the correct return context, priority
queries reflect changes, and unsupported ties leave priority unchanged.

Actual startup attempts UID `0xFFFFFFFF`, receives `0x80020198`, and continues
without a fabricated priority change. The second constructor completes after
creating eight inactive timers. The third completes through shared mutex setup;
the fourth target `0x08AF2A10` is the next uncovered boundary. The two existing
shared ranges at `0x08B01DA0`/`0x08B01E14` now have explicit roots so different
constructor/destructor wrappers cannot duplicate their ownership. Thirty roots
cover 3523 words. 35 Python cases, nine CTest suites and Android ARM64 compilation
pass. The probe retains its 4096-dispatch and 15-second bounds.
