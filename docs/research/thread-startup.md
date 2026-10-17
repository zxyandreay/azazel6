# Thread startup and bootstrap handoff

**VERIFIED 2026-10-18 checkpoint:** the native probe now starts the game's child
thread after module bootstrap returns. This remains bounded bring-up, not game boot.

The strict export-table reader finds variable NID `0x0F7C276C` at guest
`0x08B89E18`: three parameters, priority 32, stack size 1024 and attributes
`0x80000000`. The pinned uOFW `include/modulemgr_nids.h` identifies this export;
`src/kd/modulemgr/modulemgr.c` consumes its priority/stack/attributes and establishes
the module GP before thread creation. This replaces the experimental 64 KiB
bootstrap reservation. Host allocation addresses remain host policy.

## Implemented behavior

The scheduler owns complete Allegrex contexts (integer, FP and VFPU state), ready,
running and dormant status, copied arguments, exit status and monotonic UIDs.
StartThread validates UID/state, size, canonical guest range and available stack
before changing state. Argument bytes are snapshotted before stack initialization,
so an input buffer overlapping the destination cannot be destroyed mid-copy.
Oversized argument blocks stop instead of reproducing firmware corruption.

The initial user context uses the module GP, an aligned argument copy preceding
a 256-byte TLS area, and a 64-byte argument-home reservation below it. The TLS UID,
stack-base fields and terminal words match the pinned PSP autotest observations;
the layout and argument-home convention were also inspected in pinned Yakumo's
`profiles/mhp3rd/host/kernel/kernel.cpp`. Original implementation, no source copied.
Unused CPU fields begin zero as a bounded host policy; complete firmware reset
state, privilege behavior and VFPU execution are not established by this test.

A more urgent ready thread preempts at StartThread; equal/lower priority stays
queued. ExitThread, ExitDeleteThread and normal return select the highest-priority
ready context. An explicit host return gate at `0x08000000`, outside the loaded
image/allocations, replaces a firmware exit trampoline. Context-token changes
prevent import wrappers returning into the old caller. No runnable threads stops
the experiment. Waiting, callbacks, timer/interrupt preemption and fair time slices
are not implemented. This is not a complete PSP kernel.

## Native evidence

The profile explicitly generates the observed roots `0x08804124` and `0x08804238`:
233 instruction words total, not an executed-instruction count. Both bounded CFGs
exclude callees. Input hash is checked before generation and execution.

- Initial bootstrap SP is `0x09FFFEC0`, priority 32, stack 1024 bytes.
- The created thread has priority 40 and stack `[0x09FF7C00, 0x09FFFC00)`.
- StartThread returns to bootstrap at `0x0880420C`; it does not prematurely switch.
- Bootstrap reaches the host return gate, which activates the ready child.
- The child executes translated instructions at `0x08804238` and stops at an
  uncovered direct call `0x08B787CC`, RA `0x08804284`, SP `0x09FFF6D0`.

The runner correctly records this last stop as a coverage failure (exit 1), not a
missing-import boundary or successful boot. Local evidence is under
`local/entry-probe-start`. The 15-second subprocess limit, 64-dispatch budget and
disabled interpreter remain in force. Module-start arguments are still zero;
the actual loadexec pathname/argument block remains unverified.

## Validation and continuation

21 Python tests, six native CTest suites and Android ARM64 compilation pass.
New tests cover export bounds, priority handoff, full context preservation,
double start, deleting ready threads, thread return/exit-delete, overlapping
argument copies, pointer overflow and argument-size limits. Synthetic execution
does not establish firmware equivalence.

Next: trace-guided AOT expansion from `0x08B787CC`, implement required services
with explicit memory ownership, then establish file/loadexec arguments before
claiming successful process initialization. Milestone 002 remains in progress;
Milestone 003 kernel work has begun but title-screen progression is not implemented.
