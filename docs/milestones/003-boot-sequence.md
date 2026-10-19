# Milestone 003: boot sequence

**IN PROGRESS, 2026-10-19.** Native bring-up is implemented; title-screen boot is
not. The current bounded experiment completes the full 59-entry constructor loop, creates
four mutexes and eight inactive timers, registers its exit callback and completes
the post-constructor object's 2048-record initialization loop. It allocates a
single-block fixed pool and completes language get/set, then reaches event-flag
creation after 15765 dispatches. See
[constructor-completion evidence](../research/constructor-coverage.md) and
[object/exception-boundary evidence](../research/startup-objects.md) and
[fixed-pool evidence](../research/fixed-pools.md) and
[language-mode evidence](../research/language-mode.md).

Implemented foundations include thread lifecycle and priority handoff, shared
stack/partition allocation, resident module identity, nested interrupt state,
conservative startup reachability and guarded constructor dispatch. These remain
restricted contracts, not a complete PSP kernel.

Next dependencies are synchronization waits and timeouts, broader verified AOT,
filesystem/ISO access and module loading as observed. General timers, callbacks,
interrupt delivery, overlays, rendering and title-screen progression remain
unimplemented. Preserve the external timeout and rejecting unknown-import policy.
