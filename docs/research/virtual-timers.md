# Inactive virtual timers

**DOCUMENTED:** pinned PSPSDK declares timer creation/deletion and recommends a
null options pointer. Pinned uOFW threadman exports identify create `0x20FFF560`,
delete `0x328F9E52` and start `0xC68D9437`; its timer bodies remain unfinished.
Pinned pspautotests `tests/threads/vtimers/{create,delete,start}.expected` supplies
behavioral observations: newly created timers are inactive with zero base/current
time, names are bounded, duplicate names are allowed and stale IDs are rejected.
Those reference tests were neither copied nor executed here.

**VERIFIED, 2026-10-18:** the profile owns inactive timer metadata per runtime,
with copied names, canonical guest-memory checks, a 4096-object host limit and a
separate monotonically increasing UID range. Create/delete are implemented.
Non-null options and mutations with interrupts suspended explicitly stop as
unsupported profile cases, rather than claiming PSP rejects those operations.
Starting a valid timer stops before mutation; invalid IDs return documented
errors. Clock, handler and active-timer semantics remain unimplemented.

The native constructor creates one inactive timer and calls the newly covered
four-word helper `0x08B03630`, which reaches ChangeThreadPriority (`0x71BC9871`)
with UID `0xFFFFFFFF`, priority 19. Twenty-five roots cover 3363 words. Execution
has not yet attempted to activate the timer. Synthetic tests cover names,
ownership, stale IDs, capacity/reuse, address overflow, unsupported options and
the activation boundary. 35 Python cases, nine CTest suites and Android ARM64
compilation pass; execution retains the 4096-dispatch/15-second bounds.

**PLANNED:** implement the observed priority-change contract, then continue
constructor execution. A later active timer requires a clock and callback design.
The pinned generic runtime exposes dispatch/preemption hooks but no PSP clock.
Yakumo's separate MHP3rd profile charges a game-specific 1000-microsecond
starvation quantum and uses fixed callback scratch memory. Those choices are not
evidence for Tekken and must not be imported as its timing or memory layout.
