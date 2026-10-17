# Startup mutex ownership

**VERIFIED, 2026-10-18:** the first constructor creates an unlocked mutex, returns
to the guarded constructor loop, and reaches the second target `0x08B033BC`.
The next coverage boundary is shared constructor/destructor control flow, not a
missing mutex create service. The profile now supports create/delete, uncontended
lock, try-lock, unlock, recursion and release on owner exit.

## Evidence and scope

NIDs and error constants come from pinned uOFW
`28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9`, threadman exports and common/errors.h.
**DOCUMENTED behavioral evidence:** pinned pspautotests
`071a2a6f14477b8b186bd53201e2f2c0adc80679`, mutex create/lock/try/unlock/delete
expected results describe names, ownership, recursion, overflow, stale IDs and
owner exit. These are reference observations, not PSP hardware tests reproduced
here. No reference test code or binaries were copied or executed.

Independently authored synthetic contracts verify those supported transitions.
Names are copied with a 31-byte bound; canonical address checks precede reads.
Mutex UIDs occupy a separate monotonic range. Metadata is capped at 4096 objects.
FIFO/priority and recursive attributes are accepted; other valid but unmodeled
attributes/options stop explicitly. Invalid attributes/counts return documented
errors. Contended blocking locks, non-null timeouts and operations with interrupts
disabled stop without acquiring or changing ownership. No wait queue, callback
processing or timeout clock is claimed.

31 Python tests, nine CTest suites and Android ARM64 compilation passed. The real
trace has one mutex and remains bounded to 64 dispatches / 15 seconds with no
interpreter. Later constructor targets and the title screen are still unverified.
