# Milestone 003: boot sequence

**IN PROGRESS; READY FOR GRAPHICS INTEGRATION, 2026-10-20.** Native bring-up is implemented; title-screen boot is
not. The current bounded experiment completes the full 59-entry constructor loop, creates
four mutexes and eight inactive timers, registers its exit callback and completes
the post-constructor object's 2048-record initialization loop. It allocates a
single-block fixed pool, completes language get/set and creates an event flag.
Both workers reach Ready while the main thread enters its display setup path,
reads EDRAM size and registers a GE finish callback. It reaches VBLANK handler
registration after 16801 dispatches. See
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

Next dependencies are synchronization waits and timeouts, broader verified AOT,
filesystem/ISO access and module loading as observed. General timers, callbacks,
interrupt delivery, overlays, rendering and title-screen progression remain
unimplemented. Preserve the external timeout and rejecting unknown-import policy.

The next execution dependency is display interrupt integration in
[Milestone 004](004-graphics-input.md). Begin that work while keeping this
milestone's title-screen gate open. Missing kernel/I/O/module services still
belong here and must be implemented when encountered.

## Candidate-root inspection handoff (2026-10-22)

**IMPLEMENTED tooling, not verified game progression:** `tools/inspect_root.py`
provides a strict-hash, bounded, local-only CFG inspection for the next uncovered
direct helper `0x08ACD010`. It never expands authorized AOT roots automatically;
unknown indirect transfers and size limits produce explicit research blockers.
The source and synthetic test cases were added without access to the user-owned
EBOOT in this session. Re-run the complete Python suite, execute the inspector
locally with the exact EBOOT, review its candidate call targets and only then
consider carefully expanding startup coverage. The prior 280-root/23,880-word
checkpoint and title-screen gate remain unchanged.
