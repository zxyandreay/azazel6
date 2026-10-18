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
