# Milestone 003: boot sequence

**IN PROGRESS, 2026-10-27.** Native bring-up is implemented; title-screen boot is
not. The current bounded experiment completes the full 59-entry constructor loop, creates
four mutexes and eight inactive timers, registers its exit callback and completes
the post-constructor object's 2048-record initialization loop. It allocates a
single-block fixed pool, completes language get/set and creates an event flag.
Both workers now wait on their shared event flag. The main thread completes
VBLANK registration, the first GE initialization list and display-mode setup
under bounded idle events, then advances through guarded graphics methods,
resource layout/allocation and checked clock conversion. The latest 438-root
native slice follows the post-clear object graph and later state callback,
executes five VBLANK events and completes four GE lists. Two GE clears write
261,120 guest-VRAM pixel positions. It reads the nominal 222/222/111 MHz clock
profile, constructs the next startup objects and registers a checked source-only
`fatms0:` callback without accessing a host filesystem. The new boundary is missing
`scePowerRegisterCallback` (`0x04B7766E`) at `0x08B89210`; title boot
remains open. See
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

**2026-10-25 static checkpoint:** The exact ordinary-BLEZ delay-slot source for
`0x08862338` and its candidate callees were recorded before native continuation.
That static gate is now superseded by the 2026-10-27 executed checkpoint above;
its provenance remains in [static investigation](../research/callback-08862338.md).

## Candidate-root inspection

**VERIFIED tooling:** `tools/inspect_root.py` provides strict-hash, bounded,
local CFG inspection and distinguishes new candidates, existing roots and import
stubs. It never expands authorized AOT roots automatically; unknown indirect
transfers and size limits require explicit review. It has been executed on the
exact local input during the [graphics startup continuation](../research/startup-indirect-2026-10-22.md),
alongside synthetic regressions. Those verified coverage increments preserve
this milestone's open title-screen gate.
