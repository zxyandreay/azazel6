# Milestone 003: boot sequence

**IN PROGRESS, 2026-10-25.** Native bring-up is implemented; title-screen boot is
not. The current bounded experiment completes the full 59-entry constructor loop, creates
four mutexes and eight inactive timers, registers its exit callback and completes
the post-constructor object's 2048-record initialization loop. It allocates a
single-block fixed pool, completes language get/set and creates an event flag.
Both workers now wait on their shared event flag. The main thread completes
VBLANK registration, the first GE initialization list and display-mode setup
under bounded idle events, then advances through guarded graphics methods,
resource layout/allocation and checked wide clock conversion. The latest
374-root native slice enters callback `0x088610C4`, builds a clear command block,
returns from completed-work DrawSync, swaps framebuffer descriptors and waits for
a third VBLANK. Two GE clears now write guest VRAM, and both FINISH callbacks return after
reading the explicit guest clock. The new boundary is guarded callback target
`0x08862338` from `0x08860FBC`; title boot remains open. See
[current startup evidence](../research/startup-indirect-2026-10-22.md),
[constructor-completion evidence](../research/constructor-coverage.md) and
[object/exception-boundary evidence](../research/startup-objects.md) and
[fixed-pool evidence](../research/fixed-pools.md) and
[language-mode evidence](../research/language-mode.md) and
[event-flag evidence](../research/event-flags.md) and
[worker dispatch evidence](../research/worker-dispatch.md) and
[graphics memory evidence](../research/graphics-memory.md) and
[GE/VBLANK handoff evidence](../research/ge-callbacks.md).

Implemented foundations include thread lifecycle and priority handoff, shared
stack/partition allocation, resident module identity, nested interrupt state,
conservative startup reachability and guarded constructor dispatch. These remain
restricted contracts, not a complete PSP kernel.

Next dependencies are broader synchronization/timeouts, verified AOT,
filesystem/ISO access and module loading as observed. General active timers,
general callback delivery, overlays, rendering and title-screen progression remain
unimplemented. Preserve the external timeout and rejecting unknown-import policy.

Display interrupt and GE foundations are implemented in
[Milestone 004](004-graphics-input.md); guest drawing integration remains open.
Keep this milestone's title-screen gate open. Missing kernel/I/O/module services still
belong here and must be implemented when encountered.

## Candidate-root inspection

**VERIFIED tooling:** `tools/inspect_root.py` provides strict-hash, bounded,
local CFG inspection and distinguishes new candidates, existing roots and import
stubs. It never expands authorized AOT roots automatically; unknown indirect
transfers and size limits require explicit review. It has been executed on the
exact local input during the [graphics startup continuation](../research/startup-indirect-2026-10-22.md),
alongside synthetic regressions. Those verified coverage increments preserve
this milestone's open title-screen gate.
