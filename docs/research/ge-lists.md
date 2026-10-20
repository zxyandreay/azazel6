# Bounded GE control streams

**VERIFIED synthetic behavior, 2026-10-20:** a profile-owned GE queue models FIFO
list leases, stalls, bounded command execution, control flow and callback return.
This is not game display-list execution and is not connected to rendering.

Each list has an explicit aligned canonical-RAM interval. Every command read and
branch target is checked against that interval. Active overlapping intervals reject.
The queue retains at most 64 records, uses non-reused monotonically increasing
host IDs, and allows explicit reclamation. A stalled head cannot be bypassed by
later lists. Updating a stall validates before mutation. Per-call work is bounded,
and a separate 65536-command lifetime budget terminates looping streams even if
the host keeps submitting small work slices. Call depth is capped at eight as a
research resource limit, not a claim about hardware stack capacity.

The independently authored decoder supports NOP, BASE, JUMP, CALL, RET, FINISH,
handler-suspend SIGNAL and END. FINISH/SIGNAL require a following END, including
across a stall. Unsupported drawing/state commands, SIGNAL modes, invalid returns,
unowned targets and exhausted budgets produce terminal failures with diagnostics.
Each list begins with BASE zero and owns its control stack. Cross-list GE state,
offset/origin modes, VRAM lists, context save/restore and guest list-ID semantics
are unimplemented; therefore `sceGeListEnQueue` is deliberately not installed yet.

Callback registrations now have per-slot generations. A queue captures a generation
at submission and rechecks it before dispatch and completion. Unregister/reuse
cannot redirect an old event. Callback arguments remain opaque 32-bit values.
Events have separate monotonically increasing completion tokens. SIGNAL waits for
its callback return; FINISH does not mark the list complete until the callback
returns. An in-flight event prevents list deletion. Duplicate/wrong acknowledgments
reject. Null callbacks need no guest execution.

The host callback bridge uses the owned interrupt stack and guarded return gate.
It checks runtime ownership, interrupt availability and AOT coverage, delivers the
16-bit event value and callback argument, then restores the complete interrupted
context before acknowledging the event. Synthetic AOT functions exercise this
whole route under a finite dispatch bound. No guest renderer or successful GE
completion is fabricated to advance the real-input probe.

**DOCUMENTED references:** pinned PSPSDK `guInternal.h`, `sceGuFinishId.c`,
`sceGuSignal.c`, `sceGuCallList.c`, `pspgu.h` and `pspge.h` describe command numbers,
terminal pairs, SIGNAL modes and callback arguments. Existing GE callback
[ownership evidence](ge-callbacks.md) establishes the registry. Reference source
was inspected only; no implementation was copied. The finite host leases, IDs and
budgets above are explicit Azazel6 policies.

**PLANNED:** inspect the first actual game list; establish all state/command and
memory-lifetime contracts it needs; then bind observed GE services and connect a
renderer. DrawSync/ListSync waits must be driven by actual queue progress. A Vulkan
clear screen or synthetic triangle would not satisfy the menu-rendering gate.
