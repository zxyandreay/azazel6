# Milestone 003: boot sequence

**IN PROGRESS, 2026-10-18.** Native bring-up is implemented; title-screen boot is
not. The current bounded experiment completes the first constructor and executes the second through completed string formatting and creates an inactive virtual timer and reaches priority change. See
[inactive-timer evidence](../research/virtual-timers.md).

Implemented foundations include thread lifecycle and priority handoff, shared
stack/partition allocation, resident module identity, nested interrupt state,
conservative startup reachability and guarded constructor dispatch. These remain
restricted contracts, not a complete PSP kernel.

Next dependencies are synchronization waits and timeouts, broader verified AOT,
filesystem/ISO access and module loading as observed. General timers, callbacks,
interrupt delivery, overlays, rendering and title-screen progression remain
unimplemented. Preserve the external timeout and rejecting unknown-import policy.
